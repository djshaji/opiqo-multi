#include <malloc.h>
#include "jalv.h"
#include "jalv/state.h"
#include "zix/zix.h"
#include "jalv/string_utils.h"
#include "logging_macros.h"
#include "jalv/process_setup.h"
#include "serd/serd.h"
#include "jalv/frontend.h"
#include "jalv/macros.h"
#include "lv2/patch/patch.h"
#include "jack/types.h"


static bool
jalv_apply_control_arg(Jalv* jalv, const char* s)
{
    char  sym[256] = {'\0'};
    float val      = 0.0f;
    if (sscanf(s, "%240[^=]=%f", sym, &val) != 2) {
        LOGD ("Ignoring invalid value `%s'\n", s);
        return false;
    }

    Control* const control = get_named_control(&jalv->controls, sym);
    if (!control) {
        LOGD (
                "Ignoring value for unknown control `%s'\n", sym);
        return false;
    }

    jalv_set_control(jalv, control, sizeof(float), jalv->urids.atom_Float, &val);
    LOGD ("%s = %f\n", sym, val);

    return true;
}

static void
jalv_init_ui_settings(Jalv* const jalv)
{
    const JalvOptions* const opts     = &jalv->opts;
    JalvSettings* const      settings = &jalv->settings;

    if (!settings->ring_size) {
        /* The UI ring is fed by plugin output ports (usually one), and the UI
           updates roughly once per cycle.  The ring size is a few times the size
           of the MIDI output to give the UI a chance to keep up. */
        settings->ring_size = settings->midi_buf_size * N_BUFFER_CYCLES;
    }

    if (opts->update_rate <= 0.0f) {
        // Calculate a reasonable UI update frequency
        settings->ui_update_hz = jalv_frontend_refresh_rate(jalv);
    }

    if (opts->scale_factor <= 0.0f) {
        // Calculate the monitor's scale factor
        settings->ui_scale_factor = jalv_frontend_scale_factor(jalv);
    }

    // The UI can only go so fast, clamp to reasonable limits
    settings->ui_update_hz = MAX(1.0f, MIN(60.0f, settings->ui_update_hz));
    settings->ring_size    = MAX(4096, settings->ring_size);
    LOGD ("Comm buffers: %u bytes\n", settings->ring_size);
    LOGD ("Update rate:  %.01f Hz\n", settings->ui_update_hz);
    LOGD ("Scale factor: %.01f\n", settings->ui_scale_factor);
}

/// Return true iff Jalv supports the given feature
static bool
feature_is_supported(const Jalv* jalv, const char* uri)
{
    if (!strcmp(uri, "http://lv2plug.in/ns/lv2core#isLive") ||
        !strcmp(uri, "http://lv2plug.in/ns/lv2core#inPlaceBroken")) {
        return true;
    }

    for (const LV2_Feature* const* f = jalv->feature_list; *f; ++f) {
        if (!strcmp(uri, (*f)->URI)) {
            return true;
        }
    }
    return false;
}

static void
jalv_create_controls(Jalv* jalv, bool writable)
{
    const LilvPlugin* plugin         = jalv->plugin;
    LilvWorld*        world          = jalv->world;
    LilvNode*         patch_writable = lilv_new_uri(world, LV2_PATCH__writable);
    LilvNode*         patch_readable = lilv_new_uri(world, LV2_PATCH__readable);

    LilvNodes* properties =
            lilv_world_find_nodes(world,
                                  lilv_plugin_get_uri(plugin),
                                  writable ? patch_writable : patch_readable,
                                  NULL);
    LILV_FOREACH (nodes, p, properties) {
        const LilvNode* property = lilv_nodes_get(properties, p);
        Control*        record   = NULL;

        if (!writable &&
            lilv_world_ask(
                    world, lilv_plugin_get_uri(plugin), patch_writable, property)) {
            // Find existing writable control
            for (size_t i = 0; i < jalv->controls.n_controls; ++i) {
                if (lilv_node_equals(jalv->controls.controls[i]->node, property)) {
                    record              = jalv->controls.controls[i];
                    record->is_readable = true;
                    break;
                }
            }

            if (record) {
                continue;
            }
        }

        record = new_property_control(jalv->world,
                                      property,
                                      &jalv->nodes,
                                      jalv_mapper_urid_map(jalv->mapper),
                                      &jalv->forge);

        if (writable) {
            record->is_writable = true;
        } else {
            record->is_readable = true;
        }

        if (record->value_type) {
            add_control(&jalv->controls, record);
        } else {
            LOGD (
                     "Parameter <%s> has unknown value type, ignored\n",
                     lilv_node_as_string(record->node));
            free(record);
        }
    }
    lilv_nodes_free(properties);

    lilv_node_free(patch_readable);
    lilv_node_free(patch_writable);
}

/**
   Create a port structure from data description.

   This is called before plugin and Jack instantiation.  The remaining
   instance-specific setup (e.g. buffers) is done later in activate_port().
*/
static int
create_port(Jalv* jalv, uint32_t port_index)
{
    JalvPort* const port = &jalv->ports[port_index];

    port->lilv_port = lilv_plugin_get_port_by_index(jalv->plugin, port_index);
    port->index     = port_index;
    port->flow      = FLOW_UNKNOWN;

    JalvProcessPort* const pport = &jalv->process.ports[port_index];
    if (jalv_process_port_init(&jalv->process.ports[port_index],
                               &jalv->nodes,
                               jalv->plugin,
                               port->lilv_port)) {
        return 1;
    }

    port->type = pport->type;
    port->flow = pport->flow;

    if (lilv_port_is_a(
            jalv->plugin, port->lilv_port, jalv->nodes.lv2_ControlPort)) {
        add_control(&jalv->controls,
                    new_port_control(jalv->plugin,
                                     port->lilv_port,
                                     port->index,
                                     jalv->settings.sample_rate,
                                     &jalv->nodes,
                                     &jalv->forge));
    }

    // Store index if this is the designated control input port
    if (jalv->process.control_in == UINT32_MAX && pport->is_primary &&
        port->flow == FLOW_INPUT && port->type == TYPE_EVENT) {
        jalv->process.control_in = port_index;
    }

    // Update maximum buffer sizes
    const uint32_t buf_size =
            pport->buf_size ? pport->buf_size : jalv->settings.midi_buf_size;
    jalv->opts.ring_size = MAX(jalv->opts.ring_size, buf_size * N_BUFFER_CYCLES);
    if (port->flow == FLOW_INPUT) {
        jalv->process.process_msg_size =
                MAX(jalv->process.process_msg_size, buf_size);
    } else if (port->flow == FLOW_OUTPUT) {
        jalv->ui_msg_size = MAX(jalv->ui_msg_size, buf_size);
    }

    return 0;
}

/// Create port structures from data (via create_port()) for all ports
static int
jalv_create_ports(Jalv* jalv)
{
    const uint32_t n_ports = lilv_plugin_get_num_ports(jalv->plugin);

    jalv->num_ports         = n_ports;
    jalv->ports             = (JalvPort*)calloc(n_ports, sizeof(JalvPort));
    jalv->process.num_ports = n_ports;
    jalv->process.ports =
            (JalvProcessPort*)calloc(n_ports, sizeof(JalvProcessPort));

    // Allocate control port buffers array and set to default values
    jalv->process.controls_buf = (float*)calloc(n_ports, sizeof(float));
    lilv_plugin_get_port_ranges_float(
            jalv->plugin, NULL, NULL, jalv->process.controls_buf);

    for (uint32_t i = 0; i < jalv->num_ports; ++i) {
        if (create_port(jalv, i)) {
            return 1;
        }
    }

    return 0;
}

int
jalv_backend_open_(JalvBackend* const     backend,
                  const JalvURIDs* const urids,
                  JalvSettings* const    settings,
                  JalvProcess* const     process,
                  ZixSem* const          done,
                  const char* const      name,
                  const bool             exact_name)
{
    LOGD("[jalv_backend_open_] implement me: Initializing backend");
    // Set audio engine properties
    settings->sample_rate   = (float)48000;
    settings->block_length  = 4096;
    settings->midi_buf_size = 4096;
    void* const arg = (void*)backend;

//    backend->urids              = urids;
//    backend->settings           = settings;
//    backend->process            = process;
//    backend->done               = done;
//    backend->client             = NULL;
//    backend->is_internal_client = false;
    return 0;
}

/// Find the initial state and set jalv->plugin
static LilvState*
open_plugin_state(Jalv* const         jalv,
                  LV2_URID_Map* const urid_map,
                  const char* const   load_arg)
{
    LilvWorld* const         world   = jalv->world;
    const LilvPlugins* const plugins = lilv_world_get_all_plugins(world);
    LilvState*               state   = NULL;

    LOGD("[open_plugin_state] Finding initial plugin state: %s", load_arg ? load_arg : "(none)");
    if (!load_arg) {
        // No URI or path given, open plugin selector
        LilvNode* const plugin_uri = jalv_frontend_select_plugin(world);
        if (plugin_uri) {
            state = lilv_state_new_from_world(jalv->world, urid_map, plugin_uri);
            jalv->plugin = lilv_plugins_get_by_uri(plugins, plugin_uri);
            lilv_node_free(plugin_uri);
        }
    } else {
        // URI or path given as command-line argument
        const char* const arg = load_arg;
        if (serd_uri_string_has_scheme((const uint8_t*)arg)) {
            LilvNode* state_uri = lilv_new_uri(jalv->world, arg);
            state = lilv_state_new_from_world(jalv->world, urid_map, state_uri);
            LOGD ("[open_plugin_state] lilv_state_new_from_world: %s\n", arg);
            lilv_node_free(state_uri);
        } else {
            state = lilv_state_new_from_file(jalv->world, urid_map, NULL, arg);
            LOGD ("[open_plugin_state] lilv_state_new_from_file: %s\n", arg);
        }

        if (state) {
            jalv->plugin =
                    lilv_plugins_get_by_uri(plugins, lilv_state_get_plugin_uri(state));
        } else {
            LOGD ( "Failed to load state \"%s\"\n", load_arg);
        }
    }

    return state;
}

static void
init_feature(LV2_Feature* const dest, const char* const URI, void* data)
{
    dest->URI  = URI;
    dest->data = data;
}

static void
jalv_init_features(Jalv* const jalv)
{
    // urid:map
    init_feature(&jalv->features.map_feature,
                 LV2_URID__map,
                 jalv_mapper_urid_map(jalv->mapper));

    // urid:unmap
    init_feature(&jalv->features.unmap_feature,
                 LV2_URID__unmap,
                 jalv_mapper_urid_unmap(jalv->mapper));

    // state:makePath
    jalv->features.make_path.handle = jalv;
    jalv->features.make_path.path   = jalv_make_path;
    init_feature(&jalv->features.make_path_feature,
                 LV2_STATE__makePath,
                 &jalv->features.make_path);

    // worker:schedule (normal)
    jalv->features.sched.schedule_work = jalv_worker_schedule;
    init_feature(
            &jalv->features.sched_feature, LV2_WORKER__schedule, &jalv->features.sched);

    // worker:schedule (state)
    jalv->features.ssched.schedule_work = jalv_worker_schedule;
    init_feature(&jalv->features.state_sched_feature,
                 LV2_WORKER__schedule,
                 &jalv->features.ssched);

    // log:log
    jalv->features.llog.handle  = &jalv->log;
    jalv->features.llog.printf  = jalv_printf;
    jalv->features.llog.vprintf = jalv_vprintf;
    init_feature(&jalv->features.log_feature, LV2_LOG__log, &jalv->features.llog);

    // (options:options is initialized later by jalv_init_options())

    // state:threadSafeRestore
    init_feature(
            &jalv->features.safe_restore_feature, LV2_STATE__threadSafeRestore, NULL);

    // ui:requestValue
    jalv->features.request_value.handle = jalv;
    init_feature(&jalv->features.request_value_feature,
                 LV2_UI__requestValue,
                 &jalv->features.request_value);
}


int
jalv_open_(Jalv* const jalv, const char* const load_arg)
{
    JalvSettings* const settings = &jalv->settings;

    settings->block_length    = 4096U;
    settings->midi_buf_size   = 1024U;
    settings->ring_size       = jalv->opts.ring_size;
    settings->ui_update_hz    = jalv->opts.update_rate;
    settings->ui_scale_factor = jalv->opts.scale_factor;

    // Load the LV2 world
    LilvWorld* const world = lilv_world_new();
    lilv_world_set_option(world, LILV_OPTION_OBJECT_INDEX, NULL);
    lilv_world_load_all(world);

    jalv->world       = world;
    jalv->mapper      = jalv_mapper_new();
    jalv->log.urids   = &jalv->urids;
    jalv->log.tracing = jalv->opts.trace;

    // Set up atom dumping for debugging if enabled
    LV2_URID_Map* const   urid_map   = jalv_mapper_urid_map(jalv->mapper);
    LV2_URID_Unmap* const urid_unmap = jalv_mapper_urid_unmap(jalv->mapper);
    if (jalv->opts.dump) {
        jalv->dumper = jalv_dumper_new(urid_map, urid_unmap);
    }

    zix_sem_init(&jalv->work_lock, 1);
    zix_sem_init(&jalv->done, 0);
    jalv_init_urids(jalv->mapper, &jalv->urids);
    jalv_init_nodes(world, &jalv->nodes);
    jalv_init_features(jalv);
    lv2_atom_forge_init(&jalv->forge, urid_map);

    // Create temporary directory for plugin state
//    jalv->temp_dir = zix_create_temporary_directory(NULL, "jalvXXXXXX");
    if (!jalv->temp_dir) {
        LOGD ("Failed to create temporary state directory\n");
    }

    // Find the initial state (and thereby the plugin URI)
    LilvState* state = open_plugin_state(jalv, urid_map, load_arg);
    if (!state || !jalv->plugin) {
        LOGD ("[todo] Failed to open state\n");
        return -2;
    }

    if (!jalv->plugin) {
        LOGD ("No plugin selected\n");
        return -3;
    }

    LOGD (
             "Plugin:       %s\n",
             lilv_node_as_string(lilv_plugin_get_uri(jalv->plugin)));

    // Set client name from plugin name if the user didn't specify one
    jalv->plugin_name = lilv_plugin_get_name(jalv->plugin);
    if (!jalv->opts.name) {
        jalv->opts.name = jalv_strdup(lilv_node_as_string(jalv->plugin_name));
    }

    // Check for thread-safe state restore() method
    jalv->safe_restore =
            lilv_plugin_has_feature(jalv->plugin, jalv->nodes.state_threadSafeRestore);

    // Get a plugin UI
    jalv->uis = lilv_plugin_get_uis(jalv->plugin);
    if (!jalv->opts.generic_ui) {
        LOGD ("[jalv_open] Implement me");
    }

    // Initialize process thread
    const uint32_t update_frames =
            (uint32_t)(settings->sample_rate / settings->ui_update_hz);
    jalv_process_init(&jalv->process,
                      &jalv->urids,
                      jalv->mapper,
                      update_frames,
                      jalv->opts.trace);

    // Create workers if necessary
    if (lilv_plugin_has_extension_data(jalv->plugin,
                                       jalv->nodes.work_interface)) {
        jalv->process.worker        = jalv_worker_new(&jalv->work_lock, true);
        jalv->features.sched.handle = jalv->process.worker;
        if (jalv->safe_restore) {
            jalv->process.state_worker   = jalv_worker_new(&jalv->work_lock, false);
            jalv->features.ssched.handle = jalv->process.state_worker;
        }
    }

    // Open backend (to set the sample rate, among other thigns)
    if (jalv_backend_open_(jalv->backend,
                          &jalv->urids,
                          &jalv->settings,
                          &jalv->process,
                          &jalv->done,
                          jalv->opts.name,
                          jalv->opts.name_exact)) {
        LOGD ( "Failed to connect to audio system\n");
        return -6;
    }

    LOGD (
            "Sample rate:  %u Hz\n", (uint32_t)settings->sample_rate);
    LOGD ("Block length: %u frames\n", settings->block_length);
    LOGD ("MIDI buffers: %zu bytes\n", settings->midi_buf_size);

    // Create port structures
    if (jalv_create_ports(jalv)) {
        return -10;
    }

    // Create input and output control structures
    jalv_create_controls(jalv, true);
    jalv_create_controls(jalv, false);

    jalv_init_ui_settings(jalv);
    jalv_init_lv2_options(&jalv->features, &jalv->urids, settings);

    // Create Plugin => UI communication buffers
    jalv->ui_msg_size = MAX(jalv->ui_msg_size, settings->midi_buf_size);
    jalv->ui_msg      = zix_aligned_alloc(NULL, 8U, jalv->ui_msg_size);

    // Build feature list for passing to plugins
    const LV2_Feature* const features[] = {&jalv->features.map_feature,
                                           &jalv->features.unmap_feature,
                                           &jalv->features.sched_feature,
                                           &jalv->features.log_feature,
                                           &jalv->features.options_feature,
                                           &static_features[0],
                                           &static_features[1],
                                           &static_features[2],
                                           &static_features[3],
                                           NULL};

    jalv->feature_list = (const LV2_Feature**)calloc(1, sizeof(features));
    if (!jalv->feature_list) {
        LOGD ( "Failed to allocate feature list\n");
        return -7;
    }
    memcpy(jalv->feature_list, features, sizeof(features));

    // Check that any required features are supported
    LilvNodes* req_feats = lilv_plugin_get_required_features(jalv->plugin);
    LILV_FOREACH (nodes, f, req_feats) {
        const char* uri = lilv_node_as_uri(lilv_nodes_get(req_feats, f));
        if (!feature_is_supported(jalv, uri)) {
            LOGD ( "Feature %s is not supported\n", uri);
            return -8;
        }
    }
    lilv_nodes_free(req_feats);

    // Instantiate the plugin
    LilvInstance* const instance = lilv_plugin_instantiate(
            jalv->plugin, settings->sample_rate, jalv->feature_list);
    if (!instance) {
        LOGD ( "Failed to instantiate plugin\n");
        return -9;
    }

    // Point things to the instance that require it

    jalv->features.ext_data.data_access =
            lilv_instance_get_descriptor(instance)->extension_data;

    const LV2_Worker_Interface* worker_iface =
            (const LV2_Worker_Interface*)lilv_instance_get_extension_data(
                    instance, LV2_WORKER__interface);

    jalv_worker_attach(jalv->process.worker, worker_iface, instance->lv2_handle);
    jalv_worker_attach(
            jalv->process.state_worker, worker_iface, instance->lv2_handle);
    LOGD ("\n");

    // Allocate port buffers
    jalv_process_activate(
            &jalv->process, &jalv->urids, instance, &jalv->settings);

    // Apply loaded state to plugin instance if necessary
    if (state) {
        jalv_apply_state(jalv, state);
        lilv_state_free(state);
    }

    // Apply initial controls from command-line arguments
    if (jalv->opts.controls) {
        for (char** c = jalv->opts.controls; *c; ++c) {
            jalv_apply_control_arg(jalv, *c);
        }
    }

    // Create Jack ports and connect plugin ports to buffers
    for (uint32_t i = 0; i < jalv->num_ports; ++i) {
        jalv_connect_ports(jalv->backend, &jalv->process, i);
    }

    return 0;
}


void
jalv_connect_ports (JalvBackend* const backend,
                           JalvProcess* const proc,
                           const uint32_t     port_index)
{
    LOGD("[jalv_connect_ports] implement me: Connecting ports");
    JalvProcessPort* const port   = &proc->ports[port_index];

    // Connect unsupported ports to NULL (known to be optional by this point)
    if (port->flow == FLOW_UNKNOWN || port->type == TYPE_UNKNOWN) {
        lilv_instance_connect_port(proc->instance, port_index, NULL);
        return;
    }

    // Build Jack flags for port
    enum JackPortFlags jack_flags =
            (port->flow == FLOW_INPUT) ? JackPortIsInput : JackPortIsOutput;

    // Connect the port based on its type
    switch (port->type) {
        case TYPE_UNKNOWN:
            break;
        case TYPE_CONTROL:
            LOGD("[jalv_connect_ports] Connect control port %u to buffer\n", port_index);
            lilv_instance_connect_port(
                    proc->instance, port_index, &proc->controls_buf[port_index]);
            break;
        case TYPE_AUDIO:
            LOGD("[jalv_connect_ports] Connect audio port %u to Jack\n", port_index);
//            port->sys_port = jack_port_register(
//                    client, port->symbol, JACK_DEFAULT_AUDIO_TYPE, jack_flags, 0);
            break;
        case TYPE_EVENT:
            LOGD("[jalv_connect_ports] Connect event port %u to Jack MIDI\n", port_index);
//            if (port->supports_midi) {
//                port->sys_port = jack_port_register(
//                        client, port->symbol, JACK_DEFAULT_MIDI_TYPE, jack_flags, 0);
//            }
            break;
    }

}

/*
 * LV2OboeHost.hpp
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Minimal LV2 host for Android using Oboe (headless, audio-only).
 */

#pragma once

#include <oboe/Oboe.h>

#include "lv2_ringbuffer.h"
#include <lilv/lilv.h>

#include <lv2/urid/urid.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/atom/forge.h>
#include <lv2/options/options.h>
#include <lv2/parameters/parameters.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/patch/patch.h>
#include <lv2/worker/worker.h>
#include <lv2/state/state.h>
#include <lv2/resize-port/resize-port.h>

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class LV2OboeHost : public oboe::AudioStreamDataCallback {
public:
    LV2OboeHost() = default;

    ~LV2OboeHost() {
        closeHost();
    }

    void init_world() {
        world = lilv_world_new();
        lilv_world_load_all(world);
        plugs = lilv_world_get_all_plugins(world);
    }

    bool init_oboe(const char* uri, int32_t sample_rate, int32_t frames_per_burst) {
        plugin_uri = uri;
        if (!world) init_world();
        max_block_length = static_cast<uint32_t>(frames_per_burst);
        if (!init_lilv()) return false;
        if (!init_ports()) return false;
        if (!init_instance(sample_rate)) return false;
        return init_audio(sample_rate, frames_per_burst);
    }

    bool init_audio(int32_t sample_rate, int32_t frames_per_burst) {
        oboe::AudioStreamBuilder builder;
        oboe::Result result = builder
            .setDirection(oboe::Direction::Output)
            .setPerformanceMode(oboe::PerformanceMode::LowLatency)
            .setSharingMode(oboe::SharingMode::Exclusive)
            .setFormat(oboe::AudioFormat::Float)
            .setChannelCount(2)
            .setSampleRate(sample_rate)
            .setFramesPerCallback(frames_per_burst)
            .setDataCallback(this)
            .openStream(audio_stream);

        if (result != oboe::Result::OK) return false;

        channel_capacity = frames_per_burst;
        left_channel.reset(new float[channel_capacity]);
        right_channel.reset(new float[channel_capacity]);
        std::fill(left_channel.get(), left_channel.get() + channel_capacity, 0.0f);
        std::fill(right_channel.get(), right_channel.get() + channel_capacity, 0.0f);
        return true;
    }

    void start_audio() {
        if (audio_stream) audio_stream->start();
    }

    void stop_audio() {
        if (audio_stream) audio_stream->stop();
    }

    void closeHost() {
        stop_audio();
        if (audio_stream) {
            audio_stream->close();
            audio_stream.reset();
        }

        if (instance) {
            lilv_instance_deactivate(instance);
        }

        stop_worker();

        if (instance) {
            lilv_instance_free(instance);
            instance = nullptr;
        }

        for (auto& p : ports) {
            if (p.atom) free(p.atom);
            delete p.atom_state;
        }
        ports.clear();

        if (world) {
            freeNodes();
            lilv_world_free(world);
            world = nullptr;
        }
    }

    void set_control_value(uint32_t port_index, float value) {
        if (port_index >= ports.size()) return;
        Port& p = ports[port_index];
        if (!p.is_control || !p.is_input) return;
        p.control = value;
    }

    bool set_atom_message(uint32_t port_index, uint32_t type, const void* data, uint32_t size) {
        if (!data || port_index >= ports.size()) return false;
        Port& p = ports[port_index];
        if (!p.is_atom || !p.is_input) return false;
        p.atom_state->ui_to_dsp.resize(size);
        memcpy(p.atom_state->ui_to_dsp.data(), data, size);
        p.atom_state->ui_to_dsp_type = type;
        p.atom_state->ui_to_dsp_pending.store(true, std::memory_order_release);
        return true;
    }

    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream* /*stream*/,
        void* audioData,
        int32_t numFrames) override {

        if (shutdown.load(std::memory_order_acquire))
            return oboe::DataCallbackResult::Stop;

        if (!audioData || numFrames <= 0 || numFrames > channel_capacity)
            return oboe::DataCallbackResult::Stop;

        auto* buffer = static_cast<float*>(audioData);

        for (int32_t i = 0; i < numFrames; ++i) {
            left_channel[i] = buffer[i * 2];
            right_channel[i] = buffer[i * 2 + 1];
        }

        uint32_t input_index = 0;
        uint32_t output_index = 0;
        for (auto& p : ports) {
            if (!p.is_audio) continue;

            float* target = left_channel.get();
            if (p.is_input) {
                target = (input_index == 0) ? left_channel.get() : right_channel.get();
                ++input_index;
            } else {
                target = (output_index == 0) ? left_channel.get() : right_channel.get();
                ++output_index;
            }

            lilv_instance_connect_port(instance, p.index, target);
        }

        for (auto& p : ports) {
            if (p.is_atom && !p.is_input) {
                p.atom->atom.type = 0;
                p.atom->atom.size = p.atom_buf_size - sizeof(LV2_Atom);
            }

            if (p.is_atom && p.is_input) {
                if (p.atom_state->ui_to_dsp_pending.exchange(false, std::memory_order_acquire)) {
                    p.atom->atom.type = urids.atom_Sequence;
                    p.atom->atom.size = 0;
                    const uint32_t body_size = p.atom_state->ui_to_dsp.size();
                    uint8_t evbuf[sizeof(LV2_Atom_Event) + required_atom_size];
                    LV2_Atom_Event* ev = (LV2_Atom_Event*)evbuf;
                    ev->time.frames = 0;
                    ev->body.type  = p.atom_state->ui_to_dsp_type;
                    ev->body.size  = body_size;
                    memcpy((uint8_t*)LV2_ATOM_BODY(&ev->body),
                        p.atom_state->ui_to_dsp.data(), body_size);
                    lv2_atom_sequence_append_event(p.atom, p.atom_buf_size, ev);
                }
            }
        }

        lilv_instance_run(instance, numFrames);

        if (host_worker.iface) deliver_worker_responses(&host_worker);

        for (auto& p : ports) {
            if (p.is_atom && p.is_input) p.atom->atom.size = 0;
            if (p.is_atom && !p.is_input) {
                LV2_Atom_Sequence* seq = p.atom;
                LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
                    if (ev->body.size == 0) break;
                    if (p.atom->atom.type == 0) break;
                    const uint32_t total = sizeof(LV2_Atom) + ev->body.size;
                    if (lv2_ringbuffer_write_space(p.atom_state->dsp_to_ui) >= total) {
                        lv2_ringbuffer_write(p.atom_state->dsp_to_ui,
                            (const char*)&ev->body, total);
                    }
                }
                p.atom->atom.type = 0;
                p.atom->atom.size = required_atom_size;
            }
        }

        for (int32_t i = 0; i < numFrames; ++i) {
            buffer[i * 2] = left_channel[i];
            buffer[i * 2 + 1] = right_channel[i];
        }

        return oboe::DataCallbackResult::Continue;
    }

private:
    struct LV2HostWorker {
        lv2_ringbuffer_t* requests = nullptr;
        lv2_ringbuffer_t* responses = nullptr;

        LV2_Worker_Schedule schedule;
        LV2_Feature feature;
        const LV2_Worker_Interface* iface = nullptr;
        LV2_Handle dsp_handle;

        std::atomic<bool> running{false};
        std::atomic<bool> work_pending{false};
        std::thread worker_thread;

        std::vector<uint8_t> response_buffer;
    };

    static LV2_Worker_Status host_schedule_work(
                    LV2_Worker_Schedule_Handle handle,
                    uint32_t size, const void* data) {

        auto* w = (LV2HostWorker*)handle;
        const size_t total = sizeof(uint32_t) + size;
        if (lv2_ringbuffer_write_space(w->requests) < total)
            return LV2_WORKER_ERR_NO_SPACE;

        lv2_ringbuffer_write(w->requests, (const char*)&size, sizeof(uint32_t));
        lv2_ringbuffer_write(w->requests, (const char*)data, size);
        w->work_pending.store(true, std::memory_order_release);

        return LV2_WORKER_SUCCESS;
    }

    static void worker_thread_func(LV2HostWorker* w) {
        while (w->running.load()) {
            if (lv2_ringbuffer_read_space(w->requests) < sizeof(uint32_t)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            uint32_t size;
            lv2_ringbuffer_peek(w->requests, (char*)&size, sizeof(uint32_t));

            if (lv2_ringbuffer_read_space(w->requests) < sizeof(uint32_t) + size) {
                continue;
            }

            lv2_ringbuffer_read(w->requests, (char*)&size, sizeof(uint32_t));
            std::vector<uint8_t> buf(size);
            lv2_ringbuffer_read(w->requests, (char*)buf.data(), size);
            w->iface->work(w->dsp_handle, host_respond, w, size, buf.data());
        }
    }

    static LV2_Worker_Status host_respond(
                    LV2_Worker_Respond_Handle handle,
                    uint32_t size, const void* data) {

        auto* w = (LV2HostWorker*)handle;
        const size_t total = sizeof(uint32_t) + size;

        if (lv2_ringbuffer_write_space(w->responses) < total)
            return LV2_WORKER_ERR_NO_SPACE;

        lv2_ringbuffer_write(w->responses, (const char*)&size, sizeof(uint32_t));
        lv2_ringbuffer_write(w->responses, (const char*)data, size);

        return LV2_WORKER_SUCCESS;
    }

    void deliver_worker_responses(LV2HostWorker* w) {
        constexpr size_t kDrainChunk = 256;
        uint8_t scratch[kDrainChunk];
        while (true) {
            if (lv2_ringbuffer_read_space(w->responses) < sizeof(uint32_t)) break;

            uint32_t size;
            lv2_ringbuffer_peek(w->responses, (char*)&size, sizeof(uint32_t));

            if (lv2_ringbuffer_read_space(w->responses) < sizeof(uint32_t) + size) break;

            lv2_ringbuffer_read(w->responses, (char*)&size, sizeof(uint32_t));

            if (size <= w->response_buffer.size()) {
                lv2_ringbuffer_read(w->responses, (char*)w->response_buffer.data(), size);
                w->iface->work_response(w->dsp_handle, size, w->response_buffer.data());
                continue;
            }

            size_t remaining = size;
            while (remaining > 0) {
                const size_t chunk = std::min(remaining, kDrainChunk);
                lv2_ringbuffer_read(w->responses, (char*)scratch, chunk);
                remaining -= chunk;
            }
        }
    }

    void stop_worker() {
        if (!host_worker.running.exchange(false))
            return;

        if (host_worker.worker_thread.joinable())
            host_worker.worker_thread.join();

        if (host_worker.requests) {
            lv2_ringbuffer_free(host_worker.requests);
            host_worker.requests = nullptr;
        }

        if (host_worker.responses) {
            lv2_ringbuffer_free(host_worker.responses);
            host_worker.responses = nullptr;
        }

        host_worker.iface = nullptr;
        host_worker.dsp_handle = nullptr;
    }

    struct AtomState {
        std::vector<uint8_t> ui_to_dsp;
        uint32_t ui_to_dsp_type = 0;
        std::atomic<bool> ui_to_dsp_pending{false};

        lv2_ringbuffer_t* dsp_to_ui = nullptr;

        AtomState(size_t sz = 16384) {
            dsp_to_ui = lv2_ringbuffer_create(sz);
        }

        ~AtomState() {
            lv2_ringbuffer_free(dsp_to_ui);
        }
    };

    struct Port {
        uint32_t index = 0;
        bool is_audio = false;
        bool is_input = false;
        bool is_control = false;
        bool is_atom = false;
        bool is_midi = false;

        float control = 0.0f;
        float defvalue = 0.0f;

        LV2_Atom_Sequence* atom = nullptr;
        uint32_t atom_buf_size = 8192;
        AtomState* atom_state = nullptr;

        std::string uri;
        const char* symbol = nullptr;
    };

    struct {
        LV2_URID atom_eventTransfer;
        LV2_URID atom_Sequence;
        LV2_URID atom_Object;
        LV2_URID atom_Float;
        LV2_URID atom_Int;
        LV2_URID atom_Double;
        LV2_URID midi_Event;
        LV2_URID buf_maxBlock;
        LV2_URID atom_Path;
        LV2_URID patch_Get;
        LV2_URID patch_Set;
        LV2_URID patch_property;
        LV2_URID patch_value;
        LV2_URID atom_Blank;
        LV2_URID atom_Chunk;
        LV2_URID param_sampleRate;
    } urids;

    void init_urids() {
        urids.atom_eventTransfer = map_uri(this, LV2_ATOM__eventTransfer);
        urids.atom_Sequence    = map_uri(this, LV2_ATOM__Sequence);
        urids.atom_Blank       = map_uri(this, LV2_ATOM__Blank);
        urids.atom_Chunk       = map_uri(this, LV2_ATOM__Chunk);
        urids.atom_Object      = map_uri(this, LV2_ATOM__Object);
        urids.atom_Float       = map_uri(this, LV2_ATOM__Float);
        urids.atom_Int         = map_uri(this, LV2_ATOM__Int);
        urids.atom_Double      = map_uri(this, LV2_ATOM__Double);
        urids.midi_Event       = map_uri(this, LV2_MIDI__MidiEvent);
        urids.buf_maxBlock     = map_uri(this, LV2_BUF_SIZE__maxBlockLength);
        urids.atom_Path        = map_uri(this, LV2_ATOM__Path);
        urids.patch_Get        = map_uri(this, LV2_PATCH__Get);
        urids.patch_Set        = map_uri(this, LV2_PATCH__Set);
        urids.patch_property   = map_uri(this, LV2_PATCH__property);
        urids.patch_value      = map_uri(this, LV2_PATCH__value);
        urids.param_sampleRate = map_uri(this, LV2_PARAMETERS__sampleRate);
    }

    static LV2_URID map_uri(LV2_URID_Map_Handle h, const char* uri) {
        auto* self = static_cast<LV2OboeHost*>(h);
        auto it = self->urid_map.find(uri);
        if (it != self->urid_map.end())
            return it->second;

        LV2_URID id = self->urid_map.size() + 1;
        self->urid_map[uri] = id;
        self->urid_unmap[id]  = uri;
        return id;
    }

    static const char* unmap_uri(LV2_URID_Unmap_Handle h, LV2_URID urid) {
        auto* self = static_cast<LV2OboeHost*>(h);

        auto it = self->urid_unmap.find(urid);
        if (it == self->urid_unmap.end())
            return nullptr;

        return it->second.c_str();
    }

    std::unordered_map<std::string, LV2_URID> urid_map;
    std::unordered_map<LV2_URID, std::string> urid_unmap;

    LV2_URID_Map um;
    LV2_URID_Unmap unm;

    struct {
        LV2_Feature um_f;
        LV2_Feature unm_f;

        LV2_Feature map_path_feature;
        LV2_Feature make_path_feature;
        LV2_Feature free_path_feature;
        LV2_Feature bbl_feature;
    } features;

    static char* make_path_func(LV2_State_Make_Path_Handle, const char* path) {
        return strdup(path);
    }

    static char* map_path_func(LV2_State_Map_Path_Handle, const char* abstract_path) {
        return strdup(abstract_path);
    }

    static void free_path_func(LV2_State_Free_Path_Handle, char* path) {
        free(path);
    }

    void init_features() {
        um.handle = this;
        um.map = map_uri;
        unm.handle = this;
        unm.unmap = unmap_uri;

        map_path.handle      = nullptr;
        map_path.abstract_path = map_path_func;
        make_path.handle = nullptr;
        make_path.path   = make_path_func;
        free_path.handle = nullptr;
        free_path.free_path = free_path_func;

        features.bbl_feature.URI  = LV2_BUF_SIZE__boundedBlockLength;
        features.bbl_feature.data = nullptr;

        features.um_f.URI = LV2_URID__map;
        features.um_f.data = &um;

        features.unm_f.URI = LV2_URID__unmap;
        features.unm_f.data = &unm;

        features.map_path_feature.URI = LV2_STATE__mapPath;
        features.map_path_feature.data = &map_path;

        features.make_path_feature.URI = LV2_STATE__makePath;
        features.make_path_feature.data = &make_path;

        features.free_path_feature.URI = LV2_STATE__freePath;
        features.free_path_feature.data = &free_path;

        host_worker.schedule.handle = &host_worker;
        host_worker.schedule.schedule_work = host_schedule_work;
        host_worker.feature.URI  = LV2_WORKER__schedule;
        host_worker.feature.data = &host_worker.schedule;
    }

    bool feature_is_supported(const char* uri, const LV2_Feature*const* f) {
        for (; *f; ++f)
            if (!strcmp(uri, (*f)->URI)) return true;
        return false;
    }

    bool check_resize_port_requirements(const LilvPlugin* plugin) {
        uint32_t n = lilv_plugin_get_num_ports(plugin);
        LilvNode* min_size =
            lilv_new_uri(world, LV2_RESIZE_PORT__minimumSize);
        bool ok = true;

        for (uint32_t i = 0; i < n; ++i) {
            const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
            if (!lilv_port_is_a(plugin, port, atom_class)) continue;
            LilvNodes* sizes = lilv_port_get_value(plugin, port, min_size);
            if (!sizes || lilv_nodes_size(sizes) == 0) continue;
            const LilvNode* n = lilv_nodes_get_first(sizes);
            uint32_t required = lilv_node_as_int(n);

            if (required > required_atom_size) {
                required_atom_size = required;
            }
            lilv_nodes_free(sizes);
        }
        lilv_node_free(min_size);
        return ok;
    }

    bool checkFeatures(const LilvPlugin* plugin, const LV2_Feature*const* feat) {
        LilvNodes* requests = lilv_plugin_get_required_features(plugin);
        LILV_FOREACH(nodes, f, requests) {
            const char* uri = lilv_node_as_uri(lilv_nodes_get(requests, f));
            if (!feature_is_supported(uri, feat)) {
                lilv_nodes_free(requests);
                return false;
            }
        }
        lilv_nodes_free(requests);
        return true;
    }

    bool init_lilv() {
        plugin = lilv_plugins_get_by_uri(plugs, lilv_new_uri(world, plugin_uri));
        if (!plugin) return false;

        audio_class     = lilv_new_uri(world, LV2_CORE__AudioPort);
        control_class   = lilv_new_uri(world, LV2_CORE__ControlPort);
        atom_class      = lilv_new_uri(world, LV2_ATOM__AtomPort);
        input_class     = lilv_new_uri(world, LV2_CORE__InputPort);
        rsz_minimumSize = lilv_new_uri(world, LV2_RESIZE_PORT__minimumSize);
        init_urids();
        init_features();
        lilv_is_inited.store(true);
        if (!check_resize_port_requirements(plugin)) return false;

        return true;
    }

    void freeNodes() {
        if (!lilv_is_inited.load()) return;
        lilv_node_free(audio_class);
        lilv_node_free(control_class);
        lilv_node_free(atom_class);
        lilv_node_free(input_class);
        lilv_node_free(rsz_minimumSize);
    }

    bool init_ports() {
        uint32_t n = lilv_plugin_get_num_ports(plugin);
        ports.reserve(n);
        LilvNode* midi_event = lilv_new_uri(world, LV2_MIDI__MidiEvent);

        for (uint32_t i = 0; i < n; ++i) {
            const LilvPort* lp = lilv_plugin_get_port_by_index(plugin, i);
            Port p;
            p.index = i;

            p.is_audio   = lilv_port_is_a(plugin, lp, audio_class);
            p.is_control = lilv_port_is_a(plugin, lp, control_class);
            p.is_atom    = lilv_port_is_a(plugin, lp, atom_class);
            p.is_input   = lilv_port_is_a(plugin, lp, input_class);
            p.is_midi    = lilv_port_supports_event(plugin, lp, midi_event);

            const LilvNode* sym = lilv_port_get_symbol(plugin, lp);
            if (sym) {
                p.uri = std::string(lilv_node_as_uri(lilv_plugin_get_uri(plugin)))
                      + "#" + lilv_node_as_string(sym);
                p.symbol = lilv_node_as_string(sym);
            }

            if (p.is_atom) {
                p.atom_buf_size = required_atom_size;

                p.atom = (LV2_Atom_Sequence*)aligned_alloc(64, p.atom_buf_size);
                memset(p.atom, 0, p.atom_buf_size);
                p.atom->atom.type = urids.atom_Sequence;

                if (p.is_input) {
                    p.atom->atom.size = sizeof(LV2_Atom_Sequence_Body);
                    p.atom->body.unit = 0;
                    p.atom->body.pad  = 0;
                } else {
                    p.atom->atom.size = 0;
                }

                p.atom_state = new AtomState;
            }

            if (p.is_control && p.is_input) {
                LilvNode *pdflt, *pmin, *pmax;
                lilv_port_get_range(plugin, lp, &pdflt, &pmin, &pmax);
                if (pmin) lilv_node_free(pmin);
                if (pmax) lilv_node_free(pmax);
                if (pdflt) {
                    p.defvalue = lilv_node_as_float(pdflt);
                    lilv_node_free(pdflt);
                }
            }

            ports.push_back(p);
        }
        lilv_node_free(midi_event);
        return true;
    }

    bool init_instance(double sample_rate) {
        LV2_Options_Option options[] = {
            {
                LV2_OPTIONS_INSTANCE,
                0,
                urids.buf_maxBlock,
                sizeof(uint32_t),
                urids.atom_Int,
                &max_block_length
            },
            { LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr }
        };

        LV2_Feature opt_f { LV2_OPTIONS__options, options };

        LV2_Feature* feats[] = { &features.um_f, &features.unm_f, &opt_f,
                            &features.bbl_feature, &host_worker.feature, nullptr };

        if (!checkFeatures(plugin, feats)) return false;

        instance = lilv_plugin_instantiate(plugin, sample_rate, feats);
        if (!instance) return false;

        const LV2_Worker_Interface* iface = (const LV2_Worker_Interface*)
            lilv_instance_get_extension_data(instance, LV2_WORKER__interface);

        if (iface) {
            host_worker.iface = iface;
            host_worker.dsp_handle = lilv_instance_get_handle(instance);
            host_worker.requests  = lv2_ringbuffer_create(8192);
            host_worker.responses = lv2_ringbuffer_create(8192);
            host_worker.response_buffer.resize(8192);
            host_worker.running.store(true);
            host_worker.worker_thread =
                std::thread(worker_thread_func, &host_worker);
        }

        for (auto& p : ports) {
            if (p.is_audio) continue;
            if (p.is_control)
                lilv_instance_connect_port(instance, p.index, &p.control);
            if (p.is_atom)
                lilv_instance_connect_port(instance, p.index, p.atom);
        }
        lilv_instance_activate(instance);
        return true;
    }

    const char* plugin_uri = nullptr;

    LilvWorld* world = nullptr;
    const LilvPlugins* plugs =  nullptr;
    const LilvPlugin* plugin = nullptr;
    LilvInstance* instance = nullptr;

    LilvNode *audio_class = nullptr;
    LilvNode *control_class = nullptr;
    LilvNode *atom_class = nullptr;
    LilvNode *input_class = nullptr;
    LilvNode *rsz_minimumSize = nullptr;

    uint32_t max_block_length = 4096;
    uint32_t required_atom_size = 8192;

    std::atomic<bool> lilv_is_inited{false};
    std::atomic<bool> shutdown{false};

    LV2_State_Map_Path map_path;
    LV2_State_Make_Path make_path;
    LV2_State_Free_Path free_path;

    LV2HostWorker host_worker;
    std::vector<Port> ports;

    std::shared_ptr<oboe::AudioStream> audio_stream;
    std::unique_ptr<float[]> left_channel;
    std::unique_ptr<float[]> right_channel;
    int32_t channel_capacity = 0;
};

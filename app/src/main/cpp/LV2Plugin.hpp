/*
 * LV2Plugin.hpp
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Generic LV2 Plugin Management - Backend Agnostic
 *
 * This header provides a generic, backend-agnostic abstraction for loading,
 * instantiating, and managing LV2 plugins. It handles:
 * - Plugin discovery and instantiation via Lilv
 * - Control port management (float, toggle, trigger)
 * - Atom port communication (UI↔DSP via lock-free ringbuffers)
 * - Worker thread support for non-RT plugin tasks
 * - URID mapping and LV2 feature negotiation
 * - State save/load via Lilv
 *
 * Designed for backends (JACK, Oboe, etc.) to inherit or embed.
 */

#pragma once

#if defined(__ANDROID__)
    #include "logging_macros.h"
#endif
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
#include <lv2/midi/midi.h>

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <variant>

#include "json.hpp"
using json = nlohmann::json;
// ============================================================================
// PluginControl - Abstract Base Class & Factory
// ============================================================================

class PluginControl {
public:
    virtual ~PluginControl() = default;

    enum class Type {
        ControlFloat,
        Toggle,
        Trigger,
        AtomPort
    };

    virtual void setValue(const std::variant<float, bool, std::vector<uint8_t>>& value) = 0;
    virtual std::variant<float, bool, std::vector<uint8_t>> getValue() const = 0;
    virtual Type getType() const = 0;
    virtual const char* getSymbol() const = 0;
    virtual const LilvPort* getPort() const = 0;
    virtual void reset() = 0;

    // Factory: caller owns returned pointer
    static PluginControl* create(LilvWorld* world, const LilvPlugin* plugin,
                                  const LilvPort* port, const LilvNode* audio_class,
                                  const LilvNode* control_class, const LilvNode* atom_class);
};

// ============================================================================
// ControlPortFloat - Float control with min/max/default
// ============================================================================

class ControlPortFloat : public PluginControl {
public:
    ControlPortFloat(LilvWorld* world, const LilvPlugin* plugin,
                     const LilvPort* port)
        : port_(port), value_(0.0f), defvalue_(0.0f), minval_(0.0f), maxval_(1.0f) {
        
        // Extract port symbol
        const LilvNode* sym = lilv_port_get_symbol(plugin, port);
        symbol_ = sym ? lilv_node_as_string(sym) : "";
        
        // Extract min/max/default
        LilvNode *pmin, *pmax, *pdflt;
        lilv_port_get_range(plugin, port, &pdflt, &pmin, &pmax);
        
        if (pmin) {
            minval_ = lilv_node_as_float(pmin);
            lilv_node_free(pmin);
        }
        if (pmax) {
            maxval_ = lilv_node_as_float(pmax);
            lilv_node_free(pmax);
        }
        if (pdflt) {
            defvalue_ = lilv_node_as_float(pdflt);
            lilv_node_free(pdflt);
        }
        
        value_ = defvalue_;
    }
    
    void setValue(const std::variant<float, bool, std::vector<uint8_t>>& val) override {
        try {
            float fval = std::get<float>(val);
            value_ = std::clamp(fval, minval_, maxval_);
        } catch (const std::bad_variant_access&) {
            // Type mismatch, ignore
        }
    }
    
    std::variant<float, bool, std::vector<uint8_t>> getValue() const override {
        return value_;
    }
    
    Type getType() const override { return Type::ControlFloat; }
    const char* getSymbol() const override { return symbol_.c_str(); }
    const LilvPort* getPort() const override { return port_; }
    void reset() override { value_ = defvalue_; }
    
    float* getValuePtr() { return &value_; }

private:
    const LilvPort* port_;
    float value_, defvalue_, minval_, maxval_;
    std::string symbol_;
};

// ============================================================================
// ToggleControl - Boolean control
// ============================================================================

class ToggleControl : public PluginControl {
public:
    ToggleControl(LilvWorld* world, const LilvPlugin* plugin,
                  const LilvPort* port)
        : port_(port), value_(false), defvalue_(false) {
        
        const LilvNode* sym = lilv_port_get_symbol(plugin, port);
        symbol_ = sym ? lilv_node_as_string(sym) : "";
        
        LilvNode *pmin, *pmax, *pdflt;
        lilv_port_get_range(plugin, port, &pdflt, &pmin, &pmax);
        if (pdflt) {
            defvalue_ = lilv_node_as_float(pdflt) > 0.5f;
            lilv_node_free(pdflt);
        }
        if (pmin) lilv_node_free(pmin);
        if (pmax) lilv_node_free(pmax);
        
        value_ = defvalue_;
    }
    
    void setValue(const std::variant<float, bool, std::vector<uint8_t>>& val) override {
        try {
            value_ = std::get<bool>(val);
        } catch (const std::bad_variant_access&) {
            try {
                value_ = std::get<float>(val) > 0.5f;
            } catch (const std::bad_variant_access&) {
                // Type mismatch, ignore
            }
        }
    }
    
    std::variant<float, bool, std::vector<uint8_t>> getValue() const override {
        return value_;
    }
    
    Type getType() const override { return Type::Toggle; }
    const char* getSymbol() const override { return symbol_.c_str(); }
    const LilvPort* getPort() const override { return port_; }
    void reset() override { value_ = defvalue_; }
    
    float getAsFloat() const { return value_ ? 1.0f : 0.0f; }

private:
    const LilvPort* port_;
    bool value_, defvalue_;
    std::string symbol_;
};

// ============================================================================
// TriggerControl - Momentary impulse control
// ============================================================================

class TriggerControl : public PluginControl {
public:
    TriggerControl(LilvWorld* world, const LilvPlugin* plugin,
                   const LilvPort* port)
        : port_(port), armed_(false) {
        
        const LilvNode* sym = lilv_port_get_symbol(plugin, port);
        symbol_ = sym ? lilv_node_as_string(sym) : "";
    }
    
    void setValue(const std::variant<float, bool, std::vector<uint8_t>>& val) override {
        try {
            armed_ = std::get<bool>(val);
        } catch (const std::bad_variant_access&) {
            try {
                armed_ = std::get<float>(val) > 0.5f;
            } catch (const std::bad_variant_access&) {
                // Type mismatch
            }
        }
    }
    
    std::variant<float, bool, std::vector<uint8_t>> getValue() const override {
        return armed_;
    }
    
    Type getType() const override { return Type::Trigger; }
    const char* getSymbol() const override { return symbol_.c_str(); }
    const LilvPort* getPort() const override { return port_; }
    void reset() override { armed_ = false; }
    
    bool isArmed() { return armed_; }
    float getAsFloat() const { return armed_ ? 1.0f : 0.0f; }

private:
    const LilvPort* port_;
    bool armed_;
    std::string symbol_;
};

// ============================================================================
// AtomState - Shared atom communication for UI↔DSP
// ============================================================================

struct AtomState {
    std::vector<uint8_t> ui_to_dsp;
    uint32_t ui_to_dsp_type = 0;
    std::atomic<bool> ui_to_dsp_pending{false};
    mutable std::mutex ui_to_dsp_mutex;
    lv2_ringbuffer_t* dsp_to_ui = nullptr;
    
    AtomState(size_t ringbuffer_size = 16384) {
        dsp_to_ui = lv2_ringbuffer_create(ringbuffer_size);
    }
    
    ~AtomState() {
        if (dsp_to_ui) lv2_ringbuffer_free(dsp_to_ui);
    }
};

// ============================================================================
// AtomPortControl - Variable-size atom port with ringbuffer communication
// ============================================================================

class AtomPortControl : public PluginControl {
public:
    AtomPortControl(LilvWorld* world, const LilvPlugin* plugin,
                    const LilvPort* port)
        : port_(port) {
        
        const LilvNode* sym = lilv_port_get_symbol(plugin, port);
        symbol_ = sym ? lilv_node_as_string(sym) : "";
        
        // Create shared atom state
        atom_state_ = new AtomState();
    }
    
    ~AtomPortControl() override {
        delete atom_state_;
    }
    
    void setValue(const std::variant<float, bool, std::vector<uint8_t>>& val) override {
        try {
            auto data = std::get<std::vector<uint8_t>>(val);
            // TODO: how to get type? For now, store data and wait for caller to set type
            {
                std::lock_guard<std::mutex> lock(atom_state_->ui_to_dsp_mutex);
                atom_state_->ui_to_dsp = std::move(data);
            }
            atom_state_->ui_to_dsp_pending.store(true, std::memory_order_release);
        } catch (const std::bad_variant_access&) {
            // Type mismatch, ignore
        }
    }
    
    std::variant<float, bool, std::vector<uint8_t>> getValue() const override {
        std::lock_guard<std::mutex> lock(atom_state_->ui_to_dsp_mutex);
        return atom_state_->ui_to_dsp;
    }
    
    Type getType() const override { return Type::AtomPort; }
    const char* getSymbol() const override { return symbol_.c_str(); }
    const LilvPort* getPort() const override { return port_; }
    void reset() override {
        std::lock_guard<std::mutex> lock(atom_state_->ui_to_dsp_mutex);
        atom_state_->ui_to_dsp.clear();
    }
    
    AtomState* getAtomState() { return atom_state_; }
    void setMessageType(uint32_t type_urid) { atom_state_->ui_to_dsp_type = type_urid; }

private:
    const LilvPort* port_;
    AtomState* atom_state_;
    std::string symbol_;
};

// ============================================================================
// LV2Plugin - Generic LV2 Plugin Manager
// ============================================================================
 
class LV2Plugin {
public:
    json writables ;

    void send_path_parameter(const char* property_uri, const char* abs_path) {
        if (!property_uri || !*property_uri || !abs_path || !*abs_path) {
            LOGE("send_path_parameter: invalid property_uri or abs_path");
            return;
        }

        // Prefer a real filesystem path here, not a content:// URI.
        if (abs_path[0] != '/') {
            LOGE("send_path_parameter: expected absolute filesystem path, got: %s", abs_path);
            return;
        }

        // Find one atom input port suitable for control messages.
        Port* target = nullptr;
        for (auto& p : ports_) {
            if (p.is_atom && p.is_input && p.atom_state && !p.is_midi) {
                target = &p;
                break;
            }
        }

        if (!target) {
            LOGE("send_path_parameter: no suitable atom input port found");
            return;
        }

        const LV2_URID property_urid = map_uri(property_uri);

        // Enough room for object header + property/value atoms + path string.
        std::vector<uint8_t> forge_buf(512 + std::strlen(abs_path));
        LV2_Atom_Forge forge;
        LV2_Atom_Forge_Frame frame;

        lv2_atom_forge_init(&forge, &um_);
        lv2_atom_forge_set_buffer(&forge, forge_buf.data(), (uint32_t)forge_buf.size());

        if (!lv2_atom_forge_object(&forge, &frame, 0, urids_.patch_Set)) {
            LOGE("send_path_parameter: failed to forge patch:Set header");
            return;
        }

        lv2_atom_forge_key(&forge, urids_.patch_property);
        if (!lv2_atom_forge_urid(&forge, property_urid)) {
            LOGE("send_path_parameter: failed to forge patch:property");
            return;
        }

        lv2_atom_forge_key(&forge, urids_.patch_value);
        if (!lv2_atom_forge_path(&forge, abs_path, (uint32_t)std::strlen(abs_path))) {
            LOGE("send_path_parameter: failed to forge patch:value path");
            return;
        }

        lv2_atom_forge_pop(&forge, &frame);

        const LV2_Atom* atom = reinterpret_cast<const LV2_Atom*>(forge_buf.data());
        if (!atom || atom->type != urids_.atom_Object || atom->size == 0) {
            LOGE("send_path_parameter: forged atom is invalid");
            return;
        }

        // process() wraps queued bytes as the event body, so store only the atom body.
        const uint8_t* body = reinterpret_cast<const uint8_t*>(LV2_ATOM_BODY(atom));
        {
            std::lock_guard<std::mutex> lock(target->atom_state->ui_to_dsp_mutex);
            target->atom_state->ui_to_dsp.assign(body, body + atom->size);
            target->atom_state->ui_to_dsp_type = atom->type;  // atom:Object
        }
        target->atom_state->ui_to_dsp_pending.store(true, std::memory_order_release);
        LOGD("send_path_parameter: sent path '%s' as atom:Object with property '%s' (URID %u) to port '%s'",
             abs_path, property_uri, property_urid, target->symbol.c_str());

        writables[property_uri] = abs_path;
    }


    void send_filename_to_plugin(const char* filename, const char * uri ) {
        send_path_parameter(uri, filename);
    }

    bool enabled = true;
    // Constructor: caller provides discovered Lilv world and plugin
    LV2Plugin(LilvWorld* world, LilvPlugin* plugin, double sample_rate, uint32_t max_block_length)
        : world_(world), plugin_(plugin), sample_rate_(sample_rate),
          max_block_length_(max_block_length), instance_(nullptr),
          audio_class_(nullptr), control_class_(nullptr), atom_class_(nullptr),
          input_class_(nullptr), rsz_minimumSize_(nullptr),
          required_atom_size_(8192), shutdown_(false) {
    }

    // Constructor: resolve plugin by URI from an existing Lilv world
    LV2Plugin(LilvWorld* world, const char* plugin_uri, double sample_rate, uint32_t max_block_length)
        : world_(world), plugin_(nullptr), sample_rate_(sample_rate),
          max_block_length_(max_block_length), instance_(nullptr),
          audio_class_(nullptr), control_class_(nullptr), atom_class_(nullptr),
          input_class_(nullptr), rsz_minimumSize_(nullptr),
          required_atom_size_(8192), shutdown_(false) {
        if (world_ && plugin_uri) {
            const LilvPlugins* plugins = lilv_world_get_all_plugins(world_);
            LilvNode* uri = lilv_new_uri(world_, plugin_uri);
            if (uri) {
                plugin_ = const_cast<LilvPlugin*>(lilv_plugins_get_by_uri(plugins, uri));
                lilv_node_free(uri);
            }
        }
    }

    ~LV2Plugin() {
        // closePlugin();
    }

    // Initialize plugin: discover ports, create controls, instantiate instance
    bool initialize() {
        if (!plugin_) {
            LOGE("Plugin is null, cannot initialize");
            return false;
        }

        if (!world_) {
            LOGE("Lilv world is null, cannot initialize plugin");
            return false;
        }

        if (!world_ || !plugin_) return false;

        // Create port class nodes
        audio_class_ = lilv_new_uri(world_, LV2_CORE__AudioPort);
        control_class_ = lilv_new_uri(world_, LV2_CORE__ControlPort);
        atom_class_ = lilv_new_uri(world_, LV2_ATOM__AtomPort);
        input_class_ = lilv_new_uri(world_, LV2_CORE__InputPort);
        rsz_minimumSize_ = lilv_new_uri(world_, LV2_RESIZE_PORT__minimumSize);

        init_urids();
        init_features();
        
        if (!check_resize_port_requirements()) {
            LOGE("Plugin requires resize-port feature but does not meet requirements");
            return false;
        }
        if (!init_ports()) {
            LOGE("Failed to initialize plugin ports and controls");
            return false;
        }

        if (!init_instance()) {
            LOGE("Failed to instantiate plugin");
            return false;
        }

        return true;
    }

    // Lifecycle
    void start() {
        shutdown_.store(false, std::memory_order_release);
        if (instance_) lilv_instance_activate(instance_);
    }

    void stop() {
        shutdown_.store(true, std::memory_order_release);
        if (instance_) lilv_instance_deactivate(instance_);
    }

    void closePlugin() {
        IN

        const bool was_closed = closed_.exchange(true, std::memory_order_acq_rel);
        if (was_closed) {
            LOGD("closePlugin: plugin already closed, skipping duplicate cleanup");
            OUT
            return;
        }

        shutdown_.store(true, std::memory_order_release);
        stop_worker();

        if (instance_) {
            lilv_instance_deactivate(instance_);
            lilv_instance_free(instance_);
            instance_ = nullptr;
        }

        // Free port buffers and controls
        for (auto& p : ports_) {
            if (p.atom) free(p.atom);
            delete p.atom_state;
        }
        ports_.clear();
        
        for (auto* control : controls_) {
            delete control;
        }
        controls_.clear();

        // Free Lilv nodes (but NOT world or plugin—caller owns those)
        if (audio_class_) {
            lilv_node_free(audio_class_);
            audio_class_ = nullptr;
        }
        if (control_class_) {
            lilv_node_free(control_class_);
            control_class_ = nullptr;
        }
        if (atom_class_) {
            lilv_node_free(atom_class_);
            atom_class_ = nullptr;
        }
        if (input_class_) {
            lilv_node_free(input_class_);
            input_class_ = nullptr;
        }
        if (rsz_minimumSize_) {
            lilv_node_free(rsz_minimumSize_);
            rsz_minimumSize_ = nullptr;
        }
        OUT
    }

    // RT-safe audio processing with atom message handling
    bool process(float* inputBuffer, float* outputBuffer, int numFrames) {
        if (!enabled) {
            // If plugin is bypassed, just copy input to output
            if (inputBuffer && outputBuffer && numFrames > 0) {
                std::memcpy(outputBuffer, inputBuffer, sizeof(float) * numFrames);
            }
            return true;
        }

        if (shutdown_.load(std::memory_order_acquire) || !instance_)
            return false;

        if (!inputBuffer || !outputBuffer || numFrames <= 0)
            return false;

        // --- Step A: Connect audio port buffers ---
        uint32_t input_index = 0, output_index = 0;
        for (auto& p : ports_) {
            if (!p.is_audio) continue;
            
            float* target = inputBuffer;
            if (!p.is_input) target = outputBuffer;
            lilv_instance_connect_port(instance_, p.index, target);
        }

        // --- Step B: Process incoming UI→DSP atom messages ---
        for (auto& p : ports_) {
            if (!p.is_atom || !p.is_input) continue;
            if (!p.atom_state || !p.atom) continue;
            
            // Check for pending UI message
            if (p.atom_state->ui_to_dsp_pending.exchange(false, std::memory_order_acquire)) {
                // Wrap UI data in LV2_Atom_Event and append to sequence
                p.atom->atom.type = urids_.atom_Sequence;
                p.atom->atom.size = sizeof(LV2_Atom_Sequence_Body);
                p.atom->body.unit = 0;
                p.atom->body.pad = 0;

                std::vector<uint8_t> payload;
                uint32_t message_type = 0;
                {
                    std::lock_guard<std::mutex> lock(p.atom_state->ui_to_dsp_mutex);
                    payload = p.atom_state->ui_to_dsp;
                    message_type = p.atom_state->ui_to_dsp_type;
                }

                const uint32_t body_size = static_cast<uint32_t>(payload.size());
                const uint32_t event_size = static_cast<uint32_t>(sizeof(LV2_Atom_Event)) + body_size;
                const uint32_t max_event_space =
                    (p.atom_buf_size > sizeof(LV2_Atom_Sequence_Body))
                        ? (p.atom_buf_size - sizeof(LV2_Atom_Sequence_Body))
                        : 0;

                if (event_size > max_event_space) {
                    LOGE("process: dropping oversized UI->DSP atom event (port=%s, event=%u, max=%u)",
                         p.symbol.c_str(), event_size, max_event_space);
                    continue;
                }

                std::vector<uint8_t> evbuf(event_size);
                LV2_Atom_Event* ev = reinterpret_cast<LV2_Atom_Event*>(evbuf.data());
                
                ev->time.frames = 0;
                ev->body.type = message_type;
                ev->body.size = body_size;
                if (body_size > 0 && !payload.empty()) {
                    memcpy((uint8_t*)LV2_ATOM_BODY(&ev->body), payload.data(), body_size);
                }
                
                if (!lv2_atom_sequence_append_event(p.atom, p.atom_buf_size, ev)) {
                    LOGE("process: failed to append UI->DSP atom event (port=%s, body=%u)",
                         p.symbol.c_str(), body_size);
                }
            }
        }

        // --- Step C: Run plugin ---
        lilv_instance_run(instance_, numFrames);

        // --- Step D: Deliver worker responses and finalize worker cycle ---
        if (host_worker_.enabled.load(std::memory_order_acquire)) {
            host_worker_.rt_readers.fetch_add(1, std::memory_order_acq_rel);

            if (host_worker_.enabled.load(std::memory_order_acquire)) {
                const LV2_Worker_Interface* iface = host_worker_.iface.load(std::memory_order_acquire);
                if (iface) {
                    deliver_worker_responses();
                    if (iface->end_run) {
                        iface->end_run(host_worker_.dsp_handle);
                    }
                }
            }

            host_worker_.rt_readers.fetch_sub(1, std::memory_order_acq_rel);
        }

        // --- Step E: Read outgoing DSP→UI atom messages ---
        for (auto& p : ports_) {
            // Reset input atom port for next cycle
            if (p.is_atom && p.is_input) {
                p.atom->atom.type = urids_.atom_Sequence;
                p.atom->atom.size = sizeof(LV2_Atom_Sequence_Body);
                p.atom->body.unit = 0;
                p.atom->body.pad = 0;
            }

            // Copy output atoms to ringbuffer
            if (p.is_atom && !p.is_input) {
                LV2_Atom_Sequence* seq = p.atom;
                LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
                    if (ev->body.size == 0) break;
                    if (seq->atom.type == 0) break;
                    
                    const uint32_t total = sizeof(LV2_Atom) + ev->body.size;
                    if (lv2_ringbuffer_write_space(p.atom_state->dsp_to_ui) >= total) {
                        lv2_ringbuffer_write(p.atom_state->dsp_to_ui,
                                           (const char*)&ev->body, total);
                    }
                }
                
                // Reset output buffer for next process cycle
                p.atom->atom.type = 0;
                p.atom->atom.size = required_atom_size_;
            }
        }

        return true;
    }

    // Control access
    PluginControl* getControl(const char* symbol) {
        for (auto* control : controls_) {
            if (std::string(control->getSymbol()) == symbol)
                return control;
        }
        return nullptr;
    }

    uint32_t getPortCount() const { return ports_.size(); }
    const LilvPort* getPort(uint32_t index) const {
        if (index >= ports_.size()) return nullptr;
        return ports_[index].lilv_port;
    }

    // Get ringbuffer for reading DSP→UI atoms
    lv2_ringbuffer_t* getAtomOutputRingbuffer(const char* portSymbol) {
        for (auto& p : ports_) {
            if (!p.is_atom || p.is_input) continue;
            const LilvNode* sym = lilv_port_get_symbol(plugin_, p.lilv_port);
            if (sym && std::string(lilv_node_as_string(sym)) == portSymbol) {
                return p.atom_state->dsp_to_ui;
            }
        }
        return nullptr;
    }

    // Helper to read one atom message from ringbuffer. Returns total bytes copied.
    static size_t readAtomMessage(lv2_ringbuffer_t* rb, uint8_t* outBuffer, size_t maxSize) {
        if (!rb || !outBuffer || maxSize < sizeof(LV2_Atom)) return 0;

        if (lv2_ringbuffer_read_space(rb) < sizeof(LV2_Atom)) return 0;

        LV2_Atom atom_header;
        lv2_ringbuffer_peek(rb, (char*)&atom_header, sizeof(LV2_Atom));

        // Guard against corrupted/unsupported payloads.
        if (atom_header.size > (16u * 1024u * 1024u)) {
            LOGE("readAtomMessage: atom payload too large (%u)", atom_header.size);
            return 0;
        }

        const uint32_t total = sizeof(LV2_Atom) + atom_header.size;
        if (total > maxSize || lv2_ringbuffer_read_space(rb) < total) return 0;

        lv2_ringbuffer_read(rb, (char*)outBuffer, total);
        return total;
    }

    // Read one message from an atom output port by symbol.
    bool readAtomMessage(const char* portSymbol, std::vector<uint8_t>& outMessage) {
        lv2_ringbuffer_t* rb = getAtomOutputRingbuffer(portSymbol);
        if (!rb) return false;

        if (lv2_ringbuffer_read_space(rb) < sizeof(LV2_Atom)) return false;

        LV2_Atom atom_header;
        lv2_ringbuffer_peek(rb, (char*)&atom_header, sizeof(LV2_Atom));
        const uint32_t total = sizeof(LV2_Atom) + atom_header.size;

        if (atom_header.size > (16u * 1024u * 1024u)) {
            LOGE("readAtomMessage(port): atom payload too large (%u)", atom_header.size);
            return false;
        }

        if (lv2_ringbuffer_read_space(rb) < total) return false;

        outMessage.resize(total);
        const size_t read = readAtomMessage(rb, outMessage.data(), outMessage.size());
        if (read == 0) {
            outMessage.clear();
            return false;
        }

        outMessage.resize(read);
        return true;
    }

    bool extractPathFromAtomMessage(const uint8_t* msg,
                                    size_t msgSize,
                                    std::string& outPath,
                                    std::string* outPropertyUri = nullptr) const {
        if (!msg || msgSize < sizeof(LV2_Atom)) return false;

        const auto* atom = reinterpret_cast<const LV2_Atom*>(msg);
        const size_t total = sizeof(LV2_Atom) + atom->size;
        if (total > msgSize) return false;

        // Expected for patch:Set messages
        if (atom->type != urids_.atom_Object) return false;

        const auto* obj = reinterpret_cast<const LV2_Atom_Object*>(atom);
        if (obj->body.otype != urids_.patch_Set) return false;

        const LV2_Atom* property = nullptr;
        const LV2_Atom* value = nullptr;

        lv2_atom_object_get(
                obj,
                urids_.patch_property, &property,
                urids_.patch_value, &value,
                0);

        if (!property || !value) return false;
        if (property->type != urids_.atom_URID) return false;

        // Optional: decode property URI
        if (outPropertyUri) {
            const auto* p = reinterpret_cast<const LV2_Atom_URID*>(property);
            const char* propUri = unm_.unmap(unm_.handle, p->body);
            *outPropertyUri = propUri ? propUri : "";
        }

        // Path is usually atom:Path, but some plugins may use atom:String
        if (value->type != urids_.atom_Path && value->type != urids_.atom_String) {
            return false;
        }

        const char* s = reinterpret_cast<const char*>(LV2_ATOM_BODY(value));
        const size_t maxLen = value->size;               // includes NUL when forged as string/path
        const size_t n = strnlen(s, maxLen);             // safe if missing NUL
        outPath.assign(s, n);

        return true;
    }

    // Drain up to maxMessages from one atom output port.
    size_t readAtomMessages(const char* portSymbol,
                            std::vector<std::vector<uint8_t>>& outMessages,
                            size_t maxMessages = 64) {
        if (!portSymbol || maxMessages == 0) return 0;

        size_t count = 0;
        std::vector<uint8_t> msg;
        while (count < maxMessages && readAtomMessage(portSymbol, msg)) {
            outMessages.push_back(msg);
            ++count;
        }

        return count;
    }

    // State management
    bool saveState(const std::string& filePath) {
        if (!instance_ || !plugin_) return false;
        
        LilvState* state = lilv_state_new_from_instance(plugin_, instance_,
                                                        &um_, reinterpret_cast<const char *>(&unm_), nullptr, nullptr,
                                                        nullptr,
                                                        reinterpret_cast<LilvGetPortValueFunc>(set_port_value), this,
                                                        0, nullptr);
        if (!state) return false;
        
        int result = lilv_state_save(world_, &um_, &unm_, state, filePath.c_str(), nullptr, nullptr);
        lilv_state_free(state);
        
        return result == 0;
    }

    bool loadState(const std::string& filePath) {
        if (!instance_) return false;
        
        LilvState* state = lilv_state_new_from_file(world_, &um_, nullptr, filePath.c_str());
        if (!state) return false;
        
        LV2_Feature* feats[] = { &features_.um_f, &features_.unm_f,
                                &features_.map_path_feature,
                                &features_.make_path_feature,
                                &features_.free_path_feature,
                                nullptr };
        
        lilv_state_restore(state, instance_, set_port_value, this, 0, feats);
        lilv_state_free(state);
        
        return true;
    }

// ========== URID Mapping ==========
struct {
    LV2_URID atom_eventTransfer;
    LV2_URID atom_Sequence;
    LV2_URID atom_Object;
    LV2_URID atom_URID;
    LV2_URID atom_String;
    LV2_URID atom_Float;
    LV2_URID atom_Int;
    LV2_URID atom_Double;
    LV2_URID midi_Event;
    LV2_URID buf_maxBlock;
    LV2_URID buf_minBlock;
    LV2_URID buf_nominalBlock;
    LV2_URID buf_seqSize;
    LV2_URID atom_Path;
    LV2_URID patch_Get;
    LV2_URID patch_Set;
    LV2_URID patch_property;
    LV2_URID patch_value;
    LV2_URID atom_Blank;
    LV2_URID atom_Chunk;
    LV2_URID param_sampleRate;
} urids_;
private:

    void init_urids() {
        urids_.atom_eventTransfer = map_uri(LV2_ATOM__eventTransfer);
        urids_.atom_Sequence = map_uri(LV2_ATOM__Sequence);
        urids_.atom_Blank = map_uri(LV2_ATOM__Blank);
        urids_.atom_Chunk = map_uri(LV2_ATOM__Chunk);
        urids_.atom_Object = map_uri(LV2_ATOM__Object);
        urids_.atom_URID = map_uri(LV2_ATOM__URID);
        urids_.atom_String = map_uri(LV2_ATOM__String);
        urids_.atom_Float = map_uri(LV2_ATOM__Float);
        urids_.atom_Int = map_uri(LV2_ATOM__Int);
        urids_.atom_Double = map_uri(LV2_ATOM__Double);
        urids_.midi_Event = map_uri(LV2_MIDI__MidiEvent);
        urids_.buf_maxBlock     = map_uri(LV2_BUF_SIZE__maxBlockLength);
        urids_.buf_minBlock     = map_uri(LV2_BUF_SIZE__minBlockLength);
        urids_.buf_nominalBlock = map_uri(LV2_BUF_SIZE__nominalBlockLength);
        urids_.buf_seqSize      = map_uri(LV2_BUF_SIZE__sequenceSize);
        urids_.atom_Path = map_uri(LV2_ATOM__Path);
        urids_.patch_Get = map_uri(LV2_PATCH__Get);
        urids_.patch_Set = map_uri(LV2_PATCH__Set);
        urids_.patch_property = map_uri(LV2_PATCH__property);
        urids_.patch_value = map_uri(LV2_PATCH__value);
        urids_.param_sampleRate = map_uri(LV2_PARAMETERS__sampleRate);
    }

    static LV2_URID map_uri(LV2_URID_Map_Handle h, const char* uri) {
        auto* self = static_cast<LV2Plugin*>(h);
        auto it = self->urid_map_.find(uri);
        if (it != self->urid_map_.end()) return it->second;
        
        LV2_URID id = self->urid_map_.size() + 1;
        self->urid_map_[uri] = id;
        self->urid_unmap_[id] = uri;
        return id;
    }

    static const char* unmap_uri(LV2_URID_Unmap_Handle h, LV2_URID urid) {
        auto* self = static_cast<LV2Plugin*>(h);
        auto it = self->urid_unmap_.find(urid);
        if (it == self->urid_unmap_.end()) return nullptr;
        return it->second.c_str();
    }

    LV2_URID map_uri(const char* uri) {
        return map_uri((LV2_URID_Map_Handle)this, uri);
    }

    std::unordered_map<std::string, LV2_URID> urid_map_;
    std::unordered_map<LV2_URID, std::string> urid_unmap_;

public:
    LV2_URID_Map um_;
    LV2_URID_Unmap unm_;
private:
    // ========== LV2 Features ==========
    struct {
        LV2_Feature um_f;
        LV2_Feature unm_f;
        LV2_Feature map_path_feature;
        LV2_Feature make_path_feature;
        LV2_Feature free_path_feature;
        LV2_Feature bbl_feature;
    } features_;

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
        um_.handle = this;
        um_.map = map_uri;
        unm_.handle = this;
        unm_.unmap = unmap_uri;

        map_path_.handle = nullptr;
        map_path_.abstract_path = map_path_func;
        make_path_.handle = nullptr;
        make_path_.path = make_path_func;
        free_path_.handle = nullptr;
        free_path_.free_path = free_path_func;

        features_.bbl_feature.URI = LV2_BUF_SIZE__boundedBlockLength;
        features_.bbl_feature.data = nullptr;

        features_.um_f.URI = LV2_URID__map;
        features_.um_f.data = &um_;

        features_.unm_f.URI = LV2_URID__unmap;
        features_.unm_f.data = &unm_;

        features_.map_path_feature.URI = LV2_STATE__mapPath;
        features_.map_path_feature.data = &map_path_;

        features_.make_path_feature.URI = LV2_STATE__makePath;
        features_.make_path_feature.data = &make_path_;

        features_.free_path_feature.URI = LV2_STATE__freePath;
        features_.free_path_feature.data = &free_path_;

        host_worker_.schedule.handle = &host_worker_;
        host_worker_.schedule.schedule_work = host_schedule_work;
        host_worker_.feature.URI = LV2_WORKER__schedule;
        host_worker_.feature.data = &host_worker_.schedule;
    }

    LV2_State_Map_Path map_path_;
    LV2_State_Make_Path make_path_;
    LV2_State_Free_Path free_path_;

    static void set_port_value(const char* port_symbol, void* user_data,
                               const void* value, uint32_t size, uint32_t type) {
        auto* self = static_cast<LV2Plugin*>(user_data);
        for (auto& p : self->ports_) {
            const LilvNode* sym = lilv_port_get_symbol(self->plugin_, p.lilv_port);
            if (sym && std::string(lilv_node_as_string(sym)) == port_symbol) {
                if (p.is_control && size == sizeof(float)) {
                    p.control = *(const float*)value;
                    lilv_instance_connect_port(self->instance_, p.index, &p.control);
                }
                break;
            }
        }
    }

    // ========== Feature Support Checking ==========
    bool feature_is_supported(const char* uri, const LV2_Feature*const* f) {
        for (; *f; ++f)
            if (!strcmp(uri, (*f)->URI)) return true;
        return false;
    }

    bool checkFeatures(const LV2_Feature*const* feat) {
        LilvNodes* requests = lilv_plugin_get_required_features(plugin_);
        LILV_FOREACH(nodes, f, requests) {
            const char* uri = lilv_node_as_uri(lilv_nodes_get(requests, f));
            if (!feature_is_supported(uri, feat)) {
                LOGE("Plugin requires unsupported feature: %s", uri);
                lilv_nodes_free(requests);
                return false;
            }
        }
        lilv_nodes_free(requests);
        return true;
    }

    bool check_resize_port_requirements() {
        uint32_t n = lilv_plugin_get_num_ports(plugin_);
        LilvNode* min_size = lilv_new_uri(world_, LV2_RESIZE_PORT__minimumSize);

        for (uint32_t i = 0; i < n; ++i) {
            const LilvPort* port = lilv_plugin_get_port_by_index(plugin_, i);
            if (!lilv_port_is_a(plugin_, port, atom_class_)) continue;
            
            LilvNodes* sizes = lilv_port_get_value(plugin_, port, min_size);
            if (!sizes || lilv_nodes_size(sizes) == 0) continue;
            
            const LilvNode* n = lilv_nodes_get_first(sizes);
            uint32_t required = lilv_node_as_int(n);
            required_atom_size_ = std::max(required_atom_size_, required);
            lilv_nodes_free(sizes);
        }
        lilv_node_free(min_size);
        return true;
    }

    // ========== Port Initialization ==========
    bool init_ports() {
        uint32_t n = lilv_plugin_get_num_ports(plugin_);
        ports_.reserve(n);

        LilvNode* midi_event = lilv_new_uri(world_, LV2_MIDI__MidiEvent);

        for (uint32_t i = 0; i < n; ++i) {
            const LilvPort* lp = lilv_plugin_get_port_by_index(plugin_, i);
            Port p;
            p.name = lilv_node_as_string(lilv_port_get_name(plugin_, lp));
            const LilvNode* sym = lilv_port_get_symbol(plugin_, lp);
            p.symbol = sym ? lilv_node_as_string(sym) : "";
            p.index = i;
            p.lilv_port = lp;
            p.is_audio = lilv_port_is_a(plugin_, lp, audio_class_);
            p.is_control = lilv_port_is_a(plugin_, lp, control_class_);
            p.is_atom = lilv_port_is_a(plugin_, lp, atom_class_);
            p.is_input = lilv_port_is_a(plugin_, lp, input_class_);
            p.is_midi = lilv_port_supports_event(plugin_, lp, midi_event);
            p.control = 0.0f;
            p.defvalue = 0.0f;
            p.atom = nullptr;
            p.atom_state = nullptr;

            // Allocate and initialize atom ports
            if (p.is_atom) {
                p.atom_buf_size = required_atom_size_;
                p.atom = (LV2_Atom_Sequence*)aligned_alloc(64, p.atom_buf_size);
                memset(p.atom, 0, p.atom_buf_size);
                p.atom->atom.type = urids_.atom_Sequence;

                if (p.is_input) {
                    p.atom->atom.size = sizeof(LV2_Atom_Sequence_Body);
                    p.atom->body.unit = 0;
                    p.atom->body.pad = 0;
                } else {
                    p.atom->atom.size = 0;
                }

                p.atom_state = new AtomState();
            }

            // Extract default values for control inputs
            if (p.is_control && p.is_input) {
                LilvNode *pdflt, *pmin, *pmax;
                lilv_port_get_range(plugin_, lp, &pdflt, &pmin, &pmax);
                if (pmin) lilv_node_free(pmin);
                if (pmax) lilv_node_free(pmax);
                if (pdflt) {
                    p.defvalue = lilv_node_as_float(pdflt);
                    lilv_node_free(pdflt);
                }
                p.control = p.defvalue;
            }

            ports_.push_back(p);

            // Create PluginControl instance for control/atom ports
            if (p.is_control || p.is_atom) {
                PluginControl* control = PluginControl::create(world_, plugin_, lp,
                                                                audio_class_, control_class_, atom_class_);
                if (control) controls_.push_back(control);
            }
        }

        lilv_node_free(midi_event);
        return true;
    }

    struct Port {
        uint32_t index = 0;
        std::string symbol, name;
        const LilvPort* lilv_port = nullptr;
        bool is_audio = false, is_input = false, is_control = false;
        bool is_atom = false, is_midi = false;

        float control = 0.0f, defvalue = 0.0f;
        LV2_Atom_Sequence* atom = nullptr;
        uint32_t atom_buf_size = 8192;
        AtomState* atom_state = nullptr;
    };

    // ========== Plugin Instantiation ==========
    bool init_instance() {
        LV2_Options_Option options[] = {
            { LV2_OPTIONS_INSTANCE, 0, urids_.buf_minBlock,     sizeof(uint32_t), urids_.atom_Int, &max_block_length_ },
            { LV2_OPTIONS_INSTANCE, 0, urids_.buf_maxBlock,     sizeof(uint32_t), urids_.atom_Int, &max_block_length_ },
            { LV2_OPTIONS_INSTANCE, 0, urids_.buf_nominalBlock, sizeof(uint32_t), urids_.atom_Int, &max_block_length_ },
            { LV2_OPTIONS_INSTANCE, 0, urids_.buf_seqSize,      sizeof(uint32_t), urids_.atom_Int, &required_atom_size_ },
            { LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr }
        };

        LV2_Feature opt_f { LV2_OPTIONS__options, options };

        // Pre-allocate ringbuffers BEFORE plugin instantiation (Fix #3)
        // This ensures feature is valid if plugin calls schedule_work during init
        lv2_ringbuffer_t* ring_req = lv2_ringbuffer_create(8192);
        lv2_ringbuffer_t* ring_resp = lv2_ringbuffer_create(8192);
        
        if (ring_req && ring_resp) {
            host_worker_.requests.store(ring_req, std::memory_order_release);
            host_worker_.responses.store(ring_resp, std::memory_order_release);
        } else {
            LOGE("Failed to pre-allocate worker ringbuffers; worker support disabled");
            if (ring_req) lv2_ringbuffer_free(ring_req);
            if (ring_resp) lv2_ringbuffer_free(ring_resp);
        }

        LV2_Feature* feats[] = { &features_.um_f, &features_.unm_f, &opt_f,
                    &features_.bbl_feature, &features_.map_path_feature,
                    &features_.make_path_feature, &features_.free_path_feature,
                    &host_worker_.feature, nullptr };

        if (!checkFeatures(feats)) {
            LOGE("Plugin requires unsupported features, cannot instantiate");
            return false;
        }

        instance_ = lilv_plugin_instantiate(plugin_, sample_rate_, feats);
        if (!instance_) {
            HERE LOGE("Failed to instantiate plugin");
            return false;
        }

        // Setup worker if plugin provides interface
        const LV2_Worker_Interface* iface = (const LV2_Worker_Interface*)
            lilv_instance_get_extension_data(instance_, LV2_WORKER__interface);

        if (iface) {
            lv2_ringbuffer_t* req = host_worker_.requests.load(std::memory_order_acquire);
            lv2_ringbuffer_t* resp = host_worker_.responses.load(std::memory_order_acquire);
            
            if (req && resp) {
                // Ringbuffers were pre-allocated, now enable worker
                host_worker_.dsp_handle = lilv_instance_get_handle(instance_);
                host_worker_.iface.store(iface, std::memory_order_release);
                host_worker_.running.store(true, std::memory_order_release);
                host_worker_.worker_thread = std::thread(worker_thread_func, &host_worker_);
                host_worker_.enabled.store(true, std::memory_order_release);
                host_worker_.response_buffer.resize(8192);
            } else {
                LOGE("Worker ringbuffers not available; worker support disabled for this plugin instance");
                host_worker_.dsp_handle = nullptr;
                host_worker_.enabled.store(false, std::memory_order_release);
            }
        } else {
            // Plugin doesn't support worker interface; clear pre-allocated ringbuffers
            lv2_ringbuffer_t* req = host_worker_.requests.exchange(nullptr, std::memory_order_acq_rel);
            lv2_ringbuffer_t* resp = host_worker_.responses.exchange(nullptr, std::memory_order_acq_rel);
            if (req) lv2_ringbuffer_free(req);
            if (resp) lv2_ringbuffer_free(resp);
        }

        // Connect control and atom ports
        for (auto& p : ports_) {
            if (p.is_audio) continue;
            if (p.is_control) {
                lilv_instance_connect_port(instance_, p.index, &p.control);
//                LOGD ("[%s] Connected control port %u to value %f", lilv_node_as_string(lilv_plugin_get_name(plugin_)), p.index, p.control);
            }
            if (p.is_atom)
                lilv_instance_connect_port(instance_, p.index, p.atom);
            if (! p.is_atom and ! p.is_control) {
//                lilv_instance_connect_port(instance_, p.index, nullptr);
                LOGE ("[%s] Warning: Unconnected port %u (not control or atom)", lilv_node_as_string(lilv_plugin_get_name(plugin_)), p.index);
            }
        }

        lilv_instance_activate(instance_);
        return true;
    }

    // ========== Worker Thread ==========
    struct LV2HostWorker {
        std::atomic<lv2_ringbuffer_t*> requests{nullptr};
        std::atomic<lv2_ringbuffer_t*> responses{nullptr};

        LV2_Worker_Schedule schedule;
        LV2_Feature feature;
        std::atomic<const LV2_Worker_Interface*> iface{nullptr};
        LV2_Handle dsp_handle;

        std::atomic<bool> enabled{false};
        std::atomic<bool> running{false};
        std::atomic<uint32_t> rt_readers{0};
        std::atomic<uint32_t> scheduler_readers{0};
        std::atomic<uint32_t> responder_readers{0};
        std::thread worker_thread;

        std::vector<uint8_t> response_buffer;

        ~LV2HostWorker() {
            // Safety net: stop_worker() should have been called first, but guard against
            // a joinable thread reaching the destructor (e.g. race between init and close).
            running.store(false, std::memory_order_release);
            if (worker_thread.joinable()) {
                LOGE("LV2HostWorker: thread still joinable in destructor — joining now");
                worker_thread.join();
            }
            lv2_ringbuffer_t* req = requests.exchange(nullptr, std::memory_order_acq_rel);
            if (req) lv2_ringbuffer_free(req);
            lv2_ringbuffer_t* resp = responses.exchange(nullptr, std::memory_order_acq_rel);
            if (resp) lv2_ringbuffer_free(resp);
        }
    };

    static LV2_Worker_Status host_schedule_work(
        LV2_Worker_Schedule_Handle handle, uint32_t size, const void* data) {
        
        auto* w = (LV2HostWorker*)handle;
        if (!w) {
            return LV2_WORKER_ERR_NO_SPACE;
        }
        
        // Gate: plugin must not schedule work after shutdown begins
        if (!w->enabled.load(std::memory_order_acquire)) {
            return LV2_WORKER_ERR_NO_SPACE;
        }
        
        // Track incoming scheduler calls to prevent use-after-free race
        w->scheduler_readers.fetch_add(1, std::memory_order_acq_rel);
        
        // Load atomic pointer safely
        lv2_ringbuffer_t* req = w->requests.load(std::memory_order_acquire);
        if (!req) {
            w->scheduler_readers.fetch_sub(1, std::memory_order_acq_rel);
            return LV2_WORKER_ERR_NO_SPACE;
        }

        const size_t total = sizeof(uint32_t) + size;
        if (lv2_ringbuffer_write_space(req) < total) {
            w->scheduler_readers.fetch_sub(1, std::memory_order_acq_rel);
            return LV2_WORKER_ERR_NO_SPACE;
        }

        lv2_ringbuffer_write(req, (const char*)&size, sizeof(uint32_t));
        lv2_ringbuffer_write(req, (const char*)data, size);
        
        w->scheduler_readers.fetch_sub(1, std::memory_order_acq_rel);
        return LV2_WORKER_SUCCESS;
    }

    static void worker_thread_func(LV2HostWorker* w) {
        while (w->running.load(std::memory_order_acquire)) {
            lv2_ringbuffer_t* req = w->requests.load(std::memory_order_acquire);
            const LV2_Worker_Interface* iface = w->iface.load(std::memory_order_acquire);
            if (!req || !iface) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (lv2_ringbuffer_read_space(req) < sizeof(uint32_t)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            uint32_t size;
            lv2_ringbuffer_peek(req, (char*)&size, sizeof(uint32_t));

            if (lv2_ringbuffer_read_space(req) < sizeof(uint32_t) + size) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }

            lv2_ringbuffer_read(req, (char*)&size, sizeof(uint32_t));
            std::vector<uint8_t> buf(size);
            lv2_ringbuffer_read(req, (char*)buf.data(), size);
            iface->work(w->dsp_handle, host_respond, w, size, buf.data());
        }
    }

    static LV2_Worker_Status host_respond(
        LV2_Worker_Respond_Handle handle, uint32_t size, const void* data) {
        
        auto* w = (LV2HostWorker*)handle;
        if (!w) {
            return LV2_WORKER_ERR_NO_SPACE;
        }

        // Track responder calls to prevent ringbuffer use-after-free during shutdown
        w->responder_readers.fetch_add(1, std::memory_order_acq_rel);

        lv2_ringbuffer_t* resp = w->responses.load(std::memory_order_acquire);
        if (!resp) {
            w->responder_readers.fetch_sub(1, std::memory_order_acq_rel);
            return LV2_WORKER_ERR_NO_SPACE;
        }

        const size_t total = sizeof(uint32_t) + size;

        if (lv2_ringbuffer_write_space(resp) < total) {
            w->responder_readers.fetch_sub(1, std::memory_order_acq_rel);
            return LV2_WORKER_ERR_NO_SPACE;
        }

        lv2_ringbuffer_write(resp, (const char*)&size, sizeof(uint32_t));
        lv2_ringbuffer_write(resp, (const char*)data, size);

        w->responder_readers.fetch_sub(1, std::memory_order_acq_rel);
        return LV2_WORKER_SUCCESS;
    }

    void deliver_worker_responses() {
        lv2_ringbuffer_t* resp = host_worker_.responses.load(std::memory_order_acquire);
        const LV2_Worker_Interface* iface = host_worker_.iface.load(std::memory_order_acquire);
        if (!resp || !iface) {
            return;
        }

        while (true) {
            if (lv2_ringbuffer_read_space(resp) < sizeof(uint32_t)) break;

            uint32_t size;
            lv2_ringbuffer_peek(resp, (char*)&size, sizeof(uint32_t));

            if (lv2_ringbuffer_read_space(resp) < sizeof(uint32_t) + size) break;

            lv2_ringbuffer_read(resp, (char*)&size, sizeof(uint32_t));

            if (size > host_worker_.response_buffer.size()) {
                host_worker_.response_buffer.resize(size);
            }

            lv2_ringbuffer_read(resp, (char*)host_worker_.response_buffer.data(), size);
            iface->work_response(host_worker_.dsp_handle, size, host_worker_.response_buffer.data());
        }
    }

    void stop_worker() {
        IN

        const bool already_stopped = worker_stopped.exchange(true, std::memory_order_acq_rel);
        if (already_stopped) {
            LOGD("stop_worker: worker cleanup already completed, skipping duplicate call");
            OUT
            return;
        }

        // Gate: prevent new schedule_work() calls
        host_worker_.enabled.store(false, std::memory_order_release);

        // Wait for in-flight deliver_worker_responses() calls (process path)
        while (host_worker_.rt_readers.load(std::memory_order_acquire) != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        // Wait for in-flight host_schedule_work() calls (plugin direct path)
        while (host_worker_.scheduler_readers.load(std::memory_order_acquire) != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const bool was_running = host_worker_.running.exchange(false, std::memory_order_acq_rel);

        if (host_worker_.worker_thread.joinable())
            host_worker_.worker_thread.join();

        if (!was_running) {
            LOGD ("Worker thread not running, cleaning up worker resources");
        }

        // Wait for any in-flight host_respond() calls from plugin work() callbacks
        while (host_worker_.responder_readers.load(std::memory_order_acquire) != 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Load and clear atomic pointers safely
        lv2_ringbuffer_t* req = host_worker_.requests.exchange(nullptr, std::memory_order_acq_rel);
        if (req) {
            lv2_ringbuffer_free(req);
        }

        lv2_ringbuffer_t* resp = host_worker_.responses.exchange(nullptr, std::memory_order_acq_rel);
        if (resp) {
            lv2_ringbuffer_free(resp);
        }

        host_worker_.iface.store(nullptr, std::memory_order_release);
        host_worker_.dsp_handle = nullptr;
        OUT
    }

    // ========== Member variables ==========
    LilvWorld* world_;
    LilvInstance* instance_;

    LilvNode *audio_class_, *control_class_, *atom_class_, *input_class_, *rsz_minimumSize_;

    double sample_rate_;
    uint32_t max_block_length_;
    uint32_t required_atom_size_;

public:
    LilvPlugin* plugin_;
    std::vector<Port> ports_;
private:
    std::vector<PluginControl*> controls_;

    LV2HostWorker host_worker_;

    std::atomic<bool> shutdown_;
    std::atomic<bool> closed_;
    std::atomic<bool> worker_stopped{false};
};

// ============================================================================
// PluginControl Factory Implementation
// ============================================================================

inline PluginControl* PluginControl::create(LilvWorld* world, const LilvPlugin* plugin,
                                            const LilvPort* port, const LilvNode* audio_class,
                                            const LilvNode* control_class, const LilvNode* atom_class) {
    if (lilv_port_is_a(plugin, port, control_class)) {
        return new ControlPortFloat(world, plugin, port);
    } else if (lilv_port_is_a(plugin, port, atom_class)) {
        return new AtomPortControl(world, plugin, port);
    }
    // TODO: detect toggle/trigger properties
    return nullptr;
}

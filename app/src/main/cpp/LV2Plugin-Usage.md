# LV2Plugin Class - Usage Guide & Documentation

Generic, backend-agnostic LV2 plugin management with real-time safe audio processing, atom port communication, and worker thread support.

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Detailed Examples](#detailed-examples)
4. [API Reference](#api-reference)
5. [Threading & RT-Safety](#threading--rt-safety)
6. [Best Practices](#best-practices)

---

## Overview

**LV2Plugin** is a generic wrapper around LV2 plugin instances that handles:
- **Port discovery** and type classification (audio, control, atom)
- **Control management** (float, toggle, trigger, atom) via `PluginControl` subclasses
- **Real-time safe audio processing** with lock-free ringbuffers
- **Atom message routing** between UI and DSP threads
- **Worker thread management** for non-RT plugin tasks
- **URID mapping** and LV2 feature negotiation
- **State save/load** via Lilv

**Design principle**: Caller provides Lilv discovery; `LV2Plugin` manages instantiation and execution.

**Thread model**:
- **RT thread** (Oboe/JACK callback): `process()` only
- **UI thread**: `getControl()->setValue()`, `loadState()`, ringbuffer reads
- **Worker thread** (internal): Automatically spawned if plugin provides `LV2_Worker_Interface`

---

## Quick Start

### 1. Discover Plugin

```cpp
#include "LV2Plugin.hpp"

// Create Lilv world
LilvWorld* world = lilv_world_new();
lilv_world_load_all(world);

// Find plugin by URI or name
const LilvPlugins* all_plugins = lilv_world_get_all_plugins(world);
LilvNode* plugin_uri_node = lilv_new_uri(world, "http://example.com/my_plugin");
const LilvPlugin* plugin = lilv_plugins_get_by_uri(all_plugins, plugin_uri_node);

if (!plugin) {
    std::cerr << "Plugin not found\n";
    lilv_node_free(plugin_uri_node);
    lilv_world_free(world);
    return;
}
```

### 2. Create & Initialize

```cpp
// Create LV2Plugin instance
LV2Plugin lv2_plugin(world, (LilvPlugin*)plugin, 48000.0, 256);

// Initialize: discovers ports, creates controls, instantiates
if (!lv2_plugin.initialize()) {
    std::cerr << "Failed to initialize plugin\n";
    return;
}

// Start audio processing
lv2_plugin.start();
```

### 3. Connect Audio & Process

```cpp
// In audio callback (RT thread — real-time safe):
bool process_audio() {
    float left_in[256], right_in[256], left_out[256], right_out[256];
    
    // Get input from hardware/previous plugin
    get_input_buffers(left_in, right_in, 256);
    
    // Process through plugin
    if (!lv2_plugin.process(left_in, left_out, 256)) {
        return false;
    }
    if (!lv2_plugin.process(right_in, right_out, 256)) {
        return false;
    }
    
    // Send output to hardware/next stage
    send_output_buffers(left_out, right_out, 256);
    return true;
}
```

### 4. Set Controls (UI Thread)

```cpp
// Get control by name (extracted from Lilv port symbol)
PluginControl* gain = lv2_plugin.getControl("gain");
if (gain) {
    gain->setValue(0.5f);  // Set to 50%
}

// Get current value
auto val = gain->getValue();
float current_gain = std::get<float>(val);
std::cout << "Current gain: " << current_gain << std::endl;

// Reset to default
gain->reset();
```

### 5. Cleanup

```cpp
// Stop audio processing
lv2_plugin.stop();

// Destruction is automatic via destructor
// (closePlugin() called automatically)

lilv_node_free(plugin_uri_node);
lilv_world_free(world);
```

---

## Detailed Examples

### Example 1: Basic Gain Plugin

```cpp
#include "LV2Plugin.hpp"
#include <iostream>

int main() {
    // Setup Lilv
    LilvWorld* world = lilv_world_new();
    lilv_world_load_all(world);
    
    // Find a simple gain plugin
    LilvNode* plugin_uri = lilv_new_uri(world, "http://lv2plug.in/plugins/eg-am#amp");
    const LilvPlugin* plugin = lilv_plugins_get_by_uri(
        lilv_world_get_all_plugins(world), plugin_uri);
    
    if (!plugin) {
        std::cerr << "Gain plugin not found\n";
        return 1;
    }
    
    // Create and initialize
    LV2Plugin lv2_plugin(world, (LilvPlugin*)plugin, 48000.0, 256);
    if (!lv2_plugin.initialize()) {
        std::cerr << "Plugin init failed\n";
        return 1;
    }
    
    std::cout << "Plugin initialized with " << lv2_plugin.getPortCount() 
              << " ports\n";
    
    lv2_plugin.start();
    
    // Simulate 10 audio frames
    float in[256], out[256];
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 256; ++j) in[j] = 0.1f;  // Silent input
        lv2_plugin.process(in, out, 256);
    }
    
    lv2_plugin.stop();
    
    lilv_node_free(plugin_uri);
    lilv_world_free(world);
    
    return 0;
}
```

**Output:**
```
Plugin initialized with 3 ports
```

---

### Example 2: Control Parameter from UI

```cpp
#include "LV2Plugin.hpp"
#include <thread>

int main() {
    // ... setup plugin (as in Example 1) ...
    
    lv2_plugin.start();
    
    // Simulate UI thread adjusting parameters
    std::thread ui_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Find and adjust gain
        PluginControl* gain = lv2_plugin.getControl("gain");
        if (gain && gain->getType() == PluginControl::Type::ControlFloat) {
            std::cout << "Setting gain to 0.7\n";
            gain->setValue(0.7f);
        }
        
        // Find and adjust bypass (if exists)
        PluginControl* bypass = lv2_plugin.getControl("bypass");
        if (bypass && bypass->getType() == PluginControl::Type::Toggle) {
            std::cout << "Bypass is on\n";
            bypass->setValue(true);
        }
    });
    
    // Simulate audio thread
    float in[256], out[256];
    for (int i = 0; i < 100; ++i) {
        for (int j = 0; j < 256; ++j) in[j] = 0.5f;
        lv2_plugin.process(in, out, 256);
    }
    
    ui_thread.join();
    lv2_plugin.stop();
    
    return 0;
}
```

---

### Example 3: Handle Atom Messages (DSP→UI)

```cpp
#include "LV2Plugin.hpp"
#include <iostream>
#include <thread>

int main() {
    // ... setup plugin ...
    
    lv2_plugin.start();
    
    // UI thread reads atoms from plugin
    std::thread ui_thread([&]() {
        for (int frame = 0; frame < 5; ++frame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // Check for output atoms from first atom port
            lv2_ringbuffer_t* rb = lv2_plugin.getAtomOutputRingbuffer("output");
            if (!rb) continue;
            
            // Read atoms
            uint8_t atom_data[1024];
            size_t size = 0;
            
            while ((size = LV2Plugin::readAtomMessage(rb, atom_data, sizeof(atom_data))) > 0) {
                LV2_Atom* atom = (LV2_Atom*)atom_data;
                std::cout << "Received atom: type=" << atom->type 
                          << " size=" << atom->size << std::endl;
            }
        }
    });
    
    // DSP thread
    float in[256], out[256];
    for (int i = 0; i < 100; ++i) {
        for (int j = 0; j < 256; ++j) in[j] = 0.1f;
        lv2_plugin.process(in, out, 256);
    }
    
    ui_thread.join();
    lv2_plugin.stop();
    
    return 0;
}
```

---

### Example 4: Send Atom Messages (UI→DSP)

```cpp
#include "LV2Plugin.hpp"

int main() {
    // ... setup plugin ...
    
    lv2_plugin.start();
    
    // Get an atom port control
    PluginControl* atom_control = lv2_plugin.getControl("atom_input");
    if (!atom_control || atom_control->getType() != PluginControl::Type::AtomPort) {
        std::cerr << "No atom input port\n";
        return 1;
    }
    
    // Cast to AtomPortControl (only safe if we know it's AtomPortControl)
    // Better: check type first
    AtomPortControl* atom_port = dynamic_cast<AtomPortControl*>(atom_control);
    if (!atom_port) {
        std::cerr << "Failed to cast to AtomPortControl\n";
        return 1;
    }
    
    // Create atom message (example: simple atom int)
    std::vector<uint8_t> message(sizeof(int32_t));
    *(int32_t*)message.data() = 42;
    
    // Set message type URID (would get from URID map in real code)
    // For now, using placeholder
    atom_port->setMessageType(10);  // Atom_Int URID
    
    // Send to plugin
    atom_port->setValue(message);
    
    // Process a few frames to deliver the message
    float in[256], out[256];
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 256; ++j) in[j] = 0.0f;
        lv2_plugin.process(in, out, 256);
    }
    
    lv2_plugin.stop();
    
    return 0;
}
```

---

### Example 5: State Save/Load

```cpp
#include "LV2Plugin.hpp"
#include <iostream>

int main() {
    // ... setup and initialize plugin ...
    
    lv2_plugin.start();
    
    // Adjust some parameters
    PluginControl* gain = lv2_plugin.getControl("gain");
    if (gain) {
        gain->setValue(0.75f);
    }
    
    // Save state
    if (lv2_plugin.saveState("/tmp/my_preset.ttl")) {
        std::cout << "State saved\n";
    } else {
        std::cerr << "Failed to save state\n";
    }
    
    // Reset plugin state
    if (gain) {
        gain->reset();
    }
    
    // Load state back
    if (lv2_plugin.loadState("/tmp/my_preset.ttl")) {
        std::cout << "State loaded\n";
    } else {
        std::cerr << "Failed to load state\n";
    }
    
    // Check that gain was restored
    if (gain) {
        auto val = gain->getValue();
        float restored = std::get<float>(val);
        std::cout << "Restored gain: " << restored << std::endl;
    }
    
    lv2_plugin.stop();
    
    return 0;
}
```

---

### Example 6: Multiple Control Types

```cpp
#include "LV2Plugin.hpp"
#include <iomanip>

int main() {
    // ... setup plugin ...
    
    lv2_plugin.initialize();
    
    // List all controls
    std::cout << "Available controls:\n";
    std::cout << std::left << std::setw(20) << "Symbol" 
              << std::setw(15) << "Type" 
              << "Value\n";
    std::cout << std::string(50, '-') << "\n";
    
    for (uint32_t i = 0; i < lv2_plugin.getPortCount(); ++i) {
        const LilvPort* port = lv2_plugin.getPort(i);
        PluginControl* ctrl = lv2_plugin.getControl(
            lilv_node_as_string(lilv_port_get_symbol(
                (const LilvPlugin*)lv2_plugin.getPort(i), port)));
        
        if (!ctrl) continue;
        
        std::string type_str;
        std::string value_str;
        
        switch (ctrl->getType()) {
            case PluginControl::Type::ControlFloat: {
                type_str = "Float";
                float v = std::get<float>(ctrl->getValue());
                value_str = std::to_string(v);
                break;
            }
            case PluginControl::Type::Toggle: {
                type_str = "Toggle";
                bool v = std::get<bool>(ctrl->getValue());
                value_str = v ? "ON" : "OFF";
                break;
            }
            case PluginControl::Type::Trigger: {
                type_str = "Trigger";
                value_str = "-";
                break;
            }
            case PluginControl::Type::AtomPort: {
                type_str = "Atom";
                value_str = "(binary)";
                break;
            }
        }
        
        std::cout << std::left << std::setw(20) << ctrl->getSymbol()
                  << std::setw(15) << type_str
                  << value_str << "\n";
    }
    
    return 0;
}
```

---

## API Reference

### LV2Plugin Class

#### Constructor

```cpp
LV2Plugin(LilvWorld* world, LilvPlugin* plugin, 
          double sample_rate, uint32_t max_block_length)
```

- **world**: Lilv world (caller owns)
- **plugin**: Lilv plugin descriptor (caller owns)
- **sample_rate**: Audio sample rate (e.g., 48000.0)
- **max_block_length**: Maximum frames per process() call (e.g., 256)

#### Lifecycle

```cpp
bool initialize()
```
- Discovers ports, creates PluginControl instances, instantiates plugin
- Must be called once before processing
- Returns `false` if instantiation fails

```cpp
void start()
```
- Activates plugin DSP (safe to call from any thread)
- Must be called after initialize() before process()

```cpp
void stop()
```
- Deactivates plugin DSP
- Safe to call from any thread
- Called automatically by destructor

```cpp
void closePlugin()
```
- Frees all resources (buffers, controls, instance)
- Called automatically by destructor; explicit call optional

#### Audio Processing (RT-Safe)

```cpp
bool process(float* inputBuffer, float* outputBuffer, int numFrames)
```

- **inputBuffer**: Pointer to input samples (must be non-null, size ≥ numFrames)
- **outputBuffer**: Pointer to output samples (must be non-null, size ≥ numFrames)
- **numFrames**: Number of samples to process (0 < numFrames ≤ max_block_length)
- Returns `false` if plugin not running or arguments invalid
- **RT-safe**: No allocations, locks, or I/O
- Handles:
  - Audio buffer connections to plugin ports
  - UI→DSP atom message delivery
  - Plugin execution
  - Worker response delivery
  - DSP→UI atom ringbuffer writing

#### Control Access

```cpp
PluginControl* getControl(const char* symbol)
```
- Get control by port symbol (e.g., "gain", "bypass")
- Returns `nullptr` if not found
- **Not RT-safe** (but fast)

```cpp
uint32_t getPortCount() const
```
- Total number of ports (audio, control, atom)

```cpp
const LilvPort* getPort(uint32_t index) const
```
- Get Lilv port descriptor by index
- Returns `nullptr` if index out of range

#### Atom Communication

```cpp
lv2_ringbuffer_t* getAtomOutputRingbuffer(const char* portSymbol)
```
- Get ringbuffer for reading DSP→UI atoms from port
- Returns `nullptr` if port not found or not atom port
- **Not RT-safe** (but typically called once at setup)

```cpp
static size_t readAtomMessage(lv2_ringbuffer_t* rb, 
                              uint8_t* outBuffer, size_t maxSize)
```
- Read one atom message from ringbuffer
- Returns bytes read (0 if no complete message available)
- **Not RT-safe** (use from UI thread only)
- `outBuffer` must be ≥ `maxSize` bytes

#### State Management

```cpp
bool saveState(const std::string& filePath)
```
- Serialize plugin state (controls, worker state) to file
- Returns `true` on success
- **Not RT-safe** (call from UI thread)

```cpp
bool loadState(const std::string& filePath)
```
- Restore plugin state from file
- Returns `true` on success
- Updates all control values
- **Not RT-safe** (call from UI thread)

---

### PluginControl Class

Abstract base class for control ports and atom ports.

#### Virtual Methods

```cpp
virtual void setValue(const std::variant<float, bool, std::vector<uint8_t>>& value)
```
- Set control value (type must match)
- Type mismatch silently ignored

```cpp
virtual std::variant<float, bool, std::vector<uint8_t>> getValue() const
```
- Get current control value
- Extract with `std::get<T>()`

```cpp
virtual PluginControl::Type getType() const
```
- Returns: `ControlFloat`, `Toggle`, `Trigger`, `AtomPort`

```cpp
virtual const char* getSymbol() const
```
- Port symbol from Lilv (e.g., "gain")

```cpp
virtual const LilvPort* getPort() const
```
- Lilv port descriptor (for metadata access)

```cpp
virtual void reset()
```
- Reset to default value

#### Subclasses

**ControlPortFloat**
- Represents float control port
- Auto-clamps to min/max
- Default extracted from Lilv

**ToggleControl**
- Represents boolean control (0.0 or 1.0)
- Accepts both `bool` and `float` (> 0.5 → true)

**TriggerControl**
- Represents momentary control
- Accepts `bool` or `float`

**AtomPortControl**
- Represents variable-size atom port
- Accepts `std::vector<uint8_t>`
- Ringbuffer for DSP→UI output

---

## Threading & RT-Safety

### Threads

| Thread | Context | Safe Methods |
|--------|---------|--------------|
| **DSP/RT** | Oboe/JACK callback | `process()` only |
| **UI** | Main/Android UI thread | `getControl()->setValue()`, `loadState()`, `saveState()`, ringbuffer reads |
| **Worker** | Internal worker thread | Automatic (managed by LV2Plugin) |

### RT-Safety Rules in `process()`

✅ **What's safe:**
- Audio buffer connections (stack-based)
- Control value reads (atomic float)
- Atom message wrapping (stack buffer)
- Ringbuffer writes (acquire/release semantics)
- Plugin execution
- Worker response delivery

❌ **What's NOT safe:**
- Memory allocation/deallocation
- Locks or mutexes (use atomics)
- I/O operations
- Lilv calls
- `printf` or logging to console

### Atom Communication Flow

```
UI Thread                           RT Thread (process)
───────────                         ──────────────────

PluginControl->setValue(data)
  │
  └─> store in ui_to_dsp buffer
  └─> ui_to_dsp_pending.store(true, release)
                                    │
                                    └─> ui_to_dsp_pending.exchange(false, acquire)
                                    └─> wrap in LV2_Atom_Event
                                    └─> append to atom sequence
                                    └─> lilv_instance_run()
                                    │
                                    └─> iterate output atoms
                                    └─> write to dsp_to_ui ringbuffer (release)

UI reads ringbuffer                 
  └─> lv2_ringbuffer_read()        
  └─> process atoms (acquire semantics)
```

### Memory Ordering

| Operation | Memory Order | Why |
|-----------|--------------|-----|
| UI sets `ui_to_dsp_pending` | `release` | DSP must see buffered data |
| DSP reads `ui_to_dsp_pending` | `acquire` | Visibility of buffered data |
| Write to `dsp_to_ui` ringbuffer | `release` | Implicit in ringbuffer, write_ptr updated |
| Read from `dsp_to_ui` | `acquire` | Implicit in ringbuffer, read_ptr updated |

---

## Best Practices

### 1. Always Check Return Values

```cpp
if (!lv2_plugin.initialize()) {
    std::cerr << "Plugin initialization failed\n";
    return;
}

PluginControl* ctrl = lv2_plugin.getControl("gain");
if (!ctrl) {
    std::cerr << "Control 'gain' not found\n";
    return;
}
```

### 2. Handle Type Safely

```cpp
// Option A: Use variant directly
auto val = ctrl->getValue();
float fval = std::get<float>(val);  // Throws if wrong type

// Option B: Check type first
if (ctrl->getType() == PluginControl::Type::ControlFloat) {
    float fval = std::get<float>(ctrl->getValue());
}

// Option C: Try/catch
try {
    float fval = std::get<float>(ctrl->getValue());
} catch (const std::bad_variant_access&) {
    std::cerr << "Wrong control type\n";
}
```

### 3. Initialize Before Audio

```cpp
LV2Plugin plugin(world, lilv_plugin, sample_rate, block_size);

if (!plugin.initialize()) return;
plugin.start();

// ONLY THEN start calling process()
plugin.process(in, out, frames);
```

### 4. Separate UI and RT Threads

```cpp
// UI Thread: Set parameters
void on_slider_change(float value) {
    PluginControl* ctrl = plugin.getControl("gain");
    if (ctrl) ctrl->setValue(value);  // Not RT-safe here, but OK from UI
}

// RT Thread: Process audio
void audio_callback() {
    plugin.process(in, out, frames);  // RT-safe
}
```

### 5. Read Atoms Asynchronously

```cpp
// UI Thread: Drain atom ringbuffer periodically (e.g., 60 FPS)
void ui_loop() {
    lv2_ringbuffer_t* rb = plugin.getAtomOutputRingbuffer("output");
    uint8_t atom_buf[1024];
    size_t size;
    
    while ((size = LV2Plugin::readAtomMessage(rb, atom_buf, sizeof(atom_buf))) > 0) {
        handle_atom((LV2_Atom*)atom_buf, size);
    }
}
```

### 6. Save/Load States

```cpp
// After adjusting plugin parameters
if (!plugin.saveState("/path/to/preset.ttl")) {
    std::cerr << "Failed to save preset\n";
}

// Later: Restore
if (!plugin.loadState("/path/to/preset.ttl")) {
    std::cerr << "Failed to load preset\n";
}
```

### 7. Cleanup Properly

```cpp
// Option A: RAII (recommended)
{
    LV2Plugin plugin(world, lilv_plugin, sr, block_size);
    plugin.initialize();
    plugin.start();
    // Use plugin...
    plugin.stop();
    // Destructor calls closePlugin()
}  // All cleaned up

// Option B: Manual
plugin.stop();
plugin.closePlugin();
```

### 8. Monitor Worker Status

If plugin provides worker interface:
- Worker thread spawns automatically in `initialize()`
- No action needed; responses delivered in `process()` cycle
- Check Lilv for `LV2_WORKER__interface` to verify support

---

## Limitations

- **Single plugin instance**: One plugin per `LV2Plugin` object (no plugin chaining)
- **No MIDI**: Audio-only currently; MIDI support via atom ports future work
- **No UI launch**: Headless only; no plugin GUI support
- **State format**: Lilv TTL format only (no custom serialization)
- **Discovery**: Caller responsible for Lilv world and plugin lookup

---

## Troubleshooting

### Plugin won't initialize
- Check `lilv_plugin` is valid (not `nullptr`)
- Verify required LV2 features are supported (unlikely—core features always provided)
- Check port types and buffer sizes

### Audio produces silence
- Verify `start()` called after `initialize()`
- Check input/output buffers are connected correctly in caller
- Ensure `process()` called regularly (RT callback)

### Controls don't update
- Verify control symbol matches port name from Lilv
- Check return type of `getValue()` matches control type
- Ensure `process()` running (needed for some plugins to update)

### Atom messages not arriving
- Verify atom output port exists and is discovered
- Check `getAtomOutputRingbuffer()` returns non-null
- Ensure UI thread reads ringbuffer periodically (not blocking)
- Plugin may not generate atoms for all inputs

### Build errors
- Ensure Lilv headers installed: `pkg-config --cflags lilv-0`
- Compile with `-std=c++17` or later
- Link with: `pkg-config --libs lilv-0`

---

## See Also

- [LV2 Specification](http://lv2plug.in/)
- [Lilv Documentation](https://doxygen.drobilla.net/lilv/index.html)
- [Example Backends](../LV2OboeHost.hpp), [LV2JackX11Host.hpp](../LV2JackX11Host.hpp)

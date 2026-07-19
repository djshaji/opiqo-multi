# `LV2Plugin.hpp` API Reference

Portable reference for reusing `app/src/main/cpp/LV2Plugin.hpp` in another LV2 host.

This document focuses on the **public API**, **host integration contract**, **threading model**, and **behavioral guarantees/caveats** of the wrapper as it exists in this repository.

## Overview

`LV2Plugin` is a C++17 wrapper around a single LV2 plugin instance discovered via Lilv.

It provides:
- plugin instantiation from `LilvPlugin*` or plugin URI
- port discovery and per-port runtime state
- control access through `PluginControl`
- atom input/output transport
- LV2 worker support
- Lilv state save/load helpers
- URID map/unmap services and common LV2 features

It does **not** provide:
- global plugin discovery UI
- graph scheduling across many plugins
- host audio I/O
- host-side pointer ownership/synchronization
- multi-bus routing abstraction

## Build / Dependency Requirements

Required headers and libraries visible from `LV2Plugin.hpp`:
- C++17
- Lilv
- LV2 headers for:
  - `urid`
  - `atom`
  - `options`
  - `parameters`
  - `buf-size`
  - `patch`
  - `worker`
  - `state`
  - `resize-port`
  - `midi`
- `lv2_ringbuffer.h`
- `json.hpp` (`nlohmann::json` single-header)

Android-specific logging:
- if `__ANDROID__` is defined, the header includes `logging_macros.h`
- on another platform, either provide equivalent macros or remove/replace those calls

## Public Types

## `PluginControl`

Abstract base class for host-visible control access.

### Enum

```cpp
enum class Type {
    ControlFloat,
    Toggle,
    Trigger,
    AtomPort
};
```

### Virtual interface

```cpp
virtual void setValue(const std::variant<float, bool, std::vector<uint8_t>>& value) = 0;
virtual std::variant<float, bool, std::vector<uint8_t>> getValue() const = 0;
virtual Type getType() const = 0;
virtual const char* getSymbol() const = 0;
virtual const LilvPort* getPort() const = 0;
virtual void reset() = 0;
```

### Factory

```cpp
static PluginControl* create(
    LilvWorld* world,
    const LilvPlugin* plugin,
    const LilvPort* port,
    const LilvNode* audio_class,
    const LilvNode* control_class,
    const LilvNode* atom_class);
```

#### Notes
- current factory behavior in this repository:
  - control ports → `ControlPortFloat`
  - atom ports → `AtomPortControl`
  - toggle/trigger inference is **not** currently implemented in the factory
- hosts wanting strict toggle/trigger classes will need to extend factory logic

## `ControlPortFloat`

Represents a float control port with min/max/default values extracted from Lilv.

### Constructor

```cpp
ControlPortFloat(LilvWorld* world, const LilvPlugin* plugin, const LilvPort* port)
```

### Additional method

```cpp
float* getValuePtr()
```

#### Behavior
- clamps writes to `[min, max]`
- `reset()` restores default
- `getValue()` returns `float`

## `ToggleControl`

Represents a boolean-like control.

### Constructor

```cpp
ToggleControl(LilvWorld* world, const LilvPlugin* plugin, const LilvPort* port)
```

### Additional method

```cpp
float getAsFloat() const
```

#### Behavior
- accepts `bool`
- also accepts `float` (`> 0.5f` means `true`)
- `getValue()` returns `bool`

#### Caveat
- defined in the header, but not currently produced by `PluginControl::create()` in this repo

## `TriggerControl`

Represents a momentary trigger/armed state.

### Constructor

```cpp
TriggerControl(LilvWorld* world, const LilvPlugin* plugin, const LilvPort* port)
```

### Additional methods

```cpp
bool isArmed()
float getAsFloat() const
```

#### Caveat
- defined in the header, but not currently produced by `PluginControl::create()` in this repo

## `AtomState`

Shared transport state for one atom-capable port.

### Members

```cpp
std::vector<uint8_t> ui_to_dsp;
uint32_t ui_to_dsp_type = 0;
std::atomic<bool> ui_to_dsp_pending{false};
mutable std::mutex ui_to_dsp_mutex;
lv2_ringbuffer_t* dsp_to_ui = nullptr;
```

### Constructor

```cpp
AtomState(size_t ringbuffer_size = 16384)
```

#### Behavior
- allocates a DSP→UI ringbuffer on construction
- frees it on destruction

## `AtomPortControl`

Represents an atom port exposed through `PluginControl`.

### Constructor / destructor

```cpp
AtomPortControl(LilvWorld* world, const LilvPlugin* plugin, const LilvPort* port)
~AtomPortControl() override
```

### Additional methods

```cpp
AtomState* getAtomState()
void setMessageType(uint32_t type_urid)
```

#### Behavior
- `setValue()` expects `std::vector<uint8_t>`
- bytes are queued into `ui_to_dsp`
- message type must be supplied separately when needed

## `LV2Plugin`

Single-plugin runtime wrapper.

## Construction

### Constructor from resolved `LilvPlugin*`

```cpp
LV2Plugin(
    LilvWorld* world,
    LilvPlugin* plugin,
    double sample_rate,
    uint32_t max_block_length)
```

### Constructor from plugin URI

```cpp
LV2Plugin(
    LilvWorld* world,
    const char* plugin_uri,
    double sample_rate,
    uint32_t max_block_length)
```

### Destructor

```cpp
~LV2Plugin()
```

#### Important lifetime note
The destructor in this repository does **not** automatically call `closePlugin()`.

Host code should use:

```cpp
plugin->stop();
plugin->closePlugin();
delete plugin;
```

## Lifecycle API

### `initialize`

```cpp
bool initialize()
```

#### Purpose
- validates `world_` / `plugin_`
- creates required Lilv class nodes
- initializes URIDs and host features
- checks resize-port requirements
- discovers ports and creates controls
- instantiates the LV2 plugin instance
- sets up worker support if provided by the plugin

#### Call requirements
- call once before `start()` or `process()`
- not real-time safe

#### Returns
- `true` on success
- `false` on any setup/instantiation failure

### `start`

```cpp
void start()
```

#### Purpose
- clears shutdown flag
- activates the Lilv instance if present

#### Call requirements
- call after `initialize()`
- not intended as a real-time callback operation, though it is lightweight

### `stop`

```cpp
void stop()
```

#### Purpose
- sets shutdown flag
- deactivates the Lilv instance if present

### `closePlugin`

```cpp
void closePlugin()
```

#### Purpose
Full teardown of plugin-owned resources:
- worker shutdown
- `LilvInstance` deactivate/free
- per-port atom buffer cleanup
- control object deletion
- Lilv node cleanup

#### Guarantees
- guarded against double-close via `closed_`
- worker shutdown guarded by `worker_stopped`

#### Call requirements
- host must ensure the audio thread can no longer call `process()` on this instance
- not real-time safe

## Real-Time Processing

### `process`

```cpp
bool process(float* inputBuffer, float* outputBuffer, int numFrames)
```

#### Purpose
Runs one audio block through the plugin.

#### Expected calling context
- audio/DSP thread only
- plugin already initialized and started
- `numFrames > 0`
- buffers valid for the size expected by the host

#### Behavior
1. if `enabled == false`, copies input to output and returns `true`
2. rejects null buffers, stopped instance, or invalid frame counts
3. connects audio ports to `inputBuffer` / `outputBuffer`
4. packages pending UI→DSP atom messages into atom sequences
5. calls `lilv_instance_run(instance_, numFrames)`
6. delivers worker responses
7. drains DSP→UI atom events into per-port ringbuffers

#### Returns
- `true` when processing or bypass-copy succeeds
- `false` when instance is unavailable or arguments are invalid

#### Real-time constraints
This is the method intended for real-time use in this codebase.

#### Caveat: audio-port mapping model
This implementation maps:
- every audio input port → the same `inputBuffer`
- every audio output port → the same `outputBuffer`

This is suitable for simple serial/in-place hosts, but may need adaptation for:
- split buses
- independent left/right buffers
- sidechains
- complex graph routing

## Control Access API

### `getControl`

```cpp
PluginControl* getControl(const char* symbol)
```

#### Purpose
Returns a control object by Lilv port symbol.

#### Returns
- pointer to a control object
- `nullptr` if not found

#### Notes
- convenient for symbol-based hosts
- not designed for RT usage in this project

### `getPortCount`

```cpp
uint32_t getPortCount() const
```

Returns the size of `ports_`.

### `getPort`

```cpp
const LilvPort* getPort(uint32_t index) const
```

Returns the Lilv port descriptor for an index, or `nullptr` if out of range.

## Atom / Message API

### `send_path_parameter`

```cpp
void send_path_parameter(const char* property_uri, const char* abs_path)
```

#### Purpose
Queues a `patch:Set` message containing an `atom:Path` for delivery during the next `process()`.

#### Requirements
- `property_uri` must be non-empty
- `abs_path` must be a non-empty absolute filesystem path
- a non-MIDI atom input port must exist

#### Behavior
- finds the first suitable atom input port
- forges a `patch:Set` object using `LV2_Atom_Forge`
- stores only the atom body in `ui_to_dsp`
- marks the message pending
- stores the selection in `writables[property_uri]`

#### Caveat
This function explicitly rejects non-filesystem paths such as `content://...`.

### `send_filename_to_plugin`

```cpp
void send_filename_to_plugin(const char* filename, const char* uri)
```

Thin alias for `send_path_parameter(uri, filename)`.

### `getAtomOutputRingbuffer`

```cpp
lv2_ringbuffer_t* getAtomOutputRingbuffer(const char* portSymbol)
```

Returns the DSP→UI ringbuffer for an atom output port symbol, or `nullptr`.

### `readAtomMessage` (static)

```cpp
static size_t readAtomMessage(
    lv2_ringbuffer_t* rb,
    uint8_t* outBuffer,
    size_t maxSize)
```

#### Purpose
Reads one full atom payload from a ringbuffer into caller-owned storage.

#### Returns
- total bytes copied (`sizeof(LV2_Atom) + payload`)
- `0` if no complete message is available or the buffer is too small

#### Safety checks
- requires at least `sizeof(LV2_Atom)` bytes available
- rejects payloads above 16 MiB
- requires complete message availability before reading

### `readAtomMessage` (by port symbol)

```cpp
bool readAtomMessage(const char* portSymbol, std::vector<uint8_t>& outMessage)
```

Reads one complete message from the named atom output port.

### `extractPathFromAtomMessage`

```cpp
bool extractPathFromAtomMessage(
    const uint8_t* msg,
    size_t msgSize,
    std::string& outPath,
    std::string* outPropertyUri = nullptr) const
```

#### Purpose
Decodes a `patch:Set` / `atom:Object` message carrying a path-like value.

#### Expected format
- outer type: `atom:Object`
- object type: `patch:Set`
- `patch:property` present as `atom:URID`
- `patch:value` present as `atom:Path` or `atom:String`

#### Returns
- `true` on successful decode
- `false` if the atom is not in the expected format

### `readAtomMessages`

```cpp
size_t readAtomMessages(
    const char* portSymbol,
    std::vector<std::vector<uint8_t>>& outMessages,
    size_t maxMessages = 64)
```

Drains up to `maxMessages` messages from one atom output port.

## State API

### `saveState`

```cpp
bool saveState(const std::string& filePath)
```

#### Purpose
Serializes LV2 plugin state to a Lilv-managed state file.

#### Returns
- `true` on success
- `false` if no instance/plugin is available or Lilv fails

### `loadState`

```cpp
bool loadState(const std::string& filePath)
```

#### Purpose
Restores LV2 plugin state from file.

#### Returns
- `true` on success
- `false` if no instance exists or state loading fails

#### Notes
- uses the wrapper’s map/unmap/path features
- not real-time safe

## Public Data Members

These are publicly exposed by the current header and may be used by a host.

### `writables`

```cpp
json writables;
```

Host-visible JSON map of path-property URI → absolute path recorded by `send_path_parameter()`.

### `enabled`

```cpp
bool enabled = true;
```

When set to `false`, `process()` bypasses the plugin and copies input to output.

### `plugin_`

```cpp
LilvPlugin* plugin_;
```

Resolved Lilv plugin descriptor owned externally by the `LilvWorld`.

### `ports_`

```cpp
std::vector<Port> ports_;
```

Runtime port list discovered in `initialize()`.

#### `Port` fields

```cpp
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
```

#### Host usage notes
- `control` is directly writable and is how this repository’s Android UI updates control ports
- direct `ports_` access is convenient but tightly couples your host to wrapper internals
- prefer `getControl(symbol)` if you want a cleaner host API

### URID facilities

```cpp
LV2_URID_Map um_;
LV2_URID_Unmap unm_;
```

And a public `urids_` struct containing commonly used URIDs:
- atom transfer/object/string/path/etc.
- midi event
- buf-size options
- patch URIs
- parameter sample-rate URI

Useful when a host wants to interpret returned atom messages consistently with the plugin instance.

## Host Integration Contract

Your host is responsible for:
- creating and keeping alive a `LilvWorld*`
- configuring `LILV_OPTION_LV2_PATH` if needed
- loading the world before constructing URI-based instances
- ensuring `max_block_length` matches real callback requirements
- publishing/removing `LV2Plugin*` safely across threads
- calling `closePlugin()` before deletion

## Threading Model

### Intended thread usage

| Operation | RT-safe in this codebase? | Notes |
|---|---:|---|
| `process()` | Yes | Intended DSP entry point |
| `initialize()` | No | Lilv + allocation work |
| `start()` | No | Use outside callback |
| `stop()` | No | Use outside callback |
| `closePlugin()` | No | Frees resources / joins worker |
| `getControl()` | No | Fast, but not treated as RT API |
| `saveState()` / `loadState()` | No | File + Lilv state APIs |
| `send_path_parameter()` | No | Forges atom + locks atom state |
| `readAtomMessage*()` | No | UI/control thread consumption |

### External synchronization
`LV2Plugin` internally protects atom transport and worker teardown, but it does **not** solve host-level pointer lifetime races.

If one thread can replace/delete a plugin while another thread may call `process()`, the host must provide its own synchronization.

## Worker Support Behavior

If the plugin exposes `LV2_WORKER__interface`, `initialize()` / `init_instance()` will:
- preallocate request/response ringbuffers
- publish `LV2_WORKER__schedule` feature
- start a worker thread
- deliver `work_response()` during `process()`

### Shutdown behavior
`closePlugin()` stops worker scheduling, waits for in-flight readers/responders, joins the worker thread, and frees worker ringbuffers.

## Error / Failure Behavior

Common causes of `initialize()` failure:
- null `LilvWorld*`
- unresolved plugin URI / null `plugin_`
- unsupported required plugin features
- Lilv instantiation failure

Common causes of `process()` returning `false`:
- `shutdown_ == true`
- `instance_ == nullptr`
- null audio buffers
- non-positive frame count

Common causes of `send_path_parameter()` failure:
- empty URI/path
- relative path
- no suitable atom input port
- forge failure

## Minimal Host Example

```cpp
LilvWorld* world = lilv_world_new();
// optionally set LV2 path here
lilv_world_load_all(world);

LV2Plugin* plugin = new LV2Plugin(world, pluginUri, sampleRate, maxBlockLength);
if (!plugin->initialize()) {
    delete plugin;
    plugin = nullptr;
    return;
}

plugin->start();

// audio callback / DSP thread
plugin->process(inputBuffer, outputBuffer, frames);

// UI thread
if (PluginControl* gain = plugin->getControl("gain")) {
    gain->setValue(0.8f);
}

plugin->send_path_parameter(propertyUri, absoluteFilePath);

std::vector<uint8_t> msg;
if (plugin->readAtomMessage("notify", msg)) {
    // decode if desired
}

plugin->stop();
plugin->closePlugin();
delete plugin;

lilv_world_free(world);
```

## Practical Caveats for Reuse

- The current port factory does not auto-create `ToggleControl` / `TriggerControl`.
- `process()` assumes a simple buffer topology.
- `closePlugin()` is required for correct cleanup.
- Hosts using content/document URIs will likely need to copy files to local storage before calling `send_path_parameter()`.
- Hosts needing a polished browser/UI should perform a separate metadata scan via Lilv rather than relying only on runtime `ports_`.

## Related Documents in This Repository

- `LV2Plugin-REUSE.md` — integration-oriented reuse guide based on how this app uses the wrapper
- `app/src/main/cpp/LV2Plugin-Usage.md` — larger in-repo usage/documentation file
- `MUTEX_IMPLEMENTATION.md` — host-side synchronization pattern used by `opiqo-multi`


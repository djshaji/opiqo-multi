# Reusing `LV2Plugin.hpp` in Other Projects

This document explains how `app/src/main/cpp/LV2Plugin.hpp` is used in **opiqo-multi**, and what you need to provide when reusing it in another host.

## Checklist

- [x] Understand what `LV2Plugin` owns vs what the host owns
- [x] Copy the required companion files and link dependencies
- [x] Follow the exact lifecycle: discover → construct → `initialize()` → `start()` → `process()` → `stop()` → `closePlugin()` → `delete`
- [x] Keep all Lilv/discovery/state work off the real-time audio thread
- [x] Add host-side locking around plugin pointer replacement/destruction
- [x] Decide how your host will expose controls, file-path parameters, and presets

## What `LV2Plugin.hpp` Actually Provides

`LV2Plugin` is a **single-plugin runtime wrapper**, not a full host by itself.

It handles:
- LV2 instantiation through Lilv
- port discovery and control objects
- control value storage
- atom input/output buffering
- LV2 worker support
- Lilv state save/load helpers
- URID mapping and LV2 features required by many plugins

It does **not** handle:
- plugin discovery UI
- audio device I/O
- plugin-chain ownership
- plugin pointer synchronization across threads
- preset file format for your app

In `opiqo-multi`, those responsibilities are split across:
- `app/src/main/cpp/LiveEffectEngine.h/.cpp` — host-level ownership of `LV2Plugin*`
- `app/src/main/cpp/FullDuplexPass.h` — Oboe audio callback that calls `process()`
- `app/src/main/cpp/jni_bridge.cpp` — plugin load/unload, parameter updates, file-path messages, preset export
- `app/src/main/java/org/acoustixaudio/opiqo/multi/UI.java` — UI generated from plugin metadata

## Files to Copy or Replace

Minimum files from this repo:
- `app/src/main/cpp/LV2Plugin.hpp`
- `app/src/main/cpp/lv2_ringbuffer.h`
- `app/src/main/cpp/json.hpp`

Optional / platform-specific:
- `app/src/main/cpp/logging_macros.h`
  - Required as-is on Android
  - On desktop/non-Android, replace `LOGD/LOGE/...` with your own logging macros

System / external headers and libs required by the header:
- Lilv
- LV2 headers (`atom`, `urid`, `worker`, `state`, `patch`, `options`, `resize-port`, `midi`, `buf-size`)
- C++17 STL

From this project’s `CMakeLists.txt`, the relevant linked LV2/Lilv libraries are:
- `liblilv`
- `libsord`
- `libserd`
- `libsratom`
- `libzix`
- any LV2 plugin dependencies your target plugins need

## Ownership Model

This is the most important rule when reusing the file:

### `LV2Plugin` owns
- its `LilvInstance*`
- its per-port buffers (`Port::atom`, control storage)
- worker request/response ringbuffers
- control objects
- internal URID map/unmap tables

### Your host must own
- the `LilvWorld*`
- discovery of `LilvPlugin*` / plugin URIs
- the `LV2Plugin*` lifetime
- synchronization when multiple threads can see the pointer
- the audio callback that feeds `process()`

In `opiqo-multi`, the world is created once in `LiveEffectEngine::initLV2()` and reused for all plugin instances.

## The Actual Lifecycle Used in `opiqo-multi`

### 1. Create a shared Lilv world once
See `LiveEffectEngine::initLV2()`.

The host creates the world and some reusable class/property nodes:
- `LV2_CORE__AudioPort`
- `LV2_CORE__ControlPort`
- `LV2_ATOM__AtomPort`
- `LV2_CORE__InputPort`
- `LV2_CORE__toggled`
- `LV2_CORE__enumeration`
- `LV2_PATCH__writable`

`LV2Plugin` itself does **not** do plugin-library discovery for your whole app. It only resolves one plugin when constructed.

### 2. Discover plugins outside `LV2Plugin`
See `Java_org_acoustixaudio_opiqo_multi_AudioEngine_initPlugins()` in `jni_bridge.cpp`.

`opiqo-multi` calls:
- `lilv_world_set_option(world, LILV_OPTION_LV2_PATH, ...)`
- `lilv_world_load_all(world)`
- `lilv_world_get_all_plugins(world)`

Then it builds its own JSON metadata for UI:
- plugin name/URI/author
- port list
- control ranges
- toggle/enumeration detection
- `patch:writable` file parameters

**Reuse implication:** if you need a plugin browser or auto-generated UI, you must build that layer yourself. `LV2Plugin` does not expose a ready-made “plugin info” API.

### 3. Construct one `LV2Plugin` per loaded slot/plugin
In `jni_bridge.cpp`, `addPlugin()` does this:

```cpp
plugin = new LV2Plugin(engine->world, uriChars, engine->sampleRate, engine->blockSize);
if (!plugin->initialize()) { ... }
plugin->start();
```

Constructor inputs mean:
- `world`: existing, already-loaded `LilvWorld*`
- `plugin URI` or `LilvPlugin*`
- `sample_rate`: the runtime sample rate your audio engine will use
- `max_block_length`: the largest block you will ever pass to `process()`

### 4. Publish the plugin pointer only after initialization succeeds
`opiqo-multi` does not place the new plugin in the audio chain until after:
- construction
- `initialize()`
- `start()`

That is important. Do not let the audio thread see a half-initialized instance.

### 5. Process audio only from the real-time callback
See `FullDuplexPass::processPluginChain()`.

`opiqo-multi` calls:

```cpp
if (plugin1) plugin1->process(buffer, buffer, numSamples);
if (plugin2) plugin2->process(buffer, buffer, numSamples);
if (plugin3) plugin3->process(buffer, buffer, numSamples);
if (plugin4) plugin4->process(buffer, buffer, numSamples);
```

The host chains plugins **serially** and **in-place**.

### 6. Stop and destroy explicitly
`LV2Plugin` does **not** auto-clean everything in the destructor here. `closePlugin()` is expected before deletion.

`opiqo-multi` uses:

```cpp
plugin->closePlugin();
delete plugin;
```

Do not rely on `delete plugin;` alone.

## Host-Side Threading Pattern

`LV2Plugin::process()` is the only method treated as real-time safe in this project.

Everything else happens on non-RT threads:
- plugin add/remove
- control/UI writes
- file-path atom sending
- state save/load
- plugin discovery

### Host locking pattern used here
`LiveEffectEngine` owns a single `std::mutex pluginMutex` protecting `plugin1..plugin4`.

- `jni_bridge.cpp` locks it before replacing/deleting/enabling plugins or writing parameters
- `FullDuplexPass::processPluginChain()` locks the same mutex before calling `process()` on the current pointers

That pattern prevents the audio thread from running `process()` on a plugin being deleted.

### Bypass gate during mutation
Before plugin replacement/removal, `opiqo-multi` also sets `engine->bypass = true`, mutates pointers, then restores it to `false`.

That is a host-level safety gate, separate from `LV2Plugin` itself.

## Audio Integration Details

`LV2Plugin::process(float* inputBuffer, float* outputBuffer, int numFrames)` expects:
- plugin already initialized and started
- buffers already valid
- no zero/negative frame count

Inside `process()`, `LV2Plugin`:
1. connects all audio input ports to `inputBuffer`
2. connects all audio output ports to `outputBuffer`
3. appends any pending UI→DSP atom messages
4. runs `lilv_instance_run(instance_, numFrames)`
5. delivers worker responses
6. copies DSP→UI atom messages into ringbuffers

### Important reuse caveat: audio port model
This implementation uses **one input buffer pointer** for every audio input port and **one output buffer pointer** for every audio output port.

That fits `opiqo-multi`’s simple in-place chain, but may not fit:
- plugins with separate buses
- plugins expecting distinct left/right buffers
- sidechain/topology-heavy plugins

If your host needs independent channel buffers, you will likely need to adapt `process()`.

### Important reuse caveat: block size contract
`max_block_length` is part of construction and plugin instantiation.

In `LiveEffectEngine::setPluginBlockSize()`, this project refuses block-size changes while audio is running.

If your callback buffer size can change, either:
- instantiate with a safe maximum block size, or
- add a host-side block adapter like `FullDuplexPass` does

## Control / Parameter Handling

There are **two ways** controls appear in this codebase.

### A. Generic API available from `LV2Plugin`
You can fetch a symbolic control and set it via `PluginControl`:

```cpp
PluginControl* control = plugin->getControl("GAIN");
if (control) {
    control->setValue(0.5f);
}
```

That is the most reusable API if your host addresses parameters by symbol.

### B. What `opiqo-multi` actually uses for UI sliders
The Android UI works from port indices discovered in `initPlugins()`, and JNI writes directly into:

```cpp
plugin->ports_.at(index).control = value;
```

See `Java_org_acoustixaudio_opiqo_multi_AudioEngine_setValue()`.

This is simpler for a data-driven UI because `initPlugins()` already exported port indices/min/max/default values to Java.

### UI generation pattern in this project
`UI.java` uses the JSON from `getPluginInfo()` to build controls dynamically:
- `control` ports become `Slider`
- `toggled` ports become `ToggleButton`
- `dropdown` ports are built from Lilv scale points
- file-backed `patch:writable` parameters become file pickers + `Spinner`

That pattern is useful if your host has a UI layer separate from the audio engine: discover once with Lilv, then drive runtime parameter changes by index or symbol.

## Atom / File-Path Parameters

This is one of the most reusable parts of the wrapper.

### What `opiqo-multi` uses it for
Some plugins expose `patch:writable` parameters for selecting files (for example, model files or impulse responses).

The host discovers those properties in `initPlugins()` and stores them in JSON under `writableParams`.

When the user chooses a file, Java calls:

```java
AudioEngine.setFilePath(position, key, filePath);
```

JNI then calls:

```cpp
p->send_path_parameter(uriStr.c_str(), pathStr.c_str());
```

### What `send_path_parameter()` expects
- a **property URI** such as a `patch:writable` parameter URI
- an **absolute filesystem path**

It explicitly rejects non-filesystem paths:
- no empty strings
- no `content://...`
- must begin with `/`

### What it sends
It forges a `patch:Set` object containing:
- `patch:property` = your property URID
- `patch:value` = `atom:Path`

Then it queues the message for delivery on the next `process()` call by writing into the atom input state:
- `ui_to_dsp`
- `ui_to_dsp_type`
- `ui_to_dsp_pending`

### Reading replies from the plugin
After sending a path, `opiqo-multi` tries to read an atom response from the `notify` output port:

```cpp
std::vector<uint8_t> msg;
if (p->readAtomMessage("notify", msg)) {
    std::string path, property;
    p->extractPathFromAtomMessage(msg.data(), msg.size(), path, &property);
}
```

That is optional, but useful if your plugin acknowledges or reflects state through atom output.

### Reuse implication
If your plugins use atom messaging for more than file paths, the same mechanism can be reused:
- store bytes in `AtomPortControl`
- set the message URID
- let `process()` deliver them
- drain output ringbuffers from a non-RT thread

## DSP→UI Atom Output Pattern

`LV2Plugin` keeps a lock-free ringbuffer per atom output port (`AtomState::dsp_to_ui`).

Use this from a non-RT thread:

```cpp
std::vector<uint8_t> msg;
if (plugin->readAtomMessage("notify", msg)) {
    // decode msg
}
```

For batch draining:

```cpp
std::vector<std::vector<uint8_t>> messages;
plugin->readAtomMessages("notify", messages, 64);
```

In `opiqo-multi`, atom output is not deeply integrated into UI refresh yet; it is mainly used as a verification/debug path after sending file-path messages.

## Presets and State

There are **two distinct concepts** in this repo.

### 1. App preset export (used today)
`jni_bridge.cpp` exports a JSON object with:
- plugin name
- plugin URI
- every non-audio port’s `control` value
- `enabled`
- `writables`

That is what `getPreset()` / `getPresetList()` return.

This means the current app preset system is mostly a **host-defined snapshot**, not direct LV2 state serialization.

### 2. LV2 state API (available in `LV2Plugin`)
`LV2Plugin` also exposes:

```cpp
bool saveState(const std::string& filePath);
bool loadState(const std::string& filePath);
```

These use Lilv state APIs and are the better choice if you want:
- plugin-native state
- worker-managed plugin state
- TTL-based state files

### Reuse recommendation
If you are building a new host, decide early whether your presets are:
- **host snapshots** of controls and file selections, or
- **true LV2 states** via `saveState()` / `loadState()`, or
- a hybrid of both

`opiqo-multi` currently demonstrates the host-snapshot model more heavily.

## Worker Thread Support

`LV2Plugin.hpp` already contains host-side support for plugins that expose `LV2_Worker_Interface`.

### What it does internally
- preallocates worker request/response ringbuffers **before** plugin instantiation
- exposes `LV2_WORKER__schedule` as a feature
- starts a worker thread if the plugin provides the worker interface
- delivers `work_response()` from `process()`
- shuts down carefully using atomic reader counters

### Why preallocation matters
This project preallocates worker ringbuffers before `lilv_plugin_instantiate()` because some plugins may call `schedule_work()` during initialization.

If those ringbuffers did not exist yet, worker scheduling during init could crash or fail.

### What you need to do in another host
Usually nothing extra if you keep `LV2Plugin.hpp` as-is.

Just make sure you:
- call `closePlugin()` before deletion
- never free the plugin while `process()` can still run

## Teardown Rules

The safest shutdown sequence, based on this codebase, is:

1. stop exposing the plugin to the audio thread
2. wait until the host knows `process()` is no longer running on it
3. call `plugin->closePlugin()`
4. `delete plugin`

In `opiqo-multi`, pointer removal from the active chain happens under `pluginMutex`, and `FullDuplexPass` uses the same mutex before calling `process()`.

### Why `closePlugin()` matters
It does all of the real cleanup:
- deactivates and frees the `LilvInstance`
- stops the worker thread
- waits for in-flight worker readers/responders to finish
- frees atom buffers
- deletes control objects
- frees Lilv nodes created during initialization

### Double-close protection already exists
The wrapper uses atomics like `closed_` and `worker_stopped` to avoid duplicate teardown. That helps, but you should still treat `closePlugin()` as a one-time host lifecycle operation.

## Important Gotchas When Reusing This Header

### 1. `process()` is the RT-safe entry point
Do not call these from the real-time audio callback:
- `initialize()`
- `closePlugin()`
- `saveState()` / `loadState()`
- Lilv discovery APIs
- file-path/UI messaging setup

### 2. The host must synchronize pointer lifetime
`LV2Plugin` is internally careful with atomics and worker teardown, but it does **not** protect your external `LV2Plugin*` from being replaced concurrently.

You still need host-level locking or a lock-free ownership design.

### 3. `max_block_length` must match reality
Instantiate with a maximum block size large enough for your callback pattern.

If you pass larger blocks later than the value used during construction, you can break plugin assumptions.

### 4. File parameters require real filesystem paths
If your app uses document providers or content URIs, you may need to copy the selected file into local storage first.

That is exactly what Android hosts often need to do.

### 5. The host may need a metadata layer in addition to `LV2Plugin`
`LV2Plugin` is runtime-oriented. If you need labels, ranges, enumerations, authors, file-type hints, etc., do what `initPlugins()` does and build a separate discovery/metadata pass.

### 6. Multi-bus plugins may need adaptation
The current implementation assumes a simple audio-port mapping model. Verify audio-port topology before dropping it into a DAW-style host.

### 7. `ports_` is practical but host-coupling-heavy
`opiqo-multi` uses `ports_` directly for parameter updates and preset export. That is convenient, but it couples the host to the wrapper internals.

For a cleaner reusable integration, prefer:
- `getControl(symbol)` for writes
- host-side metadata caches for UI
- a narrower abstraction around preset export/import

## Minimal Reuse Recipe

If you want to lift this into another project with minimal friction, the host flow should look like this:

```cpp
LilvWorld* world = lilv_world_new();
// configure LV2 path
lilv_world_load_all(world);

LV2Plugin* plugin = new LV2Plugin(world, pluginUri, sampleRate, maxBlockLength);
if (!plugin->initialize()) {
    delete plugin;
    plugin = nullptr;
    return false;
}

plugin->start();

// audio callback
plugin->process(inBuffer, outBuffer, framesOrSamplesUsedByYourHost);

// UI thread examples
if (PluginControl* gain = plugin->getControl("gain")) {
    gain->setValue(0.75f);
}

plugin->send_path_parameter(propertyUri, absolutePath);

// shutdown
plugin->stop();
plugin->closePlugin();
delete plugin;
```

## How `opiqo-multi` Extends It Into a Full Host

The bigger architecture around `LV2Plugin` is what makes it reusable:

1. `LiveEffectEngine` owns the shared Lilv world and active plugin pointers.
2. `jni_bridge.cpp` performs discovery, loading, deletion, preset export, and UI-triggered mutations.
3. `FullDuplexPass` adapts hardware callback sizes and runs the serial plugin chain.
4. `UI.java` uses discovered metadata to build controls dynamically.

If you reuse only `LV2Plugin.hpp`, expect to rebuild those surrounding layers in forms appropriate for your platform.

## Best Fit for This Wrapper

`LV2Plugin.hpp` is a strong fit for hosts that are:
- single-plugin players
- pedalboard / serial-chain hosts
- embedded audio apps
- mobile audio apps with a small number of plugins
- tools that need file-path atom support and worker support without writing a full LV2 host from scratch

It is a less direct fit for hosts that need:
- arbitrary bus routing
- sample-accurate multi-port graph scheduling
- a strong separation between public API and host internals

## Reference Files in This Repo

Use these as the concrete examples of integration:
- `app/src/main/cpp/LiveEffectEngine.cpp` — shared-world setup and stream start
- `app/src/main/cpp/LiveEffectEngine.h` — ownership of plugin slots + mutex
- `app/src/main/cpp/FullDuplexPass.h` — real-time chaining pattern
- `app/src/main/cpp/jni_bridge.cpp` — plugin construction, deletion, parameter writes, presets, file-path messages
- `app/src/main/java/org/acoustixaudio/opiqo/multi/UI.java` — dynamic control generation from discovered metadata

## Short Version

If you reuse `LV2Plugin.hpp`, treat it as a **well-equipped plugin instance wrapper**, not a complete host.

The winning pattern from this project is:
- keep one shared `LilvWorld`
- create one `LV2Plugin` per loaded instance
- call `initialize()`/`start()` before publishing it
- call `process()` only from the audio thread
- guard pointer lifetime with a host mutex
- use `closePlugin()` before `delete`
- build plugin discovery/UI/preset logic outside the wrapper


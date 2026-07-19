# AGENTS.md - Opiqo Multi Effects Processor

Guide for AI coding agents working on this LV2 plugin host for Android.

## Project Overview

**opiqo-multi** is a professional-grade **Guitar Multi-Effects Processor** for Android that hosts and chains up to 4 **LV2 (LADSPA Version 2)** audio plugins simultaneously. It combines:
- **Google Oboe** for real-time, low-latency audio I/O
- **Lilv/LV2** plugin discovery and management
- **Android JNI** bridge between Java UI and C++ native audio engine
- **54 bundled guitar effects plugins** (Guitarix-derived)

Key distinction: This is **not a simple audio app** — it's a professional plugin host with strict real-time constraints, multi-threaded synchronization, and complex state management.

## Critical Architecture Patterns

### 1. JNI/Java↔C++ Boundary
**Location:** `app/src/main/java/org/acoustixaudio/opiqo/multi/AudioEngine.java` (Java declarations) ↔ `app/src/main/cpp/jni_bridge.cpp` (C++ implementations)

**Pattern:**
- `AudioEngine.java` declares all native methods as `static native`
- `jni_bridge.cpp` implements each as `JNIEXPORT ... JNICALL Java_org_acoustixaudio_opiqo_multi_AudioEngine_<method>`
- **Golden Rule:** Every JNI call must check `if (engine == nullptr)` and log errors via `LOGE()` macro

**Common Calls:**
- `create()` → instantiate `LiveEffectEngine`
- `addPlugin(position, uri)` → load LV2 plugin into slot (1–4)
- `setValue(plugin, index, value)` → set control parameter
- `setPluginEnabled(plugin, bool)` → enable/disable slot without unloading
- `deletePlugin(plugin)` → unload and free plugin

### 2. Real-Time Audio Thread Safety (Critical!)
**Location:** `app/src/main/cpp/FullDuplexPass.h` (audio callback) ↔ `jni_bridge.cpp` (JNI modifications)

**Pattern:**
- **RT Thread (Oboe callback):** Runs in `FullDuplexPass::onBothStreamsReady()` — processes audio through 4 plugin slots
- **UI/JNI Thread:** Modifies plugins via `addPlugin()`, `deletePlugin()`, `setPluginEnabled()`
- **Synchronization:** `std::mutex pluginMutex` (owned by `LiveEffectEngine`, guarded with `std::lock_guard`)

**DO:** Always lock before accessing plugin pointers in JNI:
```cpp
std::lock_guard<std::mutex> lock(engine->pluginMutex);
// Access plugin1, plugin2, etc.
```

**DON'T:** Call `malloc`, `free`, or Lilv functions from RT callback. Use atomic stores for control values.

### 3. LV2Plugin Wrapper Class
**Location:** `app/src/main/cpp/LV2Plugin.hpp`

**Purpose:** Generic, real-time-safe LV2 plugin wrapper. Handles:
- Port discovery (audio, control, atom ports)
- Control management (`setValue`, `getValue`, `reset`)
- Atom message ringbuffers (UI→DSP, DSP→UI)
- State save/load (via Lilv TTL)
- Worker thread management (automatic for plugins with `LV2_Worker_Interface`)

**Usage Pattern:**
```cpp
LV2Plugin plugin(world, lilv_plugin, sample_rate_48000, block_size_256);
if (!plugin.initialize()) { /* error */ }
plugin.start();
plugin.process(input_buffer, output_buffer, num_frames);  // In RT callback
plugin.stop();
```

**RT-Safe Methods:** Only `process()` is safe from audio callback. Parameter changes go through `getControl()->setValue()` from UI thread (uses lock-free ringbuffer internally).

### 4. Preset System
**Location:** `app/src/main/java/org/acoustixaudio/opiqo/multi/MainActivity.java` (preset loading/saving)

**Pattern:** Presets are JSON files containing plugin state. Each plugin has:
- Plugin URI
- Enabled/disabled flag
- Control parameter values
- Per-plugin state (via `saveState()`/`loadState()`)

**Key Methods:**
- `getPreset(int plugin)` → JSON string of plugin state (includes URI, controls, parameters)
- `getPresetList()` → JSON array of all saved presets

### 5. UI Dynamism
**Location:** `app/src/main/java/org/acoustixaudio/opiqo/multi/UI.java`

**Pattern:** Sliders and controls are built **dynamically** at runtime from `getPluginInfo()` JSON response:
- Query native: `AudioEngine.getPluginInfo()` → JSON with all plugin metadata
- Parse ports: audio, control (min/max/default), atom
- Build sliders with correct ranges and labels
- Wire slider changes to `AudioEngine.setValue(plugin, portIndex, value)`

**Implication:** Never hardcode UI elements for plugins. All plugin UIs are data-driven.

## Threading Model

| Thread | Role | Key Methods | RT-Safe? |
|--------|------|-------------|----------|
| **Main/UI** | Android lifecycle, button clicks, preset UI | `MainActivity`, `SettingsActivity`, UI callbacks | ❌ Blocks allowed |
| **Audio RT** | Oboe full-duplex callback, processes 4 plugins | `FullDuplexPass::onBothStreamsReady()`, `LV2Plugin::process()` | ✅ NO blocking, NO I/O, NO malloc |
| **Worker** | LV2 worker tasks (non-RT plugin jobs) | Managed by `LV2Plugin`, automatic | ⚠️ Limited I/O allowed |

**Critical Sync Points:**
1. Plugin pointer access: protected by `pluginMutex` (lock in JNI, lock in RT callback)
2. Parameter updates: lock-free ringbuffer in `LV2Plugin` (atomic stores)
3. Atom messages (UI↔DSP): ringbuffer reads/writes (acquire/release semantics)

## Build & Development Workflows

### Build Commands
```bash
# Clean build
./gradlew clean

# Debug APK (unoptimized, full symbols)
./gradlew assembleDebug

# Release APK
./gradlew assembleRelease

# Install on device/emulator
./gradlew installDebug

# Unit tests
./gradlew test

# Instrumented tests (on device)
./gradlew connectedAndroidTest
```

### CMake & NDK
- **CMake version:** 3.22.1+ (configured in `build.gradle`)
- **C++ Standard:** `-std=c++17` (set in `CMakeLists.txt`)
- **Architectures:** Auto-compiled for armeabi-v7a, arm64-v8a, x86, x86_64
- **Prebuilt libraries:** `app/src/main/libs/<ABI>/` (Lilv, Oboe, libsndfile, etc.)

**Rebuild native code only:**
```bash
./gradlew assembleDebug --rerun-tasks  # Forces CMake rebuild
```

### Adding New LV2 Plugins
1. Cross-compile `.so` for target ABIs → place in `app/src/main/libs/<ABI>/`
2. Create bundle directory: `app/src/main/assets/lv2/<PluginName>.lv2/`
3. Add `manifest.ttl` and plugin `.ttl` metadata file
4. Rebuild app — Lilv discovers automatically

**No C++/Java code changes needed.**

## Logging & Debugging

### C++ Logging
**Macros in `app/src/main/cpp/logging_macros.h`:**
```cpp
LOGV(...)  // Verbose
LOGD(...)  // Debug
LOGI(...)  // Info
LOGW(...)  // Warning
LOGE(...)  // Error
```

**Use in native code:** All errors in JNI functions must be logged via `LOGE()`. Check logcat:
```bash
adb logcat | grep opiqo
```

### Common Error Patterns
1. **"Engine is null"** → `AudioEngine.create()` not called before other JNI methods
2. **Plugin won't load** → Check Lilv plugin discovery; verify `.so` matches device ABI
3. **No audio output** → Check `setEffectOn(true)` called; verify input/output devices selected; check RECORD_AUDIO permission
4. **Crash in audio callback** → Likely malloc/lock in RT code; inspect `FullDuplexPass.h` and `LV2Plugin::process()`

## Key Files & Responsibilities

| File | Purpose |
|------|---------|
| `MainActivity.java` | App lifecycle, permissions, tab pager (4 pedal slots), plugin loading/deletion |
| `AudioEngine.java` | JNI declarations (interface to native C++) |
| `UI.java` | Dynamic slider/control builder from plugin metadata JSON |
| `LiveEffectEngine.h/cpp` | Owns Lilv world, 4 plugin slots, mutex, audio engine creation |
| `FullDuplexPass.h` | Oboe full-duplex stream callback; chains 4 plugins in series |
| `LV2Plugin.hpp` | Generic LV2 wrapper; handles ports, controls, state, atoms, workers |
| `jni_bridge.cpp` | Java↔C++ gateway; all `AudioEngine` native implementations |
| `CMakeLists.txt` | Native build config; links Lilv, Oboe, libsndfile, etc. |

## Conventions & Patterns

### Plugin Port Indexing
- Ports discovered by Lilv at `initialize()`
- Audio ports typically: input[0], output[1]
- Control ports: indices depend on plugin (query via `getPluginInfo()` JSON)
- **Always verify port types** before connecting buffers

### Control Value Ranges
- Float controls: `[min, max]` from Lilv metadata
- UI slider maps range to `[0.0, 1.0]` internally, then scales to plugin range
- Default value set at plugin load; resettable via `reset()`

### State Serialization
- Presets saved as JSON in app's `filesDir/presets/`
- Each preset JSON contains plugin URI + control values
- On import, app deserializes and calls `setPluginEnabled()`, `setValue()` for each plugin

### Error Handling Strategy
- **Java/JNI:** Check return values (`jboolean`, `jint`); log errors; don't crash UI
- **C++/RT:** No error handling (can't allocate/print in callback); use atomics for flags
- **Lilv queries:** Always null-check pointers returned by Lilv

## Testing Patterns

### Test.java
Contains developer helpers (`pluginLoader()`, `stressTest()`) for verifying plugin loading under stress. Called manually during development, not part of standard CI.

### Instrumented Tests
Run on device: `./gradlew connectedAndroidTest`. Tests UI interactions, permission flows, lifecycle.

### Unit Tests
Run on host: `./gradlew test`. For utilities, JSON parsing, etc. (minimal coverage currently).

## Performance Considerations

### Audio Latency
- Target: <20ms round-trip (Android high-latency platform)
- Oboe uses AAudio when available (low-latency mode)
- Mutex overhead: <1% CPU impact per audio frame
- Plugin chain: serial processing (P1→P2→P3→P4) accumulates latency

### Memory
- Plugins instantiated once and kept resident (even if bypassed)
- Preset exports JSON to `filesDir` (~5KB typical)
- Bundle copying on first launch: ~50MB to `filesDir/lv2/`

### Multithreading Overhead
- 1 audio RT thread (Oboe callback)
- 1 worker thread per plugin with `LV2_Worker_Interface`
- JNI calls block until native method returns (no async callbacks)

## Gotchas & Anti-Patterns

❌ **DON'T:**
- Access plugin pointers without `pluginMutex` lock in JNI functions
- Allocate memory or call `printf` in `process()` or RT callback
- Call Lilv functions from audio callback
- Mix `lilv_node_free()` and Lilv ownership (check docs)
- Set slider values directly without going through `setValue()`

✅ **DO:**
- Always lock before reading/writing plugin pointers in JNI
- Use lock-free ringbuffers for atom messages
- Check `engine != nullptr` at start of every JNI function
- Reset and re-initialize Lilv world on app lifecycle transitions
- Export/import presets via Android document picker (permissions)

## Useful Links

- **LV2 Spec:** https://lv2plug.in/
- **Lilv Docs:** https://doxygen.drobilla.net/lilv/
- **Google Oboe:** https://github.com/google/oboe
- **LV2Plugin-Usage.md:** Full API documentation in `app/src/main/cpp/LV2Plugin-Usage.md`
- **MUTEX_IMPLEMENTATION.md:** Thread-safety design document

---

**Last Updated:** July 2026  
**Version:** 0.97  
**Target API:** Android 12+ (API 31+), Gradle 8.0+, AGP 9.3.0+


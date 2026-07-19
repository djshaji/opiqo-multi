# Migration Plan: Reuse Audio Flow + Dynamic Plugin UI

## Objectives

- Reuse the existing native audio host pipeline (Oboe + Lilv + LV2 chain).
- Reuse dynamic plugin UI generation (`UI.java` pattern) with minimal rewrite.
- Build a new app that ships only a small allowlist ("a couple") of plugins.
- Keep real-time safety guarantees from current implementation.

## Design Principles

- Keep `LV2Plugin.hpp` as a reusable plugin-runtime primitive.
- Keep JNI as a stable contract boundary (`AudioEngine` facade).
- Move host internals into libraries/modules, keep app layer thin.
- Restrict plugins in app policy/UI, not in RT engine logic (initially).
- Migrate in small safe steps; verify audio at every step.

## Target Architecture

### 1) Module Boundaries

- `audio-host-core` (Android library + native code)
  - Owns audio engine lifecycle, plugin slots, JNI bridge, RT callback pipeline.
  - Reuses:
    - `LiveEffectEngine.h/.cpp`
    - `FullDuplexPass.h`
    - `LV2Plugin.hpp`
    - `jni_bridge.cpp` (refactored to a stable API surface)
    - native helpers (`logging_macros.h`, `lv2_ringbuffer.h`, `LockFreeQueue.*`, `FileWriter.*`, etc.)

- `plugin-ui-kit` (Android library, Java)
  - Owns dynamic plugin UI and pedal-slot paging components.
  - Reuses/adapts:
    - `UI.java`
    - `CollectionFragment.java`
    - `CollectionAdapter.java`
    - `ObjectFragment.java`

- `preset-kit` (optional Android library)
  - Owns preset import/export schema and storage behavior.
  - Reuses parts of `MainActivity`/`SettingsActivity` preset logic.

- `new-app` (application module)
  - Branding, product logic, plugin allowlist, user flows.
  - No RT engine internals.

### 2) Runtime Data Flow

1. App starts, loads native lib from `audio-host-core`.
2. App copies LV2 bundles to app-private storage, calls `initPlugins(path)`.
3. UI queries `getPluginInfo()` and builds controls dynamically.
4. User actions call JNI facade:
   - `addPlugin(slot, uri)`
   - `setValue(slot, portIndex, value)`
   - `setPluginEnabled(slot, enabled)`
   - `setFilePath(slot, propertyUri, absPath)`
5. RT callback (`FullDuplexPass`) processes audio through chain under pointer-safety lock.
6. Preset layer serializes host snapshot and/or LV2 state depending on product choice.

### 3) Threading/Safety Contract (must stay unchanged)

- Audio RT thread: only `LV2Plugin::process()` and non-blocking work.
- UI/JNI thread: plugin load/remove, control writes, preset and file-path operations.
- Plugin pointer ownership protected by `pluginMutex` shared between JNI and callback.
- Keep bypass gate around plugin mutation (`bypass=true` during add/delete).

## Stable JNI Contract for Reuse

Keep this API shape in `audio-host-core`:

- Engine lifecycle: `create`, `delete`, `setEffectOn`, `setAPI`, `setPluginBlockSize`
- Discovery/metadata: `initPlugins`, `getPluginInfo`, `getWritables`
- Plugin chain control: `addPlugin`, `deletePlugin`, `setPluginEnabled`, `setValue`, `bypass`
- File/atom params: `setFilePath`
- Presets/state: `getPreset`, `getPresetList`, `printPreset`
- Devices/gain/recording: `setRecordingDeviceId`, `setPlaybackDeviceId`, `setGain`, recording APIs

This lets `plugin-ui-kit` and `new-app` remain mostly engine-agnostic.

## Concrete Module Layout (Gradle)

```text
opiqo-next/
  settings.gradle
  build.gradle
  gradle/libs.versions.toml

  app/                               # new product app
    build.gradle
    src/main/java/com/example/newapp/
      MainActivity.java
      SettingsActivity.java
      AppPluginPolicy.java
      AppPresetCoordinator.java
    src/main/res/

  audio-host-core/                   # reusable host SDK (Android library)
    build.gradle
    src/main/java/org/acoustixaudio/audiohost/
      AudioEngineFacade.java         # Java JNI facade (stable API)
      AudioEngineConfig.java
    src/main/cpp/
      CMakeLists.txt
      jni_bridge.cpp
      LiveEffectEngine.cpp/.h
      FullDuplexPass.h
      LV2Plugin.hpp
      lv2_ringbuffer.h
      logging_macros.h
      FileWriter.cpp/.h
      LockFreeQueue.cpp/.h
      ...native dependencies glue...
    src/main/assets/lv2/             # optional shared default bundles

  plugin-ui-kit/                     # reusable dynamic UI SDK (Android library)
    build.gradle
    src/main/java/org/acoustixaudio/pluginui/
      PluginScreenFragment.java      # adapted from CollectionFragment/ObjectFragment
      PluginPagerAdapter.java        # adapted from CollectionAdapter
      DynamicPluginView.java         # adapted from UI.java
      PluginUiContract.java          # callbacks to host/app
      FilePickerContract.java
      PluginMetadataParser.java
    src/main/res/layout/
      ...ported plugin UI layouts...

  preset-kit/                        # optional reusable preset support
    build.gradle
    src/main/java/org/acoustixaudio/preset/
      PresetStore.java
      PresetJsonSchema.java
      PresetImportExport.java

  docs/
    Migration-Plan.md
```

## Java Package Strategy

- Keep reusable code in neutral packages:
  - `org.acoustixaudio.audiohost`
  - `org.acoustixaudio.pluginui`
  - `org.acoustixaudio.preset`
- Keep app-specific code in product namespace:
  - `com.example.newapp`

## Plugin Restriction Strategy ("couple of plugins")

- Do not change RT engine first.
- Add allowlist in app/picker layer:
  - Filter `getPluginInfo()` to allowed URIs.
  - Prevent adding non-allowlisted URI in app logic.
- Optionally enforce allowlist in JNI as secondary safety check later.

## Migration Phases

### Phase 0: Baseline Snapshot

- Copy current app and ensure it builds/runs unchanged.
- Record baseline behavior (plugin load, control movement, preset save/load, audio start/stop).

### Phase 1: Extract `audio-host-core`

- Move native/JNI engine files into library module.
- Keep API names compatible with existing `AudioEngine` calls.
- Wire old app to use new module without UI changes.
- Validate audio and plugin operations.

### Phase 2: Extract `plugin-ui-kit`

- Move `UI.java` and pager/fragments into reusable library.
- Introduce `PluginUiContract` callbacks for app integration.
- Keep dynamic JSON-driven control building unchanged.
- Validate slider/toggle/dropdown/file-path flows.

### Phase 3: Build `new-app`

- Create thin app shell with branding + allowlist + preset policy.
- Show only desired slot count in UI (e.g., 2 slots) while backend may still support 4.
- Validate end-to-end with your selected plugins.

### Phase 4: Optional Internal Simplification

- If needed, reduce native slot count from 4 to 2.
- Only do this after stable release candidate and regression tests.

## Interface Contracts Between Modules

### `plugin-ui-kit` -> app

- `onPluginSelected(slot, uri)`
- `onDeletePlugin(slot)`
- `onSetControlValue(slot, index, value)`
- `onSetFilePath(slot, propertyUri, absPath)`
- `onSetPluginEnabled(slot, enabled)`

### app -> `audio-host-core`

- Forward callbacks to JNI facade methods.
- Provide app-specific file handling and permissions.

## Testing Matrix (required before each phase completion)

- Engine lifecycle: create/delete/start/stop repeated 50+ times.
- Plugin lifecycle under load: add/delete repeatedly while audio running.
- UI control stress: rapid slider moves across both plugins.
- File-path atom params: select/change file repeatedly and verify plugin receives updates.
- Preset round-trip: export/import and immediate reload.
- Device route changes: input/output device switch while effect is off/on.

## Risks and Mitigations

- JNI API drift breaks UI kit
  - Mitigation: freeze facade interface and version it.
- RT regressions due to refactor
  - Mitigation: no behavior changes in Phase 1; refactor layout first, logic later.
- Over-coupling to `ports_` internals
  - Mitigation: gradually add symbol-based control APIs while keeping index path for compatibility.

## Deliverables

- `audio-host-core` reusable module
- `plugin-ui-kit` reusable module
- optional `preset-kit`
- `new-app` consuming modules with plugin allowlist
- migration test checklist + release gate criteria

## Success Criteria

- New app can run two selected plugins with same low-latency flow.
- Dynamic controls are generated from metadata without hardcoded plugin UI.
- No RT thread crashes/races during add/delete/parameter updates.
- Engine module can be reused in another app with minimal changes.

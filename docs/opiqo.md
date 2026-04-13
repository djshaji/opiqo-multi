## Project Overview: Opiqo Multi Effects Processor

**Opiqo Multi** is an open-source **Guitar Multi-Effects Processor** for Android. It hosts up to **4 LV2 (LADSPA Version 2)** audio plugins simultaneously in a swipeable pedal-board UI, giving guitarists a professional-grade effects chain directly on their Android device.

---

### Key Capabilities

* **Multi-Plugin Chain:** Up to 4 LV2 plugins run in series — Pedal 1 → Pedal 2 → Pedal 3 → Pedal 4.
* **Per-Pedal Bypass:** Each loaded plugin has its own enable/disable toggle, letting you include or exclude it from the chain without losing its settings.
* **Dynamic Plugin Management:** Plugins are loaded and removed at runtime. Tap `+` on any empty pedal slot to pick from the bundled library; tap Delete to remove a loaded plugin.
* **Real-time Parameter Control:** Every LV2 control port is exposed as a labelled slider. Values update the plugin in real time during audio processing.
* **Audio Device Selection:** Choose input (microphone / audio interface) and output (speaker / headphones) devices independently at runtime.
* **Master Volume:** A global gain slider scales the final output level.
* **54 Bundled Guitar Effects:** Ships with a curated collection of Guitarix-derived distortion, overdrive, fuzz, modulation, and amp-simulator plugins — no extra downloads needed.

---

### Why Opiqo Matters

Android's audio stack has historically been challenging for low-latency, professional-grade plugin hosting. Opiqo Multi addresses this by combining Google's **Oboe** library for low-latency full-duplex audio with the **Lilv** LV2 host library and a custom `LV2Plugin` C++ wrapper that is real-time safe.

| Feature | Description |
| --- | --- |
| **Multi-Pedal UI** | Swipeable tabs (Pedal 1–4), each hosting one LV2 plugin slot |
| **Plugin Discovery** | Bundles are copied from APK assets to app storage; Lilv scans them at startup |
| **Parameter Control** | Per-port sliders with min/max/default from the plugin's LV2 metadata |
| **Plugin Bypass** | Per-plugin `Switch` widget in each plugin's header row |
| **State Management** | `LV2Plugin::saveState` / `loadState` via Lilv TTL serialization |
| **Cross-Architecture** | NDK compilation for armeabi-v7a, arm64-v8a, x86, x86_64 |

---

### High-Level Architecture

```
[Android UI Layer]
  MainActivity  ─── CollectionFragment ─── CollectionAdapter
                         │
                 ObjectFragment × 4         (one per pedal slot)
                         │
                      UI.java               (builds sliders + bypass + delete)

[JNI Boundary]
  AudioEngine.java  ←→  jni_bridge.cpp

[Native Layer]
  LiveEffectEngine  ←→  FullDuplexPass      (Oboe full-duplex stream)
       │
  LV2Plugin × 4                             (plugin1 … plugin4)
       │
  Lilv + LV2 library stack
```

> **Audio thread safety:** `LV2Plugin::process()` is the only method called from the Oboe RT callback. Parameter changes from the UI thread use atomic stores; atom messages use a lock-free ringbuffer.

---

### Getting Started

1. **Clone the Repository** and open in Android Studio.
2. **Build:** `./gradlew assembleDebug`
3. **Install:** `./gradlew installDebug`
4. **Grant** the `RECORD_AUDIO` permission when prompted.
5. **Enable** the Power toggle to start audio processing.
6. **Add Plugins:** Tap `+` on any pedal tab, pick a plugin from the list.
7. **Adjust Parameters:** Move the sliders under the plugin name.
8. **Bypass / Delete:** Use the toggle to bypass or the Delete button to remove.

---

### Adding Your Own LV2 Plugins

1. Cross-compile the plugin `.so` for Android ABIs (armeabi-v7a, arm64-v8a, …).
2. Place `.so` files in `app/src/main/jniLibs/<ABI>/`.
3. Create the `.lv2` bundle directory in `app/src/main/assets/lv2/` with `manifest.ttl` and the plugin `.ttl` file.
4. Rebuild — the plugin appears in the picker automatically.

---

### Internal API Highlights

* **`LV2Plugin`** (`cpp/LV2Plugin.hpp`): Generic LV2 wrapper. Handles port discovery, control management, atom ringbuffers, worker threads, and state save/load. See [`LV2Plugin-Usage.md`](app/src/main/cpp/LV2Plugin-Usage.md) for full API docs.
* **`LiveEffectEngine`** (`cpp/LiveEffectEngine.h/.cpp`): Oboe full-duplex engine; owns `plugin1`–`plugin4` pointers and the `gain` float.
* **`jni_bridge.cpp`**: All `AudioEngine` JNI methods — `create`, `setEffectOn`, `addPlugin`, `deletePlugin`, `setPluginEnabled`, `setValue`, `setGain`, `initPlugins`, `getPluginInfo`, and device ID setters.
* **`UI.java`**: Dynamically builds the per-plugin control panel from the JSON port metadata returned by `AudioEngine.getPluginInfo()`.

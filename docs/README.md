# opiqo Guitar Multi Effects Pedal for Android

<img width="256" height="256" alt="logo" src="https://github.com/user-attachments/assets/7d71d06e-968d-41aa-9aac-ce96d500d65b" />

A professional-grade **Guitar Multi-Effects Processor** for Android that hosts and runs **LV2 plugins**. Transform your mobile device into a powerful audio processing studio with real-time effects using industry-standard LV2 plugin format.

## Features

### Core Capabilities
- **Floating point precision audio processing** for high-quality sound
- **32 bit wav / flac file recording support** for lossless audio recording and playback
- **LV2 Plugin Host**: Full support for standard LV2 (LADSPA Version 2) audio plugins
- **Real-time Audio Processing**: Low-latency audio I/O using Google's Oboe library
- **Guitar Effects**: Process guitar audio through a chain of up to 4 LV2 effects plugins simultaneously
- **Native Performance**: Built with C++ for efficient audio processing
- **Master On/Off Control**: Toggle to enable/disable the entire effects chain

### Multi-Pedal Chain
- **4 Independent Pedal Slots**: Swipe between 4 pedal tabs, each hosting one LV2 plugin
- **Per-Pedal Enable/Disable**: Each pedal has its own bypass toggle to include or exclude it from the signal chain without removing it
- **Dynamic Plugin Loading**: Tap the `+` button on any empty pedal slot to pick and load a plugin from the bundled library
- **Plugin Deletion**: Remove a loaded plugin from any slot with the Delete button, restoring the slot to empty
- **Real-time Parameter Control**: Sliders for every plugin control port, labelled and set to the plugin's default values

### Audio Device Management
- **Input Device Selection**: Spinner to choose the recording (input) audio device from all connected inputs
- **Output Device Selection**: Spinner to choose the playback (output) audio device from all connected outputs
- **Master Volume**: Slider to adjust the overall output gain

### Audio Recording
- **Multi-Format Recording**: Record processed audio output directly from the app in your preferred format
- **Supported Formats**: WAV (lossless), MP3, Opus, FLAC (lossless), OGG Vorbis
- **Post-FX Capture**: Recordings capture the full processed signal — after all active pedal effects and master volume
- **On-Device Storage**: Recordings are saved to the device's storage for easy access and sharing

### Preset Management
- **Preset Backup and Restore**: Export all saved presets to a single JSON file from the Settings screen
- **System Document Picker Integration**: Choose any writable location when exporting and any readable JSON file when importing
- **Automatic Preset Reload**: Imported presets are written into the app's presets directory and reloaded immediately

### Technical Features
- **Google Oboe Integration**: Modern, low-latency audio API for Android
- **NDK Support**: High-performance native code compilation
- **LV2 Plugin Standards**: Compatible with standard LV2 plugins (JACK, Lilv, Sord libraries)
- **LV2 buf-size Compliance**: Correctly advertises `bufsz:boundedBlockLength` with `minBlockLength`, `maxBlockLength`, `nominalBlockLength`, and `sequenceSize` options sourced from the real Oboe burst size at runtime
- **Multi-threaded Audio**: Efficient full-duplex background audio processing via `FullDuplexPass`
- **Android 12+**: Optimized for modern Android devices (API level 31+)
- **54 Bundled Plugins**: Ships with a curated library of guitar effects ready to use out of the box

## Requirements

### System Requirements
- **Android**: API 31 (Android 12) or higher
- **Java**: JDK 11+
- **Gradle**: Compatible with Gradle 8.0+
- **Android NDK**: For native code compilation
- **CMake**: Version 3.22.1 or higher

### Dependencies
- **Google Oboe**: Real-time audio I/O library (v1.10.0)
- **LV2 Plugin Libraries**:
  - JACK Audio Connection Kit (`libjack`)
  - Lilv (LV2 plugin host library)
  - Sord (Data storage library)
  - Serd (Data serialization library)
  - Zix (Utility library)
  - Sratom (Atom serialization)

## Installation

### Prerequisites
1. Android Studio (Latest stable version recommended)
2. Android SDK with API 31+ support
3. Android NDK installation
4. CMake 3.22.1+

### Build Steps

1. **Clone the repository**
   ```bash
   git clone https://github.com/yourusername/opiqo-multi.git
   cd opiqoGuitarMultiEffectsProcessor
   ```

2. **Configure local.properties** (if not present)
   ```bash
   echo "sdk.dir=/path/to/android/sdk" > local.properties
   echo "ndk.dir=/path/to/android/ndk" >> local.properties
   ```

3. **Verify Gradle Wrapper**
   Ensure you're using the included Gradle wrapper (not a system-wide installation):
   ```bash
   chmod +x ./gradlew
   ```

4. **Build the project**
   ```bash
   ./gradlew build
   ```
   
   > **Note on Oboe Configuration**: The project uses Google's Oboe library (v1.10.0) which is fetched as a Gradle dependency with prefab support. The `build.gradle` includes `buildFeatures { prefab true }` to enable CMake to discover Oboe via `find_package(oboe REQUIRED CONFIG)`. If you encounter CMake errors about finding Oboe, ensure:
   > - Gradle sync completes successfully
   > - The Oboe prefab package is downloaded (check `~/.gradle/caches/transforms-*/`)
   > - You're using Android Gradle Plugin 9.0.0+ (supports prefab v2)

5. **Generate APK**
   ```bash
   ./gradlew assembleDebug    # For debug build
   ./gradlew assembleRelease  # For release build
   ```

6. **Install on device/emulator**
   ```bash
   ./gradlew installDebug
   ```

## Architecture

### Project Structure
```
opiqo-multi/
├── app/
│   ├── src/
│   │   ├── main/
│   │   │   ├── java/org/acoustixaudio/opiqo/multi/
│   │   │   │   ├── MainActivity.java          # Main UI activity; permission handling, plugin loading
│   │   │   │   ├── SettingsActivity.java      # Settings UI; audio options and preset import/export
│   │   │   │   ├── AudioEngine.java           # JNI declarations for the native audio engine
│   │   │   │   ├── CollectionFragment.java    # Tab-pager fragment (hosts 4 pedal tabs)
│   │   │   │   ├── CollectionAdapter.java     # ViewPager2 adapter (4 pedal slots)
│   │   │   │   ├── ObjectFragment.java        # Single pedal slot fragment
│   │   │   │   ├── UI.java                    # Dynamic plugin UI builder (sliders, bypass, delete)
│   │   │   │   └── Test.java                  # Developer test helpers
│   │   │   ├── cpp/
│   │   │   │   ├── CMakeLists.txt             # Native build configuration
│   │   │   │   ├── multi.cpp                  # Main native audio processor
│   │   │   │   ├── jni_bridge.cpp             # Java/Native JNI interface
│   │   │   │   ├── LiveEffectEngine.cpp       # Full-duplex audio engine (Oboe)
│   │   │   │   ├── LiveEffectEngine.h         # Engine header; holds plugin1–plugin4 slots
│   │   │   │   ├── FullDuplexPass.h           # Full-duplex Oboe stream helper
│   │   │   │   ├── LV2Plugin.hpp              # Generic LV2 plugin wrapper (controls, state, RT)
│   │   │   │   ├── LV2OboeHost.hpp            # Single-plugin Oboe host (output only)
│   │   │   │   ├── LV2Plugin-Usage.md         # LV2Plugin API usage guide
│   │   │   │   ├── jalv.cpp / jalv.h          # JACK/LV2 host utilities
│   │   │   │   ├── lv2_ringbuffer.h           # Lock-free ringbuffer for atom messages
│   │   │   │   ├── json.hpp                   # nlohmann/json (single-header)
│   │   │   │   ├── kitty.cpp                  # Utility helpers
│   │   │   │   └── logging_macros.h           # Android NDK logging helpers
│   │   │   ├── assets/lv2/                    # 54 bundled LV2 plugin bundles
│   │   │   ├── res/
│   │   │   │   ├── layout/
│   │   │   │   │   ├── activity_main.xml      # Root layout: pager + control bar
│   │   │   │   │   ├── pager.xml              # TabLayout + ViewPager2
│   │   │   │   │   └── plugin.xml             # Pedal slot: plugin UI container + add button
│   │   │   │   └── ...
│   │   │   └── AndroidManifest.xml
│   │   ├── androidTest/                       # Instrumented tests
│   │   └── test/                              # Unit tests
│   ├── build.gradle                           # App-level build configuration
│   └── proguard-rules.pro                     # ProGuard rules
├── gradle/
│   ├── libs.versions.toml                     # Centralized dependency management
│   └── wrapper/
├── build.gradle                               # Project-level build configuration
├── settings.gradle                            # Gradle settings
├── opiqo.md                                   # Project overview document
└── README.md                                  # This file
```

### Audio Processing Pipeline

```
Guitar / Microphone Input
         ↓
    [Oboe Audio API]          ← Low-latency full-duplex stream
         ↓
   [JNI Bridge Layer]         ← Java ↔ C++ boundary
         ↓
 [LiveEffectEngine / FullDuplexPass]
         ↓
  [LV2Plugin Slot 1 (Pedal 1)]  ← bypassed if disabled
         ↓
  [LV2Plugin Slot 2 (Pedal 2)]  ← bypassed if disabled
         ↓
  [LV2Plugin Slot 3 (Pedal 3)]  ← bypassed if disabled
         ↓
  [LV2Plugin Slot 4 (Pedal 4)]  ← bypassed if disabled
         ↓
   [Master Gain (volume)]
         ↓
    [Output Speaker / Headphones]
```

### LV2 Plugin State & Atom Messaging

The `LV2Plugin` class manages plugin state through:
- **Parameter Ports**: Control inputs for plugin parameters (sliders)
- **Audio Ports**: Input and output audio buffers
- **Atom Ports**: Event/message communication for dynamic control (e.g., file paths)

When sending file paths or other atom messages to plugins:
1. **Write atom sequence** to the ring buffer with file path and port index
2. **LV2Plugin reads** the message during real-time processing
3. **Plugin processes** the atom message in the next audio cycle
4. **State persists** until a new atom message is received

> **Note on File Loading**: If a plugin loads an external file (via atom message), ensure you only send the file path once per file change. Some plugins may continue to reference the loaded file state. Clear or reset the file path message only when intentionally unloading the file.

## Usage

### Getting Started

1. **Launch the App**
   - Open the installed app on your Android device
   - The app will request `RECORD_AUDIO` permission on first run
   - Grant the permission to enable audio processing

2. **Enable the Effects Chain**
   - Press the **Power** toggle at the bottom of the screen to start audio processing
   - When enabled, audio is captured from the selected input device, passed through any loaded plugins, and sent to the selected output device

3. **Load a Plugin onto a Pedal Slot**
   - Each of the 4 pedal tabs (Pedal 1–4) shows a `+` placeholder when empty
   - Tap the `+` area to open the plugin picker dialog
   - Select any plugin from the bundled library to load it into that slot
   - The plugin's parameter sliders appear immediately after loading

4. **Adjust Plugin Parameters**
   - Each loaded plugin displays a slider for every controllable port
   - Move a slider to change the parameter value in real time
   - The slider range and default value come directly from the plugin's LV2 metadata

5. **Enable or Bypass a Pedal**
   - Use the toggle switch in the plugin header to enable or bypass that pedal
   - Bypass removes the plugin from the signal chain without unloading it

6. **Remove a Plugin**
   - Tap the **Delete** button at the bottom of a loaded plugin to unload it
   - The pedal slot returns to the empty `+` state

7. **Select Audio Devices**
   - Use the **Input Device** spinner to choose the microphone or audio interface input
   - Use the **Output Device** spinner to route audio to a specific output (speaker, headphones, etc.)

8. **Adjust Master Volume**
   - Move the **Volume** slider (bottom bar) to scale the overall output level

9. **Open Settings**
   - Tap the **Settings** button on the main screen to open the preferences screen
   - Use this screen to adjust audio-related options and manage preset import/export

10. **Export Presets**
   - In **Settings > Presets**, tap **Export**
   - Choose where to save the generated `presets.json` file using Android's document picker
   - The exported file contains all saved presets from the app's internal presets directory

11. **Import Presets**
   - In **Settings > Presets**, tap **Import**
   - Pick a previously exported JSON file
   - The app restores each preset file and reloads the preset list immediately

### UI Components
- **Power Toggle**: Master switch to start/stop the audio engine and all plugin processing
- **Volume Slider**: Controls master output gain
- **Pedal Tabs (Pedal 1–4)**: Swipeable tabs, each hosting one LV2 plugin slot
- **Plugin `+` Button**: Opens the plugin selection dialog for an empty slot
- **Plugin Bypass Switch**: Per-plugin enable/disable toggle in each plugin's header
- **Plugin Parameter Sliders**: One slider per control port; labels and ranges from the plugin
- **Delete Button**: Removes and unloads the plugin from its slot
- **Input / Output Device Spinners**: Select which audio hardware to use
- **Settings Button**: Opens the preferences screen for audio options and preset management

## Permissions

The app requires the following Android permissions:

- **RECORD_AUDIO**: Necessary to capture guitar audio input from the device microphone

These permissions are requested at runtime on Android 6.0 (API level 23) and above.

**Permission Request Timing**: The `RECORD_AUDIO` permission is requested when the user first attempts to:
1. Tap the Power toggle to start the audio engine
2. Load a plugin onto a pedal slot (if permission not yet granted)

> **Important**: Permission requests must be registered before the Activity reaches `STARTED` state. If you encounter the error `LifecycleOwner is attempting to register while current state is RESUMED`, ensure that permission request launchers are registered in `onCreate()` or during Activity initialization, not during user interactions after the Activity is already running.

## Bundled LV2 Plugins

The app ships with 54 guitar effects plugins in `app/src/main/assets/lv2/`. They are automatically copied to the app's private storage on first launch.

### Distortion / Overdrive / Fuzz
| Plugin | Description |
|--------|-------------|
| FatFrog | Fuzz effect |
| GxAxisFace | Axis Face fuzz |
| GxBaJaTubeDriver | Tube driver overdrive |
| GxBoobTube | Boob Tube overdrive |
| GxBottleRocket | Bottle Rocket distortion |
| GxClubDrive | Club Drive overdrive |
| GxCreamMachine | Cream Machine distortion |
| GxDOP250 | DOD 250 Overdrive Preamp |
| GxEpic | Epic distortion |
| GxEternity | Eternity overdrive |
| GxFz1b | Maestro FZ-1B fuzz |
| GxFz1s | Maestro FZ-1S fuzz |
| GxGuvnor | Guvnor distortion |
| GxHeathkit | Heathkit distortion |
| GxHotBox | Hot Box overdrive |
| GxHyperion | Hyperion fuzz |
| GxKnightFuzz | Knight Fuzz |
| GxLiquidDrive | Liquid Drive overdrive |
| GxLuna | Luna overdrive |
| GxMicroAmp | Micro Amp clean boost |
| GxPlexi | Plexi amp sim |
| GxSD1 | SD-1 Super Overdrive sim |
| GxSD2Lead | SD-2 Lead distortion sim |
| GxSaturator | Saturator |
| GxShakaTube | Shaka Tube overdrive |
| GxSloopyBlue | Sloopy Blue overdrive |
| GxSupersonic | Supersonic distortion |
| GxSuppaToneBender | Supra Tone Bender fuzz |
| GxTimRay | Tim Ray overdrive |
| GxToneMachine | Tone Machine fuzz |
| GxTubeDistortion | Tube Distortion |
| GxVintageFuzzMaster | Vintage Fuzz Master |
| GxVoodoFuzz | Voodoo Fuzz |
| bluesbreaker | Blues Breaker overdrive |
| gx_bajatubedriver | Baja Tube Driver (alt bundle) |
| gx_sloopyblue | Sloopy Blue (alt bundle) |

### Modulation / Filter / Special Effects
| Plugin | Description |
|--------|-------------|
| GxQuack | Auto-wah / quack filter |
| GxSlowGear | Slow-gear volume swell |
| GxSunFace | Sun Face fuzz/sustainer |
| GxSuperFuzz | Super Fuzz |
| GxUVox720k | UniVox 720k effect |
| GxVmk2 | VMK2 effect |
| LittleFly | Little Fly effect |
| ModularAmpToolKit | Modular amp toolkit |

### Amp Simulators
| Plugin | Description |
|--------|-------------|
| GxBlueAmp | Blue amp simulator |
| GxSVT | Ampeg SVT bass amp sim |
| GxUltraCab | Ultra cabinet simulator |
| GxVBassPreAmp | V-Bass preamp simulator |
| GxValveCaster | Valve Caster amp sim |
| Ratatouille | Amp/cab simulator |
| VintageAC30 | Vintage AC30 amp sim |
| VintageTubeOverdrive | Vintage tube overdrive |
| XDarkTerror | Dark Terror amp sim |
| XTinyTerror | Tiny Terror amp sim |

## Development

### Adding New LV2 Plugin Bundles

To bundle additional LV2 plugins with the app:

1. **Compile the Plugin for Android**
   - Cross-compile the plugin's `.so` library for each target ABI (armeabi-v7a, arm64-v8a, x86, x86_64) using the Android NDK
   - Ensure all dependencies are statically linked or also provided as `.so` files

2. **Create the LV2 Bundle**
   - Create a directory under `app/src/main/assets/lv2/` named `<PluginName>.lv2/`
   - Add the `manifest.ttl` (bundle manifest) and the plugin-specific `.ttl` metadata file
   - Add the compiled `.so` library to `app/src/main/jniLibs/<ABI>/`

3. **Rebuild the App**
   - Run `./gradlew assembleDebug`; the new plugin will be discovered automatically at runtime via Lilv

> **Note:** The app copies all bundles from `assets/lv2/` to the app's private `files/lv2/` directory on first launch and passes this path to Lilv for plugin discovery. No changes to Java or C++ code are needed to add new bundles.

### Querying LV2 Port Information with Lilv

The app uses the Lilv library to discover and query LV2 plugin metadata. Common operations:

**Get Port Information:**
```c++
LilvPlugin* plugin = lilv_plugins_get_by_uri(plugins, plugin_uri);
LilvPorts* ports = lilv_plugin_get_ports(plugin);

LILV_FOREACH(ports, i, ports) {
    LilvPort* port = lilv_ports_get(ports, i);
    const LilvNode* symbol = lilv_port_get_symbol(plugin, port);
    const LilvNode* name = lilv_port_get_name(plugin, port);
    int port_index = lilv_port_get_index(plugin, port);
    
    // Check port type (audio, control, atom, etc.)
    bool is_audio = lilv_port_is_a(plugin, port, node_AudioPort);
    bool is_control = lilv_port_is_a(plugin, port, node_ControlPort);
    bool is_atom = lilv_port_is_a(plugin, port, node_AtomPort);
}
```

**Get Port Range (for sliders):**
```c++
LilvNode* min_val = NULL;
LilvNode* max_val = NULL;
LilvNode* default_val = NULL;

lilv_port_get_range(plugin, port, &default_val, &min_val, &max_val);
float min = lilv_node_as_float(min_val);
float max = lilv_node_as_float(max_val);
float def = lilv_node_as_float(default_val);
```

**Pass File Path via Atom Message:**
```c++
// 1. Create atom sequence with file path
LV2_Atom_Sequence* atom_seq = (LV2_Atom_Sequence*) buffer;
atom_seq->atom.type = uris->Sequence;
atom_seq->atom.size = sizeof(LV2_Atom_Sequence_Body);

// 2. Add Path atom to sequence
LV2_Atom* path_atom = (LV2_Atom*) ((char*)atom_seq + sizeof(LV2_Atom_Sequence_Body));
path_atom->type = uris->Path;
path_atom->size = strlen(file_path) + 1;
strcpy((char*)(path_atom + 1), file_path);

// 3. Write to plugin's atom input port
plugin_instance->port_buffers[atom_input_port_index] = (float*)atom_seq;
```

### Native Code Compilation

The project uses CMake for native code compilation. Key files:

- **CMakeLists.txt**: Defines build rules for native libraries
- **Source Files**:
  - `multi.cpp`: Main native audio processor entry point
  - `jni_bridge.cpp`: JNI bindings for Java/C++ communication; all `AudioEngine` native methods
  - `LiveEffectEngine.cpp`: Full-duplex Oboe engine; holds 4 plugin slots
  - `LV2Plugin.hpp`: Generic, backend-agnostic LV2 wrapper with RT-safe processing
  - `jalv.cpp`: JACK/LV2 host utilities

### Building for Different Architectures

The NDK automatically compiles for multiple Android architectures:
- `armeabi-v7a` (32-bit ARM)
- `arm64-v8a` (64-bit ARM) 
- `x86` (Intel 32-bit)
- `x86_64` (Intel 64-bit)

## Dependencies & Libraries

### Build Dependencies
- **Android Gradle Plugin**: 9.0.1+
- **Material Design**: 1.10.0
- **AndroidX**:
  - appcompat: 1.6.1
  - activity: 1.8.0
  - constraintlayout: 2.1.4

### Audio Libraries
- **Google Oboe**: 1.10.0 - Real-time audio I/O

### LV2 Plugin Libraries (Prebuilt)
Located in `app/src/main/libs/`:
- `libjack_static.a` - JACK Audio Connection Kit
- `libjackserver_static.a` - JACK Server
- `libjalv_static.a` - JALV Host
- `liblilv.a` - Lilv Plugin Host Library
- `libsord-0_static.a` - Sord RDF Library
- `libserd-0.a` - Serd Serialization Library
- `libsratom.a` - Sratom Atom Library
- `libzix.a` - Zix Utility Library

## Performance Considerations

- **Audio Latency**: Minimized using Google Oboe's low-latency APIs
- **Real-time Processing**: Native C++ implementation ensures responsive audio processing
- **Memory Efficient**: Optimized for mobile device constraints
- **Thermal Management**: Efficient code to prevent device overheating

## Testing

### Run Unit Tests
```bash
./gradlew test
```

### Run Instrumented Tests (on device/emulator)
```bash
./gradlew connectedAndroidTest
```

## Troubleshooting

### Build & Compilation Issues

#### CMake Oboe Configuration Error
If you see: `Could not find a package configuration file provided by "oboe" (oboeConfig.cmake)`

**Solution Steps:**
1. **Force Gradle Sync**: In Android Studio, go to **File > Sync Now** to re-download all dependencies
2. **Clear Gradle Cache**: 
   ```bash
   ./gradlew clean
   rm -rf ~/.gradle/caches/transforms-*/
   ```
3. **Rebuild**:
   ```bash
   ./gradlew build
   ```
4. **Verify Gradle Version**: Ensure you're on Gradle 8.0+ (AGP 9.0.0+). Update in `gradle/wrapper/gradle-wrapper.properties` if needed:
   ```properties
   distributionUrl=https\://services.gradle.org/distributions/gradle-8.3-all.zip
   ```
5. **Check build.gradle**: Verify `buildFeatures { prefab true }` is present in the `android {}` block
6. **Use Gradle Wrapper**: Always use `./gradlew` instead of system `gradle` command

#### Missing NDK or CMake
- Ensure Android NDK is installed via SDK Manager
- Update CMake to version 3.22.1 or newer via SDK Manager

#### Other Build Failures
- Clean and rebuild: `./gradlew clean build`
- Check that all prebuilt libraries in `app/src/main/libs/` match your target architecture
- Verify C++ standard is correctly set (`android.defaultConfig.externalNativeBuild.cmake.cppFlags`)

### Audio & Permissions Issues

### No Audio Output
- Verify RECORD_AUDIO permission is granted
- Check device volume settings and the in-app Volume slider
- Ensure the Power toggle is enabled
- Check that at least one pedal slot has a plugin loaded
- Verify the correct Input and Output devices are selected

### Plugin Not Loading
- Check Logcat for `[load plugin]` error messages
- Ensure the plugin's `.so` library is compiled for your device's ABI
- Verify the plugin's `.ttl` manifest is valid and the URI matches


### Plugin Discovery Issues
- Verify LV2 plugin bundle is in `app/src/main/assets/lv2/`
- Check `manifest.ttl` and plugin `.ttl` files are valid Turtle RDF
- Check plugin architecture matches device (arm64-v8a, armeabi-v7a, etc.)
- Review Lilv logs (`[initPlugins]`) for plugin discovery errors

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see LICENSE file for details.

## Acknowledgments

- **Google Oboe**: Modern, low-latency audio API for Android
- **LV2 Project**: Standard plugin format and utilities
- **JACK Audio**: Professional audio routing infrastructure
- **Guitarix**: Source of many of the bundled guitar effects plugins
- **AndroidX**: Modern Android development libraries

## Support & Documentation

### Resources
- [LV2 Plugin Specification](https://lv2plug.in/)
- [LV2Plugin API Guide](app/src/main/cpp/LV2Plugin-Usage.md)
- [Google Oboe Documentation](https://github.com/google/oboe)
- [Android NDK Guide](https://developer.android.com/ndk)
- [Android Audio Documentation](https://developer.android.com/guide/topics/media)
- [JACK Audio Documentation](https://jackaudio.org/)

### Useful Links
- **Project Repository**: [GitHub Link]
- **Issue Tracker**: [GitHub Issues]
- **Wiki**: [Project Wiki]

## Changelog

### Version 0.9
- **LV2 buf-size spec compliance**: Host now advertises `bufsz:boundedBlockLength` with a complete `options:options` array including `minBlockLength`, `maxBlockLength`, `nominalBlockLength`, and `sequenceSize`
- **Runtime block size**: Block size communicated to plugins is now the real Oboe `framesPerBurst` value, not a hardcoded fallback
- **URID completeness**: All buf-size URIDs (`minBlockLength`, `nominalBlockLength`, `sequenceSize`) are now registered in the URID map

### Version 0.8
- **Audio recording**: Record processed audio to WAV, MP3, Opus, FLAC, or OGG Vorbis formats directly from the app
- Post-FX recording captures the full effects chain output
- On-device storage with easy access and sharing

### Version 0.7
- **Multi-pedal chain**: 4 independent pedal slots in a swipeable tab UI
- **Per-pedal bypass toggle**: Enable/disable individual plugins without removing them
- **Dynamic plugin loading**: Add or delete plugins at runtime from bundled library
- **Real-time parameter sliders**: Per-port sliders with labels and correct default values
- **Input/output device selection**: Choose audio hardware at runtime
- **Master volume control**: Output gain slider
- **54 bundled LV2 plugins**: Curated guitar effects ready to use
- LV2 plugin host implementation (Lilv + custom `LV2Plugin` wrapper)
- Google Oboe full-duplex audio I/O integration
- Multi-architecture NDK support (armeabi-v7a, arm64-v8a, x86, x86_64)
- Android 12+ compatibility

---

**Made with ♥ for mobile audio enthusiasts**

*opiqo-multi: Professional guitar effects processing on Android*


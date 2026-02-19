# opiqo-multi

A professional-grade **Guitar Multi-Effects Processor** for Android that hosts and runs **LV2 plugins**. Transform your mobile device into a powerful audio processing studio with real-time effects using industry-standard LV2 plugin format.

## Features

### Core Capabilities
- **LV2 Plugin Host**: Full support for standard LV2 (LADSPA Version 2) audio plugins
- **Real-time Audio Processing**: Low-latency audio I/O using Google's Oboe library
- **Guitar Effects**: Process guitar audio through multiple LV2-compatible effects plugins
- **Native Performance**: Built with C++ for efficient audio processing
- **Easy On/Off Control**: Simple toggle interface to enable/disable effects

### Technical Features
- **Google Oboe Integration**: Modern, low-latency audio API for Android
- **NDK Support**: High-performance native code compilation
- **LV2 Plugin Standards**: Compatible with standard LV2 plugins (JACK, Lilv, Sord libraries)
- **Multi-threaded Audio**: Efficient background audio processing
- **Android 12+**: Optimized for modern Android devices (API level 31+)

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

3. **Build the project**
   ```bash
   ./gradlew build
   ```

4. **Generate APK**
   ```bash
   ./gradlew assembleDebug    # For debug build
   ./gradlew assembleRelease  # For release build
   ```

5. **Install on device/emulator**
   ```bash
   ./gradlew installDebug
   ```

## Architecture

### Project Structure
```
opiqoGuitarMultiEffectsProcessor/
├── app/
│   ├── src/
│   │   ├── main/
│   │   │   ├── java/org/acoustixaudio/opiqo/multi/
│   │   │   │   ├── MainActivity.java          # Main UI activity
│   │   │   │   └── AudioEngine.java           # Audio processing engine
│   │   │   ├── cpp/
│   │   │   │   ├── CMakeLists.txt             # Native build configuration
│   │   │   │   ├── multi.cpp                  # Main native audio processor
│   │   │   │   ├── jni_bridge.cpp             # Java/Native interface
│   │   │   │   ├── LiveEffectEngine.cpp       # LV2 effects engine
│   │   │   │   └── jalv.cpp                   # JACK/LV2 host implementation
│   │   │   ├── res/                           # UI resources
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
└── README.md                                  # This file
```

### Audio Processing Pipeline

```
Guitar Input (Microphone)
         ↓
    [Oboe Audio API]
         ↓
  [JNI Bridge Layer]
         ↓
[LV2 Plugin Host (Jalv)]
         ↓
[Audio Effects Processing]
    - Effect 1
    - Effect 2
    - Effect N
         ↓
    [Output Speaker]
```

## Usage

### Getting Started

1. **Launch the App**
   - Open the installed app on your Android device
   - The app will request `RECORD_AUDIO` permission on first run
   - Grant the permission to enable audio processing

2. **Enable Effects**
   - Toggle the "Effect On/Off" switch to enable/disable audio processing
   - When enabled, the app will process audio through loaded LV2 plugins

3. **Load LV2 Plugins**
   - Place LV2 plugin files in the appropriate directory
   - The plugin host will automatically discover and load compatible plugins

### UI Components
- **On/Off Toggle**: Main switch to enable/disable audio effects processing
- **Status Indicator**: Shows current audio processing state

## Permissions

The app requires the following Android permissions:

- **RECORD_AUDIO**: Necessary to capture guitar audio input from the device microphone

These permissions are requested at runtime on Android 6.0 (API level 23) and above.

## Development

### Adding New Effects

To add new LV2 plugin support:

1. **Prepare LV2 Plugin**
   - Ensure plugin is compatible with Android NDK
   - Compile plugin binaries for target architectures (x86, armeabi-v7a, arm64-v8a)

2. **Update CMakeLists.txt**
   - Add plugin library paths to the CMake configuration
   - Link plugin libraries in `target_link_libraries()`

3. **Update LiveEffectEngine.cpp**
   - Register new plugin in the host
   - Add plugin parameter controls

### Native Code Compilation

The project uses CMake for native code compilation. Key files:

- **CMakeLists.txt**: Defines build rules for native libraries
- **Source Files**:
  - `multi.cpp`: Main native audio processor
  - `jni_bridge.cpp`: JNI bindings for Java/C++ communication
  - `LiveEffectEngine.cpp`: LV2 plugin host implementation
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

### No Audio Output
- Verify RECORD_AUDIO permission is granted
- Check device volume settings
- Ensure microphone is not blocked
- Verify LV2 plugins are properly loaded

### Build Failures
- Ensure NDK is properly installed
- Check CMake version compatibility
- Verify all prebuilt libraries are present in `app/src/main/libs/`
- Clean and rebuild: `./gradlew clean build`

### Plugin Loading Issues
- Verify LV2 plugin manifest files (TTL) are valid
- Check plugin architecture matches device (arm64-v8a, armeabi-v7a, etc.)
- Review Lilv logs for plugin discovery errors

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
- **AndroidX**: Modern Android development libraries

## Support & Documentation

### Resources
- [LV2 Plugin Specification](https://lv2plug.in/)
- [Google Oboe Documentation](https://github.com/google/oboe)
- [Android NDK Guide](https://developer.android.com/ndk)
- [Android Audio Documentation](https://developer.android.com/guide/topics/media)
- [JACK Audio Documentation](https://jackaudio.org/)

### Useful Links
- **Project Repository**: [GitHub Link]
- **Issue Tracker**: [GitHub Issues]
- **Wiki**: [Project Wiki]

## Changelog

### Version 1.0.0 (Initial Release)
- LV2 plugin host implementation
- Google Oboe audio I/O integration
- Basic UI with effect on/off control
- Multi-architecture NDK support
- Android 12+ compatibility

---

**Made with ♥ for mobile audio enthusiasts**

*opiqo-multi: Professional guitar effects processing on Android*


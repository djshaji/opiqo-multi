## Project Overview: Opiqo

**Opiqo** is a streamlined, open-source bridge designed to bring the power of **LV2 (LADSPA Version 2)** audio plugins to the Android ecosystem. Traditionally, high-end audio processing and synthesis via LV2 have been desktop-centric; Opiqo breaks that barrier, offering a general-purpose library port that is both lightweight and developer-friendly.

---

### Key Capabilities

* **LV2 Portability:** Enables the hosting and execution of LV2 plugins directly within Android applications.
* **General Purpose Design:** Unlike project-specific wrappers, Opiqo is built to be integrated into any Android audio project, from DAWs (Digital Audio Workstations) to standalone synthesizers.
* **Open Source:** Full transparency and community-driven improvements, allowing developers to customize the integration to their specific needs.
* **Easy Integration:** Focuses on reducing the boilerplate code typically required to manage shared libraries (`.so` files) and plugin state on mobile devices.

---

### Why Opiqo Matters

Android’s audio stack has historically been challenging for low-latency, professional-grade plugin hosting. Opiqo simplifies this by handling the heavy lifting of plugin discovery and parameter mapping.

| Feature | Description |
| --- | --- |
| **Plugin Discovery** | Automatically scans and identifies available LV2 bundles on the Android filesystem. |
| **Parameter Control** | Provides a clean API to map UI elements (sliders, knobs) to plugin ports. |
| **State Management** | Supports saving and loading plugin presets and configurations. |
| **Cross-Architecture** | Designed to handle the diverse ARM/x86 environment of Android devices. |

---

### High-Level Architecture

Opiqo sits between the Android Native Development Kit (NDK) and the audio processing layer. It translates standard LV2 host requirements into an Android-compatible format, ensuring that audio buffers are processed efficiently without significant overhead.

> **Pro-Tip:** For the best performance, Opiqo is best used in conjunction with **Oboe**, Google’s C++ library for low-latency audio.

---

### Getting Started

To integrate Opiqo, you typically follow these steps:

1. **Clone the Repository:** Integrate the source into your Android project via CMake.
2. **Plugin Deployment:** Include your desired `.lv2` bundles in your assets or external storage.
3. **Initialization:** Use the Opiqo API to "world" (initialize) the plugin environment.
4. **Process Loop:** Feed your audio buffers into the Opiqo engine within your high-priority audio thread.

---

Would you like me to generate a **sample `build.gradle` or CMake configuration** to help you start the integration process?

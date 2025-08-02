
A plugin for real-time **speech detection** using Vosk VAD (voice activity detection) with phoneme recognization. Plug into Unity+mediapipe to rig mouth animation based on the sound you make with your voice.
Faster than Whisper. Expected latency: ~100–300ms

```text
| Phoneme | Example | Mouth shape |
| ------- | ------- | ----------- |
| /a/     | “cat”   | wide open   |
| /oo/    | “food”  | round lips  |
| /m/     | “mom”   | closed lips |
| /th/    | “think” | tongue out  |
```

It is designed for live streamers, VTubers, and developers who want to trigger animations, expressions, or effects based on the user's speech (and expected mouth shape).


### ! Important ! 

To enable phoneme-based mouth animation, you need to download the full Vosk model in `/models`:

```text
vosk-model-small-en-us-0.15	~50MB	🚫 Phoneme not supported
vosk-model-en-us-0.22	~180MB	✅ Supported
```

Latest releases can be downloaded via https://alphacephei.com/vosk/models
Make sure to refer to the correct model in the python code.
```python
model = Model("models/vosk-model-en-us-0.22")
```

---

## 🔧 Features

- 🔊 Real-time voice activity detection from OBS audio input  
- 🟢 Optional integration with Unity via **UDP** or WebSocket  
- 📈 Supports future expansion to phoneme-based lip-sync  
- 🖥️ Lightweight C++ OBS audio filter (no dependencies beyond OBS + optional Qt6)  

---

## 🚀 Getting Started

### ✅ Requirements

- OBS Studio 29+  
- CMake 3.20+  
- Qt6 (optional, for UI controls)  
- Windows / macOS / Linux  

---

### 📦 Building from Source

If you cloned from obs-plugintemplate and renamed your plugin (e.g., speech-vad):
```bash
git clone https://github.com/yourusername/speech-detection-vad-for-obs.git
cd speech-detection-vad-for-obs

mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX="C:\Program Files\obs-studio" 
cmake --build . --config Release --target install
```

(You may need to open terminal with Admin rights for successful build)

Restart OBS > Audio Input Source (Right-click) > Filters > Add new filter > Speech VAD Filter should now appear.
(If not, check `C:\Program Files\obs-studio\obs-plugins\64bit\speech-detection-vad-for-obs.dll` exists after cmake build and install)



### 🎮 Unity Integration (Optional)

If you want to drive mouth animations or expressions in Unity:

- Enable the UDP output toggle in OBS plugin settings.

- Use the included Unity C# UDP listener to receive: 
   
   - isSpeaking boolean
   
   - (or) speech energy level
   
   - (or) viseme labels like AH, M, O for full lip-sync



📜 License
MIT License — free to use and modify.




-----

This plugin was built on...


# OBS Plugin Template

## Introduction

The plugin template is meant to be used as a starting point for OBS Studio plugin development. It includes:

* Boilerplate plugin source code
* A CMake project file
* GitHub Actions workflows and repository actions

## Supported Build Environments

| Platform  | Tool   |
|-----------|--------|
| Windows   | Visal Studio 17 2022 |
| macOS     | XCode 16.0 |
| Windows, macOS  | CMake 3.30.5 |
| Ubuntu 24.04 | CMake 3.28.3 |
| Ubuntu 24.04 | `ninja-build` |
| Ubuntu 24.04 | `pkg-config`
| Ubuntu 24.04 | `build-essential` |

## Quick Start

An absolute bare-bones [Quick Start Guide](https://github.com/obsproject/obs-plugintemplate/wiki/Quick-Start-Guide) is available in the wiki.

## Documentation

All documentation can be found in the [Plugin Template Wiki](https://github.com/obsproject/obs-plugintemplate/wiki).

Suggested reading to get up and running:

* [Getting started](https://github.com/obsproject/obs-plugintemplate/wiki/Getting-Started)
* [Build system requirements](https://github.com/obsproject/obs-plugintemplate/wiki/Build-System-Requirements)
* [Build system options](https://github.com/obsproject/obs-plugintemplate/wiki/CMake-Build-System-Options)

## GitHub Actions & CI

Default GitHub Actions workflows are available for the following repository actions:

* `push`: Run for commits or tags pushed to `master` or `main` branches.
* `pr-pull`: Run when a Pull Request has been pushed or synchronized.
* `dispatch`: Run when triggered by the workflow dispatch in GitHub's user interface.
* `build-project`: Builds the actual project and is triggered by other workflows.
* `check-format`: Checks CMake and plugin source code formatting and is triggered by other workflows.

The workflows make use of GitHub repository actions (contained in `.github/actions`) and build scripts (contained in `.github/scripts`) which are not needed for local development, but might need to be adjusted if additional/different steps are required to build the plugin.

### Retrieving build artifacts

Successful builds on GitHub Actions will produce build artifacts that can be downloaded for testing. These artifacts are commonly simple archives and will not contain package installers or installation programs.

### Building a Release

To create a release, an appropriately named tag needs to be pushed to the `main`/`master` branch using semantic versioning (e.g., `12.3.4`, `23.4.5-beta2`). A draft release will be created on the associated repository with generated installer packages or installation programs attached as release artifacts.

## Signing and Notarizing on macOS

Basic concepts of codesigning and notarization on macOS are explained in the correspodning [Wiki article](https://github.com/obsproject/obs-plugintemplate/wiki/Codesigning-On-macOS) which has a specific section for the [GitHub Actions setup](https://github.com/obsproject/obs-plugintemplate/wiki/Codesigning-On-macOS#setting-up-code-signing-for-github-actions).

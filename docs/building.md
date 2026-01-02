# Building from Source

UhbikWrapper uses CMake and automatically fetches JUCE and clap-juce-extensions during configuration.

## Prerequisites

### Linux (Ubuntu/Debian)

Run the setup script to install all dependencies:
```bash
./setup.sh
```

Or install manually:
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libfreetype6-dev \
    libasound2-dev \
    libx11-dev \
    libxcomposite-dev \
    libxcursor-dev \
    libxinerama-dev \
    libxext-dev \
    libxrandr-dev \
    libglu1-mesa-dev \
    libgtk-3-dev
```

### Windows

1. Install [Visual Studio 2019+](https://visualstudio.microsoft.com/) or Build Tools for Visual Studio
2. Install [CMake](https://cmake.org/download/) (3.15 or newer)
3. Ensure CMake is in your PATH

### macOS

1. Install Xcode Command Line Tools:
   ```bash
   xcode-select --install
   ```
2. Install CMake (via Homebrew):
   ```bash
   brew install cmake
   ```

## Building

### Quick Build (All Platforms)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output locations:
- VST3: `build/UhbikWrapper_artefacts/Release/VST3/UhbikWrapper.vst3`
- CLAP: `build/UhbikWrapper_artefacts/Release/CLAP/UhbikWrapper.clap`
- Standalone: `build/UhbikWrapper_artefacts/Release/Standalone/UhbikWrapper`

Auto-installed to:
- VST3: `~/.vst3/UhbikWrapper.vst3`
- CLAP: `~/.clap/UhbikWrapper.clap`

### Windows

```bash
cmake -B build
cmake --build build --config Release
```

Output locations:
- VST3: `build\UhbikWrapper_artefacts\Release\VST3\UhbikWrapper.vst3`
- CLAP: `build\UhbikWrapper_artefacts\Release\CLAP\UhbikWrapper.clap`

Install to:
- VST3: `C:\Program Files\Common Files\VST3\`
- CLAP: `C:\Program Files\Common Files\CLAP\`

### macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output locations:
- VST3: `build/UhbikWrapper_artefacts/Release/VST3/UhbikWrapper.vst3`
- CLAP: `build/UhbikWrapper_artefacts/Release/CLAP/UhbikWrapper.clap`
- AU: `build/UhbikWrapper_artefacts/Release/AU/UhbikWrapper.component`

Install to:
- VST3: `/Library/Audio/Plug-Ins/VST3/`
- CLAP: `/Library/Audio/Plug-Ins/CLAP/`
- AU: `/Library/Audio/Plug-Ins/Components/`

## Build Options

### Debug Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

### Clean Rebuild

```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Project Structure

```
UhbikWrapper/
├── CMakeLists.txt          # Build configuration
├── Source/
│   ├── PluginProcessor.cpp # Audio processing, effect chain
│   ├── PluginProcessor.h
│   ├── PluginEditor.cpp    # GUI
│   ├── PluginEditor.h
│   ├── CLAPPluginHost.cpp  # CLAP hosting
│   ├── CLAPPluginHost.h
│   ├── PresetBrowser.cpp   # Preset management
│   ├── PresetBrowser.h
│   ├── EffectSlot.cpp      # Effect slot UI
│   ├── EffectSlot.h
│   ├── LFO.h               # LFO + modulation types
│   ├── Envelope.h          # ADSR envelope
│   └── StepSequencer.h     # Step sequencer
├── docs/                   # Documentation
└── setup.sh                # Linux dependency installer
```

## Dependencies (Auto-Fetched)

These are downloaded automatically during CMake configuration:

- **JUCE** (master) - Audio plugin framework
- **clap-juce-extensions** (main) - CLAP format support
  - Includes CLAP SDK
  - Includes clap-helpers

## Troubleshooting

### CMake can't find compiler
- **Linux**: Install `build-essential`
- **Windows**: Install Visual Studio Build Tools
- **macOS**: Run `xcode-select --install`

### Missing X11 headers (Linux)
```bash
sudo apt-get install libx11-dev libxext-dev
```

### Plugin doesn't appear in DAW
1. Check the plugin is in the correct folder
2. Rescan plugins in your DAW
3. Check DAW's plugin log for errors

### macOS Gatekeeper blocks plugin
```bash
xattr -cr "/Library/Audio/Plug-Ins/VST3/UhbikWrapper.vst3"
sudo codesign --force --deep --sign - "/Library/Audio/Plug-Ins/VST3/UhbikWrapper.vst3"
```

## GitHub Actions CI

The repository includes automated builds for all platforms. Every push triggers:
- Linux build (Ubuntu)
- Windows build (MSVC)
- macOS build (Xcode)

Tagged releases (e.g., `v0.2.0`) automatically create GitHub Releases with downloadable artifacts.

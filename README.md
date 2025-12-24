# Uhbik Wrapper

A lightweight, "Reason-style" VST3 wrapper designed specifically for hosting **U-he Uhbik** and other VST3 effects. It provides a transparent, "Combinator-like" interface that allows you to chain multiple effects together and manage them as a single meta-preset.

## Features

*   **Effect Chain**: Load unlimited VST3 effects in series
*   **Plugin Scanner**: Automatically discovers all VST3 plugins in `~/.vst3/`
*   **Rack-Style GUI**: Dark rack interface with orange header, inspired by hardware rack units
*   **Per-Effect Controls**:
    - **Edit**: Opens the plugin's native GUI in a popup window
    - **Bypass**: Toggle effect on/off without removing it
    - **Remove**: Delete effect from chain
    - **Reorder**: Move effects up/down in the chain
*   **Preset System**: Save and load entire effect chains as `.uhbikchain` files
*   **UI Zoom**: Scale the interface from 100% to 300%
*   **Transparent Hosting**: Passes audio directly through the chain with zero added coloration

## Prerequisites (Linux)

To build this project, you need the following system libraries:
*   **C++ Compiler**: GCC (g++)
*   **Build Tools**: CMake, pkg-config
*   **Audio/GUI Libraries**: libfreetype6, libasound2, libx11, libxcomposite, libxcursor, libxinerama, libxext, libxrandr, libglu1-mesa, libgtk-3

## Quick Start / Build

The project includes an automated setup script that installs dependencies and compiles the plugin.

1.  Open a terminal in the project directory.
2.  Run the setup script:
    ```bash
    ./setup.sh
    ```
3.  Once the build says **"BUILD COMPLETE"**, the VST3 plugin will be installed at:
    `~/.vst3/Uhbik Wrapper.vst3`

## Usage

1.  **Load in DAW**: The plugin auto-installs to `~/.vst3/` so your DAW should find it automatically
2.  **Add Effects**: Use the dropdown to select a plugin and click [+] to add it to the chain
3.  **Edit Effects**: Click "Edit" on any effect to open its native GUI
4.  **Reorder**: Use the up/down arrows to change effect order
5.  **Save Presets**: View menu > Save Preset to save your chain
6.  **Load Presets**: View menu > Load Preset to restore a saved chain
7.  **Zoom**: View menu > select zoom level (100%, 150%, 200%, 300%)

## Project Structure

*   `Source/PluginProcessor.cpp`: Effect chain management and audio processing
*   `Source/PluginProcessor.h`: Data structures for effect slots
*   `Source/PluginEditor.cpp`: Main rack GUI and controls
*   `Source/PluginEditor.h`: Editor component declarations
*   `Source/EffectSlot.cpp`: Individual effect slot UI component
*   `Source/EffectSlot.h`: Slot component with Edit/Bypass/Remove buttons
*   `CMakeLists.txt`: Build configuration that fetches JUCE automatically
*   `setup.sh`: Automated dependency installer and builder

## Roadmap

- [x] **Plugin Chaining**: Support loading multiple effects in series
- [x] **Preset Serialization**: Save and load entire chain state
- [x] **Plugin Scanner**: Discover available VST3 plugins
- [x] **Effect Reordering**: Move effects up/down in the chain
- [x] **UI Zoom**: Scale interface for different screen sizes
- [ ] **Preset Browser**: Folder-based preset organization like Uhbik's browser
- [ ] **DAW Parameter Exposure**: Show loaded plugin names in DAW interface
- [ ] **Macro Knobs**: Add knobs mapped to internal plugin parameters

## License

Open Source - MIT License

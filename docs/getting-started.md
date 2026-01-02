# Getting Started with UhbikWrapper

UhbikWrapper is a plugin host that lets you chain multiple VST3 and CLAP effects together and save them as unified presets. Think of it as a "Combinator" for effects.

## Installation

### From GitHub Releases
1. Download the latest release for your platform from [Releases](https://github.com/dlarseninclusive/UhbikWrapper/releases)
2. Extract the archive
3. Copy the plugin to your system's plugin folder:

| Platform | VST3 Location | CLAP Location |
|----------|---------------|---------------|
| **Linux** | `~/.vst3/` | `~/.clap/` |
| **Windows** | `C:\Program Files\Common Files\VST3\` | `C:\Program Files\Common Files\CLAP\` |
| **macOS** | `/Library/Audio/Plug-Ins/VST3/` | `/Library/Audio/Plug-Ins/CLAP/` |

### macOS Gatekeeper Note
Downloaded plugins are quarantined on macOS. Remove the quarantine:
```bash
xattr -cr "/Library/Audio/Plug-Ins/VST3/UhbikWrapper.vst3"
xattr -cr "/Library/Audio/Plug-Ins/CLAP/UhbikWrapper.clap"
sudo codesign --force --deep --sign - "/Library/Audio/Plug-Ins/VST3/UhbikWrapper.vst3"
```

## First Launch

1. **Load the plugin** in your DAW as an effect on a track
2. **Scan for plugins** happens automatically on first launch
3. The interface shows:
   - **Left panel**: Preset Browser
   - **Center**: Effect rack (empty initially)
   - **Bottom**: Collapsible Modulation and Ducker panels

## Adding Effects

1. Use the **format filter** dropdown (All/CLAP/VST3) to filter available plugins
2. Select a plugin from the **dropdown menu**
3. The plugin is automatically added to the chain
4. Repeat to add more effects - they process in series (top to bottom)

## Effect Controls

Each effect slot has:
- **Edit**: Opens the plugin's native GUI
- **Bypass**: Toggles the effect on/off (audio passes through)
- **X**: Removes the effect from the chain
- **Up/Down arrows**: Reorder effects in the chain

### Per-Effect Mixing
Click the effect header to expand mixing controls:
- **In**: Input gain (-24 to +24 dB)
- **Out**: Output gain (-24 to +24 dB)
- **Mix**: Dry/wet blend (0-100%)
- **Init**: Reset all mixing controls to default

## Level Meters

- Each effect shows input/output level meters
- Master input/output meters appear in the footer
- Green = normal, Yellow = hot, Red = clipping

## UI Scaling

Click **View** to change the UI scale:
- 100%, 150%, 200%, or 300%
- Setting persists across sessions

## Next Steps

- [Preset Management](presets.md) - Save and organize effect chains
- [Modulation System](modulation.md) - Automate parameters with LFOs, envelopes, and step sequencers
- [Ducker](ducker.md) - Sidechain-triggered volume ducking
- [Building from Source](building.md) - Compile your own builds

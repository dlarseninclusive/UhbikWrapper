# Preset Management

UhbikWrapper saves entire effect chains as presets, including all plugin states, mixing settings, modulation routes, and ducker configuration.

## Preset Browser

The preset browser is always visible on the left side of the interface.

### Navigating Presets

- **Folder dropdown**: Select a folder to browse
- **Preset list**: Shows all presets in the current folder
- **Click**: Select a preset (shows details below)
- **Double-click**: Load the preset immediately
- **Load button**: Load the selected preset

### Preset Information

When you select a preset, you'll see:
- **Plugin list**: Effects in the chain
- **Author**: Who created the preset
- **Tags**: Categorization keywords
- **Notes**: Description or usage tips

### Missing Plugins

If a preset uses plugins you don't have installed:
- The preset appears with an **orange background**
- A **warning icon** indicates missing plugins
- You can still load it, but missing effects will be skipped

## Saving Presets

1. Build your effect chain
2. Click **Save** in the preset browser
3. Fill in the metadata:
   - **Name**: Preset name (required)
   - **Author**: Your name
   - **Tags**: Comma-separated keywords (e.g., "reverb, ambient, pad")
   - **Notes**: Description or usage instructions
4. Click **Save**

### What Gets Saved

- All effects in the chain (plugin ID and state)
- Per-effect settings (bypass, input/output gain, mix)
- Modulation sources (LFO, envelope, step sequencer settings)
- Modulation routes (all connections in the mod matrix)
- Ducker settings (threshold, amount, attack, release, hold)
- UI state (panel expansion, zoom level)

## Organizing Presets

### Creating Folders

1. Click **New Folder** in the preset browser
2. Enter a folder name
3. The folder appears in the dropdown

### Folder Structure

Presets are stored in:
```
~/Documents/UhbikWrapper/Presets/
├── My Presets/
│   ├── Ambient/
│   │   └── Lush Reverb.uhbikchain
│   └── Distortion/
│       └── Gritty Drive.uhbikchain
└── Factory/
    └── ...
```

### Managing Presets

- **Edit**: Modify preset metadata (name, author, tags, notes)
- **Delete**: Remove preset (with confirmation)
- **Open Folder**: Open the preset location in your file manager

## Preset File Format

Presets use the `.uhbikchain` extension and are stored as XML:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<UhbikWrapperPreset version="1">
  <Metadata>
    <Name>My Preset</Name>
    <Author>Your Name</Author>
    <Tags>tag1, tag2</Tags>
    <Notes>Description here</Notes>
  </Metadata>
  <EffectChain>
    <Effect index="0" format="CLAP" id="com.u-he.Uhbik Delay">
      <State><!-- base64 encoded plugin state --></State>
      <Mixing inputGain="0" outputGain="0" mix="100"/>
      <Bypassed>false</Bypassed>
    </Effect>
    <!-- more effects... -->
  </EffectChain>
  <Modulation>
    <!-- LFO, envelope, step sequencer, and route settings -->
  </Modulation>
  <Ducker enabled="false" threshold="-20" amount="50"
          attack="5" release="200" hold="0"/>
</UhbikWrapperPreset>
```

## Tips

- **Back up presets**: Copy the `~/Documents/UhbikWrapper/Presets/` folder
- **Share presets**: Send `.uhbikchain` files to others (they need the same plugins)
- **Version control**: Presets are text-based XML, great for git
- **Init preset**: Click **Init** to clear the chain and start fresh

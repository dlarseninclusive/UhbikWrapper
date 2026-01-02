# Modulation System

UhbikWrapper includes a powerful modulation system for automating parameters on CLAP plugins. The system supports LFOs, envelopes, step sequencers, and macro knobs as modulation sources.

> **Note**: Modulation only works with CLAP plugins that expose modulatable parameters. VST3 plugins cannot be modulated through this system.

## Accessing the Modulation Panel

Click the **MODULATION** button at the bottom of the interface to expand the modulation panel. The panel has four tabs:

- **LFOs** - 4 low-frequency oscillators
- **Envs** - 2 ADSR envelopes
- **Seqs** - 2 step sequencers
- **Matrix** - Modulation routing

## LFOs

The LFOs tab provides 4 independent LFOs for continuous modulation.

### Controls per LFO:
- **Waveform**: Sine, Triangle, Saw, Square, or S&H (Sample & Hold)
- **Rate**: Frequency in Hz (0.01 to 20 Hz)
- **Depth**: Modulation intensity (0-100%)

### Waveform Types:
| Waveform | Description |
|----------|-------------|
| **Sine** | Smooth, natural movement |
| **Triangle** | Linear up/down sweep |
| **Saw** | Ramp from -1 to +1 |
| **Square** | Instant switch between min/max |
| **S&H** | Random value held for each cycle |

## Envelopes

The Envs tab provides 2 ADSR envelope generators for triggered modulation.

### Controls per Envelope:
- **Attack**: Time to reach peak (0.1ms to 2000ms)
- **Decay**: Time to fall to sustain level (0.1ms to 2000ms)
- **Sustain**: Held level while triggered (0-100%)
- **Release**: Time to fall to zero after release (0.1ms to 5000ms)
- **Depth**: Overall envelope intensity (0-100%)
- **Trigger**: Manually trigger the envelope

### Use Cases:
- Filter sweeps on note hits
- Transient shaping
- Ducking effects
- Slow swells and fades

## Step Sequencers

The Seqs tab provides 2 step sequencers for rhythmic modulation patterns.

### Controls per Sequencer:
- **16 Step Sliders**: Drag vertically to set each step value (0-100%)
- **Division**: Note division for tempo sync (1/1, 1/2, 1/4, 1/8, 1/16, 1/32)
- **Glide**: Smoothing between steps (0-100%)
- **Depth**: Overall sequencer intensity (0-100%)
- **Pattern**: Load preset patterns

### Preset Patterns:
| Pattern | Description |
|---------|-------------|
| Ramp Up | Linear increase 0→100% |
| Ramp Down | Linear decrease 100%→0 |
| Triangle | Up then down |
| Square | First half high, second half low |
| Random | Random values for each step |
| Clear | Reset all steps to 50% |

### Tips:
- Use **Glide** to smooth choppy patterns
- Lower **Division** values (1/1, 1/2) create slow sweeps
- Higher values (1/16, 1/32) create rapid rhythmic effects

## Mod Matrix

The Matrix tab is where you connect modulation sources to plugin parameters.

### Creating a Modulation Route:

1. **Select Source**: Choose from:
   - LFO 1-4
   - Env 1-2
   - Seq 1-2
   - Macro 1-8

2. **Select Slot**: Choose which effect slot to modulate

3. **Select Parameter**: Choose from the plugin's modulatable parameters
   > Only parameters marked as modulatable by the plugin will appear

4. **Set Amount**: Adjust modulation depth (-100% to +100%)
   - Positive values: modulation adds to parameter value
   - Negative values: modulation subtracts from parameter value

5. **Click "Add Route"**: Creates the modulation connection

### Managing Routes:

- **Active Routes list**: Shows all current modulation connections
- **Click X**: Remove a route
- **Clear All**: Remove all modulation routes

### Example Routings:

| Source | Target | Effect |
|--------|--------|--------|
| LFO 1 (Sine, 0.5 Hz) | Filter Cutoff | Classic auto-wah |
| Env 1 | Filter Cutoff | Triggered filter sweep |
| Seq 1 (1/16) | Pan | Rhythmic panning |
| LFO 2 (Triangle, 2 Hz) | Delay Time | Chorus/vibrato effect |
| Macro 1 | Multiple params | Performance control |

## Macros

The 8 Macro knobs (accessible via your DAW's parameter automation) can be used as modulation sources:

1. Automate or map the Macro parameter in your DAW
2. In the Mod Matrix, select "Macro 1-8" as the source
3. Route to any modulatable parameter

This allows MIDI CC or DAW automation to control plugin parameters dynamically.

## Technical Notes

- Modulation runs at **64-sample granularity** for smooth automation
- Each modulation source outputs bipolar values (-1 to +1)
- The **Amount** parameter scales this to the target parameter's range
- Multiple sources can target the same parameter (values sum)
- Modulation state is saved with presets

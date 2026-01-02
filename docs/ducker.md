# Ducker (Sidechain Volume Control)

The built-in ducker automatically reduces the volume of your effect chain when audio exceeds a threshold. This is useful for:

- **Ducking reverb/delay** under dry vocals or drums
- **Creating pumping effects** in electronic music
- **Preventing effect buildup** during loud passages

## Accessing the Ducker

Click the **DUCKER** button at the bottom of the interface to expand the ducker panel.

## Controls

### Enable (ON button)
Toggles the ducker on/off. When off, audio passes through unaffected.

### Threshold (-60 to 0 dB)
The input level that triggers ducking.
- **Lower values** (-40 to -60 dB): Duck on quiet signals
- **Higher values** (-10 to 0 dB): Only duck on loud peaks

### Amount (0-100%)
How much to reduce the volume when ducking.
- **0%**: No ducking (pass-through)
- **50%**: Reduce by half
- **100%**: Full silence when triggered

### Attack (0.1 to 100 ms)
How quickly ducking kicks in after threshold is crossed.
- **Fast** (0.1-5 ms): Tight, immediate ducking
- **Slow** (20-100 ms): Gradual fade-down

### Release (10 to 2000 ms)
How quickly volume returns after signal drops below threshold.
- **Fast** (10-100 ms): Snappy recovery
- **Slow** (500-2000 ms): Gradual fade-up, smoother

### Hold (0 to 500 ms)
Minimum time to stay ducked before release begins.
- **0 ms**: Release starts immediately when signal drops
- **100+ ms**: Prevents rapid on/off switching on dynamic material

## Signal Flow

```
Input → Effect Chain → Ducker → Output
          ↑
     Sidechain Input (if available)
```

The ducker monitors:
1. **Main input** by default
2. **Sidechain input** if your DAW provides one

## DAW Setup for Sidechain

### Bitwig
1. Load UhbikWrapper on a track
2. In the device panel, find the sidechain input
3. Route audio from another track to the sidechain

### Ableton Live
1. Load UhbikWrapper on a return track
2. Create an audio track with your trigger source
3. Set the trigger track's output to the return track's sidechain

### Other DAWs
Check your DAW's documentation for sidechain routing to VST3/CLAP plugins.

## Common Settings

### Vocal Ducking (Reverb/Delay)
Duck effects under vocals:
- **Threshold**: -20 dB
- **Amount**: 40-60%
- **Attack**: 5-10 ms
- **Release**: 200-400 ms
- **Hold**: 0-50 ms

### Pumping Effect (EDM)
Create rhythmic pumping:
- **Threshold**: -30 dB
- **Amount**: 80-100%
- **Attack**: 1-5 ms
- **Release**: 100-300 ms
- **Hold**: 0 ms

### Subtle Cleanup
Prevent effect buildup:
- **Threshold**: -10 dB
- **Amount**: 20-30%
- **Attack**: 20-50 ms
- **Release**: 500-1000 ms
- **Hold**: 100 ms

## Gain Reduction Meter

The ducker shows real-time gain reduction in the panel. This helps you visualize how much ducking is occurring.

## Tips

- **Start with low Amount** and increase until you hear the effect
- **Use Hold** to prevent "chattering" on percussive material
- **Longer Release** sounds more natural, shorter sounds more aggressive
- **Combine with modulation**: Use an envelope to trigger additional parameter changes alongside ducking

# UhbikWrapper Development Plan

## Overview
Planned enhancements to add DAW parameter exposure, macro controls, and visualizations so Bitwig (and other DAWs) can display plugin names and automate controls.

## Planned Features

### User Requirements
- **8 macro knobs** exposed to DAW for automation
- **Plugin names visible** in DAW parameter panel
- **Level meters** per effect slot + master output
- **Popup spectrum analyzer** per plugin and master
- **Wet/Dry mix** per effect
- **Input/Output gain** controls
- **MIDI Learn** for macro knobs

---

## Files to Modify/Create

| File | Action | Purpose |
|------|--------|---------|
| `Source/PluginProcessor.h` | Modify | Add AudioProcessorValueTreeState, parameters |
| `Source/PluginProcessor.cpp` | Modify | Parameter definitions, metering, MIDI learn |
| `Source/PluginEditor.h` | Modify | Add meter components, macro knob UI |
| `Source/PluginEditor.cpp` | Modify | Meter display, spectrum popup, macro controls |
| `Source/EffectSlot.h` | Modify | Add meter, wet/dry, gain controls per slot |
| `Source/EffectSlot.cpp` | Modify | Implement slot-level controls and meters |
| `Source/LevelMeter.h` | Create | Reusable level meter component |
| `Source/LevelMeter.cpp` | Create | Meter rendering and peak hold |
| `Source/SpectrumAnalyzer.h` | Create | FFT spectrum analyzer component |
| `Source/SpectrumAnalyzer.cpp` | Create | FFT processing and visualization |
| `Source/MacroKnob.h` | Create | Macro knob with parameter mapping |
| `Source/MacroKnob.cpp` | Create | Knob UI and target assignment |
| `CMakeLists.txt` | Modify | Add new source files, juce_dsp module |

---

## Implementation Phases

### Phase 1: DAW Parameter System (Foundation)

**Goal:** Expose parameters to DAW so Bitwig can see plugin info

**PluginProcessor.h additions:**
```cpp
// Parameter IDs
static constexpr int MAX_SLOTS = 16;
static constexpr int NUM_MACROS = 8;

// AudioProcessorValueTreeState for all parameters
std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;

// Parameters to expose:
// - slot_X_name (StringParameter, read-only display)
// - slot_X_bypass (BoolParameter)
// - slot_X_wetdry (FloatParameter 0-100%)
// - slot_X_gain (FloatParameter -24 to +24 dB)
// - macro_X (FloatParameter 0-1)
// - master_input_gain (FloatParameter)
// - master_output_gain (FloatParameter)
```

**Implementation steps:**
1. Add `juce::AudioProcessorValueTreeState` to PluginProcessor
2. Create parameter layout with all parameters
3. Connect parameters to internal state
4. Update `getStateInformation()`/`setStateInformation()` to use APVTS

### Phase 2: Per-Slot Controls (Wet/Dry, Gain)

**Goal:** Add mix and gain controls to each effect slot

**EffectSlot struct additions:**
```cpp
struct EffectSlot {
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription description;
    bool bypassed = false;
    float wetDryMix = 1.0f;      // 0=dry, 1=wet
    float gainDb = 0.0f;         // -24 to +24 dB
    // Level metering data
    std::atomic<float> inputLevel{0.0f};
    std::atomic<float> outputLevel{0.0f};
};
```

**processBlock() changes:**
```cpp
for (auto& slot : effectChain)
{
    if (slot.plugin != nullptr && !slot.bypassed)
    {
        // Capture input level
        slot.inputLevel = buffer.getRMSLevel(0, 0, buffer.getNumSamples());

        // Store dry signal
        juce::AudioBuffer<float> dryBuffer;
        dryBuffer.makeCopyOf(buffer);

        // Process wet
        slot.plugin->processBlock(buffer, midiMessages);

        // Apply gain
        float gain = juce::Decibels::decibelsToGain(slot.gainDb);
        buffer.applyGain(gain);

        // Mix wet/dry
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.addFrom(ch, 0, dryBuffer, ch, 0, buffer.getNumSamples(),
                          1.0f - slot.wetDryMix);
            buffer.applyGain(ch, 0, buffer.getNumSamples(), slot.wetDryMix);
        }

        // Capture output level
        slot.outputLevel = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    }
}
```

### Phase 3: Level Meters

**Goal:** Visual feedback for levels per slot and master

**LevelMeter.h:**
```cpp
class LevelMeter : public juce::Component, public juce::Timer
{
public:
    void setLevel(float newLevel);
    void paint(juce::Graphics& g) override;
    void timerCallback() override;

private:
    float currentLevel = 0.0f;
    float peakLevel = 0.0f;
    int peakHoldCounter = 0;
};
```

**UI placement:**
- Small vertical meter on each effect slot (input/output)
- Larger master meter in footer or header area

### Phase 4: Macro Knobs

**Goal:** 8 automatable knobs that can be mapped to any hosted plugin parameter

**MacroKnob system:**
```cpp
struct MacroMapping {
    int slotIndex = -1;           // Which effect slot
    int parameterIndex = -1;      // Which parameter in that plugin
    float rangeMin = 0.0f;        // Mapping range
    float rangeMax = 1.0f;
};

class MacroKnob : public juce::Slider
{
    std::vector<MacroMapping> mappings;  // One macro can control multiple params
    void valueChanged() override;         // Apply to all mapped params
};
```

**UI placement:**
- Row of 8 knobs below header or above effect chain
- Right-click to assign/clear mappings

### Phase 5: Spectrum Analyzer

**Goal:** FFT visualization as popup per plugin and master

**SpectrumAnalyzer.h:**
```cpp
class SpectrumAnalyzer : public juce::Component, public juce::Timer
{
public:
    void pushSamples(const float* samples, int numSamples);
    void paint(juce::Graphics& g) override;

private:
    juce::dsp::FFT fft{11};  // 2048 point FFT
    std::array<float, 2048> fftData;
    std::array<float, 1024> spectrum;
};
```

**Popup behavior:**
- Click meter or spectrum button on slot -> opens popup spectrum window
- Master spectrum in expandable footer panel

### Phase 6: MIDI Learn

**Goal:** Map hardware MIDI CC to macro knobs

**Implementation:**
```cpp
// In PluginProcessor
bool midiLearnMode = false;
int midiLearnTargetMacro = -1;
std::array<int, NUM_MACROS> midiCCMappings;  // CC number per macro, -1 = unmapped

// In processBlock, check incoming MIDI
void handleMidiLearn(const juce::MidiMessage& msg)
{
    if (midiLearnMode && msg.isController())
    {
        midiCCMappings[midiLearnTargetMacro] = msg.getControllerNumber();
        midiLearnMode = false;
    }
}
```

---

## UI Layout (Target)

```
+-------------------------------------------------------------------------+
| PRESETS | EFFECT RACK              [M1][M2][M3][M4][M5][M6][M7][M8]     |
|         |                          ------------------------------------ |
| folders | [Uhbik-A      ][||][W/D][Gain][Edit][B][X][^][v]              |
| presets | [Uhbik-D      ][||][W/D][Gain][Edit][B][X][^][v]              |
|         | [Diva Filter  ][||][W/D][Gain][Edit][B][X][^][v]              |
|         |                                                                |
| Notes   |----------------------------------------------------------------|
| Save    | [In ||] Status: 3 effects | Latency: 512 smp | [Out ||][FFT]  |
+-------------------------------------------------------------------------+

Legend:
[||]    = Level meter (stereo)
[W/D]   = Wet/Dry knob
[Gain]  = Gain knob
[M1-8]  = Macro knobs
[FFT]   = Master spectrum popup button
```

---

## Implementation Order

1. **Phase 1a**: Add APVTS to PluginProcessor (foundation)
2. **Phase 1b**: Create slot parameters (bypass, wet/dry, gain)
3. **Phase 1c**: Create macro parameters (8 float params)
4. **Phase 2**: Implement wet/dry and gain in processBlock
5. **Phase 3a**: Create LevelMeter component
6. **Phase 3b**: Add meters to EffectSlot UI
7. **Phase 3c**: Add master meters to footer
8. **Phase 4a**: Create MacroKnob component
9. **Phase 4b**: Add macro row to editor UI
10. **Phase 4c**: Implement parameter mapping system
11. **Phase 5a**: Create SpectrumAnalyzer component (requires juce_dsp)
12. **Phase 5b**: Add spectrum popup windows
13. **Phase 6**: Implement MIDI learn system

---

## Dependencies

- Add `juce_dsp` module to CMakeLists.txt for FFT/spectrum analyzer
- No external dependencies required

## Technical Notes

- Parameters use JUCE's AudioProcessorValueTreeState for DAW compatibility
- Level metering uses atomic floats for thread-safe audio->UI communication
- Spectrum analyzer runs on timer, not audio thread (push samples, process on GUI thread)
- MIDI learn mappings stored in plugin state for recall
- Cross-platform VST3 paths needed for Windows/macOS support

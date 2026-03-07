#pragma once

// X11's Xlib.h defines 'None' as a macro, which conflicts with juce_dsp's
// DelayLineInterpolationTypes::None. Undefine it before including juce_dsp.
#ifdef None
  #undef None
#endif

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

class MultibandProcessor
{
public:
    static constexpr int NUM_BANDS = 3;

    // Band indices
    static constexpr int LOW_BAND = 0;
    static constexpr int MID_BAND = 1;
    static constexpr int HIGH_BAND = 2;

    MultibandProcessor();

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    // Split input buffer into 3 band buffers (low, mid, high)
    // bandBuffers must be a pre-allocated array of 3 AudioBuffers
    void split(const juce::AudioBuffer<float>& input,
               juce::AudioBuffer<float>* bandBuffers);

    // Crossover frequencies
    void setLowMidCrossover(float freqHz);
    void setMidHighCrossover(float freqHz);
    float getLowMidCrossover() const { return lowMidFreq; }
    float getMidHighCrossover() const { return midHighFreq; }

    // Per-band state (gain/solo/mute/metering)
    struct BandState
    {
        std::atomic<float> gainDb{0.0f};      // -24 to +24 dB
        std::atomic<bool> solo{false};
        std::atomic<bool> mute{false};
        std::atomic<float> outputLevelL{0.0f}; // 0.0 to 1.0 peak
        std::atomic<float> outputLevelR{0.0f};
    };
    BandState bandStates[NUM_BANDS];

    // Apply per-band gain/solo/mute and sum bands back to output buffer
    void sumBands(juce::AudioBuffer<float>* bandBuffers,
                  juce::AudioBuffer<float>& output);

private:
    float lowMidFreq = 200.0f;
    float midHighFreq = 2000.0f;
    double currentSampleRate = 44100.0;
    bool prepared = false;

    // Linkwitz-Riley 24dB/oct (4th order) crossover filters
    //
    // Topology:
    //   Input ──┬── LP1(lowMidFreq) ──────────────────→ Low band
    //           └── HP1(lowMidFreq) ──┬── LP2(midHighFreq) ─→ Mid band
    //                                  └── HP2(midHighFreq) ─→ High band
    //
    // Per-channel filters (L and R processed independently)
    juce::dsp::LinkwitzRileyFilter<float> lowMidLP[2];   // Low band output
    juce::dsp::LinkwitzRileyFilter<float> lowMidHP[2];   // Feeds into second split
    juce::dsp::LinkwitzRileyFilter<float> midHighLP[2];  // Mid band output
    juce::dsp::LinkwitzRileyFilter<float> midHighHP[2];  // High band output

    void updateFilterFrequencies();
};

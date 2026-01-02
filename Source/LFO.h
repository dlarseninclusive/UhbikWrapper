#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <atomic>

// LFO waveform types
enum class LFOWaveform
{
    Sine,
    Triangle,
    Saw,
    Square,
    Random  // Sample & Hold
};

// Simple LFO for parameter modulation
class LFO
{
public:
    LFO() = default;

    void prepare(double sampleRate)
    {
        currentSampleRate = sampleRate;
        phase = 0.0;
        randomValue = 0.0f;
    }

    void setFrequency(float hz)
    {
        frequency = hz;
    }

    void setWaveform(LFOWaveform wf)
    {
        waveform = wf;
    }

    void setDepth(float d)
    {
        depth = juce::jlimit(0.0f, 1.0f, d);
    }

    // Reset phase (e.g., on transport start)
    void reset()
    {
        phase = 0.0;
    }

    // Process one sample and return modulation value [-depth, +depth]
    float tick()
    {
        if (currentSampleRate <= 0.0)
            return 0.0f;

        float value = 0.0f;

        switch (waveform)
        {
            case LFOWaveform::Sine:
                value = std::sin(phase * 2.0 * juce::MathConstants<double>::pi);
                break;

            case LFOWaveform::Triangle:
            {
                // Triangle: 0->1->0->-1->0
                double t = std::fmod(phase, 1.0);
                if (t < 0.25)
                    value = static_cast<float>(t * 4.0);
                else if (t < 0.75)
                    value = static_cast<float>(2.0 - t * 4.0);
                else
                    value = static_cast<float>(t * 4.0 - 4.0);
                break;
            }

            case LFOWaveform::Saw:
                // Saw: -1 to +1 over one cycle
                value = static_cast<float>(std::fmod(phase, 1.0) * 2.0 - 1.0);
                break;

            case LFOWaveform::Square:
                value = (std::fmod(phase, 1.0) < 0.5) ? 1.0f : -1.0f;
                break;

            case LFOWaveform::Random:
            {
                // Sample & Hold: new random value at each cycle
                double newPhase = phase + frequency / currentSampleRate;
                if (std::floor(newPhase) > std::floor(phase))
                    randomValue = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
                value = randomValue;
                break;
            }
        }

        // Advance phase
        phase += frequency / currentSampleRate;
        if (phase >= 1.0)
            phase -= std::floor(phase);

        return value * depth;
    }

    // Process a block and fill buffer with modulation values
    void processBlock(float* buffer, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
            buffer[i] = tick();
    }

    float getFrequency() const { return frequency; }
    float getDepth() const { return depth; }
    LFOWaveform getWaveform() const { return waveform; }

private:
    double currentSampleRate = 44100.0;
    double phase = 0.0;
    float frequency = 1.0f;    // Hz
    float depth = 1.0f;        // 0.0 to 1.0
    LFOWaveform waveform = LFOWaveform::Sine;
    float randomValue = 0.0f;  // For S&H

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFO)
};

// Modulation target - identifies a parameter in a slot
struct ModulationTarget
{
    int slotIndex = -1;             // Which effect slot
    clap_id paramId = 0;            // CLAP parameter ID
    juce::String paramName;         // For display
    double minValue = 0.0;          // Parameter range
    double maxValue = 1.0;
    bool isModulatable = false;

    bool isValid() const { return slotIndex >= 0 && isModulatable; }
};

// A modulation routing (LFO -> Parameter)
struct ModulationRoute
{
    int lfoIndex = 0;               // Which LFO (0-3)
    ModulationTarget target;
    float amount = 0.0f;            // Modulation amount (-1 to +1, scaled to param range)
    bool enabled = true;

    bool isValid() const { return target.isValid() && enabled; }
};

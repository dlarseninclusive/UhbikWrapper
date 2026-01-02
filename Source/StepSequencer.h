#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cmath>

// Step Sequencer for modulation
class StepSequencer
{
public:
    static constexpr int MAX_STEPS = 32;

    StepSequencer()
    {
        // Initialize all steps to 0.5 (center)
        for (auto& step : steps)
            step = 0.5f;
    }

    void prepare(double sampleRate)
    {
        currentSampleRate = sampleRate;
        reset();
    }

    void reset()
    {
        currentStep = 0;
        stepProgress = 0.0f;
        currentValue = steps[0];
        previousValue = steps[0];
    }

    // Set tempo sync parameters
    void setTempo(double bpm) { tempoBPM = bpm; }
    void setDivision(int div) { division = juce::jlimit(1, 64, div); } // 1 = whole, 4 = quarter, 16 = sixteenth
    void setNumSteps(int num) { numSteps = juce::jlimit(1, MAX_STEPS, num); }

    // Set step values
    void setStep(int index, float value)
    {
        if (index >= 0 && index < MAX_STEPS)
            steps[static_cast<size_t>(index)] = juce::jlimit(0.0f, 1.0f, value);
    }

    float getStep(int index) const
    {
        if (index >= 0 && index < MAX_STEPS)
            return steps[static_cast<size_t>(index)];
        return 0.0f;
    }

    // Glide/smoothing between steps
    void setGlide(float glideAmount) { glide = juce::jlimit(0.0f, 1.0f, glideAmount); }

    // Swing (affects even steps)
    void setSwing(float swingAmount) { swing = juce::jlimit(0.0f, 1.0f, swingAmount); }

    // Process one sample and return sequencer value [-1, 1] (bipolar from 0-1 steps)
    float tick()
    {
        if (currentSampleRate <= 0.0 || tempoBPM <= 0.0)
            return (currentValue - 0.5f) * 2.0f * depth;

        // Calculate step duration in samples
        // At division=4 (quarter notes), one step = one beat
        double beatsPerStep = 4.0 / static_cast<double>(division);
        double secondsPerBeat = 60.0 / tempoBPM;
        double samplesPerStep = currentSampleRate * secondsPerBeat * beatsPerStep;

        // Apply swing to even steps
        if ((currentStep % 2) == 1 && swing > 0.0f)
        {
            samplesPerStep *= (1.0f + swing * 0.5f);
        }

        // Advance position
        stepProgress += 1.0f / static_cast<float>(samplesPerStep);

        if (stepProgress >= 1.0f)
        {
            stepProgress -= 1.0f;
            previousValue = steps[static_cast<size_t>(currentStep)];
            currentStep = (currentStep + 1) % numSteps;
        }

        // Calculate output value with optional glide
        float targetValue = steps[static_cast<size_t>(currentStep)];

        if (glide > 0.0f)
        {
            // Smooth transition using glide
            float glideProgress = juce::jmin(1.0f, stepProgress / glide);
            currentValue = previousValue + (targetValue - previousValue) * glideProgress;
        }
        else
        {
            currentValue = targetValue;
        }

        // Convert to bipolar [-1, 1] and apply depth
        return (currentValue - 0.5f) * 2.0f * depth;
    }

    // Free-running mode (not synced to tempo)
    void setFreeRunning(bool freeRun) { freeRunning = freeRun; }
    void setFreeRate(float hz) { freeRateHz = juce::jmax(0.01f, hz); }

    float tickFreeRunning()
    {
        if (currentSampleRate <= 0.0)
            return (currentValue - 0.5f) * 2.0f * depth;

        // Calculate step duration for free-running mode
        float stepsPerSecond = freeRateHz * static_cast<float>(numSteps);
        float samplesPerStep = static_cast<float>(currentSampleRate) / stepsPerSecond;

        stepProgress += 1.0f / samplesPerStep;

        if (stepProgress >= 1.0f)
        {
            stepProgress -= 1.0f;
            previousValue = steps[static_cast<size_t>(currentStep)];
            currentStep = (currentStep + 1) % numSteps;
        }

        float targetValue = steps[static_cast<size_t>(currentStep)];

        if (glide > 0.0f)
        {
            float glideProgress = juce::jmin(1.0f, stepProgress / glide);
            currentValue = previousValue + (targetValue - previousValue) * glideProgress;
        }
        else
        {
            currentValue = targetValue;
        }

        return (currentValue - 0.5f) * 2.0f * depth;
    }

    // Main tick function - chooses mode
    float process()
    {
        return freeRunning ? tickFreeRunning() : tick();
    }

    // Depth control
    void setDepth(float d) { depth = juce::jlimit(0.0f, 1.0f, d); }
    float getDepth() const { return depth; }

    // Getters
    int getCurrentStep() const { return currentStep; }
    int getNumSteps() const { return numSteps; }
    float getGlide() const { return glide; }
    float getSwing() const { return swing; }
    int getDivision() const { return division; }
    bool isFreeRunning() const { return freeRunning; }
    float getFreeRate() const { return freeRateHz; }

    // Preset patterns
    void setPattern(int patternIndex)
    {
        switch (patternIndex)
        {
            case 0: // Ramp up
                for (int i = 0; i < MAX_STEPS; ++i)
                    steps[static_cast<size_t>(i)] = static_cast<float>(i) / static_cast<float>(MAX_STEPS - 1);
                break;

            case 1: // Ramp down
                for (int i = 0; i < MAX_STEPS; ++i)
                    steps[static_cast<size_t>(i)] = 1.0f - static_cast<float>(i) / static_cast<float>(MAX_STEPS - 1);
                break;

            case 2: // Triangle
                for (int i = 0; i < MAX_STEPS; ++i)
                {
                    float t = static_cast<float>(i) / static_cast<float>(MAX_STEPS - 1);
                    steps[static_cast<size_t>(i)] = t < 0.5f ? t * 2.0f : 2.0f - t * 2.0f;
                }
                break;

            case 3: // Square
                for (int i = 0; i < MAX_STEPS; ++i)
                    steps[static_cast<size_t>(i)] = (i < MAX_STEPS / 2) ? 1.0f : 0.0f;
                break;

            case 4: // Random
                for (int i = 0; i < MAX_STEPS; ++i)
                    steps[static_cast<size_t>(i)] = juce::Random::getSystemRandom().nextFloat();
                break;

            case 5: // Clear (all center)
                for (int i = 0; i < MAX_STEPS; ++i)
                    steps[static_cast<size_t>(i)] = 0.5f;
                break;

            default:
                break;
        }
    }

    // Copy step values to/from array
    void getSteps(float* dest, int count) const
    {
        int n = juce::jmin(count, MAX_STEPS);
        for (int i = 0; i < n; ++i)
            dest[i] = steps[static_cast<size_t>(i)];
    }

    void setSteps(const float* src, int count)
    {
        int n = juce::jmin(count, MAX_STEPS);
        for (int i = 0; i < n; ++i)
            steps[static_cast<size_t>(i)] = juce::jlimit(0.0f, 1.0f, src[i]);
    }

private:
    double currentSampleRate = 44100.0;
    double tempoBPM = 120.0;

    std::array<float, MAX_STEPS> steps;
    int numSteps = 16;
    int division = 16;  // 16th notes by default
    int currentStep = 0;
    float stepProgress = 0.0f;
    float currentValue = 0.5f;
    float previousValue = 0.5f;

    float glide = 0.0f;
    float swing = 0.0f;
    float depth = 1.0f;

    bool freeRunning = false;
    float freeRateHz = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepSequencer)
};

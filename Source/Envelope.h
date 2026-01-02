#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <atomic>

// DAHDSR Envelope Generator
class Envelope
{
public:
    enum class Stage
    {
        Idle,
        Delay,
        Attack,
        Hold,
        Decay,
        Sustain,
        Release
    };

    Envelope() = default;

    void prepare(double sampleRate)
    {
        currentSampleRate = sampleRate;
        reset();
    }

    void reset()
    {
        stage = Stage::Idle;
        currentValue = 0.0f;
        stageProgress = 0.0f;
    }

    // Trigger the envelope (e.g., on note on)
    void trigger()
    {
        stage = Stage::Delay;
        stageProgress = 0.0f;
        // Don't reset currentValue - allows retriggering from current position
    }

    // Release the envelope (e.g., on note off)
    void release()
    {
        if (stage != Stage::Idle && stage != Stage::Release)
        {
            stage = Stage::Release;
            stageProgress = 0.0f;
            releaseStartValue = currentValue;
        }
    }

    // Process one sample and return envelope value [0, 1]
    float tick()
    {
        if (currentSampleRate <= 0.0)
            return 0.0f;

        switch (stage)
        {
            case Stage::Idle:
                currentValue = 0.0f;
                break;

            case Stage::Delay:
            {
                float delayTime = delayMs / 1000.0f;
                float delayInc = 1.0f / (static_cast<float>(currentSampleRate) * delayTime + 1.0f);
                stageProgress += delayInc;
                currentValue = 0.0f;
                if (stageProgress >= 1.0f)
                {
                    stage = Stage::Attack;
                    stageProgress = 0.0f;
                }
                break;
            }

            case Stage::Attack:
            {
                float attackTime = attackMs / 1000.0f;
                float attackInc = 1.0f / (static_cast<float>(currentSampleRate) * attackTime + 1.0f);
                stageProgress += attackInc;

                // Exponential curve for more musical attack
                if (attackCurve > 0.0f)
                    currentValue = std::pow(stageProgress, 1.0f / (1.0f + attackCurve));
                else
                    currentValue = stageProgress;

                if (stageProgress >= 1.0f)
                {
                    currentValue = 1.0f;
                    stage = Stage::Hold;
                    stageProgress = 0.0f;
                }
                break;
            }

            case Stage::Hold:
            {
                float holdTime = holdMs / 1000.0f;
                float holdInc = 1.0f / (static_cast<float>(currentSampleRate) * holdTime + 1.0f);
                stageProgress += holdInc;
                currentValue = 1.0f;
                if (stageProgress >= 1.0f)
                {
                    stage = Stage::Decay;
                    stageProgress = 0.0f;
                }
                break;
            }

            case Stage::Decay:
            {
                float decayTime = decayMs / 1000.0f;
                float decayInc = 1.0f / (static_cast<float>(currentSampleRate) * decayTime + 1.0f);
                stageProgress += decayInc;

                // Exponential decay
                float decayRange = 1.0f - sustainLevel;
                if (decayCurve > 0.0f)
                    currentValue = 1.0f - decayRange * std::pow(stageProgress, 1.0f + decayCurve);
                else
                    currentValue = 1.0f - decayRange * stageProgress;

                if (stageProgress >= 1.0f)
                {
                    currentValue = sustainLevel;
                    stage = Stage::Sustain;
                    stageProgress = 0.0f;
                }
                break;
            }

            case Stage::Sustain:
                currentValue = sustainLevel;
                // Stay here until release() is called
                break;

            case Stage::Release:
            {
                float releaseTime = releaseMs / 1000.0f;
                float releaseInc = 1.0f / (static_cast<float>(currentSampleRate) * releaseTime + 1.0f);
                stageProgress += releaseInc;

                // Exponential release
                if (releaseCurve > 0.0f)
                    currentValue = releaseStartValue * (1.0f - std::pow(stageProgress, 1.0f + releaseCurve));
                else
                    currentValue = releaseStartValue * (1.0f - stageProgress);

                if (stageProgress >= 1.0f)
                {
                    currentValue = 0.0f;
                    stage = Stage::Idle;
                    stageProgress = 0.0f;
                }
                break;
            }
        }

        return currentValue * depth;
    }

    // Parameters (all times in ms)
    void setDelay(float ms) { delayMs = juce::jmax(0.0f, ms); }
    void setAttack(float ms) { attackMs = juce::jmax(0.1f, ms); }
    void setHold(float ms) { holdMs = juce::jmax(0.0f, ms); }
    void setDecay(float ms) { decayMs = juce::jmax(0.1f, ms); }
    void setSustain(float level) { sustainLevel = juce::jlimit(0.0f, 1.0f, level); }
    void setRelease(float ms) { releaseMs = juce::jmax(0.1f, ms); }
    void setDepth(float d) { depth = juce::jlimit(0.0f, 1.0f, d); }

    // Curve parameters (0 = linear, positive = exponential)
    void setAttackCurve(float c) { attackCurve = juce::jlimit(-1.0f, 2.0f, c); }
    void setDecayCurve(float c) { decayCurve = juce::jlimit(-1.0f, 2.0f, c); }
    void setReleaseCurve(float c) { releaseCurve = juce::jlimit(-1.0f, 2.0f, c); }

    // Getters
    float getDelay() const { return delayMs; }
    float getAttack() const { return attackMs; }
    float getHold() const { return holdMs; }
    float getDecay() const { return decayMs; }
    float getSustain() const { return sustainLevel; }
    float getRelease() const { return releaseMs; }
    float getDepth() const { return depth; }
    float getCurrentValue() const { return currentValue; }
    Stage getStage() const { return stage; }
    bool isActive() const { return stage != Stage::Idle; }

    // For visualization - get envelope shape as array of points
    std::vector<float> getEnvelopeShape(int numPoints = 100) const
    {
        std::vector<float> shape(static_cast<size_t>(numPoints));

        // Calculate total time
        float totalTime = delayMs + attackMs + holdMs + decayMs + 200.0f + releaseMs; // 200ms sustain for viz
        float timePerPoint = totalTime / static_cast<float>(numPoints);

        float time = 0.0f;
        for (int i = 0; i < numPoints; ++i)
        {
            float value = 0.0f;

            if (time < delayMs)
            {
                value = 0.0f;
            }
            else if (time < delayMs + attackMs)
            {
                float t = (time - delayMs) / attackMs;
                value = attackCurve > 0.0f ? std::pow(t, 1.0f / (1.0f + attackCurve)) : t;
            }
            else if (time < delayMs + attackMs + holdMs)
            {
                value = 1.0f;
            }
            else if (time < delayMs + attackMs + holdMs + decayMs)
            {
                float t = (time - delayMs - attackMs - holdMs) / decayMs;
                float decayRange = 1.0f - sustainLevel;
                value = decayCurve > 0.0f ?
                    1.0f - decayRange * std::pow(t, 1.0f + decayCurve) :
                    1.0f - decayRange * t;
            }
            else if (time < delayMs + attackMs + holdMs + decayMs + 200.0f)
            {
                value = sustainLevel;
            }
            else
            {
                float t = (time - delayMs - attackMs - holdMs - decayMs - 200.0f) / releaseMs;
                t = juce::jmin(1.0f, t);
                value = releaseCurve > 0.0f ?
                    sustainLevel * (1.0f - std::pow(t, 1.0f + releaseCurve)) :
                    sustainLevel * (1.0f - t);
            }

            shape[static_cast<size_t>(i)] = value;
            time += timePerPoint;
        }

        return shape;
    }

private:
    double currentSampleRate = 44100.0;

    // Stage state
    Stage stage = Stage::Idle;
    float currentValue = 0.0f;
    float stageProgress = 0.0f;
    float releaseStartValue = 0.0f;

    // Parameters
    float delayMs = 0.0f;
    float attackMs = 10.0f;
    float holdMs = 0.0f;
    float decayMs = 100.0f;
    float sustainLevel = 0.7f;
    float releaseMs = 200.0f;
    float depth = 1.0f;

    // Curves
    float attackCurve = 0.5f;
    float decayCurve = 0.5f;
    float releaseCurve = 0.5f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Envelope)
};

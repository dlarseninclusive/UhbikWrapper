#include "MultibandProcessor.h"

MultibandProcessor::MultibandProcessor()
{
    // Set filter types
    for (int ch = 0; ch < 2; ++ch)
    {
        lowMidLP[ch].setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
        lowMidHP[ch].setType(juce::dsp::LinkwitzRileyFilterType::highpass);
        midHighLP[ch].setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
        midHighHP[ch].setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    }
}

void MultibandProcessor::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;  // We process each channel independently

    for (int ch = 0; ch < 2; ++ch)
    {
        lowMidLP[ch].prepare(spec);
        lowMidHP[ch].prepare(spec);
        midHighLP[ch].prepare(spec);
        midHighHP[ch].prepare(spec);
    }

    updateFilterFrequencies();
    prepared = true;
}

void MultibandProcessor::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        lowMidLP[ch].reset();
        lowMidHP[ch].reset();
        midHighLP[ch].reset();
        midHighHP[ch].reset();
    }
}

void MultibandProcessor::setLowMidCrossover(float freqHz)
{
    lowMidFreq = juce::jlimit(20.0f, 20000.0f, freqHz);
    // Enforce lowMid < midHigh
    if (lowMidFreq >= midHighFreq)
        lowMidFreq = midHighFreq * 0.5f;
    updateFilterFrequencies();
}

void MultibandProcessor::setMidHighCrossover(float freqHz)
{
    midHighFreq = juce::jlimit(20.0f, 20000.0f, freqHz);
    // Enforce midHigh > lowMid
    if (midHighFreq <= lowMidFreq)
        midHighFreq = lowMidFreq * 2.0f;
    updateFilterFrequencies();
}

void MultibandProcessor::updateFilterFrequencies()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        lowMidLP[ch].setCutoffFrequency(lowMidFreq);
        lowMidHP[ch].setCutoffFrequency(lowMidFreq);
        midHighLP[ch].setCutoffFrequency(midHighFreq);
        midHighHP[ch].setCutoffFrequency(midHighFreq);
    }
}

void MultibandProcessor::split(const juce::AudioBuffer<float>& input,
                                juce::AudioBuffer<float>* bandBuffers)
{
    if (!prepared)
        return;

    const int numSamples = input.getNumSamples();
    const int numChannels = juce::jmin(input.getNumChannels(), 2);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* inputData = input.getReadPointer(ch);
        float* lowData = bandBuffers[LOW_BAND].getWritePointer(ch);
        float* midData = bandBuffers[MID_BAND].getWritePointer(ch);
        float* highData = bandBuffers[HIGH_BAND].getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = inputData[i];

            // First split: low/mid crossover
            // Channel arg is 0 since each filter is prepared with numChannels=1
            float lowSample = lowMidLP[ch].processSample(0, sample);
            float highPassSample = lowMidHP[ch].processSample(0, sample);

            // Second split: mid/high crossover (on the high-passed signal)
            float midSample = midHighLP[ch].processSample(0, highPassSample);
            float highSample = midHighHP[ch].processSample(0, highPassSample);

            lowData[i] = lowSample;
            midData[i] = midSample;
            highData[i] = highSample;
        }
    }
}

void MultibandProcessor::sumBands(juce::AudioBuffer<float>* bandBuffers,
                                   juce::AudioBuffer<float>& output)
{
    const int numSamples = output.getNumSamples();
    const int numChannels = juce::jmin(output.getNumChannels(), 2);

    // Check if any band is soloed
    bool anySoloed = false;
    for (int b = 0; b < NUM_BANDS; ++b)
    {
        if (bandStates[b].solo.load())
        {
            anySoloed = true;
            break;
        }
    }

    // Clear output before summing
    for (int ch = 0; ch < numChannels; ++ch)
        output.clear(ch, 0, numSamples);

    for (int b = 0; b < NUM_BANDS; ++b)
    {
        // Determine if this band should be audible
        bool muted = bandStates[b].mute.load();
        bool soloed = bandStates[b].solo.load();

        // If any band is soloed, only soloed bands play (unless also muted)
        if (anySoloed && !soloed)
            continue;
        if (muted)
            continue;

        float gain = juce::Decibels::decibelsToGain(bandStates[b].gainDb.load());

        for (int ch = 0; ch < numChannels; ++ch)
            output.addFrom(ch, 0, bandBuffers[b], ch, 0, numSamples, gain);

        // Update per-band level metering
        if (numChannels >= 2)
        {
            float peakL = bandBuffers[b].getMagnitude(0, 0, numSamples);
            float peakR = bandBuffers[b].getMagnitude(1, 0, numSamples);
            float currentL = bandStates[b].outputLevelL.load();
            float currentR = bandStates[b].outputLevelR.load();
            bandStates[b].outputLevelL.store(peakL > currentL ? peakL : currentL * 0.95f);
            bandStates[b].outputLevelR.store(peakR > currentR ? peakR : currentR * 0.95f);
        }
    }
}

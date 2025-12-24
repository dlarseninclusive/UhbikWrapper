#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class UhbikWrapperAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    UhbikWrapperAudioProcessorEditor (UhbikWrapperAudioProcessor&);
    ~UhbikWrapperAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    UhbikWrapperAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UhbikWrapperAudioProcessorEditor)
};

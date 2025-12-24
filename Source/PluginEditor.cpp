#include "PluginProcessor.h"
#include "PluginEditor.h"

UhbikWrapperAudioProcessorEditor::UhbikWrapperAudioProcessorEditor (UhbikWrapperAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Set the initial size of the "Combinator" faceplate
    setSize (600, 200);
}

UhbikWrapperAudioProcessorEditor::~UhbikWrapperAudioProcessorEditor()
{
}

void UhbikWrapperAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background - Reason-style dark grey/metal?
    g.fillAll (juce::Colour (0xff2a2a2a));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Uhbik Wrapper (Empty)", getLocalBounds(), juce::Justification::centred, 1);
}

void UhbikWrapperAudioProcessorEditor::resized()
{
    // Layout 8 macro knobs here later
}

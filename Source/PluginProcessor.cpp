#include "PluginProcessor.h"
#include "PluginEditor.h"

UhbikWrapperAudioProcessor::UhbikWrapperAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       )
#endif
{
    // Register the VST3 format so we can host VST3s
    pluginFormatManager.addDefaultFormats();
}

UhbikWrapperAudioProcessor::~UhbikWrapperAudioProcessor()
{
}

const juce::String UhbikWrapperAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool UhbikWrapperAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool UhbikWrapperAudioProcessor::producesMidi() const
{
   #if JucePlugin_WantsMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool UhbikWrapperAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double UhbikWrapperAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int UhbikWrapperAudioProcessor::getNumPrograms()
{
    return 1;
}

int UhbikWrapperAudioProcessor::getCurrentProgram()
{
    return 0;
}

void UhbikWrapperAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String UhbikWrapperAudioProcessor::getProgramName (int index)
{
    return {};
}

void UhbikWrapperAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void UhbikWrapperAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Initialize our hosted plugins here in the future
}

void UhbikWrapperAudioProcessor::releaseResources()
{
    // Free resources
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool UhbikWrapperAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void UhbikWrapperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear excess output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // --- PASSTHROUGH (Transparent) ---
    // For now, this plugin just passes audio through untouched.
    // Eventually, we will process 'buffer' through our hosted VSTs here.
}

bool UhbikWrapperAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* UhbikWrapperAudioProcessor::createEditor()
{
    return new UhbikWrapperAudioProcessorEditor (*this);
}

void UhbikWrapperAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save presets here
}

void UhbikWrapperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Load presets here
}

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UhbikWrapperAudioProcessor();
}

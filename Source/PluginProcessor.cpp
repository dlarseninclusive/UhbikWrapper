#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
#include <thread>
#include <chrono>

juce::AudioProcessorValueTreeState::ParameterLayout UhbikWrapperAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Master controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"inputGain", 1}, "Input Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mix", 1}, "Dry/Wet Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    // 8 Macro knobs
    for (int i = 0; i < NUM_MACROS; ++i)
    {
        juce::String id = "macro" + juce::String(i + 1);
        juce::String name = "Macro " + juce::String(i + 1);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id, 1}, name,
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    }

    return { params.begin(), params.end() };
}

UhbikWrapperAudioProcessor::UhbikWrapperAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)  // Sidechain input
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       ),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    pluginFormatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
    scanForPlugins();
    ensurePresetsFolderExists();
}

UhbikWrapperAudioProcessor::~UhbikWrapperAudioProcessor()
{
    effectChain.clear();
}

void UhbikWrapperAudioProcessor::scanForPlugins()
{
    juce::FileSearchPath searchPath;

#if JUCE_WINDOWS
    // Windows: C:\Program Files\Common Files\VST3
    searchPath.add(juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory)
        .getChildFile("Common Files").getChildFile("VST3"));
    // Also user's local VST3 folder
    searchPath.add(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("VST3"));
#elif JUCE_MAC
    // macOS: /Library/Audio/Plug-Ins/VST3 and ~/Library/Audio/Plug-Ins/VST3
    searchPath.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
    searchPath.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library/Audio/Plug-Ins/VST3"));
#else
    // Linux: ~/.vst3
    searchPath.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile(".vst3"));
#endif

    // Scan all paths
    for (int i = 0; i < searchPath.getNumPaths(); ++i)
    {
        auto vst3Dir = searchPath[i];
        if (!vst3Dir.exists())
        {
            DBG("VST3 directory not found: " + vst3Dir.getFullPathName());
            continue;
        }

        DBG("Scanning for plugins in: " + vst3Dir.getFullPathName());

        for (auto* format : pluginFormatManager.getFormats())
        {
            juce::PluginDirectoryScanner scanner(
                knownPluginList,
                *format,
                juce::FileSearchPath(vst3Dir.getFullPathName()),
                true,
                juce::File(),
                false
            );

            juce::String pluginName;
            while (scanner.scanNextFile(true, pluginName))
            {
                DBG("Found plugin: " + pluginName);
            }
        }
    }

    DBG("Total plugins found: " + juce::String(knownPluginList.getNumTypes()));
}

void UhbikWrapperAudioProcessor::addPlugin(const juce::PluginDescription& desc)
{
    if (debugLogging.load())
        std::cerr << "[RACK] Adding plugin: " << desc.name << std::endl << std::flush;

    juce::String errorMsg;
    auto plugin = pluginFormatManager.createPluginInstance(
        desc,
        getSampleRate() > 0 ? getSampleRate() : 44100.0,
        getBlockSize() > 0 ? getBlockSize() : 512,
        errorMsg
    );

    if (plugin != nullptr)
    {
        if (debugLogging.load())
            std::cerr << "[RACK] Plugin created, configuring buses..." << std::endl << std::flush;

        int numInputBuses = plugin->getBusCount(true);
        int numOutputBuses = plugin->getBusCount(false);
        if (debugLogging.load())
            std::cerr << "[RACK] Plugin has " << numInputBuses << " input buses, "
                      << numOutputBuses << " output buses" << std::endl << std::flush;

        // Always enable sidechain on hosted plugins that support it
        // We'll handle routing in processBlock based on whether wrapper has sidechain connected
        if (numInputBuses > 1)
        {
            auto* pluginSidechain = plugin->getBus(true, 1);
            if (pluginSidechain != nullptr)
            {
                if (debugLogging.load())
                    std::cerr << "[RACK] Enabling sidechain bus on hosted plugin (always enabled)" << std::endl << std::flush;
                pluginSidechain->enable(true);
            }
        }

        // Log final channel counts
        if (debugLogging.load())
            std::cerr << "[RACK] Plugin total channels: "
                      << plugin->getTotalNumInputChannels() << " in, "
                      << plugin->getTotalNumOutputChannels() << " out" << std::endl << std::flush;

        double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
        int bs = getBlockSize() > 0 ? getBlockSize() : 512;

        if (debugLogging.load())
            std::cerr << "[RACK] Preparing with SR=" << sr << " BS=" << bs << std::endl << std::flush;
        plugin->prepareToPlay(sr, bs);
        if (debugLogging.load())
            std::cerr << "[RACK] Plugin prepared successfully" << std::endl << std::flush;

        EffectSlot slot;
        slot.plugin = std::move(plugin);
        slot.description = desc;
        slot.bypassed = false;
        slot.ready.store(true);  // Mark as ready after prepareToPlay

        {
            const juce::SpinLock::ScopedLockType lock(chainLock);
            effectChain.push_back(std::move(slot));
        }

        if (debugLogging.load())
            std::cerr << "[RACK] Plugin added to chain. Chain size: " << effectChain.size() << std::endl << std::flush;
    }
    else
    {
        if (debugLogging.load())
            std::cerr << "[RACK] Failed to create plugin: " << errorMsg << std::endl << std::flush;
    }

    sendChangeMessage();
}

void UhbikWrapperAudioProcessor::removePlugin(int index)
{
    if (debugLogging.load())
        std::cerr << "[RACK] removePlugin called with index: " << index << std::endl << std::flush;

    if (index < 0 || index >= static_cast<int>(effectChain.size()))
    {
        if (debugLogging.load())
            std::cerr << "[RACK] Invalid index for removal: " << index << std::endl << std::flush;
        return;
    }

    {
        const juce::SpinLock::ScopedLockType lock(chainLock);
        effectChain.erase(effectChain.begin() + index);
    }

    if (debugLogging.load())
        std::cerr << "[RACK] Plugin removed. Chain size: " << effectChain.size() << std::endl << std::flush;
    sendChangeMessage();
}

void UhbikWrapperAudioProcessor::movePlugin(int fromIndex, int toIndex)
{
    if (fromIndex >= 0 && fromIndex < static_cast<int>(effectChain.size()) &&
        toIndex >= 0 && toIndex < static_cast<int>(effectChain.size()) &&
        fromIndex != toIndex)
    {
        const juce::SpinLock::ScopedLockType lock(chainLock);
        auto slot = std::move(effectChain[static_cast<size_t>(fromIndex)]);
        effectChain.erase(effectChain.begin() + fromIndex);
        effectChain.insert(effectChain.begin() + toIndex, std::move(slot));
        sendChangeMessage();
    }
}

void UhbikWrapperAudioProcessor::clearChain()
{
    if (debugLogging.load())
        std::cerr << "[RACK] clearChain called. Current size: " << effectChain.size() << std::endl << std::flush;

    {
        const juce::SpinLock::ScopedLockType lock(chainLock);
        effectChain.clear();
    }

    if (debugLogging.load())
        std::cerr << "[RACK] Chain cleared. New size: " << effectChain.size() << std::endl << std::flush;
    sendChangeMessage();
}

void UhbikWrapperAudioProcessor::setPluginBypassed(int index, bool bypassed)
{
    if (index >= 0 && index < static_cast<int>(effectChain.size()))
    {
        effectChain[static_cast<size_t>(index)].bypassed = bypassed;
        sendChangeMessage();
    }
}

void UhbikWrapperAudioProcessor::setSlotInputGain(int index, float gainDb)
{
    if (index >= 0 && index < static_cast<int>(effectChain.size()))
    {
        effectChain[static_cast<size_t>(index)].inputGainDb.store(juce::jlimit(-24.0f, 24.0f, gainDb));
    }
}

void UhbikWrapperAudioProcessor::setSlotOutputGain(int index, float gainDb)
{
    if (index >= 0 && index < static_cast<int>(effectChain.size()))
    {
        effectChain[static_cast<size_t>(index)].outputGainDb.store(juce::jlimit(-24.0f, 24.0f, gainDb));
    }
}

void UhbikWrapperAudioProcessor::setSlotMix(int index, float mixPercent)
{
    if (index >= 0 && index < static_cast<int>(effectChain.size()))
    {
        effectChain[static_cast<size_t>(index)].mixPercent.store(juce::jlimit(0.0f, 100.0f, mixPercent));
    }
}

juce::AudioPluginInstance* UhbikWrapperAudioProcessor::getPluginAt(int index)
{
    if (index >= 0 && index < static_cast<int>(effectChain.size()))
    {
        return effectChain[static_cast<size_t>(index)].plugin.get();
    }
    return nullptr;
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
    const juce::SpinLock::ScopedLockType lock(chainLock);

    auto* wrapperSidechain = getBus(true, 1);
    bool wrapperHasSidechain = (wrapperSidechain != nullptr && wrapperSidechain->isEnabled());

    if (debugLogging.load())
        std::cerr << "[RACK] prepareToPlay: SR=" << sampleRate << " BS=" << samplesPerBlock
                  << " sidechain=" << (wrapperHasSidechain ? "CONNECTED" : "not connected") << std::endl << std::flush;

    for (auto& slot : effectChain)
    {
        if (slot.plugin != nullptr)
        {
            slot.plugin->prepareToPlay(sampleRate, samplesPerBlock);
        }
    }
}

void UhbikWrapperAudioProcessor::releaseResources()
{
    const juce::SpinLock::ScopedLockType lock(chainLock);
    for (auto& slot : effectChain)
    {
        if (slot.plugin != nullptr)
        {
            slot.plugin->releaseResources();
        }
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool UhbikWrapperAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Main output must be mono or stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Main input must match main output
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    // Sidechain (second input bus) can be disabled, mono, or stereo
    if (layouts.inputBuses.size() > 1)
    {
        auto sidechainLayout = layouts.getChannelSet(true, 1);
        if (!sidechainLayout.isDisabled()
            && sidechainLayout != juce::AudioChannelSet::mono()
            && sidechainLayout != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}
#endif

void UhbikWrapperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Try to acquire lock - if chain is being modified, skip this block
    const juce::SpinLock::ScopedTryLockType lock(chainLock);
    if (!lock.isLocked())
        return;

    // Get parameter values
    float inputGainDb = apvts.getRawParameterValue("inputGain")->load();
    float outputGainDb = apvts.getRawParameterValue("outputGain")->load();
    float mixPercent = apvts.getRawParameterValue("mix")->load();

    float inputGain = juce::Decibels::decibelsToGain(inputGainDb);
    float outputGain = juce::Decibels::decibelsToGain(outputGainDb);
    float wetMix = mixPercent / 100.0f;
    float dryMix = 1.0f - wetMix;

    // Check if we have sidechain input (buffer has more than 2 channels)
    const int numBufferChannels = buffer.getNumChannels();
    const int mainChannels = 2;  // Stereo main
    const bool hasSidechainInput = (numBufferChannels > mainChannels);
    const int numSamples = buffer.getNumSamples();

    // Store dry signal for mix
    juce::AudioBuffer<float> dryBuffer;
    if (dryMix > 0.0f)
    {
        dryBuffer.setSize(mainChannels, numSamples, false, false, true);
        for (int ch = 0; ch < mainChannels && ch < numBufferChannels; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }

    // Apply input gain to main channels only
    for (int ch = 0; ch < mainChannels && ch < numBufferChannels; ++ch)
        buffer.applyGain(ch, 0, numSamples, inputGain);

    // Measure master input levels (after input gain)
    if (numBufferChannels >= 2)
    {
        float peakL = buffer.getMagnitude(0, 0, numSamples);
        float peakR = buffer.getMagnitude(1, 0, numSamples);
        // Simple peak hold with decay
        float currentL = masterInputLevelL.load();
        float currentR = masterInputLevelR.load();
        masterInputLevelL.store(peakL > currentL ? peakL : currentL * 0.95f);
        masterInputLevelR.store(peakR > currentR ? peakR : currentR * 0.95f);
    }

    // Process each effect in the chain
    juce::AudioBuffer<float> slotDryBuffer(mainChannels, numSamples);

    for (auto& slot : effectChain)
    {
        if (slot.plugin != nullptr && slot.ready.load() && !slot.bypassed)
        {
            // Get per-slot mixing parameters
            float slotInputGain = juce::Decibels::decibelsToGain(slot.inputGainDb.load());
            float slotOutputGain = juce::Decibels::decibelsToGain(slot.outputGainDb.load());
            float slotMixPct = slot.mixPercent.load();
            float slotWet = slotMixPct / 100.0f;
            float slotDry = 1.0f - slotWet;

            // Save dry signal for per-slot mix (only if mix < 100%)
            if (slotDry > 0.0f)
            {
                for (int ch = 0; ch < mainChannels && ch < numBufferChannels; ++ch)
                    slotDryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
            }

            // Apply per-slot input gain
            if (slotInputGain != 1.0f)
            {
                for (int ch = 0; ch < mainChannels && ch < numBufferChannels; ++ch)
                    buffer.applyGain(ch, 0, numSamples, slotInputGain);
            }

            // Measure per-slot input levels
            if (numBufferChannels >= 2)
            {
                float peakL = buffer.getMagnitude(0, 0, numSamples);
                float peakR = buffer.getMagnitude(1, 0, numSamples);
                float currentL = slot.inputLevelL.load();
                float currentR = slot.inputLevelR.load();
                slot.inputLevelL.store(peakL > currentL ? peakL : currentL * 0.95f);
                slot.inputLevelR.store(peakR > currentR ? peakR : currentR * 0.95f);
            }

            int pluginInputChannels = slot.plugin->getTotalNumInputChannels();

            if (pluginInputChannels <= mainChannels)
            {
                // Plugin doesn't use sidechain - pass main channels only
                if (numBufferChannels >= mainChannels)
                {
                    float* channelData[2] = { buffer.getWritePointer(0), buffer.getWritePointer(1) };
                    juce::AudioBuffer<float> mainBuffer(channelData, mainChannels, numSamples);
                    slot.plugin->processBlock(mainBuffer, midiMessages);
                }
            }
            else if (hasSidechainInput && numBufferChannels >= 4)
            {
                // Plugin uses sidechain and we have sidechain input - pass full buffer
                slot.plugin->processBlock(buffer, midiMessages);
            }
            else
            {
                // Plugin uses sidechain but wrapper doesn't have sidechain connected
                // Create a 4-channel buffer with main audio + silent sidechain
                juce::AudioBuffer<float> pluginBuffer(4, numSamples);

                // Copy main channels
                pluginBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
                pluginBuffer.copyFrom(1, 0, buffer, 1, 0, numSamples);

                // Clear sidechain channels (silence)
                pluginBuffer.clear(2, 0, numSamples);
                pluginBuffer.clear(3, 0, numSamples);

                slot.plugin->processBlock(pluginBuffer, midiMessages);

                // Copy processed main channels back
                buffer.copyFrom(0, 0, pluginBuffer, 0, 0, numSamples);
                buffer.copyFrom(1, 0, pluginBuffer, 1, 0, numSamples);
            }

            // Apply per-slot output gain
            if (slotOutputGain != 1.0f)
            {
                for (int ch = 0; ch < mainChannels && ch < numBufferChannels; ++ch)
                    buffer.applyGain(ch, 0, numSamples, slotOutputGain);
            }

            // Apply per-slot wet/dry mix
            if (slotDry > 0.0f)
            {
                for (int ch = 0; ch < mainChannels && ch < numBufferChannels; ++ch)
                {
                    buffer.applyGain(ch, 0, numSamples, slotWet);
                    buffer.addFrom(ch, 0, slotDryBuffer, ch, 0, numSamples, slotDry);
                }
            }

            // Measure per-slot output levels
            if (numBufferChannels >= 2)
            {
                float peakL = buffer.getMagnitude(0, 0, numSamples);
                float peakR = buffer.getMagnitude(1, 0, numSamples);
                float currentL = slot.outputLevelL.load();
                float currentR = slot.outputLevelR.load();
                slot.outputLevelL.store(peakL > currentL ? peakL : currentL * 0.95f);
                slot.outputLevelR.store(peakR > currentR ? peakR : currentR * 0.95f);
            }
        }
    }

    // Apply wet/dry mix
    if (dryMix > 0.0f)
    {
        for (int ch = 0; ch < mainChannels && ch < numBufferChannels; ++ch)
        {
            buffer.applyGain(ch, 0, numSamples, wetMix);
            buffer.addFrom(ch, 0, dryBuffer, ch, 0, numSamples, dryMix);
        }
    }

    // Apply output gain to main channels
    for (int ch = 0; ch < mainChannels && ch < numBufferChannels; ++ch)
        buffer.applyGain(ch, 0, numSamples, outputGain);

    // Measure master output levels
    if (numBufferChannels >= 2)
    {
        float peakL = buffer.getMagnitude(0, 0, numSamples);
        float peakR = buffer.getMagnitude(1, 0, numSamples);
        float currentL = masterOutputLevelL.load();
        float currentR = masterOutputLevelR.load();
        masterOutputLevelL.store(peakL > currentL ? peakL : currentL * 0.95f);
        masterOutputLevelR.store(peakR > currentR ? peakR : currentR * 0.95f);
    }
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
    if (debugLogging.load())
        std::cerr << "[RACK] getStateInformation called. Chain size: " << effectChain.size() << std::endl << std::flush;

    juce::ValueTree state("EffectChainState");
    state.setProperty("version", 3, nullptr);  // Version 3 adds APVTS
    state.setProperty("chainSize", static_cast<int>(effectChain.size()), nullptr);

    // Save UI state
    state.setProperty("debugLogging", debugLogging.load(), nullptr);
    state.setProperty("uiScale", uiScale.load(), nullptr);

    // Save APVTS parameters
    auto apvtsState = apvts.copyState();
    state.addChild(apvtsState, -1, nullptr);

    for (size_t i = 0; i < effectChain.size(); ++i)
    {
        auto& slot = effectChain[i];
        juce::ValueTree slotState("Slot");
        slotState.setProperty("index", static_cast<int>(i), nullptr);
        slotState.setProperty("bypassed", slot.bypassed, nullptr);
        slotState.setProperty("pluginName", slot.description.name, nullptr);

        // Per-slot mixing parameters
        slotState.setProperty("inputGainDb", slot.inputGainDb.load(), nullptr);
        slotState.setProperty("outputGainDb", slot.outputGainDb.load(), nullptr);
        slotState.setProperty("mixPercent", slot.mixPercent.load(), nullptr);

        auto descXml = slot.description.createXml();
        if (descXml != nullptr)
        {
            slotState.setProperty("description", descXml->toString(), nullptr);
            if (debugLogging.load())
                std::cerr << "[RACK] Saving slot " << i << ": " << slot.description.name << std::endl << std::flush;
        }

        if (slot.plugin != nullptr)
        {
            juce::MemoryBlock pluginState;
            slot.plugin->getStateInformation(pluginState);
            slotState.setProperty("pluginState", pluginState.toBase64Encoding(), nullptr);
            if (debugLogging.load())
                std::cerr << "[RACK] Saved plugin state size: " << pluginState.getSize() << std::endl << std::flush;
        }

        state.addChild(slotState, -1, nullptr);
    }

    auto xml = state.createXml();
    if (xml != nullptr)
    {
        copyXmlToBinary(*xml, destData);
        if (debugLogging.load())
            std::cerr << "[RACK] State saved. Total size: " << destData.getSize() << std::endl << std::flush;
    }
}

void UhbikWrapperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (debugLogging.load())
        std::cerr << "[RACK] setStateInformation called. Data size: " << sizeInBytes << std::endl << std::flush;

    if (data == nullptr || sizeInBytes == 0)
    {
        if (debugLogging.load())
            std::cerr << "[RACK] No state data to restore" << std::endl << std::flush;
        return;
    }

    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr)
    {
        if (debugLogging.load())
            std::cerr << "[RACK] Failed to parse XML from binary" << std::endl << std::flush;
        return;
    }

    juce::ValueTree state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid() || state.getType().toString() != "EffectChainState")
    {
        if (debugLogging.load())
            std::cerr << "[RACK] Invalid state format" << std::endl << std::flush;
        return;
    }

    // Restore UI state
    debugLogging.store(static_cast<bool>(state.getProperty("debugLogging", false)));
    uiScale.store(static_cast<float>(state.getProperty("uiScale", 1.0f)));

    // Restore APVTS parameters
    auto apvtsChild = state.getChildWithName("Parameters");
    if (apvtsChild.isValid())
    {
        apvts.replaceState(apvtsChild);
        if (debugLogging.load())
            std::cerr << "[RACK] APVTS state restored" << std::endl << std::flush;
    }

    int savedChainSize = state.getProperty("chainSize", 0);
    if (debugLogging.load())
        std::cerr << "[RACK] Restoring " << savedChainSize << " plugins" << std::endl << std::flush;

    std::vector<EffectSlot> newChain;

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto slotState = state.getChild(i);
        if (slotState.getType().toString() != "Slot")
            continue;

        juce::String pluginName = slotState.getProperty("pluginName", "Unknown");
        if (debugLogging.load())
            std::cerr << "[RACK] Restoring slot " << i << ": " << pluginName << std::endl << std::flush;

        juce::String descXmlStr = slotState.getProperty("description").toString();
        auto descElement = juce::XmlDocument::parse(descXmlStr);
        if (descElement == nullptr)
        {
            if (debugLogging.load())
                std::cerr << "[RACK] Failed to parse plugin description XML" << std::endl << std::flush;
            continue;
        }

        juce::PluginDescription desc;
        desc.loadFromXml(*descElement);

        juce::String errorMsg;
        auto plugin = pluginFormatManager.createPluginInstance(
            desc,
            getSampleRate() > 0 ? getSampleRate() : 44100.0,
            getBlockSize() > 0 ? getBlockSize() : 512,
            errorMsg
        );

        if (plugin != nullptr)
        {
            // Always enable sidechain on hosted plugins that support it
            int numInputBuses = plugin->getBusCount(true);
            if (numInputBuses > 1)
            {
                auto* pluginSidechain = plugin->getBus(true, 1);
                if (pluginSidechain != nullptr)
                {
                    if (debugLogging.load())
                        std::cerr << "[RACK] Enabling sidechain bus during restore (always enabled)" << std::endl << std::flush;
                    pluginSidechain->enable(true);
                }
            }

            plugin->prepareToPlay(
                getSampleRate() > 0 ? getSampleRate() : 44100.0,
                getBlockSize() > 0 ? getBlockSize() : 512
            );

            juce::String pluginStateBase64 = slotState.getProperty("pluginState").toString();
            if (pluginStateBase64.isNotEmpty())
            {
                juce::MemoryBlock pluginStateData;
                pluginStateData.fromBase64Encoding(pluginStateBase64);
                plugin->setStateInformation(pluginStateData.getData(), static_cast<int>(pluginStateData.getSize()));
                if (debugLogging.load())
                    std::cerr << "[RACK] Restored plugin state: " << pluginStateData.getSize() << " bytes" << std::endl << std::flush;
            }

            EffectSlot slot;
            slot.plugin = std::move(plugin);
            slot.description = desc;
            slot.bypassed = static_cast<bool>(slotState.getProperty("bypassed", false));
            slot.ready.store(true);  // Mark as ready after prepareToPlay

            // Restore per-slot mixing parameters
            slot.inputGainDb.store(static_cast<float>(slotState.getProperty("inputGainDb", 0.0f)));
            slot.outputGainDb.store(static_cast<float>(slotState.getProperty("outputGainDb", 0.0f)));
            slot.mixPercent.store(static_cast<float>(slotState.getProperty("mixPercent", 100.0f)));

            newChain.push_back(std::move(slot));
            if (debugLogging.load())
                std::cerr << "[RACK] Plugin restored successfully" << std::endl << std::flush;
        }
        else
        {
            if (debugLogging.load())
                std::cerr << "[RACK] Failed to create plugin: " << errorMsg << std::endl << std::flush;
        }
    }

    {
        const juce::SpinLock::ScopedLockType lock(chainLock);
        effectChain = std::move(newChain);
    }

    if (debugLogging.load())
        std::cerr << "[RACK] State restored. Chain size: " << effectChain.size() << std::endl << std::flush;
    sendChangeMessage();
}

juce::File UhbikWrapperAudioProcessor::getPresetsFolder()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("UhbikWrapper")
        .getChildFile("Presets");
}

void UhbikWrapperAudioProcessor::ensurePresetsFolderExists()
{
    auto folder = getPresetsFolder();
    if (!folder.exists())
    {
        folder.createDirectory();
    }
}

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UhbikWrapperAudioProcessor();
}

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
#include <thread>
#include <chrono>

UhbikWrapperAudioProcessor::UhbikWrapperAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       )
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
    juce::File vst3Dir = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".vst3");

    if (!vst3Dir.exists())
    {
        DBG("VST3 directory not found: " + vst3Dir.getFullPathName());
        return;
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

        // Try to disable sidechain (second input bus) if present
        if (numInputBuses > 1)
        {
            auto* sidechain = plugin->getBus(true, 1);
            if (sidechain != nullptr)
            {
                if (debugLogging.load())
                    std::cerr << "[RACK] Disabling sidechain bus" << std::endl << std::flush;
                sidechain->enable(false);
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
        sendChangeMessage();
    }
    else
    {
        if (debugLogging.load())
            std::cerr << "[RACK] Failed to create plugin: " << errorMsg << std::endl << std::flush;
    }
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

void UhbikWrapperAudioProcessor::setPluginBypassed(int index, bool bypassed)
{
    if (index >= 0 && index < static_cast<int>(effectChain.size()))
    {
        effectChain[static_cast<size_t>(index)].bypassed = bypassed;
        sendChangeMessage();
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

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Try to acquire lock - if chain is being modified, skip this block
    const juce::SpinLock::ScopedTryLockType lock(chainLock);
    if (!lock.isLocked())
        return;

    // Process each effect in the chain
    for (auto& slot : effectChain)
    {
        if (slot.plugin != nullptr && slot.ready.load() && !slot.bypassed)
        {
            slot.plugin->processBlock(buffer, midiMessages);
        }
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
    state.setProperty("version", 1, nullptr);
    state.setProperty("chainSize", static_cast<int>(effectChain.size()), nullptr);

    for (size_t i = 0; i < effectChain.size(); ++i)
    {
        auto& slot = effectChain[i];
        juce::ValueTree slotState("Slot");
        slotState.setProperty("index", static_cast<int>(i), nullptr);
        slotState.setProperty("bypassed", slot.bypassed, nullptr);
        slotState.setProperty("pluginName", slot.description.name, nullptr);

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

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
    // Force immediate flush to see output before any crash
    std::cerr << "[RACK] === CONSTRUCTOR START ===" << std::endl;
    std::cerr.flush();

    pluginFormatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());

    std::cerr << "[RACK] Scanning VST3..." << std::endl;
    std::cerr.flush();

    scanForPlugins();

    std::cerr << "[RACK] === CONSTRUCTOR DONE ===" << std::endl;
    std::cerr.flush();

    ensurePresetsFolderExists();
}

UhbikWrapperAudioProcessor::~UhbikWrapperAudioProcessor()
{
    effectChain.clear();
}

void UhbikWrapperAudioProcessor::scanForPlugins()
{
    availablePlugins.clear();

    // === Scan VST3 plugins ===
    juce::FileSearchPath searchPath;

#if JUCE_WINDOWS
    searchPath.add(juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory)
        .getChildFile("Common Files").getChildFile("VST3"));
    searchPath.add(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("VST3"));
#elif JUCE_MAC
    searchPath.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
    searchPath.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library/Audio/Plug-Ins/VST3"));
#else
    searchPath.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile(".vst3"));
#endif

    for (int i = 0; i < searchPath.getNumPaths(); ++i)
    {
        auto vst3Dir = searchPath[i];
        if (!vst3Dir.exists())
            continue;

        DBG("Scanning VST3 in: " + vst3Dir.getFullPathName());

        for (auto* format : pluginFormatManager.getFormats())
        {
            juce::PluginDirectoryScanner scanner(
                knownPluginList, *format,
                juce::FileSearchPath(vst3Dir.getFullPathName()),
                true, juce::File(), false
            );

            juce::String pluginName;
            while (scanner.scanNextFile(true, pluginName))
            {
                DBG("Found VST3: " + pluginName);
            }
        }
    }

    // Add VST3 plugins to unified list
    for (const auto& vst3Desc : knownPluginList.getTypes())
    {
        UnifiedPluginDescription unified;
        unified.format = UnifiedPluginDescription::Format::VST3;
        unified.name = vst3Desc.name;
        unified.pluginId = vst3Desc.uniqueId != 0 ? juce::String(vst3Desc.uniqueId) : vst3Desc.fileOrIdentifier;
        unified.pluginPath = vst3Desc.fileOrIdentifier;
        unified.vendor = vst3Desc.manufacturerName;
        unified.isInstrument = vst3Desc.isInstrument;
        unified.vst3Desc = vst3Desc;
        availablePlugins.push_back(unified);
    }

    std::cerr << "[RACK] VST3 plugins found: " << knownPluginList.getNumTypes() << std::endl << std::flush;

    // === Scan CLAP plugins ===
    // Delay CLAP scan slightly to avoid conflicts with library loading during project restore
    std::cerr << "[RACK] Deferring CLAP scan..." << std::endl;
    std::cerr.flush();
    juce::Timer::callAfterDelay(500, [this]() {
        std::cerr << "[RACK] Starting deferred CLAP scan..." << std::endl;
        std::cerr.flush();
        clapScanner.clear();
        clapScanner.scanDefaultLocations();
        std::cerr << "[RACK] CLAP scan complete. Found: " << clapScanner.getPlugins().size() << std::endl;
        std::cerr.flush();

        // Add CLAP plugins to unified list
        for (const auto& clapDesc : clapScanner.getPlugins())
        {
            if (clapDesc.isInstrument)
                continue;
            UnifiedPluginDescription unified;
            unified.format = UnifiedPluginDescription::Format::CLAP;
            unified.name = clapDesc.name + " (CLAP)";
            unified.pluginId = clapDesc.pluginId;
            unified.pluginPath = clapDesc.pluginPath;
            unified.vendor = clapDesc.vendor;
            unified.isInstrument = clapDesc.isInstrument;
            unified.clapDesc = clapDesc;
            availablePlugins.push_back(unified);
        }
        std::cerr << "[RACK] CLAP effects added. Total plugins: " << availablePlugins.size() << std::endl;
        std::cerr.flush();

        // Notify any listeners that the plugin list has changed
        sendChangeMessage();
    });

    // VST3 plugins are available immediately, CLAP plugins will be added after delay
    std::cerr << "[RACK] VST3 plugins available immediately: " << availablePlugins.size() << std::endl;
    std::cerr.flush();
}

void UhbikWrapperAudioProcessor::addPlugin(const juce::PluginDescription& desc)
{
    if (debugLogging.load())
        std::cerr << "[RACK] Adding VST3 plugin: " << desc.name << std::endl << std::flush;

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

        if (numInputBuses > 1)
        {
            auto* pluginSidechain = plugin->getBus(true, 1);
            if (pluginSidechain != nullptr)
            {
                if (debugLogging.load())
                    std::cerr << "[RACK] Enabling sidechain bus on hosted plugin" << std::endl << std::flush;
                pluginSidechain->enable(true);
            }
        }

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
        slot.vst3Plugin = std::move(plugin);
        slot.description.format = UnifiedPluginDescription::Format::VST3;
        slot.description.name = desc.name;
        slot.description.pluginId = desc.uniqueId != 0 ? juce::String(desc.uniqueId) : desc.fileOrIdentifier;
        slot.description.pluginPath = desc.fileOrIdentifier;
        slot.description.vendor = desc.manufacturerName;
        slot.description.isInstrument = desc.isInstrument;
        slot.description.vst3Desc = desc;
        slot.bypassed = false;
        slot.ready.store(true);

        {
            const juce::SpinLock::ScopedLockType lock(chainLock);
            effectChain.push_back(std::move(slot));
        }

        if (debugLogging.load())
            std::cerr << "[RACK] VST3 plugin added. Chain size: " << effectChain.size() << std::endl << std::flush;
    }
    else
    {
        if (debugLogging.load())
            std::cerr << "[RACK] Failed to create VST3 plugin: " << errorMsg << std::endl << std::flush;
    }

    sendChangeMessage();
}

void UhbikWrapperAudioProcessor::addPlugin(const CLAPPluginDescription& desc)
{
    if (debugLogging.load())
        std::cerr << "[RACK] Adding CLAP plugin: " << desc.name << std::endl << std::flush;

    auto clapPlugin = std::make_unique<CLAPPluginInstance>(desc);

    if (!clapPlugin->load())
    {
        if (debugLogging.load())
            std::cerr << "[RACK] Failed to load CLAP plugin" << std::endl << std::flush;
        sendChangeMessage();
        return;
    }

    double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    int bs = getBlockSize() > 0 ? getBlockSize() : 512;

    if (!clapPlugin->activate(sr, 1, static_cast<uint32_t>(bs)))
    {
        if (debugLogging.load())
            std::cerr << "[RACK] Failed to activate CLAP plugin" << std::endl << std::flush;
        sendChangeMessage();
        return;
    }

    EffectSlot slot;
    slot.clapPlugin = std::move(clapPlugin);
    slot.description.format = UnifiedPluginDescription::Format::CLAP;
    slot.description.name = desc.name;
    slot.description.pluginId = desc.pluginId;
    slot.description.pluginPath = desc.pluginPath;
    slot.description.vendor = desc.vendor;
    slot.description.isInstrument = desc.isInstrument;
    slot.description.clapDesc = desc;
    slot.bypassed = false;
    slot.ready.store(true);

    {
        const juce::SpinLock::ScopedLockType lock(chainLock);
        effectChain.push_back(std::move(slot));
    }

    if (debugLogging.load())
        std::cerr << "[RACK] CLAP plugin added. Chain size: " << effectChain.size() << std::endl << std::flush;

    sendChangeMessage();
}

void UhbikWrapperAudioProcessor::addPlugin(const UnifiedPluginDescription& desc)
{
    if (desc.format == UnifiedPluginDescription::Format::CLAP)
        addPlugin(desc.clapDesc);
    else
        addPlugin(desc.vst3Desc);
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
        return effectChain[static_cast<size_t>(index)].vst3Plugin.get();
    }
    return nullptr;
}

void UhbikWrapperAudioProcessor::closeAllCLAPEditors()
{
    for (auto& slot : effectChain)
    {
        if (slot.clapPlugin)
            slot.clapPlugin->closeEditor();
    }
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

    currentSampleRate = sampleRate;
    duckerEnvelope = 0.0f;
    duckerHoldCounter = 0.0f;

    auto* wrapperSidechain = getBus(true, 1);
    bool wrapperHasSidechain = (wrapperSidechain != nullptr && wrapperSidechain->isEnabled());

    // Always log prepareToPlay for debugging
    std::cerr << "[RACK] prepareToPlay: SR=" << sampleRate << " BS=" << samplesPerBlock
              << " sidechain=" << (wrapperHasSidechain ? "CONNECTED" : "not connected") << std::endl;
    std::cerr.flush();

    for (auto& slot : effectChain)
    {
        if (slot.vst3Plugin != nullptr)
        {
            slot.vst3Plugin->prepareToPlay(sampleRate, samplesPerBlock);
        }
        else if (slot.clapPlugin != nullptr)
        {
            // CLAP plugins were already activated during addPlugin,
            // but we may need to re-activate with new settings
            if (slot.clapPlugin->isActive())
            {
                slot.clapPlugin->deactivate();
            }
            slot.clapPlugin->activate(sampleRate, 1, static_cast<uint32_t>(samplesPerBlock));
        }
    }
}

void UhbikWrapperAudioProcessor::releaseResources()
{
    const juce::SpinLock::ScopedLockType lock(chainLock);
    for (auto& slot : effectChain)
    {
        if (slot.vst3Plugin != nullptr)
        {
            slot.vst3Plugin->releaseResources();
        }
        else if (slot.clapPlugin != nullptr)
        {
            slot.clapPlugin->deactivate();
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
    static int processCallCount = 0;
    if (processCallCount < 5)
    {
        std::cerr << "[RACK] processBlock #" << processCallCount << " samples=" << buffer.getNumSamples() << std::endl << std::flush;
        processCallCount++;
    }

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
        if (slot.hasPlugin() && slot.ready.load() && !slot.bypassed)
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

            // Process either VST3 or CLAP plugin
            if (slot.isVST3())
            {
                int pluginInputChannels = slot.vst3Plugin->getTotalNumInputChannels();

                if (pluginInputChannels <= mainChannels)
                {
                    // Plugin doesn't use sidechain - pass main channels only
                    if (numBufferChannels >= mainChannels)
                    {
                        float* channelData[2] = { buffer.getWritePointer(0), buffer.getWritePointer(1) };
                        juce::AudioBuffer<float> mainBuffer(channelData, mainChannels, numSamples);
                        slot.vst3Plugin->processBlock(mainBuffer, midiMessages);
                    }
                }
                else if (hasSidechainInput && numBufferChannels >= 4)
                {
                    // Plugin uses sidechain and we have sidechain input - pass full buffer
                    slot.vst3Plugin->processBlock(buffer, midiMessages);
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

                    slot.vst3Plugin->processBlock(pluginBuffer, midiMessages);

                    // Copy processed main channels back
                    buffer.copyFrom(0, 0, pluginBuffer, 0, 0, numSamples);
                    buffer.copyFrom(1, 0, pluginBuffer, 1, 0, numSamples);
                }
            }
            else if (slot.isCLAP())
            {
                // CLAP processing - pass stereo buffer
                if (slot.clapPlugin != nullptr && slot.clapPlugin->isActive() && numBufferChannels >= mainChannels)
                {
                    static int clapProcessCount = 0;
                    if (clapProcessCount < 3)
                    {
                        std::cerr << "[RACK] CLAP process #" << clapProcessCount << std::endl << std::flush;
                        clapProcessCount++;
                    }
                    float* channelData[2] = { buffer.getWritePointer(0), buffer.getWritePointer(1) };
                    juce::AudioBuffer<float> mainBuffer(channelData, mainChannels, numSamples);
                    slot.clapPlugin->process(mainBuffer, midiMessages);
                }
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

    // === DUCKER PROCESSING ===
    if (duckerEnabled.load() && hasSidechainInput)
    {
        // Get ducker parameters
        float thresholdDb = duckerThresholdDb.load();
        float amount = duckerAmount.load() / 100.0f;  // Convert to 0-1
        float attackMs = duckerAttackMs.load();
        float releaseMs = duckerReleaseMs.load();
        float holdMs = duckerHoldMs.load();

        // Calculate envelope coefficients
        float attackCoef = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * attackMs * 0.001f));
        float releaseCoef = std::exp(-1.0f / (static_cast<float>(currentSampleRate) * releaseMs * 0.001f));
        float holdSamples = static_cast<float>(currentSampleRate) * holdMs * 0.001f;

        // Convert threshold to linear
        float thresholdLin = juce::Decibels::decibelsToGain(thresholdDb);

        // Process sample-by-sample for accurate envelope
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Get sidechain level (channels 2 and 3)
            float scLeft = (numBufferChannels > 2) ? std::abs(buffer.getSample(2, sample)) : 0.0f;
            float scRight = (numBufferChannels > 3) ? std::abs(buffer.getSample(3, sample)) : scLeft;
            float scLevel = std::max(scLeft, scRight);

            // Envelope follower with hold
            float targetEnv = (scLevel > thresholdLin) ? 1.0f : 0.0f;

            if (targetEnv > duckerEnvelope)
            {
                // Attack - sidechain is above threshold
                duckerEnvelope = attackCoef * duckerEnvelope + (1.0f - attackCoef) * targetEnv;
                duckerHoldCounter = holdSamples;  // Reset hold counter
            }
            else if (duckerHoldCounter > 0.0f)
            {
                // Hold phase - maintain current envelope
                duckerHoldCounter -= 1.0f;
            }
            else
            {
                // Release - sidechain is below threshold and hold expired
                duckerEnvelope = releaseCoef * duckerEnvelope + (1.0f - releaseCoef) * targetEnv;
            }

            // Calculate gain reduction (1.0 = no reduction, 0.0 = full reduction)
            float gainReduction = 1.0f - (duckerEnvelope * amount);

            // Apply gain reduction to main channels
            buffer.setSample(0, sample, buffer.getSample(0, sample) * gainReduction);
            if (mainChannels > 1)
                buffer.setSample(1, sample, buffer.getSample(1, sample) * gainReduction);
        }

        // Store gain reduction for UI metering (convert to dB-friendly value)
        duckerGainReduction.store(duckerEnvelope * amount);
    }
    else
    {
        // Ducker disabled or no sidechain - decay the meter
        float currentGR = duckerGainReduction.load();
        if (currentGR > 0.001f)
            duckerGainReduction.store(currentGR * 0.95f);
        else
            duckerGainReduction.store(0.0f);
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
    state.setProperty("version", 4, nullptr);  // Version 4 adds ducker
    state.setProperty("chainSize", static_cast<int>(effectChain.size()), nullptr);

    // Save UI state
    state.setProperty("debugLogging", debugLogging.load(), nullptr);
    state.setProperty("uiScale", uiScale.load(), nullptr);

    // Save ducker state
    state.setProperty("duckerEnabled", duckerEnabled.load(), nullptr);
    state.setProperty("duckerThresholdDb", duckerThresholdDb.load(), nullptr);
    state.setProperty("duckerAmount", duckerAmount.load(), nullptr);
    state.setProperty("duckerAttackMs", duckerAttackMs.load(), nullptr);
    state.setProperty("duckerReleaseMs", duckerReleaseMs.load(), nullptr);
    state.setProperty("duckerHoldMs", duckerHoldMs.load(), nullptr);

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

        // Save format type
        slotState.setProperty("format", slot.description.format == UnifiedPluginDescription::Format::CLAP ? "CLAP" : "VST3", nullptr);

        if (slot.isVST3())
        {
            // VST3: Save PluginDescription XML
            auto descXml = slot.description.vst3Desc.createXml();
            if (descXml != nullptr)
            {
                slotState.setProperty("description", descXml->toString(), nullptr);
                if (debugLogging.load())
                    std::cerr << "[RACK] Saving VST3 slot " << i << ": " << slot.description.name << std::endl << std::flush;
            }

            if (slot.vst3Plugin != nullptr)
            {
                juce::MemoryBlock pluginState;
                slot.vst3Plugin->getStateInformation(pluginState);
                slotState.setProperty("pluginState", pluginState.toBase64Encoding(), nullptr);
                if (debugLogging.load())
                    std::cerr << "[RACK] Saved VST3 state size: " << pluginState.getSize() << std::endl << std::flush;
            }
        }
        else if (slot.isCLAP())
        {
            // CLAP: Save CLAPPluginDescription fields
            slotState.setProperty("clapPluginId", slot.description.clapDesc.pluginId, nullptr);
            slotState.setProperty("clapPluginPath", slot.description.clapDesc.pluginPath, nullptr);
            slotState.setProperty("clapVendor", slot.description.clapDesc.vendor, nullptr);
            slotState.setProperty("clapName", slot.description.clapDesc.name, nullptr);
            slotState.setProperty("clapVersion", slot.description.clapDesc.version, nullptr);

            if (debugLogging.load())
                std::cerr << "[RACK] Saving CLAP slot " << i << ": " << slot.description.name << std::endl << std::flush;

            if (slot.clapPlugin != nullptr)
            {
                juce::MemoryBlock clapState;
                slot.clapPlugin->getState(clapState);
                if (clapState.getSize() > 0)
                {
                    slotState.setProperty("pluginState", clapState.toBase64Encoding(), nullptr);
                    if (debugLogging.load())
                        std::cerr << "[RACK] Saved CLAP state size: " << clapState.getSize() << std::endl << std::flush;
                }
            }
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
    // Always log restore start to debug crashes
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

    // Restore ducker state
    duckerEnabled.store(static_cast<bool>(state.getProperty("duckerEnabled", false)));
    duckerThresholdDb.store(static_cast<float>(state.getProperty("duckerThresholdDb", -20.0f)));
    duckerAmount.store(static_cast<float>(state.getProperty("duckerAmount", 50.0f)));
    duckerAttackMs.store(static_cast<float>(state.getProperty("duckerAttackMs", 5.0f)));
    duckerReleaseMs.store(static_cast<float>(state.getProperty("duckerReleaseMs", 200.0f)));
    duckerHoldMs.store(static_cast<float>(state.getProperty("duckerHoldMs", 0.0f)));

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
        juce::String format = slotState.getProperty("format", "VST3").toString();

        if (debugLogging.load())
            std::cerr << "[RACK] Restoring " << format << " slot " << i << ": " << pluginName << std::endl << std::flush;

        double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
        int bs = getBlockSize() > 0 ? getBlockSize() : 512;

        if (format == "CLAP")
        {
            // Restore CLAP plugin
            std::cerr << "[RACK] Restoring CLAP plugin..." << std::endl << std::flush;

            CLAPPluginDescription clapDesc;
            clapDesc.pluginId = slotState.getProperty("clapPluginId", "").toString();
            clapDesc.pluginPath = slotState.getProperty("clapPluginPath", "").toString();
            clapDesc.vendor = slotState.getProperty("clapVendor", "").toString();
            clapDesc.name = slotState.getProperty("clapName", "").toString();
            clapDesc.version = slotState.getProperty("clapVersion", "").toString();

            std::cerr << "[RACK] CLAP desc: " << clapDesc.name << " path=" << clapDesc.pluginPath << std::endl << std::flush;

            auto clapPlugin = std::make_unique<CLAPPluginInstance>(clapDesc);
            std::cerr << "[RACK] CLAP instance created, loading..." << std::endl << std::flush;

            bool loaded = clapPlugin->load();
            std::cerr << "[RACK] CLAP load result: " << (loaded ? "OK" : "FAILED") << std::endl << std::flush;

            if (loaded && clapPlugin->activate(sr, 1, static_cast<uint32_t>(bs)))
            {
                // Restore CLAP state
                juce::String pluginStateBase64 = slotState.getProperty("pluginState").toString();
                if (pluginStateBase64.isNotEmpty())
                {
                    juce::MemoryBlock pluginStateData;
                    pluginStateData.fromBase64Encoding(pluginStateBase64);
                    clapPlugin->setState(pluginStateData.getData(), pluginStateData.getSize());
                    if (debugLogging.load())
                        std::cerr << "[RACK] Restored CLAP state: " << pluginStateData.getSize() << " bytes" << std::endl << std::flush;
                }

                EffectSlot slot;
                slot.clapPlugin = std::move(clapPlugin);
                slot.description.format = UnifiedPluginDescription::Format::CLAP;
                slot.description.name = clapDesc.name;
                slot.description.pluginId = clapDesc.pluginId;
                slot.description.pluginPath = clapDesc.pluginPath;
                slot.description.vendor = clapDesc.vendor;
                slot.description.clapDesc = clapDesc;
                slot.bypassed = static_cast<bool>(slotState.getProperty("bypassed", false));
                slot.ready.store(true);

                slot.inputGainDb.store(static_cast<float>(slotState.getProperty("inputGainDb", 0.0f)));
                slot.outputGainDb.store(static_cast<float>(slotState.getProperty("outputGainDb", 0.0f)));
                slot.mixPercent.store(static_cast<float>(slotState.getProperty("mixPercent", 100.0f)));

                newChain.push_back(std::move(slot));
                if (debugLogging.load())
                    std::cerr << "[RACK] CLAP plugin restored successfully" << std::endl << std::flush;
            }
            else
            {
                if (debugLogging.load())
                    std::cerr << "[RACK] Failed to load/activate CLAP plugin" << std::endl << std::flush;
            }
        }
        else
        {
            // Restore VST3 plugin
            juce::String descXmlStr = slotState.getProperty("description").toString();
            auto descElement = juce::XmlDocument::parse(descXmlStr);
            if (descElement == nullptr)
            {
                if (debugLogging.load())
                    std::cerr << "[RACK] Failed to parse VST3 plugin description XML" << std::endl << std::flush;
                continue;
            }

            juce::PluginDescription desc;
            desc.loadFromXml(*descElement);

            juce::String errorMsg;
            auto plugin = pluginFormatManager.createPluginInstance(desc, sr, bs, errorMsg);

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
                            std::cerr << "[RACK] Enabling sidechain bus during restore" << std::endl << std::flush;
                        pluginSidechain->enable(true);
                    }
                }

                plugin->prepareToPlay(sr, bs);

                juce::String pluginStateBase64 = slotState.getProperty("pluginState").toString();
                if (pluginStateBase64.isNotEmpty())
                {
                    juce::MemoryBlock pluginStateData;
                    pluginStateData.fromBase64Encoding(pluginStateBase64);
                    plugin->setStateInformation(pluginStateData.getData(), static_cast<int>(pluginStateData.getSize()));
                    if (debugLogging.load())
                        std::cerr << "[RACK] Restored VST3 state: " << pluginStateData.getSize() << " bytes" << std::endl << std::flush;
                }

                EffectSlot slot;
                slot.vst3Plugin = std::move(plugin);
                slot.description.format = UnifiedPluginDescription::Format::VST3;
                slot.description.name = desc.name;
                slot.description.pluginId = desc.uniqueId != 0 ? juce::String(desc.uniqueId) : desc.fileOrIdentifier;
                slot.description.pluginPath = desc.fileOrIdentifier;
                slot.description.vendor = desc.manufacturerName;
                slot.description.isInstrument = desc.isInstrument;
                slot.description.vst3Desc = desc;
                slot.bypassed = static_cast<bool>(slotState.getProperty("bypassed", false));
                slot.ready.store(true);

                slot.inputGainDb.store(static_cast<float>(slotState.getProperty("inputGainDb", 0.0f)));
                slot.outputGainDb.store(static_cast<float>(slotState.getProperty("outputGainDb", 0.0f)));
                slot.mixPercent.store(static_cast<float>(slotState.getProperty("mixPercent", 100.0f)));

                newChain.push_back(std::move(slot));
                if (debugLogging.load())
                    std::cerr << "[RACK] VST3 plugin restored successfully" << std::endl << std::flush;
            }
            else
            {
                if (debugLogging.load())
                    std::cerr << "[RACK] Failed to create VST3 plugin: " << errorMsg << std::endl << std::flush;
            }
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

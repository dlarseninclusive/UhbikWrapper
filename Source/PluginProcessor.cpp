#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>
#include <thread>
#include <chrono>

juce::AudioProcessorValueTreeState::ParameterLayout UhbikWrapperAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Master input/output gain
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("master_input_gain", 1),
        "Master Input Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("master_output_gain", 1),
        "Master Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    // 8 Macro knobs (0-1 range, automatable by DAW)
    for (int i = 0; i < NUM_MACROS; ++i)
    {
        juce::String id = "macro_" + juce::String(i + 1);
        juce::String name = "Macro " + juce::String(i + 1);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(id, 1),
            name,
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            0.0f));
    }

    // Per-slot parameters (bypass, wet/dry, gain) for MAX_SLOTS
    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        juce::String slotNum = juce::String(i + 1);

        // Bypass
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("slot_" + slotNum + "_bypass", 1),
            "Slot " + slotNum + " Bypass",
            false));

        // Wet/Dry mix (0-100%)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("slot_" + slotNum + "_wetdry", 1),
            "Slot " + slotNum + " Wet/Dry",
            juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
            100.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));

        // Gain (-24 to +24 dB)
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("slot_" + slotNum + "_gain", 1),
            "Slot " + slotNum + " Gain",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));
    }

    return { params.begin(), params.end() };
}

UhbikWrapperAudioProcessor::UhbikWrapperAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
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
        std::cerr << "[RACK] Plugin created, preparing to play..." << std::endl << std::flush;
        plugin->prepareToPlay(
            getSampleRate() > 0 ? getSampleRate() : 44100.0,
            getBlockSize() > 0 ? getBlockSize() : 512
        );

        EffectSlot slot;
        slot.plugin = std::move(plugin);
        slot.description = desc;
        slot.bypassed = false;

        chainBusy.store(true);
        effectChain.push_back(std::move(slot));
        chainBusy.store(false);

        std::cerr << "[RACK] Plugin added to chain. Chain size: " << effectChain.size() << std::endl << std::flush;
        sendChangeMessage();
    }
    else
    {
        std::cerr << "[RACK] Failed to create plugin: " << errorMsg << std::endl << std::flush;
    }
}

void UhbikWrapperAudioProcessor::removePlugin(int index)
{
    std::cerr << "[RACK] removePlugin called with index: " << index << std::endl << std::flush;

    if (index < 0 || index >= static_cast<int>(effectChain.size()))
    {
        std::cerr << "[RACK] Invalid index for removal: " << index << std::endl << std::flush;
        return;
    }

    chainBusy.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Let audio thread finish
    effectChain.erase(effectChain.begin() + index);
    chainBusy.store(false);

    std::cerr << "[RACK] Plugin removed. Chain size: " << effectChain.size() << std::endl << std::flush;
    sendChangeMessage();
}

void UhbikWrapperAudioProcessor::movePlugin(int fromIndex, int toIndex)
{
    if (fromIndex >= 0 && fromIndex < static_cast<int>(effectChain.size()) &&
        toIndex >= 0 && toIndex < static_cast<int>(effectChain.size()) &&
        fromIndex != toIndex)
    {
        chainBusy.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto slot = std::move(effectChain[static_cast<size_t>(fromIndex)]);
        effectChain.erase(effectChain.begin() + fromIndex);
        effectChain.insert(effectChain.begin() + toIndex, std::move(slot));
        chainBusy.store(false);
        sendChangeMessage();
    }
}

void UhbikWrapperAudioProcessor::setPluginBypassed(int index, bool bypassed)
{
    if (index >= 0 && index < static_cast<int>(effectChain.size()))
    {
        effectChain[static_cast<size_t>(index)].bypassed = bypassed;

        // Sync to APVTS parameter
        juce::String slotNum = juce::String(index + 1);
        if (auto* param = apvts.getParameter("slot_" + slotNum + "_bypass"))
            param->setValueNotifyingHost(bypassed ? 1.0f : 0.0f);

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
    chainBusy.store(true);
    for (auto& slot : effectChain)
    {
        if (slot.plugin != nullptr)
        {
            slot.plugin->prepareToPlay(sampleRate, samplesPerBlock);
        }
    }
    chainBusy.store(false);
}

void UhbikWrapperAudioProcessor::releaseResources()
{
    chainBusy.store(true);
    for (auto& slot : effectChain)
    {
        if (slot.plugin != nullptr)
        {
            slot.plugin->releaseResources();
        }
    }
    chainBusy.store(false);
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

    // Skip processing if chain is being modified
    if (chainBusy.load())
        return;

    // Apply master input gain
    float masterInGainDb = apvts.getRawParameterValue("master_input_gain")->load();
    float masterInGain = juce::Decibels::decibelsToGain(masterInGainDb);
    if (masterInGain != 1.0f)
        buffer.applyGain(masterInGain);

    // Capture master input level (RMS of first channel, or average of stereo)
    float inLevel = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        inLevel += buffer.getRMSLevel(ch, 0, buffer.getNumSamples());
    masterInputLevel.store(inLevel / static_cast<float>(buffer.getNumChannels()));

    // Process effect chain
    for (size_t i = 0; i < effectChain.size(); ++i)
    {
        auto& slot = effectChain[i];
        juce::String slotNum = juce::String(static_cast<int>(i) + 1);

        // Read parameters from APVTS
        bool bypassed = apvts.getRawParameterValue("slot_" + slotNum + "_bypass")->load() > 0.5f;
        float wetDryPercent = apvts.getRawParameterValue("slot_" + slotNum + "_wetdry")->load();
        float slotGainDb = apvts.getRawParameterValue("slot_" + slotNum + "_gain")->load();

        // Sync to slot struct for UI access
        slot.bypassed = bypassed;
        slot.wetDryMix = wetDryPercent / 100.0f;
        slot.gainDb = slotGainDb;

        if (slot.plugin != nullptr && !slot.bypassed)
        {
            // Capture input level
            float slotInLevel = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                slotInLevel += buffer.getRMSLevel(ch, 0, buffer.getNumSamples());
            slot.inputLevel.store(slotInLevel / static_cast<float>(buffer.getNumChannels()));

            // Store dry signal for wet/dry mixing
            juce::AudioBuffer<float> dryBuffer;
            if (slot.wetDryMix < 1.0f)
            {
                dryBuffer.makeCopyOf(buffer);
            }

            // Process through plugin (wet signal)
            slot.plugin->processBlock(buffer, midiMessages);

            // Apply slot gain to wet signal
            float slotGain = juce::Decibels::decibelsToGain(slot.gainDb);
            if (slotGain != 1.0f)
                buffer.applyGain(slotGain);

            // Mix wet/dry
            if (slot.wetDryMix < 1.0f)
            {
                float wet = slot.wetDryMix;
                float dry = 1.0f - wet;

                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    auto* wetData = buffer.getWritePointer(ch);
                    auto* dryData = dryBuffer.getReadPointer(ch);

                    for (int s = 0; s < buffer.getNumSamples(); ++s)
                    {
                        wetData[s] = wetData[s] * wet + dryData[s] * dry;
                    }
                }
            }

            // Capture output level
            float slotOutLevel = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                slotOutLevel += buffer.getRMSLevel(ch, 0, buffer.getNumSamples());
            slot.outputLevel.store(slotOutLevel / static_cast<float>(buffer.getNumChannels()));
        }
        else
        {
            // Bypassed - set levels to 0
            slot.inputLevel.store(0.0f);
            slot.outputLevel.store(0.0f);
        }
    }

    // Apply master output gain
    float masterOutGainDb = apvts.getRawParameterValue("master_output_gain")->load();
    float masterOutGain = juce::Decibels::decibelsToGain(masterOutGainDb);
    if (masterOutGain != 1.0f)
        buffer.applyGain(masterOutGain);

    // Capture master output level
    float outLevel = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        outLevel += buffer.getRMSLevel(ch, 0, buffer.getNumSamples());
    masterOutputLevel.store(outLevel / static_cast<float>(buffer.getNumChannels()));
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
    std::cerr << "[RACK] getStateInformation called. Chain size: " << effectChain.size() << std::endl << std::flush;

    juce::ValueTree state("EffectChainState");
    state.setProperty("version", 2, nullptr);  // Version 2 includes APVTS
    state.setProperty("chainSize", static_cast<int>(effectChain.size()), nullptr);

    // Save APVTS parameters state
    auto apvtsState = apvts.copyState();
    state.addChild(apvtsState, -1, nullptr);

    for (size_t i = 0; i < effectChain.size(); ++i)
    {
        auto& slot = effectChain[i];
        juce::ValueTree slotState("Slot");
        slotState.setProperty("index", static_cast<int>(i), nullptr);
        slotState.setProperty("bypassed", slot.bypassed, nullptr);
        slotState.setProperty("wetDryMix", slot.wetDryMix, nullptr);
        slotState.setProperty("gainDb", slot.gainDb, nullptr);
        slotState.setProperty("pluginName", slot.description.name, nullptr);

        auto descXml = slot.description.createXml();
        if (descXml != nullptr)
        {
            slotState.setProperty("description", descXml->toString(), nullptr);
            std::cerr << "[RACK] Saving slot " << i << ": " << slot.description.name << std::endl << std::flush;
        }

        if (slot.plugin != nullptr)
        {
            juce::MemoryBlock pluginState;
            slot.plugin->getStateInformation(pluginState);
            slotState.setProperty("pluginState", pluginState.toBase64Encoding(), nullptr);
            std::cerr << "[RACK] Saved plugin state size: " << pluginState.getSize() << std::endl << std::flush;
        }

        state.addChild(slotState, -1, nullptr);
    }

    auto xml = state.createXml();
    if (xml != nullptr)
    {
        copyXmlToBinary(*xml, destData);
        std::cerr << "[RACK] State saved. Total size: " << destData.getSize() << std::endl << std::flush;
    }
}

void UhbikWrapperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::cerr << "[RACK] setStateInformation called. Data size: " << sizeInBytes << std::endl << std::flush;

    if (data == nullptr || sizeInBytes == 0)
    {
        std::cerr << "[RACK] No state data to restore" << std::endl << std::flush;
        return;
    }

    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr)
    {
        std::cerr << "[RACK] Failed to parse XML from binary" << std::endl << std::flush;
        return;
    }

    juce::ValueTree state = juce::ValueTree::fromXml(*xml);
    if (!state.isValid() || state.getType().toString() != "EffectChainState")
    {
        std::cerr << "[RACK] Invalid state format" << std::endl << std::flush;
        return;
    }

    int version = state.getProperty("version", 1);
    int savedChainSize = state.getProperty("chainSize", 0);
    std::cerr << "[RACK] Restoring " << savedChainSize << " plugins (version " << version << ")" << std::endl << std::flush;

    // Restore APVTS state (version 2+)
    auto apvtsState = state.getChildWithName("Parameters");
    if (apvtsState.isValid())
    {
        apvts.replaceState(apvtsState);
        std::cerr << "[RACK] APVTS state restored" << std::endl << std::flush;
    }

    std::vector<EffectSlot> newChain;
    int slotIndex = 0;

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto slotState = state.getChild(i);
        if (slotState.getType().toString() != "Slot")
            continue;

        juce::String pluginName = slotState.getProperty("pluginName", "Unknown");
        std::cerr << "[RACK] Restoring slot " << slotIndex << ": " << pluginName << std::endl << std::flush;

        juce::String descXmlStr = slotState.getProperty("description").toString();
        auto descElement = juce::XmlDocument::parse(descXmlStr);
        if (descElement == nullptr)
        {
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
                std::cerr << "[RACK] Restored plugin state: " << pluginStateData.getSize() << " bytes" << std::endl << std::flush;
            }

            EffectSlot slot;
            slot.plugin = std::move(plugin);
            slot.description = desc;
            slot.bypassed = static_cast<bool>(slotState.getProperty("bypassed", false));
            slot.wetDryMix = static_cast<float>(slotState.getProperty("wetDryMix", 1.0f));
            slot.gainDb = static_cast<float>(slotState.getProperty("gainDb", 0.0f));

            // Sync to APVTS parameters
            juce::String slotNum = juce::String(slotIndex + 1);
            if (auto* param = apvts.getParameter("slot_" + slotNum + "_bypass"))
                param->setValueNotifyingHost(slot.bypassed ? 1.0f : 0.0f);
            if (auto* param = apvts.getParameter("slot_" + slotNum + "_wetdry"))
                param->setValueNotifyingHost(slot.wetDryMix);
            if (auto* param = apvts.getParameter("slot_" + slotNum + "_gain"))
                param->setValueNotifyingHost(param->convertTo0to1(slot.gainDb));

            newChain.push_back(std::move(slot));
            std::cerr << "[RACK] Plugin restored successfully" << std::endl << std::flush;
            slotIndex++;
        }
        else
        {
            std::cerr << "[RACK] Failed to create plugin: " << errorMsg << std::endl << std::flush;
        }
    }

    chainBusy.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    effectChain = std::move(newChain);
    chainBusy.store(false);

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
        std::cerr << "[RACK] Created presets folder: " << folder.getFullPathName() << std::endl << std::flush;
    }
}

void UhbikWrapperAudioProcessor::syncParametersToState()
{
    // Sync APVTS parameters to internal slot state
    for (size_t i = 0; i < effectChain.size() && i < MAX_SLOTS; ++i)
    {
        auto& slot = effectChain[i];
        juce::String slotNum = juce::String(static_cast<int>(i) + 1);

        if (auto* param = apvts.getParameter("slot_" + slotNum + "_bypass"))
            param->setValueNotifyingHost(slot.bypassed ? 1.0f : 0.0f);
        if (auto* param = apvts.getParameter("slot_" + slotNum + "_wetdry"))
            param->setValueNotifyingHost(slot.wetDryMix);
        if (auto* param = apvts.getParameter("slot_" + slotNum + "_gain"))
            param->setValueNotifyingHost(param->convertTo0to1(slot.gainDb));
    }
}

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UhbikWrapperAudioProcessor();
}

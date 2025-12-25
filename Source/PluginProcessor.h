#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

// Constants for DAW parameter exposure
static constexpr int MAX_SLOTS = 16;
static constexpr int NUM_MACROS = 8;

struct EffectSlot
{
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription description;
    bool bypassed = false;
    float wetDryMix = 1.0f;      // 0=dry, 1=wet
    float gainDb = 0.0f;         // -24 to +24 dB
    // Level metering data (thread-safe for audio->UI)
    std::atomic<float> inputLevel{0.0f};
    std::atomic<float> outputLevel{0.0f};

    // Default constructor
    EffectSlot() = default;

    // Move constructor (atomics can't be moved, so copy their values)
    EffectSlot(EffectSlot&& other) noexcept
        : plugin(std::move(other.plugin)),
          description(std::move(other.description)),
          bypassed(other.bypassed),
          wetDryMix(other.wetDryMix),
          gainDb(other.gainDb),
          inputLevel(other.inputLevel.load()),
          outputLevel(other.outputLevel.load())
    {}

    // Move assignment (atomics can't be moved, so copy their values)
    EffectSlot& operator=(EffectSlot&& other) noexcept
    {
        if (this != &other)
        {
            plugin = std::move(other.plugin);
            description = std::move(other.description);
            bypassed = other.bypassed;
            wetDryMix = other.wetDryMix;
            gainDb = other.gainDb;
            inputLevel.store(other.inputLevel.load());
            outputLevel.store(other.outputLevel.load());
        }
        return *this;
    }

    // Delete copy operations (unique_ptr is not copyable)
    EffectSlot(const EffectSlot&) = delete;
    EffectSlot& operator=(const EffectSlot&) = delete;
};

class UhbikWrapperAudioProcessor  : public juce::AudioProcessor,
                                    public juce::ChangeBroadcaster
{
public:
    UhbikWrapperAudioProcessor();
    ~UhbikWrapperAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Plugin Hosting Infrastructure ---
    juce::AudioPluginFormatManager pluginFormatManager;
    juce::KnownPluginList knownPluginList;

    // Effect chain - use atomic flag to prevent concurrent access
    std::vector<EffectSlot> effectChain;
    std::atomic<bool> chainBusy{false};

    // Chain management methods
    void scanForPlugins();
    void addPlugin(const juce::PluginDescription& desc);
    void removePlugin(int index);
    void movePlugin(int fromIndex, int toIndex);
    void setPluginBypassed(int index, bool bypassed);
    juce::AudioPluginInstance* getPluginAt(int index);
    int getChainSize() const { return static_cast<int>(effectChain.size()); }
    const juce::KnownPluginList& getKnownPluginList() const { return knownPluginList; }

    // Preset management
    static juce::File getPresetsFolder();
    static void ensurePresetsFolderExists();

    // DAW Parameter Exposure via APVTS
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Master level metering
    std::atomic<float> masterInputLevel{0.0f};
    std::atomic<float> masterOutputLevel{0.0f};

    // Macro knob mappings (for future use)
    struct MacroMapping {
        int slotIndex = -1;
        int parameterIndex = -1;
        float rangeMin = 0.0f;
        float rangeMax = 1.0f;
    };
    std::array<std::vector<MacroMapping>, NUM_MACROS> macroMappings;

private:
    // Parameter listener to sync APVTS with internal state
    void syncParametersToState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UhbikWrapperAudioProcessor)
};

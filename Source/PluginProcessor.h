#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

struct EffectSlot
{
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::PluginDescription description;
    bool bypassed = false;
    std::atomic<bool> ready{false};  // Set true after prepareToPlay completes

    EffectSlot() = default;
    ~EffectSlot() = default;

    // Custom move constructor since atomic isn't moveable
    EffectSlot(EffectSlot&& other) noexcept
        : plugin(std::move(other.plugin))
        , description(std::move(other.description))
        , bypassed(other.bypassed)
        , ready(other.ready.load())
    {}

    // Custom move assignment
    EffectSlot& operator=(EffectSlot&& other) noexcept
    {
        if (this != &other)
        {
            plugin = std::move(other.plugin);
            description = std::move(other.description);
            bypassed = other.bypassed;
            ready.store(other.ready.load());
        }
        return *this;
    }

    // Delete copy operations
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

    // Effect chain - use SpinLock for audio-safe synchronization
    std::vector<EffectSlot> effectChain;
    juce::SpinLock chainLock;

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

    // UI state (saved with plugin state)
    std::atomic<bool> debugLogging{true};  // On by default for debugging
    std::atomic<float> uiScale{1.0f};

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UhbikWrapperAudioProcessor)
};

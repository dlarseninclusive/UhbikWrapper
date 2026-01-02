#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "CLAPPluginHost.h"
#include "LFO.h"

// Unified plugin description that works for both VST3 and CLAP
struct UnifiedPluginDescription
{
    enum class Format { VST3, CLAP };

    Format format = Format::VST3;
    juce::String name;
    juce::String pluginId;      // VST3: uid, CLAP: reverse-DNS ID
    juce::String pluginPath;
    juce::String vendor;
    bool isInstrument = false;

    // Original descriptions for loading
    juce::PluginDescription vst3Desc;
    CLAPPluginDescription clapDesc;

    bool isValid() const { return name.isNotEmpty(); }
    juce::String getFormatName() const { return format == Format::CLAP ? "CLAP" : "VST3"; }
};

struct EffectSlot
{
    // Either VST3 or CLAP plugin (one or the other, not both)
    std::unique_ptr<juce::AudioPluginInstance> vst3Plugin;
    std::unique_ptr<CLAPPluginInstance> clapPlugin;

    UnifiedPluginDescription description;
    bool bypassed = false;
    std::atomic<bool> ready{false};  // Set true after prepareToPlay completes

    // Per-effect mixing controls
    std::atomic<float> inputGainDb{0.0f};   // -24 to +24 dB
    std::atomic<float> outputGainDb{0.0f};  // -24 to +24 dB
    std::atomic<float> mixPercent{100.0f};  // 0-100% wet

    // Level metering (updated by audio thread, read by UI)
    std::atomic<float> inputLevelL{0.0f};   // 0.0 to 1.0 peak
    std::atomic<float> inputLevelR{0.0f};
    std::atomic<float> outputLevelL{0.0f};
    std::atomic<float> outputLevelR{0.0f};

    EffectSlot() = default;
    ~EffectSlot() = default;

    // Helper to check if this slot has a valid plugin
    bool hasPlugin() const { return vst3Plugin != nullptr || clapPlugin != nullptr; }
    bool isVST3() const { return vst3Plugin != nullptr; }
    bool isCLAP() const { return clapPlugin != nullptr; }

    // Custom move constructor since atomic isn't moveable
    EffectSlot(EffectSlot&& other) noexcept
        : vst3Plugin(std::move(other.vst3Plugin))
        , clapPlugin(std::move(other.clapPlugin))
        , description(std::move(other.description))
        , bypassed(other.bypassed)
        , ready(other.ready.load())
        , inputGainDb(other.inputGainDb.load())
        , outputGainDb(other.outputGainDb.load())
        , mixPercent(other.mixPercent.load())
        , inputLevelL(other.inputLevelL.load())
        , inputLevelR(other.inputLevelR.load())
        , outputLevelL(other.outputLevelL.load())
        , outputLevelR(other.outputLevelR.load())
    {}

    // Custom move assignment
    EffectSlot& operator=(EffectSlot&& other) noexcept
    {
        if (this != &other)
        {
            vst3Plugin = std::move(other.vst3Plugin);
            clapPlugin = std::move(other.clapPlugin);
            description = std::move(other.description);
            bypassed = other.bypassed;
            ready.store(other.ready.load());
            inputGainDb.store(other.inputGainDb.load());
            outputGainDb.store(other.outputGainDb.load());
            mixPercent.store(other.mixPercent.load());
            inputLevelL.store(other.inputLevelL.load());
            inputLevelR.store(other.inputLevelR.load());
            outputLevelL.store(other.outputLevelL.load());
            outputLevelR.store(other.outputLevelR.load());
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
    // Parameter constants
    static constexpr int NUM_MACROS = 8;

    UhbikWrapperAudioProcessor();
    ~UhbikWrapperAudioProcessor() override;

    // Parameter system
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

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
    CLAPPluginScanner clapScanner;

    // Unified list of all available plugins (VST3 + CLAP)
    std::vector<UnifiedPluginDescription> availablePlugins;

    // Effect chain - use SpinLock for audio-safe synchronization
    std::vector<EffectSlot> effectChain;
    juce::SpinLock chainLock;

    // Chain management methods
    void scanForPlugins();
    void addPlugin(const juce::PluginDescription& desc);  // VST3
    void addPlugin(const CLAPPluginDescription& desc);    // CLAP
    void addPlugin(const UnifiedPluginDescription& desc); // Unified
    void removePlugin(int index);
    void movePlugin(int fromIndex, int toIndex);
    void clearChain();
    void setPluginBypassed(int index, bool bypassed);
    void setSlotInputGain(int index, float gainDb);
    void setSlotOutputGain(int index, float gainDb);
    void setSlotMix(int index, float mixPercent);
    juce::AudioPluginInstance* getPluginAt(int index);  // Returns VST3 plugin or nullptr
    int getChainSize() const { return static_cast<int>(effectChain.size()); }
    void closeAllCLAPEditors();
    const juce::KnownPluginList& getKnownPluginList() const { return knownPluginList; }
    const std::vector<UnifiedPluginDescription>& getAvailablePlugins() const { return availablePlugins; }

    // Preset management
    static juce::File getPresetsFolder();
    static void ensurePresetsFolderExists();

    // UI state (saved with plugin state)
    std::atomic<bool> debugLogging{true};  // On by default for debugging
    std::atomic<float> uiScale{1.0f};

    // Master level metering (updated by audio thread, read by UI)
    std::atomic<float> masterInputLevelL{0.0f};
    std::atomic<float> masterInputLevelR{0.0f};
    std::atomic<float> masterOutputLevelL{0.0f};
    std::atomic<float> masterOutputLevelR{0.0f};

    // Ducker parameters (exposed to UI, stored in state)
    std::atomic<bool> duckerEnabled{false};
    std::atomic<float> duckerThresholdDb{-20.0f};   // -60 to 0 dB
    std::atomic<float> duckerAmount{50.0f};          // 0 to 100%
    std::atomic<float> duckerAttackMs{5.0f};         // 0.1 to 100 ms
    std::atomic<float> duckerReleaseMs{200.0f};      // 10 to 2000 ms
    std::atomic<float> duckerHoldMs{0.0f};           // 0 to 500 ms

    // Ducker metering (for UI gain reduction display)
    std::atomic<float> duckerGainReduction{0.0f};    // 0.0 to 1.0 (amount of reduction)

    // --- Modulation System ---
    static constexpr int NUM_LFOS = 4;
    LFO lfos[NUM_LFOS];

    // Modulation routing
    std::vector<ModulationRoute> modulationRoutes;
    juce::SpinLock modulationLock;

    // Modulation management methods
    void addModulationRoute(int lfoIndex, int slotIndex, clap_id paramId, float amount);
    void removeModulationRoute(int routeIndex);
    void clearModulationRoutes();
    void setModulationAmount(int routeIndex, float amount);
    const std::vector<ModulationRoute>& getModulationRoutes() const { return modulationRoutes; }

    // Get modulatable parameters from a slot
    std::vector<CLAPParameterInfo> getModulatableParametersForSlot(int slotIndex) const;

    // LFO control
    void setLFOFrequency(int lfoIndex, float hz);
    void setLFOWaveform(int lfoIndex, LFOWaveform waveform);
    void setLFODepth(int lfoIndex, float depth);
    LFO* getLFO(int index) { return (index >= 0 && index < NUM_LFOS) ? &lfos[index] : nullptr; }

private:
    // Ducker envelope state (audio thread only)
    float duckerEnvelope = 0.0f;
    float duckerHoldCounter = 0.0f;
    double currentSampleRate = 44100.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UhbikWrapperAudioProcessor)
};

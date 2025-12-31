#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <clap/clap.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>

// Forward declarations
struct CLAPPluginInstance;
class CLAPPluginInstance;

// Component that hosts a CLAP plugin's GUI
class CLAPEditorComponent : public juce::Component
{
public:
    CLAPEditorComponent(CLAPPluginInstance* instance);
    ~CLAPEditorComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void parentHierarchyChanged() override;

    bool isGuiCreated() const { return guiCreated; }

private:
    CLAPPluginInstance* pluginInstance = nullptr;
    bool guiCreated = false;
    bool guiAttached = false;
    uint32_t guiWidth = 800;
    uint32_t guiHeight = 600;

    void createAndAttachGui();
    void destroyGui();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CLAPEditorComponent)
};

// Description of a CLAP plugin (analogous to juce::PluginDescription)
struct CLAPPluginDescription
{
    juce::String name;
    juce::String vendor;
    juce::String version;
    juce::String pluginId;      // CLAP plugin ID (reverse DNS)
    juce::String pluginPath;    // Path to .clap file
    juce::String description;
    bool isInstrument = false;
    bool hasGUI = false;

    // For compatibility with existing code
    bool isValid() const { return pluginId.isNotEmpty() && pluginPath.isNotEmpty(); }
};

// Hosted CLAP plugin instance
class CLAPPluginInstance
{
public:
    CLAPPluginInstance(const CLAPPluginDescription& desc);
    ~CLAPPluginInstance();

    // Lifecycle
    bool load();
    void unload();
    bool isLoaded() const { return plugin != nullptr; }

    // Audio processing
    bool activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount);
    void deactivate();
    bool isActive() const { return activated; }

    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    // State
    void getState(juce::MemoryBlock& destData);
    void setState(const void* data, size_t sizeInBytes);

    // GUI
    bool hasEditor() const;
    juce::Component* createEditor();
    void closeEditor();

    // For CLAPEditorComponent to access
    const clap_plugin* getPlugin() const { return plugin; }
    const clap_plugin_gui* getGuiExtension() const { return guiExt; }

    // Info
    const CLAPPluginDescription& getDescription() const { return description; }
    juce::String getName() const { return description.name; }

    // Parameters
    uint32_t getParameterCount() const;
    bool getParameterInfo(uint32_t paramIndex, clap_param_info* info) const;
    double getParameterValue(clap_id paramId) const;
    void setParameterValue(clap_id paramId, double value);

private:
    CLAPPluginDescription description;

    // Library handle
    void* libraryHandle = nullptr;
    const clap_plugin_entry* entry = nullptr;
    const clap_plugin_factory* factory = nullptr;
    const clap_plugin* plugin = nullptr;

    // Host implementation
    clap_host host;
    static const void* hostGetExtension(const clap_host* host, const char* extensionId);
    static void hostRequestRestart(const clap_host* host);
    static void hostRequestProcess(const clap_host* host);
    static void hostRequestCallback(const clap_host* host);

    // State
    bool activated = false;
    double currentSampleRate = 44100.0;
    uint32_t currentBlockSize = 512;

    // Audio port info - per-port channel counts
    struct AudioPortInfo {
        uint32_t channelCount = 0;
        bool isMain = false;
    };
    std::vector<AudioPortInfo> inputPorts;
    std::vector<AudioPortInfo> outputPorts;
    uint32_t totalInputChannels = 0;
    uint32_t totalOutputChannels = 0;

    // Process buffers - organized per port (CLAP requires this)
    std::vector<std::vector<float*>> inputPortBuffers;   // [port][channel]
    std::vector<std::vector<float*>> outputPortBuffers;  // [port][channel]
    std::vector<clap_audio_buffer> inputAudioBuffers;    // One per input port
    std::vector<clap_audio_buffer> outputAudioBuffers;   // One per output port
    clap_process processContext;

    // Scratch buffer for silent channels (sidechain when not connected)
    juce::AudioBuffer<float> scratchBuffer;

    // Event queues (required by CLAP process)
    clap_input_events inputEvents;
    clap_output_events outputEvents;

    // Static callbacks for empty event queues
    static uint32_t inputEventsSize(const clap_input_events* list);
    static const clap_event_header* inputEventsGet(const clap_input_events* list, uint32_t index);
    static bool outputEventsTryPush(const clap_output_events* list, const clap_event_header* event);

    // GUI
    std::unique_ptr<juce::Component> editorComponent;

    // Extensions cache
    const clap_plugin_audio_ports* audioPortsExt = nullptr;
    const clap_plugin_params* paramsExt = nullptr;
    const clap_plugin_state* stateExt = nullptr;
    const clap_plugin_gui* guiExt = nullptr;

    void initHost();
    bool queryExtensions();
    bool setupAudioPorts();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CLAPPluginInstance)
};

// Scanner for finding CLAP plugins
class CLAPPluginScanner
{
public:
    CLAPPluginScanner() = default;

    // Scan for CLAP plugins in standard locations
    void scanDefaultLocations();

    // Scan a specific directory
    void scanDirectory(const juce::File& directory);

    // Scan a specific .clap file
    void scanFile(const juce::File& clapFile);

    // Get discovered plugins
    const std::vector<CLAPPluginDescription>& getPlugins() const { return plugins; }

    // Find a plugin by ID
    const CLAPPluginDescription* findPluginById(const juce::String& pluginId) const;

    // Clear plugin list
    void clear() { plugins.clear(); }

private:
    std::vector<CLAPPluginDescription> plugins;

    void extractPluginsFromFile(const juce::File& clapFile);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CLAPPluginScanner)
};

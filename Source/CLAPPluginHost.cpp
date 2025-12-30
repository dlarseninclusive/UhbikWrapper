#include "CLAPPluginHost.h"
#include <iostream>

#if JUCE_WINDOWS
    #include <windows.h>
    #define CLAP_LOAD_LIBRARY(path) LoadLibraryA(path)
    #define CLAP_GET_SYMBOL(lib, sym) GetProcAddress((HMODULE)lib, sym)
    #define CLAP_UNLOAD_LIBRARY(lib) FreeLibrary((HMODULE)lib)
#else
    #include <dlfcn.h>
    #define CLAP_LOAD_LIBRARY(path) dlopen(path, RTLD_LOCAL | RTLD_LAZY)
    #define CLAP_GET_SYMBOL(lib, sym) dlsym(lib, sym)
    #define CLAP_UNLOAD_LIBRARY(lib) dlclose(lib)
#endif

// ============================================================================
// CLAPPluginInstance
// ============================================================================

CLAPPluginInstance::CLAPPluginInstance(const CLAPPluginDescription& desc)
    : description(desc)
{
    initHost();
}

CLAPPluginInstance::~CLAPPluginInstance()
{
    unload();
}

void CLAPPluginInstance::initHost()
{
    host.clap_version = CLAP_VERSION;
    host.host_data = this;
    host.name = "UhbikWrapper";
    host.vendor = "Inclusive Audio";
    host.url = "https://github.com/dlarseninclusive/UhbikWrapper";
    host.version = "0.1.0";
    host.get_extension = &CLAPPluginInstance::hostGetExtension;
    host.request_restart = &CLAPPluginInstance::hostRequestRestart;
    host.request_process = &CLAPPluginInstance::hostRequestProcess;
    host.request_callback = &CLAPPluginInstance::hostRequestCallback;

    // Initialize event queues (required for process())
    inputEvents.ctx = this;
    inputEvents.size = &CLAPPluginInstance::inputEventsSize;
    inputEvents.get = &CLAPPluginInstance::inputEventsGet;

    outputEvents.ctx = this;
    outputEvents.try_push = &CLAPPluginInstance::outputEventsTryPush;
}

const void* CLAPPluginInstance::hostGetExtension(const clap_host* /*host*/, const char* /*extensionId*/)
{
    // TODO: Implement host extensions (log, thread-check, etc.)
    return nullptr;
}

void CLAPPluginInstance::hostRequestRestart(const clap_host* /*host*/)
{
    std::cerr << "[CLAP Host] Plugin requested restart" << std::endl;
}

void CLAPPluginInstance::hostRequestProcess(const clap_host* /*host*/)
{
    // Plugin wants to be processed even without audio input
}

void CLAPPluginInstance::hostRequestCallback(const clap_host* /*host*/)
{
    // Plugin wants main-thread callback
}

// Empty event queue callbacks
uint32_t CLAPPluginInstance::inputEventsSize(const clap_input_events* /*list*/)
{
    return 0;  // No input events
}

const clap_event_header* CLAPPluginInstance::inputEventsGet(const clap_input_events* /*list*/, uint32_t /*index*/)
{
    return nullptr;  // No events to get
}

bool CLAPPluginInstance::outputEventsTryPush(const clap_output_events* /*list*/, const clap_event_header* /*event*/)
{
    return true;  // Accept but ignore output events for now
}

bool CLAPPluginInstance::load()
{
    if (isLoaded())
        return true;

    juce::String path = description.pluginPath;

#if JUCE_MAC
    // On macOS, .clap is a bundle - we need to load the actual binary inside
    juce::File clapBundle(path);
    juce::File binary = clapBundle.getChildFile("Contents/MacOS").getChildFile(clapBundle.getFileNameWithoutExtension());
    if (!binary.existsAsFile())
    {
        // Try finding any executable in MacOS folder
        auto macosDir = clapBundle.getChildFile("Contents/MacOS");
        auto files = macosDir.findChildFiles(juce::File::findFiles, false);
        if (!files.isEmpty())
            binary = files[0];
    }
    path = binary.getFullPathName();
#endif

    std::cerr << "[CLAP Host] Loading: " << path << std::endl;

    // Load the dynamic library
    libraryHandle = CLAP_LOAD_LIBRARY(path.toRawUTF8());
    if (!libraryHandle)
    {
        std::cerr << "[CLAP Host] Failed to load library" << std::endl;
        return false;
    }

    // Get the entry point (clap_entry is a data symbol, not a function)
    entry = (const clap_plugin_entry*)CLAP_GET_SYMBOL(libraryHandle, "clap_entry");

    if (!entry)
    {
        std::cerr << "[CLAP Host] No clap_entry found" << std::endl;
        CLAP_UNLOAD_LIBRARY(libraryHandle);
        libraryHandle = nullptr;
        return false;
    }

    // Initialize the entry
    if (!entry->init(description.pluginPath.toRawUTF8()))
    {
        std::cerr << "[CLAP Host] Entry init failed" << std::endl;
        CLAP_UNLOAD_LIBRARY(libraryHandle);
        libraryHandle = nullptr;
        entry = nullptr;
        return false;
    }

    // Get the plugin factory
    factory = static_cast<const clap_plugin_factory*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (!factory)
    {
        std::cerr << "[CLAP Host] No plugin factory" << std::endl;
        entry->deinit();
        CLAP_UNLOAD_LIBRARY(libraryHandle);
        libraryHandle = nullptr;
        entry = nullptr;
        return false;
    }

    // Create the plugin instance
    plugin = factory->create_plugin(factory, &host, description.pluginId.toRawUTF8());
    if (!plugin)
    {
        std::cerr << "[CLAP Host] Failed to create plugin instance" << std::endl;
        entry->deinit();
        CLAP_UNLOAD_LIBRARY(libraryHandle);
        libraryHandle = nullptr;
        entry = nullptr;
        factory = nullptr;
        return false;
    }

    // Initialize the plugin
    if (!plugin->init(plugin))
    {
        std::cerr << "[CLAP Host] Plugin init failed" << std::endl;
        plugin->destroy(plugin);
        entry->deinit();
        CLAP_UNLOAD_LIBRARY(libraryHandle);
        libraryHandle = nullptr;
        entry = nullptr;
        factory = nullptr;
        plugin = nullptr;
        return false;
    }

    // Query extensions
    queryExtensions();

    std::cerr << "[CLAP Host] Plugin loaded: " << description.name << std::endl;
    return true;
}

void CLAPPluginInstance::unload()
{
    if (activated)
        deactivate();

    closeEditor();

    if (plugin)
    {
        plugin->destroy(plugin);
        plugin = nullptr;
    }

    if (entry)
    {
        entry->deinit();
        entry = nullptr;
    }

    factory = nullptr;
    audioPortsExt = nullptr;
    paramsExt = nullptr;
    stateExt = nullptr;
    guiExt = nullptr;

    if (libraryHandle)
    {
        CLAP_UNLOAD_LIBRARY(libraryHandle);
        libraryHandle = nullptr;
    }
}

bool CLAPPluginInstance::queryExtensions()
{
    if (!plugin)
        return false;

    audioPortsExt = static_cast<const clap_plugin_audio_ports*>(
        plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
    paramsExt = static_cast<const clap_plugin_params*>(
        plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    stateExt = static_cast<const clap_plugin_state*>(
        plugin->get_extension(plugin, CLAP_EXT_STATE));
    guiExt = static_cast<const clap_plugin_gui*>(
        plugin->get_extension(plugin, CLAP_EXT_GUI));

    return true;
}

bool CLAPPluginInstance::setupAudioPorts()
{
    if (!plugin || !audioPortsExt)
    {
        // Default to stereo in/out if no audio ports extension
        numAudioInputs = 2;
        numAudioOutputs = 2;
        return true;
    }

    // Count audio ports
    numAudioInputs = 0;
    numAudioOutputs = 0;

    uint32_t inputPortCount = audioPortsExt->count(plugin, true);
    uint32_t outputPortCount = audioPortsExt->count(plugin, false);

    for (uint32_t i = 0; i < inputPortCount; ++i)
    {
        clap_audio_port_info info;
        if (audioPortsExt->get(plugin, i, true, &info))
            numAudioInputs += info.channel_count;
    }

    for (uint32_t i = 0; i < outputPortCount; ++i)
    {
        clap_audio_port_info info;
        if (audioPortsExt->get(plugin, i, false, &info))
            numAudioOutputs += info.channel_count;
    }

    std::cerr << "[CLAP Host] Audio ports: " << numAudioInputs << " in, " << numAudioOutputs << " out" << std::endl;
    return true;
}

bool CLAPPluginInstance::activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount)
{
    if (!plugin || activated)
        return false;

    currentSampleRate = sampleRate;
    currentBlockSize = maxFrameCount;

    setupAudioPorts();

    if (!plugin->activate(plugin, sampleRate, minFrameCount, maxFrameCount))
    {
        std::cerr << "[CLAP Host] Plugin activation failed" << std::endl;
        return false;
    }

    // Allocate buffer pointer arrays
    uint32_t maxChannels = juce::jmax(numAudioInputs, numAudioOutputs);
    inputBufferPtrs.resize(numAudioInputs > 0 ? numAudioInputs : 2);
    outputBufferPtrs.resize(numAudioOutputs > 0 ? numAudioOutputs : 2);

    // Allocate scratch buffer for extra channels (sidechain, etc.)
    // This provides silent buffers for channels we don't have real data for
    scratchBuffer.setSize(static_cast<int>(maxChannels), static_cast<int>(maxFrameCount));
    scratchBuffer.clear();

    // Start processing
    if (!plugin->start_processing(plugin))
    {
        std::cerr << "[CLAP Host] start_processing failed" << std::endl;
        plugin->deactivate(plugin);
        return false;
    }

    activated = true;
    std::cerr << "[CLAP Host] Plugin activated at " << sampleRate << " Hz" << std::endl;
    return true;
}

void CLAPPluginInstance::deactivate()
{
    if (!plugin || !activated)
        return;

    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    activated = false;

    inputBufferPtrs.clear();
    outputBufferPtrs.clear();

    std::cerr << "[CLAP Host] Plugin deactivated" << std::endl;
}

void CLAPPluginInstance::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    if (!plugin || !activated)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Make sure scratch buffer is large enough
    if (scratchBuffer.getNumSamples() < numSamples)
        scratchBuffer.setSize(scratchBuffer.getNumChannels(), numSamples);

    // Setup input buffer pointers - use real buffers for available channels
    for (int ch = 0; ch < juce::jmin(numChannels, static_cast<int>(inputBufferPtrs.size())); ++ch)
        inputBufferPtrs[static_cast<size_t>(ch)] = buffer.getWritePointer(ch);

    // Use scratch buffer (silence) for extra channels the plugin expects (e.g., sidechain)
    for (size_t ch = static_cast<size_t>(numChannels); ch < inputBufferPtrs.size(); ++ch)
    {
        if (static_cast<int>(ch) < scratchBuffer.getNumChannels())
        {
            scratchBuffer.clear(static_cast<int>(ch), 0, numSamples);
            inputBufferPtrs[ch] = scratchBuffer.getWritePointer(static_cast<int>(ch));
        }
    }

    // Setup output buffer pointers (same as input for in-place processing)
    for (int ch = 0; ch < juce::jmin(numChannels, static_cast<int>(outputBufferPtrs.size())); ++ch)
        outputBufferPtrs[static_cast<size_t>(ch)] = buffer.getWritePointer(ch);

    // Use scratch buffer for extra output channels
    for (size_t ch = static_cast<size_t>(numChannels); ch < outputBufferPtrs.size(); ++ch)
    {
        if (static_cast<int>(ch) < scratchBuffer.getNumChannels())
            outputBufferPtrs[ch] = scratchBuffer.getWritePointer(static_cast<int>(ch));
    }

    // Setup CLAP audio buffers - report the actual channel counts the plugin expects
    inputAudioBuffer.data32 = inputBufferPtrs.data();
    inputAudioBuffer.data64 = nullptr;
    inputAudioBuffer.channel_count = static_cast<uint32_t>(inputBufferPtrs.size());
    inputAudioBuffer.latency = 0;
    inputAudioBuffer.constant_mask = 0;

    outputAudioBuffer.data32 = outputBufferPtrs.data();
    outputAudioBuffer.data64 = nullptr;
    outputAudioBuffer.channel_count = static_cast<uint32_t>(outputBufferPtrs.size());
    outputAudioBuffer.latency = 0;
    outputAudioBuffer.constant_mask = 0;

    // Setup process context
    memset(&processContext, 0, sizeof(processContext));
    processContext.steady_time = -1;
    processContext.frames_count = static_cast<uint32_t>(numSamples);
    processContext.transport = nullptr;
    processContext.audio_inputs = &inputAudioBuffer;
    processContext.audio_outputs = &outputAudioBuffer;
    processContext.audio_inputs_count = 1;
    processContext.audio_outputs_count = 1;
    processContext.in_events = &inputEvents;
    processContext.out_events = &outputEvents;

    // Process!
    plugin->process(plugin, &processContext);
}

void CLAPPluginInstance::getState(juce::MemoryBlock& destData)
{
    if (!plugin || !stateExt)
        return;

    struct WriteContext
    {
        juce::MemoryBlock* data;
    };

    clap_ostream stream;
    WriteContext ctx{&destData};
    stream.ctx = &ctx;
    stream.write = [](const clap_ostream* s, const void* buffer, uint64_t size) -> int64_t
    {
        auto* context = static_cast<WriteContext*>(s->ctx);
        context->data->append(buffer, static_cast<size_t>(size));
        return static_cast<int64_t>(size);
    };

    stateExt->save(plugin, &stream);
}

void CLAPPluginInstance::setState(const void* data, size_t sizeInBytes)
{
    if (!plugin || !stateExt || !data || sizeInBytes == 0)
        return;

    struct ReadContext
    {
        const uint8_t* data;
        size_t size;
        size_t position;
    };

    clap_istream stream;
    ReadContext ctx{static_cast<const uint8_t*>(data), sizeInBytes, 0};
    stream.ctx = &ctx;
    stream.read = [](const clap_istream* s, void* buffer, uint64_t size) -> int64_t
    {
        auto* context = static_cast<ReadContext*>(s->ctx);
        size_t remaining = context->size - context->position;
        size_t toRead = juce::jmin(static_cast<size_t>(size), remaining);
        if (toRead > 0)
        {
            memcpy(buffer, context->data + context->position, toRead);
            context->position += toRead;
        }
        return static_cast<int64_t>(toRead);
    };

    stateExt->load(plugin, &stream);
}

bool CLAPPluginInstance::hasEditor() const
{
    return guiExt != nullptr;
}

juce::Component* CLAPPluginInstance::createEditor()
{
    // TODO: Implement GUI hosting
    // This requires platform-specific window embedding
    return nullptr;
}

void CLAPPluginInstance::closeEditor()
{
    // Only destroy GUI if one was actually created
    if (editorComponent != nullptr && guiExt && plugin)
    {
        guiExt->destroy(plugin);
    }
    editorComponent.reset();
}

uint32_t CLAPPluginInstance::getParameterCount() const
{
    if (!plugin || !paramsExt)
        return 0;
    return paramsExt->count(plugin);
}

bool CLAPPluginInstance::getParameterInfo(uint32_t paramIndex, clap_param_info* info) const
{
    if (!plugin || !paramsExt || !info)
        return false;
    return paramsExt->get_info(plugin, paramIndex, info);
}

double CLAPPluginInstance::getParameterValue(clap_id paramId) const
{
    if (!plugin || !paramsExt)
        return 0.0;
    double value = 0.0;
    paramsExt->get_value(plugin, paramId, &value);
    return value;
}

void CLAPPluginInstance::setParameterValue(clap_id paramId, double value)
{
    if (!plugin || !paramsExt)
        return;
    // Note: For proper implementation, parameter changes should go through the event system
    // This is a simplified direct-set approach
}

// ============================================================================
// CLAPPluginScanner
// ============================================================================

void CLAPPluginScanner::scanDefaultLocations()
{
    juce::StringArray paths;

#if JUCE_WINDOWS
    // Windows CLAP paths
    paths.add("C:\\Program Files\\Common Files\\CLAP");
    paths.add(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                  .getChildFile("CLAP").getFullPathName());
#elif JUCE_MAC
    // macOS CLAP paths
    paths.add("/Library/Audio/Plug-Ins/CLAP");
    paths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                  .getChildFile("Library/Audio/Plug-Ins/CLAP").getFullPathName());
#else
    // Linux CLAP paths
    paths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                  .getChildFile(".clap").getFullPathName());
    paths.add("/usr/lib/clap");
    paths.add("/usr/local/lib/clap");
#endif

    for (const auto& path : paths)
    {
        juce::File dir(path);
        if (dir.isDirectory())
        {
            std::cerr << "[CLAP Scanner] Scanning: " << path << std::endl;
            scanDirectory(dir);
        }
    }

    std::cerr << "[CLAP Scanner] Found " << plugins.size() << " CLAP plugins" << std::endl;
}

void CLAPPluginScanner::scanDirectory(const juce::File& directory)
{
    if (!directory.isDirectory())
        return;

    for (const auto& file : directory.findChildFiles(juce::File::findFilesAndDirectories, false))
    {
        if (file.hasFileExtension(".clap") || file.isDirectory())
        {
            if (file.hasFileExtension(".clap"))
                scanFile(file);
            else
                scanDirectory(file);  // Recurse into subdirectories
        }
    }
}

void CLAPPluginScanner::scanFile(const juce::File& clapFile)
{
    if (!clapFile.exists())
        return;

    // Skip our own plugin to avoid recursion/crashes
    if (clapFile.getFileNameWithoutExtension().containsIgnoreCase("UhbikWrapper"))
    {
        std::cerr << "[CLAP Scanner] Skipping self: " << clapFile.getFileName() << std::endl;
        return;
    }

    extractPluginsFromFile(clapFile);
}

void CLAPPluginScanner::extractPluginsFromFile(const juce::File& clapFile)
{
    juce::String path = clapFile.getFullPathName();

    // On Linux, resolve symlinks (common for u-he plugins)
#if JUCE_LINUX
    if (clapFile.isSymbolicLink())
    {
        juce::File resolved = clapFile.getLinkedTarget();
        if (resolved.existsAsFile())
            path = resolved.getFullPathName();
    }
#endif

#if JUCE_MAC
    // On macOS, .clap is a bundle
    juce::File binary = clapFile.getChildFile("Contents/MacOS").getChildFile(clapFile.getFileNameWithoutExtension());
    if (!binary.existsAsFile())
    {
        auto macosDir = clapFile.getChildFile("Contents/MacOS");
        auto files = macosDir.findChildFiles(juce::File::findFiles, false);
        if (!files.isEmpty())
            binary = files[0];
        else
            return;
    }
    path = binary.getFullPathName();
#endif

    // Load the library temporarily to read plugin info
    void* lib = CLAP_LOAD_LIBRARY(path.toRawUTF8());
    if (!lib)
        return;

    // clap_entry is a data symbol, not a function
    const clap_plugin_entry* entry = (const clap_plugin_entry*)CLAP_GET_SYMBOL(lib, "clap_entry");

    if (!entry)
    {
        CLAP_UNLOAD_LIBRARY(lib);
        return;
    }

    if (!entry->init(clapFile.getFullPathName().toRawUTF8()))
    {
        CLAP_UNLOAD_LIBRARY(lib);
        return;
    }

    auto* factory = static_cast<const clap_plugin_factory*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (factory)
    {
        uint32_t count = factory->get_plugin_count(factory);
        for (uint32_t i = 0; i < count; ++i)
        {
            auto* desc = factory->get_plugin_descriptor(factory, i);
            if (desc)
            {
                CLAPPluginDescription pluginDesc;
                pluginDesc.pluginId = desc->id;
                pluginDesc.name = desc->name;
                pluginDesc.vendor = desc->vendor;
                pluginDesc.version = desc->version;
                pluginDesc.description = desc->description ? desc->description : "";
                pluginDesc.pluginPath = clapFile.getFullPathName();

                // Check features for instrument vs effect
                if (desc->features)
                {
                    for (const char* const* f = desc->features; *f; ++f)
                    {
                        if (strcmp(*f, CLAP_PLUGIN_FEATURE_INSTRUMENT) == 0)
                            pluginDesc.isInstrument = true;
                    }
                }

                plugins.push_back(pluginDesc);
                std::cerr << "[CLAP Scanner] Found: " << pluginDesc.name << " (" << pluginDesc.pluginId << ")" << std::endl;
            }
        }
    }

    entry->deinit();
    CLAP_UNLOAD_LIBRARY(lib);
}

const CLAPPluginDescription* CLAPPluginScanner::findPluginById(const juce::String& pluginId) const
{
    for (const auto& plugin : plugins)
    {
        if (plugin.pluginId == pluginId)
            return &plugin;
    }
    return nullptr;
}

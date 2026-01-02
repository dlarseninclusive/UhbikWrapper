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

// Static timer support structure (cross-platform)
clap_host_timer_support CLAPPluginInstance::hostTimerSupport = {
    &CLAPPluginInstance::hostRegisterTimer,
    &CLAPPluginInstance::hostUnregisterTimer
};

bool CLAPPluginInstance::hostRegisterTimer(const clap_host* host, uint32_t periodMs, clap_id* timerId)
{
    auto* self = static_cast<CLAPPluginInstance*>(host->host_data);
    clap_id id = self->nextTimerId++;

    uint64_t now = static_cast<uint64_t>(juce::Time::currentTimeMillis());
    self->registeredTimers.push_back({id, periodMs, now});
    *timerId = id;

    std::cerr << "[CLAP Host] register_timer: id=" << id << " period=" << periodMs << "ms" << std::endl;
    return true;
}

bool CLAPPluginInstance::hostUnregisterTimer(const clap_host* host, clap_id timerId)
{
    auto* self = static_cast<CLAPPluginInstance*>(host->host_data);
    std::cerr << "[CLAP Host] unregister_timer: id=" << timerId << std::endl;

    auto& timers = self->registeredTimers;
    timers.erase(std::remove_if(timers.begin(), timers.end(),
        [timerId](const RegisteredTimer& t) { return t.id == timerId; }), timers.end());
    return true;
}

void CLAPPluginInstance::fireTimers()
{
    if (registeredTimers.empty() || !timerExt || !plugin)
        return;

    uint64_t now = static_cast<uint64_t>(juce::Time::currentTimeMillis());

    for (auto& timer : registeredTimers)
    {
        if (now - timer.lastFireTime >= timer.periodMs)
        {
            timer.lastFireTime = now;
            timerExt->on_timer(plugin, timer.id);
        }
    }
}

// Static GUI host support structure
clap_host_gui CLAPPluginInstance::hostGui = {
    &CLAPPluginInstance::hostGuiResizeHintsChanged,
    &CLAPPluginInstance::hostGuiRequestResize,
    &CLAPPluginInstance::hostGuiRequestShow,
    &CLAPPluginInstance::hostGuiRequestHide,
    &CLAPPluginInstance::hostGuiClosed
};

void CLAPPluginInstance::hostGuiResizeHintsChanged(const clap_host* /*host*/)
{
    // Plugin's resize hints changed - we don't use these currently
}

bool CLAPPluginInstance::hostGuiRequestResize(const clap_host* host, uint32_t width, uint32_t height)
{
    auto* self = static_cast<CLAPPluginInstance*>(host->host_data);
    std::cerr << "[CLAP Host] GUI request_resize: " << width << " x " << height << std::endl;

    if (self->editorWindow)
    {
        // Resize the editor window to match the plugin's requested size
        juce::MessageManager::callAsync([self, width, height]()
        {
            if (self->editorWindow)
            {
                int w = static_cast<int>(width);
                int h = static_cast<int>(height);
                self->editorWindow->setSize(w, h);
                self->editorWindow->centreWithSize(w, h);
            }
        });
        return true;
    }
    return false;
}

bool CLAPPluginInstance::hostGuiRequestShow(const clap_host* host)
{
    auto* self = static_cast<CLAPPluginInstance*>(host->host_data);
    if (self->editorWindow)
    {
        self->editorWindow->setVisible(true);
        return true;
    }
    return false;
}

bool CLAPPluginInstance::hostGuiRequestHide(const clap_host* host)
{
    auto* self = static_cast<CLAPPluginInstance*>(host->host_data);
    if (self->editorWindow)
    {
        self->editorWindow->setVisible(false);
        return true;
    }
    return false;
}

void CLAPPluginInstance::hostGuiClosed(const clap_host* /*host*/, bool /*wasDestroyed*/)
{
    // Plugin notified us that its GUI was closed
    std::cerr << "[CLAP Host] GUI closed notification" << std::endl;
}

#if JUCE_LINUX
// Static POSIX FD support structure
clap_host_posix_fd_support CLAPPluginInstance::hostPosixFdSupport = {
    &CLAPPluginInstance::hostRegisterFD,
    &CLAPPluginInstance::hostModifyFD,
    &CLAPPluginInstance::hostUnregisterFD
};

bool CLAPPluginInstance::hostRegisterFD(const clap_host* host, int fd, clap_posix_fd_flags_t flags)
{
    auto* self = static_cast<CLAPPluginInstance*>(host->host_data);
    std::cerr << "[CLAP Host] register_fd: fd=" << fd << " flags=" << flags << std::endl;

    // Check if already registered
    for (auto& reg : self->registeredFDs)
    {
        if (reg.fd == fd)
        {
            reg.flags = flags;
            return true;
        }
    }

    self->registeredFDs.push_back({fd, flags});
    return true;
}

bool CLAPPluginInstance::hostModifyFD(const clap_host* host, int fd, clap_posix_fd_flags_t flags)
{
    auto* self = static_cast<CLAPPluginInstance*>(host->host_data);
    std::cerr << "[CLAP Host] modify_fd: fd=" << fd << " flags=" << flags << std::endl;

    for (auto& reg : self->registeredFDs)
    {
        if (reg.fd == fd)
        {
            reg.flags = flags;
            return true;
        }
    }
    return false;
}

bool CLAPPluginInstance::hostUnregisterFD(const clap_host* host, int fd)
{
    auto* self = static_cast<CLAPPluginInstance*>(host->host_data);
    std::cerr << "[CLAP Host] unregister_fd: fd=" << fd << std::endl;

    auto& fds = self->registeredFDs;
    fds.erase(std::remove_if(fds.begin(), fds.end(),
        [fd](const RegisteredFD& r) { return r.fd == fd; }), fds.end());
    return true;
}

void CLAPPluginInstance::pollFDs()
{
    if (registeredFDs.empty() || !posixFdExt || !plugin)
        return;

    // Build poll structures
    std::vector<struct pollfd> pfds;
    pfds.reserve(registeredFDs.size());

    for (const auto& reg : registeredFDs)
    {
        struct pollfd pfd;
        pfd.fd = reg.fd;
        pfd.events = 0;
        if (reg.flags & CLAP_POSIX_FD_READ) pfd.events |= POLLIN;
        if (reg.flags & CLAP_POSIX_FD_WRITE) pfd.events |= POLLOUT;
        if (reg.flags & CLAP_POSIX_FD_ERROR) pfd.events |= POLLERR;
        pfd.revents = 0;
        pfds.push_back(pfd);
    }

    // Non-blocking poll
    int result = poll(pfds.data(), pfds.size(), 0);

    // Dispatch events to plugin - call on_fd for each registered FD
    // even if poll returns 0, as a timer tick for the plugin
    for (size_t i = 0; i < pfds.size(); ++i)
    {
        clap_posix_fd_flags_t flags = 0;

        if (result > 0 && pfds[i].revents != 0)
        {
            // Real events detected
            if (pfds[i].revents & POLLIN) flags |= CLAP_POSIX_FD_READ;
            if (pfds[i].revents & POLLOUT) flags |= CLAP_POSIX_FD_WRITE;
            if (pfds[i].revents & POLLERR) flags |= CLAP_POSIX_FD_ERROR;
        }
        else
        {
            // No events, but still call on_fd as a tick with READ flag
            // Some plugins expect regular polling
            flags = CLAP_POSIX_FD_READ;
        }

        posixFdExt->on_fd(plugin, registeredFDs[i].fd, flags);
    }
}
#endif

const void* CLAPPluginInstance::hostGetExtension(const clap_host* host, const char* extensionId)
{
    juce::ignoreUnused(host);

    // GUI support (for resize requests)
    if (strcmp(extensionId, CLAP_EXT_GUI) == 0)
    {
        std::cerr << "[CLAP Host] Providing gui extension" << std::endl;
        return &hostGui;
    }

    // Timer support (cross-platform)
    if (strcmp(extensionId, CLAP_EXT_TIMER_SUPPORT) == 0)
    {
        std::cerr << "[CLAP Host] Providing timer-support extension" << std::endl;
        return &hostTimerSupport;
    }

#if JUCE_LINUX
    if (strcmp(extensionId, CLAP_EXT_POSIX_FD_SUPPORT) == 0)
    {
        std::cerr << "[CLAP Host] Providing posix-fd-support extension" << std::endl;
        return &hostPosixFdSupport;
    }
#endif

    // Other extensions not implemented yet
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

    // Timer support (cross-platform)
    timerExt = static_cast<const clap_plugin_timer_support*>(
        plugin->get_extension(plugin, CLAP_EXT_TIMER_SUPPORT));
    if (timerExt)
        std::cerr << "[CLAP Host] Plugin supports timer-support" << std::endl;

#if JUCE_LINUX
    posixFdExt = static_cast<const clap_plugin_posix_fd_support*>(
        plugin->get_extension(plugin, CLAP_EXT_POSIX_FD_SUPPORT));
    if (posixFdExt)
        std::cerr << "[CLAP Host] Plugin supports posix-fd-support" << std::endl;
#endif

    return true;
}

bool CLAPPluginInstance::setupAudioPorts()
{
    inputPorts.clear();
    outputPorts.clear();
    totalInputChannels = 0;
    totalOutputChannels = 0;

    if (!plugin || !audioPortsExt)
    {
        // Default to single stereo port in/out if no audio ports extension
        inputPorts.push_back({2, true});
        outputPorts.push_back({2, true});
        totalInputChannels = 2;
        totalOutputChannels = 2;
        std::cerr << "[CLAP Host] No audio ports ext, defaulting to stereo" << std::endl;
        return true;
    }

    // Get input ports info
    uint32_t inputPortCount = audioPortsExt->count(plugin, true);
    for (uint32_t i = 0; i < inputPortCount; ++i)
    {
        clap_audio_port_info info;
        if (audioPortsExt->get(plugin, i, true, &info))
        {
            bool isMain = (info.flags & CLAP_AUDIO_PORT_IS_MAIN) != 0;
            inputPorts.push_back({info.channel_count, isMain});
            totalInputChannels += info.channel_count;
            std::cerr << "[CLAP Host] Input port " << i << ": " << info.channel_count
                      << " ch, " << (isMain ? "main" : "aux") << std::endl;
        }
    }

    // Get output ports info
    uint32_t outputPortCount = audioPortsExt->count(plugin, false);
    for (uint32_t i = 0; i < outputPortCount; ++i)
    {
        clap_audio_port_info info;
        if (audioPortsExt->get(plugin, i, false, &info))
        {
            bool isMain = (info.flags & CLAP_AUDIO_PORT_IS_MAIN) != 0;
            outputPorts.push_back({info.channel_count, isMain});
            totalOutputChannels += info.channel_count;
            std::cerr << "[CLAP Host] Output port " << i << ": " << info.channel_count
                      << " ch, " << (isMain ? "main" : "aux") << std::endl;
        }
    }

    // Ensure at least one port each
    if (inputPorts.empty())
        inputPorts.push_back({2, true});
    if (outputPorts.empty())
        outputPorts.push_back({2, true});

    std::cerr << "[CLAP Host] Total: " << inputPorts.size() << " input ports ("
              << totalInputChannels << " ch), " << outputPorts.size() << " output ports ("
              << totalOutputChannels << " ch)" << std::endl;
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

    // Allocate per-port buffer pointer arrays
    inputPortBuffers.resize(inputPorts.size());
    for (size_t port = 0; port < inputPorts.size(); ++port)
        inputPortBuffers[port].resize(inputPorts[port].channelCount, nullptr);

    outputPortBuffers.resize(outputPorts.size());
    for (size_t port = 0; port < outputPorts.size(); ++port)
        outputPortBuffers[port].resize(outputPorts[port].channelCount, nullptr);

    // Allocate CLAP audio buffer structures (one per port)
    inputAudioBuffers.resize(inputPorts.size());
    outputAudioBuffers.resize(outputPorts.size());

    // Allocate scratch buffer for silent channels (sidechain, etc.)
    uint32_t maxChannels = juce::jmax(totalInputChannels, totalOutputChannels);
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

    inputPortBuffers.clear();
    outputPortBuffers.clear();
    inputAudioBuffers.clear();
    outputAudioBuffers.clear();

    std::cerr << "[CLAP Host] Plugin deactivated" << std::endl;
}

void CLAPPluginInstance::process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    if (!plugin || !activated)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();  // Typically 2 (stereo)

    // Make sure scratch buffer is large enough
    if (scratchBuffer.getNumSamples() < numSamples)
        scratchBuffer.setSize(scratchBuffer.getNumChannels(), numSamples);

    // Clear scratch buffer for this block (used for silent sidechain channels)
    scratchBuffer.clear();

    // Track which JUCE buffer channel we're at
    int juceChannel = 0;
    int scratchChannel = 0;

    // Setup input port buffers
    // Port 0 (main) gets real audio, other ports (sidechain) get silence
    for (size_t port = 0; port < inputPorts.size(); ++port)
    {
        uint32_t portChannels = inputPorts[port].channelCount;
        bool isMain = inputPorts[port].isMain;

        for (uint32_t ch = 0; ch < portChannels; ++ch)
        {
            if (isMain && juceChannel < numChannels)
            {
                // Main port - use real audio from JUCE buffer
                inputPortBuffers[port][ch] = buffer.getWritePointer(juceChannel);
                juceChannel++;
            }
            else
            {
                // Aux port (sidechain) or no more real channels - use silence
                inputPortBuffers[port][ch] = scratchBuffer.getWritePointer(scratchChannel % scratchBuffer.getNumChannels());
                scratchChannel++;
            }
        }

        // Setup the CLAP audio buffer for this port
        inputAudioBuffers[port].data32 = inputPortBuffers[port].data();
        inputAudioBuffers[port].data64 = nullptr;
        inputAudioBuffers[port].channel_count = portChannels;
        inputAudioBuffers[port].latency = 0;
        inputAudioBuffers[port].constant_mask = 0;
    }

    // Reset for output
    juceChannel = 0;
    scratchChannel = 0;

    // Setup output port buffers
    // Port 0 (main) writes to JUCE buffer, other ports write to scratch
    for (size_t port = 0; port < outputPorts.size(); ++port)
    {
        uint32_t portChannels = outputPorts[port].channelCount;
        bool isMain = outputPorts[port].isMain;

        for (uint32_t ch = 0; ch < portChannels; ++ch)
        {
            if (isMain && juceChannel < numChannels)
            {
                // Main port - write to real JUCE buffer
                outputPortBuffers[port][ch] = buffer.getWritePointer(juceChannel);
                juceChannel++;
            }
            else
            {
                // Aux port or no more real channels - write to scratch (discarded)
                outputPortBuffers[port][ch] = scratchBuffer.getWritePointer(scratchChannel % scratchBuffer.getNumChannels());
                scratchChannel++;
            }
        }

        // Setup the CLAP audio buffer for this port
        outputAudioBuffers[port].data32 = outputPortBuffers[port].data();
        outputAudioBuffers[port].data64 = nullptr;
        outputAudioBuffers[port].channel_count = portChannels;
        outputAudioBuffers[port].latency = 0;
        outputAudioBuffers[port].constant_mask = 0;
    }

    // Setup process context
    memset(&processContext, 0, sizeof(processContext));
    processContext.steady_time = -1;
    processContext.frames_count = static_cast<uint32_t>(numSamples);
    processContext.transport = nullptr;
    processContext.audio_inputs = inputAudioBuffers.data();
    processContext.audio_outputs = outputAudioBuffers.data();
    processContext.audio_inputs_count = static_cast<uint32_t>(inputAudioBuffers.size());
    processContext.audio_outputs_count = static_cast<uint32_t>(outputAudioBuffers.size());
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
#if JUCE_LINUX
    if (!plugin || !guiExt)
        return false;
    return guiExt->is_api_supported(plugin, CLAP_WINDOW_API_X11, false);
#else
    return false;  // Only Linux/X11 implemented for now
#endif
}

CLAPEditorWindow* CLAPPluginInstance::createEditorWindow()
{
    std::cerr << "[CLAP Host] createEditorWindow called" << std::endl;

    if (!hasEditor())
    {
        std::cerr << "[CLAP Host] hasEditor() returned false" << std::endl;
        return nullptr;
    }

    // Close existing editor if any
    closeEditor();

    std::cerr << "[CLAP Host] Creating CLAPEditorWindow" << std::endl;

    // Create the raw X11 editor window
    editorWindow = std::make_unique<CLAPEditorWindow>(this);

    if (editorWindow->isGuiCreated())
    {
        std::cerr << "[CLAP Host] CLAPEditorWindow created successfully" << std::endl;
        return editorWindow.get();
    }
    else
    {
        std::cerr << "[CLAP Host] Failed to create CLAP GUI" << std::endl;
        editorWindow.reset();
        return nullptr;
    }
}

void CLAPPluginInstance::closeEditor()
{
    if (editorWindow)
    {
        // The window destructor will handle GUI cleanup
        editorWindow.reset();
    }
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
// CLAPEditorWindow - JUCE-based window with POSIX FD polling for CLAP GUI
// ============================================================================

CLAPEditorWindow::CLAPEditorWindow(CLAPPluginInstance* instance)
    : DocumentWindow(instance->getName() + " (CLAP)",
                     juce::Colour(0xff1e1e1e),
                     DocumentWindow::closeButton)
    , pluginInstance(instance)
{
    std::cerr << "[CLAP GUI] CLAPEditorWindow constructor started" << std::endl;

    auto* gui = pluginInstance->getGuiExtension();
    auto* plugin = pluginInstance->getPlugin();

    if (!gui || !plugin)
    {
        std::cerr << "[CLAP GUI] No GUI extension" << std::endl;
        return;
    }

#if JUCE_LINUX
    const char* api = CLAP_WINDOW_API_X11;
#elif JUCE_MAC
    const char* api = CLAP_WINDOW_API_COCOA;
#elif JUCE_WINDOWS
    const char* api = CLAP_WINDOW_API_WIN32;
#else
    const char* api = nullptr;
#endif

    // Use embedded mode
    std::cerr << "[CLAP GUI] Creating embedded GUI..." << std::endl;

    if (gui->create(plugin, api, false))  // false = embedded
    {
        guiCreated = true;
        std::cerr << "[CLAP GUI] Created EMBEDDED GUI" << std::endl;

        setUsingNativeTitleBar(true);
        setResizable(false, false);

        content = std::make_unique<Content>();

        uint32_t width = 800, height = 600;
        gui->get_size(plugin, &width, &height);
        std::cerr << "[CLAP GUI] Plugin requested size: " << width << " x " << height << std::endl;

        int w = static_cast<int>(width);
        int h = static_cast<int>(height);

        // Set content size and add to window
        content->setSize(w, h);
        setContentNonOwned(content.get(), true);

        // Centre the window on screen
        centreWithSize(getWidth(), getHeight());

        std::cerr << "[CLAP GUI] Window size: " << getWidth() << " x " << getHeight() << std::endl;

        setVisible(true);
        attachPluginGui();

        // Start timer to poll POSIX FDs for plugin event handling
        startTimer(16);  // ~60fps
        std::cerr << "[CLAP GUI] Started FD polling timer" << std::endl;
    }
    else
    {
        std::cerr << "[CLAP GUI] Failed to create GUI" << std::endl;
    }

    std::cerr << "[CLAP GUI] CLAPEditorWindow constructor finished, guiCreated=" << guiCreated << std::endl;
}

CLAPEditorWindow::~CLAPEditorWindow()
{
    std::cerr << "[CLAP GUI] CLAPEditorWindow destructor" << std::endl;

    stopTimer();

    if (pluginInstance && guiCreated)
    {
        auto* gui = pluginInstance->getGuiExtension();
        auto* plugin = pluginInstance->getPlugin();

        if (gui && plugin)
        {
            gui->hide(plugin);
            gui->destroy(plugin);
            std::cerr << "[CLAP GUI] CLAP GUI destroyed" << std::endl;
        }
    }

    content.reset();
}

void CLAPEditorWindow::closeButtonPressed()
{
    std::cerr << "[CLAP GUI] Close button pressed" << std::endl;
    stopTimer();

    // Properly destroy the GUI
    if (pluginInstance && guiCreated)
    {
        auto* gui = pluginInstance->getGuiExtension();
        auto* plugin = pluginInstance->getPlugin();

        if (gui && plugin)
        {
            gui->hide(plugin);
            gui->destroy(plugin);
            guiCreated = false;
            std::cerr << "[CLAP GUI] CLAP GUI destroyed on close" << std::endl;
        }
    }

    setVisible(false);
}

void CLAPEditorWindow::timerCallback()
{
    if (!pluginInstance)
        return;

    // Fire registered timers (cross-platform)
    pluginInstance->fireTimers();

#if JUCE_LINUX
    // Poll registered file descriptors and dispatch events to plugin
    pluginInstance->pollFDs();
#endif
}

void CLAPEditorWindow::attachPluginGui()
{
    if (!guiCreated || !pluginInstance || !content)
        return;

    auto* gui = pluginInstance->getGuiExtension();
    auto* plugin = pluginInstance->getPlugin();
    if (!gui || !plugin)
        return;

    // Get the native window handle from the content component
    auto* peer = content->getPeer();
    if (!peer)
    {
        std::cerr << "[CLAP GUI] No peer available" << std::endl;
        return;
    }

    void* nativeHandle = peer->getNativeHandle();
    if (!nativeHandle)
    {
        std::cerr << "[CLAP GUI] No native handle" << std::endl;
        return;
    }

    std::cerr << "[CLAP GUI] Native handle: " << nativeHandle << std::endl;

    clap_window_t window;
#if JUCE_LINUX
    window.api = CLAP_WINDOW_API_X11;
    window.x11 = reinterpret_cast<clap_xwnd>(nativeHandle);
#elif JUCE_MAC
    window.api = CLAP_WINDOW_API_COCOA;
    window.cocoa = nativeHandle;
#elif JUCE_WINDOWS
    window.api = CLAP_WINDOW_API_WIN32;
    window.win32 = nativeHandle;
#endif

    if (gui->set_parent(plugin, &window))
    {
        std::cerr << "[CLAP GUI] Parent set successfully" << std::endl;

        if (gui->show(plugin))
        {
            std::cerr << "[CLAP GUI] Plugin GUI shown" << std::endl;

#if JUCE_LINUX
            // Debug: find child window and check its event mask
            Display* display = XOpenDisplay(nullptr);
            if (display)
            {
                Window parentWin = reinterpret_cast<Window>(nativeHandle);
                Window root, parent;
                Window* children = nullptr;
                unsigned int numChildren = 0;

                XSync(display, False);

                if (XQueryTree(display, parentWin, &root, &parent, &children, &numChildren))
                {
                    std::cerr << "[CLAP GUI] Parent window " << parentWin << " has " << numChildren << " children" << std::endl;
                    for (unsigned int i = 0; i < numChildren; ++i)
                    {
                        XWindowAttributes attrs;
                        if (XGetWindowAttributes(display, children[i], &attrs))
                        {
                            std::cerr << "[CLAP GUI] Child " << i << ": window=" << children[i]
                                      << " size=" << attrs.width << "x" << attrs.height
                                      << " all_event_masks=0x" << std::hex << attrs.all_event_masks
                                      << " your_event_mask=0x" << attrs.your_event_mask << std::dec << std::endl;
                        }
                    }
                    if (children) XFree(children);
                }
                XCloseDisplay(display);
            }
#endif
        }
        else
        {
            std::cerr << "[CLAP GUI] Failed to show plugin GUI" << std::endl;
        }
    }
    else
    {
        std::cerr << "[CLAP GUI] Failed to set parent" << std::endl;
    }
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

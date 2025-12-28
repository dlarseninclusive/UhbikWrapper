#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

UhbikWrapperAudioProcessorEditor::UhbikWrapperAudioProcessorEditor (UhbikWrapperAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (700, 500);
    setResizable(true, true);
    setResizeLimits(500, 300, 2000, 2000);
    setWantsKeyboardFocus(true);

    audioProcessor.addChangeListener(this);

    // Preset browser (always visible)
    presetBrowser = std::make_unique<PresetBrowser>(UhbikWrapperAudioProcessor::getPresetsFolder());
    presetBrowser->setListener(this);
    addAndMakeVisible(*presetBrowser);
    presetBrowser->setBounds(0, 0, 200, 500);

    chainViewport.setViewedComponent(&chainContainer, false);
    chainViewport.setScrollBarsShown(true, false);
    chainViewport.setScrollBarThickness(12);
    addAndMakeVisible(chainViewport);

    pluginSelector.setTextWhenNothingSelected("Select plugin to add...");
    pluginSelector.addListener(this);
    addAndMakeVisible(pluginSelector);

    addButton.addListener(this);
    addButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff44aa44));
    addAndMakeVisible(addButton);

    viewMenuButton.addListener(this);
    viewMenuButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff555555));
    addAndMakeVisible(viewMenuButton);

    debugToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::black);
    debugToggle.setColour(juce::ToggleButton::tickColourId, juce::Colours::black);
    debugToggle.setToggleState(audioProcessor.debugLogging.load(), juce::dontSendNotification);
    debugToggle.onClick = [this]() {
        audioProcessor.debugLogging.store(debugToggle.getToggleState());
    };
    addAndMakeVisible(debugToggle);

    populatePluginSelector();
    refreshChainDisplay();

    // Apply saved UI scale
    uiScale = audioProcessor.uiScale.load();
    if (uiScale != 1.0f)
    {
        setSize(static_cast<int>(700 * uiScale), static_cast<int>(500 * uiScale));
    }

    startTimerHz(2);
}

UhbikWrapperAudioProcessorEditor::~UhbikWrapperAudioProcessorEditor()
{
    audioProcessor.removeChangeListener(this);
    pluginSelector.removeListener(this);
    addButton.removeListener(this);
    viewMenuButton.removeListener(this);
    pluginEditorWindows.clear();
}

void UhbikWrapperAudioProcessorEditor::timerCallback()
{
    auto chainSize = audioProcessor.getChainSize();
    juce::String newStatus = juce::String(chainSize) + " effect(s) in chain";

    if (statusMessage != newStatus)
    {
        statusMessage = newStatus;
        repaint();
    }
}

void UhbikWrapperAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refreshChainDisplay();
}

void UhbikWrapperAudioProcessorEditor::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &pluginSelector)
    {
        int selectedIndex = pluginSelector.getSelectedItemIndex();
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(effectPlugins.size()))
        {
            std::cerr << "[UI] Auto-adding plugin: " << effectPlugins[static_cast<size_t>(selectedIndex)].name << std::endl << std::flush;
            audioProcessor.addPlugin(effectPlugins[static_cast<size_t>(selectedIndex)]);
            pluginSelector.setSelectedItemIndex(-1, juce::dontSendNotification); // Reset selection
        }
    }
}

void UhbikWrapperAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &addButton)
    {
        int selectedIndex = pluginSelector.getSelectedItemIndex();
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(effectPlugins.size()))
        {
            audioProcessor.addPlugin(effectPlugins[static_cast<size_t>(selectedIndex)]);
            pluginSelector.setSelectedItemIndex(-1, juce::dontSendNotification);
        }
    }
    else if (button == &viewMenuButton)
    {
        showViewMenu();
    }
}

void UhbikWrapperAudioProcessorEditor::populatePluginSelector()
{
    pluginSelector.clear();
    effectPlugins.clear();

    auto& pluginList = audioProcessor.getKnownPluginList();
    auto types = pluginList.getTypes();

    // Filter for effects only (no instruments)
    for (const auto& desc : types)
    {
        if (!desc.isInstrument)
        {
            effectPlugins.push_back(desc);
        }
    }

    std::cerr << "[UI] Populating selector with " << effectPlugins.size() << " effects (filtered from " << types.size() << " total)" << std::endl << std::flush;

    int id = 1;
    for (const auto& desc : effectPlugins)
    {
        pluginSelector.addItem(desc.name, id++);
    }
}

void UhbikWrapperAudioProcessorEditor::refreshChainDisplay()
{
    slotComponents.clear();

    int chainSize = audioProcessor.getChainSize();
    int slotHeight = 60;
    int slotSpacing = 4;
    int leftPadding = 8;
    int rightPadding = 20;  // More space for scrollbar
    int topPadding = 8;

    int containerWidth = chainViewport.getWidth();
    if (containerWidth < 100) containerWidth = 300;  // Fallback if not yet sized

    chainContainer.setSize(
        containerWidth - rightPadding,
        chainSize > 0 ? chainSize * (slotHeight + slotSpacing) + topPadding * 2 : 100
    );

    for (int i = 0; i < chainSize; ++i)
    {
        auto& slot = audioProcessor.effectChain[static_cast<size_t>(i)];
        bool canMoveUp = (i > 0);
        bool canMoveDown = (i < chainSize - 1);
        auto slotComp = std::make_unique<EffectSlotComponent>(
            i,
            slot.description.name,
            slot.bypassed,
            canMoveUp,
            canMoveDown
        );
        slotComp->setListener(this);
        slotComp->setBounds(leftPadding, topPadding + i * (slotHeight + slotSpacing),
                            containerWidth - leftPadding - rightPadding, slotHeight);
        chainContainer.addAndMakeVisible(slotComp.get());
        slotComponents.push_back(std::move(slotComp));
    }

    chainContainer.repaint();
}

void UhbikWrapperAudioProcessorEditor::effectSlotEditClicked(int slotIndex)
{
    std::cerr << "[UI] Edit clicked for slot: " << slotIndex << std::endl << std::flush;
    openPluginEditor(slotIndex);
}

void UhbikWrapperAudioProcessorEditor::effectSlotBypassClicked(int slotIndex)
{
    std::cerr << "[UI] Bypass clicked for slot: " << slotIndex << std::endl << std::flush;
    if (slotIndex >= 0 && slotIndex < audioProcessor.getChainSize())
    {
        bool currentBypass = audioProcessor.effectChain[static_cast<size_t>(slotIndex)].bypassed;
        audioProcessor.setPluginBypassed(slotIndex, !currentBypass);
    }
}

void UhbikWrapperAudioProcessorEditor::effectSlotRemoveClicked(int slotIndex)
{
    std::cerr << "[UI] Remove clicked for slot: " << slotIndex << std::endl << std::flush;

    // Use async call to avoid issues with deleting while in callback
    juce::MessageManager::callAsync([this, slotIndex]() {
        audioProcessor.removePlugin(slotIndex);
    });
}

void UhbikWrapperAudioProcessorEditor::effectSlotMoveUpClicked(int slotIndex)
{
    std::cerr << "[UI] Move up clicked for slot: " << slotIndex << std::endl << std::flush;
    if (slotIndex > 0)
    {
        juce::MessageManager::callAsync([this, slotIndex]() {
            audioProcessor.movePlugin(slotIndex, slotIndex - 1);
        });
    }
}

void UhbikWrapperAudioProcessorEditor::effectSlotMoveDownClicked(int slotIndex)
{
    std::cerr << "[UI] Move down clicked for slot: " << slotIndex << std::endl << std::flush;
    if (slotIndex < audioProcessor.getChainSize() - 1)
    {
        juce::MessageManager::callAsync([this, slotIndex]() {
            audioProcessor.movePlugin(slotIndex, slotIndex + 1);
        });
    }
}

void UhbikWrapperAudioProcessorEditor::openPluginEditor(int slotIndex)
{
    auto* plugin = audioProcessor.getPluginAt(slotIndex);
    if (plugin == nullptr || !plugin->hasEditor())
        return;

    auto* editor = plugin->createEditor();
    if (editor == nullptr)
        return;

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = plugin->getName();
    options.dialogBackgroundColour = juce::Colour(0xff1e1e1e);
    options.content.setOwned(editor);
    options.launchAsync();
}

void UhbikWrapperAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Preset browser width offset
    int browserWidth = 200;

    // Dark rack background (only rack area, not preset browser)
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(browserWidth, 0, getWidth() - browserWidth, getHeight());

    // Header bar with gradient (after preset browser)
    juce::ColourGradient headerGradient(juce::Colour(0xffff8800), static_cast<float>(browserWidth), 0,
                                         juce::Colour(0xffcc5500), static_cast<float>(browserWidth), 50, false);
    g.setGradientFill(headerGradient);
    g.fillRect(browserWidth, 0, getWidth() - browserWidth, 50);

    // Header text (after preset browser)
    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    g.drawFittedText("EFFECT RACK", browserWidth + 15, 0, 250, 50, juce::Justification::centredLeft, 1);

    // Rack rails on left and right (after preset browser)
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(browserWidth, 50, 15, getHeight() - 80);
    g.fillRect(getWidth() - 15, 50, 15, getHeight() - 80);

    // Rail holes
    g.setColour(juce::Colour(0xff1a1a1a));
    for (int y = 70; y < getHeight() - 50; y += 30)
    {
        g.fillEllipse(static_cast<float>(browserWidth + 4), static_cast<float>(y), 7.0f, 7.0f);
        g.fillEllipse(static_cast<float>(getWidth() - 11), static_cast<float>(y), 7.0f, 7.0f);
    }

    // Footer bar (after preset browser)
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(browserWidth, getHeight() - 30, getWidth() - browserWidth, 30);

    g.setColour(juce::Colours::lightgrey);
    g.setFont(12.0f);
    g.drawFittedText(statusMessage, browserWidth, getHeight() - 30, getWidth() - browserWidth, 30, juce::Justification::centred, 1);

    // Empty state message
    if (audioProcessor.getChainSize() == 0)
    {
        auto emptyBounds = chainViewport.getBounds();
        g.setColour(juce::Colour(0xff666666));
        g.setFont(16.0f);
        g.drawFittedText("Select a plugin from the dropdown to add to the rack",
                         emptyBounds, juce::Justification::centred, 2);
    }
}

void UhbikWrapperAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Preset browser on left (always visible) - always reserve space
    int browserWidth = 200;
    auto browserBounds = bounds.removeFromLeft(browserWidth);
    if (presetBrowser != nullptr)
    {
        presetBrowser->setBounds(browserBounds);
    }

    // Header area
    auto headerBounds = bounds.removeFromTop(50);
    int buttonY = 11;  // Center buttons vertically in 50px header

    // Right side: [selector] [+]
    int addBtnWidth = 40;
    int selectorWidth = juce::jmin(200, bounds.getWidth() / 3);

    addButton.setBounds(getWidth() - addBtnWidth - 15, buttonY, addBtnWidth, 28);
    pluginSelector.setBounds(addButton.getX() - selectorWidth - 6, buttonY, selectorWidth, 28);

    // View menu button after title (EFFECT RACK is at browserWidth + 15, ~140px wide)
    viewMenuButton.setBounds(browserWidth + 170, buttonY, 50, 28);

    // Debug toggle after View button
    debugToggle.setBounds(browserWidth + 230, buttonY, 90, 28);

    // Footer area
    bounds.removeFromBottom(30);

    // Rack rails
    bounds.removeFromLeft(15);
    bounds.removeFromRight(15);

    chainViewport.setBounds(bounds);
    refreshChainDisplay();
}

void UhbikWrapperAudioProcessorEditor::savePreset()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save Effect Chain Preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.uhbikchain"
    );

    auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File{})
            return;

        // Ensure .uhbikchain extension
        if (!file.hasFileExtension(".uhbikchain"))
            file = file.withFileExtension(".uhbikchain");

        // Get state from processor
        juce::MemoryBlock stateData;
        audioProcessor.getStateInformation(stateData);

        // Write to file
        if (file.replaceWithData(stateData.getData(), stateData.getSize()))
        {
            std::cerr << "[UI] Preset saved to: " << file.getFullPathName() << std::endl << std::flush;
        }
        else
        {
            std::cerr << "[UI] Failed to save preset" << std::endl << std::flush;
        }
    });
}

void UhbikWrapperAudioProcessorEditor::loadPreset()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Effect Chain Preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.uhbikchain"
    );

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File{} || !file.existsAsFile())
            return;

        juce::MemoryBlock stateData;
        if (file.loadFileAsData(stateData))
        {
            std::cerr << "[UI] Loading preset from: " << file.getFullPathName() << std::endl << std::flush;
            audioProcessor.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
        }
        else
        {
            std::cerr << "[UI] Failed to load preset file" << std::endl << std::flush;
        }
    });
}

void UhbikWrapperAudioProcessorEditor::setUIScale(float scale)
{
    uiScale = scale;
    audioProcessor.uiScale.store(scale);  // Save to processor for persistence
    setScaleFactor(scale);
}

void UhbikWrapperAudioProcessorEditor::showViewMenu()
{
    juce::PopupMenu menu;

    menu.addSectionHeader("Zoom");
    menu.addItem(1, "100%", true, uiScale == 1.0f);
    menu.addItem(2, "150%", true, uiScale == 1.5f);
    menu.addItem(3, "200%", true, uiScale == 2.0f);
    menu.addItem(4, "300%", true, uiScale == 3.0f);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&viewMenuButton),
        [this](int result)
        {
            switch (result)
            {
                case 1: setUIScale(1.0f); break;
                case 2: setUIScale(1.5f); break;
                case 3: setUIScale(2.0f); break;
                case 4: setUIScale(3.0f); break;
                default: break;
            }
        });
}

void UhbikWrapperAudioProcessorEditor::presetSelected(const juce::File& presetFile)
{
    std::cerr << "[UI] Loading preset: " << presetFile.getFullPathName() << std::endl << std::flush;

    juce::MemoryBlock stateData;
    if (presetFile.loadFileAsData(stateData))
    {
        audioProcessor.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
    }
}

void UhbikWrapperAudioProcessorEditor::savePresetRequested(const juce::File& folder, const juce::String& name)
{
    auto file = folder.getChildFile(name + ".uhbikchain");

    std::cerr << "[UI] Saving preset to: " << file.getFullPathName() << std::endl << std::flush;

    juce::MemoryBlock stateData;
    audioProcessor.getStateInformation(stateData);

    if (file.replaceWithData(stateData.getData(), stateData.getSize()))
    {
        std::cerr << "[UI] Preset saved successfully" << std::endl << std::flush;
        if (presetBrowser != nullptr)
            presetBrowser->refresh();
    }
}

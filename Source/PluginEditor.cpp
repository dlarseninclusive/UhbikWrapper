#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

UhbikWrapperAudioProcessorEditor::UhbikWrapperAudioProcessorEditor (UhbikWrapperAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (500, 500);
    setResizable(true, true);
    setResizeLimits(400, 300, 2000, 2000);

    audioProcessor.addChangeListener(this);

    chainViewport.setViewedComponent(&chainContainer, false);
    chainViewport.setScrollBarsShown(true, false);
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

    populatePluginSelector();
    refreshChainDisplay();

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

void UhbikWrapperAudioProcessorEditor::comboBoxChanged(juce::ComboBox*)
{
}

void UhbikWrapperAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &addButton)
    {
        int selectedIndex = pluginSelector.getSelectedItemIndex();
        if (selectedIndex >= 0)
        {
            auto& pluginList = audioProcessor.getKnownPluginList();
            auto types = pluginList.getTypes();
            if (selectedIndex < static_cast<int>(types.size()))
            {
                audioProcessor.addPlugin(types[static_cast<size_t>(selectedIndex)]);
            }
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

    auto& pluginList = audioProcessor.getKnownPluginList();
    auto types = pluginList.getTypes();

    std::cerr << "[UI] Populating selector with " << types.size() << " plugins" << std::endl << std::flush;

    int id = 1;
    for (const auto& desc : types)
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
    int padding = 8;

    int containerWidth = chainViewport.getWidth() - 20;
    if (containerWidth < 400) containerWidth = 400;

    chainContainer.setSize(
        containerWidth,
        chainSize > 0 ? chainSize * (slotHeight + slotSpacing) + padding * 2 : 100
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
        slotComp->setBounds(padding, padding + i * (slotHeight + slotSpacing),
                            containerWidth - padding * 2, slotHeight);
        chainContainer.addAndMakeVisible(slotComp.get());
        slotComponents.push_back(std::move(slotComp));
    }

    repaint();
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
    // Dark rack background
    g.fillAll (juce::Colour (0xff1a1a1a));

    // Header bar with gradient
    juce::ColourGradient headerGradient(juce::Colour(0xffff8800), 0, 0,
                                         juce::Colour(0xffcc5500), 0, 50, false);
    g.setGradientFill(headerGradient);
    g.fillRect(0, 0, getWidth(), 50);

    // Header text
    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    g.drawFittedText("EFFECT RACK", 15, 0, 250, 50, juce::Justification::centredLeft, 1);

    // Rack rails on left and right
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(0, 50, 15, getHeight() - 80);
    g.fillRect(getWidth() - 15, 50, 15, getHeight() - 80);

    // Rail holes
    g.setColour(juce::Colour(0xff1a1a1a));
    for (int y = 70; y < getHeight() - 50; y += 30)
    {
        g.fillEllipse(4.0f, static_cast<float>(y), 7.0f, 7.0f);
        g.fillEllipse(static_cast<float>(getWidth() - 11), static_cast<float>(y), 7.0f, 7.0f);
    }

    // Footer bar
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(0, getHeight() - 30, getWidth(), 30);

    g.setColour(juce::Colours::lightgrey);
    g.setFont(12.0f);
    g.drawFittedText(statusMessage, 0, getHeight() - 30, getWidth(), 30, juce::Justification::centred, 1);

    // Empty state message
    if (audioProcessor.getChainSize() == 0)
    {
        auto emptyBounds = chainViewport.getBounds();
        g.setColour(juce::Colour(0xff666666));
        g.setFont(16.0f);
        g.drawFittedText("Select a plugin above and click [+] to add to the rack",
                         emptyBounds, juce::Justification::centred, 2);
    }
}

void UhbikWrapperAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header area
    auto headerBounds = bounds.removeFromTop(50);
    auto padding = 15;
    int buttonY = (headerBounds.getHeight() - 28) / 2;

    // Right side: [selector] [+]
    int addBtnWidth = 40;
    int selectorWidth = juce::jmin(220, getWidth() / 3);

    addButton.setBounds(headerBounds.getRight() - addBtnWidth - padding, buttonY, addBtnWidth, 28);
    pluginSelector.setBounds(addButton.getX() - selectorWidth - 6, buttonY, selectorWidth, 28);

    // View menu button after title
    viewMenuButton.setBounds(145, buttonY, 50, 28);

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

    menu.addSeparator();
    menu.addSectionHeader("Presets");
    menu.addItem(10, "Save Preset...");
    menu.addItem(11, "Load Preset...");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&viewMenuButton),
        [this](int result)
        {
            switch (result)
            {
                case 1: setUIScale(1.0f); break;
                case 2: setUIScale(1.5f); break;
                case 3: setUIScale(2.0f); break;
                case 4: setUIScale(3.0f); break;
                case 10: savePreset(); break;
                case 11: loadPreset(); break;
                default: break;
            }
        });
}

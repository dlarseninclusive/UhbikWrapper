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
    presetBrowser = std::make_unique<PresetBrowser>(UhbikWrapperAudioProcessor::getPresetsFolder(), audioProcessor.getKnownPluginList());
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

    populatePluginSelector();
    refreshChainDisplay();

    // Ducker panel setup
    duckerToggleButton.addListener(this);
    duckerToggleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff555555));
    addAndMakeVisible(duckerToggleButton);

    duckerEnableButton.addListener(this);
    duckerEnableButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff44aa44));
    addChildComponent(duckerEnableButton);  // Hidden until expanded

    // Threshold slider (-60 to 0 dB)
    duckerThresholdSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    duckerThresholdSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    duckerThresholdSlider.setRange(-60.0, 0.0, 0.5);
    duckerThresholdSlider.setValue(audioProcessor.duckerThresholdDb.load());
    duckerThresholdSlider.setTextValueSuffix(" dB");
    duckerThresholdSlider.addListener(this);
    addChildComponent(duckerThresholdSlider);
    duckerThresholdLabel.setJustificationType(juce::Justification::centred);
    duckerThresholdLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    duckerThresholdLabel.setFont(juce::Font(12.0f));
    addChildComponent(duckerThresholdLabel);

    // Amount slider (0 to 100%)
    duckerAmountSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    duckerAmountSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    duckerAmountSlider.setRange(0.0, 100.0, 1.0);
    duckerAmountSlider.setValue(audioProcessor.duckerAmount.load());
    duckerAmountSlider.setTextValueSuffix("%");
    duckerAmountSlider.addListener(this);
    addChildComponent(duckerAmountSlider);
    duckerAmountLabel.setJustificationType(juce::Justification::centred);
    duckerAmountLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    duckerAmountLabel.setFont(juce::Font(12.0f));
    addChildComponent(duckerAmountLabel);

    // Attack slider (0.1 to 100 ms)
    duckerAttackSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    duckerAttackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    duckerAttackSlider.setRange(0.1, 100.0, 0.1);
    duckerAttackSlider.setSkewFactorFromMidPoint(10.0);
    duckerAttackSlider.setValue(audioProcessor.duckerAttackMs.load());
    duckerAttackSlider.setTextValueSuffix(" ms");
    duckerAttackSlider.addListener(this);
    addChildComponent(duckerAttackSlider);
    duckerAttackLabel.setJustificationType(juce::Justification::centred);
    duckerAttackLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    duckerAttackLabel.setFont(juce::Font(12.0f));
    addChildComponent(duckerAttackLabel);

    // Release slider (10 to 2000 ms)
    duckerReleaseSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    duckerReleaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    duckerReleaseSlider.setRange(10.0, 2000.0, 1.0);
    duckerReleaseSlider.setSkewFactorFromMidPoint(200.0);
    duckerReleaseSlider.setValue(audioProcessor.duckerReleaseMs.load());
    duckerReleaseSlider.setTextValueSuffix(" ms");
    duckerReleaseSlider.addListener(this);
    addChildComponent(duckerReleaseSlider);
    duckerReleaseLabel.setJustificationType(juce::Justification::centred);
    duckerReleaseLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    duckerReleaseLabel.setFont(juce::Font(12.0f));
    addChildComponent(duckerReleaseLabel);

    // Hold slider (0 to 500 ms)
    duckerHoldSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    duckerHoldSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    duckerHoldSlider.setRange(0.0, 500.0, 1.0);
    duckerHoldSlider.setValue(audioProcessor.duckerHoldMs.load());
    duckerHoldSlider.setTextValueSuffix(" ms");
    duckerHoldSlider.addListener(this);
    addChildComponent(duckerHoldSlider);
    duckerHoldLabel.setJustificationType(juce::Justification::centred);
    duckerHoldLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    duckerHoldLabel.setFont(juce::Font(12.0f));
    addChildComponent(duckerHoldLabel);

    // Apply saved UI scale after a short delay (JUCE needs component to be fully ready)
    uiScale = audioProcessor.uiScale.load();
    if (uiScale != 1.0f)
    {
        juce::Component::SafePointer<UhbikWrapperAudioProcessorEditor> safeThis(this);
        juce::Timer::callAfterDelay(100, [safeThis, scale = uiScale]()
        {
            if (safeThis != nullptr)
                safeThis->setScaleFactor(scale);
        });
    }

    startTimerHz(30);  // 30Hz for smooth level metering
}

UhbikWrapperAudioProcessorEditor::~UhbikWrapperAudioProcessorEditor()
{
    audioProcessor.removeChangeListener(this);
    pluginSelector.removeListener(this);
    addButton.removeListener(this);
    viewMenuButton.removeListener(this);
    duckerToggleButton.removeListener(this);
    duckerEnableButton.removeListener(this);
    duckerThresholdSlider.removeListener(this);
    duckerAmountSlider.removeListener(this);
    duckerAttackSlider.removeListener(this);
    duckerReleaseSlider.removeListener(this);
    duckerHoldSlider.removeListener(this);
    editorWindowCache.clear();
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

    // Update level meters for each slot
    for (size_t i = 0; i < slotComponents.size() && i < static_cast<size_t>(chainSize); ++i)
    {
        auto& slot = audioProcessor.effectChain[i];
        slotComponents[i]->setLevels(
            slot.inputLevelL.load(),
            slot.inputLevelR.load(),
            slot.outputLevelL.load(),
            slot.outputLevelR.load()
        );
    }

    // Repaint footer for master meters and ducker GR meter
    repaint(0, getHeight() - 30, getWidth(), 30);

    // Repaint ducker header area if expanded (for GR meter in header bar)
    if (duckerExpanded)
    {
        int browserWidth = 200;
        int duckerHeaderHeight = 25;
        int duckerExpandedHeight = 100;
        int duckerHeight = duckerHeaderHeight + duckerExpandedHeight;
        int duckerY = getHeight() - 30 - duckerHeight;
        // Repaint the header bar where the GR meter is
        repaint(browserWidth, duckerY, getWidth() - browserWidth, duckerHeaderHeight);
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
    else if (button == &duckerToggleButton)
    {
        duckerExpanded = !duckerExpanded;
        updateDuckerUI();
        resized();
    }
    else if (button == &duckerEnableButton)
    {
        audioProcessor.duckerEnabled.store(duckerEnableButton.getToggleState());
    }
}

void UhbikWrapperAudioProcessorEditor::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &duckerThresholdSlider)
        audioProcessor.duckerThresholdDb.store(static_cast<float>(slider->getValue()));
    else if (slider == &duckerAmountSlider)
        audioProcessor.duckerAmount.store(static_cast<float>(slider->getValue()));
    else if (slider == &duckerAttackSlider)
        audioProcessor.duckerAttackMs.store(static_cast<float>(slider->getValue()));
    else if (slider == &duckerReleaseSlider)
        audioProcessor.duckerReleaseMs.store(static_cast<float>(slider->getValue()));
    else if (slider == &duckerHoldSlider)
        audioProcessor.duckerHoldMs.store(static_cast<float>(slider->getValue()));
}

void UhbikWrapperAudioProcessorEditor::updateDuckerUI()
{
    duckerEnableButton.setVisible(duckerExpanded);
    duckerEnableButton.setToggleState(audioProcessor.duckerEnabled.load(), juce::dontSendNotification);

    duckerThresholdSlider.setVisible(duckerExpanded);
    duckerThresholdLabel.setVisible(duckerExpanded);
    duckerAmountSlider.setVisible(duckerExpanded);
    duckerAmountLabel.setVisible(duckerExpanded);
    duckerAttackSlider.setVisible(duckerExpanded);
    duckerAttackLabel.setVisible(duckerExpanded);
    duckerReleaseSlider.setVisible(duckerExpanded);
    duckerReleaseLabel.setVisible(duckerExpanded);
    duckerHoldSlider.setVisible(duckerExpanded);
    duckerHoldLabel.setVisible(duckerExpanded);

    // Update toggle button text
    duckerToggleButton.setButtonText(duckerExpanded ? "DUCKER v" : "DUCKER >");
}

void UhbikWrapperAudioProcessorEditor::populatePluginSelector()
{
    pluginSelector.clear();
    effectPlugins.clear();

    // Get unified list of all available plugins (VST3 + CLAP)
    const auto& allPlugins = audioProcessor.getAvailablePlugins();

    // Filter for effects only (no instruments)
    for (const auto& desc : allPlugins)
    {
        if (!desc.isInstrument)
        {
            effectPlugins.push_back(desc);
        }
    }

    std::cerr << "[UI] Populating selector with " << effectPlugins.size() << " effects (VST3 + CLAP)" << std::endl << std::flush;

    int id = 1;
    for (const auto& desc : effectPlugins)
    {
        pluginSelector.addItem(desc.name, id++);
    }
}

void UhbikWrapperAudioProcessorEditor::refreshChainDisplay()
{
    slotComponents.clear();

    // Clean up editor windows for plugins that no longer exist
    // Just hide them and remove from cache - let natural destruction happen
    for (auto it = editorWindowCache.begin(); it != editorWindowCache.end(); )
    {
        bool found = false;
        for (int i = 0; i < audioProcessor.getChainSize(); ++i)
        {
            if (audioProcessor.getPluginAt(i) == it->first)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            if (it->second != nullptr)
                it->second->setVisible(false);
            it = editorWindowCache.erase(it);
        }
        else
            ++it;
    }

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
            canMoveDown,
            slot.inputGainDb.load(),
            slot.outputGainDb.load(),
            slot.mixPercent.load()
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

void UhbikWrapperAudioProcessorEditor::effectSlotMixChanged(int slotIndex, float inputGainDb, float outputGainDb, float mixPercent)
{
    audioProcessor.setSlotInputGain(slotIndex, inputGainDb);
    audioProcessor.setSlotOutputGain(slotIndex, outputGainDb);
    audioProcessor.setSlotMix(slotIndex, mixPercent);
}

void UhbikWrapperAudioProcessorEditor::openPluginEditor(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= audioProcessor.getChainSize())
        return;

    auto* plugin = audioProcessor.getPluginAt(slotIndex);
    if (plugin == nullptr || !plugin->hasEditor())
        return;

    // Check if we already have a cached window for this plugin
    auto it = editorWindowCache.find(plugin);
    if (it != editorWindowCache.end() && it->second != nullptr)
    {
        // Reuse existing window (just show it - editor is still there)
        it->second->setVisible(true);
        it->second->toFront(true);
        return;
    }

    // Create the editor only once per plugin instance
    auto* editor = plugin->createEditor();
    if (editor == nullptr)
        return;

    // Create and cache the window - editor lives as long as the window
    auto window = std::make_unique<EditorWindow>(plugin->getName());
    window->setContentOwned(editor, true);
    window->centreWithSize(editor->getWidth(), editor->getHeight());
    window->setVisible(true);

    editorWindowCache[plugin] = std::move(window);
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

    // Ducker panel background (when expanded)
    int duckerHeaderHeight = 25;
    int duckerExpandedHeight = 100;
    int duckerHeight = duckerHeaderHeight + (duckerExpanded ? duckerExpandedHeight : 0);
    int duckerY = getHeight() - 30 - duckerHeight;

    // Ducker header bar
    g.setColour(juce::Colour(0xff333333));
    g.fillRect(browserWidth, duckerY, getWidth() - browserWidth, duckerHeaderHeight);

    if (duckerExpanded)
    {
        // Ducker panel background
        g.setColour(juce::Colour(0xff252525));
        g.fillRect(browserWidth, duckerY + duckerHeaderHeight, getWidth() - browserWidth, duckerExpandedHeight);
    }

    // GR meter in header bar (visible when expanded, to the right of toggle button)
    if (duckerExpanded)
    {
        int grMeterX = browserWidth + 100;  // After the DUCKER toggle button
        int grMeterY = duckerY + 5;         // In the header bar
        int grMeterWidth = 120;
        int grMeterHeight = 14;

        // GR label
        g.setColour(juce::Colours::white);
        g.setFont(10.0f);
        g.drawText("GR", grMeterX - 22, grMeterY, 20, grMeterHeight, juce::Justification::centredRight);

        // GR meter background with border
        g.setColour(juce::Colour(0xff0a0a0a));
        g.fillRect(grMeterX, grMeterY, grMeterWidth, grMeterHeight);
        g.setColour(juce::Colour(0xff444444));
        g.drawRect(grMeterX, grMeterY, grMeterWidth, grMeterHeight);

        // GR meter bar (orange, fills from left to right based on reduction)
        float gr = audioProcessor.duckerGainReduction.load();
        int grWidth = static_cast<int>(gr * (grMeterWidth - 2));
        if (grWidth > 0)
        {
            // Color based on amount of reduction
            if (gr > 0.7f)
                g.setColour(juce::Colour(0xffff3333));
            else if (gr > 0.4f)
                g.setColour(juce::Colour(0xffff6600));
            else
                g.setColour(juce::Colour(0xffffaa00));
            g.fillRect(grMeterX + 1, grMeterY + 1, grWidth, grMeterHeight - 2);
        }

        // GR value text to the right of meter
        float grDb = (gr > 0.001f) ? juce::Decibels::gainToDecibels(1.0f - gr) : 0.0f;
        g.setColour(juce::Colours::lightgrey);
        g.setFont(10.0f);
        g.drawText(juce::String(grDb, 1) + " dB", grMeterX + grMeterWidth + 5, grMeterY, 50, grMeterHeight, juce::Justification::centredLeft);
    }

    // Footer bar (after preset browser)
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(browserWidth, getHeight() - 30, getWidth() - browserWidth, 30);

    // Master meters in footer
    int meterWidth = 60;
    int meterHeight = 8;
    int meterY = getHeight() - 20;

    // Input meter label and bar
    g.setColour(juce::Colours::grey);
    g.setFont(10.0f);
    g.drawText("IN", browserWidth + 20, meterY - 2, 20, 12, juce::Justification::centredRight);

    // Input meter background
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(browserWidth + 45, meterY, meterWidth, meterHeight);

    // Input meter levels
    float inL = audioProcessor.masterInputLevelL.load();
    float inR = audioProcessor.masterInputLevelR.load();
    int inLevelWidth = static_cast<int>(juce::jmin(1.0f, (inL + inR) * 0.5f) * meterWidth);
    if (inLevelWidth > 0)
    {
        float avgIn = (inL + inR) * 0.5f;
        if (avgIn > 0.9f)
            g.setColour(juce::Colour(0xffff3333));
        else if (avgIn > 0.7f)
            g.setColour(juce::Colour(0xffffaa00));
        else
            g.setColour(juce::Colour(0xff44cc44));
        g.fillRect(browserWidth + 45, meterY, inLevelWidth, meterHeight);
    }

    // Output meter label and bar
    g.setColour(juce::Colours::grey);
    g.drawText("OUT", browserWidth + 115, meterY - 2, 25, 12, juce::Justification::centredRight);

    // Output meter background
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(browserWidth + 145, meterY, meterWidth, meterHeight);

    // Output meter levels
    float outL = audioProcessor.masterOutputLevelL.load();
    float outR = audioProcessor.masterOutputLevelR.load();
    int outLevelWidth = static_cast<int>(juce::jmin(1.0f, (outL + outR) * 0.5f) * meterWidth);
    if (outLevelWidth > 0)
    {
        float avgOut = (outL + outR) * 0.5f;
        if (avgOut > 0.9f)
            g.setColour(juce::Colour(0xffff3333));
        else if (avgOut > 0.7f)
            g.setColour(juce::Colour(0xffffaa00));
        else
            g.setColour(juce::Colour(0xff44cc44));
        g.fillRect(browserWidth + 145, meterY, outLevelWidth, meterHeight);
    }

    // Status message (shifted right to make room for meters)
    g.setColour(juce::Colours::lightgrey);
    g.setFont(12.0f);
    g.drawFittedText(statusMessage, browserWidth + 220, getHeight() - 30, getWidth() - browserWidth - 240, 30, juce::Justification::centred, 1);

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

    // Footer area
    bounds.removeFromBottom(30);

    // Ducker panel (collapsible, above footer)
    int duckerHeaderHeight = 25;
    int duckerExpandedHeight = 100;
    int duckerHeight = duckerHeaderHeight + (duckerExpanded ? duckerExpandedHeight : 0);

    auto duckerBounds = bounds.removeFromBottom(duckerHeight);

    // Ducker toggle button (always visible)
    duckerToggleButton.setBounds(duckerBounds.getX() + 10, duckerBounds.getY(), 80, duckerHeaderHeight);

    if (duckerExpanded)
    {
        int controlY = duckerBounds.getY() + duckerHeaderHeight + 2;
        int knobSize = 50;
        int labelHeight = 16;
        int spacing = 75;
        int startX = duckerBounds.getX() + 110;

        // Enable button
        duckerEnableButton.setBounds(startX - 60, controlY + 18, 50, 25);

        // Threshold - label above knob
        duckerThresholdLabel.setBounds(startX, controlY, knobSize, labelHeight);
        duckerThresholdSlider.setBounds(startX, controlY + labelHeight, knobSize, knobSize);

        // Amount
        duckerAmountLabel.setBounds(startX + spacing, controlY, knobSize, labelHeight);
        duckerAmountSlider.setBounds(startX + spacing, controlY + labelHeight, knobSize, knobSize);

        // Attack
        duckerAttackLabel.setBounds(startX + spacing * 2, controlY, knobSize, labelHeight);
        duckerAttackSlider.setBounds(startX + spacing * 2, controlY + labelHeight, knobSize, knobSize);

        // Release
        duckerReleaseLabel.setBounds(startX + spacing * 3, controlY, knobSize, labelHeight);
        duckerReleaseSlider.setBounds(startX + spacing * 3, controlY + labelHeight, knobSize, knobSize);

        // Hold
        duckerHoldLabel.setBounds(startX + spacing * 4, controlY, knobSize, labelHeight);
        duckerHoldSlider.setBounds(startX + spacing * 4, controlY + labelHeight, knobSize, knobSize);
    }

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

    menu.addSeparator();
    menu.addSectionHeader("Debug");
    menu.addItem(10, "Debug Logging", true, audioProcessor.debugLogging.load());

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&viewMenuButton),
        [this](int result)
        {
            switch (result)
            {
                case 1: setUIScale(1.0f); break;
                case 2: setUIScale(1.5f); break;
                case 3: setUIScale(2.0f); break;
                case 4: setUIScale(3.0f); break;
                case 10: audioProcessor.debugLogging.store(!audioProcessor.debugLogging.load()); break;
                default: break;
            }
        });
}

void UhbikWrapperAudioProcessorEditor::presetSelected(const juce::File& presetFile)
{
    std::cerr << "[UI] Loading preset: " << presetFile.getFullPathName() << std::endl << std::flush;

    // Try to load as new XML format first
    auto xmlDoc = juce::XmlDocument::parse(presetFile);
    if (xmlDoc != nullptr && xmlDoc->hasTagName("UhbikChainPreset"))
    {
        // New XML format with metadata
        juce::String stateBase64 = xmlDoc->getStringAttribute("stateData");
        juce::MemoryBlock stateData;
        stateData.fromBase64Encoding(stateBase64);
        audioProcessor.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
        std::cerr << "[UI] Loaded XML preset format" << std::endl << std::flush;
    }
    else
    {
        // Legacy binary format
        juce::MemoryBlock stateData;
        if (presetFile.loadFileAsData(stateData))
        {
            audioProcessor.setStateInformation(stateData.getData(), static_cast<int>(stateData.getSize()));
            std::cerr << "[UI] Loaded legacy binary preset format" << std::endl << std::flush;
        }
    }
}

void UhbikWrapperAudioProcessorEditor::savePresetRequested(const juce::File& folder, const juce::String& name,
                                                            const juce::String& author, const juce::String& tags,
                                                            const juce::String& notes)
{
    auto file = folder.getChildFile(name + ".uhbikchain");

    std::cerr << "[UI] Saving preset to: " << file.getFullPathName() << std::endl << std::flush;

    // Create XML preset with metadata
    juce::XmlElement preset("UhbikChainPreset");
    preset.setAttribute("version", 1);
    preset.setAttribute("name", name);
    preset.setAttribute("author", author);
    preset.setAttribute("tags", tags);
    preset.setAttribute("notes", notes);

    // Build plugin list
    juce::StringArray pluginNames;
    for (int i = 0; i < audioProcessor.getChainSize(); ++i)
    {
        auto* plugin = audioProcessor.getPluginAt(i);
        if (plugin != nullptr)
            pluginNames.add(audioProcessor.effectChain[static_cast<size_t>(i)].description.name);
    }
    preset.setAttribute("plugins", pluginNames.joinIntoString(", "));
    preset.setAttribute("pluginCount", audioProcessor.getChainSize());

    // Get state data and encode as base64
    juce::MemoryBlock stateData;
    audioProcessor.getStateInformation(stateData);
    preset.setAttribute("stateData", stateData.toBase64Encoding());

    // Save as XML
    if (preset.writeTo(file))
    {
        std::cerr << "[UI] Preset saved successfully (XML format)" << std::endl << std::flush;
        if (presetBrowser != nullptr)
            presetBrowser->refresh();
    }
}

void UhbikWrapperAudioProcessorEditor::initPresetRequested()
{
    std::cerr << "[UI] Init/clear chain requested" << std::endl << std::flush;

    // Hide editor windows first (don't destroy - let refreshChainDisplay handle cleanup)
    for (auto& entry : editorWindowCache)
    {
        if (entry.second != nullptr)
            entry.second->setVisible(false);
    }

    // Clear the effect chain asynchronously to avoid threading issues
    // (same pattern as removePlugin)
    juce::MessageManager::callAsync([this]() {
        audioProcessor.clearChain();
    });
}

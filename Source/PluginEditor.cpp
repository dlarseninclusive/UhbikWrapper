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

    // Format filter (used internally, not shown as separate control)
    formatFilter.addItem("All", 1);
    formatFilter.addItem("CLAP", 2);
    formatFilter.addItem("VST3", 3);
    formatFilter.setSelectedId(1);
    // Not adding listener or making visible - controlled via View menu

    // Type filter (Effects/Instruments/All)
    typeFilter.addItem("All", 1);
    typeFilter.addItem("Effects", 2);
    typeFilter.addItem("Instruments", 3);
    typeFilter.setSelectedId(1);
    typeFilter.addListener(this);
    addAndMakeVisible(typeFilter);

    addButton.addListener(this);
    addButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff44aa44));
    addAndMakeVisible(addButton);

    viewMenuButton.addListener(this);
    viewMenuButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff555555));
    addAndMakeVisible(viewMenuButton);

    populatePluginSelector();
    // Don't call refreshChainDisplay() here — resized() will call it
    // with proper viewport dimensions after the component is laid out.

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

    // === Modulation Panel Setup ===
    modPanelToggleButton.addListener(this);
    modPanelToggleButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4466aa));
    addAndMakeVisible(modPanelToggleButton);

    // Tab buttons
    modTabLFOsButton.addListener(this);
    modTabLFOsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff5577bb));
    addChildComponent(modTabLFOsButton);

    modTabEnvsButton.addListener(this);
    modTabEnvsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff446699));
    addChildComponent(modTabEnvsButton);

    modTabSeqsButton.addListener(this);
    modTabSeqsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff446699));
    addChildComponent(modTabSeqsButton);

    modTabMatrixButton.addListener(this);
    modTabMatrixButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff446699));
    addChildComponent(modTabMatrixButton);

    // LFO controls setup
    const char* lfoNames[] = {"LFO 1", "LFO 2", "LFO 3", "LFO 4"};
    for (int i = 0; i < 4; ++i)
    {
        auto& lfo = lfoControls[static_cast<size_t>(i)];

        // Name label
        lfo.nameLabel.setText(lfoNames[i], juce::dontSendNotification);
        lfo.nameLabel.setJustificationType(juce::Justification::centred);
        lfo.nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addChildComponent(lfo.nameLabel);

        // Rate slider (0.01 to 20 Hz)
        lfo.rateSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        lfo.rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 12);
        lfo.rateSlider.setRange(0.01, 20.0, 0.01);
        lfo.rateSlider.setSkewFactorFromMidPoint(1.0);
        lfo.rateSlider.setValue(1.0);
        lfo.rateSlider.setTextValueSuffix(" Hz");
        lfo.rateSlider.addListener(this);
        addChildComponent(lfo.rateSlider);
        lfo.rateLabel.setJustificationType(juce::Justification::centred);
        lfo.rateLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addChildComponent(lfo.rateLabel);

        // Depth slider (0 to 100%)
        lfo.depthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        lfo.depthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 12);
        lfo.depthSlider.setRange(0.0, 100.0, 1.0);
        lfo.depthSlider.setValue(100.0);
        lfo.depthSlider.setTextValueSuffix("%");
        lfo.depthSlider.addListener(this);
        addChildComponent(lfo.depthSlider);
        lfo.depthLabel.setJustificationType(juce::Justification::centred);
        lfo.depthLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addChildComponent(lfo.depthLabel);

        // Waveform selector
        lfo.waveformBox.addItem("Sine", 1);
        lfo.waveformBox.addItem("Triangle", 2);
        lfo.waveformBox.addItem("Saw", 3);
        lfo.waveformBox.addItem("Square", 4);
        lfo.waveformBox.addItem("S&H", 5);
        lfo.waveformBox.setSelectedId(1);
        lfo.waveformBox.addListener(this);
        addChildComponent(lfo.waveformBox);
    }

    // Envelope controls setup
    const char* envNames[] = {"Env 1", "Env 2"};
    for (int i = 0; i < 2; ++i)
    {
        auto& env = envControls[static_cast<size_t>(i)];

        env.nameLabel.setText(envNames[i], juce::dontSendNotification);
        env.nameLabel.setJustificationType(juce::Justification::centred);
        env.nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addChildComponent(env.nameLabel);

        // Attack slider (0.1 to 2000 ms)
        env.attackSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        env.attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 12);
        env.attackSlider.setRange(0.1, 2000.0, 0.1);
        env.attackSlider.setSkewFactorFromMidPoint(100.0);
        env.attackSlider.setValue(10.0);
        env.attackSlider.setTextValueSuffix("ms");
        env.attackSlider.addListener(this);
        addChildComponent(env.attackSlider);

        // Decay slider
        env.decaySlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        env.decaySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 12);
        env.decaySlider.setRange(0.1, 2000.0, 0.1);
        env.decaySlider.setSkewFactorFromMidPoint(100.0);
        env.decaySlider.setValue(100.0);
        env.decaySlider.setTextValueSuffix("ms");
        env.decaySlider.addListener(this);
        addChildComponent(env.decaySlider);

        // Sustain slider (0-100%)
        env.sustainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        env.sustainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 12);
        env.sustainSlider.setRange(0.0, 100.0, 1.0);
        env.sustainSlider.setValue(70.0);
        env.sustainSlider.setTextValueSuffix("%");
        env.sustainSlider.addListener(this);
        addChildComponent(env.sustainSlider);

        // Release slider
        env.releaseSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        env.releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 12);
        env.releaseSlider.setRange(0.1, 5000.0, 0.1);
        env.releaseSlider.setSkewFactorFromMidPoint(200.0);
        env.releaseSlider.setValue(200.0);
        env.releaseSlider.setTextValueSuffix("ms");
        env.releaseSlider.addListener(this);
        addChildComponent(env.releaseSlider);

        // Depth slider
        env.depthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        env.depthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 12);
        env.depthSlider.setRange(0.0, 100.0, 1.0);
        env.depthSlider.setValue(100.0);
        env.depthSlider.setTextValueSuffix("%");
        env.depthSlider.addListener(this);
        addChildComponent(env.depthSlider);

        // Trigger button
        env.triggerButton.addListener(this);
        env.triggerButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff44aa44));
        addChildComponent(env.triggerButton);
    }

    // Step Sequencer controls setup
    const char* seqNames[] = {"Seq 1", "Seq 2"};
    for (int i = 0; i < 2; ++i)
    {
        auto& seq = seqControls[static_cast<size_t>(i)];

        seq.nameLabel.setText(seqNames[i], juce::dontSendNotification);
        seq.nameLabel.setJustificationType(juce::Justification::centred);
        seq.nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addChildComponent(seq.nameLabel);

        // Step sliders (16 steps)
        for (int s = 0; s < 16; ++s)
        {
            auto& stepSlider = seq.stepSliders[static_cast<size_t>(s)];
            stepSlider.setSliderStyle(juce::Slider::LinearBarVertical);
            stepSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
            stepSlider.setRange(0.0, 1.0, 0.01);
            stepSlider.setValue(0.5);
            stepSlider.addListener(this);
            addChildComponent(stepSlider);
        }

        // Division selector
        seq.divisionBox.addItem("1/1", 1);
        seq.divisionBox.addItem("1/2", 2);
        seq.divisionBox.addItem("1/4", 4);
        seq.divisionBox.addItem("1/8", 8);
        seq.divisionBox.addItem("1/16", 16);
        seq.divisionBox.addItem("1/32", 32);
        seq.divisionBox.setSelectedId(16);
        seq.divisionBox.addListener(this);
        addChildComponent(seq.divisionBox);

        // Glide slider
        seq.glideSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        seq.glideSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 12);
        seq.glideSlider.setRange(0.0, 100.0, 1.0);
        seq.glideSlider.setValue(0.0);
        seq.glideSlider.setTextValueSuffix("%");
        seq.glideSlider.addListener(this);
        addChildComponent(seq.glideSlider);

        // Depth slider
        seq.depthSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        seq.depthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 12);
        seq.depthSlider.setRange(0.0, 100.0, 1.0);
        seq.depthSlider.setValue(100.0);
        seq.depthSlider.setTextValueSuffix("%");
        seq.depthSlider.addListener(this);
        addChildComponent(seq.depthSlider);

        // Pattern presets
        seq.patternBox.addItem("Ramp Up", 1);
        seq.patternBox.addItem("Ramp Down", 2);
        seq.patternBox.addItem("Triangle", 3);
        seq.patternBox.addItem("Square", 4);
        seq.patternBox.addItem("Random", 5);
        seq.patternBox.addItem("Clear", 6);
        seq.patternBox.setTextWhenNothingSelected("Pattern...");
        seq.patternBox.addListener(this);
        addChildComponent(seq.patternBox);
    }

    // Matrix controls - now with all source types
    matrixSourceBox.addItem("LFO 1", 1);
    matrixSourceBox.addItem("LFO 2", 2);
    matrixSourceBox.addItem("LFO 3", 3);
    matrixSourceBox.addItem("LFO 4", 4);
    matrixSourceBox.addItem("Env 1", 5);
    matrixSourceBox.addItem("Env 2", 6);
    matrixSourceBox.addItem("Seq 1", 7);
    matrixSourceBox.addItem("Seq 2", 8);
    matrixSourceBox.addItem("Macro 1", 9);
    matrixSourceBox.addItem("Macro 2", 10);
    matrixSourceBox.addItem("Macro 3", 11);
    matrixSourceBox.addItem("Macro 4", 12);
    matrixSourceBox.addItem("Macro 5", 13);
    matrixSourceBox.addItem("Macro 6", 14);
    matrixSourceBox.addItem("Macro 7", 15);
    matrixSourceBox.addItem("Macro 8", 16);
    matrixSourceBox.setSelectedId(1);
    addChildComponent(matrixSourceBox);

    matrixSlotBox.setTextWhenNothingSelected("Select Slot...");
    matrixSlotBox.addListener(this);
    addChildComponent(matrixSlotBox);

    matrixParamBox.setTextWhenNothingSelected("Select Parameter...");
    addChildComponent(matrixParamBox);

    matrixAmountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    matrixAmountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    matrixAmountSlider.setRange(-100.0, 100.0, 1.0);
    matrixAmountSlider.setValue(50.0);
    matrixAmountSlider.setTextValueSuffix("%");
    addChildComponent(matrixAmountSlider);

    matrixAddButton.addListener(this);
    matrixAddButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff44aa44));
    addChildComponent(matrixAddButton);

    matrixClearButton.addListener(this);
    matrixClearButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffaa4444));
    addChildComponent(matrixClearButton);

    matrixRoutesLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addChildComponent(matrixRoutesLabel);

    modRouteListModel = std::make_unique<ModRouteListModel>(*this);
    matrixRoutesList.setModel(modRouteListModel.get());
    matrixRoutesList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1a1a1a));
    matrixRoutesList.setRowHeight(20);
    addChildComponent(matrixRoutesList);

    // === Multiband UI Setup ===
    // Band tab buttons (always visible) — MAIN/LOW/MID/HIGH
    bandMainButton.addListener(this);
    bandMainButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff888888));
    addAndMakeVisible(bandMainButton);

    juce::Colour bandColours[] = {
        juce::Colour(0xff4488cc),  // Blue for low
        juce::Colour(0xff44aa44),  // Green for mid
        juce::Colour(0xffcc6644)   // Orange for high
    };
    juce::TextButton* bandBtns[] = { &bandLowButton, &bandMidButton, &bandHighButton };

    for (int b = 0; b < 3; ++b)
    {
        bandBtns[b]->addListener(this);
        bandBtns[b]->setColour(juce::TextButton::buttonColourId, bandColours[b]);
        addAndMakeVisible(*bandBtns[b]);

        // Solo buttons — yellow when active
        bandSoloButtons[b].setButtonText("S");
        bandSoloButtons[b].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
        bandSoloButtons[b].setColour(juce::TextButton::textColourOffId, juce::Colour(0xffaaaaaa));
        bandSoloButtons[b].addListener(this);
        addChildComponent(bandSoloButtons[b]);

        // Mute buttons — red when active
        bandMuteButtons[b].setButtonText("M");
        bandMuteButtons[b].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff333333));
        bandMuteButtons[b].setColour(juce::TextButton::textColourOffId, juce::Colour(0xffaaaaaa));
        bandMuteButtons[b].addListener(this);
        addChildComponent(bandMuteButtons[b]);

        // Per-band gain sliders
        bandGainSliders[b].setSliderStyle(juce::Slider::LinearHorizontal);
        bandGainSliders[b].setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        bandGainSliders[b].setRange(-24.0, 24.0, 0.1);
        bandGainSliders[b].setValue(0.0);
        bandGainSliders[b].setTextValueSuffix(" dB");
        bandGainSliders[b].addListener(this);
        addChildComponent(bandGainSliders[b]);
    }

    // Band chain label (shows which band's chain is displayed)
    bandChainLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    bandChainLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bandChainLabel);

    // Crossover frequency sliders
    lowMidCrossoverSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    lowMidCrossoverSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    lowMidCrossoverSlider.setRange(20.0, 2000.0, 1.0);
    lowMidCrossoverSlider.setSkewFactorFromMidPoint(200.0);
    lowMidCrossoverSlider.setValue(audioProcessor.multibandProcessor.getLowMidCrossover());
    lowMidCrossoverSlider.setTextValueSuffix(" Hz");
    lowMidCrossoverSlider.addListener(this);
    addChildComponent(lowMidCrossoverSlider);
    lowMidCrossoverLabel.setJustificationType(juce::Justification::centredRight);
    lowMidCrossoverLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    lowMidCrossoverLabel.setFont(juce::Font(11.0f));
    addChildComponent(lowMidCrossoverLabel);

    midHighCrossoverSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    midHighCrossoverSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    midHighCrossoverSlider.setRange(200.0, 20000.0, 1.0);
    midHighCrossoverSlider.setSkewFactorFromMidPoint(2000.0);
    midHighCrossoverSlider.setValue(audioProcessor.multibandProcessor.getMidHighCrossover());
    midHighCrossoverSlider.setTextValueSuffix(" Hz");
    midHighCrossoverSlider.addListener(this);
    addChildComponent(midHighCrossoverSlider);
    midHighCrossoverLabel.setJustificationType(juce::Justification::centredRight);
    midHighCrossoverLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    midHighCrossoverLabel.setFont(juce::Font(11.0f));
    addChildComponent(midHighCrossoverLabel);

    // Sync activeBandIndex with processor's multibandEnabled state (from saved preset)
    if (audioProcessor.multibandEnabled.load())
        activeBandIndex = 0;  // Default to LOW band tab
    else
        activeBandIndex = -1; // MAIN tab

    updateMultibandUI();

    // Reset UI scale to 100% on startup to prevent oversized windows.
    // User can adjust via View menu. Reset the stored value too.
    uiScale = 1.0f;
    audioProcessor.uiScale.store(1.0f);

    // Refresh chain display now that all components are set up.
    // The resized() triggered by setSize() at the top runs before the viewport
    // is configured, so in DAW hosts (which may not trigger additional resize events)
    // this explicit call ensures the chain is populated correctly.
    refreshChainDisplay();

    startTimerHz(30);  // 30Hz for smooth level metering
}

UhbikWrapperAudioProcessorEditor::~UhbikWrapperAudioProcessorEditor()
{
    audioProcessor.removeChangeListener(this);
    pluginSelector.removeListener(this);
    typeFilter.removeListener(this);
    addButton.removeListener(this);
    viewMenuButton.removeListener(this);
    duckerToggleButton.removeListener(this);
    duckerEnableButton.removeListener(this);
    duckerThresholdSlider.removeListener(this);
    duckerAmountSlider.removeListener(this);
    duckerAttackSlider.removeListener(this);
    duckerReleaseSlider.removeListener(this);
    duckerHoldSlider.removeListener(this);

    // Modulation panel cleanup
    modPanelToggleButton.removeListener(this);
    modTabLFOsButton.removeListener(this);
    modTabEnvsButton.removeListener(this);
    modTabSeqsButton.removeListener(this);
    modTabMatrixButton.removeListener(this);
    matrixAddButton.removeListener(this);
    matrixClearButton.removeListener(this);
    matrixSlotBox.removeListener(this);
    for (auto& lfo : lfoControls)
    {
        lfo.rateSlider.removeListener(this);
        lfo.depthSlider.removeListener(this);
        lfo.waveformBox.removeListener(this);
    }
    for (auto& env : envControls)
    {
        env.attackSlider.removeListener(this);
        env.decaySlider.removeListener(this);
        env.sustainSlider.removeListener(this);
        env.releaseSlider.removeListener(this);
        env.depthSlider.removeListener(this);
        env.triggerButton.removeListener(this);
    }
    for (auto& seq : seqControls)
    {
        for (auto& step : seq.stepSliders)
            step.removeListener(this);
        seq.divisionBox.removeListener(this);
        seq.glideSlider.removeListener(this);
        seq.depthSlider.removeListener(this);
        seq.patternBox.removeListener(this);
    }

    // Multiband cleanup
    bandMainButton.removeListener(this);
    bandLowButton.removeListener(this);
    bandMidButton.removeListener(this);
    bandHighButton.removeListener(this);
    lowMidCrossoverSlider.removeListener(this);
    midHighCrossoverSlider.removeListener(this);
    for (int b = 0; b < 3; ++b)
    {
        bandSoloButtons[b].removeListener(this);
        bandMuteButtons[b].removeListener(this);
        bandGainSliders[b].removeListener(this);
    }

    editorWindowCache.clear();
    audioProcessor.closeAllCLAPEditors();
}

void UhbikWrapperAudioProcessorEditor::timerCallback()
{
    auto& activeChain = getActiveChain();
    int activeSize = static_cast<int>(activeChain.size());

    juce::String newStatus;
    if (activeBandIndex >= 0)
    {
        const char* bandLabels[] = {"LOW", "MID", "HIGH"};
        newStatus = juce::String(bandLabels[activeBandIndex]) + " band: "
                  + juce::String(activeSize) + " effect(s)";
    }
    else
    {
        newStatus = juce::String(activeSize) + " effect(s) in chain";
    }

    if (statusMessage != newStatus)
    {
        statusMessage = newStatus;
        repaint();
    }

    // Update level meters for each slot
    for (size_t i = 0; i < slotComponents.size() && i < static_cast<size_t>(activeSize); ++i)
    {
        auto& slot = activeChain[i];
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
    populatePluginSelector();  // Update dropdown (e.g., after deferred CLAP scan)
    resized();  // Full layout refresh ensures viewport is properly sized before chain display
}

void UhbikWrapperAudioProcessorEditor::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &pluginSelector)
    {
        int selectedIndex = pluginSelector.getSelectedItemIndex();
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(effectPlugins.size()))
        {
            auto& desc = effectPlugins[static_cast<size_t>(selectedIndex)];
            if (activeBandIndex >= 0)
            {
                std::cerr << "[UI] Adding plugin to band " << activeBandIndex << ": " << desc.name << std::endl << std::flush;
                audioProcessor.addPluginToBand(activeBandIndex, desc);
            }
            else
            {
                std::cerr << "[UI] Adding plugin to main chain: " << desc.name << std::endl << std::flush;
                audioProcessor.addPlugin(desc);
            }
            pluginSelector.setSelectedItemIndex(-1, juce::dontSendNotification); // Reset selection
        }
    }
    else if (comboBox == &typeFilter)
    {
        populatePluginSelector();
    }
    else if (comboBox == &matrixSlotBox)
    {
        populateMatrixParamBox();
    }

    // LFO waveform combo boxes
    for (int i = 0; i < 4; ++i)
    {
        auto& lfo = lfoControls[static_cast<size_t>(i)];
        if (comboBox == &lfo.waveformBox)
        {
            int waveId = lfo.waveformBox.getSelectedId();
            if (waveId >= 1 && waveId <= 5)
            {
                audioProcessor.setLFOWaveform(i, static_cast<LFOWaveform>(waveId - 1));
            }
            return;
        }
    }

    // Step Sequencer combo boxes
    for (int i = 0; i < 2; ++i)
    {
        auto& seq = seqControls[static_cast<size_t>(i)];
        if (comboBox == &seq.divisionBox)
        {
            int div = seq.divisionBox.getSelectedId();
            audioProcessor.setStepSeqDivision(i, div);
            return;
        }
        else if (comboBox == &seq.patternBox)
        {
            int patternId = seq.patternBox.getSelectedId();
            if (patternId >= 1)
            {
                // Apply the pattern preset
                auto* seqPtr = audioProcessor.getStepSequencer(i);
                if (seqPtr != nullptr)
                {
                    seqPtr->setPattern(patternId - 1);
                    // Update step sliders to reflect new pattern
                    for (int s = 0; s < 16; ++s)
                    {
                        seq.stepSliders[static_cast<size_t>(s)].setValue(
                            seqPtr->getStep(s), juce::dontSendNotification);
                    }
                }
            }
            seq.patternBox.setSelectedId(0, juce::dontSendNotification);  // Reset selection
            return;
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
            if (activeBandIndex >= 0)
                audioProcessor.addPluginToBand(activeBandIndex, effectPlugins[static_cast<size_t>(selectedIndex)]);
            else
                audioProcessor.addPlugin(effectPlugins[static_cast<size_t>(selectedIndex)]);
            pluginSelector.setSelectedItemIndex(-1, juce::dontSendNotification);
        }
    }
    else if (button == &bandMainButton)
    {
        activeBandIndex = -1;
        audioProcessor.multibandEnabled.store(false);
        updateBandTabButtons();
        resized();
    }
    else if (button == &bandLowButton)
    {
        activeBandIndex = 0;
        audioProcessor.multibandEnabled.store(true);
        updateBandTabButtons();
        resized();
    }
    else if (button == &bandMidButton)
    {
        activeBandIndex = 1;
        audioProcessor.multibandEnabled.store(true);
        updateBandTabButtons();
        resized();
    }
    else if (button == &bandHighButton)
    {
        activeBandIndex = 2;
        audioProcessor.multibandEnabled.store(true);
        updateBandTabButtons();
        resized();
    }
    else if (button == &bandSoloButtons[0] || button == &bandSoloButtons[1] || button == &bandSoloButtons[2])
    {
        for (int b = 0; b < 3; ++b)
        {
            if (button == &bandSoloButtons[b])
            {
                bool newState = !audioProcessor.multibandProcessor.bandStates[b].solo.load();
                audioProcessor.multibandProcessor.bandStates[b].solo.store(newState);
                bandSoloButtons[b].setColour(juce::TextButton::buttonColourId,
                    newState ? juce::Colour(0xffcccc44) : juce::Colour(0xff333333));
                break;
            }
        }
    }
    else if (button == &bandMuteButtons[0] || button == &bandMuteButtons[1] || button == &bandMuteButtons[2])
    {
        for (int b = 0; b < 3; ++b)
        {
            if (button == &bandMuteButtons[b])
            {
                bool newState = !audioProcessor.multibandProcessor.bandStates[b].mute.load();
                audioProcessor.multibandProcessor.bandStates[b].mute.store(newState);
                bandMuteButtons[b].setColour(juce::TextButton::buttonColourId,
                    newState ? juce::Colour(0xffcc4444) : juce::Colour(0xff333333));
                break;
            }
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
    // Modulation panel buttons
    else if (button == &modPanelToggleButton)
    {
        modPanelExpanded = !modPanelExpanded;
        updateModulationUI();
        resized();
    }
    else if (button == &modTabLFOsButton)
    {
        currentModTab = ModTab::LFOs;
        updateModTabButtons();
        updateModulationUI();
        resized();
    }
    else if (button == &modTabEnvsButton)
    {
        currentModTab = ModTab::Envs;
        updateModTabButtons();
        updateModulationUI();
        resized();
    }
    else if (button == &modTabSeqsButton)
    {
        currentModTab = ModTab::StepSeqs;
        updateModTabButtons();
        updateModulationUI();
        resized();
    }
    else if (button == &modTabMatrixButton)
    {
        currentModTab = ModTab::Matrix;
        updateModTabButtons();
        populateMatrixSlotBox();
        updateModulationUI();
        resized();
    }
    // Envelope trigger buttons
    else if (button == &envControls[0].triggerButton)
    {
        audioProcessor.triggerEnvelope(0);
    }
    else if (button == &envControls[1].triggerButton)
    {
        audioProcessor.triggerEnvelope(1);
    }
    else if (button == &matrixAddButton)
    {
        int sourceId = matrixSourceBox.getSelectedId();
        int slotIndex = matrixSlotBox.getSelectedId() - 1;
        int paramIndex = matrixParamBox.getSelectedId() - 1;
        float amount = static_cast<float>(matrixAmountSlider.getValue()) / 100.0f;

        // Convert source selector ID to type and index
        ModSourceType sourceType;
        int sourceIndex;
        if (sourceId >= 1 && sourceId <= 4)
        {
            sourceType = ModSourceType::LFO;
            sourceIndex = sourceId - 1;
        }
        else if (sourceId >= 5 && sourceId <= 6)
        {
            sourceType = ModSourceType::Envelope;
            sourceIndex = sourceId - 5;
        }
        else if (sourceId >= 7 && sourceId <= 8)
        {
            sourceType = ModSourceType::StepSequencer;
            sourceIndex = sourceId - 7;
        }
        else if (sourceId >= 9 && sourceId <= 16)
        {
            sourceType = ModSourceType::Macro;
            sourceIndex = sourceId - 9;
        }
        else
        {
            return;
        }

        if (slotIndex >= 0 && paramIndex >= 0)
        {
            auto params = audioProcessor.getModulatableParametersForSlot(slotIndex);
            if (paramIndex < static_cast<int>(params.size()))
            {
                audioProcessor.addModulationRoute(sourceType, sourceIndex, slotIndex, params[static_cast<size_t>(paramIndex)].id, amount);
                refreshModRoutesList();
            }
        }
    }
    else if (button == &matrixClearButton)
    {
        audioProcessor.clearModulationRoutes();
        refreshModRoutesList();
    }
}

void UhbikWrapperAudioProcessorEditor::sliderValueChanged(juce::Slider* slider)
{
    // Multiband crossover sliders
    if (slider == &lowMidCrossoverSlider)
    {
        audioProcessor.setLowMidCrossover(static_cast<float>(slider->getValue()));
        return;
    }
    else if (slider == &midHighCrossoverSlider)
    {
        audioProcessor.setMidHighCrossover(static_cast<float>(slider->getValue()));
        return;
    }

    // Band gain sliders
    for (int b = 0; b < 3; ++b)
    {
        if (slider == &bandGainSliders[b])
        {
            audioProcessor.multibandProcessor.bandStates[b].gainDb.store(static_cast<float>(slider->getValue()));
            return;
        }
    }

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

    // LFO sliders
    for (int i = 0; i < 4; ++i)
    {
        auto& lfo = lfoControls[static_cast<size_t>(i)];
        if (slider == &lfo.rateSlider)
        {
            audioProcessor.setLFOFrequency(i, static_cast<float>(slider->getValue()));
            return;
        }
        else if (slider == &lfo.depthSlider)
        {
            audioProcessor.setLFODepth(i, static_cast<float>(slider->getValue()) / 100.0f);
            return;
        }
    }

    // Envelope sliders
    for (int i = 0; i < 2; ++i)
    {
        auto& env = envControls[static_cast<size_t>(i)];
        if (slider == &env.attackSlider)
        {
            audioProcessor.setEnvelopeAttack(i, static_cast<float>(slider->getValue()));
            return;
        }
        else if (slider == &env.decaySlider)
        {
            audioProcessor.setEnvelopeDecay(i, static_cast<float>(slider->getValue()));
            return;
        }
        else if (slider == &env.sustainSlider)
        {
            audioProcessor.setEnvelopeSustain(i, static_cast<float>(slider->getValue()) / 100.0f);
            return;
        }
        else if (slider == &env.releaseSlider)
        {
            audioProcessor.setEnvelopeRelease(i, static_cast<float>(slider->getValue()));
            return;
        }
        else if (slider == &env.depthSlider)
        {
            audioProcessor.setEnvelopeDepth(i, static_cast<float>(slider->getValue()) / 100.0f);
            return;
        }
    }

    // Step Sequencer sliders
    for (int i = 0; i < 2; ++i)
    {
        auto& seq = seqControls[static_cast<size_t>(i)];
        for (int s = 0; s < 16; ++s)
        {
            if (slider == &seq.stepSliders[static_cast<size_t>(s)])
            {
                audioProcessor.setStepSeqStep(i, s, static_cast<float>(slider->getValue()));
                return;
            }
        }
        if (slider == &seq.glideSlider)
        {
            audioProcessor.setStepSeqGlide(i, static_cast<float>(slider->getValue()) / 100.0f);
            return;
        }
        else if (slider == &seq.depthSlider)
        {
            audioProcessor.setStepSeqDepth(i, static_cast<float>(slider->getValue()) / 100.0f);
            return;
        }
    }
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

void UhbikWrapperAudioProcessorEditor::updateModulationUI()
{
    bool showLFOs = modPanelExpanded && (currentModTab == ModTab::LFOs);
    bool showEnvs = modPanelExpanded && (currentModTab == ModTab::Envs);
    bool showSeqs = modPanelExpanded && (currentModTab == ModTab::StepSeqs);
    bool showMatrix = modPanelExpanded && (currentModTab == ModTab::Matrix);

    // Tab buttons
    modTabLFOsButton.setVisible(modPanelExpanded);
    modTabEnvsButton.setVisible(modPanelExpanded);
    modTabSeqsButton.setVisible(modPanelExpanded);
    modTabMatrixButton.setVisible(modPanelExpanded);

    // LFO controls
    for (auto& lfo : lfoControls)
    {
        lfo.nameLabel.setVisible(showLFOs);
        lfo.rateSlider.setVisible(showLFOs);
        lfo.rateLabel.setVisible(showLFOs);
        lfo.depthSlider.setVisible(showLFOs);
        lfo.depthLabel.setVisible(showLFOs);
        lfo.waveformBox.setVisible(showLFOs);
    }

    // Envelope controls
    for (auto& env : envControls)
    {
        env.nameLabel.setVisible(showEnvs);
        env.attackSlider.setVisible(showEnvs);
        env.decaySlider.setVisible(showEnvs);
        env.sustainSlider.setVisible(showEnvs);
        env.releaseSlider.setVisible(showEnvs);
        env.depthSlider.setVisible(showEnvs);
        env.triggerButton.setVisible(showEnvs);
    }

    // Step Sequencer controls
    for (auto& seq : seqControls)
    {
        seq.nameLabel.setVisible(showSeqs);
        for (auto& step : seq.stepSliders)
            step.setVisible(showSeqs);
        seq.divisionBox.setVisible(showSeqs);
        seq.glideSlider.setVisible(showSeqs);
        seq.depthSlider.setVisible(showSeqs);
        seq.patternBox.setVisible(showSeqs);
    }

    // Matrix controls
    matrixSourceBox.setVisible(showMatrix);
    matrixSlotBox.setVisible(showMatrix);
    matrixParamBox.setVisible(showMatrix);
    matrixAmountSlider.setVisible(showMatrix);
    matrixAddButton.setVisible(showMatrix);
    matrixClearButton.setVisible(showMatrix);
    matrixRoutesLabel.setVisible(showMatrix);
    matrixRoutesList.setVisible(showMatrix);

    // Update toggle button text
    modPanelToggleButton.setButtonText(modPanelExpanded ? "MODULATION v" : "MODULATION >");

    if (showMatrix)
        refreshModRoutesList();
}

void UhbikWrapperAudioProcessorEditor::updateModTabButtons()
{
    // Highlight active tab
    juce::Colour activeCol(0xff6688cc);
    juce::Colour inactiveCol(0xff446699);

    modTabLFOsButton.setColour(juce::TextButton::buttonColourId,
        currentModTab == ModTab::LFOs ? activeCol : inactiveCol);
    modTabEnvsButton.setColour(juce::TextButton::buttonColourId,
        currentModTab == ModTab::Envs ? activeCol : inactiveCol);
    modTabSeqsButton.setColour(juce::TextButton::buttonColourId,
        currentModTab == ModTab::StepSeqs ? activeCol : inactiveCol);
    modTabMatrixButton.setColour(juce::TextButton::buttonColourId,
        currentModTab == ModTab::Matrix ? activeCol : inactiveCol);
}

void UhbikWrapperAudioProcessorEditor::populateMatrixSlotBox()
{
    matrixSlotBox.clear();
    int chainSize = audioProcessor.getChainSize();

    for (int i = 0; i < chainSize; ++i)
    {
        auto& slot = audioProcessor.effectChain[static_cast<size_t>(i)];
        if (slot.isCLAP())  // Only CLAP plugins support modulation
        {
            matrixSlotBox.addItem(slot.description.name, i + 1);
        }
    }

    matrixParamBox.clear();
}

void UhbikWrapperAudioProcessorEditor::populateMatrixParamBox()
{
    matrixParamBox.clear();
    int slotIndex = matrixSlotBox.getSelectedId() - 1;

    if (slotIndex >= 0)
    {
        auto params = audioProcessor.getModulatableParametersForSlot(slotIndex);
        int id = 1;
        for (const auto& param : params)
        {
            matrixParamBox.addItem(param.name, id++);
        }
    }
}

void UhbikWrapperAudioProcessorEditor::refreshModRoutesList()
{
    matrixRoutesList.updateContent();
    matrixRoutesList.repaint();
}

// ModRouteListModel implementation
int UhbikWrapperAudioProcessorEditor::ModRouteListModel::getNumRows()
{
    return static_cast<int>(editor.audioProcessor.getModulationRoutes().size());
}

void UhbikWrapperAudioProcessorEditor::ModRouteListModel::paintListBoxItem(
    int row, juce::Graphics& g, int w, int h, bool selected)
{
    const auto& routes = editor.audioProcessor.getModulationRoutes();
    if (row < 0 || row >= static_cast<int>(routes.size()))
        return;

    const auto& route = routes[static_cast<size_t>(row)];

    if (selected)
        g.fillAll(juce::Colour(0xff3355aa));

    g.setColour(juce::Colours::white);
    g.setFont(12.0f);

    juce::String text = route.getSourceName() + " -> " +
                        route.target.paramName + " (" +
                        juce::String(static_cast<int>(route.amount * 100)) + "%)";

    g.drawText(text, 5, 0, w - 30, h, juce::Justification::centredLeft);

    // Draw X button for removal
    g.setColour(juce::Colours::red);
    g.drawText("X", w - 20, 0, 15, h, juce::Justification::centred);
}

void UhbikWrapperAudioProcessorEditor::ModRouteListModel::listBoxItemClicked(
    int row, const juce::MouseEvent& e)
{
    // Check if click was on X button (right side)
    if (e.x > editor.matrixRoutesList.getWidth() - 25)
    {
        editor.audioProcessor.removeModulationRoute(row);
        editor.refreshModRoutesList();
    }
}

void UhbikWrapperAudioProcessorEditor::populatePluginSelector()
{
    pluginSelector.clear();
    effectPlugins.clear();

    // Get unified list of all available plugins (VST3 + CLAP)
    const auto& allPlugins = audioProcessor.getAvailablePlugins();

    // Get current filter selection (1=All, 2=CLAP, 3=VST3)
    int filterSelection = formatFilter.getSelectedId();

    // Get current type filter (1=All, 2=Effects, 3=Instruments)
    int typeSelection = typeFilter.getSelectedId();

    // Apply format and type filters
    for (const auto& desc : allPlugins)
    {
        // Apply format filter
        if (filterSelection == 2 && desc.format != UnifiedPluginDescription::Format::CLAP)
            continue;
        if (filterSelection == 3 && desc.format != UnifiedPluginDescription::Format::VST3)
            continue;

        // Apply type filter
        if (typeSelection == 2 && desc.isInstrument)
            continue;
        if (typeSelection == 3 && !desc.isInstrument)
            continue;

        effectPlugins.push_back(desc);
    }

    // Sort alphabetically by name (case-insensitive)
    std::sort(effectPlugins.begin(), effectPlugins.end(),
        [](const UnifiedPluginDescription& a, const UnifiedPluginDescription& b) {
            return a.name.compareIgnoreCase(b.name) < 0;
        });

    const char* filterName = (filterSelection == 2) ? "CLAP" : (filterSelection == 3) ? "VST3" : "All";
    std::cerr << "[UI] Populating selector with " << effectPlugins.size() << " plugins (filter: " << filterName << ")" << std::endl << std::flush;

    int id = 1;
    for (const auto& desc : effectPlugins)
    {
        // Add format suffix for "All" view
        juce::String displayName = desc.name;
        if (filterSelection == 1)  // "All" - show format to distinguish duplicates
            displayName += " [" + desc.getFormatName() + "]";
        pluginSelector.addItem(displayName, id++);
    }
}

void UhbikWrapperAudioProcessorEditor::refreshChainDisplay()
{
    slotComponents.clear();

    // Clean up VST3 editor windows for plugins that no longer exist
    for (auto it = editorWindowCache.begin(); it != editorWindowCache.end(); )
    {
        bool found = false;
        // Check main chain
        for (int i = 0; i < audioProcessor.getChainSize(); ++i)
        {
            if (audioProcessor.getPluginAt(i) == it->first)
            {
                found = true;
                break;
            }
        }
        // Check band chains
        if (!found)
        {
            for (int b = 0; b < MultibandProcessor::NUM_BANDS && !found; ++b)
            {
                for (auto& slot : audioProcessor.bandChains[b])
                {
                    if (slot.vst3Plugin.get() == it->first)
                    {
                        found = true;
                        break;
                    }
                }
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

    // Get the active chain (single-chain or current band)
    auto& activeChain = getActiveChain();
    int chainSize = static_cast<int>(activeChain.size());

    int slotHeight = 75;
    int slotSpacing = 4;
    int leftPadding = 8;
    int rightPadding = 20;
    int topPadding = 8;

    int containerWidth = chainViewport.getWidth();
    if (containerWidth < 100) containerWidth = 500;

    chainContainer.setSize(
        containerWidth - rightPadding,
        chainSize > 0 ? chainSize * (slotHeight + slotSpacing) + topPadding * 2 : 100
    );

    for (int i = 0; i < chainSize; ++i)
    {
        auto& slot = activeChain[static_cast<size_t>(i)];
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
    // openPluginEditor works with the active chain
    openPluginEditor(slotIndex);
}

void UhbikWrapperAudioProcessorEditor::effectSlotBypassClicked(int slotIndex)
{
    std::cerr << "[UI] Bypass clicked for slot: " << slotIndex << std::endl << std::flush;
    auto& chain = getActiveChain();
    if (slotIndex >= 0 && slotIndex < static_cast<int>(chain.size()))
    {
        chain[static_cast<size_t>(slotIndex)].bypassed = !chain[static_cast<size_t>(slotIndex)].bypassed;
        audioProcessor.sendChangeMessage();
    }
}

void UhbikWrapperAudioProcessorEditor::effectSlotRemoveClicked(int slotIndex)
{
    std::cerr << "[UI] Remove clicked for slot: " << slotIndex << std::endl << std::flush;

    if (activeBandIndex >= 0)
    {
        int band = activeBandIndex;
        juce::MessageManager::callAsync([this, band, slotIndex]() {
            audioProcessor.removePluginFromBand(band, slotIndex);
        });
    }
    else
    {
        juce::MessageManager::callAsync([this, slotIndex]() {
            audioProcessor.removePlugin(slotIndex);
        });
    }
}

void UhbikWrapperAudioProcessorEditor::effectSlotMoveUpClicked(int slotIndex)
{
    std::cerr << "[UI] Move up clicked for slot: " << slotIndex << std::endl << std::flush;
    if (slotIndex > 0)
    {
        if (activeBandIndex >= 0)
        {
            int band = activeBandIndex;
            juce::MessageManager::callAsync([this, band, slotIndex]() {
                audioProcessor.movePluginInBand(band, slotIndex, slotIndex - 1);
            });
        }
        else
        {
            juce::MessageManager::callAsync([this, slotIndex]() {
                audioProcessor.movePlugin(slotIndex, slotIndex - 1);
            });
        }
    }
}

void UhbikWrapperAudioProcessorEditor::effectSlotMoveDownClicked(int slotIndex)
{
    std::cerr << "[UI] Move down clicked for slot: " << slotIndex << std::endl << std::flush;
    if (slotIndex < getActiveChainSize() - 1)
    {
        if (activeBandIndex >= 0)
        {
            int band = activeBandIndex;
            juce::MessageManager::callAsync([this, band, slotIndex]() {
                audioProcessor.movePluginInBand(band, slotIndex, slotIndex + 1);
            });
        }
        else
        {
            juce::MessageManager::callAsync([this, slotIndex]() {
                audioProcessor.movePlugin(slotIndex, slotIndex + 1);
            });
        }
    }
}

void UhbikWrapperAudioProcessorEditor::effectSlotMixChanged(int slotIndex, float inputGainDb, float outputGainDb, float mixPercent)
{
    auto& chain = getActiveChain();
    if (slotIndex >= 0 && slotIndex < static_cast<int>(chain.size()))
    {
        chain[static_cast<size_t>(slotIndex)].inputGainDb.store(juce::jlimit(-24.0f, 24.0f, inputGainDb));
        chain[static_cast<size_t>(slotIndex)].outputGainDb.store(juce::jlimit(-24.0f, 24.0f, outputGainDb));
        chain[static_cast<size_t>(slotIndex)].mixPercent.store(juce::jlimit(0.0f, 100.0f, mixPercent));
    }
}

void UhbikWrapperAudioProcessorEditor::openPluginEditor(int slotIndex)
{
    std::cerr << "[UI] openPluginEditor called for slot: " << slotIndex << std::endl << std::flush;

    auto& activeChain = getActiveChain();
    if (slotIndex < 0 || slotIndex >= static_cast<int>(activeChain.size()))
    {
        std::cerr << "[UI] Invalid slot index" << std::endl << std::flush;
        return;
    }

    auto& slot = activeChain[static_cast<size_t>(slotIndex)];
    std::cerr << "[UI] Slot: isCLAP=" << slot.isCLAP() << " isVST3=" << slot.isVST3()
              << " hasPlugin=" << slot.hasPlugin() << std::endl << std::flush;

    // Handle CLAP plugins
    if (slot.isCLAP() && slot.clapPlugin)
    {
        auto* clapPlugin = slot.clapPlugin.get();
        std::cerr << "[UI] Opening CLAP editor for: " << slot.description.name << std::endl << std::flush;

        if (!clapPlugin->hasEditor())
        {
            std::cerr << "[UI] CLAP plugin has no editor" << std::endl << std::flush;
            return;
        }

        // Create the CLAP editor window
        auto* editorWindow = clapPlugin->createEditorWindow();
        if (editorWindow == nullptr)
        {
            std::cerr << "[UI] CLAP createEditorWindow returned nullptr" << std::endl << std::flush;
            return;
        }

        // The window is already visible from the constructor
        // Just bring it to front
        editorWindow->toFront(true);
        std::cerr << "[UI] CLAP editor window shown" << std::endl << std::flush;
        return;
    }

    // Handle VST3 plugins
    auto* plugin = slot.vst3Plugin.get();
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

    // Calculate panel heights
    int duckerHeaderHeight = 25;
    int duckerExpandedHeight = 100;
    int duckerHeight = duckerHeaderHeight + (duckerExpanded ? duckerExpandedHeight : 0);

    int modHeaderHeight = 25;
    int modExpandedHeight = 110;
    int modHeight = modHeaderHeight + (modPanelExpanded ? modExpandedHeight : 0);

    int duckerY = getHeight() - 30 - duckerHeight;
    int modY = duckerY - modHeight;

    // Modulation panel background
    // Header bar
    g.setColour(juce::Colour(0xff2a4466));
    g.fillRect(browserWidth, modY, getWidth() - browserWidth, modHeaderHeight);

    if (modPanelExpanded)
    {
        // Panel background
        g.setColour(juce::Colour(0xff1e2833));
        g.fillRect(browserWidth, modY + modHeaderHeight, getWidth() - browserWidth, modExpandedHeight);
    }

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
    if (getActiveChainSize() == 0)
    {
        auto emptyBounds = chainViewport.getBounds();
        g.setColour(juce::Colour(0xff666666));
        g.setFont(16.0f);
        if (activeBandIndex >= 0)
        {
            const char* bandLabels[] = {"LOW", "MID", "HIGH"};
            g.drawFittedText(juce::String("Select a plugin to add to the ") + bandLabels[activeBandIndex] + " band",
                             emptyBounds, juce::Justification::centred, 2);
        }
        else
        {
            g.drawFittedText("Select a plugin from the dropdown to add to the rack",
                             emptyBounds, juce::Justification::centred, 2);
        }
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

    // View menu button after title
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

    // Modulation panel (collapsible, above ducker)
    int modHeaderHeight = 25;
    int modExpandedHeight = 110;
    int modHeight = modHeaderHeight + (modPanelExpanded ? modExpandedHeight : 0);

    auto modBounds = bounds.removeFromBottom(modHeight);

    // Modulation toggle button (always visible)
    modPanelToggleButton.setBounds(modBounds.getX() + 10, modBounds.getY(), 110, modHeaderHeight);

    if (modPanelExpanded)
    {
        int tabY = modBounds.getY();
        int tabWidth = 50;

        // Tab buttons in header bar
        modTabLFOsButton.setBounds(modBounds.getX() + 130, tabY, tabWidth, modHeaderHeight);
        modTabEnvsButton.setBounds(modBounds.getX() + 185, tabY, tabWidth, modHeaderHeight);
        modTabSeqsButton.setBounds(modBounds.getX() + 240, tabY, tabWidth, modHeaderHeight);
        modTabMatrixButton.setBounds(modBounds.getX() + 295, tabY, tabWidth + 10, modHeaderHeight);

        int contentY = modBounds.getY() + modHeaderHeight + 5;
        int contentWidth = modBounds.getWidth();

        if (currentModTab == ModTab::LFOs)
        {
            // LFO tab layout - 4 LFOs horizontally
            int lfoWidth = (contentWidth - 40) / 4;
            int knobSize = 40;
            int labelHeight = 14;

            for (int i = 0; i < 4; ++i)
            {
                auto& lfo = lfoControls[static_cast<size_t>(i)];
                int lfoX = modBounds.getX() + 20 + i * lfoWidth;

                // Name label at top
                lfo.nameLabel.setBounds(lfoX, contentY, lfoWidth - 10, labelHeight);

                // Waveform selector
                lfo.waveformBox.setBounds(lfoX, contentY + labelHeight + 2, lfoWidth - 10, 20);

                // Rate knob
                lfo.rateLabel.setBounds(lfoX, contentY + labelHeight + 26, knobSize, 12);
                lfo.rateSlider.setBounds(lfoX, contentY + labelHeight + 38, knobSize, knobSize);

                // Depth knob
                lfo.depthLabel.setBounds(lfoX + knobSize + 5, contentY + labelHeight + 26, knobSize, 12);
                lfo.depthSlider.setBounds(lfoX + knobSize + 5, contentY + labelHeight + 38, knobSize, knobSize);
            }
        }
        else if (currentModTab == ModTab::Envs)
        {
            // Envelope tab layout - 2 envelopes horizontally
            int envWidth = (contentWidth - 40) / 2;
            int knobSize = 35;
            int labelHeight = 14;

            for (int i = 0; i < 2; ++i)
            {
                auto& env = envControls[static_cast<size_t>(i)];
                int envX = modBounds.getX() + 20 + i * envWidth;

                // Name label at top
                env.nameLabel.setBounds(envX, contentY, envWidth - 10, labelHeight);

                // ADSR knobs in a row
                int knobY = contentY + labelHeight + 2;
                int knobSpacing = knobSize + 5;

                env.attackSlider.setBounds(envX, knobY, knobSize, knobSize + 15);
                env.decaySlider.setBounds(envX + knobSpacing, knobY, knobSize, knobSize + 15);
                env.sustainSlider.setBounds(envX + knobSpacing * 2, knobY, knobSize, knobSize + 15);
                env.releaseSlider.setBounds(envX + knobSpacing * 3, knobY, knobSize, knobSize + 15);
                env.depthSlider.setBounds(envX + knobSpacing * 4, knobY, knobSize, knobSize + 15);

                // Trigger button
                env.triggerButton.setBounds(envX + knobSpacing * 5 + 5, knobY + 15, 50, 22);
            }
        }
        else if (currentModTab == ModTab::StepSeqs)
        {
            // Step Sequencer tab layout - both sequencers, stacked
            int seqHeight = (modExpandedHeight - 10) / 2;

            for (int i = 0; i < 2; ++i)
            {
                auto& seq = seqControls[static_cast<size_t>(i)];
                int seqY = contentY + i * seqHeight;

                // Name label
                seq.nameLabel.setBounds(modBounds.getX() + 10, seqY, 40, 16);

                // 16 step sliders
                int stepWidth = 20;
                int stepHeight = seqHeight - 20;
                int stepsStartX = modBounds.getX() + 55;

                for (int s = 0; s < 16; ++s)
                {
                    seq.stepSliders[static_cast<size_t>(s)].setBounds(
                        stepsStartX + s * (stepWidth + 2), seqY, stepWidth, stepHeight);
                }

                // Controls to the right of steps
                int controlsX = stepsStartX + 16 * (stepWidth + 2) + 10;
                seq.divisionBox.setBounds(controlsX, seqY, 60, 18);
                seq.glideSlider.setBounds(controlsX + 65, seqY, 50, 35);
                seq.depthSlider.setBounds(controlsX + 120, seqY, 50, 35);
                seq.patternBox.setBounds(controlsX, seqY + 22, 60, 18);
            }
        }
        else if (currentModTab == ModTab::Matrix)
        {
            // Matrix tab layout
            int rowHeight = 24;
            int boxWidth = 150;
            int startX = modBounds.getX() + 20;

            // Row 1: Source and Slot
            matrixSourceBox.setBounds(startX, contentY, boxWidth, rowHeight);
            matrixSlotBox.setBounds(startX + boxWidth + 10, contentY, boxWidth, rowHeight);

            // Row 2: Parameter and Amount
            matrixParamBox.setBounds(startX, contentY + rowHeight + 5, boxWidth, rowHeight);
            matrixAmountSlider.setBounds(startX + boxWidth + 10, contentY + rowHeight + 5, boxWidth, rowHeight);

            // Row 3: Buttons
            matrixAddButton.setBounds(startX, contentY + 2 * (rowHeight + 5), 80, rowHeight);
            matrixClearButton.setBounds(startX + 90, contentY + 2 * (rowHeight + 5), 80, rowHeight);

            // Routes list on right side
            int listX = startX + 2 * boxWidth + 30;
            int listWidth = contentWidth - listX - 20 + modBounds.getX();
            matrixRoutesLabel.setBounds(listX, contentY, listWidth, 16);
            matrixRoutesList.setBounds(listX, contentY + 18, listWidth, modExpandedHeight - 25);
        }
    }

    // Band tab bar (always visible between header and chain viewport)
    {
        int mbX = bounds.getX() + 15;

        // --- Row 1: Tab buttons [MAIN] [LOW] [MID] [HIGH] + plugin selector ---
        int row1Height = 32;
        auto row1 = bounds.removeFromTop(row1Height);
        int tabY = row1.getY() + 2;
        int tabWidth = 70;
        int tabHeight = 28;

        bandMainButton.setBounds(mbX, tabY, tabWidth, tabHeight);
        bandLowButton.setBounds(mbX + (tabWidth + 4), tabY, tabWidth, tabHeight);
        bandMidButton.setBounds(mbX + 2 * (tabWidth + 4), tabY, tabWidth, tabHeight);
        bandHighButton.setBounds(mbX + 3 * (tabWidth + 4), tabY, tabWidth, tabHeight);

        // Type filter + plugin selector to the right of tabs
        int typeFilterWidth = 95;
        int typeFilterX = mbX + 4 * (tabWidth + 4) + 15;
        typeFilter.setBounds(typeFilterX, tabY, typeFilterWidth, tabHeight);

        int selectorX = typeFilterX + typeFilterWidth + 4;
        int addBtnWidth = 40;
        int selectorWidth = juce::jmax(120, row1.getRight() - selectorX - addBtnWidth - 25);
        pluginSelector.setBounds(selectorX, tabY, selectorWidth, tabHeight);
        addButton.setBounds(selectorX + selectorWidth + 4, tabY, addBtnWidth, tabHeight);

        // --- Row 2: Band controls (crossovers + S/M/gain) — only on band tabs ---
        if (activeBandIndex >= 0)
        {
            int row2Height = 30;
            auto row2 = bounds.removeFromTop(row2Height);
            int r2Y = row2.getY() + 2;

            // S/M buttons for active band
            int smBtnW = 32;
            int smBtnH = 24;
            int b = activeBandIndex;
            bandSoloButtons[b].setBounds(mbX, r2Y, smBtnW, smBtnH);
            bandMuteButtons[b].setBounds(mbX + smBtnW + 4, r2Y, smBtnW, smBtnH);

            // Gain slider
            int gainX = mbX + 2 * (smBtnW + 4) + 10;
            int gainW = 140;
            bandGainSliders[b].setBounds(gainX, r2Y, gainW, smBtnH);

            // Crossover sliders to the right
            int crossoverX = gainX + gainW + 20;
            int sliderW = 110;
            lowMidCrossoverLabel.setBounds(crossoverX, r2Y, 50, smBtnH);
            lowMidCrossoverSlider.setBounds(crossoverX + 50, r2Y, sliderW, smBtnH);

            int cross2X = crossoverX + 50 + sliderW + 10;
            midHighCrossoverLabel.setBounds(cross2X, r2Y, 55, smBtnH);
            midHighCrossoverSlider.setBounds(cross2X + 55, r2Y, sliderW, smBtnH);
        }

        // --- Row 3: Chain label (24px) ---
        int row3Height = 24;
        auto row3 = bounds.removeFromTop(row3Height);
        bandChainLabel.setBounds(row3.getX() + 15, row3.getY(), row3.getWidth() - 30, row3Height);
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

    int currentFilter = formatFilter.getSelectedId();

    menu.addSectionHeader("Plugin Filter");
    menu.addItem(20, "Show All", true, currentFilter == 1);
    menu.addItem(21, "CLAP Only", true, currentFilter == 2);
    menu.addItem(22, "VST3 Only", true, currentFilter == 3);

    menu.addSeparator();
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
                case 20: formatFilter.setSelectedId(1); populatePluginSelector(); break;
                case 21: formatFilter.setSelectedId(2); populatePluginSelector(); break;
                case 22: formatFilter.setSelectedId(3); populatePluginSelector(); break;
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

    // Hide VST3 editor windows first (don't destroy - let refreshChainDisplay handle cleanup)
    for (auto& entry : editorWindowCache)
    {
        if (entry.second != nullptr)
            entry.second->setVisible(false);
    }

    // Note: CLAP editor windows are closed automatically when plugins are destroyed

    // Clear the effect chain asynchronously to avoid threading issues
    // (same pattern as removePlugin)
    juce::MessageManager::callAsync([this]() {
        audioProcessor.clearChain();
    });
}

// --- Multiband UI Methods ---

std::vector<EffectSlot>& UhbikWrapperAudioProcessorEditor::getActiveChain()
{
    if (activeBandIndex >= 0)
        return audioProcessor.bandChains[activeBandIndex];
    return audioProcessor.effectChain;
}

int UhbikWrapperAudioProcessorEditor::getActiveChainSize()
{
    if (activeBandIndex >= 0)
        return audioProcessor.getBandChainSize(activeBandIndex);
    return audioProcessor.getChainSize();
}

void UhbikWrapperAudioProcessorEditor::updateMultibandUI()
{
    // Tab buttons are always visible
    bandMainButton.setVisible(true);
    bandLowButton.setVisible(true);
    bandMidButton.setVisible(true);
    bandHighButton.setVisible(true);
    bandChainLabel.setVisible(true);

    // Band-specific controls only visible when a band tab is selected (not MAIN)
    bool onBandTab = (activeBandIndex >= 0);
    lowMidCrossoverSlider.setVisible(onBandTab);
    midHighCrossoverSlider.setVisible(onBandTab);
    lowMidCrossoverLabel.setVisible(onBandTab);
    midHighCrossoverLabel.setVisible(onBandTab);

    for (int b = 0; b < 3; ++b)
    {
        bool showBand = onBandTab && (b == activeBandIndex);
        bandSoloButtons[b].setVisible(showBand);
        bandMuteButtons[b].setVisible(showBand);
        bandGainSliders[b].setVisible(showBand);
    }

    updateBandTabButtons();
}

void UhbikWrapperAudioProcessorEditor::updateBandTabButtons()
{
    juce::Colour bandActiveColours[] = {
        juce::Colour(0xff4488cc),  // Low - blue
        juce::Colour(0xff44aa44),  // Mid - green
        juce::Colour(0xffcc6644)   // High - orange
    };
    juce::Colour mainActiveColour(0xff888888);
    juce::Colour inactiveColour(0xff444444);
    const char* bandLabels[] = {"LOW", "MID", "HIGH"};

    // MAIN tab highlight
    bandMainButton.setColour(juce::TextButton::buttonColourId,
        activeBandIndex == -1 ? mainActiveColour : inactiveColour);

    // Band tabs highlight
    juce::TextButton* btns[] = { &bandLowButton, &bandMidButton, &bandHighButton };
    for (int b = 0; b < 3; ++b)
    {
        btns[b]->setColour(juce::TextButton::buttonColourId,
            b == activeBandIndex ? bandActiveColours[b] : inactiveColour);

        // Only show the active band's S/M/Gain controls
        bool showBand = (b == activeBandIndex);
        bandSoloButtons[b].setVisible(showBand);
        bandMuteButtons[b].setVisible(showBand);
        bandGainSliders[b].setVisible(showBand);
    }

    // Band-specific controls visibility
    bool onBandTab = (activeBandIndex >= 0);
    lowMidCrossoverSlider.setVisible(onBandTab);
    midHighCrossoverSlider.setVisible(onBandTab);
    lowMidCrossoverLabel.setVisible(onBandTab);
    midHighCrossoverLabel.setVisible(onBandTab);

    if (activeBandIndex >= 0)
    {
        // Update S/M button colors to reflect the active band's current state
        bool soloed = audioProcessor.multibandProcessor.bandStates[activeBandIndex].solo.load();
        bool muted = audioProcessor.multibandProcessor.bandStates[activeBandIndex].mute.load();
        bandSoloButtons[activeBandIndex].setColour(juce::TextButton::buttonColourId,
            soloed ? juce::Colour(0xffcccc44) : juce::Colour(0xff333333));
        bandMuteButtons[activeBandIndex].setColour(juce::TextButton::buttonColourId,
            muted ? juce::Colour(0xffcc4444) : juce::Colour(0xff333333));

        // Sync gain slider value from processor state
        bandGainSliders[activeBandIndex].setValue(
            audioProcessor.multibandProcessor.bandStates[activeBandIndex].gainDb.load(),
            juce::dontSendNotification);

        // Band chain label
        int bandSize = audioProcessor.getBandChainSize(activeBandIndex);
        juce::String labelText = juce::String(bandLabels[activeBandIndex]) + " Band ("
                               + juce::String(bandSize) + " effect" + (bandSize != 1 ? "s" : "") + ")";
        bandChainLabel.setText(labelText, juce::dontSendNotification);
        bandChainLabel.setColour(juce::Label::textColourId, bandActiveColours[activeBandIndex]);
    }
    else
    {
        // MAIN tab - show main chain info
        int chainSize = audioProcessor.getChainSize();
        juce::String labelText = juce::String("Main Chain (")
                               + juce::String(chainSize) + " effect" + (chainSize != 1 ? "s" : "") + ")";
        bandChainLabel.setText(labelText, juce::dontSendNotification);
        bandChainLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    }
}

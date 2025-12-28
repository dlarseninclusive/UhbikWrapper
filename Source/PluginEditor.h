#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "EffectSlot.h"
#include "PresetBrowser.h"

class UhbikWrapperAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                          private juce::Timer,
                                          private juce::ChangeListener,
                                          private juce::ComboBox::Listener,
                                          private juce::Button::Listener,
                                          public EffectSlotComponent::Listener,
                                          public PresetBrowser::Listener
{
public:
    UhbikWrapperAudioProcessorEditor (UhbikWrapperAudioProcessor&);
    ~UhbikWrapperAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void effectSlotEditClicked(int slotIndex) override;
    void effectSlotBypassClicked(int slotIndex) override;
    void effectSlotRemoveClicked(int slotIndex) override;
    void effectSlotMoveUpClicked(int slotIndex) override;
    void effectSlotMoveDownClicked(int slotIndex) override;

    // PresetBrowser::Listener
    void presetSelected(const juce::File& presetFile) override;
    void savePresetRequested(const juce::File& folder, const juce::String& name) override;

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;
    void buttonClicked(juce::Button* button) override;

    void populatePluginSelector();
    void refreshChainDisplay();
    void openPluginEditor(int slotIndex);

    UhbikWrapperAudioProcessor& audioProcessor;
    juce::String statusMessage;

    juce::Viewport chainViewport;
    juce::Component chainContainer;
    std::vector<std::unique_ptr<EffectSlotComponent>> slotComponents;
    std::vector<juce::PluginDescription> effectPlugins; // Effects only (no instruments)

    juce::ComboBox pluginSelector;
    juce::TextButton addButton{"+"};
    juce::TextButton viewMenuButton{"View"};
    juce::ToggleButton debugToggle{"Debug Log"};

    float uiScale = 1.0f;
    std::unique_ptr<PresetBrowser> presetBrowser;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::vector<std::unique_ptr<juce::DialogWindow>> pluginEditorWindows;

    void savePreset();
    void loadPreset();
    void setUIScale(float scale);
    void showViewMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UhbikWrapperAudioProcessorEditor)
};

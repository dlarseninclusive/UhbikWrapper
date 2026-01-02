#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <map>
#include "PluginProcessor.h"
#include "EffectSlot.h"
#include "PresetBrowser.h"

class UhbikWrapperAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                          private juce::Timer,
                                          private juce::ChangeListener,
                                          private juce::ComboBox::Listener,
                                          private juce::Button::Listener,
                                          private juce::Slider::Listener,
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
    void effectSlotMixChanged(int slotIndex, float inputGainDb, float outputGainDb, float mixPercent) override;

    // PresetBrowser::Listener
    void presetSelected(const juce::File& presetFile) override;
    void savePresetRequested(const juce::File& folder, const juce::String& name,
                             const juce::String& author, const juce::String& tags,
                             const juce::String& notes) override;
    void initPresetRequested() override;

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;

    void populatePluginSelector();
    void refreshChainDisplay();
    void openPluginEditor(int slotIndex);

    UhbikWrapperAudioProcessor& audioProcessor;
    juce::String statusMessage;

    juce::Viewport chainViewport;
    juce::Component chainContainer;
    std::vector<std::unique_ptr<EffectSlotComponent>> slotComponents;
    std::vector<UnifiedPluginDescription> effectPlugins; // Effects only (no instruments) - VST3 and CLAP

    juce::ComboBox pluginSelector;
    juce::ComboBox formatFilter;  // Filter by CLAP/VST3/All
    juce::TextButton addButton{"+"};
    juce::TextButton viewMenuButton{"View"};

    float uiScale = 1.0f;
    std::unique_ptr<PresetBrowser> presetBrowser;

    std::unique_ptr<juce::FileChooser> fileChooser;

    // Cache editor windows to avoid recreating (workaround for u-he crash)
    struct EditorWindow : public juce::DocumentWindow
    {
        EditorWindow(const juce::String& name)
            : DocumentWindow(name, juce::Colour(0xff1e1e1e), DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar(true);
            setResizable(false, false);
        }
        void closeButtonPressed() override { setVisible(false); }
    };
    std::map<juce::AudioPluginInstance*, std::unique_ptr<EditorWindow>> editorWindowCache;
    // Note: CLAP editor windows are managed by CLAPPluginInstance::editorWindow

    void savePreset();
    void loadPreset();
    void setUIScale(float scale);
    void showViewMenu();
    void updateDuckerUI();

    // Ducker panel (collapsible)
    bool duckerExpanded = false;
    juce::TextButton duckerToggleButton{"DUCKER"};
    juce::ToggleButton duckerEnableButton{"ON"};
    juce::Slider duckerThresholdSlider;
    juce::Slider duckerAmountSlider;
    juce::Slider duckerAttackSlider;
    juce::Slider duckerReleaseSlider;
    juce::Slider duckerHoldSlider;
    juce::Label duckerThresholdLabel{"", "Thresh"};
    juce::Label duckerAmountLabel{"", "Amount"};
    juce::Label duckerAttackLabel{"", "Attack"};
    juce::Label duckerReleaseLabel{"", "Release"};
    juce::Label duckerHoldLabel{"", "Hold"};

    // Modulation panel (collapsible, tabbed)
    bool modPanelExpanded = false;
    juce::TextButton modPanelToggleButton{"MODULATION"};

    // Tab buttons
    enum class ModTab { LFOs, Envs, StepSeqs, Matrix };
    ModTab currentModTab = ModTab::LFOs;
    juce::TextButton modTabLFOsButton{"LFOs"};
    juce::TextButton modTabEnvsButton{"Envs"};
    juce::TextButton modTabSeqsButton{"Seqs"};
    juce::TextButton modTabMatrixButton{"Matrix"};

    // LFO controls (4 LFOs)
    struct LFOControls {
        juce::Slider rateSlider;
        juce::Slider depthSlider;
        juce::ComboBox waveformBox;
        juce::Label rateLabel{"", "Rate"};
        juce::Label depthLabel{"", "Depth"};
        juce::Label nameLabel{"", "LFO 1"};
    };
    std::array<LFOControls, 4> lfoControls;

    // Envelope controls (2 envelopes)
    struct EnvControls {
        juce::Slider attackSlider;
        juce::Slider decaySlider;
        juce::Slider sustainSlider;
        juce::Slider releaseSlider;
        juce::Slider depthSlider;
        juce::TextButton triggerButton{"Trigger"};
        juce::Label nameLabel{"", "Env 1"};
    };
    std::array<EnvControls, 2> envControls;

    // Step Sequencer controls (2 sequencers)
    struct SeqControls {
        std::array<juce::Slider, 16> stepSliders;  // 16 visible steps
        juce::ComboBox divisionBox;
        juce::Slider glideSlider;
        juce::Slider depthSlider;
        juce::ComboBox patternBox;
        juce::Label nameLabel{"", "Seq 1"};
    };
    std::array<SeqControls, 2> seqControls;
    int currentSeqIndex = 0;  // Which sequencer is displayed

    // Matrix controls
    juce::ComboBox matrixSourceBox;      // Select LFO source
    juce::ComboBox matrixSlotBox;        // Select effect slot
    juce::ComboBox matrixParamBox;       // Select parameter
    juce::Slider matrixAmountSlider;     // Modulation amount
    juce::TextButton matrixAddButton{"Add Route"};
    juce::TextButton matrixClearButton{"Clear All"};
    juce::Label matrixRoutesLabel{"", "Active Routes:"};
    juce::ListBox matrixRoutesList;

    // Route list model
    class ModRouteListModel : public juce::ListBoxModel {
    public:
        ModRouteListModel(UhbikWrapperAudioProcessorEditor& e) : editor(e) {}
        int getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    private:
        UhbikWrapperAudioProcessorEditor& editor;
    };
    std::unique_ptr<ModRouteListModel> modRouteListModel;

    void updateModulationUI();
    void updateModTabButtons();
    void populateMatrixSlotBox();
    void populateMatrixParamBox();
    void refreshModRoutesList();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UhbikWrapperAudioProcessorEditor)
};

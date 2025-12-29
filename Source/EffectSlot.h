#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class EffectSlotComponent : public juce::Component,
                            public juce::Button::Listener,
                            public juce::Slider::Listener
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void effectSlotEditClicked(int slotIndex) = 0;
        virtual void effectSlotBypassClicked(int slotIndex) = 0;
        virtual void effectSlotRemoveClicked(int slotIndex) = 0;
        virtual void effectSlotMoveUpClicked(int slotIndex) = 0;
        virtual void effectSlotMoveDownClicked(int slotIndex) = 0;
        virtual void effectSlotMixChanged(int slotIndex, float inputGainDb, float outputGainDb, float mixPercent) = 0;
    };

    EffectSlotComponent(int index, const juce::String& pluginName, bool bypassed, bool canMoveUp, bool canMoveDown,
                        float inputGainDb = 0.0f, float outputGainDb = 0.0f, float mixPercent = 100.0f);
    ~EffectSlotComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;

    void setListener(Listener* l) { listener = l; }
    void setSlotIndex(int index) { slotIndex = index; }
    int getSlotIndex() const { return slotIndex; }
    void setBypassed(bool bypassed);
    void setPluginName(const juce::String& name);
    void updateBypassButtonColour();
    void setCanMove(bool up, bool down);
    void setMixValues(float inputGainDb, float outputGainDb, float mixPercent);

private:
    int slotIndex;
    juce::String pluginName;
    bool isBypassed;

    juce::Label nameLabel;
    juce::TextButton upButton{"^"};
    juce::TextButton downButton{"v"};
    juce::TextButton editButton{"Edit"};
    juce::TextButton bypassButton{"B"};
    juce::TextButton removeButton{"X"};

    // Per-effect mixing controls
    juce::Slider inputGainSlider;
    juce::Slider outputGainSlider;
    juce::Slider mixSlider;
    juce::Label inputGainLabel;
    juce::Label outputGainLabel;
    juce::Label mixLabel;

    Listener* listener = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectSlotComponent)
};

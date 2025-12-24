#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class EffectSlotComponent : public juce::Component,
                            public juce::Button::Listener
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
    };

    EffectSlotComponent(int index, const juce::String& pluginName, bool bypassed, bool canMoveUp, bool canMoveDown);
    ~EffectSlotComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void buttonClicked(juce::Button* button) override;

    void setListener(Listener* l) { listener = l; }
    void setSlotIndex(int index) { slotIndex = index; }
    int getSlotIndex() const { return slotIndex; }
    void setBypassed(bool bypassed);
    void setPluginName(const juce::String& name);
    void updateBypassButtonColour();
    void setCanMove(bool up, bool down);

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

    Listener* listener = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectSlotComponent)
};

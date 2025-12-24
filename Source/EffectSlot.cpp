#include "EffectSlot.h"

EffectSlotComponent::EffectSlotComponent(int index, const juce::String& name, bool bypassed, bool canMoveUp, bool canMoveDown)
    : slotIndex(index), pluginName(name), isBypassed(bypassed)
{
    nameLabel.setText(pluginName, juce::dontSendNotification);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    nameLabel.setFont(juce::Font(15.0f, juce::Font::bold));
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(nameLabel);

    upButton.addListener(this);
    upButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff555555));
    upButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    upButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    upButton.setEnabled(canMoveUp);
    addAndMakeVisible(upButton);

    downButton.addListener(this);
    downButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff555555));
    downButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    downButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    downButton.setEnabled(canMoveDown);
    addAndMakeVisible(downButton);

    editButton.addListener(this);
    editButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4466aa));
    editButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    editButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(editButton);

    bypassButton.addListener(this);
    updateBypassButtonColour();
    addAndMakeVisible(bypassButton);

    removeButton.addListener(this);
    removeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffaa3333));
    removeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    removeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(removeButton);
}

EffectSlotComponent::~EffectSlotComponent()
{
    upButton.removeListener(this);
    downButton.removeListener(this);
    editButton.removeListener(this);
    bypassButton.removeListener(this);
    removeButton.removeListener(this);
}

void EffectSlotComponent::updateBypassButtonColour()
{
    bypassButton.setColour(juce::TextButton::buttonColourId,
                           isBypassed ? juce::Colour(0xff666666) : juce::Colour(0xff44aa44));
    bypassButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    bypassButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
}

void EffectSlotComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Rack module background - metallic dark grey with gradient
    juce::ColourGradient gradient(juce::Colour(0xff3a3a3a), 0, 0,
                                   juce::Colour(0xff252525), 0, static_cast<float>(bounds.getHeight()), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    // Top highlight line
    g.setColour(juce::Colour(0xff4a4a4a));
    g.drawHorizontalLine(1, 2.0f, static_cast<float>(bounds.getWidth() - 2));

    // Left status bar (orange when active, grey when bypassed)
    g.setColour(isBypassed ? juce::Colour(0xff555555) : juce::Colour(0xffff7700));
    g.fillRoundedRectangle(2.0f, 4.0f, 6.0f, static_cast<float>(bounds.getHeight() - 8), 2.0f);

    // Border
    g.setColour(juce::Colour(0xff1a1a1a));
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 4.0f, 1.0f);

    // Rack screw holes (decorative)
    g.setColour(juce::Colour(0xff222222));
    g.fillEllipse(bounds.getWidth() - 18.0f, 6.0f, 10.0f, 10.0f);
    g.fillEllipse(bounds.getWidth() - 18.0f, bounds.getHeight() - 16.0f, 10.0f, 10.0f);
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawEllipse(bounds.getWidth() - 18.0f, 6.0f, 10.0f, 10.0f, 1.0f);
    g.drawEllipse(bounds.getWidth() - 18.0f, bounds.getHeight() - 16.0f, 10.0f, 10.0f, 1.0f);
}

void EffectSlotComponent::resized()
{
    auto bounds = getLocalBounds().reduced(8, 6);

    // Up/down buttons on the left
    auto arrowWidth = 24;
    auto arrowHeight = 20;
    upButton.setBounds(bounds.getX(), bounds.getY(), arrowWidth, arrowHeight);
    downButton.setBounds(bounds.getX(), bounds.getBottom() - arrowHeight, arrowWidth, arrowHeight);
    bounds.removeFromLeft(arrowWidth + 4);

    // Status bar space
    bounds.removeFromLeft(10);

    auto buttonWidth = 45;
    auto buttonHeight = 26;

    // Buttons on the right
    auto buttonArea = bounds.removeFromRight(buttonWidth * 3 + 12);

    int buttonY = (bounds.getHeight() - buttonHeight) / 2;

    editButton.setBounds(buttonArea.getX(), buttonY, buttonWidth, buttonHeight);
    bypassButton.setBounds(buttonArea.getX() + buttonWidth + 4, buttonY, buttonWidth, buttonHeight);
    removeButton.setBounds(buttonArea.getX() + buttonWidth * 2 + 8, buttonY, buttonWidth, buttonHeight);

    nameLabel.setBounds(bounds.getX(), 0, bounds.getWidth() - 4, getHeight());
}

void EffectSlotComponent::buttonClicked(juce::Button* button)
{
    if (listener == nullptr)
        return;

    if (button == &editButton)
    {
        listener->effectSlotEditClicked(slotIndex);
    }
    else if (button == &bypassButton)
    {
        listener->effectSlotBypassClicked(slotIndex);
    }
    else if (button == &removeButton)
    {
        listener->effectSlotRemoveClicked(slotIndex);
    }
    else if (button == &upButton)
    {
        listener->effectSlotMoveUpClicked(slotIndex);
    }
    else if (button == &downButton)
    {
        listener->effectSlotMoveDownClicked(slotIndex);
    }
}

void EffectSlotComponent::setCanMove(bool up, bool down)
{
    upButton.setEnabled(up);
    downButton.setEnabled(down);
}

void EffectSlotComponent::setBypassed(bool bypassed)
{
    isBypassed = bypassed;
    updateBypassButtonColour();
    repaint();
}

void EffectSlotComponent::setPluginName(const juce::String& name)
{
    pluginName = name;
    nameLabel.setText(pluginName, juce::dontSendNotification);
}

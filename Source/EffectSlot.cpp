#include "EffectSlot.h"

EffectSlotComponent::EffectSlotComponent(int index, const juce::String& name, bool bypassed, bool canMoveUp, bool canMoveDown,
                                         float inputGainDb, float outputGainDb, float mixPercent)
    : slotIndex(index), pluginName(name), isBypassed(bypassed)
{
    nameLabel.setText(pluginName, juce::dontSendNotification);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    nameLabel.setFont(juce::Font(14.0f, juce::Font::bold));
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

    // Input gain slider
    inputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    inputGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    inputGainSlider.setRange(-24.0, 24.0, 0.1);
    inputGainSlider.setValue(inputGainDb, juce::dontSendNotification);
    inputGainSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff44aa44));
    inputGainSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff333333));
    inputGainSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    inputGainSlider.addListener(this);
    addAndMakeVisible(inputGainSlider);

    inputGainLabel.setText("In", juce::dontSendNotification);
    inputGainLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    inputGainLabel.setFont(juce::Font(10.0f));
    inputGainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(inputGainLabel);

    // Output gain slider
    outputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    outputGainSlider.setRange(-24.0, 24.0, 0.1);
    outputGainSlider.setValue(outputGainDb, juce::dontSendNotification);
    outputGainSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffaa4444));
    outputGainSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff333333));
    outputGainSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    outputGainSlider.addListener(this);
    addAndMakeVisible(outputGainSlider);

    outputGainLabel.setText("Out", juce::dontSendNotification);
    outputGainLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    outputGainLabel.setFont(juce::Font(10.0f));
    outputGainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputGainLabel);

    // Mix slider
    mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    mixSlider.setRange(0.0, 100.0, 1.0);
    mixSlider.setValue(mixPercent, juce::dontSendNotification);
    mixSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff4488cc));
    mixSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff333333));
    mixSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    mixSlider.addListener(this);
    addAndMakeVisible(mixSlider);

    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    mixLabel.setFont(juce::Font(10.0f));
    mixLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mixLabel);
}

EffectSlotComponent::~EffectSlotComponent()
{
    upButton.removeListener(this);
    downButton.removeListener(this);
    editButton.removeListener(this);
    bypassButton.removeListener(this);
    removeButton.removeListener(this);
    inputGainSlider.removeListener(this);
    outputGainSlider.removeListener(this);
    mixSlider.removeListener(this);
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

    // Draw level meters (positioned after arrows, before name)
    int meterX = 40;  // After arrows and status bar
    int meterWidth = 14;
    int meterHeight = bounds.getHeight() - 12;
    int meterY = 6;

    // Input meter
    drawMeter(g, juce::Rectangle<int>(meterX, meterY, meterWidth, meterHeight), inputLevelL, inputLevelR);

    // Output meter
    drawMeter(g, juce::Rectangle<int>(meterX + meterWidth + 4, meterY, meterWidth, meterHeight), outputLevelL, outputLevelR);
}

void EffectSlotComponent::resized()
{
    auto bounds = getLocalBounds().reduced(8, 4);

    // Up/down buttons on the left
    auto arrowWidth = 24;
    auto arrowHeight = 18;
    upButton.setBounds(bounds.getX(), bounds.getY(), arrowWidth, arrowHeight);
    downButton.setBounds(bounds.getX(), bounds.getBottom() - arrowHeight, arrowWidth, arrowHeight);
    bounds.removeFromLeft(arrowWidth + 4);

    // Status bar space
    bounds.removeFromLeft(10);

    // Space for level meters (2 meters * 14px + gap)
    bounds.removeFromLeft(36);

    auto buttonWidth = 40;
    auto buttonHeight = 24;

    // Buttons on the right (Edit, Bypass, Remove)
    auto buttonArea = bounds.removeFromRight(buttonWidth * 3 + 8);
    int buttonY = (bounds.getHeight() - buttonHeight) / 2;
    editButton.setBounds(buttonArea.getX(), buttonY, buttonWidth, buttonHeight);
    bypassButton.setBounds(buttonArea.getX() + buttonWidth + 2, buttonY, buttonWidth, buttonHeight);
    removeButton.setBounds(buttonArea.getX() + buttonWidth * 2 + 4, buttonY, buttonWidth, buttonHeight);

    // Knobs area (3 knobs with labels)
    auto knobSize = 36;
    auto knobSpacing = 4;
    auto labelHeight = 12;
    auto knobAreaWidth = knobSize * 3 + knobSpacing * 2;
    auto knobArea = bounds.removeFromRight(knobAreaWidth + 8);
    int knobY = (bounds.getHeight() - knobSize - labelHeight) / 2;

    inputGainSlider.setBounds(knobArea.getX(), knobY, knobSize, knobSize);
    inputGainLabel.setBounds(knobArea.getX(), knobY + knobSize, knobSize, labelHeight);

    outputGainSlider.setBounds(knobArea.getX() + knobSize + knobSpacing, knobY, knobSize, knobSize);
    outputGainLabel.setBounds(knobArea.getX() + knobSize + knobSpacing, knobY + knobSize, knobSize, labelHeight);

    mixSlider.setBounds(knobArea.getX() + (knobSize + knobSpacing) * 2, knobY, knobSize, knobSize);
    mixLabel.setBounds(knobArea.getX() + (knobSize + knobSpacing) * 2, knobY + knobSize, knobSize, labelHeight);

    // Plugin name fills the remaining space
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

void EffectSlotComponent::sliderValueChanged(juce::Slider* slider)
{
    if (listener == nullptr)
        return;

    listener->effectSlotMixChanged(slotIndex,
                                   static_cast<float>(inputGainSlider.getValue()),
                                   static_cast<float>(outputGainSlider.getValue()),
                                   static_cast<float>(mixSlider.getValue()));
}

void EffectSlotComponent::setMixValues(float inputGainDb, float outputGainDb, float mixPercent)
{
    inputGainSlider.setValue(inputGainDb, juce::dontSendNotification);
    outputGainSlider.setValue(outputGainDb, juce::dontSendNotification);
    mixSlider.setValue(mixPercent, juce::dontSendNotification);
}

void EffectSlotComponent::setLevels(float inL, float inR, float outL, float outR)
{
    inputLevelL = inL;
    inputLevelR = inR;
    outputLevelL = outL;
    outputLevelR = outR;
    repaint();  // Trigger repaint to update meters
}

void EffectSlotComponent::drawMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float levelL, float levelR)
{
    // Background
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(bounds);

    int meterHeight = bounds.getHeight();
    int meterWidth = bounds.getWidth() / 2 - 1;

    // Left channel
    auto leftBounds = bounds.removeFromLeft(meterWidth);
    int levelHeight = static_cast<int>(juce::jmin(1.0f, levelL) * meterHeight);

    // Gradient from green to yellow to red
    if (levelHeight > 0)
    {
        auto meterRect = leftBounds.withTop(leftBounds.getBottom() - levelHeight);
        if (levelL > 0.9f)
            g.setColour(juce::Colour(0xffff3333));  // Red for hot
        else if (levelL > 0.7f)
            g.setColour(juce::Colour(0xffffaa00));  // Yellow/orange
        else
            g.setColour(juce::Colour(0xff44cc44));  // Green
        g.fillRect(meterRect);
    }

    // Right channel
    bounds.removeFromLeft(2);  // Gap
    auto rightBounds = bounds;
    levelHeight = static_cast<int>(juce::jmin(1.0f, levelR) * meterHeight);

    if (levelHeight > 0)
    {
        auto meterRect = rightBounds.withTop(rightBounds.getBottom() - levelHeight);
        if (levelR > 0.9f)
            g.setColour(juce::Colour(0xffff3333));
        else if (levelR > 0.7f)
            g.setColour(juce::Colour(0xffffaa00));
        else
            g.setColour(juce::Colour(0xff44cc44));
        g.fillRect(meterRect);
    }
}

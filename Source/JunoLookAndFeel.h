#pragma once

#include <JuceHeader.h>

//==============================================================================
// Dark panel, coloured fader caps and LED push buttons in the style of the
// Juno-106 front panel.
class JunoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    JunoLookAndFeel()
    {
        setColour (juce::Slider::thumbColourId, juce::Colour (0xff4a90d9));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a2e));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2a2a2e));
        setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.85f));
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff222226));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, slider);
            return;
        }

        const float cx = (float) x + (float) width * 0.5f;

        // Tick marks
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        const int ticks = 5;
        for (int i = 0; i <= ticks; ++i)
        {
            const float ty = (float) y + (float) height * (float) i / (float) ticks;
            g.drawHorizontalLine ((int) ty, cx - 9.0f, cx + 9.0f);
        }

        // Slot
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (cx - 2.0f, (float) y, 4.0f, (float) height, 2.0f);

        // Fader cap
        const float capW = 22.0f, capH = 13.0f;
        juce::Rectangle<float> cap (cx - capW * 0.5f, sliderPos - capH * 0.5f, capW, capH);
        g.setColour (juce::Colour (0xff151517));
        g.fillRoundedRectangle (cap, 2.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (cap, 2.0f, 1.0f);

        // Coloured stripe across the cap
        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.fillRect (cap.withHeight (3.0f).translated (0.0f, capH * 0.5f - 1.5f).reduced (2.0f, 0.0f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool isOver, bool isDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);

        auto base = juce::Colour (0xff2a2a2e);
        if (isDown) base = base.darker (0.3f);
        else if (isOver) base = base.brighter (0.08f);

        g.setColour (base);
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

        // LED strip along the top edge
        auto led = bounds.removeFromTop (5.0f).reduced (5.0f, 1.5f);
        if (button.getToggleState())
        {
            g.setColour (juce::Colour (0xffff4b2e));
            g.fillRoundedRectangle (led, 1.5f);
            g.setColour (juce::Colour (0x55ff4b2e));
            g.drawRoundedRectangle (led.expanded (1.5f), 2.0f, 1.5f);
        }
        else
        {
            g.setColour (juce::Colour (0xff551a10));
            g.fillRoundedRectangle (led, 1.5f);
        }
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return juce::Font (juce::FontOptions (11.0f, juce::Font::bold));
    }
};

#pragma once
#include <JuceHeader.h>

class InstaWidthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Colour palette — matches the Insta* family
    static inline const juce::Colour bgDark       { 0xff1a1a2e };
    static inline const juce::Colour bgMedium     { 0xff16213e };
    static inline const juce::Colour bgLight      { 0xff0f3460 };
    static inline const juce::Colour textPrimary  { 0xffe0e0e0 };
    static inline const juce::Colour textSecondary { 0xff888899 };
    static inline const juce::Colour accent       { 0xff00ff88 };
    static inline const juce::Colour warningCol   { 0xffff5555 };

    // Knob type property key — "dark" gives blue, default gives orange
    static constexpr const char* knobTypeProperty = "knobType";

    InstaWidthLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    juce::Font getRegularFont (float height) const;
    juce::Font getMediumFont  (float height) const;
    juce::Font getBoldFont    (float height) const;

    void drawBackgroundTexture (juce::Graphics& g, juce::Rectangle<int> area);

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;

private:
    juce::Typeface::Ptr typefaceRegular;
    juce::Typeface::Ptr typefaceMedium;
    juce::Typeface::Ptr typefaceBold;

    juce::Image noiseTexture;
    void generateNoiseTexture();
};

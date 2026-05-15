#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include "Goniometer.h"
#include "CorrelationMeter.h"

class InstaWidthEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit InstaWidthEditor (InstaWidthProcessor& p);
    ~InstaWidthEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    InstaWidthProcessor& processor;
    InstaWidthLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 600 };  // 600 ms hover delay

    void installTooltips();

    // Helper to build a labelled rotary slider attached to an APVTS parameter
    struct KnobUnit
    {
        juce::Slider knob;
        juce::Label  caption;
        juce::Label  value;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void configureKnob (KnobUnit& u, const juce::String& caption, const juce::String& paramId, bool darkStyle = false);

    // Header
    juce::Label  titleLabel { {}, "INSTAWIDTH" };
    juce::Label  versionLabel { {}, "v1.0.0" };
    juce::ToggleButton bypassToggle;
    juce::Label  bypassLabel { {}, "BYPASS" };

    // Mode strip
    juce::Label  modeLabel { {}, "MODE" };
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    juce::Label  firLabel { {}, "FIR" };
    juce::ComboBox firBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> firAttachment;
    juce::Label  latencyLabel { {}, "0 ms" };

    // Auto Mono Safety toggle in the mode strip
    juce::Label autoSafeLabel { {}, "AUTO MONO" };
    juce::ToggleButton autoSafeToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoSafeAttachment;

    // Width knobs
    KnobUnit kWLow, kWMid, kWHigh;
    juce::Label wLabel { {}, "STEREO WIDTH" };

    // Crossover
    KnobUnit kFLow, kFHigh;
    juce::Label xLabel { {}, "CROSSOVER" };

    // Side processing
    KnobUnit kTilt;
    KnobUnit kMonoFreq;
    juce::ToggleButton monoToggle;
    juce::Label monoLabel  { {}, "MONOMAKER" };
    juce::Label tiltLabel  { {}, "SIDE TILT" };

    // Cached section rectangles for paint() — populated from resized()
    juce::Rectangle<int> rWidth, rXover, rMono, rTilt, rDeess, rOutput;

    // De-esser
    juce::ToggleButton deessToggle;
    juce::Label deessLabel { {}, "SIDE DE-ESSER" };
    KnobUnit kDeessFreq, kDeessThresh, kDeessRange;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> deessAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    // Output
    KnobUnit kOutput;

    // Visualisation
    Goniometer goniometer;
    CorrelationMeter corrMeter;

    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstaWidthEditor)
};

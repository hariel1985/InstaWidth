#include "PluginEditor.h"

namespace
{
    constexpr int kMinW = 1060;
    constexpr int kMinH = 640;
    constexpr int kMaxW = 1700;
    constexpr int kMaxH = 1000;
}

InstaWidthEditor::InstaWidthEditor (InstaWidthProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    constrainer.setSizeLimits (kMinW, kMinH, kMaxW, kMaxH);
    setResizable (true, true);
    setConstrainer (&constrainer);
    setSize (kMinW, kMinH);

    processor.setMeterCallbacks (
        [this] (float l, float r) { goniometer.pushSample (l, r); },
        [this] (float l, float r) { corrMeter.pushSample (l, r); });
    corrMeter.prepare (processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0);

    // Header
    titleLabel.setFont (lookAndFeel.getBoldFont (24.0f));
    titleLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::accent);
    addAndMakeVisible (titleLabel);

    versionLabel.setFont (lookAndFeel.getRegularFont (12.0f));
    versionLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    versionLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (versionLabel);

    bypassLabel.setFont (lookAndFeel.getMediumFont (11.0f));
    bypassLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    bypassLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bypassLabel);
    addAndMakeVisible (bypassToggle);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassToggle);

    // Mode strip
    modeLabel.setFont (lookAndFeel.getMediumFont (11.0f));
    modeLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    modeLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (modeLabel);

    modeBox.addItem ("Minimum Phase", 1);
    modeBox.addItem ("Linear Phase",  2);
    addAndMakeVisible (modeBox);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, "mode", modeBox);

    firLabel.setFont (lookAndFeel.getMediumFont (11.0f));
    firLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    firLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (firLabel);

    firBox.addItemList (juce::StringArray { "512", "1024", "2048", "4096", "8192", "16384" }, 1);
    addAndMakeVisible (firBox);
    firAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, "firQuality", firBox);

    latencyLabel.setFont (lookAndFeel.getRegularFont (11.0f));
    latencyLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    latencyLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (latencyLabel);

    auto styleHeaderLabel = [this] (juce::Label& lbl)
    {
        lbl.setFont (lookAndFeel.getBoldFont (12.0f));
        lbl.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textPrimary);
        lbl.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (lbl);
    };

    // Section headers
    styleHeaderLabel (wLabel);      wLabel.setText ("STEREO WIDTH", juce::dontSendNotification);
    styleHeaderLabel (xLabel);      xLabel.setText ("CROSSOVER",    juce::dontSendNotification);
    styleHeaderLabel (monoLabel);   monoLabel.setText ("MONOMAKER",  juce::dontSendNotification);
    styleHeaderLabel (tiltLabel);   tiltLabel.setText ("SIDE TILT",  juce::dontSendNotification);
    styleHeaderLabel (deessLabel);  deessLabel.setText ("SIDE DE-ESSER", juce::dontSendNotification);

    // Knobs
    configureKnob (kWLow,  "LOW",  "wLow");
    configureKnob (kWMid,  "MID",  "wMid");
    configureKnob (kWHigh, "HIGH", "wHigh");
    configureKnob (kFLow,  "L→M", "fLow",  true);  // L→M arrow
    configureKnob (kFHigh, "M→H", "fHigh", true);  // M→H arrow
    configureKnob (kTilt,     "TILT",   "tilt");
    configureKnob (kMonoFreq, "FREQ",   "monoFreq", true);
    configureKnob (kDeessFreq,   "FREQ",   "deessFreq", true);
    configureKnob (kDeessThresh, "THRESH", "deessThresh", true);
    configureKnob (kDeessRange,  "RANGE",  "deessRange", true);
    configureKnob (kOutput, "OUTPUT", "output");

    addAndMakeVisible (monoToggle);
    monoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "monoOn", monoToggle);

    addAndMakeVisible (deessToggle);
    deessAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "deessOn", deessToggle);

    addAndMakeVisible (goniometer);
    addAndMakeVisible (corrMeter);

    startTimerHz (15);
}

InstaWidthEditor::~InstaWidthEditor()
{
    stopTimer();
    processor.setMeterCallbacks (nullptr, nullptr);
    setLookAndFeel (nullptr);
}

void InstaWidthEditor::configureKnob (KnobUnit& u, const juce::String& caption,
                                      const juce::String& paramId, bool darkStyle)
{
    u.knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    u.knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    if (darkStyle)
        u.knob.getProperties().set (InstaWidthLookAndFeel::knobTypeProperty, "dark");
    addAndMakeVisible (u.knob);

    u.caption.setFont (lookAndFeel.getMediumFont (11.0f));
    u.caption.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    u.caption.setJustificationType (juce::Justification::centred);
    u.caption.setText (caption, juce::dontSendNotification);
    addAndMakeVisible (u.caption);

    u.value.setFont (lookAndFeel.getRegularFont (10.0f));
    u.value.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textPrimary);
    u.value.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (u.value);

    u.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, paramId, u.knob);
}

void InstaWidthEditor::timerCallback()
{
    auto setText = [this] (KnobUnit& u, const juce::String& paramId)
    {
        if (auto* p = processor.apvts.getParameter (paramId))
            u.value.setText (p->getCurrentValueAsText(), juce::dontSendNotification);
    };

    setText (kWLow,  "wLow");
    setText (kWMid,  "wMid");
    setText (kWHigh, "wHigh");
    setText (kFLow,  "fLow");
    setText (kFHigh, "fHigh");
    setText (kTilt,  "tilt");
    setText (kMonoFreq, "monoFreq");
    setText (kDeessFreq,   "deessFreq");
    setText (kDeessThresh, "deessThresh");
    setText (kDeessRange,  "deessRange");
    setText (kOutput, "output");

    const int latencySamples = processor.getCurrentLatency();
    const double sr = processor.getSampleRate();
    const double ms = (sr > 0.0) ? 1000.0 * latencySamples / sr : 0.0;
    latencyLabel.setText (latencySamples <= 0
                              ? juce::String ("0 ms")
                              : juce::String (ms, 1) + " ms latency",
                          juce::dontSendNotification);
}

void InstaWidthEditor::paint (juce::Graphics& g)
{
    g.fillAll (InstaWidthLookAndFeel::bgDark);
    lookAndFeel.drawBackgroundTexture (g, getLocalBounds());

    // Header & mode strip dividers
    g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.5f));
    g.drawHorizontalLine (56, 0.0f, (float) getWidth());
    g.drawHorizontalLine (92, 0.0f, (float) getWidth());

    // Section background panels — drawn behind their controls so each block reads as a unit
    auto drawSection = [&] (juce::Rectangle<int> r, juce::String title)
    {
        if (r.isEmpty()) return;
        auto fr = r.toFloat();
        g.setColour (InstaWidthLookAndFeel::bgMedium.withAlpha (0.55f));
        g.fillRoundedRectangle (fr, 6.0f);
        g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.7f));
        g.drawRoundedRectangle (fr, 6.0f, 1.0f);

        g.setColour (InstaWidthLookAndFeel::accent.withAlpha (0.85f));
        g.setFont (lookAndFeel.getBoldFont (10.5f));
        g.drawText (title, r.removeFromTop (18).reduced (10, 2),
                    juce::Justification::centredLeft);
    };

    drawSection (rWidth,  "STEREO WIDTH");
    drawSection (rXover,  "CROSSOVER");
    drawSection (rMono,   "MONOMAKER");
    drawSection (rTilt,   "SIDE TILT");
    drawSection (rDeess,  "SIDE DE-ESSER");
    drawSection (rOutput, "OUTPUT");
}

void InstaWidthEditor::resized()
{
    auto area = getLocalBounds();

    // ---- Header (56 px)
    auto header = area.removeFromTop (56);
    titleLabel.setBounds (header.removeFromLeft (220).withTrimmedLeft (16));
    auto headerRight = header.removeFromRight (220);
    bypassToggle.setBounds (headerRight.removeFromRight (60).reduced (8, 16));
    bypassLabel .setBounds (headerRight.removeFromRight (70).reduced (4, 18));
    versionLabel.setBounds (headerRight.reduced (8, 18));

    // ---- Mode strip (36 px)
    auto mode = area.removeFromTop (36);
    mode.removeFromLeft (16);
    modeLabel.setBounds (mode.removeFromLeft (50));
    modeBox  .setBounds (mode.removeFromLeft (150).reduced (2, 6));
    mode.removeFromLeft (16);
    firLabel .setBounds (mode.removeFromLeft (32));
    firBox   .setBounds (mode.removeFromLeft (110).reduced (2, 6));
    latencyLabel.setBounds (mode.removeFromLeft (200).reduced (8, 6));

    // ---- Bottom correlation strip (44 px)
    auto bottom = area.removeFromBottom (44);
    corrMeter.setBounds (bottom.reduced (14, 8));

    area.reduce (14, 14);

    // ---- Left column: goniometer (square, takes ~45% of width)
    const int leftW = juce::jlimit (320, 480, area.getWidth() * 45 / 100);
    auto leftCol = area.removeFromLeft (leftW);
    {
        int dim = std::min (leftCol.getWidth(), leftCol.getHeight());
        goniometer.setBounds (leftCol.withSizeKeepingCentre (dim, dim));
    }
    area.removeFromLeft (14);

    // ---- Right column: stacked sections.
    //  Row 1: Stereo Width (3 knobs)
    //  Row 2: Crossover (2 knobs) + Side Tilt (1 knob)  — side-by-side
    //  Row 3: Monomaker (toggle + 1 knob)
    //  Row 4: Side De-esser (toggle + 3 knobs)
    //  Row 5: Output (1 big knob)

    auto right = area;
    const int gap = 10;
    const int rows = 5;
    int rowH = (right.getHeight() - gap * (rows - 1)) / rows;

    const int titleH = 22;          // header bar inside section
    const int captionH = 14;
    const int valueH = 14;

    auto layoutKnob = [&] (KnobUnit& u, juce::Rectangle<int> cell)
    {
        u.caption.setBounds (cell.removeFromTop (captionH));
        u.value  .setBounds (cell.removeFromBottom (valueH));
        u.knob   .setBounds (cell.reduced (4));
    };

    // Row 1 — Stereo Width
    {
        rWidth = right.removeFromTop (rowH);
        auto body = rWidth.reduced (10, 4);
        body.removeFromTop (titleH);
        int cellW = body.getWidth() / 3;
        layoutKnob (kWLow,  body.removeFromLeft (cellW));
        layoutKnob (kWMid,  body.removeFromLeft (cellW));
        layoutKnob (kWHigh, body);
        right.removeFromTop (gap);
    }

    // Row 2 — Crossover (left, 2/3) + Side Tilt (right, 1/3)
    {
        auto row = right.removeFromTop (rowH);
        rXover = row.removeFromLeft (row.getWidth() * 2 / 3);
        rXover.removeFromRight (gap / 2);
        rTilt = row;
        rTilt.removeFromLeft (gap / 2);

        auto xBody = rXover.reduced (10, 4);
        xBody.removeFromTop (titleH);
        int xCellW = xBody.getWidth() / 2;
        layoutKnob (kFLow,  xBody.removeFromLeft (xCellW));
        layoutKnob (kFHigh, xBody);

        auto tBody = rTilt.reduced (10, 4);
        tBody.removeFromTop (titleH);
        layoutKnob (kTilt, tBody);

        right.removeFromTop (gap);
    }

    // Row 3 — Monomaker (toggle on left, freq knob on right)
    {
        rMono = right.removeFromTop (rowH);
        auto body = rMono.reduced (10, 4);
        body.removeFromTop (titleH);
        int half = body.getWidth() / 2;
        auto togCell = body.removeFromLeft (half);
        monoToggle.setBounds (togCell.withSizeKeepingCentre (60, std::min (28, togCell.getHeight() - 8)));
        layoutKnob (kMonoFreq, body);
        right.removeFromTop (gap);
    }

    // Row 4 — Side De-esser (toggle + 3 knobs)
    {
        rDeess = right.removeFromTop (rowH);
        auto body = rDeess.reduced (10, 4);
        body.removeFromTop (titleH);
        int cellW = body.getWidth() / 4;
        auto togCell = body.removeFromLeft (cellW);
        deessToggle.setBounds (togCell.withSizeKeepingCentre (60, std::min (28, togCell.getHeight() - 8)));
        layoutKnob (kDeessFreq,   body.removeFromLeft (cellW));
        layoutKnob (kDeessThresh, body.removeFromLeft (cellW));
        layoutKnob (kDeessRange,  body);
        right.removeFromTop (gap);
    }

    // Row 5 — Output
    {
        rOutput = right.removeFromTop (rowH);
        auto body = rOutput.reduced (10, 4);
        body.removeFromTop (titleH);
        // Centre the single knob, sized to roughly match the other knob columns
        int knobW = juce::jlimit (90, 180, body.getHeight());
        auto cell = body.withSizeKeepingCentre (knobW, body.getHeight());
        layoutKnob (kOutput, cell);
    }
}

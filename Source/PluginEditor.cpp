#include "PluginEditor.h"

InstaWidthEditor::InstaWidthEditor (InstaWidthProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    constrainer.setSizeLimits (820, 520, 1600, 1000);
    setResizable (true, true);
    setConstrainer (&constrainer);
    setSize (980, 600);

    // Audio thread → component sample fifos (callbacks captured by ref are safe
    // because the processor outlives the editor; the editor clears them on destruction)
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
    addAndMakeVisible (versionLabel);

    bypassLabel.setFont (lookAndFeel.getMediumFont (11.0f));
    bypassLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    bypassLabel.setJustificationType (juce::Justification::centred);
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

    // Width knobs (orange = primary)
    configureKnob (kWLow,  "LOW",  "wLow");
    configureKnob (kWMid,  "MID",  "wMid");
    configureKnob (kWHigh, "HIGH", "wHigh");
    wLabel.setFont (lookAndFeel.getBoldFont (12.0f));
    wLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textPrimary);
    wLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (wLabel);

    // Crossover knobs (blue = secondary)
    configureKnob (kFLow,  "L–M",  "fLow",  true);
    configureKnob (kFHigh, "M–H",  "fHigh", true);
    xLabel.setFont (lookAndFeel.getBoldFont (12.0f));
    xLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textPrimary);
    xLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (xLabel);

    // Side processing
    configureKnob (kTilt,     "TILT", "tilt");
    configureKnob (kMonoFreq, "FREQ", "monoFreq", true);
    monoLabel.setFont (lookAndFeel.getBoldFont (12.0f));
    monoLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textPrimary);
    monoLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (monoLabel);
    sideLabel.setFont (lookAndFeel.getBoldFont (12.0f));
    sideLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textPrimary);
    sideLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (sideLabel);

    addAndMakeVisible (monoToggle);
    monoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "monoOn", monoToggle);

    // De-esser
    deessLabel.setFont (lookAndFeel.getBoldFont (12.0f));
    deessLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textPrimary);
    deessLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (deessLabel);
    addAndMakeVisible (deessToggle);
    deessAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "deessOn", deessToggle);

    configureKnob (kDeessFreq,   "FREQ",   "deessFreq", true);
    configureKnob (kDeessThresh, "THRESH", "deessThresh", true);
    configureKnob (kDeessRange,  "RANGE",  "deessRange", true);

    // Output
    configureKnob (kOutput, "OUTPUT", "output");

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
    // Update value labels by reading APVTS text representation
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
    if (latencySamples <= 0)
        latencyLabel.setText ("0 ms", juce::dontSendNotification);
    else
        latencyLabel.setText (juce::String (ms, 1) + " ms latency", juce::dontSendNotification);
}

void InstaWidthEditor::paint (juce::Graphics& g)
{
    g.fillAll (InstaWidthLookAndFeel::bgDark);
    lookAndFeel.drawBackgroundTexture (g, getLocalBounds());

    // Header divider
    auto headerBottom = 56;
    g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.5f));
    g.drawHorizontalLine (headerBottom, 0.0f, (float) getWidth());

    // Mode strip divider
    auto modeBottom = headerBottom + 36;
    g.drawHorizontalLine (modeBottom, 0.0f, (float) getWidth());

    // Group backgrounds for the control panel on the right
    g.setColour (InstaWidthLookAndFeel::bgMedium.withAlpha (0.45f));
    for (auto* lbl : { &wLabel, &xLabel, &monoLabel, &sideLabel, &deessLabel })
    {
        auto r = lbl->getBounds().expanded (4, 2);
        r.setHeight (16);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
    }
}

void InstaWidthEditor::resized()
{
    auto area = getLocalBounds();

    // Header
    auto header = area.removeFromTop (56);
    titleLabel.setBounds (header.removeFromLeft (220).withTrimmedLeft (16));
    auto headerRight = header.removeFromRight (200);
    bypassLabel.setBounds (headerRight.removeFromTop (24).withTrimmedRight (60));
    bypassToggle.setBounds (headerRight.removeFromTop (24).withTrimmedLeft (130).withTrimmedRight (10));
    versionLabel.setBounds (header.removeFromRight (60));

    // Mode strip
    auto mode = area.removeFromTop (36);
    mode.removeFromLeft (12);
    modeLabel.setBounds  (mode.removeFromLeft (50));
    modeBox.setBounds    (mode.removeFromLeft (140).reduced (2, 6));
    mode.removeFromLeft (12);
    firLabel.setBounds   (mode.removeFromLeft (32));
    firBox.setBounds     (mode.removeFromLeft (90).reduced (2, 6));
    latencyLabel.setBounds (mode.removeFromLeft (160).reduced (4, 6));

    // Bottom correlation meter
    auto bottom = area.removeFromBottom (40);
    corrMeter.setBounds (bottom.reduced (12, 6));

    area.reduce (12, 12);

    // Left: goniometer
    auto leftSize = std::min (area.getHeight(), area.getWidth() / 2);
    auto leftCol = area.removeFromLeft (leftSize);
    {
        // Square it
        int dim = std::min (leftCol.getWidth(), leftCol.getHeight());
        auto sq = leftCol.withSizeKeepingCentre (dim, dim);
        goniometer.setBounds (sq);
    }
    area.removeFromLeft (12);

    // Right column: control layout
    // Five horizontal rows: Width(3 knobs) / Xover(2 knobs) / Side(Tilt + Mono on + freq) / Deess(toggle + 3 knobs) / Output
    const int rowGap = 8;
    const int captionH = 14;
    const int valueH   = 12;

    auto rightCol = area;
    int totalRows = 5;
    int totalGap  = rowGap * (totalRows - 1);
    int rowH = (rightCol.getHeight() - totalGap) / totalRows;

    auto layoutKnob = [captionH, valueH] (KnobUnit& u, juce::Rectangle<int> cell)
    {
        u.caption.setBounds (cell.removeFromTop (captionH));
        u.value  .setBounds (cell.removeFromBottom (valueH));
        u.knob   .setBounds (cell);
    };

    auto layoutRow = [&] (juce::Rectangle<int> row, juce::Label& titleLbl,
                          std::initializer_list<KnobUnit*> knobs,
                          juce::ToggleButton* toggle = nullptr)
    {
        titleLbl.setBounds (row.removeFromTop (16));
        int n = (int) knobs.size() + (toggle ? 1 : 0);
        int cellW = row.getWidth() / n;
        if (toggle)
        {
            toggle->setBounds (row.removeFromLeft (cellW).reduced (8, row.getHeight() / 4));
        }
        for (auto* k : knobs)
        {
            layoutKnob (*k, row.removeFromLeft (cellW).reduced (4));
        }
    };

    auto rowW   = rightCol.removeFromTop (rowH); layoutRow (rowW, wLabel, { &kWLow, &kWMid, &kWHigh });
    rightCol.removeFromTop (rowGap);
    auto rowX   = rightCol.removeFromTop (rowH); layoutRow (rowX, xLabel, { &kFLow, &kFHigh });
    rightCol.removeFromTop (rowGap);

    // Side processing row: tilt + monomaker(toggle + freq)
    auto rowS = rightCol.removeFromTop (rowH);
    sideLabel.setBounds (rowS.removeFromTop (16));
    {
        int cellW = rowS.getWidth() / 3;
        layoutKnob (kTilt, rowS.removeFromLeft (cellW).reduced (4));
        // Mono section
        auto monoArea = rowS.removeFromLeft (cellW * 2);
        monoLabel.setBounds (monoArea.removeFromTop (14));
        int half = monoArea.getWidth() / 2;
        monoToggle.setBounds (monoArea.removeFromLeft (half).reduced (8, monoArea.getHeight() / 4));
        layoutKnob (kMonoFreq, monoArea.reduced (4));
    }
    rightCol.removeFromTop (rowGap);

    auto rowD = rightCol.removeFromTop (rowH); layoutRow (rowD, deessLabel, { &kDeessFreq, &kDeessThresh, &kDeessRange }, &deessToggle);
    rightCol.removeFromTop (rowGap);

    auto rowO = rightCol.removeFromTop (rowH);
    {
        rowO.removeFromTop (16);
        int cellW = rowO.getWidth() / 3;
        layoutKnob (kOutput, rowO.removeFromLeft (cellW).withSizeKeepingCentre (cellW, rowO.getHeight()).reduced (4));
    }
}

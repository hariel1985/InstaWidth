#include "PluginEditor.h"

namespace
{
    constexpr int kMinW = 1000;
    constexpr int kMinH = 620;
    constexpr int kMaxW = 1800;
    constexpr int kMaxH = 1100;
    constexpr int kDefaultW = 1180;
    constexpr int kDefaultH = 720;
}

InstaWidthEditor::InstaWidthEditor (InstaWidthProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    constrainer.setSizeLimits (kMinW, kMinH, kMaxW, kMaxH);
    setResizable (true, true);
    setConstrainer (&constrainer);
    setSize (kDefaultW, kDefaultH);

    processor.setMeterCallbacks (
        [this] (float l, float r) { goniometer.pushSample (l, r); },
        [this] (float l, float r) { corrMeter.pushSample (l, r); });
    corrMeter.prepare (processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0);

    // Header
    addAndMakeVisible (titleLabel);
    titleLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::accent);

    addAndMakeVisible (versionLabel);
    versionLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    versionLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (bypassLabel);
    bypassLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    bypassLabel.setJustificationType (juce::Justification::centredRight);

    addAndMakeVisible (bypassToggle);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassToggle);

    // Mode strip
    addAndMakeVisible (modeLabel);
    modeLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    modeLabel.setJustificationType (juce::Justification::centredRight);

    modeBox.addItem ("Minimum Phase", 1);
    modeBox.addItem ("Linear Phase",  2);
    addAndMakeVisible (modeBox);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, "mode", modeBox);

    addAndMakeVisible (firLabel);
    firLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    firLabel.setJustificationType (juce::Justification::centredRight);

    firBox.addItemList (juce::StringArray { "512", "1024", "2048", "4096", "8192", "16384" }, 1);
    addAndMakeVisible (firBox);
    firAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, "firQuality", firBox);

    addAndMakeVisible (latencyLabel);
    latencyLabel.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    latencyLabel.setJustificationType (juce::Justification::centredLeft);

    // Section header labels (text + colour set here, font set in resized())
    for (auto* lbl : { &wLabel, &xLabel, &monoLabel, &tiltLabel, &deessLabel })
    {
        lbl->setColour (juce::Label::textColourId, InstaWidthLookAndFeel::accent);
        lbl->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (*lbl);
    }
    wLabel.setText     ("STEREO WIDTH",   juce::dontSendNotification);
    xLabel.setText     ("CROSSOVER",      juce::dontSendNotification);
    monoLabel.setText  ("MONOMAKER",      juce::dontSendNotification);
    tiltLabel.setText  ("SIDE TILT",      juce::dontSendNotification);
    deessLabel.setText ("SIDE DE-ESSER",  juce::dontSendNotification);

    // Knobs
    configureKnob (kWLow,  "LOW",  "wLow");
    configureKnob (kWMid,  "MID",  "wMid");
    configureKnob (kWHigh, "HIGH", "wHigh");
    configureKnob (kFLow,  "L-M",  "fLow",  true);
    configureKnob (kFHigh, "M-H",  "fHigh", true);
    configureKnob (kTilt,        "TILT",   "tilt");
    configureKnob (kMonoFreq,    "FREQ",   "monoFreq", true);
    configureKnob (kDeessFreq,   "FREQ",   "deessFreq", true);
    configureKnob (kDeessThresh, "THRESH", "deessThresh", true);
    configureKnob (kDeessRange,  "RANGE",  "deessRange", true);
    configureKnob (kOutput,      "OUTPUT", "output");

    addAndMakeVisible (monoToggle);
    monoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "monoOn", monoToggle);

    addAndMakeVisible (deessToggle);
    deessAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "deessOn", deessToggle);

    addAndMakeVisible (goniometer);
    addAndMakeVisible (corrMeter);

    installTooltips();

    startTimerHz (15);
}

void InstaWidthEditor::installTooltips()
{
    bypassToggle.setTooltip ("Bypass the whole plugin -- input passes through unprocessed.");

    modeBox.setTooltip (
        "Processing mode.\n"
        "- Minimum Phase: IIR-based. Low CPU, zero latency, classic character.\n"
        "  Best for tracking and live monitoring.\n"
        "- Linear Phase: symmetric FIR convolution. Zero phase distortion but\n"
        "  adds latency (the DAW compensates automatically). Best for mastering\n"
        "  and parallel buses.");

    firBox.setTooltip (
        "FIR length (Linear Phase mode only).\n"
        "More taps = sharper crossover transitions, but more latency.\n"
        "512 (~6 ms)  -  2048 (~23 ms)  -  8192 (~93 ms)  -  16384 (~186 ms).");

    kWLow.knob.setTooltip (
        "LOW Stereo Width -- width multiplier for the bass band (below the L-M crossover).\n"
        "0% = full mono   100% = unchanged   200% = doubled side signal.\n"
        "Keep low bass close to 100%, or use Monomaker for mono safety -- bass\n"
        "summed out of phase will cancel on mono playback systems.");
    kWMid.knob.setTooltip (
        "MID Stereo Width -- width in the mid band (between the two crossovers).\n"
        "This is where vocals, kick body and snare usually sit -- handle with care.\n"
        "0% = mono, 100% = unchanged, 200% = doubled side.");
    kWHigh.knob.setTooltip (
        "HIGH Stereo Width -- width in the high band (above the M-H crossover).\n"
        "Air, cymbals, presence -- generally safe to widen aggressively.\n"
        "0% = mono, 100% = unchanged, 200% = doubled side.");

    kFLow.knob.setTooltip (
        "Low / Mid crossover frequency.\n"
        "Linkwitz-Riley 24 dB/oct slope.\n"
        "Typically set just above the kick fundamental (80-200 Hz).");
    kFHigh.knob.setTooltip (
        "Mid / High crossover frequency.\n"
        "Linkwitz-Riley 24 dB/oct.\n"
        "Above the main body (vocal, snare), below the presence/air region (1-4 kHz).");

    kTilt.knob.setTooltip (
        "Side Tilt EQ -- spectral tilt on the SIDE channel only.\n"
        "Negative = darker, more focused stereo image (more side bass, less side highs).\n"
        "Positive = airier, breathier image (more side highs, less side bass).\n"
        "Pivot at 1 kHz, +/- 6 dB at the ends.\n"
        "The MID (center) channel is left completely untouched.");

    monoToggle.setTooltip (
        "Monomaker on/off.\n"
        "When on, the side channel is high-pass filtered below the cutoff,\n"
        "making everything mono in the low end. Essential safety against\n"
        "sub-bass cancellation on mono playback (clubs, mobile speakers, radio).");
    kMonoFreq.knob.setTooltip (
        "Monomaker cutoff frequency.\n"
        "Below this, the side signal is suppressed -> full mono.\n"
        "Default 120 Hz, range 20-500 Hz.\n"
        "Higher cutoff = safer mono compatibility, less stereo air down low.");

    deessToggle.setTooltip (
        "Side de-esser on/off.\n"
        "Dynamic gain reduction applied to the SIDE channel only,\n"
        "in the detector's frequency band.\n"
        "The MID (center) channel is completely unaffected -- vocals do not lisp.");
    kDeessFreq.knob.setTooltip (
        "De-esser detector center frequency.\n"
        "Tune this to the sibilance / cymbal harshness on the side --\n"
        "typically 5-8 kHz.");
    kDeessThresh.knob.setTooltip (
        "De-esser threshold.\n"
        "Above this level the side gets attenuated.\n"
        "More negative = more sensitive, triggers sooner.");
    kDeessRange.knob.setTooltip (
        "Maximum gain reduction on the side channel.\n"
        "0 dB = no effect, 24 dB = very heavy duck when it triggers.");

    kOutput.knob.setTooltip ("Output gain -- final stage after all processing, +/- 24 dB.");

    goniometer.setTooltip (
        "Stereo goniometer (Lissajous display).\n"
        "L/R rotated by 45 deg -- vertical axis = MID (mono), horizontal axis = SIDE.\n"
        "Narrow, vertical pattern = mono-compatible.\n"
        "Horizontal line = full anti-phase (bad).\n"
        "Round fluffy cloud = healthy stereo.");
    corrMeter.setTooltip (
        "Pearson correlation between L and R, per band.\n"
        "LOW / MID / HIGH = correlation inside that frequency range.\n"
        "OVERALL = full-spectrum correlation.\n"
        "+1 = mono, 0 = independent, -1 = full anti-phase.\n"
        "The red warning only fires when a band stays below its threshold for ~330 ms\n"
        "(LOW threshold -0.05, MID -0.30, HIGH -0.45). Modern commercial masters\n"
        "often sit slightly negative in the MID/HIGH bands -- that is normal and\n"
        "intentional for stereo width.");
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

    u.caption.setColour (juce::Label::textColourId, InstaWidthLookAndFeel::textSecondary);
    u.caption.setJustificationType (juce::Justification::centred);
    u.caption.setText (caption, juce::dontSendNotification);
    addAndMakeVisible (u.caption);

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
    setText (kMonoFreq,    "monoFreq");
    setText (kDeessFreq,   "deessFreq");
    setText (kDeessThresh, "deessThresh");
    setText (kDeessRange,  "deessRange");
    setText (kOutput,      "output");

    const int latencySamples = processor.getCurrentLatency();
    const double sr = processor.getSampleRate();
    const double ms = (sr > 0.0) ? 1000.0 * latencySamples / sr : 0.0;
    latencyLabel.setText (latencySamples <= 0
                              ? juce::String ("0 ms")
                              : juce::String (ms, 1) + " ms latency",
                          juce::dontSendNotification);

    // Keep the correlation meter's per-band split aligned with the user's crossover settings
    if (auto* fl = processor.apvts.getRawParameterValue ("fLow"))
        if (auto* fh = processor.apvts.getRawParameterValue ("fHigh"))
            corrMeter.setCrossovers (fl->load(), fh->load());
}

void InstaWidthEditor::paint (juce::Graphics& g)
{
    g.fillAll (InstaWidthLookAndFeel::bgDark);
    lookAndFeel.drawBackgroundTexture (g, getLocalBounds());

    g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.5f));
    g.drawHorizontalLine (56, 0.0f, (float) getWidth());
    g.drawHorizontalLine (92, 0.0f, (float) getWidth());

    auto drawSection = [&] (juce::Rectangle<int> r)
    {
        if (r.isEmpty()) return;
        auto fr = r.toFloat();
        g.setColour (InstaWidthLookAndFeel::bgMedium.withAlpha (0.55f));
        g.fillRoundedRectangle (fr, 6.0f);
        g.setColour (InstaWidthLookAndFeel::bgLight.withAlpha (0.7f));
        g.drawRoundedRectangle (fr, 6.0f, 1.0f);
    };

    drawSection (rWidth);
    drawSection (rXover);
    drawSection (rTilt);
    drawSection (rMono);
    drawSection (rDeess);
    drawSection (rOutput);
}

void InstaWidthEditor::resized()
{
    auto area = getLocalBounds();

    // Window-relative scale: lets every label/knob grow when the window grows in either dimension.
    const float scale = juce::jlimit (1.0f, 1.8f,
                                      std::min ((float) getWidth()  / (float) kDefaultW,
                                                (float) getHeight() / (float) kDefaultH));

    const int targetKnobSize = juce::jlimit (70, 150, (int) (90.0f * scale));
    const int captionH       = juce::jlimit (14, 22,  (int) (16.0f * scale));
    const int valueH         = juce::jlimit (14, 22,  (int) (16.0f * scale));
    const int sectionTitleH  = juce::jlimit (18, 28,  (int) (20.0f * scale));

    const float captionFontH = juce::jlimit (10.5f, 16.0f, 11.0f * scale);
    const float valueFontH   = juce::jlimit (10.5f, 16.0f, 11.0f * scale);
    const float sectionTitleFontH = juce::jlimit (11.0f, 16.0f, 11.5f * scale);
    const float titleFontH   = juce::jlimit (22.0f, 32.0f, 24.0f * scale);

    titleLabel.setFont   (lookAndFeel.getBoldFont    (titleFontH));
    versionLabel.setFont (lookAndFeel.getRegularFont (juce::jlimit (11.0f, 14.0f, 12.0f * scale)));
    bypassLabel.setFont  (lookAndFeel.getMediumFont  (juce::jlimit (10.5f, 14.0f, 11.0f * scale)));
    modeLabel.setFont    (lookAndFeel.getMediumFont  (juce::jlimit (10.5f, 14.0f, 11.0f * scale)));
    firLabel.setFont     (lookAndFeel.getMediumFont  (juce::jlimit (10.5f, 14.0f, 11.0f * scale)));
    latencyLabel.setFont (lookAndFeel.getRegularFont (juce::jlimit (10.5f, 14.0f, 11.0f * scale)));

    for (auto* lbl : { &wLabel, &xLabel, &monoLabel, &tiltLabel, &deessLabel })
        lbl->setFont (lookAndFeel.getBoldFont (sectionTitleFontH));

    for (auto* u : { &kWLow, &kWMid, &kWHigh, &kFLow, &kFHigh, &kTilt, &kMonoFreq,
                     &kDeessFreq, &kDeessThresh, &kDeessRange, &kOutput })
    {
        u->caption.setFont (lookAndFeel.getMediumFont (captionFontH));
        u->value  .setFont (lookAndFeel.getRegularFont (valueFontH));
    }

    // ---- Header (56 px)
    auto header = area.removeFromTop (56);
    titleLabel.setBounds (header.removeFromLeft (260).withTrimmedLeft (16));
    auto headerRight = header.removeFromRight (240);
    bypassToggle.setBounds (headerRight.removeFromRight (70).reduced (10, 16));
    bypassLabel .setBounds (headerRight.removeFromRight (80).reduced (4, 18));
    versionLabel.setBounds (headerRight.reduced (8, 18));

    // ---- Mode strip (36 px)
    auto mode = area.removeFromTop (36);
    mode.removeFromLeft (16);
    modeLabel.setBounds (mode.removeFromLeft (50));
    modeBox  .setBounds (mode.removeFromLeft (160).reduced (2, 6));
    mode.removeFromLeft (16);
    firLabel .setBounds (mode.removeFromLeft (32));
    firBox   .setBounds (mode.removeFromLeft (110).reduced (2, 6));
    latencyLabel.setBounds (mode.removeFromLeft (260).reduced (8, 6));

    // ---- Correlation strip at the very bottom (4 stacked sub-bars: Low/Mid/High/Overall)
    auto bottom = area.removeFromBottom (juce::jlimit (78, 110, (int) (88.0f * scale)));
    corrMeter.setBounds (bottom.reduced (14, 6));

    area.reduce (12, 12);

    // ---- Split main area into top (gonio + width/xover/tilt) and bottom (mono/deess/output)
    const int gap = 12;
    const int topRowH    = area.getHeight() * 58 / 100;
    auto topRow    = area.removeFromTop (topRowH);
    area.removeFromTop (gap);
    auto bottomRow = area;

    // ---- TOP ROW: goniometer on the left (square), three sections stacked on the right
    {
        // Goniometer height-limited to topRowH; width = topRowH so it's square
        int gonioDim = std::min (topRow.getHeight(), topRow.getWidth() / 2);
        auto gonioCell = topRow.removeFromLeft (gonioDim);
        goniometer.setBounds (gonioCell.withSizeKeepingCentre (gonioDim, gonioDim));
        topRow.removeFromLeft (gap);

        // Right side: 2 stacked sections in this top row
        //   Row A: STEREO WIDTH (3 knobs)
        //   Row B: CROSSOVER (2 knobs) + SIDE TILT (1 knob)
        const int subRowGap = 10;
        int subRowH = (topRow.getHeight() - subRowGap) / 2;

        rWidth = topRow.removeFromTop (subRowH);
        topRow.removeFromTop (subRowGap);
        auto rowB = topRow.removeFromTop (subRowH);

        // STEREO WIDTH
        {
            auto body = rWidth.reduced (12, 8);
            wLabel.setBounds (body.removeFromTop (sectionTitleH));
            int cellW = body.getWidth() / 3;
            auto layoutKnob = [&] (KnobUnit& u, juce::Rectangle<int> cell)
            {
                u.caption.setBounds (cell.removeFromTop (captionH));
                u.value  .setBounds (cell.removeFromBottom (valueH));
                int side = std::min ({ cell.getWidth(), cell.getHeight(), targetKnobSize });
                u.knob.setBounds (cell.withSizeKeepingCentre (side, side));
            };
            layoutKnob (kWLow,  body.removeFromLeft (cellW));
            layoutKnob (kWMid,  body.removeFromLeft (cellW));
            layoutKnob (kWHigh, body);
        }

        // CROSSOVER (2/3) + SIDE TILT (1/3)
        {
            rXover = rowB.removeFromLeft (rowB.getWidth() * 2 / 3);
            rXover.removeFromRight (subRowGap / 2);
            rTilt  = rowB;
            rTilt.removeFromLeft (subRowGap / 2);

            auto layoutKnob = [&] (KnobUnit& u, juce::Rectangle<int> cell)
            {
                u.caption.setBounds (cell.removeFromTop (captionH));
                u.value  .setBounds (cell.removeFromBottom (valueH));
                int side = std::min ({ cell.getWidth(), cell.getHeight(), targetKnobSize });
                u.knob.setBounds (cell.withSizeKeepingCentre (side, side));
            };

            {
                auto body = rXover.reduced (12, 8);
                xLabel.setBounds (body.removeFromTop (sectionTitleH));
                int cellW = body.getWidth() / 2;
                layoutKnob (kFLow,  body.removeFromLeft (cellW));
                layoutKnob (kFHigh, body);
            }
            {
                auto body = rTilt.reduced (12, 8);
                tiltLabel.setBounds (body.removeFromTop (sectionTitleH));
                layoutKnob (kTilt, body);
            }
        }
    }

    // ---- BOTTOM ROW: 3 horizontal sections — MONOMAKER, SIDE DE-ESSER, OUTPUT
    {
        // Allocations: Monomaker 22%, De-esser 56%, Output 22%
        int totalW = bottomRow.getWidth();
        int monoW   = totalW * 22 / 100;
        int outputW = totalW * 22 / 100;
        int deessW  = totalW - monoW - outputW - 2 * gap;

        rMono = bottomRow.removeFromLeft (monoW);
        bottomRow.removeFromLeft (gap);
        rDeess = bottomRow.removeFromLeft (deessW);
        bottomRow.removeFromLeft (gap);
        rOutput = bottomRow;

        auto layoutKnobBig = [&] (KnobUnit& u, juce::Rectangle<int> cell)
        {
            u.caption.setBounds (cell.removeFromTop (captionH));
            u.value  .setBounds (cell.removeFromBottom (valueH));
            int side = std::min ({ cell.getWidth(), cell.getHeight(), targetKnobSize });
            u.knob.setBounds (cell.withSizeKeepingCentre (side, side));
        };

        // MONOMAKER — toggle + freq knob
        {
            auto body = rMono.reduced (12, 8);
            monoLabel.setBounds (body.removeFromTop (sectionTitleH));
            int half = body.getWidth() / 2;
            auto togCell = body.removeFromLeft (half);
            int togH = juce::jlimit (26, 40, (int) (30.0f * scale));
            int togW = juce::jlimit (52, 80, (int) (60.0f * scale));
            monoToggle.setBounds (togCell.withSizeKeepingCentre (togW, togH));
            layoutKnobBig (kMonoFreq, body);
        }

        // SIDE DE-ESSER — toggle + 3 knobs
        {
            auto body = rDeess.reduced (12, 8);
            deessLabel.setBounds (body.removeFromTop (sectionTitleH));
            int cellW = body.getWidth() / 4;
            auto togCell = body.removeFromLeft (cellW);
            int togH = juce::jlimit (26, 40, (int) (30.0f * scale));
            int togW = juce::jlimit (52, 80, (int) (60.0f * scale));
            deessToggle.setBounds (togCell.withSizeKeepingCentre (togW, togH));
            layoutKnobBig (kDeessFreq,   body.removeFromLeft (cellW));
            layoutKnobBig (kDeessThresh, body.removeFromLeft (cellW));
            layoutKnobBig (kDeessRange,  body);
        }

        // OUTPUT
        {
            auto body = rOutput.reduced (12, 8);
            // No section title; let the OUTPUT caption serve.
            layoutKnobBig (kOutput, body);
        }
    }
}

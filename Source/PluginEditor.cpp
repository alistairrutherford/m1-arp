#include "PluginEditor.h"

using namespace m1ui;

namespace
{
constexpr int kPages = 10;
const char* kPageNames[kPages] = {
    "Play", "Osc", "Pitch", "Filter", "Amp", "Mod", "Arp", "FX", "Combi", "Kit"
};
const char* kRates[10] = { "1/1", "1/2", "1/2T", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
const juce::StringArray kArpModes { "Up", "Down", "Up/Down", "Down/Up", "Random", "As Played", "Chord", "Pattern" };
const juce::StringArray kWaves { "Triangle", "Saw Up", "Saw Down", "Square" };
const juce::StringArray kPolarity { "-", "0", "+" };
const juce::StringArray kOscSelect { "Off", "OSC 1", "OSC 2", "Both" };
const juce::StringArray kCombiTypes { "Single", "Layer", "Split", "Velocity", "Multi" };
const juce::StringArray kModes { "Single", "Double", "Drums" };

juce::StringArray multisoundNames()
{
    juce::StringArray s;
    for (int i = 0; i < m1::kMultisounds; ++i)
        s.add (juce::String (i).paddedLeft ('0', 2) + "  " + m1::multisoundRole (i));
    return s;
}

juce::StringArray drumNames()
{
    juce::StringArray s;
    for (int i = 0; i < m1::kDrumSamples; ++i)
        s.add (juce::String (i).paddedLeft ('0', 2) + "  " + m1::synth::drumName (i));
    return s;
}

juce::StringArray effectNames()
{
    juce::StringArray s;
    for (int i = 0; i <= 33; ++i)
        s.add (juce::String (i).paddedLeft ('0', 2) + "  " + m1::effectName (i));
    return s;
}

juce::String balanceText (int b)
{
    b = juce::jlimit (0, 98, b);
    return juce::String (99 - b) + ":" + juce::String (b + 1);
}

juce::String timeTip() { return "0-99: 2 ms to 5 s, exponential"; }
}

// ===================================================================== Display

M1ArpAudioProcessorEditor::Display::Display()
{
    name.setFont (font (24.0f, true));
    name.setColour (juce::Label::textColourId, colours::lcdText);
    name.setColour (juce::Label::backgroundWhenEditingColourId, colours::lcd.brighter (0.15f));
    name.setColour (juce::Label::textWhenEditingColourId, colours::lcdText);
    name.setColour (juce::Label::outlineWhenEditingColourId, colours::lcdDim);
    name.setEditable (false, true, false);
    name.setTooltip ("Double-click to rename. Click the arrow to browse.");
    addAndMakeVisible (name);
}

void M1ArpAudioProcessorEditor::Display::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 7.0f);
    auto glass = r.reduced (3.0f);
    juce::ColourGradient grad (colours::lcd.brighter (0.12f), glass.getX(), glass.getY(),
                               colours::lcd.darker (0.3f), glass.getX(), glass.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (glass, 5.0f);
    // faint scanlines
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    for (float y = glass.getY() + 2.0f; y < glass.getBottom(); y += 3.0f)
        g.drawHorizontalLine ((int) y, glass.getX() + 2.0f, glass.getRight() - 2.0f);

    auto inner = glass.reduced (12.0f, 6.0f);
    g.setFont (font (11.5f, true));
    g.setColour (colours::lcdDim);
    g.drawText (topLine, inner.removeFromTop (14.0f), juce::Justification::centredLeft);
    g.drawText (rightTop, glass.reduced (12.0f, 6.0f).removeFromTop (14.0f).withTrimmedRight (22.0f), juce::Justification::centredRight);
    g.setColour (loading ? colours::accent : colours::lcdDim.brighter (0.3f));
    g.setFont (font (11.5f));
    g.drawText (loading ? "Building multisamples..." : infoLine, glass.reduced (12.0f, 6.0f).removeFromBottom (14.0f),
                juce::Justification::centredLeft);

    // browse arrow
    juce::Path arrow;
    const float ax = glass.getRight() - 16.0f, ay = glass.getCentreY();
    arrow.addTriangle (ax - 5.0f, ay - 3.0f, ax + 5.0f, ay - 3.0f, ax, ay + 4.0f);
    g.setColour (colours::lcdText.withAlpha (0.8f));
    g.fillPath (arrow);
}

void M1ArpAudioProcessorEditor::Display::resized()
{
    name.setBounds (getLocalBounds().reduced (10, 0).withTrimmedRight (26).withSizeKeepingCentre (getWidth() - 46, 30));
}

void M1ArpAudioProcessorEditor::Display::mouseUp (const juce::MouseEvent& e)
{
    if (! name.isBeingEdited() && onBrowse && (e.x > getWidth() - 34 || e.getNumberOfClicks() == 1))
        onBrowse();
}

// ======================================================================= Meter

void M1ArpAudioProcessorEditor::Meter::push (float peak)
{
    level = juce::jmax (peak, level * 0.82f);
    held = juce::jmax (peak, held - 0.01f);
    repaint();
}

void M1ArpAudioProcessorEditor::Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, 3.0f);
    auto toY = [&] (float lin)
    {
        const float db = juce::Decibels::gainToDecibels (lin, -48.0f);
        return juce::jmap (db, -48.0f, 0.0f, r.getBottom() - 2.0f, r.getY() + 2.0f);
    };
    const float y = toY (level);
    auto bar = r.reduced (3.0f, 2.0f).withTop (y);
    juce::ColourGradient grad (juce::Colour (0xffe0503a), 0.0f, r.getY(), colours::accent2, 0.0f, r.getBottom(), false);
    grad.addColour (0.25, colours::accent);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (colours::text.withAlpha (0.8f));
    g.fillRect (r.getX() + 3.0f, toY (held), r.getWidth() - 6.0f, 1.5f);
}

// ====================================================================== Editor

M1ArpAudioProcessorEditor::M1ArpAudioProcessorEditor (M1ArpAudioProcessor& p)
    : juce::AudioProcessorEditor (p),
      processor (p),
      keyboard (p.keyboardState, p.clickState)
{
    setLookAndFeel (&look);

    display.onBrowse = [this] { browse(); };
    display.name.onTextChange = [this] { commitName(); };
    addAndMakeVisible (display);

    prevButton.onClick = [this] { step (-1); };
    nextButton.onClick = [this] { step (1); };
    prevButton.setTooltip ("Previous");
    nextButton.setTooltip ("Next");
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    progButton.onClick = [this] { if (processor.isCombiMode()) processor.setCombiMode (false); };
    combiButton.onClick = [this] { if (! processor.isCombiMode()) processor.setCombiMode (true); };
    progButton.setTooltip ("Play single programs");
    combiButton.setTooltip ("Play combinations (layers, splits, multi)");
    addAndMakeVisible (progButton);
    addAndMakeVisible (combiButton);

    saveButton.onClick = [this] { chooseFile (true); };
    loadButton.onClick = [this] { chooseFile (false); };
    saveButton.setTooltip ("Save every program, combination and kit to a .m1arp file");
    loadButton.setTooltip ("Load a .m1arp file");
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);

    master.bindParam (processor.apvts, "master", [] (double v) { return juce::String (v, 1) + " dB"; });
    master.slider.setTooltip ("Output level. A soft limiter keeps the output under 0 dBFS.");
    addAndMakeVisible (master);
    addAndMakeVisible (meter);

    for (int i = 0; i < kPages; ++i)
    {
        tabs[i].setButtonText (kPageNames[i]);
        tabs[i].setRadioGroupId (1001);
        tabs[i].setClickingTogglesState (true);
        tabs[i].setToggleState (i == pageIndex, juce::dontSendNotification);
        tabs[i].onClick = [this, i]
        {
            if (pageIndex != i)
            {
                pageIndex = i;
                requestRebuild();
            }
        };
        addAndMakeVisible (tabs[i]);
    }

    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    keyboard.setAvailableRange (21, 108);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe9e6df));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1c1e22));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff9a9790));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::accent.withAlpha (0.85f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::accent.withAlpha (0.25f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.4f));
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, colours::panel);
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (960, 640, 1700, 1200);
    setSize (1080, 730);
    refreshHeader();
    startTimerHz (30);
}

M1ArpAudioProcessorEditor::~M1ArpAudioProcessorEditor()
{
    stopTimer();
    viewport.setViewedComponent (nullptr, true);
    setLookAndFeel (nullptr);
}

void M1ArpAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    auto header = getLocalBounds().removeFromTop (88).toFloat();
    juce::ColourGradient grad (juce::Colour (0xff23262c), 0.0f, 0.0f, juce::Colour (0xff17191d), 0.0f, header.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (header);
    g.setColour (colours::panelEdge);
    g.drawHorizontalLine ((int) header.getBottom() - 1, 0.0f, (float) getWidth());

    // Wordmark
    g.setColour (colours::text);
    g.setFont (font (34.0f, true));
    g.drawText ("M1", 18, 14, 60, 40, juce::Justification::centredLeft);
    g.setColour (colours::accent);
    g.drawText ("ARP", 70, 14, 90, 40, juce::Justification::centredLeft);
    g.setColour (colours::dim);
    g.setFont (font (10.5f, true));
    g.drawText ("PCM SYNTH  /  ARPEGGIATOR", 20, 52, 200, 14, juce::Justification::centredLeft);

    // Keyboard bed
    auto kb = keyboard.getBounds().expanded (8, 6).toFloat();
    g.setColour (juce::Colour (0xff101215));
    g.fillRoundedRectangle (kb, 6.0f);

    // Labels for header groups
    g.setFont (font (10.0f, true));
    g.setColour (colours::dim);
    g.drawText ("MODE", progButton.getX(), progButton.getY() - 15, 100, 12, juce::Justification::centredLeft);
    g.drawText ("PRESET FILE", saveButton.getX(), saveButton.getY() - 15, 100, 12, juce::Justification::centredLeft);
}

void M1ArpAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (88).reduced (14, 12);
    header.removeFromLeft (200);

    auto right = header.removeFromRight (120);
    meter.setBounds (right.removeFromRight (12).reduced (0, 6));
    right.removeFromRight (4);
    master.setBounds (right.removeFromRight (70).withTrimmedTop (-4));

    auto files = header.removeFromRight (150).withTrimmedTop (18).withTrimmedBottom (10);
    saveButton.setBounds (files.removeFromLeft (70));
    files.removeFromLeft (8);
    loadButton.setBounds (files.removeFromLeft (70));
    header.removeFromRight (16);

    auto modes = header.removeFromRight (150).withTrimmedTop (18).withTrimmedBottom (10);
    progButton.setBounds (modes.removeFromLeft (73));
    modes.removeFromLeft (4);
    combiButton.setBounds (modes.removeFromLeft (73));
    header.removeFromRight (16);

    prevButton.setBounds (header.removeFromLeft (30).reduced (0, 14));
    header.removeFromLeft (6);
    nextButton.setBounds (header.removeFromRight (30).reduced (0, 14));
    header.removeFromRight (6);
    display.setBounds (header);

    auto tabRow = area.removeFromTop (40).reduced (14, 6);
    const int w = tabRow.getWidth() / kPages;
    for (int i = 0; i < kPages; ++i)
        tabs[i].setBounds (tabRow.removeFromLeft (i == kPages - 1 ? tabRow.getWidth() : w).reduced (3, 0));

    auto kb = area.removeFromBottom (96).reduced (22, 14);
    keyboard.setBounds (kb);
    const int whiteKeys = 52;
    keyboard.setKeyWidth ((float) kb.getWidth() / (float) whiteKeys);

    viewport.setBounds (area.reduced (14, 4));
    if (page != nullptr)
        page->layout (viewport.getWidth() - viewport.getScrollBarThickness() - 2);
}

void M1ArpAudioProcessorEditor::timerCallback()
{
    if (processor.structureRevision() != shownRevision)
        refreshHeader();
    const bool loading = ! processor.bankReady();
    if (loading != shownLoading)
    {
        shownLoading = loading;
        refreshDisplay();
    }
    meter.push (processor.outputPeak());
    if (page != nullptr)
        for (auto* t : page->paramToggles)
            t->refreshFromParam();
}

m1::Program* M1ArpAudioProcessorEditor::currentProgram()
{
    auto& s = processor.session();
    if (s.numPrograms <= 0)
        return nullptr;
    if (processor.isCombiMode() && s.numCombis > 0)
    {
        const int index = s.combis[s.currentCombi].timbre[0].program;
        return &s.programs[m1::clampInt (index, 0, s.numPrograms - 1)];
    }
    return &s.programs[m1::clampInt (s.currentProgram, 0, s.numPrograms - 1)];
}

m1::Combination* M1ArpAudioProcessorEditor::currentCombi()
{
    auto& s = processor.session();
    if (s.numCombis <= 0)
        return nullptr;
    return &s.combis[m1::clampInt (s.currentCombi, 0, s.numCombis - 1)];
}

m1::DrumKit* M1ArpAudioProcessorEditor::currentKit()
{
    auto* p = currentProgram();
    if (p == nullptr)
        return nullptr;
    return &processor.session().kits[m1::clampInt (p->kit, 0, m1::kKits - 1)];
}

void M1ArpAudioProcessorEditor::refreshHeader()
{
    shownRevision = processor.structureRevision();
    progButton.setToggleState (! processor.isCombiMode(), juce::dontSendNotification);
    combiButton.setToggleState (processor.isCombiMode(), juce::dontSendNotification);
    refreshDisplay();
    rebuild();
}

void M1ArpAudioProcessorEditor::refreshDisplay()
{
    const auto& s = processor.session();
    display.loading = ! processor.bankReady();
    juce::String name;
    if (processor.isCombiMode() && s.numCombis > 0)
    {
        const auto& c = s.combis[s.currentCombi];
        display.topLine = "COMBI " + juce::String (s.currentCombi).paddedLeft ('0', 2);
        display.rightTop = kCombiTypes[m1::clampInt (c.type, 0, 4)].toUpperCase();
        name = c.name;
        juce::String info = "FX1 " + juce::String (m1::effectName (c.fx1.type)) + "   FX2 "
                            + m1::effectName (m1::resolvedPartner (c.fx1.type, c.fx2.type));
        if (c.arp.on && c.type != (int) m1::CombiType::Multi)
            info << "   ARP " << kArpModes[m1::clampInt (c.arp.mode, 0, 7)] << " " << kRates[m1::clampInt (c.arp.rate, 0, 9)];
        display.infoLine = info;
    }
    else if (s.numPrograms > 0)
    {
        const auto& p = s.programs[s.currentProgram];
        display.topLine = "PROGRAM " + juce::String (s.currentProgram).paddedLeft ('0', 2);
        juce::String mode = kModes[m1::clampInt (p.mode, 0, 2)].toUpperCase();
        if (p.mode != (int) m1::OscMode::Drums)
            mode << "  " << (p.assign == (int) m1::Assign::Mono ? "MONO" : "POLY");
        display.rightTop = mode;
        name = p.name;
        juce::String info;
        if (p.mode == (int) m1::OscMode::Drums)
            info << "KIT " << s.kits[m1::clampInt (p.kit, 0, m1::kKits - 1)].name;
        else
        {
            info << m1::multisoundRole (p.osc1.multisound);
            if (p.mode == (int) m1::OscMode::Double)
                info << " + " << m1::multisoundRole (p.osc2.multisound);
        }
        info << "   FX " << m1::effectName (p.fx1.type);
        const int fx2 = m1::resolvedPartner (p.fx1.type, p.fx2.type);
        if (fx2 != 0)
            info << " > " << m1::effectName (fx2);
        const bool arpOn = processor.apvts.getRawParameterValue ("arp_on")->load() > 0.5f;
        if (arpOn)
            info << "   ARP " << kArpModes[m1::clampInt (p.arp.mode, 0, 7)];
        display.infoLine = info;
    }
    if (! display.name.isBeingEdited())
        display.name.setText (name, juce::dontSendNotification);
    display.repaint();
}

void M1ArpAudioProcessorEditor::step (int delta)
{
    const auto& s = processor.session();
    if (processor.isCombiMode())
    {
        if (s.numCombis > 0)
            processor.setCurrentCombi ((s.currentCombi + delta + s.numCombis) % s.numCombis);
    }
    else if (s.numPrograms > 0)
        processor.setCurrentProgram ((s.currentProgram + delta + s.numPrograms) % s.numPrograms);
}

void M1ArpAudioProcessorEditor::browse()
{
    const auto& s = processor.session();
    juce::PopupMenu menu;
    const bool combi = processor.isCombiMode();
    const int n = combi ? s.numCombis : s.numPrograms;
    const int current = combi ? s.currentCombi : s.currentProgram;
    menu.addSectionHeader (combi ? "Combinations" : "Programs");
    for (int i = 0; i < n; ++i)
        menu.addItem (i + 1, juce::String (i).paddedLeft ('0', 2) + "   " + (combi ? s.combis[i].name : s.programs[i].name),
                      true, i == current);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&display).withMinimumWidth (display.getWidth())
                            .withMaximumNumColumns (2),
                        [safe = juce::Component::SafePointer<M1ArpAudioProcessorEditor> (this), combi] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;
                            if (combi)
                                safe->processor.setCurrentCombi (result - 1);
                            else
                                safe->processor.setCurrentProgram (result - 1);
                        });
}

void M1ArpAudioProcessorEditor::commitName()
{
    const auto text = display.name.getText().substring (0, m1::kNameLen - 1);
    if (processor.isCombiMode())
    {
        if (auto* c = currentCombi())
            m1::copyName (c->name, text.toRawUTF8());
        processor.publishSession();
    }
    else if (processor.getNumPrograms() > 0)
        processor.changeProgramName (processor.getCurrentProgram(), text);
    refreshDisplay();
}

void M1ArpAudioProcessorEditor::chooseFile (bool save)
{
    const auto flags = save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
                            : juce::FileBrowserComponent::openMode;
    const auto dir = m1::BankCache::presetDirectory();
    dir.createDirectory();
    chooser = std::make_unique<juce::FileChooser> (save ? "Save preset" : "Load preset", dir, "*.m1arp");
    chooser->launchAsync (flags | juce::FileBrowserComponent::canSelectFiles,
                          [this, save] (const juce::FileChooser& box)
                          {
                              const auto file = box.getResult();
                              if (file == juce::File())
                                  return;
                              auto target = file;
                              if (save && ! target.hasFileExtension ("m1arp"))
                                  target = target.withFileExtension ("m1arp");
                              if (save)
                                  processor.savePreset (target);
                              else
                                  processor.loadPreset (target);
                          });
}

void M1ArpAudioProcessorEditor::edited()
{
    processor.publishSession();
    if (page != nullptr)
        page->repaintGraphs();
    refreshDisplay();
}

void M1ArpAudioProcessorEditor::requestRebuild()
{
    if (rebuildPending)
        return;
    rebuildPending = true;
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<M1ArpAudioProcessorEditor> (this)]
    {
        if (safe != nullptr)
        {
            safe->rebuildPending = false;
            safe->rebuild();
        }
    });
}

void M1ArpAudioProcessorEditor::rebuild()
{
    for (int i = 0; i < kPages; ++i)
        tabs[i].setToggleState (i == pageIndex, juce::dontSendNotification);

    const auto scroll = viewport.getViewPosition();
    auto* newPage = new Page();
    auto* program = currentProgram();
    switch (pageIndex)
    {
        case 0: buildPlay (*newPage); break;
        case 1: if (program) buildOsc (*newPage, *program); break;
        case 2: if (program) buildPitch (*newPage, *program); break;
        case 3: if (program) buildFilter (*newPage, *program); break;
        case 4: if (program) buildAmp (*newPage, *program); break;
        case 5: if (program) buildMod (*newPage, *program); break;
        case 6: if (program) buildArp (*newPage, *program); break;
        case 7: if (program) buildFx (*newPage, *program); break;
        case 8: buildCombi (*newPage); break;
        case 9: buildKit (*newPage); break;
        default: break;
    }
    const int width = juce::jmax (400, viewport.getWidth() - viewport.getScrollBarThickness() - 2);
    newPage->layout (width);
    page = newPage;
    viewport.setViewedComponent (newPage, true);
    viewport.setViewPosition (scroll);
}

// ======================================================================= Pages

void M1ArpAudioProcessorEditor::buildPlay (Page& pg)
{
    auto& apvts = processor.apvts;
    auto& perf = pg.add (new Section ("Performance", 8));
    const char* ids[8] = { "play_osc_balance", "play_cutoff", "play_level", "play_kbd",
                           "play_velocity", "play_attack", "play_release", "play_effect" };
    const char* names[8] = { "Osc Bal", "Cutoff", "Level", "Kbd Track", "Velocity", "Attack", "Release", "Effect" };
    for (int i = 0; i < 8; ++i)
    {
        auto& k = perf.add (new Knob (names[i], "Temporary offset (-10..+10). Cleared on program change."));
        k.bindParam (apvts, ids[i], [] (double v) { return signedText ((int) std::lround (v)); });
    }
    perf.add (new Caption ("Quick offsets on top of the stored program, the same idea as the hardware's play page. "
                           "They are automatable and reset when you change program.", 8, 34));

    auto& trims = pg.add (new Section ("Automation Trims", 4));
    trims.tint = colours::accent2;
    const char* tids[4] = { "cutoff", "release", "fx1_balance", "fx2_balance" };
    const char* tnames[4] = { "Cutoff", "Release", "FX1 Mix", "FX2 Mix" };
    for (int i = 0; i < 4; ++i)
    {
        auto& k = trims.add (new Knob (tnames[i]));
        k.bindParam (apvts, tids[i], [] (double v) { return signedText ((int) std::lround (v)); });
        k.setAccent (colours::accent2);
    }

    auto& arp = pg.add (new Section ("Arpeggiator", 3));
    auto& on = arp.add (new Toggle ("Arp"));
    on.bindParam (apvts, "arp_on");
    pg.paramToggles.push_back (&on);
    arp.add (new Caption ("", 2, 52));
    auto& rate = arp.add (new Knob ("Rate"));
    rate.bindParam (apvts, "arp_rate", [] (double v) { return juce::String (kRates[juce::jlimit (0, 9, (int) std::lround (v))]); });
    auto& gate = arp.add (new Knob ("Gate"));
    gate.bindParam (apvts, "arp_gate", [] (double v) { return juce::String ((int) std::lround (v)) + "%"; });

    if (auto* p = currentProgram())
    {
        auto& info = pg.add (new Section ("Program", 6));
        info.tint = colours::dim;
        juce::String text;
        const auto& s = processor.session();
        text << "Mode: " << kModes[m1::clampInt (p->mode, 0, 2)] << ", "
             << (p->assign == (int) m1::Assign::Mono ? "mono" : "poly") << (p->hold ? ", hold" : "") << "\n";
        if (p->mode == (int) m1::OscMode::Drums)
            text << "Kit: " << s.kits[m1::clampInt (p->kit, 0, m1::kKits - 1)].name << "\n";
        else
        {
            text << "OSC 1: " << m1::multisoundRole (p->osc1.multisound) << "  level " << p->osc1.level << "\n";
            if (p->mode == (int) m1::OscMode::Double)
                text << "OSC 2: " << m1::multisoundRole (p->osc2.multisound) << "  level " << p->osc2.level
                     << "  interval " << signedText (p->interval) << "  detune " << signedText (p->detune) << "\n";
        }
        text << "Effects: " << m1::effectName (p->fx1.type) << " " << balanceText (p->fx1.balance);
        const int fx2 = m1::resolvedPartner (p->fx1.type, p->fx2.type);
        if (fx2 != 0)
            text << (p->placement == 1 ? "  |  " : "  >  ") << m1::effectName (fx2) << " " << balanceText (p->fx2.balance);
        info.add (new Caption (text, 6, 86));
    }
}

void M1ArpAudioProcessorEditor::addOscSelector (Page& pg, const m1::Program& p)
{
    if (p.mode != (int) m1::OscMode::Double)
    {
        oscEdit = 0;
        return;
    }
    auto& sel = pg.add (new Section ("Edit", 3));
    sel.add (new Segmented ("Oscillator", { "OSC 1", "OSC 2" }, &oscEdit, 0, [this] { requestRebuild(); }, 3));
}

void M1ArpAudioProcessorEditor::buildOsc (Page& pg, m1::Program& p)
{
    auto changed = [this] { edited(); };
    auto structural = [this] { edited(); requestRebuild(); };

    auto& voice = pg.add (new Section ("Voice", 9));
    voice.add (new Segmented ("Mode", kModes, &p.mode, 0, structural, 3));
    voice.add (new Segmented ("Assign", { "Poly", "Mono" }, &p.assign, 0, changed, 2));
    auto& hold = voice.add (new Toggle ("Hold"));
    hold.bind (&p.hold, changed);
    hold.button.setTooltip ("Ignore note-off (drum style)");
    juce::StringArray kits;
    for (int i = 0; i < m1::kKits; ++i)
        kits.add (juce::String (i + 1) + "  " + juce::String (processor.session().kits[i].name));
    auto& kit = voice.add (new Choice ("Drum Kit", 3));
    kit.bind (&p.kit, kits, 0, changed);
    kit.setAlpha (p.mode == (int) m1::OscMode::Drums ? 1.0f : 0.35f);

    const juce::StringArray octaves { "16'", "8'", "4'" };
    const bool drums = p.mode == (int) m1::OscMode::Drums;
    auto& o1 = pg.add (new Section ("Oscillator 1", 6));
    if (drums)
        o1.add (new Caption ("Drums mode: OSC 1 plays the drum kit chosen above. Edit the kit on the Kit page.", 6, 52));
    else
    {
        auto& ms = o1.add (new Choice ("Multisound", 4));
        ms.bind (&p.osc1.multisound, multisoundNames(), 0, changed);
        o1.add (new Segmented ("Octave", octaves, &p.osc1.octave, -1, changed, 2));
    }
    auto& l1 = o1.add (new Knob ("Level"));
    l1.bindInt (&p.osc1.level, 0, 99, changed, nullptr, 99);

    auto& o2 = pg.add (new Section ("Oscillator 2", 6));
    o2.tint = colours::accent2;
    auto& ms2 = o2.add (new Choice ("Multisound", 4));
    ms2.bind (&p.osc2.multisound, multisoundNames(), 0, changed);
    o2.add (new Segmented ("Octave", octaves, &p.osc2.octave, -1, changed, 2));
    auto& l2 = o2.add (new Knob ("Level"));
    l2.bindInt (&p.osc2.level, 0, 99, changed, nullptr, 99);
    auto& iv = o2.add (new Knob ("Interval", "Semitones relative to OSC 1"));
    iv.bindInt (&p.interval, -12, 12, changed);
    auto& dt = o2.add (new Knob ("Detune", "Cents"));
    dt.bindInt (&p.detune, -50, 50, changed);
    auto& ds = o2.add (new Knob ("Delay", "OSC 2 delay start: 0 = with the note, 99 = 500 ms"));
    ds.bindInt (&p.delayStart, 0, 99, changed);
    for (auto* c : o2.cells)
        if (auto* k = dynamic_cast<Knob*> (c))
            k->setAccent (colours::accent2);
    if (p.mode != (int) m1::OscMode::Double)
    {
        o2.setAlpha (0.4f);
        o2.add (new Caption ("Used in Double mode", 2, 30));
    }
}

void M1ArpAudioProcessorEditor::buildPitch (Page& pg, m1::Program& p)
{
    addOscSelector (pg, p);
    auto changed = [this] { edited(); };
    auto& peg = oscEdit == 0 ? p.pitch1 : p.pitch2;
    auto& s = pg.add (new Section (juce::String ("Pitch EG  OSC ") + juce::String (oscEdit + 1), 8));
    auto* graph = &s.add (new EnvelopeView (EnvelopeView::Pitch, nullptr, &peg, 8));
    pg.graphs.push_back (graph);
    struct K { const char* n; int* v; int lo; };
    const K ks[] = { { "Start", &peg.start, -99 }, { "Atk Time", &peg.at, 0 }, { "Atk Level", &peg.al, -99 },
                     { "Dcy Time", &peg.dt, 0 }, { "Rel Time", &peg.rt, 0 }, { "Rel Level", &peg.rl, -99 } };
    for (auto& k : ks)
    {
        auto& knob = s.add (new Knob (k.n, k.lo == 0 ? timeTip() : "Pitch in cents (99 = 1 octave)"));
        knob.bindInt (k.v, k.lo, 99, changed);
        knob.setAccent (juce::Colour (0xffb59cf0));
    }
    auto& v = pg.add (new Section ("Velocity", 2));
    auto& vl = v.add (new Knob ("EG Level"));
    vl.bindInt (&peg.velLevel, -99, 99, changed);
    auto& vt = v.add (new Knob ("EG Time"));
    vt.bindInt (&peg.velTime, -99, 99, changed);
    auto& mg = pg.add (new Section ("Pitch Bend", 2));
    auto& bend = mg.add (new Knob ("Range", "Semitones"));
    bend.bindInt (&p.joy.bend, -12, 12, changed);
}

void M1ArpAudioProcessorEditor::buildFilter (Page& pg, m1::Program& p)
{
    addOscSelector (pg, p);
    auto changed = [this] { edited(); };
    const bool two = oscEdit == 1;
    auto& egSpec = two ? p.vdf2 : p.vdf1;
    auto& vel = two ? p.vdfVel2 : p.vdfVel1;
    auto& kbd = two ? p.vdfKbd2 : p.vdfKbd1;
    const juce::String suffix = "  OSC " + juce::String (oscEdit + 1);

    auto& f = pg.add (new Section ("VDF" + suffix, 2));
    f.tint = colours::accent2;
    auto& cut = f.add (new Knob ("Cutoff", "100 Hz to 16 kHz, low-pass, no resonance"));
    cut.bindInt (two ? &p.cutoff2 : &p.cutoff1, 0, 99, changed, [] (int v) { return juce::String (v); });
    cut.setAccent (colours::accent2);
    auto& ei = f.add (new Knob ("EG Int", "How far the VDF EG moves the cutoff (99 = 4 octaves)"));
    ei.bindInt (two ? &p.egInt2 : &p.egInt1, 0, 99, changed);
    ei.setAccent (colours::accent2);

    auto& e = pg.add (new Section ("VDF EG" + suffix, 8));
    e.tint = colours::accent2;
    pg.graphs.push_back (&e.add (new EnvelopeView (EnvelopeView::Filter, &egSpec, nullptr, 8)));
    struct K { const char* n; int* v; bool level; };
    const K ks[] = { { "Attack", &egSpec.at, false }, { "Atk Lvl", &egSpec.al, true }, { "Decay", &egSpec.dt, false },
                     { "Break", &egSpec.bp, true }, { "Slope", &egSpec.st, false }, { "Sustain", &egSpec.sl, true },
                     { "Release", &egSpec.rt, false }, { "Rel Lvl", &egSpec.rl, true } };
    for (auto& k : ks)
    {
        auto& knob = e.add (new Knob (k.n, k.level ? "Relative to the cutoff, -99..+99" : timeTip()));
        knob.bindInt (k.v, k.level ? -99 : 0, 99, changed);
        knob.setAccent (colours::accent2);
    }

    auto& v = pg.add (new Section ("Velocity", 6));
    auto& vi = v.add (new Knob ("EG Int", "Velocity scales the EG intensity"));
    vi.bindInt (&vel.egInt, -99, 99, changed);
    auto& vt = v.add (new Knob ("EG Time"));
    vt.bindInt (&vel.egTime, 0, 99, changed);
    const char* pol[4] = { "AT", "DT", "ST", "RT" };
    int* vp[4] = { &vel.atPol, &vel.dtPol, &vel.stPol, &vel.rtPol };
    for (int i = 0; i < 4; ++i)
        v.add (new Choice (pol[i], 1)).bind (vp[i], kPolarity, -1, changed);

    auto& k = pg.add (new Section ("Keyboard Track", 7));
    auto& kc = k.add (new Knob ("Centre"));
    kc.bindInt (&kbd.centre, 24, 108, changed, [] (int n) { return noteText (n); }, 60);
    auto& ka = k.add (new Knob ("Cutoff", "0 = cutoff follows pitch exactly"));
    ka.bindInt (&kbd.amount, -99, 99, changed);
    auto& kt = k.add (new Knob ("EG Time"));
    kt.bindInt (&kbd.egTime, 0, 99, changed);
    int* kp[4] = { &kbd.atPol, &kbd.dtPol, &kbd.stPol, &kbd.rtPol };
    for (int i = 0; i < 4; ++i)
        k.add (new Choice (pol[i], 1)).bind (kp[i], kPolarity, -1, changed);
}

void M1ArpAudioProcessorEditor::buildAmp (Page& pg, m1::Program& p)
{
    addOscSelector (pg, p);
    auto changed = [this] { edited(); };
    const bool two = oscEdit == 1;
    auto& egSpec = two ? p.vda2 : p.vda1;
    auto& vel = two ? p.vdaVel2 : p.vdaVel1;
    auto& kbd = two ? p.vdaKbd2 : p.vdaKbd1;
    const juce::String suffix = "  OSC " + juce::String (oscEdit + 1);

    auto& e = pg.add (new Section ("VDA EG" + suffix, 8));
    pg.graphs.push_back (&e.add (new EnvelopeView (EnvelopeView::Amp, &egSpec, nullptr, 8)));
    struct K { const char* n; int* v; bool level; };
    const K ks[] = { { "Attack", &egSpec.at, false }, { "Atk Lvl", &egSpec.al, true }, { "Decay", &egSpec.dt, false },
                     { "Break", &egSpec.bp, true }, { "Slope", &egSpec.st, false }, { "Sustain", &egSpec.sl, true },
                     { "Release", &egSpec.rt, false }, { "Rel Lvl", &egSpec.rl, true } };
    for (auto& k : ks)
    {
        auto& knob = e.add (new Knob (k.n, k.level ? "Amplitude 0..99" : timeTip()));
        knob.bindInt (k.v, 0, 99, changed);
    }

    auto& lv = pg.add (new Section ("Level", 1));
    auto& level = lv.add (new Knob ("OSC Level"));
    level.bindInt (two ? &p.osc2.level : &p.osc1.level, 0, 99, changed, nullptr, 99);

    auto& v = pg.add (new Section ("Velocity", 6));
    auto& vi = v.add (new Knob ("Amp Sens", "How much velocity changes the level"));
    vi.bindInt (&vel.egInt, -99, 99, changed);
    auto& vt = v.add (new Knob ("EG Time"));
    vt.bindInt (&vel.egTime, 0, 99, changed);
    const char* pol[4] = { "AT", "DT", "ST", "RT" };
    int* vp[4] = { &vel.atPol, &vel.dtPol, &vel.stPol, &vel.rtPol };
    for (int i = 0; i < 4; ++i)
        v.add (new Choice (pol[i], 1)).bind (vp[i], kPolarity, -1, changed);

    auto& k = pg.add (new Section ("Keyboard Track", 7));
    auto& kc = k.add (new Knob ("Centre"));
    kc.bindInt (&kbd.centre, 24, 108, changed, [] (int n) { return noteText (n); }, 60);
    auto& ka = k.add (new Knob ("Level", "Level change above and below the centre key"));
    ka.bindInt (&kbd.amount, -99, 99, changed);
    auto& kt = k.add (new Knob ("EG Time"));
    kt.bindInt (&kbd.egTime, 0, 99, changed);
    int* kp[4] = { &kbd.atPol, &kbd.dtPol, &kbd.stPol, &kbd.rtPol };
    for (int i = 0; i < 4; ++i)
        k.add (new Choice (pol[i], 1)).bind (kp[i], kPolarity, -1, changed);
}

void M1ArpAudioProcessorEditor::buildMod (Page& pg, m1::Program& p)
{
    auto changed = [this] { edited(); };
    auto mg = [&] (const char* title, m1::Mg& m, juce::Colour tint)
    {
        auto& s = pg.add (new Section (title, 7));
        s.tint = tint;
        s.add (new Choice ("Wave", 2)).bind (&m.wave, kWaves, 0, changed);
        s.add (new Choice ("Target", 2)).bind (&m.oscSelect, kOscSelect, 0, changed);
        auto& ks = s.add (new Toggle ("Key Sync"));
        ks.bind (&m.keySync, changed);
        s.add (new Caption ("", 2, 52));
        auto& f = s.add (new Knob ("Freq", "0.1 Hz to 20 Hz"));
        f.bindInt (&m.frequency, 0, 99, changed);
        auto& d = s.add (new Knob ("Delay", timeTip()));
        d.bindInt (&m.delay, 0, 99, changed);
        auto& in = s.add (new Knob ("Intensity"));
        in.bindInt (&m.intensity, 0, 99, changed);
        for (auto* c : s.cells)
            if (auto* k = dynamic_cast<Knob*> (c))
                k->setAccent (tint);
    };
    mg ("Pitch MG (vibrato)", p.pitchMg, juce::Colour (0xffb59cf0));
    mg ("VDF MG (wah)", p.vdfMg, colours::accent2);

    auto& at = pg.add (new Section ("Aftertouch", 5));
    auto& a1 = at.add (new Knob ("Pitch", "Semitones at full pressure"));
    a1.bindInt (&p.after.pitch, -12, 12, changed);
    auto& a2 = at.add (new Knob ("Pitch MG"));
    a2.bindInt (&p.after.pitchMg, 0, 99, changed);
    auto& a3 = at.add (new Knob ("VDF"));
    a3.bindInt (&p.after.vdf, -99, 99, changed);
    auto& a4 = at.add (new Knob ("VDF MG"));
    a4.bindInt (&p.after.vdfMg, 0, 99, changed);
    auto& a5 = at.add (new Knob ("VDA"));
    a5.bindInt (&p.after.vda, -99, 99, changed);

    auto& js = pg.add (new Section ("Pitch Bend + Mod Wheel", 6));
    auto& b = js.add (new Knob ("Bend", "Pitch bend range, semitones"));
    b.bindInt (&p.joy.bend, -12, 12, changed);
    auto& sw = js.add (new Knob ("VDF Sweep", "Mod wheel opens or closes the filter"));
    sw.bindInt (&p.joy.vdfSweep, -99, 99, changed);
    auto& j1 = js.add (new Knob ("Pitch MG", "Mod wheel adds vibrato"));
    j1.bindInt (&p.joy.pitchMg, 0, 99, changed);
    auto& j2 = js.add (new Knob ("PMG Freq"));
    j2.bindInt (&p.joy.pitchMgFreq, 0, 99, changed);
    auto& j3 = js.add (new Knob ("VDF MG"));
    j3.bindInt (&p.joy.vdfMg, 0, 99, changed);
    auto& j4 = js.add (new Knob ("VMG Freq"));
    j4.bindInt (&p.joy.vdfMgFreq, 0, 99, changed);
}

void M1ArpAudioProcessorEditor::buildArp (Page& pg, m1::Program& p)
{
    auto changed = [this] { edited(); };
    auto& apvts = processor.apvts;
    m1::ArpSettings* arp = &p.arp;
    auto* combi = currentCombi();
    const bool combiMode = processor.isCombiMode() && combi != nullptr;
    if (combiMode)
        arp = &combi->arp;

    auto& s = pg.add (new Section ("Arpeggiator", 9));
    auto& on = s.add (new Toggle ("Arp"));
    on.bindParam (apvts, "arp_on");
    pg.paramToggles.push_back (&on);
    s.add (new Choice ("Mode", 2)).bind (&arp->mode, kArpModes, 0, changed);
    auto& hold = s.add (new Toggle ("Latch"));
    hold.bind (&arp->hold, changed);
    s.add (new Caption (combiMode && combi->type == (int) m1::CombiType::Multi
                            ? "Multi: each timbre has its own arp switch, mode, gate and octaves on the Combi page. Rate and swing are shared."
                            : "Rate, gate and on/off are host-automatable. Turn on Latch to keep the pattern after you let go.",
                        5, 52));
    auto& rate = s.add (new Knob ("Rate"));
    rate.bindParam (apvts, "arp_rate", [] (double v) { return juce::String (kRates[juce::jlimit (0, 9, (int) std::lround (v))]); });
    auto& gate = s.add (new Knob ("Gate"));
    gate.bindParam (apvts, "arp_gate", [] (double v) { return juce::String ((int) std::lround (v)) + "%"; });
    auto& oct = s.add (new Knob ("Octaves"));
    oct.bindInt (&arp->octaves, 1, 4, changed, nullptr, 1);
    auto& sw = s.add (new Knob ("Swing"));
    sw.bindInt (&arp->swing, 50, 75, changed, [] (int v) { return juce::String (v) + "%"; }, 50);

    const int width = juce::jmax (400, viewport.getWidth() - viewport.getScrollBarThickness() - 2);
    const int cols = juce::jmax (8, (width - 20) / Section::kCol);
    auto& pat = pg.add (new Section ("Pattern  (Pattern mode)", cols));
    pat.tint = colours::accent2;
    pat.add (new StepGrid (arp, changed, cols));
    pat.add (new Caption ("Click a lamp to switch a step on or off. Drag a bar to set its velocity. "
                          "Click the octave cell to raise it (shift-click or right-click lowers it).", cols, 22));
    if (arp->mode != (int) m1::ArpMode::Pattern)
        pat.setAlpha (0.55f);
}

void M1ArpAudioProcessorEditor::buildFx (Page& pg, m1::Program& p)
{
    auto changed = [this] { edited(); };
    auto structural = [this] { edited(); requestRebuild(); };
    m1::Fx* fx1 = &p.fx1;
    m1::Fx* fx2 = &p.fx2;
    int* placement = &p.placement;
    auto* combi = currentCombi();
    if (processor.isCombiMode() && combi != nullptr)
    {
        fx1 = &combi->fx1;
        fx2 = &combi->fx2;
        placement = &combi->placement;
    }
    fx2->type = m1::resolvedPartner (fx1->type, fx2->type);

    auto slot = [&] (const char* title, m1::Fx& fx, int other, bool second, juce::Colour tint)
    {
        auto& s = pg.add (new Section (title, 6));
        s.tint = tint;
        auto& type = s.add (new Choice ("Type", 4));
        type.bind (&fx.type, effectNames(), 0, structural);
        if (second)
            for (int t = 0; t <= 33; ++t)
                type.box.setItemEnabled (t + 1, m1::effectPairAllowed (other, t));
        auto& bal = s.add (new Knob ("Dry:Fx", "Dry to effect balance, 99:1 (dry) to 1:99 (all effect)"));
        bal.bindInt (&fx.balance, 0, 98, changed, [] (int v) { return balanceText (v); });
        bal.setAccent (tint);
        s.add (new Caption ("", 1, 10));
        int named = 0;
        for (int i = 0; i < 8; ++i)
            if (const char* n = m1::effectParamName (fx.type, i))
            {
                auto& k = s.add (new Knob (n));
                k.bindInt (&fx.p[i], 0, 99, changed, nullptr, 50);
                k.setAccent (tint);
                ++named;
            }
        if (fx.type == 0)
            s.add (new Caption ("No effect.", 6, 30));
        else if (named == 0)
            s.add (new Caption ("This type has no parameters beyond its balance.", 6, 30));
    };
    slot ("Effect 1", *fx1, 0, false, colours::accent);
    slot ("Effect 2", *fx2, fx1->type, true, colours::accent2);

    auto& r = pg.add (new Section ("Routing", 3));
    r.tint = colours::dim;
    r.add (new Segmented ("Placement", { "Serial", "Parallel" }, placement, 0, changed, 3));
    r.add (new Caption ("Serial: FX1 feeds FX2.\nParallel: both hear the dry voices.\n"
                        "Ensemble (24) and Rotary (25) cannot pair with another modulation effect, so those types are greyed out.",
                        3, 110));
    if (processor.isCombiMode())
        r.add (new Caption ("Editing the combination's effects.", 3, 20));
}

void M1ArpAudioProcessorEditor::buildCombi (Page& pg)
{
    auto* combi = currentCombi();
    if (combi == nullptr)
        return;
    auto changed = [this] { edited(); };
    auto structural = [this] { edited(); requestRebuild(); };
    const auto& s = processor.session();

    auto& head = pg.add (new Section ("Combination  " + juce::String (combi->name), 10));
    head.add (new Segmented ("Type", kCombiTypes, &combi->type, 0, structural, 5));
    auto& split = head.add (new Knob ("Split Key"));
    split.bindInt (&combi->splitKey, 0, 127, changed, [] (int n) { return noteText (n); }, 60);
    split.setAlpha (combi->type == (int) m1::CombiType::Split ? 1.0f : 0.35f);
    auto& vp = head.add (new Knob ("Vel Point"));
    vp.bindInt (&combi->velPoint, 1, 127, changed, nullptr, 64);
    vp.setAlpha (combi->type == (int) m1::CombiType::Velocity ? 1.0f : 0.35f);
    head.add (new Choice ("Arp Mode", 2)).bind (&combi->arp.mode, kArpModes, 0, changed);
    auto& sw = head.add (new Knob ("Arp Swing"));
    sw.bindInt (&combi->arp.swing, 50, 75, changed, [] (int v) { return juce::String (v) + "%"; }, 50);
    if (! processor.isCombiMode())
        head.add (new Caption ("You are in PROG mode. Press COMBI in the header to hear this combination.", 10, 22));

    int active = 1;
    if (combi->type == (int) m1::CombiType::Multi) active = 8;
    else if (combi->type != (int) m1::CombiType::Single) active = 2;

    using C = TableSection::Column;
    auto& table = pg.add (new TableSection ("Timbres", {
        C { "#", 22 }, C { "Program", 160 }, C { "Level", 50 }, C { "Pan", 50 }, C { "MIDI Ch", 52 },
        C { "Key Lo", 54 }, C { "Key Hi", 54 }, C { "Vel Lo", 48 }, C { "Vel Hi", 48 }, C { "Trans", 48 },
        C { "Detune", 50 }, C { "Damper", 52 }, C { "Arp", 40 }, C { "Arp Mode", 92 }, C { "Gate", 48 }, C { "Oct", 40 } }));
    table.getProperties().set ("fullWidth", false);
    juce::StringArray programs;
    for (int i = 0; i < s.numPrograms; ++i)
        programs.add (juce::String (i).paddedLeft ('0', 2) + "  " + s.programs[i].name);

    for (int i = 0; i < m1::kTimbres; ++i)
    {
        auto& t = combi->timbre[i];
        auto* idx = table.make (new juce::Label ({}, juce::String (i + 1)));
        idx->setColour (juce::Label::textColourId, colours::dim);
        auto* prog = table.make (new juce::ComboBox());
        for (int k = 0; k < programs.size(); ++k)
            prog->addItem (programs[k], k + 1);
        prog->setSelectedId (t.program + 1, juce::dontSendNotification);
        prog->onChange = [prog, &t, changed] { t.program = prog->getSelectedId() - 1; changed(); };
        auto* damper = table.make (new juce::TextButton ("Sus"));
        damper->setClickingTogglesState (true);
        damper->setToggleState (t.damper != 0, juce::dontSendNotification);
        damper->onClick = [damper, &t, changed] { t.damper = damper->getToggleState() ? 1 : 0; changed(); };
        auto* arpOn = table.make (new juce::TextButton ("On"));
        arpOn->setClickingTogglesState (true);
        arpOn->setToggleState (t.arp.on != 0, juce::dontSendNotification);
        arpOn->onClick = [arpOn, &t, changed] { t.arp.on = arpOn->getToggleState() ? 1 : 0; changed(); };
        auto* arpMode = table.make (new juce::ComboBox());
        for (int k = 0; k < kArpModes.size(); ++k)
            arpMode->addItem (kArpModes[k], k + 1);
        arpMode->setSelectedId (t.arp.mode + 1, juce::dontSendNotification);
        arpMode->onChange = [arpMode, &t, changed] { t.arp.mode = arpMode->getSelectedId() - 1; changed(); };
        const bool multi = combi->type == (int) m1::CombiType::Multi;
        if (! multi)
        {
            arpOn->setEnabled (false);
            arpMode->setEnabled (false);
        }
        table.addRow ({
            idx, prog,
            table.make (new BarSlider (&t.level, 0, 99, changed)),
            table.make (new BarSlider (&t.pan, -50, 50, changed, [] (int v) { return v == 0 ? juce::String ("C") : (v < 0 ? "L" + juce::String (-v) : "R" + juce::String (v)); })),
            table.make (new BarSlider (&t.channel, 0, 16, changed, [] (int v) { return v == 0 ? juce::String ("All") : juce::String (v); })),
            table.make (new BarSlider (&t.keyLo, 0, 127, changed, [] (int n) { return noteText (n); })),
            table.make (new BarSlider (&t.keyHi, 0, 127, changed, [] (int n) { return noteText (n); })),
            table.make (new BarSlider (&t.velLo, 1, 127, changed)),
            table.make (new BarSlider (&t.velHi, 1, 127, changed)),
            table.make (new BarSlider (&t.transpose, -24, 24, changed)),
            table.make (new BarSlider (&t.detune, -50, 50, changed)),
            damper, arpOn, arpMode,
            table.make (new BarSlider (&t.arp.gate, 1, 100, changed, [] (int v) { return juce::String (v) + "%"; })),
            table.make (new BarSlider (&t.arp.octaves, 1, 4, changed)),
        }, i < active ? 1.0f : 0.35f);
    }
}

void M1ArpAudioProcessorEditor::buildKit (Page& pg)
{
    auto* program = currentProgram();
    auto* kit = currentKit();
    if (program == nullptr || kit == nullptr)
        return;
    auto changed = [this] { edited(); };
    auto structural = [this] { edited(); requestRebuild(); };

    auto& head = pg.add (new Section ("Drum Kit", 8));
    juce::StringArray kits;
    for (int i = 0; i < m1::kKits; ++i)
        kits.add (juce::String (i + 1) + "  " + juce::String (processor.session().kits[i].name));
    head.add (new Choice ("Program Kit", 3)).bind (&program->kit, kits, 0, structural);
    head.add (new Caption (program->mode == (int) m1::OscMode::Drums
                               ? "Click a key name to audition it. Keys in the same group cut each other off (hi-hats)."
                               : "This program is not in Drums mode. Set Mode to Drums on the Osc page to play the kit.",
                           5, 52));

    using C = TableSection::Column;
    auto& table = pg.add (new TableSection ("Keys", {
        C { "Key", 78 }, C { "Sample", 170 }, C { "Tune", 70 }, C { "Level", 70 }, C { "Decay", 70 },
        C { "Pan", 70 }, C { "Group", 60 }, C { "Plays On", 70 } }));
    const auto names = drumNames();
    for (int i = 0; i < m1::kDrumKeys; ++i)
    {
        auto& key = kit->key[i];
        const int note = key.note != 0 ? key.note : m1::kDrumKeyBase + i;
        auto* play = table.make (new juce::TextButton (juce::String (note) + "  " + noteText (note)));
        play->setTooltip ("Audition");
        play->onClick = [this, note]
        {
            processor.clickState.noteOn (1, note, 0.85f);
            juce::Timer::callAfterDelay (120, [safe = juce::Component::SafePointer<M1ArpAudioProcessorEditor> (this), note]
            {
                if (safe != nullptr)
                    safe->processor.clickState.noteOff (1, note, 0.0f);
            });
        };
        auto* sample = table.make (new juce::ComboBox());
        for (int k = 0; k < names.size(); ++k)
            sample->addItem (names[k], k + 1);
        sample->setSelectedId (key.sample + 1, juce::dontSendNotification);
        sample->onChange = [sample, &key, changed] { key.sample = sample->getSelectedId() - 1; changed(); };
        table.addRow ({
            play, sample,
            table.make (new BarSlider (&key.tune, -50, 50, changed)),
            table.make (new BarSlider (&key.level, 0, 99, changed)),
            table.make (new BarSlider (&key.decay, 0, 99, changed, [] (int v) { return v >= 99 ? juce::String ("Full") : juce::String (v); })),
            table.make (new BarSlider (&key.pan, -50, 50, changed, [] (int v) { return v == 0 ? juce::String ("C") : (v < 0 ? "L" + juce::String (-v) : "R" + juce::String (v)); })),
            table.make (new BarSlider (&key.group, 0, 7, changed, [] (int v) { return v == 0 ? juce::String ("-") : juce::String (v); })),
            table.make (new BarSlider (&key.note, 0, 127, changed, [] (int n) { return noteText (n); })),
        });
    }
}

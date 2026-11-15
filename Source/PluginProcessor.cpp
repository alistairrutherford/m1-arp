#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::AudioParameterInt* addInt (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                                 const char* id, const char* name, int lo, int hi, int value)
{
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, lo, hi, value));
    return nullptr;
}
}

M1ArpAudioProcessor::M1ArpAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    edit = m1::makeFactory();
    published = edit;
    // The multisample bank is synthesised (or read from its cache) on a
    // background thread shared by every instance; prepareToPlay waits for it.
    m1::BankCache::instance().start();
    pushPerformanceFrom (edit.programs[0].arp);
    refreshTail (edit);
}

juce::AudioProcessorValueTreeState::ParameterLayout M1ArpAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    addInt (layout, "play_osc_balance", "Osc Balance", -10, 10, 0);
    addInt (layout, "play_cutoff", "Play Cutoff", -10, 10, 0);
    addInt (layout, "play_level", "Play Level", -10, 10, 0);
    addInt (layout, "play_kbd", "Play Keyboard", -10, 10, 0);
    addInt (layout, "play_velocity", "Play Velocity", -10, 10, 0);
    addInt (layout, "play_attack", "Play Attack", -10, 10, 0);
    addInt (layout, "play_release", "Play Release", -10, 10, 0);
    addInt (layout, "play_effect", "Play Effect", -10, 10, 0);
    addInt (layout, "cutoff", "Cutoff", -99, 99, 0);
    addInt (layout, "release", "Release", -99, 99, 0);
    // An int, not a bool: AudioParameterBool::setValue keeps the raw 0..1 float.
    // Restoring a state whose on/off side did not change then leaves that float
    // in place, and pluginval rejects the restore.
    addInt (layout, "arp_on", "Arp", 0, 1, 0);
    addInt (layout, "arp_rate", "Arp Rate", 0, 9, 5);
    addInt (layout, "arp_gate", "Arp Gate", 1, 100, 50);
    addInt (layout, "fx1_balance", "FX1 Balance", -98, 98, 0);
    addInt (layout, "fx2_balance", "FX2 Balance", -98, 98, 0);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "master", 1 }, "Master",
        juce::NormalisableRange<float> (-36.0f, 6.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

float M1ArpAudioProcessor::paramFloat (const char* id) const
{
    if (auto* v = apvts.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}

void M1ArpAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    bankPtr.store (m1::BankCache::instance().wait (60000), std::memory_order_release);
    engine.setBank (bankPtr.load (std::memory_order_acquire));
    masterGain = juce::Decibels::decibelsToGain (paramFloat ("master"), -36.0f);
}

void M1ArpAudioProcessor::releaseResources()
{
    engine.reset();
}

bool M1ArpAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::disabled() || in == juce::AudioChannelSet::stereo();
}

void M1ArpAudioProcessor::publish()
{
    seq.fetch_add (1, std::memory_order_acq_rel);
    published = edit;
    seq.fetch_add (1, std::memory_order_release);
    refreshTail (edit);
}

void M1ArpAudioProcessor::publishSession()
{
    publish();
}

void M1ArpAudioProcessor::snapshot (m1::Session& dst) const
{
    for (;;)
    {
        const auto a = seq.load (std::memory_order_acquire);
        if (a & 1u)
            continue;
        dst = published;
        const auto b = seq.load (std::memory_order_acquire);
        if (a == b)
            return;
    }
}

void M1ArpAudioProcessor::refreshTail (const m1::Session& s)
{
    int t1 = 0, t2 = 0;
    if (s.combiMode != 0 && s.numCombis > 0)
    {
        const auto& c = s.combis[m1::clampInt (s.currentCombi, 0, s.numCombis - 1)];
        t1 = c.fx1.type;
        t2 = m1::resolvedPartner (t1, c.fx2.type);
    }
    else if (s.numPrograms > 0)
    {
        const auto& p = s.programs[m1::clampInt (s.currentProgram, 0, s.numPrograms - 1)];
        t1 = p.fx1.type;
        t2 = m1::resolvedPartner (t1, p.fx2.type);
    }
    tailSeconds.store ((float) m1::EffectRack::tailSeconds (t1, t2));
}

m1::Performance M1ArpAudioProcessor::readPerformance() const
{
    m1::Performance p;
    p.play.oscBalance = (int) paramFloat ("play_osc_balance");
    p.play.cutoff = (int) paramFloat ("play_cutoff");
    p.play.level = (int) paramFloat ("play_level");
    p.play.kbd = (int) paramFloat ("play_kbd");
    p.play.velocity = (int) paramFloat ("play_velocity");
    p.play.attack = (int) paramFloat ("play_attack");
    p.play.release = (int) paramFloat ("play_release");
    p.play.effect = (int) paramFloat ("play_effect");
    p.cutoffAdd = (int) paramFloat ("cutoff");
    p.releaseAdd = (int) paramFloat ("release");
    p.arpOn = paramFloat ("arp_on") > 0.5f ? 1 : 0;
    p.arpRate = (int) paramFloat ("arp_rate");
    p.arpGate = (int) paramFloat ("arp_gate");
    p.fx1Add = (int) paramFloat ("fx1_balance");
    p.fx2Add = (int) paramFloat ("fx2_balance");
    return p;
}

void M1ArpAudioProcessor::pullPerformanceIntoSession()
{
    const int on = paramFloat ("arp_on") > 0.5f ? 1 : 0;
    const int rate = (int) paramFloat ("arp_rate");
    const int gate = (int) paramFloat ("arp_gate");
    if (edit.combiMode != 0 && edit.numCombis > 0)
    {
        auto& c = edit.combis[m1::clampInt (edit.currentCombi, 0, edit.numCombis - 1)];
        c.arp.rate = rate;
        if (c.type != (int) m1::CombiType::Multi)
        {
            c.arp.on = on;
            c.arp.gate = gate;
        }
    }
    else if (edit.numPrograms > 0)
    {
        auto& p = edit.programs[m1::clampInt (edit.currentProgram, 0, edit.numPrograms - 1)];
        p.arp.on = on;
        p.arp.rate = rate;
        p.arp.gate = gate;
    }
}

void M1ArpAudioProcessor::pushPerformanceFrom (const m1::ArpSettings& arp)
{
    auto set = [this] (const char* id, float value)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    set ("play_osc_balance", 0);
    set ("play_cutoff", 0);
    set ("play_level", 0);
    set ("play_kbd", 0);
    set ("play_velocity", 0);
    set ("play_attack", 0);
    set ("play_release", 0);
    set ("play_effect", 0);
    set ("cutoff", 0);
    set ("release", 0);
    set ("fx1_balance", 0);
    set ("fx2_balance", 0);
    set ("arp_on", arp.on ? 1.0f : 0.0f);
    set ("arp_rate", (float) m1::clampInt (arp.rate, 0, 9));
    set ("arp_gate", (float) m1::clampInt (arp.gate, 1, 100));
}

void M1ArpAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    if (resetRequested.exchange (false))
    {
        engine.reset();
        keyboardState.allNotesOff (0);
    }
    snapshot (renderSession);
    auto* bank = bankPtr.load (std::memory_order_acquire);
    if (bank == nullptr)
    {
        bank = m1::BankCache::instance().get();
        if (bank != nullptr)
            bankPtr.store (bank, std::memory_order_release);
    }
    engine.setBank (bank);

    m1::Transport tr;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            if (auto bpm = pos->getBpm())
                tr.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition())
                tr.ppq = *ppq;
            tr.playing = pos->getIsPlaying();
        }

    const int n = buffer.getNumSamples();
    clickState.processNextMidiBuffer (midi, 0, n, true);
    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;
    engine.process (left, right, n, midi, renderSession, tr, readPerformance());
    keyboardState.processNextMidiBuffer (midi, 0, n, false);

    // Master level (smoothed per sample) and a soft clipper that is
    // transparent below -2 dBFS and never exceeds 0 dBFS.
    const float target = juce::Decibels::decibelsToGain (paramFloat ("master"), -36.0f);
    const float step = (target - masterGain) / (float) std::max (1, n);
    float peak = 0.0f;
    auto clip = [] (float x)
    {
        constexpr float knee = 0.8f;
        const float a = std::abs (x);
        if (a <= knee)
            return x;
        const float y = knee + (1.0f - knee) * std::tanh ((a - knee) / (1.0f - knee));
        return x < 0.0f ? -y : y;
    };
    for (int i = 0; i < n; ++i)
    {
        masterGain += step;
        left[i] = clip (left[i] * masterGain);
        if (right != left)
            right[i] = clip (right[i] * masterGain);
        peak = std::max (peak, std::max (std::abs (left[i]), std::abs (right[i])));
    }
    masterGain = target;
    if (peak > peakMeter.load (std::memory_order_relaxed))
        peakMeter.store (peak, std::memory_order_relaxed);
}

void M1ArpAudioProcessor::reset()
{
    engine.reset();
    keyboardState.allNotesOff (0);
}

juce::AudioProcessorEditor* M1ArpAudioProcessor::createEditor()
{
    return new M1ArpAudioProcessorEditor (*this);
}

int M1ArpAudioProcessor::getNumPrograms()
{
    return std::max (1, edit.numPrograms);
}

int M1ArpAudioProcessor::getCurrentProgram()
{
    return edit.currentProgram;
}

void M1ArpAudioProcessor::setCurrentProgram (int index)
{
    if (edit.numPrograms <= 0)
        return;
    pullPerformanceIntoSession();
    edit.currentProgram = m1::clampInt (index, 0, edit.numPrograms - 1);
    if (edit.combiMode == 0)
        pushPerformanceFrom (edit.programs[edit.currentProgram].arp);
    publish();
    resetRequested.store (true);
    revision.fetch_add (1);
}

const juce::String M1ArpAudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= edit.numPrograms)
        return {};
    return edit.programs[index].name;
}

void M1ArpAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    if (index < 0 || index >= edit.numPrograms)
        return;
    m1::copyName (edit.programs[index].name, newName.toRawUTF8());
    publish();
}

void M1ArpAudioProcessor::setCombiMode (bool on)
{
    pullPerformanceIntoSession();
    edit.combiMode = on ? 1 : 0;
    if (on && edit.numCombis > 0)
        pushPerformanceFrom (edit.combis[m1::clampInt (edit.currentCombi, 0, edit.numCombis - 1)].arp);
    else if (edit.numPrograms > 0)
        pushPerformanceFrom (edit.programs[edit.currentProgram].arp);
    publish();
    resetRequested.store (true);
    revision.fetch_add (1);
}

void M1ArpAudioProcessor::setCurrentCombi (int index)
{
    if (edit.numCombis <= 0)
        return;
    pullPerformanceIntoSession();
    edit.currentCombi = m1::clampInt (index, 0, edit.numCombis - 1);
    if (edit.combiMode != 0)
        pushPerformanceFrom (edit.combis[edit.currentCombi].arp);
    publish();
    resetRequested.store (true);
    revision.fetch_add (1);
}

void M1ArpAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    pullPerformanceIntoSession();
    publish();
    auto xml = m1::sessionToXml (edit);
    if (auto params = apvts.copyState().createXml())
        xml->addChildElement (params.release());
    copyXmlToBinary (*xml, destData);
}

void M1ArpAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName ("M1ARP"))
        {
            edit = m1::sessionFromXml (*xml);
            if (auto* params = xml->getChildByName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*params));
            else if (edit.combiMode != 0 && edit.numCombis > 0)
                pushPerformanceFrom (edit.combis[edit.currentCombi].arp);
            else if (edit.numPrograms > 0)
                pushPerformanceFrom (edit.programs[edit.currentProgram].arp);
            publish();
            resetRequested.store (true);
            revision.fetch_add (1);
        }
    }
}

void M1ArpAudioProcessor::savePreset (const juce::File& file)
{
    pullPerformanceIntoSession();
    publish();
    auto xml = m1::sessionToXml (edit);
    if (auto params = apvts.copyState().createXml())
        xml->addChildElement (params.release());
    xml->writeTo (file);
}

void M1ArpAudioProcessor::loadPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName ("M1ARP"))
        {
            edit = m1::sessionFromXml (*xml);
            if (auto* params = xml->getChildByName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*params));
            else if (edit.numPrograms > 0)
                pushPerformanceFrom (edit.programs[edit.currentProgram].arp);
            publish();
            resetRequested.store (true);
            revision.fetch_add (1);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new M1ArpAudioProcessor();
}

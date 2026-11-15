// Integration test: the real processor renders a factory program and a preset
// round-trip still sounds. Run via run_integration.sh after a Release build.
#include "PluginProcessor.h"
#include <cmath>
#include <iostream>

namespace
{
int failures = 0;

void check (bool cond, const char* what)
{
    std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (! cond)
        ++failures;
}

double renderRms (M1ArpAudioProcessor& proc, int note = 60)
{
    juce::MidiBuffer on;
    on.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer empty;
    double acc = 0.0;
    int n = 0;
    for (int b = 0; b < 40; ++b)
    {
        buf.clear();
        proc.processBlock (buf, b == 0 ? on : empty);
        for (int i = 0; i < 512; ++i)
        {
            const float s = buf.getSample (0, i);
            acc += (double) s * s;
            ++n;
        }
    }
    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::noteOff (1, note), 0);
    buf.clear();
    proc.processBlock (buf, off);
    for (int b = 0; b < 20; ++b)
    {
        buf.clear();
        proc.processBlock (buf, empty);
    }
    return std::sqrt (acc / std::max (1, n));
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::cout << "Processor integration\n";

    M1ArpAudioProcessor proc;
    proc.setRateAndBufferSizeDetails (48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    check (proc.getNumPrograms() >= 24, "factory programs are published to the host");
    check (proc.getProgramName (0).isNotEmpty(), "the default program has a name");
    const double piano = renderRms (proc);
    std::cout << "  piano rms " << piano << "\n";
    check (piano > 1.0e-3, "the factory piano produces sound");

    {
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer on;
        on.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
        proc.processBlock (buf, on);
        check (proc.keyboardState.isNoteOn (1, 64), "keyboard state lights the voice note");

        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
        buf.clear();
        proc.processBlock (buf, off);
        for (int b = 0; b < 80; ++b)
        {
            buf.clear();
            juce::MidiBuffer empty;
            proc.processBlock (buf, empty);
        }
        check (! proc.keyboardState.isNoteOn (1, 64), "keyboard state clears after note-off");

        proc.clickState.noteOn (1, 72, 0.8f);
        juce::MidiBuffer empty;
        buf.clear();
        proc.processBlock (buf, empty);
        check (proc.keyboardState.isNoteOn (1, 72), "a clicked key is injected and displayed");
        proc.clickState.noteOff (1, 72, 0.0f);
        buf.clear();
        proc.processBlock (buf, empty);
        for (int b = 0; b < 80; ++b)
        {
            buf.clear();
            proc.processBlock (buf, empty);
        }
        check (! proc.keyboardState.isNoteOn (1, 72), "a clicked key clears on release");
    }

    juce::MemoryBlock state;
    proc.getStateInformation (state);
    check (state.getSize() > 32, "session state is written");

    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("m1arp_state_test.m1arp");
    file.deleteFile();
    proc.savePreset (file);
    check (file.existsAsFile() && file.getSize() > 0, "a .m1arp preset is written");

    proc.setCurrentProgram (std::min (9, proc.getNumPrograms() - 1));
    proc.setStateInformation (state.getData(), (int) state.getSize());
    check (proc.getCurrentProgram() == 0, "setStateInformation restores the program");
    check (renderRms (proc) > 1.0e-3, "restored state still produces sound");

    proc.setCurrentProgram (std::min (6, proc.getNumPrograms() - 1));
    proc.loadPreset (file);
    check (proc.getCurrentProgram() == 0, "loadPreset restores the saved program");
    check (renderRms (proc) > 1.0e-3, "a loaded preset still produces sound");
    file.deleteFile();

    if (auto* parameter = proc.apvts.getParameter ("arp_on"))
        parameter->setValueNotifyingHost (1.0f);
    proc.session().programs[proc.getCurrentProgram()].arp.octaves = 2;
    proc.session().programs[proc.getCurrentProgram()].arp.on = 1;
    proc.publishSession();
    check (renderRms (proc) > 1.0e-3, "arp on with transport stopped produces sound");

    proc.clickState.noteOn (1, 60, 0.8f);
    bool sawOctave = false;
    juce::AudioBuffer<float> buf (2, 512);
    for (int b = 0; b < 80 && ! sawOctave; ++b)
    {
        buf.clear();
        juce::MidiBuffer empty;
        proc.processBlock (buf, empty);
        sawOctave = proc.keyboardState.isNoteOn (1, 72);
    }
    check (sawOctave, "keyboard displays the arp octave");
    proc.clickState.noteOff (1, 60, 0.0f);

    return failures == 0 ? 0 : 1;
}

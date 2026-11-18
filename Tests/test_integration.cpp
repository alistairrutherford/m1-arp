// Integration test: the real processor renders a factory program and a preset
// round-trip still sounds; the editor's Info View describes every control on every page.
// Run via run_integration.sh after a Release build.
#include "PluginProcessor.h"
#include "PluginEditor.h"
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
    for (int b = 0; b < 200; ++b)
    {
        buf.clear();
        juce::MidiBuffer empty;
        proc.processBlock (buf, empty);
    }

    // ---- The Info View ------------------------------------------------------------------------
    std::cout << "Info View\n";
    {
        std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
        auto* editor = dynamic_cast<M1ArpAudioProcessorEditor*> (base.get());
        check (editor != nullptr, "the editor opens");
        if (editor != nullptr)
        {
            bool brief = true;
            for (const auto& k : M1ArpAudioProcessorEditor::infoKeys())
                brief = brief && M1ArpAudioProcessorEditor::infoText (k).length() <= 200
                        && M1ArpAudioProcessorEditor::infoTitle (k).length() <= 24;
            check (brief, "entries stay brief (titles at most 24 characters, text at most 200)");

            // Every control on every page, in PROG and COMBI mode, a Double program and a Drums
            // one, has an entry: everything tagged with a key, and the header.
            juce::StringArray missing;
            int controls = 0;
            std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
            {
                const auto id = c.getComponentID();
                const bool control = dynamic_cast<juce::Slider*> (&c) != nullptr || dynamic_cast<juce::ComboBox*> (&c) != nullptr
                                  || dynamic_cast<juce::Button*> (&c) != nullptr;
                if (control && c.isVisible())
                {
                    ++controls;
                    // The nearest component with a key (the control, or the cell it is part of) must
                    // have its own entry, not just its section's.
                    juce::String key;
                    for (auto* p = &c; p != nullptr && p != editor && key.isEmpty(); p = p->getParentComponent())
                        key = p->getComponentID();
                    if (M1ArpAudioProcessorEditor::infoTitle (key).isEmpty())
                        missing.addIfNotAlreadyThere ((key.isNotEmpty() ? key : c.getName()) + " (no key)");
                }
                for (auto* child : c.getChildren())
                    walk (*child);
            };
            auto& s = proc.session();
            int doubleProgram = -1, drumsProgram = -1;
            for (int i = 0; i < s.numPrograms; ++i)
            {
                if (doubleProgram < 0 && s.programs[i].mode == (int) m1::OscMode::Double) doubleProgram = i;
                if (drumsProgram < 0 && s.programs[i].mode == (int) m1::OscMode::Drums) drumsProgram = i;
            }
            for (const int combi : { 0, 1 })
                for (const int program : { 0, doubleProgram, drumsProgram })
                {
                    if (program < 0)
                        continue;
                    proc.setCombiMode (combi == 1);
                    proc.setCurrentProgram (program);
                    for (int pg = 0; pg < M1ArpAudioProcessorEditor::numPages(); ++pg)
                    {
                        editor->showPage (pg);
                        walk (*editor);
                    }
                }
            proc.setCombiMode (false);
            proc.setCurrentProgram (0);
            check (missing.isEmpty(), ("every control on every page has an Info View entry (" + juce::String (controls) + " looked at)"
                                       + (missing.isEmpty() ? juce::String() : "; none for: " + missing.joinIntoString (", "))).toRawUTF8());

            // Pointing: a knob shows its entry, the strip is nothing, leaving shows the hint.
            editor->showPage (3);   // Filter
            juce::Point<int> cutoffAt;
            for (int y = 0; y < editor->getHeight() && cutoffAt.isOrigin(); y += 2)
                for (int x = 0; x < editor->getWidth() && cutoffAt.isOrigin(); x += 2)
                    if (editor->infoKeyAt ({ x, y }) == "Filter:VDF/Cutoff")
                        cutoffAt = { x + 8, y + 8 };
            editor->showInfoFor (editor->infoKeyAt (cutoffAt));
            check (editor->infoView().title == "Cutoff" && editor->infoView().text.contains ("16 kHz"), "pointing at the VDF's Cutoff shows its entry");
            const auto strip = editor->infoView().getBounds();
            check (strip.getBottom() == editor->getHeight() - 10 && editor->infoKeyAt (strip.getCentre()).isEmpty(),
                   "the strip runs along the bottom and isn't itself something to point at");
            editor->showInfoFor ({});
            check (editor->infoView().title.isEmpty() && editor->infoView().text == M1ArpAudioProcessorEditor::infoHint(), "nothing pointed at shows the hint");

            // The README's screenshot: the Arp page, pointing at the arp's Mode.
            editor->showPage (6);
            juce::Point<int> modeAt;
            for (int y = 0; y < editor->getHeight() && modeAt.isOrigin(); y += 2)
                for (int x = 0; x < editor->getWidth() && modeAt.isOrigin(); x += 2)
                    if (editor->infoKeyAt ({ x, y }) == "Arp:Arpeggiator/Mode")
                        modeAt = { x + 8, y + 8 };
            editor->showInfoFor (editor->infoKeyAt (modeAt));
            check (editor->infoView().title == "Mode", "pointing at the arp's Mode shows its entry");
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);   // as the README's, at 2x
            auto out = juce::File::getCurrentWorkingDirectory().getChildFile ("Tests/build/editor-info.png");
            out.deleteFile();
            juce::FileOutputStream stream (out);
            juce::PNGImageFormat().writeImageToStream (image, stream);
            std::cout << "  (wrote " << out.getFullPathName() << ")\n";
        }
    }

    return failures == 0 ? 0 : 1;
}

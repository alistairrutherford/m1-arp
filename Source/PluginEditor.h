#pragma once

#include "PluginProcessor.h"
#include "EditorComponents.h"

class M1ArpAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit M1ArpAudioProcessorEditor (M1ArpAudioProcessor&);
    ~M1ArpAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // The Info View (a strip along the bottom, as NoteCap's and Live's own): the key of the control
    // at `p` (editor coordinates; "Filter:VDF EG/Attack", "header.master"…; empty over nothing
    // described, the strip itself included), its entry, and showing one as hovering does.
    juce::String infoKeyAt (juce::Point<int> p);
    void showInfoFor (const juce::String& key);
    static juce::String infoTitle (const juce::String& key);
    static juce::String infoText (const juce::String& key);
    static juce::StringArray infoKeys();
    static juce::String infoHint();
    const m1ui::InfoView& infoView() const noexcept { return info; }

    // For tests: show a page (0 = Play … 9 = Kit) at once.
    void showPage (int index);
    static int numPages();

    void mouseEnter (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    // The keyboard shows the notes the voices receive (after the arp); clicks
    // go in ahead of the arp through the processor's click state.
    struct ArpDisplayKeyboard : public juce::MidiKeyboardComponent
    {
        ArpDisplayKeyboard (juce::MidiKeyboardState& display, juce::MidiKeyboardState& clicks)
            : juce::MidiKeyboardComponent (display, juce::MidiKeyboardComponent::horizontalKeyboard),
              clickState (clicks) {}

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto nv = getNoteAndVelocityAtPosition (e.position);
            if (nv.note >= 0)
            {
                clickState.noteOn (1, nv.note, juce::jlimit (0.2f, 1.0f, nv.velocity));
                clickedNote = nv.note;
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const auto nv = getNoteAndVelocityAtPosition (e.position);
            if (nv.note != clickedNote)
            {
                if (clickedNote >= 0)
                    clickState.noteOff (1, clickedNote, 0.0f);
                if (nv.note >= 0)
                    clickState.noteOn (1, nv.note, juce::jlimit (0.2f, 1.0f, nv.velocity));
                clickedNote = nv.note;
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (clickedNote >= 0)
            {
                clickState.noteOff (1, clickedNote, 0.0f);
                clickedNote = -1;
            }
        }

        juce::MidiKeyboardState& clickState;
        int clickedNote = -1;
    };

    // LCD-style program / combination display.
    struct Display : public juce::Component
    {
        juce::Label name;
        juce::String topLine, infoLine, rightTop;
        bool loading = false;
        std::function<void()> onBrowse;

        Display();
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseUp (const juce::MouseEvent&) override;
    };

    struct Meter : public juce::Component
    {
        float level = 0.0f, held = 0.0f;
        void push (float peak);
        void paint (juce::Graphics&) override;
    };

    void timerCallback() override;
    void refreshHeader();
    void refreshDisplay();
    void requestRebuild();
    void rebuild();
    void edited();
    void step (int delta);
    void browse();
    void commitName();
    void chooseFile (bool save);
    m1::Program* currentProgram();
    m1::Combination* currentCombi();
    m1::DrumKit* currentKit();

    // Page builders
    void buildPlay (m1ui::Page&);
    void buildOsc (m1ui::Page&, m1::Program&);
    void buildPitch (m1ui::Page&, m1::Program&);
    void buildFilter (m1ui::Page&, m1::Program&);
    void buildAmp (m1ui::Page&, m1::Program&);
    void buildMod (m1ui::Page&, m1::Program&);
    void buildArp (m1ui::Page&, m1::Program&);
    void buildFx (m1ui::Page&, m1::Program&);
    void buildCombi (m1ui::Page&);
    void buildKit (m1ui::Page&);
    void addOscSelector (m1ui::Page&, const m1::Program&);
    void tagPage (m1ui::Page&);
    void updateInfo (const juce::MouseEvent&);

    M1ArpAudioProcessor& processor;
    m1ui::Look look;
    Display display;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TextButton progButton { "PROG" }, combiButton { "COMBI" };
    juce::TextButton saveButton { "Save" }, loadButton { "Load" };
    m1ui::Knob master { "Master" };
    Meter meter;
    juce::TextButton tabs[10];
    juce::Viewport viewport;
    m1ui::Page* page = nullptr;
    ArpDisplayKeyboard keyboard;
    m1ui::InfoView info;
    std::unique_ptr<juce::FileChooser> chooser;
    int pageIndex = 0;
    int oscEdit = 0;
    int shownRevision = -1;
    bool shownLoading = true;
    bool rebuildPending = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (M1ArpAudioProcessorEditor)
};

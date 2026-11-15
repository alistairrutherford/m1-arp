#pragma once

// Editor building blocks: look-and-feel, knobs, choice boxes, sections,
// envelope graphs and the arp step grid. Header-only; included by
// PluginEditor.cpp only.

#include <JuceHeader.h>
#include "Model.h"

namespace m1ui
{

namespace colours
{
inline const juce::Colour background { 0xff17191d };
inline const juce::Colour panel      { 0xff21242a };
inline const juce::Colour panelEdge  { 0xff2f333b };
inline const juce::Colour well       { 0xff121418 };
inline const juce::Colour text       { 0xffe9e5dc };
inline const juce::Colour dim        { 0xff8b9099 };
inline const juce::Colour accent     { 0xfff0a640 };
inline const juce::Colour accent2    { 0xff57c7b8 };
inline const juce::Colour lcd        { 0xff0d2421 };
inline const juce::Colour lcdText    { 0xff9df2dc };
inline const juce::Colour lcdDim     { 0xff4f8f81 };
}

inline juce::Font font (float size, bool bold = false)
{
    auto opts = juce::FontOptions (size);
    return juce::Font (bold ? opts.withStyle ("Bold") : opts);
}

inline juce::String signedText (int v) { return v > 0 ? "+" + juce::String (v) : juce::String (v); }
inline juce::String noteText (int n) { return juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, n), true, true, 4); }

// --------------------------------------------------------------- LookAndFeel

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        using namespace colours;
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));
        setColour (juce::Slider::thumbColourId, accent);
        setColour (juce::Slider::trackColourId, accent);
        setColour (juce::Slider::backgroundColourId, well);
        setColour (juce::ComboBox::backgroundColourId, well);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, panelEdge);
        setColour (juce::ComboBox::arrowColourId, dim);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1f2227));
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.85f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::PopupMenu::headerTextColourId, accent);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::TextEditor::backgroundColourId, well);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
        setColour (juce::TextEditor::outlineColourId, panelEdge);
        setColour (juce::TextEditor::focusedOutlineColourId, accent);
        setColour (juce::CaretComponent::caretColourId, accent);
        setColour (juce::ScrollBar::thumbColourId, panelEdge.brighter (0.2f));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff2a2e35));
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, panelEdge);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider& s) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float ring = juce::jmax (3.0f, r * 0.16f);
        const float angle = start + pos * (end - start);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const auto col = s.isEnabled() ? s.findColour (juce::Slider::thumbColourId) : colours::dim;

        juce::Path track;
        track.addCentredArc (c.x, c.y, r - ring * 0.5f, r - ring * 0.5f, 0.0f, start, end, true);
        g.setColour (colours::well);
        g.strokePath (track, juce::PathStrokeType (ring, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float from = bipolar ? (start + end) * 0.5f : start;
        if (std::abs (angle - from) > 0.001f)
        {
            juce::Path value;
            value.addCentredArc (c.x, c.y, r - ring * 0.5f, r - ring * 0.5f, 0.0f,
                                 juce::jmin (from, angle), juce::jmax (from, angle), true);
            g.setColour (col);
            g.strokePath (value, juce::PathStrokeType (ring, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float knobR = r - ring - 2.5f;
        juce::ColourGradient grad (juce::Colour (0xff3a3f48), c.x, c.y - knobR,
                                   juce::Colour (0xff22252b), c.x, c.y + knobR, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

        juce::Path pointer;
        const float pw = juce::jmax (2.0f, knobR * 0.14f);
        pointer.addRoundedRectangle (-pw * 0.5f, -knobR + 2.0f, pw, knobR * 0.55f, pw * 0.5f);
        g.setColour (s.isEnabled() ? colours::text : colours::dim);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos,
                           float maxPos, const juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearBar)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
            return;
        }
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        g.setColour (colours::well);
        g.fillRoundedRectangle (r, 3.0f);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const float zero = bipolar ? (float) s.valueToProportionOfLength (0.0) * w + x : (float) x;
        const float left = juce::jmin (zero, pos), right = juce::jmax (zero, pos);
        g.setColour ((s.isEnabled() ? s.findColour (juce::Slider::thumbColourId) : colours::dim).withAlpha (0.35f));
        g.fillRoundedRectangle (juce::Rectangle<float> (left, r.getY() + 1.0f, juce::jmax (1.5f, right - left), r.getHeight() - 2.0f), 2.0f);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (12.0f));
        l->setColour (juce::Label::textColourId, colours::text);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundWhenEditingColourId, colours::well);
        l->setColour (juce::Label::outlineWhenEditingColourId, colours::accent);
        return l;
    }

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        auto* slider = dynamic_cast<juce::Slider*> (label.getParentComponent());
        if (slider == nullptr)
        {
            LookAndFeel_V4::drawLabel (g, label);
            return;
        }
        // Slider value boxes: a quiet well, no outline.
        const bool bar = slider->getSliderStyle() == juce::Slider::LinearBar;
        if (! bar)
        {
            g.setColour (colours::well.withAlpha (0.7f));
            g.fillRoundedRectangle (label.getLocalBounds().toFloat().reduced (4.0f, 0.0f), 3.0f);
        }
        if (! label.isBeingEdited())
        {
            g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (slider->isEnabled() ? 1.0f : 0.5f));
            g.setFont (font (bar ? 12.0f : 11.5f));
            g.drawFittedText (label.getText(), label.getLocalBounds().reduced (2, 0),
                              bar ? juce::Justification::centred : juce::Justification::centred, 1, 0.8f);
        }
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        auto base = on ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
        if (over && ! on) base = base.brighter (0.12f);
        if (down) base = base.darker (0.15f);
        g.setColour (base);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? base.brighter (0.2f) : colours::panelEdge);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (13.0f, h * 0.55f), true); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.hasKeyboardFocus (true) ? colours::accent : box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path arrow;
        const float ax = w - 13.0f, ay = h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (box.isEnabled() ? colours::dim : colours::dim.withAlpha (0.4f));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
    juce::Font getPopupMenuFont() override { return font (13.0f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};

// -------------------------------------------------------------------- Cells

// Base for anything placed in a Section grid. `span` is in grid columns.
struct Cell : public juce::Component
{
    int span = 1;
    int cellHeight = 86;
};

// Rotary knob with a title above and a value below. Bound either to an int
// in the edit session (setter callback) or to an APVTS parameter.
struct Knob : public Cell
{
    juce::Label title;
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    Knob (const juce::String& name, const juce::String& tip = {})
    {
        title.setText (name, juce::dontSendNotification);
        title.setFont (font (11.5f, true));
        title.setColour (juce::Label::textColourId, colours::dim);
        title.setJustificationType (juce::Justification::centred);
        title.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (title);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (160);
        slider.setVelocityBasedMode (false);
        if (tip.isNotEmpty())
            slider.setTooltip (tip);
        addAndMakeVisible (slider);
    }

    void bindInt (int* value, int lo, int hi, std::function<void()> changed,
                  std::function<juce::String (int)> format = nullptr, int defaultValue = INT_MIN)
    {
        slider.setRange (lo, hi, 1);
        slider.getProperties().set ("bipolar", lo < 0 && hi > 0);
        if (format)
            slider.textFromValueFunction = [format] (double v) { return format ((int) std::lround (v)); };
        else if (lo < 0)
            slider.textFromValueFunction = [] (double v) { return signedText ((int) std::lround (v)); };
        slider.setValue (value != nullptr ? *value : 0, juce::dontSendNotification);
        slider.updateText();
        slider.setDoubleClickReturnValue (true, defaultValue != INT_MIN ? defaultValue : juce::jlimit (lo, hi, 0));
        slider.onValueChange = [this, value, changed]
        {
            if (value != nullptr)
                *value = (int) std::lround (slider.getValue());
            if (changed)
                changed();
        };
    }

    void bindParam (juce::AudioProcessorValueTreeState& state, const juce::String& id,
                    std::function<juce::String (double)> format = nullptr)
    {
        auto* p = state.getParameter (id);
        if (p != nullptr)
        {
            const auto range = p->getNormalisableRange();
            slider.getProperties().set ("bipolar", range.start < 0.0f && range.end > 0.0f);
        }
        // The attachment installs the parameter's own text functions, so the
        // custom formatter goes on afterwards.
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
        if (format)
        {
            slider.textFromValueFunction = format;
            slider.updateText();
        }
        if (p != nullptr)
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    }

    void setAccent (juce::Colour c) { slider.setColour (juce::Slider::thumbColourId, c); }

    void resized() override
    {
        auto r = getLocalBounds();
        title.setBounds (r.removeFromTop (15));
        slider.setBounds (r.reduced (2, 0));
    }
};

// A drop-down with a title above.
struct Choice : public Cell
{
    juce::Label title;
    juce::ComboBox box;

    Choice (const juce::String& name, int spanIn = 2)
    {
        span = spanIn;
        cellHeight = 52;
        title.setText (name, juce::dontSendNotification);
        title.setFont (font (11.5f, true));
        title.setColour (juce::Label::textColourId, colours::dim);
        title.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (title);
        addAndMakeVisible (box);
    }

    void bind (int* value, const juce::StringArray& items, int origin, std::function<void()> changed)
    {
        box.clear (juce::dontSendNotification);
        for (int i = 0; i < items.size(); ++i)
            box.addItem (items[i], i + 1);
        if (value != nullptr)
            box.setSelectedId (*value - origin + 1, juce::dontSendNotification);
        box.onChange = [this, value, origin, changed]
        {
            if (value != nullptr && box.getSelectedId() > 0)
                *value = box.getSelectedId() - 1 + origin;
            if (changed)
                changed();
        };
    }

    void resized() override
    {
        auto r = getLocalBounds();
        title.setBounds (r.removeFromTop (16).withTrimmedLeft (2));
        box.setBounds (r.removeFromTop (26));
    }
};

// A row of mutually exclusive buttons bound to an int.
struct Segmented : public Cell
{
    juce::Label title;
    juce::OwnedArray<juce::TextButton> buttons;

    Segmented (const juce::String& name, const juce::StringArray& items, int* value, int origin,
               std::function<void()> changed, int spanIn = 2)
    {
        span = spanIn;
        cellHeight = 52;
        title.setText (name, juce::dontSendNotification);
        title.setFont (font (11.5f, true));
        title.setColour (juce::Label::textColourId, colours::dim);
        addAndMakeVisible (title);
        for (int i = 0; i < items.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (items[i]));
            b->setClickingTogglesState (false);
            b->setToggleState (value != nullptr && *value - origin == i, juce::dontSendNotification);
            b->onClick = [this, i, value, origin, changed]
            {
                if (value != nullptr)
                    *value = i + origin;
                for (int k = 0; k < buttons.size(); ++k)
                    buttons[k]->setToggleState (k == i, juce::dontSendNotification);
                if (changed)
                    changed();
            };
            addAndMakeVisible (b);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        title.setBounds (r.removeFromTop (16).withTrimmedLeft (2));
        r = r.removeFromTop (26);
        const int w = r.getWidth() / juce::jmax (1, buttons.size());
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setBounds ((i == buttons.size() - 1 ? r : r.removeFromLeft (w)).reduced (1, 0));
    }
};

// On/off switch bound to an int (or to an APVTS parameter).
struct Toggle : public Cell
{
    juce::Label title;
    juce::TextButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

    explicit Toggle (const juce::String& name, const juce::String& caption = "On")
    {
        cellHeight = 52;
        title.setText (name, juce::dontSendNotification);
        title.setFont (font (11.5f, true));
        title.setColour (juce::Label::textColourId, colours::dim);
        title.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (title);
        onText = caption;
        button.setButtonText ("Off");
        button.setClickingTogglesState (true);
        button.onStateChange = [this] { syncText(); };
        addAndMakeVisible (button);
    }

    // Toggle state set with dontSendNotification does not fire onStateChange,
    // so every path that sets the state also refreshes the caption.
    void syncText() { button.setButtonText (button.getToggleState() ? onText : juce::String ("Off")); }

    juce::String onText;

    void bind (int* value, std::function<void()> changed)
    {
        button.setToggleState (value != nullptr && *value != 0, juce::dontSendNotification);
        syncText();
        button.onClick = [this, value, changed]
        {
            if (value != nullptr)
                *value = button.getToggleState() ? 1 : 0;
            if (changed)
                changed();
        };
    }

    void bindParam (juce::AudioProcessorValueTreeState& state, const juce::String& id)
    {
        // arp_on is an int parameter, so drive it by hand rather than a ButtonAttachment.
        auto* p = state.getParameter (id);
        button.setToggleState (p != nullptr && p->getValue() > 0.5f, juce::dontSendNotification);
        syncText();
        button.onClick = [this, p]
        {
            if (p != nullptr)
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (button.getToggleState() ? 1.0f : 0.0f);
                p->endChangeGesture();
            }
        };
        paramToWatch = p;
    }

    void refreshFromParam()
    {
        if (paramToWatch != nullptr)
        {
            button.setToggleState (paramToWatch->getValue() > 0.5f, juce::dontSendNotification);
            syncText();
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        title.setBounds (r.removeFromTop (16));
        button.setBounds (r.removeFromTop (26).reduced (6, 0));
    }

    juce::RangedAudioParameter* paramToWatch = nullptr;
};

// ------------------------------------------------------------ Envelope view

struct EnvelopeView : public Cell
{
    enum Kind { Amp, Filter, Pitch };

    EnvelopeView (Kind k, const m1::Eg4* e, const m1::PitchEg* p, int spanIn)
        : kind (k), eg (e), peg (p)
    {
        span = spanIn;
        cellHeight = 104;
        setInterceptsMouseClicks (false, false);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f, 4.0f);
        g.setColour (colours::well);
        g.fillRoundedRectangle (r, 6.0f);
        r = r.reduced (10.0f, 10.0f);
        const bool bipolar = kind != Amp;
        const float zeroY = bipolar ? r.getCentreY() : r.getBottom();
        g.setColour (colours::panelEdge);
        g.drawHorizontalLine ((int) zeroY, r.getX(), r.getRight());

        auto yFor = [&] (int level)
        {
            const float v = juce::jlimit (-99, 99, level) / 99.0f;
            return bipolar ? r.getCentreY() - v * r.getHeight() * 0.5f : r.getBottom() - v * r.getHeight();
        };
        auto wFor = [] (int t) { return 0.18f + 0.82f * juce::jlimit (0, 99, t) / 99.0f; };

        struct Pt { float t; int level; };
        std::vector<Pt> pts;
        float sustainAt = 0.0f;
        if (kind == Pitch && peg != nullptr)
        {
            pts = { { 0.0f, peg->start }, { wFor (peg->at), peg->al }, { wFor (peg->dt), 0 } };
            sustainAt = (float) pts.size();
            pts.push_back ({ 0.9f, 0 });
            pts.push_back ({ wFor (peg->rt), peg->rl });
        }
        else if (eg != nullptr)
        {
            pts = { { 0.0f, 0 }, { wFor (eg->at), eg->al }, { wFor (eg->dt), eg->bp }, { wFor (eg->st), eg->sl } };
            sustainAt = (float) pts.size();
            pts.push_back ({ 0.9f, eg->sl });
            pts.push_back ({ wFor (eg->rt), eg->rl });
        }
        if (pts.empty())
            return;
        float total = 0.0f;
        for (auto& p : pts) total += p.t;
        const float scale = r.getWidth() / juce::jmax (0.01f, total);

        juce::Path line, fill;
        float x = r.getX();
        std::vector<juce::Point<float>> nodes;
        for (size_t i = 0; i < pts.size(); ++i)
        {
            x += pts[i].t * scale;
            const juce::Point<float> p (x, yFor (pts[i].level));
            if (i == 0) { line.startNewSubPath (p); fill.startNewSubPath (p.x, zeroY); fill.lineTo (p); }
            else { line.lineTo (p); fill.lineTo (p); }
            nodes.push_back (p);
        }
        fill.lineTo (x, zeroY);
        fill.closeSubPath();

        const auto col = kind == Filter ? colours::accent2 : (kind == Pitch ? juce::Colour (0xffb59cf0) : colours::accent);
        g.setColour (col.withAlpha (0.16f));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Sustain region
        const int s = (int) sustainAt;
        if (s >= 1 && s < (int) nodes.size())
        {
            g.setColour (colours::dim.withAlpha (0.5f));
            const float dash[] = { 3.0f, 3.0f };
            g.drawDashedLine (juce::Line<float> (nodes[(size_t) s - 1].x, r.getY(), nodes[(size_t) s - 1].x, r.getBottom()), dash, 2, 1.0f);
            g.drawDashedLine (juce::Line<float> (nodes[(size_t) s].x, r.getY(), nodes[(size_t) s].x, r.getBottom()), dash, 2, 1.0f);
            g.setFont (font (10.0f));
            g.drawText ("note off", juce::Rectangle<float> (nodes[(size_t) s].x + 3.0f, r.getY() - 2.0f, 60.0f, 12.0f),
                        juce::Justification::centredLeft);
        }
        g.setColour (colours::text);
        for (auto& p : nodes)
            g.fillEllipse (p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);
    }

    Kind kind;
    const m1::Eg4* eg = nullptr;
    const m1::PitchEg* peg = nullptr;
};

// A caption cell (used for notes inside a section).
struct Caption : public Cell
{
    juce::String text;
    Caption (const juce::String& t, int spanIn, int height = 40) : text (t) { span = spanIn; cellHeight = height; }
    void paint (juce::Graphics& g) override
    {
        g.setColour (colours::dim);
        g.setFont (font (12.0f));
        g.drawFittedText (text, getLocalBounds().reduced (4, 2), juce::Justification::topLeft, 4);
    }
};

// ------------------------------------------------------------------ Section

struct Section : public juce::Component
{
    static constexpr int kCol = 70;
    static constexpr int kTitle = 24;

    juce::String title;
    int columns = 4;
    juce::Colour tint = colours::accent;
    juce::OwnedArray<Cell> cells;

    Section (const juce::String& t, int cols) : title (t), columns (cols) {}

    template <typename T>
    T& add (T* cell)
    {
        cells.add (cell);
        addAndMakeVisible (cell);
        return *cell;
    }

    int preferredWidth() const { return columns * kCol + 20; }

    int preferredHeight() const
    {
        int y = kTitle + 6, x = 0, rowH = 0;
        for (auto* c : cells)
        {
            if (x + c->span > columns && x > 0)
            {
                y += rowH + 6;
                x = 0;
                rowH = 0;
            }
            x += c->span;
            rowH = juce::jmax (rowH, c->cellHeight);
        }
        return y + rowH + 12;
    }

    void resized() override
    {
        int y = kTitle + 6, x = 0, rowH = 0;
        const int left = 10;
        for (auto* c : cells)
        {
            if (x + c->span > columns && x > 0)
            {
                y += rowH + 6;
                x = 0;
                rowH = 0;
            }
            c->setBounds (left + x * kCol, y, c->span * kCol - 4, c->cellHeight);
            x += c->span;
            rowH = juce::jmax (rowH, c->cellHeight);
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (colours::panelEdge);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
        g.setColour (tint);
        g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, 9.0f, 3.0f, 12.0f), 1.5f);
        g.setColour (colours::text);
        g.setFont (font (12.5f, true));
        g.drawText (title.toUpperCase(), juce::Rectangle<float> (21.0f, 4.0f, r.getWidth() - 30.0f, 22.0f),
                    juce::Justification::centredLeft);
    }
};

// A section with a header row and fixed-width columns, one row per item.
struct TableSection : public juce::Component
{
    struct Column { juce::String name; int width; };

    juce::String title;
    std::vector<Column> columns;
    juce::OwnedArray<juce::Component> owned;
    std::vector<std::vector<juce::Component*>> rows;
    std::vector<float> rowAlpha;
    static constexpr int kRow = 26;

    TableSection (const juce::String& t, std::vector<Column> cols) : title (t), columns (std::move (cols)) {}

    template <typename T>
    T* make (T* c)
    {
        owned.add (c);
        addAndMakeVisible (c);
        return c;
    }

    void addRow (std::vector<juce::Component*> cells, float alpha = 1.0f)
    {
        for (auto* c : cells)
            if (c != nullptr)
                c->setAlpha (alpha);
        rows.push_back (std::move (cells));
        rowAlpha.push_back (alpha);
    }

    int preferredWidth() const
    {
        int w = 20;
        for (auto& c : columns) w += c.width + 4;
        return w;
    }

    int preferredHeight() const { return 24 + 24 + (int) rows.size() * (kRow + 3) + 10; }

    void resized() override
    {
        int y = 48;
        for (auto& row : rows)
        {
            int x = 10;
            for (size_t i = 0; i < row.size() && i < columns.size(); ++i)
            {
                if (row[i] != nullptr)
                    row[i]->setBounds (x, y, columns[i].width, kRow);
                x += columns[i].width + 4;
            }
            y += kRow + 3;
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (colours::panelEdge);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
        g.setColour (colours::accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, 9.0f, 3.0f, 12.0f), 1.5f);
        g.setColour (colours::text);
        g.setFont (font (12.5f, true));
        g.drawText (title.toUpperCase(), juce::Rectangle<float> (21.0f, 4.0f, r.getWidth() - 30.0f, 22.0f),
                    juce::Justification::centredLeft);
        g.setFont (font (11.0f, true));
        g.setColour (colours::dim);
        int x = 10;
        for (auto& c : columns)
        {
            g.drawText (c.name, x, 28, c.width, 16, juce::Justification::centredLeft);
            x += c.width + 4;
        }
        for (size_t i = 0; i < rows.size(); ++i)
            if (i % 2 == 1)
            {
                g.setColour (juce::Colours::white.withAlpha (0.025f));
                g.fillRect (6, 48 + (int) i * (kRow + 3) - 1, getWidth() - 12, kRow + 2);
            }
    }
};

// Compact horizontal value bar for table cells.
struct BarSlider : public juce::Slider
{
    BarSlider (int* value, int lo, int hi, std::function<void()> changed,
               std::function<juce::String (int)> format = nullptr)
        : juce::Slider (juce::Slider::LinearBar, juce::Slider::TextBoxLeft)
    {
        setRange (lo, hi, 1);
        getProperties().set ("bipolar", lo < 0 && hi > 0);
        if (format)
            textFromValueFunction = [format] (double v) { return format ((int) std::lround (v)); };
        else if (lo < 0)
            textFromValueFunction = [] (double v) { return signedText ((int) std::lround (v)); };
        setValue (value != nullptr ? *value : 0, juce::dontSendNotification);
        updateText();
        setMouseDragSensitivity (200);
        setDoubleClickReturnValue (true, juce::jlimit (lo, hi, 0));
        onValueChange = [this, value, changed]
        {
            if (value != nullptr)
                *value = (int) std::lround (getValue());
            if (changed)
                changed();
        };
    }
};

// ------------------------------------------------------------ Arp step grid

struct StepGrid : public Cell
{
    explicit StepGrid (m1::ArpSettings* a, std::function<void()> changed, int spanIn)
        : arp (a), onChange (std::move (changed))
    {
        span = spanIn;
        cellHeight = 190;
    }

    void paint (juce::Graphics& g) override
    {
        if (arp == nullptr)
            return;
        const auto area = getLocalBounds().toFloat();
        const float colW = area.getWidth() / m1::kArpSteps;
        for (int i = 0; i < m1::kArpSteps; ++i)
        {
            const auto& st = arp->steps[i];
            auto col = juce::Rectangle<float> (area.getX() + i * colW, area.getY(), colW, area.getHeight()).reduced (3.0f, 0.0f);
            if ((i / 4) % 2 == 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.025f));
                g.fillRect (col.expanded (3.0f, 0.0f));
            }
            // LED
            auto led = ledArea (col);
            g.setColour (st.on ? colours::accent : colours::well);
            g.fillRoundedRectangle (led, 3.0f);
            g.setColour (st.on ? colours::accent.brighter (0.3f) : colours::panelEdge);
            g.drawRoundedRectangle (led, 3.0f, 1.0f);
            // Velocity bar
            auto bar = barArea (col);
            g.setColour (colours::well);
            g.fillRoundedRectangle (bar, 3.0f);
            const float v = juce::jlimit (1, 127, st.velocity) / 127.0f;
            auto fillR = bar.withTop (bar.getBottom() - bar.getHeight() * v);
            g.setColour ((st.on ? colours::accent2 : colours::dim).withAlpha (st.on ? 0.85f : 0.3f));
            g.fillRoundedRectangle (fillR, 3.0f);
            g.setColour (colours::text.withAlpha (st.on ? 0.9f : 0.4f));
            g.setFont (font (10.5f));
            g.drawText (juce::String (st.velocity), bar.withHeight (14.0f), juce::Justification::centred);
            // Octave
            auto oct = octaveArea (col);
            g.setColour (colours::well);
            g.fillRoundedRectangle (oct, 3.0f);
            g.setColour (st.octave == 0 ? colours::dim : colours::text);
            g.setFont (font (11.5f, true));
            g.drawText (st.octave == 0 ? "0" : signedText (st.octave), oct, juce::Justification::centred);
            // Index
            g.setColour (colours::dim);
            g.setFont (font (10.0f));
            g.drawText (juce::String (i + 1), col.withTop (col.getBottom() - 14.0f), juce::Justification::centred);
        }
        g.setColour (colours::dim);
        g.setFont (font (10.0f));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = stepAt (e.position);
        if (i < 0 || arp == nullptr)
            return;
        auto col = columnRect (i);
        if (ledArea (col).expanded (2.0f).contains (e.position))
        {
            arp->steps[i].on = arp->steps[i].on ? 0 : 1;
            dragMode = 0;
        }
        else if (octaveArea (col).contains (e.position))
        {
            const int delta = (e.mods.isPopupMenu() || e.mods.isShiftDown()) ? -1 : 1;
            int o = arp->steps[i].octave + delta;
            if (o > 2) o = -2;
            if (o < -2) o = 2;
            arp->steps[i].octave = o;
            dragMode = 0;
        }
        else
        {
            dragMode = 1;
            setVelocity (e.position);
        }
        changed();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragMode == 1)
        {
            setVelocity (e.position);
            changed();
        }
    }

private:
    juce::Rectangle<float> columnRect (int i) const
    {
        const float colW = getWidth() / (float) m1::kArpSteps;
        return juce::Rectangle<float> (i * colW, 0.0f, colW, (float) getHeight()).reduced (3.0f, 0.0f);
    }
    static juce::Rectangle<float> ledArea (juce::Rectangle<float> c) { return c.withHeight (18.0f); }
    static juce::Rectangle<float> barArea (juce::Rectangle<float> c) { return c.withTrimmedTop (24.0f).withTrimmedBottom (46.0f); }
    static juce::Rectangle<float> octaveArea (juce::Rectangle<float> c) { return c.withTop (c.getBottom() - 40.0f).withHeight (22.0f); }

    int stepAt (juce::Point<float> p) const
    {
        const int i = (int) (p.x / (getWidth() / (float) m1::kArpSteps));
        return (i >= 0 && i < m1::kArpSteps) ? i : -1;
    }

    void setVelocity (juce::Point<float> p)
    {
        const int i = stepAt (p);
        if (i < 0)
            return;
        auto bar = barArea (columnRect (i));
        const float v = 1.0f - (p.y - bar.getY()) / juce::jmax (1.0f, bar.getHeight());
        arp->steps[i].velocity = juce::jlimit (1, 127, (int) std::lround (v * 127.0f));
    }

    void changed()
    {
        repaint();
        if (onChange)
            onChange();
    }

    m1::ArpSettings* arp = nullptr;
    std::function<void()> onChange;
    int dragMode = 0;
};

// --------------------------------------------------------------------- Page

// Lays sections out left to right, wrapping, inside the viewport.
struct Page : public juce::Component
{
    juce::OwnedArray<juce::Component> sections;
    std::vector<EnvelopeView*> graphs;
    std::vector<Toggle*> paramToggles;

    template <typename T>
    T& add (T* s)
    {
        sections.add (s);
        addAndMakeVisible (s);
        return *s;
    }

    void layout (int width)
    {
        int x = 0, y = 0, rowH = 0;
        for (auto* c : sections)
        {
            int w = 0, h = 0;
            if (auto* s = dynamic_cast<Section*> (c)) { w = s->preferredWidth(); h = s->preferredHeight(); }
            else if (auto* t = dynamic_cast<TableSection*> (c)) { w = t->preferredWidth(); h = t->preferredHeight(); }
            const bool full = (bool) c->getProperties().getWithDefault ("fullWidth", false);
            if (full)
                w = width;
            w = juce::jmin (w, width);
            if (x > 0 && x + w > width)
            {
                x = 0;
                y += rowH + 10;
                rowH = 0;
            }
            c->setBounds (x, y, w, h);
            x += w + 10;
            rowH = juce::jmax (rowH, h);
        }
        setSize (width, y + rowH + 4);
    }

    void repaintGraphs()
    {
        for (auto* g : graphs)
            g->repaint();
    }
};

} // namespace m1ui

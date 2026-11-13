#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "Theme.h"

// Small helpers shared by the editor's parts
struct ParamHelp
{
    explicit ParamHelp (CompProcessor& p) : proc (p) {}
    CompProcessor& proc;
    juce::RangedAudioParameter* get (const juce::String& id) const { return proc.apvts.getParameter (id); }
    float value (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void set (const juce::String& id, float v) const { if (auto* p = get (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); }
    void begin (const juce::String& id) const { if (auto* p = get (id)) p->beginChangeGesture(); }
    void end (const juce::String& id) const { if (auto* p = get (id)) p->endChangeGesture(); }
    void setGesture (const juce::String& id, float v) const { begin (id); set (id, v); end (id); }
    // Option(Alt)+click: back to the default value, one gesture = one undo step
    static bool altDown() { return juce::ModifierKeys::currentModifiers.isAltDown(); }
    bool resetDefault (const juce::String& id) const
    {
        auto* p = get (id); if (p == nullptr) return false;
        proc.undo.beginNewTransaction();
        p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture();
        return true;
    }
    void toggleGesture (const juce::String& id) const { if (altDown()) resetDefault (id); else setGesture (id, value (id) > 0.5f ? 0.0f : 1.0f); }
    bool ja() const { return proc.uiState.getProperty ("lang", "ja").toString() == "ja"; }
    juce::String tr (const char* j, const char* e) const { return juce::String::fromUTF8 (ja() ? j : e); }
    int selected() const { return juce::jlimit (0, 5, (int) proc.uiState.getProperty ("selected", 2)); }
    void select (int i) const { proc.uiState.setProperty ("selected", i, nullptr); }
    // hover help, filled by the editor from the component under the mouse
    juce::String hoverTitle, hoverText;
};

// Short help shown in the info card when the mouse is over a control (Learn off)
inline void setHelp (juce::Component& c, const char* title, const char* ja, const char* en)
{
    auto& p = c.getProperties();
    p.set ("helpT", juce::String::fromUTF8 (title)); p.set ("helpJ", juce::String::fromUTF8 (ja)); p.set ("helpE", juce::String::fromUTF8 (en));
}
// ComboBox that jumps to the default on Option+click instead of opening its menu
struct ResetCombo : public juce::ComboBox
{
    std::function<void()> onAlt;
    void mouseDown (const juce::MouseEvent& e) override { skip = e.mods.isAltDown() && onAlt && ! e.mods.isPopupMenu(); if (skip) onAlt(); else juce::ComboBox::mouseDown (e); }
    void mouseUp (const juce::MouseEvent& e) override { if (skip) { skip = false; return; } juce::ComboBox::mouseUp (e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (! skip) juce::ComboBox::mouseDrag (e); }
private:
    bool skip = false;
};
inline void markReset (juce::Component& c) { c.getProperties().set ("resettable", true); }


inline juce::String fmtHz (double f)
{
    if (f >= 1000) return juce::String (f / 1000.0, f >= 10000 ? 1 : 2) + "k";
    return juce::String ((int) std::round (f));
}
inline juce::String fmtDb (double g, int d = 1)
{
    const double r = std::round (g * std::pow (10, d)) / std::pow (10, d);
    return (r > 0 ? "+" : r < 0 ? juce::String::fromUTF8 ("\xe2\x88\x92") : "") + juce::String (std::abs (r), d);
}

// =============================================================================
//  DragValue: drag up / down (Shift = fine), wheel, double-click to type
// =============================================================================
inline void drawLock (juce::Graphics& g, float x, float y)       // small padlock
{
    g.setColour (theme::accent);
    g.fillRoundedRectangle (x, y + 3.5f, 7.0f, 5.0f, 1.0f);
    juce::Path p; p.addCentredArc (x + 3.5f, y + 3.6f, 2.2f, 2.6f, 0.0f, -1.5708f, 1.5708f, true); g.strokePath (p, juce::PathStrokeType (1.2f));
}
class DragValue : public juce::Component
{
public:
    DragValue (ParamHelp& h, juce::String captionText) : ph (h), caption (std::move (captionText)) { setOpaque (false); markReset (*this); }
    std::function<juce::String (float)> format = [] (float v) { return juce::String (v, 2); };
    std::function<float (const juce::String&)> parse = [] (const juce::String& s) { return s.getFloatValue(); };
    // the value colour is read from the current look at paint time (a colour stored at construction would predate the look)
    bool compact = false;                       // caption beside the value instead of under it
    void setParam (const juce::String& id) { if (id != paramID) { paramID = id; getProperties().set ("pid", id); repaint(); } }
    void setCaption (const juce::String& c) { if (c != caption) { caption = c; repaint(); } }
    bool isBusy() const { return dragging || editor != nullptr; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (theme::hilite (over || dragging ? 0.25f : 0.1f));
        g.fillRoundedRectangle (r, 4.0f);
        if (paramID.isNotEmpty() && ph.proc.isLocked (paramID)) drawLock (g, (float) getWidth() - 11.0f, 2.0f);
        if (editor != nullptr) return;
        if (const auto note = lockNote ? lockNote() : juce::String(); note.isNotEmpty())
        {
            g.setColour (theme::faint); g.setFont (theme::ui (9.5f));
            g.drawText (caption, 6, 0, getWidth() - 12, 14, juce::Justification::centredLeft);
            g.setFont (theme::ui (9.0f)); g.drawFittedText (note, 6, 12, getWidth() - 10, getHeight() - 13, juce::Justification::centredLeft, 1, 0.7f);
            return;
        }
        const float v = ph.value (paramID);
        if (compact)
        {
            g.setColour (theme::muted); g.setFont (theme::ui (9.5f));
            g.drawText (caption, 6, 0, getWidth() - 12, 14, juce::Justification::centredLeft);
            g.setColour (theme::text); g.setFont (theme::mono (getWidth() < 90 ? 11.0f : 12.5f, true));
            g.drawText (format (v), 6, 12, getWidth() - 12, getHeight() - 14, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (theme::text); g.setFont (theme::mono (15.0f, true));
            g.drawText (format (v), 0, 0, getWidth(), getHeight() - 14, juce::Justification::centred);
            g.setColour (theme::muted); g.setFont (theme::ui (9.5f));
            g.drawText (caption, 0, getHeight() - 15, getWidth(), 14, juce::Justification::centred);
        }
    }
    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
    std::function<void()> onRightClick;
    std::function<juce::String()> lockNote;      // non-empty = this value is fixed by the chosen method: show the reason instead
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())                              // right-click: lock / unlock (kept through model and preset changes)
        { if (onRightClick) onRightClick(); else if (paramID.isNotEmpty()) { ph.proc.setLocked (paramID, ! ph.proc.isLocked (paramID)); repaint(); } return; }
        if (lockNote && lockNote().isNotEmpty()) return;
        if (e.mods.isAltDown() && ph.resetDefault (paramID)) { repaint(); return; }      // Option+click: default (no drag)
        if (auto* p = ph.get (paramID)) { start = p->getValue(); lastY = e.getScreenPosition().toFloat().y; p->beginChangeGesture(); dragging = true; }
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto* p = ph.get (paramID); if (p == nullptr || ! dragging) return;
        const float sens = e.mods.isShiftDown() ? 0.0008f : 0.005f;      // 200 px = full range, Shift ×1/6
        // screen coordinates: the popup follows the node, so local positions would feed back
        const float y = e.getScreenPosition().toFloat().y;
        start = juce::jlimit (0.0f, 1.0f, start + (lastY - y) * sens); lastY = y;
        p->setValueNotifyingHost (start); repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging) return;
        if (auto* p = ph.get (paramID)) p->endChangeGesture();
        dragging = false; setMouseCursor (juce::MouseCursor::NormalCursor); repaint();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (lockNote && lockNote().isNotEmpty()) return;
        auto* p = ph.get (paramID); if (p == nullptr || w.isInertial || std::abs (w.deltaY) < 0.01f) return;
        const float step = (e.mods.isShiftDown() ? 0.002f : 0.01f) * (w.deltaY > 0 ? 1.0f : -1.0f) * (w.isReversed ? -1.0f : 1.0f);
        p->beginChangeGesture(); p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + step)); p->endChangeGesture(); repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (lockNote && lockNote().isNotEmpty()) return;
        editor = std::make_unique<juce::TextEditor>();
        editor->setFont (theme::mono (13.0f, true)); editor->setJustification (juce::Justification::centred);
        editor->setText (format (ph.value (paramID)).trim(), false); editor->selectAll();
        editor->setBounds (getLocalBounds().withTrimmedBottom (compact ? 0 : 14));
        editor->onReturnKey = [this] { commit (true); };
        editor->onEscapeKey = [this] { commit (false); };
        editor->onFocusLost = [this] { commit (true); };
        addAndMakeVisible (*editor); editor->grabKeyboardFocus();
    }

private:
    void commit (bool apply)
    {
        if (editor == nullptr) return;
        const auto txt = editor->getText();
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<DragValue> (this)] { if (sp != nullptr) { sp->editor.reset(); sp->repaint(); } });
        if (apply && txt.trim().isNotEmpty())
            if (auto* p = ph.get (paramID))
                ph.setGesture (paramID, p->getNormalisableRange().getRange().clipValue (parse (txt.replace (juce::String::fromUTF8 ("\xe2\x88\x92"), "-"))));
    }
    ParamHelp& ph; juce::String paramID, caption;
    float start = 0, lastY = 0; bool over = false, dragging = false;
    std::unique_ptr<juce::TextEditor> editor;
};

inline float parseHz (const juce::String& s) { auto t = s.trim(); float v = t.getFloatValue(); if (t.containsIgnoreCase ("k")) v *= 1000.0f; return v; }

// =============================================================================
//  Seg: a small segmented switch bound to a choice parameter
// =============================================================================
class Seg : public juce::Component
{
public:
    Seg (ParamHelp& h, juce::StringArray labels) : ph (h), items (std::move (labels)) { markReset (*this); }
    void setParam (const juce::String& id) { paramID = id; repaint(); }
    juce::Colour onColour = theme::accent;
    std::function<bool()> enabled = [] { return true; };
    std::function<bool (int)> itemOn = [] (int) { return true; };       // dims single items (still clickable)
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::shade (0.33f)); g.fillRoundedRectangle (r, 4.0f);
        const int sel = paramID.isEmpty() ? 0 : (int) ph.value (paramID);
        const float w = r.getWidth() / (float) items.size();
        for (int i = 0; i < items.size(); ++i)
        {
            const bool en = enabled() && itemOn (i);
            auto c = juce::Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (2.0f);
            if (i == sel) { g.setColour (onColour.withAlpha (en ? 1.0f : 0.4f)); g.fillRoundedRectangle (c, 3.0f); }
            g.setColour (i == sel ? theme::onAccent : (en ? theme::muted : theme::faint));
            g.setFont (theme::ui (10.0f, i == sel));
            g.drawText (items[i], c, juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (paramID.isEmpty() || e.mods.isPopupMenu()) return;
        if (e.mods.isAltDown() && ph.resetDefault (paramID)) { repaint(); return; }
        const int i = juce::jlimit (0, items.size() - 1, (int) (e.x * items.size() / std::max (1, getWidth())));
        ph.proc.undo.beginNewTransaction();
        ph.setGesture (paramID, (float) i); repaint();
    }
private:
    ParamHelp& ph; juce::StringArray items; juce::String paramID;
};

// =============================================================================
//  Floating band editor. Translucent (no blur, so it costs nothing).
//    row 1  ● B3  type  slope  🎧
//    row 2  FREQ  GAIN  Q
//    row 3  PHASE Min|Lin   CH St|M|S
//    row 4  DYN  In|SC  THR  ABOVE  BELOW  meter
//    row 5  TRANSIENT ─────●───── BODY

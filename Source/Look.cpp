#include "Look.h"

using namespace juce;

RememberLookAndFeel::RememberLookAndFeel()
{
    const auto& T = RememberColours::theme();
    setColour (PopupMenu::backgroundColourId, T.card);
    setColour (PopupMenu::textColourId, T.text);
    setColour (PopupMenu::headerTextColourId, T.textDim);
    setColour (PopupMenu::highlightedBackgroundColourId, T.accent);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TextEditor::textColourId, T.text);
    setColour (TextEditor::backgroundColourId, T.inset);
    setColour (TextEditor::highlightColourId, T.accent.withAlpha (0.35f));
    setColour (TextEditor::highlightedTextColourId, T.text);
    setColour (CaretComponent::caretColourId, T.accent);
    setColour (TooltipWindow::backgroundColourId, T.card);
    setColour (TooltipWindow::textColourId, T.text);
    setColour (TooltipWindow::outlineColourId, T.cardBorder);
}

void RememberLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    const auto& T = RememberColours::theme();
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool active = (bool) b.getProperties().getWithDefault ("active", false);
    Colour c = active ? T.accent : (over || down ? T.btnHover : T.btn);
    if (down && active) c = c.darker (0.15f);
    if (! b.isEnabled()) c = c.withAlpha (0.5f);
    g.setColour (c);
    g.fillRoundedRectangle (r, T.radius);
    if (! active)
    {
        g.setColour (T.cardBorder);
        g.drawRoundedRectangle (r, T.radius, 1.0f);
    }
}

void RememberLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const auto& T = RememberColours::theme();
    const bool active = (bool) b.getProperties().getWithDefault ("active", false);
    const float h = (float) b.getHeight();
    g.setFont (Font (rememberFont (jlimit (10.0f, 20.0f, h * 0.36f), true)));
    g.setColour (! b.isEnabled() ? T.btnTextDim : active ? Colours::white : T.btnText);
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), Justification::centred, true);
}

void RememberLookAndFeel::fillTextEditorBackground (Graphics& g, int w, int h, TextEditor&)
{
    const auto& T = RememberColours::theme();
    g.setColour (T.inset);
    g.fillRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, T.radius);
}

void RememberLookAndFeel::drawTextEditorOutline (Graphics& g, int w, int h, TextEditor& e)
{
    const auto& T = RememberColours::theme();
    g.setColour (e.hasKeyboardFocus (true) ? T.accent : T.cardBorder);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, T.radius, 1.0f);
}

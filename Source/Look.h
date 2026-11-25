#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colours.h"

inline juce::FontOptions rememberFont (float size, bool bold = false)
{
    const auto& t = RememberColours::theme();
    juce::FontOptions f (size, bold ? juce::Font::bold : juce::Font::plain);
    if (t.fontName[0] != 0) f = f.withName (t.fontName);
    return f;
}

// Flache Buttons. Property "active" = hervorgehoben (Akzentfarbe)
class RememberLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RememberLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
};

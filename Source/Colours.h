#pragma once
#include <juce_graphics/juce_graphics.h>

// Remember: helles Design im Stil der anderen Plugins (Kopfzeile, orange Rahmen), Standardschrift von JUCE
namespace RememberColours
{
    struct Theme
    {
        const char* name = "Light";
        const char* fontName = "";   // leer = JUCE-Standardschrift
        juce::Colour bgTop, bgBottom, card, cardBorder, inset, text, textDim;
        juce::Colour wave, waveDim, accent, start, playStart, grid, bar, selection, frame;
        juce::Colour btn, btnHover, btnText, btnTextDim;
        float radius = 6.0f;
        float titleSpacing = 12.0f;
    };

    inline const Theme& theme()
    {
        static const Theme t = []
        {
            Theme a;
            auto C = [] (juce::uint32 v) { return juce::Colour (v); };
            a.bgTop = C (0xfff6f5f2);       a.bgBottom = C (0xffeceae5);
            a.card = C (0xffffffff);        a.cardBorder = C (0xffd8d5ce);
            a.inset = C (0xfffcfbf9);       a.text = C (0xff1f2126);
            a.textDim = C (0xff868a93);
            a.wave = C (0xff3b4250);        a.waveDim = C (0xffb9bdc6);
            a.accent = C (0xffff7a2f);      a.start = C (0xff25a063);
            a.playStart = C (0xff3b82f6);
            a.grid = C (0x1a000000);        a.bar = C (0x40000000);
            a.selection = C (0xffff7a2f);
            a.frame = C (0xffff7a2f);       // oranger Rahmen wie bei den anderen Plugins
            a.btn = C (0xffe8e5de);         a.btnHover = C (0xffdcd8cf);
            a.btnText = C (0xff1f2126);     a.btnTextDim = C (0xff9a9ea6);
            return a;
        }();
        return t;
    }
}

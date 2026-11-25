#pragma once
#include <juce_graphics/juce_graphics.h>

// Remember: dunkles Grau-Design im Stil der anderen Plugins (Kopfzeile, orange Rahmen), Standardschrift von JUCE
namespace RememberColours
{
    struct Theme
    {
        const char* name = "Light";
        const char* fontName = "";   // leer = JUCE-Standardschrift
        juce::Colour bgTop, bgBottom, card, cardBorder, inset, text, textDim;
        juce::Colour wave, waveDim, accent, start, playStart, grid, bar, selection, frame;
        juce::Colour btn, btnHover, btnText, btnTextDim, glow;
        float radius = 6.0f;
        float titleSpacing = 12.0f;
    };

    inline const Theme& theme()
    {
        static const Theme t = []
        {
            Theme a;
            auto C = [] (juce::uint32 v) { return juce::Colour (v); };
            a.bgTop = C (0xff3d3e42);       a.bgBottom = C (0xff303134);
            a.card = C (0xff2b2c2f);        a.cardBorder = C (0xff55575c);
            a.inset = C (0xff232427);       a.text = C (0xffe8e8ea);
            a.textDim = C (0xff9a9da3);
            a.wave = C (0xffcfd2d8);        a.waveDim = C (0xff666970);
            a.accent = C (0xffff7a2f);      a.start = C (0xff3ccf8a);
            a.playStart = C (0xff5aa0f5);
            a.grid = C (0x22ffffff);        a.bar = C (0x55ffffff);
            a.selection = C (0xffff7a2f);
            a.frame = C (0xffff7a2f);       // oranger Rahmen wie bei den anderen Plugins
            a.btn = C (0xff4b4d52);         a.btnHover = C (0xff595b61);
            a.btnText = C (0xffeceef0);     a.btnTextDim = C (0xff777a80);
            a.glow = C (0xff4f9dff);
            return a;
        }();
        return t;
    }
}

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
            // Grundfarben wie bei The One und den anderen Plugins
            a.bgTop = C (0xff17181c);       a.bgBottom = C (0xff0d0e11);
            a.card = C (0xff1d1f24);        a.cardBorder = C (0xff2b2e35);
            a.inset = C (0xff121317);       a.text = C (0xffeceef2);
            a.textDim = C (0xff7d828c);
            a.wave = C (0xffcfd2d8);        a.waveDim = C (0xff565a63);
            a.accent = C (0xffff7a2f);      a.start = C (0xff3ddc84);
            a.playStart = C (0xff4d8dff);
            a.grid = C (0x1fffffff);        a.bar = C (0x4dffffff);
            a.selection = C (0xffff7a2f);
            a.frame = C (0xffff7a2f);       // oranger Rahmen wie bei den anderen Plugins
            a.btn = C (0xff26282e);         a.btnHover = C (0xff2c2f36);
            a.btnText = C (0xffeceef2);     a.btnTextDim = C (0xff5f636c);
            a.glow = C (0xff4d8dff);
            return a;
        }();
        return t;
    }
}

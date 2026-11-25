#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "Clip.h"
#include "Engine.h"

// Dateinamen und WAV-Export. Der Export streamt in Stuecken direkt aus dem Ringpuffer (kein grosser Zwischenspeicher)
namespace RememberExport
{
    struct ClipMeta
    {
        juce::String project, type, form, key, scale;   // scale: "", "Major", "Minor"
        double bpm = 0.0;                               // 0 = unbekannt
    };

    juce::String sanitise (const juce::String&);
    // Projekt_Typ_Form_128bpm_Amin  (leere Teile entfallen)
    juce::String buildStem (const ClipMeta&);
    // naechste freie Take-Nummer fuer diesen Stem im Ordner (Stem_T01.wav, Stem_T02.wav ...)
    int nextTake (const juce::File& folder, const juce::String& stem);
    juce::String fileName (const juce::String& stem, int take);
    juce::File defaultFolder();

    // Bittiefe fuer den Export: 16, 24 oder 32 (Float). Standard 24
    int& bitDepth();
    // Stellen der Take-Nummer (2 = T01, 3 = T001, 4 = T0001)
    int& takeDigits();

    // Schreibt [start, start+len) als WAV (bitDepth()). Leerer String = ok, sonst Fehlertext
    juce::String writeWav (const RememberDSP::Storage&, int64_t start, int64_t len, double sampleRate, const juce::File& file);
    // Schreibt den bearbeiteten Editor-Clip (mit allen Aenderungen) als WAV (bitDepth())
    juce::String writeClipWav (const RememberClip::Clip&, const juce::File& file);
}

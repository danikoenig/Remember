#pragma once
// Remember: Editor-Kern. Ein Clip (Audio im Speicher) mit Bearbeitungsfunktionen, Lautheit (ITU BS.1770 / EBU R128)
// und Undo/Redo. Reiner C++17-Code ohne JUCE, einzeln testbar (tests/ClipTest.cpp).
#include <cstdint>
#include <memory>
#include <vector>

namespace RememberClip
{
    using Channels = std::vector<std::vector<float>>;

    struct Range
    {
        int64_t a = 0, b = 0;           // [a, b)
        int64_t length() const { return b - a; }
        bool empty() const { return b <= a; }
    };

    struct Clip
    {
        double sampleRate = 48000.0;
        Channels ch;                    // ch[kanal][sample]
        double bpm = 0.0;               // 0 = unbekannt (kein Raster)
        float beatsPerBar = 4.0f;
        double gridOffset = 0.0;        // Sample-Position eines Taktanfangs im Clip (kann ausserhalb liegen)

        int numChannels() const { return (int) ch.size(); }
        int64_t length() const { return ch.empty() ? 0 : (int64_t) ch[0].size(); }
        bool empty() const { return length() == 0; }
        double samplesPerBeat() const { return bpm > 1.0 ? sampleRate * 60.0 / bpm : 0.0; }
    };

    Range clampRange (const Clip&, Range);

    // Bearbeitung (r wird auf den Clip begrenzt). Raster (gridOffset) wird mitgefuehrt
    Channels extract (const Clip&, Range r);                          // Kopie des Bereichs
    void deleteRange (Clip&, Range r);                                // Bereich entfernen, Luecke schliessen
    void crop (Clip&, Range r);                                       // nur den Bereich behalten
    void insert (Clip&, int64_t pos, const Channels& data);           // einfuegen
    void reverse (Clip&, Range r);
    void applyGain (Clip&, Range r, float linear);
    enum class FadeShape { Smooth, Linear, EqualPower, Exponential };
    void fadeIn (Clip&, Range r, FadeShape = FadeShape::Smooth);                                     // 0 -> 1 ueber den Bereich
    void fadeOut (Clip&, Range r, FadeShape = FadeShape::Smooth);                                    // 1 -> 0 ueber den Bereich
    Range findNonSilent (const Clip&, float thresholdDb);             // leer, wenn alles still ist
    void trimSilence (Clip&, float thresholdDb);                      // Stille vorne und hinten entfernen

    // Wrap: r ist der markierte Nachklang (Tail). Er wird aus dem Clip genommen und ueber die gesamte Tail-Laenge
    // (gleiche Leistung) mit dem Anfang des Clips ueberblendet: der Tail blendet aus, der Anfang ein.
    // Der Clip endet exakt am Beginn der Auswahl. Ist der Tail laenger als der Rest, wird nur der Anfang des Tails verwendet
    void wrap (Clip&, Range tail);
    // Rotate: dreht den Clip kreisfoermig, sodass pos der neue Anfang wird. Laenge bleibt gleich.
    // An der alten Schnittstelle blendet ein kurzer Ueberblend (xfade Samples je Seite, gleiche Leistung) Klicks weg
    void rotate (Clip&, int64_t pos, int64_t xfade);

    float peakAbs (const Clip&, Range r);
    // Integrierte Lautheit in LUFS (BS.1770-4, Gating -70 LUFS absolut und -10 LU relativ). -infinity bei Stille
    double integratedLufs (const Clip&, Range r);

    struct NormalizeResult { bool ok = false; bool limited = false; double gainDb = 0.0; };
    // Normalisiert auf einen Spitzenwert (dBFS)
    NormalizeResult normalizePeak (Clip&, Range r, double targetDb);
    // Normalisiert auf Lautheit (LUFS). Die Verstaerkung wird so begrenzt, dass der Spitzenwert ceilingDb nicht ueberschreitet
    NormalizeResult normalizeLufs (Clip&, Range r, double targetLufs, double ceilingDb);
    // Normalisiert auf den RMS-Pegel (dBFS, alle Kanaele zusammen), Spitzen-Obergrenze wie bei LUFS
    NormalizeResult normalizeRms (Clip&, Range r, double targetDb, double ceilingDb);
    double rmsDb (const Clip&, Range r);

    // Clip mit Undo/Redo und interner Zwischenablage. Nur Oberflaeche (message thread)
    class Session
    {
    public:
        Clip clip;
        Channels clipboard;
        uint64_t version = 0;           // steigt bei jeder Aenderung

        bool hasClip() const { return ! clip.empty(); }
        void load (Clip&& c);           // ersetzt den Clip (rueckgaengig machbar)
        void beginEdit();               // vor jeder Bearbeitung aufrufen: Zustand fuer Undo sichern
        void touch() { ++version; }
        bool undo();
        bool redo();
        bool canUndo() const { return ! undoStack.empty(); }
        bool canRedo() const { return ! redoStack.empty(); }
        void clear();

    private:
        std::vector<Clip> undoStack, redoStack;
        static constexpr size_t maxUndo = 24;
    };
}

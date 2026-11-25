#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colours.h"
#include "Clip.h"
#include "Engine.h"

// Anzeige der Aufnahme: Wellenform, Takt-/Beat-Raster, Play-Start, Startpunkt und Auswahl mit Snapping.
// Die Ansicht laeuft live mit. Sie friert ein, solange die Maus darueber steht, HOLD aktiv ist oder
// ein Startpunkt / eine Auswahl gesetzt ist (LIVE hebt das auf).
class HistoryView : public juce::Component, private juce::Timer
{
public:
    explicit HistoryView (RememberDSP::Engine&);
    ~HistoryView() override { stopTimer(); }

    // 0 aus, 1 = 1/16, 2 = 1/8, 3 = Beat, 4 = Takt
    void setSnapMode (int mode) { snapMode = mode; }
    void setHold (bool h) { hold = h; repaint(); }
    bool isHeld() const { return hold; }
    bool isLive() const { return ! frozen(); }
    void goLive();                         // Auswahl und Startpunkt loeschen, Hold aus
    void clearSelection();
    void selectBars (int bars);            // ab Startpunkt, sonst die letzten vollstaendigen Takte
    void zoomFit();
    bool hasSelection() const { return selA >= 0 && selB >= 0 && selA != selB; }
    // normalisiert (start < end), true wenn eine gueltige Auswahl da ist
    bool getSelection (int64_t& start, int64_t& end) const;
    // Tempo und Taktlaenge am Beginn der Auswahl (false = Host liefert nichts)
    bool getSelectionTempo (double& bpm, float& beatsPerBar) const;
    // Kopiert die Auswahl samt Tempo und Raster in einen Clip (fuer den Editor)
    bool grabClip (RememberClip::Clip& out) const;
    // Laenge der Auswahl in Takten (false = unbekannt)
    bool getSelectionBars (double& bars) const;

    std::function<void()> onChanged;       // Auswahl, Startpunkt oder Ansicht geaendert

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    bool frozen() const { return hold || hovering || dragging != None || startMarker >= 0 || hasSelection(); }
    void updateView (const RememberDSP::Storage&);
    void clampView (const RememberDSP::Storage&);
    int64_t sampleAt (float x) const { return viewStart + (int64_t) ((double) x / juce::jmax (1, getWidth()) * (double) viewSpan); }
    float xFor (int64_t s) const { return (float) ((double) (s - viewStart) / (double) viewSpan * getWidth()); }
    int64_t snap (int64_t s, bool free) const;
    double snapBeats (float beatsPerBar) const;
    void changed() { repaint(); if (onChanged) onChanged(); }
    juce::Rectangle<float> waveArea() const;
    void zoomAround (float x, double factor);

    RememberDSP::Engine& engine;
    int snapMode = 3;
    bool hold = false, hovering = false;

    int64_t viewStart = 0, viewSpan = 0;
    int64_t selA = -1, selB = -1, startMarker = -1;
    enum Drag { None, NewSel, EdgeA, EdgeB } dragging = None;
    float downX = 0.0f;
    bool moved = false;

    std::vector<RememberDSP::Meta> metas;
    std::vector<RememberDSP::GridPoint> grid;
    std::vector<int64_t> playStarts;
    std::vector<float> mn, mx;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HistoryView)
};

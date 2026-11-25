#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Colours.h"
#include "PluginProcessor.h"

// Editor-Ansicht des Clips: Wellenform, Raster (Tempo vom Host zum Zeitpunkt der Aufnahme), Auswahl mit Snapping,
// Startmarke fuer Vorhoeren und Einfuegen, Abspielposition. Die Bearbeitung selbst macht der Editor.
class ClipView : public juce::Component, private juce::Timer
{
public:
    explicit ClipView (RememberProcessor&);
    ~ClipView() override { stopTimer(); }

    void setSnapMode (int mode) { snapMode = mode; }
    bool getSelection (RememberClip::Range& r) const;   // normalisiert, auf den Clip begrenzt
    bool hasSelection() const { RememberClip::Range r; return getSelection (r); }
    void setSelection (int64_t a, int64_t b);
    void clearSelection();
    int64_t getMarker() const { return marker; }
    void setMarker (int64_t m);
    void refresh (bool refit = false);                  // nach einer Aenderung des Clips
    void zoomFit();

    std::function<void()> onContext;                    // Rechtsklick
    std::function<void()> onChanged;                    // Auswahl oder Marke geaendert
    std::function<bool (const juce::KeyPress&)> onKey;  // Tastaturkuerzel an den Editor

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void rebuildPeaks();
    void getBins (int64_t start, int64_t span, int bins, float* mn, float* mx) const;
    int64_t sampleAt (float x) const { return (int64_t) std::llround (viewStart + (double) x / juce::jmax (1, getWidth()) * viewSpan); }
    float xFor (double s) const { return (float) ((s - viewStart) / viewSpan * getWidth()); }
    int64_t snap (int64_t s, bool free) const;
    double snapStep() const;                            // Samples, 0 = aus
    void clampView();
    void changed() { repaint(); if (onChanged) onChanged(); }

    RememberProcessor& processor;
    int snapMode = 3;
    double viewStart = 0.0, viewSpan = 1.0;
    int64_t selA = -1, selB = -1, marker = -1;
    enum Drag { None, NewSel, EdgeA, EdgeB } dragging = None;
    float downX = 0.0f;
    bool moved = false;

    uint64_t peaksVersion = ~0ull;
    int64_t peaksLen = -1;
    std::vector<float> pmin, pmax;                      // je 256 Samples, ueber alle Kanaele
    std::vector<float> mn, mx;
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipView)
};

#include "ClipView.h"
#include "Look.h"

using namespace juce;
using namespace RememberClip;

namespace
{
    constexpr float rulerH = 22.0f;
    constexpr int pk = 256;
}

ClipView::ClipView (RememberProcessor& p) : processor (p)
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    startTimerHz (30);
}

void ClipView::timerCallback()
{
    const bool playing = processor.getAudition().isPlaying();
    if (playing || wasPlaying) repaint();
    wasPlaying = playing;
}

bool ClipView::getSelection (RememberClip::Range& r) const
{
    if (selA < 0 || selB < 0 || selA == selB) return false;
    const auto& c = processor.getSession().clip;
    r = clampRange (c, { std::min (selA, selB), std::max (selA, selB) });
    return ! r.empty();
}

void ClipView::setSelection (int64_t a, int64_t b)
{
    selA = a; selB = b;
    changed();
}

void ClipView::clearSelection()
{
    selA = selB = -1;
    changed();
}

void ClipView::setMarker (int64_t m)
{
    marker = m;
    changed();
}

void ClipView::zoomFit()
{
    const auto& c = processor.getSession().clip;
    viewStart = 0.0;
    viewSpan = std::max<double> (64.0, (double) c.length());
    repaint();
}

void ClipView::refresh (bool refit)
{
    const auto& c = processor.getSession().clip;
    const int64_t n = c.length();
    if (selA > n || selB > n) { selA = selB = -1; }
    if (marker > n) marker = -1;
    if (refit || viewSpan <= 1.0) zoomFit();
    clampView();
    repaint();
}

void ClipView::clampView()
{
    const double n = (double) processor.getSession().clip.length();
    viewSpan = std::min (std::max (viewSpan, 32.0), std::max (32.0, n));
    viewStart = std::max (0.0, std::min (viewStart, n - viewSpan));
}

void ClipView::rebuildPeaks()
{
    const auto& s = processor.getSession();
    if (peaksVersion == s.version && peaksLen == s.clip.length()) return;
    peaksVersion = s.version;
    peaksLen = s.clip.length();
    const int64_t n = s.clip.length(), blocks = (n + pk - 1) / pk;
    pmin.assign ((size_t) blocks, 0.0f);
    pmax.assign ((size_t) blocks, 0.0f);
    for (int64_t b = 0; b < blocks; ++b)
    {
        float lo = 1.0e9f, hi = -1.0e9f;
        const int64_t e = std::min (n, (b + 1) * pk);
        for (const auto& v : s.clip.ch)
            for (int64_t i = b * pk; i < e; ++i) { lo = std::min (lo, v[(size_t) i]); hi = std::max (hi, v[(size_t) i]); }
        pmin[(size_t) b] = lo;
        pmax[(size_t) b] = hi;
    }
}

void ClipView::getBins (int64_t start, int64_t span, int bins, float* lo, float* hi) const
{
    const auto& c = processor.getSession().clip;
    const int64_t n = c.length();
    for (int i = 0; i < bins; ++i)
    {
        const int64_t s = start + (int64_t) ((double) span * i / bins);
        const int64_t e = std::max<int64_t> (s + 1, start + (int64_t) ((double) span * (i + 1) / bins));
        if (s < 0 || s >= n) { lo[i] = 1.0f; hi[i] = -1.0f; continue; }
        const int64_t ee = std::min (e, n);
        float a = 1.0e9f, b = -1.0e9f;
        if (ee - s >= pk * 2)
        {
            const int64_t b0 = (s + pk - 1) / pk, b1 = ee / pk;
            for (int64_t k = b0; k < b1; ++k) { a = std::min (a, pmin[(size_t) k]); b = std::max (b, pmax[(size_t) k]); }
        }
        if (b < a)
            for (const auto& v : c.ch)
                for (int64_t t = s; t < ee; ++t) { a = std::min (a, v[(size_t) t]); b = std::max (b, v[(size_t) t]); }
        lo[i] = a;
        hi[i] = b;
    }
}

double ClipView::snapStep() const
{
    const auto& c = processor.getSession().clip;
    const double spb = c.samplesPerBeat();
    if (spb <= 0.0 || snapMode == 0) return 0.0;
    switch (snapMode)
    {
        case 1: return spb * 0.25;
        case 2: return spb * 0.5;
        case 3: return spb;
        default: return spb * (double) c.beatsPerBar;
    }
}

int64_t ClipView::snap (int64_t s, bool free) const
{
    const auto& c = processor.getSession().clip;
    const double step = snapStep();
    if (free || step <= 0.0) return std::max<int64_t> (0, std::min (s, c.length()));
    double v = c.gridOffset + std::round (((double) s - c.gridOffset) / step) * step;
    // Anfang und Ende des Clips rasten auch ein
    const double reach = std::max (step * 0.5, 1.0);
    if (std::abs (v - (double) s) > reach) v = (double) s;
    if (std::abs ((double) s) < std::min (reach, step * 0.25)) v = 0.0;
    if (std::abs ((double) (c.length() - s)) < std::min (reach, step * 0.25)) v = (double) c.length();
    return std::max<int64_t> (0, std::min ((int64_t) std::llround (v), c.length()));
}

// ---------------------------------------------------------------------------------------------

void ClipView::mouseMove (const MouseEvent& e)
{
    RememberClip::Range r;
    const bool nearEdge = getSelection (r) && (std::abs (e.position.x - xFor ((double) r.a)) < 6.0f || std::abs (e.position.x - xFor ((double) r.b)) < 6.0f);
    setMouseCursor (nearEdge ? MouseCursor::LeftRightResizeCursor : MouseCursor::NormalCursor);
}

void ClipView::mouseDown (const MouseEvent& e)
{
    if (! processor.getSession().hasClip()) return;
    downX = e.position.x;
    moved = false;
    const bool free = e.mods.isCtrlDown() || e.mods.isCommandDown();
    RememberClip::Range r;
    if (getSelection (r))
    {
        if (std::abs (e.position.x - xFor ((double) r.a)) < 6.0f) { selA = r.a; selB = r.b; dragging = EdgeA; return; }
        if (std::abs (e.position.x - xFor ((double) r.b)) < 6.0f) { selA = r.a; selB = r.b; dragging = EdgeB; return; }
    }
    selA = selB = snap (sampleAt (e.position.x), free);
    dragging = NewSel;
}

void ClipView::mouseDrag (const MouseEvent& e)
{
    if (dragging == None) return;
    if (std::abs (e.position.x - downX) > 3.0f) moved = true;
    const int64_t s = snap (sampleAt (e.position.x), e.mods.isCtrlDown() || e.mods.isCommandDown());
    if (dragging == EdgeA) selA = s; else selB = s;
    repaint();
}

void ClipView::mouseUp (const MouseEvent&)
{
    if (dragging == NewSel && (! moved || selA == selB))
    {
        marker = selA;      // Klick: Startmarke (Vorhoeren und Einfuegen), keine Auswahl
        selA = selB = -1;
    }
    dragging = None;
    changed();
}

void ClipView::mouseDoubleClick (const MouseEvent& e)
{
    // Doppelklick: Takt unter der Maus waehlen (bei bekanntem Tempo), sonst alles
    const auto& c = processor.getSession().clip;
    const double bar = c.samplesPerBeat() * (double) c.beatsPerBar;
    const int64_t s = sampleAt (e.position.x);
    if (bar <= 0.0) { setSelection (0, c.length()); return; }
    const double k = std::floor (((double) s - c.gridOffset) / bar);
    const int64_t a = std::max<int64_t> (0, (int64_t) std::llround (c.gridOffset + k * bar));
    const int64_t b = std::min<int64_t> (c.length(), (int64_t) std::llround (c.gridOffset + (k + 1.0) * bar));
    if (b > a) { dragging = None; setSelection (a, b); }
}

void ClipView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& d)
{
    if (! processor.getSession().hasClip()) return;
    if (e.mods.isShiftDown() || std::abs (d.deltaX) > std::abs (d.deltaY))
    {
        const float dx = std::abs (d.deltaX) > std::abs (d.deltaY) ? d.deltaX : d.deltaY;
        viewStart -= (double) dx * viewSpan * 0.5;
    }
    else
    {
        const double anchor = viewStart + (double) e.position.x / jmax (1, getWidth()) * viewSpan;
        const double frac = (double) e.position.x / jmax (1, getWidth());
        viewSpan *= std::exp (-(double) d.deltaY * 0.9);
        clampView();
        viewStart = anchor - frac * viewSpan;
    }
    clampView();
    repaint();
}

bool ClipView::keyPressed (const KeyPress& k)
{
    if (onKey && onKey (k)) return true;
    const auto c = k.getTextCharacter();
    if (c == '+' || c == '=') { const double mid = viewStart + viewSpan * 0.5; viewSpan *= 0.6; clampView(); viewStart = mid - viewSpan * 0.5; clampView(); repaint(); return true; }
    if (c == '-') { const double mid = viewStart + viewSpan * 0.5; viewSpan /= 0.6; clampView(); viewStart = mid - viewSpan * 0.5; clampView(); repaint(); return true; }
    if (c == 'f' || c == 'F' || c == '0') { zoomFit(); return true; }
    if (k == KeyPress::escapeKey) { selA = selB = -1; changed(); return true; }
    return false;
}

// ---------------------------------------------------------------------------------------------

void ClipView::paint (Graphics& g)
{
    const auto& T = RememberColours::theme();
    const auto full = getLocalBounds().toFloat();
    g.setColour (T.inset);
    g.fillRoundedRectangle (full, T.radius);
    g.setColour (T.cardBorder);
    g.drawRoundedRectangle (full.reduced (0.5f), T.radius, 1.0f);

    const auto& session = processor.getSession();
    const auto& c = session.clip;
    if (c.empty())
    {
        g.setColour (T.textDim);
        g.setFont (Font (rememberFont (14.0f)));
        g.drawText ("Select audio in the history above and press TO EDITOR", full, Justification::centred);
        return;
    }
    rebuildPeaks();
    clampView();

    const int W = getWidth();
    const double sr = c.sampleRate;
    const int64_t vs = (int64_t) std::floor (viewStart), vspan = (int64_t) std::ceil (viewSpan);
    const auto wa = full.withTrimmedTop (rulerH);
    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (1));

    // Raster
    const double spb = c.samplesPerBeat();
    if (spb > 0.0)
    {
        const double bpb = (double) c.beatsPerBar, pxPerBeat = spb / viewSpan * W;
        const int64_t i0 = (int64_t) std::floor ((viewStart - c.gridOffset) / spb) - 1, i1 = (int64_t) std::ceil ((viewStart + viewSpan - c.gridOffset) / spb) + 1;
        // erster Taktanfang am oder nach dem Clip-Anfang bekommt die Nummer 1
        const double firstBar = std::ceil ((0.0 - c.gridOffset) / (spb * bpb) - 1.0e-6);
        int step = 1;
        while (pxPerBeat * bpb * step < 40.0) step *= 2;
        g.setFont (Font (rememberFont (11.0f)));
        for (int64_t i = i0; i <= i1; ++i)
        {
            const double pos = c.gridOffset + (double) i * spb;
            if (pos < -1.0 || pos > (double) c.length() + 1.0) continue;
            const double barsF = (double) i / bpb;
            const bool isBar = std::abs (barsF - std::round (barsF)) < 1.0e-6;
            const float x = xFor (pos);
            if (isBar)
            {
                if (pxPerBeat * bpb < 5.0) continue;
                g.setColour (T.bar.withAlpha (0.5f));
                g.fillRect (Rectangle<float> (std::floor (x), rulerH - 6.0f, 1.0f, full.getHeight() - rulerH + 6.0f));
                const int barNo = (int) std::llround (barsF - firstBar) + 1;
                if (barNo >= 1 && (step == 1 || barNo % step == 0) && x < (float) W - 90.0f)
                {
                    g.setColour (T.textDim);
                    g.drawText (String (barNo), Rectangle<float> (x + 3.0f, 3.0f, 40.0f, 14.0f), Justification::left, false);
                }
            }
            else if (pxPerBeat >= 6.0)
            {
                g.setColour (T.grid.withAlpha (0.2f));
                g.fillRect (Rectangle<float> (std::floor (x), wa.getY(), 1.0f, wa.getHeight()));
            }
        }
    }

    // Wellenform
    const int bins = std::max (1, W);
    mn.assign ((size_t) bins, 1.0f);
    mx.assign ((size_t) bins, -1.0f);
    getBins (vs, vspan, bins, mn.data(), mx.data());
    const float cy = wa.getCentreY(), hh = wa.getHeight() * 0.5f * 0.94f;
    RememberClip::Range sel;
    const bool hasSel = getSelection (sel);
    g.setColour (T.grid);
    g.fillRect (Rectangle<float> (0.0f, std::floor (cy), (float) W, 1.0f));
    for (int i = 0; i < bins; ++i)
    {
        if (mn[(size_t) i] > mx[(size_t) i]) continue;
        const float lo = jlimit (-1.0f, 1.0f, mn[(size_t) i]), hi = jlimit (-1.0f, 1.0f, mx[(size_t) i]);
        const int64_t smp = vs + (int64_t) ((double) i / bins * (double) vspan);
        const bool inside = hasSel && smp >= sel.a && smp <= sel.b;
        g.setColour (hasSel && ! inside ? T.waveDim : T.wave);
        const float y0 = cy - hi * hh, y1 = cy - lo * hh;
        g.fillRect (Rectangle<float> ((float) i, y0, 1.0f, std::max (1.0f, y1 - y0)));
    }

    // Auswahl
    if (hasSel)
    {
        const float x0 = xFor ((double) sel.a), x1 = xFor ((double) sel.b);
        g.setColour (T.selection.withAlpha (0.10f));
        g.fillRect (Rectangle<float> (x0, rulerH, x1 - x0, full.getHeight() - rulerH));
        g.setColour (T.selection);
        g.fillRect (Rectangle<float> (x0 - 1.0f, rulerH - 6.0f, 2.0f, full.getHeight() - rulerH + 6.0f));
        g.fillRect (Rectangle<float> (x1 - 1.0f, rulerH - 6.0f, 2.0f, full.getHeight() - rulerH + 6.0f));
        String label = String ((double) sel.length() / sr, 2) + " s";
        if (spb > 0.0)
        {
            const double bars = (double) sel.length() / (spb * (double) c.beatsPerBar);
            label = String (bars, std::abs (bars - std::round (bars)) < 0.005 ? 0 : 2) + (std::abs (bars - 1.0) < 1.0e-6 ? " bar" : " bars")
                    + String::fromUTF8 ("  \xc2\xb7  ") + label;
        }
        g.setFont (Font (rememberFont (12.0f, true)));
        const float tw = g.getCurrentFont().getStringWidthFloat (label) + 16.0f;
        const float cx = jlimit (tw * 0.5f + 2.0f, (float) W - tw * 0.5f - 2.0f, (x0 + x1) * 0.5f);
        const auto box = Rectangle<float> (cx - tw * 0.5f, wa.getY() + 6.0f, tw, 20.0f);
        g.setColour (T.card.withAlpha (0.94f));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (T.selection);
        g.drawRoundedRectangle (box, 5.0f, 1.0f);
        g.setColour (T.text);
        g.drawText (label, box, Justification::centred);
    }

    // Startmarke (gruen) und Abspielposition
    if (marker >= 0)
    {
        const float x = xFor ((double) marker);
        g.setColour (T.start);
        g.fillRect (Rectangle<float> (std::floor (x) - 0.5f, rulerH - 6.0f, 2.0f, full.getHeight() - rulerH + 6.0f));
        Path t;
        t.addTriangle (x - 5.0f, 2.0f, x + 5.0f, 2.0f, x, 11.0f);
        g.fillPath (t);
    }
    if (processor.getAudition().isPlaying())
    {
        const float x = xFor ((double) processor.getAudition().position());
        g.setColour (T.text);
        g.fillRect (Rectangle<float> (std::floor (x), rulerH - 6.0f, 1.5f, full.getHeight() - rulerH + 6.0f));
    }

    // Laenge unten rechts
    g.setFont (Font (rememberFont (11.0f)));
    g.setColour (T.textDim);
    g.drawText (String ((double) c.length() / sr, 2) + " s", Rectangle<float> ((float) W - 80.0f, full.getBottom() - 18.0f, 72.0f, 14.0f), Justification::centredRight, false);
    g.restoreState();
}

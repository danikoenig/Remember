#include "HistoryView.h"
#include "Align.h"
#include "Look.h"
#include <cstdlib>

using namespace juce;
using namespace RememberDSP;

namespace
{
    constexpr float rulerH = 22.0f;
    constexpr float axisH = 16.0f;
}

HistoryView::HistoryView (Engine& e) : engine (e)
{
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    startTimerHz (30);
}

Rectangle<float> HistoryView::waveArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (rulerH).withTrimmedBottom (axisH);
}

void HistoryView::timerCallback()
{
    auto st = engine.storage();
    if (st == nullptr) return;
    // Auswahl, die aus der History herausgelaufen ist, verwerfen
    const int64_t oldest = st->oldestValid();
    bool dropped = false;
    if (hasSelection() && std::min (selA, selB) < oldest) { selA = selB = -1; dropped = true; }
    if (startMarker >= 0 && startMarker < oldest) { startMarker = -1; dropped = true; }
    if (dropped && onChanged) onChanged();
    if (! frozen() || dropped) repaint();
}

void HistoryView::updateView (const Storage& st)
{
    const double sr = engine.getSampleRate();
    const int64_t w = st.writtenNow();
    if (viewSpan <= 0) viewSpan = (int64_t) (30.0 * sr);
    viewSpan = jlimit<int64_t> ((int64_t) (0.05 * sr), st.cap - 8192, viewSpan);
    if (! frozen()) viewStart = w - viewSpan;
    else clampView (st);
}

void HistoryView::clampView (const Storage& st)
{
    const int64_t w = st.writtenNow(), oldest = st.oldestValid();
    if (viewStart + viewSpan > w + viewSpan / 20) viewStart = w + viewSpan / 20 - viewSpan;
    if (viewStart < oldest && w > viewSpan) viewStart = oldest;
}

void HistoryView::goLive()
{
    selA = selB = startMarker = -1;
    hold = false;
    changed();
}

void HistoryView::clearSelection()
{
    selA = selB = startMarker = -1;
    changed();
}

void HistoryView::zoomFit()
{
    auto st = engine.storage();
    if (st == nullptr) return;
    const int64_t w = st->writtenNow();
    viewSpan = std::max<int64_t> ((int64_t) engine.getSampleRate(), w - st->oldestValid());
    viewStart = w - viewSpan;
    repaint();
}

void HistoryView::zoomAround (float x, double factor)
{
    auto st = engine.storage();
    if (st == nullptr) return;
    const double sr = engine.getSampleRate();
    const int64_t anchor = sampleAt (x);
    const double frac = (double) x / jmax (1, getWidth());
    const int64_t newSpan = jlimit<int64_t> ((int64_t) (0.05 * sr), st->cap - 8192, (int64_t) ((double) viewSpan * factor));
    viewSpan = newSpan;
    viewStart = anchor - (int64_t) (frac * (double) newSpan);
    clampView (*st);
    repaint();
}

bool HistoryView::getSelection (int64_t& s, int64_t& e) const
{
    if (! hasSelection()) return false;
    s = std::min (selA, selB);
    e = std::max (selA, selB);
    return true;
}

bool HistoryView::getSelectionTempo (double& bpm, float& bpb) const
{
    int64_t s, e;
    auto st = engine.storage();
    if (st == nullptr || ! getSelection (s, e)) return false;
    std::vector<Meta> m;
    st->readMeta (s, e, m);
    return tempoAt (m, s, bpm, bpb);
}

int64_t HistoryView::exportShift (int64_t s, int64_t e) const
{
    lastShift = 0;
    if (! autoAlignOn) return 0;
    auto st = engine.storage();
    if (st == nullptr) return 0;
    const double sr = engine.getSampleRate();
    std::vector<Meta> m;
    st->readMeta (s, e, m);
    double bpm = 0.0;
    float bpb = 4.0f;
    if (! tempoAt (m, s, bpm, bpb) || bpm < 20.0) return 0;
    std::vector<GridPoint> g;
    gridFromMeta (m, st->writtenNow(), sr, s, e, 1.0, g);
    const double beatSec = 60.0 / bpm;
    const int64_t pad = (int64_t) (0.35 * beatSec * sr);
    const int64_t a = std::max (s - pad, st->oldestValid()), z = std::min (e + pad, st->writtenNow());
    if (g.size() < 2 || z - a < (int64_t) (0.5 * sr)) return 0;
    std::vector<float> l ((size_t) (z - a)), r ((size_t) (z - a));
    float* dst[2] { l.data(), r.data() };
    if (! st->copyRange (a, z - a, dst, 2)) return 0;
    for (size_t i = 0; i < l.size(); ++i) l[i] = 0.5f * (l[i] + r[i]);
    std::vector<int64_t> beats;
    for (const auto& p : g) beats.push_back (p.sample - a);
    const auto res = RememberAlign::estimateGridOffset (l, sr, beats, beatSec);
    if (! res.ok) return 0;
    const int64_t d = res.offsetSamples;
    if (std::llabs (d) > (int64_t) (0.3 * beatSec * sr)) return 0;
    if (s + d < st->oldestValid() || e + d > st->writtenNow()) return 0;       // nicht genug Aufnahme dahinter
    lastShift = d;
    return d;
}

bool HistoryView::grabClip (RememberClip::Clip& out) const
{
    int64_t s, e;
    auto st = engine.storage();
    if (st == nullptr || ! getSelection (s, e)) return false;

    RememberClip::Clip c;
    const double sr = engine.getSampleRate();
    c.sampleRate = sr;
    c.ch.assign ((size_t) st->numCh, std::vector<float> ((size_t) (e - s)));
    std::vector<float*> ptr;
    for (auto& v : c.ch) ptr.push_back (v.data());
    const int64_t sh = exportShift (s, e);      // Fenster beginnt auf dem ersten Schlag, Laenge bleibt
    if (! st->copyRange (s + sh, e - s, ptr.data(), st->numCh)) return false;

    // Tempo und Raster: ein Taktanfang als Position im Clip
    std::vector<Meta> m;
    st->readMeta (s, e, m);
    double bpm; float bpb;
    if (tempoAt (m, s, bpm, bpb))
    {
        c.bpm = bpm;
        c.beatsPerBar = bpb;
        const Meta* ref = nullptr;
        for (const auto& x : m) { if (x.sample > s) break; if (x.flags & fPlaying) ref = &x; }
        if (ref == nullptr) for (const auto& x : m) if (x.flags & fPlaying) { ref = &x; break; }
        std::vector<GridPoint> g;
        gridFromMeta (m, st->writtenNow(), sr, s, e, 1.0, g);
        if (! g.empty() && ref != nullptr)
        {
            const double idx = std::fmod (g[0].ppq - ref->barStartPpq, (double) bpb);
            const double beatInBar = idx < 0.0 ? idx + (double) bpb : idx;
            c.gridOffset = (double) (g[0].sample - s) - beatInBar * sr * 60.0 / bpm;
        }
    }
    out = std::move (c);
    return true;
}

bool HistoryView::getSelectionBars (double& bars) const
{
    int64_t s, e;
    auto st = engine.storage();
    if (st == nullptr || ! getSelection (s, e)) return false;
    std::vector<Meta> m;
    st->readMeta (s, e, m);
    double p0, p1, bpm; float bpb;
    const int64_t w = st->writtenNow();
    if (! sampleToPpq (m, w, engine.getSampleRate(), s, p0)) return false;
    if (! sampleToPpq (m, w, engine.getSampleRate(), std::max (s, e - 1), p1)) return false;
    if (! tempoAt (m, s, bpm, bpb)) return false;
    bars = (p1 - p0 + 0.0) / (double) bpb;
    // letztes Sample liegt knapp vor dem Ende: auf Beat-Genauigkeit runden
    bars = std::round (bars * 1000.0) / 1000.0;
    return true;
}

double HistoryView::snapBeats (float bpb) const
{
    switch (snapMode)
    {
        case 1: return 0.25;
        case 2: return 0.5;
        case 3: return 1.0;
        case 4: return (double) bpb;
        default: return 0.0;
    }
}

int64_t HistoryView::snap (int64_t s, bool free) const
{
    if (free || snapMode == 0) return s;
    auto st = engine.storage();
    if (st == nullptr) return s;
    const double sr = engine.getSampleRate();
    const int64_t win = (int64_t) (6.0 * sr), w = st->writtenNow();
    std::vector<Meta> m;
    st->readMeta (s - win, s + win, m);
    double bpm; float bpb;
    if (! tempoAt (m, s, bpm, bpb)) return s;
    std::vector<GridPoint> g;
    gridFromMeta (m, w, sr, s - win, s + win, snapBeats (bpb), g);
    if (g.empty()) return s;
    int64_t best = g[0].sample;
    for (const auto& p : g)
        if (std::llabs (p.sample - s) < std::llabs (best - s)) best = p.sample;
    // nur einrasten, wenn der Punkt nah genug liegt (max. 1/3 der Ansicht, mindestens 12 Pixel)
    const int64_t reach = std::max<int64_t> ((int64_t) (viewSpan / 3), (int64_t) (12.0 * (double) viewSpan / jmax (1, getWidth())));
    return std::llabs (best - s) <= reach ? best : s;
}

void HistoryView::selectBars (int bars)
{
    auto st = engine.storage();
    if (st == nullptr) return;
    const double sr = engine.getSampleRate();
    const int64_t w = st->writtenNow();
    std::vector<Meta> m;
    int64_t s0 = -1, s1 = -1;

    if (startMarker >= 0 || hasSelection())
    {
        const int64_t start = startMarker >= 0 ? startMarker : std::min (selA, selB);
        st->readMeta (start, w, m);
        double bpm; float bpb; double ppq;
        if (! tempoAt (m, start, bpm, bpb) || ! sampleToPpq (m, w, sr, start, ppq)) return;
        int64_t endS;
        if (! ppqToSample (m, w, sr, ppq + (double) bars * (double) bpb, endS)) return;   // noch nicht aufgenommen
        s0 = start; s1 = endS;
    }
    else
    {
        // die letzten vollstaendigen Takte
        const int64_t from = std::max<int64_t> (st->oldestValid(), w - (int64_t) (240.0 * sr));
        st->readMeta (from, w, m);
        double bpm; float bpb;
        if (! tempoAt (m, w - 1, bpm, bpb)) return;
        std::vector<GridPoint> g;
        gridFromMeta (m, w, sr, from, w, (double) bpb, g);
        std::vector<GridPoint> bars_;
        for (const auto& p : g) if (p.bar) bars_.push_back (p);
        if ((int) bars_.size() < 1) return;
        const GridPoint last = bars_.back();   // letzter Taktanfang = Ende der letzten vollstaendigen Takte
        int64_t startS;
        if (! ppqToSample (m, w, sr, last.ppq - (double) bars * (double) bpb, startS)) return;
        s0 = startS; s1 = last.sample;
    }
    if (s1 <= s0) return;
    selA = s0; selB = s1; startMarker = s0;
    // Ansicht auf die Auswahl legen
    viewSpan = std::max<int64_t> ((int64_t) ((double) (s1 - s0) * 1.25), (int64_t) (1.0 * sr));
    viewStart = s0 - (viewSpan - (s1 - s0)) / 2;
    clampView (*st);
    changed();
}

// ---------------------------------------------------------------------------------------------
// Maus

void HistoryView::mouseEnter (const MouseEvent&) { hovering = true; repaint(); }
void HistoryView::mouseExit (const MouseEvent&) { hovering = false; repaint(); }
void HistoryView::mouseMove (const MouseEvent& e)
{
    const bool nearEdge = hasSelection() && (std::abs (e.position.x - xFor (selA)) < 6.0f || std::abs (e.position.x - xFor (selB)) < 6.0f);
    setMouseCursor (nearEdge ? MouseCursor::LeftRightResizeCursor : MouseCursor::NormalCursor);
}

void HistoryView::mouseDown (const MouseEvent& e)
{
    downX = e.position.x;
    moved = false;
    const bool free = e.mods.isCtrlDown() || e.mods.isCommandDown();
    if (hasSelection())
    {
        if (std::abs (e.position.x - xFor (selA)) < 6.0f) { dragging = EdgeA; return; }
        if (std::abs (e.position.x - xFor (selB)) < 6.0f) { dragging = EdgeB; return; }
    }
    const int64_t s = snap (sampleAt (e.position.x), free);
    selA = selB = s;
    dragging = NewSel;
}

void HistoryView::mouseDrag (const MouseEvent& e)
{
    if (dragging == None) return;
    if (std::abs (e.position.x - downX) > 3.0f) moved = true;
    const bool free = e.mods.isCtrlDown() || e.mods.isCommandDown();
    auto st = engine.storage();
    int64_t s = snap (sampleAt (e.position.x), free);
    if (st != nullptr) s = jlimit<int64_t> (st->oldestValid(), st->writtenNow(), s);
    if (dragging == EdgeA) selA = s; else selB = s;   // NewSel und EdgeB bewegen das zweite Ende
    if (dragging == NewSel && moved) startMarker = std::min (selA, selB);
    repaint();
}

void HistoryView::mouseUp (const MouseEvent&)
{
    if (dragging == NewSel && (! moved || selA == selB))
    {
        // einfacher Klick: Startpunkt setzen, keine Auswahl
        startMarker = selA;
        selA = selB = -1;
    }
    else if (hasSelection())
        startMarker = std::min (selA, selB);
    dragging = None;
    changed();
}

void HistoryView::mouseDoubleClick (const MouseEvent& e)
{
    // Doppelklick: den Takt unter der Maus waehlen
    auto st = engine.storage();
    if (st == nullptr) return;
    const double sr = engine.getSampleRate();
    const int64_t s = sampleAt (e.position.x), w = st->writtenNow();
    std::vector<Meta> m;
    st->readMeta (s - (int64_t) (12.0 * sr), s + (int64_t) (12.0 * sr), m);
    double bpm; float bpb;
    if (! tempoAt (m, s, bpm, bpb)) return;
    std::vector<GridPoint> g;
    gridFromMeta (m, w, sr, s - (int64_t) (12.0 * sr), s + (int64_t) (12.0 * sr), (double) bpb, g);
    int64_t a = -1, b = -1;
    for (const auto& p : g)
    {
        if (! p.bar) continue;
        if (p.sample <= s) a = p.sample;
        else { b = p.sample; break; }
    }
    if (a >= 0 && b > a) { selA = a; selB = b; startMarker = a; dragging = None; changed(); }
}

void HistoryView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& d)
{
    auto st = engine.storage();
    if (st == nullptr) return;
    if (e.mods.isShiftDown() || std::abs (d.deltaX) > std::abs (d.deltaY))
    {
        const float dx = std::abs (d.deltaX) > std::abs (d.deltaY) ? d.deltaX : d.deltaY;
        viewStart -= (int64_t) ((double) dx * (double) viewSpan * 0.5);
        clampView (*st);
        repaint();
    }
    else
        zoomAround (e.position.x, std::exp (-(double) d.deltaY * 0.9));
}

bool HistoryView::keyPressed (const KeyPress& k)
{
    auto st = engine.storage();
    if (st == nullptr) return false;
    const auto c = k.getTextCharacter();
    if (c == '+' || c == '=') { zoomAround ((float) getWidth() * 0.5f, 0.6); return true; }
    if (c == '-') { zoomAround ((float) getWidth() * 0.5f, 1.0 / 0.6); return true; }
    if (c == 'f' || c == 'F' || c == '0') { zoomFit(); return true; }
    if (k == KeyPress::escapeKey) { clearSelection(); return true; }
    if (k == KeyPress::leftKey || k == KeyPress::rightKey)
    {
        viewStart += (k == KeyPress::leftKey ? -1 : 1) * viewSpan / 8;
        clampView (*st);
        repaint();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------
// Zeichnen

static String timeLabel (double seconds)
{
    const int s = (int) std::round (seconds);
    if (s < 60) return "-" + String (s) + " s";
    return "-" + String (s / 60) + ":" + String (s % 60).paddedLeft ('0', 2);
}

void HistoryView::paint (Graphics& g)
{
    const auto& T = RememberColours::theme();
    const auto full = getLocalBounds().toFloat();
    g.setColour (T.inset);
    g.fillRoundedRectangle (full, T.radius);
    g.setColour (T.cardBorder);
    g.drawRoundedRectangle (full.reduced (0.5f), T.radius, 1.0f);

    auto st = engine.storage();
    if (st == nullptr || getWidth() < 10) return;
    updateView (*st);

    const int W = getWidth();
    const double sr = engine.getSampleRate();
    const int64_t w = st->writtenNow();
    const int64_t vs = viewStart, ve = viewStart + viewSpan;
    const auto wa = waveArea();

    g.saveState();
    g.reduceClipRegion (getLocalBounds().reduced (1));

    // --- Raster aus dem Host
    st->readMeta (vs, ve, metas);
    double bpm = 0.0; float bpb = 4.0f;
    const bool tempoKnown = tempoAt (metas, ve, bpm, bpb) || tempoAt (metas, vs, bpm, bpb);
    gridFromMeta (metas, w, sr, vs, ve, 1.0, grid);
    playStartsFromMeta (metas, vs, ve, playStarts);

    const float pxPerBeat = tempoKnown ? (float) ((60.0 / bpm) * sr * W / (double) viewSpan) : 0.0f;
    const float pxPerBar = pxPerBeat * bpb;

    // Unterteilung (1/16), wenn weit genug hineingezoomt
    if (tempoKnown && pxPerBeat > 110.0f)
    {
        std::vector<GridPoint> sub;
        gridFromMeta (metas, w, sr, vs, ve, 0.25, sub);
        g.setColour (T.grid.withAlpha (0.07f));
        for (const auto& p : sub) if (! p.beat) g.drawVerticalLine (roundToInt (xFor (p.sample)), wa.getY(), wa.getBottom());
    }
    // Beats und Takte
    for (const auto& p : grid)
    {
        const float x = xFor (p.sample);
        if (p.bar)
        {
            if (pxPerBar < 5.0f) continue;
            g.setColour (T.bar.withAlpha (0.5f));
            g.fillRect (Rectangle<float> (std::floor (x), rulerH - 6.0f, 1.0f, wa.getBottom() - rulerH + 6.0f));
        }
        else if (pxPerBeat >= 5.0f)
        {
            g.setColour (T.grid.withAlpha (0.2f));
            g.fillRect (Rectangle<float> (std::floor (x), wa.getY(), 1.0f, wa.getHeight()));
        }
    }
    // Taktnummern im Lineal
    if (tempoKnown && pxPerBar >= 8.0f)
    {
        int step = 1;
        while (pxPerBar * (float) step < 40.0f) step *= 2;
        g.setFont (Font (rememberFont (11.0f)));
        g.setColour (T.textDim);
        for (const auto& p : grid)
            if (p.bar)
            {
                const int barNo = (int) std::lround (p.ppq / (double) bpb) + 1;
                if (barNo % step != 0 && step > 1) continue;
                if (xFor (p.sample) > (float) W - 96.0f) continue;   // Platz fuer den Status-Chip
                g.drawText (String (barNo), Rectangle<float> (xFor (p.sample) + 3.0f, 3.0f, 40.0f, 14.0f), Justification::left, false);
            }
    }

    // --- Wellenform
    const int bins = std::max (1, W);
    mn.assign ((size_t) bins, 1.0f);
    mx.assign ((size_t) bins, -1.0f);
    st->getPeaks (vs, viewSpan, bins, mn.data(), mx.data());
    const float cy = wa.getCentreY(), hh = wa.getHeight() * 0.5f * 0.94f;
    int64_t s0 = 0, s1 = 0;
    const bool sel = getSelection (s0, s1);
    // Mittellinie
    g.setColour (T.grid);
    g.fillRect (Rectangle<float> (0.0f, std::floor (cy), (float) W, 1.0f));
    for (int i = 0; i < bins; ++i)
    {
        if (mn[(size_t) i] > mx[(size_t) i]) continue;
        const float lo = jlimit (-1.0f, 1.0f, mn[(size_t) i]), hi = jlimit (-1.0f, 1.0f, mx[(size_t) i]);
        const float y0 = cy - hi * hh, y1 = cy - lo * hh;
        const int64_t smp = vs + (int64_t) ((double) i / bins * (double) viewSpan);
        const bool inside = sel && smp >= s0 && smp <= s1;
        g.setColour (sel && ! inside ? T.waveDim : T.wave);
        g.fillRect (Rectangle<float> ((float) i, y0, 1.0f, std::max (1.0f, y1 - y0)));
    }

    // --- Auswahl
    if (sel)
    {
        const float x0 = xFor (s0), x1 = xFor (s1);
        g.setColour (T.selection.withAlpha (0.10f));
        g.fillRect (Rectangle<float> (x0, rulerH, x1 - x0, wa.getBottom() - rulerH));
        g.setColour (T.selection);
        g.fillRect (Rectangle<float> (x0 - 1.0f, rulerH - 6.0f, 2.0f, wa.getBottom() - rulerH + 6.0f));
        g.fillRect (Rectangle<float> (x1 - 1.0f, rulerH - 6.0f, 2.0f, wa.getBottom() - rulerH + 6.0f));
        // Beschriftung: Laenge
        double bars = 0.0;
        String label = String ((double) (s1 - s0) / sr, 2) + " s";
        if (getSelectionBars (bars) && bars > 0.0)
            label = String (bars, std::abs (bars - std::round (bars)) < 0.005 ? 0 : 2) + (std::abs (bars - 1.0) < 1.0e-6 ? " bar" : " bars") + String::fromUTF8 ("  \xc2\xb7  ") + label;
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

    // --- Play-Start (Host-Transport) und Startpunkt
    auto flag = [&] (float x, Colour c, bool down)
    {
        Path t;
        if (down) t.addTriangle (x - 5.0f, 2.0f, x + 5.0f, 2.0f, x, 11.0f);
        else t.addTriangle (x - 5.0f, 2.0f, x - 5.0f, 12.0f, x + 5.0f, 7.0f);
        g.setColour (c);
        g.fillPath (t);
    };
    for (auto p : playStarts)
    {
        const float x = xFor (p);
        g.setColour (T.playStart.withAlpha (0.8f));
        g.fillRect (Rectangle<float> (std::floor (x), rulerH - 6.0f, 1.0f, wa.getBottom() - rulerH + 6.0f));
        flag (x, T.playStart, false);
    }
    if (startMarker >= 0)
    {
        const float x = xFor (startMarker);
        g.setColour (T.start);
        g.fillRect (Rectangle<float> (std::floor (x) - 0.5f, rulerH - 6.0f, 2.0f, wa.getBottom() - rulerH + 6.0f));
        flag (x, T.start, true);
    }

    // --- Zeitachse unten (Sekunden vor "jetzt")
    {
        static const double steps[] = { 1, 2, 5, 10, 15, 30, 60, 120, 300, 600 };
        double step = steps[9];
        for (double s : steps) if (s * sr / (double) viewSpan * W >= 70.0) { step = s; break; }
        g.setFont (Font (rememberFont (10.5f)));
        g.setColour (T.textDim);
        const double tEnd = (double) (w - vs) / sr, tStart = (double) (w - ve) / sr;   // Sekunden vor jetzt am linken / rechten Rand
        for (double t = std::ceil (std::max (0.0, tStart) / step) * step; t <= tEnd; t += step)
        {
            const float x = xFor (w - (int64_t) (t * sr));
            g.fillRect (Rectangle<float> (std::floor (x), wa.getBottom(), 1.0f, 4.0f));
            if (x > 24.0f && x < (float) W - 24.0f)
                g.drawText (t < 1.0e-9 ? String ("NOW") : timeLabel (t), Rectangle<float> (x - 28.0f, wa.getBottom() + 3.0f, 56.0f, 12.0f),
                            Justification::centred, false);
        }
    }

    // --- Hinweise und Status
    {
        g.setFont (Font (rememberFont (11.0f, true)));
        const bool live = ! frozen();
        const String state = live ? "LIVE" : (hold ? "HOLD" : "FROZEN");
        const float tw = g.getCurrentFont().getStringWidthFloat (state) + 14.0f;
        const auto chip = Rectangle<float> ((float) W - tw - 8.0f, 3.0f, tw, 16.0f);
        g.setColour (live ? T.start.withAlpha (0.16f) : T.accent.withAlpha (0.16f));
        g.fillRoundedRectangle (chip, 8.0f);
        g.setColour (live ? T.start : T.accent);
        g.drawText (state, chip, Justification::centred);

        if (grid.empty() && playStarts.empty())
        {
            g.setFont (Font (rememberFont (11.0f)));
            g.setColour (T.textDim);
            g.drawText (engine.hostHasPpq.load() ? "Press play in the host for bar and beat markers"
                                                 : "The host sends no position, no bar markers",
                        Rectangle<float> (60.0f, 3.0f, (float) W - tw - 80.0f, 16.0f), Justification::centredRight, true);
        }
    }
    g.restoreState();
}

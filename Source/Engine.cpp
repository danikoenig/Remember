#include "Engine.h"

namespace RememberDSP
{
    static int64_t roundUp (int64_t v, int64_t q) { return ((v + q - 1) / q) * q; }

    Storage::Storage (int channels, int64_t capacityFrames)
        : numCh (channels),
          cap (roundUp (std::max<int64_t> (capacityFrames, peakBlock * 8), peakBlock)),
          metaCap (cap / metaInterval + 16)
    {
        data.assign ((size_t) (cap * numCh), 0.0f);
        pmin.assign ((size_t) (cap / peakBlock), 0.0f);
        pmax.assign ((size_t) (cap / peakBlock), 0.0f);
        meta.assign ((size_t) metaCap, Meta());
    }

    int64_t Storage::oldestValid() const
    {
        // Sicherheitsabstand, damit nicht gelesen wird, was der Audio-Thread gerade ueberschreibt
        const int64_t margin = 4096;
        return std::max<int64_t> (0, writtenNow() - cap + margin);
    }

    void Storage::getPeaks (int64_t start, int64_t len, int bins, float* mn, float* mx) const
    {
        const int64_t w = writtenNow(), oldest = oldestValid();
        for (int i = 0; i < bins; ++i)
        {
            const int64_t s = start + (int64_t) ((double) len * i / bins);
            const int64_t e = std::max<int64_t> (s + 1, start + (int64_t) ((double) len * (i + 1) / bins));
            if (s < oldest || e > w) { mn[i] = 1.0f; mx[i] = -1.0f; continue; }

            float lo = 1.0e9f, hi = -1.0e9f;
            if (e - s >= peakBlock * 2)
            {
                const int64_t b0 = (s + peakBlock - 1) / peakBlock, b1 = e / peakBlock;   // vollstaendige Bloecke
                for (int64_t b = b0; b < b1; ++b)
                {
                    const size_t k = (size_t) (b % (cap / peakBlock));
                    lo = std::min (lo, pmin[k]);
                    hi = std::max (hi, pmax[k]);
                }
            }
            if (hi < lo)   // zu wenig Bloecke oder kleiner Bereich: Rohdaten
            {
                for (int64_t t = s; t < e; ++t)
                    for (int c = 0; c < numCh; ++c)
                    {
                        const float v = data[(size_t) ((int64_t) c * cap + (t % cap))];
                        lo = std::min (lo, v);
                        hi = std::max (hi, v);
                    }
            }
            mn[i] = lo;
            mx[i] = hi;
        }
    }

    bool Storage::copyRange (int64_t start, int64_t len, float* const* dst, int dstChannels) const
    {
        if (len <= 0 || start < oldestValid() || start + len > writtenNow()) return false;
        for (int c = 0; c < dstChannels; ++c)
        {
            const int sc = std::min (c, numCh - 1);
            const float* src = data.data() + (int64_t) sc * cap;
            for (int64_t t = 0; t < len; ++t)
                dst[c][t] = src[(start + t) % cap];
        }
        return true;
    }

    void Storage::pushMeta (const Meta& mt)
    {
        const int64_t i = metaWritten.load (std::memory_order_relaxed);
        meta[(size_t) (i % metaCap)] = mt;
        metaWritten.store (i + 1, std::memory_order_release);
    }

    void Storage::readMeta (int64_t s0, int64_t s1, std::vector<Meta>& out, bool raw) const
    {
        out.clear();
        const int64_t sh = raw ? 0 : gridShift.load (std::memory_order_relaxed);
        s0 -= sh;
        s1 -= sh;
        const int64_t mw = metaWritten.load (std::memory_order_acquire);
        const int64_t first = std::max<int64_t> (0, mw - metaCap + 2);
        // binaere Suche: erster Eintrag mit sample > s0, davon einen zurueck
        int64_t lo = first, hi = mw;
        while (lo < hi)
        {
            const int64_t mid = (lo + hi) / 2;
            if (meta[(size_t) (mid % metaCap)].sample > s0) hi = mid; else lo = mid + 1;
        }
        int64_t i = std::max (first, lo - 1);
        for (; i < mw; ++i)
        {
            const Meta& e = meta[(size_t) (i % metaCap)];
            out.push_back (e);
            out.back().sample += sh;
            if (e.sample >= s1) break;
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Raster und Umrechnung

    namespace
    {
        struct Seg { const Meta* a; int64_t end; double ppqEnd; bool ok; };

        Seg segment (const std::vector<Meta>& m, size_t i, int64_t endSample, double sr)
        {
            Seg s { &m[i], 0, 0.0, false };
            const Meta& a = m[i];
            if (! (a.flags & fPlaying)) return s;
            s.end = (i + 1 < m.size()) ? m[i + 1].sample : endSample;
            if (s.end <= a.sample) return s;
            const bool contiguous = (i + 1 < m.size()) && ! (m[i + 1].flags & fJump) && (m[i + 1].flags & fPlaying);
            s.ppqEnd = contiguous ? m[i + 1].ppq : a.ppq + (double) (s.end - a.sample) / sr * a.bpm / 60.0;
            s.ok = s.ppqEnd > a.ppq;
            return s;
        }
    }

    void gridFromMeta (const std::vector<Meta>& m, int64_t endSample, double sr,
                       int64_t s0, int64_t s1, double gridBeats, std::vector<GridPoint>& out)
    {
        out.clear();
        if (gridBeats <= 0.0 || sr <= 0.0) return;
        for (size_t i = 0; i < m.size(); ++i)
        {
            const Seg sg = segment (m, i, endSample, sr);
            if (! sg.ok || sg.end < s0 || sg.a->sample > s1) continue;
            const Meta& a = *sg.a;
            const double spb = (double) (sg.end - a.sample) / (sg.ppqEnd - a.ppq);   // Samples pro Viertel
            int64_t k = (int64_t) std::ceil ((a.ppq - a.barStartPpq) / gridBeats - 1.0e-9);
            for (int guard = 0; guard < 200000; ++guard, ++k)
            {
                const double p = a.barStartPpq + (double) k * gridBeats;
                if (p >= sg.ppqEnd - 1.0e-9) break;
                const int64_t s = a.sample + (int64_t) std::llround ((p - a.ppq) * spb);
                if (s > s1) break;
                if (s < s0) continue;
                GridPoint g;
                g.sample = s;
                g.ppq = p;
                const double rel = p - a.barStartPpq;
                const double bars = rel / (double) a.beatsPerBar;
                g.bar = std::fabs (bars - std::round (bars)) < 1.0e-6;
                g.beat = std::fabs (rel - std::round (rel)) < 1.0e-6;
                if (! out.empty() && out.back().sample == g.sample) continue;
                out.push_back (g);
            }
        }
    }

    void playStartsFromMeta (const std::vector<Meta>& m, int64_t s0, int64_t s1, std::vector<int64_t>& out)
    {
        out.clear();
        for (const auto& e : m)
            if ((e.flags & fPlayStart) && e.sample >= s0 && e.sample <= s1) out.push_back (e.sample);
    }

    bool sampleToPpq (const std::vector<Meta>& m, int64_t endSample, double sr, int64_t s, double& ppq)
    {
        for (size_t i = 0; i < m.size(); ++i)
        {
            const Seg sg = segment (m, i, endSample, sr);
            if (! sg.ok || s < sg.a->sample || s >= sg.end) continue;
            ppq = sg.a->ppq + (double) (s - sg.a->sample) * (sg.ppqEnd - sg.a->ppq) / (double) (sg.end - sg.a->sample);
            return true;
        }
        return false;
    }

    bool ppqToSample (const std::vector<Meta>& m, int64_t endSample, double sr, double ppq, int64_t& s)
    {
        for (size_t i = 0; i < m.size(); ++i)
        {
            const Seg sg = segment (m, i, endSample, sr);
            if (! sg.ok || ppq < sg.a->ppq - 1.0e-9 || ppq >= sg.ppqEnd) continue;
            s = sg.a->sample + (int64_t) std::llround ((ppq - sg.a->ppq) * (double) (sg.end - sg.a->sample) / (sg.ppqEnd - sg.a->ppq));
            return true;
        }
        return false;
    }

    bool tempoAt (const std::vector<Meta>& m, int64_t s, double& bpm, float& beatsPerBar)
    {
        const Meta* best = nullptr;
        for (const auto& e : m)
        {
            if (e.sample > s) break;
            if (e.flags & fPlaying) best = &e;
        }
        if (best == nullptr)
            for (const auto& e : m) if (e.flags & fPlaying) { best = &e; break; }
        if (best == nullptr) return false;
        bpm = best->bpm;
        beatsPerBar = best->beatsPerBar;
        return true;
    }

    // ---------------------------------------------------------------------------------------------
    // Engine

    std::shared_ptr<Storage> Engine::storage() const
    {
        std::lock_guard<std::mutex> l (m);
        return current;
    }

    void Engine::setGridShift (int64_t samples)
    {
        gridShiftSamples = samples;
        std::lock_guard<std::mutex> l (m);
        if (current) current->gridShift.store (samples);
    }

    void Engine::publish (std::shared_ptr<Storage> s)
    {
        s->gridShift.store (gridShiftSamples.load());
        std::lock_guard<std::mutex> l (m);
        if (current) retired.push_back (current);
        current = std::move (s);
        audioPtr.store (current.get(), std::memory_order_seq_cst);
    }

    void Engine::collectGarbage()
    {
        std::lock_guard<std::mutex> l (m);
        const Storage* busy = inUse.load (std::memory_order_seq_cst);
        retired.erase (std::remove_if (retired.begin(), retired.end(),
                                       [busy] (const std::shared_ptr<Storage>& p) { return p.get() != busy; }),
                       retired.end());
    }

    void Engine::prepare (double sampleRate, int ch, double historySeconds)
    {
        const int64_t frames = (int64_t) std::llround (sampleRate * historySeconds);
        {
            std::lock_guard<std::mutex> l (m);
            if (current && std::fabs (sr.load() - sampleRate) < 1.0e-6 && current->numCh == ch && current->cap >= frames
                && current->cap < frames + 4 * peakBlock)
            {
                historySec = historySeconds;
                return;
            }
        }
        sr = sampleRate;
        channels = ch;
        historySec = historySeconds;
        publish (std::make_shared<Storage> (ch, frames));
        haveLast = false;
    }

    void Engine::setHistorySeconds (double seconds)
    {
        std::shared_ptr<Storage> old = storage();
        const double rate = sr.load();
        const int64_t frames = (int64_t) std::llround (rate * seconds);
        if (old && old->cap >= frames && old->cap < frames + 4 * peakBlock) { historySec = seconds; return; }

        auto fresh = std::make_shared<Storage> (old ? old->numCh : channels, frames);
        if (old)
        {
            // Ende der alten Aufnahme uebernehmen
            const int64_t w = old->writtenNow();
            const int64_t keep = std::min<int64_t> (fresh->cap - 4096, w - old->oldestValid());
            if (keep > 0)
            {
                const int64_t from = w - keep;
                for (int c = 0; c < fresh->numCh; ++c)
                    for (int64_t t = from; t < w; ++t)
                        fresh->data[(size_t) ((int64_t) c * fresh->cap + (t % fresh->cap))] =
                            old->data[(size_t) ((int64_t) c * old->cap + (t % old->cap))];
                const int64_t b0 = (from + peakBlock - 1) / peakBlock, b1 = w / peakBlock;
                for (int64_t b = b0; b < b1; ++b)
                {
                    fresh->pmin[(size_t) (b % (fresh->cap / peakBlock))] = old->pmin[(size_t) (b % (old->cap / peakBlock))];
                    fresh->pmax[(size_t) (b % (fresh->cap / peakBlock))] = old->pmax[(size_t) (b % (old->cap / peakBlock))];
                }
                std::vector<Meta> mm;
                old->readMeta (from, w, mm, true);
                for (const auto& e : mm) fresh->pushMeta (e);
            }
            fresh->written.store (w, std::memory_order_release);
            fresh->curMin = old->curMin;
            fresh->curMax = old->curMax;
        }
        historySec = seconds;
        publish (fresh);
    }

    void Engine::process (const float* const* in, int numCh, int n, const HostInfo& h)
    {
        if (n <= 0 || numCh <= 0) return;

        // Hazard-Pointer: Speicher fuer diesen Block festhalten
        Storage* s;
        do
        {
            s = audioPtr.load (std::memory_order_seq_cst);
            inUse.store (s, std::memory_order_seq_cst);
        } while (s != audioPtr.load (std::memory_order_seq_cst));
        if (s == nullptr) return;

        const double rate = sr.load (std::memory_order_relaxed);
        int64_t w = s->written.load (std::memory_order_relaxed);

        // --- Transport-Eintraege
        const bool playing = h.playing && h.hasPpq;
        transportPlaying.store (playing, std::memory_order_relaxed);
        hostHasPpq.store (h.hasPpq, std::memory_order_relaxed);
        lastBpm.store (h.bpm, std::memory_order_relaxed);

        Meta cur;
        cur.sample = w;
        cur.ppq = h.ppq;
        cur.bpm = h.bpm > 1.0 ? h.bpm : 120.0;
        cur.barStartPpq = h.barStartPpq;
        cur.beatsPerBar = h.beatsPerBar > 0.0f ? h.beatsPerBar : 4.0f;
        cur.flags = playing ? fPlaying : 0;

        bool need = ! haveLast;
        if (haveLast)
        {
            const bool wasPlaying = (lastMeta.flags & fPlaying) != 0;
            if (playing != wasPlaying)
            {
                need = true;
                if (playing) cur.flags |= (fPlayStart | fJump);
            }
            else if (playing)
            {
                const double pred = lastMeta.ppq + (double) (w - lastMeta.sample) / rate * (lastMeta.bpm + cur.bpm) * 0.5 / 60.0;
                if (std::fabs (h.ppq - pred) > 0.02) { cur.flags |= fJump; need = true; }
                else if (w - lastMetaSample >= metaInterval) need = true;
            }
        }
        else if (playing)
            cur.flags |= (fPlayStart | fJump);

        if (need)
        {
            s->pushMeta (cur);
            lastMeta = cur;
            lastMetaSample = w;
            haveLast = true;
        }
        if (playing)
            for (int off = metaInterval; off < n; off += metaInterval)
                if (w + off - lastMetaSample >= metaInterval)
                {
                    Meta e = cur;
                    e.sample = w + off;
                    e.ppq = h.ppq + (double) off / rate * cur.bpm / 60.0;
                    e.flags = fPlaying;
                    s->pushMeta (e);
                    lastMeta = e;
                    lastMetaSample = e.sample;
                }

        // --- Audio und Spitzenwerte
        const int sc = s->numCh;
        const int64_t cap = s->cap;
        const int64_t nPeaks = cap / peakBlock;
        float lo = s->curMin, hi = s->curMax;
        for (int i = 0; i < n; ++i)
        {
            const int64_t idx = (w + i) % cap;
            float mn = 1.0e9f, mx = -1.0e9f;
            for (int c = 0; c < sc; ++c)
            {
                const float v = in[std::min (c, numCh - 1)][i];
                s->data[(size_t) ((int64_t) c * cap + idx)] = v;
                mn = std::min (mn, v);
                mx = std::max (mx, v);
            }
            const int64_t pos = w + i;
            if (pos % peakBlock == 0) { lo = mn; hi = mx; }
            else { lo = std::min (lo, mn); hi = std::max (hi, mx); }
            if (pos % peakBlock == peakBlock - 1)
            {
                const size_t k = (size_t) ((pos / peakBlock) % nPeaks);
                s->pmin[k] = lo;
                s->pmax[k] = hi;
            }
        }
        s->curMin = lo;
        s->curMax = hi;
        s->written.store (w + n, std::memory_order_release);
    }
}

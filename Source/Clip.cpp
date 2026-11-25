#include "Clip.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace RememberClip
{
    Range clampRange (const Clip& c, Range r)
    {
        r.a = std::max<int64_t> (0, std::min (r.a, c.length()));
        r.b = std::max (r.a, std::min (r.b, c.length()));
        return r;
    }

    Channels extract (const Clip& c, Range r)
    {
        r = clampRange (c, r);
        Channels out (c.ch.size());
        for (size_t k = 0; k < c.ch.size(); ++k)
            out[k].assign (c.ch[k].begin() + r.a, c.ch[k].begin() + r.b);
        return out;
    }

    void deleteRange (Clip& c, Range r)
    {
        r = clampRange (c, r);
        if (r.empty()) return;
        for (auto& v : c.ch) v.erase (v.begin() + r.a, v.begin() + r.b);
        if (c.gridOffset >= (double) r.b) c.gridOffset -= (double) r.length();
        else if (c.gridOffset > (double) r.a) c.gridOffset = (double) r.a;
    }

    void crop (Clip& c, Range r)
    {
        r = clampRange (c, r);
        if (r.empty()) return;
        for (auto& v : c.ch)
        {
            std::vector<float> n (v.begin() + r.a, v.begin() + r.b);
            v.swap (n);
        }
        c.gridOffset -= (double) r.a;
    }

    void insert (Clip& c, int64_t pos, const Channels& data)
    {
        if (data.empty() || data[0].empty()) return;
        if (c.ch.empty()) { c.ch = data; return; }
        pos = std::max<int64_t> (0, std::min (pos, c.length()));
        const int64_t n = (int64_t) data[0].size();
        for (size_t k = 0; k < c.ch.size(); ++k)
        {
            const auto& src = data[std::min (k, data.size() - 1)];
            c.ch[k].insert (c.ch[k].begin() + pos, src.begin(), src.begin() + n);
        }
        if (c.gridOffset >= (double) pos) c.gridOffset += (double) n;
    }

    void reverse (Clip& c, Range r)
    {
        r = clampRange (c, r);
        for (auto& v : c.ch) std::reverse (v.begin() + r.a, v.begin() + r.b);
    }

    void applyGain (Clip& c, Range r, float g)
    {
        r = clampRange (c, r);
        for (auto& v : c.ch)
            for (int64_t i = r.a; i < r.b; ++i) v[(size_t) i] *= g;
    }

    static float fadeCurve (double t)   // 0..1, weich (Kosinus)
    {
        return (float) (0.5 - 0.5 * std::cos (3.14159265358979323846 * std::min (1.0, std::max (0.0, t))));
    }

    void fadeIn (Clip& c, Range r)
    {
        r = clampRange (c, r);
        const int64_t n = r.length();
        if (n < 2) return;
        for (auto& v : c.ch)
            for (int64_t i = 0; i < n; ++i) v[(size_t) (r.a + i)] *= fadeCurve ((double) i / (double) (n - 1));
    }

    void fadeOut (Clip& c, Range r)
    {
        r = clampRange (c, r);
        const int64_t n = r.length();
        if (n < 2) return;
        for (auto& v : c.ch)
            for (int64_t i = 0; i < n; ++i) v[(size_t) (r.a + i)] *= fadeCurve (1.0 - (double) i / (double) (n - 1));
    }

    Range findNonSilent (const Clip& c, float thresholdDb)
    {
        const float th = std::pow (10.0f, thresholdDb / 20.0f);
        const int64_t n = c.length();
        int64_t first = -1, last = -1;
        for (int64_t i = 0; i < n && first < 0; ++i)
            for (const auto& v : c.ch) if (std::fabs (v[(size_t) i]) > th) { first = i; break; }
        if (first < 0) return {};
        for (int64_t i = n - 1; i >= first && last < 0; --i)
            for (const auto& v : c.ch) if (std::fabs (v[(size_t) i]) > th) { last = i; break; }
        return { first, last + 1 };
    }

    void trimSilence (Clip& c, float thresholdDb)
    {
        const auto r = findNonSilent (c, thresholdDb);
        if (! r.empty()) crop (c, r);
    }

    float peakAbs (const Clip& c, Range r)
    {
        r = clampRange (c, r);
        float p = 0.0f;
        for (const auto& v : c.ch)
            for (int64_t i = r.a; i < r.b; ++i) p = std::max (p, std::fabs (v[(size_t) i]));
        return p;
    }

    // ---------------------------------------------------------------------------------------------
    // Lautheit nach ITU-R BS.1770-4 (K-Gewichtung: Hochregal + Hochpass, 400-ms-Bloecke, 100 ms Schritt, zweistufiges Gating)

    namespace
    {
        struct Biquad
        {
            double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
            double process (double x)
            {
                const double y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
        };

        constexpr double pi = 3.14159265358979323846;

        // K-Gewichtung wie in BS.1770 / libebur128 (Koeffizienten fuer jede Samplerate aus den Standard-Parametern)
        Biquad kShelf (double fs)
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (pi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            Biquad f;
            f.b0 = (Vh + Vb * K / Q + K * K) / a0;
            f.b1 = 2.0 * (K * K - Vh) / a0;
            f.b2 = (Vh - Vb * K / Q + K * K) / a0;
            f.a1 = 2.0 * (K * K - 1.0) / a0;
            f.a2 = (1.0 - K / Q + K * K) / a0;
            return f;
        }

        Biquad kHighPass (double fs)
        {
            const double f0 = 38.13547087613982, Q = 0.5003270373253953;
            const double K = std::tan (pi * f0 / fs), a0 = 1.0 + K / Q + K * K;
            Biquad f;                       // b = {1, -2, 1}
            f.b0 = 1.0; f.b1 = -2.0; f.b2 = 1.0;
            f.a1 = 2.0 * (K * K - 1.0) / a0;
            f.a2 = (1.0 - K / Q + K * K) / a0;
            return f;
        }
    }

    double integratedLufs (const Clip& c, Range r)
    {
        r = clampRange (c, r);
        const int64_t n = r.length();
        const double ninf = -std::numeric_limits<double>::infinity();
        if (n < 16 || c.ch.empty()) return ninf;

        // K-gewichtetes Quadrat je Sample, ueber alle Kanaele summiert (Kanalgewichte 1,0 fuer L/R und Mono)
        std::vector<double> sq ((size_t) n, 0.0);
        const int chans = std::min (c.numChannels(), 2);
        for (int k = 0; k < chans; ++k)
        {
            Biquad shelf = kShelf (c.sampleRate);
            Biquad hp = kHighPass (c.sampleRate);
            const auto& v = c.ch[(size_t) k];
            for (int64_t i = 0; i < n; ++i)
            {
                const double y = hp.process (shelf.process ((double) v[(size_t) (r.a + i)]));
                sq[(size_t) i] += y * y;
            }
        }
        std::vector<double> prefix ((size_t) n + 1, 0.0);
        for (int64_t i = 0; i < n; ++i) prefix[(size_t) i + 1] = prefix[(size_t) i] + sq[(size_t) i];

        const int64_t blk = (int64_t) std::llround (0.4 * c.sampleRate), hop = (int64_t) std::llround (0.1 * c.sampleRate);
        std::vector<double> power;   // mittlere Leistung je Block
        if (n < blk) power.push_back (prefix[(size_t) n] / (double) n);
        else
            for (int64_t s = 0; s + blk <= n; s += hop)
                power.push_back ((prefix[(size_t) (s + blk)] - prefix[(size_t) s]) / (double) blk);

        auto lufs = [] (double p) { return -0.691 + 10.0 * std::log10 (p); };
        const double absGate = std::pow (10.0, (-70.0 + 0.691) / 10.0);
        double sum = 0.0; size_t cnt = 0;
        for (double p : power) if (p > absGate) { sum += p; ++cnt; }
        if (cnt == 0) return ninf;
        const double relGate = std::pow (10.0, (lufs (sum / (double) cnt) - 10.0 + 0.691) / 10.0);
        sum = 0.0; cnt = 0;
        for (double p : power) if (p > absGate && p > relGate) { sum += p; ++cnt; }
        if (cnt == 0) return ninf;
        return lufs (sum / (double) cnt);
    }

    NormalizeResult normalizePeak (Clip& c, Range r, double targetDb)
    {
        NormalizeResult res;
        const float pk = peakAbs (c, r);
        if (pk < 1.0e-9f) return res;
        res.gainDb = targetDb - 20.0 * std::log10 ((double) pk);
        applyGain (c, r, (float) std::pow (10.0, res.gainDb / 20.0));
        res.ok = true;
        return res;
    }

    NormalizeResult normalizeLufs (Clip& c, Range r, double targetLufs, double ceilingDb)
    {
        NormalizeResult res;
        const double l = integratedLufs (c, r);
        const float pk = peakAbs (c, r);
        if (! std::isfinite (l) || pk < 1.0e-9f) return res;
        double g = targetLufs - l;
        const double maxG = ceilingDb - 20.0 * std::log10 ((double) pk);
        if (g > maxG) { g = maxG; res.limited = true; }
        res.gainDb = g;
        applyGain (c, r, (float) std::pow (10.0, g / 20.0));
        res.ok = true;
        return res;
    }

    // ---------------------------------------------------------------------------------------------

    void Session::load (Clip&& c)
    {
        beginEdit();
        clip = std::move (c);
        touch();
    }

    void Session::beginEdit()
    {
        undoStack.push_back (clip);
        if (undoStack.size() > maxUndo) undoStack.erase (undoStack.begin());
        redoStack.clear();
    }

    bool Session::undo()
    {
        if (undoStack.empty()) return false;
        redoStack.push_back (std::move (clip));
        clip = std::move (undoStack.back());
        undoStack.pop_back();
        touch();
        return true;
    }

    bool Session::redo()
    {
        if (redoStack.empty()) return false;
        undoStack.push_back (std::move (clip));
        clip = std::move (redoStack.back());
        redoStack.pop_back();
        touch();
        return true;
    }

    void Session::clear()
    {
        clip = Clip();
        undoStack.clear();
        redoStack.clear();
        touch();
    }
}

#pragma once
// Raster-Kalibrierung ohne JUCE (testbar in tests/ClipTest.cpp).
// Misst, wie weit die Schlaege (Onsets) im Audio hinter den Beat-Linien des Hosts liegen. Der Host meldet die Position
// des Blockanfangs, Plugins vor Remember (z. B. Instrumente mit Latenz) verzoegern das Audio aber, ohne dass der Host das weitergibt.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace RememberAlign
{
    struct Result
    {
        bool ok = false;
        int64_t offsetSamples = 0;     // > 0: das Audio liegt hinter dem Raster (Raster muss nach hinten)
        double spreadMs = 0.0;         // Streuung der einzelnen Beats (Medianabweichung)
        int hits = 0;                  // Anzahl ausgewerteter Beats
    };

    // mono: Audio. beats: Position der Beat-Linien in Samples (relativ zu mono[0]). beatSeconds: Laenge eines Beats
    inline Result estimateGridOffset (const std::vector<float>& mono, double sr, const std::vector<int64_t>& beats, double beatSeconds)
    {
        Result res;
        const int64_t n = (int64_t) mono.size();
        const int64_t blk = std::max<int64_t> (1, (int64_t) std::llround (sr * 0.001));      // 1 ms
        const int64_t nb = n / blk;
        if (nb < 16 || beats.size() < 2 || beatSeconds <= 0.0) return res;

        // Huellkurve (RMS je ms) und Anstieg
        std::vector<float> env ((size_t) nb), on ((size_t) nb, 0.0f);
        for (int64_t b = 0; b < nb; ++b)
        {
            double acc = 0.0;
            for (int64_t i = 0; i < blk; ++i) { const double v = mono[(size_t) (b * blk + i)]; acc += v * v; }
            env[(size_t) b] = (float) std::sqrt (acc / (double) blk);
        }
        for (int64_t b = 2; b < nb; ++b)
            on[(size_t) b] = std::max (0.0f, env[(size_t) b] - 0.5f * (env[(size_t) b - 1] + env[(size_t) b - 2]));

        struct Cand { int64_t grid; int64_t block; float strength; };
        std::vector<Cand> cands;
        const int64_t win = (int64_t) (0.3 * beatSeconds * 1000.0);                    // +-0.3 Beat in ms (= Bloecke)
        float best = 0.0f;
        for (const auto g : beats)
        {
            const int64_t bg = g / blk;
            int64_t bb = -1;
            float bs = 0.0f;
            for (int64_t b = std::max<int64_t> (2, bg - win); b <= std::min (nb - 1, bg + win); ++b)
                if (on[(size_t) b] > bs) { bs = on[(size_t) b]; bb = b; }
            if (bb >= 0) { cands.push_back ({ g, bb, bs }); best = std::max (best, bs); }
        }

        std::vector<double> offs;
        for (const auto& c : cands)
        {
            if (c.strength < 0.25f * best) continue;
            // Sample-genau: erster Wert, der 12 % des Spitzenwerts der naechsten 8 ms erreicht, ab 6 ms vor dem Anstieg
            const int64_t a = std::max<int64_t> (0, c.block * blk - 6 * blk), z = std::min (n, c.block * blk + 8 * blk);
            float pk = 0.0f;
            for (int64_t i = c.block * blk; i < z; ++i) pk = std::max (pk, std::fabs (mono[(size_t) i]));
            if (pk <= 0.0f) continue;
            int64_t p = c.block * blk;
            for (int64_t i = a; i < z; ++i)
                if (std::fabs (mono[(size_t) i]) >= 0.12f * pk) { p = i; break; }
            offs.push_back ((double) (p - c.grid));
        }
        if (offs.size() < 2) return res;

        std::sort (offs.begin(), offs.end());
        const double med = offs[offs.size() / 2];
        std::vector<double> dev;
        for (const auto o : offs) dev.push_back (std::fabs (o - med));
        std::sort (dev.begin(), dev.end());
        res.spreadMs = dev[dev.size() / 2] * 1000.0 / sr;
        res.hits = (int) offs.size();
        res.offsetSamples = (int64_t) std::llround (med);
        res.ok = res.spreadMs < 4.0;
        return res;
    }
}

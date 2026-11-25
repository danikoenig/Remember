// Test der Engine ohne JUCE:  g++ -std=c++17 -O1 -Wall tests/EngineTest.cpp Source/Engine.cpp -o /tmp/enginetest && /tmp/enginetest
#include "../Source/Align.h"
#include "../Source/Engine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace RememberDSP;

static int failures = 0;
#define CHECK(c) do { if (! (c)) { std::printf ("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static void feed (Engine& e, int64_t from, int64_t count, int block, double bpm, bool playing, double ppq0, double sr,
                  float amp = 0.5f, double barStart = 0.0)
{
    std::vector<float> l ((size_t) block), r ((size_t) block);
    int64_t done = 0;
    while (done < count)
    {
        const int n = (int) std::min<int64_t> (block, count - done);
        for (int i = 0; i < n; ++i) { l[(size_t) i] = amp * (((from + done + i) / 100) % 2 ? 1.0f : -1.0f); r[(size_t) i] = l[(size_t) i]; }
        const float* in[2] = { l.data(), r.data() };
        HostInfo h;
        h.playing = playing; h.hasPpq = true; h.bpm = bpm;
        h.ppq = ppq0 + (double) done / sr * bpm / 60.0;
        h.barStartPpq = barStart;
        e.process (in, 2, n, h);
        done += n;
    }
}

int main()
{
    const double sr = 48000.0;
    Engine e;
    e.prepare (sr, 2, 60.0);
    auto s = e.storage();
    CHECK (s != nullptr);

    // 1) Transport laeuft ab Sample 0, 120 bpm: 1 Beat = 24000 Samples, 1 Takt = 96000
    feed (e, 0, 480000, 480, 120.0, true, 0.0, sr);
    CHECK (s->writtenNow() == 480000);

    std::vector<Meta> m;
    s->readMeta (0, 480000, m);
    CHECK (! m.empty());
    std::vector<GridPoint> g;
    gridFromMeta (m, s->writtenNow(), sr, 0, 480000, 1.0, g);
    CHECK (g.size() == 20);                                   // Beats 0..19
    bool beatsOk = true, barsOk = true;
    for (size_t i = 0; i < g.size(); ++i)
    {
        if (g[i].sample != (int64_t) i * 24000) beatsOk = false;
        if (g[i].bar != (i % 4 == 0)) barsOk = false;
    }
    CHECK (beatsOk);
    CHECK (barsOk);

    // Raster-Versatz: Linien wandern um 3360 Samples (70 ms) nach hinten, ppq <-> Sample bleibt konsistent
    s->gridShift.store (3360);
    s->readMeta (0, 480000, m);
    gridFromMeta (m, s->writtenNow(), sr, 0, 480000, 1.0, g);
    CHECK (g.size() >= 19 && g[0].sample == 3360 && g[1].sample == 24000 + 3360);
    int64_t shifted = 0;
    CHECK (ppqToSample (m, s->writtenNow(), sr, 4.0, shifted) && shifted == 96000 + 3360);
    s->gridShift.store (0);
    s->readMeta (0, 480000, m);

    // Ende-zu-Ende: Kicks liegen 70 ms hinter den Beats, Messung -> Raster-Versatz -> Taktlinie liegt auf dem Kick
    {
        Engine e2;
        e2.prepare (sr, 2, 60.0);
        const int64_t lag = 3360, total = 240000;
        std::vector<float> a ((size_t) total, 0.0f);
        for (int k = 0; k < 10; ++k)
            for (int64_t i = 0; i < 12000; ++i)
                a[(size_t) (k * 24000 + lag + i)] = 0.8f * (float) (std::sin (2.0 * 3.14159265358979 * 60.0 * (double) i / sr) * std::exp (-(double) i / 3000.0));
        for (int64_t done = 0; done < total; done += 512)
        {
            const int n = (int) std::min<int64_t> (512, total - done);
            const float* in[2] = { a.data() + done, a.data() + done };
            HostInfo h;
            h.playing = true; h.hasPpq = true; h.bpm = 120.0; h.ppq = (double) done / sr * 2.0; h.barStartPpq = 0.0;
            e2.process (in, 2, n, h);
        }
        auto s6 = e2.storage();
        std::vector<Meta> mm;
        std::vector<GridPoint> gg;
        s6->readMeta (24000, 216000, mm);
        gridFromMeta (mm, s6->writtenNow(), sr, 24000, 216000, 1.0, gg);
        std::vector<int64_t> bt;
        for (const auto& p : gg) bt.push_back (p.sample);
        const auto est = RememberAlign::estimateGridOffset (a, sr, bt, 0.5);
        CHECK (est.ok && std::llabs (est.offsetSamples - lag) <= 48);
        e2.setGridShift (est.offsetSamples);
        s6->readMeta (24000, 216000, mm);
        gridFromMeta (mm, s6->writtenNow(), sr, 24000, 216000, 1.0, gg);
        CHECK (gg.size() >= 6 && std::llabs (gg[0].sample - (24000 + lag)) <= 48);
        const auto est2 = RememberAlign::estimateGridOffset (a, sr, [&] { std::vector<int64_t> b2; for (const auto& p : gg) b2.push_back (p.sample); return b2; }(), 0.5);
        CHECK (est2.ok && std::llabs (est2.offsetSamples) <= 48);       // danach kein Rest mehr
    }

    gridFromMeta (m, s->writtenNow(), sr, 0, 480000, 0.25, g);   // 1/16
    CHECK (g.size() == 80);
    CHECK (g[1].sample == 6000);

    std::vector<int64_t> ps;
    playStartsFromMeta (m, 0, 480000, ps);
    CHECK (ps.size() == 1 && ps[0] == 0);

    int64_t smp = 0;
    CHECK (ppqToSample (m, s->writtenNow(), sr, 8.0, smp) && smp == 192000);
    double ppq = 0;
    CHECK (sampleToPpq (m, s->writtenNow(), sr, 96000, ppq) && std::fabs (ppq - 4.0) < 1e-6);
    double bpm = 0; float bpb = 0;
    CHECK (tempoAt (m, 1000, bpm, bpb) && bpm == 120.0 && bpb == 4.0f);

    // 2) Audio lesen
    std::vector<float> a (1000), b (1000);
    float* dst[2] = { a.data(), b.data() };
    CHECK (s->copyRange (1000, 1000, dst, 2));
    CHECK (a[0] == 0.5f * (((1000) / 100) % 2 ? 1.0f : -1.0f));
    CHECK (! s->copyRange (470000, 20000, dst, 2));          // ueber das Ende

    // 3) Spitzenwerte
    std::vector<float> mn (100), mx (100);
    s->getPeaks (10000, 400000, 100, mn.data(), mx.data());
    CHECK (mn[50] == -0.5f && mx[50] == 0.5f);
    s->getPeaks (10000, 3000, 100, mn.data(), mx.data());     // Rohdaten
    CHECK (mx[10] <= 0.5f && mn[10] >= -0.5f);

    // 4) Sprung im Transport (Loop): nach 48000 Samples springt ppq zurueck auf 0
    Engine e2;
    e2.prepare (sr, 2, 30.0);
    auto s2 = e2.storage();
    feed (e2, 0, 48000, 256, 120.0, true, 0.0, sr);           // 2 Beats
    feed (e2, 48000, 48000, 256, 120.0, true, 0.0, sr);       // wieder 2 Beats ab ppq 0
    s2->readMeta (0, 96000, m);
    gridFromMeta (m, s2->writtenNow(), sr, 0, 96000, 1.0, g);
    CHECK (g.size() == 4);
    CHECK (g.size() == 4 && g[2].sample == 48000 && g[3].sample == 72000);
    playStartsFromMeta (m, 0, 96000, ps);
    CHECK (ps.size() == 1);

    // 5) Stopp und erneuter Start: neuer Play-Start-Marker
    feed (e2, 96000, 24000, 256, 120.0, false, 4.0, sr);
    feed (e2, 120000, 24000, 256, 120.0, true, 8.0, sr);
    s2->readMeta (0, 144000, m);
    playStartsFromMeta (m, 0, 144000, ps);
    CHECK (ps.size() == 2 && ps[1] == 120000);
    gridFromMeta (m, s2->writtenNow(), sr, 96000, 144000, 1.0, g);
    CHECK (g.size() == 1 && g[0].sample == 120000 && g[0].ppq == 8.0);

    // 6) Tempo-Wechsel (60 bpm: 1 Beat = 48000 Samples)
    Engine e3;
    e3.prepare (sr, 2, 30.0);
    auto s3 = e3.storage();
    feed (e3, 0, 96000, 512, 60.0, true, 0.0, sr);
    s3->readMeta (0, 96000, m);
    gridFromMeta (m, s3->writtenNow(), sr, 0, 96000, 1.0, g);
    CHECK (g.size() == 2 && g[1].sample == 48000);

    // 7) History-Laenge aendern: Ende der Aufnahme bleibt
    Engine e4;
    e4.prepare (sr, 2, 10.0);
    feed (e4, 0, 480000, 480, 120.0, true, 0.0, sr);          // 10 s, Puffer voll
    auto old = e4.storage();
    e4.setHistorySeconds (20.0);
    auto neu = e4.storage();
    CHECK (neu != old && neu->cap > old->cap);
    CHECK (neu->writtenNow() == 480000);
    std::vector<float> big (48000), big2 (48000);
    float* d2[2] = { big.data(), big2.data() };
    CHECK (neu->copyRange (470000, 10000, d2, 2));
    CHECK (big[0] == 0.5f * (((470000) / 100) % 2 ? 1.0f : -1.0f));
    feed (e4, 480000, 4800, 480, 120.0, true, 20.0, sr);       // schreibt in den neuen Speicher weiter
    CHECK (neu->writtenNow() == 484800);
    old.reset();
    e4.collectGarbage();

    // 8) Ringpuffer laeuft ueber: sehr altes Material ist nicht mehr lesbar
    Engine e5;
    e5.prepare (sr, 2, 5.0);
    feed (e5, 0, 48000 * 12, 480, 120.0, true, 0.0, sr);
    auto s5 = e5.storage();
    CHECK (! s5->copyRange (0, 1000, dst, 2));
    CHECK (s5->copyRange (s5->writtenNow() - 48000, 1000, dst, 2));
    s5->readMeta (s5->writtenNow() - 48000, s5->writtenNow(), m);
    CHECK (! m.empty());

    // 9) Mono-Eingang in Stereo-Speicher
    Engine e6;
    e6.prepare (sr, 2, 5.0);
    std::vector<float> mono (128, 0.25f);
    const float* in1[1] = { mono.data() };
    HostInfo hh;
    e6.process (in1, 1, 128, hh);
    auto s6 = e6.storage();
    s6->copyRange (0, 100, dst, 2);
    CHECK (a[5] == 0.25f && b[5] == 0.25f);

    std::printf (failures == 0 ? "ALLE TESTS OK\n" : "%d FEHLER\n", failures);
    return failures == 0 ? 0 : 1;
}

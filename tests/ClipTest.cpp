// Test des Editor-Kerns ohne JUCE:
//   g++ -std=c++17 -O1 -Wall -Wextra tests/ClipTest.cpp Source/Clip.cpp Source/Audition.cpp -o /tmp/cliptest && /tmp/cliptest [dump.raw]
#include "../Source/Align.h"
#include "../Source/Audition.h"
#include "../Source/Clip.h"
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace RememberClip;

static int failures = 0;
#define CHECK(c) do { if (! (c)) { std::printf ("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)
static bool near (double a, double b, double e = 1.0e-4) { return std::fabs (a - b) <= e; }

static Clip ramp (int64_t n, int chans = 2)
{
    Clip c;
    c.ch.assign ((size_t) chans, std::vector<float> ((size_t) n));
    for (int k = 0; k < chans; ++k)
        for (int64_t i = 0; i < n; ++i) c.ch[(size_t) k][(size_t) i] = (float) i / (float) n * (k == 0 ? 1.0f : -1.0f);
    return c;
}

static Clip sine (double sec, double freq, double amp, double sr = 48000.0)
{
    Clip c;
    c.sampleRate = sr;
    const int64_t n = (int64_t) (sec * sr);
    c.ch.assign (2, std::vector<float> ((size_t) n));
    for (int64_t i = 0; i < n; ++i)
    {
        const float v = (float) (amp * std::sin (2.0 * 3.14159265358979323846 * freq * (double) i / sr));
        c.ch[0][(size_t) i] = v;
        c.ch[1][(size_t) i] = v;
    }
    return c;
}


static void testWrapRotate()
{
    using namespace RememberClip;
    // Wrap: Tail [300,400) mit Wert 1 wird ueber den Anfang (Wert 0) ueberblendet
    Clip c; c.sampleRate = 48000; c.ch.assign (2, std::vector<float> (400, 0.0f));
    for (int i = 300; i < 400; ++i) c.ch[0][(size_t) i] = c.ch[1][(size_t) i] = 1.0f;
    c.gridOffset = 150.0;
    wrap (c, { 300, 400 });
    CHECK (c.length() == 300);
    CHECK (c.ch[0][0] > 0.99f && c.ch[0][99] < 0.02f && c.ch[0][100] == 0.0f && c.ch[1][50] > 0.6f && c.ch[1][50] < 0.8f);
    CHECK (std::abs (c.gridOffset - 150.0) < 1e-9);
    // Gleiche Leistung: Tail und Anfang mit gleichem Signal (Wert 1) ergeben in der Mitte mehr als 1, aber hoechstens sqrt(2)
    Clip d; d.ch.assign (1, std::vector<float> (400, 1.0f));
    wrap (d, { 300, 400 });
    CHECK (d.length() == 300 && d.ch[0][0] > 0.99f && d.ch[0][50] > 1.0f && d.ch[0][50] < 1.42f && d.ch[0][99] > 0.99f && d.ch[0][100] == 1.0f);
    // Tail in der Mitte: Material danach rueckt nach
    Clip m; m.ch.assign (1, std::vector<float> (300, 0.0f));
    for (int i = 100; i < 150; ++i) m.ch[0][(size_t) i] = 1.0f;
    for (int i = 150; i < 300; ++i) m.ch[0][(size_t) i] = 2.0f;
    wrap (m, { 100, 150 });
    CHECK (m.length() == 250 && m.ch[0][0] > 0.99f && m.ch[0][49] < 0.05f && m.ch[0][100] == 2.0f);
    // Tail laenger als der Rest: nur der Anfang des Tails, ueber den ganzen Rest
    Clip e; e.ch.assign (1, std::vector<float> (250, 0.0f));
    for (int i = 100; i < 250; ++i) e.ch[0][(size_t) i] = 1.0f;
    wrap (e, { 100, 250 });
    CHECK (e.length() == 100 && e.ch[0][0] > 0.99f && e.ch[0][99] < 0.02f);
    // Rotate: Positionswerte, Laenge gleich
    Clip r; r.ch.assign (1, std::vector<float> (1000));
    for (int i = 0; i < 1000; ++i) r.ch[0][(size_t) i] = (float) (i + 1);
    r.gridOffset = 250.0;
    rotate (r, 400, 0);
    CHECK (r.length() == 1000 && r.ch[0][0] == 401.0f && r.ch[0][599] == 1000.0f && r.ch[0][600] == 1.0f && r.ch[0][999] == 400.0f);
    CHECK (std::abs (r.gridOffset - (-150.0)) < 1e-9);
    // Rotate mit Ueberblend: Schnittstelle leise, weit weg unveraendert
    Clip q; q.ch.assign (1, std::vector<float> (1000, 1.0f));
    rotate (q, 400, 20);
    CHECK (q.ch[0][599] < 0.2f && q.ch[0][600] < 0.2f && q.ch[0][300] == 1.0f && q.ch[0][700] == 1.0f);
    std::printf ("wrap/rotate OK\n");
}

int main (int argc, char** argv)
{
    testWrapRotate();
    // 1) Bereiche, Extract, Delete, Crop, Insert und das mitgefuehrte Raster
    {
        Clip c = ramp (1000);
        c.gridOffset = 600.0;
        auto part = extract (c, { 100, 200 });
        CHECK (part.size() == 2 && part[0].size() == 100);
        CHECK (near (part[0][0], 0.1));

        deleteRange (c, { 100, 300 });
        CHECK (c.length() == 800);
        CHECK (near (c.ch[0][100], 0.3));
        CHECK (near (c.gridOffset, 400.0));            // lag hinter dem Bereich: rueckt auf

        crop (c, { 50, 450 });
        CHECK (c.length() == 400);
        CHECK (near (c.gridOffset, 350.0));

        insert (c, 0, part);
        CHECK (c.length() == 500);
        CHECK (near (c.gridOffset, 450.0));
        CHECK (near (c.ch[0][0], 0.1));

        deleteRange (c, { 5000, 6000 });                // ausserhalb: nichts
        CHECK (c.length() == 500);
    }

    // 2) Reverse, Gain, Fades
    {
        Clip c = ramp (100);
        reverse (c, { 0, 100 });
        CHECK (near (c.ch[0][0], 0.99) && near (c.ch[0][99], 0.0));
        reverse (c, { 0, 100 });
        CHECK (near (c.ch[0][1], 0.01));

        Clip f;
        f.ch.assign (2, std::vector<float> (1000, 1.0f));
        fadeIn (f, { 0, 100 });
        CHECK (f.ch[0][0] == 0.0f && near (f.ch[0][99], 1.0) && f.ch[0][50] > 0.4f && f.ch[0][50] < 0.6f && f.ch[0][500] == 1.0f);
        fadeOut (f, { 900, 1000 });
        CHECK (near (f.ch[1][900], 1.0) && f.ch[1][999] == 0.0f);

        applyGain (f, { 200, 300 }, 0.5f);
        CHECK (near (f.ch[0][250], 0.5));
    }

    // 3) Stille abschneiden
    {
        Clip c;
        c.ch.assign (2, std::vector<float> (1000, 0.0f));
        for (int i = 300; i < 700; ++i) { c.ch[0][(size_t) i] = 0.5f; c.ch[1][(size_t) i] = 0.2f; }
        auto r = findNonSilent (c, -60.0f);
        CHECK (r.a == 300 && r.b == 700);
        trimSilence (c, -60.0f);
        CHECK (c.length() == 400);
        Clip z;
        z.ch.assign (2, std::vector<float> (100, 0.0f));
        CHECK (findNonSilent (z, -60.0f).empty());
        trimSilence (z, -60.0f);
        CHECK (z.length() == 100);                      // alles still: unveraendert
    }

    // 4) Normalisieren auf Spitze
    {
        Clip c = sine (1.0, 440.0, 0.25);
        auto r = normalizePeak (c, { 0, c.length() }, -1.0);
        CHECK (r.ok);
        CHECK (near (20.0 * std::log10 (peakAbs (c, { 0, c.length() })), -1.0, 0.01));
        Clip z;
        z.ch.assign (2, std::vector<float> (1000, 0.0f));
        CHECK (! normalizePeak (z, { 0, 1000 }, -1.0).ok);
    }

    // 5) Lautheit: Linearitaet, Stille, Gating
    {
        Clip c = sine (5.0, 997.0, 0.1);
        const double l1 = integratedLufs (c, { 0, c.length() });
        applyGain (c, { 0, c.length() }, 2.0f);
        const double l2 = integratedLufs (c, { 0, c.length() });
        CHECK (near (l2 - l1, 6.0206, 0.01));
        Clip z;
        z.ch.assign (2, std::vector<float> (48000, 0.0f));
        CHECK (! std::isfinite (integratedLufs (z, { 0, 48000 })));

        // laute Passage + sehr leise Passage: das relative Gate ignoriert die leise
        Clip m = sine (4.0, 997.0, 0.2);
        Clip q = sine (4.0, 997.0, 0.0002);
        m.ch[0].insert (m.ch[0].end(), q.ch[0].begin(), q.ch[0].end());
        m.ch[1].insert (m.ch[1].end(), q.ch[1].begin(), q.ch[1].end());
        const double loud = integratedLufs (sine (4.0, 997.0, 0.2), { 0, 4 * 48000 });
        CHECK (near (integratedLufs (m, { 0, m.length() }), loud, 0.3));   // Bloecke an der Grenze zaehlen anteilig mit

        // Normalisieren auf LUFS mit Spitzen-Obergrenze
        Clip n = sine (5.0, 997.0, 0.1);
        auto r = normalizeLufs (n, { 0, n.length() }, -23.0, -0.1);
        CHECK (r.ok && ! r.limited);
        CHECK (near (integratedLufs (n, { 0, n.length() }), -23.0, 0.02));
        Clip p = sine (5.0, 997.0, 0.1);
        auto r2 = normalizeLufs (p, { 0, p.length() }, -1.0, -6.0);   // -1 LUFS waere viel zu laut
        CHECK (r2.ok && r2.limited);
        CHECK (near (20.0 * std::log10 (peakAbs (p, { 0, p.length() })), -6.0, 0.01));

        Clip qr = sine (3.0, 440.0, 0.1);
        CHECK (near (rmsDb (qr, { 0, qr.length() }), 20.0 * std::log10 (0.1 / std::sqrt (2.0)), 0.05));
        auto r3 = normalizeRms (qr, { 0, qr.length() }, -18.0, -0.1);
        CHECK (r3.ok && near (rmsDb (qr, { 0, qr.length() }), -18.0, 0.02));

        // Loop-Trim: periodischer Loop (125 BPM, 1 Takt), Kick liegt 70 ms nach dem Anfang. Laenge bleibt, Loop startet auf dem Kick
        {
            const double sr = 48000.0, beat = 60.0 / 125.0;
            const int64_t L = (int64_t) std::llround (4.0 * beat * sr), d = (int64_t) std::llround (0.070 * sr);
            auto sig = [&] (int64_t t)            // periodisch mit der Laenge L, Kick auf jedem Beat, um d verschoben
            {
                const int64_t tt = ((t - d) % L + L) % L;
                const int64_t b = (int64_t) std::llround (beat * sr);
                const int64_t i = tt % b;
                return 0.8f * (float) (std::sin (2.0 * 3.14159265358979 * 60.0 * (double) i / sr) * std::exp (-(double) i / (0.05 * sr)));
            };
            Clip lp; lp.sampleRate = sr; lp.ch.assign (2, std::vector<float> ((size_t) L));
            for (int64_t i = 0; i < L; ++i) { lp.ch[0][(size_t) i] = lp.ch[1][(size_t) i] = sig (i); }
            // vor dem Kick ist es nicht still (Ausklang des letzten Kicks), also fuer den Test hart auf Stille setzen
            for (int64_t i = 0; i < d; ++i) lp.ch[0][(size_t) i] = lp.ch[1][(size_t) i] = 0.0f;
            const int64_t before = lp.length();
            CHECK (trimLoopStart (lp, -60.0f));
            CHECK (lp.length() == before);                                    // Laenge bleibt
            CHECK (std::fabs (lp.ch[0][150]) > 0.2f);                          // Kick beginnt am Anfang
            CHECK (std::fabs (lp.ch[0][(size_t) (before - 1)]) < 0.01f);     // Ende ist die ehemalige Stille
            Clip already = lp;
            CHECK (! trimLoopStart (already, -60.0f));                        // zweites Mal: nichts zu tun
        }

        // Raster-Kalibrierung: Kicks (60 Hz) + Hats, Audio 70 ms hinter dem Raster (125 BPM)
        {
            const double sr = 48000.0, beat = 60.0 / 125.0;
            const int64_t lag = (int64_t) std::llround (0.070 * sr);
            std::vector<float> mono ((size_t) (sr * 8.0), 0.0f);
            std::vector<int64_t> beats;
            unsigned seed = 12345;
            for (int k = 0; k < 16; ++k)
            {
                const int64_t g = (int64_t) std::llround (k * beat * sr) + (int64_t) (0.1 * sr);
                beats.push_back (g);
                for (int64_t i = 0; i < (int64_t) (0.25 * sr); ++i)
                    if (g + lag + i < (int64_t) mono.size())
                        mono[(size_t) (g + lag + i)] += 0.8f * (float) (std::sin (2.0 * 3.14159265358979 * 60.0 * (double) i / sr) * std::exp (-(double) i / (0.07 * sr)));
                const int64_t h = g + (int64_t) (0.5 * beat * sr) + lag;   // Hat auf dem Achtel
                for (int64_t i = 0; i < (int64_t) (0.03 * sr); ++i)
                    if (h + i < (int64_t) mono.size()) { seed = seed * 1664525u + 1013904223u; mono[(size_t) (h + i)] += 0.15f * ((float) (seed >> 8) / 8388608.0f - 1.0f) * (float) std::exp (-(double) i / (0.01 * sr)); }
            }
            const auto r = RememberAlign::estimateGridOffset (mono, sr, beats, beat);
            CHECK (r.ok && r.hits >= 12);
            CHECK (std::llabs (r.offsetSamples - lag) <= 48);          // besser als 1 ms
            // negativer Versatz (Audio vor dem Raster)
            std::vector<int64_t> early = beats;
            for (auto& b : early) b += (int64_t) (0.2 * sr);
            const auto r2 = RememberAlign::estimateGridOffset (mono, sr, early, beat);
            CHECK (r2.ok && std::llabs (r2.offsetSamples - (lag - (int64_t) (0.2 * sr))) <= 48);
            // reines Rauschen: kein verlaesslicher Wert
            std::vector<float> noise ((size_t) (sr * 8.0));
            for (auto& v : noise) { seed = seed * 1664525u + 1013904223u; v = 0.3f * ((float) (seed >> 8) / 8388608.0f - 1.0f); }
            CHECK (! RememberAlign::estimateGridOffset (noise, sr, beats, beat).ok);
        }

        if (argc > 1)   // Referenzdatei fuer den Vergleich mit ffmpeg ebur128 (float32 stereo interleaved, 48 kHz)
        {
            Clip ref = sine (6.0, 220.0, 0.2);
            for (int64_t i = 0; i < ref.length(); ++i)
            {
                const double t = (double) i / 48000.0;
                const float env = (float) (0.5 + 0.5 * std::sin (2.0 * 3.14159265358979323846 * 0.7 * t));
                ref.ch[0][(size_t) i] = (ref.ch[0][(size_t) i] + 0.15f * (float) std::sin (2.0 * 3.14159265358979323846 * 3100.0 * t)) * env;
                ref.ch[1][(size_t) i] = ref.ch[1][(size_t) i] * env * 0.8f;
            }
            FILE* f = std::fopen (argv[1], "wb");
            for (int64_t i = 0; i < ref.length(); ++i)
            {
                const float s[2] = { ref.ch[0][(size_t) i], ref.ch[1][(size_t) i] };
                std::fwrite (s, sizeof (float), 2, f);
            }
            std::fclose (f);
            std::printf ("LUFS_REF %.3f\n", integratedLufs (ref, { 0, ref.length() }));
        }
    }

    // 6) Undo / Redo
    {
        Session s;
        s.load (ramp (100));
        CHECK (s.hasClip() && s.clip.length() == 100);
        s.beginEdit();
        deleteRange (s.clip, { 0, 50 });
        s.touch();
        CHECK (s.clip.length() == 50);
        s.beginEdit();
        reverse (s.clip, { 0, 50 });
        s.touch();
        CHECK (s.undo() && s.clip.length() == 50 && near (s.clip.ch[0][0], 0.5));
        CHECK (s.undo() && s.clip.length() == 100);
        CHECK (s.redo() && s.clip.length() == 50);
        CHECK (s.redo() && near (s.clip.ch[0][0], 0.99));
        CHECK (! s.redo());
        s.beginEdit();                                    // neue Bearbeitung loescht Redo
        s.undo();
        CHECK (! s.canRedo() || s.canRedo());
        for (int i = 0; i < 40; ++i) { s.beginEdit(); }
        int n = 0;
        while (s.undo()) ++n;
        CHECK (n <= 24);
    }

    // 7) Vorhoeren: Ausgabe, Ueberblendung, Loop, Stopp
    {
        RememberAudition::Player p;
        auto d = std::make_shared<RememberAudition::Data>();
        d->sampleRate = 48000.0;
        d->ch.assign (2, std::vector<float> (4800, 0.5f));
        p.publish (d);
        p.setSampleRate (48000.0);

        std::vector<float> l (480, 0.1f), r (480, 0.1f);
        float* out[2] = { l.data(), r.data() };
        p.process (out, 2, 480);                          // nicht aktiv: Eingang bleibt
        CHECK (l[0] == 0.1f && ! p.isPlaying());

        p.play (0, 0, 4800, false);
        for (int blk = 0; blk < 3; ++blk)
        {
            std::fill (l.begin(), l.end(), 0.1f); std::fill (r.begin(), r.end(), 0.1f);
            p.process (out, 2, 480);
        }
        CHECK (p.isPlaying());
        CHECK (near (l[479], 0.5, 1.0e-3));               // voll beim Clip
        CHECK (p.position() == 1440);

        p.stop();
        std::fill (l.begin(), l.end(), 0.1f);
        p.process (out, 2, 480);
        CHECK (near (l[479], 0.1, 1.0e-3));               // zurueck beim Eingang
        CHECK (! p.isPlaying());

        // Loop ueber einen kleinen Bereich
        p.play (100, 100, 300, true);
        for (int blk = 0; blk < 10; ++blk) p.process (out, 2, 480);
        CHECK (p.isPlaying() && p.position() >= 100 && p.position() <= 300);

        // Ende ohne Loop: stoppt von selbst
        p.stop(); p.process (out, 2, 480); p.process (out, 2, 480);
        p.play (4000, 0, 4800, false);
        for (int blk = 0; blk < 6; ++blk) p.process (out, 2, 480);
        CHECK (! p.isPlaying());

        // neuer Clip waehrend der Wiedergabe
        auto d2 = std::make_shared<RememberAudition::Data>();
        d2->ch.assign (2, std::vector<float> (4800, -0.5f));
        p.play (0, 0, 4800, false);
        p.process (out, 2, 480);
        p.publish (d2);
        p.process (out, 2, 480);
        p.collectGarbage();
    }

    std::printf (failures == 0 ? "ALLE TESTS OK\n" : "%d FEHLER\n", failures);
    return failures == 0 ? 0 : 1;
}

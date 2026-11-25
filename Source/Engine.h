#pragma once
// Remember: Engine. Ringpuffer (Audio), Spitzenwerte (Pyramide) und Host-Transport-Meta.
// Reiner C++17-Code ohne JUCE, damit er einzeln testbar ist (tests/EngineTest.cpp).
//
// Audio-Thread: schreibt nur in vorab angelegten Speicher, ohne Locks und ohne Allokation.
// Oberflaeche: liest ueber shared_ptr auf den Speicher. Wechsel der History-Laenge laeuft ueber einen Hazard-Pointer.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace RememberDSP
{
    constexpr int peakBlock = 256;      // Samples pro Spitzenwert-Block
    constexpr int metaInterval = 512;   // Abstand der Transport-Eintraege (Samples)

    struct HostInfo
    {
        bool playing = false;
        bool hasPpq = false;
        double ppq = 0.0;               // Position in Viertelnoten (Blockanfang)
        double bpm = 120.0;
        float beatsPerBar = 4.0f;       // Zaehler * 4 / Nenner
        double barStartPpq = 0.0;       // ppq des letzten Taktanfangs
    };

    enum : uint8_t { fPlaying = 1, fJump = 2, fPlayStart = 4 };

    struct Meta
    {
        int64_t sample = 0;             // absoluter Sample-Index (seit Start des Plugins)
        double ppq = 0.0;
        double bpm = 120.0;
        double barStartPpq = 0.0;
        float beatsPerBar = 4.0f;
        uint8_t flags = 0;
    };

    struct GridPoint
    {
        int64_t sample = 0;
        double ppq = 0.0;
        bool bar = false;
        bool beat = false;
    };

    struct Storage
    {
        Storage (int channels, int64_t capacityFrames);

        const int numCh;
        const int64_t cap;              // Frames, Vielfaches von peakBlock
        const int64_t metaCap;
        std::vector<float> data;        // numCh * cap
        std::vector<float> pmin, pmax;  // cap / peakBlock
        std::vector<Meta> meta;
        std::atomic<int64_t> written { 0 };
        std::atomic<int64_t> metaWritten { 0 };
        std::atomic<int64_t> gridShift { 0 };   // Samples: Raster (Meta-Eintraege) wird um diesen Wert nach hinten verschoben (Latenz vor dem Plugin)

        // nur Audio-Thread
        float curMin = 0.0f, curMax = 0.0f;

        // Lesen (Oberflaeche)
        int64_t writtenNow() const { return written.load (std::memory_order_acquire); }
        int64_t oldestValid() const;    // aeltestes sicher lesbares Sample
        // Min/Max je Bin ueber [start, start+len). Ungueltige Bins: mn = 1, mx = -1
        void getPeaks (int64_t start, int64_t len, int bins, float* mn, float* mx) const;
        // Kopiert [start, start+len) nach dst[ch][0..len). false, wenn ausserhalb des gueltigen Bereichs
        bool copyRange (int64_t start, int64_t len, float* const* dst, int dstChannels) const;
        // Meta-Eintraege, die [s0, s1] betreffen (inkl. dem Eintrag davor)
        // raw = true: ohne gridShift (nur intern)
        void readMeta (int64_t s0, int64_t s1, std::vector<Meta>& out, bool raw = false) const;
        void pushMeta (const Meta& m);
    };

    // Raster aus den Meta-Eintraegen. gridBeats in Viertelnoten (1 = Beat, 0.25 = 1/16, beatsPerBar = Takt)
    void gridFromMeta (const std::vector<Meta>& m, int64_t endSample, double sampleRate,
                       int64_t s0, int64_t s1, double gridBeats, std::vector<GridPoint>& out);
    void playStartsFromMeta (const std::vector<Meta>& m, int64_t s0, int64_t s1, std::vector<int64_t>& out);
    // Umrechnung Sample <-> ppq ueber die Eintraege. false, wenn nicht abbildbar
    bool sampleToPpq (const std::vector<Meta>& m, int64_t endSample, double sampleRate, int64_t s, double& ppq);
    bool ppqToSample (const std::vector<Meta>& m, int64_t endSample, double sampleRate, double ppq, int64_t& s);
    // Tempo und Taktlaenge in Vierteln am Sample (letzter Eintrag davor)
    bool tempoAt (const std::vector<Meta>& m, int64_t s, double& bpm, float& beatsPerBar);

    class Engine
    {
    public:
        // Nicht Echtzeit. Behaelt vorhandene Aufnahme, wenn Samplerate und Kanalzahl gleich bleiben
        void prepare (double sampleRate, int channels, double historySeconds);
        // Nicht Echtzeit (Oberflaeche): neue Laenge, der Anfang der alten Aufnahme wird uebernommen
        void setHistorySeconds (double seconds);
        // Echtzeit
        void process (const float* const* in, int numCh, int n, const HostInfo& h);

        std::shared_ptr<Storage> storage() const;
        void collectGarbage();          // Oberflaeche, regelmaessig
        double getSampleRate() const { return sr.load(); }
        double getHistorySeconds() const { return historySec.load(); }
        // Raster-Versatz in Samples (Latenz von Plugins vor Remember). Oberflaeche
        void setGridShift (int64_t samples);
        int64_t getGridShift() const { return gridShiftSamples.load(); }

        std::atomic<bool> transportPlaying { false };
        std::atomic<bool> hostHasPpq { false };
        std::atomic<double> lastBpm { 120.0 };

    private:
        void publish (std::shared_ptr<Storage> s);

        std::atomic<double> sr { 48000.0 };
        std::atomic<double> historySec { 300.0 };
        std::atomic<int64_t> gridShiftSamples { 0 };
        int channels = 2;

        mutable std::mutex m;           // nur Oberflaeche und prepare, nie der Audio-Thread
        std::shared_ptr<Storage> current;
        std::vector<std::shared_ptr<Storage>> retired;
        std::atomic<Storage*> audioPtr { nullptr };
        std::atomic<Storage*> inUse { nullptr };

        // nur Audio-Thread
        Meta lastMeta;
        bool haveLast = false;
        int64_t lastMetaSample = 0;
    };
}

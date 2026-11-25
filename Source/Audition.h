#pragma once
// Remember: Vorhoeren des Editor-Clips im Plugin-Ausgang. Std-only, einzeln testbar.
// Beim Vorhoeren wird der Eingang weich (ca. 10 ms) gegen den Clip getauscht. Aufgenommen wird weiter der Eingang.
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include "Clip.h"

namespace RememberAudition
{
    struct Data
    {
        RememberClip::Channels ch;
        double sampleRate = 48000.0;
        int64_t length() const { return ch.empty() ? 0 : (int64_t) ch[0].size(); }
    };

    class Player
    {
    public:
        // Oberflaeche (message thread)
        void publish (std::shared_ptr<const Data> d);
        void play (int64_t startPos, int64_t rangeA, int64_t rangeB, bool loop);   // Bereich [A,B), Start darin
        void stop();
        void setLoop (bool l) { loopOn.store (l); }
        void setSampleRate (double sr) { rate.store (sr); }
        bool isPlaying() const { return playingOut.load(); }
        int64_t position() const { return pos.load(); }
        void collectGarbage();

        // Audio-Thread
        void process (float* const* out, int numCh, int n);

    private:
        mutable std::mutex m;
        std::shared_ptr<const Data> current;
        std::vector<std::shared_ptr<const Data>> retired;
        std::atomic<const Data*> audioPtr { nullptr };
        std::atomic<const Data*> inUse { nullptr };

        std::atomic<bool> wantPlay { false }, loopOn { false }, playingOut { false };
        std::atomic<int64_t> startPos { 0 }, rangeA { 0 }, rangeB { 0 }, pos { 0 };
        std::atomic<uint32_t> cmd { 0 };
        std::atomic<double> rate { 48000.0 };

        // nur Audio-Thread
        uint32_t lastCmd = 0;
        int64_t cur = 0;
        float g = 0.0f;
    };
}

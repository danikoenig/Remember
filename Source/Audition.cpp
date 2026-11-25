#include "Audition.h"
#include <algorithm>

namespace RememberAudition
{
    void Player::publish (std::shared_ptr<const Data> d)
    {
        std::lock_guard<std::mutex> l (m);
        if (current) retired.push_back (current);
        current = std::move (d);
        audioPtr.store (current.get(), std::memory_order_seq_cst);
    }

    void Player::collectGarbage()
    {
        std::lock_guard<std::mutex> l (m);
        const Data* busy = inUse.load (std::memory_order_seq_cst);
        retired.erase (std::remove_if (retired.begin(), retired.end(),
                                       [busy] (const std::shared_ptr<const Data>& p) { return p.get() != busy; }),
                       retired.end());
    }

    void Player::play (int64_t s, int64_t a, int64_t b, bool loop)
    {
        startPos.store (s);
        rangeA.store (a);
        rangeB.store (b);
        loopOn.store (loop);
        wantPlay.store (true);
        cmd.fetch_add (1);
    }

    void Player::stop() { wantPlay.store (false); }

    void Player::process (float* const* out, int numCh, int n)
    {
        const Data* d;
        do
        {
            d = audioPtr.load (std::memory_order_seq_cst);
            inUse.store (d, std::memory_order_seq_cst);
        } while (d != audioPtr.load (std::memory_order_seq_cst));

        const uint32_t c = cmd.load();
        if (c != lastCmd) { lastCmd = c; cur = startPos.load(); }

        const bool want = wantPlay.load() && d != nullptr && d->length() > 0;
        if (! want && g <= 0.0f) { playingOut.store (false); return; }

        const int64_t len = d != nullptr ? d->length() : 0;
        const int64_t a = std::max<int64_t> (0, std::min (rangeA.load(), len));
        const int64_t b = std::max (a, std::min (rangeB.load(), len));
        const bool loop = loopOn.load();
        const float step = (float) (1.0 / std::max (16.0, 0.010 * rate.load()));
        const int dch = d != nullptr ? (int) d->ch.size() : 0;
        bool stillWant = want;

        for (int i = 0; i < n; ++i)
        {
            g = stillWant ? std::min (1.0f, g + step) : std::max (0.0f, g - step);
            float s0 = 0.0f, s1 = 0.0f;
            if (d != nullptr && dch > 0 && b > a)
            {
                if (cur >= b)
                {
                    if (loop) cur = a;
                    else stillWant = false;
                }
                if (stillWant && cur >= 0 && cur < len)
                {
                    s0 = d->ch[0][(size_t) cur];
                    s1 = d->ch[(size_t) std::min (1, dch - 1)][(size_t) cur];
                    ++cur;
                }
            }
            else stillWant = false;
            if (g > 0.0f)
                for (int k = 0; k < numCh; ++k)
                {
                    const float clipS = numCh == 1 ? 0.5f * (s0 + s1) : (k == 0 ? s0 : s1);
                    out[k][i] = out[k][i] * (1.0f - g) + clipS * g;
                }
        }
        if (! stillWant) wantPlay.store (false);
        pos.store (cur);
        playingOut.store (g > 0.0f);
    }
}

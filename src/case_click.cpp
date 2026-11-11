#include "case_click.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>

namespace headsup
{
    namespace
    {
        constexpr size_t kChannels = 2;
        constexpr float kFullScale = 32768.0f;
        constexpr float kLowest = -32768.0f, kHighest = 32767.0f;

        // A wheel's tick, made here: a snap of noise and two tones, each dying away at its own rate.
        constexpr float kTickSeconds = 0.035f;
        constexpr float kTickLevel   = 0.8f;
        constexpr float kSnapWeight  = 0.55f;
        constexpr float kSnapDecay   = 0.0012f; // seconds
        struct Ring
        {
            float hertz, decay, weight;
        };
        constexpr Ring kRings[] = {{2600.0f, 0.005f, 0.5f}, {1250.0f, 0.011f, 0.35f}};
        constexpr uint32_t kNoiseTimes = 1103515245u, kNoisePlus = 12345u; // a plain linear congruential generator
        constexpr uint32_t kNoiseBits  = 1u << 23;

        std::vector<float> Tick()
        {
            const auto frames = static_cast<size_t>(std::lround(kTickSeconds * kClickRate));
            std::vector<float> tick(frames);
            uint32_t noise = 1;
            for (size_t i = 0; i < frames; ++i)
            {
                noise         = noise * kNoiseTimes + kNoisePlus;
                const float t = static_cast<float>(i) / kClickRate;
                float v = kSnapWeight * (static_cast<float>(noise >> 8) / static_cast<float>(kNoiseBits) - 1.0f) * std::exp(-t / kSnapDecay);
                for (const Ring& ring : kRings)
                    v += ring.weight * std::sin(2.0f * std::numbers::pi_v<float> * ring.hertz * t) * std::exp(-t / ring.decay);
                tick[i] = kTickLevel * v;
            }
            return tick;
        }
    }

    std::vector<int16_t> ClickTrack(const std::vector<float>& clicks, float seconds)
    {
        static const std::vector<float> tick = Tick();
        const auto loudest = static_cast<size_t>(std::ranges::max_element(tick, {}, [](float v) { return std::abs(v); }) - tick.begin());
        const auto frames  = static_cast<size_t>(std::lround(seconds * kClickRate));
        std::vector<float> mix(frames, 0.0f);
        for (const float click : clicks)
        {
            const auto moment = static_cast<size_t>(std::lround(std::max(0.0f, click) * kClickRate));
            const size_t start = moment > loudest ? moment - loudest : 0;
            for (size_t k = 0; k < tick.size() && start + k < frames; ++k) mix[start + k] += tick[k];
        }
        std::vector<int16_t> track(frames * kChannels);
        for (size_t i = 0; i < frames; ++i)
            track[i * kChannels] = track[i * kChannels + 1] = static_cast<int16_t>(std::clamp(std::round(mix[i] * kFullScale), kLowest, kHighest));
        return track;
    }
}

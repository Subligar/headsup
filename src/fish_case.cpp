#include "fish_case.h"

#include "settings.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>

namespace headsup
{
    namespace
    {
        constexpr float kLayoutHeight = 720.0f;
        constexpr float kFullScale    = 100.0f; // percent

        // The tile the strip starts on and how far from the winner's middle it may stop, in tiles.
        constexpr int kStartTile    = 3;
        constexpr float kStopSpread = 0.38f;

        // The run as it was designed, in its own seconds; a Case of another length is read at the same shares.
        constexpr float kTimelineSeconds = 10.6f;
        constexpr float kVeilTo          = 0.5f;
        constexpr float kStripIn         = 0.15f;
        constexpr float kStripInFor      = 0.4f;
        constexpr float kSpinFrom        = 0.5f;
        constexpr float kSpinTo          = 6.5f;
        constexpr float kSettleFor       = 0.35f;
        constexpr float kStripOut        = 7.3f;
        constexpr float kStripOutFor     = 0.3f;
        constexpr float kRevealFrom      = 7.3f;
        constexpr float kIconInFor       = 0.45f;
        constexpr float kNameFrom        = 7.5f;
        constexpr float kNameInFor       = 0.35f;
        constexpr float kFadeFrom        = 10.0f;

        constexpr float kStripLift = 14.0f; // layout pixels the strip rises as it comes in
        constexpr float kBigFrom   = 0.6f;  // of the large icon's size, as it comes in

        // A catch's color by the skill it asks, from these, and the legendary fish's last.
        constexpr int kSkillFrom[]         = {0, 15, 30, 50, 70, 90};
        constexpr uint32_t kGradeColor[]   = {0xB0C3D9, 0x5E98D9, 0x4B69FF, 0x8847FF, 0xD32CE6, 0xEB4B4B};
        constexpr uint32_t kLegendaryColor = 0xE4AE39;
        constexpr const char* kSkillWords  = "Fishing skill ";

        float Clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }
        float Ramp(float from, float to, float t) { return Clamp01((t - from) / (to - from)); }
        float Smooth(float from, float to, float t)
        {
            const float u = Ramp(from, to, t);
            return u * u * (3.0f - 2.0f * u);
        }
        float EaseOut(float u) { return 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u); }
        // Past 1 and back, for the large icon's arrival.
        float EaseOutBack(float u) { return 1.0f + 2.2f * std::pow(u - 1.0f, 3.0f) + 1.2f * std::pow(u - 1.0f, 2.0f); }

        // A small generator of its own, so a run depends only on its seed.
        class Draws
        {
        public:
            explicit Draws(uint32_t seed) : m_State(seed) {}
            float Next()
            {
                m_State += 0x6D2B79F5u;
                uint32_t t = m_State;
                t          = (t ^ (t >> 15)) * (t | 1u);
                t ^= t + (t ^ (t >> 7)) * (t | 61u);
                return static_cast<float>((t ^ (t >> 14)) >> 8) / static_cast<float>(1u << 24);
            }

        private:
            uint32_t m_State;
        };

        uint16_t Pick(const std::vector<PoolEntry>& pool, Draws& draws)
        {
            const int total = std::accumulate(pool.begin(), pool.end(), 0, [](int sum, const PoolEntry& e) { return sum + e.weight; });
            float left      = draws.Next() * static_cast<float>(total);
            for (const PoolEntry& entry : pool)
            {
                left -= static_cast<float>(entry.weight);
                if (left < 0.0f) return entry.item;
            }
            return pool.back().item;
        }

        float Along(const CaseRun& run, float t) { return kStartTile + (run.stop - kStartTile) * EaseOut(Ramp(kSpinFrom, kSpinTo, t)); }
    }

    CaseRun SpinCase(uint16_t caught, const std::vector<PoolEntry>& pool, uint32_t seed)
    {
        Draws draws(seed);
        CaseRun run{std::vector<uint16_t>(kCaseTiles, caught), 0.0f};
        if (!pool.empty())
            for (uint16_t& tile : run.tiles) tile = Pick(pool, draws);
        run.tiles[kCaseWinner] = caught;
        run.stop               = static_cast<float>(kCaseWinner) + (draws.Next() * 2.0f - 1.0f) * kStopSpread;
        return run;
    }

    CaseView CaseAt(const CaseRun& run, float seconds, float length)
    {
        const float t = seconds * kTimelineSeconds / length;
        if (t < 0.0f || t >= kTimelineSeconds) return {};
        const float fade   = 1.0f - Smooth(kFadeFrom, kTimelineSeconds, t);
        const float in     = Smooth(kStripIn, kStripIn + kStripInFor, t);
        const float coming = Ramp(kRevealFrom, kRevealFrom + kIconInFor, t);
        return CaseView{Smooth(0.0f, kVeilTo, t) * fade, in * (1.0f - Smooth(kStripOut, kStripOut + kStripOutFor, t)) * fade,
            (1.0f - in) * kStripLift, Along(run, t), Smooth(kSpinTo, kSpinTo + kSettleFor, t), coming * fade,
            kBigFrom + (1.0f - kBigFrom) * EaseOutBack(coming), Smooth(kNameFrom, kNameFrom + kNameInFor, t) * fade, false};
    }

    std::vector<float> CaseClicks(const CaseRun& run, float length)
    {
        std::vector<float> clicks;
        const float distance = run.stop - kStartTile;
        for (float edge = kStartTile + 0.5f; edge <= run.stop; edge += 1.0f)
        {
            const float u = 1.0f - std::cbrt(1.0f - (edge - kStartTile) / distance); // EaseOut's inverse
            clicks.push_back((kSpinFrom + u * (kSpinTo - kSpinFrom)) * length / kTimelineSeconds);
        }
        return clicks;
    }

    uint32_t CaseColor(uint16_t item)
    {
        const FishRecord* fish = FishById(item);
        if (fish == nullptr) return kGradeColor[0];
        if (fish->legendary) return kLegendaryColor;
        return kGradeColor[std::ranges::upper_bound(kSkillFrom, static_cast<int>(fish->skill)) - std::begin(kSkillFrom) - 1];
    }

    std::string CaseSkillLine(uint16_t item)
    {
        const FishRecord* fish = FishById(item);
        return fish == nullptr ? std::string() : kSkillWords + std::to_string(fish->skill);
    }

    uint16_t TestCatch(const Angler& angler, const std::vector<PoolEntry>& pool, uint32_t seed)
    {
        if (angler.bait == 0 || pool.empty()) return kMoatCarp;
        Draws draws(seed);
        return Pick(pool, draws);
    }

    CaseSpot CasePlace(int x, int y, int scalePercent, float screenWidth, float screenHeight)
    {
        const auto at = [](int placed, float side) {
            return placed == kFishBarUnplaced ? side / 2.0f : std::clamp(static_cast<float>(placed), 0.0f, side);
        };
        return CaseSpot{at(x, screenWidth), at(y, screenHeight), screenHeight / kLayoutHeight * static_cast<float>(scalePercent) / kFullScale};
    }
}

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

        // The layout's pixels, on its 1280 by 720 frame.
        constexpr float kMiddle       = 640.0f;
        constexpr float kFrameMiddleY = 360.0f;
        constexpr float kStripY       = 340.0f;
        constexpr float kTileWidth    = 132.0f;
        constexpr float kTileHeight   = 104.0f;
        constexpr float kPitch        = 136.0f;
        constexpr float kShown        = 520.0f; // from the line, where the tiles have faded out
        constexpr float kEdge         = 170.0f; // the fade's width
        constexpr float kBandMargin   = 14.0f;
        constexpr float kIcon         = 72.0f;
        constexpr float kIconTop      = 10.0f;
        constexpr float kBarHeight    = 5.0f;
        constexpr float kRiseHeight   = 44.0f;
        constexpr float kLitGlow      = 26.0f;
        constexpr float kLitEdge      = 3.0f;
        constexpr float kLineWidth    = 2.0f;
        constexpr float kLineReach    = 12.0f; // past the tiles
        constexpr float kLineGlow     = 10.0f;
        constexpr float kTitleY       = 246.0f;
        constexpr float kBigY         = 400.0f;
        constexpr float kBigIcon      = 224.0f;
        constexpr float kHalo         = 280.0f;
        constexpr float kNameY        = 162.0f;
        constexpr float kUnderlineY   = 193.5f;
        constexpr float kUnderline    = 3.0f;
        constexpr float kUnderlinePad = 48.0f;
        constexpr float kSkillY       = 217.0f;

        constexpr uint32_t kVeil = 0x0E0924, kShade = 0x05030F, kBand = 0x080A14, kLineGold = 0xF2C94C, kLineHalo = 0xF4C76B;
        constexpr uint32_t kTitleInk = 0xEBE7F6, kNameInk = 0xF4F1FB, kSkillInk = 0xB9B5D3, kWhiteInk = 0xFFFFFF;
        constexpr uint32_t kOpaque = 0xFFFFFFFF;
        constexpr float kVeilLevel     = 0.62f;
        constexpr float kShadeLevel    = 0.6f;
        constexpr float kBandLevel     = 0.6f;
        constexpr float kDimLevel      = 0.55f; // the tiles but the catch's, once it stops
        constexpr float kRiseLevel     = 0.6f;
        constexpr float kLitGlowLevel  = 0.7f;
        constexpr float kLineHaloLevel = 0.5f;
        constexpr float kLineSettled   = 0.6f; // of the line, faded once it stops
        constexpr float kHaloLevel     = 0.55f;
        constexpr float kFullByte      = 255.0f;

        uint32_t Argb(uint32_t rgb, float alpha) { return static_cast<uint32_t>(std::lround(Clamp01(alpha) * kFullByte)) << 24 | rgb; }

        // Writes quads in screen pixels from the layout's, around the Case's spot; a clear one is left out.
        struct Painter
        {
            std::vector<CaseQuad>& out;
            const CaseSpot& spot;
            void Centered(CasePicture picture, bool add, float x, float y, float halfWidth, float halfHeight, uint32_t argb, uint16_t item = 0)
            {
                if (argb >> 24 == 0 || halfWidth <= 0.0f || halfHeight <= 0.0f) return;
                out.push_back(CaseQuad{picture, add, spot.x + (x - halfWidth - kMiddle) * spot.unit,
                    spot.y + (y - halfHeight - kFrameMiddleY) * spot.unit, 2.0f * halfWidth * spot.unit, 2.0f * halfHeight * spot.unit, argb, item});
            }
            void Cover(CasePicture picture, float width, float height, uint32_t argb)
            {
                if (argb >> 24 != 0) out.push_back(CaseQuad{picture, false, 0.0f, 0.0f, width, height, argb});
            }
        };

        // dim: the dark laid over it; lit: the catch's frame and glow once the strip stops.
        void Tile(Painter& paint, uint16_t item, float x, float y, float alpha, float dim, float lit)
        {
            if (alpha <= 0.0f) return;
            const uint32_t color = CaseColor(item);
            const float halfWide = kTileWidth / 2.0f, halfHigh = kTileHeight / 2.0f, top = y - halfHigh, bottom = y + halfHigh;
            paint.Centered(CasePicture::Glow, true, x, y, halfWide + kLitGlow, halfHigh + kLitGlow, Argb(color, kLitGlowLevel * lit * alpha));
            paint.Centered(CasePicture::Tile, false, x, y, halfWide, halfHigh, Argb(kWhiteInk, alpha));
            paint.Centered(CasePicture::Rise, false, x, bottom - kRiseHeight / 2.0f, halfWide, kRiseHeight / 2.0f, Argb(color, kRiseLevel * alpha));
            paint.Centered(CasePicture::Item, false, x, top + kIconTop + kIcon / 2.0f, kIcon / 2.0f, kIcon / 2.0f, Argb(kWhiteInk, alpha), item);
            paint.Centered(CasePicture::Solid, false, x, bottom - kBarHeight / 2.0f, halfWide, kBarHeight / 2.0f, Argb(color, alpha));
            paint.Centered(CasePicture::Solid, false, x, y, halfWide, halfHigh, Argb(kBand, dim * alpha));
            const uint32_t edge = Argb(color, lit * alpha);
            const float out     = kLitEdge / 2.0f;
            paint.Centered(CasePicture::Solid, false, x, top - out, halfWide + kLitEdge, out, edge);
            paint.Centered(CasePicture::Solid, false, x, bottom + out, halfWide + kLitEdge, out, edge);
            paint.Centered(CasePicture::Solid, false, x - halfWide - out, y, out, halfHigh, edge);
            paint.Centered(CasePicture::Solid, false, x + halfWide + out, y, out, halfHigh, edge);
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

    std::vector<CaseQuad> CaseFrame(const CaseRun& run, float seconds, float length, const CaseSpot& spot, float screenWidth,
        float screenHeight, bool background, const CaseWords& words)
    {
        std::vector<CaseQuad> out;
        const CaseView view = CaseAt(run, seconds, length);
        if (view.over || seconds <= 0.0f) return out;
        Painter paint{out, spot};
        if (background)
        {
            paint.Cover(CasePicture::Solid, screenWidth, screenHeight, Argb(kVeil, kVeilLevel * view.veil));
            paint.Cover(CasePicture::Vignette, screenWidth, screenHeight, Argb(kShade, kShadeLevel * view.veil));
        }
        if (view.strip > 0.0f)
        {
            const float y = kStripY + view.lift;
            paint.Centered(CasePicture::Band, false, kMiddle, y, kShown, kTileHeight / 2.0f + kBandMargin, Argb(kBand, kBandLevel * view.strip));
            paint.Centered(CasePicture::Title, false, kMiddle, kTitleY + view.lift, kCaseTitleHeight / 2.0f * words.title,
                kCaseTitleHeight / 2.0f, Argb(kTitleInk, view.strip));
            for (size_t i = 0; i < run.tiles.size(); ++i)
            {
                const float x = kMiddle + (static_cast<float>(i) - view.along) * kPitch, off = std::fabs(x - kMiddle);
                if (off >= kShown) continue;
                const bool caught = i == static_cast<size_t>(kCaseWinner);
                Tile(paint, run.tiles[i], x, y, view.strip * Clamp01((kShown - off) / kEdge), caught ? 0.0f : kDimLevel * view.settled,
                    caught ? view.settled : 0.0f);
            }
            const float line = view.strip * (1.0f - kLineSettled * view.settled), reach = kTileHeight / 2.0f + kLineReach;
            paint.Centered(CasePicture::Glow, true, kMiddle, y, kLineGlow, reach + kLineGlow, Argb(kLineHalo, kLineHaloLevel * line));
            paint.Centered(CasePicture::Solid, false, kMiddle, y, kLineWidth / 2.0f, reach, Argb(kLineGold, line));
        }
        if (view.reveal > 0.0f)
        {
            const uint16_t caught = run.tiles[kCaseWinner];
            const uint32_t color  = CaseColor(caught);
            paint.Centered(CasePicture::Glow, true, kMiddle, kBigY, kHalo, kHalo, Argb(color, kHaloLevel * view.reveal));
            const float half = kBigIcon / 2.0f * view.iconScale;
            paint.Centered(CasePicture::ItemCrisp, false, kMiddle, kBigY, half, half, Argb(kWhiteInk, view.reveal), caught);
            if (view.named > 0.0f)
            {
                const float nameWide = kCaseNameHeight * words.name;
                paint.Centered(CasePicture::Name, false, kMiddle, kNameY, nameWide / 2.0f, kCaseNameHeight / 2.0f, Argb(kNameInk, view.named));
                paint.Centered(CasePicture::Solid, false, kMiddle, kUnderlineY, (nameWide + kUnderlinePad) / 2.0f * EaseOut(view.named),
                    kUnderline / 2.0f, Argb(color, view.named));
                paint.Centered(CasePicture::Skill, false, kMiddle, kSkillY, kCaseSkillHeight / 2.0f * words.skill, kCaseSkillHeight / 2.0f,
                    Argb(kSkillInk, view.named));
            }
        }
        return out;
    }

    CaseBoxSet CaseBoxesAt(const CaseSpot& spot)
    {
        std::vector<CaseQuad> out;
        Painter paint{out, spot};
        const float reach = kTileHeight / 2.0f + kLineReach, half = kBigIcon / 2.0f;
        paint.Centered(CasePicture::Band, false, kMiddle, kStripY, kShown, kTileHeight / 2.0f + kBandMargin, kOpaque);
        paint.Centered(CasePicture::Solid, false, kMiddle, kStripY, kLineWidth / 2.0f, reach, kOpaque);
        paint.Centered(CasePicture::ItemCrisp, false, kMiddle, kBigY, half, half, kOpaque);
        paint.Centered(CasePicture::Name, false, kMiddle, kNameY, kShown, kCaseNameHeight / 2.0f, kOpaque);
        const auto box = [](const CaseQuad& q) { return CaseBox{q.x, q.y, q.width, q.height}; };
        return CaseBoxSet{box(out[0]), box(out[1]), box(out[2]), box(out[3])};
    }
}

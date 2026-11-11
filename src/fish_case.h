#pragma once

#include "fish_pool.h"

#include <cstdint>
#include <string>
#include <vector>

namespace headsup
{
    // The longest the game's catch animation was seen to take, from the catch message until you can cast again.
    constexpr float kCatchAnimationSeconds = 9.0f;
    constexpr float kCaseBuffer            = 0.5f; // so the Case never ends before you can cast
    constexpr float kCaseSeconds           = kCatchAnimationSeconds + kCaseBuffer;

    constexpr int kCaseTiles = 52, kCaseWinner = 46;
    // stop: where the line ends, on the winner's tile; a whole number is a tile's middle.
    struct CaseRun
    {
        std::vector<uint16_t> tiles;
        float stop = 0.0f;
    };
    // Tiles picked from the pool by weight, the catch on kCaseWinner. An empty pool: every tile the catch.
    CaseRun SpinCase(uint16_t caught, const std::vector<PoolEntry>& pool, uint32_t seed);

    // What the Case shows `seconds` in, of a run `length` long. Every share is 0 to 1; lift is in layout pixels and
    // along in tiles.
    struct CaseView
    {
        float veil = 0, strip = 0, lift = 0, along = 0, settled = 0, reveal = 0, iconScale = 0, named = 0;
        bool over = true;
    };
    CaseView CaseAt(const CaseRun& run, float seconds, float length);
    // When each tile's edge crosses the line, a click each.
    std::vector<float> CaseClicks(const CaseRun& run, float length);
    // A tile's color by the skill its catch asks, 0xRRGGBB.
    uint32_t CaseColor(uint16_t item);
    // "Fishing skill 11"; empty for what no fishing hands out.
    std::string CaseSkillLine(uint16_t item);
    // What the Test button spins to: a pick from the pool by weight, or a Moat Carp with no bait or no pool.
    uint16_t TestCatch(const Angler& angler, const std::vector<PoolEntry>& pool, uint32_t seed);

    // The Case's middle on the screen and the pixels of one layout pixel: the layout is 1280 by 720, scaled to the
    // screen's height, times the scale in percent. kFishBarUnplaced is the screen's middle; the middle stays on it.
    struct CaseSpot
    {
        float x = 0, y = 0, unit = 1;
        bool operator==(const CaseSpot&) const = default;
    };
    CaseSpot CasePlace(int x, int y, int scalePercent, float screenWidth, float screenHeight);
}

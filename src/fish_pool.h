#pragma once

#include "function_ref.h"
#include "magic_chance.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace headsup
{
    // What fishing can hand out, as Phoenix's server loads it.
    struct FishRecord
    {
        uint16_t item;
        uint8_t skill, size;
        bool isItem, legendary, shellfish, quest, questOnly;
        uint16_t rarity, keyItem;
    };
    // Nothing for what no fishing hands out.
    const FishRecord* FishById(uint16_t item);

    constexpr int kRodSlot = kRangedSlot, kBaitSlot = 3; // equipment slots
    constexpr uint16_t kMoatCarp = 4401;

    // Where and with what you fish. x, y, z are the server's: y is height (the client keeps height in Z).
    struct Angler
    {
        uint16_t zone = 0;
        float x = 0, y = 0, z = 0;
        uint16_t rod = 0, bait = 0;
        int skill = 0;
    };
    struct PoolEntry
    {
        uint16_t item;
        uint16_t weight;
        bool operator==(const PoolEntry&) const = default;
    };

    // The area of the zone a point is in, as the server's GetFishingArea picks it; none outside every area.
    std::optional<uint8_t> FishingArea(uint16_t zone, float x, float y, float z);
    // What the spot can give with that bait, sorted by item: fish by hook chance, items by rarity. When the area or
    // the bait is not known, every catch of the zone at weight 1. Empty for a zone with no fishing.
    std::vector<PoolEntry> CatchPool(const Angler& angler, FunctionRef<bool(uint16_t)> hasKeyItem);
}

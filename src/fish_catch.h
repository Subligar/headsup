#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    struct Settings;
    enum class Tag : uint8_t; // plate_template.h

    // What the server tells you of a bite, as the chat says it: its size when it bites, then a feeling about it.
    enum class FishSize : uint8_t { Small, Large, Item, Monster };
    enum class FishFeeling : uint8_t { Good, Bad, Terrible, LowSkill, VeryLowSkill, ExtremelyLowSkill, Epic };
    struct FishCatch
    {
        std::optional<FishSize> size;
        std::optional<FishFeeling> feeling;
        bool operator==(const FishCatch&) const = default;
    };
    // message: the server's number past the zone's first fishing message (ZoneRecord::fishing). A bite starts a new
    // catch; a feeling adds to it; anything else leaves it.
    void ReadFishMessage(FishCatch& fish, uint16_t message);

    // The fishing bar's line, its tags (FishBarTags) as the catch says them.
    std::vector<Tag> FishBarTags();
    std::string FishLine(std::string_view line, const FishCatch& fish);
    // The bar's fill for the catch, a rod about to break first, then a line about to snap, then its size; nothing
    // for the theme's own fill.
    std::optional<uint32_t> FishFill(const Settings& s, const FishCatch& fish);
}

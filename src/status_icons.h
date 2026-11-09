#pragma once

#include "generated/status_packs.h"
#include "plate_image.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace headsup
{
    // A theme's Status Icons: the game's own, or a pack's by its place in kStatusPackNames.
    constexpr int kVanillaStatusIcons       = -1;
    constexpr const char* kVanillaStatusName = "vanilla";
    // Every built-in theme's: the HD pack.
    constexpr int kHdStatusIcons = 0;
    static_assert(kStatusPackNames[kHdStatusIcons] == "HD");
    // Status Icons by name in the settings and a theme's file, kVanillaStatusName or a pack's folder; fallback for a
    // name of none.
    std::string StatusIconsName(int pack);
    int StatusIconsNamed(std::string_view name, int fallback);

    // Whether a status is a debuff, as XIUI's party list tells them apart; every other is a buff.
    bool IsDebuff(uint16_t id);

    // A pack's icon for a status. A pack is unpacked whole the first time one of its icons is asked for, and kept.
    // None for a pack out of range or a status it has no icon for, which shows the game's own.
    std::optional<Image> PackIcon(int pack, uint16_t id);
}

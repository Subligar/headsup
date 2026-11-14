#pragma once

#include "bar_content.h"
#include "bar_theme.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace headsup
{
    constexpr uint16_t kSignetEffect = 253, kSanctionEffect = 256; // Phoenix's status_effects.yaml
    constexpr uint32_t kNoExpiry            = 0x7FFFFFFF; // a status timer that never runs out
    constexpr int64_t kStatusTicksPerSecond = 60;

    // A status timer as the game keeps it: 60ths of a second from the Vana'diel epoch's Earth instant, wrapping at 32
    // bits (XIUI's vanatime.lua). The seconds left at unixMs, 0 once run out; nothing for one that never runs out.
    std::optional<double> StatusSecondsLeft(uint32_t raw, int64_t unixMs);

    struct StatusRecord
    {
        uint16_t id;
        const char* name;
    };
    // Phoenix's status effects in id order (data/phoenix_statuses.tsv). Some share a name, as Sleep and Sleep II do.
    std::span<const StatusRecord> StatusRecords();
    // A status's name; "" for an id Phoenix lacks.
    const char* StatusName(uint16_t id);

    // A status you have, and its seconds left: nothing for one that never runs out or whose timer could not be read.
    struct ActiveStatus
    {
        uint16_t id = 0;
        std::optional<double> secondsLeft;
        bool operator==(const ActiveStatus&) const = default;
    };
    // Each status in the game's buffs once, timed from the status icon slot that holds it: the icons and timers share
    // slots. Each array holds kStatusIconSlots and may be null.
    std::vector<ActiveStatus> ActiveStatuses(const int16_t* buffs, const int16_t* icons, const uint32_t* timers,
        int64_t unixMs);

    // What a press on the popup's star or pin tells (PopupCell::press), then the status's id.
    constexpr const char* kStatusFavoritePress = "status favorite ", *kStatusPinPress = "status pin ";
    constexpr const char* kBuffsKey = "Buffs", *kDebuffsKey = "Debuffs"; // the popup's lists' headings, which keep their scrolls

    // A pinned status with its time left, matched by name, shown by display in the theme's status icons.
    std::vector<Span> PinnedStatus(const std::vector<ActiveStatus>& active, uint16_t id, StatusDisplay display,
        const BarPalette& theme);
    // Your favorites, each with its pin and star, over every other buff in a scrolling list and every other debuff in
    // another, each scrolled its many lines: those you have first, the rest by name, one line a name, each name with
    // its icon in the theme's status icons, in one column. refused: the last pin was kept, which it says under its
    // title.
    PopupContent StatusTimersPopup(const std::vector<ActiveStatus>& active, const std::vector<uint16_t>& favorites,
        const std::vector<uint16_t>& pins, const BarPalette& theme, size_t buffsScrolled, size_t debuffsScrolled,
        bool refused = false);
}

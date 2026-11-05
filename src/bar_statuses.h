#pragma once

#include "bar_content.h"

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
    constexpr const char* kStatusListKey = "Status Timers"; // what the popup's scrolling list is kept by

    // The pinned statuses on one line, each with its time left, matched by name; its name while none is pinned.
    std::vector<Span> PinnedStatuses(const std::vector<ActiveStatus>& active, const std::vector<uint16_t>& pins);
    // Your favorites, each with its pin and star, over every other status in a scrolling list scrolled this many
    // lines: those you have first, the rest by name, one line a name.
    PopupContent StatusTimersPopup(const std::vector<ActiveStatus>& active, const std::vector<uint16_t>& favorites,
        const std::vector<uint16_t>& pins, size_t scrolled);
}

#include "bar_statuses.h"
#include "player_status.h"
#include "test.h"
#include "vanadiel.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    std::string Joined(const std::vector<Span>& spans)
    {
        std::string text;
        for (const Span& s : spans)
            text += s.text;
        return text;
    }

    constexpr int64_t kNow = 1791527400000; // 2026-10-09 06:30 UTC

    // A timer running out seconds after kNow, as the game holds it: 60ths of a second from XIUI's VANA_BASE, which
    // passed 32 bits long before kNow.
    uint32_t TimerFor(double seconds)
    {
        const int64_t ticks = (kNow - kVanaEpochUnixMs) / 1000 * 60;
        return static_cast<uint32_t>(ticks + static_cast<int64_t>(seconds * 60));
    }

    struct Slots
    {
        std::array<int16_t, kStatusIconSlots> buffs{}, icons{};
        std::array<uint32_t, kStatusIconSlots> timers{};
        Slots()
        {
            buffs.fill(-1);
            icons.fill(-1);
        }
        std::vector<ActiveStatus> Read() const { return ActiveStatuses(buffs.data(), icons.data(), timers.data(), kNow); }
    };
}

TEST(a_status_timer_counts_sixtieths_of_a_second_from_the_vana_diel_epoch_and_wraps_at_32_bits)
{
    CHECK_EQ(*StatusSecondsLeft(TimerFor(7200.0), kNow), 7200.0);
    CHECK_EQ(*StatusSecondsLeft(TimerFor(7200.0), kNow + 1000), 7199.0);
    CHECK_EQ(*StatusSecondsLeft(TimerFor(-1.0), kNow), 0.0); // ran out a second ago
    CHECK(!StatusSecondsLeft(kNoExpiry, kNow).has_value());
}

TEST(each_status_in_the_buffs_is_read_once_and_timed_from_the_icon_slot_that_holds_it)
{
    Slots s;
    CHECK(s.Read().empty());
    s.buffs[0] = s.icons[2] = 40; // Protect
    s.timers[2] = TimerFor(1800.0);
    s.buffs[1] = kSignetEffect;  // in no icon slot: no time
    s.buffs[3] = 40;             // twice in the buffs, once here
    s.buffs[4] = 0xFF;           // empty, as the packets have it
    CHECK((s.Read() == std::vector<ActiveStatus>{{40, 1800.0}, {kSignetEffect, std::nullopt}}));
    CHECK(ActiveStatuses(s.buffs.data(), nullptr, nullptr, kNow).size() == 2);
    CHECK(ActiveStatuses(nullptr, s.icons.data(), s.timers.data(), kNow).empty());
}

TEST(status_names_are_phoenixs_and_one_it_lacks_has_none)
{
    CHECK_EQ(std::string(StatusName(kSignetEffect)), "Signet");
    CHECK_EQ(std::string(StatusName(40)), "Protect");
    CHECK_EQ(std::string(StatusName(19)), "Sleep"); // Sleep II, as the game names it
    CHECK_EQ(std::string(StatusName(65000)), "");
    CHECK(StatusRecords().size() > 600);
}

TEST(each_pinned_status_shows_with_its_time_left_at_its_rank_and_a_rank_without_one_shows_nothing)
{
    std::vector<ActiveStatus> active{{40, 1800.0}, {kSignetEffect, std::nullopt}};
    const std::vector<uint16_t> pins{40, kSignetEffect, 33}; // Protect, Signet and Haste
    CHECK(PinnedStatus(active, pins, 0) == (std::vector<Span>{Text("Protect"), Text(" 30m 00s", Tone::Dim)}));
    CHECK(Joined(PinnedStatus(active, pins, 1)) == "Signet -" && Joined(PinnedStatus(active, pins, 2)) == "Haste 0s");
    CHECK(PinnedStatus(active, pins, 3).empty() && PinnedStatus(active, {}, 0).empty());
    // A status matches by name: Sleep II's time shows for a pinned Sleep.
    active.push_back(ActiveStatus{19, 60.0});
    CHECK(Joined(PinnedStatus(active, {2}, 0)) == "Sleep 1m 00s");
}

TEST(the_popup_lists_your_favorites_with_pins_over_every_other_status_those_you_have_first)
{
    const std::vector<ActiveStatus> active{{40, 1800.0}, {33, 90.0}};
    const PopupContent p = StatusTimersPopup(active, {kSignetEffect, 40}, {40}, 3);
    CHECK(Joined(p.title) == "Status Timers" && p.sections.size() == 2 && p.oneColumn);
    CHECK(p.sections[0].heading == "Favorites");
    const PopupRow& top = p.sections[0].rows[0];
    CHECK(top.columns == 4 && top.cells.size() == 8);
    // Signet: a grayed pin, a star, its name and 0, as you do not have it.
    CHECK(top.cells[0].icon == Icon::Pinned && top.cells[0].tint->tone == Tone::Dim && top.cells[0].press == std::string(kStatusPinPress) + "253");
    CHECK(top.cells[1].icon == Icon::Star && top.cells[1].press == std::string(kStatusFavoritePress) + "253");
    CHECK(Joined(top.cells[2].text) == "Signet" && Joined(top.cells[3].text) == "0s" && top.cells[3].align == Align::Right);
    CHECK(top.cells[4].icon == Icon::Pinned && Joined(top.cells[7].text) == "30m 00s"); // Protect, pinned
    const PopupRow& list = p.sections[1].rows[0];
    CHECK(list.nested && list.scroll == kStatusListKey && list.scrolled == 3 && list.columns == 3);
    std::vector<std::string> names;
    for (size_t i = 1; i < list.cells.size(); i += 3)
        names.push_back(Joined(list.cells[i].text));
    // Haste first, as you have it; then the rest by name, each name once, and no favorite.
    CHECK(names[0] == "Haste" && names[1] < names[2] && names[2] < names[3]);
    CHECK(std::ranges::count(names, std::string("Sleep")) == 1);
    CHECK(std::ranges::count(names, std::string("Protect")) == 0 && std::ranges::count(names, std::string("Signet")) == 0);
    CHECK(list.cells[0].icon == Icon::StarEmpty && list.cells[0].press == std::string(kStatusFavoritePress) + "33");
    // No favorites: a line saying how to keep one.
    CHECK(StatusTimersPopup(active, {}, {}, 0).sections[0].rows[0].kind == RowKind::Line);
    // A refused unpin says why just under the title, in the error red.
    const PopupContent refused = StatusTimersPopup(active, {kSignetEffect}, {kSignetEffect}, 0, true);
    CHECK(Joined(refused.subtitle) == "Must have at least one status pinned." && refused.subtitle[0].tone == Tone::Own &&
          refused.subtitle[0].argb == kErrorColor);
    CHECK(refused.sections[0].rows[0].kind == RowKind::Grid);
    // And a seventh pin refused says the most there can be.
    const std::vector<uint16_t> six{40, 33, 41, 42, 43, kSignetEffect};
    CHECK(Joined(StatusTimersPopup(active, six, six, 0, true).subtitle) == "You can not pin more than 6 status effects");
}

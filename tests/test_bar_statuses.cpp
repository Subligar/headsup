#include "bar_statuses.h"
#include "bar_theme.h"
#include "player_status.h"
#include "status_icons.h"
#include "test.h"
#include "vanadiel.h"

#include <algorithm>
#include <array>
#include <set>
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

TEST(each_pinned_status_shows_with_its_time_left)
{
    const BarPalette& theme = PaletteOf(BarTheme::PhoenixGold);
    const auto pinned = [&](const std::vector<ActiveStatus>& active, uint16_t id) {
        return PinnedStatus(active, id, StatusDisplay::Name, TimerFormat::Full, theme);
    };
    std::vector<ActiveStatus> active{{40, 1800.0}, {kSignetEffect, std::nullopt}};
    CHECK(pinned(active, 40) == (std::vector<Span>{Text("Protect"), Text(" 0:30:00", Tone::Dim)}));
    // One that never runs out, and one you do not have (the card: "-", not "0s").
    CHECK(Joined(pinned(active, kSignetEffect)) == "Signet -" && Joined(pinned(active, 33)) == "Haste -");
    // A status matches by name: Sleep II's time shows for a pinned Sleep.
    active.push_back(ActiveStatus{19, 60.0});
    CHECK(Joined(pinned(active, 2)) == "Sleep 0:01:00");
    CHECK(Joined(PinnedStatus(active, 2, StatusDisplay::Name, TimerFormat::Short, theme)) == "Sleep 1.0m");
}

// The card "Condensing status widget timer text", each format's rules at their edges: X hours, Y minutes, Z seconds
// left, a doubled letter two digits.
TEST(a_status_timers_time_left_reads_as_its_timer_format_says)
{
    struct Read
    {
        double seconds;
        const char *full, *simple, *short_;
    };
    constexpr Read kReads[] = {
        {0, "0:00:00", "0s", "0s"},
        {59, "0:00:59", "59s", "59s"},
        // Minutes only past a whole minute; a minute alone is all seconds.
        {60, "0:01:00", "60s", "1.0m"},
        {61, "0:01:01", "1m 1s", "1.0m"},
        {89, "0:01:29", "1m 29s", "1.4m"},
        {599, "0:09:59", "9m 59s", "9.9m"},
        {600, "0:10:00", "10m 0s", "10m"},
        {601, "0:10:01", "10m 1s", "10m"},
        {3599, "0:59:59", "59m 59s", "59m"},
        // Seconds only under an hour.
        {3600, "1:00:00", "1h 0m", "1.0h"},
        {3601, "1:00:01", "1h 0m", "1.0h"},
        {5399, "1:29:59", "1h 29m", "1.4h"},
        {35999, "9:59:59", "9h 59m", "9.9h"},
        {36000, "10:00:00", "10h 0m", "10h"},
        {36001, "10:00:01", "10h 0m", "10h"},
        {90061, "25:01:01", "25h 1m", "25h"},
    };
    for (const Read& read : kReads)
    {
        CHECK_EQ(TimerText(read.seconds, TimerFormat::Full), std::string(read.full));
        CHECK_EQ(TimerText(read.seconds, TimerFormat::Simple), std::string(read.simple));
        CHECK_EQ(TimerText(read.seconds, TimerFormat::Short), std::string(read.short_));
    }
    // A part of a second left counts as a whole one, as the game counts it down.
    CHECK_EQ(TimerText(59.2, TimerFormat::Simple), std::string("60s"));
}

TEST(display_shows_a_pinned_statuss_icon_before_its_name_or_alone_in_the_themes_status_icons)
{
    BarPalette theme  = PaletteOf(BarTheme::PhoenixGold);
    theme.statusIcons = 1;
    theme.statusBack  = true;
    const std::vector<ActiveStatus> active{{40, 1800.0}};
    const StatusMark protect{40, 1, true};
    const std::vector<Span> both = PinnedStatus(active, 40, StatusDisplay::Both, TimerFormat::Simple, theme);
    CHECK(both.size() == 3 && both[0].status == protect && both[0].text.empty());
    CHECK(!both[1].status && both[1].text == " Protect" && both[2].text == " 30m 0s");
    const std::vector<Span> icon = PinnedStatus(active, 40, StatusDisplay::Icon, TimerFormat::Simple, theme);
    CHECK(icon.size() == 2 && icon[0].status == protect && Joined(icon) == " 30m 0s");
    CHECK(std::ranges::none_of(PinnedStatus(active, 40, StatusDisplay::Name, TimerFormat::Simple, theme),
        [](const Span& s) { return s.status.has_value(); }));
}

TEST(the_popup_lists_your_favorites_with_pins_over_every_buff_then_every_debuff_those_you_have_first)
{
    const BarPalette& theme = PaletteOf(BarTheme::PhoenixGold);
    // Protect and Haste are buffs; Dia and Paralysis are debuffs.
    const std::vector<ActiveStatus> active{{40, 1800.0}, {33, 90.0}, {134, 30.0}};
    const PopupContent p = StatusTimersPopup(active, {kSignetEffect, 40}, {40}, theme, 3, 5);
    CHECK(Joined(p.title) == "Status Timers" && p.sections.size() == 3);
    CHECK(p.sections[0].heading == "Favorites" && p.sections[1].heading == "Buffs" && p.sections[2].heading == "Debuffs");
    const PopupRow& top = p.sections[0].rows[0];
    CHECK(top.columns == 4 && top.cells.size() == 8);
    // Signet: a grayed pin, a star, its icon and name and 0, as you do not have it.
    CHECK(top.cells[0].icon == Icon::Pinned && top.cells[0].tint->tone == Tone::Dim && top.cells[0].press == std::string(kStatusPinPress) + "253");
    CHECK(top.cells[1].icon == Icon::Star && top.cells[1].press == std::string(kStatusFavoritePress) + "253");
    CHECK(Joined(top.cells[2].text) == "Signet" && Joined(top.cells[3].text) == "0s" && top.cells[3].align == Align::Right);
    CHECK(top.cells[2].status == (StatusMark{kSignetEffect, kHdStatusIcons, false})); // Phoenix Gold's icons, with no background
    CHECK(top.cells[4].icon == Icon::Pinned && Joined(top.cells[7].text) == "30m 00s"); // Protect, pinned
    const auto namesOf = [](const PopupRow& list) {
        std::vector<std::string> names;
        for (size_t i = 1; i < list.cells.size(); i += 3)
            names.push_back(Joined(list.cells[i].text));
        return names;
    };
    const PopupRow &buffs = p.sections[1].rows[0], &debuffs = p.sections[2].rows[0];
    CHECK(buffs.nested && buffs.scroll == kBuffsKey && buffs.scrolled == 3 && buffs.columns == 3);
    CHECK(debuffs.nested && debuffs.scroll == kDebuffsKey && debuffs.scrolled == 5 && debuffs.columns == 3);
    const std::vector<std::string> buffNames = namesOf(buffs), debuffNames = namesOf(debuffs);
    // Haste first, as you have it; then the rest by name, each name once, and no favorite.
    CHECK(buffNames[0] == "Haste" && buffNames[1] < buffNames[2] && buffNames[2] < buffNames[3]);
    CHECK(std::ranges::count(buffNames, std::string("Protect")) == 0 && std::ranges::count(buffNames, std::string("Signet")) == 0);
    // Dia first among the debuffs; Sleep and Sleep II once, as Sleep.
    CHECK(debuffNames[0] == "Dia" && debuffNames[1] < debuffNames[2]);
    CHECK(std::ranges::count(debuffNames, std::string("Sleep")) == 1 && std::ranges::count(buffNames, std::string("Sleep")) == 0);
    CHECK(std::ranges::count(debuffNames, std::string("Haste")) == 0 && std::ranges::count(buffNames, std::string("Paralysis")) == 0);
    // Every status is in one list or the other.
    CHECK(buffNames.size() + debuffNames.size() + 2 > 500);
    CHECK(buffs.cells[0].icon == Icon::StarEmpty && buffs.cells[0].press == std::string(kStatusFavoritePress) + "33");
    CHECK(buffs.cells[1].status && buffs.cells[1].status->id == 33 && debuffs.cells[1].status->id == 134);
    // No favorites: a line saying how to keep one.
    CHECK(StatusTimersPopup(active, {}, {}, theme, 0, 0).sections[0].rows[0].kind == RowKind::Line);
    // A refused unpin says why just under the title, in the error red.
    const PopupContent refused = StatusTimersPopup(active, {kSignetEffect}, {kSignetEffect}, theme, 0, 0, true);
    CHECK(Joined(refused.subtitle) == "Must have at least one status pinned." && refused.subtitle[0].tone == Tone::Own &&
          refused.subtitle[0].argb == kErrorColor);
    CHECK(refused.sections[0].rows[0].kind == RowKind::Grid);
}

TEST(the_popups_status_icons_are_the_themes_with_its_background_setting)
{
    BarPalette theme  = PaletteOf(BarTheme::PhoenixGold);
    theme.statusIcons = 2;
    theme.statusBack  = true;
    const PopupContent p = StatusTimersPopup({}, {40}, {40}, theme, 0, 0);
    CHECK(p.sections[0].rows[0].cells[2].status == (StatusMark{40, 2, true}));
    CHECK(p.sections[1].rows[0].cells[1].status->pack == 2 && p.sections[2].rows[0].cells[1].status->back);
}

TEST(the_starred_statuses_come_first_in_their_list_and_each_name_once)
{
    const std::vector<uint16_t> starred = {41, 19}; // Shell, and Sleep by its second id
    const std::vector<uint16_t> buffs = StarredFirst(starred, false), debuffs = StarredFirst(starred, true);
    CHECK_EQ(buffs.front(), 41);
    CHECK_EQ(debuffs.front(), 2); // Sleep by its first id
    std::vector<std::string> names;
    for (const uint16_t id : debuffs)
        names.push_back(StatusName(id));
    CHECK(std::ranges::is_sorted(names.begin() + 1, names.end()));
    CHECK(std::set<std::string>(names.begin(), names.end()).size() == names.size());
    for (const uint16_t id : buffs)
        CHECK(!IsDebuff(id));
    CHECK(StarredByName(starred, 2) && StarredByName(starred, 41) && !StarredByName(starred, 40));
}

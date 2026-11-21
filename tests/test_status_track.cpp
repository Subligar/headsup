#include "action_packet.h"
#include "status_track.h"
#include "test.h"

#include <cstdint>
#include <optional>
#include <vector>

using namespace headsup;
using test::ActionPacket;

namespace
{
    constexpr uint32_t kYou = 65001, kCrab = 17186819; // the test character and a Land Crab beside it
    constexpr uint16_t kSleep = 2, kSleepII = 19, kParalysis = 4, kProtect = 40, kShell = 41, kDia = 134,
                       kDefenseBoost = 93;

    std::optional<ActionStatuses> Action(const std::vector<uint8_t>& bytes)
    {
        return ParseActionStatuses(bytes.data(), static_cast<uint32_t>(bytes.size()));
    }
    std::optional<ActionStatuses> Action(const ActionPacket& p) { return ParseActionStatuses(p.bytes.data(), p.Size()); }
    std::optional<StatusChange> Message(const std::vector<uint8_t>& bytes)
    {
        return ParseStatusMessage(bytes.data(), static_cast<uint32_t>(bytes.size()));
    }

    // Packets the local test server sent Headsup (RDM 30) casting at the Land Crab beside it and on itself, as
    // Linkpearl recorded them from Ashita's header on. The chat log read: "3 Dia", "Paralyze (paralyzed)",
    // "Protect (Protect)", the Sleep cast's start, "Sleep (asleep)", the crab's "Scissor Guard (Defense Boost)", then
    // "The Land Crab is no longer asleep.", "The Land Crab's Dia effect wears off." and, after a GM's delStatusEffect,
    // "Headsup's Shell effect wears off."
    const std::vector<uint8_t> kDiaCast = {0x28, 0x12, 0x8C, 0x00, 0x23, 0xE9, 0xFD, 0x00, 0x00, 0x01, 0xD0, 0x05,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0xC0, 0x00, 0x90, 0x41, 0x40, 0x80, 0x0B, 0x00, 0x60, 0x00, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> kParalyzeCast = {0x28, 0x12, 0xB3, 0x00, 0x23, 0xE9, 0xFD, 0x00, 0x00, 0x01, 0x90,
        0x0E, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0xC0, 0x00, 0x90, 0x41, 0x40, 0x00, 0x1D, 0x00, 0x80, 0x00, 0x00,
        0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> kProtectCast = {0x28, 0x12, 0xBE, 0x00, 0x23, 0xE9, 0xFD, 0x00, 0x00, 0x01, 0xD0, 0x0A,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x40, 0x7A, 0x3F, 0x00, 0x40, 0x80, 0x15, 0x00, 0x00, 0x05, 0x80, 0x39,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> kSleepStart = {0x28, 0x12, 0xD9, 0x00, 0x23, 0xE9, 0xFD, 0x00, 0x00, 0x01, 0xE0, 0x58,
        0x98, 0xD8, 0x1A, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x90, 0x41, 0x40, 0x00, 0x00, 0x00, 0xA0, 0x1F, 0xC0, 0x51,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> kSleepCast = {0x28, 0x12, 0xE0, 0x00, 0x23, 0xE9, 0xFD, 0x00, 0x00, 0x01, 0x50, 0x3F,
        0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0xC0, 0x00, 0x90, 0x41, 0x40, 0x80, 0x7E, 0x00, 0x40, 0x00, 0x00, 0x3B,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> kScissorGuard = {0x28, 0x12, 0x22, 0x01, 0x23, 0x03, 0x40, 0x06, 0x01, 0x01, 0x6C,
        0x6F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x90, 0x41, 0x40, 0xE0, 0x5E, 0x00, 0xA0, 0x0B, 0x80,
        0x2E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> kNoLongerAsleep = {0x29, 0x0E, 0xE3, 0x00, 0xE9, 0xFD, 0x00, 0x00, 0x03, 0x40, 0x06,
        0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x03, 0x00, 0xCC, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> kDiaWearsOff = {0x29, 0x0E, 0x1E, 0x01, 0xE9, 0xFD, 0x00, 0x00, 0x03, 0x40, 0x06, 0x01,
        0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x03, 0x00, 0xCE, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> kShellWearsOff = {0x29, 0x0E, 0x2C, 0x01, 0xE9, 0xFD, 0x00, 0x00, 0xE9, 0xFD, 0x00,
        0x00, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0xCE, 0x00, 0x00, 0x00};

    std::vector<StatusChange> Gained(uint32_t entity, uint16_t status) { return {StatusChange{entity, status, true}}; }
}

TEST(a_real_enfeeble_or_buff_puts_its_status_on_its_target)
{
    const std::optional<ActionStatuses> paralyze = Action(kParalyzeCast);
    CHECK(paralyze && paralyze->actor == kYou && paralyze->finished);
    CHECK(paralyze->changes == Gained(kCrab, kParalysis));
    CHECK(Action(kProtectCast)->changes == Gained(kYou, kProtect));
    CHECK(Action(kSleepCast)->changes == Gained(kCrab, kSleep));
    const std::optional<ActionStatuses> guard = Action(kScissorGuard); // a mob skill on itself
    CHECK(guard && guard->actor == kCrab && guard->finished);
    CHECK(guard->changes == Gained(kCrab, kDefenseBoost));
}

TEST(a_real_damage_spell_and_a_cast_start_put_no_status_on_anyone)
{
    const std::optional<ActionStatuses> dia = Action(kDiaCast); // Dia's status comes with a damage message
    CHECK(dia && dia->finished && dia->changes.empty());
    const std::optional<ActionStatuses> start = Action(kSleepStart);
    CHECK(start && start->actor == kYou && !start->finished && start->changes.empty());
}

TEST(a_real_wear_off_takes_the_status_off_whom_it_names)
{
    CHECK((Message(kNoLongerAsleep) == StatusChange{kCrab, kSleep, false}));
    CHECK((Message(kDiaWearsOff) == StatusChange{kCrab, kDia, false}));
    CHECK((Message(kShellWearsOff) == StatusChange{kYou, kShell, false}));
    std::vector<uint8_t> check = kDiaWearsOff; // a check's answer, "easy prey", with a level where the status was
    check[0x0C] = 20;
    check[0x18] = 0xAA;
    CHECK(!Message(check));
}

TEST(an_area_spell_puts_its_status_on_each_target)
{
    ActionPacket p;
    p.Head(kYou, 2).Target(kCrab, 1).Result(kSleep, 236).Target(kCrab + 1, 1).Result(kSleep, 237);
    CHECK((Action(p)->changes == std::vector<StatusChange>{{kCrab, kSleep, true}, {kCrab + 1, kSleep, true}}));
}

TEST(an_additional_effect_or_spikes_status_goes_where_its_message_says)
{
    ActionPacket added; // a mob's hit that paralyzes you
    added.Head(kCrab, 1, 1).Target(kYou, 1).Added(12, 1, kParalysis, 160);
    CHECK(Action(added)->changes == Gained(kYou, kParalysis));
    ActionPacket self; // your hit that buffs you
    self.Head(kYou, 1, 1).Target(kCrab, 1).Added(12, 1, kProtect, 166);
    CHECK(Action(self)->changes == Gained(kYou, kProtect));
    ActionPacket spikes; // hitting the crab's spikes paralyzes you
    spikes.Head(kYou, 1, 1).Target(kCrab, 1).Spiked(12, 1, kParalysis, 374);
    CHECK(Action(spikes)->changes == Gained(kYou, kParalysis));
    ActionPacket skill; // a mob skill's enfeeble
    skill.Head(kCrab, 1, 11).Target(kYou, 1).Result(kSleep, 242);
    CHECK(Action(skill)->changes == Gained(kYou, kSleep));
}

TEST(an_erase_takes_the_status_off_its_target)
{
    ActionPacket p;
    p.Head(kYou, 1).Target(kCrab, 1).Result(kParalysis, 341);
    CHECK((Action(p)->changes == std::vector<StatusChange>{{kCrab, kParalysis, false}}));
}

TEST(an_interrupted_cast_is_not_finished)
{
    ActionPacket p;
    p.Head(kYou, 1, 8).Target(kYou, 1).Result(0, 16);
    const std::optional<ActionStatuses> cast = Action(p);
    CHECK(cast && !cast->finished);
}

TEST(an_action_or_message_cut_short_is_none)
{
    for (size_t size = 0; size < kParalyzeCast.size() - 1; ++size)
        CHECK(!ParseActionStatuses(kParalyzeCast.data(), static_cast<uint32_t>(size)));
    for (size_t size = 0; size < 0x1A; ++size)
        CHECK(!ParseStatusMessage(kDiaWearsOff.data(), static_cast<uint32_t>(size)));
}

TEST(a_tracked_status_lasts_until_it_is_lost)
{
    StatusTrack track;
    track.Apply({kCrab, kParalysis, true}, 0.0);
    track.Apply({kCrab, kParalysis, true}, 1.0);
    StatusList list = track.Statuses(kCrab, 2.0);
    CHECK_EQ(list.count, 1);
    CHECK_EQ(list.ids[0], kParalysis);
    CHECK_EQ(track.Statuses(kYou, 2.0).count, 0);
    track.Apply({kCrab, kParalysis, false}, 3.0);
    CHECK_EQ(track.Statuses(kCrab, 3.0).count, 0);
}

TEST(acting_wakes_an_entity_but_keeps_its_other_statuses)
{
    StatusTrack track;
    track.Apply({kCrab, kDia, true}, 0.0);
    track.Apply({kCrab, kSleepII, true}, 0.0);
    track.Acted(kYou);
    CHECK_EQ(track.Statuses(kCrab, 1.0).count, 2);
    track.Acted(kCrab);
    const StatusList list = track.Statuses(kCrab, 1.0);
    CHECK_EQ(list.count, 1);
    CHECK_EQ(list.ids[0], kDia);
}

TEST(an_entity_gone_from_view_loses_its_statuses)
{
    StatusTrack track;
    track.Apply({kCrab, kParalysis, true}, 0.0);
    track.Apply({kYou, kProtect, true}, 0.0);
    const uint32_t living[] = {kYou};
    track.Keep(living);
    CHECK_EQ(track.Statuses(kCrab, 1.0).count, 0);
    CHECK_EQ(track.Statuses(kYou, 1.0).count, 1);
}

TEST(a_tracked_status_ends_at_its_kinds_cap_and_a_new_gain_restarts_it)
{
    StatusTrack track;
    track.Apply({kCrab, kParalysis, true}, 0.0);
    track.Apply({kCrab, kProtect, true}, 0.0);
    CHECK_EQ(track.Statuses(kCrab, kTrackedDebuffSeconds - 1.0).count, 2);
    const StatusList later = track.Statuses(kCrab, kTrackedDebuffSeconds);
    CHECK_EQ(later.count, 1);
    CHECK_EQ(later.ids[0], kProtect);
    CHECK_EQ(track.Statuses(kCrab, kTrackedBuffSeconds).count, 0);
    track.Apply({kCrab, kParalysis, true}, 100.0);
    CHECK_EQ(track.Statuses(kCrab, kTrackedDebuffSeconds + 50.0).count, 2); // with Protect
}

TEST(a_status_with_no_name_is_not_tracked)
{
    StatusTrack track;
    track.Apply({kCrab, 999, true}, 0.0);
    CHECK_EQ(track.Statuses(kCrab, 0.0).count, 0);
}

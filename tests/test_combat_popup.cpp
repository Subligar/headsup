#include "boxes.h"
#include "bar_widgets.h"
#include "combat_popup.h"
#include "screen_vertex.h"
#include "test.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace headsup;
using test::Near;

namespace
{
    // A popup quad as the game sends it: a triangle strip of four corners at one depth, every corner the same color.
    std::array<ScreenVertex, 4> Quad(float x0, float y0, float x1, float y1, float depth, uint32_t color)
    {
        return {{{x0, y0, depth, 1.0f, color, 0.0f, 0.0f}, {x1, y0, depth, 1.0f, color, 1.0f, 0.0f},
            {x0, y1, depth, 1.0f, color, 0.0f, 1.0f}, {x1, y1, depth, 1.0f, color, 1.0f, 1.0f}}};
    }

    void Add(CombatPopupList& list, const std::array<ScreenVertex, 4>& quad)
    {
        list.Add(quad.data(), quad.size(), sizeof(ScreenVertex));
    }

    // The two quads of a "Miss" in the test client at 3840x2160 (draws1.json, draws 3044 and 3045), and a single figure
    // (draws7.json, draw 3032).
    constexpr float kMissLeft = 1874.5444f, kMissRight = 1973.5444f;
    constexpr float kMissTop1 = 181.98697f, kMissBottom1 = 262.98697f, kMissTop2 = 181.99231f, kMissBottom2 = 262.99231f;
    constexpr float kFigureLeft = 1892.5444f, kFigureWidth = 54.0f, kFigureTop = 181.98697f, kFigureHeight = 81.0f;

    // Messages from the server's msg_basic.h: a hit, a miss, a cure; the actor and target are entity indexes.
    constexpr PopupEvent kHit{1, 0x400, 0x0B2}, kMiss{15, 0x0B2, 0x400}, kCure{7, 0x400, 0x400};
    constexpr uintptr_t kTask1 = 0x1000, kTask2 = 0x2000, kParticle1 = 0x9000, kParticle2 = 0xA000;

    std::array<ColorPick, kPopupTypeCount> Picks(const ColorPick& each)
    {
        std::array<ColorPick, kPopupTypeCount> picks{};
        picks.fill(each);
        return picks;
    }
    const std::array<ColorPick, kPopupTypeCount> kGamePicks = Picks(ColorPick{ColorSource::Game});
    PopupSides Each(PopupSide side)
    {
        PopupSides sides{};
        sides.fill(side);
        return sides;
    }
    const PopupPlacing kOnTarget{Each(PopupSide::Middle), 0.0f, 0.0f, 0.0f, {}};
}

TEST(every_quad_inside_a_popups_draw_widens_its_box_and_the_last_gives_its_depth_and_color)
{
    CombatPopupList list;
    list.Begin(":", kParticle1, kMiss);
    CHECK(list.Open());
    Add(list, Quad(kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0.25f, 0x40000000));
    Add(list, Quad(kMissLeft, kMissTop2, kMissRight, kMissBottom2, 0.5f, 0x80404040));
    list.End();
    CHECK(!list.Open());
    CHECK_EQ(list.Popups().size(), size_t{1});
    const CombatPopup& miss = list.Popups().front();
    CHECK(std::string_view(miss.text) == ":");
    CHECK(Near(miss.box.minX, kMissLeft) && Near(miss.box.maxX, kMissRight));
    CHECK(Near(miss.box.minY, kMissTop1) && Near(miss.box.maxY, kMissBottom2));
    CHECK_EQ(miss.depth, 0.5f);
    CHECK_EQ(miss.color, 0x80404040u);
    CHECK(miss.particle == kParticle1 && miss.event == kMiss);
}

TEST(three_figures_side_by_side_are_one_popup_as_wide_as_all_three)
{
    CombatPopupList list;
    list.Begin("123", {}, {});
    for (int i = 0; i < 3; ++i)
    {
        const float left = kFigureLeft + kFigureWidth * static_cast<float>(i);
        Add(list, Quad(left, kFigureTop, left + kFigureWidth, kFigureTop + kFigureHeight, 0.5f, 0x80404040));
    }
    list.End();
    CHECK_EQ(list.Popups().size(), size_t{1});
    CHECK(Near(list.Popups().front().box.Width(), 3 * kFigureWidth));
    CHECK(Near(list.Popups().front().box.Height(), kFigureHeight));
}

TEST(a_popup_that_sent_no_quad_is_dropped_and_a_quad_outside_a_popup_is_ignored)
{
    CombatPopupList list;
    list.Begin("12", {}, {});
    list.End();
    Add(list, Quad(kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0.5f, 0x80404040));
    CHECK(list.Popups().empty());
}

TEST(a_popup_keeps_at_most_the_games_sixteen_bytes_of_text)
{
    CombatPopupList list;
    list.Begin("12345678901234567890", {}, {});
    Add(list, Quad(kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0.5f, 0x80404040));
    list.End();
    CHECK(list.Popups().size() == 1 && std::string_view(list.Popups().front().text) == "1234567890123456");
}

TEST(a_new_frame_forgets_every_popup_and_closes_an_open_one)
{
    CombatPopupList list;
    list.Begin("7", {}, {});
    Add(list, Quad(kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0.5f, 0x80404040));
    list.NewFrame();
    CHECK(list.Popups().empty() && !list.Open());
    Add(list, Quad(kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0.5f, 0x80404040));
    CHECK(list.Popups().empty());
}

namespace
{
    CombatPopup Popup(std::string_view text, float x0, float y0, float x1, float y1, uint32_t color, float depth,
        std::optional<PopupEvent> event = std::nullopt)
    {
        CombatPopup popup;
        popup.event = event;
        text.copy(popup.text, kMaxCombatPopupText);
        popup.box   = test::Box(x0, y0, x1, y1);
        popup.color = color;
        popup.depth = depth;
        return popup;
    }

    // The measured "Miss" came as two calls of one frame on boxes 0.005 px apart: here the first is faint and black.
    const std::vector<CombatPopup> kMissTwins = {Popup(":", kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0x40000000, 0.25f),
        Popup(":", kMissLeft, kMissTop2, kMissRight, kMissBottom2, 0x80404040, 0.5f)};
}

TEST(the_games_two_calls_for_one_miss_are_one_word_where_the_later_was_as_tall_as_its_box)
{
    const std::vector<ShownCombatPopup> shown = ShownCombatPopups(kMissTwins, 100, kGamePicks, kOnTarget);
    CHECK_EQ(shown.size(), size_t{1});
    CHECK(shown[0].words == "Miss");
    CHECK_EQ(shown[0].color, 0xFF808080u);
    CHECK_EQ(shown[0].alpha, 0xFFu);
    CHECK(Near(shown[0].center.x, 1924.0444f) && Near(shown[0].center.y, 222.49231f));
    CHECK(Near(shown[0].lineHeight, 81.0f));
    CHECK_EQ(shown[0].depth, 0.5f);
    CHECK(Near(ShownCombatPopups(kMissTwins, 50, kGamePicks, kOnTarget)[0].lineHeight, 40.5f));
}

TEST(popups_with_the_same_text_far_apart_or_other_texts_on_one_box_stay_apart)
{
    const std::vector<CombatPopup> apart = {Popup("12", 1000, 200, 1108, 281, 0x80404040, 0.5f),
        Popup("12", 1200, 200, 1308, 281, 0x80404040, 0.5f)};
    CHECK_EQ(ShownCombatPopups(apart, 100, kGamePicks, kOnTarget).size(), size_t{2});
    const std::vector<CombatPopup> others = {Popup("12", 1000, 200, 1108, 281, 0x80404040, 0.5f),
        Popup("21", 1000, 200, 1108, 281, 0x80404040, 0.5f)};
    const std::vector<ShownCombatPopup> shown = ShownCombatPopups(others, 100, kGamePicks, kOnTarget);
    CHECK(shown.size() == 2 && shown[0].words == "12" && shown[1].words == "21");
}

TEST(a_popups_color_shows_twice_as_bright_and_its_alpha_twice_as_strong_up_to_opaque)
{
    const std::vector<ShownCombatPopup> half = ShownCombatPopups({Popup("7", 0, 0, 54, 81, 0x40404040, 0.5f)}, 100, kGamePicks, kOnTarget);
    CHECK(half.size() == 1 && half[0].color == 0xFF808080u && half[0].alpha == 0x80u);
    CHECK_EQ(ShownCombatPopups({Popup("7", 0, 0, 54, 81, 0xFF404040, 0.5f)}, 100, kGamePicks, kOnTarget)[0].alpha, 0xFFu);
}

TEST(a_faster_fade_takes_a_popup_further_toward_clear_at_every_step_of_the_games_fade)
{
    // The game's alpha 0x40, shown as 128 of 255: its fade speed is the power of its share of opaque.
    const auto alpha = [](uint32_t color, int fade) {
        return ShownCombatPopups({Popup("7", 0, 0, 54, 81, color, 0.5f)}, 100, kGamePicks, kOnTarget, {}, fade)[0].alpha;
    };
    CHECK_EQ(alpha(0x40404040, kDefaultCombatPopupFade), 128u);
    CHECK_EQ(alpha(0x40404040, 200), 64u);  // 255 * (128 / 255)^2
    CHECK_EQ(alpha(0x40404040, 50), 181u);  // 255 * (128 / 255)^0.5
    CHECK_EQ(alpha(0x08404040, 200), 1u);   // 16 of 255, squared
    CHECK_EQ(alpha(0xFF404040, 200), 0xFFu); // opaque stays opaque
    CHECK_EQ(alpha(0x00404040, 50), 0u);     // and clear clear
}

TEST(a_miss_says_what_happened_by_its_message)
{
    const auto words = [](uint32_t message) { return CombatPopupWords(":", PopupEvent{message, 0, 0}); };
    CHECK(words(15) == "Miss");
    CHECK(words(282) == "Evade");
    CHECK(words(32) == "Evade");
    CHECK(words(70) == "Parry");
    CHECK(words(31) == "Shadow");
    CHECK(words(85) == "Resist");
    CHECK(words(75) == "No Effect");
    CHECK(words(30) == "Anticipate");
    CHECK(words(114) == "Fail");
    CHECK(words(63) == "Miss");
    CHECK(CombatPopupWords(":", std::nullopt) == "Miss");
}

TEST(with_words_off_every_miss_is_the_games_plain_miss)
{
    PopupSwitches off;
    off.words = false;
    for (const uint32_t message : {15u, 282u, 70u, 31u, 85u, 75u, 114u})
        CHECK(CombatPopupWords(":", PopupEvent{message, 0, 0}, off) == "Miss");
}

TEST(a_number_keeps_its_figures_whatever_its_message)
{
    CHECK(CombatPopupWords("1234", kHit) == "1234");
    CHECK(CombatPopupWords("1234", PopupEvent{70, 0, 0}) == "1234");
}

TEST(an_amount_of_mp_or_tp_and_a_drain_say_what_they_are_and_hp_stays_a_number)
{
    const auto words = [](std::string_view text, uint32_t message) { return CombatPopupWords(text, PopupEvent{message, 0, 0}); };
    CHECK_EQ(words("20", 25), std::string("20 MP"));
    CHECK_EQ(words("300", 452), std::string("300 TP"));
    CHECK_EQ(words("55", 227), std::string("Drain 55"));
    CHECK_EQ(words("12", 228), std::string("Drain 12 MP"));
    CHECK_EQ(words("100", 165), std::string("Drain 100 TP"));
    CHECK_EQ(words("34", 7), std::string("34"));
    CHECK_EQ(words("128", 1), std::string("128"));
    CHECK_EQ(CombatPopupWords("20", std::nullopt), std::string("20"));
}

TEST(with_labels_off_an_amount_or_a_drain_is_the_games_bare_number)
{
    PopupSwitches off;
    off.labels = false;
    CHECK_EQ(CombatPopupWords("20", PopupEvent{25, 0, 0}, off), std::string("20"));
    CHECK_EQ(CombatPopupWords("55", PopupEvent{227, 0, 0}, off), std::string("55"));
    CHECK_EQ(CombatPopupWords("100", PopupEvent{165, 0, 0}, off), std::string("100"));
    CHECK_EQ(CombatPopupWords(":", PopupEvent{70, 0, 0}, off), std::string("Parry"));
}

TEST(a_shown_miss_has_its_messages_word)
{
    const CombatPopup parry = Popup(":", kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0x80404040, 0.5f, PopupEvent{70, 0, 0});
    CHECK(ShownCombatPopups({parry}, 100, kGamePicks, kOnTarget)[0].words == "Parry");
}

TEST(a_popups_text_is_centered_across_with_its_capitals_middle_on_the_popups_middle)
{
    // Drawn 24 px tall in a 40 x 30 image with a 3 px halo, its capitals from row 5 to row 19 under the halo. Shown in a
    // 48 px line each texture pixel is 2 image pixels, so the capitals' middle, 3 + 12 rows down, is 30 pixels down.
    const CombatTextSize text{40.0f, 30.0f, 3.0f, 5.0f, 19.0f, 24};
    const ScreenPoint center{100.4f, 200.6f};
    CHECK(PlaceCombatPopupText(center, 48.0f, text, 1.0f, 0.5f) == (SquareQuad{60.0f, 171.0f, 80.0f, 60.0f}));
    CHECK(PlaceCombatPopupText(center, 48.0f, text, 2.0f, 0.5f) == (SquareQuad{20.0f, 171.0f, 160.0f, 60.0f}));
}

TEST(the_draw_dump_lines_hold_a_popups_text_color_place_and_depth)
{
    const CombatPopup miss = Popup(":", kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0x80404040, 0.99876f);
    CHECK_EQ(DescribeCombatPopup(miss), std::string(": | 80404040 | 1874.54,181.99 to 1973.54,262.99 | 0.99876 | message -"));
    const CombatPopup parry = Popup(":", kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0x80404040, 0.99876f, PopupEvent{70, 0, 0});
    CHECK_EQ(DescribeCombatPopup(parry), std::string(": | 80404040 | 1874.54,181.99 to 1973.54,262.99 | 0.99876 | message 70"));
    const ShownCombatPopup shown = ShownCombatPopups({miss}, 100, kGamePicks, kOnTarget)[0];
    CHECK_EQ(DescribeCombatPopup(shown), std::string("Miss | FF808080 alpha 255 | 1924.04,222.49 line 81.00 | 0.99876"));
}

TEST(each_started_popup_takes_the_decision_before_it_and_its_particles_take_the_starts)
{
    PopupTies ties;
    ties.Decided(kHit);
    ties.Started(kTask1);
    ties.Decided(kMiss);
    ties.Started(kTask2);
    ties.Built(kParticle1, kTask2);
    ties.Built(kParticle2, kTask1);
    CHECK(ties.Of(kParticle1) == kMiss);
    CHECK(ties.Of(kParticle2) == kHit);
}

TEST(a_start_takes_only_the_last_decision_and_a_second_start_takes_none)
{
    PopupTies ties;
    ties.Decided(kHit);
    ties.Decided(kMiss);
    ties.Started(kTask1);
    ties.Started(kTask2);
    ties.Built(kParticle1, kTask1);
    ties.Built(kParticle2, kTask2);
    CHECK(ties.Of(kParticle1) == kMiss);
    CHECK(!ties.Of(kParticle2));
}

TEST(a_particle_of_a_task_nobody_started_has_no_message)
{
    PopupTies ties;
    ties.Built(kParticle1, kTask1);
    CHECK(!ties.Of(kParticle1));
    CHECK(!ties.Of(kParticle2));
}

TEST(a_particle_built_again_at_its_address_takes_the_new_tasks_message_or_none)
{
    PopupTies ties;
    ties.Decided(kHit);
    ties.Started(kTask1);
    ties.Decided(kCure);
    ties.Started(kTask2);
    ties.Built(kParticle1, kTask1);
    ties.Built(kParticle1, kTask2);
    CHECK(ties.Of(kParticle1) == kCure);
    ties.Built(kParticle1, 0x3000);
    CHECK(!ties.Of(kParticle1));
}

TEST(the_ties_keep_the_last_64_starts)
{
    PopupTies ties;
    for (uint32_t i = 1; i <= 65; ++i)
    {
        ties.Decided(PopupEvent{i, 0, 0});
        ties.Started(kTask1 * i);
    }
    ties.Built(kParticle1, kTask1);
    ties.Built(kParticle2, kTask1 * 2);
    ties.Built(kParticle2 + 1, kTask1 * 65);
    CHECK(!ties.Of(kParticle1));
    CHECK(ties.Of(kParticle2) && ties.Of(kParticle2)->message == 2);
    CHECK(ties.Of(kParticle2 + 1) && ties.Of(kParticle2 + 1)->message == 65);
}

namespace
{
    // The kinds are the client's own (its byte table at 0x35CAE0): 1 damage, 2 recovery, 3 miss.
    constexpr uint8_t kDamageKind = 1, kRecoveryKind = 2, kMissKind = 3;
    constexpr uint32_t kYou = 0x400, kYourPet = 0x701, kMob = 0x0B2, kPartyMember = 0x405;

    // The type the ties give a popup the game decided on for event, after the ones decided before it.
    std::optional<PopupType> TypeOf(PopupTies& ties, const PopupEvent& event)
    {
        ties.Decided(event);
        ties.Started(kTask1);
        ties.Built(kParticle1, kTask1);
        return ties.Of(kParticle1)->type;
    }

    std::optional<PopupType> TypeOf(const PopupEvent& event)
    {
        PopupTies ties;
        ties.SetYou(kYou, kYourPet);
        return TypeOf(ties, event);
    }
}

TEST(a_popup_is_typed_by_what_happened_and_plain_damage_by_whether_it_is_yours)
{
    CHECK(TypeOf(PopupEvent{67, kYou, kMob, kDamageKind}) == PopupType::Critical);
    CHECK(TypeOf(PopupEvent{67, kMob, kYou, kDamageKind}) == PopupType::Critical);
    CHECK(TypeOf(PopupEvent{353, kPartyMember, kMob, kDamageKind}) == PopupType::Critical);
    CHECK(TypeOf(PopupEvent{1, kYou, kMob, kDamageKind}) == PopupType::Dealt);
    CHECK(TypeOf(PopupEvent{1, kYourPet, kMob, kDamageKind}) == PopupType::Dealt);
    CHECK(TypeOf(PopupEvent{1, kMob, kYou, kDamageKind}) == PopupType::Taken);
    CHECK(TypeOf(PopupEvent{1, kMob, kYourPet, kDamageKind}) == PopupType::Taken);
    CHECK(!TypeOf(PopupEvent{1, kPartyMember, kMob, kDamageKind}));
    CHECK(!TypeOf(PopupEvent{1, kMob, kPartyMember, kDamageKind}));
    CHECK(TypeOf(PopupEvent{2, kPartyMember, kMob, kDamageKind}) == PopupType::Spell);
    CHECK(TypeOf(PopupEvent{252, kYou, kMob, kDamageKind}) == PopupType::Burst);
    CHECK(TypeOf(PopupEvent{7, kPartyMember, kPartyMember, kRecoveryKind}) == PopupType::Recovery);
    CHECK(TypeOf(PopupEvent{25, kYou, kYou, kRecoveryKind}) == PopupType::Recovery);
    CHECK(TypeOf(PopupEvent{15, kMob, kYou, kMissKind}) == PopupType::Miss);
    CHECK(TypeOf(PopupEvent{70, kYou, kMob, kMissKind}) == PopupType::Miss);
}

TEST(no_pet_is_not_an_entity_at_index_0)
{
    PopupTies ties;
    ties.SetYou(kYou, 0);
    CHECK(!TypeOf(ties, PopupEvent{1, 0, kMob, kDamageKind}));
}

TEST(damage_to_the_next_target_of_a_spell_is_the_spells_and_only_after_that_actors_spell)
{
    PopupTies ties;
    ties.SetYou(kYou, kYourPet);
    TypeOf(ties, PopupEvent{2, kPartyMember, kMob, kDamageKind});
    CHECK(TypeOf(ties, PopupEvent{264, kPartyMember, kMob + 1, kDamageKind}) == PopupType::Spell);
    CHECK(TypeOf(ties, PopupEvent{264, kPartyMember, kMob + 2, kDamageKind}) == PopupType::Spell);
    TypeOf(ties, PopupEvent{252, kPartyMember, kMob, kDamageKind});
    CHECK(TypeOf(ties, PopupEvent{264, kPartyMember, kMob + 1, kDamageKind}) == PopupType::Spell);
    TypeOf(ties, PopupEvent{185, kPartyMember, kMob, kDamageKind});
    CHECK(!TypeOf(ties, PopupEvent{264, kPartyMember, kMob + 1, kDamageKind}));
    TypeOf(ties, PopupEvent{2, kYou, kMob, kDamageKind});
    CHECK(!TypeOf(ties, PopupEvent{264, kPartyMember, kMob + 1, kDamageKind}));
}

TEST(a_popup_of_a_type_picked_custom_takes_that_color_at_the_games_alpha)
{
    const PopupEvent critical{67, kYou, kMob, kDamageKind, PopupType::Critical};
    const CombatPopup popup = Popup("210", 0, 0, 54, 81, 0x40404040, 0.5f, critical);
    std::array<ColorPick, kPopupTypeCount> picks{};
    picks[static_cast<size_t>(PopupType::Critical)] = ColorPick{ColorSource::Custom, ColorFromRgb(0xFF0000)};
    const ShownCombatPopup custom = ShownCombatPopups({popup}, 100, picks, kOnTarget)[0];
    CHECK_EQ(custom.color, 0xFFFF0000u);
    CHECK_EQ(custom.alpha, 0x80u);
    picks[static_cast<size_t>(PopupType::Critical)].source = ColorSource::Game;
    CHECK_EQ(ShownCombatPopups({popup}, 100, picks, kOnTarget)[0].color, 0xFF808080u);
}

TEST(a_popup_with_no_type_keeps_the_games_color_whatever_is_picked)
{
    const auto shown = [](std::optional<PopupEvent> event) {
        return ShownCombatPopups({Popup("128", 0, 0, 54, 81, 0x40404040, 0.5f, event)}, 100, Picks(ColorPick{ColorSource::Custom, ColorFromRgb(0x00FF00)}), kOnTarget)[0].color;
    };
    CHECK_EQ(shown(PopupEvent{1, kPartyMember, kMob, kDamageKind}), 0xFF808080u);
    CHECK_EQ(shown(std::nullopt), 0xFF808080u);
    CHECK_EQ(shown(PopupEvent{1, kYou, kMob, kDamageKind, PopupType::Dealt}), 0xFF00FF00u);
}

TEST(with_custom_colors_off_every_popup_keeps_the_games_color)
{
    PopupSwitches off;
    off.colors = false;
    const PopupEvent critical{67, kYou, kMob, kDamageKind, PopupType::Critical};
    std::array<ColorPick, kPopupTypeCount> picks = Picks(ColorPick{ColorSource::Custom, ColorFromRgb(0xFF0000)});
    const CombatPopup popup = Popup("210", 0, 0, 54, 81, 0x40404040, 0.5f, critical);
    CHECK_EQ(ShownCombatPopups({popup}, 100, picks, kOnTarget, off)[0].color, 0xFF808080u);
    CHECK_EQ(ShownCombatPopups({popup}, 100, picks, kOnTarget)[0].color, 0xFFFF0000u);
}

TEST(a_skillchain_says_its_name_by_the_servers_number_for_it)
{
    // The server sends 287 + n for a skillchain's damage and 384 + n for what it heals, n by its ActionProcSkillChain.
    const auto words = [](std::string_view text, uint32_t message) { return CombatPopupWords(text, PopupEvent{message, 0, 0}); };
    CHECK_EQ(words("412", 287 + 6), std::string("Fusion 412"));
    CHECK_EQ(words("412", 384 + 6), std::string("Fusion 412"));
    CHECK_EQ(words("300", 287 + 1), std::string("Light 300"));
    CHECK_EQ(words("57", 287 + 8), std::string("Liquefaction 57"));
    CHECK_EQ(words("88", 287 + 14), std::string("Impaction 88"));
    CHECK_EQ(words("900", 287 + 15), std::string("Radiance 900"));
    CHECK_EQ(words("900", 767), std::string("Radiance 900"));
    CHECK_EQ(words("900", 768), std::string("Umbra 900"));
    CHECK_EQ(words("900", 770), std::string("Umbra 900"));
}

TEST(a_skillchain_is_its_own_type_whether_it_hurts_or_heals)
{
    CHECK(TypeOf(PopupEvent{287 + 6, kPartyMember, kMob, kDamageKind}) == PopupType::Skillchain);
    CHECK(TypeOf(PopupEvent{384 + 6, kMob, kMob, kRecoveryKind}) == PopupType::Skillchain);
    CHECK(TypeOf(PopupEvent{767, kYou, kMob, kDamageKind}) == PopupType::Skillchain);
}

TEST(a_skillchain_on_element_takes_its_elements_color_and_on_custom_the_custom_one)
{
    const auto shown = [](uint32_t message, const ColorPick& pick) {
        std::array<ColorPick, kPopupTypeCount> picks = kGamePicks;
        picks[static_cast<size_t>(PopupType::Skillchain)] = pick;
        const PopupEvent skillchain{message, kYou, kMob, kDamageKind, PopupType::Skillchain};
        return ShownCombatPopups({Popup("412", 0, 0, 54, 81, 0x40404040, 0.5f, skillchain)}, 100, picks, kOnTarget)[0].color;
    };
    CHECK_EQ(shown(287 + 6, ColorPick{ColorSource::Element}), ElementColor(Element::Fire));
    CHECK_EQ(shown(287 + 14, ColorPick{ColorSource::Element}), ElementColor(Element::Lightning));
    CHECK_EQ(shown(287 + 3, ColorPick{ColorSource::Element}), ElementColor(Element::Earth));
    CHECK_EQ(shown(768, ColorPick{ColorSource::Element}), ElementColor(Element::Dark));
    CHECK_EQ(shown(287 + 6, ColorPick{ColorSource::Custom, ColorFromRgb(0x123456)}), 0xFF123456u);
    CHECK_EQ(shown(287 + 6, ColorPick{ColorSource::Game}), 0xFF808080u);
}

TEST(with_skillchain_names_off_a_skillchain_is_the_games_plain_number)
{
    PopupSwitches off;
    off.skillchainNames = false;
    CHECK_EQ(CombatPopupWords("412", PopupEvent{287 + 6, 0, 0}, off), std::string("412"));
    CHECK_EQ(CombatPopupWords("900", PopupEvent{767, 0, 0}, off), std::string("900"));
}

TEST(the_test_run_gives_its_samples_one_at_a_time_350_ms_apart_and_starts_over_when_started_again)
{
    PopupTestRun run;
    CHECK(!run.Next(1.0));
    run.Start(1.0);
    const std::optional<PopupSample> first = run.Next(1.0);
    CHECK(first.has_value());
    CHECK(!run.Next(1.0));
    CHECK(!run.Next(1.349));
    const std::optional<PopupSample> second = run.Next(1.35);
    CHECK(second && second->value != first->value);
    run.Start(1.5);
    const std::optional<PopupSample> again = run.Next(1.5);
    CHECK(again && again->message == first->message && again->value == first->value);
}

TEST(the_test_run_ends_after_its_last_sample)
{
    PopupTestRun run;
    run.Start(0.0);
    size_t samples = 0;
    while (run.Next(1000.0)) ++samples;
    CHECK(samples > 0);
    CHECK(!run.Next(2000.0));
}

TEST(the_test_runs_samples_have_every_type_with_a_color_pick)
{
    PopupTestRun run;
    run.Start(0.0);
    PopupTies ties;
    ties.SetYou(kYou, 0);
    std::array<bool, kPopupTypeCount> seen{};
    while (const std::optional<PopupSample> sample = run.Next(1000.0))
        if (const std::optional<PopupType> type = TypeOf(ties, SampleEvent(*sample, kYou, kMob)))
            seen[static_cast<size_t>(*type)] = true;
    for (int t = 0; t < kPopupTypeCount; ++t)
        CHECK(seen[static_cast<size_t>(t)]);
}

TEST(a_sample_stands_over_you_done_by_you_or_by_another)
{
    const PopupSample yours{1, 128, kDamageKind, true}, theirs{1, 64, kDamageKind, false};
    CHECK(SampleEvent(yours, kYou, kMob) == (PopupEvent{1, kYou, kYou, kDamageKind}));
    CHECK(SampleEvent(theirs, kYou, kMob) == (PopupEvent{1, kMob, kYou, kDamageKind}));
}

namespace
{
    // The line a popup the game starts on target at now is moved down by, after the ones started before it.
    uint8_t LineOf(PopupTies& ties, const PopupEvent& event, double now, uintptr_t task)
    {
        ties.SetNow(now);
        ties.Decided(event);
        ties.Started(task);
        ties.Built(task + 1, task);
        return ties.Of(task + 1)->line;
    }

    uint8_t LineOf(PopupTies& ties, uint32_t target, double now, uintptr_t task)
    {
        return LineOf(ties, PopupEvent{1, kMob, target, kDamageKind}, now, task);
    }
}

TEST(popups_started_together_on_one_target_each_take_the_next_free_line)
{
    PopupTies ties;
    CHECK_EQ(LineOf(ties, kYou, 10.0, 0x1000), 0);
    CHECK_EQ(LineOf(ties, kYou, 10.0, 0x2000), 1);
    CHECK_EQ(LineOf(ties, kYou, 10.2, 0x3000), 2);
    CHECK_EQ(LineOf(ties, kMob, 10.2, 0x4000), 0);
}

TEST(popups_on_different_sides_of_one_target_spread_in_stacks_of_their_own)
{
    PopupTies ties;
    ties.SetYou(kYou, 0);
    PopupSides sides = Each(PopupSide::Right);
    sides[static_cast<size_t>(PopupType::Recovery)] = PopupSide::Left;
    ties.SetSides(sides);
    const PopupEvent hit{1, kMob, kYou, kDamageKind}, cure{7, kYou, kYou, kRecoveryKind};
    CHECK_EQ(LineOf(ties, hit, 10.0, 0x1000), 0);
    CHECK_EQ(LineOf(ties, cure, 10.0, 0x2000), 0);
    CHECK_EQ(LineOf(ties, hit, 10.1, 0x3000), 1);
    CHECK_EQ(LineOf(ties, cure, 10.1, 0x4000), 1);
    ties.SetSides(Each(PopupSide::Middle));
    CHECK_EQ(LineOf(ties, cure, 10.2, 0x5000), 2);
}

TEST(a_popups_line_is_free_again_once_the_popup_on_it_is_gone)
{
    PopupTies ties;
    CHECK_EQ(LineOf(ties, kYou, 10.0, 0x1000), 0);
    CHECK_EQ(LineOf(ties, kYou, 10.5, 0x2000), 1);
    CHECK_EQ(LineOf(ties, kYou, 10.0 + kPopupSpreadSeconds, 0x3000), 0);
    CHECK_EQ(LineOf(ties, kYou, 10.5 + kPopupSpreadSeconds, 0x4000), 1);
}

TEST(a_popup_on_a_later_line_is_drawn_that_many_lines_lower)
{
    PopupEvent event{1, kMob, kYou, kDamageKind, PopupType::Taken};
    event.line = 2;
    const ShownCombatPopup shown = ShownCombatPopups({Popup("64", 100, 200, 154, 281, 0x80404040, 0.5f, event)}, 50, kGamePicks, kOnTarget)[0];
    CHECK(Near(shown.center.x, 127.0f));
    CHECK(Near(shown.center.y, 240.5f + 2 * 40.5f));
}

TEST(an_early_call_says_no_to_the_chat_line_and_asks_the_game_about_the_popup)
{
    EarlyPopups early;
    uint16_t mark    = 0xFFFF;
    uint32_t message = 1;
    early.Begin(&mark, &message);
    CHECK_EQ(mark, 0);
    CHECK(early.Answer(false) == kPopupNo);
    CHECK(!early.Answer(true));
    early.End();
    CHECK(!early.Answer(false) && !early.Answer(true));
}

TEST(a_record_popped_early_skips_the_games_popup_once_and_keeps_its_line)
{
    EarlyPopups early;
    uint16_t popped = 0, missed = 0;
    uint32_t message = 1;
    early.Begin(&popped, &message);
    early.Started();
    early.End();
    early.Begin(&missed, &message);
    early.End();
    early.Started(); // a popup the game starts later, outside an early call, marks nothing
    CHECK_EQ(missed, 0);
    early.Show(&popped, &message);
    CHECK(early.Answer(true) == kPopupNo);
    CHECK(!early.Answer(false));
    early.Shown();
    CHECK(!early.Answer(true));
    early.Show(&popped, &message);
    CHECK(!early.Answer(true));
    early.Shown();
    early.Show(&missed, &message);
    CHECK(!early.Answer(true));
}

TEST(a_record_parsed_before_headsup_came_pops_as_the_game_pops_it)
{
    EarlyPopups early;
    for (uint16_t garbage : {uint16_t{0x0002}, uint16_t{0xFFFF}, uint16_t{0x5A58}})
    {
        uint16_t mark    = garbage;
        uint32_t message = 0;
        early.Show(&mark, &message);
        CHECK(!early.Answer(true));
        CHECK_EQ(mark, garbage);
        early.Shown();
    }
    early.Show(nullptr, nullptr);
    CHECK(!early.Answer(true));
}

TEST(with_show_right_away_off_nothing_starts_early_and_the_chat_line_is_the_games)
{
    EarlyPopups early;
    early.SetOn(false);
    uint16_t mark    = 0x1234;
    uint32_t message = 1;
    CHECK(!early.Begin(&mark, &message));
    CHECK_EQ(mark, 0x1234);
    CHECK(!early.Answer(false) && !early.Answer(true));
    early.SetOn(true);
    CHECK(early.Begin(&mark, &message));
    CHECK(early.Answer(false) == kPopupNo);
}

TEST(with_show_right_away_off_popups_started_together_keep_the_games_one_place)
{
    PopupTies ties;
    ties.SetSpread(false);
    CHECK_EQ(LineOf(ties, kYou, 10.0, 0x1000), 0);
    CHECK_EQ(LineOf(ties, kYou, 10.0, 0x2000), 0);
}

namespace
{
    // A popup the game draws 54 x 81 with its middle at 127,240.5, over a body whose game name's bottom is at row 140
    // and whose feet are at row 540: 400 pixels tall. Popups beside it start 90% of the way up from its feet, at row 180.
    constexpr PopupEvent kOnMob{1, kYou, kMob, kDamageKind, PopupType::Dealt};
    constexpr PopupBody kMobBody{140.0f, 540.0f};
    constexpr float kHeadHeight = 0.9f, kHeadRow = 180.0f;

    PopupPlacing Placing(const PopupSides& sides, std::optional<PopupBody> body, float spread = 0.0f, float nudge = 0.0f,
        float height = kHeadHeight)
    {
        return PopupPlacing{sides, height, spread, nudge, [body](uint32_t) { return body; }};
    }

    ShownCombatPopup Placed(PopupSide side, std::optional<PopupBody> body, uint8_t line = 0, float spread = 0.0f, float nudge = 0.0f)
    {
        PopupEvent event = kOnMob;
        event.line       = line;
        return ShownCombatPopups({Popup("128", 100, 200, 154, 281, 0x80404040, 0.5f, event)}, 100, kGamePicks,
            Placing(Each(side), body, spread, nudge))[0];
    }
}

TEST(a_popup_to_the_right_starts_half_its_bodys_height_right_of_the_games_place)
{
    const ShownCombatPopup right = Placed(PopupSide::Right, kMobBody);
    CHECK(Near(right.center.x, 127.0f + 200.0f) && Near(right.center.y, kHeadRow));
    CHECK_EQ(right.across, 0.0f);
    const ShownCombatPopup left = Placed(PopupSide::Left, kMobBody);
    CHECK(Near(left.center.x, 127.0f - 200.0f) && Near(left.center.y, kHeadRow));
    CHECK_EQ(left.across, 1.0f);
}

TEST(a_popup_beside_its_target_starts_at_its_share_of_the_body_up_from_the_feet_and_later_lines_go_down)
{
    const auto row = [](PopupSide side, float height, uint8_t line) {
        PopupEvent event = kOnMob;
        event.line       = line;
        return ShownCombatPopups({Popup("128", 100, 200, 154, 281, 0x80404040, 0.5f, event)}, 100, kGamePicks,
            Placing(Each(side), kMobBody, 0.0f, 0.0f, height))[0].center.y;
    };
    CHECK(Near(row(PopupSide::Right, 0.0f, 0), 540.0f));
    CHECK(Near(row(PopupSide::Left, 1.0f, 0), 140.0f));
    CHECK(Near(row(PopupSide::Right, 0.25f, 2), 440.0f + 2 * 81.0f));
    CHECK(Near(Placed(PopupSide::Left, PopupBody{140.0f, std::nullopt}).center.y, 240.5f));
}

TEST(each_kind_of_popup_goes_to_the_side_picked_for_it)
{
    PopupSides sides = Each(PopupSide::Middle);
    sides[static_cast<size_t>(PopupType::Dealt)]    = PopupSide::Right;
    sides[static_cast<size_t>(PopupType::Recovery)] = PopupSide::Left;
    const PopupEvent cure{7, kYou, kMob, kRecoveryKind, PopupType::Recovery}, miss{15, kYou, kMob, kMissKind, PopupType::Miss};
    const std::vector<ShownCombatPopup> shown = ShownCombatPopups({Popup("128", 100, 200, 154, 281, 0x80404040, 0.5f, kOnMob),
        Popup("34", 100, 200, 154, 281, 0x80404040, 0.5f, cure), Popup(":", 100, 200, 154, 281, 0x80404040, 0.5f, miss)},
        100, kGamePicks, Placing(sides, kMobBody));
    CHECK_EQ(shown.size(), size_t{3});
    CHECK(Near(shown[0].center.x, 327.0f) && shown[0].across == 0.0f);
    CHECK(Near(shown[1].center.x, -73.0f) && shown[1].across == 1.0f);
    CHECK(Near(shown[2].center.x, 127.0f) && Near(shown[2].center.y, kHeadRow) && shown[2].across == 0.5f);
}

TEST(plain_damage_between_others_goes_where_yours_dealt_goes_on_a_mob_and_where_yours_taken_goes_on_a_player)
{
    PopupSides sides = Each(PopupSide::Middle);
    sides[static_cast<size_t>(PopupType::Dealt)] = PopupSide::Right;
    sides[static_cast<size_t>(PopupType::Taken)] = PopupSide::Left;
    const auto across = [&](std::optional<PopupEvent> event) {
        return ShownCombatPopups({Popup("128", 100, 200, 154, 281, 0x80404040, 0.5f, event)}, 100, kGamePicks,
            Placing(sides, kMobBody))[0].across;
    };
    CHECK_EQ(across(PopupEvent{1, kPartyMember, kMob, kDamageKind}), 0.0f);
    CHECK_EQ(across(PopupEvent{1, kMob, kPartyMember, kDamageKind}), 1.0f);
    CHECK_EQ(across(PopupEvent{1, kMob, kYourPet, kDamageKind}), 1.0f);
    CHECK_EQ(across(std::nullopt), 0.0f);
}

TEST(a_popup_beside_a_body_whose_feet_are_not_known_clears_it_by_a_line)
{
    const ShownCombatPopup right = Placed(PopupSide::Right, PopupBody{140.0f, std::nullopt});
    CHECK(Near(right.center.x, 127.0f + 81.0f));
    CHECK(Near(Placed(PopupSide::Right, std::nullopt).center.x, 127.0f + 81.0f));
}

TEST(a_popup_in_the_middle_is_centered_on_its_target_at_the_start_height_and_later_lines_go_down)
{
    const ShownCombatPopup middle = Placed(PopupSide::Middle, kMobBody);
    CHECK(Near(middle.center.x, 127.0f) && Near(middle.center.y, kHeadRow));
    CHECK_EQ(middle.across, 0.5f);
    CHECK(Near(Placed(PopupSide::Middle, kMobBody, 2).center.y, kHeadRow + 2 * 81.0f));
    CHECK(Near(Placed(PopupSide::Middle, std::nullopt, 1).center.y, 240.5f + 81.0f));
}

TEST(the_spread_moves_the_side_columns_out_from_the_body_and_leaves_the_middle)
{
    CHECK(Near(Placed(PopupSide::Right, kMobBody, 0, 20.0f).center.x, 127.0f + 200.0f + 20.0f));
    CHECK(Near(Placed(PopupSide::Left, kMobBody, 0, 20.0f).center.x, 127.0f - 200.0f - 20.0f));
    CHECK(Near(Placed(PopupSide::Right, std::nullopt, 0, 20.0f).center.x, 127.0f + 81.0f + 20.0f));
    CHECK(Near(Placed(PopupSide::Middle, kMobBody, 0, 20.0f).center.x, 127.0f));
}

TEST(a_nudge_moves_a_popup_up_or_down_from_its_place)
{
    const ShownCombatPopup nudged = Placed(PopupSide::Right, kMobBody, 0, 0.0f, -20.0f);
    CHECK(Near(nudged.center.x, 327.0f) && Near(nudged.center.y, kHeadRow - 20.0f));
}

TEST(a_popups_outline_is_a_tenth_of_its_height_at_its_default_thickness_and_never_under_a_pixel)
{
    CHECK_EQ(CombatPopupEdge(40, kDefaultCombatPopupOutline), 4);
    CHECK_EQ(CombatPopupEdge(40, 200), 8);
    CHECK_EQ(CombatPopupEdge(40, 50), 2);
    CHECK_EQ(CombatPopupEdge(8, 25), 1);
}

TEST(a_popups_text_starts_or_ends_on_its_place_when_it_is_beside_its_target)
{
    const CombatTextSize text{40.0f, 30.0f, 3.0f, 5.0f, 19.0f, 24};
    const ScreenPoint center{100.4f, 200.6f};
    CHECK(PlaceCombatPopupText(center, 48.0f, text, 1.0f, 0.0f) == (SquareQuad{100.0f, 171.0f, 80.0f, 60.0f}));
    CHECK(PlaceCombatPopupText(center, 48.0f, text, 2.0f, 1.0f) == (SquareQuad{-60.0f, 171.0f, 160.0f, 60.0f}));
}

namespace
{
    // Statuses by Phoenix's ids (data/phoenix_statuses.tsv): Paralysis and Slow are debuffs, Protect a buff. Messages
    // by the server's msg.lua: 230 gains, 236 is, 237 receives.
    constexpr uint32_t kParalysis = 4, kSlow = 13, kProtect = 40, kNoStatus = 9999;
    constexpr uint32_t kYourId = 65001, kPartyMemberId = 65002, kStrangerId = 65003, kMobId = 17000000;

    // HeadsUp's answer and the kind it leaves to the game's decision for message with value from actor to target, each
    // an index then an id, after the game answered answer with kind.
    std::pair<uint8_t, uint8_t> Decision(const PopupTies& ties, std::array<uint32_t, 2> actor, std::array<uint32_t, 2> target,
        uint32_t message, uint32_t value, uint8_t answer = kPopupNo, uint8_t kind = 0)
    {
        const uint8_t said = ties.Decide(actor.data(), target.data(), message, value, answer, kind);
        return {said, kind};
    }

    PopupTies StatusTies()
    {
        PopupTies ties;
        ties.SetYou(kYou, kYourPet);
        const uint32_t party[] = {kYourId, kPartyMemberId};
        ties.SetParty(party);
        ties.SetStatuses(true);
        return ties;
    }
}

TEST(a_status_message_pops_a_debuff_as_damage_and_a_buff_as_recovery)
{
    CHECK_EQ(StatusPopupKind(236, kParalysis), kDamageKind);
    CHECK_EQ(StatusPopupKind(237, kSlow), kDamageKind);
    CHECK_EQ(StatusPopupKind(230, kProtect), kRecoveryKind);
    CHECK_EQ(StatusPopupKind(236, kNoStatus), 0);
    CHECK_EQ(StatusPopupKind(1, kParalysis), 0);
}

TEST(a_status_the_game_would_not_pop_pops_when_you_or_your_party_are_in_it)
{
    const PopupTies ties = StatusTies();
    const std::pair<uint8_t, uint8_t> pop{0, kDamageKind};
    CHECK(Decision(ties, {kPartyMember, kPartyMemberId}, {kMob, kMobId}, 236, kParalysis) == pop);
    CHECK(Decision(ties, {kMob, kMobId}, {kYou, kYourId}, 236, kParalysis) == pop);
    CHECK(Decision(ties, {kYourPet, 17000001}, {kMob, kMobId}, 236, kParalysis) == pop);
    CHECK(Decision(ties, {kYou, kYourId}, {kYou, kYourId}, 230, kProtect) == (std::pair<uint8_t, uint8_t>{0, kRecoveryKind}));
    const std::pair<uint8_t, uint8_t> no{kPopupNo, 0};
    CHECK(Decision(ties, {0x500, kStrangerId}, {kMob, kMobId}, 236, kParalysis) == no);
    CHECK(Decision(ties, {kPartyMember, kPartyMemberId}, {kMob, kMobId}, 236, kNoStatus) == no);
}

TEST(the_games_own_answer_stands_for_anything_but_a_status)
{
    const PopupTies ties = StatusTies();
    CHECK(Decision(ties, {kYou, kYourId}, {kMob, kMobId}, 1, 128, 0, kDamageKind) == (std::pair<uint8_t, uint8_t>{0, kDamageKind}));
    // Damage Display set to the log only: the game says no with the kind it would have popped.
    CHECK(Decision(ties, {kYou, kYourId}, {kMob, kMobId}, 1, 128, kPopupNo, kDamageKind) ==
          (std::pair<uint8_t, uint8_t>{kPopupNo, kDamageKind}));
    PopupTies off = StatusTies();
    off.SetStatuses(false);
    CHECK(Decision(off, {kYou, kYourId}, {kMob, kMobId}, 236, kParalysis) == (std::pair<uint8_t, uint8_t>{kPopupNo, 0}));
}

TEST(a_status_popup_says_the_statuss_name_and_is_a_buff_or_a_debuff)
{
    CHECK_EQ(CombatPopupWords("4", PopupEvent{236, kMob, kYou}), std::string("Paralysis"));
    CHECK_EQ(CombatPopupWords("40", PopupEvent{230, kYou, kYou}), std::string("Protect"));
    CHECK_EQ(CombatPopupWords("9999", PopupEvent{236, kMob, kYou}), std::string("9999"));
    CHECK_EQ(CombatPopupWords("40", PopupEvent{1, kYou, kMob}), std::string("40"));
    CHECK(TypeOf(PopupEvent{236, kYou, kMob, kDamageKind}) == PopupType::Debuff);
    CHECK(TypeOf(PopupEvent{230, kMob, kMob, kRecoveryKind}) == PopupType::Buff);
}

namespace
{
    // A target record as the game parsed it after an addon (SimpleLog) blanked its message, which the server sent as 230,
    // "gains the effect of", or as 327, "starts casting".
    constexpr uint32_t kGainsEffect = 230, kStartsCasting = 327;
}

TEST(a_status_message_an_addon_blanked_is_back_in_its_record_only_while_the_game_starts_its_popup)
{
    EarlyPopups early;
    early.SetRestore(true);
    uint16_t mark    = 0xFFFF;
    uint32_t message = 0;
    early.Parsed(&mark, &message, kGainsEffect);
    CHECK_EQ(message, 0u);
    CHECK(early.Begin(&mark, &message));
    CHECK_EQ(message, kGainsEffect);
    CHECK(early.Answer(false) == kPopupNo);
    CHECK(!early.Answer(true));
    early.End();
    CHECK_EQ(message, 0u);
    CHECK(!early.Answer(false));
}

TEST(with_show_right_away_off_the_games_own_start_gets_the_blanked_status_message_and_no_chat_line)
{
    EarlyPopups early;
    early.SetOn(false);
    early.SetRestore(true);
    uint16_t mark    = 0;
    uint32_t message = 0;
    early.Parsed(&mark, &message, kGainsEffect);
    CHECK(!early.Begin(&mark, &message));
    early.Show(&mark, &message);
    CHECK_EQ(message, kGainsEffect);
    CHECK(early.Answer(false) == kPopupNo);
    CHECK(!early.Answer(true));
    early.Shown();
    CHECK_EQ(message, 0u);
    CHECK(!early.Answer(false));
}

TEST(only_a_blanked_status_message_comes_back_and_only_while_statuses_pop)
{
    const auto restored = [](bool restore, uint32_t inRecord, uint32_t sent) {
        EarlyPopups early;
        early.SetOn(false);
        early.SetRestore(restore);
        uint16_t mark    = 0xFFFF;
        uint32_t message = inRecord;
        early.Parsed(&mark, &message, sent);
        early.Show(&mark, &message);
        const uint32_t during = message;
        early.Shown();
        CHECK_EQ(message, inRecord);
        return during;
    };
    CHECK_EQ(restored(true, 0, kGainsEffect), kGainsEffect);
    CHECK_EQ(restored(true, kGainsEffect, kGainsEffect), kGainsEffect); // no addon: the game's own message stands
    CHECK_EQ(restored(true, 0, kStartsCasting), 0u);
    CHECK_EQ(restored(true, 0, 0), 0u);
    CHECK_EQ(restored(false, 0, kGainsEffect), 0u);
}

TEST(a_restored_status_that_popped_early_is_skipped_at_the_hit_without_coming_back)
{
    EarlyPopups early;
    early.SetRestore(true);
    uint16_t mark    = 0;
    uint32_t message = 0;
    early.Parsed(&mark, &message, kGainsEffect);
    early.Begin(&mark, &message);
    early.Started();
    early.End();
    early.Show(&mark, &message);
    CHECK(early.Answer(true) == kPopupNo);
    CHECK_EQ(message, 0u);
    early.Shown();
}

namespace
{
    // An action packet (0x028) as the server packs it (0x028_battle2.cpp): fields of bits, each from its lowest, from
    // byte 5 on, the bits of each byte from its lowest.
    struct ActionPacket
    {
        std::vector<uint8_t> bytes = std::vector<uint8_t>(128, 0);
        uint32_t bit               = 40;
        ActionPacket& Put(uint64_t value, uint32_t bits)
        {
            for (uint32_t i = 0; i < bits; ++i, ++bit)
                if (value >> i & 1) bytes[bit / 8] |= static_cast<uint8_t>(1 << bit % 8);
            return *this;
        }
        ActionPacket& Head(uint32_t actor, uint32_t targets) { return Put(actor, 32).Put(targets, 6).Put(0, 4).Put(4, 4).Put(43, 32).Put(0, 32); }
        ActionPacket& Target(uint32_t id, uint32_t results) { return Put(id, 32).Put(results, 4); }
        // Miss, kind, animation, info, distortion and knockback; the value and message; the modifier; with an additional
        // effect and spikes or none.
        ActionPacket& Result(uint32_t value, uint32_t message, bool extras = false)
        {
            Put(0, 3).Put(0, 2).Put(0, 12).Put(0, 5).Put(0, 2).Put(0, 3).Put(value, 17).Put(message, 10).Put(0, 31);
            if (!extras) return Put(0, 1).Put(0, 1);
            return Put(1, 1).Put(0, 6).Put(0, 4).Put(5, 17).Put(163, 10).Put(1, 1).Put(0, 6).Put(0, 4).Put(9, 14).Put(374, 10);
        }
        uint32_t Size() const { return (bit + 7) / 8; }
    };
}

TEST(a_sent_action_has_a_message_for_each_result_of_a_lone_target)
{
    ActionPacket p;
    p.Head(65001, 1).Target(65001, 3).Result(40, 230, true).Result(12, 1).Result(0, 15);
    const std::optional<SentAction> sent = ParseSentAction(p.bytes.data(), p.Size());
    CHECK(sent && sent->actor == 65001 && sent->records == 3);
    CHECK(sent->messages[0] == 230 && sent->messages[1] == 1 && sent->messages[2] == 15);
}

TEST(a_sent_action_on_several_targets_has_each_ones_first_message)
{
    ActionPacket p;
    p.Head(65001, 2).Target(17000000, 2).Result(4, 236, true).Result(1, 1).Target(17000001, 1).Result(13, 237);
    const std::optional<SentAction> sent = ParseSentAction(p.bytes.data(), p.Size());
    CHECK(sent && sent->records == 2 && sent->messages[0] == 236 && sent->messages[1] == 237);
}

TEST(a_sent_action_cut_short_is_none)
{
    ActionPacket p;
    p.Head(65001, 1).Target(65001, 2).Result(40, 230);
    CHECK(!ParseSentAction(p.bytes.data(), p.Size()));
    CHECK(!ParseSentAction(p.bytes.data(), 6));
}

TEST(the_sent_actions_go_to_their_parses_oldest_first_by_actor_and_records)
{
    SentActions sent;
    sent.Put(SentAction{65001, 1, {230}});
    sent.Put(SentAction{65002, 1, {236}});
    sent.Put(SentAction{65001, 1, {237}});
    CHECK(!sent.Take(65001, 2));
    std::optional<SentAction> first = sent.Take(65001, 1);
    CHECK(first && first->messages[0] == 230);
    std::optional<SentAction> second = sent.Take(65001, 1);
    CHECK(second && second->messages[0] == 237);
    CHECK(!sent.Take(65001, 1));
    sent.Clear();
    CHECK(!sent.Take(65002, 1));
}

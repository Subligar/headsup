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
    const std::vector<ShownCombatPopup> shown = ShownCombatPopups(kMissTwins, 100, kGamePicks);
    CHECK_EQ(shown.size(), size_t{1});
    CHECK(shown[0].words == "Miss");
    CHECK_EQ(shown[0].color, 0xFF808080u);
    CHECK_EQ(shown[0].alpha, 0xFFu);
    CHECK(Near(shown[0].center.x, 1924.0444f) && Near(shown[0].center.y, 222.49231f));
    CHECK(Near(shown[0].lineHeight, 81.0f));
    CHECK_EQ(shown[0].depth, 0.5f);
    CHECK(Near(ShownCombatPopups(kMissTwins, 50, kGamePicks)[0].lineHeight, 40.5f));
}

TEST(popups_with_the_same_text_far_apart_or_other_texts_on_one_box_stay_apart)
{
    const std::vector<CombatPopup> apart = {Popup("12", 1000, 200, 1108, 281, 0x80404040, 0.5f),
        Popup("12", 1200, 200, 1308, 281, 0x80404040, 0.5f)};
    CHECK_EQ(ShownCombatPopups(apart, 100, kGamePicks).size(), size_t{2});
    const std::vector<CombatPopup> others = {Popup("12", 1000, 200, 1108, 281, 0x80404040, 0.5f),
        Popup("21", 1000, 200, 1108, 281, 0x80404040, 0.5f)};
    const std::vector<ShownCombatPopup> shown = ShownCombatPopups(others, 100, kGamePicks);
    CHECK(shown.size() == 2 && shown[0].words == "12" && shown[1].words == "21");
}

TEST(a_popups_color_shows_twice_as_bright_and_its_alpha_twice_as_strong_up_to_opaque)
{
    const std::vector<ShownCombatPopup> half = ShownCombatPopups({Popup("7", 0, 0, 54, 81, 0x40404040, 0.5f)}, 100, kGamePicks);
    CHECK(half.size() == 1 && half[0].color == 0xFF808080u && half[0].alpha == 0x80u);
    CHECK_EQ(ShownCombatPopups({Popup("7", 0, 0, 54, 81, 0xFF404040, 0.5f)}, 100, kGamePicks)[0].alpha, 0xFFu);
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
    CHECK(ShownCombatPopups({parry}, 100, kGamePicks)[0].words == "Parry");
}

TEST(a_popups_text_is_centered_across_with_its_capitals_middle_on_the_popups_middle)
{
    // Drawn 24 px tall in a 40 x 30 image with a 3 px halo, its capitals from row 5 to row 19 under the halo. Shown in a
    // 48 px line each texture pixel is 2 image pixels, so the capitals' middle, 3 + 12 rows down, is 30 pixels down.
    const CombatTextSize text{40.0f, 30.0f, 3.0f, 5.0f, 19.0f, 24};
    const ScreenPoint center{100.4f, 200.6f};
    CHECK(PlaceCombatPopupText(center, 48.0f, text, 1.0f) == (SquareQuad{60.0f, 171.0f, 80.0f, 60.0f}));
    CHECK(PlaceCombatPopupText(center, 48.0f, text, 2.0f) == (SquareQuad{20.0f, 171.0f, 160.0f, 60.0f}));
}

TEST(the_draw_dump_lines_hold_a_popups_text_color_place_and_depth)
{
    const CombatPopup miss = Popup(":", kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0x80404040, 0.99876f);
    CHECK_EQ(DescribeCombatPopup(miss), std::string(": | 80404040 | 1874.54,181.99 to 1973.54,262.99 | 0.99876 | message -"));
    const CombatPopup parry = Popup(":", kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0x80404040, 0.99876f, PopupEvent{70, 0, 0});
    CHECK_EQ(DescribeCombatPopup(parry), std::string(": | 80404040 | 1874.54,181.99 to 1973.54,262.99 | 0.99876 | message 70"));
    const ShownCombatPopup shown = ShownCombatPopups({miss}, 100, kGamePicks)[0];
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
    const ShownCombatPopup custom = ShownCombatPopups({popup}, 100, picks)[0];
    CHECK_EQ(custom.color, 0xFFFF0000u);
    CHECK_EQ(custom.alpha, 0x80u);
    picks[static_cast<size_t>(PopupType::Critical)].source = ColorSource::Game;
    CHECK_EQ(ShownCombatPopups({popup}, 100, picks)[0].color, 0xFF808080u);
}

TEST(a_popup_with_no_type_keeps_the_games_color_whatever_is_picked)
{
    const auto shown = [](std::optional<PopupEvent> event) {
        return ShownCombatPopups({Popup("128", 0, 0, 54, 81, 0x40404040, 0.5f, event)}, 100, Picks(ColorPick{ColorSource::Custom, ColorFromRgb(0x00FF00)}))[0].color;
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
    CHECK_EQ(ShownCombatPopups({popup}, 100, picks, off)[0].color, 0xFF808080u);
    CHECK_EQ(ShownCombatPopups({popup}, 100, picks)[0].color, 0xFFFF0000u);
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
        return ShownCombatPopups({Popup("412", 0, 0, 54, 81, 0x40404040, 0.5f, skillchain)}, 100, picks)[0].color;
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
    uint8_t LineOf(PopupTies& ties, uint32_t target, double now, uintptr_t task)
    {
        ties.SetNow(now);
        ties.Decided(PopupEvent{1, kMob, target, kDamageKind});
        ties.Started(task);
        ties.Built(task + 1, task);
        return ties.Of(task + 1)->line;
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
    const ShownCombatPopup shown = ShownCombatPopups({Popup("64", 100, 200, 154, 281, 0x80404040, 0.5f, event)}, 50, kGamePicks)[0];
    CHECK(Near(shown.center.x, 127.0f));
    CHECK(Near(shown.center.y, 240.5f + 2 * 40.5f));
}

TEST(an_early_call_says_no_to_the_chat_line_and_asks_the_game_about_the_popup)
{
    EarlyPopups early;
    uint16_t mark = 0xFFFF;
    early.Begin(&mark);
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
    early.Begin(&popped);
    early.Started();
    early.End();
    early.Begin(&missed);
    early.End();
    early.Started(); // a popup the game starts later, outside an early call, marks nothing
    CHECK_EQ(missed, 0);
    early.Show(&popped);
    CHECK(early.Answer(true) == kPopupNo);
    CHECK(!early.Answer(false));
    early.Shown();
    CHECK(!early.Answer(true));
    early.Show(&popped);
    CHECK(!early.Answer(true));
    early.Shown();
    early.Show(&missed);
    CHECK(!early.Answer(true));
}

TEST(a_record_parsed_before_headsup_came_pops_as_the_game_pops_it)
{
    EarlyPopups early;
    for (uint16_t garbage : {uint16_t{0x0002}, uint16_t{0xFFFF}, uint16_t{0x5A58}})
    {
        uint16_t mark = garbage;
        early.Show(&mark);
        CHECK(!early.Answer(true));
        CHECK_EQ(mark, garbage);
        early.Shown();
    }
    early.Show(nullptr);
    CHECK(!early.Answer(true));
}

TEST(with_show_right_away_off_nothing_starts_early_and_the_chat_line_is_the_games)
{
    EarlyPopups early;
    early.SetOn(false);
    uint16_t mark = 0x1234;
    CHECK(!early.Begin(&mark));
    CHECK_EQ(mark, 0x1234);
    CHECK(!early.Answer(false) && !early.Answer(true));
    early.SetOn(true);
    CHECK(early.Begin(&mark));
    CHECK(early.Answer(false) == kPopupNo);
}

TEST(with_show_right_away_off_popups_started_together_keep_the_games_one_place)
{
    PopupTies ties;
    ties.SetSpread(false);
    CHECK_EQ(LineOf(ties, kYou, 10.0, 0x1000), 0);
    CHECK_EQ(LineOf(ties, kYou, 10.0, 0x2000), 0);
}

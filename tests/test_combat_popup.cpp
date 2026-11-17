#include "boxes.h"
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
    const std::vector<ShownCombatPopup> shown = ShownCombatPopups(kMissTwins, 100);
    CHECK_EQ(shown.size(), size_t{1});
    CHECK(shown[0].words == "Miss");
    CHECK_EQ(shown[0].color, 0xFF808080u);
    CHECK_EQ(shown[0].alpha, 0xFFu);
    CHECK(Near(shown[0].center.x, 1924.0444f) && Near(shown[0].center.y, 222.49231f));
    CHECK(Near(shown[0].lineHeight, 81.0f));
    CHECK_EQ(shown[0].depth, 0.5f);
    CHECK(Near(ShownCombatPopups(kMissTwins, 50)[0].lineHeight, 40.5f));
}

TEST(popups_with_the_same_text_far_apart_or_other_texts_on_one_box_stay_apart)
{
    const std::vector<CombatPopup> apart = {Popup("12", 1000, 200, 1108, 281, 0x80404040, 0.5f),
        Popup("12", 1200, 200, 1308, 281, 0x80404040, 0.5f)};
    CHECK_EQ(ShownCombatPopups(apart, 100).size(), size_t{2});
    const std::vector<CombatPopup> others = {Popup("12", 1000, 200, 1108, 281, 0x80404040, 0.5f),
        Popup("21", 1000, 200, 1108, 281, 0x80404040, 0.5f)};
    const std::vector<ShownCombatPopup> shown = ShownCombatPopups(others, 100);
    CHECK(shown.size() == 2 && shown[0].words == "12" && shown[1].words == "21");
}

TEST(a_popups_color_shows_twice_as_bright_and_its_alpha_twice_as_strong_up_to_opaque)
{
    const std::vector<ShownCombatPopup> half = ShownCombatPopups({Popup("7", 0, 0, 54, 81, 0x40404040, 0.5f)}, 100);
    CHECK(half.size() == 1 && half[0].color == 0xFF808080u && half[0].alpha == 0x80u);
    CHECK_EQ(ShownCombatPopups({Popup("7", 0, 0, 54, 81, 0xFF404040, 0.5f)}, 100)[0].alpha, 0xFFu);
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

TEST(a_shown_miss_has_its_messages_word)
{
    const CombatPopup parry = Popup(":", kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0x80404040, 0.5f, PopupEvent{70, 0, 0});
    CHECK(ShownCombatPopups({parry}, 100)[0].words == "Parry");
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
    const ShownCombatPopup shown = ShownCombatPopups({miss}, 100)[0];
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
    CHECK(ties.Of(kParticle2) == (PopupEvent{2, 0, 0}));
    CHECK(ties.Of(kParticle2 + 1) == (PopupEvent{65, 0, 0}));
}

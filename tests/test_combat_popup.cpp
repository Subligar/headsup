#include "boxes.h"
#include "combat_popup.h"
#include "screen_vertex.h"
#include "test.h"

#include <array>
#include <string_view>

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
}

TEST(every_quad_inside_a_popups_draw_widens_its_box_and_the_last_gives_its_depth_and_color)
{
    CombatPopupList list;
    list.Begin(":");
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
}

TEST(three_figures_side_by_side_are_one_popup_as_wide_as_all_three)
{
    CombatPopupList list;
    list.Begin("123");
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
    list.Begin("12");
    list.End();
    Add(list, Quad(kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0.5f, 0x80404040));
    CHECK(list.Popups().empty());
}

TEST(a_popup_keeps_at_most_the_games_sixteen_bytes_of_text)
{
    CombatPopupList list;
    list.Begin("12345678901234567890");
    Add(list, Quad(kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0.5f, 0x80404040));
    list.End();
    CHECK(list.Popups().size() == 1 && std::string_view(list.Popups().front().text) == "1234567890123456");
}

TEST(a_new_frame_forgets_every_popup_and_closes_an_open_one)
{
    CombatPopupList list;
    list.Begin("7");
    Add(list, Quad(kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0.5f, 0x80404040));
    list.NewFrame();
    CHECK(list.Popups().empty() && !list.Open());
    Add(list, Quad(kMissLeft, kMissTop1, kMissRight, kMissBottom1, 0.5f, 0x80404040));
    CHECK(list.Popups().empty());
}

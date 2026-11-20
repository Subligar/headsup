#include "bar_look.h"
#include "bar_document.h"
#include "notice_box.h"
#include "skin_frame.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <cmath>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    Rml::Element* Box(test::RmlSession& rml, int n)
    {
        return rml.context->GetDocument(kNoticeDocument)->GetElementById(NoticeId(n));
    }
    Rml::Element* Part(Rml::Element* box, const char* name)
    {
        Rml::ElementList found;
        box->GetElementsByClassName(found, name);
        return found.empty() ? nullptr : found.front();
    }
    float Top(Rml::Element* el) { return el->GetAbsoluteOffset(Rml::BoxArea::Border).y; }
    float Left(Rml::Element* el) { return el->GetAbsoluteOffset(Rml::BoxArea::Border).x; }
    float Height(Rml::Element* el) { return el->GetBox().GetSize(Rml::BoxArea::Border).y; }
    ShownNotice N(uint32_t id, NoticeKind kind, const char* name, float opacity = 1.0f, float share = 1.0f)
    {
        return ShownNotice{id, kind, name, opacity, share};
    }
    constexpr float kBarBottom = 45.0f;
    const BarPalette& Gold() { return PaletteOf(BarTheme::PhoenixGold); }
}

TEST(a_notice_shows_its_friend_in_a_box_300_wide_under_the_panel)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    CHECK(!box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "Two<b>&", 1.0f, 0.5f)}, false,
        kBarBottom, 0.0f, std::nullopt));
    rml.context->Update();
    Rml::Element* first = Box(rml, 0);
    CHECK_EQ(first->GetBox().GetSize(Rml::BoxArea::Border).x, 300.0f);
    CHECK_EQ(Left(first), 10.0f);
    CHECK_EQ(Top(first), 55.0f);
    // The name is text, not markup.
    CHECK(Part(first, "message")->GetInnerRML().find("Two&lt;b&gt;&amp;") != std::string::npos);
    const float inside = Part(first, "bar")->GetParentNode()->GetBox().GetSize(Rml::BoxArea::Content).x;
    CHECK(std::abs(Part(first, "bar")->GetBox().GetSize(Rml::BoxArea::Border).x - inside / 2.0f) < 1.0f);
    CHECK(Box(rml, 1)->IsClassSet(kHiddenClass));
    const Rml::Colourb title = Part(first, "title")->GetComputedValues().color();
    CHECK(title.red == ((kOnlineColor >> 16) & 0xFF) && title.green == ((kOnlineColor >> 8) & 0xFF) &&
          title.blue == (kOnlineColor & 0xFF));
    CHECK_EQ(rml.Said(), "");
    box.Close();
}

TEST(notices_stack_ten_pixels_apart_and_slide_up_when_one_goes)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    box.Update(rml.context, Gold(), settings,
        {N(1, NoticeKind::Online, "One"), N(2, NoticeKind::Offline, "Two"), N(3, NoticeKind::Online, "Three")}, false,
        kBarBottom, 0.0f, std::nullopt);
    rml.context->Update();
    CHECK(std::abs(Top(Box(rml, 1)) - (Top(Box(rml, 0)) + Height(Box(rml, 0)) + 10.0f)) < 0.5f);
    CHECK(std::abs(Top(Box(rml, 2)) - (Top(Box(rml, 1)) + Height(Box(rml, 1)) + 10.0f)) < 0.5f);
    const float twoWas = Top(Box(rml, 1));
    // One goes: the box holding Two slides toward the top, it does not jump.
    box.Update(rml.context, Gold(), settings, {N(2, NoticeKind::Offline, "Two"), N(3, NoticeKind::Online, "Three")},
        false, kBarBottom, 0.016f, std::nullopt);
    rml.context->Update();
    CHECK(Part(Box(rml, 0), "message")->GetInnerRML().find("Two") != std::string::npos);
    CHECK(Top(Box(rml, 0)) < twoWas && Top(Box(rml, 0)) > 55.0f);
    CHECK(Box(rml, 2)->IsClassSet(kHiddenClass));
    box.Update(rml.context, Gold(), settings, {N(2, NoticeKind::Offline, "Two"), N(3, NoticeKind::Online, "Three")},
        false, kBarBottom, 1.0f, std::nullopt);
    rml.context->Update();
    CHECK_EQ(Top(Box(rml, 0)), 55.0f);
    CHECK_EQ(rml.Said(), "");
    box.Close();
}

TEST(stack_upward_puts_the_oldest_on_the_spot_and_newer_ones_above)
{
    test::RmlSession rml;
    Settings settings;
    settings.friendNoticeUpward = true;
    settings.friendNoticeAcross = 2500;
    settings.friendNoticeDown   = 5000;
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One"), N(2, NoticeKind::Online, "Two")},
        false, kBarBottom, 0.0f, std::nullopt);
    rml.context->Update();
    CHECK(std::abs(Top(Box(rml, 0)) + Height(Box(rml, 0)) - 540.0f) < 0.5f);
    CHECK(std::abs(Top(Box(rml, 1)) + Height(Box(rml, 1)) + 10.0f - Top(Box(rml, 0))) < 0.5f);
    CHECK(Left(Box(rml, 0)) == 480.0f && Left(Box(rml, 1)) == 480.0f);
    box.Close();
}

TEST(a_stack_switched_to_upward_at_the_top_of_the_screen_stays_on_it)
{
    test::RmlSession rml;
    Settings settings;
    settings.friendNoticeUpward = true;
    settings.friendNoticeAcross = 2500;
    settings.friendNoticeDown   = 0;
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One")}, false, kBarBottom, 0.0f, std::nullopt);
    rml.context->Update();
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One")}, false, kBarBottom, 0.0f, std::nullopt);
    rml.context->Update();
    CHECK(Top(Box(rml, 0)) >= 0.0f);
    box.Close();
}

TEST(a_notice_fades_and_hides_its_bar_when_the_countdown_bar_is_off)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One", 0.25f)}, false, kBarBottom, 0.0f, std::nullopt);
    rml.context->Update();
    CHECK(std::abs(Box(rml, 0)->GetComputedValues().opacity() - 0.25f) < 0.001f);
    const float withBar = Height(Box(rml, 0));
    CHECK(!Part(Box(rml, 0), "bar")->IsClassSet(kHiddenClass));
    settings.friendNoticeBar = false;
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One", 0.25f)}, false, kBarBottom, 0.0f, std::nullopt);
    rml.context->Update();
    CHECK(Part(Box(rml, 0), "bar")->IsClassSet(kHiddenClass));
    CHECK(Height(Box(rml, 0)) < withBar && Height(Box(rml, 0)) >= withBar - kNoticeBarHeight - 10.0f);
    CHECK_EQ(rml.Said(), "");
    box.Close();
}

TEST(clicks_pass_through_a_notice_and_move_lets_you_drag_the_stack)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    const std::vector<ShownNotice> shown = {N(1, NoticeKind::Online, "One")};
    box.Update(rml.context, Gold(), settings, shown, false, kBarBottom, 0.0f, std::nullopt);
    rml.context->Update();
    Rml::Element* under = rml.context->GetElementAtPoint({20.0f, 65.0f});
    CHECK(under == nullptr || under->GetOwnerDocument() != rml.context->GetDocument(kNoticeDocument) ||
          under == rml.context->GetDocument(kNoticeDocument));
    box.Update(rml.context, Gold(), settings, shown, true, kBarBottom, 0.0f, std::nullopt);
    rml.context->Update();
    rml.context->ProcessMouseMove(20, 65, 0);
    rml.context->ProcessMouseButtonDown(0, 0);
    for (int step = 1; step <= 4; ++step)
    {
        // The pointer moves by (480, 540) in all: the stack from (10, 55) lands at (490, 595).
        rml.context->ProcessMouseMove(20 + 120 * step, 65 + 135 * step, 0);
        rml.context->Update();
    }
    rml.context->ProcessMouseButtonUp(0, 0);
    CHECK(box.Update(rml.context, Gold(), settings, shown, true, kBarBottom, 0.0f, std::nullopt));
    CHECK_EQ(settings.friendNoticeAcross, 2552); // 490 / 1920
    CHECK_EQ(settings.friendNoticeDown, 5509);   // 595 / 1080
    CHECK(!box.Update(rml.context, Gold(), settings, shown, true, kBarBottom, 0.0f, std::nullopt));
    rml.context->Update();
    CHECK(std::abs(Left(Box(rml, 0)) - 490.0f) < 0.5f && std::abs(Top(Box(rml, 0)) - 595.0f) < 0.5f);
    CHECK_EQ(rml.Said(), "");
    box.Close();
}

TEST(the_box_is_built_again_when_the_theme_changes_and_goes_when_closed)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    const std::vector<ShownNotice> shown = {N(1, NoticeKind::Online, "One")};
    box.Update(rml.context, Gold(), settings, shown, false, kBarBottom, 0.0f, std::nullopt);
    rml.context->Update();
    box.Update(rml.context, PaletteOf(static_cast<BarTheme>(kFixedThemeCount - 1)), settings, shown, false, kBarBottom,
        0.0f, std::nullopt);
    rml.context->Update();
    CHECK(rml.context->GetDocument(kNoticeDocument) != nullptr && !Box(rml, 0)->IsClassSet(kHiddenClass));
    CHECK(!box.Update(rml.context, Gold(), settings, {}, false, kBarBottom, 0.0f, std::nullopt));
    rml.context->Update();
    for (int n = 0; n < kMaxNotices; ++n) CHECK(Box(rml, n)->IsClassSet(kHiddenClass));
    box.Close();
    CHECK(rml.context->GetDocument(kNoticeDocument) == nullptr);
    CHECK_EQ(rml.Said(), "");
}

TEST(a_game_theme_draws_each_notice_in_the_games_window_art)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    const std::vector<ShownNotice> shown = {N(1, NoticeKind::Online, "One")};
    box.Update(rml.context, Gold(), settings, shown, false, kBarBottom, 0.0f, std::nullopt);
    rml.context->Update();
    CHECK(Part(Box(rml, 0), kArtClass) == nullptr);
    // A Window Type's art in its frame's color, 3 px: the box takes the art's border and a piece of art behind its
    // text, as the panel's popups do.
    const SkinFrame frame{2, 1.0f, 0.5f, 1.0f, 0xFF102030, 3};
    box.Update(rml.context, Gold(), settings, shown, false, kBarBottom, 0.0f, frame);
    rml.context->Update();
    Rml::Element* art = Part(Box(rml, 0), kArtClass);
    CHECK(art != nullptr);
    CHECK_EQ(art->GetBox().GetSize(Rml::BoxArea::Border).x, Box(rml, 0)->GetBox().GetSize(Rml::BoxArea::Padding).x);
    CHECK_EQ(Box(rml, 0)->GetBox().GetEdge(Rml::BoxArea::Border, Rml::BoxEdge::Left), 3.0f);
    CHECK(Part(Box(rml, 0), "message")->GetInnerRML().find("One") != std::string::npos);
    box.Close();
}

TEST(letting_go_of_shift_ends_the_drag_where_the_stack_is)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    const std::vector<ShownNotice> shown = {N(1, NoticeKind::Online, "One")};
    box.Update(rml.context, Gold(), settings, shown, true, kBarBottom, 0.0f, std::nullopt);
    rml.context->Update();
    rml.context->ProcessMouseMove(20, 65, 0);
    rml.context->ProcessMouseButtonDown(0, 0);
    for (int step = 1; step <= 2; ++step)
    {
        rml.context->ProcessMouseMove(20 + 120 * step, 65 + 135 * step, 0); // to (260, 335): the stack at (250, 325)
        rml.context->Update();
    }
    // Shift let go: the drag stops there, and the pointer moving on with the button still down takes it no further.
    box.Update(rml.context, Gold(), settings, shown, false, kBarBottom, 0.0f, std::nullopt);
    for (int step = 3; step <= 4; ++step)
    {
        rml.context->ProcessMouseMove(20 + 120 * step, 65 + 135 * step, 0);
        rml.context->Update();
    }
    rml.context->ProcessMouseButtonUp(0, 0);
    box.Update(rml.context, Gold(), settings, shown, false, kBarBottom, 0.0f, std::nullopt);
    CHECK_EQ(settings.friendNoticeAcross, SpotShare(250.0f, 1920.0f));
    CHECK_EQ(settings.friendNoticeDown, SpotShare(325.0f, 1080.0f));
    rml.context->Update();
    CHECK(std::abs(Left(Box(rml, 0)) - 250.0f) < 0.5f && std::abs(Top(Box(rml, 0)) - 325.0f) < 0.5f);
    box.Close();
}

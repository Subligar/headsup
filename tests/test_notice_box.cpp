#include "bar_look.h"
#include "fonts.h"
#include "bar_document.h"
#include "notice_box.h"
#include "skin_frame.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <algorithm>
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
    float Width(Rml::Element* el) { return el->GetBox().GetSize(Rml::BoxArea::Border).x; }
    // The panel's fonts as the plugin picks them, from what this session loaded: the bundled fonts alone.
    PanelFonts Fonts(const Settings& settings)
    {
        const std::vector<std::string> bundled = BundledFamilies();
        return PanelFontsOf(settings, Gold().shape,
            [&](const std::string& font) { return std::ranges::find(bundled, font) != bundled.end(); });
    }
    NoticeFrame Frame(bool moving = false, float seconds = 0.0f, std::optional<SkinFrame> skin = std::nullopt,
        const Settings& settings = Settings{})
    {
        return NoticeFrame{moving, kBarBottom, seconds, std::nullopt, skin, Fonts(settings)};
    }
}

TEST(a_notice_is_drawn_in_the_themes_fonts_when_the_panels_cannot_be_loaded)
{
    // Bahnschrift, a new install's panel font, comes with Windows and not with Wine; this session has it neither.
    test::RmlSession rml;
    Settings settings;
    settings.barTitleFont = settings.barTextFont = "Bahnschrift";
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::PartyInvite, "Two")}, Frame(false, 0.0f, std::nullopt, settings));
    rml.context->Update();
    CHECK_EQ(rml.Said(), "");
    box.Close();
}

TEST(a_notice_shows_its_friend_in_a_box_300_wide_under_the_panel)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    CHECK(!box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "Two<b>&", 1.0f, 0.5f)}, Frame()).moved);
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
        {N(1, NoticeKind::Online, "One"), N(2, NoticeKind::Offline, "Two"), N(3, NoticeKind::Online, "Three")}, Frame());
    rml.context->Update();
    CHECK(std::abs(Top(Box(rml, 1)) - (Top(Box(rml, 0)) + Height(Box(rml, 0)) + 10.0f)) < 0.5f);
    CHECK(std::abs(Top(Box(rml, 2)) - (Top(Box(rml, 1)) + Height(Box(rml, 1)) + 10.0f)) < 0.5f);
    const float twoWas = Top(Box(rml, 1));
    // One goes: the box holding Two slides toward the top, it does not jump.
    box.Update(rml.context, Gold(), settings, {N(2, NoticeKind::Offline, "Two"), N(3, NoticeKind::Online, "Three")},
        Frame(false, 0.016f));
    rml.context->Update();
    CHECK(Part(Box(rml, 0), "message")->GetInnerRML().find("Two") != std::string::npos);
    CHECK(Top(Box(rml, 0)) < twoWas && Top(Box(rml, 0)) > 55.0f);
    CHECK(Box(rml, 2)->IsClassSet(kHiddenClass));
    box.Update(rml.context, Gold(), settings, {N(2, NoticeKind::Offline, "Two"), N(3, NoticeKind::Online, "Three")},
        Frame(false, 1.0f));
    rml.context->Update();
    CHECK_EQ(Top(Box(rml, 0)), 55.0f);
    CHECK_EQ(rml.Said(), "");
    box.Close();
}

TEST(the_bottom_edge_puts_the_first_notice_on_the_spot_and_the_next_above)
{
    test::RmlSession rml;
    Settings settings;
    settings.noticeEdge   = NoticeEdge::Bottom;
    settings.noticeAcross = 2500;
    settings.noticeDown   = 5000;
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One"), N(2, NoticeKind::Online, "Two")},
        Frame());
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
    settings.noticeEdge   = NoticeEdge::Bottom;
    settings.noticeAcross = 2500;
    settings.noticeDown   = 0;
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One")}, Frame());
    rml.context->Update();
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One")}, Frame());
    rml.context->Update();
    CHECK(Top(Box(rml, 0)) >= 0.0f);
    box.Close();
}

TEST(a_notice_fades_and_hides_its_bar_when_the_countdown_bar_is_off)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One", 0.25f)}, Frame());
    rml.context->Update();
    CHECK(std::abs(Box(rml, 0)->GetComputedValues().opacity() - 0.25f) < 0.001f);
    const float withBar = Height(Box(rml, 0));
    CHECK(!Part(Box(rml, 0), "bar")->IsClassSet(kHiddenClass));
    settings.noticeBar = false;
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One", 0.25f)}, Frame());
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
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    Rml::Element* under = rml.context->GetElementAtPoint({20.0f, 65.0f});
    CHECK(under == nullptr || under->GetOwnerDocument() != rml.context->GetDocument(kNoticeDocument) ||
          under == rml.context->GetDocument(kNoticeDocument));
    box.Update(rml.context, Gold(), settings, shown, Frame(true));
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
    CHECK(box.Update(rml.context, Gold(), settings, shown, Frame(true)).moved);
    CHECK_EQ(settings.noticeAcross, 2552); // 490 / 1920
    CHECK_EQ(settings.noticeDown, 5509);   // 595 / 1080
    CHECK(!box.Update(rml.context, Gold(), settings, shown, Frame(true)).moved);
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
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    box.Update(rml.context, PaletteOf(static_cast<BarTheme>(kFixedThemeCount - 1)), settings, shown, Frame());
    rml.context->Update();
    CHECK(rml.context->GetDocument(kNoticeDocument) != nullptr && !Box(rml, 0)->IsClassSet(kHiddenClass));
    CHECK(!box.Update(rml.context, Gold(), settings, {}, Frame()).moved);
    rml.context->Update();
    for (int n = 0; n < kNoticeBoxes; ++n) CHECK(Box(rml, n)->IsClassSet(kHiddenClass));
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
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    CHECK(Part(Box(rml, 0), kArtClass) == nullptr);
    // A Window Type's art in its frame's color, 3 px: the box takes the art's border and a piece of art behind its
    // text, as the panel's popups do.
    const SkinFrame frame{2, 1.0f, 0.5f, 1.0f, 0xFF102030, 3};
    box.Update(rml.context, Gold(), settings, shown, Frame(false, 0.0f, frame));
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
    box.Update(rml.context, Gold(), settings, shown, Frame(true));
    rml.context->Update();
    rml.context->ProcessMouseMove(20, 65, 0);
    rml.context->ProcessMouseButtonDown(0, 0);
    for (int step = 1; step <= 2; ++step)
    {
        rml.context->ProcessMouseMove(20 + 120 * step, 65 + 135 * step, 0); // to (260, 335): the stack at (250, 325)
        rml.context->Update();
    }
    // Shift let go: the drag stops there, and the pointer moving on with the button still down takes it no further.
    box.Update(rml.context, Gold(), settings, shown, Frame());
    for (int step = 3; step <= 4; ++step)
    {
        rml.context->ProcessMouseMove(20 + 120 * step, 65 + 135 * step, 0);
        rml.context->Update();
    }
    rml.context->ProcessMouseButtonUp(0, 0);
    box.Update(rml.context, Gold(), settings, shown, Frame());
    CHECK_EQ(settings.noticeAcross, SpotShare(250.0f, 1920.0f));
    CHECK_EQ(settings.noticeDown, SpotShare(325.0f, 1080.0f));
    rml.context->Update();
    CHECK(std::abs(Left(Box(rml, 0)) - 250.0f) < 0.5f && std::abs(Top(Box(rml, 0)) - 325.0f) < 0.5f);
    box.Close();
}

TEST(a_flexible_notice_is_as_wide_as_its_words_and_no_wider_than_the_cap)
{
    test::RmlSession rml;
    Settings settings;
    settings.noticeWidth = kFlexibleNoticeWidth;
    NoticeBox box;
    box.Update(rml.context, Gold(), settings,
        {N(1, NoticeKind::Online, "Al"), N(2, NoticeKind::Online, "Bartholomew"),
            N(3, NoticeKind::Online, std::string(200, 'W').c_str())},
        Frame());
    rml.context->Update();
    CHECK(Width(Box(rml, 1)) > Width(Box(rml, 0)));
    CHECK(Width(Box(rml, 0)) < 300.0f);
    CHECK_EQ(Width(Box(rml, 2)), static_cast<float>(kMaxNoticeWidth));
    CHECK_EQ(rml.Said(), "");
    box.Close();
}

TEST(boxes_of_different_widths_line_up_on_the_anchors_side)
{
    test::RmlSession rml;
    Settings settings;
    settings.noticeWidth  = kFlexibleNoticeWidth;
    settings.noticeAcross = 5000;
    settings.noticeDown   = 2000;
    const std::vector<ShownNotice> shown = {N(1, NoticeKind::Online, "Al"), N(2, NoticeKind::Online, "Bartholomew")};
    NoticeBox box;
    const auto right  = [&](int n) { return Left(Box(rml, n)) + Width(Box(rml, n)); };
    const auto middle = [&](int n) { return Left(Box(rml, n)) + Width(Box(rml, n)) / 2.0f; };
    settings.noticeAnchor = HelpAnchor::Right;
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    CHECK(std::abs(right(0) - right(1)) < 0.5f && std::abs(right(0) - 960.0f) < 0.5f);
    settings.noticeAnchor = HelpAnchor::Center;
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    CHECK(std::abs(middle(0) - middle(1)) < 0.5f && std::abs(middle(0) - 960.0f) < 0.5f);
    settings.noticeAnchor = HelpAnchor::Left;
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    CHECK(std::abs(Left(Box(rml, 0)) - Left(Box(rml, 1))) < 0.5f && std::abs(Left(Box(rml, 0)) - 960.0f) < 0.5f);
    box.Close();
}

TEST(truncate_keeps_the_text_size_and_shrink_fits_the_line)
{
    test::RmlSession rml;
    Settings settings;
    settings.noticeWidth = 200;
    settings.noticeFit   = NoticeFit::Truncate;
    const float lookSize = MetricsOf(settings, Gold().shape).subtitle;
    const std::vector<ShownNotice> shown = {N(1, NoticeKind::Online, std::string(30, 'n').c_str())};
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    Rml::Element* message = Part(Box(rml, 0), "message");
    const float content   = Box(rml, 0)->GetBox().GetSize(Rml::BoxArea::Content).x;
    CHECK(std::abs(message->GetComputedValues().font_size() - lookSize) < 0.5f);
    CHECK(Width(message) <= content + 0.5f);
    settings.noticeFit = NoticeFit::Shrink;
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    message = Part(Box(rml, 0), "message");
    CHECK(message->GetComputedValues().font_size() < lookSize);
    const std::string text = NoticeMessage(NoticeKind::Online, std::string(30, 'n'));
    CHECK(static_cast<float>(Rml::ElementUtilities::GetStringWidth(message, text)) <=
          Box(rml, 0)->GetBox().GetSize(Rml::BoxArea::Content).x + 0.5f);
    // Too long to fit at the smallest size: it stays there and is cut at the box's edge.
    box.Update(rml.context, Gold(), settings, {N(2, NoticeKind::Online, std::string(90, 'n').c_str())}, Frame());
    rml.context->Update();
    message = Part(Box(rml, 0), "message");
    CHECK_EQ(message->GetComputedValues().font_size(), kMinNoticeText);
    CHECK(Width(message) <= Box(rml, 0)->GetBox().GetSize(Rml::BoxArea::Content).x + 0.5f);
    CHECK_EQ(rml.Said(), "");
    box.Close();
}

TEST(a_fixed_height_is_the_box_height_with_its_bar_inside)
{
    test::RmlSession rml;
    Settings settings;
    settings.noticeHeight = 40;
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One")}, Frame());
    rml.context->Update();
    CHECK_EQ(Height(Box(rml, 0)), 40.0f);
    Rml::Element* bar = Part(Box(rml, 0), "bar");
    CHECK(Top(bar) + Height(bar) <= Top(Box(rml, 0)) + 40.0f);
    CHECK(Height(bar) == kNoticeBarHeight);
    box.Close();
}

TEST(corner_rounding_and_alignment_reach_the_box)
{
    test::RmlSession rml;
    Settings settings;
    settings.noticeRounding = 12;
    settings.noticeAlign    = HelpAnchor::Center;
    const std::vector<ShownNotice> shown = {N(1, NoticeKind::Online, "One")};
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    CHECK_EQ(Box(rml, 0)->GetComputedValues().border_top_left_radius(), 12.0f);
    CHECK(Part(Box(rml, 0), "title")->GetComputedValues().text_align() == Rml::Style::TextAlign::Center);
    settings.noticeRounding                = kThemeRounding;
    settings.barTheme                      = BarTheme::Custom;
    settings.customPalette                 = Gold();
    settings.customPalette.windowRounding  = 7;
    box.Update(rml.context, settings.customPalette, settings, shown, Frame());
    rml.context->Update();
    CHECK_EQ(Box(rml, 0)->GetComputedValues().border_top_left_radius(), 7.0f);
    box.Close();
}

TEST(the_bounds_are_the_shown_boxes_outline)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    CHECK(!box.Bounds());
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One"), N(2, NoticeKind::Online, "Two")}, Frame());
    rml.context->Update();
    const float h0 = Height(Box(rml, 0)), h1 = Height(Box(rml, 1));
    const std::optional<NoticeRect> bounds = box.Bounds();
    CHECK(bounds && bounds->left == 10.0f && bounds->top == 55.0f && bounds->width == 300.0f);
    CHECK(bounds && std::abs(bounds->height - (h0 + 10.0f + h1)) < 0.01f);
    box.Update(rml.context, Gold(), settings, {}, Frame());
    CHECK(!box.Bounds());
    box.Close();
}

TEST(a_right_anchored_stack_at_the_screens_edge_stays_on_it)
{
    test::RmlSession rml;
    Settings settings;
    settings.noticeAcross = 10000;
    settings.noticeAnchor = HelpAnchor::Right;
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, {N(1, NoticeKind::Online, "One")}, Frame());
    rml.context->Update();
    CHECK(std::abs(Left(Box(rml, 0)) + Width(Box(rml, 0)) - 1920.0f) < 0.5f);
    box.Close();
}

namespace
{
    ShownNotice Invite(uint32_t id, const char* name, bool buttons)
    {
        return ShownNotice{id, NoticeKind::PartyInvite, name, 1.0f, 1.0f, buttons};
    }
    Rml::Vector2f Middle(Rml::Element* el)
    {
        return el->GetAbsoluteOffset(Rml::BoxArea::Border) + el->GetBox().GetSize(Rml::BoxArea::Border) / 2.0f;
    }
    // Down and up at that point: true when nothing of RmlUi's took the press.
    bool ClickAt(test::RmlSession& rml, Rml::Vector2f at)
    {
        rml.context->ProcessMouseMove(static_cast<int>(at.x), static_cast<int>(at.y), 0);
        const bool passed = rml.context->ProcessMouseButtonDown(0, 0);
        rml.context->ProcessMouseButtonUp(0, 0);
        return passed;
    }
}

TEST(clicks_pass_through_a_notice_but_its_buttons_take_them)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    const std::vector<ShownNotice> shown = {Invite(7, "Leader", true)};
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    CHECK(ClickAt(rml, Middle(Part(Box(rml, 0), "title"))));
    CHECK(!box.Update(rml.context, Gold(), settings, shown, Frame()).press);
    rml.context->Update();
    CHECK(!ClickAt(rml, Middle(Part(Box(rml, 0), "accept"))));
    std::optional<NoticePress> press = box.Update(rml.context, Gold(), settings, shown, Frame()).press;
    CHECK(press && press->id == 7 && press->accept);
    CHECK(!box.Update(rml.context, Gold(), settings, shown, Frame()).press);
    rml.context->Update();
    CHECK(!ClickAt(rml, Middle(Part(Box(rml, 0), "decline"))));
    press = box.Update(rml.context, Gold(), settings, shown, Frame()).press;
    CHECK(press && press->id == 7 && !press->accept);
    CHECK_EQ(rml.Said(), "");
    box.Close();
}

TEST(an_invite_without_buttons_and_a_friends_notice_show_none)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, {Invite(7, "Leader", true)}, Frame());
    rml.context->Update();
    const Rml::Vector2f was = Middle(Part(Box(rml, 0), "accept"));
    box.Update(rml.context, Gold(), settings, {Invite(7, "Leader", false), N(8, NoticeKind::Online, "Pal")}, Frame());
    rml.context->Update();
    CHECK(Part(Box(rml, 0), "buttons")->IsClassSet(kHiddenClass));
    CHECK(Part(Box(rml, 1), "buttons")->IsClassSet(kHiddenClass));
    CHECK(ClickAt(rml, was));
    CHECK(!box.Update(rml.context, Gold(), settings, {Invite(7, "Leader", false)}, Frame()).press);
    box.Close();
}

TEST(the_pointer_over_any_notice_is_told)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    const std::vector<ShownNotice> shown = {N(1, NoticeKind::Online, "One"), N(2, NoticeKind::Online, "Two")};
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    const auto with = [&](std::optional<Rml::Vector2f> pointer) {
        NoticeFrame frame = Frame();
        frame.pointer     = pointer;
        return box.Update(rml.context, Gold(), settings, shown, frame).hovered;
    };
    const Rml::Vector2f second = Middle(Box(rml, 1));
    CHECK(with(second));
    CHECK(!with(Rml::Vector2f{second.x, Top(Box(rml, 1)) - 5.0f})); // the gap
    CHECK(!with(Rml::Vector2f{1500.0f, 900.0f}));
    CHECK(!with(std::nullopt));
    box.Close();
}

TEST(a_button_clicks_while_the_stack_can_be_dragged)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    const std::vector<ShownNotice> shown = {Invite(7, "Leader", true)};
    box.Update(rml.context, Gold(), settings, shown, Frame(true));
    rml.context->Update();
    CHECK(!ClickAt(rml, Middle(Part(Box(rml, 0), "accept"))));
    const std::optional<NoticePress> press = box.Update(rml.context, Gold(), settings, shown, Frame(true)).press;
    CHECK(press && press->id == 7 && press->accept);
    rml.context->Update();
    const Rml::Vector2f title = Middle(Part(Box(rml, 0), "title"));
    rml.context->ProcessMouseMove(static_cast<int>(title.x), static_cast<int>(title.y), 0);
    rml.context->ProcessMouseButtonDown(0, 0);
    for (int step = 1; step <= 4; ++step)
    {
        rml.context->ProcessMouseMove(static_cast<int>(title.x) + 50 * step, static_cast<int>(title.y) + 25 * step, 0);
        rml.context->Update();
    }
    rml.context->ProcessMouseButtonUp(0, 0);
    CHECK(box.Update(rml.context, Gold(), settings, shown, Frame(true)).moved);
    rml.context->Update();
    CHECK(std::abs(Left(Box(rml, 0)) - 210.0f) < 0.5f && std::abs(Top(Box(rml, 0)) - 155.0f) < 0.5f);
    box.Close();
}

TEST(an_inviters_name_is_text_not_markup)
{
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, {Invite(7, "<b>&", true)}, Frame());
    rml.context->Update();
    CHECK(Part(Box(rml, 0), "message")->GetInnerRML().find("&lt;b&gt;&amp;") != std::string::npos);
    box.Close();
}

TEST(a_wider_box_further_down_the_stack_stays_on_the_screen_too)
{
    test::RmlSession rml;
    Settings settings;
    settings.noticeWidth  = kFlexibleNoticeWidth;
    settings.noticeAcross = 10000;
    const std::vector<ShownNotice> shown = {N(1, NoticeKind::Online, "Al"), N(2, NoticeKind::Online, "Bartholomew Longname")};
    NoticeBox box;
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    CHECK(Width(Box(rml, 1)) > Width(Box(rml, 0)));
    CHECK(Left(Box(rml, 1)) + Width(Box(rml, 1)) <= 1920.5f);
    settings.noticeAnchor = HelpAnchor::Right;
    settings.noticeAcross = 0;
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    CHECK(Left(Box(rml, 1)) >= -0.5f);
    box.Close();
}

TEST(the_games_window_art_fills_each_box_to_its_foot)
{
    // RmlUi sizes a piece placed in a flex box before the box knows its height: the art was only its padding tall.
    test::RmlSession rml;
    Settings settings;
    NoticeBox box;
    const SkinFrame frame{2, 1.0f, 0.5f, 1.0f, 0xFF102030, 3};
    const std::vector<ShownNotice> shown = {N(1, NoticeKind::Online, "One"),
        ShownNotice{2, NoticeKind::PartyInvite, "Two", 1.0f, 1.0f, true}};
    const auto fills = [&](int slot) {
        Rml::Element* art = Part(Box(rml, slot), kArtClass);
        return art->GetBox().GetSize(Rml::BoxArea::Border) == Box(rml, slot)->GetBox().GetSize(Rml::BoxArea::Padding);
    };
    box.Update(rml.context, Gold(), settings, shown, Frame(false, 0.0f, frame));
    rml.context->Update();
    CHECK(Height(Box(rml, 0)) > 40.0f);
    CHECK(fills(0));
    CHECK(fills(1));
    settings.noticeHeight = 120;
    box.Update(rml.context, Gold(), settings, shown, Frame(false, 0.0f, frame));
    rml.context->Update();
    CHECK_EQ(Height(Box(rml, 1)), 120.0f);
    CHECK(fills(1));
    box.Close();
}

// "Let friend notifications be as small as the friend's name and an online icon."
TEST(a_compact_friend_notice_is_its_dot_and_name_and_an_invite_stays_full)
{
    test::RmlSession rml;
    Settings settings;
    settings.noticeHeight = 80;
    NoticeBox box;
    const std::vector<ShownNotice> shown = {N(1, NoticeKind::Online, "Bartholomew"), N(2, NoticeKind::Offline, "Bartholomew"),
        N(3, NoticeKind::PartyInvite, "Bartholomew")};
    settings.friendNoticeStyle = FriendNoticeStyle::Compact;
    box.Update(rml.context, Gold(), settings, shown, Frame());
    rml.context->Update();
    for (int n = 0; n < 2; ++n)
    {
        Rml::Element* friendBox = Box(rml, n);
        CHECK(Part(friendBox, "title")->IsClassSet(kHiddenClass));
        Rml::Element* message = Part(friendBox, "message");
        CHECK_EQ(message->GetInnerRML().substr(message->GetInnerRML().rfind('>') + 1), std::string("Bartholomew"));
        // The dot is in the kind's own color, and the box holds it and the name whole, whatever Width and Height say.
        Rml::Element* dot = Part(friendBox, "dot");
        CHECK(dot != nullptr && dot->GetBox().GetSize().x > 0.0f);
        const Rml::Colourb fill = dot->GetComputedValues().background_color();
        const uint32_t want     = NoticeColor(shown[static_cast<size_t>(n)].kind);
        CHECK(fill.red == ((want >> 16) & 0xFF) && fill.green == ((want >> 8) & 0xFF) && fill.blue == (want & 0xFF));
        const float right = dot->GetAbsoluteOffset().x + Rml::ElementUtilities::GetStringWidth(message, "Bartholomew") +
                            dot->GetBox().GetSize(Rml::BoxArea::Margin).x;
        CHECK(right <= Left(friendBox) + Width(friendBox));
        CHECK(Width(friendBox) < 300.0f && Height(friendBox) < 80.0f);
    }
    // An invite has its buttons, and keeps its heading, its words and its size.
    CHECK(!Part(Box(rml, 2), "title")->IsClassSet(kHiddenClass) && Part(Box(rml, 2), "dot") == nullptr);
    CHECK(Width(Box(rml, 2)) == 300.0f && Height(Box(rml, 2)) == 80.0f);
    // And so does one that takes a compact one's box.
    box.Update(rml.context, Gold(), settings, {shown[2]}, Frame());
    rml.context->Update();
    CHECK(Width(Box(rml, 0)) == 300.0f && Height(Box(rml, 0)) == 80.0f);
    CHECK_EQ(rml.Said(), "");
    box.Close();
}

#include "menu_window.h"
#include "fonts.h"
#include "markup.h"
#include "menu_sizes.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace headsup;

namespace
{
    constexpr int kLeftButton = 0, kRightButton = 1; // RmlUi's
    const MenuLook kLook{MenuTheme::Phoenix, kBarlowCondensed, kMarcellusSC, 16, 0, false, kPhoenixMenuColors, std::nullopt, std::nullopt};

    // A bag: a list of ten items with three buttons beside it, a tab of two buttons, and a tab of words alone.
    GameWindowSpec Bag()
    {
        std::vector<GameEntry> items;
        for (int i = 1; i <= 10; ++i)
            items.push_back({{"Item " + std::to_string(i)}});
        return {"bag", "Bag",
            {{"Items", GameRowRml(GameListRml("items", items) +
                                  GameColumnRml(GameButtonRml("use", "Use") + GameButtonRml("sort", "Sort") + GameButtonRml("drop", "Drop")))},
                {"Keys", GameRowRml(GameButtonRml("key-a", "Key A") + GameButtonRml("key-b", "Key B"))},
                {"Notes", GameTextRml("Nothing yet.")}},
            GameTextRml("", "foot"), "theme", "theme-look"};
    }

    GameWindowSpec Sure(const std::string& id)
    {
        return {id, "Sure?", {{"", GameRowRml(GameButtonRml(id + "-yes", "Yes") + GameButtonRml(id + "-no", "No"))}}, "", "", ""};
    }

    struct Windows
    {
        test::RmlSession rml; // first, so the windows go before RmlUi does
        GameWindows windows;

        Windows() { windows.Update(rml.context, kLook, {}); }

        Rml::Element* Element(const std::string& window, const std::string& id)
        {
            Rml::ElementDocument* document = windows.DocumentOf(window);
            CHECK(document != nullptr);
            Rml::Element* element = document->GetElementById(id);
            CHECK(element != nullptr);
            return element;
        }
        Rml::Vector2i PointAt(const std::string& window, const std::string& id)
        {
            rml.context->Update();
            Rml::Element* element = Element(window, id);
            const Rml::Vector2f middle = element->GetAbsoluteOffset(Rml::BoxArea::Border) + element->GetBox().GetSize(Rml::BoxArea::Border) / 2.0f;
            const Rml::Vector2i at{static_cast<int>(middle.x), static_cast<int>(middle.y)};
            rml.context->ProcessMouseMove(at.x, at.y, 0);
            return at;
        }
        void Click(const std::string& window, const std::string& id, int button = kLeftButton)
        {
            PointAt(window, id);
            rml.context->ProcessMouseButtonDown(button, 0);
            rml.context->ProcessMouseButtonUp(button, 0);
        }
        void Acts(MenuAction action, int times = 1)
        {
            for (int i = 0; i < times; ++i)
                CHECK(windows.Act(action));
        }
        bool Shown(const std::string& window, const std::string& id)
        {
            rml.context->Update();
            return Element(window, id)->IsVisible(true);
        }
        // Takes an element by its middle and lets it go moved by this many pixels.
        void Drag(const std::string& window, const std::string& id, int byX, int byY)
        {
            const Rml::Vector2i from = PointAt(window, id);
            rml.context->ProcessMouseButtonDown(kLeftButton, 0);
            rml.context->ProcessMouseMove(from.x + byX / 2, from.y + byY / 2, 0);
            rml.context->ProcessMouseMove(from.x + byX, from.y + byY, 0);
            rml.context->ProcessMouseButtonUp(kLeftButton, 0);
            rml.context->Update();
        }
        Rml::Vector2f Corner(const std::string& window)
        {
            rml.context->Update();
            return windows.DocumentOf(window)->GetAbsoluteOffset(Rml::BoxArea::Border);
        }
        // Updates as time passes, until every control has eased into its look, and draws it.
        void Settle()
        {
            for (int step = 0; step < 3; ++step)
            {
                rml.context->Update();
                rml.complaints.seconds += 1.0;
            }
            rml.context->Render();
        }
    };

    // The selection's ground: the soft accent laid over a control's own ground (Phoenix 0x352325 under 0x47C55151, Aurora
    // 0x1F2B40 under 0x385EE6FF), worked out by hand.
    constexpr uint32_t kPhoenixLit = 0xFF5D3031, kAuroraLit = 0xFF2D546A;

    bool SameColor(Rml::Colourb color, uint32_t argb)
    {
        return color.red == (argb >> 16 & 0xFF) && color.green == (argb >> 8 & 0xFF) && color.blue == (argb & 0xFF) && color.alpha == argb >> 24;
    }

    // Whether the element is drawn larger than its box: the box's corner lands inside it.
    bool DrawnLarger(Rml::Element* element)
    {
        const Rml::Vector2f at = element->GetAbsoluteOffset(Rml::BoxArea::Border), size = element->GetBox().GetSize(Rml::BoxArea::Border);
        Rml::Vector2f corner = at + size;
        return element->Project(corner) && corner.x < at.x + size.x - 0.1f && corner.y < at.y + size.y - 0.1f;
    }

    // What lights a selectable element and is drawn larger behind its text.
    Rml::Element* GroundOf(Rml::Element* element)
    {
        Rml::Element* ground = element->QuerySelector(std::string(".") + kGameGroundClass);
        CHECK(ground != nullptr);
        return ground;
    }

    // Whether an entry lies inside its list's view, as far as the list's padding.
    bool InView(Rml::Element* entry, Rml::Element* list)
    {
        const float top    = entry->GetAbsoluteOffset(Rml::BoxArea::Border).y;
        const float bottom = top + entry->GetBox().GetSize(Rml::BoxArea::Border).y;
        const float from   = list->GetAbsoluteOffset(Rml::BoxArea::Padding).y;
        const float to     = from + list->GetBox().GetSize(Rml::BoxArea::Padding).y;
        return top >= from - 1.0f && bottom <= to + 1.0f;
    }
}

TEST(a_window_opens_on_its_left_most_tab_with_its_first_element_selected)
{
    Windows w;
    CHECK(w.windows.Open(Bag()));
    CHECK(w.windows.AnyOpen() && w.windows.IsOpen("bag"));
    CHECK_EQ(w.windows.TabShown("bag"), size_t{0});
    CHECK_EQ(w.windows.Selected(), std::string("items-0"));
    CHECK(w.Shown("bag", "items-0") && !w.Shown("bag", "key-a"));
    CHECK(w.Element("bag", "items-0")->IsClassSet(kGameSelClass));
    // Opened again while open, it is not opened twice.
    CHECK(!w.windows.Open(Bag()));
    CHECK_EQ(w.rml.Said(), "");
}

TEST(a_window_cannot_open_before_it_has_a_context_or_under_a_parent_not_open)
{
    test::RmlSession rml;
    GameWindows windows;
    CHECK(!windows.Open(Bag()));
    windows.Update(rml.context, kLook, {});
    CHECK(!windows.Open(Sure("sure"), "bag"));
    CHECK(!windows.AnyOpen());
}

TEST(clicking_a_tab_shows_it)
{
    Windows w;
    w.windows.Open(Bag());
    w.Click("bag", std::string(kGameTabId) + "1");
    CHECK_EQ(w.windows.TabShown("bag"), size_t{1});
    CHECK(w.Shown("bag", "key-a") && !w.Shown("bag", "items-0"));
    CHECK(w.windows.TakePressed().empty());
}

TEST(the_bumpers_step_through_the_tabs_and_stop_at_the_ends)
{
    Windows w;
    w.windows.Open(Bag());
    w.Acts(MenuAction::TabLeft);
    CHECK_EQ(w.windows.TabShown("bag"), size_t{0});
    w.Acts(MenuAction::TabRight);
    CHECK_EQ(w.windows.TabShown("bag"), size_t{1});
    CHECK_EQ(w.windows.Selected(), std::string("key-a"));
    w.Acts(MenuAction::TabRight, 2);
    CHECK_EQ(w.windows.TabShown("bag"), size_t{2});
    // A tab of nothing to select selects its own tab.
    CHECK_EQ(w.windows.Selected(), std::string(kGameTabId) + "2");
    w.Acts(MenuAction::TabLeft);
    CHECK_EQ(w.windows.TabShown("bag"), size_t{1});
}

TEST(hovering_an_element_selects_it)
{
    Windows w;
    w.windows.Open(Bag());
    w.PointAt("bag", "sort");
    CHECK_EQ(w.windows.Selected(), std::string("sort"));
    CHECK(w.Element("bag", "sort")->IsClassSet(kGameSelClass) && !w.Element("bag", "items-0")->IsClassSet(kGameSelClass));
    w.PointAt("bag", "items-3");
    CHECK_EQ(w.windows.Selected(), std::string("items-3"));
}

TEST(down_through_a_list_scrolls_it_and_wraps_back_to_its_top)
{
    Windows w;
    w.windows.Open(Bag());
    Rml::Element* list = w.Element("bag", "items");
    w.Acts(MenuAction::Down, 7);
    CHECK_EQ(w.windows.Selected(), std::string("items-7"));
    w.rml.context->Update();
    CHECK(list->GetScrollTop() > 0.0f);
    CHECK(InView(w.Element("bag", "items-7"), list));
    w.Acts(MenuAction::Down, 3);
    CHECK_EQ(w.windows.Selected(), std::string("items-0"));
    w.rml.context->Update();
    CHECK_EQ(list->GetScrollTop(), 0.0f);
    w.Acts(MenuAction::Up);
    CHECK_EQ(w.windows.Selected(), std::string("items-9"));
    w.rml.context->Update();
    CHECK(InView(w.Element("bag", "items-9"), list));
}

TEST(an_entry_selected_as_soon_as_its_list_is_set_scrolls_into_view)
{
    Windows w;
    w.windows.Open(Bag());
    std::vector<GameEntry> rings;
    for (int i = 1; i <= 10; ++i)
        rings.push_back({{"Ring " + std::to_string(i)}});
    w.windows.SetRml("bag", "items", GameEntriesRml("items", rings));
    w.windows.Select("bag", "items-8");
    w.rml.context->Update();
    CHECK(InView(w.Element("bag", "items-8"), w.Element("bag", "items")));
}

TEST(left_and_right_go_between_a_list_and_the_buttons_nearest_up_and_down)
{
    // Entries 1.8 em tall a quarter em apart, buttons 1.6 em tall half an em apart, both from the same top: entry 0's
    // middle is nearest Use's, entry 1's Sort's, and entry 2's and every one after Drop's.
    // The pointer passing over an entry leaves none kept, so Left goes to the nearest.
    Windows w;
    w.windows.Open(Bag());
    const auto keepNone = [&](const std::string& button) {
        w.PointAt("bag", "items-4");
        w.PointAt("bag", button);
    };
    w.Acts(MenuAction::Right);
    CHECK_EQ(w.windows.Selected(), std::string("use"));
    w.Acts(MenuAction::Down);
    CHECK_EQ(w.windows.Selected(), std::string("sort"));
    keepNone("sort");
    w.Acts(MenuAction::Left);
    CHECK_EQ(w.windows.Selected(), std::string("items-1"));
    w.Acts(MenuAction::Right);
    CHECK_EQ(w.windows.Selected(), std::string("sort"));
    w.Acts(MenuAction::Left);
    w.Acts(MenuAction::Down, 2);
    w.Acts(MenuAction::Right);
    CHECK_EQ(w.windows.Selected(), std::string("drop"));
    keepNone("drop");
    w.Acts(MenuAction::Left);
    CHECK_EQ(w.windows.Selected(), std::string("items-2"));
    // Nothing is right of the buttons.
    w.Acts(MenuAction::Right, 2);
    CHECK_EQ(w.windows.Selected(), std::string("drop"));
}

TEST(the_arrows_reach_the_tabs_the_cog_and_the_close_button)
{
    Windows w;
    w.windows.Open(Bag());
    w.Acts(MenuAction::Right);
    w.Acts(MenuAction::Up);
    CHECK(w.windows.Selected().starts_with(kGameTabId));
    w.Acts(MenuAction::Up);
    CHECK_EQ(w.windows.Selected(), std::string(kGameCogId));
    w.Acts(MenuAction::Right);
    CHECK_EQ(w.windows.Selected(), std::string(kMenuCloseId));
    w.Acts(MenuAction::Left);
    CHECK_EQ(w.windows.Selected(), std::string(kGameCogId));
    w.Acts(MenuAction::Down);
    CHECK(w.windows.Selected().starts_with(kGameTabId));
    // Confirm on a tab shows it.
    w.Acts(MenuAction::Left);
    const std::string tab = w.windows.Selected();
    w.Acts(MenuAction::Confirm);
    CHECK_EQ(std::string(kGameTabId) + std::to_string(w.windows.TabShown("bag")), tab);
}

TEST(a_click_and_confirm_press_a_button_once_each)
{
    Windows w;
    w.windows.Open(Bag());
    w.Click("bag", "sort");
    CHECK(w.windows.TakePressed() == std::vector<std::string>{"sort"});
    CHECK(w.windows.TakePressed().empty());
    w.Acts(MenuAction::Confirm);
    CHECK(w.windows.TakePressed() == std::vector<std::string>{"sort"});
    // A list's entry is pressed the same way.
    w.Click("bag", "items-4");
    CHECK(w.windows.TakePressed() == std::vector<std::string>{"items-4"});
}

TEST(the_cog_asks_for_its_windows_settings_page)
{
    Windows w;
    w.windows.Open(Bag());
    w.Click("bag", kGameCogId);
    const auto asked = w.windows.TakeSettingsAsked();
    CHECK(asked && asked->first == "theme" && asked->second == "theme-look");
    CHECK(!w.windows.TakeSettingsAsked());
    CHECK(w.windows.IsOpen("bag") && w.windows.TakePressed().empty());
}

TEST(cancel_closes_a_sub_window_first_then_its_parent)
{
    Windows w;
    w.windows.Open(Bag());
    CHECK(w.windows.Open(Sure("sure"), "bag"));
    CHECK_EQ(w.windows.Selected(), std::string("sure-yes"));
    w.Acts(MenuAction::Cancel);
    CHECK(!w.windows.IsOpen("sure") && w.windows.IsOpen("bag"));
    CHECK_EQ(w.windows.Selected(), std::string("items-0"));
    w.Acts(MenuAction::Cancel);
    CHECK(!w.windows.AnyOpen());
    CHECK(!w.windows.Act(MenuAction::Cancel));
    CHECK(!w.windows.Act(MenuAction::Down));
}

TEST(a_held_enter_or_escape_acts_once_however_often_windows_repeats_it)
{
    constexpr uint32_t kEnter = 0x0D, kEscape = 0x1B; // Windows' virtual keys
    Windows w;
    MenuInput input;
    const auto key = [&](uint32_t vk, bool down) { w.windows.Act(input.Feed({InputSource::Keyboard, vk, down ? 1 : 0}, 0.0)); };
    w.windows.Open(Bag());
    w.PointAt("bag", "drop");
    key(kEnter, true);
    CHECK(w.windows.TakePressed() == std::vector<std::string>{"drop"});
    CHECK(w.windows.Open(Sure("sure"), "bag"));
    key(kEnter, true);
    CHECK(w.windows.TakePressed().empty() && w.windows.IsOpen("sure"));
    key(kEnter, false);
    key(kEnter, true);
    CHECK(w.windows.TakePressed() == std::vector<std::string>{"sure-yes"});
    key(kEscape, true);
    key(kEscape, true);
    CHECK(!w.windows.IsOpen("sure") && w.windows.IsOpen("bag"));
    key(kEscape, false);
    key(kEscape, true);
    CHECK(!w.windows.AnyOpen());
}

TEST(two_windows_close_the_later_first)
{
    Windows w;
    w.windows.Open(Bag());
    w.windows.Open(Sure("other"));
    w.Acts(MenuAction::Cancel);
    CHECK(w.windows.IsOpen("bag") && !w.windows.IsOpen("other"));
}

TEST(a_sub_window_takes_the_keys_and_keeps_the_mouse_from_its_parent)
{
    Windows w;
    w.windows.Open(Bag());
    w.windows.Open(Sure("sure"), "bag");
    // The sub-window is in the middle, over the parent's middle: the parent's tabs, along its top, are clear of it.
    const std::string tab = std::string(kGameTabId) + "1";
    w.Click("bag", tab);
    CHECK(w.rml.context->GetHoverElement() == w.Element("bag", tab));
    CHECK_EQ(w.windows.TabShown("bag"), size_t{0});
    CHECK_EQ(w.windows.Selected(), std::string("sure-yes"));
    CHECK(!w.Element("bag", tab)->IsClassSet(kGameSelClass));
    // The click brought the parent no higher than its sub-window.
    w.PointAt("sure", "sure-no");
    CHECK(w.rml.context->GetHoverElement() == w.Element("sure", "sure-no"));
    w.Acts(MenuAction::Right);
    w.Acts(MenuAction::Confirm);
    CHECK(w.windows.TakePressed() == std::vector<std::string>{"sure-no"});
}

TEST(closing_a_window_closes_its_sub_windows_but_no_other)
{
    Windows w;
    w.windows.Open(Bag());
    w.windows.Open(Sure("sure"), "bag");
    w.windows.Open(Sure("other"));
    w.windows.Close("bag");
    CHECK(!w.windows.IsOpen("bag") && !w.windows.IsOpen("sure") && w.windows.IsOpen("other"));
    w.Click("other", kMenuCloseId);
    CHECK(!w.windows.AnyOpen());
}

TEST(a_right_click_over_a_window_cancels)
{
    Windows w;
    w.windows.Open(Bag());
    w.windows.Open(Sure("sure"), "bag");
    w.Click("bag", "drop", kRightButton);
    CHECK(!w.windows.IsOpen("sure") && w.windows.IsOpen("bag"));
    CHECK(w.windows.TakePressed().empty());
    w.Click("bag", "drop", kRightButton);
    CHECK(!w.windows.AnyOpen());
}

TEST(set_text_writes_into_a_windows_element)
{
    Windows w;
    w.windows.Open(Bag());
    w.windows.SetText("bag", "foot", "A < B");
    CHECK_EQ(w.Element("bag", "foot")->GetInnerRML(), EscapedRml("A < B"));
}

TEST(set_cell_writes_one_cell_of_an_entry_in_place_and_keeps_the_selection)
{
    Windows w;
    w.windows.Open({"bag", "Bag", {{"", GameListRml("items", {{{"Ring", "1"}}, {{"Earring", "2"}}})}}, "", "", ""});
    w.windows.Select("bag", "items-1");
    Rml::Element* entry = w.Element("bag", "items-1");
    Rml::Element* first = entry->GetChild(1); // after its ground
    w.windows.SetCell("bag", "items-1", 1, "3 < 4");
    CHECK(w.Element("bag", "items-1") == entry && entry->GetChild(1) == first);
    CHECK_EQ(first->GetInnerRML(), std::string("Earring"));
    CHECK_EQ(entry->GetChild(2)->GetInnerRML(), EscapedRml("3 < 4"));
    CHECK_EQ(w.Element("bag", "items-0")->GetChild(2)->GetInnerRML(), std::string("1"));
    CHECK_EQ(w.windows.Selected(), std::string("items-1"));
}

TEST(a_new_look_restyles_the_open_windows_and_keeps_where_they_are)
{
    for (const MenuTheme theme : {MenuTheme::Phoenix, MenuTheme::Aurora, MenuTheme::GameWindow})
    {
        Windows w;
        w.windows.Open(Bag());
        w.Acts(MenuAction::TabRight);
        const Rml::ElementDocument* before = w.windows.DocumentOf("bag");
        MenuLook look = kLook;
        look.theme    = theme;
        look.colors   = kAuroraMenuColors;
        if (theme == MenuTheme::GameWindow) look.skin = SkinFrame{0, 1.0f, 0.5f, 1.0f, 0xFF808080u};
        w.windows.Update(w.rml.context, look, {});
        // The controls ease from one look to the other: the update that sees the new look starts it, the next ends it.
        w.Settle();
        CHECK(w.windows.DocumentOf("bag") == before);
        CHECK_EQ(w.windows.TabShown("bag"), size_t{1});
        const std::string selected = w.windows.Selected();
        CHECK_EQ(selected, std::string("key-a"));
        CHECK(SameColor(GroundOf(w.Element("bag", selected))->GetComputedValues().background_color(), kAuroraLit));
        CHECK_EQ(w.rml.Said(), "");
    }
}

TEST(the_selection_follows_the_pointer_only_as_it_moves)
{
    // A list scrolled by the keys under a pointer at rest brings other entries under it: the keys' selection stays.
    Windows w;
    w.windows.Open(Bag());
    w.PointAt("bag", "items-2");
    CHECK_EQ(w.windows.Selected(), std::string("items-2"));
    w.Acts(MenuAction::Down, 6);
    for (int frame = 0; frame < 3; ++frame)
        w.rml.context->Update();
    CHECK(w.Element("bag", "items")->GetScrollTop() > 0.0f);
    CHECK_EQ(w.windows.Selected(), std::string("items-8"));
}

namespace
{
    struct Span
    {
        float left, right;
    };
    Span Across(Rml::Element* element)
    {
        const float left = element->GetAbsoluteOffset(Rml::BoxArea::Border).x;
        return {left, left + element->GetBox().GetSize(Rml::BoxArea::Border).x};
    }
}

TEST(a_tabs_parts_stand_side_by_side_in_the_windows_middle)
{
    Windows w;
    w.windows.Open(Bag());
    const float gap = kControlGap * static_cast<float>(kLook.fontPixels);
    const auto check = [&](const std::string& first, const std::string& last) {
        w.rml.context->Update();
        Rml::ElementDocument* document = w.windows.DocumentOf("bag");
        const float from = document->GetAbsoluteOffset(Rml::BoxArea::Content).x;
        const float to   = from + document->GetBox().GetSize(Rml::BoxArea::Content).x;
        const Span a = Across(w.Element("bag", first)), b = Across(w.Element("bag", last));
        CHECK(b.left - a.right <= gap + 0.5f);
        CHECK(std::abs((a.left - from) - (to - b.right)) <= 1.0f);
    };
    // The list and its column of buttons, then a row of two.
    check("items", "use");
    w.Acts(MenuAction::TabRight);
    check("key-a", "key-b");
}

TEST(a_windows_title_is_smaller_than_the_settings_windows_but_larger_than_its_text)
{
    Windows w;
    w.windows.Open(Bag());
    w.rml.context->Update();
    const float title = w.Element("bag", "title")->GetComputedValues().font_size();
    CHECK(title < kTitleScale * static_cast<float>(kLook.fontPixels));
    CHECK(title > static_cast<float>(kLook.fontPixels));
}

TEST(after_the_keys_choose_the_pointer_lights_nothing_until_it_moves)
{
    Windows w;
    w.windows.Open(Bag());
    const Rml::Vector2i at = w.PointAt("bag", "items-2");
    w.Acts(MenuAction::Down);
    w.Settle();
    CHECK_EQ(w.windows.Selected(), std::string("items-3"));
    Rml::Element* under = w.Element("bag", "items-2");
    CHECK(!under->IsPseudoClassSet("hover"));
    CHECK(SameColor(under->GetComputedValues().background_color(), kLook.colors.control));
    // Moved, it selects what it is over again.
    w.rml.context->ProcessMouseMove(at.x + 1, at.y, 0);
    CHECK_EQ(w.windows.Selected(), std::string("items-2"));
}

TEST(the_selection_lights_and_grows_its_ground_and_bolds_its_text_without_moving_or_scaling_it)
{
    Windows w;
    w.windows.Open(Bag());
    w.Settle();
    for (const std::string& id : {std::string("use"), std::string("items-3"), std::string(kGameTabId) + "1", std::string(kMenuCloseId)})
    {
        Rml::Element* element    = w.Element("bag", id);
        Rml::Element* ground     = GroundOf(element);
        const Rml::Vector2f text = element->GetAbsoluteOffset(Rml::BoxArea::Content);
        CHECK(!element->GetComputedValues().has_font_effect() && !DrawnLarger(ground));
        w.PointAt("bag", id);
        w.Settle();
        CHECK_EQ(w.windows.Selected(), id);
        CHECK(SameColor(ground->GetComputedValues().background_color(), kPhoenixLit));
        CHECK(DrawnLarger(ground));
        CHECK(element->GetComputedValues().has_font_effect());
        // Its text stays where it was, at its own size: a text drawn larger is blurred.
        CHECK(element->GetAbsoluteOffset(Rml::BoxArea::Content) == text);
        CHECK(!DrawnLarger(element));
    }
    CHECK_EQ(w.rml.Said(), "");
}

TEST(a_lists_entry_drawn_larger_stays_inside_its_list)
{
    Windows w;
    w.windows.Open(Bag());
    w.Settle();
    Rml::Element* list = w.Element("bag", "items");
    const float left   = list->GetAbsoluteOffset(Rml::BoxArea::Padding).x;
    const float right  = left + list->GetClientWidth();
    for (const std::string& id : {std::string("items-0"), std::string("items-1")})
    {
        w.PointAt("bag", id);
        w.Settle();
        Rml::Element* ground = GroundOf(w.Element("bag", id));
        const Rml::Vector2f at = ground->GetAbsoluteOffset(Rml::BoxArea::Border), size = ground->GetBox().GetSize(Rml::BoxArea::Border);
        // The list's sides, as the grown ground's own points: past its edges when the ground is drawn inside them.
        const float middle = at.y + size.y / 2.0f;
        Rml::Vector2f from{left, middle}, to{right, middle};
        CHECK(ground->Project(from) && ground->Project(to));
        CHECK(from.x <= at.x && to.x >= at.x + size.x);
    }
}

namespace
{
    // An entry left lit in its list: its background as the selection's, its text neither bold nor grown.
    bool KeptLit(Rml::Element* entry)
    {
        const Rml::Style::ComputedValues& look = entry->GetComputedValues();
        return SameColor(look.background_color(), kPhoenixLit) && !look.has_font_effect() && !DrawnLarger(entry);
    }
}

TEST(an_entry_the_keys_chose_stays_lit_when_the_selection_leaves_its_list_until_another_entry_is_chosen)
{
    Windows w;
    w.windows.Open(Bag());
    w.Acts(MenuAction::Down, 2);
    w.Acts(MenuAction::Right);
    CHECK_EQ(w.windows.Selected(), std::string("drop"));
    w.Settle();
    Rml::Element* kept = w.Element("bag", "items-2");
    CHECK(KeptLit(kept));
    CHECK(!KeptLit(w.Element("bag", "items-1")));
    // Among the buttons it stays lit.
    w.Acts(MenuAction::Up);
    w.Settle();
    CHECK(KeptLit(kept));
    // Another entry chosen by the keys lets it go.
    w.Acts(MenuAction::Left);
    w.Acts(MenuAction::Down);
    w.Settle();
    CHECK(SameColor(kept->GetComputedValues().background_color(), kLook.colors.control));
    // So does one the pointer chooses.
    w.Acts(MenuAction::Right);
    w.PointAt("bag", "items-4");
    w.Settle();
    CHECK(SameColor(w.Element("bag", "items-3")->GetComputedValues().background_color(), kLook.colors.control));
    CHECK_EQ(w.rml.Said(), "");
}

TEST(the_keys_come_back_into_a_list_on_its_kept_entry)
{
    // From Sort the nearest entry is items-1; the kept one is items-2.
    Windows w;
    w.windows.Open(Bag());
    w.Acts(MenuAction::Down, 2);
    w.Acts(MenuAction::Right);
    w.Acts(MenuAction::Up);
    CHECK_EQ(w.windows.Selected(), std::string("sort"));
    w.Acts(MenuAction::Left);
    CHECK_EQ(w.windows.Selected(), std::string("items-2"));
    // With none kept, the nearest.
    w.PointAt("bag", "items-5");
    w.PointAt("bag", "use");
    w.Acts(MenuAction::Down);
    w.Acts(MenuAction::Left);
    CHECK_EQ(w.windows.Selected(), std::string("items-1"));
    CHECK_EQ(w.rml.Said(), "");
}

TEST(an_entry_the_pointer_chose_is_not_kept_lit_when_it_leaves_the_list)
{
    Windows w;
    w.windows.Open(Bag());
    w.PointAt("bag", "items-3");
    w.PointAt("bag", "use");
    w.Settle();
    CHECK(SameColor(w.Element("bag", "items-3")->GetComputedValues().background_color(), kLook.colors.control));
}

TEST(a_window_dragged_by_its_title_tells_where_it_was_let_go_and_opens_there_next_time)
{
    Windows w;
    w.windows.Open(Bag());
    const Rml::Vector2f from = w.Corner("bag");
    w.Drag("bag", "title", 40, -30);
    const Rml::Vector2f to = w.Corner("bag");
    CHECK(to == from + Rml::Vector2f(40.0f, -30.0f));
    const std::vector<WindowPlace> moved = w.windows.TakeMoved();
    CHECK((moved == std::vector<WindowPlace>{{"bag", static_cast<int>(to.x), static_cast<int>(to.y)}}));
    CHECK(w.windows.TakeMoved().empty());
    // Pressing a button is no move.
    w.Click("bag", "sort");
    CHECK(w.windows.TakeMoved().empty());
    w.windows.Close("bag");
    w.windows.Update(w.rml.context, kLook, moved);
    CHECK(w.windows.Open(Bag()));
    CHECK(w.Corner("bag") == to);
    // One never moved opens in the screen's middle.
    w.windows.Open(Sure("other"));
    const Rml::Vector2f size   = w.windows.DocumentOf("other")->GetBox().GetSize(Rml::BoxArea::Border);
    const Rml::Vector2i screen = w.rml.context->GetDimensions();
    CHECK(w.Corner("other") == Rml::Vector2f(std::round((static_cast<float>(screen.x) - size.x) / 2.0f),
                                    std::round((static_cast<float>(screen.y) - size.y) / 2.0f)));
}

TEST(a_window_left_past_a_smaller_screens_edge_opens_with_a_fifth_of_it_on)
{
    Windows w;
    const Rml::Vector2i screen = w.rml.context->GetDimensions();
    w.windows.Update(w.rml.context, kLook, {{"bag", screen.x * 3, screen.y * 3}, {"other", -screen.x * 3, -50}});
    w.windows.Open(Bag());
    w.windows.Open(Sure("other"));
    const Rml::Vector2f bag = w.windows.DocumentOf("bag")->GetBox().GetSize(Rml::BoxArea::Border);
    const Rml::Vector2f other = w.windows.DocumentOf("other")->GetBox().GetSize(Rml::BoxArea::Border);
    CHECK(w.Corner("bag") == Rml::Vector2f(static_cast<float>(screen.x) - std::round(bag.x / 5.0f),
                                  static_cast<float>(screen.y) - std::round(bag.y / 5.0f)));
    // And never above the top.
    CHECK(w.Corner("other") == Rml::Vector2f(std::round(other.x / 5.0f) - other.x, 0.0f));
}

TEST(a_windows_own_style_reaches_it_and_stays_through_a_new_look)
{
    constexpr float kWide = 20.0f; // ems
    Windows w;
    GameWindowSpec bag = Bag();
    bag.style          = "#items{width:" + std::to_string(kWide) + "em;}";
    w.windows.Open(bag);
    const auto width = [&] {
        w.rml.context->Update();
        return w.Element("bag", "items")->GetBox().GetSize(Rml::BoxArea::Border).x;
    };
    CHECK_EQ(width(), kWide * static_cast<float>(kLook.fontPixels));
    MenuLook look = kLook;
    look.colors   = kAuroraMenuColors;
    w.windows.Update(w.rml.context, look, {});
    CHECK_EQ(width(), kWide * static_cast<float>(kLook.fontPixels));
    CHECK_EQ(w.rml.Said(), "");
}

TEST(cancel_on_a_window_that_owns_it_is_a_press_and_closes_nothing)
{
    Windows w;
    GameWindowSpec bag = Bag();
    bag.ownsCancel     = true;
    w.windows.Open(bag);
    w.Acts(MenuAction::Cancel);
    CHECK(w.windows.IsOpen("bag"));
    CHECK(w.windows.TakePressed() == std::vector<std::string>{kGameCancelId});
    w.Click("bag", "drop", kRightButton);
    CHECK(w.windows.IsOpen("bag"));
    CHECK(w.windows.TakePressed() == std::vector<std::string>{kGameCancelId});
    // Its close button still closes it.
    w.Click("bag", kMenuCloseId);
    CHECK(!w.windows.AnyOpen());
}

TEST(show_tab_and_select_move_a_window_as_the_keys_would)
{
    Windows w;
    w.windows.Open(Bag());
    w.windows.ShowTab("bag", 1);
    CHECK_EQ(w.windows.TabShown("bag"), size_t{1});
    CHECK_EQ(w.windows.Selected(), std::string("key-a"));
    CHECK(w.Shown("bag", "key-a") && !w.Shown("bag", "items-0"));
    w.windows.Select("bag", "key-b");
    CHECK_EQ(w.windows.Selected(), std::string("key-b"));
    CHECK(w.windows.SelectedByKeys());
    w.PointAt("bag", "key-a");
    CHECK(!w.windows.SelectedByKeys());
    // A tab past the last is not shown.
    w.windows.ShowTab("bag", 3);
    CHECK_EQ(w.windows.TabShown("bag"), size_t{1});
}

TEST(set_rml_drops_a_selection_inside_what_it_replaces)
{
    Windows w;
    w.windows.Open(Bag());
    w.Acts(MenuAction::Down, 3);
    CHECK_EQ(w.windows.Selected(), std::string("items-3"));
    // Elsewhere, it keeps the selection.
    w.windows.SetRml("bag", "foot", GameTextRml("Two left"));
    CHECK_EQ(w.windows.Selected(), std::string("items-3"));
    w.windows.SetRml("bag", "items", GameEntriesRml("items", {{{"Ring"}}, {{"Earring"}}}));
    CHECK_EQ(w.windows.Selected(), std::string());
    w.Acts(MenuAction::Down);
    CHECK_EQ(w.windows.Selected(), std::string("items-0"));
    CHECK(w.Element("bag", "items-0")->GetInnerRML().find("Ring") != std::string::npos);
    w.Acts(MenuAction::Down, 2);
    CHECK_EQ(w.windows.Selected(), std::string("items-0"));
    CHECK_EQ(w.rml.Said(), "");
}

TEST(an_entrys_cells_stand_in_order_the_first_at_its_left_the_last_at_its_right)
{
    Windows w;
    w.windows.Open({"bag", "Bag", {{"", GameListRml("items", {{{"Ring", "Left", "3"}}})}}, "", "", ""});
    w.rml.context->Update();
    Rml::Element* entry = w.Element("bag", "items-0");
    std::vector<Rml::Element*> cells;
    for (int i = 0; i < entry->GetNumChildren(); ++i)
        if (!entry->GetChild(i)->IsClassSet(kGameGroundClass)) cells.push_back(entry->GetChild(i));
    CHECK_EQ(cells.size(), size_t{3});
    CHECK_EQ(cells[0]->GetInnerRML(), std::string("Ring"));
    CHECK_EQ(cells[1]->GetInnerRML(), std::string("Left"));
    CHECK_EQ(cells[2]->GetInnerRML(), std::string("3"));
    const float left = entry->GetAbsoluteOffset(Rml::BoxArea::Content).x, right = left + entry->GetBox().GetSize(Rml::BoxArea::Content).x;
    CHECK(std::abs(Across(cells[0]).left - left) <= 0.5f);
    CHECK(std::abs(Across(cells[2]).right - right) <= 0.5f);
    CHECK(Across(cells[0]).right <= Across(cells[1]).left && Across(cells[1]).right <= Across(cells[2]).left);
}

TEST(a_first_cell_too_long_for_its_entry_is_drawn_cut_short_inside_it)
{
    Windows w;
    w.windows.Open({"bag", "Bag", {{"", GameListRml("items", {{{"Windurstian Knife of a Name Far Too Long for Any List", "3"}}})}}, "", "", ""});
    w.Settle(); // the text's width is known once drawn
    Rml::Element* name = w.Element("bag", "items-0")->GetChild(1); // past its ground
    const auto* text = rmlui_dynamic_cast<Rml::ElementText*>(name->GetChild(0));
    CHECK(text != nullptr && text->GetLines().size() == 1u);
    CHECK(static_cast<float>(text->GetLines().front().width) <= name->GetClientWidth());
}

TEST(a_dim_entry_is_muted_and_a_picked_button_is_filled)
{
    Windows w;
    w.windows.Open({"bag", "Bag",
        {{"", GameRowRml(GameListRml("items", {{{"Broken"}, true}, {{"Whole"}}}) +
                         GameColumnRml(GameButtonRml("all", "All", true) + GameButtonRml("some", "Some")))}},
        "", "", ""});
    // Away from both, so neither is drawn as the selection.
    w.windows.Select("bag", "items-1");
    w.Settle();
    CHECK(SameColor(w.Element("bag", "items-0")->GetComputedValues().color(), kLook.colors.muted));
    CHECK(!SameColor(w.Element("bag", "items-1")->GetComputedValues().color(), kLook.colors.muted));
    CHECK(w.Element("bag", "all")->GetComputedValues().has_decorator());
    CHECK(!w.Element("bag", "some")->GetComputedValues().has_decorator());
    // Selected, a dim entry stays muted.
    w.windows.Select("bag", "items-0");
    w.Settle();
    CHECK(SameColor(w.Element("bag", "items-0")->GetComputedValues().color(), kLook.colors.muted));
}

TEST(a_picked_entry_is_lit_while_the_selection_is_elsewhere)
{
    Windows w;
    GameEntry picked{{"Earring"}};
    picked.picked = true;
    w.windows.Open({"bag", "Bag",
        {{"", GameRowRml(GameListRml("items", {{{"Ring"}}, picked, {{"Necklace"}}}) + GameColumnRml(GameButtonRml("use", "Use")))}}, "", "", ""});
    w.windows.Select("bag", "use");
    w.Settle();
    CHECK(KeptLit(w.Element("bag", "items-1")));
    CHECK(SameColor(w.Element("bag", "items-2")->GetComputedValues().background_color(), kLook.colors.control));
}

namespace
{
    // A box to type in over a list, with nothing to cancel but the window.
    GameWindowSpec Finder(bool ownsCancel = false)
    {
        GameWindowSpec spec{"finder", "Find",
            {{"", GameColumnRml(GameTextBoxRml("find", "Search") + GameListRml("found", {{{"Cesti"}}, {{"Lizard Cesti"}}}))}}, "", "", ""};
        spec.ownsCancel = ownsCancel;
        return spec;
    }
}

TEST(a_selected_text_box_has_the_keyboard_and_loses_it_when_the_selection_leaves)
{
    Windows w;
    w.windows.Open(Finder());
    CHECK_EQ(w.windows.Selected(), std::string("find"));
    CHECK(w.rml.context->GetFocusElement() == w.Element("finder", "find"));
    w.Acts(MenuAction::Down);
    CHECK_EQ(w.windows.Selected(), std::string("found-0"));
    CHECK(w.rml.context->GetFocusElement() != w.Element("finder", "find"));
    w.windows.Select("finder", "find");
    CHECK(w.rml.context->GetFocusElement() == w.Element("finder", "find"));
}

TEST(what_is_typed_in_a_text_box_reads_back_and_set_value_replaces_it)
{
    Windows w;
    w.windows.Open(Finder());
    w.rml.context->ProcessTextInput("Cesti");
    CHECK_EQ(w.windows.TextOf("finder", "find"), std::string("Cesti"));
    w.windows.SetValue("finder", "find", "Lizard");
    CHECK_EQ(w.windows.TextOf("finder", "find"), std::string("Lizard"));
    CHECK_EQ(w.windows.TextOf("finder", "found"), std::string());
}

TEST(a_text_box_shows_its_hint_dimmed_while_it_is_empty)
{
    Windows w;
    w.windows.Open(Finder());
    w.Settle();
    CHECK(SameColor(w.Element("finder", "find")->GetComputedValues().color(), kLook.colors.muted));
    w.windows.SetValue("finder", "find", "Cesti");
    w.Settle();
    CHECK(SameColor(w.Element("finder", "find")->GetComputedValues().color(), kLook.colors.text));
}

TEST(in_a_text_box_confirm_presses_it_up_and_down_leave_it_and_cancel_cancels)
{
    for (const bool owns : {false, true})
    {
        Windows w;
        w.windows.Open(Finder(owns));
        // A click into it only takes the keyboard.
        w.Click("finder", "find");
        CHECK(w.windows.TakePressed().empty());
        w.Acts(MenuAction::Confirm);
        CHECK(w.windows.TakePressed() == std::vector<std::string>{"find"});
        w.rml.context->ProcessKeyDown(Rml::Input::KI_DOWN, 0);
        CHECK_EQ(w.windows.Selected(), std::string("found-0"));
        // Up from a list's top wraps to its bottom, so the box is reached again by Select.
        w.windows.Select("finder", "find");
        w.rml.context->ProcessKeyDown(Rml::Input::KI_UP, 0);
        CHECK(w.windows.Selected() != "find" && w.rml.context->GetFocusElement() != w.Element("finder", "find"));
        w.windows.Select("finder", "find");
        w.Acts(MenuAction::Cancel);
        CHECK_EQ(w.windows.IsOpen("finder"), owns);
        CHECK(w.windows.TakePressed() == (owns ? std::vector<std::string>{kGameCancelId} : std::vector<std::string>{}));
    }
}

TEST(a_held_enter_in_a_text_box_presses_it_once_and_its_repeats_once_the_box_lets_go_press_nothing)
{
    constexpr uint32_t kEnter = 0x0D; // Windows' virtual key
    Windows w;
    MenuInput input;
    // As the plugin's keyboard hook hands Enter on: to the windows' input, or to the text box with the keyboard.
    const auto enter = [&](bool down) {
        const bool typing = w.windows.Typing();
        if (WindowsHearKey(kEnter, typing, typing))
            w.windows.Act(input.Feed({InputSource::Keyboard, kEnter, down ? 1 : 0}, 0.0));
        else if (down)
            w.rml.context->ProcessKeyDown(Rml::Input::KI_RETURN, 0);
        else
            w.rml.context->ProcessKeyUp(Rml::Input::KI_RETURN, 0);
    };
    w.windows.Open(Finder());
    CHECK(w.windows.Typing());
    for (int repeat = 0; repeat < 3; ++repeat)
        enter(true);
    CHECK(w.windows.TakePressed() == std::vector<std::string>{"find"});
    // Its owner takes the selection to a match, as a search does.
    w.windows.Select("finder", "found-0");
    CHECK(!w.windows.Typing());
    enter(true);
    CHECK(w.windows.TakePressed().empty());
    enter(false);
    enter(true);
    CHECK(w.windows.TakePressed() == std::vector<std::string>{"found-0"});
}

TEST(a_text_box_that_takes_the_arrows_hears_each_as_a_press_and_keeps_the_selection)
{
    Windows w;
    w.windows.Open({"counter", "Count", {{"", GameColumnRml(GameTextBoxRml("count", "Count", true) + GameButtonRml("ok", "OK"))}}, "", "", ""});
    CHECK_EQ(w.windows.Selected(), std::string("count"));
    for (const MenuAction arrow : {MenuAction::Left, MenuAction::Right, MenuAction::Up, MenuAction::Down})
        w.Acts(arrow);
    // The keyboard's Up while it types, too.
    w.rml.context->ProcessKeyDown(Rml::Input::KI_UP, 0);
    CHECK_EQ(w.windows.Selected(), std::string("count"));
    CHECK(w.windows.TakePressed() == (std::vector<std::string>{ArrowPress("count", MenuAction::Left), ArrowPress("count", MenuAction::Right),
                                         ArrowPress("count", MenuAction::Up), ArrowPress("count", MenuAction::Down),
                                         ArrowPress("count", MenuAction::Up)}));
    CHECK(ArrowPress("count", MenuAction::Up) != ArrowPress("count", MenuAction::Down) && ArrowPress("count", MenuAction::Up) != "count");
}

namespace
{
    constexpr int kShelfItems = 800;
    constexpr std::string_view kPicturePrefix = "picture-";

    std::string PictureOf(int item) { return std::string(kPicturePrefix) + std::to_string(item); }
    GameEntry Pictured(const std::string& name, int item)
    {
        GameEntry entry{{name}};
        entry.picture = PictureOf(item);
        return entry;
    }

    // A list of many items from the first, each with its picture.
    std::vector<GameEntry> ShelfItems(int first)
    {
        std::vector<GameEntry> items;
        for (int i = 0; i < kShelfItems; ++i)
            items.push_back(Pictured("Item " + std::to_string(i), first + i));
        return items;
    }
    GameWindowSpec Shelf(const std::string& id = "shelf", int first = 1)
    {
        return {id, "Shelf", {{"", GameListRml("items", ShelfItems(first))}}, "", "", ""};
    }

    // A frame: the windows' update, then RmlUi's update and draw.
    void Frame(Windows& w)
    {
        w.windows.Update(w.rml.context, kLook, {});
        w.rml.context->Update();
        w.rml.context->Render();
    }

    bool IsPicture(const std::string& source) { return source.starts_with(kPicturePrefix); }
    size_t PicturesLoaded(const Windows& w)
    {
        return static_cast<size_t>(std::count_if(w.rml.Render().loads.begin(), w.rml.Render().loads.end(), IsPicture));
    }
    size_t PicturesHeld(const Windows& w)
    {
        return PicturesLoaded(w) - static_cast<size_t>(std::count_if(w.rml.Render().releases.begin(), w.rml.Render().releases.end(), IsPicture));
    }
    size_t LoadsOf(const Windows& w, const std::string& source)
    {
        return static_cast<size_t>(std::count(w.rml.Render().loads.begin(), w.rml.Render().loads.end(), source));
    }
    size_t ReleasesOf(const Windows& w, const std::string& source)
    {
        return static_cast<size_t>(std::count(w.rml.Render().releases.begin(), w.rml.Render().releases.end(), source));
    }
    // Where in the loads a picture's first is.
    size_t LoadedAt(const Windows& w, const std::string& source)
    {
        return static_cast<size_t>(std::find(w.rml.Render().loads.begin(), w.rml.Render().loads.end(), source) - w.rml.Render().loads.begin());
    }

    bool HasPicture(Windows& w, const std::string& window, const std::string& entry)
    {
        Rml::Element* picture = w.Element(window, entry)->QuerySelector(std::string(".") + kGamePictureClass);
        CHECK(picture != nullptr);
        return picture->GetComputedValues().has_decorator();
    }

    // Frames until one loads no picture; the most pictures one of them loaded.
    size_t Settle(Windows& w)
    {
        size_t most = 0;
        for (int frame = 0; frame < 2 * kShelfItems; ++frame)
        {
            const size_t before = PicturesLoaded(w);
            Frame(w);
            if (PicturesLoaded(w) == before) break;
            most = std::max(most, PicturesLoaded(w) - before);
        }
        return most;
    }

    void ScrollTo(Windows& w, const std::string& window, const std::string& entry)
    {
        w.Element(window, "items")->SetScrollTop(w.Element(window, entry)->GetOffsetTop());
    }

    // The most rows a list shows at once, part-shown ones too, from where its first two entries were laid out.
    size_t RowsInView(Windows& w, const std::string& window, const std::string& list)
    {
        const float pitch = w.Element(window, list + "-1")->GetAbsoluteOffset(Rml::BoxArea::Border).y -
                            w.Element(window, list + "-0")->GetAbsoluteOffset(Rml::BoxArea::Border).y;
        return static_cast<size_t>(std::ceil(w.Element(window, list)->GetClientHeight() / pitch));
    }
}

TEST(the_rows_in_view_get_their_pictures_at_once_and_a_row_scrolled_to_gets_its_own)
{
    Windows w;
    w.windows.Open(Shelf());
    Frame(w);
    CHECK(HasPicture(w, "shelf", "items-0") && !HasPicture(w, "shelf", "items-500"));
    CHECK_EQ(LoadsOf(w, PictureOf(1)), size_t{1});
    w.windows.Select("shelf", "items-500");
    Frame(w);
    CHECK_EQ(LoadsOf(w, PictureOf(501)), size_t{1});
    // Scrolled by the wheel, too.
    ScrollTo(w, "shelf", "items-300");
    Frame(w);
    CHECK_EQ(LoadsOf(w, PictureOf(301)), size_t{1});
    CHECK_EQ(w.rml.Said(), "");
}

TEST(the_rest_of_a_lists_pictures_load_a_few_a_frame_nearest_the_view_first)
{
    Windows w;
    w.windows.Open(Shelf());
    Frame(w);
    w.windows.Select("shelf", "items-400");
    Frame(w);
    const size_t most = Settle(w);
    CHECK(most > 0 && most <= RowsInView(w, "shelf", "items") + 1); // no frame loads more than a view of them
    for (int item = 1; item <= kShelfItems; ++item)
        CHECK_EQ(LoadsOf(w, PictureOf(item)), size_t{1});
    CHECK(LoadedAt(w, PictureOf(381)) < LoadedAt(w, PictureOf(101)));
    CHECK(LoadedAt(w, PictureOf(421)) < LoadedAt(w, PictureOf(701)));
    // Scrolled to, a row has its picture already.
    ScrollTo(w, "shelf", "items-700");
    Frame(w);
    CHECK_EQ(PicturesLoaded(w), size_t{kShelfItems});
}

TEST(a_row_scrolled_into_view_gets_its_picture_without_the_window_laid_out_again)
{
    Windows w;
    w.windows.Open(Shelf());
    Frame(w);
    // A box no layout gives it, which stays only while nothing is laid out again.
    const Rml::Box marked(Rml::Vector2f(1.0f, 1.0f));
    w.Element("shelf", "items-0")->SetBox(marked);
    ScrollTo(w, "shelf", "items-300");
    Frame(w);
    CHECK_EQ(LoadsOf(w, PictureOf(301)), size_t{1});
    CHECK(w.Element("shelf", "items-0")->GetBox() == marked);
}

TEST(entries_set_in_place_get_their_pictures_at_the_next_frame)
{
    Windows w;
    w.windows.Open(Shelf());
    Frame(w);
    w.windows.SetRml("shelf", "items", GameEntriesRml("items", {Pictured("Ring", 13495), Pictured("Earring", 13358)}));
    CHECK(!HasPicture(w, "shelf", "items-0"));
    Frame(w);
    CHECK_EQ(LoadsOf(w, PictureOf(13495)), size_t{1});
    CHECK_EQ(LoadsOf(w, PictureOf(13358)), size_t{1});
}

TEST(set_cell_skips_an_entrys_picture)
{
    Windows w;
    w.windows.Open({"bag", "Bag", {{"", GameListRml("items", {{{"Ring", "1"}, false, false, PictureOf(13495)}})}}, "", "", ""});
    w.windows.SetCell("bag", "items-0", 1, "2");
    Rml::Element* entry = w.Element("bag", "items-0");
    std::vector<std::string> cells;
    for (int i = 0; i < entry->GetNumChildren(); ++i)
        if (!entry->GetChild(i)->IsClassSet(kGamePictureClass) && !entry->GetChild(i)->IsClassSet(kGameGroundClass)) cells.push_back(entry->GetChild(i)->GetInnerRML());
    CHECK((cells == std::vector<std::string>{"Ring", "2"}));
}

TEST(a_picture_stays_loaded_after_its_window_closes_and_shows_again_without_loading)
{
    Windows w;
    w.windows.Open(Shelf());
    Frame(w);
    w.windows.Close("shelf");
    w.windows.Open({"bag", "Bag", {{"", GameListRml("items", {Pictured("Item 0", 1)})}}, "", "", ""});
    Frame(w);
    CHECK(HasPicture(w, "bag", "items-0"));
    CHECK_EQ(LoadsOf(w, PictureOf(1)), size_t{1});
    CHECK_EQ(ReleasesOf(w, PictureOf(1)), size_t{0});
}

TEST(past_the_pictures_kept_the_least_recently_shown_are_let_go_and_not_loaded_again)
{
    Windows w;
    w.windows.Open(Shelf());
    Settle(w);
    ScrollTo(w, "shelf", "items-" + std::to_string(kShelfItems - 1));
    Frame(w);
    // Another window's lists fill what is kept, and its last rows come into view past it.
    w.windows.Open(Shelf("store", kShelfItems + 1));
    for (int list = 2; PicturesHeld(w) < kGamePicturesKept; ++list)
    {
        CHECK(list < 10);
        w.windows.SetRml("store", "items", GameEntriesRml("items", ShelfItems(list * kShelfItems + 1)));
        Settle(w);
    }
    ScrollTo(w, "store", "items-" + std::to_string(kShelfItems - 1));
    Settle(w);
    CHECK(PicturesHeld(w) <= kGamePicturesKept);
    CHECK(HasPicture(w, "store", "items-" + std::to_string(kShelfItems - 1)));
    // What goes was out of its window's view: it is drawn no more and so not loaded again.
    const std::vector<std::string>& gone = w.rml.Render().releases;
    CHECK(!gone.empty());
    for (int item = 1; item <= kShelfItems; ++item)
    {
        const std::string entry = "items-" + std::to_string(item - 1);
        if (std::find(gone.begin(), gone.end(), PictureOf(item)) == gone.end()) continue;
        CHECK(!InView(w.Element("shelf", entry), w.Element("shelf", "items")));
        CHECK(!HasPicture(w, "shelf", entry));
        CHECK_EQ(LoadsOf(w, PictureOf(item)), size_t{1});
    }
    CHECK_EQ(ReleasesOf(w, PictureOf(kShelfItems)), size_t{0});
}

TEST(closing_every_window_lets_go_of_every_picture)
{
    Windows w;
    w.windows.Open(Shelf());
    Frame(w);
    Frame(w);
    w.windows.Close();
    CHECK(PicturesLoaded(w) > 0);
    CHECK_EQ(PicturesHeld(w), size_t{0});
}

TEST(an_entry_without_a_picture_has_none_and_one_with_has_the_same_height)
{
    Windows w;
    w.windows.Open({"bag", "Bag", {{"", GameListRml("items", {{{"Ring"}}, Pictured("Earring", 13358)})}}, "", "", ""});
    Frame(w);
    CHECK(w.Element("bag", "items-0")->QuerySelector(std::string(".") + kGamePictureClass) == nullptr);
    const auto height = [&](const char* id) { return w.Element("bag", id)->GetBox().GetSize(Rml::BoxArea::Border).y; };
    CHECK_EQ(height("items-0"), height("items-1"));
    // The picture stands left of the name.
    Rml::Element* picture = w.Element("bag", "items-1")->QuerySelector(std::string(".") + kGamePictureClass);
    CHECK(Across(picture).right <= Across(w.Element("bag", "items-1")->GetChild(2)).left);
    CHECK_EQ(w.rml.Said(), "");
}

namespace
{
    // A list whose odd entries have no details; each other's names its entry in its one line.
    GameWindowSpec Notes()
    {
        std::vector<GameEntry> entries;
        for (int i = 0; i < 4; ++i)
        {
            GameEntry entry{{"Note " + std::to_string(i)}};
            if (i % 2 == 0) entry.details = GameTextRml("About note " + std::to_string(i), "about-" + std::to_string(i));
            entries.push_back(entry);
        }
        const std::string loose = "<div id=\"loose\"" + GameDetailsAttribute(GameTextRml("About the page", "about-page")) + ">" + GameTextRml("A page") + "</div>";
        return {"notes", "Notes", {{"", GameRowRml(GameListRml("notes", entries) + GameColumnRml(GameButtonRml("keep", "Keep") + loose))}}, "", "", ""};
    }

    // The details shown, by the id of what they hold; empty while none show.
    std::string TipHolds(Windows& w)
    {
        Frame(w);
        for (int i = 0; i < w.rml.context->GetNumDocuments(); ++i)
        {
            Rml::ElementDocument* document = w.rml.context->GetDocument(i);
            Rml::Element* tip = document->GetElementById(kMenuTipId);
            if (tip == nullptr || !tip->IsVisible(true) || tip->GetNumChildren() == 0) continue;
            return tip->GetChild(0)->GetId();
        }
        return "";
    }
    Rml::Element* Tip(Windows& w)
    {
        for (int i = 0; i < w.rml.context->GetNumDocuments(); ++i)
            if (Rml::Element* tip = w.rml.context->GetDocument(i)->GetElementById(kMenuTipId)) return tip;
        CHECK(false);
        return nullptr;
    }
    Rml::Rectanglef BoxOf(Rml::Element* element)
    {
        const Rml::Vector2f at = element->GetAbsoluteOffset(Rml::BoxArea::Border);
        return Rml::Rectanglef::FromPositionSize(at, element->GetBox().GetSize(Rml::BoxArea::Border));
    }
}

TEST(the_pointer_over_an_entry_with_details_shows_them_beside_it_until_it_leaves)
{
    Windows w;
    w.windows.Open(Notes());
    const Rml::Vector2i at = w.PointAt("notes", "notes-2");
    CHECK_EQ(TipHolds(w), std::string("about-2"));
    const Rml::Rectanglef tip = BoxOf(Tip(w));
    CHECK(!tip.Contains(Rml::Vector2f(static_cast<float>(at.x), static_cast<float>(at.y)))); // never under the pointer
    w.PointAt("notes", "notes-1");
    CHECK_EQ(TipHolds(w), std::string());
    w.PointAt("notes", "notes-0");
    CHECK_EQ(TipHolds(w), std::string("about-0"));
    w.rml.context->ProcessMouseMove(1, 1, 0); // off the window
    CHECK_EQ(TipHolds(w), std::string());
    CHECK_EQ(w.rml.Said(), "");
}

TEST(the_entry_the_keys_choose_shows_its_details_beside_it)
{
    Windows w;
    w.windows.Open(Notes());
    CHECK_EQ(w.windows.Selected(), std::string("notes-0"));
    CHECK_EQ(TipHolds(w), std::string("about-0"));
    const Rml::Rectanglef tip = BoxOf(Tip(w)), entry = BoxOf(w.Element("notes", "notes-0"));
    CHECK(tip.Left() >= entry.Right() || tip.Right() <= entry.Left());
    CHECK(tip.Top() < entry.Bottom() && tip.Bottom() > entry.Top());
    w.Acts(MenuAction::Down);
    CHECK_EQ(TipHolds(w), std::string());
    w.Acts(MenuAction::Down);
    CHECK_EQ(TipHolds(w), std::string("about-2"));
    w.Acts(MenuAction::Right); // the button has none
    CHECK_EQ(TipHolds(w), std::string());
}

TEST(the_details_of_what_the_keys_chose_stand_beside_its_window_over_none_of_its_buttons)
{
    Windows w;
    w.windows.Open(Notes());
    CHECK_EQ(TipHolds(w), std::string("about-0"));
    const Rml::Rectanglef tip = BoxOf(Tip(w)), window = BoxOf(w.windows.DocumentOf("notes"));
    CHECK(tip.Left() >= window.Right() || tip.Right() <= window.Left());
}

TEST(any_part_of_a_window_with_details_shows_them_under_the_pointer)
{
    Windows w;
    w.windows.Open(Notes());
    w.rml.context->Update();
    Rml::Element* words = w.Element("notes", "loose")->GetChild(0);
    const Rml::Vector2f middle = words->GetAbsoluteOffset(Rml::BoxArea::Border) + words->GetBox().GetSize(Rml::BoxArea::Border) / 2.0f;
    w.rml.context->ProcessMouseMove(static_cast<int>(middle.x), static_cast<int>(middle.y), 0);
    CHECK_EQ(TipHolds(w), std::string("about-page"));
}

TEST(closing_a_window_puts_its_details_away)
{
    Windows w;
    w.windows.Open(Notes());
    CHECK_EQ(TipHolds(w), std::string("about-0"));
    w.windows.Close("notes");
    w.rml.context->Update();
    CHECK(!Tip(w)->IsVisible(true));
}

#include "menu_window.h"
#include "fonts.h"
#include "markup.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <optional>
#include <string>
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
        std::vector<std::string> items;
        for (int i = 1; i <= 10; ++i)
            items.push_back("Item " + std::to_string(i));
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

        Windows() { windows.Update(rml.context, kLook); }

        Rml::Element* Element(const std::string& window, const std::string& id)
        {
            Rml::ElementDocument* document = windows.DocumentOf(window);
            CHECK(document != nullptr);
            Rml::Element* element = document->GetElementById(id);
            CHECK(element != nullptr);
            return element;
        }
        void PointAt(const std::string& window, const std::string& id)
        {
            rml.context->Update();
            Rml::Element* element = Element(window, id);
            const Rml::Vector2f middle = element->GetAbsoluteOffset(Rml::BoxArea::Border) + element->GetBox().GetSize(Rml::BoxArea::Border) / 2.0f;
            rml.context->ProcessMouseMove(static_cast<int>(middle.x), static_cast<int>(middle.y), 0);
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
    };

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
    windows.Update(rml.context, kLook);
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

TEST(left_and_right_go_between_a_list_and_the_buttons_nearest_up_and_down)
{
    // Entries 1.8 em tall a quarter em apart, buttons 1.6 em tall half an em apart, both from the same top: entry 0's
    // middle is nearest Use's, entry 1's Sort's, and entry 2's and every one after Drop's.
    Windows w;
    w.windows.Open(Bag());
    w.Acts(MenuAction::Right);
    CHECK_EQ(w.windows.Selected(), std::string("use"));
    w.Acts(MenuAction::Down);
    CHECK_EQ(w.windows.Selected(), std::string("sort"));
    w.Acts(MenuAction::Left);
    CHECK_EQ(w.windows.Selected(), std::string("items-1"));
    w.Acts(MenuAction::Right);
    CHECK_EQ(w.windows.Selected(), std::string("sort"));
    w.Acts(MenuAction::Left);
    w.Acts(MenuAction::Down, 2);
    w.Acts(MenuAction::Right);
    CHECK_EQ(w.windows.Selected(), std::string("drop"));
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
        w.windows.Update(w.rml.context, look);
        // The controls ease from one look to the other: the update that sees the new look starts it, the next ends it.
        for (int step = 0; step < 3; ++step)
        {
            w.rml.context->Update();
            w.rml.complaints.seconds += 1.0;
        }
        CHECK(w.windows.DocumentOf("bag") == before);
        CHECK_EQ(w.windows.TabShown("bag"), size_t{1});
        const std::string selected = w.windows.Selected();
        CHECK_EQ(selected, std::string("key-a"));
        const Rml::Colourb edge = w.Element("bag", selected)->GetComputedValues().border_top_color();
        // The selection's edge is the look's text color.
        const uint32_t text = kAuroraMenuColors.text;
        CHECK(edge.red == (text >> 16 & 0xFF) && edge.green == (text >> 8 & 0xFF) && edge.blue == (text & 0xFF) && edge.alpha == text >> 24);
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

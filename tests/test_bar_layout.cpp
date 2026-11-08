#include "bar_layout.h"
#include "bar_placement.h"
#include "test.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

using namespace headsup;

namespace
{
    // Places the popup in front where the bar drew it, as the bar does each frame; an empty rectangle for none.
    void SetPopup(BarInput& input, const BarRect& rect, std::vector<BarTabChip> chips = {}, std::vector<BarScroll> scrolls = {},
        const BarRect& pin = {}, const BarRect& cog = {}, const BarRect& grip = {})
    {
        std::vector<PopupPlace> places;
        if (input.Open() && rect.width > 0.0f)
            places.push_back(PopupPlace{*input.Open(), rect, std::move(chips), std::move(scrolls), pin, cog, grip});
        input.SetPopups(std::move(places));
    }

    bool Near(float a, float b) { return std::fabs(a - b) < 0.01f; }

    // Separators as each shape drew them before they were the player's: ornate's diamonds and caps, pills' none.
    int ShapeSeparators(BarShape shape) { return shape == BarShape::Ornate ? kAllSeparators : 0; }
    constexpr int kBetween = SeparatorBit(Separator::Between), kCaps = SeparatorBit(Separator::EndCaps);

    // LayoutBar on the default panel, each widget in a pill of its own, unless the test says otherwise.
    BarLayout Layout(float screenWidth, const BarMetrics& metrics, BarShape shape,
        const std::array<float, kPanelSlots>& natural, float targetMinimum,
        const BarPlacement& placement = DefaultPlacement(), std::optional<int> separators = std::nullopt,
        float spacing = 0.0f, const std::array<PanelWidget, kPanelSlots>& groups = AloneGroups())
    {
        return LayoutBar(screenWidth, metrics, shape, natural, targetMinimum, placement,
            separators.value_or(ShapeSeparators(shape)), spacing, groups);
    }

    std::array<float, kPanelSlots> Widths(float zone, float weather, float vana, float target, float gil,
                                              float bags, float earth)
    {
        std::array<float, kPanelSlots> widths{};
        const std::pair<BarWidget, float> each[] = {{BarWidget::Zone, zone}, {BarWidget::Weather, weather},
            {BarWidget::Job, vana}, {BarWidget::Target, target}, {BarWidget::Currency, gil}, {BarWidget::Bags, bags},
            {BarWidget::EarthTime, earth}};
        for (const auto& [widget, width] : each)
            widths[SlotOf(widget)] = width;
        return widths;
    }

    // Natural widths, a 36 px bar.
    const BarMetrics kPills  = MetricsFor(36, BarShape::Pills);
    const BarMetrics kOrnate = MetricsFor(36, BarShape::Ornate);
}

TEST(pills_put_the_groups_at_the_edges_and_the_target_in_the_middle)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    CHECK(Near(l.height, 36.0f) && l.widgets.size() == 6);
    const float g = kPills.gap;
    CHECK(l.widgets[0].widget == BarWidget::Zone && Near(l.widgets[0].rect.x, g));
    CHECK(l.widgets[1].widget == BarWidget::Job && Near(l.widgets[1].rect.x, g + 100.0f + g)); // weather is off
    CHECK(Near(RectOf(l, BarWidget::EarthTime)->x + 50.0f, 1920.0f - g));
    CHECK(Near(RectOf(l, BarWidget::Bags)->x + 60.0f, 1920.0f - g - 50.0f - g));
    CHECK(Near(RectOf(l, BarWidget::Target)->x, 860.0f) && Near(l.targetWidth, 200.0f));
    CHECK(Near(l.widgets[0].rect.height, PillRow(kPills).height) && Near(l.widgets[0].rect.y, PillRow(kPills).y));
    CHECK(l.dividers.empty());
    std::vector<PanelWidget> order;
    for (const PlacedWidget& p : l.widgets)
        order.push_back(p.widget);
    CHECK(order == (std::vector<PanelWidget>{BarWidget::Zone, BarWidget::Job, BarWidget::Target, BarWidget::Currency,
                                           BarWidget::Bags, BarWidget::EarthTime})); // in bar order
}

TEST(ornate_starts_after_the_caps_and_divides_neighbours_with_diamonds)
{
    const BarLayout l = Layout(1920.0f, kOrnate, BarShape::Ornate, Widths(100, 80, 120, 200, 90, 60, 50), 0.0f);
    CHECK(Near(l.widgets[0].rect.x, kOrnate.cap) && Near(l.widgets[0].rect.height, 36.0f) &&
          Near(l.widgets[0].rect.y, 0.0f));
    CHECK(Near(l.widgets[1].rect.x, kOrnate.cap + 100.0f));
    CHECK(l.dividers.size() == 4); // two between three on the left, two on the right
    CHECK(Near(l.dividers[0], kOrnate.cap + 100.0f));
    // Each on the boundary between two neighbours: the start of the later one.
    CHECK(Near(l.dividers[1], RectOf(l, BarWidget::Job)->x));
    CHECK(Near(l.dividers[2], RectOf(l, BarWidget::Bags)->x));
    CHECK(Near(l.dividers[3], RectOf(l, BarWidget::EarthTime)->x));
    CHECK(Near(RectOf(l, BarWidget::Target)->height, PillRow(kOrnate).height)); // the target's frame
    CHECK(Near(RectOf(l, BarWidget::Target)->y, PillRow(kOrnate).y));
}

TEST(a_narrow_screen_shrinks_the_target_then_drops_it_and_nothing_overlaps)
{
    BarLayout l = Layout(700.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 300, 90, 60, 50), 0.0f);
    CHECK(l.targetWidth > 0.0f && l.targetWidth < 300.0f);
    l = Layout(300.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 300, 90, 60, 50), 0.0f);
    CHECK(RectOf(l, BarWidget::Target) == nullptr && l.targetWidth == 0.0f);
    for (size_t i = 0; i < l.widgets.size(); ++i)
    {
        CHECK(l.widgets[i].rect.width > 0.0f);
        for (size_t j = i + 1; j < l.widgets.size(); ++j)
            CHECK(l.widgets[i].rect.x + l.widgets[i].rect.width <= l.widgets[j].rect.x + 0.01f ||
                  l.widgets[j].rect.x + l.widgets[j].rect.width <= l.widgets[i].rect.x + 0.01f);
    }
}

TEST(hit_tests_popup_placement_and_fitting_text)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    CHECK(WidgetAt(l, 960.0f, 18.0f) == BarWidget::Target);
    CHECK(!WidgetAt(l, 500.0f, 18.0f).has_value());
    const BarRect target = *RectOf(l, BarWidget::Target);
    const BarRect under  = PlacePopup(target, 36.0f, 400.0f, 300.0f, 1920.0f, 1080.0f, 8.0f, 0.0f);
    CHECK(Near(under.x, 760.0f) && Near(under.y, 44.0f) && Near(under.height, 300.0f));
    const BarRect edge =
        PlacePopup(*RectOf(l, BarWidget::EarthTime), 36.0f, 400.0f, 2000.0f, 1920.0f, 1080.0f, 8.0f, 0.0f);
    CHECK(Near(edge.x, 1920.0f - 8.0f - 400.0f) && Near(edge.height, 1080.0f - 44.0f - 8.0f));
    auto measure = [](const std::string& s) { return 7.0f * static_cast<float>(s.size()); };
    CHECK(FitText("Goblin Bounty Hunter", 1000.0f, measure) == "Goblin Bounty Hunter");
    CHECK(FitText("Goblin Bounty Hunter", 70.0f, measure) == "Goblin...");
    CHECK(FitText("Goblin Bounty Hunter", 10.0f, measure).empty());
}

TEST(hovering_opens_leaving_closes_and_only_the_widget_or_its_pin_chip_pins)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)]   = true;
    popups[SlotOf(BarWidget::Job)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    CHECK(input.Open() == BarWidget::Target);
    SetPopup(input, BarRect{760.0f, 44.0f, 400.0f, 300.0f});
    input.Move(960.0f, 200.0f); // into the popup
    CHECK(input.Open() == BarWidget::Target);
    input.Move(960.0f, 700.0f);
    CHECK(!input.Open().has_value());
    SetPopup(input, BarRect{});
    CHECK(!input.Press(960.0f, 700.0f, true)); // the game's
    const BarRect pin{1100.0f, 52.0f, 50.0f, 20.0f};
    input.Move(960.0f, 18.0f);
    SetPopup(input, BarRect{760.0f, 44.0f, 400.0f, 300.0f}, {}, {}, pin);
    CHECK(input.Press(960.0f, 18.0f, true)); // the widget pins its popup
    CHECK(input.Pinned() == BarWidget::Target);
    CHECK(input.Press(960.0f, 18.0f, true)); // and unpins it
    CHECK(!input.Pinned().has_value());
    input.Move(960.0f, 700.0f);
    input.Move(960.0f, 18.0f); // hovered again
    CHECK(input.Press(960.0f, 200.0f, true)); // the popup's, and no pin
    CHECK(!input.Pinned().has_value());
    CHECK(input.Press(pin.x + 5.0f, pin.y + 5.0f, true)); // its pin chip pins it
    CHECK(input.Pinned() == BarWidget::Target);
    input.Move(960.0f, 700.0f);
    CHECK(input.Open() == BarWidget::Target); // stays
    const BarRect vana = *RectOf(l, BarWidget::Job);
    input.Move(vana.x + 5.0f, 18.0f);
    CHECK(input.Open() == BarWidget::Job); // hovering another shows it
    input.Move(960.0f, 700.0f);
    CHECK(input.Open() == BarWidget::Target); // back to the pinned one
    SetPopup(input, BarRect{760.0f, 44.0f, 400.0f, 300.0f}, {}, {}, pin);
    CHECK(input.Press(960.0f, 200.0f, true)); // elsewhere on the pinned popup: still pinned
    CHECK(input.Pinned() == BarWidget::Target);
    CHECK(input.Press(pin.x + 5.0f, pin.y + 5.0f, true)); // its chip again unpins
    CHECK(!input.Pinned().has_value());
    const BarRect gil = *RectOf(l, BarWidget::Currency);
    CHECK(input.Press(gil.x + 5.0f, 18.0f, true)); // the bar's, but gil has no popup
    CHECK(!input.Pinned().has_value());
    CHECK(input.Press(5.0f, 30.0f, false)); // a right press on the strip is the bar's too
}

TEST(a_popup_unpinned_from_its_widget_hides_until_the_pointer_leaves_the_widget_and_comes_back)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    CHECK(input.Press(960.0f, 18.0f, true) && input.Pinned() == BarWidget::Target);
    CHECK(input.Press(960.0f, 18.0f, true) && !input.Pinned());
    CHECK(!input.Open()); // gone, though the pointer is still on the widget
    input.Move(965.0f, 20.0f);
    CHECK(!input.Open());
    input.Move(960.0f, 700.0f);
    input.Move(960.0f, 18.0f);
    CHECK(input.Open() == BarWidget::Target); // back once it left and came back
    // Or a press pins it again at once.
    CHECK(input.Press(960.0f, 18.0f, true) && input.Press(960.0f, 18.0f, true) && !input.Open());
    CHECK(input.Press(960.0f, 18.0f, true) && input.Open() == BarWidget::Target && input.Pinned() == BarWidget::Target);
}

TEST(a_fleeting_cards_pick_is_forgotten_once_the_popup_closes_and_another_cards_is_kept)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    const BarRect bastok{770.0f, 100.0f, 60.0f, 20.0f}, boats{770.0f, 200.0f, 60.0f, 20.0f};
    BarTabChip nation{"Conquest", bastok, "Bastok"};
    nation.fleeting = true;
    SetPopup(input, BarRect{760.0f, 44.0f, 400.0f, 300.0f}, {nation, BarTabChip{"Travel", boats, "Boats"}});
    input.Move(780.0f, 110.0f);
    CHECK(input.Press(780.0f, 110.0f, true) && input.Press(780.0f, 210.0f, true));
    CHECK(input.PickedTabs().at("Conquest") == "Bastok" && input.PickedTabs().at("Travel") == "Boats");
    input.Leave(); // the popup closes
    CHECK(!input.PickedTabs().contains("Conquest") && input.PickedTabs().at("Travel") == "Boats");
}

TEST(a_dragged_popup_is_kept_by_its_edges_nearest_the_screens_sides_and_grows_away_from_them)
{
    constexpr float W = 1920.0f, H = 1080.0f;
    const PopupSpot topLeft = SpotOf(BarRect{100.0f, 200.0f, 300.0f, 400.0f}, W, H);
    CHECK(topLeft == (PopupSpot{100, 200, false, false}));
    const PopupSpot bottomRight = SpotOf(BarRect{1500.0f, 600.0f, 300.0f, 400.0f}, W, H);
    CHECK(bottomRight == (PopupSpot{120, 80, true, true})); // 1920 - 1800 and 1080 - 1000
    // Placed back at another size, its kept edges stay and it grows away from them.
    CHECK(PlaceAtSpot(topLeft, 500.0f, 450.0f, W, H) == (BarRect{100.0f, 200.0f, 500.0f, 450.0f}));
    CHECK(PlaceAtSpot(bottomRight, 500.0f, 450.0f, W, H) == (BarRect{1300.0f, 550.0f, 500.0f, 450.0f}));
    // Never off the screen.
    const BarRect big = PlaceAtSpot(topLeft, 1900.0f, 1000.0f, W, H);
    CHECK(big.x >= 0.0f && big.y >= 0.0f && big.x + big.width <= W && big.y + big.height <= H);
}

TEST(a_popup_shows_where_it_is_dragged_else_at_its_kept_spot_else_under_its_widget)
{
    constexpr float W = 1920.0f, H = 1080.0f;
    const BarRect under{800.0f, 44.0f, 300.0f, 200.0f};
    CHECK(!PlacedSpot(std::nullopt, std::nullopt));
    CHECK(PopupAt(under, 200.0f, std::nullopt, W, H) == under);
    const PopupSpot kept{120, 80, true, true};
    CHECK(PlacedSpot(std::nullopt, kept) == kept);
    CHECK(PopupAt(under, 200.0f, kept, W, H) == (BarRect{1500.0f, 800.0f, 300.0f, 200.0f}));
    // Dragged, by its top left, wherever it was kept.
    const std::optional<PopupSpot> dragged = PlacedSpot(BarRect{400.0f, 500.0f, 300.0f, 200.0f}, kept);
    CHECK(dragged == (PopupSpot{400, 500, false, false}));
    CHECK(PopupAt(under, 200.0f, dragged, W, H) == (BarRect{400.0f, 500.0f, 300.0f, 200.0f}));
    // Open, it stays across where it opened while its widget changes width, on the screen, and grows from there.
    CHECK(PopupAt(under, 200.0f, std::nullopt, W, H, 700.0f) == (BarRect{700.0f, 44.0f, 300.0f, 200.0f}));
    CHECK(PopupAt(BarRect{800.0f, 44.0f, 500.0f, 200.0f}, 200.0f, std::nullopt, W, H, 1600.0f) ==
          (BarRect{1420.0f, 44.0f, 500.0f, 200.0f}));
    // A spot of its own wins over where it opened.
    CHECK(PopupAt(under, 200.0f, kept, W, H, 700.0f) == (BarRect{1500.0f, 800.0f, 300.0f, 200.0f}));
}

TEST(a_popup_cut_to_the_room_under_the_bar_drags_down_as_far_as_it_is_put_cut_to_the_room_below_it)
{
    // The card "General Changes": pop-outs moved no more than a few pixels. One taller than the room under a 34 px bar
    // was cut to that room, and kept on the screen at that height it could go no lower than 2 gaps under the bar.
    constexpr float W = 1920.0f, H = 1080.0f, bar = 34.0f, gap = 6.0f;
    const float underBar = PopupRoom(bar, H, gap, 0.0f);
    const BarRect under  = PlacePopup(BarRect{900.0f, 3.0f, 100.0f, 29.0f}, bar, 400.0f, 1500.0f, W, H, gap, 0.0f);
    CHECK(Near(under.height, underBar));
    // Dragged 300 px down, its top goes there and its room is the screen below it.
    const std::optional<PopupSpot> placed = PlacedSpot(BarRect{under.x, under.y + 300.0f, under.width, under.height}, std::nullopt);
    const float room = SpotRoom(*placed, H);
    CHECK(Near(room, H - under.y - 300.0f));
    const BarRect dragged = PopupAt(under, room, placed, W, H);
    CHECK(Near(dragged.y, under.y + 300.0f) && Near(dragged.height, room));
    // Let go there it stays there: reaching the screen's bottom, it is kept by its top, with the same room.
    const PopupSpot left = SpotOf(dragged, W, H);
    CHECK(!left.fromBottom && Near(SpotRoom(left, H), room) && PopupAt(under, room, left, W, H) == dragged);
    // One that fits, let go in the lower half, is kept by its bottom and has the room above that.
    const BarRect low{100.0f, 600.0f, under.width, 300.0f};
    const PopupSpot bottom = SpotOf(low, W, H);
    CHECK(bottom.fromBottom && Near(SpotRoom(bottom, H), 900.0f) && PopupAt(under, 300.0f, bottom, W, H) == low);
}

TEST(a_pinned_popup_drags_by_its_title_row_and_tells_where_it_was_left)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    BarInput input;
    input.Frame(l, popups);
    const BarRect target = *RectOf(l, BarWidget::Target);
    input.Move(target.x + 5.0f, 18.0f);
    const BarRect popup{760.0f, 44.0f, 400.0f, 300.0f}, grip{760.0f, 44.0f, 300.0f, 30.0f};
    SetPopup(input, popup, {}, {}, {}, {}, grip);
    // Hovered, not pinned: its title row moves nothing.
    input.Move(800.0f, 50.0f);
    CHECK(input.Press(800.0f, 50.0f, true) && !input.Dragging());
    input.Pin(BarWidget::Target);
    input.Move(800.0f, 50.0f);
    SetPopup(input, popup, {}, {}, {}, {}, grip);
    CHECK(input.Press(800.0f, 50.0f, true) && input.Dragging());
    input.DragTo(900.0f, 150.0f);
    CHECK(input.DraggedPopup(BarWidget::Target) == std::optional<BarRect>(BarRect{860.0f, 144.0f, 400.0f, 300.0f}));
    // Where the frame showed it, cut to the room below, is where it was left.
    SetPopup(input, BarRect{860.0f, 144.0f, 400.0f, 250.0f});
    input.EndDrag();
    const std::optional<std::pair<PanelWidget, BarRect>> moved = input.TakeMoved();
    CHECK(moved && moved->first == PanelWidget{BarWidget::Target} && moved->second == (BarRect{860.0f, 144.0f, 400.0f, 250.0f}));
    CHECK(!input.TakeMoved() && !input.DraggedPopup(BarWidget::Target));
}

TEST(popups_shown_after_the_one_whose_settings_show_go_over_its_settings_and_under_the_window_unless_come_out_after_it)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    for (const BarWidget w : {BarWidget::Target, BarWidget::Zone, BarWidget::Job, BarWidget::Bags})
        popups[SlotOf(w)] = true;
    BarInput input;
    input.Frame(l, popups);
    using enum PopupLayer;
    const auto press = [&](BarWidget w) { return input.Press(RectOf(l, w)->x + 5.0f, 18.0f, true); };
    input.Pin(BarWidget::Zone);
    input.Pin(BarWidget::Job);
    CHECK(input.Layers() == (std::vector<PopupLayer>{UnderSettings, UnderSettings}));
    input.ShowSettings(BarWidget::Job);
    CHECK(press(BarWidget::Bags));
    CHECK(input.Layers() == (std::vector<PopupLayer>{UnderSettings, UnderSettings, OverSettings}));
    // The card: the window opened over them all, Job's settings with it.
    input.MenuOpen(true);
    CHECK(input.Layers() == (std::vector<PopupLayer>{UnderSettings, UnderSettings, OverSettings}));
    CHECK(press(BarWidget::Target));
    CHECK(input.Layers() == (std::vector<PopupLayer>{UnderSettings, UnderSettings, OverSettings, OverMenu}));
    // Settings shown after the window opened stay under it with their popup.
    input.ShowSettings(BarWidget::Target);
    CHECK(input.Layers() == (std::vector<PopupLayer>{UnderSettings, UnderSettings, UnderSettings, UnderSettings}));
    input.MenuOpen(false);
    CHECK(input.Layers() == (std::vector<PopupLayer>{UnderSettings, UnderSettings, UnderSettings, UnderSettings}));
}

TEST(the_settings_window_goes_over_the_popups_open_before_it_and_under_those_opened_after)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    for (const BarWidget w : {BarWidget::Target, BarWidget::Zone, BarWidget::Job})
        popups[SlotOf(w)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Pin(BarWidget::Zone);
    CHECK(!input.OverMenu(BarWidget::Zone)); // no window open
    input.MenuOpen(true);
    input.Pin(BarWidget::Job);
    CHECK(!input.OverMenu(BarWidget::Zone) && input.OverMenu(BarWidget::Job));
    // Each frame the window stays open changes nothing.
    input.MenuOpen(true);
    CHECK(!input.OverMenu(BarWidget::Zone) && input.OverMenu(BarWidget::Job));
    // One open before it, closed and opened again, comes out over it.
    input.Pin(std::nullopt);
    input.Frame(l, popups);
    input.Pin(BarWidget::Zone);
    input.Pin(BarWidget::Job);
    CHECK(input.OverMenu(BarWidget::Zone) && input.OverMenu(BarWidget::Job));
    // Opened anew, the window goes over everything showing.
    input.MenuOpen(false);
    input.MenuOpen(true);
    CHECK(!input.OverMenu(BarWidget::Zone) && !input.OverMenu(BarWidget::Job));
}

TEST(a_press_on_a_tab_picks_it_and_leaves_the_pin_alone)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    const BarRect airships{770.0f, 100.0f, 60.0f, 20.0f}, boats{834.0f, 100.0f, 50.0f, 20.0f};
    SetPopup(input, BarRect{760.0f, 44.0f, 400.0f, 300.0f}, {BarTabChip{"Travel", airships, "Airships"}, BarTabChip{"Travel", boats, "Boats"}});
    input.Move(850.0f, 110.0f);
    CHECK(input.Press(850.0f, 110.0f, true));
    CHECK(input.PickedTabs().at("Travel") == "Boats" && !input.Pinned());
    CHECK(input.Press(850.0f, 110.0f, true)); // again: still picked
    CHECK(input.PickedTabs().at("Travel") == "Boats" && !input.Pinned());
    CHECK(input.Press(800.0f, 110.0f, false)); // a right press picks nothing
    CHECK(input.PickedTabs().at("Travel") == "Boats");
    CHECK(input.Press(800.0f, 110.0f, true));
    CHECK(input.PickedTabs().at("Travel") == "Airships");
    CHECK(input.Press(960.0f, 250.0f, true)); // elsewhere on the popup: the popup's, and no pin
    CHECK(!input.Pinned().has_value());
    // With no popup drawn its tabs are gone, and what was picked stays picked for the next time.
    SetPopup(input, BarRect{});
    CHECK(!input.Press(850.0f, 110.0f, true));
    CHECK(input.PickedTabs().at("Travel") == "Airships");
}

TEST(a_press_on_a_pressable_cell_is_told_once_and_picks_no_tab_and_pins_nothing)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    const BarRect star{780.0f, 120.0f, 20.0f, 20.0f};
    SetPopup(input, BarRect{760.0f, 44.0f, 400.0f, 300.0f}, {BarTabChip{"", star, "favorite gil", true}});
    input.Move(785.0f, 125.0f);
    CHECK(input.Press(785.0f, 125.0f, true));
    CHECK(input.Press(785.0f, 125.0f, true));
    CHECK((input.TakePresses() == std::vector<std::string>{"favorite gil", "favorite gil"}));
    CHECK(input.TakePresses().empty() && input.PickedTabs().empty() && !input.Pinned());
    CHECK(input.Press(785.0f, 125.0f, false) && input.TakePresses().empty()); // a right press tells nothing
}

TEST(a_cut_help_bar_under_the_strip_opens_its_popup_and_takes_only_its_own_presses)
{
    // The renderer places Game help's own bar under the strip when its text was cut.
    BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    l.widgets.push_back(PlacedWidget{BarWidget::GameHelp, BarRect{0.0f, 46.0f, 480.0f, 60.0f}});
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::GameHelp)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Move(100.0f, 70.0f);
    CHECK(input.Open() == BarWidget::GameHelp);
    CHECK(!input.Press(600.0f, 70.0f, true)); // beside it, the game's
    CHECK(input.Press(100.0f, 70.0f, true)); // its own, which pins it as any widget's does
    CHECK(input.Pinned() == BarWidget::GameHelp);
}

TEST(pinning_another_widget_keeps_the_first_pinned_beside_it)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)]   = true;
    popups[SlotOf(BarWidget::Job)] = true;
    BarInput input;
    input.Frame(l, popups);
    const BarRect pin{1100.0f, 52.0f, 50.0f, 20.0f};
    input.Move(960.0f, 18.0f);
    SetPopup(input, BarRect{760.0f, 44.0f, 400.0f, 300.0f}, {}, {}, pin);
    CHECK(input.Press(pin.x + 5.0f, pin.y + 5.0f, true));
    CHECK(input.Pinned() == BarWidget::Target);
    // Another's popup shows beside it while it is hovered, and its own chip pins it too.
    const BarRect vana = *RectOf(l, BarWidget::Job), vanaPin{vana.x + 200.0f, 52.0f, 50.0f, 20.0f};
    input.Move(vana.x + 5.0f, 18.0f);
    CHECK((input.Shown() == std::vector<PanelWidget>{BarWidget::Target, BarWidget::Job}));
    input.SetPopups({PopupPlace{BarWidget::Target, BarRect{760.0f, 44.0f, 400.0f, 300.0f}, {}, {}, pin},
        PopupPlace{BarWidget::Job, BarRect{vana.x, 44.0f, 300.0f, 200.0f}, {}, {}, vanaPin}});
    CHECK(input.Press(vanaPin.x + 5.0f, vanaPin.y + 5.0f, true));
    CHECK(input.IsPinned(BarWidget::Target) && input.IsPinned(BarWidget::Job) && input.Pinned() == BarWidget::Job);
    input.Leave();
    CHECK((input.Shown() == std::vector<PanelWidget>{BarWidget::Target, BarWidget::Job}));
    // A press on a pinned widget on the bar unpins it alone.
    CHECK(input.Press(960.0f, 18.0f, true));
    CHECK(!input.IsPinned(BarWidget::Target) && input.Pinned() == BarWidget::Job);
}

TEST(a_press_where_two_popups_overlap_goes_to_the_one_on_top)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    popups[SlotOf(BarWidget::Job)]    = true;
    BarInput input;
    input.Frame(l, popups);
    input.Pin(BarWidget::Target);
    input.Pin(BarWidget::Job); // pinned last: drawn on top
    const BarRect under{760.0f, 44.0f, 400.0f, 300.0f}, over{900.0f, 100.0f, 300.0f, 200.0f}, chip{950.0f, 150.0f, 40.0f, 20.0f};
    input.SetPopups({PopupPlace{BarWidget::Target, under, {BarTabChip{"Travel", chip, "Boats"}}},
        PopupPlace{BarWidget::Job, over, {BarTabChip{"Jobs", chip, "Main"}}}});
    CHECK(input.Press(chip.x + 5.0f, chip.y + 5.0f, true));
    CHECK(input.PickedTabs().contains("Jobs") && !input.PickedTabs().contains("Travel"));
}

TEST(the_cog_chip_pins_the_popup_on_its_settings_and_back_and_the_pin_ends_them)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)]   = true;
    popups[SlotOf(BarWidget::Job)] = true;
    BarInput input;
    input.Frame(l, popups);
    const BarRect popup{760.0f, 44.0f, 400.0f, 300.0f}, pin{1100.0f, 52.0f, 50.0f, 20.0f}, cog{1074.0f, 52.0f, 20.0f, 20.0f};
    input.Move(960.0f, 18.0f);
    SetPopup(input, popup, {}, {}, pin, cog);
    CHECK(!input.SettingsShown());
    CHECK(input.Press(cog.x + 5.0f, cog.y + 5.0f, true)); // its settings, pinned
    CHECK(input.SettingsShown() == BarWidget::Target && input.Pinned() == BarWidget::Target);
    input.Move(960.0f, 700.0f);
    CHECK(input.SettingsShown() == BarWidget::Target); // it stays as the pointer goes
    const BarRect vana = *RectOf(l, BarWidget::Job);
    input.Move(vana.x + 5.0f, 18.0f);
    CHECK(input.Open() == BarWidget::Job && input.SettingsShown() == BarWidget::Target); // another's popup beside it
    SetPopup(input, BarRect{vana.x, 44.0f, 300.0f, 200.0f});
    input.Move(960.0f, 700.0f);
    SetPopup(input, popup, {}, {}, pin, cog);
    CHECK(input.SettingsShown() == BarWidget::Target);
    CHECK(input.Press(cog.x + 5.0f, cog.y + 5.0f, true)); // again: its content, and unpinned, as the cog pinned it
    CHECK(!input.SettingsShown() && !input.Pinned());
    // Pinned by its pin first, it stays pinned when the cog closes its settings.
    CHECK(input.Press(960.0f, 18.0f, true));
    input.Move(960.0f, 700.0f);
    CHECK(input.Pinned() == BarWidget::Target);
    CHECK(input.Press(cog.x + 5.0f, cog.y + 5.0f, true));
    CHECK(input.SettingsShown() == BarWidget::Target);
    CHECK(input.Press(cog.x + 5.0f, cog.y + 5.0f, true));
    CHECK(!input.SettingsShown() && input.Pinned() == BarWidget::Target);
    input.Press(cog.x + 5.0f, cog.y + 5.0f, true);
    CHECK(input.Press(pin.x + 5.0f, pin.y + 5.0f, true)); // unpinned, it shows its content again
    CHECK(!input.Pinned() && !input.SettingsShown());
    // Settings opened by the cog, then pinned by hand: the pin is the player's now, and the cog leaves it.
    input.Move(960.0f, 18.0f);
    CHECK(input.Press(cog.x + 5.0f, cog.y + 5.0f, true));
    CHECK(input.Press(pin.x + 5.0f, pin.y + 5.0f, true)); // the pin's press unpins and ends the settings
    CHECK(input.Press(pin.x + 5.0f, pin.y + 5.0f, true)); // pinned by hand
    CHECK(input.Press(cog.x + 5.0f, cog.y + 5.0f, true));
    CHECK(input.Press(cog.x + 5.0f, cog.y + 5.0f, true));
    CHECK(input.Pinned() == BarWidget::Target);
    input.Pin(std::nullopt);
    // As the settings window asks: shown and pinned, then closed.
    input.ShowSettings(BarWidget::Job);
    CHECK(input.SettingsShown() == BarWidget::Job && input.Pinned() == BarWidget::Job);
    input.ShowSettings(std::nullopt);
    CHECK(!input.Pinned() && !input.SettingsShown());
    // A pin moved by a command ends them too.
    input.ShowSettings(BarWidget::Job);
    input.Pin(BarWidget::Target);
    CHECK(!input.SettingsShown());
}

// Targeting a player or nothing while the pointer rests on the target takes its popup away: the hover goes with it,
// and a pin stays (a pinned target popup reads "No Target").
TEST(a_hovered_popup_closes_when_its_widget_stops_having_one)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    std::array<bool, kPanelSlots> none{};
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    SetPopup(input, BarRect{760.0f, 44.0f, 400.0f, 300.0f});
    CHECK(input.Open() == BarWidget::Target);
    input.Frame(l, none);
    CHECK(!input.Open().has_value());
    CHECK(!input.Press(960.0f, 200.0f, true)); // where the popup was: the game's
    CHECK(!input.Pinned().has_value());
    input.Frame(l, popups);
    input.Pin(BarWidget::Target);
    input.Frame(l, none);
    CHECK(input.Open() == BarWidget::Target && input.Pinned() == BarWidget::Target);
}

TEST(a_pinned_widget_that_is_switched_off_loses_its_pin)
{
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    BarInput input;
    input.Frame(Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f), popups);
    input.Pin(BarWidget::Target);
    CHECK(input.Pinned() == BarWidget::Target);
    input.Frame(Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 0, 90, 60, 50), 0.0f), popups);
    CHECK(!input.Pinned().has_value());
}

TEST(fitting_text_with_no_text_or_no_room_gives_nothing)
{
    auto measure = [](const std::string& s) { return 7.0f * static_cast<float>(s.size()); };
    CHECK(FitText("", 100.0f, measure).empty());
    CHECK(FitText("", -5.0f, measure).empty()); // a shrunk target minus its padding can be negative
    CHECK(FitText("Goblin", -5.0f, measure).empty());
    CHECK(FitText("Goblin", 0.0f, measure).empty());
}

TEST(a_cut_text_is_the_longest_start_that_fits_at_every_width)
{
    // Characters of different widths, spaces and UTF-8 (its continuation bytes add nothing).
    auto measure = [](const std::string& s) {
        float width = 0.0f;
        for (const unsigned char c : s)
            width += c == ' ' ? 3.0f : c == 'W' ? 12.0f : (c & 0xC0) == 0x80 ? 0.0f : 7.0f;
        return width;
    };
    for (const std::string text : {"Goblin Bounty Hunter", "W W  W", "Fi\xC3\xB1o Le\xC3\xB3n", "  Bat", "A"})
        for (float width = -1.0f; width <= measure(text) + 1.0f; width += 0.5f)
        {
            // Every start that ends on a whole character, its trailing spaces dropped, tried with "...".
            std::string longest;
            for (size_t end = 1; end < text.size(); ++end)
            {
                std::string cut = text.substr(0, end);
                if ((static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) continue;
                while (!cut.empty() && cut.back() == ' ')
                    cut.pop_back();
                if (!cut.empty() && measure(cut + "...") <= width) longest = cut + "...";
            }
            CHECK(FitText(text, width, measure) == (measure(text) <= width ? text : longest));
        }
}

TEST(a_shrunk_target_with_fractional_widths_stays_between_the_groups)
{
    const auto check = [](float screen, const std::array<float, kPanelSlots>& natural)
    {
        const BarLayout l = Layout(screen, kPills, BarShape::Pills, natural, 0.0f);
        const BarRect* target = RectOf(l, BarWidget::Target);
        if (target == nullptr) return;
        float leftEnd = 0.0f, rightStart = screen;
        for (const PlacedWidget& p : l.widgets)
        {
            const auto& left = DefaultPlacement()[static_cast<int>(BarSection::Left)];
            const bool onLeft = std::ranges::find(left, p.widget) != left.end();
            if (onLeft) leftEnd = std::max(leftEnd, p.rect.x + p.rect.width);
            if (!onLeft && p.widget.kind != BarWidget::Target) rightStart = std::min(rightStart, p.rect.x);
        }
        CHECK(target->x >= leftEnd + kPills.gap - 0.01f);
        CHECK(target->x + target->width <= rightStart - kPills.gap + 0.01f);
        for (size_t i = 0; i < l.widgets.size(); ++i)
            for (size_t j = i + 1; j < l.widgets.size(); ++j)
                CHECK(l.widgets[i].rect.x + l.widgets[i].rect.width <= l.widgets[j].rect.x + 0.01f ||
                      l.widgets[j].rect.x + l.widgets[j].rect.width <= l.widgets[i].rect.x + 0.01f);
    };
    check(700.37f, Widths(100.13f, 0, 120.71f, 300.29f, 90.53f, 60.17f, 50.89f));
    for (int i = 0; i < 400; ++i) // the float rounding of the target's bounds depends on the numbers
        check(600.0f + 0.37f * static_cast<float>(i), Widths(100.13f, 0, 120.71f, 300.29f, 90.53f, 60.17f, 50.89f));
}

TEST(the_hover_holds_across_the_gap_between_a_widget_and_its_popup)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    SetPopup(input, BarRect{760.0f, 44.0f, 400.0f, 300.0f});
    for (const float y : {34.0f, 40.0f, 43.0f, 60.0f})
    {
        input.Move(960.0f, y);
        CHECK(input.Open() == BarWidget::Target);
    }
    CHECK(!input.Press(960.0f, 40.0f, true)); // below the strip and above the popup: the game's
    CHECK(!input.Pinned().has_value());
    input.Move(500.0f, 40.0f); // outside the popup's columns, on no widget
    CHECK(!input.Open().has_value());
}

// Out of the game's client area no move reaches it, so Present says the pointer left: a popup hovering opened closes
// until the pointer comes back, wherever it was last, and a pinned one stays.
TEST(leaving_the_game_closes_a_hovered_popup_until_the_pointer_is_back_and_keeps_a_pinned_one)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)]   = true;
    popups[SlotOf(BarWidget::Job)] = true;
    const BarRect popup{760.0f, 44.0f, 400.0f, 300.0f};
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    SetPopup(input, popup);
    input.Move(960.0f, 200.0f); // into the popup
    CHECK(input.Open() == BarWidget::Target);
    input.Leave();
    CHECK(!input.Open().has_value() && !input.Hovered().has_value());
    input.Frame(l, popups); // the next frame, at the same layout
    CHECK(!input.Open().has_value());
    SetPopup(input, BarRect{});
    input.Move(960.0f, 18.0f);
    CHECK(input.Open() == BarWidget::Target);
    input.Leave(); // from the widget itself
    input.Frame(l, popups);
    CHECK(!input.Open().has_value() && !input.Hovered().has_value());
    input.Move(960.0f, 18.0f);
    input.Pin(BarWidget::Target);
    const BarRect vana = *RectOf(l, BarWidget::Job);
    input.Move(vana.x + 5.0f, 18.0f);
    CHECK(input.Open() == BarWidget::Job);
    input.Leave();
    CHECK(input.Open() == BarWidget::Target && input.Pinned() == BarWidget::Target);
}

TEST(resetting_drops_a_pinned_popup_and_the_pointer)
{
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Target)] = true;
    BarInput input;
    input.Frame(l, popups);
    input.Move(960.0f, 18.0f);
    input.Pin(BarWidget::Target);
    CHECK(input.Pinned() == BarWidget::Target);
    input.Reset();
    CHECK(!input.Open().has_value() && !input.Pinned().has_value() && !input.Hovered().has_value());
    CHECK(!input.Over(960.0f, 18.0f));
    input.Frame(l, popups); // the next login's first frame
    CHECK(!input.Open().has_value());
}

namespace
{
    // GDI's line box is taller than the ink in it: leading above the capitals, brackets a little over them, and the
    // title's descenders reach a quarter of its size below its baseline.
    LineInk FakeInk(bool title, int pixels)
    {
        const float px = static_cast<float>(pixels);
        return title ? LineInk{0.30f * px, 1.00f * px, 1.25f * px, 0.35f * px}
                     : LineInk{0.25f * px, 0.95f * px, 1.20f * px, 0.30f * px};
    }

    // Ink a quarter of the size: anything fits.
    LineInk SmallInk(bool, int pixels)
    {
        const float px = static_cast<float>(pixels);
        return LineInk{0.0f, 0.2f * px, 0.25f * px, 0.0f};
    }

}

TEST(pill_rows_are_whole_pixels_centered_above_the_strips_bottom_line)
{
    for (int h = kMinBarHeight; h <= kMaxBarHeight; ++h)
        for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
        {
            const BarMetrics m = MetricsFor(h, shape);
            const BarRow row   = PillRow(m);
            const float above = row.y, below = (m.height - 1.0f) - (row.y + row.height); // the last row is the line
            CHECK(Near(row.y, std::round(row.y)) && Near(row.height, std::round(row.height)));
            CHECK(above >= std::max(2.0f, 0.06f * m.height) && below >= std::max(2.0f, 0.06f * m.height));
            CHECK(std::fabs(above - below) <= 1.0f);
            const BarLayout l = Layout(1920.0f, m, shape, Widths(100, 80, 120, 200, 90, 60, 50), 0.0f);
            for (const PlacedWidget& p : l.widgets)
                if (shape == BarShape::Pills || p.widget == BarWidget::Target)
                    CHECK(Near(p.rect.y, row.y) && Near(p.rect.height, row.height));
        }
}

TEST(a_titles_descenders_stay_clear_of_its_detail_line_and_both_inside_the_pill)
{
    for (int h = kMinBarHeight; h <= kMaxBarHeight; ++h)
        for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
        {
            const BarMetrics m   = MetricsFor(h, shape);
            const BarRow row     = PillRow(m);
            const TextRow t      = FitTextRow(m, FakeInk);
            const LineInk title  = FakeInk(true, t.title), detail = FakeInk(false, t.detail);
            const LineInk alone  = FakeInk(true, t.alone);
            // Inside the pill's 1 px border, the ink keeps clear space above; descenders may hang into it below.
            const float margin = std::max(2.0f, 0.06f * row.height);
            const float inTop = row.y + 1.0f, inBottom = row.y + row.height - 1.0f;
            CHECK(t.title > t.detail);
            CHECK(t.title <= WholePixels(m.title) && t.detail <= WholePixels(m.subtitle));
            const float above = (t.titleY + title.top) - inTop;
            const float below = inBottom - (t.detailY + detail.bottom);
            CHECK(above >= margin - 0.01f && below >= 0.0f);
            // The title's top and the detail's baseline centered, as two lines read without the detail's descenders,
            // at every height (the card "General Changes": two lines a pixel higher than one at some heights).
            const float underBaseline = inBottom - (t.detailY + detail.baseline);
            CHECK(std::fabs(above - underBaseline) <= 0.01f);
            // Three rows clear from the title's descenders to the detail's capitals: two still read as touching.
            CHECK((t.detailY + detail.top) - (t.titleY + title.bottom) >= 3.0f - 0.01f);
            // A title alone (gil, the clock): its capitals centered on the pill's middle, where an icon beside it
            // is, not its taller brackets (the clock's icon sat high of its time); all its ink inside.
            const float aloneAbove = (t.aloneY + alone.capital) - inTop, aloneUnder = inBottom - (t.aloneY + alone.baseline);
            CHECK(std::fabs(aloneAbove - aloneUnder) <= 0.01f);
            CHECK((t.aloneY + alone.top) - inTop >= margin - 0.01f && t.aloneY + alone.bottom <= inBottom + 0.01f);
        }
}

TEST(a_short_row_shrinks_both_lines_together_and_a_title_alone_keeps_its_full_size)
{
    for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
    {
        const BarMetrics m = MetricsFor(24, shape);
        const TextRow t    = FitTextRow(m, FakeInk);
        CHECK(t.title < WholePixels(m.title) && t.detail < WholePixels(m.subtitle));
        CHECK(std::fabs(static_cast<float>(t.detail) / static_cast<float>(t.title) - m.subtitle / m.title) < 0.15f);
        CHECK_EQ(t.alone, WholePixels(m.title));
        // Its capitals centered in the pill, within a pixel.
        const LineInk alone = FakeInk(true, t.alone);
        const BarRow row    = PillRow(m);
        const float above = (t.aloneY + alone.capital) - row.y, below = row.y + row.height - (t.aloneY + alone.baseline);
        CHECK(std::fabs(above - below) <= 1.01f);
    }
}

TEST(a_taller_bar_never_shrinks_a_line)
{
    for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
    {
        TextRow last;
        for (int h = kMinBarHeight; h <= kMaxBarHeight; ++h)
        {
            const TextRow t = FitTextRow(MetricsFor(h, shape), FakeInk);
            CHECK(t.title >= last.title && t.detail >= last.detail && t.alone >= last.alone);
            last = t;
        }
    }
}

TEST(text_shrinks_only_when_its_ink_would_not_fit)
{
    const BarMetrics m = MetricsFor(36, BarShape::Pills);
    const TextRow roomy = FitTextRow(m, SmallInk);
    CHECK_EQ(roomy.title, WholePixels(m.title));
    CHECK_EQ(roomy.detail, WholePixels(m.subtitle));
    const TextRow tight = FitTextRow(m, FakeInk); // the full sizes leave no gap between the lines
    CHECK(tight.title <= WholePixels(m.title) && tight.detail <= WholePixels(m.subtitle));
    CHECK(tight.title + tight.detail < WholePixels(m.title) + WholePixels(m.subtitle));
    // Both lines shrink together, so the title stays the larger by about as much.
    CHECK(std::fabs(static_cast<float>(tight.detail) / static_cast<float>(tight.title) - m.subtitle / m.title) < 0.1f);
}

TEST(a_target_too_narrow_for_its_padding_icons_and_a_few_letters_is_dropped)
{
    const auto natural = Widths(100, 0, 120, 300, 90, 60, 50);
    const auto room = [](const BarLayout& l) { return l.targetWidth; };
    // 700 wide leaves the target some room, less than all of it.
    const BarLayout some = Layout(700.0f, kPills, BarShape::Pills, natural, 0.0f);
    CHECK(room(some) > 0.0f && room(some) < 300.0f);
    const BarLayout dropped = Layout(700.0f, kPills, BarShape::Pills, natural, room(some) + 1.0f);
    CHECK(RectOf(dropped, BarWidget::Target) == nullptr && dropped.targetWidth == 0.0f);
    const BarLayout kept = Layout(700.0f, kPills, BarShape::Pills, natural, room(some) - 1.0f);
    CHECK(RectOf(kept, BarWidget::Target) != nullptr && RectOf(kept, BarWidget::Target)->width >= room(some) - 1.01f);
    // A short name needs no more than all of itself.
    const BarLayout shortName = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 60, 90, 60, 50), 90.0f);
    CHECK(RectOf(shortName, BarWidget::Target) != nullptr && Near(shortName.targetWidth, 60.0f));
}

TEST(a_popup_and_its_outer_ring_keep_the_gap_from_the_screen_and_the_bar)
{
    const float gap = 6.0f, ring = 3.0f, bar = 37.0f, screenW = 1280.0f, screenH = 720.0f;
    for (const float widgetX : {0.0f, 400.0f, 1250.0f})
    {
        const BarRect p = PlacePopup(BarRect{widgetX, 3.0f, 30.0f, 29.0f}, bar, 300.0f, 2000.0f, screenW, screenH, gap,
                                     ring);
        CHECK(p.x - ring >= gap - 0.01f && p.x + p.width + ring <= screenW - gap + 0.01f);
        CHECK(p.y - ring >= bar + gap - 0.01f && p.y + p.height + ring <= screenH - gap + 0.01f);
        // Cut to the room there is, which a popup can be laid out for beforehand.
        CHECK(Near(p.height, PopupRoom(bar, screenH, gap, ring)));
        CHECK(Near(p.y + p.height + ring, screenH - gap));
    }
    // A screen too small for it still gives a popup no negative size.
    const BarRect tiny = PlacePopup(BarRect{0.0f, 0.0f, 10.0f, 10.0f}, 30.0f, 300.0f, 200.0f, 200.0f, 35.0f, gap, ring);
    CHECK(tiny.height >= 0.0f && tiny.width >= 0.0f);
}

TEST(the_shortest_cut_of_a_name_is_its_first_letters_and_an_ellipsis)
{
    CHECK(Shortest("Crimson-toothed Pawberry", 3) == "Cri...");
    CHECK(Shortest("Bat", 3) == "Bat");
    CHECK(Shortest("Ab Cd", 3) == "Ab...");
    CHECK(Shortest("\xC3\x89" "clair", 3) == "\xC3\x89" "cl...");
    CHECK(Shortest("", 3).empty());
}

namespace
{
    // Windows' mouse messages and the button flags in their wParam (winuser.h).
    constexpr uint32_t WM_MOUSEMOVE = 0x200, WM_LBUTTONDOWN = 0x201, WM_LBUTTONUP = 0x202, WM_LBUTTONDBLCLK = 0x203,
                       WM_RBUTTONDOWN = 0x204, WM_RBUTTONUP = 0x205, WM_RBUTTONDBLCLK = 0x206, WM_MOUSEWHEEL = 0x20A;
    constexpr uint32_t MK_LBUTTON = 0x1, MK_RBUTTON = 0x2;

    // On the target's pill, and well below the bar: the game's.
    constexpr float kOnBarX = 960.0f, kOnBarY = 18.0f, kGameY = 700.0f;

    struct Clicks
    {
        BarInput input;
        BarClicks clicks{input};
        Clicks()
        {
            std::array<bool, kPanelSlots> popups{};
            popups[SlotOf(BarWidget::Target)] = true;
            input.Frame(Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 0.0f),
                popups);
        }
        // The bar on, nothing blocking and no Ashita window under the pointer, unless said.
        bool Take(uint32_t message, float y, uint32_t buttons = 0, bool barOn = true, bool overGui = false,
            bool blocked = false)
        {
            return clicks.Take(message, buttons, kOnBarX, y, barOn, overGui, blocked);
        }
    };
}

TEST(a_press_the_bar_takes_keeps_its_release_from_the_game_once)
{
    Clicks c;
    CHECK(c.Take(WM_LBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_MOUSEMOVE, kGameY, MK_LBUTTON)); // dragged off the bar, still held
    CHECK(c.Take(WM_LBUTTONUP, kGameY));             // released over the game: still the bar's
    CHECK(!c.Take(WM_LBUTTONUP, kGameY));            // the next one is the game's
    CHECK(c.Take(WM_RBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_LBUTTONUP, kOnBarY)); // each button keeps its own
    CHECK(c.Take(WM_RBUTTONUP, kGameY));
    CHECK(!c.Take(WM_RBUTTONUP, kGameY));
}

TEST(a_double_click_on_the_bar_is_a_press_the_bar_takes)
{
    Clicks c;
    CHECK(c.Take(WM_LBUTTONDBLCLK, kOnBarY));
    CHECK(c.Take(WM_LBUTTONUP, kOnBarY));
    CHECK(c.Take(WM_RBUTTONDBLCLK, kOnBarY));
    CHECK(c.Take(WM_RBUTTONUP, kOnBarY));
}

TEST(a_move_with_the_button_up_ends_a_press_released_outside_the_window)
{
    Clicks c;
    CHECK(c.Take(WM_LBUTTONDOWN, kOnBarY));
    CHECK(c.Take(WM_RBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_MOUSEMOVE, kGameY, MK_RBUTTON)); // the left one went up outside
    CHECK(!c.Take(WM_LBUTTONUP, kGameY));
    CHECK(c.Take(WM_RBUTTONUP, kGameY));
}

// Released outside the window and back with no move the game saw (Alt-Tab with the pointer already over it): the
// next press shows the last one ended, and the game gets both halves of a click it was given.
TEST(a_press_the_bar_does_not_take_ends_one_it_took)
{
    Clicks c;
    CHECK(c.Take(WM_LBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_LBUTTONDOWN, kGameY));
    CHECK(!c.Take(WM_LBUTTONUP, kGameY));
    CHECK(c.Take(WM_RBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_RBUTTONDBLCLK, kOnBarY, 0, true, true)); // over an Ashita window: not the bar's either
    CHECK(!c.Take(WM_RBUTTONUP, kOnBarY));
    CHECK(c.Take(WM_LBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_LBUTTONDOWN, kOnBarY, 0, true, false, true)); // another plugin blocked it
    CHECK(!c.Take(WM_LBUTTONUP, kOnBarY));
}

// Over an Ashita window, or blocked by another plugin, a move is not the bar's, and it is the only news the pointer
// left the bar: a popup hovering opened closes, and a pinned one stays.
TEST(a_move_the_bar_may_not_see_closes_a_hovered_popup_and_keeps_a_pinned_one)
{
    for (const bool overGui : {true, false})
    {
        Clicks c;
        CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY));
        CHECK(c.input.Open() == BarWidget::Target);
        CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY, 0, true, overGui, !overGui));
        CHECK(!c.input.Open().has_value());
        c.input.Pin(BarWidget::Target);
        CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY, 0, true, overGui, !overGui));
        CHECK(c.input.Open() == BarWidget::Target && !c.input.Hovered().has_value());
    }
}

// The popup is drawn over Ashita's windows, so inside it the pointer is the popup's even with one under it.
TEST(the_open_popup_keeps_the_pointer_over_an_ashita_window_under_it)
{
    Clicks c;
    CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY));
    SetPopup(c.input, BarRect{760.0f, 44.0f, 400.0f, 300.0f}, {}, {}, BarRect{kOnBarX - 20.0f, 190.0f, 50.0f, 20.0f});
    CHECK(!c.Take(WM_MOUSEMOVE, 200.0f, 0, true, true)); // into the popup, over XIUI's window
    CHECK(c.input.Open() == BarWidget::Target);
    CHECK(c.Take(WM_LBUTTONDOWN, 200.0f, 0, true, true)); // a press on its pin chip pins it
    CHECK(c.Take(WM_LBUTTONUP, 200.0f, 0, true, true));
    CHECK(c.input.Pinned() == BarWidget::Target);
    CHECK(c.Take(WM_LBUTTONDOWN, 200.0f, 0, true, true)); // and unpins it
    CHECK(c.Take(WM_LBUTTONUP, 200.0f, 0, true, true));
    CHECK(!c.Take(WM_LBUTTONDOWN, 200.0f, 0, true, false, true)); // another plugin blocking it still wins
    CHECK(!c.Take(WM_MOUSEMOVE, kGameY, 0, true, true)); // out of it over the window: it closes
    CHECK(!c.input.Open().has_value());
}

TEST(the_wheel_over_a_popups_scrolling_area_scrolls_it_three_lines_a_notch)
{
    Clicks c;
    CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY));
    SetPopup(c.input, BarRect{760.0f, 44.0f, 400.0f, 300.0f}, {},
        {BarScroll{"By Element/Dark", BarRect{770.0f, 150.0f, 380.0f, 120.0f}, 7, BarRect{1140.0f, 155.0f, 10.0f, 110.0f}, 30.0f}});
    const auto wheel = [&](int notches) {
        return c.clicks.Take(WM_MOUSEWHEEL, static_cast<uint32_t>(static_cast<uint16_t>(-notches * 120)) << 16, 0.0f, 0.0f,
            true, false, false);
    };
    CHECK(!c.Take(WM_MOUSEMOVE, 200.0f)); // over it, where the game last saw the pointer
    CHECK(wheel(1) && c.input.Scrolled().at("By Element/Dark") == 3);
    CHECK(wheel(2) && c.input.Scrolled().at("By Element/Dark") == 7); // no further than its last line
    CHECK(wheel(-1) && c.input.Scrolled().at("By Element/Dark") == 4);
    CHECK(!c.Take(WM_MOUSEMOVE, 100.0f)); // the popup, above the area: the game keeps its wheel
    CHECK(!wheel(1) && c.input.Scrolled().at("By Element/Dark") == 4);
}

TEST(a_closed_popup_opens_again_scrolled_to_its_top_and_without_its_refusal)
{
    Clicks c;
    CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY));
    SetPopup(c.input, BarRect{760.0f, 44.0f, 400.0f, 300.0f}, {},
        {BarScroll{"Status Timers", BarRect{770.0f, 150.0f, 380.0f, 120.0f}, 7, BarRect{1140.0f, 155.0f, 10.0f, 110.0f}, 30.0f}});
    CHECK(!c.Take(WM_MOUSEMOVE, 200.0f));
    CHECK(c.clicks.Take(WM_MOUSEWHEEL, static_cast<uint32_t>(static_cast<uint16_t>(-120)) << 16, 0.0f, 0.0f, true, false, false));
    c.input.Refuse();
    CHECK(c.input.Scrolled().at("Status Timers") == 3 && c.input.Refused());
    CHECK(!c.Take(WM_MOUSEMOVE, kGameY)); // away: it closes
    CHECK(!c.input.Open().has_value() && c.input.Scrolled().empty() && !c.input.Refused());
}

TEST(the_scrollbar_drags_its_thumb_and_a_press_on_its_track_jumps_there)
{
    Clicks c;
    CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY));
    // The track runs 100 to 300 down the grip at x 950 to 970; its thumb is 50 tall; 30 lines to scroll.
    const auto popup = [&] {
        SetPopup(c.input, BarRect{760.0f, 44.0f, 400.0f, 300.0f}, {},
            {BarScroll{"By Element/Dark", BarRect{770.0f, 90.0f, 380.0f, 220.0f}, 30, BarRect{950.0f, 100.0f, 20.0f, 200.0f}, 50.0f}});
    };
    popup();
    const auto scrolled = [&] { return c.input.Scrolled().count("By Element/Dark") ? c.input.Scrolled().at("By Element/Dark") : 0; };
    CHECK(!c.Take(WM_MOUSEMOVE, 125.0f));
    CHECK(c.Take(WM_LBUTTONDOWN, 125.0f)); // the thumb's middle
    CHECK(c.Take(WM_MOUSEMOVE, 200.0f, MK_LBUTTON)); // its top to 175: halfway down the 150 it moves
    CHECK_EQ(scrolled(), static_cast<size_t>(15));
    popup();
    CHECK(c.Take(WM_MOUSEMOVE, 600.0f, MK_LBUTTON)); // far below the popup: it stops at the end and keeps the drag
    CHECK_EQ(scrolled(), static_cast<size_t>(30));
    CHECK(c.Take(WM_LBUTTONUP, 600.0f));
    CHECK(!c.input.Pinned().has_value());
    CHECK(!c.Take(WM_MOUSEMOVE, 125.0f)); // let go: moving does nothing
    CHECK_EQ(scrolled(), static_cast<size_t>(30));
    popup();
    // A press on the track above the thumb centers it there: 110 puts its top above the track, so the start.
    CHECK(c.Take(WM_LBUTTONDOWN, 110.0f));
    CHECK_EQ(scrolled(), static_cast<size_t>(0));
    CHECK(c.Take(WM_LBUTTONUP, 110.0f));
    CHECK(!c.input.Pinned().has_value());
}

namespace
{
    // A pinned Target popup in the screen's right half, a fraction of a pixel across as measured text makes it, put
    // each frame where rml_bar puts it and kept where a drag leaves it as the plugin keeps it.
    struct DraggedPopup
    {
        static constexpr float W = 1920.0f, H = 1080.0f, kTitleRow = 30.0f;
        Clicks c;
        BarRect under{1300.6f, 44.0f, 400.4f, 300.0f};
        std::optional<PopupSpot> spot;
        BarRect shown{};
        DraggedPopup()
        {
            c.input.Pin(BarWidget::Target);
            Frame();
        }
        void Frame()
        {
            shown = PopupAt(under, under.height, PlacedSpot(c.input.DraggedPopup(BarWidget::Target), spot), W, H);
            SetPopup(c.input, shown, {}, {}, {}, {}, BarRect{shown.x, shown.y, shown.width, kTitleRow});
            if (const auto moved = c.input.TakeMoved()) spot = SpotOf(moved->second, W, H);
        }
        // RmlUi is given whole pixels.
        float Left() const { return std::round(shown.x); }
        float Top() const { return std::round(shown.y); }
        bool Take(uint32_t message, float x, float y, uint32_t buttons = 0)
        {
            return c.clicks.Take(message, buttons, x, y, true, false, false);
        }
    };
}

// The card's "moves a single pixel each time": with no move reaching the drag, each press and release on the title
// row moved it a pixel left, as its spot was cut to whole pixels downward and measured from the right.
TEST(a_pinned_popup_pressed_and_let_go_by_its_title_row_stays_where_it_shows)
{
    DraggedPopup d;
    const float left = d.Left(), top = d.Top();
    for (int drag = 0; drag < 3; ++drag)
    {
        const float x = d.shown.x + 50.0f, y = d.shown.y + 10.0f;
        CHECK(d.Take(WM_LBUTTONDOWN, x, y));
        d.Frame();
        CHECK_EQ(d.Left(), left);
        CHECK(d.Take(WM_LBUTTONUP, x, y));
        d.Frame();
        d.Frame();
        CHECK(d.spot.has_value());
        CHECK_EQ(d.Left(), left);
        CHECK_EQ(d.Top(), top);
    }
}

// The drag goes by where the pointer is, not by the button flags of the moves: the end is the release, or the button
// read up from Windows each frame.
TEST(a_popup_drag_follows_moves_whose_button_flag_reads_up)
{
    DraggedPopup d;
    const float left = d.Left(), top = d.Top();
    const float x = d.shown.x + 50.0f, y = d.shown.y + 10.0f;
    CHECK(d.Take(WM_LBUTTONDOWN, x, y));
    for (int step = 1; step <= 3; ++step)
    {
        CHECK(d.Take(WM_MOUSEMOVE, x, y + 100.0f * static_cast<float>(step)));
        d.Frame();
        CHECK_EQ(d.Top(), top + 100.0f * static_cast<float>(step));
    }
    CHECK(d.Take(WM_LBUTTONUP, x, y + 300.0f));
    d.Frame();
    d.Frame();
    CHECK_EQ(d.Top(), top + 300.0f);
    CHECK_EQ(d.Left(), left);
}

// Where no move message comes while the button is held, Windows' pointer read each frame drags it, and the button
// read up there ends it where it shows: let go outside the window, no release message comes either.
TEST(a_popup_drag_follows_windows_pointer_each_frame_and_ends_once_its_button_is_up)
{
    DraggedPopup d;
    const float left = d.Left(), top = d.Top();
    const float x = d.shown.x + 50.0f, y = d.shown.y + 10.0f;
    CHECK(d.Take(WM_LBUTTONDOWN, x, y));
    for (int frame = 1; frame <= 3; ++frame)
    {
        d.c.clicks.Held(x - 40.0f * static_cast<float>(frame), y + 100.0f * static_cast<float>(frame), true);
        d.Frame();
        CHECK_EQ(d.Top(), top + 100.0f * static_cast<float>(frame));
        CHECK_EQ(d.Left(), left - 40.0f * static_cast<float>(frame));
    }
    d.c.clicks.Held(x, y + 500.0f, false);
    CHECK(!d.c.input.Dragging());
    d.Frame();
    d.Frame();
    CHECK_EQ(d.Top(), top + 300.0f);
    CHECK_EQ(d.Left(), left - 120.0f);
    CHECK(!d.Take(WM_MOUSEMOVE, x, y + 600.0f)); // let go: moving does nothing to it
    d.Frame();
    CHECK_EQ(d.Top(), top + 300.0f);
}

TEST(the_bar_going_off_gives_the_game_every_release)
{
    Clicks c;
    CHECK(c.Take(WM_LBUTTONDOWN, kOnBarY));
    CHECK(c.Take(WM_RBUTTONDOWN, kOnBarY));
    CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY, MK_LBUTTON | MK_RBUTTON, false));
    CHECK(!c.Take(WM_LBUTTONUP, kOnBarY));
    CHECK(!c.Take(WM_RBUTTONUP, kOnBarY));
}

TEST(the_bar_takes_nothing_off_it_or_while_it_may_not)
{
    Clicks c;
    for (const uint32_t message : {WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP, WM_MOUSEWHEEL})
        CHECK(!c.Take(message, kGameY));
    CHECK(!c.Take(WM_LBUTTONDOWN, kOnBarY, 0, false));              // off
    CHECK(!c.Take(WM_LBUTTONDOWN, kOnBarY, 0, true, true));         // under an Ashita window
    CHECK(!c.Take(WM_LBUTTONDOWN, kOnBarY, 0, true, false, true));  // blocked
    CHECK(!c.Take(WM_LBUTTONUP, kOnBarY));
    CHECK(!c.input.Pinned().has_value());
    CHECK(!c.Take(WM_MOUSEWHEEL, kOnBarY)); // the wheel stays the game's on the bar too
    // Only a move the bar may see hovers.
    CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY, 0, true, true));
    CHECK(!c.input.Open().has_value());
    CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY, 0, true, false, true));
    CHECK(!c.input.Open().has_value());
    CHECK(!c.Take(WM_MOUSEMOVE, kOnBarY));
    CHECK(c.input.Open() == BarWidget::Target);
}

TEST(every_widget_has_one_place_on_the_bar_and_none_overlap_on_a_narrow_screen)
{
    std::array<float, kPanelSlots> natural{};
    natural.fill(10.0f);
    const BarLayout wide = Layout(4000.0f, kPills, BarShape::Pills, natural, 0.0f);
    CHECK_EQ(wide.widgets.size(), static_cast<size_t>(kPlacedWidgetCount));
    for (int w = 0; w < kBarWidgetCount; ++w)
        CHECK((RectOf(wide, static_cast<BarWidget>(w)) != nullptr) == OnTheBar(static_cast<BarWidget>(w)));
    for (const auto& [width, screen] : {std::pair{120.0f, 1280.0f}, std::pair{250.0f, 640.0f}})
    {
        natural.fill(width);
        const BarLayout narrow = Layout(screen, kPills, BarShape::Pills, natural, 60.0f);
        CHECK(!narrow.widgets.empty());
        for (size_t a = 0; a < narrow.widgets.size(); ++a)
        {
            const BarRect& r = narrow.widgets[a].rect;
            CHECK(r.x >= 0.0f && r.x + r.width <= screen); // at 640 px the left group alone is wider than the screen
            for (size_t b = a + 1; b < narrow.widgets.size(); ++b)
            {
                const BarRect& o = narrow.widgets[b].rect;
                CHECK(r.x + r.width <= o.x || o.x + o.width <= r.x);
            }
        }
    }
}

namespace
{
    BarPlacement Sections(std::vector<PanelWidget> left, std::vector<PanelWidget> center, std::vector<PanelWidget> right)
    {
        return BarPlacement{std::move(left), std::move(center), std::move(right)};
    }

    std::array<float, kPanelSlots> SomeWidths()
    {
        std::array<float, kPanelSlots> natural{};
        natural[SlotOf(BarWidget::Zone)]      = 100.0f;
        natural[SlotOf(BarWidget::Target)]    = 200.0f;
        natural[SlotOf(BarWidget::Currency)]       = 90.0f;
        natural[SlotOf(BarWidget::Bags)]      = 60.0f;
        natural[SlotOf(BarWidget::EarthTime)] = 50.0f;
        return natural;
    }

    int SectionOf(const BarPlacement& p, PanelWidget w)
    {
        for (int s = 0; s < kBarSectionCount; ++s)
            if (std::ranges::find(p[s], w) != p[s].end()) return s;
        return -1;
    }

    // Every widget an edge (a pills gap, an ornate end cap) from the screen's sides; widgets in different sections a
    // gap apart, pills a gap apart within one too, and an ornate target clear of the diamond on its neighbour's edge.
    void CheckApartAndInside(const BarLayout& l, float screen, const BarPlacement& p = DefaultPlacement(),
                             BarShape shape = BarShape::Pills)
    {
        const bool ornate        = shape == BarShape::Ornate;
        const BarMetrics& m      = ornate ? kOrnate : kPills;
        const float edge         = ornate ? m.cap : m.gap;
        const float diamondClear = static_cast<float>(WholePixels(m.diamond)) / 2.0f + 2.0f;
        for (size_t a = 0; a < l.widgets.size(); ++a)
        {
            const BarRect& r = l.widgets[a].rect;
            CHECK(r.x >= edge - 0.01f && r.x + r.width <= screen - edge + 0.01f);
            for (size_t b = a + 1; b < l.widgets.size(); ++b)
            {
                const PanelWidget wa = l.widgets[a].widget, wb = l.widgets[b].widget;
                const BarRect& o   = l.widgets[b].rect;
                const bool framed  = wa == BarWidget::Target || wb == BarWidget::Target;
                const float apart  = SectionOf(p, wa) != SectionOf(p, wb) || !ornate ? m.gap
                                     : framed                                       ? diamondClear
                                                                                    : 0.0f;
                CHECK(r.x + r.width + apart <= o.x + 0.01f || o.x + o.width + apart <= r.x + 0.01f);
            }
        }
    }
}

TEST(the_center_holds_several_widgets_centered_as_a_group)
{
    const float g     = kPills.gap;
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, SomeWidths(), 0.0f,
        Sections({BarWidget::Zone}, {BarWidget::Currency, BarWidget::Target, BarWidget::Bags}, {BarWidget::EarthTime}));
    const float total = 90.0f + g + 200.0f + g + 60.0f, x0 = 960.0f - total / 2.0f;
    CHECK(Near(RectOf(l, BarWidget::Currency)->x, x0) && Near(RectOf(l, BarWidget::Target)->x, x0 + 90.0f + g));
    CHECK(Near(RectOf(l, BarWidget::Bags)->x, x0 + 90.0f + g + 200.0f + g));
    CHECK(WidgetAt(l, x0 + 10.0f, PillRow(kPills).y + 1.0f) == BarWidget::Currency); // hover finds it where it is
}

TEST(an_empty_center_leaves_the_sides_alone_and_the_target_can_sit_in_a_side)
{
    const float g     = kPills.gap;
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, SomeWidths(), 0.0f,
        Sections({BarWidget::Zone, BarWidget::Target}, {}, {BarWidget::Currency, BarWidget::Bags, BarWidget::EarthTime}));
    CHECK(Near(RectOf(l, BarWidget::Target)->x, g + 100.0f + g) && Near(l.targetWidth, 200.0f));
    CHECK(Near(RectOf(l, BarWidget::Target)->y, PillRow(kPills).y));
    CHECK(Near(RectOf(l, BarWidget::EarthTime)->x + 50.0f, 1920.0f - g));
    const BarLayout ornate = Layout(1920.0f, kOrnate, BarShape::Ornate, SomeWidths(), 0.0f,
        Sections({BarWidget::Zone, BarWidget::Target}, {}, {BarWidget::Currency}));
    CHECK(Near(RectOf(ornate, BarWidget::Target)->height, PillRow(kOrnate).height)); // still framed
}

TEST(a_target_in_a_side_section_shrinks_to_the_room_left_and_goes_when_that_is_too_little)
{
    const float g = kPills.gap;
    const auto right = Sections({BarWidget::Zone}, {}, {BarWidget::Target, BarWidget::Currency});
    // At 400 px the room between zone and gil, a gap from each, is 400 - 4g - 190 = 185.5 px: under the target's 200.
    const float room  = (400.0f - g - 90.0f - g) - (g + 100.0f + g);
    const BarLayout l = Layout(400.0f, kPills, BarShape::Pills, SomeWidths(), 40.0f, right);
    CHECK(Near(l.targetWidth, std::floor(room))); // whole pixels
    const BarRect& target = *RectOf(l, BarWidget::Target);
    CHECK(target.x >= g + 100.0f + g - 0.01f && target.x + target.width + g <= RectOf(l, BarWidget::Currency)->x + 0.01f);
    CheckApartAndInside(l, 400.0f, right);
    const BarLayout tight = Layout(400.0f, kPills, BarShape::Pills, SomeWidths(), room + 1.0f, right);
    CHECK(RectOf(tight, BarWidget::Target) == nullptr && RectOf(tight, BarWidget::Currency) != nullptr);
}

TEST(a_target_first_in_the_left_section_gives_way_before_any_right_widget)
{
    const float g     = kPills.gap;
    const auto first  = Sections({BarWidget::Target, BarWidget::Zone}, {}, {BarWidget::Currency, BarWidget::Bags,
                                                                            BarWidget::EarthTime});
    // Everything at its width: 2 edges, target, gap, zone, then a gap, gil, gap, bags, gap, Earth time.
    const float demand = g + 200.0f + g + 100.0f + g + 90.0f + g + 60.0f + g + 50.0f + g;
    const BarLayout l  = Layout(500.0f, kPills, BarShape::Pills, SomeWidths(), 40.0f, first);
    CHECK(Near(l.targetWidth, std::floor(200.0f - (demand - 500.0f))));
    for (const BarWidget w : {BarWidget::Zone, BarWidget::Currency, BarWidget::Bags, BarWidget::EarthTime})
        CHECK(RectOf(l, w) != nullptr);
    CheckApartAndInside(l, 500.0f, first);
}

TEST(a_crowded_center_shrinks_its_target_then_drops_it_then_drops_from_its_end)
{
    const float g     = kPills.gap;
    const auto center = Sections({BarWidget::Zone}, {BarWidget::Target, BarWidget::Currency, BarWidget::Bags}, {});
    // At 450 px the center has 450 - 3g - 100 px: about 31 px short of the target, gil and bags at their widths.
    const BarLayout l = Layout(450.0f, kPills, BarShape::Pills, SomeWidths(), 40.0f, center);
    const float room  = 450.0f - 2.0f * g - (g + 100.0f);
    CHECK(Near(l.targetWidth, std::floor(room - 90.0f - 60.0f - 2.0f * g)) && RectOf(l, BarWidget::Bags) != nullptr);
    const BarLayout noTarget = Layout(450.0f, kPills, BarShape::Pills, SomeWidths(), 400.0f, center);
    CHECK(RectOf(noTarget, BarWidget::Target) == nullptr && RectOf(noTarget, BarWidget::Bags) != nullptr);
    // At 250 px, gil and bags alone are too wide: bags, at the center's end, goes.
    const BarLayout narrow = Layout(250.0f, kPills, BarShape::Pills, SomeWidths(), 400.0f, center);
    CHECK(RectOf(narrow, BarWidget::Currency) != nullptr && RectOf(narrow, BarWidget::Bags) == nullptr);
    for (const BarLayout* each : {&l, &noTarget, &narrow})
        CheckApartAndInside(*each, each == &narrow ? 250.0f : 450.0f, center);
}

TEST(any_widget_in_any_section_keeps_every_widget_apart_and_on_screen)
{
    std::array<float, kPanelSlots> natural{};
    for (int w = 0; w < kBarWidgetCount; ++w)
        for (int s = 0; s < kBarSectionCount && OnTheBar(static_cast<BarWidget>(w)); ++s)
            for (const auto& [width, screen] : {std::pair{10.0f, 4000.0f}, std::pair{120.0f, 1280.0f},
                     std::pair{250.0f, 640.0f}})
            {
                BarPlacement p = DefaultPlacement();
                int from       = 0;
                size_t index   = 0;
                for (int f = 0; f < kBarSectionCount; ++f)
                    for (size_t i = 0; i < p[f].size(); ++i)
                        if (p[f][i] == static_cast<BarWidget>(w)) from = f, index = i;
                MoveWidget(p, static_cast<BarSection>(from), index, static_cast<BarSection>(s), p[s].size());
                natural.fill(width);
                for (const BarShape shape : {BarShape::Pills, BarShape::Ornate})
                {
                    const BarLayout l = Layout(screen, shape == BarShape::Pills ? kPills : kOrnate, shape, natural,
                        60.0f, p);
                    if (screen > 3000.0f) CHECK_EQ(l.widgets.size(), static_cast<size_t>(kPlacedWidgetCount));
                    CheckApartAndInside(l, screen, p, shape);
                    // The target gives way first: whatever shows without it shows with it.
                    std::array<float, kPanelSlots> noTarget = natural;
                    noTarget[SlotOf(BarWidget::Target)] = 0.0f;
                    for (const PlacedWidget& kept : Layout(screen, shape == BarShape::Pills ? kPills : kOrnate,
                             shape, noTarget, 60.0f, p).widgets)
                        CHECK(RectOf(l, kept.widget) != nullptr);
                }
            }
}

TEST(a_target_that_gave_way_comes_back_into_the_room_the_dropped_widgets_left)
{
    const float g = kPills.gap;
    // Zone 100 and Vana'diel time 120 on the left, gil 90, bags 60 and Earth time 50 on the right. Against every
    // widget the target would keep floor(S - 420 - 7g) px, under its 40 px minimum; gil needs S >= 420 + 6g, so it
    // goes too, and without gil the room between the groups is S - 330 - 6g = 70.5 px.
    const float screen = 400.5f + 6.0f * g;
    const BarLayout l  = Layout(screen, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 40.0f);
    CHECK(RectOf(l, BarWidget::Currency) == nullptr);
    const BarRect* target = RectOf(l, BarWidget::Target);
    CHECK(target != nullptr);
    const float room = RectOf(l, BarWidget::Bags)->x - g - (RectOf(l, BarWidget::Job)->x + 120.0f) - g;
    CHECK(Near(room, 70.5f) && Near(l.targetWidth, std::floor(room)) && target && Near(target->width, 70.0f));
    CHECK(target && target->x >= RectOf(l, BarWidget::Job)->x + 120.0f + g - 0.01f);
    CheckApartAndInside(l, screen);
}

TEST(an_ornate_target_in_a_side_section_keeps_its_frame_clear_of_the_diamonds_beside_it)
{
    // A divider's diamond is drawn WholePixels(diamond) wide, centered on its x; a frame keeps 2 px clear of it.
    const float clear = static_cast<float>(WholePixels(kOrnate.diamond)) / 2.0f + 2.0f;
    const auto sides  = Sections({BarWidget::Zone, BarWidget::Target, BarWidget::Currency}, {}, {BarWidget::EarthTime});
    const BarLayout l = Layout(1920.0f, kOrnate, BarShape::Ornate, SomeWidths(), 0.0f, sides);
    const BarRect& target = *RectOf(l, BarWidget::Target);
    CHECK(Near(target.width, 200.0f));
    CHECK_EQ(l.dividers.size(), static_cast<size_t>(2)); // zone | target | gil
    for (const float d : l.dividers)
        CHECK(std::fabs(d - target.x) >= clear - 0.01f && std::fabs(d - (target.x + target.width)) >= clear - 0.01f &&
              !(d > target.x && d < target.x + target.width));
    // The diamonds still sit on the plain neighbours' edges, and the section is that much wider.
    CHECK(Near(l.dividers[0], kOrnate.cap + 100.0f) && Near(target.x, kOrnate.cap + 100.0f + clear));
    CHECK(Near(RectOf(l, BarWidget::Currency)->x, target.x + 200.0f + clear));
    CHECK(Near(l.dividers[1], RectOf(l, BarWidget::Currency)->x));
    CheckApartAndInside(l, 1920.0f, sides, BarShape::Ornate);
}

TEST(a_target_in_a_side_section_also_takes_back_the_room_a_dropped_neighbour_left)
{
    const float g = kPills.gap;
    // The center case's widths with the target first on the right: placed inward, Earth time, bags, gil, then the
    // target. Against every widget it would keep floor(S - 420 - 7g) px, under 40; gil needs S >= 420 + 6g and goes;
    // without it the target has S - 330 - 6g = 70.5 px between Vana'diel time and bags.
    const float screen = 400.5f + 6.0f * g;
    const auto right   = Sections({BarWidget::Zone, BarWidget::Job}, {},
        {BarWidget::Target, BarWidget::Currency, BarWidget::Bags, BarWidget::EarthTime});
    const BarLayout l  = Layout(screen, kPills, BarShape::Pills, Widths(100, 0, 120, 200, 90, 60, 50), 40.0f, right);
    CHECK(RectOf(l, BarWidget::Currency) == nullptr && RectOf(l, BarWidget::Target) != nullptr);
    CHECK(Near(l.targetWidth, 70.0f));
    CheckApartAndInside(l, screen, right);
}

TEST(a_target_in_a_left_section_that_passes_the_screen_edge_takes_back_the_room_left)
{
    const float g = kPills.gap;
    // Left: zone 100, the target 200, gil 90. Against all three the target would keep floor(S - 190 - 4g) px, less
    // than nothing; gil then needs S >= 190 + 3g and goes past the edge; zone alone leaves the target S - 100 - 3g.
    const float screen = 170.5f + 3.0f * g;
    const auto left    = Sections({BarWidget::Zone, BarWidget::Target, BarWidget::Currency}, {}, {});
    const BarLayout l  = Layout(screen, kPills, BarShape::Pills, Widths(100, 0, 0, 200, 90, 0, 0), 40.0f, left);
    CHECK(RectOf(l, BarWidget::Currency) == nullptr && RectOf(l, BarWidget::Zone) != nullptr);
    CHECK(Near(l.targetWidth, 70.0f));
    CheckApartAndInside(l, screen, left);
}

TEST(a_target_that_fits_exactly_keeps_its_whole_width)
{
    // The default bar with the screen exactly as wide as everything: float sums can come out a hair over the screen,
    // which must not cost the target a pixel.
    const float g = kPills.gap;
    for (int i = 0; i < 1000; ++i)
    {
        const float want = 800.0f + 0.001f * static_cast<float>(i);
        const double sum = 2.0 * g + 100.3 + g + 120.7 + g + want + g + 90.1 + g + 60.9 + g + 50.3;
        const BarLayout l = Layout(static_cast<float>(sum), kPills, BarShape::Pills,
            Widths(100.3f, 0, 120.7f, want, 90.1f, 60.9f, 50.3f), 40.0f);
        CHECK_EQ(l.targetWidth, want);
    }
}

TEST(the_right_section_keeps_a_gap_from_the_left_or_its_inner_widget_goes)
{
    const float g     = kPills.gap;
    const auto sides  = Sections({BarWidget::Zone}, {}, {BarWidget::Currency});
    // Gil against the right edge would start half a gap after zone ends.
    const float screen = g + 100.0f + g / 2.0f + 90.0f + g;
    const BarLayout l  = Layout(screen, kPills, BarShape::Pills, Widths(100, 0, 0, 0, 90, 0, 0), 0.0f, sides);
    CHECK(RectOf(l, BarWidget::Zone) != nullptr && RectOf(l, BarWidget::Currency) == nullptr);
    CheckApartAndInside(l, screen, sides);
}

TEST(end_caps_take_the_room_between_the_panels_edges_and_its_sections)
{
    for (const auto& [shape, metrics] : {std::pair{BarShape::Pills, kPills}, std::pair{BarShape::Ornate, kOrnate}})
    {
        const auto widths = Widths(100, 80, 120, 200, 90, 60, 50);
        const BarLayout caps = Layout(1920.0f, metrics, shape, widths, 0.0f, DefaultPlacement(), kCaps);
        const BarLayout none = Layout(1920.0f, metrics, shape, widths, 0.0f, DefaultPlacement(), 0);
        CHECK(Near(RectOf(caps, BarWidget::Zone)->x, metrics.cap) && Near(RectOf(none, BarWidget::Zone)->x, metrics.gap));
        const BarRect& clock = *RectOf(caps, BarWidget::EarthTime);
        CHECK(Near(clock.x + clock.width, 1920.0f - metrics.cap));
        CHECK(caps.dividers.empty() && none.dividers.empty());
    }
}

TEST(pills_with_diamonds_center_one_in_a_wider_gap_between_neighbours_of_a_section)
{
    const float g = kPills.gap, d = static_cast<float>(WholePixels(kPills.diamond));
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 80, 120, 200, 90, 60, 50), 0.0f,
        DefaultPlacement(), kBetween);
    const BarRect& zone = *RectOf(l, BarWidget::Zone);
    const BarRect& weather = *RectOf(l, BarWidget::Weather);
    CHECK(Near(weather.x, zone.x + zone.width + g + d + g)); // a gap, the diamond, a gap
    CHECK(l.dividers.size() == 4);                          // two on the left, two on the right, none by the target
    CHECK(Near(l.dividers[0], zone.x + zone.width + g + d / 2.0f));
    for (const float dx : l.dividers) // every diamond keeps a gap from both pills beside it
        for (const PlacedWidget& p : l.widgets)
            CHECK(dx + d / 2.0f + g <= p.rect.x + 0.01f || p.rect.x + p.rect.width + g <= dx - d / 2.0f + 0.01f);
    CHECK(Layout(1920.0f, kPills, BarShape::Pills, Widths(100, 80, 120, 200, 90, 60, 50), 0.0f).dividers.empty());
}

TEST(ornate_without_diamonds_keeps_its_widgets_where_they_were)
{
    const auto widths = Widths(100, 80, 120, 200, 90, 60, 50);
    const BarLayout with = Layout(1920.0f, kOrnate, BarShape::Ornate, widths, 0.0f);
    const BarLayout none = Layout(1920.0f, kOrnate, BarShape::Ornate, widths, 0.0f, DefaultPlacement(), kCaps);
    CHECK(none.dividers.empty() && with.dividers.size() == 4);
    for (size_t i = 0; i < with.widgets.size(); ++i)
        CHECK(with.widgets[i].rect == none.widgets[i].rect);
}

TEST(diamonds_divide_neighbours_within_each_section_and_never_between_sections)
{
    const auto center = Sections({BarWidget::Zone}, {BarWidget::Currency, BarWidget::Bags}, {BarWidget::EarthTime});
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, SomeWidths(), 0.0f, center, kBetween);
    CHECK(l.dividers.size() == 1); // between gil and bags, in the center
    CHECK(l.dividers[0] > RectOf(l, BarWidget::Currency)->x && l.dividers[0] < RectOf(l, BarWidget::Bags)->x);
}

TEST(both_lines_shrink_together_keeping_the_font_size_ratio_when_they_do_not_fit)
{
    for (const BarShape shape : {BarShape::Ornate, BarShape::Pills})
    {
        // Ratio 1 on a 45 px bar: 18 over 18 needs more than its 28 px of ink, so both come down, the same size.
        const TextRow even = FitTextRow(MetricsFor(45, shape, 1.0f), FakeInk);
        CHECK(even.title < 18 && even.title == even.detail && even.alone == 18);
        // Ratio 0.5: the second line twice the first, both down by the same share.
        const TextRow tall = FitTextRow(MetricsFor(45, shape, 0.5f), FakeInk);
        CHECK(tall.detail > tall.title && std::fabs(static_cast<float>(tall.detail) / static_cast<float>(tall.title) - 2.0f) < 0.25f);
        const BarRow row    = PillRow(MetricsFor(45, shape));
        const LineInk title = FakeInk(true, tall.title), detail = FakeInk(false, tall.detail);
        CHECK(tall.titleY + title.top >= row.y && tall.detailY + detail.bottom <= row.y + row.height);
        CHECK((tall.detailY + detail.top) - (tall.titleY + title.bottom) >= 3.0f - 0.01f);
        // Ratio 2 on a tall bar fits as it is: the second line half the first.
        const TextRow small = FitTextRow(MetricsFor(64, shape, 2.0f), FakeInk);
        CHECK(small.title == WholePixels(0.40f * 64.0f) && small.detail == WholePixels(0.40f * 64.0f / 2.0f));
    }
}

TEST(each_copy_of_a_widget_takes_a_place_of_its_own)
{
    BarPlacement placement = DefaultPlacement();
    const std::optional<PanelWidget> copy = AddCopy(placement, BarWidget::Currency);
    CHECK(copy.has_value());
    std::array<float, kPanelSlots> natural = Widths(100, 0, 120, 200, 90, 60, 50);
    natural[SlotOf(*copy)] = 70.0f;
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, natural, 0.0f, placement);
    const BarRect* first = RectOf(l, BarWidget::Currency);
    const BarRect* second = RectOf(l, *copy);
    CHECK(first != nullptr && second != nullptr);
    CHECK(Near(second->width, 70.0f) && Near(second->x, first->x + first->width + kPills.gap)); // right after it
}

TEST(widget_spacing_adds_room_between_every_widget_and_each_neighbour)
{
    const std::array<float, kPanelSlots> natural = Widths(100, 80, 120, 200, 90, 60, 50);
    for (const auto& [shape, metrics] : {std::pair{BarShape::Pills, kPills}, std::pair{BarShape::Ornate, kOrnate}})
    {
        const BarLayout plain = Layout(1920.0f, metrics, shape, natural, 0.0f);
        const BarLayout spaced = Layout(1920.0f, metrics, shape, natural, 0.0f, DefaultPlacement(), std::nullopt, 5.0f);
        CHECK(Near(RectOf(spaced, BarWidget::Zone)->x, RectOf(plain, BarWidget::Zone)->x));
        CHECK(Near(RectOf(spaced, BarWidget::Weather)->x, RectOf(plain, BarWidget::Weather)->x + 10.0f));
        CHECK(Near(RectOf(spaced, BarWidget::Job)->x, RectOf(plain, BarWidget::Job)->x + 20.0f));
        // An ornate divider stands in the middle of the room it is in.
        if (shape == BarShape::Ornate)
        {
            const BarRect& zone = *RectOf(spaced, BarWidget::Zone);
            CHECK(Near(spaced.dividers[0], zone.x + zone.width + 5.0f));
        }
    }
}

namespace
{
    // The default panel with Zone combined with Weather, and Bags with Gil.
    PanelGroups Combined()
    {
        CombineLinks links{};
        links[SlotOf(BarWidget::Zone)] = BarWidget::Weather;
        links[SlotOf(BarWidget::Bags)] = BarWidget::Currency;
        const BarPlacement p{std::vector<PanelWidget>{BarWidget::Zone, BarWidget::Weather, BarWidget::Job},
            std::vector<PanelWidget>{BarWidget::Target},
            std::vector<PanelWidget>{BarWidget::Currency, BarWidget::Bags, BarWidget::EarthTime}};
        return Combine(p, links);
    }

    BarLayout CombinedBar(float screen, BarShape shape, const BarMetrics& m, std::optional<int> separators = std::nullopt,
                          float spacing = 0.0f)
    {
        const PanelGroups g = Combined();
        return Layout(screen, m, shape, Widths(100, 80, 120, 200, 90, 60, 50), 0.0f, g.placement, separators, spacing,
            g.group);
    }
}

TEST(combined_widgets_touch_with_no_gap_or_diamond_between_them)
{
    for (const auto& [shape, metrics] : {std::pair{BarShape::Pills, kPills}, std::pair{BarShape::Ornate, kOrnate}})
        for (const int separators : {0, kBetween, kCaps, kBetween | kCaps})
        {
            const float spacing = 10.0f;
            const BarLayout l = CombinedBar(1920.0f, shape, metrics, separators, spacing);
            const BarRect zone = *RectOf(l, BarWidget::Zone), weather = *RectOf(l, BarWidget::Weather);
            const BarRect bags = *RectOf(l, BarWidget::Bags), gil = *RectOf(l, BarWidget::Currency);
            CHECK(Near(weather.x, zone.x + zone.width));
            CHECK(Near(gil.x, bags.x + bags.width)); // Gil pulled after Bags
            // Between groups, as between any neighbours: here Weather's own room and the gap or diamond.
            const BarLayout alone = Layout(1920.0f, metrics, shape, Widths(100, 80, 120, 200, 90, 60, 50), 0.0f,
                DefaultPlacement(), separators, spacing);
            const float apart = RectOf(alone, BarWidget::Job)->x - RectOf(alone, BarWidget::Weather)->x -
                                RectOf(alone, BarWidget::Weather)->width;
            CHECK(Near(RectOf(l, BarWidget::Job)->x, weather.x + weather.width + apart));
            for (const float divider : l.dividers)
                CHECK(!Near(divider, weather.x) && !Near(divider, gil.x));
            if ((separators & kBetween) != 0) CHECK_EQ(l.dividers.size(), static_cast<size_t>(2)); // one a side
        }
}

TEST(a_linked_pills_widgets_keep_no_padding_between_them_but_the_linked_padding)
{
    const PanelGroups g = Combined();
    std::array<float, kPanelSlots> linked{}, shown{};
    linked[SlotOf(BarWidget::Zone)] = 12.0f; // after Zone, before the Weather it pulls
    linked[SlotOf(BarWidget::Bags)] = 7.0f;  // after Bags, before Gil
    for (const BarWidget w : {BarWidget::Zone, BarWidget::Weather, BarWidget::Job, BarWidget::Target, BarWidget::Currency,
             BarWidget::Bags, BarWidget::EarthTime})
        shown[SlotOf(w)] = 1.0f;
    constexpr float kPad = 9.0f;
    const std::array<WidgetPads, kPanelSlots> pads = PillPads(g.placement, g.group, shown, linked, kPad);
    const auto of = [&](BarWidget w) { return pads[SlotOf(w)]; };
    CHECK(of(BarWidget::Zone).left == kPad && of(BarWidget::Zone).right == 12.0f);
    CHECK(of(BarWidget::Weather).left == 0.0f && of(BarWidget::Weather).right == kPad);
    CHECK(of(BarWidget::Bags).left == kPad && of(BarWidget::Bags).right == 7.0f);
    CHECK(of(BarWidget::Currency).left == 0.0f && of(BarWidget::Currency).right == kPad);
    CHECK(of(BarWidget::Job).left == kPad && of(BarWidget::Job).right == kPad); // in a pill of its own
    // A widget of the pill with nothing to show leaves its neighbour the edge.
    shown[SlotOf(BarWidget::Weather)] = 0.0f;
    CHECK(PillPads(g.placement, g.group, shown, linked, kPad)[SlotOf(BarWidget::Zone)].right == kPad);
}

TEST(a_combined_pill_that_does_not_fit_goes_whole)
{
    // Room on the left for Zone but not Weather beside it: both go, and the right side's Bags and Gil go together.
    const float g = kPills.gap;
    const BarLayout left = CombinedBar(g + 100.0f + 40.0f, BarShape::Pills, kPills);
    CHECK(RectOf(left, BarWidget::Zone) == nullptr && RectOf(left, BarWidget::Weather) == nullptr);
    const BarLayout right = CombinedBar(g + 50.0f + g + 60.0f + 40.0f, BarShape::Pills, kPills);
    CHECK(RectOf(right, BarWidget::EarthTime) != nullptr);
    CHECK(RectOf(right, BarWidget::Bags) == nullptr && RectOf(right, BarWidget::Currency) == nullptr);
    // The center drops a combined pill whole from its end.
    CombineLinks links{};
    links[SlotOf(BarWidget::Weather)] = BarWidget::Job;
    const BarPlacement p{std::vector<PanelWidget>{}, std::vector<PanelWidget>{BarWidget::Zone, BarWidget::Weather, BarWidget::Job},
        std::vector<PanelWidget>{}};
    const PanelGroups cg = Combine(p, links);
    const BarLayout center = Layout(2.0f * g + 100.0f + g + 80.0f + 60.0f, kPills, BarShape::Pills,
        Widths(100, 80, 120, 0, 0, 0, 0), 0.0f, cg.placement, std::nullopt, 0.0f, cg.group);
    CHECK(RectOf(center, BarWidget::Zone) != nullptr);
    CHECK(RectOf(center, BarWidget::Weather) == nullptr && RectOf(center, BarWidget::Job) == nullptr);
    CHECK(left.dividers.empty() && right.dividers.empty());
}

TEST(a_combined_pill_hovers_and_pins_as_one_widget_named_by_its_first)
{
    const BarLayout l = CombinedBar(1920.0f, BarShape::Pills, kPills);
    const BarRect zone = *RectOf(l, BarWidget::Zone), weather = *RectOf(l, BarWidget::Weather);
    CHECK(WidgetAt(l, weather.x + 5.0f, 18.0f) == std::optional<PanelWidget>(BarWidget::Zone));
    CHECK(WidgetAt(l, zone.x + 5.0f, 18.0f) == std::optional<PanelWidget>(BarWidget::Zone));
    const std::optional<BarRect> pill = GroupRect(l, BarWidget::Zone);
    CHECK(pill && Near(pill->x, zone.x) && Near(pill->width, zone.width + weather.width));
    CHECK(!GroupRect(l, BarWidget::Weather)); // no pill is named by it
    // Only Weather has a popup: its pill opens it under Zone's name, and a pin holds while Zone is hidden.
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Weather)] = true;
    SharePopups(popups, Combined().group);
    CHECK(popups[SlotOf(BarWidget::Zone)]);
    BarInput input;
    input.Frame(l, popups);
    const BarRect pin{weather.x + 250.0f, 52.0f, 40.0f, 20.0f};
    input.Move(weather.x + 5.0f, 18.0f);
    SetPopup(input, BarRect{weather.x, 44.0f, 300.0f, 200.0f}, {}, {}, pin);
    CHECK(input.Press(pin.x + 5.0f, pin.y + 5.0f, true));
    CHECK(input.Pinned() == std::optional<PanelWidget>(BarWidget::Zone));
    const PanelGroups g = Combined();
    const BarLayout hiddenZone = Layout(1920.0f, kPills, BarShape::Pills, Widths(0, 80, 120, 200, 90, 60, 50), 0.0f,
        g.placement, std::nullopt, 0.0f, g.group);
    input.Frame(hiddenZone, popups);
    CHECK(input.Pinned() == std::optional<PanelWidget>(BarWidget::Zone));
}

TEST(a_widget_in_a_combined_pill_knows_which_sides_it_shares)
{
    const BarLayout l = CombinedBar(1920.0f, BarShape::Pills, kPills);
    const auto joins = [&](BarWidget w) { const PillJoins j = JoinsOf(l, w); return std::pair{j.before, j.after}; };
    CHECK(joins(BarWidget::Zone) == std::pair(false, true));
    CHECK(joins(BarWidget::Weather) == std::pair(true, false));
    CHECK(joins(BarWidget::Job) == std::pair(false, false));
    CHECK(joins(BarWidget::Bags) == std::pair(false, true));
    CHECK(joins(BarWidget::Currency) == std::pair(true, false));
}

TEST(a_press_on_the_settings_widget_asks_for_the_settings_window_and_pins_nothing)
{
    std::array<float, kPanelSlots> widths = Widths(100, 0, 120, 200, 90, 60, 50);
    widths[SlotOf(BarWidget::Settings)]   = 30.0f;
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, widths, 0.0f);
    const BarRect cog = *RectOf(l, BarWidget::Settings);
    BarInput input;
    input.Frame(l, {});
    input.Move(cog.x + 5.0f, 18.0f);
    CHECK(!input.Open());
    CHECK(input.Press(cog.x + 5.0f, 18.0f, true));
    CHECK((input.TakePresses() == std::vector<std::string>{kOpenSettingsPress}) && !input.Pinned());
    CHECK(input.Press(cog.x + 5.0f, 18.0f, false) && input.TakePresses().empty()); // a right press tells nothing
}

// The card: the Settings widget, and a widget with nothing to pop out (Party alone, Target with nothing targeted),
// light as the pointer goes over them as the others do.
TEST(a_widget_lights_under_the_pointer_whether_or_not_it_has_a_popup)
{
    std::array<float, kPanelSlots> widths = Widths(100, 0, 120, 200, 90, 60, 50);
    widths[SlotOf(BarWidget::Settings)]   = 30.0f;
    const BarLayout l = Layout(1920.0f, kPills, BarShape::Pills, widths, 0.0f);
    std::array<bool, kPanelSlots> popups{};
    popups[SlotOf(BarWidget::Zone)] = true;
    BarInput input;
    input.Frame(l, popups);
    const auto over = [&](BarWidget w) { input.Move(RectOf(l, w)->x + 5.0f, 18.0f); };
    const auto lit  = [&](BarWidget w) { return input.Lit(w, popups[SlotOf(w)]); };
    over(BarWidget::Settings);
    CHECK(lit(BarWidget::Settings) && !lit(BarWidget::Target) && !lit(BarWidget::Zone));
    over(BarWidget::Target);
    CHECK(lit(BarWidget::Target) && !lit(BarWidget::Settings));
    over(BarWidget::Zone);
    CHECK(lit(BarWidget::Zone) && !lit(BarWidget::Target));
    // Off the bar, a pinned one stays lit while it has a popup to show.
    input.Pin(BarWidget::Zone);
    input.Pin(BarWidget::Target);
    input.Leave();
    CHECK(lit(BarWidget::Zone) && !lit(BarWidget::Target) && !lit(BarWidget::Settings));
}

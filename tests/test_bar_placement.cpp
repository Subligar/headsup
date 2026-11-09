#include "bar_placement.h"
#include "test.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    constexpr BarSection kLeft = BarSection::Left, kCenter = BarSection::Center, kRight = BarSection::Right;
    using List = std::vector<PanelWidget>;

    const List& In(const BarPlacement& p, BarSection s) { return p[static_cast<int>(s)]; }

    // The widgets the bar had before the newer ones were added, in the section's order.
    List Original(const List& section)
    {
        List kept;
        for (const PanelWidget w : section)
            if (static_cast<int>(w.kind) <= static_cast<int>(BarWidget::EarthTime)) kept.push_back(w);
        return kept;
    }
}

TEST(the_default_placement_is_todays_bar_with_every_widget_once)
{
    const BarPlacement& d = DefaultPlacement();
    CHECK(Original(In(d, kLeft)) == (List{BarWidget::Zone, BarWidget::Weather}));
    CHECK(In(d, kCenter) == (List{BarWidget::Target})); // the target alone in the center
    CHECK(Original(In(d, kRight)) == (List{BarWidget::Currency, BarWidget::Bags, BarWidget::EarthTime}));
    for (int w = 0; w < kBarWidgetCount; ++w)
    {
        int seen = 0;
        for (const List& section : d)
            seen += static_cast<int>(std::count(section.begin(), section.end(), static_cast<BarWidget>(w)));
        // Game help has its own bar, and a created widget is on it only once made.
        CHECK_EQ(seen, OnTheBar(static_cast<BarWidget>(w)) && static_cast<BarWidget>(w) != BarWidget::Custom ? 1 : 0);
        CHECK(BarWidgetName(static_cast<BarWidget>(w)) != nullptr);
    }
}

TEST(a_placement_reads_back_what_it_wrote)
{
    const BarPlacement& d = DefaultPlacement();
    CHECK_EQ(PlacementNames(In(d, kCenter)), std::string("target"));
    CHECK(PlacementFrom({PlacementNames(In(d, kLeft)), PlacementNames(In(d, kCenter)), PlacementNames(In(d, kRight))}) ==
          d);
    CHECK(PlacementFrom({"", "", ""}) == d); // an old file: every widget in its default place
}

TEST(unknown_and_repeated_names_are_dropped_and_a_widget_named_nowhere_goes_home_at_the_end)
{
    const BarPlacement p = PlacementFrom({"job moon job", "zone", "earthTime target"});
    CHECK(In(p, kLeft).front() == BarWidget::Job && Original(In(p, kLeft)) == (List{BarWidget::Weather})); // weather named nowhere
    CHECK(In(p, kCenter) == (List{BarWidget::Zone}));
    CHECK(Original(In(p, kRight)) == (List{BarWidget::EarthTime, BarWidget::Target, BarWidget::Currency, BarWidget::Bags}));
}

TEST(a_file_that_placed_game_help_in_a_section_loses_it_there)
{
    // Game help left the sections for its own bar under the top bar; a file from before still names it.
    const BarPlacement p = PlacementFrom({"zone gameHelp", "target", ""});
    for (const List& section : p)
        CHECK(std::count(section.begin(), section.end(), BarWidget::GameHelp) == 0);
    CHECK(In(p, kLeft).front() == BarWidget::Zone && In(p, kCenter) == (List{BarWidget::Target}));
}

TEST(a_widget_moves_up_and_down_within_its_section)
{
    BarPlacement p = DefaultPlacement();
    const List left = In(p, kLeft);
    CHECK(MoveWidget(p, kLeft, 1, kLeft, 0)); // the weather to the front
    CHECK(Original(In(p, kLeft)) == (List{BarWidget::Weather, BarWidget::Zone}));
    CHECK(MoveWidget(p, kLeft, 0, kLeft, 2)); // back down, before the third: after Zone
    CHECK(Original(In(p, kLeft)) == (List{BarWidget::Zone, BarWidget::Weather}));
    CHECK(MoveWidget(p, kLeft, 0, kLeft, In(p, kLeft).size())); // to the end
    CHECK(In(p, kLeft).back() == BarWidget::Zone && In(p, kLeft).size() == left.size());
}

TEST(a_widget_moves_across_sections_and_into_an_empty_one)
{
    BarPlacement p = DefaultPlacement();
    CHECK(MoveWidget(p, kRight, 0, kLeft, 1)); // Gil after Zone
    CHECK(In(p, kLeft)[1] == BarWidget::Currency && In(p, kRight).front() == BarWidget::Bags);
    CHECK(MoveWidget(p, kCenter, 0, kRight, In(p, kRight).size())); // the target to the right group's end
    CHECK(In(p, kCenter).empty() && In(p, kRight).back() == BarWidget::Target);
    CHECK(MoveWidget(p, kRight, 0, kCenter, 0)); // into the empty center
    CHECK(In(p, kCenter) == (List{BarWidget::Bags}));
}

TEST(a_drop_where_the_widget_already_is_or_out_of_range_changes_nothing)
{
    BarPlacement p = DefaultPlacement();
    CHECK(!MoveWidget(p, kLeft, 1, kLeft, 1) && !MoveWidget(p, kLeft, 1, kLeft, 2)); // before or after itself
    CHECK(!MoveWidget(p, kLeft, In(p, kLeft).size(), kRight, 0));                    // no such widget
    CHECK(!MoveWidget(p, kLeft, 0, kRight, In(p, kRight).size() + 1));                // past the end
    CHECK(p == DefaultPlacement());
}

TEST(a_copy_goes_after_the_last_of_its_widget_with_the_lowest_number_free_and_four_at_most)
{
    BarPlacement p = DefaultPlacement();
    const auto [section, at] = PlaceOf(p, BarWidget::Currency);
    CHECK(AddCopy(p, BarWidget::Currency) == (PanelWidget{BarWidget::Currency, 1}));
    CHECK(p[section][at + 1] == (PanelWidget{BarWidget::Currency, 1}));
    CHECK(AddCopy(p, BarWidget::Currency) == (PanelWidget{BarWidget::Currency, 2}));
    CHECK(p[section][at + 2] == (PanelWidget{BarWidget::Currency, 2}));
    // Taken off, a copy's number is the next one given.
    CHECK(RemoveCopy(p, PanelWidget{BarWidget::Currency, 1}) && !Placed(p, PanelWidget{BarWidget::Currency, 1}));
    CHECK(AddCopy(p, BarWidget::Currency) == (PanelWidget{BarWidget::Currency, 1}));
    CHECK(AddCopy(p, BarWidget::Currency) == (PanelWidget{BarWidget::Currency, 3}));
    CHECK(!AddCopy(p, BarWidget::Currency).has_value());
    // The first copy always stays, and the target and game help have none.
    CHECK(!RemoveCopy(p, BarWidget::Currency) && Placed(p, BarWidget::Currency));
    CHECK(!AddCopy(p, BarWidget::Target).has_value() && !AddCopy(p, BarWidget::GameHelp).has_value());
}

TEST(copies_read_back_by_their_numbers_and_a_number_out_of_range_is_dropped)
{
    BarPlacement p = DefaultPlacement();
    AddCopy(p, BarWidget::Currency);
    AddCopy(p, BarWidget::Currency);
    const std::array<std::string, kBarSectionCount> names{PlacementNames(In(p, kLeft)), PlacementNames(In(p, kCenter)),
        PlacementNames(In(p, kRight))};
    CHECK(names[static_cast<int>(kRight)].find("gil gil:2 gil:3") != std::string::npos);
    CHECK(PlacementFrom(names) == p);
    const BarPlacement odd = PlacementFrom({"gil:5 gil:1 target:2 gil:x zone:2", "", ""});
    CHECK(In(odd, kLeft).front() == (PanelWidget{BarWidget::Zone, 1})); // and the first copies after, each in its place
    for (int copy = 1; copy < kMostCopies; ++copy)
        CHECK(!Placed(odd, PanelWidget{BarWidget::Currency, copy}));
    CHECK(PlaceOf(odd, BarWidget::Target).first == static_cast<int>(kCenter));
}

namespace
{
    // Links by slot, from (widget, the one combined with it) pairs.
    CombineLinks Links(std::initializer_list<std::pair<PanelWidget, PanelWidget>> pairs)
    {
        CombineLinks links{};
        for (const auto& [from, to] : pairs)
            links[SlotOf(from)] = to;
        return links;
    }
}

TEST(a_combined_widget_leaves_its_place_for_one_right_after_the_widget_it_is_combined_with)
{
    BarPlacement p{List{BarWidget::Zone, BarWidget::Weather}, List{BarWidget::Target}, List{BarWidget::Currency, BarWidget::Party}};
    const PanelGroups g = Combine(p, Links({{BarWidget::Zone, BarWidget::Party}}));
    CHECK(In(g.placement, kLeft) == (List{BarWidget::Zone, BarWidget::Party, BarWidget::Weather}));
    CHECK(In(g.placement, kRight) == (List{BarWidget::Currency}));
    CHECK(g.group[SlotOf(BarWidget::Party)] == PanelWidget{BarWidget::Zone});
    CHECK(g.group[SlotOf(BarWidget::Zone)] == PanelWidget{BarWidget::Zone});
    CHECK(g.group[SlotOf(BarWidget::Weather)] == PanelWidget{BarWidget::Weather});
    CHECK(Members(g, BarWidget::Zone) == (List{BarWidget::Zone, BarWidget::Party}));
    CHECK(Members(g, BarWidget::Currency) == (List{BarWidget::Currency}));
}

TEST(combined_widgets_chain_and_copies_combine_like_any_widget)
{
    const PanelWidget world2{BarWidget::World, 1};
    BarPlacement p{List{BarWidget::Zone, BarWidget::Weather, world2}, List{}, List{BarWidget::Party}};
    const PanelGroups g = Combine(p, Links({{BarWidget::Zone, BarWidget::Party}, {BarWidget::Party, world2}}));
    CHECK(In(g.placement, kLeft) == (List{BarWidget::Zone, BarWidget::Party, world2, BarWidget::Weather}));
    CHECK(g.group[SlotOf(world2)] == PanelWidget{BarWidget::Zone});
}

TEST(a_link_that_would_close_a_ring_or_pull_a_widget_twice_is_dropped)
{
    BarPlacement p{List{BarWidget::Zone, BarWidget::Weather, BarWidget::Party}, List{}, List{}};
    // Zone pulls Conquest; Conquest pulling Zone back would leave neither a place.
    const PanelGroups ring = Combine(p, Links({{BarWidget::Zone, BarWidget::Party}, {BarWidget::Party, BarWidget::Zone}}));
    CHECK(In(ring.placement, kLeft) == (List{BarWidget::Zone, BarWidget::Party, BarWidget::Weather}));
    // Weather asks for Conquest too: the lower slot, Zone, has it.
    const PanelGroups twice = Combine(p, Links({{BarWidget::Weather, BarWidget::Party}, {BarWidget::Zone, BarWidget::Party}}));
    CHECK(In(twice.placement, kLeft) == (List{BarWidget::Zone, BarWidget::Party, BarWidget::Weather}));
}

TEST(the_target_an_unplaced_copy_and_a_widget_itself_never_combine)
{
    const PanelWidget gil2{BarWidget::Currency, 1};
    BarPlacement p{List{BarWidget::Zone}, List{BarWidget::Target}, List{BarWidget::Currency}};
    const PanelGroups g = Combine(p, Links({{BarWidget::Zone, BarWidget::Target}, {BarWidget::Target, BarWidget::Currency},
                                            {BarWidget::Currency, gil2}, {gil2, BarWidget::Zone}, {BarWidget::Zone, BarWidget::Zone}}));
    CHECK(g.placement == p);
    for (int slot = 0; slot < kPanelSlots; ++slot)
        CHECK(g.group[slot] == WidgetInSlot(slot));
}

TEST(a_combined_widget_names_its_partner_as_the_settings_file_does)
{
    CHECK(PanelWidgetNamed("party") == std::optional<PanelWidget>(BarWidget::Party));
    CHECK(PanelWidgetNamed("world:3") == std::optional<PanelWidget>(PanelWidget{BarWidget::World, 2}));
    CHECK(!PanelWidgetNamed("") && !PanelWidgetNamed("target:2") && !PanelWidgetNamed("nothing"));
}

TEST(every_copy_of_every_widget_has_a_slot_of_its_own)
{
    std::vector<bool> taken(kPanelSlots);
    for (int w = 0; w < kBarWidgetCount; ++w)
        for (int copy = 0; copy < CopiesOf(static_cast<BarWidget>(w)); ++copy)
        {
            const PanelWidget widget{static_cast<BarWidget>(w), copy};
            const int slot = SlotOf(widget);
            CHECK(slot >= 0 && slot < kPanelSlots && !taken[static_cast<size_t>(slot)]);
            taken[static_cast<size_t>(slot)] = true;
            CHECK(WidgetInSlot(slot) == widget);
        }
    CHECK(std::ranges::all_of(taken, [](bool t) { return t; }));
    CHECK_EQ(CopiesOf(BarWidget::Custom), kMostCreatedWidgets);
}

TEST(a_created_widget_goes_at_the_right_sections_end_with_the_lowest_number_free_up_to_the_most)
{
    BarPlacement p = DefaultPlacement();
    for (const List& section : p)
        CHECK(std::ranges::none_of(section, [](PanelWidget w) { return w.kind == BarWidget::Custom; }));
    CHECK(AddCreated(p) == (PanelWidget{BarWidget::Custom, 0}));
    CHECK(In(p, kRight).back() == (PanelWidget{BarWidget::Custom, 0}));
    MoveWidget(p, kRight, In(p, kRight).size() - 1, kLeft, 0);
    CHECK(AddCreated(p) == (PanelWidget{BarWidget::Custom, 1}));
    CHECK(In(p, kRight).back() == (PanelWidget{BarWidget::Custom, 1})); // not after the first, which moved
    // Every created widget can be taken off, its number given again.
    CHECK(RemoveCopy(p, PanelWidget{BarWidget::Custom, 0}) && !Placed(p, PanelWidget{BarWidget::Custom, 0}));
    CHECK(AddCreated(p) == (PanelWidget{BarWidget::Custom, 0}));
    while (AddCreated(p)) {}
    for (int copy = 0; copy < kMostCreatedWidgets; ++copy)
        CHECK(Placed(p, PanelWidget{BarWidget::Custom, copy}));
    // A clone of one goes after the last of them, as any copy does, while there is room.
    CHECK(RemoveCopy(p, PanelWidget{BarWidget::Custom, 5}));
    CHECK(AddCopy(p, BarWidget::Custom) == (PanelWidget{BarWidget::Custom, 5}));
    CHECK(!AddCopy(p, BarWidget::Custom));
}

TEST(created_widgets_read_back_by_their_numbers_past_nine)
{
    CHECK(PanelWidgetNamed("custom") == std::optional(PanelWidget{BarWidget::Custom, 0}));
    CHECK(PanelWidgetNamed("custom:20") == std::optional(PanelWidget{BarWidget::Custom, kMostCreatedWidgets - 1}));
    CHECK(PanelWidgetName(PanelWidget{BarWidget::Custom, 11}) == "custom:12");
    CHECK(!PanelWidgetNamed("custom:21") && !PanelWidgetNamed("custom:1") && !PanelWidgetNamed("gil:12") &&
          !PanelWidgetNamed("custom:2x"));
    BarPlacement p = DefaultPlacement();
    for (int i = 0; i < 12; ++i)
        AddCreated(p);
    const std::array<std::string, kBarSectionCount> names{PlacementNames(In(p, kLeft)), PlacementNames(In(p, kCenter)),
        PlacementNames(In(p, kRight))};
    CHECK(PlacementFrom(names) == p);
}

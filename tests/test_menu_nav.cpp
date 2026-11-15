#include "menu_nav.h"
#include "test.h"

#include <optional>
#include <vector>

using namespace headsup;

namespace
{
    // A list of four entries (the last scrolled out of view), three buttons to its right, and a second list past them
    // whose first entry is scrolled out of view.
    enum : size_t
    {
        kEntry0,
        kEntry1,
        kEntry2,
        kEntry3,
        kButton0,
        kButton1,
        kButton2,
        kOtherHidden,
        kOtherShown,
    };
    const std::vector<NavBox> kWindow{
        {0, 0, 100, 20, 0},
        {0, 25, 100, 45, 0},
        {0, 50, 100, 70, 0},
        {0, 75, 100, 95, 0, false},
        {120, 0, 200, 20},
        {120, 30, 200, 50},
        {120, 68, 200, 88},
        {220, 0, 300, 20, 1, false},
        {220, 25, 300, 45, 1},
    };
}

TEST(down_and_up_step_through_a_list_and_wrap_at_its_ends)
{
    CHECK(NextFocus(kWindow, kEntry0, MenuAction::Down) == kEntry1);
    CHECK(NextFocus(kWindow, kEntry1, MenuAction::Up) == kEntry0);
    // Its own entries scrolled out of view are still reached: the list scrolls to them.
    CHECK(NextFocus(kWindow, kEntry2, MenuAction::Down) == kEntry3);
    CHECK(NextFocus(kWindow, kEntry3, MenuAction::Down) == kEntry0);
    CHECK(NextFocus(kWindow, kEntry0, MenuAction::Up) == kEntry3);
}

TEST(a_list_of_one_entry_has_nowhere_to_wrap_to)
{
    const std::vector<NavBox> boxes{{0, 0, 100, 20, 0}, {0, 40, 100, 60}};
    CHECK(!NextFocus(boxes, 0, MenuAction::Down));
    CHECK(!NextFocus(boxes, 0, MenuAction::Up));
}

TEST(right_from_a_list_entry_picks_the_button_nearest_it_up_and_down)
{
    CHECK(NextFocus(kWindow, kEntry0, MenuAction::Right) == kButton0);
    // Entry 1's middle (35) is 25 from button 0's (10) and 5 from button 1's (40).
    CHECK(NextFocus(kWindow, kEntry1, MenuAction::Right) == kButton1);
    CHECK(NextFocus(kWindow, kEntry2, MenuAction::Right) == kButton2);
}

TEST(left_from_a_button_goes_back_into_the_list_beside_it)
{
    CHECK(NextFocus(kWindow, kButton0, MenuAction::Left) == kEntry0);
    CHECK(NextFocus(kWindow, kButton2, MenuAction::Left) == kEntry2);
}

TEST(another_lists_entries_out_of_view_are_skipped)
{
    // Button 0 lines up with the second list's first entry, which is scrolled away: its second entry is taken.
    CHECK(NextFocus(kWindow, kButton0, MenuAction::Right) == kOtherShown);
    // From button 2, the first list's last entry lines up best, but it is out of view.
    CHECK(NextFocus(kWindow, kButton2, MenuAction::Left) == kEntry2);
}

TEST(buttons_step_up_and_down_their_column)
{
    CHECK(NextFocus(kWindow, kButton0, MenuAction::Down) == kButton1);
    CHECK(NextFocus(kWindow, kButton2, MenuAction::Up) == kButton1);
}

TEST(nothing_that_way_moves_nowhere)
{
    CHECK(!NextFocus(kWindow, kButton0, MenuAction::Up));
    CHECK(!NextFocus(kWindow, kOtherShown, MenuAction::Right));
    CHECK(!NextFocus(kWindow, kEntry0, MenuAction::Left));
    CHECK(!NextFocus(kWindow, kEntry0, MenuAction::Confirm));
}

TEST(only_a_box_wholly_past_the_edge_counts)
{
    // The second box starts inside the first: it is not to its right.
    const std::vector<NavBox> boxes{{0, 0, 20, 20}, {10, 0, 40, 20}};
    CHECK(!NextFocus(boxes, 0, MenuAction::Right));
}

TEST(being_in_line_counts_for_more_than_being_near)
{
    // Lined up 80 px away, against 10 px away but 60 px off the line: lined up wins, as long as a pixel across counts
    // for more than 80 / 70 of one along.
    const std::vector<NavBox> boxes{{0, 0, 20, 20}, {100, 0, 120, 20}, {30, 60, 50, 80}};
    CHECK(NextFocus(boxes, 0, MenuAction::Right) == size_t{1});
    // Equally in line, the nearer wins.
    const std::vector<NavBox> row{{0, 0, 20, 20}, {100, 0, 120, 20}, {50, 0, 70, 20}};
    CHECK(NextFocus(row, 0, MenuAction::Right) == size_t{2});
}

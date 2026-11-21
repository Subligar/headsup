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

namespace
{
    // As Ashita hands them over: Windows' virtual keys, XInput's button bits by their place (the d-pad 0 to 3, the
    // bumpers 8 and 9, A 12, B 13, X 14) and its left stick as 18 (across) and 19 (up), DirectInput's DIJOYSTATE
    // offsets (the hat at 32 in hundredths of a degree, centred 0xFFFF; button n at 48 + n, down 0x80).
    MenuEvent Key(uint32_t vk, bool down = true) { return {InputSource::Keyboard, vk, down ? 1 : 0}; }
    MenuEvent Pad(uint32_t button, int32_t state) { return {InputSource::XInput, button, state}; }
    MenuEvent Hat(int32_t hundredths) { return {InputSource::DirectInput, 32, hundredths}; }
    MenuEvent DirectButton(uint32_t n, bool down) { return {InputSource::DirectInput, 48 + n, down ? 0x80 : 0}; }
    constexpr int32_t kHatCentred = 0xFFFF;
}

TEST(the_arrows_enter_and_escape_are_the_windows_keys)
{
    MenuInput input;
    CHECK(input.Feed(Key(0x26), 0.0) == MenuAction::Up);
    CHECK(input.Feed(Key(0x28), 0.0) == MenuAction::Down);
    CHECK(input.Feed(Key(0x25), 0.0) == MenuAction::Left);
    CHECK(input.Feed(Key(0x27), 0.0) == MenuAction::Right);
    CHECK(input.Feed(Key(0x0D), 0.0) == MenuAction::Confirm);
    CHECK(input.Feed(Key(0x1B), 0.0) == MenuAction::Cancel);
    CHECK(input.Feed(Key(0x26, false), 0.0) == MenuAction::None);
    CHECK(input.Feed(Key('A'), 0.0) == MenuAction::None);
    // Windows repeats a held key itself: each of its presses moves.
    CHECK(input.Feed(Key(0x28), 0.1) == MenuAction::Down);
    CHECK(input.Due(10.0) == MenuAction::None);
}

TEST(xinput_a_b_and_the_bumpers_confirm_cancel_and_change_tabs)
{
    MenuInput input;
    CHECK(input.Feed(Pad(12, 1), 0.0) == MenuAction::Confirm);
    CHECK(input.Feed(Pad(12, 0), 0.0) == MenuAction::None);
    CHECK(input.Feed(Pad(13, 1), 0.0) == MenuAction::Cancel);
    CHECK(input.Feed(Pad(8, 1), 0.0) == MenuAction::TabLeft);
    CHECK(input.Feed(Pad(9, 1), 0.0) == MenuAction::TabRight);
    CHECK(input.Feed(Pad(14, 1), 0.0) == MenuAction::None);
    // The triggers are not the windows'.
    CHECK(input.Feed(Pad(16, 255), 0.0) == MenuAction::None);
    CHECK(input.Due(10.0) == MenuAction::None);
}

TEST(a_held_dpad_moves_at_once_then_repeats_after_the_delay)
{
    MenuInput input;
    CHECK(input.Feed(Pad(1, 1), 5.0) == MenuAction::Down);
    CHECK(input.Due(5.0) == MenuAction::None);
    CHECK(input.Due(5.0 + kPadRepeatDelay - 0.01) == MenuAction::None);
    const double first = 5.0 + kPadRepeatDelay;
    CHECK(input.Due(first) == MenuAction::Down);
    CHECK(input.Due(first) == MenuAction::None);
    CHECK(input.Due(first + kPadRepeatInterval - 0.01) == MenuAction::None);
    CHECK(input.Due(first + kPadRepeatInterval) == MenuAction::Down);
    // Let go, it stops.
    CHECK(input.Feed(Pad(1, 0), first + kPadRepeatInterval) == MenuAction::None);
    CHECK(input.Due(first + 10.0) == MenuAction::None);
}

TEST(the_last_dpad_direction_pressed_is_the_one_held)
{
    MenuInput input;
    CHECK(input.Feed(Pad(0, 1), 0.0) == MenuAction::Up);
    CHECK(input.Feed(Pad(3, 1), 0.2) == MenuAction::Right);
    // Up let go while Right is held: Right goes on, its delay counted from its own press.
    CHECK(input.Feed(Pad(0, 0), 0.3) == MenuAction::None);
    CHECK(input.Due(0.2 + kPadRepeatDelay) == MenuAction::Right);
    CHECK(input.Feed(Pad(3, 0), 1.0) == MenuAction::None);
    CHECK(input.Due(5.0) == MenuAction::None);
}

TEST(the_left_stick_moves_past_its_dead_zone_and_repeats_like_the_dpad)
{
    MenuInput input;
    CHECK(input.Feed(Pad(19, kStickDeadZone), 0.0) == MenuAction::None);
    CHECK(input.Due(10.0) == MenuAction::None);
    // Up is positive; past the dead zone it moves, once, however much further it goes.
    CHECK(input.Feed(Pad(19, kStickDeadZone + 1), 10.0) == MenuAction::Up);
    CHECK(input.Feed(Pad(19, 32767), 10.1) == MenuAction::None);
    CHECK(input.Due(10.0 + kPadRepeatDelay) == MenuAction::Up);
    // Pushed more across than up, it goes across.
    CHECK(input.Feed(Pad(18, -32768), 11.0) == MenuAction::Left);
    CHECK(input.Feed(Pad(19, 0), 11.1) == MenuAction::None);
    CHECK(input.Feed(Pad(18, 0), 11.2) == MenuAction::None);
    CHECK(input.Due(20.0) == MenuAction::None);
    CHECK(input.Feed(Pad(19, -20000), 21.0) == MenuAction::Down);
    CHECK(input.Feed(Pad(18, 20000), 21.0) == MenuAction::None); // as far across as down: still down
}

TEST(a_directinput_hat_moves_by_its_angle_and_a_diagonal_stops_it)
{
    MenuInput input;
    CHECK(input.Feed(Hat(0), 0.0) == MenuAction::Up);
    CHECK(input.Feed(Hat(0), 0.1) == MenuAction::None);
    CHECK(input.Feed(Hat(9000), 0.2) == MenuAction::Right);
    CHECK(input.Feed(Hat(4500), 0.3) == MenuAction::None);
    CHECK(input.Due(5.0) == MenuAction::None);
    CHECK(input.Feed(Hat(18000), 6.0) == MenuAction::Down);
    CHECK(input.Due(6.0 + kPadRepeatDelay) == MenuAction::Down);
    CHECK(input.Feed(Hat(kHatCentred), 7.0) == MenuAction::None);
    CHECK(input.Due(8.0) == MenuAction::None);
    // Some drivers give the centre as -1 in all 32 bits.
    CHECK(input.Feed(Hat(27000), 9.0) == MenuAction::Left);
    CHECK(input.Feed(Hat(-1), 9.1) == MenuAction::None);
    CHECK(input.Due(20.0) == MenuAction::None);
}

TEST(directinput_buttons_follow_an_xbox_pads_order)
{
    MenuInput input;
    CHECK(input.Feed(DirectButton(0, true), 0.0) == MenuAction::Confirm);
    CHECK(input.Feed(DirectButton(0, false), 0.0) == MenuAction::None);
    CHECK(input.Feed(DirectButton(1, true), 0.0) == MenuAction::Cancel);
    CHECK(input.Feed(DirectButton(4, true), 0.0) == MenuAction::TabLeft);
    CHECK(input.Feed(DirectButton(5, true), 0.0) == MenuAction::TabRight);
    CHECK(input.Feed(DirectButton(2, true), 0.0) == MenuAction::None);
}

TEST(reset_lets_go_of_every_hold)
{
    MenuInput input;
    CHECK(input.Feed(Pad(2, 1), 0.0) == MenuAction::Left);
    CHECK(input.Feed(Pad(19, 32767), 0.0) == MenuAction::Up);
    input.Reset();
    CHECK(input.Due(10.0) == MenuAction::None);
    // The stick is back in its middle as far as the windows know.
    CHECK(input.Feed(Pad(18, 0), 10.0) == MenuAction::None);
}

TEST(a_release_is_kept_from_the_game_only_when_its_press_was)
{
    InputTaken taken;
    CHECK(taken.Keep(Pad(12, 1), true));
    CHECK(taken.Keep(Pad(12, 0), false)); // the window closed between: the game never saw the press
    CHECK(!taken.Keep(Pad(13, 1), false));
    CHECK(!taken.Keep(Pad(13, 0), true)); // the window opened between: the game saw the press
    // Windows' repeats of a key held from before the window opened go on to the game, and so does its release.
    CHECK(!taken.Keep(Key(0x28), false));
    CHECK(!taken.Keep(Key(0x28), true));
    CHECK(!taken.Keep(Key(0x28, false), true));
    // A hat turned from one way to another is still the one press.
    CHECK(taken.Keep(Hat(0), true));
    CHECK(taken.Keep(Hat(9000), false));
    CHECK(taken.Keep(Hat(kHatCentred), false));
    CHECK(!taken.Keep(Hat(0), false));
}

TEST(only_what_the_windows_use_is_kept_and_the_stick_while_one_is_open)
{
    InputTaken taken;
    CHECK(!taken.Keep(Key('A'), true));
    CHECK(!taken.Keep(Pad(14, 1), true));
    CHECK(!taken.Keep(Pad(16, 255), true));
    CHECK(!taken.Keep(DirectButton(2, true), true));
    CHECK(!taken.Keep({InputSource::DirectInput, 0, 100}, true)); // a DirectInput stick is left to the game
    CHECK(taken.Keep(Pad(18, 100), true));
    CHECK(taken.Keep(Pad(19, 0), true));
    CHECK(!taken.Keep(Pad(19, 32767), false));
    CHECK(taken.Keep(DirectButton(5, true), true));
}

TEST(an_input_reads_as_its_source_number_and_value)
{
    CHECK_EQ(MenuEventText(Pad(12, 1)), std::string("XInput 12 = 1"));
    CHECK_EQ(MenuEventText(Hat(9000)), std::string("DirectInput 32 = 9000"));
    CHECK_EQ(MenuEventText(Key(0x26, false)), std::string("Key 38 = 0"));
    CHECK_EQ(std::string(MenuActionName(MenuAction::TabRight)), std::string("TabRight"));
}

#include "pointer_keys.h"
#include "test.h"

#include <algorithm>

using namespace headsup;

TEST(the_pointer_stays_only_while_every_key_held_is_one_to_keep)
{
    const std::vector<uint8_t> keep = {0x11, 0x1B}; // Ctrl, Esc
    const auto held = [](std::vector<uint8_t> keys) {
        return [keys](uint8_t vk) { return std::ranges::find(keys, vk) != keys.end(); };
    };
    CHECK(KeepsPointer(held({0x11}), keep));
    CHECK(KeepsPointer(held({0x11, 0xA2}), keep)); // Windows reports left Ctrl down as both Ctrl and left Ctrl
    CHECK(KeepsPointer(held({0x11, 0x1B}), keep));
    CHECK(KeepsPointer(held({0x11, 0x01}), keep)); // the left mouse button is no key
    CHECK(!KeepsPointer(held({0x11, 0x51}), keep)); // Ctrl+Q: Q hides
    CHECK(!KeepsPointer(held({}), keep));           // a key already let go: as the game does
    CHECK(!KeepsPointer(held({0x11}), {}));
}

TEST(mouse_buttons_are_not_keys)
{
    for (const uint8_t button : {0x01, 0x02, 0x04, 0x05, 0x06})
        CHECK(MouseButton(button));
    CHECK(!MouseButton(0x1B));
}

TEST(keys_have_names_a_player_knows)
{
    CHECK(KeyName(0x11) == "Ctrl");
    CHECK(KeyName(0x12) == "Alt");
    CHECK(KeyName(0x1B) == "Esc");
    CHECK(KeyName(0x51) == "Q");
    CHECK(KeyName(0x35) == "5");
    CHECK(KeyName(0x70) == "F1");
    CHECK(KeyName(0x7B) == "F12");
    CHECK(KeyName(0x60) == "Num 0");
    CHECK(KeyName(0x26) == "Up");
    CHECK(KeyName(0xDB) == "Key DB"); // one with no name here
}

TEST(the_keys_held_are_each_key_down_once_without_mouse_buttons)
{
    // Windows reports left Ctrl down as both Ctrl and left Ctrl.
    const auto down = [](uint8_t vk) { return vk == 0x11 || vk == 0xA2 || vk == 0x51 || vk == 0x01; };
    CHECK(HeldKeys(down) == (std::vector<uint8_t>{0x11, 0x51}));
    CHECK(HeldKeys([](uint8_t) { return false; }).empty());
}

namespace
{
    constexpr uint8_t kF10 = 0x79, kF11 = 0x7A;
}

TEST(a_key_counts_once_per_press)
{
    KeyPress press;
    CHECK(!press.Pressed(kF10, false)); // the key the settings held at load, up
    CHECK(press.Pressed(kF10, true));
    CHECK(!press.Pressed(kF10, true)); // held
    CHECK(!press.Pressed(kF10, false));
    CHECK(press.Pressed(kF10, true));
}

// The settings window takes the next key pressed, so the key is down in the frame it becomes the one to watch.
TEST(the_press_that_chose_a_key_is_not_a_press_of_it)
{
    KeyPress press;
    CHECK(!press.Pressed(0, false));
    CHECK(!press.Pressed(kF10, true)); // chosen
    CHECK(!press.Pressed(kF10, true));
    CHECK(!press.Pressed(kF10, false));
    CHECK(press.Pressed(kF10, true));
    CHECK(!press.Pressed(kF11, true)); // another chosen while the first is still down
    CHECK(!press.Pressed(kF11, false));
    CHECK(press.Pressed(kF11, true));
}

TEST(no_key_is_never_pressed)
{
    KeyPress press;
    CHECK(!press.Pressed(0, true));
    CHECK(!press.Pressed(0, false));
    CHECK(!press.Pressed(0, true));
}

TEST(a_key_to_press_is_a_real_key_with_its_sides_as_one_or_none)
{
    CHECK_EQ(BindableKey(kF10), kF10);
    CHECK_EQ(BindableKey(0xA2), 0x11); // left Ctrl
    CHECK_EQ(BindableKey(0xA3), 0x11); // right Ctrl
    CHECK_EQ(BindableKey(0xA0), 0x10); // left Shift
    CHECK_EQ(BindableKey(0xA5), 0x12); // right Alt
    CHECK_EQ(BindableKey(0x51), 0x51); // Q
    CHECK_EQ(BindableKey(0x02), 0);    // the right mouse button
    CHECK_EQ(BindableKey(0), 0);
    CHECK_EQ(BindableKey(0xFF), 0);    // past the last key
    CHECK_EQ(BindableKey(999), 0);
    CHECK_EQ(BindableKey(-1), 0);
}

TEST(lock_cursor_to_window_clips_to_the_games_client_area_while_it_has_the_focus_and_lets_go_only_of_its_own_clip)
{
    const ScreenRect game{100, 50, 1380, 770}, screen{0, 0, 2560, 1440}, moved{200, 50, 1480, 770};
    // Focused with the setting on: clipped to the game, again once the game moves or something else clips.
    CHECK(CursorClipStep(true, game, screen, false) == ClipStep::Clip);
    CHECK(CursorClipStep(true, game, game, true) == ClipStep::Keep);
    CHECK(CursorClipStep(true, moved, game, true) == ClipStep::Clip);
    // A game window with no client area (minimized) is no place to keep the pointer.
    CHECK(CursorClipStep(true, ScreenRect{}, screen, false) == ClipStep::Keep);
    CHECK(CursorClipStep(true, ScreenRect{}, game, true) == ClipStep::Release);
    // Out of focus, or the setting off: HeadsUp lets go of its own clip, and leaves anyone else's alone.
    CHECK(CursorClipStep(false, game, game, true) == ClipStep::Release);
    CHECK(CursorClipStep(false, game, game, false) == ClipStep::Keep);
    CHECK(CursorClipStep(false, game, screen, false) == ClipStep::Keep);
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace headsup
{
    // What the player asked of HeadsUp's game windows, by key, pad or mouse.
    enum class MenuAction : uint8_t
    {
        None,
        Up,
        Down,
        Left,
        Right,
        Confirm,
        Cancel,
        TabLeft,
        TabRight,
    };

    constexpr int kNoList = -1;

    // A selectable element of a window, in the screen's pixels. list: which list it is an entry of, or kNoList; shown:
    // whether an entry is inside its list's scrolled view.
    struct NavBox
    {
        float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;
        int list   = kNoList;
        bool shown = true;
    };

    // The element a move goes to from current, by its index in boxes; none when nothing lies that way. Up and Down in a
    // list step through its entries in their order, from its last to its first and back. Anything else goes to the
    // nearest box wholly past current's edge that way, the one nearest across preferred, and never to another list's
    // entry scrolled out of view.
    std::optional<size_t> NextFocus(std::span<const NavBox> boxes, size_t current, MenuAction move);

    // Where an input came from, as Ashita hands it over.
    enum class InputSource : uint8_t
    {
        Keyboard,
        XInput,
        DirectInput,
    };

    // One input: a key by its virtual-key code, value 1 down and 0 up; an XInput button or axis by Ashita's number for
    // it and its state; a DirectInput pad's offset in DIJOYSTATE and its data.
    struct MenuEvent
    {
        InputSource source = InputSource::Keyboard;
        uint32_t code      = 0;
        int32_t value      = 0;
    };

    // A held d-pad or left stick moves the selection once, then again after the delay and every interval after it.
    constexpr double kPadRepeatDelay    = 0.4;
    constexpr double kPadRepeatInterval = 0.1;
    // How far the left stick goes from its middle (of 32767) before it moves the selection.
    constexpr int32_t kStickDeadZone = 16384;

    // Whether the windows act on this input (some keys, the d-pad, A, B, the bumpers and the left stick): whether it
    // holds its key or button down, and whether it is the stick, which has no press of its own.
    struct MenuControl
    {
        bool down = false;
        bool axis = false;
    };
    std::optional<MenuControl> ControlOf(const MenuEvent& event);

    // What the windows keep from the game while one is open: a key's or button's press, and its release only when its
    // press was kept, so the game never misses the release of a press it saw; the left stick whenever one is open.
    class InputTaken
    {
    public:
        bool Keep(const MenuEvent& event, bool open);

    private:
        std::map<std::pair<InputSource, uint32_t>, bool> m_Down; // the keys and buttons down: whether their press was kept
    };

    // The actions inputs ask for, a held d-pad or left stick repeating its move.
    class MenuInput
    {
    public:
        MenuAction Feed(const MenuEvent& event, double now);
        // A held move due again by now; None when none is.
        MenuAction Due(double now);
        // Lets go of every hold: no window is open to take them.
        void Reset();

    private:
        struct Hold
        {
            MenuAction move = MenuAction::None;
            double next     = 0.0;
        };
        // The move a hold makes as it changes to this one: none when it stays the same or is let go.
        static MenuAction Change(Hold& hold, MenuAction move, double now);

        Hold m_Pad, m_Stick;
        int32_t m_StickX = 0, m_StickY = 0;
    };

    // An input as Ashita gave it and an action by name, for a developer to read.
    std::string MenuEventText(const MenuEvent& event);
    const char* MenuActionName(MenuAction action);
}

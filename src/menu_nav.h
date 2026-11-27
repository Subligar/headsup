#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <set>
#include <string>
#include <string_view>
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

    // A held d-pad moves the selection once, then again after the delay and every interval after it.
    constexpr double kPadRepeatDelay    = 0.4;
    constexpr double kPadRepeatInterval = 0.1;

    // Ashita's numbers for XInput's A and B, and the DirectInput buttons the game's config tool sets for Confirm and
    // Cancel by default.
    constexpr uint32_t kPadA = 12, kPadB = 13, kDirectConfirm = 1, kDirectCancel = 2;

    // The pad buttons the player set for the game's Confirm and Cancel: an XInput button by Ashita's number for it, a
    // DirectInput button by its index; none for one left unset. By default the game's own (A and B; buttons 1 and 2).
    struct PadBindings
    {
        std::optional<uint32_t> confirm = kPadA, cancel = kPadB;
        std::optional<uint32_t> directConfirm = kDirectConfirm, directCancel = kDirectCancel;
    };
    // From padsin000, the game's pad settings in its registry key: a comma list of the button set for each of its
    // actions. A slot that cannot be read keeps its default.
    PadBindings PadBindingsFrom(std::string_view padsin);

    // What the windows keep from the game while one is open (some keys, the d-pad, the pad's Confirm and Cancel and the
    // bumpers; never a stick): a key's or button's press, and its release only when its
    // press was kept, so the game never misses the release of a press it saw.
    class InputTaken
    {
    public:
        explicit InputTaken(const PadBindings& pad = {}) : m_Pad(pad) {}
        bool Keep(const MenuEvent& event, bool open);
        // Forgets the keys let go whose release never came here, by whether each is down now (keyDown, by its virtual
        // key); true while a key whose press was kept is still down, as the game reads the keyboard past the callback.
        bool HoldsKeys(const std::function<bool(uint32_t)>& keyDown);

    private:
        PadBindings m_Pad;
        std::map<std::pair<InputSource, uint32_t>, bool> m_Down; // the keys and buttons down: whether their press was kept
    };

    // Whether a key goes to the windows' input rather than to the text box that has the keyboard (typing): Enter and
    // Escape do in a game window's box (windowsBox), so they act once a press there too, however long they are held.
    bool WindowsHearKey(uint32_t vk, bool typing, bool windowsBox);

    // The actions inputs ask for, a held d-pad repeating its move.
    class MenuInput
    {
    public:
        explicit MenuInput(const PadBindings& pad = {}) : m_Bindings(pad) {}
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
        // The action on a press of a code not held already, and none on its repeats or release.
        static MenuAction Once(std::set<uint32_t>& held, uint32_t code, bool down, MenuAction action);

        PadBindings m_Bindings;
        Hold m_Pad;
        std::set<uint32_t> m_Held; // XInput's buttons and triggers down, as a trigger tells how far it is pulled each change
        std::set<uint32_t> m_Keys; // Enter and Escape down
    };

    // An input as Ashita gave it and an action by name, for a developer to read.
    std::string MenuEventText(const MenuEvent& event);
    const char* MenuActionName(MenuAction action);
}

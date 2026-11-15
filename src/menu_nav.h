#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

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
}

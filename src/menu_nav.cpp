#include "menu_nav.h"

#include <cmath>

namespace headsup
{
    namespace
    {
        // A pixel across the move costs this many along it: a box in line beats a nearer one off to the side.
        constexpr float kCrossWeight = 2.0f;
        // Pixels a box may reach back over the edge it lies past, for layouts that land a hair apart.
        constexpr float kTouching = 0.5f;

        bool Vertical(MenuAction move) { return move == MenuAction::Up || move == MenuAction::Down; }
        bool Across(MenuAction move) { return move == MenuAction::Left || move == MenuAction::Right; }

        // How far past from's edge that way to lies, negative when it does not lie wholly past it.
        float GapFrom(const NavBox& from, const NavBox& to, MenuAction move)
        {
            switch (move)
            {
            case MenuAction::Up: return from.top - to.bottom;
            case MenuAction::Down: return to.top - from.bottom;
            case MenuAction::Left: return from.left - to.right;
            default: return to.left - from.right;
            }
        }

        std::optional<size_t> StepInList(std::span<const NavBox> boxes, size_t current, bool down)
        {
            const size_t count = boxes.size();
            for (size_t step = 1; step < count; ++step)
            {
                const size_t at = down ? (current + step) % count : (current + count - step) % count;
                if (boxes[at].list == boxes[current].list) return at;
            }
            return std::nullopt;
        }
    }

    std::optional<size_t> NextFocus(std::span<const NavBox> boxes, size_t current, MenuAction move)
    {
        if (current >= boxes.size() || !(Vertical(move) || Across(move))) return std::nullopt;
        const NavBox& from = boxes[current];
        if (Vertical(move) && from.list != kNoList) return StepInList(boxes, current, move == MenuAction::Down);
        std::optional<size_t> best;
        float bestScore = 0.0f;
        for (size_t i = 0; i < boxes.size(); ++i)
        {
            const NavBox& to = boxes[i];
            const float gap  = GapFrom(from, to, move);
            if (i == current || !to.shown || gap < -kTouching) continue;
            const float across = Vertical(move) ? std::abs((to.left + to.right) - (from.left + from.right)) / 2.0f
                                                : std::abs((to.top + to.bottom) - (from.top + from.bottom)) / 2.0f;
            const float score = gap + kCrossWeight * across;
            if (!best || score < bestScore)
            {
                best      = i;
                bestScore = score;
            }
        }
        return best;
    }
}

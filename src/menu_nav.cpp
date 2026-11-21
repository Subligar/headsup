#include "menu_nav.h"

#include <array>
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

        // Windows' virtual keys, which a pure file cannot take from its headers.
        constexpr uint32_t kKeyLeft = 0x25, kKeyUp = 0x26, kKeyRight = 0x27, kKeyDown = 0x28, kKeyEnter = 0x0D, kKeyEscape = 0x1B;
        // Ashita's XInput numbers: a button is its bit's place in XINPUT_GAMEPAD's wButtons, then the triggers (16, 17)
        // and the sticks' axes.
        constexpr uint32_t kPadUp = 0, kPadDown = 1, kPadLeft = 2, kPadRight = 3, kPadLeftBumper = 8, kPadRightBumper = 9,
                           kPadA = 12, kPadB = 13;
        // The game's own confirm and cancel buttons are the player's to set (padsin000): these are what XInput pads name them.
        constexpr uint32_t kPadConfirm = kPadA, kPadCancel = kPadB;
        // DirectInput's DIJOYSTATE: the first hat's offset, in hundredths of a degree from up, its low word all ones when
        // centred; the buttons' offsets from the first's, each down with its top bit.
        constexpr uint32_t kHatOffset = 32, kFirstButton = 48;
        constexpr uint32_t kHatCentred = 0xFFFF;
        constexpr int32_t kHatQuarter  = 9000;
        constexpr int32_t kButtonDown = 0x80;
        // DirectInput numbers a pad's buttons its own way: these are an Xbox pad's (A, B, the bumpers), until a player's
        // pad says otherwise.
        constexpr uint32_t kDirectConfirm = 0, kDirectCancel = 1, kDirectTabLeft = 4, kDirectTabRight = 5;
        constexpr std::array kHatMoves{MenuAction::Up, MenuAction::Right, MenuAction::Down, MenuAction::Left};

        MenuAction KeyAction(uint32_t vk)
        {
            switch (vk)
            {
            case kKeyUp: return MenuAction::Up;
            case kKeyDown: return MenuAction::Down;
            case kKeyLeft: return MenuAction::Left;
            case kKeyRight: return MenuAction::Right;
            case kKeyEnter: return MenuAction::Confirm;
            case kKeyEscape: return MenuAction::Cancel;
            default: return MenuAction::None;
            }
        }

        MenuAction PadAction(uint32_t button)
        {
            switch (button)
            {
            case kPadUp: return MenuAction::Up;
            case kPadDown: return MenuAction::Down;
            case kPadLeft: return MenuAction::Left;
            case kPadRight: return MenuAction::Right;
            case kPadConfirm: return MenuAction::Confirm;
            case kPadCancel: return MenuAction::Cancel;
            case kPadLeftBumper: return MenuAction::TabLeft;
            case kPadRightBumper: return MenuAction::TabRight;
            default: return MenuAction::None;
            }
        }

        MenuAction DirectButtonAction(uint32_t offset)
        {
            switch (offset - kFirstButton)
            {
            case kDirectConfirm: return MenuAction::Confirm;
            case kDirectCancel: return MenuAction::Cancel;
            case kDirectTabLeft: return MenuAction::TabLeft;
            case kDirectTabRight: return MenuAction::TabRight;
            default: return MenuAction::None;
            }
        }

        bool HatCentred(int32_t data) { return (static_cast<uint32_t>(data) & kHatCentred) == kHatCentred; }

        // A diagonal is no move: it is passed through on the way between two.
        MenuAction HatMove(int32_t data)
        {
            if (HatCentred(data) || data < 0 || data % kHatQuarter != 0) return MenuAction::None;
            const auto quarter = static_cast<size_t>(data / kHatQuarter);
            return quarter < kHatMoves.size() ? kHatMoves[quarter] : MenuAction::None;
        }

        bool Moves(MenuAction action) { return action >= MenuAction::Up && action <= MenuAction::Right; }

        constexpr std::array kActionNames{"None", "Up", "Down", "Left", "Right", "Confirm", "Cancel", "TabLeft", "TabRight"};
        constexpr std::array kSourceNames{"Key", "XInput", "DirectInput"};
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

    std::optional<MenuControl> ControlOf(const MenuEvent& event)
    {
        switch (event.source)
        {
        case InputSource::Keyboard:
            if (KeyAction(event.code) == MenuAction::None) return std::nullopt;
            return MenuControl{event.value != 0};
        case InputSource::XInput:
            if (PadAction(event.code) == MenuAction::None) return std::nullopt;
            return MenuControl{event.value != 0};
        case InputSource::DirectInput:
            if (event.code == kHatOffset) return MenuControl{!HatCentred(event.value)};
            if (event.code < kFirstButton || DirectButtonAction(event.code) == MenuAction::None) return std::nullopt;
            return MenuControl{(event.value & kButtonDown) != 0};
        }
        return std::nullopt;
    }

    bool InputTaken::Keep(const MenuEvent& event, bool open)
    {
        const std::optional<MenuControl> control = ControlOf(event);
        if (!control) return false;
        const auto key = std::pair(event.source, event.code);
        const auto at  = m_Down.find(key);
        if (!control->down)
        {
            if (at == m_Down.end()) return false;
            const bool kept = at->second;
            m_Down.erase(at);
            return kept;
        }
        if (at != m_Down.end()) return at->second;
        m_Down.emplace(key, open);
        return open;
    }

    MenuAction MenuInput::Change(Hold& hold, MenuAction move, double now)
    {
        if (move == hold.move) return MenuAction::None;
        hold = {move, now + kPadRepeatDelay};
        return move;
    }

    MenuAction MenuInput::Feed(const MenuEvent& event, double now)
    {
        switch (event.source)
        {
        case InputSource::Keyboard: return event.value != 0 ? KeyAction(event.code) : MenuAction::None;
        case InputSource::XInput:
        {
            const MenuAction action = PadAction(event.code);
            if (!Moves(action)) return event.value != 0 ? action : MenuAction::None;
            if (event.value != 0) return Change(m_Pad, action, now);
            if (m_Pad.move == action) Change(m_Pad, MenuAction::None, now);
            return MenuAction::None;
        }
        case InputSource::DirectInput:
            if (event.code == kHatOffset) return Change(m_Pad, HatMove(event.value), now);
            if (event.code < kFirstButton || (event.value & kButtonDown) == 0) return MenuAction::None;
            return DirectButtonAction(event.code);
        }
        return MenuAction::None;
    }

    MenuAction MenuInput::Due(double now)
    {
        if (m_Pad.move == MenuAction::None || now < m_Pad.next) return MenuAction::None;
        m_Pad.next = now + kPadRepeatInterval;
        return m_Pad.move;
    }

    void MenuInput::Reset() { *this = MenuInput{}; }

    std::string MenuEventText(const MenuEvent& event)
    {
        return std::string(kSourceNames[static_cast<size_t>(event.source)]) + " " + std::to_string(event.code) + " = " +
               std::to_string(event.value);
    }

    const char* MenuActionName(MenuAction action) { return kActionNames[static_cast<size_t>(action)]; }
}

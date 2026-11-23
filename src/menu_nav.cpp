#include "menu_nav.h"

#include <algorithm>
#include <array>
#include <charconv>
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
        constexpr uint32_t kPadUp = 0, kPadDown = 1, kPadLeft = 2, kPadRight = 3, kPadStart = 4, kPadBack = 5, kPadLeftThumb = 6,
                           kPadRightThumb = 7, kPadLeftBumper = 8, kPadRightBumper = 9, kPadX = 14, kPadY = 15, kPadLeftTrigger = 16,
                           kPadRightTrigger = 17;
        // A trigger counts as pressed past this (XINPUT_GAMEPAD_TRIGGER_THRESHOLD).
        constexpr int32_t kTriggerPress = 30;
        // padsin000's slots for Cancel and Confirm; a slot of -1 is unset. On an XInput pad the game numbers the buttons
        // its own way (B, X, Y, A, the d-pad right, left, up and down, LB, LT, L3, RB, RT, R3, Start, Back): these are
        // Ashita's numbers for them.
        constexpr size_t kCancelSlot = 5, kConfirmSlot = 7;
        constexpr std::array kGamePadButtons{kPadB, kPadX, kPadY, kPadA, kPadRight, kPadLeft, kPadUp, kPadDown, kPadLeftBumper,
            kPadLeftTrigger, kPadLeftThumb, kPadRightBumper, kPadRightTrigger, kPadRightThumb, kPadStart, kPadBack};
        constexpr uint32_t kDirectButtons = 32; // DIJOYSTATE's
        // DirectInput's DIJOYSTATE: the first hat's offset, in hundredths of a degree from up, its low word all ones when
        // centred; the buttons' offsets from the first's, each down with its top bit.
        constexpr uint32_t kHatOffset = 32, kFirstButton = 48;
        constexpr uint32_t kHatCentred = 0xFFFF;
        constexpr int32_t kHatQuarter  = 9000;
        constexpr int32_t kButtonDown = 0x80;
        // DirectInput numbers a pad's buttons its own way: these are an Xbox pad's bumpers.
        constexpr uint32_t kDirectTabLeft = 4, kDirectTabRight = 5;
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

        // The player's Confirm and Cancel come first: they may be set on a button the windows use otherwise.
        MenuAction PadAction(uint32_t button, const PadBindings& pad)
        {
            if (button == pad.confirm) return MenuAction::Confirm;
            if (button == pad.cancel) return MenuAction::Cancel;
            switch (button)
            {
            case kPadUp: return MenuAction::Up;
            case kPadDown: return MenuAction::Down;
            case kPadLeft: return MenuAction::Left;
            case kPadRight: return MenuAction::Right;
            case kPadLeftBumper: return MenuAction::TabLeft;
            case kPadRightBumper: return MenuAction::TabRight;
            default: return MenuAction::None;
            }
        }

        bool PadDown(const MenuEvent& event)
        {
            const bool trigger = event.code == kPadLeftTrigger || event.code == kPadRightTrigger;
            return trigger ? event.value > kTriggerPress : event.value != 0;
        }

        MenuAction DirectButtonAction(uint32_t offset, const PadBindings& pad)
        {
            const uint32_t button = offset - kFirstButton;
            if (button == pad.directConfirm) return MenuAction::Confirm;
            if (button == pad.directCancel) return MenuAction::Cancel;
            switch (button)
            {
            case kDirectTabLeft: return MenuAction::TabLeft;
            case kDirectTabRight: return MenuAction::TabRight;
            default: return MenuAction::None;
            }
        }

        // A slot's number: -1 for unset, none when it cannot be read.
        std::optional<int> Slot(std::string_view list, size_t slot)
        {
            for (size_t i = 0; i < slot; ++i)
            {
                const size_t comma = list.find(',');
                if (comma == std::string_view::npos) return std::nullopt;
                list.remove_prefix(comma + 1);
            }
            list = list.substr(0, list.find(','));
            int value = 0;
            const auto [end, error] = std::from_chars(list.data(), list.data() + list.size(), value);
            if (error != std::errc{} || end != list.data() + list.size()) return std::nullopt;
            return value;
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

        // Whether the windows act on this input: whether it holds its key or button down.
        struct MenuControl
        {
            bool down = false;
        };

        std::optional<MenuControl> ControlOf(const MenuEvent& event, const PadBindings& pad)
        {
            switch (event.source)
            {
            case InputSource::Keyboard:
                if (KeyAction(event.code) == MenuAction::None) return std::nullopt;
                return MenuControl{event.value != 0};
            case InputSource::XInput:
                if (PadAction(event.code, pad) == MenuAction::None) return std::nullopt;
                return MenuControl{PadDown(event)};
            case InputSource::DirectInput:
                if (event.code == kHatOffset) return MenuControl{!HatCentred(event.value)};
                if (event.code < kFirstButton || DirectButtonAction(event.code, pad) == MenuAction::None) return std::nullopt;
                return MenuControl{(event.value & kButtonDown) != 0};
            }
            return std::nullopt;
        }

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

    PadBindings PadBindingsFrom(std::string_view padsin)
    {
        PadBindings pad;
        const auto xinput = [](int value) -> std::optional<uint32_t> {
            if (value < 0 || static_cast<size_t>(value) >= kGamePadButtons.size()) return std::nullopt;
            return kGamePadButtons[static_cast<size_t>(value)];
        };
        const auto direct = [](int value) -> std::optional<uint32_t> {
            if (value < 0 || static_cast<uint32_t>(value) >= kDirectButtons) return std::nullopt;
            return static_cast<uint32_t>(value);
        };
        if (const std::optional<int> confirm = Slot(padsin, kConfirmSlot))
        {
            pad.confirm       = xinput(*confirm);
            pad.directConfirm = direct(*confirm);
        }
        if (const std::optional<int> cancel = Slot(padsin, kCancelSlot))
        {
            pad.cancel       = xinput(*cancel);
            pad.directCancel = direct(*cancel);
        }
        return pad;
    }

    bool InputTaken::Keep(const MenuEvent& event, bool open)
    {
        const std::optional<MenuControl> control = ControlOf(event, m_Pad);
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

    bool InputTaken::HoldsKeys(const std::function<bool(uint32_t)>& keyDown)
    {
        const auto key = [](const auto& down) { return down.first.first == InputSource::Keyboard; };
        std::erase_if(m_Down, [&](const auto& down) { return key(down) && !keyDown(down.first.second); });
        return std::any_of(m_Down.begin(), m_Down.end(), [&](const auto& down) { return key(down) && down.second; });
    }

    MenuAction MenuInput::Change(Hold& hold, MenuAction move, double now)
    {
        if (move == hold.move) return MenuAction::None;
        hold = {move, now + kPadRepeatDelay};
        return move;
    }

    MenuAction MenuInput::Once(std::set<uint32_t>& held, uint32_t code, bool down, MenuAction action)
    {
        const bool pressed = down && !held.contains(code);
        if (down)
            held.insert(code);
        else
            held.erase(code);
        return pressed ? action : MenuAction::None;
    }

    MenuAction MenuInput::Feed(const MenuEvent& event, double now)
    {
        switch (event.source)
        {
        case InputSource::Keyboard:
        {
            // Windows repeats a held key's press: an arrow moves again each time, but Confirm and Cancel act once.
            const MenuAction action = KeyAction(event.code);
            if (action == MenuAction::None || Moves(action)) return event.value != 0 ? action : MenuAction::None;
            return Once(m_Keys, event.code, event.value != 0, action);
        }
        case InputSource::XInput:
        {
            const MenuAction action = PadAction(event.code, m_Bindings);
            const bool down         = PadDown(event);
            if (action == MenuAction::None) return MenuAction::None;
            if (!Moves(action)) return Once(m_Held, event.code, down, action);
            if (down) return Change(m_Pad, action, now);
            if (m_Pad.move == action) Change(m_Pad, MenuAction::None, now);
            return MenuAction::None;
        }
        case InputSource::DirectInput:
            if (event.code == kHatOffset) return Change(m_Pad, HatMove(event.value), now);
            if (event.code < kFirstButton || (event.value & kButtonDown) == 0) return MenuAction::None;
            return DirectButtonAction(event.code, m_Bindings);
        }
        return MenuAction::None;
    }

    MenuAction MenuInput::Due(double now)
    {
        if (m_Pad.move == MenuAction::None || now < m_Pad.next) return MenuAction::None;
        m_Pad.next = now + kPadRepeatInterval;
        return m_Pad.move;
    }

    void MenuInput::Reset()
    {
        m_Pad = {};
        m_Held.clear();
        m_Keys.clear();
    }

    std::string MenuEventText(const MenuEvent& event)
    {
        return std::string(kSourceNames[static_cast<size_t>(event.source)]) + " " + std::to_string(event.code) + " = " +
               std::to_string(event.value);
    }

    const char* MenuActionName(MenuAction action) { return kActionNames[static_cast<size_t>(action)]; }
}

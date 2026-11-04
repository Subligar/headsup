#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace headsup
{
    // Keys are Windows virtual-key codes, and left and right Ctrl, Shift and Alt are one key each.
    constexpr uint8_t kFirstKey = 0x08, kLastKey = 0xFE;
    bool MouseButton(uint8_t vk);
    // Whether a key down counts as held: not a mouse button, nor a sided Ctrl, Shift or Alt, which Windows reports
    // with the plain one.
    bool HeldKey(uint8_t vk);

    std::string KeyName(uint8_t vk);

    // The key a code names once normalized, or 0 when it is no key: a mouse button, or past either end.
    uint8_t BindableKey(int vk);

    // One chosen key's presses, each counted once. A key just chosen counts as held: choosing it was a press.
    class KeyPress
    {
    public:
        bool Pressed(uint8_t key, bool down);

    private:
        uint8_t m_Key = 0;
        bool m_Down   = false;
    };

    // Every key down, once each and normalized, from a test of one key (GetAsyncKeyState in the plugin).
    template <typename Down>
    std::vector<uint8_t> HeldKeys(Down down)
    {
        std::vector<uint8_t> held;
        for (unsigned vk = kFirstKey; vk <= kLastKey; ++vk)
        {
            const auto key = static_cast<uint8_t>(vk);
            if (HeldKey(key) && down(key)) held.push_back(key);
        }
        return held;
    }

    // FFXI hides its pointer when a key sets off one of its commands; it stays when every key held is one to keep.
    // Allocates nothing: the game's command check calls it.
    template <typename Down>
    bool KeepsPointer(Down down, const std::vector<uint8_t>& keep)
    {
        bool any = false;
        for (unsigned vk = kFirstKey; vk <= kLastKey; ++vk)
        {
            const auto key = static_cast<uint8_t>(vk);
            if (!HeldKey(key) || !down(key)) continue;
            if (std::find(keep.begin(), keep.end(), key) == keep.end()) return false;
            any = true;
        }
        return any;
    }
    constexpr size_t kMaxKeptKeys = 32;
}

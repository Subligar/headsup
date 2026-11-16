#pragma once

#include "screen_box.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace headsup
{
    // The game keeps a combat popup's text in 16 bytes of its particle, with no room for a terminator.
    constexpr size_t kMaxCombatPopupText = 16;

    // One call of the game's popup draw: its text, and what the quads it sent drew. The box holds all its quads, in
    // the scene image's pixels; the depth and color are its last quad's (the color its first vertex's).
    struct CombatPopup
    {
        char text[kMaxCombatPopupText + 1] = {};
        uint32_t color = 0;
        ScreenBox box;
        float depth = 0.0f;
    };

    // The frame's popups, grouped by the call that drew them, never by counting quads.
    class CombatPopupList
    {
    public:
        void NewFrame();
        // The game's draw of one popup starts.
        void Begin(const char* text);
        bool Open() const { return m_Open; }
        // A quad the open draw sent, as ScreenVertex vertices stride bytes apart; nothing outside a draw.
        void Add(const void* vertices, size_t count, size_t stride);
        void End() { m_Open = false; }
        // A draw that sent no quad drew nothing and is not here.
        const std::vector<CombatPopup>& Popups() const { return m_Popups; }

    private:
        std::vector<CombatPopup> m_Popups;
        char m_Text[kMaxCombatPopupText + 1] = {};
        bool m_Open = false, m_Added = false;
    };
}

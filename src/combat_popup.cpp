#include "combat_popup.h"

#include "screen_vertex.h"

#include <cstring>
#include <utility>

namespace headsup
{
    void CombatPopupList::NewFrame()
    {
        m_Popups.clear();
        m_Open = false;
    }

    void CombatPopupList::Begin(const char* text)
    {
        // The game's text has no terminator when it fills its 16 bytes: nothing past them is read.
        const size_t length = text != nullptr ? strnlen(text, kMaxCombatPopupText) : 0;
        if (length > 0) std::memcpy(m_Text, text, length);
        m_Text[length] = '\0';
        m_Open         = true;
        m_Added        = false;
    }

    void CombatPopupList::Add(const void* vertices, size_t count, size_t stride)
    {
        if (!m_Open || vertices == nullptr || count == 0 || stride < sizeof(ScreenVertex)) return;
        if (!std::exchange(m_Added, true))
        {
            m_Popups.emplace_back();
            std::memcpy(m_Popups.back().text, m_Text, sizeof(m_Text));
        }
        CombatPopup& popup = m_Popups.back();
        const auto* bytes  = static_cast<const uint8_t*>(vertices);
        for (size_t i = 0; i < count; ++i)
        {
            ScreenVertex vertex{};
            std::memcpy(&vertex, bytes + i * stride, sizeof(vertex));
            popup.box.Add(vertex.x, vertex.y);
            if (i == 0)
            {
                popup.depth = vertex.z;
                popup.color = vertex.color;
            }
        }
    }
}

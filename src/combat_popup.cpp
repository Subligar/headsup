#include "combat_popup.h"

#include "argb.h"
#include "bar_content.h"
#include "markup.h"
#include "plate_image.h"
#include "screen_vertex.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace headsup
{
    void PopupTies::Ties::Put(uintptr_t key, const PopupEvent& event)
    {
        const auto known = std::find(keys.begin(), keys.end(), key);
        const size_t at  = known != keys.end() ? static_cast<size_t>(known - keys.begin())
                                               : std::exchange(next, (next + 1) % kKeptTies);
        keys[at]   = key;
        events[at] = event;
    }

    void PopupTies::Ties::Forget(uintptr_t key) { std::replace(keys.begin(), keys.end(), key, uintptr_t{0}); }

    std::optional<PopupEvent> PopupTies::Ties::Of(uintptr_t key) const
    {
        const auto known = std::find(keys.begin(), keys.end(), key);
        if (key == 0 || known == keys.end()) return std::nullopt;
        return events[static_cast<size_t>(known - keys.begin())];
    }

    // A task or particle the game makes again at an address it freed is a new one.
    void PopupTies::Started(uintptr_t task)
    {
        if (m_Pending) m_Tasks.Put(task, *m_Pending);
        else m_Tasks.Forget(task);
        m_Pending.reset();
    }

    void PopupTies::Built(uintptr_t particle, uintptr_t task)
    {
        if (const std::optional<PopupEvent> event = m_Tasks.Of(task)) m_Particles.Put(particle, *event);
        else m_Particles.Forget(particle);
    }

    void CombatPopupList::NewFrame()
    {
        m_Popups.clear();
        m_Open = false;
    }

    void CombatPopupList::Begin(const char* text, uintptr_t particle, std::optional<PopupEvent> event)
    {
        m_Particle = particle;
        m_Event    = event;
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
            m_Popups.back().particle = m_Particle;
            m_Popups.back().event    = m_Event;
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

namespace headsup
{
    namespace
    {
        // By the server's msg_basic.h ids of the messages the game shows as a miss.
        std::string_view MissWord(uint32_t message)
        {
            switch (message)
            {
            case 32:
            case 282: return "Evade";
            case 70: return "Parry";
            case 30: return "Anticipate";
            case 14:
            case 31:
            case 535: return "Shadow";
            case 85:
            case 284:
            case 655: return "Resist";
            case 75:
            case 156:
            case 189:
            case 283:
            case 323:
            case 355:
            case 408:
            case 422:
            case 423:
            case 425: return "No Effect";
            case 114:
            case 137:
            case 153:
            case 244: return "Fail";
            default: return kMissWord;
            }
        }

        constexpr std::string_view kDrainWord = "Drain ", kMpLabel = " MP", kTpLabel = " TP";
        struct AmountLabel
        {
            std::string_view before, after;
        };
        // By the server's msg_basic.h ids of amounts of MP and TP, and of HP, MP and TP drained.
        AmountLabel LabelOf(uint32_t message)
        {
            switch (message)
            {
            case 25:
            case 152:
            case 224:
            case 276:
            case 451:
            case 588: return {{}, kMpLabel};
            case 452: return {{}, kTpLabel};
            case 132:
            case 161:
            case 187:
            case 227: return {kDrainWord, {}};
            case 162:
            case 225:
            case 228: return {kDrainWord, kMpLabel};
            case 165:
            case 226:
            case 454: return {kDrainWord, kTpLabel};
            default: return {};
            }
        }
    }

    PopupSwitches PopupSwitchesOf(const Settings& settings) { return PopupSwitches{settings.combatPopupWords}; }

    std::string CombatPopupWords(std::string_view text, std::optional<PopupEvent> event, const PopupSwitches& on)
    {
        if (text == std::string_view(&kMissGlyph, 1)) return std::string(event && on.words ? MissWord(event->message) : kMissWord);
        const AmountLabel label = event ? LabelOf(event->message) : AmountLabel{};
        return std::string(label.before) + std::string(text) + std::string(label.after);
    }

    namespace
    {
        bool Twins(const CombatPopup& earlier, const CombatPopup& later)
        {
            return std::string_view(earlier.text) == later.text &&
                   std::hypot(later.box.CenterX() - earlier.box.CenterX(), later.box.CenterY() - earlier.box.CenterY()) <=
                       later.box.Height() / 2.0f;
        }
    }

    std::vector<ShownCombatPopup> ShownCombatPopups(const std::vector<CombatPopup>& popups, int sizePercent, const PopupSwitches& on)
    {
        std::vector<ShownCombatPopup> shown;
        for (size_t i = 0; i < popups.size(); ++i)
        {
            const CombatPopup& p = popups[i];
            if (std::any_of(popups.begin() + static_cast<std::ptrdiff_t>(i) + 1, popups.end(),
                    [&](const CombatPopup& later) { return Twins(p, later); }))
                continue;
            const uint32_t alpha = std::min(Channel(p.color, kAlphaShift) * kCombatPopupModulate, kOpaqueAlpha);
            shown.push_back(ShownCombatPopup{CombatPopupWords(p.text, p.event, on), ShownColor(p.color, kCombatPopupModulate),
                alpha, ScreenPoint{p.box.CenterX(), p.box.CenterY()}, p.box.Height() * static_cast<float>(sizePercent) / kPercent,
                p.depth});
        }
        return shown;
    }

    SquareQuad PlaceCombatPopupText(ScreenPoint center, float lineHeight, const CombatTextSize& text, float pixelShape)
    {
        const float fit = lineHeight / static_cast<float>(text.raster), width = text.width * fit * pixelShape;
        const float capitals = text.halo + (text.capitalTop + text.capitalBottom) / 2.0f;
        return SquareQuad{std::round(center.x - width / 2.0f), std::round(center.y - capitals * fit), width, text.height * fit};
    }

    int CombatPopupEdge(int raster) { return WholePixels(static_cast<float>(raster) * kCombatPopupEdgePerHeight); }

    std::string DescribeCombatPopup(const CombatPopup& popup)
    {
        const ScreenBox& b = popup.box;
        return std::string(popup.text) + " | " + Printed("%08X", popup.color) + " | " + Decimal(b.minX, 2) + "," +
               Decimal(b.minY, 2) + " to " + Decimal(b.maxX, 2) + "," + Decimal(b.maxY, 2) + " | " + Decimal(popup.depth, 5) +
               " | message " + (popup.event ? std::to_string(popup.event->message) : "-");
    }

    std::string DescribeCombatPopup(const ShownCombatPopup& popup)
    {
        return popup.words + " | " + Printed("%08X", popup.color) + " alpha " + std::to_string(popup.alpha) + " | " +
               Decimal(popup.center.x, 2) + "," + Decimal(popup.center.y, 2) + " line " + Decimal(popup.lineHeight, 2) + " | " +
               Decimal(popup.depth, 5);
    }
}

#include "fish_bar.h"

#include "bar_document.h"
#include "markup.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace headsup
{
    namespace
    {
        constexpr float kFramePad   = 3.0f; // pixels between its frame and its socket
        constexpr float kFrameRound = 4.0f;

        constexpr float kNameLineShare = 1.2f; // of its text size

        std::string FishBarRml(const BarPalette& p, int height, int textSize, const std::string& font,
            std::optional<uint32_t> fill, bool draggable)
        {
            const float nameHeight = static_cast<float>(FishNameHeight(textSize));
            const std::string start = RcssColor(fill.value_or(p.accent)), end = RcssColor(fill.value_or(p.accentEnd));
            std::string css = "body{display:block;width:100%;height:100%;pointer-events:none;z-index:5;}";
            css += "div{display:block;}";
            css += std::string(".") + kHiddenClass + "{display:none;}";
            css += std::string("#") + kFishBarId + "{position:absolute;box-sizing:border-box;height:" +
                   Px(static_cast<float>(height)) + ";padding:" + Px(kFramePad) + ";border:1px " +
                   RcssColor(p.panelBorder) + ";border-radius:" + Px(kFrameRound) + ";decorator:vertical-gradient(" +
                   RcssColor(p.panelTop) + " " + RcssColor(p.panelBottom) + ");" +
                   (draggable ? "pointer-events:auto;drag:drag;" : "") + "}";
            css += ".socket{height:100%;" + SocketRules(kFrameRound / 2.0f) + "}";
            css += std::string("#") + kFishFillId + "{height:100%;decorator:horizontal-gradient(" + start + " " + end + ");}";
            css += std::string("#") + kFishNameId + "{position:absolute;left:0px;top:" + Px(-nameHeight - 1.0f) +
                   ";white-space:pre;font-family:\"" + font + "\";font-size:" + Px(static_cast<float>(textSize)) +
                   ";line-height:" + Px(nameHeight) + ";color:" + RcssColor(p.text) + ";" + ShadowEffect(textSize) + "}";
            return "<rml><head><style>" + css + "</style></head><body id=\"" + std::string(kFishBarDocument) +
                   "\"><div id=\"" + kFishBarId + "\" class=\"" + kHiddenClass + "\"><div id=\"" + kFishNameId +
                   "\"></div><div class=\"socket\"><div id=\"" + kFishFillId + "\"></div></div></div></body></rml>";
        }
    }

    std::optional<float> FishShare(const FishStamina& fish)
    {
        if (fish.full <= 0) return std::nullopt;
        return std::clamp(static_cast<float>(fish.drawn) / static_cast<float>(fish.full), 0.0f, 1.0f);
    }

    ScreenBox VerticesBox(const void* vertices, uint32_t count, uint32_t stride)
    {
        ScreenBox box;
        const auto* bytes = static_cast<const uint8_t*>(vertices);
        for (uint32_t v = 0; v < count; ++v)
        {
            float xy[2];
            std::memcpy(xy, bytes + static_cast<size_t>(v) * stride, sizeof(xy));
            box.Add(xy[0], xy[1]);
        }
        return box;
    }

    bool AboveWindow(const ScreenBox& box, const WindowRect& window) { return box.valid && box.maxY <= window[1]; }

    bool FishHooked(uint32_t status) { return status == kHookedStatus || status == kOldHookedStatus; }

    int FishNameHeight(int textSize) { return static_cast<int>(std::lround(static_cast<float>(textSize) * kNameLineShare)); }

    FishPlace FishBarPlace(int x, int y, int width, int height, int nameHeight, float screenWidth, float screenHeight)
    {
        const float w = static_cast<float>(width), h = static_cast<float>(height), above = static_cast<float>(nameHeight);
        const float left = x == kFishBarUnplaced ? (screenWidth - w) / 2.0f : static_cast<float>(x);
        const float top  = y == kFishBarUnplaced ? std::round(screenHeight * kFishBarStartDown) : static_cast<float>(y);
        return FishPlace{std::clamp(left, 0.0f, std::max(0.0f, screenWidth - w)),
            std::clamp(top, above, std::max(above, screenHeight - h))};
    }

    bool FishBar::Update(Rml::Context* context, const BarPalette& palette, Settings& settings,
        const std::optional<FishBarView>& view)
    {
        if (!view && m_Document == nullptr) return false;
        const Look look{palette, settings.fishBarHeight, settings.fishBarTextSize, view ? view->font : m_Look.font,
            view ? view->fill : m_Look.fill, settings.fishBarDraggable};
        if (m_Document != nullptr && !(look == m_Look)) Close();
        if (m_Document == nullptr)
        {
            m_Document = context->LoadDocumentFromMemory(
                FishBarRml(palette, look.height, look.textSize, look.font, look.fill, look.draggable), kFishBarDocument);
            if (m_Document == nullptr) return false;
            m_Look = look;
            m_Line.clear();
            m_Bar  = m_Document->GetElementById(kFishBarId);
            m_Fill = m_Document->GetElementById(kFishFillId);
            m_Name = m_Document->GetElementById(kFishNameId);
            for (const Rml::EventId id : {Rml::EventId::Dragstart, Rml::EventId::Drag, Rml::EventId::Dragend})
                m_Bar->AddEventListener(id, this);
            m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        }
        // A bar hidden mid-drag (the fish got away) may never hear the button let go: the drag ends with it.
        if (!view) m_Dragging = false;
        bool moved = false;
        if (m_Dragged && !m_Dragging)
        {
            settings.fishBarX = static_cast<int>(std::lround(m_Dragged->x));
            settings.fishBarY = static_cast<int>(std::lround(m_Dragged->y));
            m_Dragged.reset();
            moved = true;
        }
        m_Bar->SetClass(kHiddenClass, !view);
        if (!view) return moved;
        if (view->line != m_Line)
        {
            m_Name->SetInnerRML(EscapedRml(view->line));
            m_Line = view->line;
        }
        const Rml::Vector2i screen = context->GetDimensions();
        const FishPlace at = m_Dragging && m_Dragged
                                 ? *m_Dragged
                                 : FishBarPlace(settings.fishBarX, settings.fishBarY, settings.fishBarWidth,
                                       settings.fishBarHeight, FishNameHeight(settings.fishBarTextSize),
                                       static_cast<float>(screen.x), static_cast<float>(screen.y));
        const Placed placed{at.x, at.y, static_cast<float>(settings.fishBarWidth), view->share};
        if (placed == m_Placed) return moved;
        m_Placed = placed;
        m_Bar->SetProperty(Rml::PropertyId::Left, Rml::Property(placed.x, Rml::Unit::PX));
        m_Bar->SetProperty(Rml::PropertyId::Top, Rml::Property(placed.y, Rml::Unit::PX));
        m_Bar->SetProperty(Rml::PropertyId::Width, Rml::Property(placed.width, Rml::Unit::PX));
        m_Fill->SetProperty(Rml::PropertyId::Width, Rml::Property(placed.share * kPercentWhole, Rml::Unit::PERCENT));
        return moved;
    }

    void FishBar::ProcessEvent(Rml::Event& event)
    {
        const Rml::Vector2f pointer{event.GetParameter<float>("mouse_x", 0.0f), event.GetParameter<float>("mouse_y", 0.0f)};
        const Rml::Vector2i screen = m_Document->GetContext()->GetDimensions();
        const float width = m_Bar->GetBox().GetSize(Rml::BoxArea::Border).x;
        switch (event.GetId())
        {
        case Rml::EventId::Dragstart:
            m_Grab     = pointer - m_Bar->GetAbsoluteOffset(Rml::BoxArea::Border);
            m_Dragging = true;
            break;
        case Rml::EventId::Drag:
        {
            const Rml::Vector2f corner = pointer - m_Grab;
            const float height = m_Bar->GetBox().GetSize(Rml::BoxArea::Border).y;
            m_Dragged = FishBarPlace(static_cast<int>(std::lround(corner.x)), static_cast<int>(std::lround(corner.y)),
                static_cast<int>(std::lround(width)), static_cast<int>(std::lround(height)),
                FishNameHeight(m_Look.textSize), static_cast<float>(screen.x), static_cast<float>(screen.y));
            m_Bar->SetProperty(Rml::PropertyId::Left, Rml::Property(m_Dragged->x, Rml::Unit::PX));
            m_Bar->SetProperty(Rml::PropertyId::Top, Rml::Property(m_Dragged->y, Rml::Unit::PX));
            m_Placed.reset();
            break;
        }
        case Rml::EventId::Dragend: m_Dragging = false; break;
        default: break;
        }
    }

    void FishBar::Close()
    {
        if (m_Document == nullptr) return;
        for (const Rml::EventId id : {Rml::EventId::Dragstart, Rml::EventId::Drag, Rml::EventId::Dragend})
            m_Bar->RemoveEventListener(id, this);
        m_Document->Close();
        m_Document = nullptr;
        m_Bar = m_Fill = m_Name = nullptr;
        m_Dragging = false;
        m_Dragged.reset();
        m_Placed.reset();
    }
}

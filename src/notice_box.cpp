#include "notice_box.h"

#include "bar_document.h"
#include "bar_look.h"
#include "markup.h"
#include "menu_document.h"

#include <algorithm>
#include <cmath>

namespace headsup
{
    namespace
    {
        constexpr int kNoticesZ           = kMenuWindowZ - 1; // under the settings window
        constexpr float kBoxRound         = 4.0f;
        constexpr float kBoxPadAcross     = 10.0f, kBoxPadDown = 8.0f;
        constexpr float kBarGap           = 6.0f; // between the message and the countdown bar
        constexpr const char* kMovingClass = "moving";
        constexpr const char* kOnlineClass = "online", *kOfflineClass = "offline";
        constexpr int kTitle = 0, kMessage = 1, kBar = 2, kBoxParts = 3;

        std::string NoticeRml(const BarPalette& p, const std::string& titleFont, const std::string& textFont,
            float titleSize, float textSize, const std::optional<SkinFrame>& skin)
        {
            const auto font = [](const std::string& family, float size) {
                return "font-family:\"" + family + "\";font-size:" + Px(size) + ";";
            };
            std::string css = "body{display:block;width:100%;height:100%;pointer-events:none;z-index:" +
                              std::to_string(kNoticesZ) + ";}";
            css += "div{display:block;}";
            css += std::string(".") + kHiddenClass + "{display:none;}";
            css += std::string("#") + kNoticeStackId + "{position:absolute;width:" + Px(kNoticeWidth) + ";height:0px;}";
            css += std::string("#") + kNoticeStackId + "." + kMovingClass + "{pointer-events:auto;drag:drag;}";
            css += ".box{position:absolute;left:0px;box-sizing:border-box;width:" + Px(kNoticeWidth) + ";padding:" +
                   Px(kBoxPadDown) + " " + Px(kBoxPadAcross) + ";border-radius:" + Px(kBoxRound) + ";" +
                   (skin ? SkinEdgeRules(*skin)
                         : "border:1px " + RcssColor(p.panelBorder) + ";decorator:vertical-gradient(" +
                               RcssColor(p.panelTop) + " " + RcssColor(p.panelBottom) + ");") +
                   "}";
            // The art sits behind the text, which is positioned to be drawn after it.
            if (skin) css += ArtRules(".box", kBoxRound, SkinFillRules(*skin)) + ".title,.message,.bar{position:relative;}";
            css += ".title{white-space:nowrap;" + font(titleFont, titleSize) +
                   ShadowEffect(static_cast<int>(std::lround(titleSize))) + "}";
            css += ".message{" + font(textFont, textSize) + "color:" + RcssColor(p.text) + ";" +
                   ShadowEffect(static_cast<int>(std::lround(textSize))) + "}";
            css += ".bar{height:" + Px(kNoticeBarHeight) + ";margin-top:" + Px(kBarGap) + ";}";
            for (const NoticeKind kind : {NoticeKind::Online, NoticeKind::Offline})
            {
                const std::string kindClass = kind == NoticeKind::Online ? kOnlineClass : kOfflineClass;
                const std::string color     = RcssColor(NoticeColor(kind));
                css += "." + kindClass + " .title{color:" + color + ";}." + kindClass + " .bar{background-color:" + color + ";}";
            }
            std::string boxes;
            for (int slot = 0; slot < kMaxNotices; ++slot)
                boxes += "<div id=\"" + NoticeId(slot) + "\" class=\"box " + kHiddenClass +
                         "\">" + (skin ? kArtRml : "") +
                         "<div class=\"title\"></div><div class=\"message\"></div><div class=\"bar\"></div></div>";
            return "<rml><head><style>" + css + "</style></head><body id=\"" + std::string(kNoticeDocument) +
                   "\"><div id=\"" + kNoticeStackId + "\">" + boxes + "</div></body></rml>";
        }

        // The title, message and bar, past the art when the box has it.
        Rml::Element* PartOf(Rml::Element* box, int index)
        {
            return box->GetChild(box->GetNumChildren() - kBoxParts + index);
        }
    }

    std::string NoticeId(int slot) { return "notice" + std::to_string(slot); }

    bool NoticeBox::Update(Rml::Context* context, const BarPalette& palette, Settings& settings,
        const std::vector<ShownNotice>& shown, bool moving, float barBottom, float seconds,
        const std::optional<SkinFrame>& skin)
    {
        if (shown.empty() && !moving && m_Document == nullptr) return false;
        const BarMetrics metrics = MetricsOf(settings, palette.shape, WidgetOptions{});
        const Look look{palette, BarTitleFont(settings, palette.shape), BarTextFont(settings), metrics.title,
            metrics.subtitle, skin};
        if (m_Document != nullptr && !(look == m_Look)) Close();
        if (m_Document == nullptr)
        {
            m_Document = context->LoadDocumentFromMemory(
                NoticeRml(palette, look.titleFont, look.textFont, look.titleSize, look.textSize, look.skin), kNoticeDocument);
            if (m_Document == nullptr) return false;
            m_Look  = look;
            m_Stack = m_Document->GetElementById(kNoticeStackId);
            m_Filled.assign(kMaxNotices, std::nullopt);
            m_At.clear();
            for (const Rml::EventId id : {Rml::EventId::Dragstart, Rml::EventId::Drag, Rml::EventId::Dragend})
                m_Stack->AddEventListener(id, this);
            m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        }
        // Shift let go mid-drag ends the drag where the stack is, whatever the pointer does until the button goes up.
        m_Grabbable = moving;
        if (!moving) m_Dragging = false;
        bool moved = false;
        if (m_Dragged && !m_Dragging)
        {
            const Rml::Vector2i screen  = context->GetDimensions();
            settings.friendNoticeAcross = SpotShare(m_Dragged->x, static_cast<float>(screen.x));
            settings.friendNoticeDown   = SpotShare(m_Dragged->y, static_cast<float>(screen.y));
            m_Dragged.reset();
            moved = true;
        }
        m_Stack->SetClass(kMovingClass, moving);

        bool changed = false;
        for (int slot = 0; slot < kMaxNotices; ++slot)
        {
            Rml::Element* box = m_Document->GetElementById(NoticeId(slot));
            const std::optional<ShownNotice> notice =
                slot < static_cast<int>(shown.size()) ? std::optional(shown[static_cast<size_t>(slot)]) : std::nullopt;
            const std::optional<ShownNotice>& was = m_Filled[static_cast<size_t>(slot)];
            box->SetClass(kHiddenClass, !notice);
            PartOf(box, kBar)->SetClass(kHiddenClass, !settings.friendNoticeBar);
            if (!notice)
            {
                changed = changed || was.has_value();
                m_Filled[static_cast<size_t>(slot)].reset();
                continue;
            }
            if (!was || was->id != notice->id || was->kind != notice->kind || was->name != notice->name)
            {
                box->SetClass(kOnlineClass, notice->kind == NoticeKind::Online);
                box->SetClass(kOfflineClass, notice->kind == NoticeKind::Offline);
                PartOf(box, kTitle)->SetInnerRML(EscapedRml(NoticeTitle(notice->kind)));
                PartOf(box, kMessage)->SetInnerRML(EscapedRml(NoticeMessage(notice->kind, notice->name)));
                changed = true;
            }
            box->SetProperty(Rml::PropertyId::Opacity, Rml::Property(notice->opacity, Rml::Unit::NUMBER));
            PartOf(box, kBar)->SetProperty(
                Rml::PropertyId::Width, Rml::Property(notice->share * kPercentWhole, Rml::Unit::PERCENT));
            m_Filled[static_cast<size_t>(slot)] = notice;
        }
        if (changed) m_Document->UpdateDocument();

        std::vector<float> heights;
        for (size_t slot = 0; slot < shown.size(); ++slot)
            heights.push_back(m_Document->GetElementById(NoticeId(static_cast<int>(slot)))
                                  ->GetBox()
                                  .GetSize(Rml::BoxArea::Border)
                                  .y);
        const std::vector<float> tops = NoticeTops(heights, 0.0f, settings.friendNoticeUpward);
        m_Upward      = settings.friendNoticeUpward;
        m_FirstHeight = heights.empty() ? 0.0f : heights.front();
        std::map<uint32_t, float> at;
        for (size_t slot = 0; slot < shown.size(); ++slot)
        {
            const auto before = m_At.find(shown[slot].id);
            const float top   = before == m_At.end() ? tops[slot] : Slid(before->second, tops[slot], seconds);
            at[shown[slot].id] = top;
            m_Document->GetElementById(NoticeId(static_cast<int>(slot)))
                ->SetProperty(Rml::PropertyId::Top, Rml::Property(top, Rml::Unit::PX));
        }
        m_At = std::move(at);

        if (!m_Dragging)
        {
            const Rml::Vector2i screen = context->GetDimensions();
            const NoticeSpot spot = NoticeAnchor(settings.friendNoticeAcross, settings.friendNoticeDown,
                settings.friendNoticeUpward, static_cast<float>(screen.x), static_cast<float>(screen.y), barBottom,
                m_FirstHeight);
            m_Stack->SetProperty(Rml::PropertyId::Left, Rml::Property(spot.x, Rml::Unit::PX));
            m_Stack->SetProperty(Rml::PropertyId::Top, Rml::Property(spot.y, Rml::Unit::PX));
        }
        return moved;
    }

    void NoticeBox::ProcessEvent(Rml::Event& event)
    {
        if (!m_Grabbable) return;
        const Rml::Vector2f pointer{event.GetParameter<float>("mouse_x", 0.0f), event.GetParameter<float>("mouse_y", 0.0f)};
        switch (event.GetId())
        {
        case Rml::EventId::Dragstart:
            m_Grab     = pointer - m_Stack->GetAbsoluteOffset(Rml::BoxArea::Border);
            m_Dragging = true;
            break;
        case Rml::EventId::Drag:
        {
            const Rml::Vector2i screen = m_Document->GetContext()->GetDimensions();
            const Rml::Vector2f corner = pointer - m_Grab;
            const float width = static_cast<float>(screen.x), height = static_cast<float>(screen.y);
            // Through the saved shares, so it lands where the settings will put it, kept on the screen.
            m_Dragged = NoticeAnchor(SpotShare(corner.x, width), SpotShare(corner.y, height), m_Upward, width, height,
                0.0f, m_FirstHeight);
            m_Stack->SetProperty(Rml::PropertyId::Left, Rml::Property(m_Dragged->x, Rml::Unit::PX));
            m_Stack->SetProperty(Rml::PropertyId::Top, Rml::Property(m_Dragged->y, Rml::Unit::PX));
            break;
        }
        case Rml::EventId::Dragend: m_Dragging = false; break;
        default: break;
        }
    }

    void NoticeBox::Close()
    {
        if (m_Document == nullptr) return;
        for (const Rml::EventId id : {Rml::EventId::Dragstart, Rml::EventId::Drag, Rml::EventId::Dragend})
            m_Stack->RemoveEventListener(id, this);
        m_Document->Close();
        m_Document = nullptr;
        m_Stack    = nullptr;
        m_Filled.clear();
        m_At.clear();
        m_Dragging = false;
        m_Dragged.reset();
    }
}

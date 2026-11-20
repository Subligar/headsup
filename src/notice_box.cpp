#include "notice_box.h"

#include "bar_document.h"
#include "bar_look.h"
#include "markup.h"
#include "menu_document.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr int kNoticesZ           = kMenuWindowZ - 1; // under the settings window
        constexpr float kBoxPadAcross     = 10.0f, kBoxPadDown = 8.0f;
        constexpr float kBarGap           = 6.0f; // between the message and the countdown bar
        constexpr const char* kMovingClass = "moving";
        constexpr const char* kOnlineClass = "online", *kOfflineClass = "offline", *kInviteClass = "invite";
        constexpr const char* kTextClass = "text", *kTitleClass = "title", *kMessageClass = "message";
        constexpr const char* kButtonsClass = "buttons", *kBarClass = "bar";
        constexpr const char* kButtonClass = "btn", *kDeclineClass = "decline", *kAcceptClass = "accept";
        constexpr float kButtonsGap = 6.0f, kButtonPad = 3.0f; // above the row and between its two; inside each

        const char* KindClass(NoticeKind kind)
        {
            if (IsInvite(kind)) return kInviteClass;
            return kind == NoticeKind::Online ? kOnlineClass : kOfflineClass;
        }

        const char* AlignWord(HelpAnchor align)
        {
            switch (align)
            {
            case HelpAnchor::Left: return "left";
            case HelpAnchor::Center: return "center";
            case HelpAnchor::Right: return "right";
            }
            return "left";
        }

        template <typename Look>
        std::string NoticeRml(const Look& look)
        {
            const BarPalette& p = look.palette;
            const auto font = [](const std::string& family, float size) {
                return "font-family:\"" + family + "\";font-size:" + Px(size) + ";";
            };
            const bool fixedHeight = look.height != kFlexibleNoticeHeight;
            std::string css = "body{display:block;width:100%;height:100%;pointer-events:none;z-index:" +
                              std::to_string(kNoticesZ) + ";}";
            css += "div{display:block;}";
            // As wide as a Flexible box may be, so a box's shrink to its words is never cut short by the stack.
            css += std::string("#") + kNoticeStackId + "{position:absolute;width:" + Px(kMaxNoticeWidth) + ";height:0px;}";
            css += std::string("#") + kNoticeStackId + "." + kMovingClass + "{pointer-events:auto;drag:drag;}";
            css += std::string(".box{position:absolute;left:0px;top:0px;box-sizing:border-box;display:flex;flex-direction:column;") +
                   // A Flexible box is laid out at its cap, then given its words' width: RmlUi lays an
                   // absolutely placed flex box as wide as the stack, not as its words.
                   "width:" + Px(static_cast<float>(look.width == kFlexibleNoticeWidth ? kMaxNoticeWidth : look.width)) + ";" +
                   (fixedHeight ? "height:" + Px(static_cast<float>(look.height)) + ";" : std::string()) +
                   "padding:" + Px(kBoxPadDown) + " " + Px(kBoxPadAcross) + ";border-radius:" + Px(look.radius) + ";" +
                   (look.skin ? SkinEdgeRules(*look.skin)
                              : "border:1px " + RcssColor(p.panelBorder) + ";decorator:vertical-gradient(" +
                                    RcssColor(p.panelTop) + " " + RcssColor(p.panelBottom) + ");") +
                   "}";
            // The art sits behind the text, which is positioned to be drawn after it.
            if (look.skin)
                css += ArtRules(".box", look.radius, SkinFillRules(*look.skin)) + ".text,.buttons,.bar{position:relative;}";
            css += std::string(".") + kTextClass + "{flex:1 1 auto;min-height:0px;text-align:" + AlignWord(look.align) + ";" +
                   (fixedHeight ? "overflow:hidden;" : "") + "}";
            // Shrink stops at kMinNoticeText, so it is cut there too rather than run out of the box.
            const std::string oneLine = "white-space:nowrap;overflow:hidden;text-overflow:ellipsis;";
            css += ".title{" + oneLine + font(look.titleFont, look.titleSize) +
                   ShadowEffect(static_cast<int>(std::lround(look.titleSize))) + "}";
            css += ".message{" + oneLine + font(look.textFont, look.textSize) + "color:" + RcssColor(p.text) + ";" +
                   ShadowEffect(static_cast<int>(std::lround(look.textSize))) + "}";
            css += ".buttons,.bar{flex:0 0 auto;}";
            // Only the buttons take the pointer: pointer-events is inherited, and the body's is none.
            css += std::string(".") + kButtonsClass + "{display:flex;flex-direction:row;margin-top:" + Px(kButtonsGap) + ";}";
            css += std::string(".") + kButtonClass + "{flex:1 1 0px;pointer-events:auto;text-align:center;padding:" +
                   Px(kButtonPad) + " 0px;border:1px " + RcssColor(p.tagBorder) + ";border-radius:" + Px(look.radius) +
                   ";background-color:" + RcssColor(p.tagFill) + ";color:" + RcssColor(p.tagText) + ";" +
                   font(look.textFont, look.textSize) + "}";
            css += std::string(".") + kButtonClass + ":hover{background-color:" + RcssColor(p.highlight) + ";}";
            css += std::string(".") + kDeclineClass + "{margin-right:" + Px(kButtonsGap) + ";}";
            css += ".bar{height:" + Px(kNoticeBarHeight) + ";margin-top:" + Px(kBarGap) + ";}";
            for (const NoticeKind kind : {NoticeKind::Online, NoticeKind::Offline, NoticeKind::PartyInvite})
            {
                const std::string kindClass = KindClass(kind);
                const std::string color     = RcssColor(NoticeColor(kind));
                css += "." + kindClass + " .title{color:" + color + ";}." + kindClass + " .bar{background-color:" + color + ";}";
            }
            // Last, so it wins over the box's own display.
            css += std::string(".") + kHiddenClass + "{display:none;}";
            std::string boxes;
            for (int slot = 0; slot < kNoticeBoxes; ++slot)
                boxes += "<div id=\"" + NoticeId(slot) + "\" class=\"box " + kHiddenClass + "\">" +
                         (look.skin ? kArtRml : "") + "<div class=\"" + kTextClass + "\"><div class=\"" + kTitleClass +
                         "\"></div><div class=\"" + kMessageClass + "\"></div></div><div class=\"" + kButtonsClass +
                         " " + kHiddenClass + "\"><div class=\"" + kButtonClass + " " + kDeclineClass + "\">" +
                         kDeclineWord + "</div><div class=\"" + kButtonClass + " " + kAcceptClass + "\">" + kAcceptWord +
                         "</div></div><div class=\"" + kBarClass + "\"></div></div>";
            return "<rml><head><style>" + css + "</style></head><body id=\"" + std::string(kNoticeDocument) +
                   "\"><div id=\"" + kNoticeStackId + "\">" + boxes + "</div></body></rml>";
        }

        Rml::Element* Part(Rml::Element* box, const char* name)
        {
            Rml::ElementList found;
            box->GetElementsByClassName(found, name);
            return found.front();
        }

        Rml::Vector2f Size(Rml::Element* element, Rml::BoxArea area = Rml::BoxArea::Border)
        {
            return element->GetBox().GetSize(area);
        }
    }

    std::string NoticeId(int slot) { return "notice" + std::to_string(slot); }

    NoticeTook NoticeBox::Update(Rml::Context* context, const BarPalette& palette, Settings& settings,
        const std::vector<ShownNotice>& shown, const NoticeFrame& frame)
    {
        NoticeTook took;
        if (shown.empty() && !frame.moving && m_Document == nullptr) return took;
        const BarMetrics metrics = MetricsOf(settings, palette.shape, WidgetOptions{});
        const Look look{palette, BarTitleFont(settings, palette.shape), BarTextFont(settings), metrics.title,
            metrics.subtitle, frame.skin, settings.noticeWidth, settings.noticeHeight, settings.noticeFit,
            settings.noticeAlign, NoticeRadius(settings.noticeRounding, OwnPalette(settings).windowRounding, metrics.panelRadius)};
        if (m_Document != nullptr && !(look == m_Look)) Close();
        if (m_Document == nullptr)
        {
            m_Document = context->LoadDocumentFromMemory(NoticeRml(look), kNoticeDocument);
            if (m_Document == nullptr) return took;
            m_Look  = look;
            m_Stack = m_Document->GetElementById(kNoticeStackId);
            m_Filled.assign(kNoticeBoxes, std::nullopt);
            m_At.clear();
            for (const Rml::EventId id : {Rml::EventId::Dragstart, Rml::EventId::Drag, Rml::EventId::Dragend})
                m_Stack->AddEventListener(id, this);
            m_Document->AddEventListener(Rml::EventId::Click, this);
            m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        }
        // Moving let go mid-drag ends the drag where the stack is, whatever the pointer does until the button goes up.
        m_Grabbable = frame.moving;
        if (!frame.moving) m_Dragging = false;
        if (m_Dragged && !m_Dragging)
        {
            const Rml::Vector2i screen = context->GetDimensions();
            settings.noticeAcross      = SpotShare(m_Dragged->x, static_cast<float>(screen.x));
            settings.noticeDown        = SpotShare(m_Dragged->y, static_cast<float>(screen.y));
            m_Dragged.reset();
            took.moved = true;
        }
        m_Stack->SetClass(kMovingClass, frame.moving);
        took.press = std::exchange(m_Press, std::nullopt);

        std::vector<int> refit; // boxes whose words changed, to size and fit once laid out
        bool changed = false;
        for (int slot = 0; slot < kNoticeBoxes; ++slot)
        {
            Rml::Element* box = m_Document->GetElementById(NoticeId(slot));
            const std::optional<ShownNotice> notice =
                slot < static_cast<int>(shown.size()) ? std::optional(shown[static_cast<size_t>(slot)]) : std::nullopt;
            const std::optional<ShownNotice>& was = m_Filled[static_cast<size_t>(slot)];
            box->SetClass(kHiddenClass, !notice);
            Part(box, kBarClass)->SetClass(kHiddenClass, !settings.noticeBar);
            Part(box, kButtonsClass)->SetClass(kHiddenClass, !notice || !notice->buttons);
            if (!notice)
            {
                changed = changed || was.has_value();
                m_Filled[static_cast<size_t>(slot)].reset();
                continue;
            }
            if (!was || was->id != notice->id || was->kind != notice->kind || was->name != notice->name ||
                was->buttons != notice->buttons)
            {
                for (const char* kindClass : {kOnlineClass, kOfflineClass, kInviteClass})
                    box->SetClass(kindClass, std::string_view(kindClass) == KindClass(notice->kind));
                for (const char* line : {kTitleClass, kMessageClass}) Part(box, line)->RemoveProperty(Rml::PropertyId::FontSize);
                Part(box, kTitleClass)->SetInnerRML(EscapedRml(NoticeTitle(notice->kind)));
                Part(box, kMessageClass)->SetInnerRML(EscapedRml(NoticeMessage(notice->kind, notice->name)));
                refit.push_back(slot);
                changed = true;
            }
            box->SetProperty(Rml::PropertyId::Opacity, Rml::Property(notice->opacity, Rml::Unit::NUMBER));
            Part(box, kBarClass)->SetProperty(
                Rml::PropertyId::Width, Rml::Property(notice->share * kPercentWhole, Rml::Unit::PERCENT));
            m_Filled[static_cast<size_t>(slot)] = notice;
        }
        if (changed) m_Document->UpdateDocument();
        for (const int slot : refit)
        {
            Rml::Element* box     = m_Document->GetElementById(NoticeId(slot));
            Rml::Element* title   = Part(box, kTitleClass);
            Rml::Element* message = Part(box, kMessageClass);
            const ShownNotice& notice = shown[static_cast<size_t>(slot)];
            const std::string titleText = NoticeTitle(notice.kind), messageText = NoticeMessage(notice.kind, notice.name);
            const auto titleWide   = static_cast<float>(Rml::ElementUtilities::GetStringWidth(title, titleText));
            const auto messageWide = static_cast<float>(Rml::ElementUtilities::GetStringWidth(message, messageText));
            float room = Size(box, Rml::BoxArea::Content).x;
            bool capped = true;
            if (look.width == kFlexibleNoticeWidth)
            {
                const float edges = Size(box).x - room; // padding and border
                const float wide  = std::min(static_cast<float>(kMaxNoticeWidth), edges + std::ceil(std::max(titleWide, messageWide)));
                box->SetProperty(Rml::PropertyId::Width, Rml::Property(wide, Rml::Unit::PX));
                room   = wide - edges;
                capped = wide >= static_cast<float>(kMaxNoticeWidth); // under its cap it is as wide as its words
            }
            if (look.fit != NoticeFit::Shrink) continue;
            const FittedText fit = FittedSizes(look.titleSize, look.textSize, titleWide, messageWide, capped ? room : 0.0f,
                Size(title).y, Size(message).y,
                look.height != kFlexibleNoticeHeight ? Size(Part(box, kTextClass), Rml::BoxArea::Content).y : 0.0f);
            // Whole pixels, down: the font is drawn at a whole size, and rounding up could be wider than the room.
            if (fit.title < look.titleSize)
                title->SetProperty(Rml::PropertyId::FontSize, Rml::Property(std::floor(fit.title), Rml::Unit::PX));
            if (fit.message < look.textSize)
                message->SetProperty(Rml::PropertyId::FontSize, Rml::Property(std::floor(fit.message), Rml::Unit::PX));
        }
        if (!refit.empty()) m_Document->UpdateDocument();

        std::vector<Rml::Vector2f> sizes;
        std::vector<float> heights;
        for (size_t slot = 0; slot < shown.size(); ++slot)
        {
            sizes.push_back(Size(m_Document->GetElementById(NoticeId(static_cast<int>(slot)))));
            heights.push_back(sizes.back().y);
        }
        const std::vector<float> tops = NoticeTops(heights, 0.0f, settings.noticeEdge);
        m_Anchor      = settings.noticeAnchor;
        m_Edge        = settings.noticeEdge;
        m_Wide        = 0.0f;
        for (const Rml::Vector2f& size : sizes) m_Wide = std::max(m_Wide, size.x);
        m_FirstHeight = heights.empty() ? 0.0f : heights.front();
        std::map<uint32_t, float> at;
        std::vector<NoticeRect> placed;
        for (size_t slot = 0; slot < shown.size(); ++slot)
        {
            const auto before = m_At.find(shown[slot].id);
            const float top   = before == m_At.end() ? tops[slot] : Slid(before->second, tops[slot], frame.seconds);
            const float left  = -NoticeLeft(sizes[slot].x, m_Anchor);
            at[shown[slot].id] = top;
            Rml::Element* box  = m_Document->GetElementById(NoticeId(static_cast<int>(slot)));
            box->SetProperty(Rml::PropertyId::Left, Rml::Property(left, Rml::Unit::PX));
            box->SetProperty(Rml::PropertyId::Top, Rml::Property(top, Rml::Unit::PX));
            placed.push_back({left, top, sizes[slot].x, sizes[slot].y});
        }
        m_At = std::move(at);

        const Rml::Vector2f stackAt = m_Stack->GetAbsoluteOffset(Rml::BoxArea::Border);
        NoticeSpot spot{stackAt.x, stackAt.y}; // where a drag has it
        if (!m_Dragging)
        {
            const Rml::Vector2i screen = context->GetDimensions();
            spot = NoticeAnchor(settings.noticeAcross, settings.noticeDown, m_Anchor, m_Edge, static_cast<float>(screen.x),
                static_cast<float>(screen.y), frame.barBottom, m_Wide, m_FirstHeight);
            m_Stack->SetProperty(Rml::PropertyId::Left, Rml::Property(spot.x, Rml::Unit::PX));
            m_Stack->SetProperty(Rml::PropertyId::Top, Rml::Property(spot.y, Rml::Unit::PX));
        }
        m_Bounds.reset();
        m_Boxes.clear();
        for (const NoticeRect& box : placed)
        {
            const NoticeRect on{spot.x + box.left, spot.y + box.top, box.width, box.height};
            m_Boxes.push_back(on);
            if (!m_Bounds)
            {
                m_Bounds = on;
                continue;
            }
            const float right = std::max(m_Bounds->left + m_Bounds->width, on.left + on.width);
            const float bottom = std::max(m_Bounds->top + m_Bounds->height, on.top + on.height);
            m_Bounds->left   = std::min(m_Bounds->left, on.left);
            m_Bounds->top    = std::min(m_Bounds->top, on.top);
            m_Bounds->width  = right - m_Bounds->left;
            m_Bounds->height = bottom - m_Bounds->top;
        }
        took.hovered = frame.pointer && OverAny(m_Boxes, frame.pointer->x, frame.pointer->y);
        return took;
    }

    void NoticeBox::ProcessEvent(Rml::Event& event)
    {
        if (event.GetId() == Rml::EventId::Click)
        {
            Rml::Element* button = event.GetTargetElement();
            while (button != nullptr && !button->IsClassSet(kButtonClass)) button = button->GetParentNode();
            if (button == nullptr) return;
            for (int slot = 0; slot < static_cast<int>(m_Filled.size()); ++slot)
                if (m_Filled[static_cast<size_t>(slot)] && button->GetParentNode()->GetParentNode() == m_Document->GetElementById(NoticeId(slot)))
                    m_Press = NoticePress{m_Filled[static_cast<size_t>(slot)]->id, button->IsClassSet(kAcceptClass)};
            return;
        }
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
            m_Dragged = NoticeAnchor(SpotShare(corner.x, width), SpotShare(corner.y, height), m_Anchor, m_Edge, width,
                height, 0.0f, m_Wide, m_FirstHeight);
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
        m_Document->RemoveEventListener(Rml::EventId::Click, this);
        m_Document->Close();
        m_Document = nullptr;
        m_Stack    = nullptr;
        m_Filled.clear();
        m_At.clear();
        m_Bounds.reset();
        m_Boxes.clear();
        m_Press.reset();
        m_Dragging = false;
        m_Dragged.reset();
    }
}

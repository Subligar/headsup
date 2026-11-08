#include "rml_bar.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace headsup
{
    namespace
    {
        // Each widget's Linked Padding, by SlotOf.
        std::array<float, kPanelSlots> LinkedRoom(const Settings& settings)
        {
            std::array<float, kPanelSlots> room{};
            for (int slot = 0; slot < kPanelSlots; ++slot)
                room[static_cast<size_t>(slot)] = static_cast<float>(settings.widgets[static_cast<size_t>(slot)].linkPadding);
            return room;
        }

        constexpr float kFitSteps = 10.0f; // a fitted text's room is kept in tenths of a pixel
        constexpr int kFewLetters = 3;     // the least of the target's name worth its room on the bar
        constexpr const char* kCapital = "H"; // its ink is a font's capitals'
        constexpr const char* kDocumentName      = "top panel";

        void Set(Rml::Element* element, Rml::PropertyId property, std::optional<float>& was, float value)
        {
            if (element == nullptr || was == value) return;
            was = value;
            element->SetProperty(property, Rml::Property(value, Rml::Unit::PX));
        }
    }

    void RmlBar::Close()
    {
        if (m_Document != nullptr) m_Document->Close();
        m_Document = nullptr;
        m_Visible  = false;
        m_Shown    = {};
        m_Dividers = {};
        m_RightCap = nullptr;
        m_Help     = nullptr;
        m_HelpDrawn.reset();
        ClosePopup();
    }

    void RmlBar::ClosePopup()
    {
        for (Rml::ElementDocument*& document : m_PopupDocuments)
            if (document != nullptr) document->Close();
        m_PopupDocuments = {};
        m_Popups        = {};
        m_SettingsBody.reset();
    }

    const RmlBar::FaceLine& RmlBar::FaceLineOf(RmlHost& host, const char* family, int pixels)
    {
        for (const FaceLine& known : m_FaceLines)
            if (known.pixels == pixels && known.family == family) return known;
        const Rml::FontMetrics metrics = host.Metrics(family, pixels);
        const LineInk ink              = m_Gdi.FontInk(family, pixels, kDetailGlyphs);
        int width = 0, height = 0;
        m_Gdi.Extent("H", family, pixels, 0, width, height);
        FaceLine line{family, pixels, static_cast<float>(height), ink.top, ink.bottom,
            WholePixels(metrics.ascent + metrics.descent)};
        // RmlUi puts a line's baseline its font's ascent under half of what the line's height has over the font's.
        const float baseline =
            (static_cast<float>(line.rmlHeight) - (metrics.ascent + metrics.descent)) / 2.0f + metrics.ascent;
        line.drop = std::round(ink.baseline - baseline);
        return m_FaceLines.emplace_back(line);
    }

    void RmlBar::Release()
    {
        Close();
        m_Gdi.Release();
    }

    float RmlBar::Width(RmlHost& host, const std::string& text, const TextFace& face) const
    {
        return host.TextWidth(text, face.family, WholePixels(face.size));
    }

    float RmlBar::SpansWidth(RmlHost& host, const std::vector<Span>& spans, const TextFace& face) const
    {
        float width = 0.0f;
        for (const Span& s : spans)
            width += MarkWidth(s, face.size) + (s.room.empty() ? Width(host, s.text, face) : s.box);
        return width;
    }


    char RmlBar::WidestDigitOf(RmlHost& host, const TextFace& face)
    {
        const int pixels = WholePixels(face.size);
        for (const FaceDigit& known : m_WidestDigits)
            if (known.pixels == pixels && known.family == face.family) return known.digit;
        const char digit = WidestDigit([&](char d) { return Width(host, std::string(1, d), face); });
        m_WidestDigits.push_back(FaceDigit{face.family, pixels, digit});
        return digit;
    }

    float RmlBar::SteadyWidth(RmlHost& host, std::vector<Span> spans, const TextFace& face)
    {
        // Never less than the line as it reads now, should a pair of its real figures kern wider.
        const float shown = SpansWidth(host, spans, face);
        const char digit  = WidestDigitOf(host, face);
        for (Span& s : spans)
            s.text = SteadyFigures(s.text, digit);
        return std::max(shown, SpansWidth(host, spans, face));
    }

    const std::string& RmlBar::Fitted(RmlHost& host, Memo<FitKey, std::string>& memo, std::string text,
        const TextFace& face, float room)
    {
        // Rounded down, so the cut text never needs more than the room it was given.
        const long tenths = std::lround(std::floor(room * kFitSteps));
        return memo.Get(FitKey{std::move(text), face.family, WholePixels(face.size), tenths}, [&](const FitKey& key) {
            return FitText(key.text, static_cast<float>(key.room) / kFitSteps,
                [&](const std::string& s) { return Width(host, s, face); });
        });
    }

    BarLinePlace RmlBar::PlaceLine(RmlHost& host, const char* family, int pixels, const char* glyphs, float top)
    {
        BarLinePlace line;
        if (pixels <= 0) return line;
        const LineInk ink = m_Gdi.FontInk(family, pixels, glyphs);
        int width = 0, height = 0;
        m_Gdi.Extent("H", family, pixels, 0, width, height);
        const Rml::FontMetrics metrics = host.Metrics(family, pixels);
        line.height                    = WholePixels(metrics.ascent + metrics.descent);
        // RmlUi puts a line's baseline its font's ascent under half of what the line's height has over the font's.
        const float baseline =
            (static_cast<float>(line.height) - (metrics.ascent + metrics.descent)) / 2.0f + metrics.ascent;
        const float shift = ink.baseline - baseline;
        line.top          = std::round(top) + shift;
        // The icons after a line's text are centered on the line GDI draws, as tall as its letters' size.
        const float icon = std::round(static_cast<float>(pixels));
        line.iconDrop    = std::round(std::round(top) + (static_cast<float>(height) - icon) / 2.0f) - std::round(line.top);
        const LineInk capital = m_Gdi.FontInk(family, pixels, kCapital);
        line.capsMiddle       = baseline - (capital.baseline - capital.top) / 2.0f;
        return line;
    }

    const RmlBar::Rows& RmlBar::RowsFor(RmlHost& host, const BarMetrics& m, BarShape shape, float fontRatio, int helpText,
        const char* titleFont, const char* detailFont)
    {
        return m_Rows.Get(LinesKey{m.height, shape, fontRatio, titleFont, detailFont, helpText},
            [&](const LinesKey&) { return MakeRows(host, m, titleFont, detailFont, helpText); });
    }

    RmlBar::Rows RmlBar::MakeRows(RmlHost& host, const BarMetrics& m, const char* titleFont, const char* detailFont,
        int helpText)
    {
        Rows rows;
        rows.lines = FitTextRow(m, [&](bool title, int pixels) {
            return title ? m_Gdi.FontInk(titleFont, pixels, kTitleGlyphs)
                         : m_Gdi.FontInk(detailFont, pixels, kDetailGlyphs);
        });
        const TextRow& t = rows.lines;
        rows.place.title        = PlaceLine(host, titleFont, t.title, kTitleGlyphs, t.titleY);
        rows.place.figures      = PlaceLine(host, detailFont, t.title, kTitleGlyphs, t.titleY);
        rows.place.alone        = PlaceLine(host, titleFont, t.alone, kTitleGlyphs, t.aloneY);
        rows.place.figuresAlone = PlaceLine(host, detailFont, t.alone, kTitleGlyphs, t.aloneY);
        rows.place.detail       = PlaceLine(host, detailFont, t.detail, kDetailGlyphs, t.detailY);
        const int helpPixels    = HelpTextPixels(helpText, t.title);
        rows.place.helpCaption  = PlaceLine(host, titleFont, helpPixels, kTitleGlyphs, 0.0f);
        rows.place.help         = PlaceLine(host, detailFont, helpPixels, kTitleGlyphs, 0.0f);
        // A line box is as tall for any text in its font: the caption's and the help's, as GDI draws them.
        int width = 0, caption = 0, help = 0;
        m_Gdi.Extent("H", titleFont, helpPixels, 0, width, caption);
        m_Gdi.Extent("H", detailFont, helpPixels, 0, width, help);
        rows.helpBarHeight = std::round(static_cast<float>(std::max(caption, help)) + 2.0f * HelpBarPad(m));
        return rows;
    }

    bool RmlBar::Document(RmlHost& host, const BarLook& look, const BarTextPlace& place)
    {
        // Built again whenever its text would differ, so every setting it reads takes effect.
        std::string text = BarDocument(look, place);
        if (m_Document != nullptr && text == m_DocumentText) return true;
        // A strip of other colors or rows is another image: the one before is let go, or every step of a color's
        // slider would keep its own.
        std::string strip = StripSource(look.palette, WholePixels(look.metrics.height), WholePixels(look.metrics.below));
        if (m_Document != nullptr && strip != m_StripSource) Rml::ReleaseTexture(m_StripSource);
        Close();
        m_Document = host.Bar()->LoadDocumentFromMemory(text, kDocumentName);
        if (m_Document == nullptr) return false;
        m_DocumentText = std::move(text);
        m_StripSource  = std::move(strip);
        for (int slot = 0; slot < kPanelSlots; ++slot)
            if (OnTheBar(WidgetInSlot(slot).kind)) m_Shown[slot].element = m_Document->GetElementById(WidgetId(WidgetInSlot(slot)));
        for (int d = 0; d < kDividerCount; ++d)
            m_Dividers[d].element = m_Document->GetElementById(DividerId(d));
        m_RightCap     = m_Document->GetElementById(kRightCapId);
        m_RightCapLeft.reset();
        m_Help         = m_Document->GetElementById(kHelpId);
        m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        m_Visible = true;
        return true;
    }

    BarLayout RmlBar::PlaceWidgets(RmlHost& host, const BarLook& look, const BarState& state, float screenWidth,
        Widgets& widgets)
    {
        const BarMetrics& m = look.metrics;
        std::array<float, kPanelSlots> natural{};
        widgets.panel = Panel(look.settings);
        for (int i = 0; i < kPanelSlots; ++i)
        {
            WidgetContent& c = widgets.contents[i];
            c                = BarWidgetContent(WidgetInSlot(i), state, look.settings);
            widgets.hasPopup[i] = c.hasPopup; // kept off the panel, a combined widget's popup still joins its pill's
            if (!c.shown) continue;
            const auto steady = [&](std::vector<Span>& line, const TextFace& face) {
                if (std::ranges::none_of(line, [](const Span& s) { return !s.room.empty(); })) return;
                SteadyBoxes(line, Width(host, std::string(1, WidestDigitOf(host, face)), face),
                    [&](const std::string& text) { return Width(host, text, face); });
            };
            if (!c.cells)
            {
                steady(c.title, look.TitleFace(c));
                steady(c.subtitle, look.DetailFace());
            }
            Shown& shown        = m_Shown[i];
            if (!shown.measured || !(shown.content == c))
            {
                shown.content  = c;
                shown.measured = true;
                // A widget of cells is as wide as its columns; another as its longer line.
                shown.columns = c.cells ? CellColumns(c, [&](bool title, const Span& span) {
                    return SteadyWidth(host, {span}, title && c.subtitle.empty() ? look.TitleFace(c) : look.DetailFace());
                }) : std::vector<float>{};
                const float text = c.cells ? CellsWidth(shown.columns, m.gap)
                                           : std::max(SteadyWidth(host, c.title, look.TitleFace(c)),
                                                 c.subtitle.empty() ? 0.0f : SteadyWidth(host, c.subtitle, look.DetailFace()));
                shown.natural  = look.Leading(c) + text;
            }
            natural[i] = shown.natural;
        }
        const std::array<WidgetPads, kPanelSlots> pads =
            PillPads(widgets.panel.placement, widgets.panel.group, natural, LinkedRoom(look.settings),
                static_cast<float>(look.settings.barWidgetPadding));
        for (int i = 0; i < kPanelSlots; ++i)
        {
            m_Shown[i].pads = pads[static_cast<size_t>(i)];
            if (natural[i] > 0.0f) natural[i] += pads[static_cast<size_t>(i)].left + pads[static_cast<size_t>(i)].right;
        }
        // The target goes when its room cannot hold its padding, its icons and the start of its longest text.
        WidgetContent& target = widgets.contents[SlotOf(BarWidget::Target)];
        const WidgetPads targetPads = pads[SlotOf(BarWidget::Target)];
        const float targetPadding   = targetPads.left + targetPads.right;
        const size_t targetName     = WidestTextSpan(target.title);
        const float targetMinimum =
            targetPadding + look.Leading(target) +
            (targetName >= target.title.size()
                    ? 0.0f
                    : Width(host, Shortest(target.title[targetName].text, kFewLetters), look.TitleFace(target)));
        SharePopups(widgets.hasPopup, widgets.panel.group);
        const BarLayout layout = LayoutBar(screenWidth, m, look.palette.shape, natural, targetMinimum,
            widgets.panel.placement, look.settings.panelSeparators, static_cast<float>(look.settings.barWidgetSpacing),
            widgets.panel.group);
        if (layout.targetWidth > 0.0f && layout.targetWidth < natural[SlotOf(BarWidget::Target)])
        {
            // On each line the longest text is cut, as the name is; the rest keeps its room.
            const float room = layout.targetWidth - targetPadding - look.Leading(target);
            const auto cut   = [&](std::vector<Span>& line, Memo<FitKey, std::string>& memo, const TextFace& face) {
                const size_t at = WidestTextSpan(line);
                if (at >= line.size()) return;
                std::string& text = line[at].text;
                const float rest  = SpansWidth(host, line, face) - Width(host, text, face);
                text = Fitted(host, memo, std::move(text), face, room - rest);
            };
            cut(target.title, m_TitleFit, look.TitleFace(target));
            cut(target.subtitle, m_SubtitleFit, look.DetailFace());
        }
        return layout;
    }

    void RmlBar::ShowWidgets(const BarLook& look, const BarLayout& layout, const Widgets& widgets,
        const BarInput& input, float screenWidth)
    {
        const BarMetrics& m = look.metrics;
        for (int w = 0; w < kPanelSlots; ++w)
        {
            Shown& shown = m_Shown[w];
            if (shown.element == nullptr) continue;
            const PanelWidget widget = WidgetInSlot(w);
            const BarRect* rect = RectOf(layout, widget);
            shown.element->SetClass(kHiddenClass, rect == nullptr);
            if (rect == nullptr) continue;
            // A plain ornate widget keeps the divider's clear space as its margin, but where it shares its pill.
            const PillJoins joins = JoinsOf(layout, widget);
            const WidgetSide before = SideOf(look, widget, shown.pads.left, joins.before);
            const WidgetSide after  = SideOf(look, widget, shown.pads.right, joins.after);
            Set(shown.element, Rml::PropertyId::Left, shown.left, rect->x);
            Set(shown.element, Rml::PropertyId::Width, shown.width, rect->width - before.margin - after.margin);
            Set(shown.element, Rml::PropertyId::MarginLeft, shown.marginLeft, before.margin);
            Set(shown.element, Rml::PropertyId::MarginRight, shown.marginRight, after.margin);
            Set(shown.element, Rml::PropertyId::PaddingLeft, shown.paddingLeft, before.padding);
            Set(shown.element, Rml::PropertyId::PaddingRight, shown.paddingRight, after.padding);
            const int section = PlaceOf(widgets.panel.placement, widget).first;
            for (int k = 0; k < kBarSectionCount; ++k)
                shown.element->SetClass(SectionClass(static_cast<BarSection>(k)), k == section);
            for (int k = static_cast<int>(WidgetAlign::Left); k < kWidgetAlignCount; ++k)
                shown.element->SetClass(AlignClass(static_cast<WidgetAlign>(k)), k == static_cast<int>(look.settings.Widget(widget).align));
            shown.element->SetClass(kJoinBeforeClass, joins.before);
            shown.element->SetClass(kJoinAfterClass, joins.after);
            const PanelWidget head = widgets.panel.group[w];
            shown.element->SetClass(kFlatClass, FlatPill(look, widgets.panel, head));
            shown.element->SetClass(kBackedClass, look.settings.widgetBackground);
            shown.element->SetClass(kLitClass, input.Lit(head, widgets.hasPopup[SlotOf(head)]));
            if (widget.kind == BarWidget::Target) shown.element->SetClass(kTargetClass, look.settings.widgetBackground);
            const WidgetContent& content = widgets.contents[w];
            if (shown.marked && shown.drawn == content) continue;
            shown.drawn  = content;
            shown.marked = true;
            shown.element->SetInnerRML(WidgetRml(content, look, shown.columns));
        }
        const auto diamond = static_cast<float>(WholePixels(m.diamond));
        for (size_t d = 0; d < m_Dividers.size(); ++d)
        {
            Divider& divider = m_Dividers[d];
            if (divider.element == nullptr) continue;
            const bool shown = d < layout.dividers.size();
            divider.element->SetClass(kHiddenClass, !shown);
            if (shown)
                Set(divider.element, Rml::PropertyId::Left, divider.left,
                    std::round(layout.dividers[d] - diamond / 2.0f));
        }
        const auto cap = static_cast<float>(WholePixels(m.diamond * kCapScale));
        Set(m_RightCap, Rml::PropertyId::Left, m_RightCapLeft, std::round(screenWidth - m.cap / 2.0f - cap / 2.0f));
    }

    BarRect RmlBar::ShowHelpBar(RmlHost& host, const BarLook& look, const Rows& rows, const BarState& state, float top,
        float screenWidth, bool& cut)
    {
        cut = false;
        HelpBarParts parts;
        if (look.settings.Widget(BarWidget::GameHelp).on && state.strip &&
            !(state.strip->caption.empty() && state.strip->help.empty()))
        {
            const BarMetrics& m        = look.metrics;
            const TextFace captionFace = HelpCaptionFace(look), helpFace = HelpFace(look);
            parts.bag                  = StripBagFill(state);
            const float bagWidth       = parts.bag ? Width(host, parts.bag->text, captionFace) : 0.0f;
            const Settings& s          = look.settings;
            const HelpBarSpan span =
                HelpBarRoom(screenWidth, HelpBarLeft(m), s.helpBarWidth, s.helpBarFullWidth, s.helpBarAnchor, s.helpBarX);
            const float room = span.width - 2.0f * m.padding - (bagWidth > 0.0f ? bagWidth + m.gap : 0.0f);
            const HelpBarFit& fit = m_HelpBar.Get(
                HelpBarKey{*state.strip, captionFace.family, helpFace.family, WholePixels(captionFace.size),
                    WholePixels(helpFace.size), std::lround(std::floor(room * kFitSteps)),
                    std::lround(m.padding * kFitSteps)},
                [&](const HelpBarKey& k) {
                    HelpBarFit made{HelpBarLine(k.strip, static_cast<float>(k.room) / kFitSteps,
                        static_cast<float>(k.gap) / kFitSteps,
                        [&](const std::string& s) { return Width(host, s, captionFace); },
                        [&](const std::string& s) { return Width(host, s, helpFace); })};
                    made.captionWidth = Width(host, made.text.caption, captionFace);
                    made.helpWidth    = Width(host, made.text.help, helpFace);
                    return made;
                });
            cut           = fit.text.cut;
            parts.caption = fit.text.caption;
            parts.help    = fit.text.help;
            const float height = HelpBarHeight(look.settings.helpBarHeight, rows.helpBarHeight);
            parts.row          = PlaceHelpBar(span, s.helpBarAnchor, s.helpBarFullWidth, top, height, m.padding, m.gap,
                         fit.captionWidth, bagWidth, fit.helpWidth);
        }
        const BarRect box = parts.row.box;
        std::pair drawn{std::move(parts), look.settings.helpBarBackground};
        if (m_Help != nullptr && drawn != m_HelpDrawn)
        {
            m_Help->SetInnerRML(HelpBarRml(drawn.first, look, rows.place));
            m_HelpDrawn = std::move(drawn);
        }
        return box;
    }

    bool RmlBar::PopupDocumentFor(RmlHost& host, const BarLook& look)
    {
        PopupFaces faces;
        for (int f = 0; f < kPopupFontCount; ++f)
        {
            const TextFace face  = look.FaceOf(static_cast<PopupFont>(f));
            const int pixels     = WholePixels(face.size);
            const FaceLine& line = FaceLineOf(host, face.family, pixels);
            faces[f]             = PopupFace{face.family, pixels, line.rmlHeight, line.drop};
        }
        std::string text = PopupDocument(look, faces);
        if (m_PopupDocuments[0] != nullptr && text == m_PopupDocumentText) return true;
        ClosePopup();
        m_PopupFaces = faces;
        for (Rml::ElementDocument*& document : m_PopupDocuments)
        {
            document = host.Overlay()->LoadDocumentFromMemory(text, kPopupDocumentName);
            if (document == nullptr)
            {
                ClosePopup();
                return false;
            }
            document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        }
        m_PopupDocuments[static_cast<size_t>(PopupLayer::OverSettings)]->SetProperty(Rml::PropertyId::ZIndex,
            Rml::Property(static_cast<float>(kPopupsOverSettingsZ), Rml::Unit::NUMBER));
        m_PopupDocuments[static_cast<size_t>(PopupLayer::OverMenu)]->SetProperty(Rml::PropertyId::ZIndex,
            Rml::Property(static_cast<float>(kPopupsOverMenuZ), Rml::Unit::NUMBER));
        m_PopupDocumentText = std::move(text);
        return true;
    }

    std::vector<PopupPlace> RmlBar::ShowPopups(RmlHost& host, const BarLook& look, const BarState& state,
        const BarLayout& layout, float screenWidth, float screenHeight, const BarInput& input, bool hidden)
    {
        m_SettingsBody.reset();
        std::vector<PopupPlace> places;
        std::array<bool, kPanelSlots> shown{};
        const std::vector<PanelWidget> order = input.Shown();
        const std::vector<PopupLayer> layers = input.Layers();
        for (size_t i = 0; i < order.size(); ++i)
            if (std::optional<PopupPlace> place =
                    ShowPopup(host, look, state, layout, screenWidth, screenHeight, input, hidden, order[i], layers[i]))
            {
                const PanelWidget pill = order[i];
                shown[SlotOf(pill)] = true;
                // Drawn in the order shown, the last on top, as input finds them.
                m_Popups[SlotOf(pill)].element->SetProperty(Rml::PropertyId::ZIndex,
                    Rml::Property(static_cast<float>(places.size()), Rml::Unit::NUMBER));
                places.push_back(std::move(*place));
            }
        for (int slot = 0; slot < kPanelSlots; ++slot)
        {
            Popup& popup = m_Popups[static_cast<size_t>(slot)];
            if (shown[static_cast<size_t>(slot)]) continue;
            for (Rml::Element* element : popup.elements)
                if (element != nullptr)
                {
                    element->SetClass(kHiddenClass, true);
                    element->SetClass(kOpenClass, false);
                }
            popup.shown = false;
        }
        return places;
    }

    std::optional<PopupPlace> RmlBar::ShowPopup(RmlHost& host, const BarLook& look, const BarState& state,
        const BarLayout& layout, float screenWidth, float screenHeight, const BarInput& input, bool hidden, PanelWidget open,
        PopupLayer layer)
    {
        const std::optional<BarRect> anchor = GroupRect(layout, open);
        if (!anchor || !PopupDocumentFor(host, look)) return std::nullopt;
        Popup& popup = m_Popups[SlotOf(open)];
        Rml::Element*& element = popup.elements[static_cast<size_t>(layer)];
        if (element == nullptr)
        {
            Rml::ElementDocument* document = m_PopupDocuments[static_cast<size_t>(layer)];
            element = document->AppendChild(document->CreateElement("div"));
            element->SetClass(kPopupClass, true);
        }
        if (popup.element != element)
        {
            // Moved over or under the settings or the window: drawn anew in that document.
            if (popup.element != nullptr)
            {
                popup.element->SetClass(kHiddenClass, true);
                popup.element->SetClass(kOpenClass, false);
            }
            popup.element = element;
            popup.marked  = false;
            popup.left = popup.top = popup.width = popup.height = std::nullopt;
        }
        const bool settings = input.SettingsShown() == open;
        if (!popup.shown) popup.keptX.reset();
        if (!popup.shown || popup.settings != settings)
        {
            popup.widest = 0.0f;
            popup.widths = {};
        }
        popup.shown    = true;
        popup.settings = settings;
        const BarPalette& p = look.palette;
        const BarMetrics& m = look.metrics;
        // Figures measured at the font's widest digit, so a popup does not change its width as its times tick.
        auto measure = [&](const Span& s, PopupFont f) {
            const TextFace face  = look.FaceOf(f);
            const FaceLine& line = FaceLineOf(host, face.family, WholePixels(face.size));
            const auto [width, fresh] =
                popup.widths[static_cast<size_t>(f)].try_emplace(SteadyFigures(s.text, WidestDigitOf(host, face)));
            if (fresh) width->second = Width(host, width->first, face);
            return TextSize{width->second, line.height, line.inkTop, line.inkBottom, 0.0f};
        };
        // The ornate panel's outer border and the line under the strip count: the gap is from them. The help bar's
        // popup goes under the help bar's box.
        const auto out        = static_cast<float>(OuterPad(PopupPanel(look)));
        const float barBottom = open.kind == BarWidget::GameHelp ? anchor->y + anchor->height : BarBottom(p, m);
        const bool pinned     = input.IsPinned(open);
        const std::optional<PopupSpot> placed = PlacedSpot(input.DraggedPopup(open), look.settings.Widget(open).popupSpot);
        // Laid out for the room under the bar wherever it is placed: a lower spot's smaller room would flow its cards
        // into more columns, widening it.
        PopupKey key{PanelPopupContent(open, state, look.settings), pinned, m.height, m.popupText,
            PopupRoom(barBottom, screenHeight, m.gap, out), popup.widest,
            settings ? std::optional(settingsRoom.value_or(SettingsRoom{})) : std::nullopt};
        bool fresh            = false;
        const PopupLayout& pl = popup.layout.Get(std::move(key), [&](const PopupKey& k) {
            fresh           = true;
            PopupLayout cut = LayoutPopup(k.content, m, k.pinned, measure, k.room, k.least, k.settings);
            CutPopup(cut, k.room, m.popupPadding);
            return cut;
        });
        popup.widest = std::max(popup.widest, pl.width);
        const BarRect under = PlacePopup(*anchor, barBottom, pl.width, pl.height, screenWidth, screenHeight, m.gap, out);
        // Placed by a drag or a spot, it opens under its widget again once that is reset.
        if (placed) popup.keptX.reset();
        else if (!popup.keptX) popup.keptX = under.x;
        const BarRect rect = PopupAt(under, pl.height, placed, screenWidth, screenHeight, BarBottom(p, m) + out, popup.keptX);
        if (fresh || !popup.marked)
        {
            popup.element->SetInnerRML(PopupRml(pl, look, m_PopupFaces));
            popup.marked = true;
        }
        Set(popup.element, Rml::PropertyId::Left, popup.left, std::round(rect.x));
        Set(popup.element, Rml::PropertyId::Top, popup.top, std::round(rect.y));
        Set(popup.element, Rml::PropertyId::Width, popup.width, rect.width);
        Set(popup.element, Rml::PropertyId::Height, popup.height, rect.height);
        popup.element->SetClass(kHiddenClass, hidden);
        popup.element->SetClass(kOpenClass, !hidden);
        PopupPlace place{open, rect};
        for (const PopupTabChip& chip : pl.chips)
            place.chips.push_back(BarTabChip{chip.key, BarRect{rect.x + chip.x, rect.y + chip.y, chip.width, chip.height},
                chip.tab, chip.press, chip.fleeting});
        place.pin = pl.pin.width > 0.0f ? BarRect{rect.x + pl.pin.x, rect.y + pl.pin.y, pl.pin.width, pl.pin.height} : BarRect{};
        place.cog = pl.cog.width > 0.0f ? BarRect{rect.x + pl.cog.x, rect.y + pl.cog.y, pl.cog.width, pl.cog.height} : BarRect{};
        place.grip = BarRect{rect.x, rect.y, rect.width, pl.titleRow};
        if (settings && !hidden) m_SettingsBody = BarRect{rect.x + pl.body.x, rect.y + pl.body.y, pl.body.width, pl.body.height};
        for (const PopupScroll& scroll : pl.scrolls)
            place.scrolls.push_back(BarScroll{scroll.key, BarRect{rect.x + scroll.x, rect.y + scroll.y, scroll.width, scroll.height},
                scroll.most, BarRect{rect.x + scroll.grip.x, rect.y + scroll.grip.y, scroll.grip.width, scroll.grip.height},
                scroll.thumb});
        return place;
    }

    void RmlBar::Update(RmlHost& host, const BarState& state, const Settings& settings, const GameSkinLook* skin,
        float screenWidth, float screenHeight, BarInput& input, bool hidden)
    {
        m_HelpReach.reset();
        m_SettingsBody.reset();
        m_BarBottom = 0.0f;
        if (!BarOn(settings) || !host.Running() || screenWidth <= 0.0f)
        {
            Close();
            input.Frame(BarLayout{}, {});
            input.SetPopups({});
            return;
        }
        const BarPalette& p = PaletteFor(settings, skin);
        const BarMetrics m  = MetricsFor(settings.barHeight, p.shape, settings.barFontRatio, settings.popupTextScale,
            settings.barIconScale, settings.popupWidthScale, settings.popupHeightScale);
        // The player's fonts where RmlUi can read them, the theme's where it cannot.
        const char* titleFont = BarTitleFont(settings, p.shape);
        if (!host.EnsureFont(titleFont)) titleFont = TitleFont(p.shape);
        const char* detailFont = BarTextFont(settings);
        if (!host.EnsureFont(detailFont)) detailFont = kDetailFont;
        const Rows& rows =
            RowsFor(host, m, p.shape, settings.barFontRatio, settings.helpBarTextSize, titleFont, detailFont);
        BarLook look{p, settings, m, PillRow(m), rows.lines, titleFont, p.shape == BarShape::Ornate, detailFont,
            std::nullopt};
        if (UsesGameArt(settings) && skin != nullptr) look.skin = FrameAsSet(settings, skin->frame);
        m_BarBottom   = BarBottom(p, m);
        Widgets widgets;
        if (!Document(host, look, rows.place))
        {
            input.Frame(BarLayout{}, {});
            input.SetPopups({});
            return;
        }
        BarLayout layout = PlaceWidgets(host, look, state, screenWidth, widgets);
        ShowWidgets(look, layout, widgets, input, screenWidth);
        // The help bar where the player moved it from its place under the top bar.
        bool helpCut        = false;
        const float helpTop = HelpBarTop(p, m) + static_cast<float>(settings.helpBarY);
        const BarRect help  = ShowHelpBar(host, look, rows, state, helpTop, screenWidth, helpCut);
        if (helpCut)
        {
            // Hovered, it opens the popup with the rest, as a widget would.
            layout.widgets.push_back(PlacedWidget{BarWidget::GameHelp, help});
            widgets.hasPopup[SlotOf(BarWidget::GameHelp)] = true;
        }
        if (settings.Widget(BarWidget::GameHelp).on)
        {
            const HelpBarSpan span = HelpBarRoom(screenWidth, HelpBarLeft(m), settings.helpBarWidth,
                settings.helpBarFullWidth, settings.helpBarAnchor, settings.helpBarX);
            m_HelpReach = BarRect{span.x, helpTop, span.width, HelpBarHeight(settings.helpBarHeight, rows.helpBarHeight)};
        }
        input.Frame(layout, widgets.hasPopup);
        input.SetPopups(ShowPopups(host, look, state, layout, screenWidth, screenHeight, input, hidden));
        if (m_Visible == hidden)
        {
            m_Visible = !hidden;
            if (hidden)
                m_Document->Hide();
            else
                m_Document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        }
        m_Gdi.Sweep();
    }
}

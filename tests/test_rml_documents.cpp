#include "bar_design.h"
#include "bar_layout.h"
#include "bar_document.h"
#include "bar_job.h"
#include "bar_widgets.h"
#include "menu_document.h"
#include "window_skin.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

using namespace headsup;

namespace
{
    constexpr int kBarHeight = 45;

    BarLook LookOf(const BarPalette& p, const Settings& settings)
    {
        const BarMetrics m  = MetricsFor(kBarHeight, p.shape);
        return BarLook{p, settings, m, PillRow(m), TextRow{13, 11, 10.0f, 24.0f, 13, 12.0f},
            BarTitleFont(settings, p.shape), p.shape == BarShape::Ornate, BarTextFont(settings), std::nullopt};
    }

    // A text's width as RmlUi draws it in a face.
    float DrawnWidth(const std::string& text, const TextFace& face)
    {
        Rml::FontEngineInterface* engine = Rml::GetFontEngineInterface();
        const Rml::FontFaceHandle handle = engine->GetFontFaceHandle(Rml::StringUtilities::ToLower(face.family),
            Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, WholePixels(face.size));
        static const Rml::String kNoLanguage;
        const Rml::TextShapingContext shaping{kNoLanguage, Rml::Style::Direction::Auto, Rml::Style::FontKerning::Auto, 0.0f};
        return static_cast<float>(engine->GetStringWidth(handle, text, shaping));
    }

    // The rooms of a widget's ticking spans, measured as the bar measures them.
    void Steady(WidgetContent& c, const BarLook& look)
    {
        const auto steady = [](std::vector<Span>& line, const TextFace& face) {
            const char digit = WidestDigit([&](char d) { return DrawnWidth(std::string(1, d), face); });
            SteadyBoxes(line, DrawnWidth(std::string(1, digit), face), [&](const std::string& t) { return DrawnWidth(t, face); });
        };
        steady(c.title, look.TitleFace(c));
        steady(c.subtitle, look.DetailFace());
    }

    float LeftOf(Rml::Element* e) { return e->GetAbsoluteOffset(Rml::BoxArea::Border).x; }
}

TEST(rmlui_reads_every_themes_bar_and_popups_without_a_complaint)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    const PopupFaces faces = {PopupFace{"Barlow Condensed", 24, 29}, PopupFace{"Barlow Condensed", 18, 22},
        PopupFace{"Barlow Condensed", 15, 18}, PopupFace{"Barlow Condensed", 15, 18}};
    Settings settings;
    std::vector<BarPalette> palettes;
    for (int t = 0; t < kFixedThemeCount; ++t)
        palettes.push_back(PaletteOf(static_cast<BarTheme>(t)));
    for (const BarShape shape : {BarShape::Ornate, BarShape::Pills, BarShape::Glass})
    {
        BarPalette custom = settings.customPalette;
        custom.shape      = shape;
        custom.border     = 3;
        palettes.push_back(custom);
    }
    struct Drawn
    {
        BarPalette palette;
        std::optional<SkinFrame> skin;
    };
    std::vector<Drawn> drawn;
    for (const BarPalette& palette : palettes)
        drawn.push_back({palette, std::nullopt});
    // The game's art, its corners squeezed on a bar shorter than two of them.
    BarPalette window = settings.customPalette;
    window.shape      = BarShape::Window;
    drawn.push_back({window, std::nullopt});
    drawn.push_back({window, SkinFrame{0, 4.0f / 3.0f, 0.5f, 1.0f}});
    drawn.push_back({window, SkinFrame{7, 3.0f, 0.75f, 0.5f}});
    for (const Drawn& d : drawn)
    {
        BarLook look = LookOf(d.palette, settings);
        look.skin    = d.skin;
        test::RmlSession rml;
        Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
        Rml::ElementDocument* popups = rml.context->LoadDocumentFromMemory(PopupDocument(look, faces), "popups");
        CHECK(bar != nullptr && popups != nullptr);
        bar->Show();
        popups->Show();
        // A popup that opens: what a theme does as one opens is read only then.
        Rml::Element* popup = popups->AppendChild(popups->CreateElement("div"));
        popup->SetClass(kPopupClass, true);
        CHECK(popup != nullptr);
        popup->SetClass(kOpenClass, true);
        rml.context->Update();
        rml.context->Render();
        CHECK_EQ(rml.Said(), "");
    }
}

TEST(a_widgets_content_sits_in_the_middle_of_a_wider_box_with_its_lines_against_its_sections_side)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    Settings settings;
    for (const BarSection section : {BarSection::Left, BarSection::Center, BarSection::Right})
    {
        test::RmlSession rml;
        Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(LookOf(PaletteOf(BarTheme::UmbraDark), settings), place), "bar");
        bar->Show();
        Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::Bags));
        widget->SetClass(kHiddenClass, false);
        widget->SetClass(SectionClass(section), true);
        widget->SetProperty(Rml::PropertyId::Width, Rml::Property(300.0f, Rml::Unit::PX));
        WidgetContent c;
        c.shown    = true;
        c.icons    = {Icon::Bag};
        c.title    = {Text("20 / 30")};
        c.subtitle = {Text("Inventory and more")};
        widget->SetInnerRML(WidgetRml(c, LookOf(PaletteOf(BarTheme::UmbraDark), settings), {}));
        rml.context->Update();
        Rml::ElementList icons, titles, details;
        widget->GetElementsByClassName(icons, "icon");
        widget->GetElementsByClassName(titles, "title");
        widget->GetElementsByClassName(details, "detail");
        const float boxLeft = widget->GetAbsoluteOffset(Rml::BoxArea::Content).x;
        const float boxRight = boxLeft + widget->GetBox().GetSize(Rml::BoxArea::Content).x;
        const float first = icons.front()->GetAbsoluteOffset(Rml::BoxArea::Border).x;
        const float title = titles.front()->GetAbsoluteOffset(Rml::BoxArea::Border).x + titles.front()->GetBox().GetSize(Rml::BoxArea::Border).x;
        const float detail = details.front()->GetAbsoluteOffset(Rml::BoxArea::Border).x + details.front()->GetBox().GetSize(Rml::BoxArea::Border).x;
        const float last = std::max(title, detail);
        // As much room before the icon as after the longer line, whatever the section.
        CHECK(std::fabs((first - boxLeft) - (boxRight - last)) <= 1.0f);
        CHECK(first - boxLeft > 20.0f);
        // The shorter title lines up with the detail line on the section's side: its left, its middle or its right.
        const float titleLeft = titles.front()->GetAbsoluteOffset(Rml::BoxArea::Border).x, detailLeft = details.front()->GetAbsoluteOffset(Rml::BoxArea::Border).x;
        Rml::Element* titleText = titles.front()->GetFirstChild();
        const float inkLeft = titleText->GetAbsoluteOffset(Rml::BoxArea::Border).x, inkRight = inkLeft + titleText->GetBox().GetSize(Rml::BoxArea::Border).x;
        CHECK(std::fabs(titleLeft - detailLeft) <= 1.0f);
        if (section == BarSection::Left) CHECK(std::fabs(inkLeft - detailLeft) <= 1.0f);
        if (section == BarSection::Right) CHECK(std::fabs(inkRight - detail) <= 1.0f);
        if (section == BarSection::Center) CHECK(std::fabs((inkLeft - detailLeft) - (detail - inkRight)) <= 1.0f);
        CHECK_EQ(rml.Said(), "");
    }
}

TEST(a_widgets_line_draws_a_span_of_spaces_alone_as_wide_as_it_is_measured)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    Settings settings;
    const BarLook look = LookOf(PaletteOf(BarTheme::UmbraDark), settings);
    test::RmlSession rml;
    Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
    bar->Show();
    Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::World));
    widget->SetClass(kHiddenClass, false);
    const auto width = [&](std::vector<Span> title) {
        WidgetContent c;
        c.shown = true;
        c.title = std::move(title);
        widget->SetInnerRML(WidgetRml(c, look, {}));
        rml.context->Update();
        Rml::ElementList lines;
        widget->GetElementsByClassName(lines, "t");
        return lines.front()->GetBox().GetSize(Rml::BoxArea::Border).x;
    };
    const float apart = width({Text("Bastok"), Text("    "), Text("Jeuno")}), together = width({Text("Bastok    Jeuno")});
    CHECK(together > width({Text("BastokJeuno")}) + 4.0f);
    CHECK(std::fabs(apart - together) <= 1.0f);
}

TEST(the_target_widget_goes_back_to_breathing_after_its_popup_without_a_complaint)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    Settings settings;
    for (int t = 0; t < kFixedThemeCount; ++t)
    {
        test::RmlSession rml;
        Rml::ElementDocument* bar =
            rml.context->LoadDocumentFromMemory(BarDocument(LookOf(PaletteOf(static_cast<BarTheme>(t)), settings), place), "bar");
        bar->Show();
        Rml::Element* target = bar->GetElementById(WidgetId(BarWidget::Target));
        CHECK(target != nullptr);
        // The pointer comes onto the widget (its popup opens), then leaves it (the popup closes).
        for (const bool lit : {true, false})
        {
            target->SetClass(kLitClass, lit);
            for (int frame = 0; frame < 10; ++frame)
            {
                rml.complaints.seconds += 0.05;
                rml.context->Update();
                rml.context->Render();
            }
        }
        CHECK_EQ(rml.Said(), "");
    }
}

TEST(every_widget_lit_has_the_same_corners_and_one_showing_its_background_has_it_unlit)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    Settings settings;
    std::vector<BarPalette> palettes;
    for (int t = 0; t < kFixedThemeCount; ++t)
        palettes.push_back(PaletteOf(static_cast<BarTheme>(t)));
    for (const BarShape shape : {BarShape::Ornate, BarShape::Pills, BarShape::Glass})
    {
        BarPalette custom = settings.customPalette;
        custom.shape      = shape;
        palettes.push_back(custom);
    }
    for (const BarPalette& palette : palettes)
    {
        const BarLook look = LookOf(palette, settings);
        test::RmlSession rml;
        Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
        bar->Show();
        // Each widget as the panel marks it.
        const auto drawn = [&](PanelWidget widget, bool lit, bool backed) -> const Rml::Style::ComputedValues& {
            settings.widgetBackground          = backed;
            const PanelGroups panel            = Panel(settings);
            Rml::Element* element              = bar->GetElementById(WidgetId(widget));
            CHECK(element != nullptr);
            element->SetClass(kHiddenClass, false);
            element->SetClass(kFlatClass, FlatPill(look, panel, widget));
            element->SetClass(kBackedClass, backed);
            element->SetClass(kLitClass, lit);
            for (int update = 0; update < 3; ++update)
            {
                rml.complaints.seconds += 1.0; // past every easing
                rml.context->Update();
            }
            return element->GetComputedValues();
        };
        std::optional<float> corners;
        for (const std::vector<PanelWidget>& section : settings.barSections)
            for (const PanelWidget widget : section)
            {
                if (widget.kind == BarWidget::Target) continue; // a frame of its own
                const float radius = drawn(widget, true, false).border_top_left_radius();
                if (!corners) corners = radius;
                if (radius != *corners)
                    throw test::Failure{std::string(BarWidgetName(widget.kind)) + " lit has corners of " +
                                        std::to_string(radius) + " px, the others " + std::to_string(*corners)};
                // Shown, it is the pill the theme gives its other widgets, or its tint where it gives them none.
                const int alpha = drawn(widget, false, true).background_color().alpha;
                if (palette.shape == BarShape::Pills ? alpha != static_cast<int>(palette.pill >> 24) : alpha == 0)
                    throw test::Failure{std::string(BarWidgetName(widget.kind)) + " shows no background"};
            }
        CHECK_EQ(rml.Said(), "");
    }
}

TEST(a_widgets_content_stands_a_pixel_above_where_its_lines_are_placed_in_every_shape)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    Settings settings;
    for (const BarShape shape : {BarShape::Ornate, BarShape::Pills, BarShape::Glass, BarShape::Window})
    {
        BarPalette palette = settings.customPalette;
        palette.shape      = shape;
        const BarLook look = LookOf(palette, settings);
        test::RmlSession rml;
        Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
        bar->Show();
        Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::Weather));
        CHECK(widget != nullptr);
        WidgetContent c;
        c.shown    = true;
        c.title    = {Text("Clear")};
        c.subtitle = {Text("Firesday")};
        widget->SetClass(kHiddenClass, false);
        widget->SetInnerRML(WidgetRml(c, look, {}));
        rml.context->Update();
        Rml::ElementList titles;
        widget->GetElementsByClassName(titles, "title");
        CHECK(!titles.empty());
        CHECK_EQ(titles.front()->GetAbsoluteOffset(Rml::BoxArea::Border).y, std::round(place.title.top) - 1.0f);
        CHECK_EQ(rml.Said(), "");
    }
}

TEST(a_target_without_its_background_looks_as_any_other_widget_lit_or_not)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    Settings settings;
    for (int t = 0; t < kFixedThemeCount; ++t)
    {
        const BarLook look = LookOf(PaletteOf(static_cast<BarTheme>(t)), settings);
        test::RmlSession rml;
        Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
        bar->Show();
        Rml::Element* target  = bar->GetElementById(WidgetId(BarWidget::Target));
        Rml::Element* weather = bar->GetElementById(WidgetId(BarWidget::Weather));
        CHECK(target != nullptr && weather != nullptr);
        target->SetClass(kTargetClass, false); // Show Background off
        for (const bool lit : {false, true})
        {
            for (Rml::Element* widget : {target, weather})
            {
                widget->SetClass(kHiddenClass, false);
                widget->SetClass(kLitClass, lit);
            }
            for (int update = 0; update < 3; ++update)
            {
                rml.complaints.seconds += 1.0; // past every easing
                rml.context->Update();
            }
            const auto& a = target->GetComputedValues();
            const auto& b = weather->GetComputedValues();
            if (a.background_color() != b.background_color() || a.border_top_color() != b.border_top_color())
                throw test::Failure{"theme " + std::to_string(t) + (lit ? " lit" : "") + ": the target is drawn apart"};
        }
        CHECK_EQ(rml.Said(), "");
    }
}

TEST(an_exp_bar_in_a_widgets_line_stands_on_its_texts_capitals_middle_with_its_own_text_on_the_lines_baseline)
{
    Settings settings;
    const BarLook look = LookOf(settings.customPalette, settings);
    test::RmlSession rml;
    Rml::FontEngineInterface* engine = Rml::GetFontEngineInterface();
    const auto metricsAt = [&](int pixels) -> const Rml::FontMetrics& {
        return engine->GetFontMetrics(engine->GetFontFaceHandle(Rml::StringUtilities::ToLower(look.detailFont),
            Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, pixels));
    };
    // Each line as RmlUi lays it out, its capitals' middle where the font's capital height puts it over the baseline.
    const auto placed = [&](int pixels, float top) {
        const Rml::FontMetrics& m = metricsAt(pixels);
        BarLinePlace line{static_cast<int>(std::lround(m.ascent + m.descent)), top, 1.0f};
        const float baseline = (static_cast<float>(line.height) - m.ascent - m.descent) / 2.0f + m.ascent;
        line.capsMiddle      = baseline - test::CapHeight(look.detailFont, static_cast<float>(pixels)) / 2.0f;
        return line;
    };
    BarTextPlace place;
    place.figures = placed(look.lines.title, 8.0f);
    place.detail  = placed(look.lines.detail, 23.0f);
    Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
    bar->Show();
    Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::Job));
    widget->SetClass(kHiddenClass, false);
    const auto top = [](Rml::Element* e) { return e->GetAbsoluteOffset(Rml::BoxArea::Border).y; };
    // Where RmlUi puts the baseline of a line of text in an element: half its leading and its font's ascent down.
    const auto baselineOf = [&](Rml::Element* e, int pixels) {
        const Rml::FontMetrics& m = metricsAt(pixels);
        return top(e) + (e->GetLineHeight() - m.ascent - m.descent) / 2.0f + m.ascent;
    };
    for (const bool onTitle : {true, false})
        for (const Align align : {Align::Left, Align::Center, Align::Right})
        {
            WidgetContent c;
            c.shown    = true;
            c.figures  = true; // as the Job & EXP widget's are
            Span exp   = Text("");
            exp.bar    = InlineBar{0.25f, 120, 14, "25%", align};
            const std::vector<Span> line{Text("1,200 til next level"), Text("  "), exp};
            c.title    = onTitle ? line : ListOf(Text("WAR 30"));
            c.subtitle = onTitle ? ListOf(Text("WAR 30")) : line;
            widget->SetInnerRML(WidgetRml(c, look, {}));
            rml.context->Update();
            Rml::ElementList bars, tracks, fills, texts, lines;
            widget->GetElementsByClassName(bars, "ibar");
            widget->GetElementsByClassName(tracks, "track");
            widget->GetElementsByClassName(fills, "fill");
            widget->GetElementsByClassName(texts, "btext");
            widget->GetElementsByClassName(lines, "t");
            CHECK(bars.size() == 1 && tracks.size() == 1 && fills.size() == 1 && texts.size() == 1 && lines.size() == 2);
            const int pixels   = onTitle ? look.lines.title : look.lines.detail;
            Rml::Element* text = lines[onTitle ? 0 : 1];
            // As long and thick as the widget's settings say, filled a quarter.
            const Rml::Vector2f track = tracks.front()->GetBox().GetSize(Rml::BoxArea::Border);
            CHECK_EQ(track.x, 120.0f);
            CHECK_EQ(track.y, 14.0f);
            const Rml::Vector2f fill = fills.front()->GetBox().GetSize(Rml::BoxArea::Border);
            CHECK(std::abs(fill.x - track.x / 4.0f) < 1.0f && fill.y == track.y);
            // Its middle on the middle of the capitals and figures beside it (the card "Job and EXP Widget": it sat
            // on the lower case's middle, low of the figures).
            const float capsMiddle = baselineOf(text, pixels) - test::CapHeight(look.detailFont, static_cast<float>(pixels)) / 2.0f;
            CHECK(std::abs(top(tracks.front()) + track.y / 2.0f - capsMiddle) <= 0.5f);
            // Its text on the line's own baseline, across the bar's width, lined up as Bar Text Alignment says.
            Rml::Element* said = texts.front();
            CHECK(std::abs(baselineOf(said, pixels) - baselineOf(text, pixels)) <= 0.01f);
            CHECK_EQ(said->GetBox().GetSize(Rml::BoxArea::Border).x, track.x);
            CHECK(said->GetAbsoluteOffset(Rml::BoxArea::Border).x == tracks.front()->GetAbsoluteOffset(Rml::BoxArea::Border).x);
            const Rml::Style::TextAlign wanted = align == Align::Left ? Rml::Style::TextAlign::Left
                                                 : align == Align::Right ? Rml::Style::TextAlign::Right
                                                                         : Rml::Style::TextAlign::Center;
            CHECK(said->GetComputedValues().text_align() == wanted);
            CHECK_EQ(said->GetInnerRML(), "25%");
            // The bar takes no more height than its line had.
            CHECK_EQ(text->GetBox().GetSize(Rml::BoxArea::Border).y, text->GetLineHeight());
        }
    CHECK_EQ(rml.Said(), "");
}

TEST(a_pop_outs_title_card_heading_and_text_draw_at_18_16_and_14_px_and_a_heading_never_wider_than_its_title)
{
    // The user's sizes at a Text Scale of 1: the title 18 px, a card's heading 16 px, its text 14 px. Each word is
    // the same in all three, so the smaller size must draw it narrower.
    constexpr const char* kWord = "Conquest";
    Settings settings;
    for (const BarTheme theme : {BarTheme::UmbraDark, BarTheme::PhoenixGold})
    {
        test::RmlSession rml;
        const BarLook look = LookOf(PaletteOf(theme), settings);
        Rml::FontEngineInterface* engine = Rml::GetFontEngineInterface();
        PopupFaces faces;
        for (int f = 0; f < kPopupFontCount; ++f)
        {
            const TextFace face = look.FaceOf(static_cast<PopupFont>(f));
            const auto pixels   = static_cast<int>(std::lround(face.size));
            const Rml::FontMetrics& m = engine->GetFontMetrics(engine->GetFontFaceHandle(Rml::StringUtilities::ToLower(face.family),
                Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, pixels));
            faces[static_cast<size_t>(f)] = PopupFace{face.family, pixels, static_cast<int>(std::lround(m.ascent + m.descent)), 0.0f};
        }
        const auto measure = [&](const Span& s, PopupFont f) {
            const PopupFace& face = faces[static_cast<size_t>(f)];
            static const Rml::String kNoLanguage;
            const Rml::TextShapingContext shaping{kNoLanguage, Rml::Style::Direction::Auto, Rml::Style::FontKerning::Auto, 0.0f};
            const Rml::FontFaceHandle handle = engine->GetFontFaceHandle(Rml::StringUtilities::ToLower(face.family),
                Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, face.pixels);
            const auto line = static_cast<float>(face.line);
            return TextSize{static_cast<float>(engine->GetStringWidth(handle, s.text, shaping)), line, 0.2f * line, line};
        };
        PopupContent content;
        content.title = {Text(kWord)};
        content.sections.push_back(PopupSection{kWord, {PopupRow{RowKind::Line, 0, {PopupCell{std::nullopt, false, {Text(kWord)}}}}}});
        const PopupLayout layout = LayoutPopup(content, look.metrics, false, measure);
        Rml::ElementDocument* popups = rml.context->LoadDocumentFromMemory(PopupDocument(look, faces), "popups");
        popups->Show();
        Rml::Element* popup = popups->AppendChild(popups->CreateElement("div"));
        popup->SetClass(kPopupClass, true);
        popup->SetInnerRML(PopupRml(layout, look, faces));
        rml.context->Update();
        struct Drawn
        {
            float pixels = 0.0f, width = 0.0f;
        };
        const auto drawn = [&](PopupFont font) {
            Rml::ElementList found;
            popup->GetElementsByClassName(found, "f" + std::to_string(static_cast<int>(font)));
            for (Rml::Element* e : found)
                if (e->GetInnerRML() == kWord)
                    return Drawn{e->GetComputedValues().font_size(), static_cast<float>(Rml::ElementUtilities::GetStringWidth(e, Rml::String(kWord)))};
            return Drawn{};
        };
        const Drawn title = drawn(PopupFont::Title), heading = drawn(PopupFont::Heading), text = drawn(PopupFont::Text);
        CHECK_EQ(title.pixels, 18.0f);
        CHECK_EQ(heading.pixels, 16.0f);
        CHECK_EQ(text.pixels, 14.0f);
        CHECK(text.width > 0.0f && text.width < heading.width && heading.width < title.width);
        CHECK_EQ(rml.Said(), "");
    }
}

// Where they all overlap, what the pointer finds on top is what is drawn on top. The card: a popup's settings, opened
// before the window, drew over the window while the rest of that popup was under it.
TEST(the_settings_window_draws_over_a_popup_and_its_settings_opened_before_it_and_under_those_after)
{
    test::RmlSession rml;
    Settings settings;
    const BarLook look = LookOf(PaletteOf(BarTheme::UmbraDark), settings);
    const PopupFaces faces = {PopupFace{"Barlow Condensed", 18, 22}, PopupFace{"Barlow Condensed", 14, 17},
        PopupFace{"Barlow Condensed", 16, 19}, PopupFace{"Barlow Condensed", 16, 19}};
    const MenuLook menu{MenuTheme::Phoenix, kBarlowCondensed, kMarcellusSC, 16, 0, false, kPhoenixMenuColors, std::nullopt};
    // Each popup document as the bar makes it, a popup at the top left, and the settings view and window there too.
    const auto popups = [&](std::optional<int> z) {
        Rml::ElementDocument* d = rml.context->LoadDocumentFromMemory(PopupDocument(look, faces), "popups");
        if (z) d->SetProperty(Rml::PropertyId::ZIndex, Rml::Property(static_cast<float>(*z), Rml::Unit::NUMBER));
        Rml::Element* popup = d->AppendChild(d->CreateElement("div"));
        popup->SetClass(kPopupClass, true);
        // The bar takes the pointer over popups, not RmlUi: found here only to see which is on top.
        popup->SetAttribute("style", "left:0px;top:0px;width:200px;height:200px;pointer-events:auto;");
        d->Show();
        return popup;
    };
    const auto shown = [&](Rml::ElementDocument* d) {
        d->SetProperty(Rml::PropertyId::Left, Rml::Property(0.0f, Rml::Unit::PX));
        d->SetProperty(Rml::PropertyId::Top, Rml::Property(0.0f, Rml::Unit::PX));
        d->Show();
    };
    const auto top = [&] {
        rml.context->Update();
        Rml::Element* hit = rml.context->GetElementAtPoint(Rml::Vector2f(100.0f, 100.0f));
        return hit == nullptr ? nullptr : hit->GetOwnerDocument();
    };
    Rml::Element* under = popups(std::nullopt);
    Rml::ElementDocument* view = rml.context->LoadDocumentFromMemory(MenuViewDocument(menu), "view");
    view->GetElementById(kMenuViewBodyId)->SetAttribute("style", "width:200px;height:200px;");
    shown(view);
    CHECK(top() == view); // the settings over their popup
    Rml::Element* overSettings = popups(kPopupsOverSettingsZ);
    CHECK(top() == overSettings->GetOwnerDocument()); // a popup shown after it over both
    Rml::ElementDocument* window = rml.context->LoadDocumentFromMemory(MenuDocument(menu), "window");
    shown(window);
    CHECK(top() == window);
    CHECK(top() != under->GetOwnerDocument());
    Rml::Element* overMenu = popups(kPopupsOverMenuZ);
    CHECK(top() == overMenu->GetOwnerDocument()); // one come out after the window opened
}

TEST(a_widgets_alignment_puts_its_content_and_its_lines_against_that_side_whatever_its_section)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    Settings settings;
    for (const WidgetAlign align : {WidgetAlign::Left, WidgetAlign::Center, WidgetAlign::Right})
    {
        test::RmlSession rml;
        const BarLook look        = LookOf(PaletteOf(BarTheme::UmbraDark), settings);
        Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
        bar->Show();
        Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::Bags));
        widget->SetClass(kHiddenClass, false);
        // In the left section, which would put its lines against the left.
        widget->SetClass(SectionClass(BarSection::Left), true);
        widget->SetClass(AlignClass(align), true);
        widget->SetProperty(Rml::PropertyId::Width, Rml::Property(300.0f, Rml::Unit::PX));
        WidgetContent c;
        c.shown    = true;
        c.icons    = {Icon::Bag};
        c.title    = {Text("20 / 30")};
        c.subtitle = {Text("Inventory and more")};
        widget->SetInnerRML(WidgetRml(c, look, {}));
        rml.context->Update();
        Rml::ElementList icons, titles, details;
        widget->GetElementsByClassName(icons, "icon");
        widget->GetElementsByClassName(titles, "title");
        widget->GetElementsByClassName(details, "detail");
        const auto left  = [](Rml::Element* e) { return e->GetAbsoluteOffset(Rml::BoxArea::Border).x; };
        const auto right = [&](Rml::Element* e) { return left(e) + e->GetBox().GetSize(Rml::BoxArea::Border).x; };
        const float boxLeft = widget->GetAbsoluteOffset(Rml::BoxArea::Content).x;
        const float boxRight = boxLeft + widget->GetBox().GetSize(Rml::BoxArea::Content).x;
        Rml::Element* title = titles.front()->GetFirstChild();
        const float first = left(icons.front()), last = std::max(right(titles.front()), right(details.front()));
        if (align == WidgetAlign::Left)
            CHECK(std::fabs(first - boxLeft) <= 1.0f && std::fabs(left(title) - left(details.front())) <= 1.0f);
        if (align == WidgetAlign::Right)
            CHECK(std::fabs(last - boxRight) <= 1.0f && std::fabs(right(title) - right(details.front())) <= 1.0f);
        if (align == WidgetAlign::Center)
        {
            CHECK(std::fabs((first - boxLeft) - (boxRight - last)) <= 1.0f && first - boxLeft > 20.0f);
            CHECK(std::fabs((left(title) - left(details.front())) - (right(details.front()) - right(title))) <= 1.0f);
        }
        CHECK_EQ(rml.Said(), "");
    }
}

TEST(two_neighbouring_widgets_are_drawn_the_widget_spacing_apart_in_every_shape)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    constexpr int kSpacing = 2;
    Settings settings;
    settings.panelSeparators  = 0;
    settings.barWidgetSpacing = kSpacing;
    const BarPlacement left{std::vector<PanelWidget>{BarWidget::Zone, BarWidget::Weather}, {}, {}};
    std::array<float, kPanelSlots> natural{};
    natural[SlotOf(BarWidget::Zone)]    = 100.0f;
    natural[SlotOf(BarWidget::Weather)] = 80.0f;
    for (const BarTheme theme : {BarTheme::UmbraDark, BarTheme::Aurora, BarTheme::Vanilla, BarTheme::PhoenixGold})
    {
        test::RmlSession rml;
        const BarLook look        = LookOf(PaletteOf(theme), settings);
        Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
        bar->Show();
        const BarLayout layout = LayoutBar(1920.0f, look.metrics, look.palette.shape, natural, 0.0f, left,
            settings.panelSeparators, static_cast<float>(kSpacing), AloneGroups());
        // Placed as the bar places them.
        const auto show = [&](BarWidget kind) {
            Rml::Element* e = bar->GetElementById(WidgetId(kind));
            e->SetClass(kHiddenClass, false);
            const BarRect& rect = *RectOf(layout, kind);
            const float padding = static_cast<float>(settings.barWidgetPadding);
            const WidgetSide side = SideOf(look, kind, padding, false);
            e->SetProperty(Rml::PropertyId::Left, Rml::Property(rect.x, Rml::Unit::PX));
            e->SetProperty(Rml::PropertyId::Width, Rml::Property(rect.width - 2.0f * side.margin, Rml::Unit::PX));
            for (const Rml::PropertyId id : {Rml::PropertyId::MarginLeft, Rml::PropertyId::MarginRight})
                e->SetProperty(id, Rml::Property(side.margin, Rml::Unit::PX));
            for (const Rml::PropertyId id : {Rml::PropertyId::PaddingLeft, Rml::PropertyId::PaddingRight})
                e->SetProperty(id, Rml::Property(side.padding, Rml::Unit::PX));
            return e;
        };
        Rml::Element* zone = show(BarWidget::Zone);
        Rml::Element* weather = show(BarWidget::Weather);
        rml.context->Update();
        const float zoneRight = LeftOf(zone) + zone->GetBox().GetSize(Rml::BoxArea::Border).x;
        CHECK(std::fabs(LeftOf(weather) - zoneRight - static_cast<float>(kSpacing)) < 0.01f);
        CHECK_EQ(rml.Said(), "");
    }
}

TEST(an_adjusted_icon_draws_moved_or_sized_in_its_own_room_in_the_bar_and_the_pop_outs)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    Settings settings;
    const BarLook look = LookOf(PaletteOf(BarTheme::UmbraDark), settings);
    test::RmlSession rml;
    // Where a point drawn at x, y of an element's box sits, as a share of its size from its top left corner.
    const auto drawnAt = [](Rml::Element* e, float x, float y) {
        const Rml::Vector2f at = e->GetAbsoluteOffset(Rml::BoxArea::Border), size = e->GetBox().GetSize(Rml::BoxArea::Border);
        Rml::Vector2f point{at.x + x, at.y + y};
        CHECK(e->Project(point));
        return Rml::Vector2f{(point.x - at.x) / size.x, (point.y - at.y) / size.y};
    };
    const auto near = [](Rml::Vector2f a, float x, float y) { return std::fabs(a.x - x) < 0.01f && std::fabs(a.y - y) < 0.01f; };
    const auto size = [](Rml::Element* e) { return e->GetBox().GetSize(Rml::BoxArea::Border); };

    Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
    bar->Show();
    Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::Bags));
    widget->SetClass(kHiddenClass, false);
    WidgetContent c;
    c.shown   = true;
    c.picture = GamePicture::BastokCrest;
    c.icons   = {Icon::Bag, Icon::Clock, Icon::Star};
    c.title   = {Span{"", Tone::Normal, 0, GamePicture::WindurstCrest}, Text("20 / 30")};
    widget->SetInnerRML(WidgetRml(c, look, {}));
    // RmlUi works out a transform as it draws.
    rml.context->Update();
    rml.context->Render();
    Rml::ElementList icons, inline_;
    widget->GetElementsByClassName(icons, "icon");
    widget->GetElementsByClassName(inline_, "line-icon");
    CHECK_EQ(icons.size(), static_cast<size_t>(4));
    CHECK_EQ(inline_.size(), static_cast<size_t>(1));
    // The conquest crests at 85% of their room around its middle and a pixel down, wherever they are, as they read
    // larger than an icon.
    const Rml::Vector2f crest = size(icons[0]);
    CHECK(near(drawnAt(icons[0], crest.x * 0.075f, crest.y * 0.075f + 1.0f), 0.0f, 0.0f));
    const Rml::Vector2f windurst = size(inline_[0]);
    CHECK(near(drawnAt(inline_[0], windurst.x * 0.925f, windurst.y * 0.925f + 1.0f), 1.0f, 1.0f));
    // The bag a pixel down, at its size; the clock at 80% of its room; the star as it is.
    CHECK(near(drawnAt(icons[1], 0.0f, 1.0f), 0.0f, 0.0f));
    const Rml::Vector2f bag = size(icons[1]);
    CHECK(near(drawnAt(icons[1], bag.x, bag.y + 1.0f), 1.0f, 1.0f));
    const Rml::Vector2f clock = size(icons[2]);
    CHECK(near(drawnAt(icons[2], clock.x * 0.1f, clock.y * 0.1f), 0.0f, 0.0f));
    CHECK(near(drawnAt(icons[3], 0.0f, 0.0f), 0.0f, 0.0f));
    // Their rooms are an icon's, all alike.
    CHECK_EQ(crest.x, bag.x);
    CHECK_EQ(clock.x, bag.x);

    // A pop-out's the same way.
    PopupFaces faces;
    for (PopupFace& face : faces)
        face = PopupFace{look.detailFont, 14, 16, 0.0f};
    PopupLayout layout;
    layout.width  = 200.0f;
    layout.height = 100.0f;
    PopupPiece clockPiece{PopupPiece::Kind::Icon, 10.0f, 10.0f, 20.0f, 20.0f, {}};
    clockPiece.icon = Icon::Clock;
    PopupPiece crestPiece{PopupPiece::Kind::Picture, 40.0f, 10.0f, 20.0f, 20.0f, {}};
    crestPiece.picture = GamePicture::SanDoriaCrest;
    layout.pieces = {clockPiece, crestPiece};
    Rml::ElementDocument* popups = rml.context->LoadDocumentFromMemory(PopupDocument(look, faces), "popups");
    popups->Show();
    Rml::Element* popup = popups->AppendChild(popups->CreateElement("div"));
    popup->SetClass(kPopupClass, true);
    popup->SetInnerRML(PopupRml(layout, look, faces));
    rml.context->Update();
    rml.context->Render();
    Rml::ElementList pictures;
    popup->GetElementsByTagName(pictures, "img");
    CHECK_EQ(pictures.size(), static_cast<size_t>(2));
    CHECK(near(drawnAt(pictures[0], 2.0f, 2.0f), 0.0f, 0.0f));
    CHECK(near(drawnAt(pictures[1], 1.5f, 2.5f), 0.0f, 0.0f));
    CHECK_EQ(rml.Said(), "");
}

TEST(a_status_icon_fills_its_square_in_a_widgets_line_and_a_pop_out_and_with_its_background_sits_in_it_as_xiuis_does)
{
    Settings settings;
    const BarLook look = LookOf(PaletteOf(BarTheme::UmbraDark), settings);
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    test::RmlSession rml;
    const auto box = [](Rml::Element* e) {
        const Rml::Vector2f at = e->GetAbsoluteOffset(Rml::BoxArea::Border), size = e->GetBox().GetSize(Rml::BoxArea::Border);
        return std::array<float, 4>{at.x, at.y, size.x, size.y};
    };
    const auto near = [](float a, float b) { return std::fabs(a - b) < 0.05f; };
    // XIUI's party list: the background 1.1 icons wide and 1/0.75 of that tall, a buff's 0.33 of an icon above the
    // icon and a debuff's 0.11, 1/15 of an icon left of it. The icon fills its box as it does with no background, and
    // the background's picture is square, its 1.1 icons wide in the middle of it, so it reaches past the box.
    const float tall = 1.1f / 0.75f, left = 1.0f / 15.0f + (tall - 1.1f) / 2.0f;
    const auto checkIn = [&](Rml::Element* square, const Rml::ElementList& images, bool debuff) {
        const auto [x, y, w, h] = box(square);
        CHECK(images.size() == 2 && w > 0.0f && near(w, h));
        const auto back = box(images[0]), shown = box(images[1]);
        CHECK(near(shown[0], x) && near(shown[1], y) && near(shown[2], w) && near(shown[3], h));
        CHECK(near(back[2], w * tall) && near(back[3], h * tall));
        CHECK(near(back[0], x - w * left) && near(back[1], y - h * (debuff ? 0.11f : 0.33f)));
        CHECK(images[0]->GetAttribute<Rml::String>("src", "") == IconSource(debuff ? Icon::DebuffBack : Icon::BuffBack));
    };

    Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
    bar->Show();
    Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::StatusTimers));
    widget->SetClass(kHiddenClass, false);
    WidgetContent c;
    c.shown = true;
    Span protect = Text(""), dia = Text("");
    protect.status = StatusMark{40, 0, true};
    dia.status     = StatusMark{134, kVanillaStatusIcons, true};
    Span plain     = Text("");
    plain.status   = StatusMark{33, 1, false};
    c.title        = {protect, Text(" Protect"), Text("  "), dia, Text(" Dia")};
    c.subtitle     = {plain, Text(" Haste")};
    widget->SetInnerRML(WidgetRml(c, look, {}));
    rml.context->Update();
    Rml::ElementList marks, lines;
    widget->GetElementsByClassName(marks, "smark");
    widget->GetElementsByClassName(lines, "line-icon");
    CHECK(marks.size() == 3 && lines.size() == 3);
    for (size_t i = 0; i < 2; ++i)
    {
        Rml::ElementList images;
        marks[i]->GetElementsByTagName(images, "img");
        checkIn(marks[i], images, i == 1);
        CHECK(images[1]->GetAttribute<Rml::String>("src", "") == StatusSource(i == 0 ? protect.status.value() : dia.status.value()));
    }
    // A mark is a line's icon square, the name after it.
    Rml::ElementList hasteImages;
    marks[2]->GetElementsByTagName(hasteImages, "img");
    const auto haste = box(marks[2]), hasteIcon = box(hasteImages.at(0));
    CHECK(hasteImages.size() == 1 && near(hasteIcon[0], haste[0]) && near(hasteIcon[2], haste[2]) && near(hasteIcon[3], haste[3]));
    CHECK(near(haste[3], static_cast<float>(look.lines.detail)));

    PopupFaces faces;
    for (PopupFace& face : faces)
        face = PopupFace{look.detailFont, 14, 16, 0.0f};
    PopupLayout layout;
    layout.width  = 200.0f;
    layout.height = 100.0f;
    PopupPiece piece{PopupPiece::Kind::Status, 10.0f, 10.0f, 22.0f, 22.0f, {}};
    piece.status = StatusMark{134, 2, true};
    PopupPiece bare = piece;
    bare.x       = 60.0f;
    bare.status  = StatusMark{134, 2, false};
    layout.pieces = {piece, bare};
    Rml::ElementDocument* popups = rml.context->LoadDocumentFromMemory(PopupDocument(look, faces), "popups");
    popups->Show();
    Rml::Element* popup = popups->AppendChild(popups->CreateElement("div"));
    popup->SetClass(kPopupClass, true);
    popup->SetInnerRML(PopupRml(layout, look, faces));
    rml.context->Update();
    Rml::ElementList pictures;
    popup->GetElementsByTagName(pictures, "img");
    Rml::Element* square = popups->AppendChild(popups->CreateElement("div")); // where the piece's box is
    square->SetProperty("position", "absolute");
    const Rml::Vector2f origin = popup->GetAbsoluteOffset(Rml::BoxArea::Border);
    square->SetProperty("left", Rml::ToString(origin.x + 10.0f) + "px");
    square->SetProperty("top", Rml::ToString(origin.y + 10.0f) + "px");
    square->SetProperty("width", "22px");
    square->SetProperty("height", "22px");
    rml.context->Update();
    CHECK_EQ(pictures.size(), static_cast<size_t>(3));
    pictures.pop_back();
    checkIn(square, pictures, true);
    // The same status with no background is drawn the same size.
    Rml::ElementList bareImages;
    popup->GetElementsByTagName(bareImages, "img");
    CHECK(near(box(bareImages[1])[2], box(bareImages[2])[2]) && near(box(bareImages[1])[3], box(bareImages[2])[3]));
    CHECK_EQ(rml.Said(), "");
}

TEST(a_targets_distance_keeps_its_room_as_it_changes_so_the_text_around_it_stands_still)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    Settings settings;
    settings.Widget(BarWidget::Target).design = DesignFrom(BarWidget::Target, "name distance player |");
    const BarLook look = LookOf(PaletteOf(BarTheme::UmbraDark), settings);
    test::RmlSession rml;
    Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
    bar->Show();
    Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::Target));
    widget->SetClass(kHiddenClass, false);
    widget->SetProperty(Rml::PropertyId::Width, Rml::Property(400.0f, Rml::Unit::PX));
    ActorInfo player;
    player.kind = player.state.kind = EntityKind::Player;
    std::snprintf(player.name, sizeof(player.name), "%s", "Tanyrus");
    player.jobs = PlayerJobs{3, 75, 4, 37};
    BarState state;
    state.target = &player;
    // Where the name, the distance's room and the jobs after it start, with this distance.
    const auto drawn = [&](float yalms) {
        state.targetDistance = yalms;
        WidgetContent c      = BarWidgetContent(BarWidget::Target, state, settings);
        Steady(c, look);
        widget->SetInnerRML(WidgetRml(c, look, {}));
        rml.context->Update();
        Rml::ElementList lines;
        widget->GetElementsByClassName(lines, "t");
        Rml::Element* line = lines.front();
        CHECK(line->GetNumChildren() == 3);
        if (line->GetNumChildren() != 3) return std::array<float, 3>{};
        return std::array{LeftOf(line->GetChild(0)), LeftOf(line->GetChild(1)), LeftOf(line->GetChild(2))};
    };
    const std::array<float, 3> first = drawn(1.1f);
    for (const float yalms : {8.8f, 11.1f, 44.4f, 99.9f, 0.0f, 17.5f})
        CHECK(drawn(yalms) == first);
    CHECK(first[2] > first[1] && first[1] > first[0]);
    CHECK_EQ(rml.Said(), "");
}

TEST(a_clocks_times_keep_their_room_and_each_figure_its_place_as_the_seconds_tick)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    for (const bool twentyFour : {false, true})
        for (const bool seconds : {true, false})
        {
            Settings settings;
            WidgetOptions& o = settings.Widget(BarWidget::EarthTime);
            o.design         = DesignFrom(BarWidget::EarthTime, "time vana | -server -date");
            o.clock24        = twentyFour;
            o.clockSeconds   = seconds;
            const BarLook look = LookOf(PaletteOf(BarTheme::UmbraDark), settings);
            test::RmlSession rml;
            Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
            bar->Show();
            Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::EarthTime));
            widget->SetClass(kHiddenClass, false);
            widget->SetProperty(Rml::PropertyId::Width, Rml::Property(400.0f, Rml::Unit::PX));
            BarState state;
            // Where the minutes' and seconds' figures start, then the Vana'diel time after the clock.
            const auto drawn = [&](int hour, int minute, int second) {
                state.earthHour = hour, state.earthMinute = minute, state.earthSecond = second;
                WidgetContent c = BarWidgetContent(BarWidget::EarthTime, state, settings);
                Steady(c, look);
                widget->SetInnerRML(WidgetRml(c, look, {}));
                rml.context->Update();
                Rml::ElementList rooms, figures;
                widget->GetElementsByClassName(rooms, "steady");
                CHECK(rooms.size() == (twentyFour ? 2u : 3u));
                if (rooms.empty()) return std::vector<float>{};
                rooms.front()->GetElementsByClassName(figures, "fig");
                const size_t kept = seconds ? 4 : 2;
                CHECK(figures.size() >= kept);
                if (figures.size() < kept) return std::vector<float>{};
                std::vector<float> at;
                for (size_t i = figures.size() - kept; i < figures.size(); ++i)
                    at.push_back(LeftOf(figures[i]));
                for (Rml::Element* room : rooms)
                    at.push_back(LeftOf(room));
                return at;
            };
            const std::vector<float> first = drawn(9, 5, 7);
            for (const auto& [hour, minute, second] :
                std::array{std::tuple{10, 11, 11}, std::tuple{12, 59, 58}, std::tuple{21, 40, 1}, std::tuple{0, 0, 0},
                    std::tuple{11, 11, 11}, std::tuple{23, 48, 44}})
                CHECK(drawn(hour, minute, second) == first);
            CHECK_EQ(rml.Said(), "");
        }
}

TEST(a_ticking_span_keeps_its_gap_to_an_icon_after_it)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    Settings settings;
    const BarLook look = LookOf(PaletteOf(BarTheme::UmbraDark), settings);
    test::RmlSession rml;
    Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
    bar->Show();
    Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::Target));
    widget->SetClass(kHiddenClass, false);
    widget->SetProperty(Rml::PropertyId::Width, Rml::Property(400.0f, Rml::Unit::PX));
    // The icon's left, and the room's right, after a name and a distance.
    const auto drawn = [&](std::string far) {
        Span distance = Text(std::move(far));
        distance.room = {"(00.0)"};
        Span sight;
        sight.icon = Icon::Sight;
        WidgetContent c;
        c.shown = true;
        AddPart(c.title, {Text("Goblin")});
        AddPart(c.title, {distance});
        AddPart(c.title, {sight});
        Steady(c, look);
        widget->SetInnerRML(WidgetRml(c, look, {}));
        rml.context->Update();
        Rml::ElementList rooms, icons;
        widget->GetElementsByClassName(rooms, "steady");
        widget->GetElementsByClassName(icons, "line-icon");
        CHECK(rooms.size() == 1 && icons.size() == 1);
        if (rooms.empty() || icons.empty()) return std::pair{0.0f, 0.0f};
        return std::pair{icons.front()->GetAbsoluteOffset(Rml::BoxArea::Margin).x,
            LeftOf(rooms.front()) + rooms.front()->GetBox().GetSize(Rml::BoxArea::Border).x};
    };
    const auto [icon, room] = drawn("(5.2)");
    CHECK(drawn("(42.0)").first == icon);
    // Two spaces of the font apart, as the gap between any two parts.
    CHECK(std::fabs(icon - room - DrawnWidth("  ", look.TitleFace(WidgetContent{}))) <= 1.0f);
    CHECK_EQ(rml.Said(), "");
}

TEST(a_lines_runs_of_text_each_line_up_on_their_own_side_of_the_widgets_text_without_overlapping)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    Settings settings;
    test::RmlSession rml;
    const BarLook look        = LookOf(PaletteOf(BarTheme::UmbraDark), settings);
    Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
    bar->Show();
    Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::Job));
    widget->SetClass(kHiddenClass, false);
    widget->SetClass(SectionClass(BarSection::Center), true); // which would center a line
    const auto run = [](std::string text, Align align) {
        Span span  = Text(std::move(text));
        span.align = align;
        return span;
    };
    const auto left  = [](Rml::Element* e) { return e->GetAbsoluteOffset(Rml::BoxArea::Border).x; };
    const auto right = [&](Rml::Element* e) { return left(e) + e->GetBox().GetSize(Rml::BoxArea::Border).x; };
    // Each run's text as drawn, by its words.
    const auto drawn = [&](const char* words) {
        Rml::ElementList texts;
        widget->GetElementsByClassName(texts, "t");
        for (Rml::Element* t : texts)
            if (t->GetInnerRML().find(words) != std::string::npos && t->GetComputedValues().visibility() == Rml::Style::Visibility::Visible)
                return t;
        return static_cast<Rml::Element*>(nullptr);
    };
    // A short top line under a long one: its runs at the text's left and right edges and its middle.
    WidgetContent c;
    c.shown    = true;
    c.title    = {run("WAR 30", Align::Left), run("  61%", Align::Right)};
    c.subtitle = {Text("12,345 / 20,000 EXP and a long way to go")};
    widget->SetInnerRML(WidgetRml(c, look, {}));
    rml.context->Update();
    Rml::ElementList texts;
    widget->GetElementsByClassName(texts, "text");
    Rml::Element* text = texts.front();
    Rml::Element *jobs = drawn("WAR 30"), *percent = drawn("61%"), *detail = drawn("long way");
    CHECK(jobs != nullptr && percent != nullptr && detail != nullptr);
    CHECK(std::fabs(left(jobs) - left(text)) <= 0.5f);
    CHECK(std::fabs(right(percent) - right(text)) <= 0.5f);
    CHECK(std::fabs(right(detail) - right(text)) <= 0.5f && std::fabs(left(detail) - left(text)) <= 0.5f);
    // Its gap is left off at the right: the percent ends at the edge as wide as its own text.
    CHECK(std::fabs(percent->GetBox().GetSize(Rml::BoxArea::Border).x - DrawnWidth("61%", look.TitleFace(c))) <= 1.0f);
    // Drawn on the top line's own row.
    CHECK(std::fabs(jobs->GetAbsoluteOffset(Rml::BoxArea::Border).y - percent->GetAbsoluteOffset(Rml::BoxArea::Border).y) <= 0.01f);
    // A centered run in the middle of the text.
    c.title = {run("WAR 30", Align::Center)};
    widget->SetInnerRML(WidgetRml(c, look, {}));
    rml.context->Update();
    jobs = drawn("WAR 30");
    CHECK(std::fabs((left(jobs) - left(text)) - (right(text) - right(jobs))) <= 1.0f && left(jobs) - left(text) > 20.0f);
    // The longer line: the text is as wide as all its runs, so they never overlap, two spaces apart.
    c.title    = {run("12,345 / 20,000 EXP and a long way", Align::Left), run("  WAR 30 / NIN 15", Align::Right)};
    c.subtitle = {Text("61%")};
    widget->SetInnerRML(WidgetRml(c, look, {}));
    rml.context->Update();
    Rml::Element *exp = drawn("long way"), *both = drawn("NIN 15");
    CHECK(std::fabs(left(exp) - left(text)) <= 0.5f && std::fabs(right(both) - right(text)) <= 0.5f);
    CHECK(std::fabs(left(both) - right(exp) - DrawnWidth("  ", look.TitleFace(c))) <= 1.0f);
    CHECK_EQ(rml.Said(), "");
}

TEST(the_job_widgets_text_alignments_line_up_against_the_whole_width_of_both_its_lines)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    constexpr uint8_t kWar = 1, kNin = 13;
    JobSample sample;
    sample.mainJob   = kWar;
    sample.mainLevel = 30;
    sample.subJob    = kNin;
    sample.subLevel  = 15;
    sample.exp       = 12345;
    sample.expNeeded = 20000;
    const JobReport job{sample, std::nullopt};
    Settings settings;
    test::RmlSession rml;
    const BarLook look        = LookOf(PaletteOf(BarTheme::UmbraDark), settings);
    Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
    bar->Show();
    Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::Job));
    widget->SetClass(kHiddenClass, false);
    const auto left  = [](Rml::Element* e) { return e->GetAbsoluteOffset(Rml::BoxArea::Border).x; };
    const auto right = [&](Rml::Element* e) { return left(e) + e->GetBox().GetSize(Rml::BoxArea::Border).x; };
    const auto drawn = [&](const char* words) {
        Rml::ElementList texts;
        widget->GetElementsByClassName(texts, "t");
        for (Rml::Element* t : texts)
            if (t->GetInnerRML().find(words) != std::string::npos && t->GetComputedValues().visibility() == Rml::Style::Visibility::Visible)
                return t;
        throw test::Failure{std::string("nothing drawn says ") + words};
    };
    // As the panel composes it, its lines of figures.
    const auto composed = [&](std::string_view design, const WidgetOptions& o) {
        WidgetContent base;
        base.shown                     = true;
        std::vector<PartContent> parts = JobParts(job, o, base);
        CHECK(base.figures);
        return ComposeContent(DesignFrom(BarWidget::Job, design), std::move(parts), base);
    };
    // The card's picture: a 250 px bar over the jobs and the EXP earned, far wider than they are.
    for (const WidgetAlign widgetAlign : {WidgetAlign::Left, WidgetAlign::Center, WidgetAlign::Right})
        for (const Align align : {Align::Left, Align::Center, Align::Right})
        {
            widget->SetClass(AlignClass(WidgetAlign::Left), widgetAlign == WidgetAlign::Left);
            widget->SetClass(AlignClass(WidgetAlign::Center), widgetAlign == WidgetAlign::Center);
            widget->SetClass(AlignClass(WidgetAlign::Right), widgetAlign == WidgetAlign::Right);
            WidgetOptions o;
            o.expBarWidth  = 250;
            o.earnedAlign  = align;
            const WidgetContent c = composed("bar | earned", o);
            widget->SetInnerRML(WidgetRml(c, look, {}));
            rml.context->Update();
            Rml::ElementList tracks;
            widget->GetElementsByClassName(tracks, "track");
            CHECK(tracks.size() == 1);
            Rml::Element* earned = drawn("12,345 / 20,000 EXP");
            const float barLeft = left(tracks.front()), barRight = right(tracks.front());
            if (align == Align::Left) CHECK(std::fabs(left(earned) - barLeft) <= 0.5f);
            if (align == Align::Right) CHECK(std::fabs(right(earned) - barRight) <= 0.5f);
            if (align == Align::Center)
                CHECK(std::fabs((left(earned) - barLeft) - (barRight - right(earned))) <= 1.0f && left(earned) - barLeft > 20.0f);
        }
    // Jobs at the left and the EXP earned at the right, as in the picture: each at its own end of the bar.
    WidgetOptions o;
    o.expBarWidth  = 250;
    o.jobInfoAlign = Align::Left;
    o.earnedAlign  = Align::Right;
    WidgetContent c = composed("bar | jobs earned", o);
    widget->SetInnerRML(WidgetRml(c, look, {}));
    rml.context->Update();
    Rml::ElementList tracks;
    widget->GetElementsByClassName(tracks, "track");
    CHECK(std::fabs(left(drawn("WAR 30")) - left(tracks.front())) <= 0.5f);
    CHECK(std::fabs(right(drawn("12,345 / 20,000 EXP")) - right(tracks.front())) <= 0.5f);
    // The texts on top over a short bar: the bar line follows the widget's Alignment, the texts the whole width.
    o.expBarWidth  = 40;
    o.percentAlign = Align::Right;
    c = composed("jobs earned percent | bar", o);
    widget->SetInnerRML(WidgetRml(c, look, {}));
    rml.context->Update();
    Rml::ElementList texts;
    widget->GetElementsByClassName(texts, "text");
    CHECK(std::fabs(left(drawn("WAR 30")) - left(texts.front())) <= 0.5f);
    CHECK(std::fabs(right(drawn("61%")) - right(texts.front())) <= 0.5f);
    CHECK_EQ(rml.Said(), "");
}

TEST(an_exp_bar_draws_in_its_themes_colors_or_its_own)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, {}, {}};
    Settings settings;
    for (const BarTheme theme : {BarTheme::UmbraDark, BarTheme::Aurora})
    {
        test::RmlSession rml;
        const BarPalette& palette = PaletteOf(theme);
        const BarLook look        = LookOf(palette, settings);
        Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
        bar->Show();
        Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::Job));
        widget->SetClass(kHiddenClass, false);
        const auto drawn = [&](std::optional<uint32_t> back, std::optional<uint32_t> fill) {
            WidgetContent c;
            c.shown   = true;
            Span exp  = Text("");
            exp.bar   = InlineBar{0.5f, 100, 6, "", Align::Center, back, fill};
            c.title   = {exp};
            widget->SetInnerRML(WidgetRml(c, look, {}));
            rml.context->Update();
            Rml::ElementList tracks, fills;
            widget->GetElementsByClassName(tracks, "track");
            widget->GetElementsByClassName(fills, "fill");
            const Rml::Colourb track = tracks.front()->GetComputedValues().background_color();
            const auto& own = fills.front()->GetComputedValues();
            return std::tuple{static_cast<uint32_t>(track.alpha) << 24 | track.red << 16 | track.green << 8 | track.blue,
                own.background_color(), fills.front()->GetProperty(Rml::PropertyId::Decorator)->ToString()};
        };
        // The theme's: the divider, a glass of white in Glass, and the accent's gradient.
        const auto [themeBack, themeFill, themeDecorator] = drawn(std::nullopt, std::nullopt);
        CHECK_EQ(themeBack, theme == BarTheme::Aurora ? 0x1FFFFFFFu : palette.divider);
        CHECK(themeDecorator.find("horizontal-gradient") != std::string::npos);
        // Its own: the track in its color and the fill in one color of its own.
        const auto [ownBack, ownFill, ownDecorator] = drawn(0x80102030u, 0xFF20C040u);
        CHECK_EQ(ownBack, 0x80102030u);
        CHECK(ownFill == Rml::Colourb(0x20, 0xC0, 0x40, 0xFF) && ownDecorator.find("gradient") == std::string::npos);
        CHECK_EQ(rml.Said(), "");
    }
}

TEST(a_widgets_icon_stands_on_the_middle_of_its_lone_lines_figures_as_a_clock_ticks)
{
    // The user's profile when they saw the clock's icon off its time: a 45 px panel in the game's window art, icons at
    // 65%, a 24-hour clock of one line.
    Settings settings;
    settings.barFontRatio = 1.2f;
    settings.barIconScale = 65;
    WidgetOptions& o      = settings.Widget(BarWidget::EarthTime);
    o.design              = DesignFrom(BarWidget::EarthTime, "time | -server -vana -date");
    o.clock24             = true;
    o.clockSeconds        = false;
    const BarPalette& p   = PaletteOf(BarTheme::Vanilla);
    const BarMetrics m    = MetricsFor(kBarHeight, p.shape, settings.barFontRatio, kDefaultPopupScale, settings.barIconScale);
    const char* font      = BarTitleFont(settings, p.shape);
    test::RmlSession rml;
    const test::GlyphInk glyphs(font);
    const TextRow lines = FitTextRow(m, [&](bool title, int pixels) {
        return glyphs.LineInk(title ? kTitleGlyphs : kDetailGlyphs, pixels);
    });
    const BarLook look{p, settings, m, PillRow(m), lines, font, false, BarTextFont(settings), std::nullopt};
    Rml::FontEngineInterface* engine = Rml::GetFontEngineInterface();
    const Rml::FontMetrics& metrics  = engine->GetFontMetrics(engine->GetFontFaceHandle(Rml::StringUtilities::ToLower(font),
        Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, lines.alone));
    // The lone line as the plugin places it: RmlUi's baseline on the one the ink was fitted with.
    const float rmlBaseline = (std::round(metrics.ascent + metrics.descent) - metrics.ascent - metrics.descent) / 2.0f + metrics.ascent;
    BarTextPlace place;
    place.alone = BarLinePlace{WholePixels(metrics.ascent + metrics.descent),
        std::round(lines.aloneY) + glyphs.LineInk(kTitleGlyphs, lines.alone).baseline - rmlBaseline, 0.0f};
    Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
    bar->Show();
    Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::EarthTime));
    widget->SetClass(kHiddenClass, false);
    const auto top = [](Rml::Element* e) { return e->GetAbsoluteOffset(Rml::BoxArea::Border).y; };
    const float figures = glyphs.Above("0123456789", lines.alone); // the digits' ink over their baseline
    BarState state;
    for (const auto& [hour, minute] : {std::pair{21, 7}, std::pair{0, 0}, std::pair{13, 48}})
    {
        state.earthHour = hour, state.earthMinute = minute;
        WidgetContent c = BarWidgetContent(BarWidget::EarthTime, state, settings);
        Steady(c, look);
        widget->SetInnerRML(WidgetRml(c, look, {}));
        rml.context->Update();
        Rml::ElementList icons, digits;
        widget->GetElementsByClassName(icons, "icon");
        widget->GetElementsByClassName(digits, "fig");
        CHECK(icons.size() == 1 && digits.size() == 4);
        if (icons.empty() || digits.empty()) return;
        const float icon = top(icons.front()) + icons.front()->GetBox().GetSize(Rml::BoxArea::Border).y / 2.0f;
        for (Rml::Element* digit : digits)
        {
            const float baseline = top(digit) + (digit->GetLineHeight() - metrics.ascent - metrics.descent) / 2.0f + metrics.ascent;
            CHECK(std::fabs(baseline - figures / 2.0f - icon) <= 0.5f);
        }
    }
    CHECK_EQ(rml.Said(), "");
}

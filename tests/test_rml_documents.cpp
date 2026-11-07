#include "bar_document.h"
#include "menu_document.h"
#include "window_skin.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <array>
#include <cmath>
#include <optional>
#include <string>
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

TEST(the_settings_window_draws_over_the_lower_popups_and_its_settings_view_and_upper_popups_over_it)
{
    test::RmlSession rml;
    Settings settings;
    const BarLook look = LookOf(PaletteOf(BarTheme::UmbraDark), settings);
    const PopupFaces faces = {PopupFace{"Barlow Condensed", 18, 22}, PopupFace{"Barlow Condensed", 14, 17},
        PopupFace{"Barlow Condensed", 16, 19}, PopupFace{"Barlow Condensed", 16, 19}};
    const MenuLook menu{MenuTheme::Phoenix, kBarlowCondensed, kMarcellusSC, 16, 0, false, kPhoenixMenuColors, std::nullopt};
    Rml::ElementDocument* popups = rml.context->LoadDocumentFromMemory(PopupDocument(look, faces), "popups");
    Rml::ElementDocument* window = rml.context->LoadDocumentFromMemory(MenuDocument(menu), "window");
    Rml::ElementDocument* view   = rml.context->LoadDocumentFromMemory(MenuViewDocument(menu), "view");
    CHECK(popups != nullptr && window != nullptr && view != nullptr);
    for (Rml::ElementDocument* d : {popups, window, view})
        d->Show();
    rml.context->Update();
    const auto z = [](Rml::ElementDocument* d) { return d->GetComputedValues().z_index().value; };
    CHECK(z(popups) < z(window) && z(window) < z(view));
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
    // The crests at 85% of their room around its middle, wherever they are, as they read larger than an icon.
    const Rml::Vector2f crest = size(icons[0]);
    CHECK(near(drawnAt(icons[0], crest.x * 0.075f, crest.y * 0.075f), 0.0f, 0.0f));
    const Rml::Vector2f windurst = size(inline_[0]);
    CHECK(near(drawnAt(inline_[0], windurst.x * 0.925f, windurst.y * 0.925f), 1.0f, 1.0f));
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
    CHECK(near(drawnAt(pictures[1], 1.5f, 1.5f), 0.0f, 0.0f));
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
    // icon and a debuff's 0.11, 1/15 of an icon left of it. Fit to the box's height, the icon is 0.75/1.1 of it.
    const float icon = 0.75f / 1.1f, left = (1.0f - 0.75f) / 2.0f + icon / 15.0f;
    const auto checkIn = [&](Rml::Element* square, const Rml::ElementList& images, bool debuff) {
        const auto [x, y, w, h] = box(square);
        CHECK(images.size() == 2 && w > 0.0f && near(w, h));
        const auto back = box(images[0]), shown = box(images[1]);
        CHECK(near(back[0], x) && near(back[1], y) && near(back[2], w) && near(back[3], h));
        CHECK(near(shown[2], w * icon) && near(shown[3], h * icon));
        CHECK(near(shown[0], x + w * left) && near(shown[1], y + h * icon * (debuff ? 0.11f : 0.33f)));
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
    layout.pieces = {piece};
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
    checkIn(square, pictures, true);
    CHECK_EQ(rml.Said(), "");
}

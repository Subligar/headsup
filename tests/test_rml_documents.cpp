#include "bar_document.h"
#include "window_skin.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

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
        const BarMetrics m  = MetricsFor(kBarHeight, p.shape, kAutoBarTextSize, kAutoBarTextSize, kAutoPopupTextSize);
        return BarLook{p, settings, m, PillRow(m), TextRow{13, 11, 10.0f, 24.0f, 13, 12.0f},
            BarTitleFont(settings, p.shape), p.shape == BarShape::Ornate, BarTextFont(settings), std::nullopt};
    }
}

TEST(rmlui_reads_every_themes_bar_and_popups_without_a_complaint)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, BarLinePlace{14, 15.0f, 3.0f}, {}, {}};
    const PopupFaces faces = {PopupFace{"Barlow Condensed", 24, 29, 0}, PopupFace{"Barlow Condensed", 18, 22, 0},
        PopupFace{"Barlow Condensed", 15, 18, 2}, PopupFace{"Barlow Condensed", 15, 18, 0}};
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
        Rml::Element* popup = popups->GetElementById(kPopupId);
        CHECK(popup != nullptr);
        popup->SetClass(kOpenClass, true);
        rml.context->Update();
        rml.context->Render();
        CHECK_EQ(rml.Said(), "");
    }
}

TEST(the_target_widget_goes_back_to_breathing_after_its_popup_without_a_complaint)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, BarLinePlace{14, 15.0f, 3.0f}, {}, {}};
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
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, BarLinePlace{14, 15.0f, 3.0f}, {}, {}};
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
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, BarLinePlace{14, 15.0f, 3.0f}, {}, {}};
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
        widget->SetInnerRML(WidgetRml(c, look, {}, {}));
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
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, BarLinePlace{14, 15.0f, 3.0f}, {}, {}};
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

TEST(an_exp_bar_in_a_widgets_line_draws_as_long_and_thick_as_the_old_bar_beside_the_text_filled_to_its_share)
{
    const BarTextPlace place{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
        BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f}, BarLinePlace{14, 15.0f, 3.0f}, {}, {}};
    Settings settings;
    const BarLook look = LookOf(settings.customPalette, settings);
    test::RmlSession rml;
    Rml::ElementDocument* bar = rml.context->LoadDocumentFromMemory(BarDocument(look, place), "bar");
    bar->Show();
    Rml::Element* widget = bar->GetElementById(WidgetId(BarWidget::Job));
    WidgetContent c;
    c.shown     = true;
    Span exp    = Text("");
    exp.bar     = 0.25f;
    c.title     = {Text("WAR 30")};
    c.subtitle  = {Text("1,200 EXP to next"), Text("  "), exp};
    widget->SetClass(kHiddenClass, false);
    widget->SetInnerRML(WidgetRml(c, look, {}, {}));
    rml.context->Update();
    Rml::ElementList bars, fills;
    widget->GetElementsByClassName(bars, "ibar");
    widget->GetElementsByClassName(fills, "fill");
    CHECK(bars.size() == 1 && fills.size() == 1);
    // At a 45 px bar: two bar heights long (90) and 0.14 of one thick (6), as the bar beside the text was.
    const Rml::Vector2f track = bars.front()->GetBox().GetSize(Rml::BoxArea::Border);
    CHECK_EQ(track.x, 90.0f);
    CHECK_EQ(track.y, 6.0f);
    const Rml::Vector2f fill = fills.front()->GetBox().GetSize(Rml::BoxArea::Border);
    CHECK(std::abs(fill.x - track.x / 4.0f) < 1.0f && fill.y == track.y);
    CHECK_EQ(rml.Said(), "");
}

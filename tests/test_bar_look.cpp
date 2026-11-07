#include "bar_look.h"
#include "bar_placement.h"
#include "test.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

using namespace headsup;

namespace
{
    bool Near(float a, float b) { return std::fabs(a - b) < 0.01f; }

    BarLook LookOf(const BarPalette& p, const Settings& settings, TextRow lines = {}, int height = 45,
        float popupScale = kDefaultPopupScale)
    {
        const BarMetrics m = MetricsFor(height, p.shape, kDefaultFontRatio, popupScale);
        return BarLook{p, settings, m, PillRow(m), lines, BarTitleFont(settings, p.shape), p.shape == BarShape::Ornate,
            BarTextFont(settings), std::nullopt};
    }

    BarLook LookOf(BarTheme theme, const Settings& settings, TextRow lines = {}, int height = 45,
        float popupScale = kDefaultPopupScale)
    {
        return LookOf(PaletteOf(theme), settings, lines, height, popupScale);
    }

    bool Family(const TextFace& face, const char* family) { return std::string(face.family) == family; }

    // A widget with both its lines: its title is in the two-line size.
    WidgetContent TwoLines()
    {
        WidgetContent content;
        content.title    = {Span{"Bags"}};
        content.subtitle = {Span{"27/30"}};
        return content;
    }
}

TEST(the_custom_theme_takes_the_settings_palette_and_starts_as_phoenix_gold)
{
    Settings s;
    CHECK(PaletteFor(s, nullptr) == PaletteOf(BarTheme::PhoenixGold) && s.customPalette == PaletteOf(BarTheme::PhoenixGold));
    s.customPalette.accent = 0xFF00FF00;
    CHECK(PaletteFor(s, nullptr) == PaletteOf(BarTheme::PhoenixGold)); // kept for when Custom is picked
    s.barTheme = BarTheme::Custom;
    CHECK(PaletteFor(s, nullptr).accent == 0xFF00FF00u && PaletteFor(s, nullptr).stripTop == PaletteOf(BarTheme::PhoenixGold).stripTop);
    s.barTheme = BarTheme::UmbraDark;
    CHECK(PaletteFor(s, nullptr) == PaletteOf(BarTheme::UmbraDark));
}

TEST(each_tone_takes_its_color_on_the_bar_and_in_a_popup)
{
    // Spec, Palettes: the bar's title and subtitle text, the popups' text, headings and tag text by theme; values take
    // the badge colors from settings, and neutral ones #8d8d8d.
    Settings settings;
    settings.weakBadgeColor   = ColorFromRgb(0x112233);
    settings.resistBadgeColor = ColorFromRgb(0x445566);
    settings.immuneBadgeColor = ColorFromRgb(0x778899);
    for (int theme = 0; theme < kFixedThemeCount; ++theme)
    {
        const BarLook look  = LookOf(static_cast<BarTheme>(theme), settings);
        const BarPalette& p = look.palette;
        auto bar   = [&](Tone tone) { return look.BarColor(Span{"x", tone, 0xFF010203}); };
        auto popup = [&](Tone tone) { return look.PopupColor(Span{"x", tone, 0xFF010203}); };
        CHECK(bar(Tone::Normal) == p.title && popup(Tone::Normal) == p.text);
        CHECK(bar(Tone::Dim) == p.subtitle && popup(Tone::Dim) == p.subtitle);
        CHECK(popup(Tone::Heading) == p.heading && popup(Tone::Tag) == p.tagText);
        CHECK_EQ(popup(Tone::Neutral), 0xFF8D8D8Du);
        CHECK(popup(Tone::Weak) == 0xFF112233u && popup(Tone::Resist) == 0xFF445566u &&
              popup(Tone::Immune) == 0xFF778899u);
        CHECK(bar(Tone::Own) == 0xFF010203u && popup(Tone::Own) == 0xFF010203u);
    }
}

TEST(ornate_popups_head_their_sections_in_the_title_font_and_pills_popups_in_the_detail_font)
{
    // Spec, Shapes: ornate titles and headings in Marcellus SC, the rest in Barlow Condensed; pills all in Barlow.
    Settings settings;
    const BarLook ornate = LookOf(BarTheme::PhoenixGold, settings, TextRow{14, 11, true});
    const BarLook pills  = LookOf(BarTheme::UmbraDark, settings, TextRow{13, 11, true});
    CHECK(Family(ornate.FaceOf(PopupFont::Title), "Marcellus SC") &&
          Family(ornate.FaceOf(PopupFont::Heading), "Marcellus SC") &&
          Family(ornate.FaceOf(PopupFont::Text), "Barlow Condensed"));
    for (const PopupFont font : {PopupFont::Title, PopupFont::Text, PopupFont::Heading})
        CHECK(Family(pills.FaceOf(font), "Barlow Condensed"));
    // A pop-out's text 14 px, its card headings 16 px and its title 18 px at a Pop-out Scale of 1.
    CHECK(Near(ornate.FaceOf(PopupFont::Text).size, 14.0f) && Near(ornate.FaceOf(PopupFont::Heading).size, 16.0f) &&
          Near(ornate.FaceOf(PopupFont::Title).size, 18.0f));
    // A widget's title in the shape's title font, its detail line in Barlow, at the sizes their row fitted.
    CHECK(Family(ornate.TitleFace(TwoLines()), "Marcellus SC") && ornate.TitleFace(TwoLines()).size == 14.0f);
    CHECK(Family(pills.TitleFace(TwoLines()), "Barlow Condensed") && pills.TitleFace(TwoLines()).size == 13.0f);
    CHECK(Family(ornate.DetailFace(), "Barlow Condensed") && ornate.DetailFace().size == 11.0f);
}

TEST(the_pinned_mark_takes_the_headings_family_and_size)
{
    Settings settings;
    const BarLook pills = LookOf(BarTheme::UmbraDark, settings, {}, 45, 1.5f), ornate = LookOf(BarTheme::PhoenixGold, settings);
    for (const BarLook* look : {&pills, &ornate})
        CHECK(look->FaceOf(PopupFont::Pin).family == look->FaceOf(PopupFont::Heading).family &&
              look->FaceOf(PopupFont::Pin).size == look->FaceOf(PopupFont::Heading).size);
}

TEST(a_widgets_text_starts_after_its_icons_and_coin_each_with_a_gap)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent weather;
    weather.icons = {Icon::Lightning, Icon::Lightning};
    CHECK(Near(look.Leading(weather), 2.0f * (0.65f * 34.0f + 7.65f))); // icons 65% of the pill's 34 px inside, gaps 0.17 H
    CHECK(Near(look.Leading(WidgetContent{}), 0.0f));
    // Gil's coin is drawn 17 px wide at 45 px (0.375 H, whole pixels), and its amount starts a gap after that.
    WidgetContent gil;
    gil.coin = true;
    CHECK_EQ(CoinPixels(look.metrics), 17);
    CHECK(Near(look.Leading(gil), 17.0f + 7.65f));
    // A zone's crest has an icon's room (its adjustment draws it smaller inside it).
    WidgetContent zone;
    zone.picture = GamePicture::BastokCrest;
    CHECK(Near(look.Leading(zone), 0.65f * 34.0f + 7.65f));
}

TEST(the_ornate_target_is_framed_and_a_lit_widget_tinted)
{
    Settings settings;
    const BarLook look  = LookOf(BarTheme::PhoenixGold, settings);
    const BarPalette& p = look.palette;
    CHECK(!look.BoxOf(BarWidget::Weather, false).has_value());
    const std::optional<BoxStyle> tint = look.BoxOf(BarWidget::Weather, true);
    CHECK(tint.has_value() && tint->top == p.highlight && tint->bottom == p.highlight && tint->border == 0u);
    const std::optional<BoxStyle> frame = look.BoxOf(BarWidget::Target, false);
    CHECK(frame.has_value() && frame->top == p.highlight && frame->border == p.accent);
    // The frame always has the hover tint, so hovering the target doubles it: 0x1A, 10%, to 0x34.
    const std::optional<BoxStyle> hovered = look.BoxOf(BarWidget::Target, true);
    CHECK(hovered.has_value() && hovered->border == p.accent);
    CHECK(hovered->top == 0x34C9A24Au && hovered->bottom == 0x34C9A24Au);
}

TEST(pills_frame_every_widget_but_zone_and_earth_time_until_lit)
{
    Settings settings;
    const BarLook look  = LookOf(BarTheme::UmbraDark, settings);
    const BarPalette& p = look.palette;
    for (const BarWidget flat : {BarWidget::Zone, BarWidget::EarthTime})
    {
        CHECK(!look.BoxOf(flat, false).has_value());
        const std::optional<BoxStyle> lit = look.BoxOf(flat, true);
        CHECK(lit.has_value() && lit->top == p.highlight && lit->border == p.accent);
    }
    const std::optional<BoxStyle> bags = look.BoxOf(BarWidget::Bags, false);
    CHECK(bags.has_value() && bags->top == p.pill && bags->border == p.pillBorder);
    const std::optional<BoxStyle> target = look.BoxOf(BarWidget::Target, false);
    CHECK(target.has_value() && target->top == p.pill && target->border == p.accent);
    CHECK(Near(bags->radius, look.metrics.pillRadius));
}

TEST(a_widget_of_figures_draws_its_title_in_the_detail_font_in_every_theme)
{
    // Marcellus SC's 0 reads as an O and its small caps turn "2d 8h" into capitals: figures take Barlow Condensed at
    // the title's size, and a title of words keeps the shape's title font.
    Settings settings;
    WidgetContent figures, words;
    figures.figures = true;
    for (int theme = 0; theme < kFixedThemeCount; ++theme)
    {
        const BarLook look = LookOf(static_cast<BarTheme>(theme), settings, TextRow{.title = 14, .detail = 11, .alone = 17});
        CHECK(Family(look.TitleFace(figures), "Barlow Condensed") && look.TitleFace(figures).size == 17.0f);
        // Alone, a title takes the size of a line alone; over a second line, the two-line size.
        CHECK(Family(look.TitleFace(words), look.TitleFace(TwoLines()).family) && look.TitleFace(words).size == 17.0f);
        CHECK_EQ(look.TitleFace(TwoLines()).size, 14.0f);
    }
    CHECK(Family(LookOf(BarTheme::PhoenixGold, settings).TitleFace(words), "Marcellus SC"));
}

TEST(the_bar_takes_the_players_fonts_or_its_themes)
{
    Settings settings;
    CHECK(std::string(BarTitleFont(settings, BarShape::Ornate)) == TitleFont(BarShape::Ornate));
    CHECK(std::string(BarTextFont(settings)) == kDetailFont);
    settings.barTitleFont = "Cinzel";
    settings.barTextFont  = "Verdana";
    const char* title     = BarTitleFont(settings, BarShape::Ornate);
    CHECK(std::string(title) == "Cinzel" && std::string(BarTextFont(settings)) == "Verdana");
    settings.barTitleFont = "Georgia";
    BarTitleFont(settings, BarShape::Ornate);
    CHECK(std::string(title) == "Cinzel"); // an earlier name stays where caches keep it
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    CHECK(std::string(look.DetailFace().family) == "Verdana" && std::string(look.TitleFace(TwoLines()).family) == "Georgia");
}

TEST(the_games_art_is_in_use_for_vanilla_or_a_custom_theme_with_its_background)
{
    Settings s;
    CHECK(!UsesGameArt(s));
    s.barTheme = BarTheme::Vanilla;
    CHECK(UsesGameArt(s));
    s.barTheme = BarTheme::Custom;
    CHECK(!UsesGameArt(s));
    s.customPalette.shape = BarShape::Window; // the shape alone is not the art
    CHECK(!UsesGameArt(s));
    s.customPalette.background = BarBackground::Window;
    CHECK(UsesGameArt(s));
    s.barTheme = BarTheme::Aurora;
    CHECK(!UsesGameArt(s));
}

TEST(vanilla_without_the_games_art_is_phoenix_golds_colors_and_with_it_the_arts)
{
    Settings s;
    s.barTheme = BarTheme::Vanilla;
    CHECK(PaletteFor(s, nullptr) == PaletteOf(BarTheme::Vanilla));
    CHECK_EQ(PaletteFor(s, nullptr).accent, PaletteOf(BarTheme::PhoenixGold).accent);
    const GameSkinLook skin{SkinFrame{2, 1.0f, 0.5f, 1.0f}, PaletteOf(BarTheme::UmbraDark), kAuroraMenuColors};
    CHECK(PaletteFor(s, &skin) == PaletteOf(BarTheme::UmbraDark));
}

namespace
{
    double Linear(uint32_t argb, int shift)
    {
        const double c = static_cast<double>(argb >> shift & 0xFF) / 255.0;
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    }
    // WCAG's contrast ratio of two opaque colors.
    double Contrast(uint32_t a, uint32_t b)
    {
        const auto luminance = [](uint32_t c) { return 0.2126 * Linear(c, 16) + 0.7152 * Linear(c, 8) + 0.0722 * Linear(c, 0); };
        const double la = luminance(a), lb = luminance(b);
        return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
    }
    int Brightness(uint32_t argb) { return static_cast<int>((argb >> 16 & 0xFF) + (argb >> 8 & 0xFF) + (argb & 0xFF)); }
}

TEST(a_copy_of_a_built_in_theme_looks_as_it_does_until_a_change_and_then_changes_only_that)
{
    for (const BarTheme theme : {BarTheme::PhoenixGold, BarTheme::UmbraDark, BarTheme::PhoenixRed, BarTheme::Aurora})
    {
        Settings built;
        built.barTheme = theme;
        Settings copy;
        copy.barTheme             = BarTheme::Custom;
        copy.customPalette        = PaletteOf(theme);
        copy.customPalette.parent = theme;
        CHECK(MenuThemeFor(copy, nullptr) == MenuThemeFor(built, nullptr));
        CHECK(MenuColorsFor(copy, nullptr) == MenuColorsFor(built, nullptr));
        CHECK(PaletteFor(copy, nullptr) == PaletteFor(built, nullptr));
        // A new accent changes the window's accent, and its background stays the theme's.
        copy.customPalette.accent = 0xFF12AB34;
        const MenuColors was = MenuColorsFor(built, nullptr), now = MenuColorsFor(copy, nullptr);
        CHECK(now.accent == 0xFF12AB34u && now.background == was.background && now.card == was.card && now.text == was.text);
    }
}

TEST(the_settings_window_takes_the_top_panels_theme)
{
    Settings s;
    // Phoenix Gold, the default, gives a window lit in its gold.
    CHECK(MenuThemeFor(s, nullptr) == MenuTheme::Phoenix);
    CHECK_EQ(MenuColorsFor(s, nullptr).accent, PaletteOf(BarTheme::PhoenixGold).accent);
    // Phoenix Red keeps the window drawn from phoenix-xi.com's palette, and Aurora its glass.
    s.barTheme = BarTheme::PhoenixRed;
    CHECK(MenuColorsFor(s, nullptr) == kPhoenixMenuColors);
    s.barTheme = BarTheme::Aurora;
    CHECK(MenuThemeFor(s, nullptr) == MenuTheme::Aurora && MenuColorsFor(s, nullptr) == kAuroraMenuColors);
    // Custom's own colors.
    s.barTheme             = BarTheme::Custom;
    s.customPalette.accent = 0xFF12AB34;
    CHECK_EQ(MenuColorsFor(s, nullptr).accent, 0xFF12AB34u);
    // The game's art, once read, for Vanilla and a Custom theme on its art; Phoenix Gold's colors until then.
    const GameSkinLook skin{SkinFrame{2, 1.0f, 0.5f, 1.0f}, PaletteOf(BarTheme::UmbraDark), kAuroraMenuColors};
    s.customPalette.background = BarBackground::Window;
    CHECK(MenuThemeFor(s, &skin) == MenuTheme::GameWindow && MenuColorsFor(s, &skin) == kAuroraMenuColors);
    s.barTheme = BarTheme::Vanilla;
    CHECK(MenuThemeFor(s, &skin) == MenuTheme::GameWindow);
    CHECK(MenuThemeFor(s, nullptr) == MenuTheme::Phoenix);
    CHECK_EQ(MenuColorsFor(s, nullptr).accent, PaletteOf(BarTheme::PhoenixGold).accent);
}

TEST(a_window_from_a_panels_colors_reads_well_and_steps_up_from_its_page)
{
    for (int t = 0; t < kFixedThemeCount; ++t)
    {
        const MenuColors c = MenuColorsFrom(PaletteOf(static_cast<BarTheme>(t)));
        const uint32_t page = c.background | 0xFF000000u;
        CHECK(Contrast(c.text, page) >= 7.0 && Contrast(c.heading, page) >= 7.0 && Contrast(c.label, page) >= 4.5);
        // Cards, controls and a hovered control each a step lighter than the page, and the trays darker.
        CHECK(Brightness(page) < Brightness(c.card) && Brightness(c.card) < Brightness(c.control) &&
              Brightness(c.control) < Brightness(c.hover) && Brightness(c.inset) < Brightness(page));
        CHECK(c.accent == PaletteOf(static_cast<BarTheme>(t)).accent);
    }
}

TEST(widgets_pop_outs_and_their_sections_are_rounded_as_set_or_as_the_theme_does)
{
    Settings settings;
    for (const BarTheme theme : {BarTheme::PhoenixGold, BarTheme::UmbraDark})
    {
        const BarLook own = LookOf(theme, settings);
        const float widget = own.BoxOf(BarWidget::Target, true)->radius, popup = PopupPanel(own).radius,
                    card   = PopupCard(own).radius;
        Settings set = settings;
        set.widgetRounding  = 11;
        set.popupRounding   = 0;
        set.sectionRounding = 20;
        const BarLook rounded = LookOf(theme, set);
        CHECK(Near(rounded.BoxOf(BarWidget::Target, true)->radius, 11.0f) && Near(rounded.BoxOf(BarWidget::Bags, true)->radius, 11.0f));
        CHECK(Near(PopupPanel(rounded).radius, 0.0f) && Near(PopupCard(rounded).radius, 20.0f));
        // The theme's own, by default.
        CHECK(settings.widgetRounding == kThemeRounding && widget > 0.0f && popup > 0.0f && card > 0.0f);
        CHECK(Near(widget, theme == BarTheme::PhoenixGold ? 3.0f : own.metrics.pillRadius) && Near(popup, own.metrics.panelRadius));
    }
}

TEST(the_help_bar_is_rounded_as_set_or_as_the_theme_does)
{
    Settings settings;
    CHECK_EQ(settings.helpBarRounding, kThemeRounding);
    const BarLook own = LookOf(BarTheme::UmbraDark, settings);
    CHECK(Near(HelpBarPanel(own).radius, own.metrics.panelRadius));
    settings.helpBarRounding = 9;
    CHECK(Near(HelpBarPanel(LookOf(BarTheme::UmbraDark, settings)).radius, 9.0f));
}

TEST(a_themes_own_corner_rounding_rounds_whatever_is_set_by_theme)
{
    Settings settings;
    BarPalette p = PaletteOf(BarTheme::PhoenixGold);
    CHECK_EQ(p.rounding, kShapeRounding);
    p.rounding = 7;
    const BarLook look = LookOf(p, settings);
    CHECK(Near(look.BoxOf(BarWidget::Target, true)->radius, 7.0f) && Near(PopupPanel(look).radius, 7.0f));
    CHECK(Near(PopupCard(look).radius, 7.0f) && Near(HelpBarPanel(look).radius, 7.0f));
    // A rounding of their own still wins.
    settings.widgetRounding  = 2;
    settings.helpBarRounding = 0;
    const BarLook own = LookOf(p, settings);
    CHECK(Near(own.BoxOf(BarWidget::Target, true)->radius, 2.0f) && Near(HelpBarPanel(own).radius, 0.0f));
    CHECK(Near(PopupPanel(own).radius, 7.0f));
}

TEST(a_widget_is_padded_as_the_panel_says_or_as_the_theme_does)
{
    const BarMetrics m = MetricsFor(45, BarShape::Pills);
    Settings s;
    CHECK_EQ(WidgetPadding(m, s), m.padding); // the theme's
    s.barWidgetPadding = 6;
    CHECK_EQ(WidgetPadding(m, s), 6.0f);
}

TEST(the_help_bar_takes_its_set_width_text_size_and_height)
{
    Settings settings;
    TextRow lines;
    lines.title = 16;
    CHECK_EQ(HelpFace(LookOf(BarTheme::PhoenixGold, settings, lines)).size, 16.0f);
    CHECK_EQ(HelpCaptionFace(LookOf(BarTheme::PhoenixGold, settings, lines)).size, 16.0f);
    settings.helpBarTextSize = 21;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings, lines);
    CHECK_EQ(HelpFace(look).size, 21.0f);
    CHECK_EQ(HelpCaptionFace(look).size, 21.0f);
    CHECK(Family(HelpCaptionFace(look), look.titleFont) && Family(HelpFace(look), look.detailFont));
    CHECK_EQ(HelpBarHeight(kAutoHelpBarHeight, 24.0f), 24.0f);
    CHECK_EQ(HelpBarHeight(40, 24.0f), 40.0f);
}

TEST(the_fishing_bars_font_is_its_own_or_the_panels_text_font)
{
    Settings settings;
    CHECK(std::string(FishBarFont(settings)) == kDetailFont);
    settings.barTextFont = "Georgia";
    CHECK(std::string(FishBarFont(settings)) == "Georgia");
    settings.fishBarFont = "Verdana";
    CHECK(std::string(FishBarFont(settings)) == "Verdana");
}

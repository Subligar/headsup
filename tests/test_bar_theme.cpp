#include "bar_theme.h"
#include "cursor_picture.h"
#include "skin_frame.h"
#include "test.h"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    bool Near(float a, float b) { return std::fabs(a - b) < 0.01f; }
}

TEST(each_theme_has_the_mockups_colors_and_shape)
{
    const BarPalette& gold = PaletteOf(BarTheme::PhoenixGold);
    CHECK(gold.shape == BarShape::Ornate && gold.accent == 0xFFC9A24Au && gold.title == 0xFFEFE3C6u);
    CHECK_EQ(gold.below, 0x59C9A24Au); // the gold glow at 35%
    const BarPalette& umbra = PaletteOf(BarTheme::UmbraDark);
    CHECK(umbra.shape == BarShape::Pills && umbra.accent == 0xFF5B8DEFu && umbra.pill == 0xFF222226u);
    CHECK(PaletteOf(BarTheme::PhoenixRed).shape == BarShape::Ornate);
    CHECK_EQ(PaletteOf(BarTheme::PhoenixRed).accent, 0xFFC55151u);
    // Vanilla is Phoenix Gold's colors on the game's art until the art's own are read.
    const BarPalette& vanilla = PaletteOf(BarTheme::Vanilla);
    CHECK(vanilla.background == BarBackground::Window && vanilla.shape == BarShape::Window && vanilla.accent == gold.accent);
    CHECK_EQ(vanilla.border, kSkinEdge); // the art's own edge, drawn by Border Thickness
    // Aurora is laid out as pills are, in its own slate and cyan.
    const BarPalette& aurora = PaletteOf(BarTheme::Aurora);
    CHECK(aurora.shape == BarShape::Glass && aurora.accent == 0xFF5EE6FFu && aurora.title == 0xFFEAF2FFu);
}

TEST(a_theme_file_reads_back_every_part_of_its_palette)
{
    BarPalette p = PaletteOf(BarTheme::UmbraDark);
    for (size_t i = 0; i < std::size(kPaletteColors); ++i)
        p.*kPaletteColors[i].field = 0x01020300u + static_cast<uint32_t>(i);
    p.shape        = BarShape::Glass;
    p.border       = 4;
    p.background   = BarBackground::Window;
    p.windowType   = 6;
    p.rounding     = 9;
    p.separator    = DashMark::Square;
    p.parent       = BarTheme::Vanilla;
    const std::string file = ThemeFileText(p);
    CHECK(PaletteFromThemeFile(file) == p);
    // A part it lacks, or cannot be read, is Phoenix Gold's; a file of none of its parts is no theme.
    const std::optional<BarPalette> partial = PaletteFromThemeFile("[theme]\nTitle=ff102030\nAccent=nonsense\n");
    CHECK(partial && partial->title == 0xFF102030u && partial->accent == PaletteOf(BarTheme::PhoenixGold).accent);
    CHECK(!PaletteFromThemeFile("[other]\nTitle=ff102030\n") && !PaletteFromThemeFile(""));
    // A window type past the game's eight is its last; a background of no name is Phoenix Gold's.
    const std::optional<BarPalette> wild = PaletteFromThemeFile("[theme]\nTitle=ff102030\nWindowType=40\nBackground=stars\n");
    CHECK(wild && wild->windowType == kWindowTypes && wild->background == BarBackground::Gradient);
    // A rounding past the most is the most, and none read is the shape's own.
    const std::optional<BarPalette> round = PaletteFromThemeFile("[theme]\nTitle=ff102030\nRounding=99\n");
    CHECK(round && round->rounding == kMaxRounding && partial->rounding == kShapeRounding);
}

TEST(the_themes_folder_lists_its_theme_files_by_name)
{
    CHECK((FolderFileNames({"Dusk.ini", "dawn.INI", "notes.txt", "Dusk.cur"}, kThemeExtensions) ==
           std::vector<std::string>{"dawn", "Dusk"}));
}

TEST(sizes_follow_the_bar_height)
{
    const BarMetrics m = MetricsFor(36, BarShape::Ornate);
    // An icon at 65% of the pill's inside: 29 px rounded, less a pixel of border a side.
    CHECK(Near(m.height, 36.0f) && Near(m.title, 14.4f) && Near(m.subtitle, 12.0f) && Near(m.icon, 0.65f * 27.0f));
    // The pill leaves room above and below it for its text; the end cap, room either side of its diamond.
    CHECK(Near(m.pillHeight, 28.8f) && Near(m.cap, 23.04f));
    CHECK(Near(MetricsFor(36, BarShape::Pills).panelRadius, 0.28f * 12.0f / 0.39f) && Near(m.panelRadius, 0.11f * 12.0f / 0.39f));
    CHECK(Near(MetricsFor(200, BarShape::Pills).height, 64.0f) && Near(MetricsFor(1, BarShape::Pills).height, 24.0f));
    CHECK(std::string(TitleFont(BarShape::Ornate)) == "Marcellus SC" &&
          std::string(TitleFont(BarShape::Pills)) == kDetailFont);
}

TEST(the_bars_sizes_follow_its_height_and_the_pop_outs_their_own)
{
    for (const int h : {24, 36, 45, 64})
        for (const BarShape shape : {BarShape::Ornate, BarShape::Pills})
        {
            const BarMetrics m = MetricsFor(h, shape);
            const float H = static_cast<float>(h), u = 12.0f / 0.39f; // the height whose share 12 px is
            CHECK(Near(m.title, 0.40f * H) && Near(m.subtitle, 0.40f * H / 1.2f) && Near(m.gap, 0.17f * H));
            CHECK(Near(m.popupText, 14.0f) && Near(m.popupTitle, 18.0f) && Near(m.heading, 16.0f));
            CHECK(Near(m.popupPadding, 0.36f * u) && Near(m.rowGap, 0.11f * u));
            CHECK(Near(m.panelRadius, (shape == BarShape::Ornate ? 0.11f : 0.28f) * u));
        }
}

TEST(the_font_size_ratio_divides_the_second_lines_size)
{
    CHECK(Near(MetricsFor(45, BarShape::Ornate, 1.0f).subtitle, 18.0f));
    CHECK(Near(MetricsFor(45, BarShape::Ornate, 2.0f).subtitle, 9.0f) && Near(MetricsFor(45, BarShape::Ornate, 0.5f).subtitle, 36.0f));
    CHECK(Near(MetricsFor(45, BarShape::Ornate, 0.5f).title, 18.0f)); // the main line keeps its own
    // Its slider: 1 in the middle, the ends 0.2 and 2, and each position back where it was.
    CHECK(Near(FontRatioAt(kFontRatioSteps), 1.0f) && Near(FontRatioAt(0), 0.2f) && Near(FontRatioAt(2 * kFontRatioSteps), 2.0f));
    for (int position = 0; position <= 2 * kFontRatioSteps; ++position)
        CHECK_EQ(FontRatioPosition(FontRatioAt(position)), position);
    CHECK(FontRatioPosition(kDefaultFontRatio) > kFontRatioSteps && FontRatioPosition(9.0f) == 2 * kFontRatioSteps);
}

TEST(the_pop_out_text_scale_scales_its_text_alone_and_the_bar_keeps_its_own)
{
    const BarMetrics small = MetricsFor(64, BarShape::Pills, kDefaultFontRatio, 0.5f);
    const BarMetrics big   = MetricsFor(24, BarShape::Pills, kDefaultFontRatio, 2.0f);
    CHECK(Near(small.popupText, 7.0f) && Near(small.popupTitle, 9.0f) && Near(big.popupText, 28.0f) && Near(big.popupTitle, 36.0f));
    const BarMetrics one = MetricsFor(45, BarShape::Pills);
    CHECK(Near(big.heading, 2.0f * one.heading));
    // The pop-out's own padding, row gaps and corners stay as they are.
    CHECK(Near(big.popupPadding, one.popupPadding) && Near(big.rowGap, one.rowGap) && Near(big.panelRadius, one.panelRadius));
    CHECK(Near(small.title, 0.40f * 64.0f) && Near(big.title, 0.40f * 24.0f));
    // Width and Height are carried for the layout.
    const BarMetrics boxed = MetricsFor(45, BarShape::Pills, kDefaultFontRatio, kDefaultPopupScale, kDefaultIconScale, 1.5f, 0.5f);
    CHECK(Near(boxed.popupWidth, 1.5f) && Near(boxed.popupHeight, 0.5f) && Near(boxed.popupText, one.popupText));
    CHECK(Near(one.popupWidth, 1.0f) && Near(one.popupHeight, 1.0f));
}

TEST(icons_take_their_scale_of_the_most_a_pill_holds)
{
    // A 45 px bar's pill is 36 px: 34 inside its borders.
    CHECK(Near(MetricsFor(45, BarShape::Pills, kDefaultFontRatio, kDefaultPopupScale, 100).icon, 34.0f));
    CHECK(Near(MetricsFor(45, BarShape::Pills, kDefaultFontRatio, kDefaultPopupScale, 50).icon, 17.0f));
}

TEST(a_theme_file_keeps_the_window_shape)
{
    BarPalette p = PaletteOf(BarTheme::UmbraDark);
    p.shape      = BarShape::Window;
    CHECK(PaletteFromThemeFile(ThemeFileText(p)) == p);
}

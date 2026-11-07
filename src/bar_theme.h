#pragma once

#include "bar_types.h"
#include "dash_mark.h"
#include "fonts.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace headsup
{
    enum class BarShape : uint8_t
    {
        Ornate, // a strip with an edge line and glow, diamond caps and dividers, a framed target
        Pills,  // a strip with each widget in a rounded pill
        Glass,  // pills of glass that show only when lit, a line of light under the strip, and glass popups
        Window, // the game's window art under the strip, the popups and the settings window; pills only when lit
    };

    constexpr int kBarShapeCount = static_cast<int>(BarShape::Window) + 1;
    constexpr int kMaxStripBorder = 6; // rows of the strip's line at its foot, or the art's edge in pixels
    constexpr int kShapeRounding = -1, kMaxRounding = 32; // a corner's radius in pixels, or the shape's own

    // What the strip is: one color, a gradient through three stops, or the game's window art.
    enum class BarBackground : uint8_t { Plain, Gradient, Window };
    constexpr int kBarBackgroundCount = static_cast<int>(BarBackground::Window) + 1;
    constexpr int kWindowTypes      = 8; // the game's, in Config > Windows > Shared
    constexpr int kFollowGameWindow = 0; // a Window Type setting's: the type the game is set to, where 1 to 8 pin one

    // D3DCOLORs (ARGB); 0 is none.
    struct BarPalette
    {
        BarShape shape;
        uint32_t stripTop, stripMiddle, stripBottom; // the strip's gradient: top, 55% down, bottom
        uint32_t bottomLine, underLine, below;       // the strip's last row, one row under it, and what fades below
        uint32_t accent;                             // diamonds, frames, the pin mark, the target's border
        uint32_t accentEnd;                          // what the accent runs into where it is a gradient
        uint32_t pill, pillBorder;
        uint32_t title, subtitle, divider;
        uint32_t panelTop, panelBottom, panelBorder, panelOuter;
        uint32_t text, heading, tagFill, tagBorder, tagText;
        uint32_t highlight; // over a hovered widget
        int border = 1;     // how many of the strip's last rows are bottomLine, or how wide the art's edge is
        int rounding = kShapeRounding; // the corners of whatever is rounded by the theme
        BarBackground background = BarBackground::Gradient; // Plain is stripTop alone
        int windowType    = kFollowGameWindow; // the art's, for the Window background
        DashMark separator = DashMark::Diamond; // the panel's separators, and what a nameplate's dash is drawn as
        BarTheme parent    = BarTheme::PhoenixGold; // a custom theme's: the built-in one it was first copied from
        bool operator==(const BarPalette&) const = default;
    };

    // Every color of a palette, by the name its keys carry in the settings and in a theme's file.
    struct PaletteColor
    {
        const char* name;
        uint32_t BarPalette::*field;
    };
    inline constexpr PaletteColor kPaletteColors[] = {{"StripTop", &BarPalette::stripTop},
        {"StripMiddle", &BarPalette::stripMiddle}, {"StripBottom", &BarPalette::stripBottom},
        {"BottomLine", &BarPalette::bottomLine}, {"UnderLine", &BarPalette::underLine}, {"Below", &BarPalette::below},
        {"Accent", &BarPalette::accent}, {"AccentEnd", &BarPalette::accentEnd}, {"Pill", &BarPalette::pill},
        {"PillBorder", &BarPalette::pillBorder}, {"Title", &BarPalette::title}, {"Subtitle", &BarPalette::subtitle},
        {"Divider", &BarPalette::divider}, {"PanelTop", &BarPalette::panelTop}, {"PanelBottom", &BarPalette::panelBottom},
        {"PanelBorder", &BarPalette::panelBorder}, {"PanelOuter", &BarPalette::panelOuter}, {"Text", &BarPalette::text},
        {"Heading", &BarPalette::heading}, {"TagFill", &BarPalette::tagFill}, {"TagBorder", &BarPalette::tagBorder},
        {"TagText", &BarPalette::tagText}, {"Highlight", &BarPalette::highlight}};
    inline constexpr const char* kBarShapeNames[kBarShapeCount] = {"ornate", "pills", "glass", "window"};
    inline constexpr const char* kBarBackgroundNames[kBarBackgroundCount] = {"plain", "gradient", "window"};
    inline constexpr const char* kDashMarkNames[kDashMarkCount] = {"no", "dot", "square", "diamond"};
    inline constexpr const char* kBarThemeNames[kBarThemeCount] = {"phoenixGold", "umbraDark", "phoenixRedOrnate", "aurora",
        "custom", "gameWindow"};

    // A saved theme's file, in the themes folder: a [theme] section of its palette's parts by kPaletteColors' names,
    // Shape, Border, Rounding, Background, WindowType, Separator and Parent. Read back, a part it lacks or that cannot be
    // read is Phoenix Gold's, and a file of none of the colors is no theme.
    constexpr const char* kThemeExtensions[] = {".ini"};
    std::string ThemeFileText(const BarPalette& palette);
    std::optional<BarPalette> PaletteFromThemeFile(std::string_view file);

    // A fixed theme's palette; Custom's is the settings' own (PaletteFor in bar_look.h). Vanilla's is Phoenix Gold's
    // colors on the art, for while the art's own are not read.
    const BarPalette& PaletteOf(BarTheme theme);

    // Pixel sizes, each a share of the bar's height H (MetricsFor).
    struct BarMetrics
    {
        float height;       // H, clamped to kMinBarHeight..kMaxBarHeight
        float title;        // the most, as FitTextRow makes both lines smaller when their ink would not fit
        float subtitle;     // the title over the Font Size Ratio: the most, as the title's is
        float icon;         // Icon Scale's share of the pill's inside
        float padding;      // inside a widget, left and right
        float gap;          // between pills, around the target, after a widget's icons and around a popup
        float cap;          // an ornate end cap's width, with clear space either side of its diamond
        float diamond;
        float pillHeight;   // rounded to whole pixels by PillRow
        float pillRadius;
        float below;        // the glow or shadow under the strip
        float popupText;    // kPopupTextPixels times the pop-outs' Text Scale
        float popupTitle;   // kPopupTitlePixels times it
        float heading;      // kPopupHeadingPixels times it; the rest follow the spacing
        float popupPadding;
        float rowGap;       // between popup rows
        float panelRadius;  // by shape; none for the art's corners
        float popupWidth;   // each pop-out section's width, times its own
        float popupHeight;  // and its height
        bool operator==(const BarMetrics&) const = default;
    };
    // A pop-out's text, title and card headings at a Text Scale of 1, and the text size its spacing is set by.
    constexpr float kPopupTextPixels = 14.0f, kPopupTitlePixels = 18.0f, kPopupHeadingPixels = 16.0f, kPopupSpacingPixels = 12.0f;
    constexpr float kPillBorder      = 1.0f;                           // a pill's or frame's
    BarMetrics MetricsFor(int barHeight, BarShape shape, float fontRatio = kDefaultFontRatio,
        float popupTextScale = kDefaultPopupScale, int iconScale = kDefaultIconScale, float popupWidth = kDefaultPopupScale,
        float popupHeight = kDefaultPopupScale);

    // Ornate titles are Marcellus SC; everything else is the bundled Barlow Condensed.
    const char* TitleFont(BarShape shape);
    constexpr const char* kDetailFont = kBarlowCondensed;
}

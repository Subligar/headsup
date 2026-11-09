#pragma once

#include "bar_layout.h"
#include "bar_theme.h"
#include "bar_widgets.h"
#include "plate_image.h"
#include "popup_layout.h"
#include "settings.h"
#include "skin_frame.h"
#include "window_skin.h"

#include <cstdint>
#include <optional>

namespace headsup
{
    // A text's font family and pixel size.
    struct TextFace
    {
        const char* family = nullptr;
        float size         = 0.0f;
    };

    // A frame's palette, sizes and fonts: what every part of the bar is drawn with.
    struct BarLook
    {
        const BarPalette& palette;
        const Settings& settings;
        BarMetrics metrics;
        BarRow row;    // the pills' and the ornate target frame's
        TextRow lines; // the widgets' text
        const char* titleFont;
        bool ornate;
        const char* detailFont = kDetailFont; // details, figures and popup text
        std::optional<SkinFrame> skin;        // the game's art the strip and the popups are drawn in, for the Window shape

        // A span's color on the bar, and in a popup.
        uint32_t BarColor(const Span& span) const;
        uint32_t PopupColor(const Span& span) const;
        // A widget's title and detail line.
        // A widget's title: in the detail font at the title's size for a widget of figures, in the title face else.
        TextFace TitleFace(const WidgetContent& content) const;
        TextFace DetailFace() const;
        TextFace FaceOf(PopupFont font) const;
        // The icons and picture beside a widget's text.
        float Leading(const WidgetContent& content) const;
        // How a widget's box is drawn, or nothing for none. lit: hovered or open, with a popup; backed: its Show
        // Background on. Ornate: the target's frame and a lit widget's tint. Pills: a widget's pill while lit or backed.
        std::optional<BoxStyle> BoxOf(BarWidget widget, bool lit, bool backed) const;
        // The corners of a widget's box at a Corner Rounding.
        float WidgetRadius(int rounding) const;
    };

    // Whether the game's window art is drawn: the theme's background is the Window.
    bool UsesGameArt(const Settings& settings);
    // The settings window in the top panel's theme: Aurora's glass for Aurora, the game's art once it is read for the
    // Window shape, and the Phoenix window else; its colors phoenix-xi.com's for the Phoenix Red themes, Aurora's and
    // the art's for theirs, and the panel's own for the rest (MenuColorsFrom).
    MenuTheme MenuThemeFor(const Settings& settings, const GameSkinLook* skin);
    MenuColors MenuColorsFor(const Settings& settings, const GameSkinLook* skin);
    // A window in a panel's colors: its page the panel's foot, cards and controls each a step toward its text.
    MenuColors MenuColorsFrom(const BarPalette& palette);
    // The game's art as the settings draw it: without its border while Border is off.
    SkinFrame FrameAsSet(const Settings& settings, SkinFrame frame);
    // Whether a pill shows only while lit: the theme gives none of its widgets a box then and Show Widget Background
    // is off.

    // The palette the bar is drawn with: the picked theme's, the player's own for Custom, or the skin's for Vanilla
    // (Phoenix Gold's colors while none is read).
    const BarPalette& PaletteFor(const Settings& settings, const GameSkinLook* skin);

    // The bar's fonts: the player's, or the theme's when they set none. Each name lasts as long as the process, for the
    // caches that keep it by reference.
    const char* BarTitleFont(const Settings& settings, BarShape shape);
    const char* BarTextFont(const Settings& settings);
    // A widget's own of those, or the panel's where it set none.
    const char* WidgetTitleFont(const Settings& settings, BarShape shape, const WidgetOptions& own);
    const char* WidgetTextFont(const Settings& settings, const WidgetOptions& own);
    // The sizes a widget is drawn at: the panel's, at its own Font Size Ratio, Icon Scale and Pop-out Text Scale where
    // it set them. The panel's own with a widget of none.
    BarMetrics MetricsOf(const Settings& settings, BarShape shape, const WidgetOptions& own);
    // The fishing bar's line's font, or the panel's text font.
    const char* FishBarFont(const Settings& settings);

    constexpr float kCapScale      = 1.4f;  // an end cap's diamond, in diamonds
    constexpr float kLineIconGap   = 2.0f;  // pixels before an icon among a line's text
    constexpr float kPictureGap    = 6.0f;  // before a game picture there
    // The room a span's mark takes before its text, at letters size pixels tall.
    float MarkWidth(const Span& span, float size);
    // What a widget's title and detail line can hold, for how far their ink reaches: capitals, figures (taller
    // than the capitals in Marcellus, and some descend), a "[S]" zone's brackets, the bags' slash, descenders
    // (Marcellus's Q and J too) and commas; the detail line's ascenders, descenders and comma. Popup text holds
    // what a detail line does.
    constexpr const char* kTitleGlyphs  = "H0123456789[]/gjpqyQJ,";
    constexpr const char* kDetailGlyphs = "Hl0123456789%gjpqy,";
    // The row the palette draws under a strip, when it has one.
    float UnderLine(const BarPalette& palette);
    // Where the top bar's strip and its under line end.
    float BarBottom(const BarPalette& palette, const BarMetrics& metrics);
    // Where the help bar starts: the bar's gap under the top bar, so the two read as separate bars.
    float HelpBarTop(const BarPalette& palette, const BarMetrics& metrics);
    // As far from the screen's left edge as from the top bar.
    float HelpBarLeft(const BarMetrics& metrics);
    // Clear above and below the help bar's row.
    float HelpBarPad(const BarMetrics& metrics);
    // The help bar's caption and help, at its own text size or the bar's title size.
    int HelpTextPixels(int setting, int barTitle);
    TextFace HelpCaptionFace(const BarLook& look);
    TextFace HelpFace(const BarLook& look);
    // Its set height, or fitted, as tall as its text.
    float HelpBarHeight(int setting, float fitted);
    // The boxes a popup and the help bar are drawn with: a popup's panel with its outer border, a section's card (a
    // lift on the panel, rounded as a pill theme's panel is), a tag's pill height tall (the ornate themes keep their
    // gold edge around it), and the help bar's panel, bordered all round.
    BoxStyle PopupPanel(const BarLook& look);
    BoxStyle PopupCard(const BarLook& look);
    BoxStyle PopupTag(const BarLook& look, float height);
    BoxStyle HelpBarPanel(const BarLook& look);
    constexpr uint32_t kStarColor     = 0xFFE8C35A; // a popup's star, gold
    constexpr float kShadowStrength   = 0.6f;       // the bar's text shadow, of the mockup's

}

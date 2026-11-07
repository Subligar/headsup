#pragma once

#include "function_ref.h"

#include "bar_content.h"
#include "bar_game_help.h"
#include "bar_look.h"
#include "bar_theme.h"
#include "bar_types.h"
#include "game_font.h"
#include "icons.h"
#include "markup.h"
#include "plate_image.h"
#include "popup_layout.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace headsup
{
    // The images the bar's documents name, drawn by the bar's own image code when RmlUi asks for them: an icon by
    // its number, a theme's strip column (its rows and the rows fading below), a diamond or a coin by its pixels.
    constexpr std::string_view kHostSource = "hu:";
    std::string IconSource(Icon icon);
    std::string StripSource(const BarPalette& palette, int rows, int belowRows);
    std::string SeparatorSource(DashMark mark, uint32_t argb, int pixels);
    std::string CoinSource(int pixels);
    // A game item's picture, which only the plugin can make (from the game's resources), and the item a name is.
    std::string ItemSource(uint16_t item);
    std::optional<uint16_t> ItemOfSource(std::string_view name);
    std::string ResetSource(uint32_t argb, int pixels); // the circular arrow that puts a color back
    std::string CogSource(uint32_t argb, int pixels);   // the cog on a popup's settings chip
    // The image a name stands for; nothing for a name that is none of the above.
    std::optional<Image> HostImage(std::string_view name);
    // A picture of the game's, from what was read of its files at load: nothing for another name, or when it was not read.
    std::string GamePictureSource(GamePicture picture);
    std::optional<Image> GamePictureImage(const std::optional<PlayOnlineIcons>& icons, const std::optional<NationCrests>& crests,
        std::string_view name);

    // A line of a widget as RmlUi lays it out: its line box's height, where its top goes from the strip's top, how
    // far under that top the icons after its text start, and how far its capitals' middle is, where an EXP bar's goes.
    struct BarLinePlace
    {
        int height       = 0;
        float top        = 0.0f;
        float iconDrop   = 0.0f;
        float capsMiddle = 0.0f;
        bool operator==(const BarLinePlace&) const = default;
    };
    // Where the widgets' lines go so their ink lands on the rows the bar's text row gives: a title in the title
    // font and a title of figures in the detail font, over a detail line and alone, and a detail line.
    struct BarTextPlace
    {
        BarLinePlace title, figures, alone, figuresAlone, detail;
        BarLinePlace helpCaption, help; // the help bar's, at its own text size
        bool operator==(const BarTextPlace&) const = default;
    };

    // The bar's markup and style: the strip across the screen, the end caps when the settings show them, a divider
    // for each pair of neighbours and every placed widget's element, all sized from the look. The widgets and dividers
    // carry the hidden class and no place: the plugin shows and places them.
    std::string BarDocument(const BarLook& look, const BarTextPlace& place);
    std::string WidgetId(PanelWidget widget);
    // A widget's margin and padding on one side as its element takes them, for a room from its edge to what it
    // shows: inside a pill's or a frame's border, or past a plain ornate widget's margin. A side it shares in a
    // combined pill has neither there, so all of the room is padding.
    struct WidgetSide
    {
        float margin = 0.0f, padding = 0.0f;
    };
    WidgetSide SideOf(const BarLook& look, PanelWidget widget, float room, bool shared);
    std::string DividerId(int divider);
    constexpr int kDividerCount       = kMostPlaced - 1;
    constexpr const char* kStripId    = "strip";
    constexpr const char* kRightCapId = "cap-right";
    constexpr const char* kHiddenClass = "hidden";
    constexpr const char* kLitClass    = "lit"; // hovered or open, with a popup
    constexpr const char* kTargetClass     = "target"; // the target's own frame and tint: off with Show Background off
    constexpr const char* kFlatClass       = "flat"; // a pill shown only while lit
    constexpr const char* kBackedClass     = "backed"; // showing its pill or tint while not lit (Show Background)
    // Shares its pill with the widget before it, or after it, and has no edge or round corners on that side.
    constexpr const char* kJoinBeforeClass = "join-before";
    constexpr const char* kJoinAfterClass  = "join-after";
    // A widget's class for the section it is in, whose side of the panel its content lines up with.
    const char* SectionClass(BarSection section);
    // A widget's class for its Alignment, which wins over its section's; empty for Set by Location.
    const char* AlignClass(WidgetAlign align);
    // A widget's inner markup: its icons, coin, title and detail lines in their colors, and its progress bar.
    // columns: a widget of cells' column widths (CellColumns).
    std::string WidgetRml(const WidgetContent& content, const BarLook& look, const std::vector<float>& columns);
    // A widget of cells' columns: each as wide as its widest cell, which width measures (title: on the first line).
    std::vector<float> CellColumns(const WidgetContent& content, FunctionRef<float(bool title, const Span&)> width);
    float CellsWidth(const std::vector<float>& columns, float gap);

    // Game help's own bar under the top bar: its box when it has a background, and its caption, the bag's count and
    // its help where the row puts them, each centered on the row. Nothing for a row with no box.
    struct HelpBarParts
    {
        HelpBarRow row;
        std::string caption, help;
        std::optional<Span> bag;
        bool operator==(const HelpBarParts&) const = default;
    };
    std::string HelpBarRml(const HelpBarParts& parts, const BarLook& look, const BarTextPlace& place);
    constexpr const char* kHelpId = "help";

    // A popup's text faces, by PopupFont, as RmlUi sets them: the look's faces in whole pixels, each with the
    // height of its line box and how far under a laid out line's top that box starts, so its baseline is the
    // layout's.
    struct PopupFace
    {
        std::string family;
        int pixels = 0, line = 0;
        float drop = 0.0f;
        bool operator==(const PopupFace&) const = default;
    };
    constexpr float kPercentWhole = 100.0f; // a share's whole, as RCSS percentages count it
    constexpr int kPopupFontCount = static_cast<int>(PopupFont::Pin) + 1;
    using PopupFaces = std::array<PopupFace, kPopupFontCount>;
    // The popups' document: an empty popup over the whole screen that takes no clicks, styled from the look.
    std::string PopupDocument(const BarLook& look, const PopupFaces& faces);
    // Sunk into what holds it: darker, its upper edges in shadow and its lower ones lit.
    std::string SocketRules(float radius);
    // The panel's text shadow as a font effect, for text of that many pixels.
    std::string ShadowEffect(int pixels);
    constexpr const char* kOpenClass = "open"; // on a popup while it shows, so what it does as it opens plays each time
    constexpr const char* kPopupClass = "popup"; // each pill's popup's element
    // A laid out popup's markup, from its top left corner: its panel, then each piece where the layout put it.
    std::string PopupRml(const PopupLayout& layout, const BarLook& look, const PopupFaces& faces);
}

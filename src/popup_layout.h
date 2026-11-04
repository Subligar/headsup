#pragma once

#include "bar_layout.h"
#include "bar_theme.h"
#include "bar_widgets.h"
#include "function_ref.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace headsup
{
    enum class PopupFont : uint8_t
    {
        Title,   // TitleFont, larger than popupText
        Text,    // the detail font at popupText
        Heading, // TitleFont at heading size; uppercase and letter-spaced in the detail font for pills
        Pin,     // the pinned mark: a heading's font and size, set solid
    };

    struct PopupPiece
    {
        enum class Kind : uint8_t
        {
            Text,
            Icon,
            ImmuneMark, // a ×, its ink centered in the piece: inside an immune icon, at its bottom right
            Star,       // a starred cell's star, after its text
            Rule,       // an ornate heading's fading line
            Tag,        // a tag's frame, under its text
            Card,       // a section's rounded ground, under its heading and rows
            Socket,     // the sunken ground under a tab's rows
            Tab,        // a card's tab's chip, under its name; open while picked
            Item,       // a game item's picture, which the plugin serves
            ScrollTrack, // a scrolling socket's scrollbar, in its right padding
            ScrollThumb, // and the part of it its shown lines are
            Picture,     // a picture from the game's files, as an icon is drawn
            PinChip,     // the pin's chip, drawn as a tab's; open while pinned
            PinWord,     // the word on it, drawn as text
            CogChip,     // the settings chip left of the pin, drawn as a tab's; open while the settings show
            Cog,         // the cog on it
        };
        Kind kind = Kind::Text;
        float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f; // from the popup's top-left corner
        Span span;                                             // Text
        PopupFont font = PopupFont::Text;
        Icon icon      = Icon::AggroNQ; // Icon
        size_t line    = 0;             // in the layout's lines
        bool open      = false;         // Tab: picked
        bool right     = false;         // Text: drawn against its box's right, as a pair's value is
        uint16_t item  = 0;             // Item
        GamePicture picture = GamePicture::None; // Picture
        bool tinted = false; // Icon: one of HeadsUp's own white ones, drawn in its span's color
    };

    // Where a card's tab is, from the popup's top-left corner, to click: its key the card's heading. Or a pressable
    // cell's, whose tab is what a press on it tells the plugin.
    struct PopupTabChip
    {
        std::string key;
        float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
        size_t line = 0;
        std::string tab;
        bool press = false;
    };

    // A socket that shows only some of its lines, from the popup's top-left corner, and how many lines it scrolls.
    struct PopupScroll
    {
        std::string key;
        float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
        size_t line = 0;
        size_t most = 0;
        BarRect grip{};      // the socket's right padding down its scrollbar's track, which a press drags
        float thumb = 0.0f;  // the thumb's length in it
    };

    // The title, or a section's card: a popup cut short keeps or drops each whole.
    struct PopupLine
    {
        float bottom = 0.0f; // from the popup's top
    };

    struct PopupLayout
    {
        float width = 0.0f, height = 0.0f;
        std::vector<PopupPiece> pieces; // line by line
        std::vector<PopupLine> lines;   // top to bottom
        std::vector<PopupTabChip> chips;
        std::vector<PopupScroll> scrolls;
        BarRect pin{}; // the pin chip, which a press pins or unpins the popup by; none in an empty popup
        BarRect cog{}; // the settings chip, which a press shows the widget's settings by, or its content again
        BarRect body{}; // where the settings go while they show: under the title, as wide as the popup's inside
    };

    // The room the widget's settings take, as the settings window lays them out, which a popup showing them leaves
    // under its title in place of its sections. 0 tall before they were first laid out.
    struct SettingsRoom
    {
        float width = 0.0f, height = 0.0f;
        bool operator==(const SettingsRoom&) const = default;
    };

    struct TextSize
    {
        float width = 0.0f, height = 0.0f;     // the line box GDI draws
        float inkTop = 0.0f, inkBottom = 0.0f; // its font's ink in it, from its top: capitals and figures to descenders
        float overhang = 0.0f;                 // how far its ink reaches past width: a last letter can pass its advance
    };
    using SpanMeasure = FunctionRef<TextSize(const Span&, PopupFont)>;

    // The title, then each section with a heading as a card holding it and its rows; rows without a heading sit
    // under the title, on the panel. A pair's value ends at its card's right edge. When the cards stacked would run
    // taller than kTwoColumnsAbove popup text sizes, they flow into two columns as even as their order allows, and
    // into more when two would run taller than room, a card too tall for it going on in the next under its heading.
    // The pin chip sits at the title row's top right: "pin", or lit and "pinned" while pinned, with the settings chip
    // left of it, lit while settings show. It is never narrower than least, its cards widening to fill it. With
    // settings, their room is left under the title in place of the sections, no taller than room leaves.
    constexpr float kTwoColumnsAbove = 24.0f;
    PopupLayout LayoutPopup(const PopupContent& content, const BarMetrics& metrics, BarShape shape, bool pinned,
                            SpanMeasure measure, float room = std::numeric_limits<float>::infinity(), float least = 0.0f,
                            std::optional<SettingsRoom> settings = std::nullopt);
    // A layout taller than height keeps the title and the cards that end padding or more above it, and shrinks to
    // them with its padding under the lowest.
    void CutPopup(PopupLayout& layout, float height, float padding);
}

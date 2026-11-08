#pragma once

#include "bar_types.h"
#include "figures.h"
#include "function_ref.h"
#include "icons.h"
#include "plate_settings.h"
#include "status_icons.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace headsup
{
    // How a piece of text is colored: by the theme, by its meaning (the badge colors in settings), or by its own color.
    enum class Tone : uint8_t
    {
        Normal,  // the bar's title color; a popup's text color
        Dim,     // the bar's subtitle color; quieter popup text
        Heading,
        Tag,     // a tag's word, in the tag's text color
        Neutral, // a value with no effect, #8d8d8d
        Weak,
        Resist,
        Immune,
        Own, // argb
    };

    // A picture read from the game's files at load, drawn in place of a span's text as tall as its letters.
    enum class GamePicture : uint8_t
    {
        None,
        PlayOnlineMail,
        PlayOnlineMessenger,
        SanDoriaCrest,
        BastokCrest,
        WindurstCrest,
        BeastmenCrest,
        JeunoCrest,
        AhtUrhganCrest,
    };

    // An EXP bar among a line's text: width by height pixels, filled to done of its width, its text across it.
    struct InlineBar
    {
        float done = 0.0f;
        int width = 0, height = 0;
        std::string text;
        Align align = Align::Center; // its text's
        bool operator==(const InlineBar&) const = default;
    };

    // A status's icon from a theme's Status Icons (kVanillaStatusIcons or a pack's place), over its buff's or debuff's
    // background with the theme's Show Status Background.
    struct StatusMark
    {
        uint16_t id = 0;
        int pack    = kVanillaStatusIcons;
        bool back   = false;
        bool operator==(const StatusMark&) const = default;
    };

    // Some text, and before it at most one mark: a game picture, an icon, gil's coin or a status's icon as tall as its
    // letters, or an EXP bar.
    struct Span
    {
        std::string text;
        Tone tone           = Tone::Normal;
        uint32_t argb       = 0;
        GamePicture picture = GamePicture::None;
        std::optional<Icon> icon{};
        bool coin = false;
        std::optional<InlineBar> bar{};
        uint16_t item = 0; // a game item's picture
        std::optional<StatusMark> status{};
        // A text that ticks (a time, a distance) keeps the room of the widest of these, its digits any digit: each
        // digit drawn in a cell as wide as its font's widest, the text against the room's right, so nothing after it
        // moves.
        std::vector<std::string> room{};
        // That room's width with the text's trailing spaces, the spaces' width, and the widest digit's (SteadyBoxes).
        float box = 0.0f, trail = 0.0f, digit = 0.0f;
        bool operator==(const Span&) const = default;
    };
    constexpr bool HasMark(const Span& s)
    {
        return s.picture != GamePicture::None || s.icon || s.coin || s.bar || s.item != 0 || s.status;
    }

    struct WidgetContent
    {
        bool shown = false;
        std::vector<Icon> icons; // before the text, beside both lines
        bool coin = false;       // gil's coin there
        bool cog = false;        // the settings cog, alone: the Settings widget
        GamePicture picture = GamePicture::None; // a picture from the game's files there, as an icon is
        bool iconsRight = false; // those after the text instead
        std::vector<Span> title;
        std::vector<Span> subtitle; // empty: one line
        bool hasPopup = false;
        // The title holds figures, drawn in the detail font in every theme: an ornate title font's 0 reads as an O.
        bool figures = false;
        // Each span of either line is a cell, and the cells at one place of their lines a column, the columns a gap
        // apart: the Conquest widget's two by two.
        bool cells = false;
        bool operator==(const WidgetContent&) const = default;
    };

    struct PopupCell
    {
        std::optional<Icon> icon;
        bool immune = false; // the icon carries a × in the immunity color
        std::vector<Span> text;
        bool star = false; // a star after the text: who holds what the cell's row is about
        uint16_t item = 0; // a game item's picture before the text, in place of an icon
        bool coin     = false; // gil's coin there, in place of an icon
        std::optional<StatusMark> status{}; // a status's icon there, in place of an icon
        Align align   = Align::Left; // in a grid, against its column's left, in its middle or against its right
        GamePicture picture = GamePicture::None; // a picture from the game's files before the text, as an icon is
        std::optional<Span> tint{}; // its icon, one of HeadsUp's own white ones, drawn in this span's color
        std::string press = {}; // a press on the cell tells the plugin this (ApplyPopupPress)
        bool operator==(const PopupCell&) const = default;
    };

    enum class RowKind : uint8_t
    {
        Line, // cells one after another
        Tags, // each cell framed as a tag
        Grid, // cells in columns
        Pair, // a label and its value, the value at the right edge
    };

    struct PopupRow
    {
        RowKind kind = RowKind::Line;
        int columns  = 0; // Grid
        std::vector<PopupCell> cells;
        bool nested = false; // set in a socket with the nested rows beside it, as a tab's rows are
        bool header = false; // Grid: its first line heads its columns, and each group of them set side by side
        bool spread = false; // Grid: a right-aligned column at the card's right edge, a centered middle one right of middle
        bool trailing = false; // Grid: its columns after the first against the card's right edge, as they are to each other
        bool inset  = false; // in from the card's sides as far as a socket's rows, to line up with a socket under it
        bool roomy  = false; // Grid: its columns further apart
        // A socket's first row: what its scrolling is kept by, and how many of its lines it is scrolled past.
        std::string scroll{};
        size_t scrolled = 0;
        bool operator==(const PopupRow&) const = default;
    };

    // One of a card's tabs: its name on its chip, and the rows it shows while picked.
    struct PopupTab
    {
        Span name;
        std::vector<PopupRow> rows;
        std::optional<Icon> icon{}; // before its name
        GamePicture picture = GamePicture::None; // or a picture from the game's files there
        bool operator==(const PopupTab&) const = default;
    };

    struct PopupSection
    {
        std::string heading; // empty for the rows under the title
        std::vector<PopupRow> rows;
        std::vector<PopupTab> tabs{}; // under the heading, a click picking one, by the heading
        size_t tab = 0;               // the picked one, whose rows follow the section's own
        bool beside = false; // in one row with the card before it, the two as tall as each other
        // A headed card's chip at its heading's right tells this when pressed, which collapses or opens it; none
        // without it. Collapsed, it shows its heading alone.
        std::string collapsePress{};
        bool collapsed = false;
        bool under = false; // in the column of the card before it, under it: after a card beside, a column beside a card
        size_t tabsAcross = 0; // its tabs a row; 0 for as many as fit
        bool tabEachOpen  = false; // its tab is tab again each time the popup opens, until another is picked
        bool operator==(const PopupSection&) const = default;
    };

    struct PopupContent
    {
        std::optional<Icon> icon;
        std::vector<Span> title;
        std::vector<Span> subtitle; // a line right under the title, outside any card
        std::vector<PopupSection> sections;
        bool oneColumn = false; // its cards down one column, however tall
        bool settings  = true;  // its widget's settings show in place of it behind a cog: Game help's are in their own card
        // While its settings show, what its title's clone and remove chips tell when pressed; empty for none.
        std::string clonePress{}, removePress{};
        // While it is pinned somewhere the player dragged it, what its title's reset chip tells; empty for none.
        std::string resetPress{};
        bool operator==(const PopupContent&) const = default;
    };

    constexpr const char* kDot = " \xC2\xB7 "; // a middle dot between two parts of a line
    // What a card is kept by (its picked tab, its scrolling, whether it is collapsed): its heading without what is in
    // brackets, which can change while it shows, as the tally's countdown in the Conquest card's does.
    inline std::string_view CardKey(std::string_view heading) { return heading.substr(0, heading.find(" (")); }
    // Units of time, one place for every widget's clocks and spans.
    constexpr int64_t kMsPerSecond = 1000, kSecondsPerMinute = 60, kSecondsPerHour = 3600, kSecondsPerDay = 86400;
    constexpr int64_t kMsPerMinute = kMsPerSecond * kSecondsPerMinute, kMsPerHour = kMsPerSecond * kSecondsPerHour,
                      kMsPerDay = kMsPerSecond * kSecondsPerDay;

    // value in a printf format: Printed("%.1f ms", 16.66) is "16.7 ms".
    template <typename Value>
    std::string Printed(const char* format, Value value)
    {
        char text[48];
        std::snprintf(text, sizeof(text), format, value);
        return text;
    }
    // "2d 8h", "8h 30m", "5m 09s", "9s": the two largest units, rounded up to the whole second.
    std::string SpanText(double seconds);

    // "21:07", or "9:07 PM" on a 12-hour clock.
    // second is shown when it is 0 or more: "21:07:05", "9:07:05 PM".
    std::string ClockText(int hour, int minute, bool twentyFour, int second = -1);
    // The same as spans that keep their room as it ticks: the time, then AM or PM in a room of its own.
    std::vector<Span> ClockSpans(int hour, int minute, bool twentyFour, int second = -1);
    // "09:05" in a span that keeps its room.
    Span HourMinuteSpan(int hour, int minute);
    // "09:05"
    std::string HourMinute(int hour, int minute);
    // A figure running low, and one gone wrong.
    constexpr uint32_t kWarningColor = 0xFFFFB347;
    constexpr uint32_t kAlarmColor   = 0xFFFF5A5A;

    // A braced list would copy its items: an initializer_list's elements are const.
    template <typename T, typename... More>
    std::vector<T> ListOf(T first, More... more)
    {
        std::vector<T> items;
        items.reserve(1 + sizeof...(more));
        items.push_back(std::move(first));
        (items.push_back(std::move(more)), ...);
        return items;
    }

    // A part added to a line of parts, two spaces after the one before.
    void AddPart(std::vector<Span>& line, std::vector<Span> part);
    Span Text(std::string text, Tone tone = Tone::Normal);
    Span Colored(std::string text, uint32_t argb);
    PopupCell Cell(std::vector<Span> text, std::optional<Icon> icon = std::nullopt);
    PopupCell Cell(Span text, std::optional<Icon> icon = std::nullopt);
    PopupRow Line(PopupCell cell);
    PopupRow LineOf(std::string text, Tone tone = Tone::Normal);
    // "Label value", the label dim and the value at the right edge.
    PopupRow Labeled(std::string label, Span value);
    // The same with the label in its own tone or color.
    PopupRow Labeled(Span label, Span value);
    PopupRow Grid(int columns, size_t cells);
    // text with each of its digits replaced by digit. A line's room is measured from this, with its font's widest
    // digit, so figures that tick never change the room it takes.
    std::string SteadyFigures(std::string_view text, char digit);
    // The digit that measures widest, the lowest of those that tie.
    char WidestDigit(FunctionRef<float(char)> measure);
    constexpr bool IsDigit(char c) { return c >= '0' && c <= '9'; }
    // text as a span with room draws it: each digit alone, each run of other characters together.
    std::vector<std::string_view> FigurePieces(std::string_view text);
    // Sets box, trail and digit of each span with room on a line of one font: digit its widest digit's width, width a
    // text's.
    void SteadyBoxes(std::vector<Span>& line, float digit, FunctionRef<float(const std::string&)> width);
}

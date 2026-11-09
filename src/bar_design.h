#pragma once

#include "bar_content.h"
#include "bar_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // What a widget can show on the panel, each part on its own button in the Widget Designer.
    enum class WidgetPart : uint8_t
    {
        ZoneName, ZoneRegion, ZoneCrest, ZoneNation,
        WeatherIcon, WeatherName, VanaDate, VanaDay, VanaTime, VanaMoon,
        TargetAggro, TargetName, TargetDistance, TargetDetection, TargetLevel, TargetPlayer, TargetWeak,
        CurrencyCoin, Currency1Name, Currency2Name, Currency1Amount, Currency2Amount, Currency1Icon, Currency2Icon,
        Bag1Count, Bag2Count, Bag1Name, Bag2Name, BagIcon,
        ClockTime, ClockServer, ClockVana, ClockIcon, ClockDate,
        JobJobs, JobEarned, JobToNext, JobPercent, JobBar,
        PerfFps, PerfSent, PerfReceived, PerfPercent, PerfMemory, PerfMessages,
        PartyCount, PartyHealth,
        StatusPin1, StatusPin2, StatusPin3, StatusPin4, StatusPin5, StatusPin6,
        WorldShip, WorldGuild, WorldSanDoria, WorldBastok, WorldWindurst, WorldBeastmen,
    };
    constexpr int kWidgetPartCount = static_cast<int>(WidgetPart::WorldBeastmen) + 1;

    struct DesignChip
    {
        WidgetPart part;
        bool on = true;
        bool operator==(const DesignChip&) const = default;
    };
    constexpr int kDesignLines = 2;
    // A widget's two lines, each its parts in order, shown or not. Every part of its widget is on one line once.
    struct WidgetDesign
    {
        std::array<std::vector<DesignChip>, kDesignLines> lines;
        bool operator==(const WidgetDesign&) const = default;
    };

    std::span<const WidgetPart> PartsOf(BarWidget kind); // none for the game's help
    // The part that is the widget's icon, placed by its Icon setting rather than in its design; none for a widget
    // without one.
    std::optional<WidgetPart> IconPartOf(BarWidget kind);
    WidgetDesign DefaultDesign(BarWidget kind);
    // A part's name in the settings file, one of its widget's alone. Saved: never rename one.
    const char* PartFileName(WidgetPart part);
    // The lines' file names a space apart, " | " between the lines, an off part's after a '-'.
    std::string DesignText(const WidgetDesign& design);
    // The text DesignText writes back as a design, made whole; nothing for the default.
    WidgetDesign DesignFrom(BarWidget kind, std::string_view text);
    // Each of kind's parts once, the first kept: another widget's dropped, a missing one added off at the end of the
    // second line.
    WidgetDesign Whole(BarWidget kind, WidgetDesign design);
    // The first part on when none is, for a widget with parts.
    void TurnOnFirst(WidgetDesign& design);
    bool PartOn(const WidgetDesign& design, WidgetPart part);
    bool AnyPartOn(const WidgetDesign& design);
    // The part at from on fromLine moved to toLine before the part at at (counted before the move), or to its end.
    // False when nothing moved.
    bool MovePart(WidgetDesign& design, int fromLine, size_t from, int toLine, size_t at);
    // False, and nothing changed, for the last part on, unless pillShows: another widget of its pill has a part on.
    bool TurnOff(WidgetDesign& design, WidgetPart part, bool pillShows);

    // What a part shows now: nothing when it has nothing to say.
    struct PartContent
    {
        WidgetPart part;
        std::vector<Span> spans;
        std::optional<Align> align{}; // a text's own place on its line
    };
    // base with its lines made of parts as design orders them, a part's gap between two. The parts of marks alone at
    // the very start of the first line are its leading icons and coin; a first line with nothing gives way to the
    // second. A part on the second line dims unless it shades its own text. On a line with aligned parts, those of one
    // alignment are one run (Span::align), left, center then right; a part with none goes with the part before it, else
    // the one after.
    WidgetContent ComposeContent(const WidgetDesign& design, std::vector<PartContent> parts, WidgetContent base);
    // The picture a part's chip in the Widget Designer shows before its name: an icon or a game picture.
    struct PartMark
    {
        std::optional<Icon> icon;
        GamePicture picture = GamePicture::None;
        bool operator==(const PartMark&) const = default;
    };
    // The first mark among a part's spans, if any.
    std::optional<PartMark> MarkOf(const std::vector<Span>& spans);
    // What a part of icons shows while it has none now (no target, no weather yet); nothing for a part of text.
    PartMark DefaultMark(WidgetPart part);

    // A copy's pop-out: rows of cards, any widget's, each card once and a row 1 to kMostCardsInRow of them.
    constexpr size_t kMostCardsInRow = 3;
    struct PopupDesign
    {
        std::vector<std::vector<CardKind>> rows;
        bool operator==(const PopupDesign&) const = default;
    };
    BarWidget WidgetOfCard(CardKind card);
    std::vector<CardKind> CardsOf(BarWidget kind); // none for the game's help and the Settings widget
    // A card's name in the settings file, one of all the cards' alone. Saved: never rename one.
    const char* CardFileName(CardKind card);
    // The widget's own cards in rows, as its pop-out laid them out before it had a design.
    PopupDesign DefaultPopupDesign(BarWidget kind);
    // The rows' file names a space apart, " | " between rows; kNoCards for none.
    std::string PopupDesignText(const PopupDesign& design);
    // The text PopupDesignText writes back as a design, made whole; nothing for kind's default.
    PopupDesign PopupDesignFrom(BarWidget kind, std::string_view text);
    // Each card once, the first kept; a row past kMostCardsInRow cut there, an empty one dropped.
    PopupDesign WholePopup(PopupDesign design);
    bool HoldsCard(const PopupDesign& design, CardKind card);
    // Whether card can go in row: one with room, one holding it already, or the new row after the last.
    bool RowTakes(const PopupDesign& design, size_t row, CardKind card);
    // card put in row before the card at at (counted before the move), or at its end; row past the last makes a new
    // one. It leaves where it was, and a row left empty goes. False when nothing changed or the row has no room.
    bool PlaceCard(PopupDesign& design, CardKind card, size_t row, size_t at);
    // False when the design does not hold it.
    bool RemoveCard(PopupDesign& design, CardKind card);

    // Which of a line's spans is cut when the line is too long: the one with the most letters, a dim one only when no
    // other has any; the size when none has any.
    size_t WidestTextSpan(const std::vector<Span>& line);
}

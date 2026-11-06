#pragma once

#include "bar_content.h"
#include "bar_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
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
        WeatherIcon, WeatherName, WeatherChance,
        VanaDay, VanaTime, VanaDate, VanaMoon,
        TargetAggro, TargetName, TargetDistance, TargetDetection, TargetLevel, TargetPlayer, TargetWeak,
        CurrencyCoin, CurrencyAmount, CurrencyName,
        BagCount, BagName, BagIcon,
        ClockTime, ClockServer, ClockVana,
        JobJobs, JobExp, JobBar,
        PerfFps, PerfSent, PerfReceived, PerfPercent, PerfPing, PerfMemory, PerfMessages,
        PartyCount, PartyHealth,
        StatusPins,
        WorldIcon, WorldMain, WorldDetails,
    };
    constexpr int kWidgetPartCount = static_cast<int>(WidgetPart::WorldDetails) + 1;

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
    WidgetDesign DefaultDesign(BarWidget kind);
    // A part's name in the settings file, one of its widget's alone. Saved: never rename one.
    const char* PartFileName(WidgetPart part);
    // The lines' file names a space apart, " | " between the lines, an off part's after a '-'.
    std::string DesignText(const WidgetDesign& design);
    // The text DesignText writes back as a design, made whole; nothing for the default.
    WidgetDesign DesignFrom(BarWidget kind, std::string_view text);
    // Each of kind's parts once, the first kept: another widget's dropped, a missing one added off at the end of the
    // second line, and the first part on when none is.
    WidgetDesign Whole(BarWidget kind, WidgetDesign design);
    bool PartOn(const WidgetDesign& design, WidgetPart part);
    // The part at from on fromLine moved to toLine before the part at at (counted before the move), or to its end.
    // False when nothing moved.
    bool MovePart(WidgetDesign& design, int fromLine, size_t from, int toLine, size_t at);
    // False, and nothing changed, for the last part on: a widget always shows something.
    bool TurnOff(WidgetDesign& design, WidgetPart part);

    // What a part shows now: nothing when it has nothing to say.
    struct PartContent
    {
        WidgetPart part;
        std::vector<Span> spans;
    };
    // base with its lines made of parts as design orders them, a part's gap between two. The parts of marks alone at
    // the very start of the first line are its leading icons and coin; a first line with nothing gives way to the
    // second. A part on the second line dims unless it shades its own text.
    WidgetContent ComposeContent(const WidgetDesign& design, std::vector<PartContent> parts, WidgetContent base);
    // The picture a part's chip in the Widget Designer shows before its name: an icon, a game picture or gil's coin.
    struct PartMark
    {
        std::optional<Icon> icon;
        GamePicture picture = GamePicture::None;
        bool coin           = false;
        bool operator==(const PartMark&) const = default;
    };
    // The first mark among a part's spans, if any.
    std::optional<PartMark> MarkOf(const std::vector<Span>& spans);
    // What a part of icons shows while it has none now (no target, no weather yet); nothing for a part of text.
    PartMark DefaultMark(WidgetPart part);

    // Which of a line's spans is cut when the line is too long: the one with the most letters, a dim one only when no
    // other has any; the size when none has any.
    size_t WidestTextSpan(const std::vector<Span>& line);
}

#pragma once

#include <array>
#include <cstdint>

namespace headsup
{
    enum class BarTheme : uint8_t
    {
        PhoenixGold,
        UmbraDark,
        PhoenixRed,
        Aurora,  // glass, and a line of light under the strip
        Custom,  // one of the player's own, Settings::customPalette, saved in the themes folder
        Vanilla, // the game's own window art, of the Window Type the game is set to, and colors from it
    };
    constexpr int kBarThemeCount   = static_cast<int>(BarTheme::Vanilla) + 1;
    constexpr int kFixedThemeCount = static_cast<int>(BarTheme::Custom); // those with a palette of their own

    // Where the panel shows separators, a bit each: between neighbouring widgets, and at its two ends.
    enum class Separator : uint8_t { Between, EndCaps };
    constexpr int kSeparatorCount = static_cast<int>(Separator::EndCaps) + 1;
    constexpr uint8_t SeparatorBit(Separator part) { return static_cast<uint8_t>(1u << static_cast<int>(part)); }
    constexpr uint8_t kAllSeparators = (1u << kSeparatorCount) - 1;

    // Which side of the screen the help bar keeps to, and where its text sits in a bar wider than it.
    enum class HelpAnchor : uint8_t
    {
        Left,
        Center,
        Right,
    };
    constexpr int kHelpAnchorCount = static_cast<int>(HelpAnchor::Right) + 1;

    // Tables by BarWidget (settings keys, menu labels, defaults) hold one entry each; kDefaultLeft, kDefaultCenter and
    // kDefaultRight in bar_placement.cpp give the bar's first order; the player's is Settings::barSections.
    enum class BarWidget : uint8_t
    {
        Zone,
        Weather,
        Target,
        Currency,
        Bags,
        EarthTime,
        GameHelp,
        Performance,
        Job,
        Party,
        StatusTimers,
        World,
        Settings, // the cog alone, which opens the settings window: no popup, settings or copies
    };
    constexpr int kBarWidgetCount = static_cast<int>(BarWidget::Settings) + 1;
    // Which widgets a new settings file shows, by BarWidget. to_array takes its size from the list, so a widget added
    // to BarWidget without a default here fails the assert.
    constexpr auto kBarWidgetDefaults =
        std::to_array<bool>({true, true, true, true, true, true, true, true, true, false, false, false, false});
    static_assert(kBarWidgetDefaults.size() == kBarWidgetCount);

    // What the Performance widget shows on the panel, a bit each, in this order on its line.
    enum class PerfPart : uint8_t { Fps, Sent, Received, Percent, Memory, Messages };
    constexpr int kPerfPartCount = static_cast<int>(PerfPart::Messages) + 1;
    constexpr uint8_t PerfBit(PerfPart part) { return static_cast<uint8_t>(1u << static_cast<int>(part)); }
    constexpr uint8_t kDefaultPerfParts = PerfBit(PerfPart::Sent) | PerfBit(PerfPart::Received) |
                                          PerfBit(PerfPart::Percent) | PerfBit(PerfPart::Messages);
    constexpr int kContainerCount = 17; // Ashita's containers 0 (inventory) to 16 (wardrobe 8), the bags a widget can show

    // A widget on the panel: which, and which of its copies. Copy 0 is the one every settings file has; the others are
    // there while the player keeps them on the panel.
    constexpr int kMostCopies = 4; // of one widget
    struct PanelWidget
    {
        BarWidget kind = BarWidget::Zone;
        int copy       = 0;
        constexpr PanelWidget() = default;
        constexpr PanelWidget(BarWidget widget, int number = 0) // a widget alone is its first copy
            : kind(widget), copy(number)
        {}
        bool operator==(const PanelWidget&) const = default;
    };
    // Tables by PanelWidget hold a slot for every copy of every widget.
    constexpr int kPanelSlots = kBarWidgetCount * kMostCopies;
    constexpr int SlotOf(PanelWidget widget) { return static_cast<int>(widget.kind) * kMostCopies + widget.copy; }
    constexpr PanelWidget WidgetInSlot(int slot) { return PanelWidget{static_cast<BarWidget>(slot / kMostCopies), slot % kMostCopies}; }
    // The target has one frame the bar gives way around, and game help a bar of its own: neither has copies.
    constexpr bool HasCopies(BarWidget widget)
    {
        return widget != BarWidget::Target && widget != BarWidget::GameHelp && widget != BarWidget::Settings;
    }
    // Game help's text is the help bar's, placed by its Anchor; the Settings widget is its cog alone.
    constexpr bool Aligns(BarWidget widget) { return widget != BarWidget::GameHelp && widget != BarWidget::Settings; }

    // What an EXP text says: what is earned of the level, what is left to the next, or the share earned as a percent;
    // the EXP bar's can say the jobs too, as Job Text writes them.
    enum class ExpText : uint8_t
    {
        Earned,
        ToNext,
        Percent,
        Jobs,
    };
    constexpr int kExpTextCount = static_cast<int>(ExpText::Jobs) + 1;
    // How the Job & EXP widget's Job Info says the jobs: "WAR 75 / NIN 37", "WAR 75" or "75 WAR / NIN".
    enum class JobText : uint8_t
    {
        MainSub,
        Main,
        LevelFirst,
    };
    constexpr int kJobTextCount = static_cast<int>(JobText::LevelFirst) + 1;
    // How the Status Timers widget shows a pinned status: its name, its icon, or its icon then its name.
    enum class StatusDisplay : uint8_t
    {
        Name,
        Icon,
        Both,
    };
    constexpr int kStatusDisplayCount = static_cast<int>(StatusDisplay::Both) + 1;
    constexpr int kMinExpBarWidth = 40, kMaxExpBarWidth = 400, kDefaultExpBarWidth = 100;  // pixels
    constexpr int kMinExpBarHeight = 2, kMaxExpBarHeight = 16, kDefaultExpBarHeight = 6;   // pixels

    constexpr int kMinBarHeight = 24, kMaxBarHeight = 64; // pixels
    // A bar or popup text size setting's leftmost position, Auto: the size follows the bar height. Set sizes run from
    // the next pixel, the fonts' readable floor and the bar's smallest detail line.
    constexpr int kAutoBarTextSize = 8, kMaxBarTextSize = 32; // pixels
    // Use Custom Icons: when the Forecast widget shows HeadsUp's own weather pictures in place of the game's element
    // icons.
    enum class CustomIcons : uint8_t { Never, NoVanilla, Always };
    constexpr int kCustomIconsCount = static_cast<int>(CustomIcons::Always) + 1;
    // Where the player dragged a pinned pop-out, kept by its edges nearest the screen's sides: its left edge from the
    // screen's left or its right edge from the right, and its top from the top or its bottom from the bottom. It grows
    // away from those edges as its content does.
    struct PopupSpot
    {
        int x = 0, y = 0;
        bool fromRight = false, fromBottom = false;
        bool operator==(const PopupSpot&) const = default;
    };
    // Where a widget's icon goes: nowhere, before its text or after it.
    enum class IconSide : uint8_t { Hide, Left, Right };
    constexpr int kIconSideCount = static_cast<int>(IconSide::Right) + 1;
    // Which side of its box a widget's content and lines go against: its section's, or one of its own.
    enum class WidgetAlign : uint8_t { Location, Left, Center, Right };
    constexpr int kWidgetAlignCount = static_cast<int>(WidgetAlign::Right) + 1;

    // Font Size Ratio: the Main line's size over the Secondary's. Its slider has 1 at its middle, steps of
    // kFontRatioStepBelow under it and kFontRatioStepAbove over it.
    constexpr float kMinFontRatio = 0.2f, kMaxFontRatio = 2.0f, kDefaultFontRatio = 1.2f;
    constexpr int kFontRatioSteps = 20; // a side
    constexpr float kFontRatioStepBelow = (1.0f - kMinFontRatio) / kFontRatioSteps;
    constexpr float kFontRatioStepAbove = (kMaxFontRatio - 1.0f) / kFontRatioSteps;
    // Its slider's positions, 0 to 2 * kFontRatioSteps: the ratio at one, and the nearest to a ratio.
    constexpr float FontRatioAt(int position)
    {
        return position <= kFontRatioSteps ? kMinFontRatio + kFontRatioStepBelow * static_cast<float>(position)
                                           : 1.0f + kFontRatioStepAbove * static_cast<float>(position - kFontRatioSteps);
    }
    constexpr int FontRatioPosition(float ratio)
    {
        const float steps = ratio <= 1.0f ? (ratio - kMinFontRatio) / kFontRatioStepBelow
                                          : static_cast<float>(kFontRatioSteps) + (ratio - 1.0f) / kFontRatioStepAbove;
        const int position = static_cast<int>(steps + 0.5f);
        return position < 0 ? 0 : position > 2 * kFontRatioSteps ? 2 * kFontRatioSteps : position;
    }
    // A pop-out's Text Scale, Width and Height, times its own sizes.
    constexpr float kMinPopupScale = 0.2f, kMaxPopupScale = 2.0f, kDefaultPopupScale = 1.0f;
    constexpr float kMinPopupBoxScale = kDefaultPopupScale; // Width and Height: never smaller than a pop-out's own
    // Icon Scale: a widget's icons, in percent of the most its widget holds.
    constexpr int kMinIconScale = 20, kMaxIconScale = 100, kDefaultIconScale = 65;
}

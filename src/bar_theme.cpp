#include "bar_theme.h"

#include "ini_text.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <vector>

namespace headsup
{
    namespace
    {
        constexpr const char* kThemeSection = "theme";
        constexpr const char* kShapeKey = "Shape", *kBorderKey = "Border", *kBackgroundKey = "Background";
        constexpr const char* kWindowTypeKey = "WindowType", *kWindowBorderKey = "WindowBorder", *kSeparatorKey = "Separator";
        constexpr const char* kTrueText = "true", *kFalseText = "false";

        template <typename Enum>
        void Named(std::optional<std::string> text, const char* const* names, int count, Enum& value)
        {
            for (int i = 0; text && i < count; ++i)
                if (*text == names[i]) value = static_cast<Enum>(i);
        }
        constexpr int kHex = 16;

        std::optional<int> NumberIn(const std::optional<std::string>& text, int base)
        {
            int value = 0;
            if (!text || text->empty()) return std::nullopt;
            const char* end = text->data() + text->size();
            const auto [at, problem] = std::from_chars(text->data(), end, value, base);
            return problem == std::errc{} && at == end ? std::optional<int>(value) : std::nullopt;
        }

        // BarMetrics' shares of H; from heading on, of the H the popup's text size stands for.
        constexpr float kTitleShare = 0.40f, kPopupTextShare = 0.39f;
        constexpr float kPaddingShare = 0.33f, kGapShare = 0.17f, kCapShare = 0.64f;
        constexpr float kDiamondShare = 0.22f, kPillHeightShare = 0.80f, kPillRadiusShare = 0.17f, kBelowShare = 0.28f;
        constexpr float kHeadingShare = 0.33f, kPopupPaddingShare = 0.36f, kRowGapShare = 0.11f;
        constexpr float kOrnateRadiusShare = 0.11f, kPillsRadiusShare = 0.28f;

        // Alpha is the first byte: 0x1A is 10%, 0x33 20%, 0x59 35%, 0x80 50% and 0xF7 97%.
        constexpr BarPalette kPalettes[kFixedThemeCount] = {
            {.shape = BarShape::Ornate, .stripTop = 0xFF2A2219, .stripMiddle = 0xFF15110C, .stripBottom = 0xFF0B0907,
                .bottomLine = 0xFFC9A24A, .underLine = 0xFF3D2F14, .below = 0x59C9A24A, .accent = 0xFFC9A24A, .accentEnd = 0xFFEFE3C6,
                .pill = 0, .pillBorder = 0, .title = 0xFFEFE3C6, .subtitle = 0xFFA8936D, .divider = 0xFF8A6D2C,
                .panelTop = 0xF71C160F, .panelBottom = 0xF70C0A08, .panelBorder = 0xFFC9A24A, .panelOuter = 0xFF3D2F14,
                .text = 0xFFE4D8BD, .heading = 0xFFC9A24A, .tagFill = 0, .tagBorder = 0xFF6B5422, .tagText = 0xFFD8C49A,
                .highlight = 0x1AC9A24A},
            {.shape = BarShape::Pills, .stripTop = 0xFF2B2B30, .stripMiddle = 0xFF212125, .stripBottom = 0xFF17171A,
                .bottomLine = 0xFF000000, .underLine = 0, .below = 0x80000000, .accent = 0xFF5B8DEF, .accentEnd = 0xFF9CC2FF,
                .pill = 0xFF222226, .pillBorder = 0xFF3A3A42, .title = 0xFFE9E9EE, .subtitle = 0xFF8E8E99, .divider = 0,
                .panelTop = 0xFF1D1D21, .panelBottom = 0xFF1D1D21, .panelBorder = 0xFF3A3A42, .panelOuter = 0,
                .text = 0xFFDEDEE5, .heading = 0xFF8E8E99, .tagFill = 0xFF2C2C33, .tagBorder = 0, .tagText = 0xFFC5C5D0,
                .highlight = 0xFF2A2A30},
            {.shape = BarShape::Ornate, .stripTop = 0xFF2B1A1A, .stripMiddle = 0xFF180E0E, .stripBottom = 0xFF0F0909,
                .bottomLine = 0xFFC55151, .underLine = 0xFF3A1B1B, .below = 0x59C55151, .accent = 0xFFC55151, .accentEnd = 0xFFE06F5E,
                .pill = 0, .pillBorder = 0, .title = 0xFFFFF8F8, .subtitle = 0xFFD2ABAB, .divider = 0xFF8A4545,
                .panelTop = 0xF7291C1C, .panelBottom = 0xF7180E0E, .panelBorder = 0xFFC55151, .panelOuter = 0xFF3A1B1B,
                .text = 0xFFF3E6E6, .heading = 0xFFD2ABAB, .tagFill = 0, .tagBorder = 0xFF6E3434, .tagText = 0xFFE8C9C9,
                .highlight = 0x1AC55151},
            {.shape = BarShape::Glass, .stripTop = 0xE6131C2B, .stripMiddle = 0xE60E1522, .stripBottom = 0xE60A0F18,
                .bottomLine = 0xFF1B2636, .underLine = 0, .below = 0x595EE6FF, .accent = 0xFF5EE6FF, .accentEnd = 0xFFB48CFF,
                .pill = 0x1AFFFFFF, .pillBorder = 0x24B8D8FF, .title = 0xFFEAF2FF, .subtitle = 0xFF8FA3BF, .divider = 0,
                .panelTop = 0xF2131C2B, .panelBottom = 0xF20A0F18, .panelBorder = 0x595EE6FF, .panelOuter = 0,
                .text = 0xFFE3ECFA, .heading = 0xFF5EE6FF, .tagFill = 0x265EE6FF, .tagBorder = 0, .tagText = 0xFFCFE9FF,
                .highlight = 0x335EE6FF},
        };
    }

    const BarPalette& PaletteOf(BarTheme theme)
    {
        static const BarPalette vanilla = [] {
            BarPalette p = kPalettes[static_cast<int>(BarTheme::PhoenixGold)];
            p.shape      = BarShape::Window;
            p.background = BarBackground::Window;
            return p;
        }();
        if (theme == BarTheme::Vanilla) return vanilla;
        return kPalettes[std::clamp(static_cast<int>(theme), 0, kFixedThemeCount - 1)];
    }

    BarMetrics MetricsFor(int barHeight, BarShape shape, float fontRatio, float popupScale, int iconScale)
    {
        const float h = static_cast<float>(std::clamp(barHeight, kMinBarHeight, kMaxBarHeight));
        // The pop-out's sizes stand for the height whose share its text is, so the whole pop-out scales together.
        const float popupText = kPopupTextPixels * popupScale, u = popupText / kPopupTextShare;
        const float title     = kTitleShare * h;
        const float pill      = kPillHeightShare * h;
        return BarMetrics{.height = h, .title = title, .subtitle = title / fontRatio,
            .icon = static_cast<float>(iconScale) / 100.0f * (std::round(pill) - 2.0f * kPillBorder), .padding = kPaddingShare * h,
            .gap = kGapShare * h, .cap = kCapShare * h, .diamond = kDiamondShare * h, .pillHeight = pill,
            .pillRadius = kPillRadiusShare * h, .below = kBelowShare * h, .popupText = popupText,
            .popupTitle = kPopupTitlePixels * popupScale, .heading = kHeadingShare * u, .popupPadding = kPopupPaddingShare * u,
            .rowGap = kRowGapShare * u,
            .panelRadius = (shape == BarShape::Ornate ? kOrnateRadiusShare : shape == BarShape::Window ? 0.0f : kPillsRadiusShare) * u};
    }

    const char* TitleFont(BarShape shape) { return shape == BarShape::Ornate ? kMarcellusSC : kDetailFont; }

    std::string ThemeFileText(const BarPalette& palette)
    {
        std::vector<KeyText> values;
        for (const PaletteColor& color : kPaletteColors)
        {
            char hex[16];
            std::snprintf(hex, sizeof(hex), "%08x", palette.*color.field);
            values.push_back({color.name, hex});
        }
        values.push_back({kShapeKey, kBarShapeNames[static_cast<int>(palette.shape)]});
        values.push_back({kBorderKey, std::to_string(palette.border)});
        values.push_back({kBackgroundKey, kBarBackgroundNames[static_cast<int>(palette.background)]});
        values.push_back({kWindowTypeKey, std::to_string(palette.windowType)});
        values.push_back({kWindowBorderKey, palette.windowBorder ? kTrueText : kFalseText});
        values.push_back({kSeparatorKey, kDashMarkNames[static_cast<int>(palette.separator)]});
        return WithIniValues("", kThemeSection, values);
    }

    std::optional<BarPalette> PaletteFromThemeFile(std::string_view file)
    {
        BarPalette palette = PaletteOf(BarTheme::PhoenixGold);
        bool any           = false;
        for (const PaletteColor& color : kPaletteColors)
            if (const std::optional<std::string> text = IniValue(file, kThemeSection, color.name))
            {
                uint32_t value = 0;
                const char* end = text->data() + text->size();
                const auto [at, problem] = std::from_chars(text->data(), end, value, kHex);
                if (problem != std::errc{} || at != end || text->empty()) continue;
                palette.*color.field = value;
                any                  = true;
            }
        Named(IniValue(file, kThemeSection, kShapeKey), kBarShapeNames, kBarShapeCount, palette.shape);
        Named(IniValue(file, kThemeSection, kBackgroundKey), kBarBackgroundNames, kBarBackgroundCount, palette.background);
        Named(IniValue(file, kThemeSection, kSeparatorKey), kDashMarkNames, kDashMarkCount, palette.separator);
        if (const std::optional<int> border = NumberIn(IniValue(file, kThemeSection, kBorderKey), 10))
            palette.border = std::clamp(*border, 0, kMaxStripBorder);
        if (const std::optional<int> type = NumberIn(IniValue(file, kThemeSection, kWindowTypeKey), 10))
            palette.windowType = std::clamp(*type, kFollowGameWindow, kWindowTypes);
        if (const std::optional<std::string> frame = IniValue(file, kThemeSection, kWindowBorderKey))
            palette.windowBorder = *frame != kFalseText;
        return any ? std::optional<BarPalette>(palette) : std::nullopt;
    }
}

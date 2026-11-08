#include "bar_look.h"

#include "argb.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kNeutralColor = 0xFF8D8D8D;
        // A window in a panel's colors: the page's alpha, each step of a card, a control and a hovered one toward the
        // text, the trays toward black, a label's and a muted word's share of the way to the subtitle and the page,
        // and the alphas of the edges and the soft accent, as the Phoenix window has them.
        constexpr uint32_t kMenuPageAlpha = 0xF7, kMenuEdgeAlpha = 0x24, kMenuRaisedAlpha = 0x3D, kMenuSoftAlpha = 0x47;
        constexpr float kCardStep = 0.07f, kInsetShade = 0.4f, kLabelShare = 0.3f, kMutedShare = 0.4f;
        constexpr float kFrameRadius     = 3.0f;  // the ornate target frame and hover tint
        constexpr int kLitTarget         = 2;     // a hovered ornate target's tint, in hover tints' alpha
        constexpr float kHelpPadShare    = 0.5f;   // of the gap: clear above and below the help bar's row
        constexpr int kOuterGap          = 2;          // an ornate popup's outer border
        constexpr uint32_t kCardFill     = 0x12FFFFFF; // a popup's section card: white over the panel, faintly
        constexpr uint32_t kCardEdge     = 0x1EFFFFFF;
        constexpr float kCardRadius      = 6.0f;

        constexpr uint32_t MenuColors::*kMenuColorFields[] = {&MenuColors::background, &MenuColors::card,
            &MenuColors::control, &MenuColors::hover, &MenuColors::inset, &MenuColors::border, &MenuColors::raisedEdge,
            &MenuColors::text, &MenuColors::heading, &MenuColors::label, &MenuColors::muted, &MenuColors::accent,
            &MenuColors::accentHover, &MenuColors::accentSoft, &MenuColors::accentEnd};

        // The built-in theme in use, or the one a custom theme was copied from.
        BarTheme BuiltInOf(const Settings& settings)
        {
            return settings.barTheme == BarTheme::Custom ? settings.customPalette.parent : settings.barTheme;
        }

        // A corner's radius as set, else the theme's own, else its shape's.
        float Rounded(int setting, const BarPalette& palette, float shape)
        {
            if (setting != kThemeRounding) return static_cast<float>(setting);
            return palette.rounding != kShapeRounding ? static_cast<float>(palette.rounding) : shape;
        }

        const char* Interned(const std::string& name)
        {
            static std::set<std::string> names; // a set's names never move
            return names.insert(name).first->c_str();
        }

        uint32_t AlphaTimes(uint32_t argb, int times)
        {
            const int alpha = std::min(0xFF, Channel(argb, kAlphaShift) * times);
            return (argb & ~kAlphaMask) | static_cast<uint32_t>(alpha) << kAlphaShift;
        }
    }

    uint32_t BarLook::BarColor(const Span& span) const
    {
        return span.tone == Tone::Normal ? palette.title : PopupColor(span);
    }

    uint32_t BarLook::PopupColor(const Span& span) const
    {
        switch (span.tone)
        {
        case Tone::Normal: return palette.text;
        case Tone::Dim: return palette.subtitle;
        case Tone::Heading: return palette.heading;
        case Tone::Tag: return palette.tagText;
        case Tone::Neutral: return kNeutralColor;
        case Tone::Weak: return ToArgb(settings.weakBadgeColor);
        case Tone::Resist: return ToArgb(settings.resistBadgeColor);
        case Tone::Immune: return ToArgb(settings.immuneBadgeColor);
        case Tone::Own: return span.argb;
        }
        return palette.text;
    }

    const BarPalette& PaletteFor(const Settings& settings, const GameSkinLook* skin)
    {
        return settings.barTheme == BarTheme::Vanilla && skin != nullptr ? skin->palette : OwnPalette(settings);
    }

    bool UsesGameArt(const Settings& settings) { return OwnPalette(settings).background == BarBackground::Window; }

    MenuTheme MenuThemeFor(const Settings& settings, const GameSkinLook* skin)
    {
        if (BuiltInOf(settings) == BarTheme::Aurora) return MenuTheme::Aurora;
        if (skin != nullptr && UsesGameArt(settings)) return MenuTheme::GameWindow;
        return MenuTheme::Phoenix;
    }

    MenuColors MenuColorsFor(const Settings& settings, const GameSkinLook* skin)
    {
        MenuColors own;
        switch (MenuThemeFor(settings, skin))
        {
        case MenuTheme::Aurora: own = kAuroraMenuColors; break;
        case MenuTheme::GameWindow: return skin->menu;
        case MenuTheme::Phoenix:
            if (BuiltInOf(settings) != BarTheme::PhoenixRed) return MenuColorsFrom(PaletteFor(settings, skin));
            own = kPhoenixMenuColors;
            break;
        }
        if (settings.barTheme != BarTheme::Custom) return own;
        // A copy keeps the window its theme has, but for each color its own palette's changes would make.
        const MenuColors from = MenuColorsFrom(PaletteOf(settings.customPalette.parent)),
                         now  = MenuColorsFrom(PaletteFor(settings, skin));
        for (uint32_t MenuColors::* const color : kMenuColorFields)
            if (now.*color != from.*color) own.*color = now.*color;
        return own;
    }

    MenuColors MenuColorsFrom(const BarPalette& p)
    {
        const uint32_t page = p.panelBottom | kAlphaMask;
        const auto toward = [](uint32_t from, uint32_t to, float share) {
            const auto channel = [&](int shift) {
                const float a = Channel(from, shift), b = Channel(to, shift);
                return static_cast<uint8_t>(std::lround(a + (b - a) * share));
            };
            return Opaque(channel(kRedShift), channel(kGreenShift), channel(kBlueShift));
        };
        return MenuColors{WithAlpha(page, kMenuPageAlpha), toward(page, p.text, kCardStep), toward(page, p.text, 2.0f * kCardStep),
            toward(page, p.text, 3.0f * kCardStep), toward(page, kBlack, kInsetShade), WithAlpha(p.subtitle, kMenuEdgeAlpha),
            WithAlpha(p.subtitle, kMenuRaisedAlpha), p.text, p.title, toward(p.text, p.subtitle, kLabelShare),
            toward(p.subtitle, page, kMutedShare), p.accent, p.accentEnd, WithAlpha(p.accent, kMenuSoftAlpha), p.accentEnd};
    }

    SkinFrame FrameAsSet(const Settings& settings, SkinFrame frame)
    {
        frame.width = OwnPalette(settings).border;
        if (frame.width == 0) frame.edge = 0;
        return frame;
    }

    float WidgetPadding(const BarMetrics& metrics, const Settings& settings)
    {
        return settings.barWidgetPadding == kThemePadding ? metrics.padding : static_cast<float>(settings.barWidgetPadding);
    }

    bool FlatPill(const BarLook& look, const PanelGroups& panel, PanelWidget head)
    {
        return !look.ornate && !look.settings.widgetBackground &&
               std::ranges::none_of(Members(panel, head), [&](PanelWidget member) { return look.BoxOf(member.kind, false).has_value(); });
    }

    const char* BarTitleFont(const Settings& settings, BarShape shape)
    {
        return settings.barTitleFont.empty() ? TitleFont(shape) : Interned(settings.barTitleFont);
    }

    const char* BarTextFont(const Settings& settings)
    {
        return settings.barTextFont.empty() ? kDetailFont : Interned(settings.barTextFont);
    }

    const char* FishBarFont(const Settings& settings)
    {
        return settings.fishBarFont.empty() ? BarTextFont(settings) : Interned(settings.fishBarFont);
    }

    TextFace BarLook::TitleFace(const WidgetContent& content) const
    {
        const auto size = static_cast<float>(content.subtitle.empty() ? lines.alone : lines.title);
        return TextFace{content.figures ? detailFont : titleFont, size};
    }

    TextFace BarLook::DetailFace() const { return TextFace{detailFont, static_cast<float>(lines.detail)}; }

    TextFace BarLook::FaceOf(PopupFont font) const
    {
        switch (font)
        {
        case PopupFont::Title: return TextFace{titleFont, metrics.popupTitle};
        case PopupFont::Heading:
        case PopupFont::Pin: return TextFace{titleFont, metrics.heading};
        case PopupFont::Text: break;
        }
        return TextFace{detailFont, metrics.popupText};
    }

    float BarLook::Leading(const WidgetContent& content) const
    {
        const size_t icons = content.icons.size() + (content.picture != GamePicture::None ? 1 : 0);
        return static_cast<float>(icons) * (metrics.icon + metrics.gap) +
               (content.cog ? metrics.icon : 0.0f); // alone, with no text to keep a gap from
    }

    std::optional<BoxStyle> BarLook::BoxOf(BarWidget widget, bool lit) const
    {
        const bool isTarget = widget == BarWidget::Target;
        if (ornate)
        {
            if (!isTarget && !lit) return std::nullopt;
            // The target's frame always has the tint, so a hovered one has more of it.
            const uint32_t fill = isTarget && lit ? AlphaTimes(palette.highlight, kLitTarget) : palette.highlight;
            return BoxStyle{.radius = Rounded(settings.widgetRounding, palette, kFrameRadius), .top = fill, .bottom = fill,
                .border = isTarget ? palette.accent : 0u};
        }
        const bool flat = widget == BarWidget::Zone || widget == BarWidget::EarthTime;
        if (flat && !lit) return std::nullopt;
        const uint32_t fill = lit ? palette.highlight : palette.pill;
        return BoxStyle{.radius = Rounded(settings.widgetRounding, palette, metrics.pillRadius), .top = fill, .bottom = fill,
            .border = isTarget || lit ? palette.accent : palette.pillBorder};
    }

    float MarkWidth(const Span& span, float size)
    {
        const float side = std::round(size);
        return span.picture != GamePicture::None ? kPictureGap + side
               : span.icon || span.coin || span.item != 0 || span.status ? kLineIconGap + side
               : span.bar                        ? static_cast<float>(span.bar->width)
                                                 : 0.0f;
    }

    float UnderLine(const BarPalette& palette) { return palette.underLine != 0 ? 1.0f : 0.0f; }
    float BarBottom(const BarPalette& palette, const BarMetrics& metrics) { return metrics.height + UnderLine(palette); }
    float HelpBarTop(const BarPalette& palette, const BarMetrics& metrics)
    {
        return BarBottom(palette, metrics) + std::round(metrics.gap);
    }
    float HelpBarLeft(const BarMetrics& metrics) { return std::round(metrics.gap); }
    float HelpBarPad(const BarMetrics& metrics) { return std::round(metrics.gap * kHelpPadShare); }
    int HelpTextPixels(int setting, int barTitle) { return setting > kAutoBarTextSize ? setting : barTitle; }
    namespace
    {
        float HelpPixels(const BarLook& look)
        {
            return static_cast<float>(HelpTextPixels(look.settings.helpBarTextSize, look.lines.title));
        }
    }
    TextFace HelpCaptionFace(const BarLook& look) { return TextFace{look.titleFont, HelpPixels(look)}; }
    TextFace HelpFace(const BarLook& look) { return TextFace{look.detailFont, HelpPixels(look)}; }
    float HelpBarHeight(int setting, float fitted)
    {
        return setting > kAutoHelpBarHeight ? static_cast<float>(setting) : fitted;
    }

    BoxStyle PopupPanel(const BarLook& look)
    {
        const BarPalette& p = look.palette;
        return BoxStyle{.radius = Rounded(look.settings.popupRounding, p, look.metrics.panelRadius), .top = p.panelTop,
            .bottom = p.panelBottom, .border = p.panelBorder, .outer = p.panelOuter, .outerGap = kOuterGap};
    }

    BoxStyle PopupCard(const BarLook& look)
    {
        const float shape = std::max(look.metrics.panelRadius, kCardRadius);
        return BoxStyle{.radius = Rounded(look.settings.sectionRounding, look.palette, shape), .top = kCardFill,
            .bottom = kCardFill, .border = kCardEdge};
    }

    BoxStyle PopupTag(const BarLook& look, float height)
    {
        const BarPalette& p = look.palette;
        return BoxStyle{.radius = height / 2.0f, .top = p.tagFill, .bottom = p.tagFill,
            .border = look.ornate ? p.tagBorder : 0u};
    }

    BoxStyle HelpBarPanel(const BarLook& look)
    {
        const BarPalette& p = look.palette;
        return BoxStyle{.radius = Rounded(look.settings.helpBarRounding, p, look.metrics.panelRadius), .top = p.panelTop,
            .bottom = p.panelBottom, .border = p.panelBorder};
    }

}

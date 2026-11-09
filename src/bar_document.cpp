#include "bar_document.h"

#include "icon_adjust.h"

#include "argb.h"
#include "bar_layout.h"
#include "bar_placement.h"
#include "markup.h"
#include "menu_document.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

namespace headsup
{
    namespace
    {
        constexpr std::string_view kIconKind = "icon", kStripKind = "strip",
                                   kResetKind = "reset", kCogKind = "cog", kGameKind = "game",
                                   kItemKind = "item", kStatusKind = "status";
        // A separator's, by DashMark.
        constexpr std::string_view kSeparatorKinds[kDashMarkCount] = {"dash", "dot", "square", "diamond"};
        // XIUI's party list draws a status's background 1.1 icons wide and 0.75 as wide as tall, from 0.3 of its width
        // above the icon for a buff and 0.1 for a debuff, the icon 0.1 / 1.5 of an icon in from its left. Here the icon
        // fills its mark's box as it does with no background, and the background's square picture reaches past it.
        constexpr float kBackWide = 1.1f, kBackAspect = 0.75f, kBuffDrop = 0.3f, kDebuffDrop = 0.1f, kBackInset = 0.1f / 1.5f;
        constexpr float kBackedIcon = kBackAspect / kBackWide; // the icon's side, of the background's square
        // RmlUi blurs a glow by a Gaussian of this share of its width; the bar's shadow is a CSS blur radius, twice
        // its Gaussian's.
        constexpr float kGlowSigmaPerWidth = 0.4f;
        constexpr float kSigmaPerRadius    = 0.5f;
        constexpr int kShadowDrop          = 1; // pixels down
        constexpr uint32_t kSocketFill     = 0x38000000; // a socket: black over its card
        constexpr uint32_t kSocketShade    = 0x59000000, kSocketLight = 0x14FFFFFF;
        constexpr uint32_t kLitTab         = 0x40; // the picked tab's fill: its accent at a quarter
        constexpr uint32_t kScrollTrack    = 0x1AFFFFFF, kScrollThumb = 0x73FFFFFF; // white, faint and less so
        constexpr float kSignArm = 0.5f, kSignBar = 0.12f; // a collapse chip's sign: its bars' length and width, of its side
        // The accent's alpha in the target's frame as it breathes, from and to, and in a lit title's glow.
        constexpr uint32_t kBreatheLow = 0x59, kBreatheHigh = 0xCC, kLitGlow = 0x80;
        // How a widget eases into its lit look.
        const std::string kEaseBackground = "background-color 0.2s cubic-out", kEaseBorder = "border-color 0.2s cubic-out";
        constexpr float kGlint             = 2.0f;       // the line of light at the foot of a glass strip, in pixels
        constexpr uint32_t kGlassTop = 0xF21B2740, kGlassFoot = 0xF20C1220; // a popup's panel, down its height
        constexpr uint32_t kGlassFrame    = 0x40;       // the accent's alpha in a popup's frame
        constexpr char kSeparator      = '/';
        // A strip's name: its kind, top, middle, bottom, last row, the row under and what fades below, then its border
        // and its rows.
        constexpr size_t kStripColors = 6, kStripParts = 1 + kStripColors + 3;
        constexpr int kHexBase         = 16;
        constexpr float kBoxBorder     = 1.0f; // a pill's or frame's, inside its edge
        // The widgets' content starts a pixel under the pill row's top in every shape, bordered or not.
        constexpr float kContentInset  = 1.0f;
        // And everything in it stands a pixel higher than its lines' places, where it reads as centered in the pill.
        constexpr float kContentLift   = 1.0f;
        const char* const kTextAligns[kAlignCount] = {"left", "center", "right"}; // by Align
        constexpr float kBarTextInset = 2.0f; // an EXP bar's text from its ends, in pixels

        std::string Hex(uint32_t value)
        {
            char text[16];
            std::snprintf(text, sizeof(text), "%08x", value);
            return text;
        }

        // The parts of a name after its prefix, between its separators.
        std::vector<std::string_view> Parts(std::string_view name)
        {
            std::vector<std::string_view> parts;
            while (true)
            {
                const size_t at = name.find(kSeparator);
                parts.push_back(name.substr(0, at));
                if (at == std::string_view::npos) break;
                name.remove_prefix(at + 1);
            }
            return parts;
        }

        std::optional<uint32_t> Number(std::string_view text, int base = 10)
        {
            uint32_t value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
            if (error != std::errc() || end != text.data() + text.size()) return std::nullopt;
            return value;
        }

        // A status's icon filling a square box of side at x, y in unit, each moved and sized by its own icon
        // adjustment. With Show Status Background its background is drawn under it, larger than the box.
        std::string StatusPictures(const StatusMark& mark, float x, float y, float side, const char* unit)
        {
            const auto at = [&](float left, float top, float size) {
                return "left:" + RcssNumber(x + left * side) + unit + ";top:" + RcssNumber(y + top * side) + unit +
                       ";width:" + RcssNumber(size * side) + unit + ";height:" + RcssNumber(size * side) + unit + ";";
            };
            const std::string icon = "<img class=\"p\" src=\"" + StatusSource(mark) + "\" style=\"" + at(0.0f, 0.0f, 1.0f) +
                                     AdjustRules(AdjustOf(mark)) + "\"/>";
            if (!mark.back) return icon;
            const Icon back   = IsDebuff(mark.id) ? Icon::DebuffBack : Icon::BuffBack;
            const float drop  = (IsDebuff(mark.id) ? kDebuffDrop : kBuffDrop) * kBackWide * kBackedIcon;
            const float inset = (1.0f - kBackAspect) / 2.0f + kBackInset * kBackedIcon;
            return "<img class=\"p\" src=\"" + IconSource(back) + "\" style=\"" +
                   at(-inset / kBackedIcon, -drop / kBackedIcon, 1.0f / kBackedIcon) + AdjustRules(AdjustOf(back)) + "\"/>" + icon;
        }

        // A span's mark, before its text.
        std::string Picture(const Span& span, const BarLook& look)
        {
            if (span.picture != GamePicture::None)
                return "<img class=\"line-icon pic\" src=\"" + GamePictureSource(span.picture) + "\"" +
                       AdjustStyle(AdjustOf(span.picture)) + "/>";
            if (span.icon || span.coin)
            {
                const Icon icon = span.coin ? Icon::Gil : *span.icon;
                return "<img class=\"line-icon\" src=\"" + IconSource(icon) + "\"" + AdjustStyle(AdjustOf(icon)) + "/>";
            }
            if (span.item != 0) return "<img class=\"line-icon\" src=\"" + ItemSource(span.item) + "\"/>";
            if (span.status) return "<span class=\"line-icon smark\">" + StatusPictures(*span.status, 0.0f, 0.0f, kPercentWhole, "%") + "</span>";
            if (span.bar)
            {
                const InlineBar& bar = *span.bar;
                const auto height    = static_cast<float>(bar.height);
                std::string rml = "<span class=\"ibar\" style=\"width:" + Px(static_cast<float>(bar.width)) +
                                  ";\"><span class=\"track\" style=\"height:" + Px(height) + ";margin-top:" +
                                  Px(-height / 2.0f) + ";" + (bar.back ? "background-color:" + RcssColor(*bar.back) + ";" : "") +
                                  "\"><span class=\"fill\" style=\"width: " +
                                  RcssNumber(std::clamp(bar.done, 0.0f, 1.0f) * kPercentWhole) + "%;" +
                                  (bar.fill ? "decorator:none;background-color:" + RcssColor(*bar.fill) + ";" : "") +
                                  "\"></span></span>";
                if (!bar.text.empty())
                    rml += "<span class=\"btext\" style=\"text-align:" + std::string(kTextAligns[static_cast<int>(bar.align)]) +
                           ";color:" + RcssColor(look.BarColor(Span{bar.text})) + ";\">" + EscapedRml(bar.text) + "</span>";
                return rml + "</span>";
            }
            return {};
        }

        std::string Spans(const std::vector<Span>& spans, const BarLook& look)
        {
            // RmlUi's parser drops a text of spaces alone, which the width was measured with: those spaces go on the end
            // of the text before them, or the start of the next.
            struct Run
            {
                std::string mark, text;
                uint32_t color;
                const Span* steady = nullptr;
            };
            std::vector<Run> runs;
            std::string carried;
            for (const Span& span : spans)
            {
                const bool spaces = !HasMark(span) && !span.text.empty() && span.text.find_first_not_of(' ') == std::string::npos;
                if (spaces && !runs.empty() && !runs.back().text.empty())
                    runs.back().text += span.text;
                else if (spaces)
                    carried += span.text;
                else
                    runs.push_back(Run{Picture(span, look), std::exchange(carried, {}) + span.text, look.BarColor(span),
                        span.room.empty() ? nullptr : &span});
            }
            std::string rml;
            for (const Run& run : runs)
            {
                rml += run.mark;
                if (run.steady != nullptr)
                {
                    // Its leading spaces are the room's own left, its trailing ones a margin: a text of spaces alone
                    // between two cells would not be drawn.
                    const Span& s = *run.steady;
                    const size_t first = std::min(s.text.find_first_not_of(' '), s.text.size());
                    const size_t last  = std::max(first, s.text.find_last_not_of(' ') + 1);
                    rml += "<span class=\"steady\" style=\"width:" + Px(s.box - s.trail) + ";margin-right:" + Px(s.trail) +
                           ";color:" + RcssColor(run.color) + ";\">";
                    for (const std::string_view piece : FigurePieces(std::string_view(s.text).substr(first, last - first)))
                        rml += IsDigit(piece.front())
                                   ? "<span class=\"fig\" style=\"width:" + Px(s.digit) + ";\">" + std::string(piece) + "</span>"
                                   : EscapedRml(piece);
                    rml += "</span>";
                }
                else if (!run.text.empty())
                    rml += "<span style=\"color:" + RcssColor(run.color) + ";\">" + EscapedRml(run.text) + "</span>";
            }
            return rml;
        }

        // A line of cells, each as wide as its column and the gap after it but the last.
        std::string Cells(const std::vector<Span>& spans, const BarLook& look, const std::vector<float>& columns)
        {
            std::string rml;
            for (size_t i = 0; i < spans.size() && i < columns.size(); ++i)
                rml += "<span class=\"cell\" style=\"width:" + Px(columns[i] + (i + 1 < spans.size() ? look.metrics.gap : 0.0f)) +
                       ";color:" + RcssColor(look.BarColor(spans[i])) + ";\">" + Picture(spans[i], look) + EscapedRml(spans[i].text) + "</span>";
            return rml;
        }

        std::string Line(const char* classes, const std::vector<Span>& spans, const BarLook& look,
            const std::vector<float>& columns = {})
        {
            if (!columns.empty() || std::ranges::none_of(spans, [](const Span& s) { return s.align.has_value(); }))
                return std::string("<div class=\"") + classes + "\"><span class=\"t\">" +
                       (columns.empty() ? Spans(spans, look) : Cells(spans, look, columns)) + "</span></div>";
            // Each run across the whole line on its own side, the line kept as wide as all of them by a hidden copy.
            std::string rml = std::string("<div class=\"") + classes + " runs\"><span class=\"t sizer\">" + Spans(spans, look) + "</span>";
            for (size_t start = 0, end = 0; start < spans.size(); start = end)
            {
                for (end = start + 1; end < spans.size() && !spans[end].align; ++end)
                {
                }
                std::vector<Span> run(spans.begin() + static_cast<std::ptrdiff_t>(start), spans.begin() + static_cast<std::ptrdiff_t>(end));
                run.front().text.erase(0, run.front().text.find_first_not_of(' ')); // its gap from the run before is the copy's
                const std::optional<Align> align = spans[start].align;
                rml += std::string("<div class=\"run\"") +
                       (align ? std::string(" style=\"text-align:") + kTextAligns[static_cast<int>(*align)] + ";\"" : "") +
                       "><span class=\"t\">" + Spans(run, look) + "</span></div>";
            }
            return rml + "</div>";
        }

        // A box's fill, edge and corners as BoxOf gives them; no box is a clear one, its room kept.
        std::string BoxRules(const std::optional<BoxStyle>& box)
        {
            if (!box) return "background-color:transparent;border-color:transparent;";
            const BoxStyle& style = *box;
            return "background-color:" + RcssColor(style.top) + ";border-color:" +
                   (style.border != 0 ? RcssColor(style.border) : std::string("transparent")) + ";border-radius:" +
                   Px(style.radius) + ";";
        }

        std::string FaceRules(const std::string& font, int pixels, int lineHeight)
        {
            return "font-family:\"" + font + "\";font-size:" + Px(static_cast<float>(pixels)) + ";line-height:" +
                   Px(static_cast<float>(lineHeight)) + ";";
        }

        std::string LineRules(const std::string& font, int pixels, const BarLinePlace& line, float rowTop)
        {
            return FaceRules(font, pixels, line.height) + "margin-top:" + Px(std::round(line.top) - rowTop) + ";" +
                   ShadowEffect(pixels);
        }

        // A box's place and size as an inline style: whole pixels, as the bar's boxes are drawn.
        std::string BoxAt(float x, float y, float width, float height)
        {
            return "left:" + Px(std::round(x)) + ";top:" + Px(std::round(y)) + ";width:" +
                   Px(static_cast<float>(WholePixels(width))) + ";height:" + Px(static_cast<float>(WholePixels(height))) + ";";
        }

        // width: set against the right of the box from x that wide, in whole pixels; 0 for text from x.
        std::string TextAt(const char* classes, float x, float y, uint32_t color, std::string_view text, float width = 0.0f)
        {
            const std::string box =
                width > 0.0f ? "width:" + Px(std::round(x + width) - std::round(x)) + ";text-align:right;" : std::string();
            return std::string("<div class=\"p ") + classes + "\" style=\"left:" + Px(std::round(x)) + ";top:" +
                   Px(std::round(y)) + ";" + box + "color:" + RcssColor(color) + ";\">" + EscapedRml(text) + "</div>";
        }

        // A panel's fill top to bottom, its edge and its corners.
        std::string PanelRules(const BoxStyle& style)
        {
            return "box-sizing:border-box;border:1px " + (style.border != 0 ? RcssColor(style.border) : std::string("transparent")) +
                   ";border-radius:" + Px(style.radius) + ";decorator:vertical-gradient(" + RcssColor(style.top) + " " +
                   RcssColor(style.bottom) + ");";
        }

        // A box of one fill, its edge and, when it has them, its corners.
        std::string FlatRules(const BoxStyle& style, bool corners)
        {
            return "box-sizing:border-box;border:1px " + (style.border != 0 ? RcssColor(style.border) : std::string("transparent")) +
                   ";" + (corners ? "border-radius:" + Px(style.radius) + ";" : std::string()) + "background-color:" +
                   RcssColor(style.top) + ";";
        }
    }

    std::string IconSource(Icon icon)
    {
        return std::string(kHostSource) + std::string(kIconKind) + kSeparator + std::to_string(static_cast<int>(icon));
    }

    std::string GamePictureSource(GamePicture picture)
    {
        return std::string(kHostSource) + std::string(kGameKind) + kSeparator + std::to_string(static_cast<int>(picture));
    }

    std::optional<Image> GamePictureImage(const std::optional<PlayOnlineIcons>& icons, const std::optional<NationCrests>& crests,
        std::string_view name)
    {
        const auto is = [&](GamePicture picture) { return name == GamePictureSource(picture); };
        const ColorPicture* picture = icons && is(GamePicture::PlayOnlineMail)       ? &icons->mail
                                      : icons && is(GamePicture::PlayOnlineMessenger) ? &icons->messenger
                                      : crests && is(GamePicture::SanDoriaCrest)      ? &crests->sanDoria
                                      : crests && is(GamePicture::BastokCrest)        ? &crests->bastok
                                      : crests && is(GamePicture::WindurstCrest)      ? &crests->windurst
                                      : crests && is(GamePicture::BeastmenCrest)      ? &crests->beastmen
                                      : crests && is(GamePicture::JeunoCrest)         ? &crests->jeuno
                                      : crests && is(GamePicture::AhtUrhganCrest)     ? &crests->ahtUrhgan
                                                                                      : nullptr;
        if (picture == nullptr) return std::nullopt;
        Image image{picture->width, picture->height, std::vector<uint32_t>(picture->bgra.size() / sizeof(uint32_t))};
        // The picture's bytes are B, G, R, A: an ARGB word's bytes in memory.
        std::memcpy(image.argb.data(), picture->bgra.data(), image.argb.size() * sizeof(uint32_t));
        return image;
    }

    std::string StripSource(const BarPalette& palette, int rows, int belowRows)
    {
        std::string name = std::string(kHostSource) + std::string(kStripKind);
        // The art's palette runs through its stops where its art is not read.
        const bool gradient = palette.background != BarBackground::Plain;
        const uint32_t top = palette.stripTop, middle = gradient ? palette.stripMiddle : top,
                       bottom = gradient ? palette.stripBottom : top;
        for (const uint32_t color : {top, middle, bottom, palette.bottomLine, palette.underLine, palette.below})
            name += kSeparator + Hex(color);
        return name + kSeparator + std::to_string(palette.border) + kSeparator + std::to_string(rows) + kSeparator +
               std::to_string(belowRows);
    }

    std::string SeparatorSource(DashMark mark, uint32_t argb, int pixels)
    {
        return std::string(kHostSource) + std::string(kSeparatorKinds[static_cast<int>(mark)]) + kSeparator + Hex(argb) +
               kSeparator + std::to_string(pixels);
    }

    std::string ItemSource(uint16_t item)
    {
        return std::string(kHostSource) + std::string(kItemKind) + kSeparator + std::to_string(item);
    }

    std::optional<uint16_t> ItemOfSource(std::string_view name)
    {
        if (name.substr(0, kHostSource.size()) != kHostSource) return std::nullopt;
        const std::vector<std::string_view> parts = Parts(name.substr(kHostSource.size()));
        if (parts.size() != 2 || parts[0] != kItemKind) return std::nullopt;
        const std::optional<uint32_t> item = Number(parts[1]);
        if (!item || *item == 0 || *item > UINT16_MAX) return std::nullopt;
        return static_cast<uint16_t>(*item);
    }

    std::string StatusSource(const StatusMark& mark)
    {
        return std::string(kHostSource) + std::string(kStatusKind) + kSeparator +
               (mark.pack == kVanillaStatusIcons ? std::string(kVanillaStatusName) : std::to_string(mark.pack)) + kSeparator +
               std::to_string(mark.id);
    }

    std::optional<StatusMark> StatusOfSource(std::string_view name)
    {
        if (name.substr(0, kHostSource.size()) != kHostSource) return std::nullopt;
        const std::vector<std::string_view> parts = Parts(name.substr(kHostSource.size()));
        if (parts.size() != 3 || parts[0] != kStatusKind) return std::nullopt;
        const std::optional<uint32_t> pack = Number(parts[1]), id = Number(parts[2]);
        if ((parts[1] != kVanillaStatusName && (!pack || *pack >= static_cast<uint32_t>(kStatusPackCount))) || !id ||
            *id > UINT16_MAX)
            return std::nullopt;
        return StatusMark{static_cast<uint16_t>(*id), pack ? static_cast<int>(*pack) : kVanillaStatusIcons};
    }

    std::optional<Image> StatusImage(std::string_view name, FunctionRef<std::optional<Image>(uint16_t)> game)
    {
        const std::optional<StatusMark> status = StatusOfSource(name);
        if (!status) return std::nullopt;
        if (std::optional<Image> own = PackIcon(status->pack, status->id)) return own;
        if (std::optional<Image> games = game(status->id)) return games;
        return HostImage(IconSource(Icon::UnknownStatus));
    }

    std::string ResetSource(uint32_t argb, int pixels)
    {
        return std::string(kHostSource) + std::string(kResetKind) + kSeparator + Hex(argb) + kSeparator + std::to_string(pixels);
    }

    std::string CogSource(uint32_t argb, int pixels)
    {
        return std::string(kHostSource) + std::string(kCogKind) + kSeparator + Hex(argb) + kSeparator + std::to_string(pixels);
    }

    std::optional<Image> HostImage(std::string_view name)
    {
        if (name.substr(0, kHostSource.size()) != kHostSource) return std::nullopt;
        const std::vector<std::string_view> parts = Parts(name.substr(kHostSource.size()));
        const std::string_view kind               = parts[0];
        auto number = [&](size_t part, int base = 10) {
            return part < parts.size() ? Number(parts[part], base) : std::nullopt;
        };
        if (kind == kIconKind && parts.size() == 2)
        {
            const std::optional<uint32_t> icon = number(1);
            if (!icon || *icon >= static_cast<uint32_t>(kIconCount)) return std::nullopt;
            const IconBitmap& bitmap = IconImage(static_cast<Icon>(*icon));
            Image image{static_cast<int>(bitmap.width), static_cast<int>(bitmap.height), {}};
            image.argb.resize(static_cast<size_t>(bitmap.width) * bitmap.height);
            // The bitmap's bytes are B, G, R, A: an ARGB word's bytes in memory.
            std::memcpy(image.argb.data(), bitmap.bgra, image.argb.size() * sizeof(uint32_t));
            return image;
        }
        if (kind == kStripKind && parts.size() == kStripParts)
        {
            std::array<uint32_t, kStripColors> colors{};
            for (size_t i = 0; i < kStripColors; ++i)
                if (const std::optional<uint32_t> color = number(1 + i, kHexBase))
                    colors[i] = *color;
                else
                    return std::nullopt;
            const std::optional<uint32_t> border = number(1 + kStripColors), rows = number(2 + kStripColors),
                                          below = number(3 + kStripColors);
            if (!border || !rows || !below || *border > static_cast<uint32_t>(kMaxStripBorder)) return std::nullopt;
            return StripColumn(static_cast<int>(*rows), static_cast<int>(*below), colors[0], colors[1], colors[2], colors[3],
                colors[4], colors[5], static_cast<int>(*border));
        }
        const auto separator = std::ranges::find(kSeparatorKinds, kind);
        if ((separator != std::end(kSeparatorKinds) || kind == kResetKind || kind == kCogKind) && parts.size() == 3)
        {
            const std::optional<uint32_t> color = number(1, kHexBase), pixels = number(2);
            if (!color || !pixels) return std::nullopt;
            const int size = static_cast<int>(*pixels);
            if (separator != std::end(kSeparatorKinds))
                return SeparatorImage(static_cast<DashMark>(separator - std::begin(kSeparatorKinds)), size, *color);
            return kind == kResetKind ? ResetArrow(size, *color) : Cog(size, *color);
        }
        return std::nullopt;
    }

    std::string WidgetId(PanelWidget widget)
    {
        return std::string("w-") + BarWidgetName(widget.kind) + (widget.copy > 0 ? "-" + std::to_string(widget.copy + 1) : "");
    }
    std::string DividerId(int divider) { return "d-" + std::to_string(divider); }

    const char* SectionClass(BarSection section)
    {
        constexpr const char* kClasses[kBarSectionCount] = {"in-left", "in-center", "in-right"};
        return kClasses[static_cast<int>(section)];
    }

    uint32_t ExpBarBack(const BarPalette& palette)
    {
        constexpr uint32_t kGlassTrack = 0x1FFFFFFFu; // a glass of white
        return palette.shape == BarShape::Glass ? kGlassTrack : palette.divider;
    }

    const char* AlignClass(WidgetAlign align)
    {
        constexpr const char* kClasses[kWidgetAlignCount] = {"", "align-left", "align-center", "align-right"};
        return kClasses[static_cast<int>(align)];
    }

    WidgetSide SideOf(const BarLook& look, PanelWidget widget, float room, bool shared)
    {
        if (shared) return WidgetSide{0.0f, room};
        const bool plain    = look.ornate && widget.kind != BarWidget::Target;
        const bool diamonds = (look.settings.panelSeparators & SeparatorBit(Separator::Between)) != 0;
        const float margin  = plain && diamonds ? DiamondClearance(look.metrics) : 0.0f;
        return WidgetSide{margin, std::max(0.0f, room - (plain ? margin : kBoxBorder))};
    }

    namespace
    {
        // A color with another alpha.
        uint32_t Faded(uint32_t argb, uint32_t alpha) { return (argb & ~kAlphaMask) | alpha << kAlphaShift; }

        // The glass shape's own: a line of light under the strip, and widgets that show no pill until they are lit.
        std::string GlassBarRules(const BarLook& look)
        {
            const uint32_t cyan = look.palette.accent, violet = look.palette.accentEnd;
            const std::string c = RcssColor(cyan), v = RcssColor(violet);
            std::string css;
            css += ".glint{position:absolute;top:" + Px(look.metrics.height - kGlint) + ";height:" + Px(kGlint) + ";}";
            css += ".glint.a{left:0%;width:30%;decorator:horizontal-gradient(" + RcssColor(Faded(cyan, 0)) + " " + c + ");}";
            css += ".glint.b{left:30%;width:40%;decorator:horizontal-gradient(" + c + " " + v + ");}";
            css += ".glint.c{left:70%;width:30%;decorator:horizontal-gradient(" + v + " " + RcssColor(Faded(violet, 0)) + ");}";
            css += ".pills .widget{background-color:transparent;border-color:transparent;}";
            css += ".pills .widget.target,.pills .widget.backed{background-color:" + RcssColor(Faded(cyan, 0x14)) + ";}";
            css += ".pills .widget.lit{background-color:" + RcssColor(Faded(cyan, 0x2E)) + ";border-color:" +
                   RcssColor(Faded(cyan, 0xB3)) + ";}";
            return css;
        }
    }

    namespace
    {
        // The rules of a widget's lines in its fonts and on its rows, each selector after scope.
        std::string TextRules(const std::string& scope, const char* titleFont, const char* detailFont, const TextRow& lines,
            const BarTextPlace& place, float rowTop)
        {
            auto square = [](float size) { return "width:" + Px(size) + ";height:" + Px(size) + ";"; };
            std::string css;
            css += scope + ".title{" + LineRules(titleFont, lines.title, place.title, rowTop) + "}";
            css += scope + ".title.alone{" + LineRules(titleFont, lines.alone, place.alone, rowTop) + "}";
            css += scope + ".title.figures{" + LineRules(detailFont, lines.title, place.figures, rowTop) + "}";
            css += scope + ".title.figures.alone{" + LineRules(detailFont, lines.alone, place.figuresAlone, rowTop) + "}";
            css += scope + ".title .line-icon{" + square(static_cast<float>(lines.title)) + "margin-top:" + Px(place.title.iconDrop) + ";}";
            css += scope + ".title.alone .line-icon{" + square(static_cast<float>(lines.alone)) + "margin-top:" + Px(place.alone.iconDrop) + ";}";
            css += scope + ".title.figures .line-icon{margin-top:" + Px(place.figures.iconDrop) + ";}";
            css += scope + ".title.figures.alone .line-icon{margin-top:" + Px(place.figuresAlone.iconDrop) + ";}";
            const float underTitle   = std::round(place.title.top) + static_cast<float>(place.title.height);
            const float underFigures = std::round(place.figures.top) + static_cast<float>(place.figures.height);
            css += scope + ".detail{" + LineRules(detailFont, lines.detail, place.detail, underTitle) + "}";
            css += scope + ".detail.fig{margin-top:" + Px(std::round(place.detail.top) - underFigures) + ";}";
            css += scope + ".detail .line-icon{" + square(static_cast<float>(lines.detail)) + "margin-top:" +
                   Px(place.detail.iconDrop) + ";}";
            // A widget of cells draws its top line at the detail size too, so a picture is as big on either line.
            const float smaller = std::round(static_cast<float>(lines.title - lines.detail) / 2.0f);
            css += scope + ".title.cells{font-size:" + Px(static_cast<float>(lines.detail)) + ";}";
            css += scope + ".title.cells .line-icon{" + square(static_cast<float>(lines.detail)) + "margin-top:" +
                   Px(place.title.iconDrop + smaller) + ";}";
            css += scope + ".title.figures.cells .line-icon{margin-top:" + Px(place.figures.iconDrop + smaller) + ";}";
            // An EXP bar's middle on its line's capitals.
            for (const auto& [line, at] : {std::pair{".title", &place.title}, std::pair{".title.alone", &place.alone},
                     std::pair{".title.figures", &place.figures}, std::pair{".title.figures.alone", &place.figuresAlone},
                     std::pair{".detail", &place.detail}})
                css += scope + line + " .track{top:" + Px(at->capsMiddle) + ";}";
            return css;
        }
    }

    std::string BarStyle(const BarLook& look, const BarTextPlace& place, const std::vector<WidgetText>& own)
    {
        const BarPalette& p  = look.palette;
        const BarMetrics& m  = look.metrics;
        const BarRow& row    = look.row;
        const TextRow& lines = look.lines;
        // The game's art frames the bar alone: the glow other themes fade out under the strip would hang its frame low.
        // With no glow the strip's image has no rows for it, and the element must not either, or the image stretches.
        const int stripRows = WholePixels(m.height), belowRows = look.skin || p.below == 0 ? 0 : WholePixels(m.below);
        const float stripHeight = static_cast<float>(stripRows + belowRows) + (p.underLine != 0 ? 1.0f : 0.0f);
        const float mid         = row.y + row.height / 2.0f;
        const float rowTop      = row.y + kContentInset; // where a widget's content starts
        const float clear       = DiamondClearance(m);
        const int divider       = WholePixels(m.diamond), cap = WholePixels(m.diamond * kCapScale);
        // Show Background is each widget's own: its backed class.
        auto boxOf = [&](BarWidget widget, bool lit) { return look.BoxOf(widget, lit, true); };
        auto square = [](float size) { return "width:" + Px(size) + ";height:" + Px(size) + ";"; };
        auto centered = [&](float size) { return Px(std::round(mid - size / 2.0f)); };
        const std::string shape = look.ornate ? ".ornate" : ".pills";

        std::string css;
        css += "body{display:block;width:100%;height:100%;font-family:\"" + std::string(look.detailFont) + "\";color:" +
               RcssColor(p.title) + ";}";
        css += "div{display:block;}";
        css += std::string(".") + kHiddenClass + "{display:none;}";
        css += std::string("#") + kStripId + "{position:absolute;left:0px;top:0px;width:100%;height:" + Px(stripHeight) + ";" +
               (look.skin ? SkinRules(*look.skin) : "decorator:image(\"" + StripSource(p, stripRows, belowRows) + "\");") + "}";
        css += ".cap{position:absolute;top:" + centered(static_cast<float>(cap)) + ";" + square(static_cast<float>(cap)) + "}";
        css += "#cap-left{left:" + Px(std::round(m.cap / 2.0f - static_cast<float>(cap) / 2.0f)) + ";}";
        css += ".divider{position:absolute;top:" + centered(static_cast<float>(divider)) + ";" +
               square(static_cast<float>(divider)) + "}";

        // Every widget takes the pill row. A bordered one keeps its border inside its room; a plain ornate one
        // keeps the divider's clear space either side as margin, so its tint ends short of the diamonds.
        css += ".widget{position:absolute;top:" + Px(row.y) + ";height:" + Px(row.height) +
               ";box-sizing:border-box;white-space:nowrap;border-width:" + Px(kBoxBorder) + ";padding:0px " +
               Px(m.padding - kBoxBorder) + ";" + BoxRules(std::nullopt) +
               "transition:" + kEaseBackground + "," + kEaseBorder + ";}";
        if (look.ornate)
        {
            css += ".ornate .widget{border-width:0px;margin:0px " + Px(clear) + ";padding:" + Px(kContentInset) + " " +
                   Px(m.padding - clear) + " 0px " + Px(m.padding - clear) + ";}";
            css += ".ornate .widget.backed,.ornate .widget.lit{" + BoxRules(boxOf(BarWidget::Zone, true)) + "}";
            css += ".ornate .widget.target{border-width:" + Px(kBoxBorder) + ";margin:0px;padding:0px " +
                   Px(m.padding - kBoxBorder) + ";" + BoxRules(boxOf(BarWidget::Target, false)) + "}";
            css += ".ornate .widget.target.lit{" + BoxRules(boxOf(BarWidget::Target, true)) + "}";
        }
        else
        {
            css += ".pills .widget{" + BoxRules(look.BoxOf(BarWidget::Weather, false, false)) + "}";
            css += ".pills .widget.backed{" + BoxRules(boxOf(BarWidget::Weather, false)) + "}";
            // Before the lit rule, which then gives it the accent as every lit pill has.
            css += ".pills .widget.target{" + BoxRules(boxOf(BarWidget::Target, false)) + "}";
            css += ".pills .widget.lit{" + BoxRules(boxOf(BarWidget::Weather, true)) + "}";
        }
        // A widget eases into its lit look (above) and its title glows there; the target's frame breathes from the
        // accent into what it runs into until it is lit.
        // RmlUi refuses a transition on a property an animation drives, once a border side, as the breathing comes back.
        css += ".widget.target{animation:2.4s sine-in-out infinite alternate breathe;transition:" + kEaseBackground + ";}";
        css += "@keyframes breathe{from{border-color:" + RcssColor(Faded(p.accent, kBreatheLow)) + ";}to{border-color:" +
               RcssColor(Faded(p.accentEnd, kBreatheHigh)) + ";}}";
        css += std::string(".widget.") + kLitClass + "{animation:none;}";
        css += std::string(".widget.") + kLitClass + " .title{font-effect:glow(0px 3px 0px 0px " +
               RcssColor(Faded(p.accent, kLitGlow)) + ");}";

        // A widget's content in the middle of its box, which can be wider (figures are measured at their widest digit),
        // and each of its lines against the widest on the side its section is on.
        css += ".content{position:relative;top:" + Px(-kContentLift) + ";height:100%;text-align:center;}";
        for (const auto& [section, align] : {std::pair{BarSection::Left, "left"}, std::pair{BarSection::Center, "center"},
                 std::pair{BarSection::Right, "right"}})
            css += std::string(".widget.") + SectionClass(section) + " .text{text-align:" + align + ";}";
        // Its own Alignment puts both against one side.
        for (const auto& [align, side] : {std::pair{WidgetAlign::Left, "left"}, std::pair{WidgetAlign::Center, "center"},
                 std::pair{WidgetAlign::Right, "right"}})
            css += std::string(".widget.") + AlignClass(align) + " .content,.widget." + AlignClass(align) + " .text{text-align:" +
                   side + ";}";
        css += ".icon,.text,.line-icon,.t{display:inline-block;vertical-align:top;}";
        auto icon = [&](float size) { return square(size) + "margin-top:" + Px(std::round(mid - size / 2.0f) - rowTop) + ";"; };
        css += ".icon{" + icon(m.icon) + "margin-right:" + Px(m.gap) + ";}";
        css += ".icon.right{margin-right:0px;margin-left:" + Px(m.gap) + ";}";
        css += ".icon.cog{margin-right:0px;}";
        css += ".t{white-space:pre;}";
        css += ".cell{display:inline-block;white-space:pre;}";
        // Only a figure in its steady box: the second line of figures is a .fig too, and must stay a block as wide as
        // the text, or its runs line up against its own width (Job & EXP's earned EXP short of the bar's end).
        css += ".steady{display:inline-block;white-space:pre;text-align:right;}.steady .fig{display:inline-block;text-align:center;}";
        css += ".line-icon{margin-left:" + Px(kLineIconGap) + ";}";
        css += ".smark{position:relative;}"; // a status's icon and its background, placed inside it
        css += ".line-icon.pic{margin-left:" + Px(kPictureGap) + ";}";
        css += TextRules("", look.titleFont, look.detailFont, lines, place, rowTop);
        for (const WidgetText& text : own)
        {
            const std::string scope = "#" + WidgetId(text.widget) + " ";
            css += TextRules(scope, text.titleFont, text.detailFont, text.lines, text.place, rowTop);
            css += scope + ".icon{" + icon(text.metrics.icon) + "}";
        }
        // An EXP bar among a line's text, taking no height of its line: its track from its capitals' middle up and down
        // (TextRules), its fill a block, as a span takes no width, and its text over it on the line's own baseline.
        css += ".ibar{display:inline-block;vertical-align:top;position:relative;height:0px;}";
        css += ".ibar .track{position:absolute;left:0px;width:100%;background-color:" + RcssColor(ExpBarBack(p)) + ";}";
        // A line's runs of their own alignment (Line).
        css += ".runs{position:relative;}.sizer{visibility:hidden;}.run{position:absolute;left:0px;right:0px;top:0px;}";
        css += ".ibar .fill{display:block;}";
        css += ".ibar .btext{position:absolute;left:0px;top:0px;width:100%;white-space:pre;box-sizing:border-box;padding:0px " +
               Px(kBarTextInset) + ";}";
        // Progress is a gradient that pulses.
        css += ".fill{height:100%;decorator:horizontal-gradient(" + RcssColor(p.accent) + " " + RcssColor(p.accentEnd) +
               ");animation:1.8s sine-in-out infinite alternate pulse;}";
        css += "@keyframes pulse{from{opacity:0.65;}to{opacity:1;}}";
        // Game help's bar: its pieces are placed in the screen's own pixels.
        css += std::string("#") + kHelpId + "{position:absolute;left:0px;top:0px;width:0px;height:0px;}";
        css += ".p{position:absolute;white-space:pre;}";
        css += ".help-box{position:absolute;" + (look.skin ? SkinRules(*look.skin) : PanelRules(HelpBarPanel(look))) + "}";
        const int help = HelpTextPixels(look.settings.helpBarTextSize, lines.title);
        css += ".hc{" + FaceRules(look.titleFont, help, place.helpCaption.height) + ShadowEffect(help) + "}";
        css += ".hh{" + FaceRules(look.detailFont, help, place.help.height) + ShadowEffect(help) + "}";
        if (look.palette.shape == BarShape::Glass) css += GlassBarRules(look);
        // Last, over every shape's own corners and borders, and a pill's own corners, its first widget's.
        const std::string joinBefore = std::string(".") + kJoinBeforeClass +
                                       "{border-left-width:0px;border-top-left-radius:0px;border-bottom-left-radius:0px;}";
        const std::string joinAfter = std::string(".") + kJoinAfterClass +
                                      "{border-right-width:0px;border-top-right-radius:0px;border-bottom-right-radius:0px;}";
        css += shape + " .widget" + joinBefore + shape + " .widget" + joinAfter;
        const PanelGroups panel = Panel(look.settings);
        for (int slot = 0; slot < kPanelSlots; ++slot)
            if (const std::optional<int> own = look.settings.Widget(panel.group[slot]).rounding)
            {
                const std::string id = "#" + WidgetId(WidgetInSlot(slot));
                css += id + "{border-radius:" + Px(look.WidgetRadius(*own)) + ";}" + id + joinBefore + id + joinAfter;
            }
        return css;
    }

    std::string BarMarkup(const BarLook& look)
    {
        const BarPalette& p = look.palette;
        const BarMetrics& m = look.metrics;
        const int divider = WholePixels(m.diamond), cap = WholePixels(m.diamond * kCapScale);
        const std::string capSource     = SeparatorSource(p.separator, p.accent, cap);
        const std::string dividerSource = SeparatorSource(p.separator, look.ornate ? p.divider : p.accent, divider);
        std::string rml = std::string("<body class=\"") + (look.ornate ? "ornate" : "pills") + "\">";
        rml += std::string("<div id=\"") + kStripId + "\"></div>";
        if (p.shape == BarShape::Glass) rml += "<div class=\"glint a\"></div><div class=\"glint b\"></div><div class=\"glint c\"></div>";
        if ((look.settings.panelSeparators & SeparatorBit(Separator::EndCaps)) != 0)
            rml += "<img class=\"cap\" id=\"cap-left\" src=\"" + capSource + "\"/><img class=\"cap\" id=\"" + kRightCapId +
                   "\" src=\"" + capSource + "\"/>";
        for (int d = 0; d < kDividerCount; ++d)
            rml += "<img class=\"divider hidden\" id=\"" + DividerId(d) + "\" src=\"" + dividerSource + "\"/>";
        for (int slot = 0; slot < kPanelSlots; ++slot)
        {
            const PanelWidget widget = WidgetInSlot(slot);
            if (!OnTheBar(widget.kind) || (widget.copy > 0 && !HasCopies(widget.kind))) continue;
            rml += std::string("<div class=\"widget hidden") + (widget.kind == BarWidget::Target ? std::string(" ") + kTargetClass : "") +
                   "\" id=\"" + WidgetId(widget) + "\"></div>";
        }
        rml += std::string("<div id=\"") + kHelpId + "\"></div>";
        return rml + "</body>";
    }

    std::string BarDocument(const BarLook& look, const BarTextPlace& place, const std::vector<WidgetText>& own)
    {
        return "<rml><head><style>" + BarStyle(look, place, own) + "</style></head>" + BarMarkup(look) + "</rml>";
    }

    std::string HelpBarRml(const HelpBarParts& parts, const BarLook& look, const BarTextPlace& place)
    {
        const BarRect& box = parts.row.box;
        if (box.width <= 0.0f) return {};
        std::string rml;
        if (look.settings.helpBarBackground)
            rml += "<div class=\"help-box\" style=\"" + BoxAt(box.x, box.y, box.width, box.height) + "\"></div>";
        auto centered = [&](int lineHeight) { return box.y + (box.height - static_cast<float>(lineHeight)) / 2.0f; };
        const float captionY = centered(place.helpCaption.height);
        if (!parts.caption.empty()) rml += TextAt("hc", parts.row.captionX, captionY, look.palette.title, parts.caption);
        if (parts.bag) rml += TextAt("hc", parts.row.bagX, captionY, look.BarColor(*parts.bag), parts.bag->text);
        if (!parts.help.empty())
            rml += TextAt("hh", parts.row.helpX, centered(place.help.height), look.palette.text, parts.help);
        return rml;
    }

    // The bar's text shadow as a font effect: the letters blurred, once a pixel down and once in place.
    std::string ShadowEffect(int pixels)
    {
        const float sigma = static_cast<float>(pixels) * kShadowPerHeight * kSigmaPerRadius;
        const std::string blur = Px(static_cast<float>(WholePixels(sigma / kGlowSigmaPerWidth)));
        const std::string color = RcssColor(WithAlpha(kBlack, static_cast<uint32_t>(std::lround(kShadowStrength * 255.0f))));
        return "font-effect:glow(0px " + blur + " 0px " + Px(static_cast<float>(kShadowDrop)) + " " + color +
               "),glow(0px " + blur + " 0px 0px " + color + ");";
    }

    std::string SocketRules(float radius)
    {
        return std::string("box-sizing:border-box;border:1px;border-color:") + RcssColor(kSocketShade) + " " +
               RcssColor(kSocketLight) + " " + RcssColor(kSocketLight) + " " + RcssColor(kSocketShade) +
               ";border-radius:" + Px(radius) + ";background-color:" + RcssColor(kSocketFill) + ";";
    }

    std::string PopupScope(PanelWidget pill) { return "of-" + WidgetId(pill); }

    std::string PopupStyle(const BarLook& look, const PopupFaces& faces, const std::vector<PillFaces>& own)
    {
        const BoxStyle panel = PopupPanel(look);
        std::string css = "body{display:block;width:100%;height:100%;pointer-events:none;z-index:" + std::to_string(kPopupsZ) + ";}";
        css += "div{display:block;}";
        css += std::string(".") + kHiddenClass + "{display:none;}";
        // A popup drops in from its widget.
        // A popup takes the pointer in RmlUi, so a window under it gets no hover or press through it.
        css += std::string(".") + kPopupClass + "{position:absolute;transform-origin:50% 0%;pointer-events:auto;}";
        css += std::string(".") + kPopupClass + "." + kOpenClass + "{animation:0.2s cubic-out drop-in;}";
        css += "@keyframes drop-in{from{opacity:0;transform:translateY(-10px) scale(0.96);}to{opacity:1;"
               "transform:translateY(0px) scale(1);}}";
        css += ".p{position:absolute;white-space:pre;}";
        const auto faceRules = [&](const std::string& scope, const PopupFaces& each) {
            for (int f = 0; f < kPopupFontCount; ++f)
                css += scope + ".f" + std::to_string(f) + "{" + FaceRules(each[f].family, each[f].pixels, each[f].line) +
                       ShadowEffect(each[f].pixels) + "}";
        };
        faceRules("", faces);
        for (const PillFaces& pill : own)
            faceRules("." + PopupScope(pill.pill) + " ", pill.faces);
        if (panel.outer != 0)
            css += "#outer{position:absolute;box-sizing:border-box;border:1px " + RcssColor(panel.outer) +
                   ";border-radius:" + Px(panel.radius + static_cast<float>(OuterPad(panel))) + ";}";
        css += "#panel{position:absolute;left:0px;top:0px;" + (look.skin ? SkinRules(*look.skin) : PanelRules(panel)) + "}";
        css += ".card{" + FlatRules(PopupCard(look), true) + "}";
        css += ".tag{" + FlatRules(PopupTag(look, 0.0f), false) + "}";
        // A card's tabs as tags, the picked one in the accent.
        css += ".tab{" + FlatRules(PopupTag(look, 0.0f), false) + "}";
        css += ".track{background-color:" + RcssColor(kScrollTrack) + ";}";
        css += ".thumb{background-color:" + RcssColor(kScrollThumb) + ";}";
        css += ".tab.lit{border-color:" + RcssColor(look.palette.accent) + ";background-color:" +
               RcssColor(Faded(look.palette.accent, kLitTab)) + ";}";
        css += ".socket{" + SocketRules(PopupCard(look).radius / 2.0f) + "}";
        css += ".rule{height:1px;decorator:horizontal-gradient(" + RcssColor(look.palette.divider) + " " +
               RcssColor(look.palette.divider & ~kAlphaMask) + ");}";
        css += ".mark{text-align:center;}";
        if (look.palette.shape == BarShape::Glass)
        {
            // Its panel and cards are glass.
            css += ".card{decorator:vertical-gradient(#ffffff17 #ffffff08);background-color:transparent;border-color:" +
                   RcssColor(Faded(look.palette.accent, 0x33)) + ";}";
            css += "#panel{background-color:transparent;decorator:vertical-gradient(" + RcssColor(kGlassTop) + " " +
                   RcssColor(kGlassFoot) + ");border-color:" + RcssColor(Faded(look.palette.accent, kGlassFrame)) + ";}";
        }
        return css;
    }

    std::string PopupDocument(const BarLook& look, const PopupFaces& faces, const std::vector<PillFaces>& own)
    {
        return "<rml><head><style>" + PopupStyle(look, faces, own) + "</style></head><body></body></rml>";
    }

    std::string PopupRml(const PopupLayout& layout, const BarLook& look, const PopupFaces& faces)
    {
        const BoxStyle panel = PopupPanel(look);
        const auto out       = static_cast<float>(OuterPad(panel));
        auto size = [](float width, float height) {
            return "width:" + Px(static_cast<float>(WholePixels(width))) + ";height:" +
                   Px(static_cast<float>(WholePixels(height))) + ";";
        };
        std::string rml;
        if (panel.outer != 0)
            rml += "<div id=\"outer\" style=\"left:" + Px(-out) + ";top:" + Px(-out) + ";" +
                   size(layout.width + 2.0f * out, layout.height + 2.0f * out) + "\"></div>";
        rml += "<div id=\"panel\" style=\"" + size(layout.width, layout.height) + "\"></div>";
        // Previewed cards keep their places inside one box the popup's size, scaled about the title's foot and moved to
        // the middle; the pieces come line by line, so every card's are in it.
        bool boxed = false;
        for (const PopupPiece& piece : layout.pieces)
        {
            if (layout.preview && !boxed && piece.line > 0)
            {
                boxed = true;
                rml += std::string("<div class=\"p ") + kPreviewClass + "\" style=\"left:0px;top:0px;" +
                       size(layout.width, layout.height) + "transform-origin:" + Px(look.metrics.popupPadding) + " " +
                       Px(layout.preview->top) + ";transform:translateX(" + Px(layout.preview->shift) + ") scale(" +
                       RcssNumber(layout.preview->scale) + ");\">";
            }
            const std::string face  = "f" + std::to_string(static_cast<int>(piece.font));
            const uint32_t color    = look.PopupColor(piece.span);
            const std::string place = "left:" + Px(std::round(piece.x)) + ";top:" + Px(std::round(piece.y)) + ";";
            switch (piece.kind)
            {
            case PopupPiece::Kind::Text:
                rml += TextAt(face.c_str(), piece.x, std::round(piece.y) + faces[static_cast<size_t>(piece.font)].drop, color,
                    piece.span.text, piece.right ? piece.width : 0.0f);
                break;
            case PopupPiece::Kind::ImmuneMark:
                rml += "<div class=\"p " + face + " mark\" style=\"" + BoxAt(piece.x, piece.y, piece.width, piece.height) +
                       "line-height:" + Px(static_cast<float>(WholePixels(piece.height))) + ";color:" + RcssColor(color) +
                       ";\">" + EscapedRml(piece.span.text) + "</div>";
                break;
            case PopupPiece::Kind::Icon:
                rml += "<img class=\"p\" src=\"" + IconSource(piece.icon) + "\" style=\"" + place + "width:" +
                       Px(piece.width) + ";height:" + Px(piece.height) + ";" + AdjustRules(AdjustOf(piece.icon)) +
                       (piece.tinted ? "image-color:" + RcssColor(color) + ";" : std::string()) + "\"/>";
                break;
            case PopupPiece::Kind::Item:
                rml += "<img class=\"p\" src=\"" + ItemSource(piece.item) + "\" style=\"" + place + "width:" +
                       Px(piece.width) + ";height:" + Px(piece.height) + ";\"/>";
                break;
            case PopupPiece::Kind::Picture:
                rml += "<img class=\"p\" src=\"" + GamePictureSource(piece.picture) + "\" style=\"" + place + "width:" +
                       Px(piece.width) + ";height:" + Px(piece.height) + ";" + AdjustRules(AdjustOf(piece.picture)) + "\"/>";
                break;
            case PopupPiece::Kind::Status:
                rml += StatusPictures(piece.status, std::round(piece.x), std::round(piece.y), piece.width, "px");
                break;
            case PopupPiece::Kind::Cog:
            {
                const int pixels = WholePixels(piece.width);
                rml += "<img class=\"p\" src=\"" + CogSource(look.PopupColor(Span{"", Tone::Heading, 0}), pixels) +
                       "\" style=\"" + place + size(static_cast<float>(pixels), static_cast<float>(pixels)) +
                       AdjustRules(AdjustOf(kCogName)) + "\"/>";
                break;
            }
            case PopupPiece::Kind::Star:
            {
                const int pixels = WholePixels(piece.width);
                rml += "<img class=\"p\" src=\"" + IconSource(Icon::Star) + "\" style=\"" + place +
                       size(static_cast<float>(pixels), static_cast<float>(pixels)) + "image-color:" + RcssColor(kStarColor) +
                       ";" + AdjustRules(AdjustOf(Icon::Star)) + "\"/>";
                break;
            }
            case PopupPiece::Kind::Sign:
            {
                // Bars in whole pixels, as far from each side of the chip as from the other: a glyph sat low in it.
                const int side = WholePixels(piece.width);
                int bar        = std::max(1, static_cast<int>(std::lround(static_cast<float>(side) * kSignBar)));
                int arm        = static_cast<int>(std::lround(static_cast<float>(side) * kSignArm));
                bar += (side - bar) % 2;
                arm += (side - arm) % 2;
                const float x = std::round(piece.x), y = std::round(piece.y);
                const auto in = static_cast<float>((side - bar) / 2), along = static_cast<float>((side - arm) / 2);
                const std::string fill = "background-color:" + RcssColor(color) + ";\"></div>";
                rml += "<div class=\"p\" style=\"" + BoxAt(x + along, y + in, static_cast<float>(arm), static_cast<float>(bar)) + fill;
                if (piece.span.text == kCollapsedSign)
                    rml += "<div class=\"p\" style=\"" + BoxAt(x + in, y + along, static_cast<float>(bar), static_cast<float>(arm)) + fill;
                break;
            }
            case PopupPiece::Kind::Rule:
                rml += "<div class=\"p rule\" style=\"" + place + "width:" + Px(static_cast<float>(WholePixels(piece.width))) +
                       ";\"></div>";
                break;
            case PopupPiece::Kind::Card:
                rml += "<div class=\"p card\" style=\"" + BoxAt(piece.x, piece.y, piece.width, piece.height) + "\"></div>";
                break;
            case PopupPiece::Kind::Tab:
            case PopupPiece::Kind::PinChip:
            case PopupPiece::Kind::CogChip:
            case PopupPiece::Kind::ActionChip:
                rml += std::string("<div class=\"p tab") + (piece.open ? " lit" : "") + "\" style=\"" +
                       BoxAt(piece.x, piece.y, piece.width, piece.height) + "border-radius:" + Px(piece.height / 2.0f) +
                       ";\"></div>";
                break;
            case PopupPiece::Kind::ScrollTrack:
            case PopupPiece::Kind::ScrollThumb:
                rml += std::string("<div class=\"p ") + (piece.kind == PopupPiece::Kind::ScrollTrack ? "track" : "thumb") +
                       "\" style=\"" + BoxAt(piece.x, piece.y, piece.width, piece.height) + "border-radius:" +
                       Px(piece.width / 2.0f) + ";\"></div>";
                break;
            case PopupPiece::Kind::Socket:
                rml += "<div class=\"p socket\" style=\"" + BoxAt(piece.x, piece.y, piece.width, piece.height) + "\"></div>";
                break;
            case PopupPiece::Kind::Tag:
                rml += "<div class=\"p tag\" style=\"" + BoxAt(piece.x, piece.y, piece.width, piece.height) +
                       "border-radius:" + Px(PopupTag(look, piece.height).radius) + ";\"></div>";
                break;
            }
        }
        if (boxed) rml += "</div>";
        return rml;
    }

    std::vector<float> CellColumns(const WidgetContent& content, FunctionRef<float(bool title, const Span&)> width)
    {
        std::vector<float> columns(std::max(content.title.size(), content.subtitle.size()), 0.0f);
        for (size_t i = 0; i < content.title.size(); ++i)
            columns[i] = std::max(columns[i], width(true, content.title[i]));
        for (size_t i = 0; i < content.subtitle.size(); ++i)
            columns[i] = std::max(columns[i], width(false, content.subtitle[i]));
        return columns;
    }

    float CellsWidth(const std::vector<float>& columns, float gap)
    {
        float total = 0.0f;
        for (const float column : columns)
            total += column;
        return columns.empty() ? 0.0f : total + gap * static_cast<float>(columns.size() - 1);
    }

    std::string WidgetRml(const WidgetContent& content, const BarLook& look, const std::vector<float>& columns)
    {
        const bool alone = content.subtitle.empty();
        std::string rml  = "<div class=\"content\">";
        std::string icons;
        const std::string side = content.iconsRight ? " right" : "";
        if (content.picture != GamePicture::None)
            icons += "<img class=\"icon pic" + side + "\" src=\"" + GamePictureSource(content.picture) + "\"" +
                     AdjustStyle(AdjustOf(content.picture)) + "/>";
        for (const Icon icon : content.icons)
            icons += "<img class=\"icon" + side + "\" src=\"" + IconSource(icon) + "\"" + AdjustStyle(AdjustOf(icon)) + "/>";
        if (content.cog)
            icons += "<img class=\"icon cog\" src=\"" + CogSource(look.palette.title, WholePixels(look.metrics.icon)) + "\"" +
                     AdjustStyle(AdjustOf(kCogName)) + "/>";
        if (!content.iconsRight) rml += icons;
        rml += "<div class=\"text\">";
        const std::string title = std::string("title") + (content.figures ? " figures" : "") +
                                  (alone ? " alone" : content.cells ? " cells" : "");
        static const std::vector<float> kNoColumns;
        const std::vector<float>& laid = content.cells ? columns : kNoColumns;
        rml += Line(title.c_str(), content.title, look, laid);
        if (!alone) rml += Line(content.figures ? "detail fig" : "detail", content.subtitle, look, laid);
        rml += "</div>";
        if (content.iconsRight) rml += icons;
        return rml + "</div>";
    }
}

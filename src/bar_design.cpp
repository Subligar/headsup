#include "bar_design.h"

#include <algorithm>

namespace headsup
{
    namespace
    {
        using P = WidgetPart;
        constexpr P kZone[]   = {P::ZoneName, P::ZoneRegion, P::ZoneNation};
        constexpr P kWeather[] = {P::WeatherName, P::VanaDate, P::VanaDay, P::VanaTime, P::VanaMoon};
        constexpr P kTarget[] = {P::TargetName, P::TargetDistance, P::TargetDetection, P::TargetLevel,
            P::TargetPlayer, P::TargetWeak};
        constexpr P kCurrency[] = {P::Currency1Name, P::Currency2Name, P::Currency1Amount, P::Currency2Amount,
            P::Currency1Icon, P::Currency2Icon};
        constexpr P kBags[]   = {P::BagCount, P::BagName};
        constexpr P kClock[]  = {P::ClockTime, P::ClockServer, P::ClockVana, P::ClockDate};
        constexpr P kJob[]    = {P::JobJobs, P::JobEarned, P::JobToNext, P::JobPercent, P::JobBar};
        constexpr P kPerf[]   = {P::PerfFps, P::PerfSent, P::PerfReceived, P::PerfPercent, P::PerfMemory,
            P::PerfMessages};
        constexpr P kParty[]  = {P::PartyCount, P::PartyHealth};
        constexpr P kStatus[] = {P::StatusPin1, P::StatusPin2, P::StatusPin3, P::StatusPin4, P::StatusPin5, P::StatusPin6};
        constexpr P kWorld[]  = {P::WorldShip, P::WorldGuild, P::WorldSanDoria, P::WorldBastok,
            P::WorldWindurst, P::WorldBeastmen};

        // By WidgetPart.
        constexpr const char* kFileNames[kWidgetPartCount] = {"name", "region", "crest", "nation", "icon", "weather",
            "date", "day", "time", "moon", "aggro", "name", "distance", "detection", "level", "player", "weak",
            "coin", "name1", "name2", "amount1", "amount2", "icon1", "icon2", "count", "bag", "icon", "time", "server", "vana", "icon", "date", "jobs", "earned", "tonext", "percent", "bar",
            "fps", "sent", "received", "percent", "memory", "messages", "count", "health", "pin1", "pin2",
            "pin3", "pin4", "pin5", "pin6", "ship",
            "guild", "sandoria", "bastok", "windurst", "beastmen"};

        // As each widget showed before it had a design.
        constexpr const char* kDefaults[kBarWidgetCount] = {"name | region nation", "weather | day time -date -moon",
            "name distance -detection | level player -weak", "amount1 | icon2 amount2 -name1 -name2 -icon1", "count | bag", "time | -server -vana -date", "", "-fps sent received percent -memory messages |",
            "jobs | tonext bar -earned -percent", "count | health", "pin1 pin3 pin5 | pin2 pin4 pin6",
            "ship | -guild -sandoria -bastok -windurst -beastmen", ""};

        constexpr char kLineBreak = '|', kOffMark = '-', kSpace = ' ';

        bool Holds(std::span<const WidgetPart> parts, WidgetPart part) { return std::ranges::find(parts, part) != parts.end(); }

        // The text's parts as written, unknown names left out; Whole does the rest.
        WidgetDesign Parsed(BarWidget kind, std::string_view text)
        {
            WidgetDesign design;
            int line = 0;
            while (!text.empty())
            {
                const size_t at = text.find_first_of(" |");
                const std::string_view word = text.substr(0, at);
                if (!word.empty())
                {
                    const bool off = word.front() == kOffMark;
                    const std::string_view name = off ? word.substr(1) : word;
                    for (const WidgetPart part : PartsOf(kind))
                        if (name == PartFileName(part)) design.lines[static_cast<size_t>(line)].push_back({part, !off});
                }
                if (at == std::string_view::npos) break;
                if (text[at] == kLineBreak) line = std::min(line + 1, kDesignLines - 1);
                text.remove_prefix(at + 1);
            }
            return design;
        }
    }

    std::optional<WidgetPart> IconPartOf(BarWidget kind)
    {
        switch (kind)
        {
        case BarWidget::Zone: return P::ZoneCrest;
        case BarWidget::Weather: return P::WeatherIcon;
        case BarWidget::Target: return P::TargetAggro;
        case BarWidget::Currency: return P::CurrencyCoin;
        case BarWidget::Bags: return P::BagIcon;
        case BarWidget::EarthTime: return P::ClockIcon;
        default: return std::nullopt;
        }
    }

    std::span<const WidgetPart> PartsOf(BarWidget kind)
    {
        switch (kind)
        {
        case BarWidget::Zone: return kZone;
        case BarWidget::Weather: return kWeather;
        case BarWidget::Target: return kTarget;
        case BarWidget::Currency: return kCurrency;
        case BarWidget::Bags: return kBags;
        case BarWidget::EarthTime: return kClock;
        case BarWidget::GameHelp:
        case BarWidget::Settings: return {};
        case BarWidget::Performance: return kPerf;
        case BarWidget::Job: return kJob;
        case BarWidget::Party: return kParty;
        case BarWidget::StatusTimers: return kStatus;
        case BarWidget::World: return kWorld;
        }
        return {};
    }

    WidgetDesign DefaultDesign(BarWidget kind) { return Whole(kind, Parsed(kind, kDefaults[static_cast<int>(kind)])); }

    const char* PartFileName(WidgetPart part) { return kFileNames[static_cast<int>(part)]; }

    std::string DesignText(const WidgetDesign& design)
    {
        std::string text;
        for (int line = 0; line < kDesignLines; ++line)
        {
            if (line > 0) text += text.empty() ? std::string(1, kLineBreak) : std::string(" ") + kLineBreak;
            for (const DesignChip& chip : design.lines[static_cast<size_t>(line)])
            {
                if (!text.empty()) text += kSpace;
                if (!chip.on) text += kOffMark;
                text += PartFileName(chip.part);
            }
        }
        return text;
    }

    WidgetDesign DesignFrom(BarWidget kind, std::string_view text)
    {
        // A design of none of the parts the widget has now, such as one saved before its parts changed, is no design.
        const WidgetDesign parsed = Parsed(kind, text);
        const bool none = std::ranges::all_of(parsed.lines, [](const auto& line) { return line.empty(); });
        return none ? DefaultDesign(kind) : Whole(kind, parsed);
    }

    WidgetDesign Whole(BarWidget kind, WidgetDesign design)
    {
        const std::span<const WidgetPart> parts = PartsOf(kind);
        std::vector<WidgetPart> seen;
        for (auto& line : design.lines)
            std::erase_if(line, [&](const DesignChip& chip) {
                if (!Holds(parts, chip.part) || std::ranges::find(seen, chip.part) != seen.end()) return true;
                seen.push_back(chip.part);
                return false;
            });
        for (const WidgetPart part : parts)
            if (std::ranges::find(seen, part) == seen.end()) design.lines.back().push_back({part, false});
        if (!parts.empty() && std::ranges::none_of(parts, [&](WidgetPart p) { return PartOn(design, p); }))
            for (auto& line : design.lines)
                if (!line.empty())
                {
                    line.front().on = true;
                    break;
                }
        return design;
    }

    bool PartOn(const WidgetDesign& design, WidgetPart part)
    {
        for (const auto& line : design.lines)
            for (const DesignChip& chip : line)
                if (chip.part == part) return chip.on;
        return false;
    }

    bool MovePart(WidgetDesign& design, int fromLine, size_t from, int toLine, size_t at)
    {
        if (fromLine < 0 || fromLine >= kDesignLines || toLine < 0 || toLine >= kDesignLines) return false;
        auto& source = design.lines[static_cast<size_t>(fromLine)];
        auto& target = design.lines[static_cast<size_t>(toLine)];
        if (from >= source.size()) return false;
        if (fromLine == toLine && (at == from || at == from + 1)) return false;
        const DesignChip chip = source[from];
        source.erase(source.begin() + static_cast<std::ptrdiff_t>(from));
        if (fromLine == toLine && at > from) --at;
        target.insert(target.begin() + static_cast<std::ptrdiff_t>(std::min(at, target.size())), chip);
        return true;
    }

    WidgetContent ComposeContent(const WidgetDesign& design, std::vector<PartContent> parts, WidgetContent base)
    {
        std::array<std::vector<PartContent>, kDesignLines> placed; // each line's parts with something to show
        for (int line = 0; line < kDesignLines; ++line)
            for (const DesignChip& chip : design.lines[static_cast<size_t>(line)])
            {
                const auto part = std::ranges::find(parts, chip.part, &PartContent::part);
                if (!chip.on || part == parts.end() || part->spans.empty()) continue;
                // A currency's coin stays on its line, apart from the Coin Icon.
                const bool iconsAlone = std::ranges::all_of(part->spans, [](const Span& s) { return s.icon && s.text.empty(); });
                if (line == 0 && placed[0].empty() && iconsAlone)
                {
                    for (const Span& s : part->spans)
                        base.icons.push_back(*s.icon);
                    continue;
                }
                placed[static_cast<size_t>(line)].push_back(std::move(*part));
            }
        if (placed[0].empty()) std::swap(placed[0], placed[1]);
        // The second line is the quieter one: a part there dims, unless it shades its own text.
        for (PartContent& part : placed[1])
            if (std::ranges::none_of(part.spans, [](const Span& s) { return s.tone == Tone::Dim; }))
                for (Span& span : part.spans)
                    if (span.tone == Tone::Normal && !span.text.empty()) span.tone = Tone::Dim;
        std::array<std::vector<Span>, kDesignLines> lines;
        for (int line = 0; line < kDesignLines; ++line)
        {
            std::vector<PartContent>& parts = placed[static_cast<size_t>(line)];
            std::vector<Span>& spans        = lines[static_cast<size_t>(line)];
            std::optional<Align> near;
            for (PartContent& part : parts)
                if (part.align) near = part.align;
                else part.align = near;
            near.reset();
            for (auto part = parts.rbegin(); part != parts.rend(); ++part)
                if (part->align) near = part->align;
                else part->align = near;
            std::ranges::stable_sort(parts, {}, &PartContent::align);
            for (size_t i = 0; i < parts.size(); ++i)
            {
                const size_t at = spans.size();
                AddPart(spans, std::move(parts[i].spans));
                if (parts[i].align && (i == 0 || parts[i - 1].align != parts[i].align)) spans[at].align = parts[i].align;
            }
        }
        base.title    = std::move(lines[0]);
        base.subtitle = std::move(lines[1]);
        return base;
    }

    std::optional<PartMark> MarkOf(const std::vector<Span>& spans)
    {
        for (const Span& s : spans)
            if (s.icon || s.picture != GamePicture::None || s.coin) return PartMark{s.coin ? Icon::Gil : s.icon, s.picture};
        return std::nullopt;
    }

    PartMark DefaultMark(WidgetPart part)
    {
        switch (part)
        {
        case WidgetPart::WeatherIcon:
        case WidgetPart::TargetWeak: return PartMark{Icon::Fire};
        case WidgetPart::TargetAggro: return PartMark{Icon::AggroNQ};
        case WidgetPart::TargetDetection: return PartMark{Icon::Sight};
        case WidgetPart::BagIcon: return PartMark{Icon::Bag};
        case WidgetPart::ClockIcon: return PartMark{Icon::Clock};
        case WidgetPart::ZoneCrest:
        case WidgetPart::WorldSanDoria: return PartMark{std::nullopt, GamePicture::SanDoriaCrest};
        case WidgetPart::WorldBastok: return PartMark{std::nullopt, GamePicture::BastokCrest};
        case WidgetPart::WorldWindurst: return PartMark{std::nullopt, GamePicture::WindurstCrest};
        case WidgetPart::WorldBeastmen: return PartMark{std::nullopt, GamePicture::BeastmenCrest};
        case WidgetPart::PerfMessages: return PartMark{std::nullopt, GamePicture::PlayOnlineMail};
        case WidgetPart::CurrencyCoin: return PartMark{Icon::Gil};
        default: return {};
        }
    }

    size_t WidestTextSpan(const std::vector<Span>& line)
    {
        // Letters, not the gaps a part starts with; a dim span (a label) only on a line of nothing else; never a span
        // that keeps its room, as a cut one would keep it.
        const auto letters = [](const Span& s) {
            return s.room.empty() ? std::ranges::count_if(s.text, [](char c) { return c != ' '; }) : 0;
        };
        const auto score   = [&](const Span& s) { return std::pair{s.tone != Tone::Dim, letters(s)}; };
        size_t widest = line.size();
        for (size_t i = 0; i < line.size(); ++i)
            if (letters(line[i]) > 0 && (widest == line.size() || score(line[i]) > score(line[widest]))) widest = i;
        return widest;
    }

    bool TurnOff(WidgetDesign& design, WidgetPart part)
    {
        DesignChip* found = nullptr;
        int on            = 0;
        for (auto& line : design.lines)
            for (DesignChip& chip : line)
            {
                on += chip.on ? 1 : 0;
                if (chip.part == part) found = &chip;
            }
        if (found == nullptr || !found->on || on <= 1) return false;
        found->on = false;
        return true;
    }
}

#include "bar_design.h"

#include "bar_placement.h"

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
        constexpr P kBags[]   = {P::Bag1Count, P::Bag2Count, P::Bag1Name, P::Bag2Name};
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
            "coin", "name1", "name2", "amount1", "amount2", "icon1", "icon2", "count1", "count2", "name1", "name2", "icon", "time", "server", "vana", "icon", "date", "jobs", "earned", "tonext", "percent", "bar",
            "fps", "sent", "received", "percent", "memory", "messages", "count", "health", "pin1", "pin2",
            "pin3", "pin4", "pin5", "pin6", "ship",
            "guild", "sandoria", "bastok", "windurst", "beastmen", "name"};

        // As each widget showed before it had a design.
        constexpr const char* kDefaults[kBarWidgetCount] = {"name | region nation", "weather | day time -date -moon",
            "name distance -detection | level player -weak", "amount1 | icon2 amount2 -name1 -name2 -icon1", "count1 | name1", "time | -server -vana -date", "", "-fps sent received percent -memory messages |",
            "jobs | tonext bar -earned -percent", "count | health", "pin1 pin3 pin5 | pin2 pin4 pin6",
            "ship | -guild -sandoria -bastok -windurst -beastmen", "", "name |"};

        constexpr char kLineBreak = '|', kOffMark = '-', kSpace = ' ', kWidgetMark = '.';

        // A part's name in kind's design: its own, or after its widget's.
        std::string PartKey(BarWidget kind, WidgetPart part)
        {
            const BarWidget owner = WidgetOfPart(part);
            return owner == kind ? PartFileName(part) : BarWidgetName(owner) + std::string(1, kWidgetMark) + PartFileName(part);
        }

        using W = BarWidget;
        struct CardEntry
        {
            BarWidget widget;
            const char* file;
        };
        // By CardKind.
        constexpr CardEntry kCards[kCardKindCount] = {{W::Zone, "zone.influence"}, {W::Weather, "weather.today"},
            {W::Weather, "weather.forecast"}, {W::Weather, "weather.byElement"}, {W::Target, "target.overview"},
            {W::Target, "target.damage"}, {W::Target, "target.elements"}, {W::Target, "target.statuses"},
            {W::Target, "target.immune"}, {W::Target, "target.stats"}, {W::Target, "target.moves"},
            {W::Target, "target.spells"}, {W::Target, "target.spawn"}, {W::Target, "target.drops"},
            {W::Target, "target.steal"}, {W::Target, "target.despoil"}, {W::Target, "target.shop"},
            {W::Target, "target.click"}, {W::Target, "target.trade"}, {W::Target, "target.pops"},
            {W::Target, "target.gives"}, {W::Target, "target.keyItems"}, {W::Target, "target.checks"},
            {W::Target, "target.quests"}, {W::Target, "target.missions"}, {W::Target, "target.battlefields"},
            {W::Currency, "gil.favorites"}, {W::Currency, "gil.all"}, {W::Bags, "bags.list"},
            {W::EarthTime, "earthTime.server"}, {W::Performance, "performance.connection"},
            {W::Performance, "performance.session"}, {W::Performance, "performance.playOnline"},
            {W::Performance, "performance.memory"}, {W::Performance, "performance.processor"},
            {W::Performance, "performance.linuxxi"}, {W::Performance, "performance.lua"}, {W::Job, "job.exp"},
            {W::Party, "party.members"}, {W::StatusTimers, "statusTimers.favorites"},
            {W::StatusTimers, "statusTimers.buffs"}, {W::StatusTimers, "statusTimers.debuffs"},
            {W::World, "world.conquest"}, {W::World, "world.travel"}, {W::World, "world.calendar"},
            {W::World, "world.guilds"}};

        // By BarWidget: each pop-out's cards as it laid them out before it had a design.
        constexpr const char* kPopupDefaults[kBarWidgetCount] = {"zone.influence",
            "weather.today weather.forecast | weather.byElement",
            "target.overview | target.damage target.elements | target.statuses target.immune | target.stats target.spawn"
            " | target.moves target.spells | target.drops target.steal target.despoil | target.shop target.trade"
            " | target.click target.pops | target.gives target.keyItems target.checks"
            " | target.quests target.missions target.battlefields",
            "gil.favorites | gil.all", "bags.list", "earthTime.server", "",
            "performance.connection performance.session | performance.memory performance.processor"
            " | performance.playOnline performance.linuxxi performance.lua",
            "job.exp", "party.members", "statusTimers.favorites | statusTimers.buffs | statusTimers.debuffs",
            "world.conquest world.travel world.calendar | world.guilds", "", ""};
        constexpr const char* kNoCards = "none"; // a pop-out design with no card, which "" would not be

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
                        if (name == PartKey(kind, part)) design.lines[static_cast<size_t>(line)].push_back({part, !off});
                }
                if (at == std::string_view::npos) break;
                if (text[at] == kLineBreak) line = std::min(line + 1, kDesignLines - 1);
                text.remove_prefix(at + 1);
            }
            return design;
        }
    }

    namespace
    {
        // The text's cards as written, row by row, unknown names left out; WholePopup does the rest.
        PopupDesign ParsedCards(std::string_view text)
        {
            PopupDesign design{{{}}};
            while (!text.empty())
            {
                const size_t at = text.find_first_of(" |");
                const std::string_view word = text.substr(0, at);
                for (int c = 0; c < kCardKindCount; ++c)
                    if (word == kCards[c].file) design.rows.back().push_back(static_cast<CardKind>(c));
                if (at == std::string_view::npos) break;
                if (text[at] == kLineBreak) design.rows.emplace_back();
                text.remove_prefix(at + 1);
            }
            return design;
        }
    }

    BarWidget WidgetOfCard(CardKind card) { return kCards[static_cast<int>(card)].widget; }

    std::vector<CardKind> CardsOf(BarWidget kind)
    {
        std::vector<CardKind> cards;
        for (int c = 0; c < kCardKindCount; ++c)
            if (kCards[c].widget == kind) cards.push_back(static_cast<CardKind>(c));
        return cards;
    }

    const char* CardFileName(CardKind card) { return kCards[static_cast<int>(card)].file; }

    PopupDesign DefaultPopupDesign(BarWidget kind) { return WholePopup(ParsedCards(kPopupDefaults[static_cast<int>(kind)])); }

    std::string PopupDesignText(const PopupDesign& design)
    {
        std::string text;
        for (const auto& row : design.rows)
        {
            if (!text.empty()) text += std::string(1, kSpace) + kLineBreak;
            for (const CardKind card : row)
                text += (text.empty() ? "" : std::string(1, kSpace)) + CardFileName(card);
        }
        return text.empty() ? kNoCards : text;
    }

    PopupDesign PopupDesignFrom(BarWidget kind, std::string_view text)
    {
        return text.empty() ? DefaultPopupDesign(kind) : WholePopup(ParsedCards(text));
    }

    PopupDesign WholePopup(PopupDesign design)
    {
        std::vector<CardKind> seen;
        for (auto& row : design.rows)
        {
            std::erase_if(row, [&](CardKind card) {
                if (std::ranges::find(seen, card) != seen.end()) return true;
                seen.push_back(card);
                return false;
            });
            if (row.size() > kMostCardsInRow) row.resize(kMostCardsInRow);
        }
        std::erase_if(design.rows, [](const auto& row) { return row.empty(); });
        return design;
    }

    bool HoldsCard(const PopupDesign& design, CardKind card)
    {
        return std::ranges::any_of(design.rows, [&](const auto& row) { return std::ranges::find(row, card) != row.end(); });
    }

    bool RowTakes(const PopupDesign& design, size_t row, CardKind card)
    {
        if (row >= design.rows.size()) return true;
        const auto& cards = design.rows[row];
        return cards.size() < kMostCardsInRow || std::ranges::find(cards, card) != cards.end();
    }

    bool PlaceCard(PopupDesign& design, CardKind card, size_t row, size_t at)
    {
        std::optional<std::pair<size_t, size_t>> from; // its row and place
        for (size_t r = 0; r < design.rows.size(); ++r)
            if (const auto i = std::ranges::find(design.rows[r], card); i != design.rows[r].end())
                from = std::pair{r, static_cast<size_t>(i - design.rows[r].begin())};
        row = std::min(row, design.rows.size());
        if (!RowTakes(design, row, card)) return false;
        if (row == design.rows.size()) design.rows.emplace_back();
        auto& target = design.rows[row];
        at           = std::min(at, target.size());
        if (from && from->first == row && (at == from->second || at == from->second + 1)) return false;
        target.insert(target.begin() + static_cast<std::ptrdiff_t>(at), card);
        if (from)
        {
            auto& source = design.rows[from->first];
            const size_t old = from->first == row && at <= from->second ? from->second + 1 : from->second;
            source.erase(source.begin() + static_cast<std::ptrdiff_t>(old));
        }
        std::erase_if(design.rows, [](const auto& r) { return r.empty(); });
        return true;
    }

    bool RemoveCard(PopupDesign& design, CardKind card)
    {
        bool removed = false;
        for (auto& row : design.rows)
            removed = std::erase(row, card) > 0 || removed;
        std::erase_if(design.rows, [](const auto& r) { return r.empty(); });
        return removed;
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
        case BarWidget::Custom:
        {
            // Its Name, then each widget's icon and parts, in the widgets' order.
            static const std::vector<WidgetPart> every = [] {
                std::vector<WidgetPart> all{P::CustomName};
                for (int k = 0; k < static_cast<int>(BarWidget::Custom); ++k)
                {
                    if (const std::optional<WidgetPart> icon = IconPartOf(static_cast<BarWidget>(k))) all.push_back(*icon);
                    for (const WidgetPart part : PartsOf(static_cast<BarWidget>(k)))
                        all.push_back(part);
                }
                return all;
            }();
            return every;
        }
        case BarWidget::Performance: return kPerf;
        case BarWidget::Job: return kJob;
        case BarWidget::Party: return kParty;
        case BarWidget::StatusTimers: return kStatus;
        case BarWidget::World: return kWorld;
        }
        return {};
    }

    BarWidget WidgetOfPart(WidgetPart part)
    {
        for (int k = 0; k < static_cast<int>(BarWidget::Custom); ++k)
            if (const auto kind = static_cast<BarWidget>(k); IconPartOf(kind) == part || Holds(PartsOf(kind), part)) return kind;
        return BarWidget::Custom;
    }

    WidgetDesign DefaultDesign(BarWidget kind) { return Whole(kind, Parsed(kind, kDefaults[static_cast<int>(kind)])); }

    const char* PartFileName(WidgetPart part) { return kFileNames[static_cast<int>(part)]; }

    std::string DesignText(BarWidget kind, const WidgetDesign& design)
    {
        std::string text;
        for (int line = 0; line < kDesignLines; ++line)
        {
            if (line > 0) text += text.empty() ? std::string(1, kLineBreak) : std::string(" ") + kLineBreak;
            for (const DesignChip& chip : design.lines[static_cast<size_t>(line)])
            {
                if (!text.empty()) text += kSpace;
                if (!chip.on) text += kOffMark;
                text += PartKey(kind, chip.part);
            }
        }
        return text;
    }

    WidgetDesign DesignFrom(BarWidget kind, std::string_view text)
    {
        // A design of none of the parts the widget has now, such as one saved before its parts changed, is no design;
        // a created widget's is empty, as its pill may show another widget's parts.
        const WidgetDesign parsed = Parsed(kind, text);
        if (kind == BarWidget::Custom) return text.empty() ? DefaultDesign(kind) : Whole(kind, parsed);
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
        if (kind == BarWidget::Custom) return design;
        for (const WidgetPart part : parts)
            if (std::ranges::find(seen, part) == seen.end()) design.lines.back().push_back({part, false});
        return design;
    }

    bool PutPart(WidgetDesign& design, WidgetPart part, int line, size_t at)
    {
        if (line < 0 || line >= kDesignLines || std::ranges::any_of(design.lines, [&](const auto& l) {
                return std::ranges::find(l, part, &DesignChip::part) != l.end();
            }))
            return false;
        auto& into = design.lines[static_cast<size_t>(line)];
        into.insert(into.begin() + static_cast<std::ptrdiff_t>(std::min(at, into.size())), DesignChip{part, true});
        return true;
    }

    bool TakePart(WidgetDesign& design, WidgetPart part, bool pillShows)
    {
        for (auto& line : design.lines)
            if (const auto chip = std::ranges::find(line, part, &DesignChip::part); chip != line.end())
            {
                if (chip->on && !TurnOff(design, part, pillShows)) return false;
                line.erase(chip);
                return true;
            }
        return false;
    }

    void TurnOnFirst(WidgetDesign& design)
    {
        if (AnyPartOn(design)) return;
        for (auto& line : design.lines)
            if (!line.empty())
            {
                line.front().on = true;
                return;
            }
    }

    bool AnyPartOn(const WidgetDesign& design)
    {
        return std::ranges::any_of(design.lines, [](const auto& line) { return std::ranges::any_of(line, &DesignChip::on); });
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

    bool TurnOff(WidgetDesign& design, WidgetPart part, bool pillShows)
    {
        DesignChip* found = nullptr;
        int on            = 0;
        for (auto& line : design.lines)
            for (DesignChip& chip : line)
            {
                on += chip.on ? 1 : 0;
                if (chip.part == part) found = &chip;
            }
        if (found == nullptr || !found->on || (on <= 1 && !pillShows)) return false;
        found->on = false;
        return true;
    }
}

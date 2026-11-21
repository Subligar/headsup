#include "bar_party.h"

#include "bar_job.h"
#include "zones.h"

#include <algorithm>
#include <iterator>

namespace headsup
{
    namespace
    {
        // By job number, NON (0) to RUN (22): grades.yaml's mp grade is not none.
        constexpr bool kHasMp[] = {false, false, false, true, true, true, false, true, true, false, false, false,
            false, false, false, true, true, false, false, false, true, true, true};
        constexpr int kColumns        = 5;
        constexpr uint8_t kLowPercent = 50, kCriticalPercent = 25, kFullPercent = 100;

        // A figure in its percent's color: red under 25 %, amber under 50 %.
        Span Level(std::string text, uint8_t percent)
        {
            if (percent < kCriticalPercent) return Colored(std::move(text), kAlarmColor);
            if (percent < kLowPercent) return Colored(std::move(text), kWarningColor);
            return Text(std::move(text));
        }

        // Listed by the game before its figures arrive: no job yet.
        bool NoData(const PartyMember& m) { return m.mainJob == 0; }

        std::string Percent(uint8_t percent) { return std::to_string(percent) + "%"; }

        // As the server works it out: floored, but at least 1 while above 0.
        uint32_t PercentOf(uint32_t now, uint32_t max) { return now == 0 ? 0 : std::max(1u, now * kFullPercent / max); }

        // "1,200/1,500", or the figure alone while its max is not known.
        std::string OutOf(uint32_t now, uint32_t max) { return max == 0 ? Grouped(now) : Grouped(now) + "/" + Grouped(max); }

        int Members(const Party& party)
        {
            return static_cast<int>(
                std::count_if(party.begin(), party.end(), [](const auto& m) { return m.has_value(); }));
        }
    }

    bool JobHasMp(uint8_t job) { return job < std::size(kHasMp) && kHasMp[job]; }

    bool UsesMp(const PartyMember& m) { return JobHasMp(m.mainJob) || (m.subLevel > 0 && JobHasMp(m.subJob)); }

    uint32_t PoolMax(uint32_t now, uint8_t percent, uint32_t known)
    {
        if (percent >= kFullPercent) return now;
        if (known >= now && known > 0 && PercentOf(now, known) == percent) return known;
        return percent == 0 ? 0 : now * kFullPercent / percent;
    }

    std::vector<PartContent> PartyParts(const Party& party, WidgetContent& base)
    {
        const int count = Members(party);
        if (count <= 1) return {{WidgetPart::PartyCount, ListOf(Text("Solo"))}};
        std::vector<Span> health;
        std::optional<uint8_t> hp, mp;
        for (const std::optional<PartyMember>& m : party)
        {
            if (!m || !m->here || NoData(*m)) continue;
            hp = std::min(hp.value_or(m->hpPercent), m->hpPercent);
            if (UsesMp(*m)) mp = std::min(mp.value_or(m->mpPercent), m->mpPercent);
        }
        if (hp) health = ListOf(Text("HP ", Tone::Dim), Level(Percent(*hp), *hp));
        if (mp)
        {
            if (!health.empty()) health.push_back(Text(kDot, Tone::Dim));
            health.push_back(Text("MP ", Tone::Dim));
            health.push_back(Level(Percent(*mp), *mp));
        }
        base.hasPopup = true;
        base.figures  = true;
        return {{WidgetPart::PartyCount, ListOf(Text("Party " + std::to_string(count)))},
            {WidgetPart::PartyHealth, std::move(health)}};
    }

    PopupContent PartyPopup(const Party& party)
    {
        PopupContent popup;
        popup.title   = ListOf(Text("Party"));
        PopupRow grid = Grid(kColumns, kColumns * (kPartySize + 1));
        for (const char* heading : {"Name", "Job & Level", "HP", "MP", "TP"})
            grid.cells.push_back(Cell(Text(heading, Tone::Dim)));
        std::vector<PopupRow> away; // a member elsewhere: one row of name and zone, after the grid
        for (const std::optional<PartyMember>& m : party)
        {
            if (!m) continue;
            if (!m->here)
            {
                // Zone 0: changing zones, or their data has not arrived.
                const std::string where = m->zone == 0 ? "Zoning" : ZoneTitle(m->zone);
                away.push_back(PopupRow{RowKind::Pair, 0, ListOf(Cell(Text(m->name)), Cell(Text(where, Tone::Dim)))});
                continue;
            }
            grid.cells.push_back(Cell(Text(m->name)));
            if (NoData(*m))
            {
                for (int c = 1; c < kColumns; ++c)
                    grid.cells.push_back(Cell(Text("-", Tone::Dim)));
                continue;
            }
            grid.cells.push_back(Cell(Text(JobsText(m->mainJob, m->mainLevel, m->subJob, m->subLevel), Tone::Dim)));
            grid.cells.push_back(Cell(Level(OutOf(m->hp, m->hpMax), m->hpPercent)));
            grid.cells.push_back(Cell(UsesMp(*m) ? Level(OutOf(m->mp, m->mpMax), m->mpPercent) : Text("-", Tone::Dim)));
            grid.cells.push_back(
                Cell(m->tp >= kWeaponSkillTp ? Colored(Grouped(m->tp), kReadyColor) : Text(Grouped(m->tp))));
        }
        PopupSection rows = Section(CardKind::PartyMembers, "", ListOf(std::move(grid)));
        for (PopupRow& row : away)
            rows.rows.push_back(std::move(row));
        popup.sections = ListOf(std::move(rows));
        return popup;
    }
}

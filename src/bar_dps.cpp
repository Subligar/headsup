#include "bar_dps.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace headsup
{
    void DamageMeter::SetFighters(std::vector<Fighter> fighters) { m_Fighters = std::move(fighters); }

    void DamageMeter::Add(double now, uint32_t actor, uint32_t damage)
    {
        while (!m_Hits.empty() && now - m_Hits.front().at >= kMaxDpsSeconds)
            m_Hits.pop_front();
        if (damage == 0) return;
        for (const Fighter& f : m_Fighters)
        {
            const bool pet = f.pet == actor;
            if (f.id == actor || pet)
            {
                m_Hits.push_back({now, f.id, damage, pet});
                return;
            }
        }
    }

    std::vector<DpsRow> DamageMeter::Rows(double now, int seconds, bool pets) const
    {
        std::vector<double> dealt(m_Fighters.size(), 0.0);
        for (const Hit& hit : m_Hits)
        {
            if (now - hit.at >= seconds || (hit.pet && !pets)) continue;
            const auto f = std::ranges::find(m_Fighters, hit.fighter, &Fighter::id);
            if (f != m_Fighters.end()) dealt[static_cast<size_t>(f - m_Fighters.begin())] += hit.damage;
        }
        std::vector<DpsRow> rows;
        for (size_t i = 0; i < m_Fighters.size(); ++i)
            if (dealt[i] > 0) rows.push_back({m_Fighters[i].name, dealt[i] / seconds, i == 0});
        std::ranges::stable_sort(rows, std::ranges::greater{}, &DpsRow::dps);
        return rows;
    }

    namespace
    {
        std::string Figure(double dps) { return Grouped(static_cast<uint64_t>(std::lround(dps))); }
    }

    std::vector<PartContent> DpsParts(const std::vector<DpsRow>& rows, bool party, WidgetContent& base)
    {
        double yours = 0.0, total = 0.0;
        for (const DpsRow& row : rows)
        {
            if (row.you) yours = row.dps;
            total += row.dps;
        }
        base.figures  = true;
        base.hasPopup = !rows.empty();
        return {{WidgetPart::DpsYou, ListOf(Text(Figure(yours)), Text(" DPS", Tone::Dim))},
            {WidgetPart::DpsParty, party ? ListOf(Text("Party ", Tone::Dim), Text(Figure(total))) : std::vector<Span>{}}};
    }

    PopupContent DpsPopup(const std::vector<DpsRow>& rows)
    {
        constexpr int kColumns = 2; // name and DPS
        PopupContent popup;
        popup.title = ListOf(Text("DPS"));
        if (rows.empty()) return popup;
        PopupRow grid = Grid(kColumns, kColumns * rows.size());
        for (const DpsRow& row : rows)
        {
            grid.cells.push_back(Cell(Text(row.name)));
            PopupCell figure = Cell(Text(Figure(row.dps)));
            figure.align     = Align::Right;
            grid.cells.push_back(std::move(figure));
        }
        popup.sections = ListOf(Section(CardKind::DpsMembers, "", ListOf(std::move(grid))));
        return popup;
    }
}

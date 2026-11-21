#include "bar_dps.h"
#include "test.h"

#include <string>
#include <vector>

using namespace headsup;

namespace
{
    constexpr uint32_t kYou = 1, kYourPet = 11, kBo = 2, kMob = 17000000;

    // You with your pet and Bo with none; you 100 at 0 s and 50 at 4 s, your pet 300 at 5 s, Bo 200 at 6 s, and a mob's
    // 999 at 6 s, which is no one's.
    DamageMeter Fought()
    {
        DamageMeter meter;
        meter.SetFighters({{kYou, kYourPet, "You"}, {kBo, 0, "Bo"}});
        meter.Add(0.0, kYou, 100);
        meter.Add(4.0, kYou, 50);
        meter.Add(5.0, kYourPet, 300);
        meter.Add(6.0, kBo, 200);
        meter.Add(6.0, kMob, 999);
        return meter;
    }

    std::string Joined(const std::vector<Span>& spans)
    {
        std::string text;
        for (const Span& s : spans)
            text += s.text;
        return text;
    }

    const std::vector<Span>& SpansOf(const std::vector<PartContent>& parts, WidgetPart part)
    {
        for (const PartContent& p : parts)
            if (p.part == part) return p.spans;
        static const std::vector<Span> none;
        return none;
    }
}

TEST(dps_is_the_damage_of_the_last_seconds_over_those_seconds_highest_first)
{
    const DamageMeter meter = Fought();
    // You 450 over 10 s, Bo 200.
    CHECK(meter.Rows(9.0, 10, true) == (std::vector<DpsRow>{{"You", 45.0, true}, {"Bo", 20.0, false}}));
    // Without pets your 300 is out, and Bo is first.
    CHECK(meter.Rows(9.0, 10, false) == (std::vector<DpsRow>{{"Bo", 20.0, false}, {"You", 15.0, true}}));
    // Over 5 s only the hits at 5 s and 6 s are under 5 s old.
    CHECK(meter.Rows(9.0, 5, true) == (std::vector<DpsRow>{{"You", 60.0, true}, {"Bo", 40.0, false}}));
    // A hit exactly 10 s old is out.
    CHECK(meter.Rows(10.0, 10, true) == (std::vector<DpsRow>{{"You", 35.0, true}, {"Bo", 20.0, false}}));
    CHECK(meter.Rows(70.0, 10, true).empty());
}

TEST(a_pets_damage_stays_its_owners_after_it_is_gone_and_no_one_elses_counts)
{
    DamageMeter meter = Fought();
    meter.SetFighters({{kYou, 0, "You"}, {kBo, 0, "Bo"}}); // your pet dismissed
    meter.Add(7.0, kYourPet, 1000);
    CHECK(meter.Rows(9.0, 10, true) == (std::vector<DpsRow>{{"You", 45.0, true}, {"Bo", 20.0, false}}));
    // Bo left the party: his hits have no row.
    meter.SetFighters({{kYou, 0, "You"}});
    CHECK(meter.Rows(9.0, 10, true) == (std::vector<DpsRow>{{"You", 45.0, true}}));
    // With no fighters, as while no DPS widget shows, nothing is kept.
    DamageMeter idle;
    idle.Add(1.0, kYou, 100);
    idle.SetFighters({{kYou, 0, "You"}});
    CHECK(idle.Rows(2.0, 10, true).empty());
}

TEST(the_dps_widget_shows_yours_over_the_partys_and_pops_out_each_member)
{
    const std::vector<DpsRow> rows = Fought().Rows(9.0, 10, true);
    WidgetContent base;
    std::vector<PartContent> parts = DpsParts(rows, true, base);
    const std::vector<Span>& yours = SpansOf(parts, WidgetPart::DpsYou);
    CHECK_EQ(Joined(yours), "45 DPS");
    CHECK(yours.size() == 2 && yours[0].tone == Tone::Normal && yours[1].tone == Tone::Dim);
    const std::vector<Span>& party = SpansOf(parts, WidgetPart::DpsParty);
    CHECK_EQ(Joined(party), "Party 65");
    CHECK(party.size() == 2 && party[0].tone == Tone::Dim && party[1].tone == Tone::Normal);
    CHECK(base.figures && base.hasPopup);

    const PopupContent popup = DpsPopup(rows);
    CHECK_EQ(Joined(popup.title), "DPS");
    CHECK(popup.sections.size() == 1 && popup.sections[0].card == CardKind::DpsMembers);
    std::vector<std::string> cells;
    for (const PopupCell& cell : popup.sections[0].rows.at(0).cells)
        cells.push_back(Joined(cell.text));
    CHECK(cells == (std::vector<std::string>{"You", "45", "Bo", "20"}));

    // Alone, no party line; with no damage, no pop-out.
    WidgetContent alone;
    parts = DpsParts(rows, false, alone);
    CHECK(SpansOf(parts, WidgetPart::DpsParty).empty());
    WidgetContent quiet;
    parts = DpsParts({}, true, quiet);
    CHECK_EQ(Joined(SpansOf(parts, WidgetPart::DpsYou)), "0 DPS");
    CHECK_EQ(Joined(SpansOf(parts, WidgetPart::DpsParty)), "Party 0");
    CHECK(!quiet.hasPopup && DpsPopup({}).sections.empty());
    // Figures are grouped, and only Bo's row is no reason to drop yours.
    WidgetContent big;
    parts = DpsParts({{"Bo", 1234.4, false}}, true, big);
    CHECK_EQ(Joined(SpansOf(parts, WidgetPart::DpsYou)), "0 DPS");
    CHECK_EQ(Joined(SpansOf(parts, WidgetPart::DpsParty)), "Party 1,234");
}

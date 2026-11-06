#include "bar_design.h"
#include "test.h"

#include <algorithm>
#include <vector>

using namespace headsup;

namespace
{
    std::vector<WidgetPart> PartsIn(const WidgetDesign& d)
    {
        std::vector<WidgetPart> parts;
        for (const auto& line : d.lines)
            for (const DesignChip& chip : line)
                parts.push_back(chip.part);
        return parts;
    }
}

TEST(every_widget_but_game_help_has_parts_and_a_default_design_holding_each_once)
{
    for (int k = 0; k < kBarWidgetCount; ++k)
    {
        const auto kind = static_cast<BarWidget>(k);
        std::vector<WidgetPart> held = PartsIn(DefaultDesign(kind)), parts(PartsOf(kind).begin(), PartsOf(kind).end());
        CHECK_EQ(parts.empty(), kind == BarWidget::GameHelp);
        std::ranges::sort(held);
        std::ranges::sort(parts);
        CHECK(held == parts);
    }
}

TEST(a_design_saves_as_text_and_loads_back)
{
    CHECK_EQ(DesignText(DefaultDesign(BarWidget::Zone)), "name | region crest nation");
    CHECK_EQ(DesignText(DefaultDesign(BarWidget::EarthTime)), "time | -server -vana");
    const char* const job = "-jobs exp bar |";
    CHECK_EQ(DesignText(DesignFrom(BarWidget::Job, job)), job);
    for (int k = 0; k < kBarWidgetCount; ++k)
    {
        const auto kind = static_cast<BarWidget>(k);
        CHECK(DesignFrom(kind, DesignText(DefaultDesign(kind))) == DefaultDesign(kind));
    }
    // Nothing saved: the default.
    CHECK(DesignFrom(BarWidget::Weather, "") == DefaultDesign(BarWidget::Weather));
}

TEST(a_design_from_text_keeps_each_part_once_and_adds_the_missing_off)
{
    const WidgetDesign d = DesignFrom(BarWidget::Zone, "crest crest nonsense amount | name");
    CHECK((d.lines[0] == std::vector<DesignChip>{{WidgetPart::ZoneCrest, true}}));
    CHECK((d.lines[1] == std::vector<DesignChip>{{WidgetPart::ZoneName, true}, {WidgetPart::ZoneRegion, false},
                             {WidgetPart::ZoneNation, false}}));
    // Never all off: the first part comes back on.
    const WidgetDesign off = DesignFrom(BarWidget::Zone, "-name -region -crest -nation |");
    CHECK(PartOn(off, WidgetPart::ZoneName) && !PartOn(off, WidgetPart::ZoneRegion));
}

TEST(a_part_moves_within_and_across_lines)
{
    WidgetDesign d = DefaultDesign(BarWidget::Zone); // name | region crest nation
    CHECK(MovePart(d, 1, 2, 0, 0)); // the nation before the name
    CHECK_EQ(DesignText(d), "nation name | region crest");
    CHECK(MovePart(d, 0, 0, 1, 2)); // and back to the end of line 2
    CHECK_EQ(DesignText(d), "name | region crest nation");
    CHECK(MovePart(d, 1, 0, 1, 3)); // the region to the end of its own line
    CHECK_EQ(DesignText(d), "name | crest nation region");
    CHECK(!MovePart(d, 1, 0, 1, 0)); // onto its own place
    CHECK(!MovePart(d, 1, 0, 1, 1)); // or just after itself
    CHECK(!MovePart(d, 0, 5, 1, 0)); // a part that is not there
    CHECK_EQ(DesignText(d), "name | crest nation region");
}

TEST(the_last_part_on_cannot_be_turned_off)
{
    WidgetDesign d = DesignFrom(BarWidget::Bags, "count | -bag");
    CHECK(!TurnOff(d, WidgetPart::BagCount));
    CHECK(PartOn(d, WidgetPart::BagCount));
    d = DefaultDesign(BarWidget::Bags);
    CHECK(TurnOff(d, WidgetPart::BagName) && !PartOn(d, WidgetPart::BagName));
}

namespace
{
    Span IconSpan(Icon icon)
    {
        Span s = Text("");
        s.icon = icon;
        return s;
    }

    WidgetContent Composed(BarWidget kind, std::string_view design, std::vector<PartContent> parts)
    {
        WidgetContent base;
        base.shown = true;
        return ComposeContent(DesignFrom(kind, design), std::move(parts), base);
    }

    std::vector<PartContent> WeatherParts()
    {
        return {{WidgetPart::WeatherIcon, {IconSpan(Icon::Fire)}}, {WidgetPart::WeatherName, {Text("Hot Spells")}},
            {WidgetPart::VanaMoon, {Text("35%", Tone::Dim)}}};
    }
}

TEST(icon_parts_first_on_the_top_line_lead_and_elsewhere_sit_inline)
{
    const WidgetContent leading = Composed(BarWidget::Weather, "icon weather | moon", WeatherParts());
    CHECK((leading.icons == std::vector<Icon>{Icon::Fire}));
    CHECK((leading.title == std::vector<Span>{Text("Hot Spells")}));
    CHECK((leading.subtitle == std::vector<Span>{Text("35%", Tone::Dim)}));
    // After the name the icon is part of the line, a part's gap before it.
    const WidgetContent after = Composed(BarWidget::Weather, "weather icon | moon", WeatherParts());
    CHECK(after.icons.empty());
    CHECK((after.title == std::vector<Span>{Text("Hot Spells"), Text("  "), IconSpan(Icon::Fire)}));
    // On the second line too.
    const WidgetContent below = Composed(BarWidget::Weather, "weather | moon icon", WeatherParts());
    CHECK(below.icons.empty() && below.subtitle.back() == IconSpan(Icon::Fire));
    // Off, nowhere.
    CHECK(Composed(BarWidget::Weather, "-icon weather | moon", WeatherParts()).icons.empty());
}

TEST(an_empty_first_line_lets_the_second_draw_alone)
{
    const WidgetContent w = Composed(BarWidget::Weather, "-weather | moon", WeatherParts());
    CHECK((w.title == std::vector<Span>{Text("35%", Tone::Dim)}) && w.subtitle.empty());
}

TEST(a_part_with_nothing_to_show_takes_no_gap)
{
    // Gil has no currency name.
    const std::vector<PartContent> gil = {{WidgetPart::CurrencyCoin, {}}, {WidgetPart::CurrencyAmount, {Text("1,200")}},
        {WidgetPart::CurrencyName, {}}};
    const WidgetContent w = Composed(BarWidget::Currency, "amount coin currency |", gil);
    CHECK((w.title == std::vector<Span>{Text("1,200")}) && w.subtitle.empty() && !w.coin);
}

TEST(a_narrow_target_cuts_its_name_wherever_it_sits)
{
    Span aggro = Text("");
    aggro.icon = Icon::AggroNQ;
    CHECK_EQ(WidestTextSpan({aggro, Text("Goblin Thug"), Text("  (4.2)", Tone::Dim)}), 1u);
    CHECK_EQ(WidestTextSpan({Text("  (4.2)", Tone::Dim), Text("  "), Text("Goblin Thug")}), 2u);
    CHECK_EQ(WidestTextSpan({aggro}), 1u);
    // A short name beside a longer distance: the name is still the one cut, as the gaps and the dim distance are not.
    CHECK_EQ(WidestTextSpan({Text("Bat", Tone::Own), Text("  (12.3)", Tone::Dim)}), 0u);
    CHECK_EQ(WidestTextSpan({Text("  "), aggro, Text("  Crab")}), 2u);
    CHECK_EQ(WidestTextSpan({}), 0u);
}

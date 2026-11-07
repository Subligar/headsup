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

TEST(a_widgets_icon_is_a_setting_of_its_own_and_no_part_of_its_design)
{
    CHECK(IconPartOf(BarWidget::Zone) == std::optional(WidgetPart::ZoneCrest));
    CHECK(IconPartOf(BarWidget::Weather) == std::optional(WidgetPart::WeatherIcon));
    CHECK(IconPartOf(BarWidget::Target) == std::optional(WidgetPart::TargetAggro));
    CHECK(IconPartOf(BarWidget::Currency) == std::optional(WidgetPart::CurrencyCoin));
    CHECK(IconPartOf(BarWidget::Bags) == std::optional(WidgetPart::BagIcon));
    CHECK(IconPartOf(BarWidget::EarthTime) == std::optional(WidgetPart::ClockIcon));
    CHECK(!IconPartOf(BarWidget::Performance) && !IconPartOf(BarWidget::GameHelp));
    for (int k = 0; k < kBarWidgetCount; ++k)
    {
        const auto kind = static_cast<BarWidget>(k);
        if (const std::optional<WidgetPart> icon = IconPartOf(kind)) CHECK(std::ranges::find(PartsOf(kind), *icon) == PartsOf(kind).end());
    }
}

TEST(a_design_saves_as_text_and_loads_back)
{
    CHECK_EQ(DesignText(DefaultDesign(BarWidget::Zone)), "name | region nation");
    CHECK_EQ(DesignText(DefaultDesign(BarWidget::EarthTime)), "time | -server -vana -date");
    const char* const job = "-jobs tonext bar | -earned -percent";
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
    const WidgetDesign d = DesignFrom(BarWidget::Zone, "nation nation nonsense amount crest | name");
    CHECK((d.lines[0] == std::vector<DesignChip>{{WidgetPart::ZoneNation, true}}));
    CHECK((d.lines[1] == std::vector<DesignChip>{{WidgetPart::ZoneName, true}, {WidgetPart::ZoneRegion, false}}));
    // Never all off: the first part comes back on.
    const WidgetDesign off = DesignFrom(BarWidget::Zone, "-name -region -nation |");
    CHECK(PartOn(off, WidgetPart::ZoneName) && !PartOn(off, WidgetPart::ZoneRegion));
}

TEST(a_part_moves_within_and_across_lines)
{
    WidgetDesign d = DefaultDesign(BarWidget::Zone); // name | region nation
    CHECK(MovePart(d, 1, 1, 0, 0)); // the nation before the name
    CHECK_EQ(DesignText(d), "nation name | region");
    CHECK(MovePart(d, 0, 0, 1, 1)); // and back to the end of line 2
    CHECK_EQ(DesignText(d), "name | region nation");
    CHECK(MovePart(d, 1, 0, 1, 2)); // the region to the end of its own line
    CHECK_EQ(DesignText(d), "name | nation region");
    CHECK(!MovePart(d, 1, 0, 1, 0)); // onto its own place
    CHECK(!MovePart(d, 1, 0, 1, 1)); // or just after itself
    CHECK(!MovePart(d, 0, 5, 1, 0)); // a part that is not there
    CHECK_EQ(DesignText(d), "name | nation region");
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
        return {{WidgetPart::WeatherName, {Text("Hot Spells")}}, {WidgetPart::VanaMoon, {Text("35%", Tone::Dim)}}};
    }

    std::vector<PartContent> TargetParts()
    {
        return {{WidgetPart::TargetDetection, {IconSpan(Icon::Sight)}}, {WidgetPart::TargetName, {Text("Goblin")}},
            {WidgetPart::TargetLevel, {Text("Lv. 12", Tone::Dim)}}};
    }
}

TEST(icon_parts_first_on_the_top_line_lead_and_elsewhere_sit_inline)
{
    const WidgetContent leading = Composed(BarWidget::Target, "detection name | level", TargetParts());
    CHECK((leading.icons == std::vector<Icon>{Icon::Sight}));
    CHECK((leading.title == std::vector<Span>{Text("Goblin")}));
    CHECK((leading.subtitle == std::vector<Span>{Text("Lv. 12", Tone::Dim)}));
    // After the name the icon is part of the line, a part's gap before it.
    const WidgetContent after = Composed(BarWidget::Target, "name detection | level", TargetParts());
    CHECK(after.icons.empty());
    CHECK((after.title == std::vector<Span>{Text("Goblin"), Text("  "), IconSpan(Icon::Sight)}));
    // On the second line too.
    const WidgetContent below = Composed(BarWidget::Target, "name | level detection", TargetParts());
    CHECK(below.icons.empty() && below.subtitle.back() == IconSpan(Icon::Sight));
    // Off, nowhere.
    CHECK(Composed(BarWidget::Target, "-detection name | level", TargetParts()).icons.empty());
}

TEST(an_empty_first_line_lets_the_second_draw_alone)
{
    const WidgetContent w = Composed(BarWidget::Weather, "-weather | moon", WeatherParts());
    CHECK((w.title == std::vector<Span>{Text("35%", Tone::Dim)}) && w.subtitle.empty());
}

TEST(a_saved_design_of_no_part_its_widget_still_has_is_the_default)
{
    CHECK(DesignFrom(BarWidget::Currency, "amount | currency") == DefaultDesign(BarWidget::Currency));
    CHECK(DesignFrom(BarWidget::Currency, "amount | currency name2") != DefaultDesign(BarWidget::Currency));
    CHECK(PartOn(DesignFrom(BarWidget::Currency, "amount | currency name2"), WidgetPart::Currency2Name));
}

TEST(a_part_with_nothing_to_show_takes_no_gap)
{
    // Most currencies have no picture of their own.
    const std::vector<PartContent> notes = {{WidgetPart::Currency1Amount, {Text("1,200")}}, {WidgetPart::Currency1Icon, {}}};
    const WidgetContent w = Composed(BarWidget::Currency, "amount1 icon1 |", notes);
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

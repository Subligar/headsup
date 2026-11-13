#include "bar_design.h"
#include "test.h"

#include <algorithm>
#include <string>
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

TEST(every_widget_but_game_help_and_settings_has_parts_and_a_default_design_holding_each_once)
{
    for (int k = 0; k < kBarWidgetCount; ++k)
    {
        const auto kind = static_cast<BarWidget>(k);
        std::vector<WidgetPart> held = PartsIn(DefaultDesign(kind)), parts(PartsOf(kind).begin(), PartsOf(kind).end());
        CHECK_EQ(parts.empty(), kind == BarWidget::GameHelp || kind == BarWidget::Settings);
        if (kind == BarWidget::Custom) continue; // it holds only the parts placed in it
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
    CHECK(IconPartOf(BarWidget::HomePoint) == std::optional(WidgetPart::HomePointIcon));
    CHECK(WidgetOfPart(WidgetPart::HomePointIcon) == BarWidget::HomePoint);
    CHECK_EQ(DesignText(BarWidget::HomePoint, DefaultDesign(BarWidget::HomePoint)), "name |");
    CHECK(!IconPartOf(BarWidget::Performance) && !IconPartOf(BarWidget::GameHelp));
    for (int k = 0; k < kBarWidgetCount; ++k)
    {
        const auto kind = static_cast<BarWidget>(k);
        if (const std::optional<WidgetPart> icon = IconPartOf(kind)) CHECK(std::ranges::find(PartsOf(kind), *icon) == PartsOf(kind).end());
    }
}

TEST(a_design_saves_as_text_and_loads_back)
{
    CHECK_EQ(DesignText(BarWidget::Zone, DefaultDesign(BarWidget::Zone)), "name | region nation");
    CHECK_EQ(DesignText(BarWidget::EarthTime, DefaultDesign(BarWidget::EarthTime)), "time | -server -vana -date");
    const char* const job = "-jobs tonext bar | -earned -percent";
    CHECK_EQ(DesignText(BarWidget::Job, DesignFrom(BarWidget::Job, job)), job);
    for (int k = 0; k < kBarWidgetCount; ++k)
    {
        const auto kind = static_cast<BarWidget>(k);
        CHECK(DesignFrom(kind, DesignText(kind, DefaultDesign(kind))) == DefaultDesign(kind));
    }
    // Nothing saved: the default.
    CHECK(DesignFrom(BarWidget::Weather, "") == DefaultDesign(BarWidget::Weather));
}

TEST(a_design_from_text_keeps_each_part_once_and_adds_the_missing_off)
{
    const WidgetDesign d = DesignFrom(BarWidget::Zone, "nation nation nonsense amount crest | name");
    CHECK((d.lines[0] == std::vector<DesignChip>{{WidgetPart::ZoneNation, true}}));
    CHECK((d.lines[1] == std::vector<DesignChip>{{WidgetPart::ZoneName, true}, {WidgetPart::ZoneRegion, false}}));
    // All off stays so: Clamp keeps one on where the widget's pill needs it.
    const WidgetDesign off = DesignFrom(BarWidget::Zone, "-name -region -nation |");
    CHECK(!PartOn(off, WidgetPart::ZoneName) && !PartOn(off, WidgetPart::ZoneRegion) && !PartOn(off, WidgetPart::ZoneNation));
}

TEST(a_part_moves_within_and_across_lines)
{
    WidgetDesign d = DefaultDesign(BarWidget::Zone); // name | region nation
    CHECK(MovePart(d, 1, 1, 0, 0)); // the nation before the name
    CHECK_EQ(DesignText(BarWidget::Zone, d), "nation name | region");
    CHECK(MovePart(d, 0, 0, 1, 1)); // and back to the end of line 2
    CHECK_EQ(DesignText(BarWidget::Zone, d), "name | region nation");
    CHECK(MovePart(d, 1, 0, 1, 2)); // the region to the end of its own line
    CHECK_EQ(DesignText(BarWidget::Zone, d), "name | nation region");
    CHECK(!MovePart(d, 1, 0, 1, 0)); // onto its own place
    CHECK(!MovePart(d, 1, 0, 1, 1)); // or just after itself
    CHECK(!MovePart(d, 0, 5, 1, 0)); // a part that is not there
    CHECK_EQ(DesignText(BarWidget::Zone, d), "name | nation region");
}

TEST(the_last_part_on_turns_off_only_when_the_widgets_pill_shows_another)
{
    WidgetDesign d = DesignFrom(BarWidget::Bags, "count1 | -name1");
    CHECK(!TurnOff(d, WidgetPart::Bag1Count, false));
    CHECK(PartOn(d, WidgetPart::Bag1Count));
    CHECK(TurnOff(d, WidgetPart::Bag1Count, true) && !PartOn(d, WidgetPart::Bag1Count));
    d = DefaultDesign(BarWidget::Bags);
    CHECK(TurnOff(d, WidgetPart::Bag1Name, false) && !PartOn(d, WidgetPart::Bag1Name));
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
    CHECK((w.title == std::vector<Span>{Text("1,200")}) && w.subtitle.empty() && w.icons.empty());
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
    // A distance alone keeps its room, so it is never the one cut.
    Span far = Text("(12.3)", Tone::Dim);
    far.room = {"(99.9)"};
    CHECK_EQ(WidestTextSpan({far}), 1u);
}

namespace
{
    std::vector<CardKind> CardsIn(const PopupDesign& d)
    {
        std::vector<CardKind> cards;
        for (const auto& row : d.rows)
            cards.insert(cards.end(), row.begin(), row.end());
        return cards;
    }
}

TEST(every_pop_out_card_is_one_widgets_and_saves_by_a_name_of_its_own)
{
    std::vector<std::string> names;
    size_t listed = 0;
    for (int k = 0; k < kBarWidgetCount; ++k)
    {
        const auto kind = static_cast<BarWidget>(k);
        const std::vector<CardKind> cards = CardsOf(kind);
        CHECK_EQ(cards.empty(), kind == BarWidget::GameHelp || kind == BarWidget::Settings || kind == BarWidget::HomePoint ||
                                    kind == BarWidget::Custom);
        for (const CardKind card : cards)
            CHECK(WidgetOfCard(card) == kind);
        listed += cards.size();
    }
    CHECK_EQ(listed, static_cast<size_t>(kCardKindCount));
    for (int c = 0; c < kCardKindCount; ++c)
        names.emplace_back(CardFileName(static_cast<CardKind>(c)));
    std::ranges::sort(names);
    CHECK(std::ranges::adjacent_find(names) == names.end());
}

TEST(each_widgets_default_pop_out_holds_its_own_cards_once_at_most_three_a_row)
{
    for (int k = 0; k < kBarWidgetCount; ++k)
    {
        const auto kind = static_cast<BarWidget>(k);
        const PopupDesign d = DefaultPopupDesign(kind);
        std::vector<CardKind> held = CardsIn(d), own = CardsOf(kind);
        for (const auto& row : d.rows)
            CHECK(!row.empty() && row.size() <= kMostCardsInRow);
        std::ranges::sort(held);
        CHECK(held == own);
    }
    // Forecast's as its pop-out was: Today beside the forecast, By Element under both.
    CHECK((DefaultPopupDesign(BarWidget::Weather).rows ==
           std::vector<std::vector<CardKind>>{{CardKind::WeatherToday, CardKind::WeatherForecast}, {CardKind::WeatherByElement}}));
}

TEST(a_pop_out_design_saves_as_text_and_loads_back)
{
    PopupDesign d{{{CardKind::ZoneInfluence, CardKind::WeatherToday}, {CardKind::CurrencyFavorites}}};
    const std::string text = PopupDesignText(d);
    CHECK_EQ(text, "zone.influence weather.today | gil.favorites");
    CHECK(PopupDesignFrom(BarWidget::Zone, text) == d);
    for (int k = 0; k < kBarWidgetCount; ++k)
    {
        const auto kind = static_cast<BarWidget>(k);
        CHECK(PopupDesignFrom(kind, PopupDesignText(DefaultPopupDesign(kind))) == DefaultPopupDesign(kind));
    }
    // Nothing saved is the default; a design of no cards saves as one.
    CHECK(PopupDesignFrom(BarWidget::Job, "") == DefaultPopupDesign(BarWidget::Job));
    CHECK(PopupDesignFrom(BarWidget::Job, PopupDesignText(PopupDesign{})).rows.empty());
}

TEST(a_pop_out_design_from_text_drops_unknown_and_repeated_cards_a_fourth_in_a_row_and_empty_rows)
{
    const PopupDesign d = PopupDesignFrom(BarWidget::Zone,
        "zone.influence nonsense zone.influence weather.today world.guilds job.exp party.members | | world.guilds bags.list");
    CHECK((d.rows == std::vector<std::vector<CardKind>>{
                         {CardKind::ZoneInfluence, CardKind::WeatherToday, CardKind::WorldGuilds}, {CardKind::BagsList}}));
}

TEST(a_card_moves_within_and_across_rows_and_into_a_new_one)
{
    PopupDesign d{{{CardKind::WorldConquest, CardKind::WorldTravel, CardKind::WorldCalendar}, {CardKind::WorldGuilds}}};
    CHECK(PlaceCard(d, CardKind::WorldCalendar, 0, 0)); // first in its row
    CHECK_EQ(PopupDesignText(d), "world.calendar world.conquest world.travel | world.guilds");
    CHECK(!PlaceCard(d, CardKind::WorldConquest, 0, 1)); // onto its own place
    CHECK(!PlaceCard(d, CardKind::WorldConquest, 0, 2)); // or just after itself
    CHECK(PlaceCard(d, CardKind::WorldGuilds, 2, 0)); // a row of its own after the others: its old one goes
    CHECK_EQ(PopupDesignText(d), "world.calendar world.conquest world.travel | world.guilds");
    CHECK(PlaceCard(d, CardKind::WorldTravel, 1, 5)); // to the end of the next row
    CHECK_EQ(PopupDesignText(d), "world.calendar world.conquest | world.guilds world.travel");
    CHECK(PlaceCard(d, CardKind::ZoneInfluence, 2, 0)); // one from the palette, in a new row
    CHECK_EQ(PopupDesignText(d), "world.calendar world.conquest | world.guilds world.travel | zone.influence");
    CHECK(RemoveCard(d, CardKind::WorldConquest) && !RemoveCard(d, CardKind::WorldConquest));
    CHECK(RemoveCard(d, CardKind::ZoneInfluence)); // its row goes with it
    CHECK_EQ(PopupDesignText(d), "world.calendar | world.guilds world.travel");
}

TEST(a_full_row_takes_no_fourth_card_but_moves_its_own)
{
    PopupDesign d{{{CardKind::WorldConquest, CardKind::WorldTravel, CardKind::WorldCalendar}, {CardKind::WorldGuilds}}};
    CHECK(!RowTakes(d, 0, CardKind::WorldGuilds) && RowTakes(d, 0, CardKind::WorldTravel));
    CHECK(RowTakes(d, 1, CardKind::WorldConquest) && RowTakes(d, 2, CardKind::WorldGuilds));
    CHECK(!PlaceCard(d, CardKind::WorldGuilds, 0, 0));
    CHECK_EQ(PopupDesignText(d), "world.conquest world.travel world.calendar | world.guilds");
    CHECK(PlaceCard(d, CardKind::WorldConquest, 0, 3));
    CHECK_EQ(PopupDesignText(d), "world.travel world.calendar world.conquest | world.guilds");
}

TEST(a_created_widget_offers_every_part_of_every_widget_once_and_starts_with_its_name_alone)
{
    const std::span<const WidgetPart> parts = PartsOf(BarWidget::Custom);
    std::vector<WidgetPart> sorted(parts.begin(), parts.end());
    std::ranges::sort(sorted);
    CHECK_EQ(sorted.size(), static_cast<size_t>(kWidgetPartCount));
    for (int p = 0; p < kWidgetPartCount; ++p)
        CHECK(sorted[static_cast<size_t>(p)] == static_cast<WidgetPart>(p));
    CHECK(PartsIn(DefaultDesign(BarWidget::Custom)) == std::vector{WidgetPart::CustomName});
    CHECK(PartOn(DefaultDesign(BarWidget::Custom), WidgetPart::CustomName));
    // Each part is one widget's, its icon too; the Name is the created widget's own.
    CHECK(WidgetOfPart(WidgetPart::ZoneName) == BarWidget::Zone && WidgetOfPart(WidgetPart::ZoneCrest) == BarWidget::Zone);
    CHECK(WidgetOfPart(WidgetPart::CurrencyCoin) == BarWidget::Currency && WidgetOfPart(WidgetPart::WorldBeastmen) == BarWidget::World);
    CHECK(WidgetOfPart(WidgetPart::CustomName) == BarWidget::Custom);
    for (int k = 0; k < kBarWidgetCount; ++k)
        for (const WidgetPart part : PartsOf(static_cast<BarWidget>(k)))
            CHECK(static_cast<BarWidget>(k) == BarWidget::Custom || WidgetOfPart(part) == static_cast<BarWidget>(k));
}

TEST(a_created_widgets_design_names_each_part_with_its_widget_and_keeps_only_the_parts_placed)
{
    const std::string text = "name zone.name gil.amount1 | -target.name zone.crest";
    const WidgetDesign d   = DesignFrom(BarWidget::Custom, text);
    CHECK_EQ(DesignText(BarWidget::Custom, d), text);
    CHECK_EQ(PartsIn(d).size(), static_cast<size_t>(5));
    CHECK(PartOn(d, WidgetPart::Currency1Amount) && !PartOn(d, WidgetPart::TargetName) && PartOn(d, WidgetPart::ZoneCrest));
    // A widget's own name for a part means nothing here, and a part placed twice keeps its first place.
    CHECK_EQ(DesignText(BarWidget::Custom, DesignFrom(BarWidget::Custom, "name amount1 zone.name | zone.name")), "name zone.name |");
    CHECK(DesignFrom(BarWidget::Custom, "") == DefaultDesign(BarWidget::Custom));
    // A built-in widget's own parts keep their plain names.
    CHECK_EQ(DesignText(BarWidget::Zone, DefaultDesign(BarWidget::Zone)), "name | region nation");
    CHECK(DesignFrom(BarWidget::Zone, "zone.name | region nation") != DefaultDesign(BarWidget::Zone));
}

TEST(a_created_widgets_saved_design_of_no_part_is_empty_and_only_a_missing_one_is_the_default)
{
    CHECK(DesignFrom(BarWidget::Custom, "|") == WidgetDesign{});
    CHECK_EQ(DesignText(BarWidget::Custom, DesignFrom(BarWidget::Custom, "|")), "|");
    CHECK(DesignFrom(BarWidget::Custom, "") == DefaultDesign(BarWidget::Custom));
    CHECK(DesignFrom(BarWidget::Zone, "|") == DefaultDesign(BarWidget::Zone)); // a built-in keeps its rule
}

TEST(a_part_put_into_a_line_is_on_and_one_taken_out_goes_unless_it_is_the_last_on)
{
    WidgetDesign d = DefaultDesign(BarWidget::Custom); // name |
    CHECK(PutPart(d, WidgetPart::ZoneName, 1, 0));
    CHECK(PutPart(d, WidgetPart::PerfFps, 0, 0));
    CHECK_EQ(DesignText(BarWidget::Custom, d), "performance.fps name | zone.name");
    CHECK(!PutPart(d, WidgetPart::ZoneName, 0, 0)); // held already: a move does that
    CHECK(PutPart(d, WidgetPart::Bag1Count, 0, 99)); // past the end, at the end
    CHECK_EQ(DesignText(BarWidget::Custom, d), "performance.fps name bags.count1 | zone.name");
    CHECK(HoldsPart(d, WidgetPart::ZoneName) && !HoldsPart(d, WidgetPart::WorldShip));
    CHECK(TakePart(d, WidgetPart::PerfFps, false) && TakePart(d, WidgetPart::ZoneName, false));
    CHECK(!TakePart(d, WidgetPart::ZoneName, false)); // not there
    CHECK(TurnOff(d, WidgetPart::Bag1Count, false));
    CHECK(TakePart(d, WidgetPart::Bag1Count, false)); // off, it goes whatever is left
    CHECK(!TakePart(d, WidgetPart::CustomName, false)); // the last on stays
    CHECK(TakePart(d, WidgetPart::CustomName, true) && PartsIn(d).empty()); // unless another of its pill shows
}

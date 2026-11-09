#include "bar_widgets.h"
#include "bar_server_time.h"
#include "ph_timers.h"
#include "labels.h"
#include "popup_layout.h"
#include "test.h"
#include "vanadiel.h"
#include "weather.h"
#include "world_info.h"

#include <cmath>

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <span>
#include <string>
#include <utility>

using namespace headsup;

namespace
{
    WidgetContent Content(PanelWidget widget, const BarState& state, const Settings& settings)
    {
        return BarWidgetContent(widget, state, settings);
    }

    // Gil's picture leads the widget, beside both lines.
    bool LeadsWithGil(const WidgetContent& w) { return std::ranges::find(w.icons, Icon::Gil) != w.icons.end(); }

    constexpr uint32_t kBountyHunter = 17199648; // Goblin Bounty Hunter, zone 103: light -3, dark 0, the rest -2

    ActorInfo Goblin()
    {
        ActorInfo info;
        info.kind     = EntityKind::Mob;
        info.alive    = true;
        info.serverId = kBountyHunter;
        info.index    = kBountyHunter & 0xFFF;
        info.mob      = MobById(kBountyHunter);
        info.level    = MakeLabel(info.mob, nullptr, 20);
        info.state.kind       = EntityKind::Mob;
        info.state.alive      = true;
        info.state.nature     = Nature::Aggressive;
        info.state.classified = Category::WillAttack;
        info.state.row        = Category::WillAttack;
        info.state.toughness  = info.level.shade;
        std::snprintf(info.name, sizeof(info.name), "%s", "Goblin Bounty Hunter");
        return info;
    }

    std::vector<Icon> EveryIcon(const ActorInfo& mob)
    {
        const IconSet icons = IconsFor(mob.mob, kAllDetection);
        return std::vector<Icon>(icons.icons, icons.icons + icons.count);
    }

    std::string Joined(const std::vector<Span>& spans)
    {
        std::string text;
        for (const Span& s : spans)
            text += s.text;
        return text;
    }

    const PopupSection* Section(const PopupContent& popup, const std::string& heading)
    {
        for (const PopupSection& s : popup.sections)
            if (s.heading == heading) return &s;
        return nullptr;
    }

    // Each row of a section as one line of text: its cells' text joined.
    std::vector<std::string> Lines(const PopupSection& section)
    {
        std::vector<std::string> lines;
        for (const PopupRow& row : section.rows)
        {
            std::string line;
            for (const PopupCell& cell : row.cells)
                line += Joined(cell.text);
            lines.push_back(line);
        }
        return lines;
    }

    // World Info's Conquest card, and the lines of its picked tab.
    PopupSection ConquestCard(const PopupContent& popup)
    {
        return *std::ranges::find_if(popup.sections, [](const PopupSection& s) { return CardKey(s.heading) == "Conquest"; });
    }
    std::vector<std::string> TabLines(const PopupSection& card)
    {
        return Lines(PopupSection{"", card.tabs.at(card.tab).rows});
    }

    std::vector<std::string> TagsOf(const PopupContent& popup)
    {
        std::vector<std::string> tags;
        for (const PopupRow& row : popup.sections[0].rows)
            if (row.kind == RowKind::Tags)
                for (const PopupCell& cell : row.cells)
                    tags.push_back(Joined(cell.text));
        return tags;
    }

    ActorInfo MobTarget(uint32_t serverId)
    {
        ActorInfo target = Goblin();
        target.mob       = MobById(serverId);
        target.serverId  = serverId;
        return target;
    }
}

TEST(the_target_widget_shows_name_level_and_con_in_their_colors)
{
    // The Aggressive row's outline pick is the Aggression palette's; at 20 a 17-20 mob's harder end is an Even Match.
    Settings settings;
    settings.palette.aggression[static_cast<int>(Nature::Aggressive)] = ColorFromRgb(0x112233);
    settings.palette.toughness[static_cast<int>(LabelShade::EvenMatch)] = ColorFromRgb(0x445566);
    const ActorInfo goblin = Goblin();
    BarState state;
    state.target          = &goblin;
    const WidgetContent w = Content(BarWidget::Target, state, settings);
    CHECK(w.shown && w.hasPopup);
    CHECK(w.icons == std::vector<Icon>{Icon::AggroNQ});
    CHECK(w.title == (std::vector<Span>{{"Goblin Bounty Hunter", Tone::Own, 0xFF112233u}}));
    CHECK(w.subtitle == (std::vector<Span>{{"Lv 17-20 EP-EM", Tone::Own, 0xFF445566u}}));
}

TEST(a_mobs_aggression_is_the_targets_icon_and_detection_and_weaknesses_are_parts_of_its_design)
{
    Settings settings;
    const ActorInfo goblin = Goblin();
    BarState state;
    state.target     = &goblin;
    WidgetOptions& o = settings.Widget(BarWidget::Target);
    const auto icons = [](const std::vector<Span>& line) {
        std::vector<Icon> out;
        for (const Span& s : line)
            if (s.icon) out.push_back(*s.icon);
        return out;
    };
    WidgetContent w = Content(BarWidget::Target, state, settings);
    CHECK(w.icons == std::vector<Icon>{Icon::AggroNQ} && icons(w.title).empty() && icons(w.subtitle).empty()); // as new
    o.design          = DesignFrom(BarWidget::Target, "name detection distance | level weak player");
    o.targetDetection = kAllDetection;
    w                 = Content(BarWidget::Target, state, settings);
    std::vector<Icon> rest = EveryIcon(goblin);
    rest.erase(rest.begin()); // the aggression icon comes first
    CHECK(w.icons == std::vector<Icon>{Icon::AggroNQ} && rest.size() > 1 && icons(w.title) == rest);
    std::vector<Icon> weak;
    const InfoIconSet all = InfoIconsFor(&ResistancesOf(*goblin.mob), BadgeRules{});
    for (int i = 0; i < all.count; ++i)
        if (all.icons[i].badge == BadgeKind::Weak || all.icons[i].badge == BadgeKind::WeakRank)
            weak.push_back(all.icons[i].icon);
    CHECK(!weak.empty() && icons(w.subtitle) == weak);
    o.targetDetection = DetectionBit(DetectionIcon::Link);
    o.design          = DesignFrom(BarWidget::Target, "name detection | level");
    o.iconSide        = IconSide::Hide;
    w                 = Content(BarWidget::Target, state, settings);
    CHECK(w.icons.empty() && icons(w.title) == std::vector<Icon>{Icon::Link} && icons(w.subtitle).empty());
}

TEST(show_mob_level_off_leaves_out_a_mobs_level_and_con)
{
    Settings settings;
    const ActorInfo goblin = Goblin();
    BarState state;
    state.target = &goblin;
    settings.Widget(BarWidget::Target).design = DesignFrom(BarWidget::Target, "aggro name distance | -level player -weak");
    const WidgetContent w = Content(BarWidget::Target, state, settings);
    CHECK(w.shown && w.subtitle.empty() && Joined(w.title) == "Goblin Bounty Hunter");
}

TEST(visibility_hides_the_target_widget_for_each_kind_of_target_left_out)
{
    Settings settings;
    const ActorInfo goblin = Goblin();
    ActorInfo player, npc, pet;
    player.kind = player.state.kind = EntityKind::Player;
    npc.kind = npc.state.kind = EntityKind::Npc;
    pet.kind = pet.state.kind = EntityKind::Npc;
    pet.state.pet             = true;
    const std::pair<TargetShow, const ActorInfo*> kinds[] = {{TargetShow::NpcsObjects, &npc}, {TargetShow::Players, &player},
        {TargetShow::Mobs, &goblin}, {TargetShow::Pets, &pet}, {TargetShow::NoTarget, nullptr}};
    BarState state;
    for (const auto& [left, _] : kinds)
    {
        settings.Widget(BarWidget::Target).targetShows = static_cast<uint8_t>(kAllTargetShows & ~TargetShowBit(left));
        for (const auto& [kind, target] : kinds)
        {
            state.target = target;
            CHECK_EQ(Content(BarWidget::Target, state, settings).shown, kind != left);
        }
    }
}

TEST(a_player_npc_or_pets_name_takes_its_entity_type_color)
{
    Settings settings;
    settings.palette.type[static_cast<int>(TargetType::Player)] = ColorFromRgb(0x112233);
    settings.palette.type[static_cast<int>(TargetType::Npc)]    = ColorFromRgb(0x445566);
    settings.palette.type[static_cast<int>(TargetType::Pet)]    = ColorFromRgb(0x778899);
    ActorInfo player, npc, pet;
    player.kind = player.state.kind = EntityKind::Player;
    npc.kind = npc.state.kind = EntityKind::Npc;
    pet.kind = pet.state.kind = EntityKind::Npc;
    pet.state.pet             = true;
    BarState state;
    for (const auto& [target, argb] : {std::pair{&player, 0xFF112233u}, {&npc, 0xFF445566u}, {&pet, 0xFF778899u}})
    {
        std::snprintf(target->name, sizeof(target->name), "%s", "Someone");
        state.target = target;
        CHECK(Content(BarWidget::Target, state, settings).title == (std::vector<Span>{{"Someone", Tone::Own, argb}}));
    }
}

TEST(no_target_reads_no_target_and_a_player_gets_only_a_name)
{
    Settings settings;
    BarState state;
    CHECK(Content(BarWidget::Target, state, settings).title == (std::vector<Span>{{"No Target", Tone::Dim}}));
    CHECK(!Content(BarWidget::Target, state, settings).hasPopup);
    CHECK(BarPopupContent(BarWidget::Target, state, settings).title == (std::vector<Span>{{"No Target", Tone::Dim}}));
    ActorInfo player;
    player.kind = EntityKind::Player;
    std::snprintf(player.name, sizeof(player.name), "%s", "Zinnia");
    state.target          = &player;
    const WidgetContent w = Content(BarWidget::Target, state, settings);
    CHECK(Joined(w.title) == "Zinnia" && w.subtitle.empty() && w.icons.empty() && !w.hasPopup);
    ActorInfo stranger = Goblin(); // a mob the Phoenix data does not have
    stranger.mob       = nullptr;
    stranger.level     = MakeLabel(nullptr, nullptr, 20);
    state.target       = &stranger;
    const WidgetContent s = Content(BarWidget::Target, state, settings);
    CHECK(Joined(s.title) == "Goblin Bounty Hunter" && Joined(s.subtitle) == stranger.level.text && !s.hasPopup);
    CHECK(s.icons.empty());
}

TEST(a_dead_mob_with_data_keeps_its_aggro_icon_and_popup_but_has_no_level)
{
    Settings settings;
    ActorInfo dead = Goblin(); // the tracker fills the level only for a living mob
    dead.alive = dead.state.alive = false;
    dead.level                    = Label{};
    dead.state.toughness          = LabelShade::Unknown;
    BarState state;
    state.target          = &dead;
    const WidgetContent w = Content(BarWidget::Target, state, settings);
    CHECK(w.icons == std::vector<Icon>{Icon::AggroNQ});
    const uint32_t nameColor = ToArgb(settings.palette.aggression[static_cast<int>(Nature::Aggressive)]);
    CHECK(w.title == (std::vector<Span>{{"Goblin Bounty Hunter", Tone::Own, nameColor}}));
    CHECK(w.subtitle.empty());
    CHECK(w.hasPopup);
    const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(popup.icon == Icon::AggroNQ);
    CHECK(popup.title == (std::vector<Span>{{"Goblin Bounty Hunter", Tone::Own, nameColor}, {" ID 220", Tone::Dim}}));
    const PopupRow& top = popup.sections[0].rows[0];
    CHECK(top.cells.size() == 1 && Joined(top.cells[0].text) == "WAR"); // WAR/WAR as its job alone
    CHECK(Section(popup, "Spawn") != nullptr);
}

TEST(the_target_popup_has_every_element_and_status_with_its_percent)
{
    Settings settings;
    const ActorInfo goblin = Goblin();
    BarState state;
    state.target             = &goblin;
    const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
    std::vector<std::string> headings;
    for (const PopupSection& s : popup.sections)
        headings.push_back(s.heading);
    CHECK(headings == (std::vector<std::string>{"", "Damage Taken", "Elements", "Statuses", "Immune", "Stats at Level 20",
                                                "TP Moves", "Spawn"}));
    const PopupRow& elements = Section(popup, "Elements")->rows[0];
    CHECK(elements.kind == RowKind::Grid && elements.columns == 4 && elements.cells.size() == 8);
    CHECK(elements.cells[6].icon == Icon::Light);
    // Unchecked, at the top of its range: magic evasion 58 at 20 (rank C+, Phoenix's sql/skill_caps.sql), light -3
    // takes floor(58 * 0.95) = 55 off it, fire -2 floor(58 * 0.96019) = 55; dark is 0.
    CHECK(elements.cells[6].text == (std::vector<Span>{{"+3%", Tone::Weak}}));
    CHECK(elements.cells[7].text == (std::vector<Span>{{"0%", Tone::Neutral}}));
    CHECK(elements.cells[0].text == (std::vector<Span>{{"+3%", Tone::Weak}}));
    // Its combat stats at 20 from data/phoenix_mob_info.tsv, then STR to CHR and its magic evasion.
    // Each stat's name against the right of its column and its value against the left of the next.
    const PopupRow& combat = Section(popup, "Stats at Level 20")->rows[0];
    CHECK(combat.kind == RowKind::Grid && combat.columns == 8 && combat.cells.size() == 12);
    const std::vector<Span> hp{{"HP", Tone::Dim}}, value{{"414", Tone::Normal}};
    CHECK(combat.cells[0].text == hp && combat.cells[0].align == Align::Right);
    CHECK(combat.cells[1].text == value && combat.cells[1].align == Align::Left);
    CHECK(Joined(combat.cells[6].text) == "DEF" && Joined(combat.cells[7].text) == "85");
    const PopupRow& stats = Section(popup, "Stats at Level 20")->rows[1];
    CHECK(stats.kind == RowKind::Grid && stats.cells.size() == 16);
    CHECK(Joined(stats.cells[14].text) == "MEva" && Joined(stats.cells[15].text) == "58");
    const PopupRow& moves = Section(popup, "TP Moves")->rows[0];
    CHECK(moves.kind == RowKind::Grid && moves.columns == 3 && moves.cells.size() == 3);
    CHECK(Joined(moves.cells[0].text) == "Goblin Rush" && Joined(moves.cells[2].text) == "Bomb Toss Suicide");
    const PopupRow& statuses = Section(popup, "Statuses")->rows[0];
    CHECK(statuses.columns == 5 && statuses.cells.size() == 10 && statuses.cells[0].icon == Icon::Paralyze);
    CHECK(Section(popup, "Immune")->rows[0].cells[0].text == (std::vector<Span>{{"None", Tone::Neutral}}));
    CHECK(Joined(Section(popup, "Spawn")->rows[0].cells[0].text) == "Respawn 5:00");
    // Its level, job and con said in full, in the con's color; its ID after its name (17199648 is 0x1067220).
    const uint32_t shade = ToArgb(settings.palette.toughness[static_cast<int>(goblin.level.shade)]);
    CHECK(popup.sections[0].rows[0].cells[0].text == (std::vector<Span>{{"Level 17-20", Tone::Own, shade}}));
    CHECK(Joined(popup.sections[0].rows[0].cells[1].text) == "WAR");
    CHECK(popup.sections[0].rows[0].cells[2].text == (std::vector<Span>{{"(Easy Prey to Even Match)", Tone::Own, shade}}));
    CHECK(Joined(popup.title) == "Goblin Bounty Hunter ID 220");
    settings.rankDisplay = RankDisplay::Rank;
    CHECK(Section(BarPopupContent(BarWidget::Target, state, settings), "Elements")->rows[0].cells[6].text ==
          (std::vector<Span>{{"-3", Tone::Weak}}));
}

namespace
{
    bool KnowsMetallicBody(uint16_t spell) { return spell == 517; }

    const PopupCell* MoveCell(const PopupContent& popup, const char* name)
    {
        const PopupSection* moves = Section(popup, "TP Moves");
        if (moves == nullptr) return nullptr;
        for (const PopupCell& cell : moves->rows[0].cells)
            if (Joined(cell.text) == name) return &cell;
        return nullptr;
    }
}

TEST(tp_moves_a_blue_mage_can_learn_are_blue_and_learned_ones_marked)
{
    // Qufim Island's Clipper: Metallic Body teaches blue magic 517, Big Scissors nothing.
    Settings settings;
    ActorInfo crab = Goblin();
    crab.mob       = MobById(17293331);
    BarState state;
    state.target = &crab;
    const std::vector<Span> blue{{"Metallic Body", Tone::Own, ToArgb(settings.blueMagicColor)}};
    const std::vector<Span> plain{{"Metallic Body", Tone::Normal}}, scissors{{"Big Scissors", Tone::Normal}};
    const auto shown = [&](const char* name) {
        const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
        const PopupCell* cell    = MoveCell(popup, name);
        return cell != nullptr ? std::optional<PopupCell>(*cell) : std::nullopt;
    };
    CHECK(shown("Metallic Body")->text == blue && !shown("Metallic Body")->star);
    CHECK(shown("Big Scissors")->text == scissors);
    // Learned, it keeps its blue and takes a star, or goes back to plain text.
    state.knowsSpell = KnowsMetallicBody;
    CHECK(shown("Metallic Body")->text == blue && shown("Metallic Body")->star);
    settings.blueMagicLearned = BlueLearned::Plain;
    CHECK(shown("Metallic Body")->text == plain && !shown("Metallic Body")->star);
    // Off, every move is plain.
    settings.blueMagic        = false;
    settings.blueMagicLearned = BlueLearned::Star;
    CHECK(shown("Metallic Body")->text == plain && !shown("Metallic Body")->star);
}

TEST(a_placeholder_names_its_nm_and_counts_down)
{
    // East Ronfaure: two Carrion Worms are Bigmouth Billy's placeholders (data/phoenix_mobs.tsv).
    Settings settings;
    ActorInfo target = Goblin();
    target.mob       = MobById(17191194);
    target.serverId  = 17191194;
    BarState state;
    state.target          = &target;
    state.targetPhSeconds = 299.0;
    const PopupContent popup  = BarPopupContent(BarWidget::Target, state, settings);
    const PopupSection* spawn = Section(popup, "Spawn");
    CHECK(Joined(spawn->rows[1].cells[0].text) == "Placeholder for Bigmouth Billy (15%)"); // his slot's chance
    CHECK(Joined(spawn->rows[2].cells[0].text) == "Back in 4:59");
    target.mob      = MobById(17191196);
    target.serverId = 17191196;
    const PopupContent nmPopup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Joined(Section(nmPopup, "Spawn")->rows.back().cells[0].text) == "2 placeholders in this zone");
}

TEST(a_placeholders_last_second_reads_as_the_nameplates_timer_does)
{
    Settings settings;
    ActorInfo target = Goblin();
    target.mob       = MobById(17191194); // a Carrion Worm, Bigmouth Billy's placeholder
    target.serverId  = 17191194;
    BarState state;
    state.target          = &target;
    state.targetPhSeconds = 0.4;
    const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Joined(Section(popup, "Spawn")->rows[2].cells[0].text) == "Back in 0:01"); // never 0:00 before it is due
}

TEST(the_tags_say_how_a_mob_aggros_links_and_detects)
{
    // data/phoenix_mobs.tsv: the Goblin Bounty Hunter has flags 33 (aggressive, links) and detects 1 (sight); the
    // Brontotaur, 16797799, flags 97 (aggressive, links, true detection) and detects 1, which true sight replaces.
    Settings settings;
    ActorInfo target = Goblin();
    BarState state;
    state.target              = &target;
    const PopupContent goblin = BarPopupContent(BarWidget::Target, state, settings);
    // Its aggression is the icon before its name, so the tags leave it out; each sense has its own icon.
    CHECK(TagsOf(goblin) == (std::vector<std::string>{"Links", "Sight 15y"}));
    const std::vector<PopupCell>& cells = goblin.sections[0].rows[1].cells;
    CHECK(cells[0].icon == std::optional<Icon>(Icon::Link) && cells[1].icon == std::optional<Icon>(Icon::Sight));
    target                        = MobTarget(16797799);
    const PopupContent brontotaur = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(TagsOf(brontotaur) == (std::vector<std::string>{"Links", "True sight 15y", "Counter"}));
    // Qufim Island's Clipper, a crab: it hears from 8 yalms and a beastmaster can charm it.
    target = MobTarget(17293331);
    CHECK(TagsOf(BarPopupContent(BarWidget::Target, state, settings)) == (std::vector<std::string>{"Sound 8y", "Charmable"}));
}

TEST(a_tags_words_take_the_tag_tone_and_nothing_else_in_the_popup_does)
{
    // The words sit in their frames, drawn in the tag's text color; the rows after keep theirs.
    // data/phoenix_mobs.tsv: the Brontotaur, 16797799, has two tags (Links, True sight) and counters (a third,
    // data/phoenix_mob_info.tsv); Bloodtear Baldurf, 17195318, flags 11 and detects 257, three (Sight, Scent, NM).
    // Neither's aggression is a tag: it is the icon before the name.
    Settings settings;
    for (const auto& [id, tags] : {std::pair{16797799u, size_t{3}}, {17195318u, size_t{3}}})
    {
        ActorInfo target = MobTarget(id);
        BarState state;
        state.target             = &target;
        const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
        size_t tagged = 0, others = 0;
        for (const PopupSection& section : popup.sections)
            for (const PopupRow& row : section.rows)
                for (const PopupCell& cell : row.cells)
                    for (const Span& span : cell.text)
                    {
                        const bool tag = row.kind == RowKind::Tags;
                        CHECK((span.tone == Tone::Tag) == tag);
                        ++(tag ? tagged : others);
                    }
        CHECK(tagged == tags && others > 0);
    }
}

namespace
{
    const char* SpellNumber(uint16_t spell)
    {
        static std::map<uint16_t, std::string> names;
        return names.try_emplace(spell, "S" + std::to_string(spell)).first->second.c_str();
    }
}

TEST(a_lottery_nm_shows_its_jobs_window_chance_and_cooldown_and_its_placeholder_its_chance)
{
    // Attohwa Chasm (data/zones/attohwa_chasm/mobs.yaml): Citipati is THF/BLM, out from 20:00 to 04:00, 15% from his
    // slot with three hours out after his kill, and takes a quarter less magic damage and half breath damage; the
    // Corse is his placeholder.
    Settings settings;
    BarState state;
    const ActorInfo citipati = MobTarget(16806162);
    state.target             = &citipati;
    const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Joined(popup.sections[0].rows[0].cells[1].text) == "THF/BLM");
    CHECK(Lines(*Section(popup, "Spawn")) == (std::vector<std::string>{"Respawn 5:00", "Spawns 20:00 to 04:00",
                                                 "15% pop chance", "3:00:00 cooldown after its kill",
                                                 "1 placeholder in this zone"}));
    const PopupRow& kinds = Section(popup, "Damage Taken")->rows[1];
    CHECK(kinds.cells.size() == 2);
    CHECK(kinds.cells[0].text == (std::vector<Span>{{"Magic", Tone::Dim}, {" ", Tone::Normal}, {"-25%", Tone::Resist}}));
    CHECK(kinds.cells[1].text == (std::vector<Span>{{"Breath", Tone::Dim}, {" ", Tone::Normal}, {"-50%", Tone::Resist}}));
    const ActorInfo corse = MobTarget(16806161);
    state.target          = &corse;
    CHECK(Lines(*Section(BarPopupContent(BarWidget::Target, state, settings), "Spawn"))[1] ==
          "Placeholder for Citipati (15%)");
    // A mob with no other kind of damage taken has only its SDT row.
    const ActorInfo goblin = Goblin();
    state.target           = &goblin;
    CHECK(Section(BarPopupContent(BarWidget::Target, state, settings), "Damage Taken")->rows.size() == 1);
}

TEST(a_casters_spells_are_the_ones_it_casts_at_its_level)
{
    // The Corse's list (data/phoenix_spells.tsv, list 8), at 67, the top of its range: each spell whose levels
    // hold 67, in the list's order.
    Settings settings;
    BarState state;
    const ActorInfo corse = MobTarget(16806161);
    state.target          = &corse;
    state.spellName       = SpellNumber;
    std::vector<std::string> expected;
    for (const int spell : {254, 258, 245, 250, 253, 216, 225, 247, 231, 259, 252, 206, 210, 274, 146, 191, 151, 166, 186})
        expected.push_back("S" + std::to_string(spell));
    const PopupContent popup   = BarPopupContent(BarWidget::Target, state, settings);
    const PopupSection* spells = Section(popup, "Spells");
    CHECK(spells != nullptr);
    if (spells == nullptr) return;
    CHECK(spells->rows[0].kind == RowKind::Grid && spells->rows[0].columns == 3);
    std::vector<std::string> names;
    for (const PopupCell& cell : spells->rows[0].cells)
        names.push_back(Joined(cell.text));
    CHECK(names == expected);
    state.spellName = nullptr; // without the game's names there is nothing to list
    CHECK(Section(BarPopupContent(BarWidget::Target, state, settings), "Spells") == nullptr);
}

TEST(an_element_it_absorbs_and_what_it_does_by_itself)
{
    // data/phoenix_mob_info.tsv: Puk WW (Windurst Waters' Puk, 16986223) absorbs wind; the Stubborn Dredvodd,
    // 16781327, regenerates; a Water Elemental of zone 37, 16928885, poisons with its hits.
    Settings settings;
    BarState state;
    const ActorInfo puk = MobTarget(16986223);
    state.target        = &puk;
    const PopupContent pukPopup = BarPopupContent(BarWidget::Target, state, settings);
    const PopupRow& elements    = Section(pukPopup, "Elements")->rows[0];
    CHECK(elements.cells[static_cast<int>(Element::Wind)].text == (std::vector<Span>{{"Absorbs", Tone::Immune}}));
    const ActorInfo dredvodd = MobTarget(16781327);
    state.target             = &dredvodd;
    CHECK(Lines(BarPopupContent(BarWidget::Target, state, settings).sections[0]).back() == "Regen");
    const ActorInfo elemental = MobTarget(16928885);
    state.target              = &elemental;
    CHECK(Lines(BarPopupContent(BarWidget::Target, state, settings).sections[0]).back() == "Additional Effect: Poison");
}

TEST(a_resisted_damage_type_shows_its_percent_in_the_resist_tone)
{
    // Protozoan, 16781317: slashing and piercing sdt -5000, impact and hand-to-hand -7500 (data/phoenix_mobs.tsv).
    Settings settings;
    const ActorInfo target = MobTarget(16781317);
    BarState state;
    state.target             = &target;
    const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
    const PopupRow& damage   = Section(popup, "Damage Taken")->rows[0];
    CHECK(damage.cells[0].icon == Icon::Slashing);
    CHECK(damage.cells[0].text == (std::vector<Span>{{"-50%", Tone::Resist}}));
    CHECK(damage.cells[3].text == (std::vector<Span>{{"-75%", Tone::Resist}}));
}

TEST(the_spawn_section_says_when_there_is_no_timer_no_placeholder_or_one_is_due)
{
    Settings settings;
    BarState state;
    const ActorInfo goblin = Goblin(); // respawn 300, no placeholder of an NM, no placeholders of its own
    state.target           = &goblin;
    const PopupContent goblinPopup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Lines(*Section(goblinPopup, "Spawn")) == (std::vector<std::string>{"Respawn 5:00", "Not an NM Placeholder"}));
    const ActorInfo snipper = MobTarget(16781313); // respawn 0
    state.target            = &snipper;
    const PopupContent snipperPopup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Lines(*Section(snipperPopup, "Spawn")).front() == "No Respawn Timer");
    const ActorInfo worm  = MobTarget(17191194); // Bigmouth Billy's placeholder
    state.target          = &worm;
    state.targetPhSeconds = 0.0;
    const PopupContent wormPopup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Section(wormPopup, "Spawn")->rows[2].cells[0].text == (std::vector<Span>{{"Due Now", Tone::Weak}}));
}

TEST(an_immunity_shows_a_cross_in_its_status_and_an_icon_under_immune)
{
    Settings settings;
    const MobRecord* mob = nullptr;
    for (uint32_t id = 0x1000000; id < 0x1200000 && mob == nullptr; ++id)
    {
        const MobRecord* m = MobById(id);
        if (m != nullptr && (ResistancesOf(*m).immunity & kImmuneParalyze) != 0) mob = m;
    }
    CHECK(mob != nullptr);
    ActorInfo target = Goblin();
    target.mob       = mob;
    target.serverId  = mob->id;
    BarState state;
    state.target             = &target;
    const PopupContent popup = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Section(popup, "Statuses")->rows[0].cells[0].text == (std::vector<Span>{{"\xC3\x97", Tone::Immune}}));
    const PopupRow& immune = Section(popup, "Immune")->rows[0];
    CHECK(!immune.cells.empty());
    for (const PopupCell& cell : immune.cells)
        CHECK(cell.immune && cell.icon.has_value() && cell.text.empty());
}

TEST(the_forecast_shows_vanadiels_date_day_time_and_moon_as_its_design_sets)
{
    Settings settings;
    BarState state;
    state.unixMs = 1791417600000LL; // Darksday 1514/03/20 09:00, 64% waxing; no zone, so no weather
    WidgetOptions& o = settings.Widget(BarWidget::Weather);
    const auto shown = [&] { return Content(BarWidget::Weather, state, settings); };
    const WidgetContent w = shown(); // icon weather | day time: no weather, so its line gives way to the next
    CHECK(w.shown && w.hasPopup && w.icons.empty() && w.subtitle.empty());
    CHECK(w.title.size() == 2 && w.title[0] == (Span{"Darksday", Tone::Own, ElementColor(Element::Dark)}));
    CHECK(w.title[1].text == "  09:00" && w.title[1].tone == Tone::Normal && !w.title[1].room.empty());
    o.design = DesignFrom(BarWidget::Weather, "day time | date moon");
    CHECK(shown().subtitle == (std::vector<Span>{{"1514/03/20", Tone::Dim}, {"  Waxing Gibbous 64%", Tone::Dim}}));
    o.design = DesignFrom(BarWidget::Weather, "moon | time");
    CHECK(shown().title == (std::vector<Span>{{"Waxing Gibbous 64%", Tone::Normal}}));
    CHECK(Joined(shown().subtitle) == "09:00" && shown().subtitle.front().tone == Tone::Dim);
    // Its pop-out's Today card has the date in its heading, the rest in its rows with what comes next beside each.
    const PopupContent popup  = BarPopupContent(BarWidget::Weather, state, settings);
    const PopupSection* today = Section(popup, "Today (1514/03/20)");
    CHECK(today != nullptr);
    std::string all;
    for (const PopupRow& row : today->rows)
        for (const PopupCell& cell : row.cells)
            all += Joined(cell.text) + "|";
    CHECK(all == "Darksday 09:00 (Next Day in 36:00)|Waxing Gibbous, 64% (Next Phase in 10:12:00)|");
    CHECK(today->rows[0].cells[0].text.back().tone == Tone::Dim && today->rows[1].cells[0].text.back().tone == Tone::Dim);
}

TEST(the_zone_widget_names_the_zone_and_its_region)
{
    Settings settings;
    BarState state;
    state.zone = 103;
    WidgetContent w = Content(BarWidget::Zone, state, settings);
    CHECK(Joined(w.title) == "Valkurm Dunes" && Joined(w.subtitle) == "Zulkheim" && w.hasPopup);
    state.zone = 999;
    w          = Content(BarWidget::Zone, state, settings);
    CHECK(Joined(w.title) == "Zone 999" && w.subtitle.empty());
    state.zone = 0;
    w          = Content(BarWidget::Zone, state, settings);
    CHECK(Joined(w.title) == "-" && !w.hasPopup);
}

TEST(gil_bags_and_earth_time)
{
    Settings settings;
    BarState state;
    CHECK(Joined(Content(BarWidget::Currency, state, settings).title) == "-");
    state.gil             = 1234567;
    const WidgetContent g = Content(BarWidget::Currency, state, settings);
    CHECK(LeadsWithGil(g) && Joined(g.title) == "1,234,567" && g.hasPopup);
    state.bags[0]         = BagCount{45, 80};
    state.bags[5]         = BagCount{30, 30}; // a full satchel
    const WidgetContent b = Content(BarWidget::Bags, state, settings);
    CHECK(Joined(b.title) == "45 / 80" && Joined(b.subtitle) == "Inventory" && b.hasPopup);
    CHECK(b.icons.size() == 1 && b.icons[0] == Icon::Bag); // its bag leads both lines
    const PopupContent bagsPopup = BarPopupContent(BarWidget::Bags, state, settings);
    CHECK(Joined(bagsPopup.title) == "Inventory");
    const PopupRow& rows         = bagsPopup.sections[0].rows[0];
    CHECK(rows.kind == RowKind::Grid && rows.columns == 2 && rows.cells.size() == 6 && rows.roomy); // Storage too, with no room
    CHECK(Joined(rows.cells[2].text) == "Storage");
    CHECK(Joined(rows.cells[4].text) == "Satchel");
    CHECK(rows.cells[5].text == (std::vector<Span>{{"30 / 30", Tone::Resist}}));
    state.earthHour   = 21;
    state.earthMinute = 7;
    state.earthYear   = 2026;
    state.earthMonth  = 10;
    state.earthDay    = 8;
    CHECK(Joined(Content(BarWidget::EarthTime, state, settings).title) == "21:07");
    CHECK(Joined(BarPopupContent(BarWidget::EarthTime, state, settings).title) == "October 8th, 2026 - 21:07");
}

// As Ashita reports the inventory: gil is item 0xFFFF in its first slot, and a bag the character cannot use has no
// room.
TEST(gil_is_only_the_gil_item_and_a_bag_without_room_is_none)
{
    CHECK(GilOf(0xFFFF, 1234567) == 1234567u);
    CHECK(GilOf(0xFFFF, 0) == 0u);
    CHECK(!GilOf(4096, 12).has_value()); // a stack of fire crystals
    CHECK(!GilOf(0, 0).has_value());     // nothing there yet
    const std::optional<BagCount> satchel = BagOf(30, 80);
    CHECK(satchel.has_value() && satchel->used == 30 && satchel->max == 80);
    CHECK(BagOf(0, 30).has_value()); // empty, with room
    CHECK(!BagOf(0, 0).has_value()); // locked
}

// While you change zones the game empties your job, gil and status icons and refills the bags one item at a time.
TEST(while_your_data_loads_the_panel_keeps_what_it_last_read_and_the_status_timers_run_on)
{
    Settings settings;
    JobSample war;
    war.mainJob = 1, war.mainLevel = 30, war.subJob = 13, war.subLevel = 15;
    BarState loaded;
    loaded.unixMs     = 1'800'000'000'000;
    loaded.zone       = 103;
    loaded.weather    = Weather::Rain;
    loaded.gil        = 1234567;
    loaded.bags[0]    = BagCount{45, 80};
    loaded.job        = JobReport{war, std::nullopt};
    loaded.statuses   = {{33, 120.0}, {40, std::nullopt}}; // Haste with two minutes left, and Protect untimed
    CharacterReadings held;
    BarState first = loaded;
    HoldWhileLoading(first, held, true);
    CHECK(Joined(Content(BarWidget::Currency, first, settings).title) == "1,234,567");

    BarState zoning;
    zoning.unixMs  = loaded.unixMs + 5'000;
    zoning.bags[0] = BagCount{3, 80}; // the first items back
    HoldWhileLoading(zoning, held, false);
    CHECK(Joined(Content(BarWidget::Currency, zoning, settings).title) == "1,234,567");
    CHECK(Joined(Content(BarWidget::Bags, zoning, settings).title) == "45 / 80");
    CHECK(Joined(Content(BarWidget::Job, zoning, settings).title) == "WAR 30 / NIN 15");
    CHECK(zoning.zone == 103 && zoning.weather == Weather::Rain);
    CHECK(zoning.statuses == (std::vector<ActiveStatus>{{33, 115.0}, {40, std::nullopt}}));
    CHECK(zoning.unixMs == loaded.unixMs + 5'000); // the clock is not held

    BarState arrived;
    arrived.unixMs  = zoning.unixMs + 1'000;
    arrived.zone    = 104;
    arrived.gil     = 1234000;
    arrived.bags[0] = BagCount{44, 80};
    HoldWhileLoading(arrived, held, true);
    CHECK(Joined(Content(BarWidget::Bags, arrived, settings).title) == "44 / 80");
    BarState again;
    HoldWhileLoading(again, held, false);
    CHECK(again.zone == 104 && again.gil == 1234000u && !again.job.has_value()); // held from the newest loaded frame
}

TEST(each_widget_follows_its_switch_and_a_switched_off_one_builds_nothing)
{
    Settings settings;
    BarState state;
    state.zone = 103;
    CHECK(Content(BarWidget::Currency, state, settings).shown);
    for (WidgetOptions& each : settings.widgets)
        each.on = false;
    for (int i = 0; i < kBarWidgetCount; ++i)
    {
        const WidgetContent w = Content(static_cast<BarWidget>(i), state, settings);
        CHECK(!w.shown && w.title.empty() && w.subtitle.empty() && w.icons.empty() && !w.hasPopup);
    }
}

TEST(before_a_weather_is_known_the_widget_shows_the_days_likely_weather)
{
    Settings settings;
    BarState state;
    state.zone   = 103;              // Valkurm Dunes, day 0: sunshine 50%, dust storm 50%
    state.unixMs = kVanaEpochUnixMs; // day 0
    const WidgetContent w = Content(BarWidget::Weather, state, settings);
    CHECK(w.shown && w.hasPopup && w.icons == (std::vector<Icon>{Icon::WeatherSunny}));
    CHECK(Joined(w.title) == "Sunshine");
}

TEST(use_custom_icons_picks_headsups_pictures_or_the_games_element_icons)
{
    // Always: HeadsUp's own, one for a double weather too.
    CHECK(WeatherIcons(Weather::DustStorm, CustomIcons::Always) == (std::vector<Icon>{Icon::WeatherDustStorm}));
    CHECK(WeatherIcons(Weather::SandStorm, CustomIcons::Always) == (std::vector<Icon>{Icon::WeatherDustStorm}));
    CHECK(WeatherIcons(Weather::None, CustomIcons::Always) == (std::vector<Icon>{Icon::WeatherFair}));
    CHECK(WeatherIcons(Weather::Darkness, CustomIcons::Always) == (std::vector<Icon>{Icon::WeatherGloom}));
    // Never: the game's element, twice for a double weather, and nothing for a weather of no element.
    CHECK(WeatherIcons(Weather::DustStorm, CustomIcons::Never) == (std::vector<Icon>{Icon::Earth}));
    CHECK(WeatherIcons(Weather::SandStorm, CustomIcons::Never) == (std::vector<Icon>{Icon::Earth, Icon::Earth}));
    CHECK(WeatherIcons(Weather::Sunshine, CustomIcons::Never).empty());
    // When No Vanilla Icon: the element where there is one.
    CHECK(WeatherIcons(Weather::Rain, CustomIcons::NoVanilla) == (std::vector<Icon>{Icon::Water}));
    CHECK(WeatherIcons(Weather::Clouds, CustomIcons::NoVanilla) == (std::vector<Icon>{Icon::WeatherCloudy}));
    // Each weather has its own picture by the card's names.
    for (int w = 0; w < kWeatherCount; ++w)
        CHECK_EQ(WeatherIcons(static_cast<Weather>(w), CustomIcons::Always).size(), static_cast<size_t>(1));
    // The widget and its pop-out follow the setting.
    Settings settings;
    BarState state;
    state.zone    = 103;
    state.unixMs  = kVanaEpochUnixMs;
    state.weather = Weather::SandStorm;
    settings.Widget(BarWidget::Weather).weatherIcons = CustomIcons::Never;
    CHECK(Content(BarWidget::Weather, state, settings).icons == (std::vector<Icon>{Icon::Earth, Icon::Earth}));
    CHECK(BarPopupContent(BarWidget::Weather, state, settings).icon == std::optional<Icon>(Icon::Earth));
}

TEST(the_forecast_pop_out_has_the_weather_for_a_title_today_beside_the_next_four_days)
{
    Settings settings;
    BarState state;
    state.zone   = 103;
    state.unixMs = kVanaEpochUnixMs;
    const PopupContent popup = BarPopupContent(BarWidget::Weather, state, settings);
    CHECK(Joined(popup.title) == "Sunshine (50%)" && popup.icon == std::optional<Icon>(Icon::WeatherSunny));
    // Today first, the forecast, then the elements.
    const PopupSection* today    = &popup.sections[0];
    const PopupSection* forecast = Section(popup, "Weather Forecast (Valkurm Dunes)");
    CHECK(today->heading.starts_with("Today (") && forecast == &popup.sections[1]);
    // Today's weathers on one line between the day and time and the moon, a slash between them.
    const std::vector<PopupCell>& weathers = today->rows[1].cells;
    CHECK(weathers.size() == 3 && Joined(weathers[0].text) == "Sunshine (50%)" && Joined(weathers[1].text) == "/" &&
          Joined(weathers[2].text) == "Dust Storm (50%)");
    CHECK(weathers[2].icon == std::optional<Icon>(Icon::WeatherDustStorm));
    CHECK(Joined(today->rows[2].cells[0].text).starts_with(Joined(ListOf(Text(MoonPhaseName(VanaAt(state.unixMs).moonPercent,
        VanaAt(state.unixMs).moonDirection))))));
    CHECK(today->rows.size() == 3);
    // Each day from tomorrow, its chances, and no notes.
    CHECK(forecast->rows[0].kind == RowKind::Grid && forecast->rows[0].columns == 4);
    const std::vector<PopupCell>& cells = forecast->rows[0].cells;
    CHECK(cells.size() == 16);
    CHECK(Joined(cells[0].text) == "Earthsday (Tomorrow)"); // day 1
    CHECK(Joined(cells[4].text) == "Watersday");            // day 2: calendar 0x0421, Sunshine 100%
    CHECK(Joined(cells[5].text) == "Sunshine (100%)" && cells[6].text.empty() && cells[7].text.empty());
    CHECK(Joined(cells[12].text) == "Iceday");              // day 4: calendar 0x0442, Sunshine 50% and Clouds 50%
    CHECK(Joined(cells[13].text) == "Sunshine (50%)" && Joined(cells[14].text) == "Clouds (50%)");
    // The days against their column's right, the weathers against their columns' left.
    for (size_t i = 0; i < cells.size(); i += 4)
        CHECK(cells[i].align == Align::Right && cells[i + 1].align == Align::Left && cells[i + 3].align == Align::Left);
}

TEST(each_headed_card_of_a_pop_out_collapses_by_its_press_and_stays_collapsed_across_days)
{
    Settings settings;
    BarState state;
    state.zone   = 103;
    state.unixMs = kVanaEpochUnixMs;
    PopupContent popup = PanelPopupContent(BarWidget::Weather, state, settings);
    const PopupSection& today = popup.sections[0];
    CHECK(!today.collapsePress.empty() && !today.collapsed);
    for (const PopupSection& section : popup.sections)
        CHECK(section.heading.empty() || !section.collapsePress.empty());
    const std::string press = today.collapsePress;
    CHECK(ApplyPopupPress(settings, press, Nation::None) == PressResult::Changed);
    popup = PanelPopupContent(BarWidget::Weather, state, settings);
    CHECK(popup.sections[0].collapsed && !popup.sections[1].collapsed);
    // The next day's Today card, under another date, is the same card.
    state.unixMs += kMsPerDay;
    popup = PanelPopupContent(BarWidget::Weather, state, settings);
    CHECK(popup.sections[0].collapsed && popup.sections[0].collapsePress == press);
    // Pressed again, it opens.
    CHECK(ApplyPopupPress(settings, press, Nation::None) == PressResult::Changed);
    CHECK(!PanelPopupContent(BarWidget::Weather, state, settings).sections[0].collapsed);
}

TEST(a_pop_out_moved_from_its_place_offers_its_reset_which_puts_it_back)
{
    Settings settings;
    BarState state;
    CHECK(PanelPopupContent(BarWidget::Bags, state, settings).resetPress.empty());
    settings.Widget(BarWidget::Bags).popupSpot = PopupSpot{10, 20, false, false};
    const std::string press = PanelPopupContent(BarWidget::Bags, state, settings).resetPress;
    CHECK(!press.empty());
    CHECK(ApplyPopupPress(settings, press, Nation::None) == PressResult::Changed);
    CHECK(!settings.Widget(BarWidget::Bags).popupSpot);
    CHECK(PanelPopupContent(BarWidget::Bags, state, settings).resetPress.empty());
}

TEST(the_forecast_names_the_weekdays_past_the_weeks_end)
{
    Settings settings;
    BarState state;
    state.zone   = 103;
    state.unixMs = 1009831536000LL; // epoch day 6, a Lightsday: the epoch and six days of 3456 Earth seconds
    const PopupContent popup = BarPopupContent(BarWidget::Weather, state, settings);
    std::vector<std::string> days;
    const std::vector<PopupCell>& cells = Section(popup, "Weather Forecast (Valkurm Dunes)")->rows[0].cells;
    for (size_t i = 0; i < cells.size(); i += 4) // the day cells come first in each row of four
        days.push_back(Joined(cells[i].text));
    CHECK(days == (std::vector<std::string>{"Darksday (Tomorrow)", "Firesday", "Earthsday", "Watersday"}));
}

TEST(a_zone_without_a_calendar_or_weather_shows_vanadiels_time_alone)
{
    Settings settings;
    BarState state;
    state.zone   = 999;
    state.unixMs = kVanaEpochUnixMs;
    const WidgetContent w = Content(BarWidget::Weather, state, settings);
    CHECK(w.shown && w.icons.empty() && Joined(w.title) == "Firesday  00:00");
    const PopupContent popup = BarPopupContent(BarWidget::Weather, state, settings);
    CHECK(popup.title.empty() && !popup.icon && popup.sections[0].heading.starts_with("Today ("));
}

TEST(an_unknown_zone_has_no_weather_odds_of_its_own)
{
    Settings settings;
    BarState state;
    state.zone   = 0; // the data gives zone 0 calendar 0, which says Sunshine
    state.unixMs = kVanaEpochUnixMs;
    CHECK(Content(BarWidget::Weather, state, settings).icons.empty());
    state.weather = Weather::DustStorm;
    const WidgetContent w = Content(BarWidget::Weather, state, settings);
    CHECK(w.shown && Joined(w.title) == "Dust Storm" && w.icons == (std::vector<Icon>{Icon::WeatherDustStorm}));
    const PopupContent popup     = BarPopupContent(BarWidget::Weather, state, settings);
    const PopupSection* forecast = Section(popup, "Weather Forecast");
    CHECK(forecast != nullptr && Joined(popup.title) == "Dust Storm"); // no chance of it to give
    const std::vector<PopupCell>& cells = forecast->rows[0].cells;
    for (size_t i = 0; i < cells.size(); ++i)
        if (i % 4 != 0) CHECK(cells[i].text.empty()); // the days alone
}

TEST(a_live_weather_outside_the_days_chances_still_shows)
{
    Settings settings;
    BarState state;
    state.zone    = 103;
    state.unixMs  = kVanaEpochUnixMs;
    state.weather = Weather::Rain; // day 0 is Sunshine or Dust Storm
    const WidgetContent w = Content(BarWidget::Weather, state, settings);
    CHECK(Joined(w.title) == "Rain" && w.icons == (std::vector<Icon>{Icon::WeatherRain}));
    const PopupContent popup = BarPopupContent(BarWidget::Weather, state, settings);
    CHECK(Joined(popup.title) == "Rain" && Joined(popup.sections[0].rows[1].cells[0].text) == "Sunshine (50%)");
}

TEST(the_weather_popup_has_a_tab_for_each_element_of_the_zones_that_can_roll_it_today_and_tomorrow)
{
    Settings settings;
    BarState state;
    state.zone   = 103;
    state.unixMs = kVanaEpochUnixMs + 777LL * 3456 * 1000; // day 777
    const uint32_t today = 777;
    const auto byElement = [&] { return *Section(BarPopupContent(BarWidget::Weather, state, settings), "By Element"); };
    const PopupSection fire = byElement();
    CHECK_EQ(fire.tabs.size(), static_cast<size_t>(kElementCount));
    CHECK(fire.tab == 0);
    for (int e = 0; e < kElementCount; ++e)
    {
        const auto element = static_cast<Element>(e);
        CHECK(fire.tabs[e].name == Colored(ElementName(element), ElementColor(element))); // in its element's color
        CHECK(fire.tabs[e].icon.has_value());                                            // after its element's icon
        CHECK_EQ(fire.tabs[e].rows.size(), static_cast<size_t>(1));                     // its table alone
    }
    CHECK(fire.tabs[0].icon != fire.tabs[1].icon);
    // Then a grid of the zones that can roll it either day, today's likeliest first, then tomorrow's.
    const std::map<std::string, std::string> pickedIce{{"By Element", "Ice"}};
    state.pickedTabs = &pickedIce;
    const PopupSection ice = byElement();
    CHECK_EQ(ice.tab, static_cast<size_t>(Element::Ice));
    const PopupRow& grid = ice.tabs[ice.tab].rows.back();
    CHECK(grid.nested && grid.header && grid.kind == RowKind::Grid && grid.columns == 3);
    std::map<uint16_t, std::pair<int, int>> chance; // today's and tomorrow's
    for (const ZoneRoll& roll : ZonesRolling(Element::Ice, today))
        chance[roll.zone].first = roll.percent;
    for (const ZoneRoll& roll : ZonesRolling(Element::Ice, today + 1))
        chance[roll.zone].second = roll.percent;
    std::vector<uint16_t> order;
    for (const auto& [zone, odds] : chance)
        order.push_back(zone);
    std::ranges::stable_sort(order, [&](uint16_t a, uint16_t b) { return chance[a] > chance[b]; });
    CHECK_EQ(grid.cells.size(), 3 * (order.size() + 1));
    // Its heads count the zones it lists, and those that can roll it today and tomorrow.
    CHECK_EQ(Joined(grid.cells[0].text), "Zone (" + std::to_string(order.size()) + ")");
    CHECK_EQ(Joined(grid.cells[1].text), "Today (" + std::to_string(ZonesRolling(Element::Ice, today).size()) + ")");
    CHECK_EQ(Joined(grid.cells[2].text), "Tomorrow (" + std::to_string(ZonesRolling(Element::Ice, today + 1).size()) + ")");
    CHECK_EQ(grid.scroll, "By Element/Ice"); // it keeps its own scrolling
    for (size_t i = 0; i < order.size(); ++i)
    {
        CHECK_EQ(Joined(grid.cells[3 * (i + 1)].text), ZoneById(order[i])->name);
        // A day the zone cannot roll it reads as a dash.
        if (chance[order[i]].second == 0) CHECK_EQ(Joined(grid.cells[3 * (i + 1) + 2].text), "-");
    }
    // The first zone's weathers today, each with its share.
    const ZoneRoll top = ZonesRolling(Element::Ice, today).front();
    std::string shares;
    for (const WeatherChance& w : top.weathers)
        shares += (shares.empty() ? "" : kDot) + std::string(WeatherName(w.weather)) + " (" + std::to_string(w.percent) + "%)";
    const size_t row = static_cast<size_t>(std::ranges::find(order, top.zone) - order.begin());
    CHECK(row < order.size());
    CHECK_EQ(Joined(grid.cells[3 * (row + 1) + 1].text), shares);
}

TEST(the_weather_popups_longest_element_fits_a_1080_screen_and_scrolls_through_its_zones)
{
    // The element that most zones can roll, picked: its zones scroll in their socket, and nothing is cut.
    Settings settings;
    BarState state;
    state.zone   = 103;
    state.unixMs = kVanaEpochUnixMs + 777LL * 3456 * 1000;
    Element most = Element::Fire;
    for (int e = 0; e < kElementCount; ++e)
        if (ZonesRolling(static_cast<Element>(e), 777).size() > ZonesRolling(most, 777).size()) most = static_cast<Element>(e);
    const std::map<std::string, std::string> picked{{"By Element", ElementName(most)}};
    state.pickedTabs = &picked;
    const BarMetrics m = MetricsFor(45, BarShape::Pills);
    const auto measure = [](const Span& span, PopupFont font) {
        const float height = font == PopupFont::Title ? 24.0f : 18.0f;
        return TextSize{8.0f * static_cast<float>(span.text.size()), height, 0.3f * height, height - 1.0f};
    };
    const float room = 1080.0f - 45.0f - 2.0f * m.gap;
    const auto shows = [&](const PopupLayout& l, const std::string& text) {
        return std::ranges::any_of(l.pieces, [&](const PopupPiece& p) { return p.span.text == text; });
    };
    const std::string key = std::string("By Element/") + ElementName(most);
    PopupLayout l = LayoutPopup(BarPopupContent(BarWidget::Weather, state, settings), m, false, measure, room);
    const size_t all = l.pieces.size();
    CutPopup(l, room, m.popupPadding);
    CHECK(l.height <= room + 0.01f && l.pieces.size() == all);
    CHECK(l.scrolls.size() == 1 && l.scrolls.front().key == key);
    // Scrolled to its end, its last zone shows.
    const PopupContent first = BarPopupContent(BarWidget::Weather, state, settings);
    const PopupRow& grid     = Section(first, "By Element")->tabs[Section(first, "By Element")->tab].rows.back();
    const std::string last   = Joined(grid.cells[grid.cells.size() - 3].text);
    CHECK(!shows(l, last));
    const std::map<std::string, size_t> scrolled{{key, l.scrolls.front().most}};
    state.scrolled = &scrolled;
    const PopupLayout end = LayoutPopup(BarPopupContent(BarWidget::Weather, state, settings), m, false, measure, room);
    CHECK(shows(end, last) && end.height <= room + 0.01f);
}

TEST(the_zone_says_who_holds_its_region_once_known)
{
    Settings settings;
    BarState state;
    state.zone = 103; // Valkurm Dunes, Zulkheim (region 1)
    CHECK(Joined(Content(BarWidget::Zone, state, settings).subtitle) == "Zulkheim");
    RegionOwners owners{};
    owners[1]    = Nation::Bastok;
    state.owners = owners;
    const WidgetContent w = Content(BarWidget::Zone, state, settings);
    // Its name after the region, as the default design has it, and its crest for the widget's icon.
    CHECK(w.subtitle == (std::vector<Span>{{"Zulkheim", Tone::Dim}, {"  Bastok", Tone::Own, NationColor(Nation::Bastok)}}));
    CHECK(w.picture == GamePicture::BastokCrest);
    state.zone = 230; // Southern San d'Oria: a city region, not in the conquest packet, with its nation's crest
    CHECK(Joined(Content(BarWidget::Zone, state, settings).subtitle) == "San d'Oria");
    CHECK(Content(BarWidget::Zone, state, settings).picture == GamePicture::SanDoriaCrest);
}

TEST(a_widgets_icon_shows_on_the_side_its_setting_picks_or_not_at_all)
{
    Settings settings;
    BarState state;
    state.zone = 103;
    RegionOwners owners{};
    owners[1]    = Nation::Bastok;
    state.owners = owners;
    WidgetOptions& zone = settings.Widget(BarWidget::Zone);
    CHECK(zone.iconSide == IconSide::Left);
    WidgetContent w = Content(BarWidget::Zone, state, settings);
    CHECK(w.picture == GamePicture::BastokCrest && !w.iconsRight);
    zone.iconSide = IconSide::Right;
    w = Content(BarWidget::Zone, state, settings);
    CHECK(w.picture == GamePicture::BastokCrest && w.iconsRight);
    zone.iconSide = IconSide::Hide;
    CHECK(Content(BarWidget::Zone, state, settings).picture == GamePicture::None);
    // The bag's icon and gil's the same way.
    WidgetOptions& bags = settings.Widget(BarWidget::Bags);
    CHECK(Content(BarWidget::Bags, state, settings).icons == std::vector<Icon>{Icon::Bag});
    bags.iconSide = IconSide::Hide;
    CHECK(Content(BarWidget::Bags, state, settings).icons.empty());
    // And the clock's.
    WidgetOptions& clock = settings.Widget(BarWidget::EarthTime);
    clock.on = true;
    CHECK(Content(BarWidget::EarthTime, state, settings).icons == std::vector<Icon>{Icon::Clock});
    clock.iconSide = IconSide::Right;
    CHECK(Content(BarWidget::EarthTime, state, settings).iconsRight);
    clock.iconSide = IconSide::Hide;
    CHECK(Content(BarWidget::EarthTime, state, settings).icons.empty());
    WidgetOptions& gil = settings.Widget(BarWidget::Currency);
    state.gil = 1200;
    CHECK(LeadsWithGil(Content(BarWidget::Currency, state, settings)));
    gil.iconSide = IconSide::Right;
    CHECK(LeadsWithGil(Content(BarWidget::Currency, state, settings)) && Content(BarWidget::Currency, state, settings).iconsRight);
    gil.iconSide = IconSide::Hide;
    CHECK(!LeadsWithGil(Content(BarWidget::Currency, state, settings)));
}

TEST(the_zones_design_puts_the_holders_name_before_or_after_the_zone_or_below_it)
{
    Settings settings;
    WidgetOptions& o = settings.Widget(BarWidget::Zone);
    BarState state;
    state.zone = 103; // Valkurm Dunes, Zulkheim (region 1)
    RegionOwners owners{};
    owners[1]    = Nation::Bastok;
    state.owners = owners;
    const Span dunes{"Valkurm Dunes"}, region{"Zulkheim", Tone::Dim};
    const auto shown = [&](std::string_view design) {
        o.design = DesignFrom(BarWidget::Zone, design);
        return Content(BarWidget::Zone, state, settings);
    };
    CHECK(shown("nation name | region").title ==
          (std::vector<Span>{{"Bastok", Tone::Own, NationColor(Nation::Bastok)}, Text("  Valkurm Dunes")}));
    CHECK(shown("nation name | region").subtitle == std::vector<Span>{region});
    CHECK(shown("name nation | region").title == (std::vector<Span>{dunes, {"  Bastok", Tone::Own, NationColor(Nation::Bastok)}}));
    CHECK(shown("name -nation | region").title == std::vector<Span>{dunes});
    CHECK(shown("name | -region -nation").subtitle.empty());
    // The Beastmen's crest, as the conquest map shows it.
    owners[1]    = Nation::Beastmen;
    state.owners = owners;
    CHECK(shown("name | region").picture == GamePicture::BeastmenCrest);
    // Jeuno's crest in Jeuno, and Aht Urhgan's in its Besieged regions: a crest alone, with no nation named.
    state.zone = 244; // Upper Jeuno
    CHECK(shown("name | region nation").picture == GamePicture::JeunoCrest && shown("name | region nation").subtitle.size() == 1);
    state.zone = 50; // Aht Urhgan Whitegate, West Aht Urhgan
    CHECK(shown("name | region").picture == GamePicture::AhtUrhganCrest);
    // A nation's own city its crest, as no conquest holds it, whoever holds the regions around it.
    owners.fill(Nation::Beastmen);
    state.owners = owners;
    for (const auto& [zone, crest] : {std::pair{230, GamePicture::SanDoriaCrest}, std::pair{233, GamePicture::SanDoriaCrest},
             std::pair{234, GamePicture::BastokCrest}, std::pair{237, GamePicture::BastokCrest},
             std::pair{238, GamePicture::WindurstCrest}, std::pair{242, GamePicture::WindurstCrest},
             std::pair{243, GamePicture::JeunoCrest}})
    {
        state.zone = static_cast<uint16_t>(zone); // Southern San d'Oria, Chateau d'Oraguille, Bastok Mines, Metalworks, ...
        CHECK(shown("name | region nation").picture == crest && shown("name | region nation").subtitle.size() == 1);
    }
    // Before any conquest update too.
    state.owners.reset();
    state.zone = 236; // Port Bastok
    CHECK(shown("name | region").picture == GamePicture::BastokCrest);
    // Nothing outside a region.
    state.zone = 280; // Mog Garden
    CHECK(shown("name | region").picture == GamePicture::None);
}

TEST(the_zone_popup_has_the_zones_number_beside_its_name_over_its_region)
{
    Settings settings;
    BarState state;
    state.zone = 103; // Valkurm Dunes, Zulkheim (region 1)
    const PopupContent dunes = BarPopupContent(BarWidget::Zone, state, settings);
    CHECK(dunes.title == ListOf(Text("Valkurm Dunes"), Text(" (Zone #103)", Tone::Dim)));
    // Its region by name alone, dim, right under the title rather than in a card.
    CHECK(dunes.subtitle == ListOf(Text("Zulkheim", Tone::Dim)) && dunes.sections.empty());
    RegionOwners owners{};
    owners[1]    = Nation::Bastok;
    state.owners = owners; // who holds it is on the bar, and in the popup only beside its share
    CHECK(BarPopupContent(BarWidget::Zone, state, settings) == dunes);
    state.zone = 230; // Southern San d'Oria: a city region, outside the conquest
    const PopupContent city = BarPopupContent(BarWidget::Zone, state, settings);
    CHECK(Joined(city.title) == "Southern San d'Oria (Zone #230)");
    CHECK(Joined(city.subtitle) == "San d'Oria" && city.sections.empty());
    state.zone = 999; // past the data: its number is its title already, and it has no region
    const PopupContent unknown = BarPopupContent(BarWidget::Zone, state, settings);
    CHECK(Joined(unknown.title) == "Zone 999" && unknown.subtitle.empty() && unknown.sections.empty());
    // Show Zone ID in Header off: the name alone.
    settings.Widget(BarWidget::Zone).zoneId = false;
    state.zone = 103;
    CHECK(BarPopupContent(BarWidget::Zone, state, settings).title == ListOf(Text("Valkurm Dunes")));
    CHECK(Settings{}.Widget(BarWidget::Zone).zoneId);
}

TEST(the_zone_popup_ranks_the_influence_in_its_region_and_stars_who_holds_it)
{
    Settings settings;
    BarState state;
    state.zone   = 100;           // West Ronfaure, Ronfaure (region 0)
    state.unixMs = 1791527400000; // Friday 15:30 JST: 203,400 s to Monday 00:00 JST
    RegionOwners owners{};
    owners[0]    = Nation::SanDoria;
    state.owners = owners;
    CHECK(Section(BarPopupContent(BarWidget::Zone, state, settings), "Influence") == nullptr); // not known yet
    state.shares =
        RegionShares{{{Nation::SanDoria, 61}, {Nation::Bastok, 21}, {Nation::Windurst, 11}, {Nation::Beastmen, 5}}};
    const PopupContent popup       = BarPopupContent(BarWidget::Zone, state, settings);
    const PopupSection* influence = Section(popup, "Influence");
    CHECK(influence != nullptr);
    CHECK(Joined(popup.subtitle) == "Ronfaure");
    CHECK(Lines(*influence) == (std::vector<std::string>{"San d'Oria 61%", "Bastok 21%", "Windurst 11%", "Beastmen 5%",
                                   "Tally in 2d 8h"}));
    for (const PopupRow& row : influence->rows)
        CHECK(row.kind == RowKind::Pair); // each figure at the right edge
    CHECK(influence->rows[1].cells[0].text == ListOf(Colored("Bastok ", NationColor(Nation::Bastok))));
    // Each nation's crest before its name.
    CHECK(influence->rows[0].cells[0].picture == GamePicture::SanDoriaCrest &&
          influence->rows[1].cells[0].picture == GamePicture::BastokCrest &&
          influence->rows[2].cells[0].picture == GamePicture::WindurstCrest &&
          influence->rows[3].cells[0].picture == GamePicture::BeastmenCrest);
    // Which lines carry the holder's star, top to bottom.
    const auto starred = [&] {
        std::vector<bool> stars;
        const PopupContent now = BarPopupContent(BarWidget::Zone, state, settings); // named: the loop reads its rows
        for (const PopupRow& row : Section(now, "Influence")->rows)
            stars.push_back(row.cells[0].star);
        return stars;
    };
    CHECK(starred() == (std::vector<bool>{true, false, false, false, false}));
    (*state.owners)[0] = Nation::Beastmen; // a holder behind in the race keeps its place in it
    CHECK(starred() == (std::vector<bool>{false, false, false, true, false}));
    (*state.owners)[0] = Nation::None; // no one holds it
    CHECK(starred() == (std::vector<bool>{false, false, false, false, false}));
    (*state.owners)[0] = Nation::Windurst; // a new week: no influence yet, in the nations' order
    state.shares =
        RegionShares{{{Nation::SanDoria, 0}, {Nation::Bastok, 0}, {Nation::Windurst, 0}, {Nation::Beastmen, 0}}};
    CHECK(Lines(*Section(BarPopupContent(BarWidget::Zone, state, settings), "Influence")) ==
          (std::vector<std::string>{"San d'Oria 0%", "Bastok 0%", "Windurst 0%", "Beastmen 0%", "Tally in 2d 8h"}));
    CHECK(starred() == (std::vector<bool>{false, false, true, false, false}));
    state.zone = 230; // Southern San d'Oria: a city region, outside the conquest
    CHECK(Section(BarPopupContent(BarWidget::Zone, state, settings), "Influence") == nullptr);
}

namespace
{
    // You, a healer low on HP and MP, a red mage, and a black mage in Aht Urhgan Whitegate.
    Party FourMembers()
    {
        Party p{};
        p[0] = PartyMember{"Tanyrus", 1, 75, 13, 37, 1800, 0, 1200, 90, 0, 103, true};
        p[1] = PartyMember{"Healer", 3, 75, 4, 37, 520, 180, 0, 42, 18, 103, true};
        p[2] = PartyMember{"Redmage", 5, 75, 3, 37, 1000, 600, 300, 100, 60, 103, true};
        p[3] = PartyMember{"Away", 4, 75, 3, 37, 100, 50, 0, 10, 5, 50, false};
        return p;
    }

    std::string CellText(const PopupCell& cell) { return Joined(cell.text); }

    ConnectionReport SteadyLink()
    {
        ConnectionReport r;
        r.counters       = NetCounters{227, 175, 100};
        r.silence        = 0.3;
        r.receivedBytes  = 35328.0;
        r.sentBytes      = 1258291.0;
        r.countedSeconds = 4320.0;
        return r;
    }

    // Every part of the bar's state set, each to something its widgets show.
    struct FullState
    {
        ActorInfo target = Goblin();
        BarState state;
        FullState()
        {
            // East Ronfaure's Carrion Worm, one of Bigmouth Billy's placeholders: its popup counts down.
            target.mob      = MobById(17191194);
            target.serverId = 17191194;
            state.unixMs    = 1791533220000; // 2026-10-09 08:07 UTC, a day Valkurm Dunes may have clouds
            state.earthYear = 2026, state.earthMonth = 10, state.earthDay = 8;
            state.earthHour = 23, state.earthMinute = 7;
            state.zone            = 103; // Valkurm Dunes, Zulkheim
            state.target          = &target;
            state.targetPhSeconds = 299.0;
            state.targetDistance  = 8.5f;
            state.gil             = 1234567;
            for (int c = 0; c < kContainerCount; ++c)
                state.bags[c] = BagCount{static_cast<uint32_t>(c + 1), 80};
            RegionOwners owners{};
            owners[1]     = Nation::Bastok;
            state.owners  = owners;
            state.shares =
                RegionShares{{{Nation::Bastok, 52}, {Nation::SanDoria, 30}, {Nation::Windurst, 10}, {Nation::Beastmen, 8}}};
            // No weather yet: the Forecast shows the zone's likeliest, so it reads the zone.
            state.weather.reset();
            state.strip    = StripText{"Mog Wardrobe 8", "Help on the selected item"};
            state.stripBag = BagCount{3, 80};
            state.frames   = FrameReport{1000.0 / 30.0, 120.0};
            JobSample job;
            job.mainJob = 1, job.mainLevel = 30, job.subJob = 13, job.subLevel = 15;
            job.exp = 4140, job.expNeeded = 7600;
            state.job = JobReport{job, 18000.0};
            state.connection = SteadyLink();
            state.party      = FourMembers();
            state.statuses   = {ActiveStatus{kSignetEffect, 6130.0}};
            state.memory     = MemoryReport{1532, 2563, 4095};
            state.cpuPercent = 18.0;
            LinuxXIReport report;
            report.flags      = kPoolHooked | kPoolOn | kGraceActive | kGraceOn;
            report.copiesHeld = 588;
            report.addonCount = 1;
            report.lua[0]     = LinuxXIAddon{"xiui", 41250};
            state.linuxxi.report = report;
        }
    };

    // What the plugin gathers for needs: the rest stays as a BarState starts.
    BarState Gathered(const BarState& full, const BarNeeds& needs)
    {
        BarState s;
        s.weather = full.weather;
        s.owners  = full.owners;
        s.shares   = full.shares;
        if (needs.unixTime) s.unixMs = full.unixMs;
        if (needs.localTime)
        {
            s.earthYear   = full.earthYear;
            s.earthMonth  = full.earthMonth;
            s.earthDay    = full.earthDay;
            s.earthHour   = full.earthHour;
            s.earthMinute = full.earthMinute;
        }
        if (needs.zone) s.zone = full.zone;
        if (needs.target)
        {
            s.target          = full.target;
            s.targetPhSeconds = full.targetPhSeconds;
            s.targetDistance  = full.targetDistance;
            s.targetCheck     = full.targetCheck;
        }
        if (needs.gil) s.gil = full.gil;
        for (int c = 0; c < needs.containers; ++c)
            s.bags[c] = full.bags[c];
        if (needs.strip)
        {
            s.strip    = full.strip;
            s.stripBag = full.stripBag;
        }
        if (needs.frames) s.frames = full.frames;
        if (needs.job) s.job = full.job;
        if (needs.connection) s.connection = full.connection;
        if (needs.party) s.party = full.party;
        if (needs.statuses) s.statuses = full.statuses;
        if (needs.memory) s.memory = full.memory;
        if (needs.cpu) s.cpuPercent = full.cpuPercent;
        if (needs.linuxxi) s.linuxxi = full.linuxxi;
        return s;
    }

    // needs with one of its parts taken away, for each part it has.
    std::vector<BarNeeds> EachOneLess(const BarNeeds& needs)
    {
        std::vector<BarNeeds> less;
        for (bool BarNeeds::*part :
            {&BarNeeds::unixTime, &BarNeeds::localTime, &BarNeeds::zone, &BarNeeds::target, &BarNeeds::gil,
            &BarNeeds::strip, &BarNeeds::frames, &BarNeeds::job, &BarNeeds::connection,
            &BarNeeds::party, &BarNeeds::statuses, &BarNeeds::memory, &BarNeeds::cpu, &BarNeeds::linuxxi})
            if (needs.*part)
            {
                less.push_back(needs);
                less.back().*part = false;
            }
        if (needs.containers > 0)
        {
            less.push_back(needs);
            less.back().containers = needs.containers == kContainerCount ? 1 : 0;
        }
        return less;
    }

    // What the bar shows of widget from state: the widget, or Game help's own bar, and its popup when open.
    bool SameShown(BarWidget widget, std::optional<PanelWidget> open, const BarState& a, const BarState& b,
        const Settings& settings)
    {
        const bool helpBar = widget != BarWidget::GameHelp || (a.strip == b.strip && StripBagFill(a) == StripBagFill(b));
        return helpBar && Content(widget, a, settings) == Content(widget, b, settings) &&
               (!open || BarPopupContent(*open, a, settings) == BarPopupContent(*open, b, settings));
    }
}

TEST(the_bar_gathers_what_each_shown_widget_and_open_popup_reads_and_nothing_more)
{
    const FullState full;
    for (int i = 0; i < kBarWidgetCount; ++i)
    {
        const auto widget = static_cast<BarWidget>(i);
        Settings settings;
        for (WidgetOptions& each : settings.widgets)
            each.on = false;
        settings.Widget(widget).on = true;
        const bool hasPopup = Content(widget, full.state, settings).hasPopup;
        for (const std::optional<PanelWidget> open : {std::optional<PanelWidget>(), std::optional<PanelWidget>(widget)})
        {
            if (open && !hasPopup) continue;
            const BarNeeds needs = NeedsOf(settings, open ? std::vector<PanelWidget>{*open} : std::vector<PanelWidget>{});
            CHECK(SameShown(widget, open, full.state, Gathered(full.state, needs), settings));
            for (const BarNeeds& less : EachOneLess(needs))
                CHECK(!SameShown(widget, open, full.state, Gathered(full.state, less), settings));
        }
    }
}

TEST(the_other_bags_are_gathered_only_while_their_popup_is_open)
{
    Settings settings;
    CHECK(NeedsOf(settings, {}).containers == 1);
    CHECK(NeedsOf(settings, {BarWidget::Target}).containers == 1);
    CHECK(NeedsOf(settings, {BarWidget::Bags}).containers == kContainerCount);
    for (WidgetOptions& each : settings.widgets)
        each.on = false;
    const BarNeeds none = NeedsOf(settings, {BarWidget::Bags}); // a popup open as its widget is switched off
    CHECK(!none.unixTime && !none.localTime && !none.zone && !none.target && !none.gil && none.containers == 0);
}

TEST(a_caption_names_a_bag_only_when_it_is_that_bags_name)
{
    CHECK(ContainerNamed("Inventory") == 0 && ContainerNamed("Mog Wardrobe 8") == 16);
    CHECK(ContainerNamed("Items") == 0); // the Items menu as it opens: its list is the inventory's
    // The Mog House's own short captions for some of them.
    CHECK(ContainerNamed("Locker") == 4 && ContainerNamed("M.Ward.") == 8 && ContainerNamed("M.Ward. 2") == 10);
    CHECK(ContainerNamed("M.Ward. 8") == 16 && !ContainerNamed("M.Ward. 9").has_value());
    CHECK(ContainerNamed("Satchel") == 5 && ContainerNamed("Mog Sack") == 6); // as View House's lists title them
    CHECK(!ContainerNamed("Inventory 1/30").has_value()); // the game's own count, if it is there, stays the game's
    CHECK(!ContainerNamed("Commands").has_value() && !ContainerNamed("").has_value());
}

TEST(game_help_is_never_on_the_bar_and_its_popup_has_the_caption_over_the_help)
{
    Settings settings;
    BarState state;
    state.strip = StripText{"Commands", "Select a target to auto-attack."};
    CHECK(!Content(BarWidget::GameHelp, state, settings).shown); // it has its own bar
    const PopupContent p = BarPopupContent(BarWidget::GameHelp, state, settings);
    CHECK(Joined(p.title) == "Commands" && Lines(p.sections[0]) == (std::vector<std::string>{"Select a target to auto-attack."}));
    CHECK(!p.settings); // no cog: its settings are the Help Bar card's
}

TEST(a_bag_caption_gains_that_bags_count)
{
    Settings settings;
    BarState state;
    state.strip    = StripText{"Inventory", "Bronze Sword"};
    state.stripBag = BagCount{12, 30};
    CHECK(StripBagFill(state) && StripBagFill(state)->text == "12 / 30");
    CHECK(Lines(BarPopupContent(BarWidget::GameHelp, state, settings).sections[0]) ==
          (std::vector<std::string>{"Bronze Sword", "Items 12 / 30"}));
}

TEST(a_help_line_without_a_caption_wraps_in_the_popup_under_a_fixed_title)
{
    Settings settings;
    BarState state;
    const std::string help = "Select a target to auto-attack, or choose an ally to cast on, then confirm with the enter key "
                             "to begin. Press cancel to return to the previous menu.";
    state.strip = StripText{"", help};
    const PopupContent p = BarPopupContent(BarWidget::GameHelp, state, settings);
    CHECK(Joined(p.title) == "Game Help");
    const std::vector<std::string> lines = Lines(p.sections[0]);
    CHECK(lines.size() == 2);
    for (const std::string& line : lines)
        CHECK(line.size() <= 80);
    CHECK(lines[0] + " " + lines[1] == help);
}

TEST(the_clock_can_stack_the_servers_time_under_its_own_and_its_popup_has_the_resets)
{
    Settings settings;
    BarState state;
    state.unixMs    = 1791527400000 + 42000; // Friday 15:30:42 JST, Thursday 23:30:42 on the US Pacific coast
    state.earthYear = 2026, state.earthMonth = 10, state.earthDay = 9; // the computer's own: 08:30:42 in Berlin
    state.earthHour = 8, state.earthMinute = 30, state.earthSecond = 42;
    WidgetContent w = Content(BarWidget::EarthTime, state, settings);
    CHECK(w.shown && w.hasPopup && Joined(w.title) == "08:30" && w.subtitle.empty()); // not stacked unless asked
    settings.Widget(BarWidget::EarthTime).design = DesignFrom(BarWidget::EarthTime, "time | server");
    w                        = Content(BarWidget::EarthTime, state, settings);
    CHECK(Joined(w.title) == "08:30" && Joined(w.subtitle) == "JST 15:30" && w.subtitle.back().tone == Tone::Dim);
    settings.Widget(BarWidget::EarthTime).clockSeconds = true; // the seconds are the upper line's
    settings.Widget(BarWidget::EarthTime).clock24      = false;
    w                     = Content(BarWidget::EarthTime, state, settings);
    CHECK(Joined(w.title) == "8:30:42 AM" && Joined(w.subtitle) == "JST 3:30 PM");
    settings.Widget(BarWidget::EarthTime).clockZone = ClockZone::UsPacific;
    w                  = Content(BarWidget::EarthTime, state, settings);
    CHECK(Joined(w.title) == "PDT 11:30:42 PM" && Joined(w.subtitle) == "JST 3:30 PM");
    // The popup has the server's clock and its resets whether stacked or not: 8h 29m 18s to Saturday 00:00 JST.
    for (const bool stacked : {true, false})
    {
        settings.Widget(BarWidget::EarthTime).design = DesignFrom(BarWidget::EarthTime, stacked ? "time | server" : "time | -server");
        const PopupContent p     = BarPopupContent(BarWidget::EarthTime, state, settings);
        CHECK(Joined(p.title) == "October 8th, 2026 - 11:30:42pm"); // the Pacific coast's date
        CHECK(p.sections.size() == 1 && Lines(p.sections[0]) == (std::vector<std::string>{"Server Time Friday 15:30 JST",
                                                                    "Daily Reset in 8h 29m", "Conquest Tally in 2d 8h"}));
    }
}

TEST(the_performance_widget_shows_frames_a_second_and_its_popup_the_frame_times)
{
    Settings settings;
    settings.Widget(BarWidget::Performance).design = DesignFrom(BarWidget::Performance, "fps |");
    BarState state;
    CHECK(Joined(Content(BarWidget::Performance, state, settings).title) == "-");
    state.frames = FrameReport{1000.0 / 30.0, 120.0};
    const WidgetContent w = Content(BarWidget::Performance, state, settings);
    CHECK(w.shown && w.hasPopup && w.figures && Joined(w.title) == "30 fps" && w.subtitle.empty());
    const PopupContent p = BarPopupContent(BarWidget::Performance, state, settings);
    CHECK(Joined(p.title) == "Performance");
    const auto processor = std::ranges::find(p.sections, std::string("Processor"), &PopupSection::heading);
    CHECK(processor != p.sections.end() &&
          Lines(*processor) == (std::vector<std::string>{"Frame Rate 30 fps", "Frame Time 33.3 ms", "Slowest Frame 120.0 ms"}));
}

TEST(the_performance_widget_puts_each_part_picked_on_one_line_and_reads_only_those)
{
    Settings settings;
    BarState state;
    state.frames     = FrameReport{1000.0 / 30.0, 120.0};
    state.connection = SteadyLink();
    state.memory     = MemoryReport{1532, 2563, 4095};
    WidgetOptions& o = settings.Widget(BarWidget::Performance);
    // By default the rates, the game's meter and PlayOnline's messages, as the Connection widget showed.
    CHECK(Joined(Content(BarWidget::Performance, state, settings).title) == "S 175  R 227  100%");
    o.design = DesignFrom(BarWidget::Performance, "fps received memory |");
    o.memText = true;
    const std::string line = Joined(Content(BarWidget::Performance, state, settings).title);
    CHECK(line.starts_with("30 fps  R 227  Mem ") && line.find("S ") == std::string::npos);
    // Without Show Mem Text, the memory's figures alone.
    o.memText = false;
    const std::string bare = Joined(Content(BarWidget::Performance, state, settings).title);
    std::string unlabeled = line;
    unlabeled.erase(unlabeled.find("Mem "), std::string_view("Mem ").size());
    CHECK_EQ(bare, unlabeled);
    // In the design's order, on either line.
    o.design = DesignFrom(BarWidget::Performance, "received | fps");
    CHECK(Joined(Content(BarWidget::Performance, state, settings).title) == "R 227");
    CHECK(Joined(Content(BarWidget::Performance, state, settings).subtitle) == "30 fps");
    o.design = DesignFrom(BarWidget::Performance, "fps received memory |");
    CHECK(NeedsOf(settings, {}).frames && NeedsOf(settings, {}).connection &&
          NeedsOf(settings, {}).memory && !NeedsOf(settings, {}).cpu);
    o.design = DesignFrom(BarWidget::Performance, "fps |");
    CHECK(!NeedsOf(settings, {}).connection && !NeedsOf(settings, {}).memory);
    // Its popup reads everything.
    const BarNeeds open = NeedsOf(settings, {PanelWidget{BarWidget::Performance}});
    CHECK(open.connection && open.memory && open.cpu && open.linuxxi);
}

TEST(the_performance_widgets_memory_turns_amber_then_red_as_the_space_left_runs_low)
{
    Settings settings;
    settings.Widget(BarWidget::Performance).design = DesignFrom(BarWidget::Performance, "memory |");
    BarState state;
    const auto used = [&](uint32_t leftMb) {
        constexpr uint32_t kCeilingMb = 4095;
        state.memory = MemoryReport{kCeilingMb - leftMb, leftMb, kCeilingMb};
        return Content(BarWidget::Performance, state, settings).title.back();
    };
    CHECK(used(kLowLeftMb).tone == Tone::Normal);
    CHECK(used(kLowLeftMb - 1) == Colored("3,584 MB", kWarningColor)); // 4,095 - 511
    CHECK(used(kCriticalLeftMb - 1) == Colored("3,840 MB", kAlarmColor));
}

TEST(the_job_widget_shows_jobs_and_exp_to_next_or_merits_at_the_cap_over_a_bar_of_either)
{
    Settings settings;
    BarState state;
    CHECK(Joined(Content(BarWidget::Job, state, settings).title) == "-"); // no job: the title screen
    JobSample s;
    s.mainJob = 1, s.mainLevel = 30, s.subJob = 13, s.subLevel = 15;
    s.exp = 4140, s.expNeeded = 7600;
    state.job = JobReport{s, 18000.0};
    const WidgetContent w = Content(BarWidget::Job, state, settings);
    CHECK(w.shown && w.hasPopup && w.figures && Joined(w.title) == "WAR 30 / NIN 15" &&
          Joined(w.subtitle) == "3,460 til next level  ");
    CHECK(w.subtitle.back().bar && std::fabs(w.subtitle.back().bar->done - 4140.0f / 7600.0f) < 1e-6f); // the bar after it
    CHECK(Joined(BarPopupContent(BarWidget::Job, state, settings).title) == "Job and EXP"); // words: the title font
    CHECK(Lines(BarPopupContent(BarWidget::Job, state, settings).sections[0]) ==
          (std::vector<std::string>{"Earned EXP 4,140 / 7,600", "Til Next Level 3,460", "EXP Per Hour 18,000", "Next Level in 11m 32s",
              "Limit Points 0 / 10,000"})); // 7,600 - 4,140 = 3,460; at 18,000 an hour that is 692 s

    s.mainLevel = 75, s.subLevel = 37, s.merits = 7, s.meritsMax = 10, s.limitPoints = 4210;
    state.job = JobReport{s, std::nullopt};
    CHECK(Content(BarWidget::Job, state, settings).subtitle.front().text == "7 merits");
    CHECK(Content(BarWidget::Job, state, settings).subtitle.back().bar->done == 0.421f); // limit points to the next merit
    CHECK(Lines(BarPopupContent(BarWidget::Job, state, settings).sections[0]) ==
          (std::vector<std::string>{"Merits 7 / 10", "Limit Points 4,210 / 10,000", "Per Hour -",
              "Limit Mode is off, so EXP does not become merits."}));
}

TEST(the_job_widget_says_one_merit_and_promises_no_merit_without_limit_mode)
{
    Settings settings;
    BarState state;
    JobSample s;
    s.mainJob = 1, s.mainLevel = 75, s.merits = 1, s.meritsMax = 10, s.limitPoints = 100;
    state.job = JobReport{s, 5000.0};
    CHECK(Content(BarWidget::Job, state, settings).subtitle.front().text == "1 merit");
    for (const std::string& line : Lines(BarPopupContent(BarWidget::Job, state, settings).sections[0]))
        CHECK(line.find("Next merit") == std::string::npos); // Limit Mode is off
    state.job->sample.limitMode = true;
    const auto lines = Lines(BarPopupContent(BarWidget::Job, state, settings).sections[0]);
    CHECK(std::any_of(lines.begin(), lines.end(), [](const std::string& l) { return l.find("Next Merit in") == 0; }));
}

// Round 22: "If there are no messages and its the only thing selected for Performance it should just hide itself".
TEST(the_performance_widget_hides_while_messages_alone_are_on_and_none_wait)
{
    Settings settings;
    settings.Widget(BarWidget::Performance).design = DesignFrom(BarWidget::Performance, "messages |");
    BarState state;
    state.connection = SteadyLink();
    const PanelWidget performance{BarWidget::Performance};
    const auto room = [&](std::optional<PanelWidget> settingsShown) {
        return TakesRoom(Content(BarWidget::Performance, state, settings), performance, settingsShown);
    };
    CHECK(!room(std::nullopt));
    state.connection->playOnline = PlayOnlineCounts{0, 0};
    CHECK(!room(std::nullopt) && !room(PanelWidget{BarWidget::Zone}));
    // The user: "since Performance is hidden because of no message, I can't open the edit for it".
    CHECK(room(performance) && Joined(Content(BarWidget::Performance, state, settings).title) == "-");
    state.connection->playOnline = PlayOnlineCounts{0, 2};
    const WidgetContent mail     = Content(BarWidget::Performance, state, settings);
    CHECK(TakesRoom(mail, performance, std::nullopt) && !mail.title.empty());
}

// The card "Allow disabling widget text when linked": a widget linked into a pill may have every part off.
TEST(a_widget_with_every_part_off_hides_and_one_with_a_part_on_and_nothing_to_say_shows_a_dash)
{
    Settings settings;
    settings.Widget(BarWidget::Zone).design = DesignFrom(BarWidget::Zone, "-name -region -nation |");
    BarState state;
    CHECK(!TakesRoom(Content(BarWidget::Zone, state, settings), BarWidget::Zone, std::nullopt));
    CHECK(TakesRoom(Content(BarWidget::Zone, state, settings), BarWidget::Zone, PanelWidget{BarWidget::Zone}));
    settings.Widget(BarWidget::Bags).design = DesignFrom(BarWidget::Bags, "count1 |");
    CHECK(Joined(Content(BarWidget::Bags, state, settings).title) == "-");
    CHECK(TakesRoom(Content(BarWidget::Bags, state, settings), BarWidget::Bags, std::nullopt));
}

TEST(the_performance_widget_shows_the_games_rates_on_one_line_and_turns_amber_then_red)
{
    Settings settings;
    BarState state;
    state.connection = SteadyLink();
    WidgetContent w  = Content(BarWidget::Performance, state, settings);
    CHECK(w.shown && w.hasPopup && w.figures && Joined(w.title) == "S 175  R 227  100%" && w.subtitle.empty());
    CHECK(w.title[1] == Text("175"));

    state.connection->silence = 3.0;
    w = Content(BarWidget::Performance, state, settings);
    CHECK(w.title[1] == Text("175")); // the game's counters still see data arrive
    state.connection->counters->received = 0;
    w = Content(BarWidget::Performance, state, settings);
    CHECK(w.title[1] == Colored("175", kWarningColor) && w.title[3] == Colored("0", kWarningColor));

    state.connection->silence          = 13.0;
    state.connection->counters->health = 94;
    w = Content(BarWidget::Performance, state, settings);
    CHECK(w.title[1] == Colored("175", kAlarmColor) && w.subtitle.empty());

    state.connection->counters.reset(); // the chain could not be read
    state.connection->silence = 0.3;
    CHECK(Joined(Content(BarWidget::Performance, state, settings).title) == "S -  R -");
}

TEST(the_performance_popup_gives_the_rates_last_data_and_session_totals)
{
    Settings settings;
    BarState state;
    state.connection     = SteadyLink();
    const PopupContent p = BarPopupContent(BarWidget::Performance, state, settings);
    CHECK(Joined(p.title) == "Performance" && p.sections.size() == 2 && p.sections[0].heading == "Connection");
    // Short lines only: no sentence on where S and R come from (the menu tip says it).
    CHECK(Lines(p.sections[0]) ==
          (std::vector<std::string>{"Sent 175 B/s", "Received 227 B/s", "Last Server Data 1s"})); // 0.3 s, rounded up
    CHECK(p.sections[0].rows[2] == Labeled("Last Server Data", Text("1s")));
    CHECK(p.sections[1].heading == "This Session");
    CHECK(Lines(p.sections[1]) == (std::vector<std::string>{"Sent 1.2 MB", "Received 34.5 KB", "Over 1h 12m"}));
}

TEST(the_performance_popup_colors_the_last_server_data_as_the_bar_and_names_the_meter_only_under_100)
{
    Settings settings;
    BarState state;
    state.connection          = SteadyLink();
    state.connection->silence = 17.4;
    state.connection->counters->received = 0;
    PopupContent p            = BarPopupContent(BarWidget::Performance, state, settings);
    CHECK(p.sections[0].rows[2] ==
          Line(Cell(ListOf(Colored("Last Server Data ", kWarningColor), Colored("18s", kWarningColor)))));
    CHECK(p.sections[0].rows.size() == 3); // the meter is still full

    state.connection->counters->health = 94;
    p = BarPopupContent(BarWidget::Performance, state, settings);
    CHECK(p.sections[0].rows[2] ==
          Line(Cell(ListOf(Colored("Last Server Data ", kAlarmColor), Colored("18s", kAlarmColor)))));
    CHECK(Lines(p.sections[0]).back() == "The game's own meter is at 94%: it falls once 10 s pass without server data.");

    state.connection->zoning = true; // no color while zoning; the meter line stays while the meter is under 100
    p = BarPopupContent(BarWidget::Performance, state, settings);
    CHECK(p.sections[0].rows[2] == Labeled("Last Server Data", Text("18s")) && p.sections[0].rows.size() == 4);
}

TEST(the_party_widget_counts_members_and_shows_the_lowest_hp_and_mp_in_your_zone)
{
    Settings settings;
    settings.Widget(BarWidget::Party).on = true;
    BarState state;
    state.party           = FourMembers();
    const WidgetContent w = Content(BarWidget::Party, state, settings);
    CHECK(w.shown && w.hasPopup && w.figures && Joined(w.title) == "Party 4" &&
          Joined(w.subtitle) == "HP 42% \xC2\xB7 MP 18%");
    CHECK(w.subtitle[1] == Colored("42%", kWarningColor) && w.subtitle[4] == Colored("18%", kAlarmColor));

    state.party[1] = PartyMember{"Samurai", 12, 75, 1, 37, 1500, 0, 0, 100, 0, 103, true};
    state.party[2].reset();
    state.party[3].reset();
    CHECK(Joined(Content(BarWidget::Party, state, settings).subtitle) == "HP 90%"); // no MP among them

    state.party[1].reset();
    const WidgetContent solo = Content(BarWidget::Party, state, settings);
    CHECK(Joined(solo.title) == "Solo" && solo.subtitle.empty() && !solo.hasPopup);
}

TEST(the_party_popup_lists_each_member_here_and_gives_one_elsewhere_a_row_of_its_own)
{
    Settings settings;
    BarState state;
    state.party          = FourMembers();
    const PopupContent p = BarPopupContent(BarWidget::Party, state, settings);
    CHECK(Joined(p.title) == "Party" && p.sections[0].rows.size() == 2);
    const PopupRow& grid = p.sections[0].rows[0];
    CHECK(grid.kind == RowKind::Grid && grid.columns == 5 && grid.cells.size() == 20); // a heading row, then three
    CHECK(CellText(grid.cells[2]) == "HP" && CellText(grid.cells[4]) == "TP");
    CHECK(CellText(grid.cells[5]) == "Tanyrus" && CellText(grid.cells[6]) == "WAR 75 / NIN 37");
    CHECK(grid.cells[8].text == ListOf(Text("-", Tone::Dim)));          // a warrior has no MP
    CHECK(grid.cells[9].text == ListOf(Colored("1,200", kReadyColor))); // TP enough for a weapon skill
    CHECK(grid.cells[12].text == ListOf(Colored("520", kWarningColor))); // the healer's HP at 42%
    CHECK(grid.cells[13].text == ListOf(Colored("180", kAlarmColor)));   // and MP at 18%
    CHECK(CellText(grid.cells[15]) == "Redmage");
    // The member elsewhere: their name and zone on one row, after the grid, their stale figures left out.
    const PopupRow& away = p.sections[0].rows[1];
    CHECK(away.kind == RowKind::Line && away.cells.size() == 2);
    CHECK(away.cells[0].text == ListOf(Text("Away")) &&
          away.cells[1].text == ListOf(Text("In Aht Urhgan Whitegate", Tone::Dim)));
}

TEST(a_member_zoning_or_in_another_zone_never_counts_toward_the_lowest_figures)
{
    Settings settings;
    settings.Widget(BarWidget::Party).on = true;
    BarState state;
    state.party[0] = PartyMember{"Tanyrus", 4, 75, 3, 37, 1800, 900, 0, 80, 70, 103, true};
    state.party[1] = PartyMember{"Zoning", 3, 75, 4, 37, 0, 0, 0, 0, 0, 0, false};
    state.party[2] = PartyMember{"Away", 3, 75, 4, 37, 10, 10, 0, 1, 1, 50, false};
    const WidgetContent w = Content(BarWidget::Party, state, settings);
    CHECK(Joined(w.title) == "Party 3" && Joined(w.subtitle) == "HP 80% \xC2\xB7 MP 70%");
    PopupContent popup     = BarPopupContent(BarWidget::Party, state, settings);
    const PopupRow& zoning = popup.sections[0].rows[1];
    CHECK(zoning.cells[0].text == ListOf(Text("Zoning")) && zoning.cells[1].text == ListOf(Text("Zoning", Tone::Dim)));
    state.party[1]->zone = 9999; // a zone the tables lack
    popup = BarPopupContent(BarWidget::Party, state, settings);
    const PopupRow& unknown = popup.sections[0].rows[1];
    CHECK(unknown.cells[1].text == ListOf(Text("Zone 9999", Tone::Dim)));
}

TEST(a_dead_member_counts_and_turns_the_face_red)
{
    Settings settings;
    settings.Widget(BarWidget::Party).on = true;
    BarState state;
    state.party[0] = PartyMember{"Tanyrus", 4, 75, 3, 37, 1800, 900, 0, 80, 70, 103, true};
    state.party[1] = PartyMember{"Dead", 1, 75, 0, 0, 0, 0, 0, 0, 0, 103, true};
    const WidgetContent w = Content(BarWidget::Party, state, settings);
    CHECK(Joined(w.subtitle) == "HP 0% \xC2\xB7 MP 70%" && w.subtitle[1] == Colored("0%", kAlarmColor));
}

TEST(the_figures_turn_amber_below_half_and_red_below_a_quarter)
{
    Settings settings;
    settings.Widget(BarWidget::Party).on = true;
    BarState state;
    state.party[0] = PartyMember{"Tanyrus", 4, 75, 3, 37, 1, 1, 0, 50, 25, 103, true};
    state.party[1] = PartyMember{"Other", 4, 75, 3, 37, 1, 1, 0, 100, 100, 103, true};
    WidgetContent w = Content(BarWidget::Party, state, settings);
    CHECK(w.subtitle[1] == Text("50%") && w.subtitle[4] == Colored("25%", kWarningColor));
    state.party[0]->hpPercent = 49, state.party[0]->mpPercent = 24;
    w = Content(BarWidget::Party, state, settings);
    CHECK(w.subtitle[1] == Colored("49%", kWarningColor) && w.subtitle[4] == Colored("24%", kAlarmColor));
}

TEST(a_party_with_everyone_else_away_shows_only_your_own_figures)
{
    Settings settings;
    settings.Widget(BarWidget::Party).on = true;
    BarState state;
    state.party[0] = PartyMember{"Tanyrus", 1, 75, 13, 37, 1800, 0, 0, 90, 0, 103, true};
    state.party[1] = PartyMember{"Away", 4, 75, 3, 37, 100, 50, 0, 10, 5, 50, false};
    const WidgetContent w = Content(BarWidget::Party, state, settings);
    CHECK(Joined(w.title) == "Party 2" && Joined(w.subtitle) == "HP 90%");
}

TEST(a_member_whose_data_has_not_arrived_is_left_out_of_the_lowest_and_shows_dashes)
{
    Settings settings;
    settings.Widget(BarWidget::Party).on = true;
    BarState state;
    state.party[0] = PartyMember{"Tanyrus", 4, 75, 3, 37, 1800, 900, 0, 80, 70, 103, true};
    state.party[1] = PartyMember{"New", 0, 0, 0, 0, 0, 0, 0, 0, 0, 103, true}; // no job yet
    const WidgetContent w = Content(BarWidget::Party, state, settings);
    CHECK(Joined(w.title) == "Party 2" && Joined(w.subtitle) == "HP 80% \xC2\xB7 MP 70%");
    const PopupContent p = BarPopupContent(BarWidget::Party, state, settings);
    const PopupRow& grid = p.sections[0].rows[0];
    CHECK(grid.cells.size() == 15 && CellText(grid.cells[10]) == "New");
    for (int c = 11; c < 15; ++c)
        CHECK(grid.cells[c].text == ListOf(Text("-", Tone::Dim)));
}

TEST(a_checked_player_shows_their_jobs_and_a_popup_of_the_check)
{
    Settings settings;
    BarState state;
    ActorInfo player;
    player.kind     = EntityKind::Player;
    player.serverId = 0x01006A2B;
    std::snprintf(player.name, sizeof(player.name), "%s", "Tanyrus");
    state.target    = &player;
    WidgetContent w = Content(BarWidget::Target, state, settings);
    CHECK(Joined(w.title) == "Tanyrus" && w.subtitle.empty() && !w.hasPopup); // never checked: as before
    PlayerCheck check{InspectGeneral{0x01006A2B, 0x410, "Phoenix", 1, 13, 75, 37, 0}, InspectMessage{"Tanyrus", 206}};
    state.targetCheck = check;
    w = Content(BarWidget::Target, state, settings);
    CHECK(Joined(w.subtitle) == "WAR 75 / NIN 37" && w.hasPopup);
    CHECK(Lines(BarPopupContent(BarWidget::Target, state, settings).sections[0]) ==
          (std::vector<std::string>{"Jobs WAR 75 / NIN 37", "Linkshell Phoenix", "Title New Adventurer",
              "As of Your Check"}));
    check.general.masterLevel = 5;
    check.general.linkshell.clear();
    state.targetCheck = check;
    CHECK(Lines(BarPopupContent(BarWidget::Target, state, settings).sections[0]) ==
          (std::vector<std::string>{"Jobs WAR 75 / NIN 37", "Master Level 5", "Title New Adventurer",
              "As of Your Check"}));
    check.general.mainJob = check.general.subJob = check.general.mainLevel = check.general.subLevel = 0;
    state.targetCheck = check; // anonymous
    CHECK(Joined(Content(BarWidget::Target, state, settings).subtitle) == "? ? ?");
}

TEST(a_checked_players_title_shows_its_number_when_the_list_lacks_it_and_not_at_all_when_none)
{
    Settings settings;
    BarState state;
    ActorInfo player;
    player.kind     = EntityKind::Player;
    player.serverId = 0x01006A2B;
    state.target    = &player;
    state.targetCheck = PlayerCheck{InspectGeneral{0x01006A2B, 0x410, "", 1, 13, 75, 37, 0}, InspectMessage{"", 99999}};
    CHECK(Lines(BarPopupContent(BarWidget::Target, state, settings).sections[0]) ==
          (std::vector<std::string>{"Jobs WAR 75 / NIN 37", "Title #99999", "As of Your Check"}));
    state.targetCheck->message.title = 0;
    CHECK(Lines(BarPopupContent(BarWidget::Target, state, settings).sections[0]) ==
          (std::vector<std::string>{"Jobs WAR 75 / NIN 37", "As of Your Check"}));
}

TEST(the_status_timers_widget_shows_its_pins_and_reads_your_statuses_only_while_shown)
{
    Settings settings;
    BarState state;
    state.statuses = {ActiveStatus{kSignetEffect, 6130.0}};
    CHECK(!NeedsOf(settings, {}).statuses); // off on a new install
    settings.Widget(BarWidget::StatusTimers).on = true;
    CHECK(NeedsOf(settings, {}).statuses); // Signet is pinned
    settings.statusPins.clear();
    CHECK(!NeedsOf(settings, {}).statuses); // nothing pinned to show
    CHECK(NeedsOf(settings, {PanelWidget{BarWidget::StatusTimers}}).statuses);
    settings.statusPins = {kSignetEffect};
    // Its icon, then its name, as new.
    const WidgetContent shown = Content(BarWidget::StatusTimers, state, settings);
    CHECK(shown.title[0].status && shown.title[0].status->id == kSignetEffect && Joined(shown.title) == " Signet 1h 42m");
    // Its popup's favorites are the settings', Signet and Sanction on a new install.
    const PopupContent p = BarPopupContent(BarWidget::StatusTimers, state, settings);
    CHECK(Joined(p.sections[0].rows[0].cells[2].text) == "Signet" && Joined(p.sections[0].rows[0].cells[6].text) == "Sanction");
}

TEST(a_statuss_star_and_pin_add_it_and_take_it_off_and_its_star_takes_its_pin_too)
{
    Settings s;
    CHECK((s.statusPins == std::vector<uint16_t>{kSignetEffect})); // Signet is pinned on a new install
    const auto press = [&](const char* kind, uint16_t id) {
        return ApplyPopupPress(s, std::string(kind) + std::to_string(id), Nation::None);
    };
    CHECK(press(kStatusFavoritePress, 40) == PressResult::Changed);
    CHECK((s.statusFavorites == std::vector<uint16_t>{kSignetEffect, kSanctionEffect, 40}));
    CHECK(press(kStatusPinPress, 40) == PressResult::Changed);
    CHECK((s.statusPins == std::vector<uint16_t>{kSignetEffect, 40}));
    CHECK(press(kStatusPinPress, kSignetEffect) == PressResult::Changed);
    CHECK((s.statusPins == std::vector<uint16_t>{40}));
    // The last pin stays, by its pin or by its star, which would take it off too.
    CHECK(press(kStatusPinPress, 40) == PressResult::Refused);
    CHECK(press(kStatusFavoritePress, 40) == PressResult::Refused);
    CHECK((s.statusPins == std::vector<uint16_t>{40}) && s.statusFavorites.size() == 3);
    CHECK(press(kStatusPinPress, kSignetEffect) == PressResult::Changed);
    CHECK(press(kStatusFavoritePress, 40) == PressResult::Changed);
    CHECK((s.statusFavorites == std::vector<uint16_t>{kSignetEffect, kSanctionEffect}) &&
          (s.statusPins == std::vector<uint16_t>{kSignetEffect}));
    for (const char* bad : {"status favorite ", "status favorite 4x", "status pin 65000", "status pin 99999999"})
        CHECK(ApplyPopupPress(s, bad, Nation::None) == PressResult::None);
    // Six at most: a seventh pin is refused.
    s.statusPins = {40, 33, 41, 42, 43, kSignetEffect};
    CHECK(press(kStatusPinPress, 116) == PressResult::Refused && s.statusPins.size() == kMostStatusPins);
}

TEST(the_status_timers_widget_shows_each_pin_at_its_rank_where_its_design_puts_it)
{
    Settings settings;
    BarState state;
    state.statuses = {{40, 1800.0}, {33, 90.0}};
    settings.Widget(BarWidget::StatusTimers).on            = true;
    settings.Widget(BarWidget::StatusTimers).statusDisplay = StatusDisplay::Name;
    settings.statusPins = {40, 33, kSignetEffect}; // ranks 1, 2 and 3
    // As new: 1, 3 and 5 on the first line, 2, 4 and 6 on the second.
    CHECK_EQ(DesignText(BarWidget::StatusTimers, settings.Widget(BarWidget::StatusTimers).design), "pin1 pin3 pin5 | pin2 pin4 pin6");
    const WidgetContent w = Content(BarWidget::StatusTimers, state, settings);
    CHECK(Joined(w.title) == "Protect 30m 00s  Signet 0s" && Joined(w.subtitle) == "Haste 1m 30s");
    // Pinned or not decides what shows, not the design's switches.
    settings.Widget(BarWidget::StatusTimers).design = DesignFrom(BarWidget::StatusTimers, "-pin1 -pin2 -pin3 | -pin4 -pin5 -pin6");
    CHECK(Joined(Content(BarWidget::StatusTimers, state, settings).title) == "Protect 30m 00s  Haste 1m 30s  Signet 0s");
    // None pinned: its name, dim.
    settings.statusPins.clear();
    CHECK(Content(BarWidget::StatusTimers, state, settings).title == ListOf(Text("Status Timers", Tone::Dim)));
}

TEST(a_widgets_clone_and_remove_presses_clone_it_or_take_the_copy_off_and_name_whose_settings_show_next)
{
    Settings s;
    const PanelWidget zone{BarWidget::Zone};
    const PopupContent first = PanelPopupContent(zone, BarState{}, s);
    CHECK(first.removePress.empty() && !first.clonePress.empty()); // the first copy is never taken off
    const std::optional<PanelWidget> copy = ApplyWidgetPress(s, first.clonePress);
    CHECK(copy && copy->kind == BarWidget::Zone && copy->copy == 1 && Placed(s.barSections, *copy) && s.Widget(*copy).on);
    const PopupContent second = PanelPopupContent(*copy, BarState{}, s);
    CHECK(ApplyWidgetPress(s, second.removePress) == zone && !Placed(s.barSections, *copy));
    CHECK(!ApplyWidgetPress(s, "widget remove 999") && !ApplyWidgetPress(s, "widget clone x") && !ApplyWidgetPress(s, "status pin 40"));
    // The game's help has no copies.
    CHECK(PanelPopupContent(BarWidget::GameHelp, BarState{}, s).clonePress.empty());
}

TEST(the_currency_popup_lists_your_favorites_then_every_currency_by_amount_or_by_name)
{
    // A grid's lines, its cells' texts a space apart.
    const auto gridLines = [](const PopupRow& grid) {
        std::vector<std::string> lines;
        for (size_t i = 0; i < grid.cells.size(); i += static_cast<size_t>(grid.columns))
        {
            std::string line;
            for (size_t c = i; c < i + static_cast<size_t>(grid.columns); ++c)
                if (const std::string text = Joined(grid.cells[c].text); !text.empty()) line += (line.empty() ? "" : " ") + text;
            lines.push_back(line);
        }
        return lines;
    };
    Settings settings;
    BarState state;
    state.gil    = 4751;
    state.nation = Nation::Bastok;
    PopupContent p = BarPopupContent(BarWidget::Currency, state, settings);
    CHECK(Joined(p.title) == "Currencies" && p.sections.size() == 2 && p.sections[0].heading == "Favorites");
    // By default gil and your nation's conquest points, each starred; a star's press takes it off.
    const PopupRow& top = p.sections[0].rows[0];
    CHECK((gridLines(top) == std::vector<std::string>{"Gil 4,751", "Conquest Points (Bastok) -"}));
    CHECK(top.cells[0].icon == Icon::Star && top.cells[0].press == "favorite gil" && top.cells[3].align == Align::Right);
    // Each currency's picture in a column of its own after its star: gil's, a nation's crest.
    CHECK(top.columns == 4 && top.cells[1].icon == Icon::Gil && top.cells[1].text.empty() && top.cells[5].picture == GamePicture::BastokCrest);
    // In a socket that scrolls, as All Currencies' are, their amounts at its right.
    CHECK(top.nested && top.scroll == "Favorites" && top.spread);
    const PopupSection& all = p.sections[1];
    CHECK(all.tabs.size() == 2 && all.tabs[0].name.text == "By Amount" && all.tabs[1].name.text == "Alphabetically");
    CHECK((Lines(all) == std::vector<std::string>{"Their amounts show once the server sends them."}));
    const int notes = *CurrencyOfKey("allied_notes"), sparks = *CurrencyOfKey("sparks");
    for (std::optional<uint32_t>& amount : state.currencies)
        amount = 0; // the server sent them all, most at none
    state.currencies[notes]  = 12000;
    state.currencies[sparks] = 350;
    p = BarPopupContent(BarWidget::Currency, state, settings);
    // Most first, those held alike by name; every currency is listed, unstarred unless a favorite.
    const PopupRow& byAmount = p.sections[1].tabs[0].rows[0];
    std::vector<std::string> lines = gridLines(byAmount);
    CHECK(lines.size() == static_cast<size_t>(kCurrencyCount) && byAmount.nested && !byAmount.scroll.empty());
    CHECK((std::vector<std::string>(lines.begin(), lines.begin() + 4) ==
           std::vector<std::string>{"Allied Notes 12,000", "Gil 4,751", "Sparks of Eminence 350", "Ancient Beastcoins 0"}));
    CHECK(byAmount.cells[0].icon == Icon::StarEmpty && byAmount.cells[4].icon == Icon::Star); // Gil's
    CHECK(byAmount.spread && byAmount.cells[5].icon == Icon::Gil);
    // A currency with no picture leaves its place in the column empty.
    CHECK(byAmount.cells[1] == PopupCell{});
    const auto row = std::ranges::find_if(byAmount.cells, [](const PopupCell& c) { return Joined(c.text) == "Beastmen's Seals"; });
    CHECK(row != byAmount.cells.end() && (row - 1)->item == 1126); // the Beastmen's Seal item, by item_basic.sql
    const std::map<std::string, std::string> picked{{"All Currencies", "Alphabetically"}};
    state.pickedTabs = &picked;
    p = BarPopupContent(BarWidget::Currency, state, settings);
    CHECK(p.sections[1].tab == 1);
    lines = gridLines(p.sections[1].tabs[1].rows[0]);
    CHECK(lines[0] == "Allied Notes 12,000" && lines[1] == "Ancient Beastcoins 0");
}

TEST(a_currencys_star_adds_it_to_the_favorites_and_takes_it_off)
{
    Settings s;
    CHECK(ApplyPopupPress(s, "favorite sparks", Nation::Bastok) == PressResult::Changed);
    CHECK((s.currencyFavorites == std::vector<std::string>{"gil", "cp_bastok", "sparks"}));
    CHECK(ApplyPopupPress(s, "favorite cp_bastok", Nation::Bastok) == PressResult::Changed);
    CHECK((s.currencyFavorites == std::vector<std::string>{"gil", "sparks"}));
    CHECK(ApplyPopupPress(s, "favorite nothing", Nation::Bastok) == PressResult::None &&
          ApplyPopupPress(s, "something else", Nation::Bastok) == PressResult::None);
    CHECK((s.currencyFavorites == std::vector<std::string>{"gil", "sparks"}));
    // Before your nation is known, its points stay yours, whichever nation that turns out to be.
    Settings early;
    CHECK(ApplyPopupPress(early, "favorite sparks", Nation::None) == PressResult::Changed);
    CHECK((early.currencyFavorites == std::vector<std::string>{"gil", kYourConquestPoints, "sparks"}));
}

TEST(the_currency_widget_shows_two_currencies_and_your_nations_points_without_its_name)
{
    Settings settings;
    BarState state;
    state.gil    = 4751;
    state.nation = Nation::Bastok;
    WidgetOptions& o = settings.Widget(BarWidget::Currency);
    CHECK(o.currency == kGilCurrency && o.secondCurrency == kYourConquestCurrency);
    o.design        = DesignFrom(BarWidget::Currency, "amount1 name1 | amount2 name2");
    WidgetContent w = Content(BarWidget::Currency, state, settings);
    CHECK(Joined(w.title) == "4,751  Gil" && Joined(w.subtitle) == "-  Conquest Points"); // not sent yet
    for (std::optional<uint32_t>& amount : state.currencies)
        amount = 0; // the server sent them all, most at none
    state.currencies[*CurrencyOfKey("cp_bastok")] = 350;
    CHECK(Joined(Content(BarWidget::Currency, state, settings).subtitle) == "350  Conquest Points");
    o.secondCurrency = *CurrencyOfKey("cp_sandoria");
    CHECK(Joined(Content(BarWidget::Currency, state, settings).subtitle) == "0  Conquest Points (San d'Oria)");
    // Gil's picture leads whatever the currencies are.
    const int notes = *CurrencyOfKey("allied_notes");
    o.currency      = notes;
    state.currencies[notes] = 12000;
    w = Content(BarWidget::Currency, state, settings);
    CHECK(LeadsWithGil(w) && Joined(w.title) == "12,000  Allied Notes" && w.hasPopup);
    // Each currency's icon is the game's own picture of it, where it has one.
    o.design         = DesignFrom(BarWidget::Currency, "icon1 | icon2");
    o.currency       = *CurrencyOfKey("beastmen_seals");
    o.secondCurrency = kYourConquestCurrency;
    w                = Content(BarWidget::Currency, state, settings);
    CHECK(w.title.size() == 1 && w.title[0].item == 1126); // the Beastmen's Seal item, by item_basic.sql
    CHECK(w.subtitle.size() == 1 && w.subtitle[0].picture == GamePicture::BastokCrest);
    // Gil's is the Coin Icon's picture, on its own line wherever it is, and apart from the Coin Icon: either, both or none.
    o.currency = kGilCurrency;
    for (const IconSide side : {IconSide::Left, IconSide::Hide})
    {
        o.iconSide = side;
        w          = Content(BarWidget::Currency, state, settings);
        CHECK(LeadsWithGil(w) == (side == IconSide::Left) && w.title.size() == 1 && w.title[0].coin);
    }
    o.currency = notes;
    w          = Content(BarWidget::Currency, state, settings);
    CHECK(!LeadsWithGil(w) && std::ranges::none_of(w.title, [](const Span& s) { return s.item != 0; }));
}

TEST(the_currency_choices_put_your_nations_points_after_gil_and_before_the_others)
{
    const int sandoria = *CurrencyOfKey("cp_sandoria"), bastok = *CurrencyOfKey("cp_bastok"),
              windurst = *CurrencyOfKey("cp_windurst");
    const std::vector<int> bastokan = CurrencyChoices(Nation::Bastok);
    CHECK(bastokan.size() == static_cast<size_t>(kCurrencyCount));
    CHECK((std::vector<int>(bastokan.begin(), bastokan.begin() + 4) ==
           std::vector<int>{kGilCurrency, kYourConquestCurrency, sandoria, windurst}));
    CHECK(std::ranges::find(bastokan, bastok) == bastokan.end());
    const std::vector<int> unknown = CurrencyChoices(Nation::None);
    CHECK(unknown.size() == static_cast<size_t>(kCurrencyCount) + 1 && unknown[1] == kYourConquestCurrency &&
          unknown[2] == sandoria);
    CHECK(CurrencyName(kYourConquestCurrency, Nation::Bastok) == "Conquest Points");
    CHECK(CurrencyName(bastok, Nation::Bastok) == "Conquest Points");
    CHECK(CurrencyName(bastok, Nation::Windurst) == "Conquest Points (Bastok)");
}

TEST(the_target_says_how_far_it_is_after_its_name_unless_its_distance_is_off)
{
    Settings settings;
    const ActorInfo goblin = Goblin();
    BarState state;
    state.target         = &goblin;
    state.targetDistance = 12.34f;
    const std::string name = Joined(Content(BarWidget::Target, state, Settings{}).title);
    WidgetContent w = Content(BarWidget::Target, state, settings);
    CHECK(w.title.back().text == "  (12.3)" && w.title.back().tone == Tone::Dim);
    // It keeps the room of the farthest it reads, 99.9.
    CHECK((w.title.back().room == std::vector<std::string>{"(99.9)"}));
    settings.Widget(BarWidget::Target).design = DesignFrom(BarWidget::Target, "aggro name -distance | level player");
    w = Content(BarWidget::Target, state, settings);
    CHECK(Joined(w.title) + "  (12.3)" == name);
    // Without a target, or a distance, there is nothing to measure.
    settings.Widget(BarWidget::Target).design = DefaultDesign(BarWidget::Target);
    state.targetDistance.reset();
    CHECK(Joined(Content(BarWidget::Target, state, settings).title) + "  (12.3)" == name);
    state.target         = nullptr;
    state.targetDistance = 5.0f;
    CHECK(Joined(Content(BarWidget::Target, state, settings).title) == "No Target");
}

TEST(each_widget_can_drop_its_second_line_or_its_icons)
{
    Settings settings;
    BarState state;
    state.unixMs  = kVanaEpochUnixMs;
    state.weather = Weather::DustStorm;
    state.gil     = 100;
    settings.Widget(BarWidget::Weather).design = DesignFrom(BarWidget::Weather, "weather | -day -time");
    WidgetContent w = Content(BarWidget::Weather, state, settings);
    CHECK(w.subtitle.empty() && !w.icons.empty() && Joined(w.title) == "Dust Storm");
    settings.Widget(BarWidget::Weather).iconSide = IconSide::Hide;
    CHECK(Content(BarWidget::Weather, state, settings).icons.empty());
    settings.Widget(BarWidget::Currency).iconSide = IconSide::Hide;
    w = Content(BarWidget::Currency, state, settings);
    CHECK(!LeadsWithGil(w) && Joined(w.title) == "100");
}

TEST(the_job_widgets_design_places_its_exp_bar_anywhere)
{
    Settings settings;
    BarState state;
    JobSample s;
    s.mainJob = 1, s.mainLevel = 30, s.subJob = 13, s.subLevel = 15;
    s.exp = 3800, s.expNeeded = 7600;
    state.job = JobReport{s, std::nullopt};
    WidgetOptions& o = settings.Widget(BarWidget::Job);
    o.design        = DesignFrom(BarWidget::Job, "jobs bar | tonext");
    WidgetContent w = Content(BarWidget::Job, state, settings);
    CHECK(w.title.back().bar->done == 0.5f && Joined(w.subtitle) == "3,800 til next level");
    o.design = DesignFrom(BarWidget::Job, "jobs | tonext -bar");
    w        = Content(BarWidget::Job, state, settings);
    CHECK(std::ranges::none_of(w.subtitle, [](const Span& span) { return span.bar.has_value(); }));
    s.subJob  = 0, s.subLevel = 0; // no support job yet
    state.job = JobReport{s, std::nullopt};
    CHECK(Joined(Content(BarWidget::Job, state, settings).title) == "WAR 30");
}

TEST(the_job_widgets_design_can_put_its_exp_first_or_show_it_alone)
{
    Settings settings;
    BarState state;
    JobSample s;
    s.mainJob = 1, s.mainLevel = 30, s.subJob = 13, s.subLevel = 15;
    s.exp = 4140, s.expNeeded = 7600;
    state.job        = JobReport{s, std::nullopt};
    WidgetOptions& o = settings.Widget(BarWidget::Job);
    o.design         = DesignFrom(BarWidget::Job, "tonext | jobs -bar");
    WidgetContent w  = Content(BarWidget::Job, state, settings);
    // Each text a run of its own alignment, at the left by default.
    const auto left = [](Span span) {
        span.align = Align::Left;
        return ListOf(std::move(span));
    };
    CHECK(w.title == left(Text("3,460 til next level"))); // the larger line, in the title's own tone
    CHECK(w.subtitle == left(Text("WAR 30 / NIN 15", Tone::Dim)));
    CHECK(w.figures && w.hasPopup);
    o.design = DesignFrom(BarWidget::Job, "-jobs tonext | -bar");
    w        = Content(BarWidget::Job, state, settings);
    CHECK(Joined(w.title) == "3,460 til next level" && w.subtitle.empty());
    s.mainLevel = 75, s.subLevel = 37, s.merits = 7, s.meritsMax = 10, s.limitPoints = 4210;
    state.job = JobReport{s, std::nullopt};
    w         = Content(BarWidget::Job, state, settings);
    CHECK(w.title == left(Text("7 merits")) && w.subtitle.empty());
    state.job.reset(); // the title screen
    w = Content(BarWidget::Job, state, settings);
    CHECK(Joined(w.title) == "-" && w.subtitle.empty());
}

// 2026-10-09 06:30:42 UTC is Friday 15:30:42 in Japan and still Thursday 23:30:42 on the US Pacific coast, on daylight
// time.
TEST(the_clock_reads_in_the_zone_picked_for_it_with_that_zones_date_in_its_popup)
{
    Settings settings;
    BarState state;
    state.unixMs    = 1791527400000 + 42000;
    state.earthYear = 2026, state.earthMonth = 10, state.earthDay = 9; // the computer's own: 08:30:42 in Berlin
    state.earthHour = 8, state.earthMinute = 30, state.earthSecond = 42;
    CHECK(!settings.Widget(BarWidget::EarthTime).clockZone.has_value());
    CHECK(Joined(Content(BarWidget::EarthTime, state, settings).title) == "08:30");
    CHECK(Joined(BarPopupContent(BarWidget::EarthTime, state, settings).title) == "October 9th, 2026 - 08:30");
    settings.Widget(BarWidget::EarthTime).clockZone = ClockZone::UsPacific;
    CHECK(Joined(Content(BarWidget::EarthTime, state, settings).title) == "PDT 23:30");
    CHECK(Joined(BarPopupContent(BarWidget::EarthTime, state, settings).title) == "October 8th, 2026 - 23:30");
    settings.Widget(BarWidget::EarthTime).clockZone    = ClockZone::Japan;
    settings.Widget(BarWidget::EarthTime).clock24      = false;
    settings.Widget(BarWidget::EarthTime).clockSeconds = true;
    CHECK(Joined(Content(BarWidget::EarthTime, state, settings).title) == "JST 3:30:42 PM");
    CHECK(Joined(BarPopupContent(BarWidget::EarthTime, state, settings).title) == "October 9th, 2026 - 3:30:42pm");
}

TEST(a_clock_in_a_picked_zone_is_read_from_the_time_alone_and_the_computers_from_its_own)
{
    Settings settings;
    for (WidgetOptions& each : settings.widgets)
        each.on = false;
    settings.Widget(BarWidget::EarthTime).on = true;
    const BarNeeds own = NeedsOf(settings, {});
    CHECK(own.localTime && !own.unixTime);
    settings.Widget(BarWidget::EarthTime).clockZone   = ClockZone::Utc;
    const BarNeeds zoned = NeedsOf(settings, {});
    CHECK(zoned.unixTime && !zoned.localTime);
    settings.Widget(BarWidget::EarthTime).clockZone.reset();
    settings.Widget(BarWidget::EarthTime).design = DesignFrom(BarWidget::EarthTime, "time | server"); // the server's line goes by the time, the computer's by its own
    const BarNeeds stacked   = NeedsOf(settings, {});
    CHECK(stacked.unixTime && stacked.localTime);
    settings.Widget(BarWidget::EarthTime).design = DesignFrom(BarWidget::EarthTime, "time | -server");
    const BarNeeds opened    = NeedsOf(settings, {BarWidget::EarthTime}); // the popup counts down to the resets
    CHECK(opened.unixTime && opened.localTime);
}

TEST(the_clocks_secondary_line_reads_in_its_own_zone_and_vanadiels_time_is_a_part_too)
{
    Settings settings;
    BarState state;
    state.unixMs    = 1791527400000 + 42000; // Friday 15:30:42 JST
    state.earthYear = 2026, state.earthMonth = 10, state.earthDay = 9; // the computer's own: 08:30:42 in Berlin
    state.earthHour = 8, state.earthMinute = 30, state.earthSecond = 42;
    WidgetOptions& o = settings.Widget(BarWidget::EarthTime);
    CHECK(o.clockSecondZone == ClockZone::Japan);
    o.design        = DesignFrom(BarWidget::EarthTime, "time | server");
    o.clockSecondZone = ClockZone::UsPacific;
    CHECK(Joined(Content(BarWidget::EarthTime, state, settings).subtitle) == "PDT 23:30");
    o.clockSecondZone.reset(); // Automatic: the computer's, named
    CHECK(Joined(Content(BarWidget::EarthTime, state, settings).subtitle) == "CEST 08:30");
    o.design = DesignFrom(BarWidget::EarthTime, "time vana | -server");
    const VanaTime t       = VanaAt(state.unixMs);
    const std::string both = Joined(Content(BarWidget::EarthTime, state, settings).title);
    CHECK(both.starts_with("08:30") && both.ends_with(" " + Printed("%02d", t.hour) + ":" + Printed("%02d", t.minute)));
    for (WidgetOptions& each : settings.widgets)
        each.on = false;
    o.on = true;
    o.design = DesignFrom(BarWidget::EarthTime, "-time vana | -server");
    CHECK(NeedsOf(settings, {}).unixTime);
}

TEST(the_clocks_read_on_a_twelve_hour_clock_when_asked)
{
    CHECK(ClockText(21, 7, true) == "21:07" && ClockText(21, 7, false) == "9:07 PM");
    CHECK(ClockText(0, 5, false) == "12:05 AM" && ClockText(12, 0, false) == "12:00 PM");
    Settings settings;
    settings.Widget(BarWidget::EarthTime).clock24 = false;
    BarState state;
    state.earthHour = 9, state.earthMinute = 30;
    CHECK(Joined(Content(BarWidget::EarthTime, state, settings).title) == "9:30 AM");
}

TEST(the_clocks_add_seconds_when_asked)
{
    CHECK(ClockText(21, 7, true, 5) == "21:07:05" && ClockText(22, 13, false, 24) == "10:13:24 PM");
    Settings settings;
    BarState state;
    state.earthHour = 9, state.earthMinute = 30, state.earthSecond = 7;
    CHECK(Joined(Content(BarWidget::EarthTime, state, settings).title) == "09:30");
    settings.Widget(BarWidget::EarthTime).clockSeconds = true;
    CHECK(Joined(Content(BarWidget::EarthTime, state, settings).title) == "09:30:07");
}

TEST(the_clocks_local_date_says_the_computers_date_as_date_format_writes_it)
{
    const CalendarDay day{2026, 11, 7};
    CHECK_EQ(DateText(DateFormat::YearMonthDay, day), "2026/11/07");
    CHECK_EQ(DateText(DateFormat::MonthDayYear, day), "11/07/2026");
    CHECK_EQ(DateText(DateFormat::DayMonthYear, day), "07/11/2026");
    CHECK_EQ(DateText(DateFormat::MonthDay, day), "November 7th");
    CHECK_EQ(DateText(DateFormat::Iso, day), "2026-11-07");
    CHECK_EQ(DateText(DateFormat::MonthDayCommaYear, day), "November 7th, 2026");
    CHECK_EQ(DateText(DateFormat::ShortMonthDay, day), "Nov 7");
    // English ordinals, the teens included.
    const std::pair<int, const char*> ordinals[] = {{1, "January 1st"}, {2, "January 2nd"}, {3, "January 3rd"},
        {4, "January 4th"}, {11, "January 11th"}, {12, "January 12th"}, {13, "January 13th"}, {21, "January 21st"},
        {22, "January 22nd"}, {23, "January 23rd"}, {30, "January 30th"}, {31, "January 31st"}};
    for (const auto& [d, said] : ordinals)
        CHECK_EQ(DateText(DateFormat::MonthDay, CalendarDay{2026, 1, d}), said);
    Settings settings;
    BarState state;
    state.unixMs    = 1791527400000 + 42000; // still Thursday the 8th on the US Pacific coast
    state.earthYear = 2026, state.earthMonth = 10, state.earthDay = 9;
    state.earthHour = 8, state.earthMinute = 30;
    WidgetOptions& o = settings.Widget(BarWidget::EarthTime);
    CHECK(o.dateFormat == DateFormat::MonthDayYear);
    CHECK(Joined(Content(BarWidget::EarthTime, state, settings).subtitle).empty()); // off until asked
    o.design = DesignFrom(BarWidget::EarthTime, "time | date");
    CHECK(Joined(Content(BarWidget::EarthTime, state, settings).subtitle) == "10/09/2026");
    o.dateFormat = DateFormat::MonthDay;
    o.clockZone  = ClockZone::UsPacific; // Local Timezone moves the time, never the computer's date
    CHECK(Joined(Content(BarWidget::EarthTime, state, settings).subtitle) == "October 9th");
    // It needs the computer's date even while the time is another zone's.
    for (WidgetOptions& each : settings.widgets)
        each.on = false;
    o.on = true;
    CHECK(NeedsOf(settings, {}).localTime);
    o.design = DesignFrom(BarWidget::EarthTime, "time | -date");
    CHECK(!NeedsOf(settings, {}).localTime);
}

TEST(the_clocks_popup_is_headed_by_the_date_in_words_and_the_time_as_24_hour_and_seconds_say)
{
    CHECK_EQ(DateTimeTitle(CalendarDay{1999, 9, 1}, 12, 0, -1, false), "September 1st, 1999 - 12:00pm");
    CHECK_EQ(DateTimeTitle(CalendarDay{1999, 9, 1}, 0, 5, -1, false), "September 1st, 1999 - 12:05am");
    CHECK_EQ(DateTimeTitle(CalendarDay{1999, 9, 1}, 13, 4, 9, false), "September 1st, 1999 - 1:04:09pm");
    CHECK_EQ(DateTimeTitle(CalendarDay{1999, 9, 1}, 13, 4, 9, true), "September 1st, 1999 - 13:04:09");
    Settings settings;
    BarState state;
    state.unixMs    = 1791527400000 + 42000; // Thursday 23:30:42 on the US Pacific coast
    state.earthYear = 2026, state.earthMonth = 10, state.earthDay = 9;
    state.earthHour = 8, state.earthMinute = 30, state.earthSecond = 42;
    WidgetOptions& o = settings.Widget(BarWidget::EarthTime);
    o.clock24 = true, o.clockSeconds = false;
    CHECK(Joined(BarPopupContent(BarWidget::EarthTime, state, settings).title) == "October 9th, 2026 - 08:30");
    o.clock24 = false, o.clockSeconds = true;
    CHECK(Joined(BarPopupContent(BarWidget::EarthTime, state, settings).title) == "October 9th, 2026 - 8:30:42am");
    o.clockZone = ClockZone::UsPacific; // the Local Time's zone, date and time
    CHECK(Joined(BarPopupContent(BarWidget::EarthTime, state, settings).title) == "October 8th, 2026 - 11:30:42pm");
}

TEST(the_performance_widget_has_no_ping_to_offer)
{
    const auto parts = PartsOf(BarWidget::Performance);
    CHECK(std::ranges::none_of(parts, [](WidgetPart part) { return std::string(PartFileName(part)) == "ping"; }));
}

TEST(a_mobs_popup_lists_what_it_drops_at_its_treasure_hunter_0_chance_and_what_can_be_stolen)
{
    Settings settings;
    ActorInfo billy = Goblin();
    billy.serverId  = 17191196; // Bigmouth Billy: iron and mythril ore very common, gold ore to steal
    billy.mob       = MobById(billy.serverId);
    BarState state;
    state.target   = &billy;
    state.itemName = [](uint16_t item) -> const char* {
        switch (item)
        {
        case 643: return "Iron Ore";
        case 644: return "Mythril Ore";
        case 737: return "Gold Ore";
        default: return nullptr; // an item the game has no name for is left out
        }
    };
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    const PopupSection* drops = Section(p, "Drops");
    CHECK(drops != nullptr);
    if (drops != nullptr)
    {
        const std::vector<std::string> lines = Lines(*drops);
        CHECK(std::ranges::find(lines, "Iron Ore 24%") != lines.end());
        CHECK(std::ranges::find(lines, "Mythril Ore 24%") != lines.end());
    }
    const PopupSection* steal = Section(p, "Steal");
    CHECK(steal != nullptr && Lines(*steal) == (std::vector<std::string>{"Gold Ore"}));
    state.itemName = nullptr; // no names: no lists
    CHECK(Section(BarPopupContent(BarWidget::Target, state, settings), "Drops") == nullptr);
}

namespace
{
    ActorInfo NpcTarget(uint32_t serverId, const char* name)
    {
        ActorInfo npc;
        npc.kind     = EntityKind::Npc;
        npc.serverId = serverId;
        npc.index    = serverId & 0xFFF;
        std::snprintf(npc.name, sizeof(npc.name), "%s", name);
        return npc;
    }

    const char* NpcItemName(uint16_t item)
    {
        switch (item)
        {
        case 4373: return "Woozyshroom";
        case 4374: return "Sleepshroom";
        case 4375: return "Danceshroom";
        case 1823: return "Shrimp Lantern";
        case 4389: return "San d'Orian Carrot";
        case 4112: return "Potion";
        case 4128: return "Ether";
        case 4148: return "Antidote";
        case 4150: return "Eye Drops";
        case 4151: return "Echo Drops";
        case 17389: return "Bamboo Fishing Rod";
        case 17390: return "Yew Fishing Rod";
        case 17391: return "Willow Fishing Rod";
        case 17395: return "Lugworm";
        case 17396: return "Little Worm";
        case 5066: return "Scroll of Lightning Threnody";
        case 5068: return "Scroll of Light Threnody";
        default: return nullptr; // left out, as a drop the game has no name for is
        }
    }

    // A section's lines; none when the popup has no such section.
    std::vector<std::string> SectionLines(const PopupContent& popup, const std::string& heading)
    {
        const PopupSection* section = Section(popup, heading);
        return section != nullptr ? Lines(*section) : std::vector<std::string>{"(no " + heading + ")"};
    }

    std::vector<std::string> Headings(const PopupContent& popup)
    {
        std::vector<std::string> headings;
        for (const PopupSection& s : popup.sections)
            headings.push_back(s.heading);
        return headings;
    }
}

TEST(a_qms_popup_says_what_to_trade_it_and_what_that_pops)
{
    // Carpenters' Landing's qm_mycophile (its script): a Woozyshroom, Sleepshroom or Danceshroom pops Mycophile.
    Settings settings;
    const ActorInfo qm = NpcTarget(16785729, "???");
    BarState state;
    state.target   = &qm;
    state.itemName = NpcItemName;
    CHECK(Content(BarWidget::Target, state, settings).hasPopup);
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Joined(p.title) == "???");
    CHECK(Headings(p) == (std::vector<std::string>{"", "Click", "Trade", "Pops"}));
    CHECK(SectionLines(p, "") == (std::vector<std::string>{"ID 141"}));
    // Its hint in the game's words, and the three mushrooms as one trade, as the probe saw on the server.
    CHECK(SectionLines(p, "Click") ==
          (std::vector<std::string>{"There is a rotten mushroom here. There are 3", "openings in its cap."}));
    CHECK(SectionLines(p, "Trade") ==
          (std::vector<std::string>{"Woozyshroom + Sleepshroom + Danceshroom", "\xE2\x86\x92 Pops Mycophile"}));
    CHECK(SectionLines(p, "Pops") == (std::vector<std::string>{"Mycophile"}));
}

TEST(a_qm_that_pops_two_of_one_nm_names_it_once)
{
    // Al'Taieu's qm_jailer_of_prudence spawns both Jailers of Prudence, 16912846 and 16912847.
    Settings settings;
    const ActorInfo qm = NpcTarget(16912886, "???");
    BarState state;
    state.target = &qm;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Pops") ==
          (std::vector<std::string>{"Jailer of Prudence"}));
}

TEST(a_spot_whose_script_works_out_what_it_pops_says_so)
{
    // Uleguerand Range's Rabbit Footprint takes a San d'Orian Carrot and pops a coney the moon picks.
    Settings settings;
    const ActorInfo footprint = NpcTarget(16798100, "Rabbit Footprint");
    BarState state;
    state.target   = &footprint;
    state.itemName = NpcItemName;
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(SectionLines(p, "Trade") == (std::vector<std::string>{"San d'Orian Carrot"}));
    CHECK(SectionLines(p, "Pops") == (std::vector<std::string>{"Worked out by its script"}));
    if (const PopupSection* pops = Section(p, "Pops")) CHECK(pops->rows[0].cells[0].text[0].tone == Tone::Dim);
}

TEST(a_qm_shows_the_nm_the_probe_saw_it_pop_and_what_it_says)
{
    // Bibiki Bay's qm_shen, as the probe saw it on the server: its click's shrimp shells line, and a Shrimp Lantern's
    // skimming line and Shen, the words from its IDs.lua wrapped at 44 characters, the lantern in its <item>.
    Settings settings;
    const ActorInfo qm = NpcTarget(16793970, "???");
    BarState state;
    state.target   = &qm;
    state.itemName = NpcItemName;
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(SectionLines(p, "Click") ==
          (std::vector<std::string>{"Innumerable shrimp shells are floating in", "the water here."}));
    CHECK(SectionLines(p, "Trade") ==
          (std::vector<std::string>{"Shrimp Lantern", "The Shrimp Lantern begins skimming quietly",
              "across the surface of the water. ...What's", "this!? An enormous creature is chasing after",
              "it from below!!!", "\xE2\x86\x92 Pops Shen"}));
    CHECK(SectionLines(p, "Pops") == (std::vector<std::string>{"Shen"}));
}

TEST(a_hidden_qm_says_when_it_appears_and_what_a_readied_one_does)
{
    // Carpenters' Landing's Hercules Beetle tree: its zone shows it at 22:00 and hides it at 07:00, and once honeyed
    // and ready it pops the Hercules Beetle (globals.lua and qm_hercules_beetle.lua).
    Settings settings;
    const ActorInfo tree = NpcTarget(16785730, "???");
    BarState state;
    state.target         = &tree;
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(SectionLines(p, "") == (std::vector<std::string>{"ID 142", "Appears at 22:00, gone at 07:00"}));
    CHECK(SectionLines(p, "Click") == (std::vector<std::string>{"Once readied", "\xE2\x86\x92 Pops Hercules Beetle"}));
    if (const PopupSection* click = Section(p, "Click")) CHECK(click->rows[0].cells[0].text[0].tone == Tone::Dim);
}

TEST(a_quest_steps_acts_name_the_quest_its_step_and_what_it_needs)
{
    // Batallia Downs' qm_chocobotale: a click finds nothing; at step 4 of A Chocobo's Tale it calls Badshah's pack, and
    // once the NM is down it gives the Silver Comet's Collar (quests/jeuno/A_Chocobos_Tale.lua).
    Settings settings;
    const ActorInfo qm = NpcTarget(17207852, "???");
    BarState state;
    state.target = &qm;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Click") ==
          (std::vector<std::string>{"You find nothing.", "In A Chocobos Tale, step 4", "You are being attacked!",
              "\xE2\x86\x92 Pops Badshah", "In A Chocobos Tale, step 4, after its NM falls",
              "\xE2\x86\x92 Gives key item Silver Comets Collar"}));
}

TEST(an_npcs_popup_names_the_key_items_it_gives_and_checks_for)
{
    // Carpenters' Landing's qm_para: with the Antique Amulet it pops Para, and it gives the Cathedral Medallion, for
    // the quest Elderly Pursuits (quests/otherAreas).
    Settings settings;
    const ActorInfo qm = NpcTarget(16785781, "???");
    BarState state;
    state.target         = &qm;
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Headings(p) == (std::vector<std::string>{"", "Click", "Pops", "Key Items", "Checks for", "Quests"}));
    CHECK(SectionLines(p, "Click") ==
          (std::vector<std::string>{"There is nothing out of the ordinary here.", "In Elderly Pursuits, step 1",
              "You are overwhelmed by the putrid stench of", "decay!", "\xE2\x86\x92 Pops Para",
              "In Elderly Pursuits, step 2, holding its key items", "You polish the Antique Amulet with the",
              "glowing mushroom spores!", "Obtained key item: Cathedral Medallion.",
              "\xE2\x86\x92 Gives key item Cathedral Medallion"}));
    CHECK(SectionLines(p, "Quests") == (std::vector<std::string>{"Elderly Pursuits Other Areas"}));
    CHECK(SectionLines(p, "Pops") == (std::vector<std::string>{"Para"}));
    CHECK(SectionLines(p, "Key Items") == (std::vector<std::string>{"Cathedral Medallion"}));
    CHECK(SectionLines(p, "Checks for") == (std::vector<std::string>{"Antique Amulet"}));
}

TEST(a_trade_of_gil_reads_gil_and_an_item_without_a_name_is_left_out)
{
    // Whitegate's Cacaroon takes an Imperial Bronze Piece (2184, which NpcItemName cannot name) and gil.
    Settings settings;
    const ActorInfo cacaroon = NpcTarget(16982252, "Cacaroon");
    BarState state;
    state.target   = &cacaroon;
    state.itemName = NpcItemName;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Trade") == (std::vector<std::string>{"Gil"}));
}

TEST(an_npc_the_data_has_nothing_of_gets_only_its_name)
{
    Settings settings;
    const ActorInfo blank = NpcTarget(16781329, "Door");
    BarState state;
    state.target = &blank;
    const WidgetContent w = Content(BarWidget::Target, state, settings);
    CHECK(Joined(w.title) == "Door" && !w.hasPopup);
    CHECK(BarPopupContent(BarWidget::Target, state, settings).sections.empty());
}

TEST(a_shops_popup_lists_its_stock_with_prices)
{
    // Maera's stock, aboard the ship to Selbina (her script), at prices fame does not move.
    Settings settings;
    const ActorInfo maera = NpcTarget(17678362, "Maera");
    BarState state;
    state.target   = &maera;
    state.itemName = NpcItemName;
    CHECK(Content(BarWidget::Target, state, settings).hasPopup);
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Headings(p) == (std::vector<std::string>{"", "Shop"}));
    CHECK(SectionLines(p, "Shop") == (std::vector<std::string>{"Potion 910 gil", "Ether 4,832 gil", "Antidote 316 gil",
                                         "Eye Drops 2,595 gil", "Echo Drops 800 gil"}));
}

TEST(a_nation_shop_puts_what_conquest_holds_back_under_its_place_and_says_fame_moves_its_prices)
{
    // Southern San d'Oria's Lusiane, by xi.shop.nation: rank 3 always, 2 while San d'Oria is 1st or 2nd, 1 while it is
    // 1st and only to its citizens.
    Settings settings;
    const ActorInfo lusiane = NpcTarget(17719350, "Lusiane");
    BarState state;
    state.target   = &lusiane;
    state.itemName = NpcItemName;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Shop") ==
          (std::vector<std::string>{"Little Worm 4 gil", "Willow Fishing Rod 72 gil", "Scroll of Light Threnody 120 gil",
              "Scroll of Lightning Threnody 1,376 gil", "When its nation is 1st or 2nd in conquest", "Lugworm 12 gil",
              "Yew Fishing Rod 236 gil", "When its nation is 1st in conquest, to its citizens",
              "Bamboo Fishing Rod 540 gil", "Prices before fame, which moves them up to 10% either way"}));
    // The items in the normal tone, so the dim lines naming a place read as headings over them.
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    if (const PopupSection* shop = Section(p, "Shop"))
        CHECK(shop->rows[0].cells[0].text[0].tone == Tone::Normal && shop->rows[4].cells[0].text[0].tone == Tone::Dim);
}

TEST(a_shop_whose_stock_its_script_works_out_says_so)
{
    Settings settings;
    const ActorInfo dabih = NpcTarget(17772597, "Dabih Jajalioh"); // Ru'Lude Gardens: the stock follows the season
    BarState state;
    state.target   = &dabih;
    state.itemName = NpcItemName;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Shop") ==
          (std::vector<std::string>{"Worked out by its script"}));
}

TEST(an_npcs_popup_lists_its_quests_and_missions_with_their_logs)
{
    // The Gate of the Gods (quests/jeuno/Apocalypse_Nigh, missions/cop/8_1 and 8_2).
    Settings settings;
    const ActorInfo gate = NpcTarget(16916828, "Gate of the Gods");
    BarState state;
    state.target = &gate;
    CHECK(Content(BarWidget::Target, state, settings).hasPopup);
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Headings(p) == (std::vector<std::string>{"", "Quests", "Missions"}));
    CHECK(SectionLines(p, "Quests") == (std::vector<std::string>{"Apocalypse Nigh Jeuno"}));
    CHECK(SectionLines(p, "Missions") == (std::vector<std::string>{"Garden of Antiquity CoP", "A Fate Decided CoP"}));
    if (const PopupSection* quests = Section(p, "Quests"))
        CHECK(quests->rows[0].kind == RowKind::Pair && quests->rows[0].cells[0].text[0].tone == Tone::Normal &&
              quests->rows[0].cells[1].text[0].tone == Tone::Dim);
}

TEST(battlefields_and_hidden_quests_have_sections_and_what_live_has_off_is_left_out)
{
    Settings settings;
    BarState state;
    const ActorInfo circle = NpcTarget(16818249, "Memento Circle"); // battlefields/The_Shrouded_Maw
    state.target           = &circle;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Battlefields") ==
          (std::vector<std::string>{"Darkness Named", "Test Your Mite", "Waking Dreams"}));
    const ActorInfo luminant = NpcTarget(16806290, "Luminant"); // quests/hiddenQuests/Mimeo_Stone
    state.target             = &luminant;
    CHECK(SectionLines(BarPopupContent(BarWidget::Target, state, settings), "Quests") ==
          (std::vector<std::string>{"Mimeo Stone Hidden"}));
    // Misareaux Coast's Dilapidated Gate: three CoP missions, the three mobs of one's fight, and a trust quest live
    // has off.
    const ActorInfo gate = NpcTarget(16879936, "Dilapidated Gate");
    state.target         = &gate;
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(Headings(p) == (std::vector<std::string>{"", "Pops", "Missions"}));
    CHECK(SectionLines(p, "Missions").size() == 3);
    const ActorInfo retriever = NpcTarget(17105705, "Mystic Retriever"); // only in an RoV mission
    state.target              = &retriever;
    CHECK(!Content(BarWidget::Target, state, settings).hasPopup);
}

TEST(what_an_npc_is_for_shows_as_tags_under_its_id)
{
    // Oldton Movalpolos' Twinkbrix takes a trade and teleports to the Mineshaft (its script).
    Settings settings;
    const ActorInfo twinkbrix = NpcTarget(16822535, "Twinkbrix");
    BarState state;
    state.target         = &twinkbrix;
    const PopupContent p = BarPopupContent(BarWidget::Target, state, settings);
    CHECK(!p.sections.empty() && p.sections[0].heading.empty());
    if (p.sections.empty()) return;
    const std::vector<PopupRow>& rows = p.sections[0].rows;
    CHECK(rows.size() == 2 && rows.back().kind == RowKind::Tags);
    if (rows.size() != 2) return;
    CHECK(Joined(rows[1].cells[0].text) == "Teleports to Mineshaft" && rows[1].cells[0].text[0].tone == Tone::Tag);
}

TEST(each_copy_of_a_widget_shows_by_its_own_settings)
{
    Settings settings;
    BarState state;
    state.gil = 1234;
    state.currencies[3] = 50;
    const std::optional<PanelWidget> copy = CloneWidget(settings, BarWidget::Currency);
    CHECK(copy.has_value());
    settings.Widget(*copy).currency = 3;
    const WidgetContent first = Content(BarWidget::Currency, state, settings), second = Content(*copy, state, settings);
    CHECK(LeadsWithGil(first) && LeadsWithGil(second) && second.title.front().text == "50");
    // A copy not on the panel shows nothing, whatever it was set to.
    RemoveWidgetCopy(settings, *copy);
    settings.Widget(*copy).on = true;
    CHECK(!Content(*copy, state, settings).shown);
}

TEST(what_the_plugin_gathers_follows_every_copy)
{
    Settings settings;
    for (WidgetOptions& each : settings.widgets)
        each.on = false;
    const std::optional<PanelWidget> clock = CloneWidget(settings, BarWidget::EarthTime);
    CHECK(clock.has_value());
    settings.Widget(*clock).clockZone = ClockZone::Utc;
    const BarNeeds needs = NeedsOf(settings, {});
    CHECK(needs.unixTime && !needs.localTime); // only the copy shows, in a zone of its own
    settings.Widget(BarWidget::EarthTime).on = true;
    CHECK(NeedsOf(settings, {}).localTime);
    CHECK(NeedsOf(settings, {PanelWidget{BarWidget::Bags, 2}}).containers == 0); // Bags is off, any copy of it
}

TEST(the_bags_popup_lists_mog_safe_2_only_once_it_holds_something)
{
    // The server gives Mog Safe 2 its room before the second floor is open, so its room says nothing of whether
    // it can be used; what is in it does.
    Settings settings;
    BarState state;
    state.bags[0] = BagCount{26, 30};
    state.bags[kMogSafe2] = BagCount{0, 50};
    const auto listed = [&](const char* name) {
        for (const PopupSection& section : BarPopupContent(BarWidget::Bags, state, settings).sections)
            for (const PopupRow& row : section.rows)
                for (const PopupCell& cell : row.cells)
                    for (const Span& span : cell.text)
                        if (span.text == name) return true;
        return false;
    };
    CHECK(listed("Inventory") && !listed("Safe (2)"));
    state.bags[kMogSafe2] = BagCount{3, 50};
    CHECK(listed("Safe (2)"));
}

TEST(the_bags_widget_shows_the_bag_picked_for_it_and_the_plugin_reads_that_far)
{
    Settings settings;
    BarState state;
    state.bags[0] = BagCount{26, 30};
    state.bags[5] = BagCount{12, 80}; // the satchel
    CHECK_EQ(Content(BarWidget::Bags, state, settings).title.front().text, "26 / 30");
    settings.Widget(BarWidget::Bags).bag = 5;
    settings.Widget(BarWidget::Bags).design = DesignFrom(BarWidget::Bags, "count1 | name1");
    const WidgetContent satchel = Content(BarWidget::Bags, state, settings);
    CHECK(satchel.title.front().text == "12 / 80" && satchel.subtitle.front().text == "Satchel");
    CHECK_EQ(NeedsOf(settings, {}).containers, 6);
    // A bag the character has no room in reads as nothing yet.
    settings.Widget(BarWidget::Bags).bag = 6;
    CHECK_EQ(Content(BarWidget::Bags, state, settings).title.front().text, "-");
}

TEST(the_bags_widget_shows_two_bags_at_once_each_by_its_own_buttons)
{
    Settings settings;
    BarState state;
    state.bags[0]  = BagCount{26, 30};
    state.bags[11] = BagCount{40, 80}; // the third wardrobe
    WidgetOptions& o = settings.Widget(BarWidget::Bags);
    o.bag            = 0;
    o.secondBag      = 11;
    o.design         = DesignFrom(BarWidget::Bags, "name1 count1 | name2 count2");
    const WidgetContent both = Content(BarWidget::Bags, state, settings);
    CHECK_EQ(Joined(both.title), "Inventory  26 / 30");
    CHECK_EQ(Joined(both.subtitle), "Wardrobe (3)  40 / 80");
    // The plugin counts the second bag only while its count shows.
    CHECK_EQ(NeedsOf(settings, {}).containers, 12);
    o.design = DesignFrom(BarWidget::Bags, "name1 count1 | name2 -count2");
    CHECK_EQ(NeedsOf(settings, {}).containers, 1);
    // A bag of its own each: the second picked alone moves nothing of the first.
    o.design    = DesignFrom(BarWidget::Bags, "count2 | count1");
    o.secondBag = 5;
    const WidgetContent swapped = Content(BarWidget::Bags, state, settings);
    CHECK(Joined(swapped.title) == "-" && Joined(swapped.subtitle) == "26 / 30");
}

TEST(a_bag_is_named_without_mog_and_its_number_in_brackets)
{
    CHECK_EQ(ContainerName(0), "Inventory");
    CHECK_EQ(ContainerName(1), "Safe");
    CHECK_EQ(ContainerName(2), "Storage");
    CHECK_EQ(ContainerName(5), "Satchel");
    CHECK_EQ(ContainerName(8), "Wardrobe");
    CHECK_EQ(ContainerName(9), "Safe (2)");
    CHECK_EQ(ContainerName(11), "Wardrobe (3)");
    CHECK_EQ(ContainerName(16), "Wardrobe (8)");
}

TEST(the_bags_to_pick_start_with_the_inventory_storage_and_both_safes)
{
    // Then the rest in Ashita's order, the temporary items left out.
    CHECK(BagChoices() == (std::vector<int>{0, 2, 1, 9, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16}));
}

// The user: "Use short name everywhere", as the widget and its Bag dropdowns write them.
TEST(the_bags_popup_lists_the_bags_in_ashitas_order_by_their_short_names)
{
    Settings settings;
    BarState state;
    for (int c = 0; c < kContainerCount; ++c)
        state.bags[static_cast<size_t>(c)] = BagCount{1, 80};
    std::vector<std::string> names;
    for (const PopupSection& section : BarPopupContent(BarWidget::Bags, state, settings).sections)
        for (const PopupRow& row : section.rows)
            for (size_t i = 0; i < row.cells.size(); i += 2)
                names.push_back(row.cells[i].text.front().text);
    CHECK(names == (std::vector<std::string>{"Inventory", "Safe", "Storage", "Locker", "Satchel", "Sack", "Case", "Wardrobe",
                       "Safe (2)", "Wardrobe (2)", "Wardrobe (3)", "Wardrobe (4)", "Wardrobe (5)", "Wardrobe (6)",
                       "Wardrobe (7)", "Wardrobe (8)"}));
}

TEST(a_targeted_players_jobs_show_from_whatever_knows_them_unless_switched_off)
{
    Settings settings;
    BarState state;
    ActorInfo player;
    player.kind = EntityKind::Player;
    std::snprintf(player.name, sizeof(player.name), "%s", "Tanyrus");
    state.target = &player;
    CHECK(Content(BarWidget::Target, state, settings).subtitle.empty()); // nothing known of them yet
    player.jobs = PlayerJobs{1, 75, 13, 37}; // from a lookup, the party list or a check
    CHECK_EQ(Content(BarWidget::Target, state, settings).subtitle.front().text, "WAR 75 / NIN 37");
    settings.Widget(BarWidget::Target).design = DesignFrom(BarWidget::Target, "name | -player");
    CHECK(Content(BarWidget::Target, state, settings).subtitle.empty());
    // An anonymous player's are the game's question marks.
    settings.Widget(BarWidget::Target).design = DefaultDesign(BarWidget::Target);
    player.jobs.reset();
    player.status = PlayerStatus{};
    player.status->anonymous = true;
    CHECK_EQ(Content(BarWidget::Target, state, settings).subtitle.front().text, "? ? ?");
}

TEST(world_infos_nation_buttons_show_each_crest_and_the_regions_it_holds_and_its_popup_names_them)
{
    Settings settings;
    BarState state;
    WidgetOptions& o = settings.Widget(BarWidget::World);
    o.on             = true;
    o.design         = DesignFrom(BarWidget::World, "sandoria bastok windurst beastmen | -ship -guild");
    CHECK_EQ(Joined(Content(BarWidget::World, state, settings).title), "-"); // before the standings come
    RegionOwners owners{};
    owners.fill(Nation::Beastmen);
    owners[0] = Nation::SanDoria;                         // Ronfaure
    owners[3] = owners[4] = Nation::Bastok;               // Gustaberg and Derfland
    owners[5] = Nation::Windurst;
    owners[18] = Nation::None;
    state.owners = owners;
    // Each its crest and count, in its color, on the line the design puts it.
    std::vector<Span> counts;
    for (const Span& span : Content(BarWidget::World, state, settings).title)
        if (span.picture != GamePicture::None) counts.push_back(span);
    CHECK(counts.size() == 4);
    CHECK(counts[0].text == " 1" && counts[1].text == " 2" && counts[2].text == " 1" && counts[3].text == " 14");
    CHECK(counts[0].picture == GamePicture::SanDoriaCrest && counts[1].picture == GamePicture::BastokCrest &&
          counts[2].picture == GamePicture::WindurstCrest && counts[3].picture == GamePicture::BeastmenCrest);
    CHECK(counts[1].argb == NationColor(Nation::Bastok));
    // Bastok alone.
    o.design = DesignFrom(BarWidget::World, "bastok | -ship -guild -sandoria -windurst -beastmen");
    const WidgetContent bastok = Content(BarWidget::World, state, settings);
    CHECK(bastok.title.size() == 1 && bastok.title[0].text == " 2" && bastok.hasPopup);
    // The popup has every nation's regions, a tab each.
    const PopupSection card = ConquestCard(BarPopupContent(BarWidget::World, state, settings));
    CHECK(card.tabs.size() == 4 && card.tabs[1].name.text == "Bastok");
    std::map<std::string, std::string> picked{{"Conquest", "Bastok"}};
    state.pickedTabs = &picked;
    CHECK(TabLines(ConquestCard(BarPopupContent(BarWidget::World, state, settings))) ==
          (std::vector<std::string>{"Gustaberg", "Derfland"}));
}

TEST(the_bags_popup_always_lists_storage_and_never_the_temporary_items)
{
    // Storage (the Mog House's furniture storage) is listed even with no room yet, dim, and the Temporary Items bag is
    // not listed.
    Settings settings;
    BarState state;
    state.bags[0] = BagCount{26, 30};
    state.bags[3] = BagCount{0, 50}; // the temporary items
    const auto cell = [&](const char* name) -> std::optional<Span> {
        for (const PopupSection& section : BarPopupContent(BarWidget::Bags, state, settings).sections)
            for (const PopupRow& row : section.rows)
                for (size_t i = 0; i + 1 < row.cells.size(); i += 2)
                    if (!row.cells[i].text.empty() && row.cells[i].text.front().text == name) return row.cells[i + 1].text.front();
        return std::nullopt;
    };
    const std::optional<Span> storage = cell("Storage");
    CHECK(storage && storage->text == "0 / 0" && storage->tone == Tone::Dim);
    CHECK(!cell("Temporary").has_value());
    state.bags[2] = BagCount{12, 80};
    CHECK(cell("Storage")->text == "12 / 80" && cell("Storage")->tone == Tone::Normal);
    // Nor can the widget be set to show it.
    for (int choice = 0; choice < static_cast<int>(BagChoices().size()); ++choice)
        CHECK(BagChoices()[static_cast<size_t>(choice)] != 3);
}

TEST(world_infos_conquest_card_marks_your_nations_regions_without_a_supply_run)
{
    Settings settings;
    settings.Widget(BarWidget::World).on = true;
    BarState state;
    RegionOwners owners{};
    owners[3] = owners[4] = Nation::Bastok; // Gustaberg and Derfland
    state.owners = owners;
    state.nation = Nation::Bastok;
    // Before an overseer has told: one line says how to learn them.
    OutpostRuns runs;
    state.outposts = &runs;
    std::vector<std::string> lines = TabLines(ConquestCard(BarPopupContent(BarWidget::World, state, settings)));
    CHECK(lines.size() == 3 && lines[0] == "Gustaberg" && lines[2].find("overseer") != std::string::npos);
    // Told by Bastok's overseer that Gustaberg's was run: Derfland says "Supply run available" in a column against
    // the card's right, a gray flag after it; Gustaberg does not.
    runs.Heard(GuardEvent{Nation::Bastok, {1, 0, 0, 0, 1 << (3 + 5), 0, 0, 0}}, 20);
    const PopupContent popup = BarPopupContent(BarWidget::World, state, settings);
    const PopupSection told  = ConquestCard(popup);
    CHECK((TabLines(told) == std::vector<std::string>{"Gustaberg", "DerflandSupply run available"}));
    const std::vector<PopupRow>& rows = told.tabs.at(told.tab).rows;
    const auto flagged = [](const PopupRow& row) {
        return std::ranges::any_of(row.cells, [](const PopupCell& c) {
            return c.icon == Icon::Flag && c.tint && c.tint->tone == Tone::Dim && c.text.empty();
        });
    };
    CHECK(!flagged(rows[0]) && flagged(rows[1]));
    CHECK(rows[1].cells.back().icon == Icon::Flag);
    const BarMetrics m = MetricsFor(45, BarShape::Pills);
    const auto measure = [](const Span& span, PopupFont font) {
        const float height = font == PopupFont::Title ? 24.0f : 18.0f;
        return TextSize{8.0f * static_cast<float>(span.text.size()), height, 0.3f * height, height - 1.0f};
    };
    const PopupLayout l = LayoutPopup(popup, m, false, measure, 1000.0f);
    const auto piece = [&](auto is) { return std::ranges::find_if(l.pieces, is); };
    const auto name  = piece([](const PopupPiece& p) { return p.span.text == "Derfland"; });
    const auto said  = piece([](const PopupPiece& p) { return p.span.text == "Supply run available"; });
    const auto flag  = piece([](const PopupPiece& p) { return p.kind == PopupPiece::Kind::Icon && p.icon == Icon::Flag; });
    const auto ran   = piece([](const PopupPiece& p) { return p.span.text == "Gustaberg"; });
    CHECK(name != l.pieces.end() && said != l.pieces.end() && flag != l.pieces.end() && ran != l.pieces.end());
    const auto socket = piece([](const PopupPiece& p) { return p.kind == PopupPiece::Kind::Socket; });
    CHECK(socket != l.pieces.end());
    // On Derfland's line, the words after the name and the flag after them, ending as far in from the tab's right as
    // the name starts from its left; Gustaberg lined up with it.
    CHECK(said->x > name->x + name->width && flag->x >= said->x + said->width);
    CHECK(std::fabs((socket->x + socket->width) - (flag->x + flag->width) - (name->x - socket->x)) < 0.5f);
    CHECK(std::fabs((flag->y + flag->height / 2.0f) - (name->y + name->height / 2.0f)) < name->height / 2.0f);
    CHECK_EQ(ran->x, name->x);
    // Another nation's regions are not yours to run supplies to.
    owners[0] = Nation::SanDoria;
    state.owners = owners;
    const PopupSection all = ConquestCard(BarPopupContent(BarWidget::World, state, settings));
    CHECK(all.tabs.size() == 4 && all.tabs[1].name.text == "San d'Oria");
    CHECK((Lines(PopupSection{"", all.tabs[1].rows}) == std::vector<std::string>{"Ronfaure"}));
}

TEST(world_infos_conquest_card_has_your_nation_first_picked_anew_each_time_and_its_tabs_two_by_two)
{
    Settings settings;
    settings.Widget(BarWidget::World).on = true;
    BarState state;
    state.unixMs = kVanaEpochUnixMs;
    RegionOwners owners{};
    state.owners = owners;
    const auto card = [&] { return ConquestCard(BarPopupContent(BarWidget::World, state, settings)); };
    const auto names = [](const PopupSection& c) {
        std::vector<std::string> out;
        for (const PopupTab& tab : c.tabs)
            out.push_back(tab.name.text);
        return out;
    };
    CHECK((names(card()) == std::vector<std::string>{"San d'Oria", "Bastok", "Windurst", "Beastmen"})); // no nation known
    CHECK_EQ(card().tab, static_cast<size_t>(0));
    state.nation = Nation::Windurst;
    const PopupSection yours = card();
    CHECK((names(yours) == std::vector<std::string>{"Windurst", "San d'Oria", "Bastok", "Beastmen"}));
    // Each tab with its nation's crest before its name.
    CHECK(yours.tabs[0].picture == GamePicture::WindurstCrest && yours.tabs[3].picture == GamePicture::BeastmenCrest);
    CHECK(yours.tab == 0 && yours.tabsAcross == 2 && yours.tabEachOpen);
    // Its heading counts down to the tally, which the card and its picks are kept by without.
    CHECK_EQ(yours.heading, "Conquest (Next Tally in " + Countdown(SecondsToTally(state.unixMs)) + ")");
    std::map<std::string, std::string> picked{{"Conquest", "Bastok"}};
    state.pickedTabs = &picked;
    const PopupSection other = card();
    CHECK_EQ(other.tabs.at(other.tab).name.text, "Bastok");
}

namespace
{
    // Day 9, an Earthsday, at 10:00: 576 s into the airships' 864 s cycle.
    constexpr int64_t kEarthsdayTen = kVanaEpochUnixMs + (9LL * 3456 + 10 * 144) * 1000;
    template <typename T>
    int IndexOf(std::span<const T> items, const char* name, const char* T::*field)
    {
        for (size_t i = 0; i < items.size(); ++i)
            if (std::string(items[i].*field) == name) return static_cast<int>(i);
        return -1;
    }
    Settings WorldShowing(std::string_view design)
    {
        Settings settings;
        WidgetOptions& o = settings.Widget(BarWidget::World);
        o.on             = true;
        o.design         = DesignFrom(BarWidget::World, design);
        o.worldRoute     = IndexOf(TravelRoutes(), "Jeuno to San d'Oria", &TravelRoute::name);
        o.worldGuild     = IndexOf(GuildShops(), "Chaupire", &GuildShop::shops);
        return settings;
    }
}

TEST(world_infos_buttons_each_tell_their_name_and_then_its_countdown_dim_in_parentheses)
{
    BarState state;
    state.unixMs = kEarthsdayTen;
    const auto shown = [&](std::string_view part) {
        const WidgetContent w = Content(BarWidget::World, state, WorldShowing(std::string(part) + " |"));
        return Joined(w.title);
    };
    // The airship's door opens at 28 s into the next cycle: 864 - 576 + 28 s from now.
    CHECK_EQ(shown("ship"), "Jeuno to San d'Oria (Boards in 5:16)");
    // Open until 21:00, 11 Vana'diel hours away: it says when it closes, and not that it is open.
    CHECK_EQ(shown("guild"), "Woodworking (Closes in 26:24)");
    // Closed, it says when it opens.
    state.unixMs = kEarthsdayTen + static_cast<int64_t>(27 * 60 * 1000);
    CHECK(shown("guild").starts_with("Woodworking (Opens in "));
    state.unixMs = kEarthsdayTen;
    // The RSE and the moon are the pop-out's alone.
    CHECK(std::ranges::find(PartsOf(BarWidget::World), WidgetPart::WorldShip) != PartsOf(BarWidget::World).end());
    CHECK(PartsOf(BarWidget::World).size() == 6);
    // The name in the line's tone, the countdown dim.
    const WidgetContent ship = Content(BarWidget::World, state, WorldShowing("ship |"));
    CHECK(ship.title.front().tone == Tone::Normal && ship.title.back().tone == Tone::Dim);
    // Two on the panel at once, a line each.
    const WidgetContent both = Content(BarWidget::World, state, WorldShowing("ship | guild"));
    CHECK(Joined(both.title).starts_with("Jeuno") && Joined(both.subtitle).starts_with("Woodworking"));
}

TEST(the_world_info_popup_has_conquest_travel_and_the_calendar_in_a_row_and_guilds_under_them)
{
    BarState state;
    state.unixMs = kEarthsdayTen;
    state.zone   = 79;
    const Settings settings = WorldShowing("ship |");
    const PopupContent all  = PanelPopupContent(BarWidget::World, state, settings);
    std::vector<std::string> headings;
    for (const PopupSection& section : all.sections)
        headings.push_back(std::string(CardKey(section.heading)));
    CHECK((headings == std::vector<std::string>{"Conquest", "Travel", "Calendar", "Guilds"}));
    CHECK(!all.sections[0].beside && all.sections[1].beside && all.sections[2].beside && !all.sections[3].beside);
    const std::vector<std::string> lines = Lines(all.sections[2]);
    // Who, then where and when on the right, the time last and alone; the tally is in the Conquest card's heading.
    CHECK_EQ(lines.size(), static_cast<size_t>(4));
    CHECK_EQ(lines[0], "RSE Hume Female Gusgen Mines");
    CHECK_EQ(lines[1], std::string("Next RSE Elvaan Male ") + RseAt(kEarthsdayTen, 1).place + kDot + "6:19:12");
    CHECK_EQ(lines[2], "Full Moon 35:07:12");
    // The next new moon is day 88.
    CHECK_EQ(lines[3], "New Moon " + Countdown(SecondsToDay(kEarthsdayTen, 88)));
    for (const PopupRow& row : all.sections[2].rows)
    {
        CHECK(row.kind == RowKind::Pair);
        CHECK(row.cells[0].text.front().tone == Tone::Dim); // the line's name
    }
    CHECK_EQ(all.sections[2].rows[1].cells[1].text.back().text, "6:19:12");
}

TEST(each_kind_of_ship_has_a_tab_of_its_routes)
{
    BarState state;
    state.unixMs = kEarthsdayTen;
    const Settings settings = WorldShowing("ship |");
    const auto travel = [&] { return BarPopupContent(BarWidget::World, state, settings).sections.at(1); };
    // A tab a kind, the first picked until another is.
    const PopupSection first = travel();
    CHECK_EQ(first.heading, "Travel");
    std::vector<std::string> kinds;
    for (const PopupTab& tab : first.tabs)
        kinds.push_back(tab.name.text);
    CHECK((kinds == std::vector<std::string>{"Airships", "Boats", "Manaclipper", "Barge"}));
    CHECK(first.tab == 0 && first.rows.empty());
    // Each tab lists its kind's routes, set in, in three columns: the route, whether the ship arrives or departs
    // next against its right, and how long until it does against its left.
    const auto airships = std::ranges::count_if(TravelRoutes(), [](const TravelRoute& r) { return std::string(r.group) == "Airships"; });
    const auto rows = [](const PopupTab& tab) {
        CHECK(tab.rows.size() == 1 && tab.rows[0].kind == RowKind::Grid && tab.rows[0].columns == 3 && tab.rows[0].nested &&
              !tab.rows[0].spread && tab.rows[0].trailing); // the stage and time against the card's right
        std::vector<std::array<std::string, 3>> out;
        const std::vector<PopupCell>& cells = tab.rows[0].cells;
        for (size_t i = 0; i + 2 < cells.size(); i += 3)
        {
            CHECK(cells[i + 1].align == Align::Right && cells[i + 2].align == Align::Left && cells[i + 2].text.size() == 1);
            out.push_back({Joined(cells[i].text), Joined(cells[i + 1].text), Joined(cells[i + 2].text)});
        }
        return out;
    };
    const auto route = [&](const PopupTab& tab, const char* name) {
        for (const std::array<std::string, 3>& r : rows(tab))
            if (r[0] == name) return r;
        return std::array<std::string, 3>{};
    };
    CHECK_EQ(rows(first.tabs[0]).size(), static_cast<size_t>(airships));
    CHECK((route(first.tabs[0], "Jeuno to San d'Oria") == std::array<std::string, 3>{"Jeuno to San d'Oria", "Arrives in", "5:16"}));
    // Picking Boats picks its tab.
    std::map<std::string, std::string> picked{{"Travel", "Boats"}};
    state.pickedTabs = &picked;
    CHECK_EQ(travel().tab, static_cast<size_t>(1));
    picked["Travel"] = "Airships";
    state.unixMs = kVanaEpochUnixMs + 864LL * 1000 + 100 * 1000; // 100 s into a cycle: its door opened at 28 s
    CHECK((route(travel().tabs[0], "Jeuno to San d'Oria") == std::array<std::string, 3>{"Jeuno to San d'Oria", "Departs in", "1:22"}));
}

TEST(each_guild_has_a_tab_of_its_shops_hours_in_columns)
{
    BarState state;
    state.unixMs = kEarthsdayTen;
    const Settings settings = WorldShowing("ship |");
    const auto guilds = [&] { return BarPopupContent(BarWidget::World, state, settings).sections.at(3); };
    // A tab a guild, in the shops' order; no goblins selling chips among them.
    std::vector<std::string> names;
    std::map<std::string, int> count; // each guild's shops
    for (const GuildShop& shop : GuildShops())
    {
        if (names.empty() || names.back() != shop.guild) names.push_back(shop.guild);
        ++count[shop.guild];
    }
    CHECK(std::ranges::find(names, "Chips") == names.end());
    const PopupSection section = guilds();
    CHECK_EQ(section.heading, "Guilds");
    CHECK_EQ(section.tabs.size(), names.size());
    for (size_t i = 0; i < names.size(); ++i)
    {
        const PopupTab& tab = section.tabs[i];
        CHECK(tab.name == Text(names[i])); // every tab in one color
        // A title over the box of hours, with no count of the open ones.
        CHECK(!tab.rows[0].nested && tab.rows[0].cells[0].text == ListOf(Text("Shop Hours", Tone::Heading)));
    }
    // Each shop's hours: open or closed against the right, when that changes in the middle, and who and where.
    const std::map<std::string, std::string> picked{{"Guilds", "Woodworking"}};
    state.pickedTabs = &picked;
    const PopupSection woodworking = guilds();
    const PopupRow& grid           = woodworking.tabs[woodworking.tab].rows[1];
    CHECK(woodworking.tabs[woodworking.tab].name.text == "Woodworking");
    CHECK(grid.nested && grid.kind == RowKind::Grid && grid.columns == 3);
    CHECK_EQ(grid.cells.size(), static_cast<size_t>(3 * count["Woodworking"]));
    const auto at = std::ranges::find_if(grid.cells, [](const PopupCell& c) { return Joined(c.text) == "Chaupire, Northern San d'Oria"; });
    CHECK(at != grid.cells.end() && (at - grid.cells.begin()) % 3 == 2 && at->align == Align::Left);
    // Open until 21:00, 11 Vana'diel hours away.
    CHECK(Joined((at - 2)->text) == "Open" && (at - 2)->align == Align::Right);
    CHECK(Joined((at - 1)->text) == "closes in 26:24" && (at - 1)->align == Align::Center);
}

TEST(a_craft_guilds_tab_lists_todays_turn_ins_apart_from_its_shops_with_yours_lit)
{
    BarState state;
    state.unixMs = kEarthsdayTen;
    const Settings settings = WorldShowing("ship |");
    const std::map<std::string, std::string> picked{{"Guilds", "Fishing"}};
    state.pickedTabs = &picked;
    const auto fishing = [&] {
        const PopupSection guilds = BarPopupContent(BarWidget::World, state, settings).sections.at(3);
        return guilds.tabs[guilds.tab].rows;
    };
    // The shops in one socket under their title, the turn-ins under a line of their own in another.
    std::vector<PopupRow> rows = fishing();
    CHECK(rows.size() == 4 && !rows[0].nested && rows[1].nested && !rows[2].nested && rows[3].nested);
    CHECK_EQ(Lines(PopupSection{"", {rows[2]}}).front(), "Today's Turn-Ins");
    // Not fetched yet, it says so.
    CHECK_EQ(Lines(PopupSection{"", {rows[3]}}).front(), "They come from phoenix-xi.com");
    state.turnInsFailed = true;
    CHECK_EQ(Lines(PopupSection{"", {fishing()[3]}}).front(), "phoenix-xi.com did not answer; trying again soon");
    // A rank's item and its high quality on one row, the high quality in brackets: the +1, or else the one worth more,
    // or else the second; the most a day once, or both where they differ.
    GuildPatterns today;
    today.guilds[0] = {GuildTurnIn{0, 17, "Flute +1", 14, 1120}, GuildTurnIn{0, 16, "Flute", 12, 1120},
        GuildTurnIn{1, 20, "Roasted Corn", 31, 1280}, GuildTurnIn{1, 21, "Grilled Corn", 37, 1280},
        GuildTurnIn{2, 30, "Darksteel Cuisses", 6500, 7400}, GuildTurnIn{2, 31, "Dst. Cuisses +1", 7150, 7040},
        GuildTurnIn{3, 40, "", 90, 2000}, GuildTurnIn{4, 16928, "Hellfire Sword", 3488, 7000},
        GuildTurnIn{4, 16594, "Inferno Sword", 3488, 7000}};
    state.turnIns = &today;
    state.crafts  = CraftRanks{1};
    const auto grid = [&] { return fishing()[3]; };
    const auto text = [](const PopupRow& g, size_t row, size_t column) { return Joined(g.cells[6 * row + column].text); };
    // Day 0 of Phoenix's epoch: Novice to Expert are each asked for the table's first; a Recruit asks as a Novice, so
    // that row is yours, starred and lit.
    PopupRow day = grid();
    CHECK(day.kind == RowKind::Grid && day.columns == 6 && day.nested && day.header && day.scroll == "Guilds/Fishing/Turn-ins");
    CHECK(day.roomy); // its columns further apart than the shops'

    CHECK_EQ(day.cells.size(), static_cast<size_t>(6 * 9));
    CHECK(text(day, 0, 0) == "Rank" && text(day, 0, 1) == "NQ Item" && text(day, 0, 2) == "Points" &&
          text(day, 0, 3) == "HQ Item" && text(day, 0, 4) == "Points" && text(day, 0, 5) == "Max Points");
    // Each rank's item and its high quality in columns of their own, each with its picture.
    CHECK(text(day, 1, 0) == "Novice" && text(day, 1, 1) == "Flute" && day.cells[7].item == 16 && text(day, 1, 2) == "12" &&
          text(day, 1, 3) == "Flute +1" && day.cells[9].item == 17 && text(day, 1, 4) == "14" && text(day, 1, 5) == "1,120");
    CHECK(day.cells[6].star && day.cells[7].text.front().tone == Tone::Heading && day.cells[9].text.front().tone == Tone::Heading);
    CHECK(text(day, 2, 0) == "Apprentice" && !day.cells[12].star && day.cells[13].text.front().tone == Tone::Normal);
    CHECK(text(day, 8, 0) == "Expert" && text(day, 8, 1) == "Flute");
    // An Expert's own row is Expert's.
    state.crafts = CraftRanks{10};
    day          = grid();
    CHECK(!day.cells[6].star && day.cells[6 * 8].star);
    // Day 2: every rank is asked for the third; both most-a-days where they differ.
    constexpr int64_t kDayMs = 86'400'000;
    state.unixMs = kVanaEpochUnixMs + 2 * kDayMs;
    day          = grid();
    CHECK(text(day, 1, 1) == "Darksteel Cuisses" && text(day, 1, 3) == "Dst. Cuisses +1" && text(day, 1, 5) == "7,400 [7,040]");
    // Day 13: Novice 13 % 4, Apprentice 13 % 5, Journeyman 13 % 6, Craftsman 13 % 7 and Artisan 13 % 8 (none in this
    // table, so left out), Adept 13 % 9, Veteran and Expert 13 % 10.
    state.unixMs = kVanaEpochUnixMs + 13 * kDayMs;
    day          = grid();
    CHECK_EQ(day.cells.size(), static_cast<size_t>(6 * 7));
    CHECK(text(day, 1, 0) == "Novice" && text(day, 1, 1) == "Roasted Corn" && text(day, 1, 3) == "Grilled Corn" &&
          text(day, 1, 2) == "31" && text(day, 1, 4) == "37");
    // One item, no name from the site: no high quality, its cells left empty.
    CHECK(text(day, 2, 0) == "Apprentice" && text(day, 2, 1) == "Item 40" && text(day, 2, 2) == "90" && text(day, 2, 3).empty() &&
          day.cells[6 * 2 + 3].item == 0 && text(day, 2, 4).empty());
    CHECK(text(day, 3, 0) == "Journeyman" && text(day, 3, 1) == "Roasted Corn");
    // Worth the same and neither a +1, listed high quality first: Phoenix's recipes say which is which.
    CHECK(text(day, 4, 0) == "Adept" && text(day, 4, 1) == "Inferno Sword" && day.cells[6 * 4 + 1].item == 16594 &&
          text(day, 4, 3) == "Hellfire Sword" && text(day, 4, 4) == "3,488");
    CHECK(text(day, 5, 0) == "Veteran" && text(day, 5, 1) == "Item 40" && text(day, 6, 0) == "Expert" && text(day, 6, 1) == "Item 40");
    // A guild that takes no turn-ins has none.
    const std::map<std::string, std::string> pickedTenshodo{{"Guilds", "Tenshodo"}};
    state.pickedTabs = &pickedTenshodo;
    const PopupSection guilds = BarPopupContent(BarWidget::World, state, settings).sections.at(3);
    CHECK(guilds.tabs[guilds.tab].name.text == "Tenshodo" && guilds.tabs[guilds.tab].rows.size() == 2);
}

TEST(the_forecast_pop_out_puts_today_beside_the_forecast_over_the_elements)
{
    Settings settings;
    BarState state;
    state.zone   = 103;
    state.unixMs = kVanaEpochUnixMs + 777LL * 3456 * 1000;
    const PopupContent popup = PanelPopupContent(BarWidget::Weather, state, settings);
    const BarMetrics m = MetricsFor(45, BarShape::Pills);
    const auto measure = [](const Span& span, PopupFont font) {
        const float height = font == PopupFont::Title ? 24.0f : 18.0f;
        return TextSize{8.0f * static_cast<float>(span.text.size()), height, 0.3f * height, height - 1.0f};
    };
    std::vector<const PopupPiece*> cards;
    const PopupLayout l = LayoutPopup(popup, m, false, measure, 1000.0f);
    for (const PopupPiece& p : l.pieces)
        if (p.kind == PopupPiece::Kind::Card) cards.push_back(&p);
    CHECK_EQ(cards.size(), static_cast<size_t>(3));
    CHECK(cards[0]->y == cards[1]->y && cards[1]->x > cards[0]->x + cards[0]->width && cards[0]->height == cards[1]->height);
    CHECK(cards[2]->y > cards[0]->y + cards[0]->height && cards[2]->x == cards[0]->x &&
          cards[2]->width > cards[0]->width + cards[1]->width);
}

TEST(a_combined_pills_popup_joins_the_titles_and_cards_of_each_widget_shown_with_one)
{
    Settings settings;
    BarState state;
    settings.Widget(BarWidget::Bags).combine = PanelWidget{BarWidget::Currency};
    settings.Widget(BarWidget::Currency).combine  = PanelWidget{BarWidget::EarthTime};
    settings.Widget(BarWidget::EarthTime).on = false; // in the pill, but shown nowhere
    const PopupContent bags = BarPopupContent(BarWidget::Bags, state, settings);
    const PopupContent gil  = BarPopupContent(BarWidget::Currency, state, settings);
    const PopupContent both = PanelPopupContent(BarWidget::Bags, state, settings);
    std::vector<Span> title = bags.title;
    title.push_back(Span{kDot, Tone::Dim});
    title.insert(title.end(), gil.title.begin(), gil.title.end());
    CHECK(both.title == title);
    std::vector<PopupSection> sections = bags.sections;
    sections.insert(sections.end(), gil.sections.begin(), gil.sections.end());
    // Each headed card collapses under the pill's first widget's name.
    for (PopupSection& section : sections)
        if (!section.heading.empty()) section.collapsePress = kCollapsePress + CollapseKey(BarWidget::Bags, section.heading);
    CHECK(both.sections == sections);
    // A widget alone has its own popup, with its clone chip.
    PopupContent alone = BarPopupContent(BarWidget::EarthTime, state, settings);
    alone.clonePress   = std::string(kWidgetClonePress) + std::to_string(SlotOf(BarWidget::EarthTime));
    for (PopupSection& section : alone.sections)
        if (!section.heading.empty()) section.collapsePress = kCollapsePress + CollapseKey(BarWidget::EarthTime, section.heading);
    CHECK(PanelPopupContent(BarWidget::EarthTime, state, settings) == alone);
    // A combined pill whose first widget is switched off has its partner's popup alone.
    settings.Widget(BarWidget::Bags).on = false;
    PopupContent partner = gil;
    partner.clonePress   = std::string(kWidgetClonePress) + std::to_string(SlotOf(BarWidget::Bags)); // the pill's
    for (PopupSection& section : partner.sections)
        if (!section.heading.empty()) section.collapsePress = kCollapsePress + CollapseKey(BarWidget::Bags, section.heading);
    CHECK(PanelPopupContent(BarWidget::Bags, state, settings) == partner);
}

TEST(two_linked_copies_of_a_widget_show_its_title_and_cards_once)
{
    // Zone linked with a second Zone: the same zone, region and influence.
    Settings settings;
    BarState state;
    state.zone = 103;
    const PanelWidget second{BarWidget::Zone, 1};
    settings.barSections[0].push_back(second);
    settings.Widget(second).on               = true;
    settings.Widget(BarWidget::Zone).combine = second;
    const PopupContent zone   = BarPopupContent(BarWidget::Zone, state, settings);
    const PopupContent linked = PanelPopupContent(BarWidget::Zone, state, settings);
    CHECK(linked.title == zone.title && linked.sections == zone.sections);
    CHECK(Content(second, state, settings).shown); // on the panel, beside the first
    // Two widgets of a kind with different cards keep both.
    settings.Widget(BarWidget::Currency).combine = PanelWidget{BarWidget::Bags};
    const PopupContent gilBags = PanelPopupContent(BarWidget::Currency, state, settings);
    CHECK(gilBags.sections.size() == BarPopupContent(BarWidget::Currency, state, settings).sections.size() +
                                         BarPopupContent(BarWidget::Bags, state, settings).sections.size());
}

TEST(a_link_puts_the_linked_widget_just_after_its_own_in_the_saved_layout)
{
    Settings s;
    s.Widget(BarWidget::Zone).combine = PanelWidget{BarWidget::Currency}; // Gil sits on the right
    const Settings clamped = Clamp(s);
    CHECK(clamped.barSections == Panel(s).placement);
    const std::vector<PanelWidget>& left = clamped.barSections[static_cast<int>(BarSection::Left)];
    const auto zone = std::ranges::find(left, PanelWidget{BarWidget::Zone});
    CHECK(zone != left.end() && zone + 1 != left.end() && *(zone + 1) == PanelWidget{BarWidget::Currency});
}

TEST(a_linked_pills_settings_are_its_widgets_that_are_on)
{
    Settings s;
    s.Widget(BarWidget::Bags).combine     = PanelWidget{BarWidget::Currency};
    s.Widget(BarWidget::Currency).combine = PanelWidget{BarWidget::EarthTime};
    CHECK((PillSettings(s, BarWidget::Bags) == std::vector<PanelWidget>{BarWidget::Bags, BarWidget::Currency, BarWidget::EarthTime}));
    s.Widget(BarWidget::Currency).on = false;
    CHECK((PillSettings(s, BarWidget::Bags) == std::vector<PanelWidget>{BarWidget::Bags, BarWidget::EarthTime}));
    s.Widget(BarWidget::EarthTime).on = false; // one left on: as a widget alone
    CHECK((PillSettings(s, BarWidget::Bags) == std::vector<PanelWidget>{BarWidget::Bags}));
    s.Widget(BarWidget::Bags).on = false; // none on: the pill's own
    CHECK((PillSettings(s, BarWidget::Bags) == std::vector<PanelWidget>{BarWidget::Bags}));
}

TEST(a_combined_pills_open_popup_gathers_what_each_of_its_widgets_needs)
{
    Settings settings;
    settings.Widget(BarWidget::Zone).on       = true;
    settings.Widget(BarWidget::World).on      = true;
    settings.Widget(BarWidget::Zone).combine  = PanelWidget{BarWidget::World};
    settings.Widget(BarWidget::Currency).combine   = PanelWidget{BarWidget::Bags};
    CHECK(NeedsOf(settings, {BarWidget::Zone}).conquest);
    CHECK(NeedsOf(settings, {BarWidget::Currency}).containers == kContainerCount);
    CHECK(!NeedsOf(settings, {BarWidget::Weather}).conquest);
}

TEST(the_standings_are_asked_for_while_the_zone_or_a_world_info_showing_a_nation_is_on_the_panel)
{
    Settings settings;
    settings.Widget(BarWidget::Zone).on  = false;
    settings.Widget(BarWidget::World).on = true;
    CHECK(!StandingsShown(settings, {})); // its next ship
    settings.Widget(BarWidget::World).design = DesignFrom(BarWidget::World, "ship | windurst");
    CHECK(StandingsShown(settings, {}));
    settings.Widget(BarWidget::World).on = false;
    CHECK(!StandingsShown(settings, {}));
    settings.Widget(BarWidget::Zone).on = true;
    CHECK(StandingsShown(settings, {}));
}

TEST(a_designer_chip_shows_its_parts_picture_as_the_game_is_now_or_a_stand_in)
{
    Settings settings;
    BarState state;
    state.unixMs  = kVanaEpochUnixMs;
    state.weather = Weather::DustStorm;
    const auto mark = [](const std::array<PartMark, kWidgetPartCount>& marks, WidgetPart part) {
        return marks[static_cast<size_t>(part)];
    };
    // The Weather's settings open: its icon is the dust storm's; the Target's, with no target, its stand-ins.
    const auto marks = PartMarksNow({PanelWidget{BarWidget::Weather}, PanelWidget{BarWidget::Target}}, state, settings);
    const std::vector<Icon> storm = Content(BarWidget::Weather, state, settings).icons; // the icon it leads with
    CHECK(!storm.empty() && mark(marks, WidgetPart::WeatherIcon) == PartMark{storm.front()});
    CHECK(mark(marks, WidgetPart::WeatherIcon) != DefaultMark(WidgetPart::WeatherIcon));
    CHECK(mark(marks, WidgetPart::TargetAggro) == DefaultMark(WidgetPart::TargetAggro));
    CHECK(mark(marks, WidgetPart::CurrencyCoin).icon == Icon::Gil);
    CHECK(mark(marks, WidgetPart::WeatherName) == PartMark{});
    // A mob targeted: its own aggression icon.
    const ActorInfo goblin = Goblin();
    state.target = &goblin;
    CHECK(mark(PartMarksNow({PanelWidget{BarWidget::Target}}, state, settings), WidgetPart::TargetAggro) ==
          PartMark{Icon::AggroNQ});
}

TEST(the_settings_widget_is_its_cog_alone_with_no_popup_copies_or_alignment)
{
    Settings settings;
    BarState state;
    CHECK(!Content(BarWidget::Settings, state, settings).shown); // off on a new install
    settings.Widget(BarWidget::Settings).on = true;
    const WidgetContent w = Content(BarWidget::Settings, state, settings);
    CHECK(w.shown && w.cog && !w.hasPopup && w.title.empty() && w.subtitle.empty() && w.icons.empty());
    CHECK(BarPopupContent(BarWidget::Settings, state, settings).sections.empty());
    CHECK(!CloneWidget(settings, BarWidget::Settings) && !HasCopies(BarWidget::Settings) && !Combines(BarWidget::Settings));
    CHECK(!Aligns(BarWidget::Settings) && !Aligns(BarWidget::GameHelp) && Aligns(BarWidget::Currency));
}

namespace
{
    // Each card's heading without its brackets, and whether it joins the row of the one before.
    std::vector<std::pair<std::string, bool>> Rows(const PopupContent& popup)
    {
        std::vector<std::pair<std::string, bool>> rows;
        for (const PopupSection& s : popup.sections)
            rows.emplace_back(std::string(CardKey(s.heading)), s.beside);
        return rows;
    }

    // The Zone in West Ronfaure with its influence known.
    BarState RonfaureState()
    {
        BarState state;
        state.zone   = 100;
        state.unixMs = 1791527400000;
        RegionOwners owners{};
        owners[0]    = Nation::SanDoria;
        state.owners = owners;
        state.shares =
            RegionShares{{{Nation::SanDoria, 61}, {Nation::Bastok, 21}, {Nation::Windurst, 11}, {Nation::Beastmen, 5}}};
        return state;
    }
}

TEST(a_pop_out_shows_any_widgets_cards_in_its_designs_rows_each_from_its_own_widget)
{
    Settings settings;
    const BarState state = RonfaureState();
    settings.Widget(BarWidget::Zone).popup = PopupDesign{{{CardKind::ZoneInfluence, CardKind::WeatherToday},
        {CardKind::CurrencyFavorites}}};
    const PopupContent zone = PanelPopupContent(BarWidget::Zone, state, settings);
    CHECK((Rows(zone) == std::vector<std::pair<std::string, bool>>{{"Influence", false}, {"Today", true}, {"Favorites", false}}));
    CHECK(Joined(zone.title).starts_with("West Ronfaure")); // the Zone's own title
    // Each card as its own widget's pop-out has it, collapsing under this pill's name.
    PopupSection today = *Section(BarPopupContent(BarWidget::Weather, state, settings), zone.sections[1].heading);
    today.beside        = true;
    today.collapsePress = kCollapsePress + CollapseKey(BarWidget::Zone, today.heading);
    CHECK(zone.sections[1] == today);
    CHECK(zone.sections[2].rows == Section(BarPopupContent(BarWidget::Currency, state, settings), "Favorites")->rows);
    // A card the design leaves out is not shown, and the Forecast pop-out without its own Today has the rest.
    settings.Widget(BarWidget::Weather).popup = PopupDesign{{{CardKind::WeatherForecast}}};
    CHECK((Rows(PanelPopupContent(BarWidget::Weather, state, settings)) ==
           std::vector<std::pair<std::string, bool>>{{"Weather Forecast", false}}));
}

TEST(a_card_with_nothing_to_show_drops_out_of_its_row_and_the_rest_take_its_place)
{
    Settings settings;
    BarState state = RonfaureState();
    state.shares.reset(); // no influence known yet
    settings.Widget(BarWidget::Zone).popup = PopupDesign{{{CardKind::ZoneInfluence, CardKind::WeatherToday},
        {CardKind::TargetDamage, CardKind::ZoneInfluence}}};
    // Nothing targeted: the Target's cards have nothing to show, and their row goes.
    CHECK((Rows(PanelPopupContent(BarWidget::Zone, state, settings)) == std::vector<std::pair<std::string, bool>>{{"Today", false}}));
}

TEST(another_widgets_card_takes_that_widgets_first_copys_settings)
{
    Settings settings;
    BarState state = RonfaureState();
    state.unixMs   = kVanaEpochUnixMs;
    settings.Widget(BarWidget::Zone).popup = PopupDesign{{{CardKind::WeatherToday}}};
    const auto todayIcon = [&] { return PanelPopupContent(BarWidget::Zone, state, settings).sections.at(0).rows.at(1).cells.at(0).icon; };
    settings.Widget(BarWidget::Weather).weatherIcons = CustomIcons::Always;
    const std::optional<Icon> custom = todayIcon();
    settings.Widget(BarWidget::Weather).weatherIcons = CustomIcons::Never;
    CHECK(custom.has_value() && todayIcon() != custom);
    // A second Forecast's own settings do not reach it.
    const PanelWidget second = *CloneWidget(settings, BarWidget::Weather);
    settings.Widget(second).weatherIcons = CustomIcons::Always;
    CHECK(todayIcon() != custom);
}

TEST(a_linked_pill_shows_each_widgets_rows_in_turn_without_repeating_a_card)
{
    Settings settings;
    const BarState state = RonfaureState();
    settings.Widget(BarWidget::Zone).combine = PanelWidget{BarWidget::Weather};
    settings.Widget(BarWidget::Zone).popup   = PopupDesign{{{CardKind::WeatherByElement, CardKind::ZoneInfluence}}};
    const PopupContent pill = PanelPopupContent(BarWidget::Zone, state, settings);
    // The Zone's row, then the Forecast's own rows but By Element, which the Zone's row has already.
    CHECK((Rows(pill) == std::vector<std::pair<std::string, bool>>{
                             {"By Element", false}, {"Influence", true}, {"Today", false}, {"Weather Forecast", true}}));
}

TEST(a_card_placed_in_another_widgets_pop_out_has_its_widgets_data_gathered_while_that_pop_out_shows)
{
    Settings settings;
    for (WidgetOptions& each : settings.widgets)
        each.on = false;
    settings.Widget(BarWidget::Zone).on    = true;
    settings.Widget(BarWidget::Zone).popup = PopupDesign{{{CardKind::TargetOverview, CardKind::BagsList},
        {CardKind::JobExp, CardKind::PartyMembers, CardKind::CurrencyFavorites}}};
    const BarNeeds closed = NeedsOf(settings, {}), open = NeedsOf(settings, {BarWidget::Zone});
    CHECK(!closed.target && !closed.job && !closed.party && !closed.gil && closed.containers == 0);
    CHECK(open.target && open.job && open.party && open.gil && open.containers == kContainerCount);
    CHECK(PopupShows(settings, {BarWidget::Zone}, BarWidget::Target) && !PopupShows(settings, {BarWidget::Zone}, BarWidget::World));
    // Status Timers' cards there read the statuses while it shows, as its own pop-out does.
    settings.Widget(BarWidget::Zone).popup = PopupDesign{{{CardKind::StatusBuffs}}};
    CHECK(NeedsOf(settings, {BarWidget::Zone}).statuses && !NeedsOf(settings, {}).statuses);
    // The conquest standings for World Info's Conquest card in the Currency pop-out, with neither widget on.
    settings.Widget(BarWidget::Zone).on        = false;
    settings.Widget(BarWidget::Currency).on    = true;
    settings.Widget(BarWidget::Currency).popup = PopupDesign{{{CardKind::WorldConquest}}};
    CHECK(StandingsShown(settings, {BarWidget::Currency}) && !StandingsShown(settings, {}));
    CHECK(NeedsOf(settings, {BarWidget::Currency}).conquest);
}

TEST(every_card_a_widget_builds_is_one_of_its_own_and_its_default_pop_out_shows_them_all)
{
    const FullState full;
    BarState state = full.state;
    state.itemName = NpcItemName;
    std::vector<ActorInfo> targets{full.target};
    for (const uint32_t npc : {16785729u, 16912886u, 16798100u, 16793970u, 16785730u, 17207852u, 16785781u, 16982252u,
             17678362u, 17719350u, 17772597u, 16916828u, 16818249u, 16806290u, 16879936u, 17105705u, 16822535u})
        targets.push_back(NpcTarget(npc, "NPC"));
    std::set<CardKind> built;
    const Settings settings;
    for (const ActorInfo& target : targets)
    {
        state.target = &target;
        for (int k = 0; k < kBarWidgetCount; ++k)
        {
            const auto kind = static_cast<BarWidget>(k);
            const PopupContent own = BarPopupContent(kind, state, settings);
            if (!own.settings || !Content(kind, state, settings).hasPopup) continue;
            std::vector<std::string> made, shown;
            for (const PopupSection& section : own.sections)
            {
                CHECK(section.card.has_value() && WidgetOfCard(*section.card) == kind);
                if (section.card) built.insert(*section.card);
                made.push_back(section.heading);
            }
            for (const PopupSection& section : PanelPopupContent(kind, state, settings).sections)
                shown.push_back(section.heading);
            std::ranges::sort(made);
            std::ranges::sort(shown);
            CHECK(made == shown);
        }
    }
    CHECK(built.size() >= static_cast<size_t>(kCardKindCount) / 2); // the fixtures reach most of them
}

TEST(a_created_widget_shows_its_name_and_any_widgets_parts_each_as_that_widgets_first_copy_has_them)
{
    Settings settings;
    BarState state = RonfaureState();
    state.bags[static_cast<size_t>(kMogSatchel)] = BagCount{3, 30};
    const PanelWidget made = *CreateWidget(settings);
    WidgetContent w = Content(made, state, settings);
    CHECK(w.shown && Joined(w.title) == "Custom Widget" && w.subtitle.empty() && !w.hasPopup);
    // The Zone's name beside it and the first Inventory's Bag 1 under it, that widget's bag as its settings pick it.
    settings.Widget(made).design = DesignFrom(BarWidget::Custom, "name zone.name | bags.count1");
    settings.Widget(BarWidget::Bags).bag = kMogSatchel;
    w = Content(made, state, settings);
    CHECK_EQ(Joined(w.title), "Custom Widget  " + std::string(KnownZone(state.zone)->name));
    CHECK_EQ(Joined(w.subtitle), "3 / 30");
    settings.Widget(made).name = "Mule";
    CHECK(Joined(Content(made, state, settings).title).starts_with("Mule  "));
    // A widget's icon leads at the start of the first line, as an icon part does.
    settings.Widget(made).design = DesignFrom(BarWidget::Custom, "bags.icon name |");
    w = Content(made, state, settings);
    CHECK(w.icons == std::vector{Icon::Bag} && Joined(w.title) == "Mule");
    // Its pop-out is its name over the cards its design rows, any widget's.
    settings.Widget(made).popup = PopupDesign{{{CardKind::ZoneInfluence}}};
    CHECK(Content(made, state, settings).hasPopup);
    const PopupContent popup = PanelPopupContent(made, state, settings);
    CHECK(Joined(popup.title) == "Mule" && Rows(popup) == (std::vector<std::pair<std::string, bool>>{{"Influence", false}}));
    CHECK(!popup.removePress.empty() && !popup.clonePress.empty());
    // Its designer's chips show what each widget shows now.
    state.weather = Weather::DustStorm;
    const auto marks = PartMarksNow({made}, state, settings);
    CHECK(marks[static_cast<size_t>(WidgetPart::WeatherIcon)] == PartMark{Content(BarWidget::Weather, state, settings).icons.front()});
}

TEST(a_created_widgets_parts_have_their_widgets_data_gathered_while_it_shows)
{
    Settings settings;
    for (WidgetOptions& each : settings.widgets)
        each.on = false;
    const PanelWidget made = *CreateWidget(settings);
    const auto needs = [&](const char* design) {
        settings.Widget(made).design = DesignFrom(BarWidget::Custom, design);
        return NeedsOf(settings, {});
    };
    const BarNeeds name = needs("name |");
    CHECK(!name.frames && !name.gil && !name.statuses && !name.localTime && name.containers == 0 && !name.zone);
    CHECK(needs("name | performance.fps").frames && !needs("name | -performance.fps").frames);
    CHECK(needs("gil.amount1 |").gil && WidgetShown(settings, BarWidget::Currency) && !AnyCopyShown(settings, BarWidget::Currency));
    CHECK(needs("statusTimers.pin1 |").statuses && needs("earthTime.date |").localTime);
    settings.Widget(BarWidget::Bags).secondBag = kMogSatchel;
    CHECK_EQ(needs("bags.count2 |").containers, kMogSatchel + 1);
    settings.Widget(made).design = DesignFrom(BarWidget::Custom, "name | world.windurst");
    CHECK(StandingsShown(settings, {}));
    settings.Widget(made).on = false;
    CHECK(!StandingsShown(settings, {}) && !WidgetShown(settings, BarWidget::World));
}

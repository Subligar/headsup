#include "bar_widgets.h"

#include "argb.h"
#include "bar_game_help.h"
#include "bar_job.h"
#include "bar_look.h"
#include "memo.h"
#include "bar_server_time.h"
#include "npcdata.h"
#include "ph_timers.h"
#include "titles.h"
#include "vanadiel.h"
#include "world_info.h"
#include "zones.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <span>
#include <string_view>
#include <utility>

namespace headsup
{
    namespace
    {
        const char* const kContainerNames[kContainerCount] = {"Inventory", "Mog Safe", "Storage", "Temporary",
            "Mog Locker", "Mog Satchel", "Mog Sack", "Mog Case", "Mog Wardrobe", "Mog Safe 2", "Mog Wardrobe 2",
            "Mog Wardrobe 3", "Mog Wardrobe 4", "Mog Wardrobe 5", "Mog Wardrobe 6", "Mog Wardrobe 7", "Mog Wardrobe 8"};
        constexpr const char* kItemsMenu = "Items";
        constexpr const char* kAnonymousJobs = "? ? ?"; // as the game writes an anonymous player's
        // The Mog House's own short captions for some of them, as the game's strip titles their lists.
        struct ShortCaption
        {
            const char* caption;
            int container;
        };
        constexpr ShortCaption kShortCaptions[] = {{"Locker", 4}, {"Satchel", 5}, {"M.Ward.", 8}, {"M.Ward. 2", 10}, {"M.Ward. 3", 11},
            {"M.Ward. 4", 12}, {"M.Ward. 5", 13}, {"M.Ward. 6", 14}, {"M.Ward. 7", 15}, {"M.Ward. 8", 16}};
        constexpr uint32_t kElementColors[kElementCount] = {0xFFFF8A3C, 0xFF8FD8FF, kBarGreen, 0xFFD8B25A, 0xFFC58CFF,
            0xFF5B9DFF, 0xFFFFFFFF, 0xFF9A8AA8};
        constexpr int kDamageColumns = 4, kElementColumns = 4, kStatusColumns = 5, kStatColumns = 4, kBagColumns = 2;
        constexpr int kListColumns = 3; // TP moves and spells
        constexpr const char* kStatNames[kStatCount] = {"STR", "DEX", "VIT", "AGI", "INT", "MND", "CHR", "MEva"};
        constexpr const char* kCombatNames[kCombatCount] = {"HP", "MP", "ATT", "DEF", "EVA", "ACC"};
        constexpr const char* kDamageKindNames[kDamageKindCount] = {"Physical", "Magic", "Ranged", "Breath"};
        constexpr std::pair<MobTrait, const char*> kTraitNames[] = {{kTraitRegen, "Regen"}, {kTraitRefresh, "Refresh"},
            {kTraitRegain, "Regain"}, {kTraitShadows, "Shadows"}, {kTraitCounter, "Counter"}};
        constexpr int kWholeAbsorb = 100; // percent
        constexpr int kForecastDays = 5, kForecastColumns = 4;
        constexpr int kWorldGuildColumns   = 3; // a shop open or closed, when that changes, and who and where
        constexpr int kZoneRollColumns     = 3; // a zone, and its chance of the element today and tomorrow
        constexpr int kTurnInColumns       = 6; // a rank, its item and its points, its high quality's, and the most a day
        constexpr uint16_t kGilItem  = 0xFFFF;

        Span Percent(int percent)
        {
            return Text(PercentText(percent), percent > 0 ? Tone::Weak : percent < 0 ? Tone::Resist : Tone::Neutral);
        }

        Span Ranked(std::string text, int rank)
        {
            return Text(std::move(text), rank < 0 ? Tone::Weak : rank > 0 ? Tone::Resist : Tone::Neutral);
        }

        // The data gives zone 0, the unknown one, a calendar of its own, so only a known zone has odds.
        std::optional<WeatherOdds> OddsOn(const BarState& state, uint32_t vanaDay)
        {
            const ZoneRecord* zone = KnownZone(state.zone);
            return zone != nullptr ? WeatherOn(*zone, vanaDay) : std::nullopt;
        }

        bool IsMob(const ActorInfo* target)
        {
            return target != nullptr && target->kind == EntityKind::Mob && target->mob != nullptr;
        }

        // An NPC target's record; nullptr for anything else, or an NPC the data has nothing to show of.
        const NpcRecord* NpcOf(const ActorInfo* target)
        {
            return target != nullptr && target->kind == EntityKind::Npc ? NpcById(target->serverId) : nullptr;
        }

        // "WAR 75 / NIN 37", or the game's "? ? ?" for an anonymous player, whose jobs come as 0.
        std::string CheckedJobs(const InspectGeneral& g)
        {
            return g.mainJob == 0 ? kAnonymousJobs : JobsText(g.mainJob, g.mainLevel, g.subJob, g.subLevel);
        }

        // The tracker fills a mob's level only while it lives; a dead target has none, but its record still names
        // its aggro or passive icon.
        bool HasLevel(const ActorInfo& target) { return target.level.text[0] != '\0'; }

        // Every icon of its nature: aggro first, then linking and how it detects.
        std::vector<Icon> MobIcons(const ActorInfo& target)
        {
            const IconSet icons = IconsFor(target.mob, kAllDetection);
            return std::vector<Icon>(icons.icons, icons.icons + icons.count);
        }

        std::optional<Icon> MobIcon(const ActorInfo& target)
        {
            const std::vector<Icon> icons = MobIcons(target);
            return icons.empty() ? std::nullopt : std::optional<Icon>(icons.front());
        }

        uint32_t NameColor(const ActorInfo& target, const Settings& settings) { return RowColor(target.state, settings); }

        // Which of the Target widget's Visibility a target is: a pet as a pet, whatever its entity is.
        TargetShow TargetShowOf(const ActorInfo* target)
        {
            if (target == nullptr) return TargetShow::NoTarget;
            switch (PlateKindOf(target->state))
            {
            case PlateKind::Mobs: return TargetShow::Mobs;
            case PlateKind::Pets: return TargetShow::Pets;
            case PlateKind::Npcs: return TargetShow::NpcsObjects;
            case PlateKind::Self:
            case PlateKind::Players: break;
            }
            return TargetShow::Players;
        }

        uint32_t ShadeColor(const ActorInfo& target, const Settings& settings)
        {
            return ToArgb(settings.palette.toughness[static_cast<int>(target.level.shade)]);
        }

        // "15y": a sense's range in yalms.
        std::string Yalms(int yalms) { return std::to_string(yalms) + "y"; }

        // The nameplate's icons as words, so the popup and the plates say the same, sight and sound with how far
        // they reach, then NM and whether a beastmaster can charm it.
        PopupRow Tags(const MobRecord& mob)
        {
            const IconSet icons = IconsFor(&mob, kAllDetection);
            const MobInfo& info = InfoOf(mob);
            PopupRow row{RowKind::Tags, 0, {}};
            for (int i = 0; i < icons.count; ++i)
            {
                const Icon icon = icons.icons[i];
                if (DetectionOf(icon) == DetectionIcon::Aggro) continue; // the icon before the popup's title says it
                std::string tag = IconTag(icon);
                if (icon == Icon::Sight || icon == Icon::TrueSight) tag += " " + Yalms(info.sight);
                if (icon == Icon::Sound) tag += " " + Yalms(info.sound);
                row.cells.push_back(Cell(Text(std::move(tag), Tone::Tag), icon));
            }
            if (mob.flags & kMobNotorious) row.cells.push_back(Cell(Text("NM", Tone::Tag)));
            if (mob.flags & kMobCharmable) row.cells.push_back(Cell(Text("Charmable", Tone::Tag)));
            return row;
        }

        // What it does by itself, and what its melee hits add.
        PopupRow Traits(const MobRecord& mob)
        {
            const MobInfo& info = InfoOf(mob);
            PopupRow row{RowKind::Tags, 0, {}};
            for (const auto& [trait, name] : kTraitNames)
                if (info.traits & trait) row.cells.push_back(Cell(Text(name, Tone::Tag)));
            if (*info.addEffect != '\0')
                row.cells.push_back(Cell(Text(std::string_view(info.addEffect) == kUnknownAddEffect
                                                  ? std::string("Additional Effect")
                                                  : std::string("Additional Effect: ") + info.addEffect,
                    Tone::Tag)));
            return row;
        }

        // "THF/BLM", or "WAR" for WAR/WAR; nothing for a mob without a job.
        std::string MobJobs(const MobInfo& info)
        {
            const std::string main = JobName(info.mainJob), sub = JobName(info.subJob);
            return main.empty() ? std::string() : sub.empty() || sub == main ? main : main + "/" + sub;
        }

        // A stat as two cells of a grid: its name against the right of its column, its number against the left of
        // the next.
        void AddNumbered(PopupRow& row, const char* name, int64_t value)
        {
            PopupCell said = Cell(Text(name, Tone::Dim));
            said.align     = Align::Right;
            row.cells.push_back(std::move(said));
            row.cells.push_back(Cell(Text(std::to_string(value))));
        }

        // A grid of names, kListColumns a line; none when there are no names.
        std::optional<PopupSection> NameList(std::string heading, const std::vector<std::string>& names)
        {
            if (names.empty()) return std::nullopt;
            PopupRow grid = Grid(kListColumns, names.size());
            for (const std::string& name : names)
                grid.cells.push_back(Cell(Text(name)));
            return PopupSection{std::move(heading), ListOf(std::move(grid))};
        }

        // Its TP moves, those a blue mage can learn in the blue magic color, and the ones you have learned starred or
        // plain as the settings say.
        std::optional<PopupSection> TpMoves(const MobRecord& mob, const BarState& state, const Settings& settings)
        {
            const std::span<const MobSkill> skills = SkillsOf(mob);
            if (skills.empty()) return std::nullopt;
            PopupRow grid = Grid(kListColumns, skills.size());
            for (const MobSkill& skill : skills)
            {
                const bool blue    = settings.blueMagic && skill.blueSpell != 0;
                const bool learned = blue && state.knowsSpell != nullptr && state.knowsSpell(skill.blueSpell);
                if (!blue || (learned && settings.blueMagicLearned == BlueLearned::Plain))
                {
                    grid.cells.push_back(Cell(Text(skill.name)));
                    continue;
                }
                PopupCell cell = Cell(Colored(skill.name, ToArgb(settings.blueMagicColor)));
                cell.star      = learned;
                grid.cells.push_back(std::move(cell));
            }
            return PopupSection{"TP Moves", ListOf(std::move(grid))};
        }

        // The spells it casts at a level: each whose levels hold it, by the game's name.
        std::vector<std::string> SpellsAt(const MobRecord& mob, int level, const BarState& state)
        {
            std::vector<std::string> names;
            if (state.spellName == nullptr) return names;
            for (const MobSpell& spell : SpellsOf(mob))
            {
                const char* name = level >= spell.minLevel && level <= spell.maxLevel ? state.spellName(spell.spell) : nullptr;
                if (name != nullptr && *name != '\0') names.emplace_back(name);
            }
            return names;
        }

        // "20:00"
        std::string Hour(int hour) { return Printed("%02d:00", hour); }

        // Phoenix's Treasure Hunter tiers in per mille: a rate rolls as the highest at or below it (mobdata.h).
        constexpr uint16_t kDropTiers[] = {1000, 240, 150, 100, 50, 10, 5, 1};
        constexpr double kPerMillePerPercent = 10.0;

        uint16_t Tier(uint16_t rate)
        {
            for (const uint16_t tier : kDropTiers)
                if (rate >= tier) return tier;
            return 0;
        }

        // "24%", "0.5%", "1.7%".
        std::string ChanceText(double perMille)
        {
            const double percent = perMille / kPerMillePerPercent;
            return percent == std::floor(percent) ? Printed("%.0f%%", percent) : Printed("%.1f%%", percent);
        }

        // What a mob drops at Treasure Hunter 0, and what can be stolen or despoiled from it: each by the game's
        // name, without the entries a group picks for nothing.
        std::vector<PopupSection> Loot(const MobRecord& mob, const BarState& state)
        {
            PopupSection drops{"Drops", {}}, steal{"Steal", {}}, despoil{"Despoil", {}};
            for (const MobDrop& d : DropsOf(mob))
            {
                const char* name = state.itemName != nullptr ? state.itemName(d.item) : nullptr;
                if (d.item == 0 || name == nullptr || *name == '\0') continue;
                switch (d.kind)
                {
                case DropKind::Drop:
                {
                    double chance = Tier(d.rate);
                    if (d.group != 0)
                    {
                        const MobDropGroup& g = DropGroupOf(mob, d.group);
                        chance = g.weight > 0 ? static_cast<double>(g.rate) * d.rate / g.weight : 0.0;
                    }
                    if (chance > 0.0) drops.rows.push_back(Labeled(name, Text(ChanceText(chance))));
                    break;
                }
                case DropKind::Steal: steal.rows.push_back(LineOf(name)); break;
                case DropKind::Despoil: despoil.rows.push_back(LineOf(name)); break;
                }
            }
            std::vector<PopupSection> loot;
            for (PopupSection* s : {&drops, &steal, &despoil})
                if (!s->rows.empty()) loot.push_back(std::move(*s));
            return loot;
        }

        PopupSection Spawn(const MobRecord& mob, const BarState& state)
        {
            PopupSection spawn{"Spawn", {}};
            const MobInfo& info = InfoOf(mob);
            spawn.rows.push_back(LineOf(mob.respawn > 0 ? "Respawn " + Countdown(mob.respawn) : "No Respawn Timer"));
            if (info.windowStart != kNoWindow)
                spawn.rows.push_back(LineOf("Spawns " + Hour(info.windowStart) + " to " + Hour(info.windowEnd)));
            if (info.lotteryChance > 0)
            {
                spawn.rows.push_back(LineOf(std::to_string(info.lotteryChance) + "% pop chance"));
                if (info.lotteryCooldown > 0)
                    spawn.rows.push_back(LineOf(Countdown(info.lotteryCooldown) + " cooldown after its kill"));
            }
            if (mob.placeholderOf != 0)
            {
                const MobRecord* nm = MobById(mob.placeholderOf);
                const int chance    = nm != nullptr ? InfoOf(*nm).lotteryChance : 0;
                spawn.rows.push_back(LineOf(std::string("Placeholder for ") + (nm != nullptr ? nm->name : "an NM") +
                    (chance > 0 ? " (" + std::to_string(chance) + "%)" : "")));
                if (state.targetPhSeconds)
                {
                    const double left = *state.targetPhSeconds;
                    spawn.rows.push_back(LineOf(left > 0.0 ? "Back in " + Countdown(left) : "Due Now", Tone::Weak));
                }
            }
            if (mob.placeholders > 0)
                spawn.rows.push_back(LineOf(std::to_string(mob.placeholders) +
                    (mob.placeholders == 1 ? " placeholder" : " placeholders") + " in this zone"));
            if (mob.placeholderOf == 0 && mob.placeholders == 0)
                spawn.rows.push_back(LineOf("Not an NM Placeholder", Tone::Dim));
            return spawn;
        }

        // Items by the game's names, gil as "Gil", without those it has no name for.
        std::vector<std::string> ItemNames(std::span<const uint16_t> items, const BarState& state)
        {
            std::vector<std::string> names;
            for (const uint16_t item : items)
            {
                const char* name = item == kGilItem              ? "Gil"
                                   : state.itemName != nullptr ? state.itemName(item)
                                                               : nullptr;
                if (name != nullptr && *name != '\0') names.emplace_back(name);
            }
            return names;
        }

        // The mobs it pops by Phoenix's names, each once.
        std::vector<std::string> MobNames(std::span<const uint32_t> mobs)
        {
            std::vector<std::string> names;
            for (const uint32_t id : mobs)
            {
                const MobRecord* mob = MobById(id);
                if (mob != nullptr && std::ranges::find(names, mob->name) == names.end()) names.emplace_back(mob->name);
            }
            return names;
        }

        std::vector<std::string> KeyItemNames(std::span<const uint16_t> keyItems)
        {
            std::vector<std::string> names;
            for (const uint16_t keyItem : keyItems)
                if (const char* name = KeyItemNameOf(keyItem)) names.emplace_back(name);
            return names;
        }

        constexpr size_t kNpcTextChars = 44; // a message's line in the popup
        // Before what an act leads to; a font without the arrow draws Montserrat's (IsFallbackFont).
        constexpr const char* kResult = "\xE2\x86\x92 ";

        std::string JoinedNames(const std::vector<std::string>& names)
        {
            std::string text;
            for (const std::string& name : names)
                text += (text.empty() ? "" : ", ") + name;
            return text;
        }

        // A message's words with its parameters in their <item>, <keyitem> and <number>, in order; other marks stay.
        std::string SayWords(const NpcSay& say, const BarState& state)
        {
            const char* words = WordsOf(say);
            if (words == nullptr) return {};
            const std::span<const uint32_t> params = ParamsOf(say);
            size_t next = 0;
            std::string out;
            for (std::string_view rest = words; !rest.empty();)
            {
                const size_t open  = rest.find('<');
                const size_t close = open == std::string_view::npos ? open : rest.find('>', open);
                if (close == std::string_view::npos)
                {
                    out += rest;
                    break;
                }
                out += rest.substr(0, open);
                const std::string_view tag = rest.substr(open + 1, close - open - 1);
                std::string filled(rest.substr(open, close - open + 1));
                if ((tag == "item" || tag == "keyitem" || tag == "number") && next < params.size())
                {
                    const uint32_t value = params[next++];
                    const char* name     = tag == "number"   ? nullptr
                                           : tag == "keyitem" ? KeyItemNameOf(static_cast<uint16_t>(value))
                                           : state.itemName != nullptr ? state.itemName(static_cast<uint16_t>(value))
                                                                       : nullptr;
                    if (tag == "number") filled = std::to_string(value);
                    else if (name != nullptr && *name != '\0') filled = name;
                }
                out += filled;
                rest = rest.substr(close + 1);
            }
            return out;
        }

        // "22:00", or "22:00-03:59" for a run of hours.
        std::string HourRange(int from, int to) { return from == to ? Hour(from) : Hour(from) + "-" + Printed("%02d:59", to); }

        // An act's conditions in words, its quest and step first: "In A Chocobos Tale, step 4, after its NM falls".
        std::string WhenWords(const NpcAct& act, bool capital = true)
        {
            std::vector<std::string> first, rest;
            for (const NpcWhenEntry& when : WhenOf(act))
            {
                const auto keyItem = [&] {
                    const char* name = KeyItemNameOf(when.a);
                    return std::string(name != nullptr ? name : "a key item");
                };
                switch (when.kind)
                {
                case kWhenHour: rest.push_back("at " + HourRange(when.a, when.b)); break;
                case kWhenWeather:
                    if (const std::optional<Weather> weather = WeatherFromId(when.a))
                        rest.push_back(std::string("in ") + WeatherName(*weather));
                    break;
                case kWhenJob: rest.push_back(std::string("as ") + JobName(static_cast<uint8_t>(when.a))); break;
                case kWhenDay: rest.push_back(std::string("on ") + WeekdayName(when.a)); break;
                case kWhenNation:
                    rest.push_back(std::string("for ") + NationName(static_cast<Nation>(when.a + 1)) + "'s citizens");
                    break;
                case kWhenKeyItem: rest.push_back("holding " + keyItem()); break;
                case kWhenNoKeyItem: rest.push_back("without " + keyItem()); break;
                case kWhenKeyItems: rest.push_back("holding its key items"); break;
                case kWhenQuest: rest.push_back("during its quest"); break;
                case kWhenLink:
                    if (const NpcLink* link = LinkAt(when.a)) first.insert(first.begin(), std::string("in ") + link->name);
                    break;
                case kWhenStep: first.push_back("step " + std::to_string(when.a)); break;
                case kWhenPrimed: rest.push_back("once readied"); break;
                case kWhenNmKilled: rest.push_back("after its NM falls"); break;
                case kWhenRoll: rest.push_back("on a lucky roll"); break;
                case kWhenMob: rest.push_back(std::string("when ") + when.name + " dies"); break;
                case kWhenZoneStart: rest.push_back("as the zone starts"); break;
                default: break;
                }
            }
            first.insert(first.end(), rest.begin(), rest.end());
            std::string text = JoinedNames(first);
            if (capital && !text.empty()) text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
            return text;
        }

        // An act's rows: its conditions, dim, what it says, then what follows it.
        void AddActRows(PopupSection& section, const NpcAct& act, const BarState& state)
        {
            if (const std::string when = WhenWords(act); !when.empty()) section.rows.push_back(LineOf(when, Tone::Dim));
            for (const NpcSay& say : SaysOf(act))
                for (const std::string& line : WrapWords(SayWords(say, state), kNpcTextChars))
                    section.rows.push_back(LineOf(line));
            if (const std::vector<std::string> pops = MobNames(PopsOf(act)); !pops.empty())
                section.rows.push_back(LineOf(kResult + std::string("Pops ") + JoinedNames(pops)));
            if (const std::vector<std::string> gives = ItemNames(GivesOf(act), state); !gives.empty())
                section.rows.push_back(LineOf(kResult + std::string("Gives ") + JoinedNames(gives)));
            if (const std::vector<std::string> keyItems = KeyItemNames(KeyItemsOf(act)); !keyItems.empty())
                section.rows.push_back(LineOf(kResult + std::string("Gives key item ") + JoinedNames(keyItems)));
            if (act.event) section.rows.push_back(LineOf(kResult + std::string("A cutscene"), Tone::Dim));
        }

        // What clicking it does, and what each trade the probe found does, its items first; none when the probe saw
        // nothing. A trade the probe found takes the place of the plain list of what its scripts check for.
        bool AddActs(std::vector<PopupSection>& sections, const NpcRecord& npc, const BarState& state)
        {
            PopupSection click{"Click", {}}, trade{"Trade", {}};
            for (const NpcAct& act : ActsOf(npc))
            {
                if (act.kind == kActClick) AddActRows(click, act, state);
                if (act.kind != kActTrade) continue;
                std::vector<std::string> items;
                for (const NpcActItem& entry : ItemsOf(act))
                {
                    const std::vector<std::string> named = ItemNames(std::span<const uint16_t>(&entry.item, 1), state);
                    const std::string name = named.empty() ? "an item" : named.front();
                    items.push_back(entry.qty > 1 ? std::to_string(entry.qty) + " x " + name : name);
                }
                std::string line;
                for (const std::string& item : items)
                    line += (line.empty() ? "" : " + ") + item;
                trade.rows.push_back(LineOf(line));
                AddActRows(trade, act, state);
            }
            if (!click.rows.empty()) sections.push_back(std::move(click));
            const bool traded = !trade.rows.empty();
            if (traded) sections.push_back(std::move(trade));
            return traded;
        }

        // When its zone shows and hides it: "Appears at 22:00, gone at 07:00"; empty for a ??? always there.
        std::string AppearsWords(const NpcRecord& npc)
        {
            std::vector<std::string> shows, hides;
            for (const NpcAct& act : ActsOf(npc))
            {
                const auto when = WhenOf(act);
                if (when.size() == 1 && when[0].kind == kWhenZoneStart) continue;
                if (act.kind == kActShows) shows.push_back(WhenWords(act, false));
                if (act.kind == kActHides) hides.push_back("gone " + WhenWords(act, false));
            }
            if (shows.empty()) return {};
            shows.insert(shows.end(), hides.begin(), hides.end());
            return "Appears " + JoinedNames(shows);
        }

        // The dim note under a list when an NPC's scripts do more of it than the data could read.
        PopupRow UnreadNote(bool listed)
        {
            return LineOf(listed ? "And more its script works out" : "Worked out by its script", Tone::Dim);
        }

        // A name a line, and a dim note when its scripts do more of it than the data could read; none when there is
        // neither.
        void AddListing(std::vector<PopupSection>& sections, std::string heading, const std::vector<std::string>& names,
            bool unread = false)
        {
            if (names.empty() && !unread) return;
            PopupSection section{std::move(heading), {}};
            for (const std::string& name : names)
                section.rows.push_back(LineOf(name));
            if (unread) section.rows.push_back(UnreadNote(!names.empty()));
            sections.push_back(std::move(section));
        }

        // What a shop always stocks, then what a nation's shop holds back until the nation places well in conquest,
        // under the place it needs (NpcStockItem's ranks).
        constexpr std::pair<uint8_t, const char*> kStockPlaces[] = {
            {2, "When its nation is 1st or 2nd in conquest"}, {1, "When its nation is 1st in conquest, to its citizens"}};

        void AddShop(std::vector<PopupSection>& sections, const NpcRecord& npc, const BarState& state)
        {
            const NpcShop* shop  = ShopOf(npc);
            const bool unread    = (npc.unread & kNpcUnreadShop) != 0;
            PopupSection section = {"Shop", {}};
            const auto add = [&](uint8_t place, const char* note) {
                bool first = true;
                for (const NpcStockItem& entry : shop != nullptr ? StockOf(*shop) : std::span<const NpcStockItem>())
                {
                    const uint8_t at  = entry.rank == 1 || entry.rank == 2 ? entry.rank : 0;
                    const char* name = state.itemName != nullptr ? state.itemName(entry.item) : nullptr;
                    if (at != place || name == nullptr || *name == '\0') continue;
                    if (first && note != nullptr) section.rows.push_back(LineOf(note, Tone::Dim));
                    first = false;
                    // The name in the normal tone: the dim lines naming a place are the headings among them.
                    section.rows.push_back(Labeled(Text(name), Text(Grouped(entry.price) + " gil")));
                }
            };
            add(0, nullptr);
            for (const auto& [place, note] : kStockPlaces)
                add(place, note);
            if (section.rows.empty() && !unread) return;
            if (!section.rows.empty() && shop->fame)
                section.rows.push_back(LineOf("Prices before fame, which moves them up to 10% either way", Tone::Dim));
            if (unread) section.rows.push_back(UnreadNote(!section.rows.empty()));
            sections.push_back(std::move(section));
        }

        // Its quests (hidden ones too), missions and battlefields, each by name, a quest's and mission's log after it.
        void AddLinks(std::vector<PopupSection>& sections, const NpcRecord& npc)
        {
            PopupSection quests{"Quests", {}}, missions{"Missions", {}}, battlefields{"Battlefields", {}};
            for (const NpcLink* link : LinksOf(npc))
            {
                const char* log = LogNameOf(*link);
                switch (link->kind)
                {
                case kLinkQuest:
                case kLinkHidden:
                    quests.rows.push_back(Labeled(Text(link->name), Text(log != nullptr ? log : "Hidden", Tone::Dim)));
                    break;
                case kLinkMission:
                    missions.rows.push_back(Labeled(Text(link->name), Text(log != nullptr ? log : "", Tone::Dim)));
                    break;
                default: battlefields.rows.push_back(LineOf(link->name)); break;
                }
            }
            for (PopupSection* s : {&quests, &missions, &battlefields})
                if (!s->rows.empty()) sections.push_back(std::move(*s));
        }

        // What it is for, its shop, what it takes in trade and what that pops, what it gives, the key items it gives
        // and checks for, and its quests, missions and battlefields.
        std::vector<PopupSection> NpcSections(const ActorInfo& target, const NpcRecord& npc, const BarState& state)
        {
            std::vector<PopupSection> sections = ListOf(PopupSection{"",
                ListOf(Line(Cell(Text("ID " + Printed("%03X", target.serverId & kTargetIndexBits), Tone::Dim))))});
            // What it is for, as a mob's tags say what it does.
            PopupRow purposes{RowKind::Tags, 0, {}};
            for (const char* purpose : PurposesOf(npc))
                purposes.cells.push_back(Cell(Text(purpose, Tone::Tag)));
            if (!purposes.cells.empty()) sections[0].rows.push_back(std::move(purposes));
            if (const std::string appears = AppearsWords(npc); !appears.empty()) sections[0].rows.push_back(LineOf(appears));
            AddShop(sections, npc, state);
            if (!AddActs(sections, npc, state))
                AddListing(sections, "Trade", ItemNames(TradesOf(npc), state), (npc.unread & kNpcUnreadTrade) != 0);
            AddListing(sections, "Pops", MobNames(PopsOf(npc)), (npc.unread & kNpcUnreadPops) != 0);
            AddListing(sections, "Gives", ItemNames(GivesOf(npc), state), (npc.unread & kNpcUnreadGives) != 0);
            AddListing(sections, "Key Items", KeyItemNames(KeyItemsOf(npc)));
            AddListing(sections, "Checks for", KeyItemNames(NeedsOf(npc)));
            AddLinks(sections, npc);
            return sections;
        }

        PopupContent TargetPopup(const BarState& state, const Settings& settings)
        {
            PopupContent popup;
            const ActorInfo* target = state.target;
            if (target == nullptr)
            {
                popup.title = ListOf(Text("No Target", Tone::Dim));
                return popup;
            }
            if (!IsMob(target))
            {
                popup.title = ListOf(Text(target->name));
                if (target->kind == EntityKind::Player && state.targetCheck)
                {
                    const PlayerCheck& c = *state.targetCheck;
                    PopupSection rows;
                    rows.rows.push_back(Labeled("Jobs", Text(CheckedJobs(c.general))));
                    if (c.general.masterLevel > 0)
                        rows.rows.push_back(Labeled("Master Level", Text(std::to_string(c.general.masterLevel))));
                    if (!c.general.linkshell.empty())
                        rows.rows.push_back(Labeled("Linkshell", Text(c.general.linkshell)));
                    if (const std::string title = TitleName(c.message.title); !title.empty())
                        rows.rows.push_back(Labeled("Title", Text(title)));
                    else if (c.message.title != 0)
                        rows.rows.push_back(Labeled("Title", Text("#" + std::to_string(c.message.title))));
                    rows.rows.push_back(LineOf("As of Your Check", Tone::Dim));
                    popup.sections = ListOf(std::move(rows));
                }
                else if (const NpcRecord* npc = NpcOf(target))
                    popup.sections = NpcSections(*target, *npc, state);
                return popup;
            }
            const MobRecord& mob      = *target->mob;
            const MobResistances& res = ResistancesOf(mob);
            popup.icon  = MobIcon(*target);
            popup.title = ListOf(Colored(target->name, NameColor(*target, settings)),
                Text(" ID " + Printed("%03X", target->serverId & kTargetIndexBits), Tone::Dim));
            const MobDefenses& defenses = DefensesOf(mob);
            // "Level 17-20 WAR (Easy Prey to Even Match)", the level and con in the con's color.
            PopupRow identity{RowKind::Line, 0, {}};
            const uint32_t shade = ShadeColor(*target, settings);
            if (HasLevel(*target)) identity.cells.push_back(Cell(Colored("Level " + std::string(target->level.level), shade)));
            if (const std::string jobs = MobJobs(InfoOf(mob)); !jobs.empty()) identity.cells.push_back(Cell(Text(jobs)));
            if (HasLevel(*target) && target->level.con[0] != '\0')
                identity.cells.push_back(Cell(Colored("(" + std::string(target->level.con) + ")", shade)));

            PopupRow damage = Grid(kDamageColumns, kFirstElementSdt);
            for (int i = 0; i < kFirstElementSdt; ++i)
                damage.cells.push_back(Cell(Percent(SdtPercent(res.sdt[i])), SdtIcon(i)));
            PopupSection damageTaken{"Damage Taken", ListOf(std::move(damage))};
            PopupRow kinds = Grid(kDamageColumns, kDamageKindCount);
            for (int k = 0; k < kDamageKindCount; ++k)
                if (const int percent = SdtPercent(defenses.damageTaken[k]); percent != 0)
                    kinds.cells.push_back(Cell(ListOf(Text(kDamageKindNames[k], Tone::Dim), Text(" "), Percent(percent))));
            if (!kinds.cells.empty()) damageTaken.rows.push_back(std::move(kinds));

            const RankNumbers numbers{settings.rankDisplay, settings.countAccuracy, state.caster ? &*state.caster : nullptr,
                MagicTargetOf(mob, target->checked ? target->checked->level : 0)};
            PopupRow elements = Grid(kElementColumns, kElementCount);
            for (int e = 0; e < kElementCount; ++e)
            {
                const int percent = SdtPercent(res.sdt[kFirstElementSdt + e]);
                const int rank    = res.elementRanks[e];
                const int absorb  = defenses.absorb[e];
                elements.cells.push_back(Cell(absorb > 0 ? Text(absorb >= kWholeAbsorb ? std::string("Absorbs")
                                                                                       : "Absorbs " + std::to_string(absorb) + "%",
                                                               Tone::Immune)
                                              : percent != 0
                                                  ? Percent(percent)
                                                  : Ranked(ElementRankText(static_cast<Element>(e), rank, numbers), rank),
                    SdtIcon(kFirstElementSdt + e)));
            }

            PopupRow statuses = Grid(kStatusColumns, kStatusRankCount);
            for (int s = 0; s < kStatusRankCount; ++s)
            {
                const bool immuneTo = (res.immunity & StatusImmunity(s)) != 0;
                const int rank      = res.statusRanks[s];
                statuses.cells.push_back(Cell(immuneTo ? Text(kTimesSign, Tone::Immune)
                                                       : Ranked(StatusRankText(static_cast<StatusRank>(s), rank, numbers), rank),
                    StatusIcon(s)));
            }

            PopupRow immune{RowKind::Line, 0, {}};
            const InfoIconSet immunities = InfoIconsOf(InfoIconsFor(&res, BadgeRules{}), BadgeGroup::Immune);
            for (int i = 0; i < immunities.count; ++i)
                immune.cells.push_back(PopupCell{immunities.icons[i].icon, true, {}});
            if (immune.cells.empty()) immune.cells.push_back(Cell(Text("None", Tone::Neutral)));

            PopupSection identityRows{"", ListOf(std::move(identity), Tags(mob))};
            if (PopupRow traits = Traits(mob); !traits.cells.empty()) identityRows.rows.push_back(std::move(traits));
            popup.sections = ListOf(std::move(identityRows), std::move(damageTaken),
                PopupSection{"Elements", ListOf(std::move(elements))},
                PopupSection{"Statuses", ListOf(std::move(statuses))},
                PopupSection{"Immune", ListOf(std::move(immune))});
            // At the level the chances are figured for: the check's, else the top of its range.
            const int level = numbers.target ? numbers.target->level : mob.maxLevel;
            if (numbers.target)
            {
                PopupSection stats{"Stats at Level " + std::to_string(level), {}};
                if (const MobCombat* combat = CombatAt(mob, level))
                {
                    PopupRow row = Grid(2 * kStatColumns, 2 * kCombatCount);
                    for (int i = 0; i < kCombatCount; ++i)
                        AddNumbered(row, kCombatNames[i], combat->value[i]);
                    stats.rows.push_back(std::move(row));
                }
                PopupRow row = Grid(2 * kStatColumns, 2 * kStatCount);
                for (int i = 0; i < kStatCount; ++i)
                    AddNumbered(row, kStatNames[i], numbers.target->stats.value[i]);
                stats.rows.push_back(std::move(row));
                popup.sections.push_back(std::move(stats));
            }
            if (std::optional<PopupSection> moves = TpMoves(mob, state, settings)) popup.sections.push_back(std::move(*moves));
            if (std::optional<PopupSection> list = NameList("Spells", SpellsAt(mob, level, state)))
                popup.sections.push_back(std::move(*list));
            popup.sections.push_back(Spawn(mob, state));
            for (PopupSection& loot : Loot(mob, state))
                popup.sections.push_back(std::move(loot));
            return popup;
        }

        Span WeekdayText(const VanaTime& t)
        {
            return Colored(WeekdayName(t.weekday), ElementColor(WeekdayElement(t.weekday)));
        }

        std::string VanaClock(int64_t unixMs)
        {
            const VanaTime t = VanaAt(unixMs);
            return HourMinute(t.hour, t.minute);
        }
        std::string MoonText(const VanaTime& t) { return MoonPhaseName(t.moonPercent, t.moonDirection); }

        std::string VanaDate(const VanaTime& t)
        {
            return std::to_string(t.year) + "/" + Printed("%02d", t.month) + "/" + Printed("%02d", t.dayOfMonth);
        }

        bool InConquest(const ZoneRecord* zone) { return zone != nullptr && zone->region < kConquestRegionCount; }

        // The crest the game's conquest menus show for a nation.
        GamePicture NationCrest(Nation nation)
        {
            switch (nation)
            {
            case Nation::SanDoria: return GamePicture::SanDoriaCrest;
            case Nation::Bastok: return GamePicture::BastokCrest;
            case Nation::Windurst: return GamePicture::WindurstCrest;
            case Nation::Beastmen: return GamePicture::BeastmenCrest;
            default: return GamePicture::None;
            }
        }


        // Who holds the zone's region; no one before a conquest update or outside the conquest regions.
        Nation OwnerOf(const ZoneRecord* zone, const BarState& state)
        {
            return InConquest(zone) && state.owners ? (*state.owners)[zone->region] : Nation::None;
        }

        // By Weather: HeadsUp's own picture of each.
        constexpr Icon kCustomWeather[kWeatherCount] = {Icon::WeatherFair, Icon::WeatherSunny, Icon::WeatherCloudy,
            Icon::WeatherFoggy, Icon::WeatherHeatSpell, Icon::WeatherHeatSpell, Icon::WeatherRain, Icon::WeatherSqualls,
            Icon::WeatherDustStorm, Icon::WeatherDustStorm, Icon::WeatherWindy, Icon::WeatherGales, Icon::WeatherSnow,
            Icon::WeatherBlizzard, Icon::WeatherThunder, Icon::WeatherThunderstorms, Icon::WeatherAurora,
            Icon::WeatherAurora, Icon::WeatherGloom, Icon::WeatherGloom};

        std::optional<Icon> WeatherIcon(Weather weather)
        {
            const std::optional<Element> element = WeatherElement(weather);
            if (!element) return std::nullopt;
            return SdtIcon(kFirstElementSdt + static_cast<int>(*element));
        }

        // Words and the time after them as spans of their own, so only the time moves as it ticks.
        void AddTime(std::vector<Span>& spans, std::string words, double seconds)
        {
            spans.push_back(Text(std::move(words)));
            spans.push_back(Text(Countdown(seconds)));
        }

        // A ship's line: boarding until it leaves, or when its door opens.
        std::vector<Span> SailingText(const TravelRoute& route, int64_t unixMs)
        {
            const Sailing next = NextSailing(route, unixMs);
            std::vector<Span> spans;
            if (next.boarding)
            {
                spans = ListOf(Text("Boarding"), Text(kDot, Tone::Dim));
                AddTime(spans, "leaves in ", next.leaveIn);
            }
            else
                AddTime(spans, "Boards in ", next.boardIn);
            return spans;
        }

        std::vector<Span> GuildText(const GuildShop& shop, int64_t unixMs)
        {
            const GuildHours hours = GuildAt(shop, unixMs);
            const char* now        = hours.open ? "Open" : hours.holiday ? "Holiday" : "Closed";
            std::vector<Span> spans = ListOf(Text(now, hours.open ? Tone::Normal : Tone::Dim), Text(kDot, Tone::Dim));
            AddTime(spans, hours.open ? "closes in " : "opens in ", hours.changeIn);
            return spans;
        }

        // How many regions each picked nation holds: a nation alone by its name; several by their crests, each count
        // in its color, two to a line in columns.
        constexpr const char* kFavoritePress = "favorite "; // a currency's star tells this, then the currency's key

        std::optional<int> NationPoints(Nation nation)
        {
            if (nation < Nation::SanDoria || nation > Nation::Windurst) return std::nullopt;
            return kConquestPointsFirst + static_cast<int>(nation) - static_cast<int>(Nation::SanDoria);
        }

        // The favorites as currencies, each once; your nation's conquest points once your nation is known.
        std::vector<int> FavoriteCurrencies(const std::vector<std::string>& keys, Nation nation)
        {
            std::vector<int> out;
            for (const std::string& key : keys)
                if (const std::optional<int> c = key == kYourConquestPoints ? NationPoints(nation) : CurrencyOfKey(key);
                    c && std::ranges::find(out, *c) == out.end())
                    out.push_back(*c);
            return out;
        }

        // The parts of PerfPart that are the connection's.
        constexpr uint8_t kConnectionParts = PerfBit(PerfPart::Sent) | PerfBit(PerfPart::Received) |
                                             PerfBit(PerfPart::Percent) | PerfBit(PerfPart::Ping) |
                                             PerfBit(PerfPart::Messages);

        Span Mark(GamePicture picture) { return Span{"", Tone::Normal, 0, picture}; }
        Span Mark(Icon icon)
        {
            Span s = Text("");
            s.icon = icon;
            return s;
        }
        std::vector<Span> Marks(const Icon* icons, int count)
        {
            std::vector<Span> spans;
            for (int i = 0; i < count; ++i)
                spans.push_back(Mark(icons[i]));
            return spans;
        }

        // Each PerfPart's widget part.
        constexpr WidgetPart kPerfWidgetParts[kPerfPartCount] = {WidgetPart::PerfFps, WidgetPart::PerfSent,
            WidgetPart::PerfReceived, WidgetPart::PerfPercent, WidgetPart::PerfPing, WidgetPart::PerfMemory,
            WidgetPart::PerfMessages};

        // The frame rate, each of the connection's figures and the memory.
        std::vector<PartContent> PerformanceParts(const BarState& state, WidgetContent& base)
        {
            base.hasPopup = true;
            base.figures  = true;
            std::vector<PartContent> parts{{WidgetPart::PerfFps, FpsFigures(state.frames)}};
            for (int p = 0; p < kPerfPartCount; ++p)
                if (const uint8_t bit = PerfBit(static_cast<PerfPart>(p)); (bit & kConnectionParts) != 0 && state.connection)
                    parts.push_back({kPerfWidgetParts[p], ConnectionFigures(state.connection, bit)});
            std::vector<Span> memory = ListOf(Text("Mem ", Tone::Dim));
            for (Span& span : MemoryFigures(state.memory))
                memory.push_back(std::move(span));
            parts.push_back({WidgetPart::PerfMemory, std::move(memory)});
            return parts;
        }

        PopupContent PerformancePopup(const BarState& state)
        {
            PopupContent popup;
            popup.title    = ListOf(Text("Performance"));
            popup.sections = ConnectionSections(state.connection);
            for (PopupSection& section : MemorySections(state.memory, state.cpuPercent, state.frames, state.linuxxi))
                popup.sections.push_back(std::move(section));
            return popup;
        }

        // What World Info can tell, a part each: the Tracked Ship's next sailing, the Tracked Guild's hours, the RSE and
        // the moon, each its name and then its countdown, and each nation's crest and how many regions it holds.
        std::vector<PartContent> WorldParts(const BarState& state, const WidgetOptions& o, WidgetContent& base)
        {
            base.hasPopup = true;
            const auto told = [](std::vector<Span> name, std::vector<Span> details) {
                name.push_back(Text("  "));
                for (Span& span : details)
                {
                    span.tone = Tone::Dim;
                    name.push_back(std::move(span));
                }
                return name;
            };
            const TravelRoute& route = TravelRoutes()[static_cast<size_t>(o.worldRoute)];
            const GuildShop& shop    = GuildShops()[static_cast<size_t>(o.worldGuild)];
            const Rse rse            = RseAt(state.unixMs);
            const VanaTime t         = VanaAt(state.unixMs);
            const uint32_t full      = NextMoonDay(t.day, true), fresh = NextMoonDay(t.day, false);
            const bool fullFirst     = full < fresh;
            std::vector<PartContent> parts{
                {WidgetPart::WorldShip, told(ListOf(Text(route.name)), SailingText(route, state.unixMs))},
                {WidgetPart::WorldGuild, told(ListOf(Text(shop.guild)), GuildText(shop, state.unixMs))},
                {WidgetPart::WorldRse, told(ListOf(Text(rse.race)),
                    ListOf(Text(rse.place), Text(kDot), Text("ends in " + Countdown(rse.endsIn))))},
                {WidgetPart::WorldMoon, told(ListOf(Text(std::string(MoonPhaseName(t.moonPercent, t.moonDirection)) + " " +
                                                     std::to_string(t.moonPercent) + "%")),
                    ListOf(Text(std::string(fullFirst ? "Full" : "New") + " moon in " +
                                Countdown(SecondsToDay(state.unixMs, fullFirst ? full : fresh)))))}};
            for (int n = 0; n < kContenderNations; ++n)
            {
                const auto nation = static_cast<Nation>(static_cast<int>(Nation::SanDoria) + n);
                const auto part   = static_cast<WidgetPart>(static_cast<int>(WidgetPart::WorldSanDoria) + n);
                std::vector<Span> count;
                if (state.owners)
                {
                    Span held    = Colored(" " + std::to_string(std::ranges::count(*state.owners, nation)), NationColor(nation));
                    held.picture = NationCrest(nation);
                    count.push_back(std::move(held));
                }
                parts.push_back({part, std::move(count)});
            }
            return parts;
        }

        size_t ScrolledBy(const BarState& state, const std::string& key)
        {
            if (state.scrolled == nullptr) return 0;
            const auto scrolled = state.scrolled->find(key);
            return scrolled == state.scrolled->end() ? 0 : scrolled->second;
        }

        // A socket's first row: the key its scrolling is kept by, and how far it is scrolled.
        void ScrollsBy(PopupRow& first, const BarState& state, std::string key)
        {
            first.scrolled      = ScrolledBy(state, key);
            first.scroll        = std::move(key);
        }

        // The tab the player picked last in the card, or its first.
        size_t Picked(const BarState& state, const PopupSection& section, size_t fallback = 0)
        {
            if (state.pickedTabs == nullptr) return fallback;
            const auto picked = state.pickedTabs->find(std::string(CardKey(section.heading)));
            if (picked == state.pickedTabs->end()) return fallback;
            const auto tab = std::ranges::find(section.tabs, picked->second, [](const PopupTab& t) { return t.name.text; });
            return tab == section.tabs.end() ? fallback : static_cast<size_t>(tab - section.tabs.begin());
        }

        // A zone's weathers of an element on a day, likeliest first, after the likeliest one's icon; a dash for none.
        PopupCell RollCell(const ZoneRoll* roll)
        {
            if (roll == nullptr) return Cell(Text("-", Tone::Dim));
            std::vector<Span> text;
            for (const WeatherChance& chance : roll->weathers)
            {
                if (!text.empty()) text.push_back(Text(kDot, Tone::Dim));
                text.push_back(Text(std::string(WeatherName(chance.weather)) + " " + std::to_string(chance.percent) + "%"));
            }
            return Cell(std::move(text), WeatherIcon(roll->weathers.front().weather));
        }

        // A tab for each element: how many zones can roll it today and tomorrow, and which ones and how likely, today's
        // likeliest first, then tomorrow's. The server tells only your own zone's weather.
        PopupSection ByElement(const BarState& state, uint32_t today)
        {
            PopupSection section{"By Element", {}};
            // Each element's zones rolling it today and tomorrow, which change only with the day.
            using Rolls = std::array<std::pair<std::vector<ZoneRoll>, std::vector<ZoneRoll>>, kElementCount>;
            static Memo<uint32_t, Rolls> days;
            const Rolls& rolls = days.Get(today, [](uint32_t day) {
                Rolls out;
                for (int e = 0; e < kElementCount; ++e)
                    out[e] = {ZonesRolling(static_cast<Element>(e), day), ZonesRolling(static_cast<Element>(e), day + 1)};
                return out;
            });
            for (int e = 0; e < kElementCount; ++e)
            {
                const auto element = static_cast<Element>(e);
                const std::vector<ZoneRoll>& now = rolls[e].first, &next = rolls[e].second;
                PopupTab tab{Colored(ElementName(element), ElementColor(element)), {}, SdtIcon(kFirstElementSdt + e)};
                std::map<uint16_t, std::pair<const ZoneRoll*, const ZoneRoll*>> zones;
                for (const ZoneRoll& roll : now)
                    zones[roll.zone].first = &roll;
                for (const ZoneRoll& roll : next)
                    zones[roll.zone].second = &roll;
                std::vector<std::pair<uint16_t, std::pair<const ZoneRoll*, const ZoneRoll*>>> order(zones.begin(), zones.end());
                const auto percent = [](const ZoneRoll* roll) { return roll == nullptr ? 0 : roll->percent; };
                std::ranges::stable_sort(order, [&](const auto& a, const auto& b) {
                    return std::pair{percent(a.second.first), percent(a.second.second)} >
                           std::pair{percent(b.second.first), percent(b.second.second)};
                });
                PopupRow grid = Grid(kZoneRollColumns, (order.size() + 1) * kZoneRollColumns);
                grid.nested   = true;
                grid.header   = true;
                ScrollsBy(grid, state, section.heading + "/" + ElementName(element));
                // Each head counts its column: the zones listed, then those that can roll it today and tomorrow.
                for (const auto& [heading, count] : {std::pair{"Zone", order.size()}, {"Today", now.size()}, {"Tomorrow", next.size()}})
                    grid.cells.push_back(Cell(Text(std::string(heading) + " (" + std::to_string(count) + ")", Tone::Dim)));
                for (const auto& [zone, rolls] : order)
                {
                    grid.cells.push_back(Cell(Text(ZoneById(zone)->name)));
                    grid.cells.push_back(RollCell(rolls.first));
                    grid.cells.push_back(RollCell(rolls.second));
                }
                tab.rows.push_back(std::move(grid));
                section.tabs.push_back(std::move(tab));
            }
            section.tab = Picked(state, section);
            return section;
        }

        // A rank's item and its high quality: the one Phoenix's recipes make at high quality, or for one they do not
        // name, the +1, or else the one worth more, or else the second.
        struct RankTurnIn
        {
            const GuildTurnIn* item = nullptr;
            const GuildTurnIn* high = nullptr;
        };
        std::vector<RankTurnIn> ByRank(const std::vector<GuildTurnIn>& turnIns)
        {
            const auto plusOne = [](const GuildTurnIn& t) { return t.itemName.ends_with(" +1"); };
            std::vector<RankTurnIn> ranks;
            for (const GuildTurnIn& turnIn : turnIns)
            {
                const auto same = std::ranges::find_if(ranks, [&](const RankTurnIn& r) { return r.item->rank == turnIn.rank; });
                if (same == ranks.end())
                    ranks.push_back(RankTurnIn{&turnIn});
                else if (same->high == nullptr)
                {
                    const bool known = HighQualityTurnIn(turnIn.item) || HighQualityTurnIn(same->item->item);
                    const bool high  = known ? HighQualityTurnIn(turnIn.item)
                                             : plusOne(turnIn) || (!plusOne(*same->item) && turnIn.points >= same->item->points);
                    if (high)
                        same->high = &turnIn;
                    else
                    {
                        same->high = same->item;
                        same->item = &turnIn;
                    }
                }
            }
            return ranks;
        }

        // A craft guild's turn-ins today, from Phoenix's site, under a line of their own: the item each rank from Novice
        // to Expert is asked for today and its high quality, each with its picture and its points, and the most a day,
        // your rank starred and lit; until the site answers, a line saying so.
        std::vector<PopupRow> TurnIns(const BarState& state, int craft, const std::string& guild)
        {
            std::vector<PopupRow> rows = ListOf(LineOf("Today's Turn-Ins", Tone::Heading));
            if (state.turnIns == nullptr)
            {
                PopupRow waiting = LineOf(state.turnInsFailed ? "phoenix-xi.com did not answer; trying again soon"
                                                              : "They come from phoenix-xi.com",
                    Tone::Dim);
                waiting.nested = true;
                rows.push_back(std::move(waiting));
                return rows;
            }
            const std::vector<RankTurnIn> ranks = ByRank(state.turnIns->guilds[static_cast<size_t>(craft)]);
            PopupRow grid = Grid(kTurnInColumns, (kExpert - kNovice + 2) * kTurnInColumns);
            grid.nested   = true;
            grid.header   = true;
            grid.roomy    = true;
            ScrollsBy(grid, state, "Guilds/" + guild + "/Turn-ins");
            for (const char* heading : {"Rank", "NQ Item", "Points", "HQ Item", "Points", "Max Points"})
                grid.cells.push_back(Cell(Text(heading, Tone::Dim)));
            const auto figure = [](int value) { return Grouped(static_cast<uint64_t>(std::max(value, 0))); };
            for (uint8_t craftRank = kNovice; craftRank <= kExpert; ++craftRank)
            {
                const uint8_t asked = AskedRank(craftRank, state.unixMs);
                const auto rank = std::ranges::find_if(ranks, [&](const RankTurnIn& r) { return r.item->rank == asked; });
                if (rank == ranks.end()) continue;
                const GuildTurnIn& item = *rank->item;
                const GuildTurnIn* high = rank->high;
                const bool yours =
                    state.crafts && std::clamp((*state.crafts)[static_cast<size_t>(craft)], kNovice, kExpert) == craftRank;
                // An item with its picture; a rank without a high quality has its cells empty.
                const auto named = [&](const GuildTurnIn* t) {
                    if (t == nullptr) return PopupCell{};
                    PopupCell cell = Cell(Text(t->itemName.empty() ? "Item " + std::to_string(t->item) : t->itemName,
                        yours ? Tone::Heading : Tone::Normal));
                    cell.item = t->item;
                    return cell;
                };
                PopupCell level = Cell(Text(kCraftRankNames[craftRank], yours ? Tone::Normal : Tone::Dim));
                level.star      = yours;
                grid.cells.push_back(std::move(level));
                grid.cells.push_back(named(&item));
                grid.cells.push_back(Cell(Text(figure(item.points))));
                grid.cells.push_back(named(high));
                grid.cells.push_back(high ? Cell(Text(figure(high->points))) : PopupCell{});
                grid.cells.push_back(Cell(Text(figure(item.maxPoints) +
                                                   (high && high->maxPoints != item.maxPoints ? " [" + figure(high->maxPoints) + "]" : ""),
                    Tone::Dim)));
            }
            rows.push_back(std::move(grid));
            return rows;
        }

        // The favorites at the top, then every currency in a scrolling list sorted as its tab says, each with a star
        // that adds it to the favorites or takes it off.
        PopupContent CurrencyPopup(const BarState& state, const Settings& settings)
        {
            PopupContent popup;
            popup.title = ListOf(Text("Currencies"));
            const auto amount = [&](int c) { return c == kGilCurrency ? state.gil : state.currencies[c]; };
            const std::vector<int> favorites = FavoriteCurrencies(settings.currencyFavorites, state.nation.value_or(Nation::None));
            const auto add = [&](PopupRow& grid, int c) {
                const bool favorite = std::ranges::find(favorites, c) != favorites.end();
                PopupCell star;
                star.icon  = favorite ? Icon::Star : Icon::StarEmpty;
                star.tint  = favorite ? Colored("", kStarColor) : Text("", Tone::Dim);
                star.press = kFavoritePress + std::string(kCurrencyTable[c].key);
                grid.cells.push_back(std::move(star));
                grid.cells.push_back(Cell(Text(kCurrencyTable[c].name)));
                const std::optional<uint32_t> held = amount(c);
                PopupCell value = Cell(held ? Text(Grouped(*held)) : Text("-", Tone::Dim));
                value.align     = Align::Right;
                grid.cells.push_back(std::move(value));
            };
            constexpr int kColumns = 3; // the star, the name and the amount
            PopupSection top{"Favorites", {}};
            if (favorites.empty())
                top.rows.push_back(LineOf("Click a currency's star to keep it here.", Tone::Dim));
            else
            {
                PopupRow grid = Grid(kColumns, favorites.size() * kColumns);
                for (const int c : favorites)
                    add(grid, c);
                top.rows.push_back(std::move(grid));
            }
            PopupSection all{"All Currencies", {}};
            if (std::ranges::none_of(state.currencies, [](const std::optional<uint32_t>& a) { return a.has_value(); }))
                all.rows.push_back(LineOf("Their amounts show once the server sends them.", Tone::Dim));
            std::vector<int> order(kCurrencyCount);
            for (int c = 0; c < kCurrencyCount; ++c)
                order[static_cast<size_t>(c)] = c;
            const auto byName = [](int a, int b) { return std::string_view(kCurrencyTable[a].name) < kCurrencyTable[b].name; };
            for (const bool byAmount : {true, false})
            {
                // Most first, those held alike by name.
                std::ranges::sort(order, [&](int a, int b) {
                    const uint32_t x = amount(a).value_or(0), y = amount(b).value_or(0);
                    return byAmount && x != y ? x > y : byName(a, b);
                });
                PopupTab tab{Text(byAmount ? "By Amount" : "Alphabetically"), {}};
                PopupRow grid = Grid(kColumns, order.size() * kColumns);
                for (const int c : order)
                    add(grid, c);
                grid.nested = true;
                tab.rows.push_back(std::move(grid));
                ScrollsBy(tab.rows.front(), state, all.heading + "/" + tab.name.text);
                all.tabs.push_back(std::move(tab));
            }
            all.tab        = Picked(state, all);
            popup.sections = ListOf(std::move(top), std::move(all));
            return popup;
        }

        // The regions each nation holds, a tab each in two rows of two, yours first and picked each time the popup
        // opens. Your nation's say which outposts you have not run supplies to, as far as overseers told.
        PopupSection ConquestSection(const BarState& state)
        {
            PopupSection conquest{"Conquest (Next Tally in " + Countdown(SecondsToTally(state.unixMs)) + ")", {}};
            conquest.tabsAcross  = 2;
            conquest.tabEachOpen = true;
            if (!state.owners)
            {
                conquest.rows.push_back(LineOf("The standings show once the server sends them.", Tone::Dim));
                return conquest;
            }
            std::vector<Nation> nations;
            if (state.nation && *state.nation >= Nation::SanDoria && *state.nation <= Nation::Windurst) nations.push_back(*state.nation);
            for (const Nation nation : {Nation::SanDoria, Nation::Bastok, Nation::Windurst, Nation::Beastmen})
                if (std::ranges::find(nations, nation) == nations.end()) nations.push_back(nation);
            for (const Nation nation : nations)
            {
                const bool yours = state.nation == nation && state.outposts != nullptr;
                bool unknown     = false;
                PopupTab held{Colored(NationName(nation), NationColor(nation)), {}};
                for (int region = 0; region < kConquestRegionCount; ++region)
                {
                    if ((*state.owners)[region] != nation) continue;
                    const auto r                  = static_cast<uint8_t>(region);
                    const std::optional<bool> ran = yours ? state.outposts->Ran(nation, r) : std::optional<bool>(true);
                    unknown = unknown || !ran;
                    if (ran == false)
                    {
                        PopupCell flag; // gray, after the name: no supply run there yet
                        flag.icon = Icon::Flag;
                        flag.tint = Text("", Tone::Dim);
                        held.rows.push_back(PopupRow{RowKind::Line, 0, {Cell(Text(std::string(RegionName(r)))), std::move(flag)}});
                    }
                    else
                        held.rows.push_back(LineOf(std::string(RegionName(r))));
                }
                if (held.rows.empty()) held.rows.push_back(LineOf("None", Tone::Dim));
                if (unknown)
                    held.rows.push_back(LineOf("Talk to a conquest overseer in your nation's city to see your supply runs.", Tone::Dim));
                for (PopupRow& row : held.rows)
                    row.nested = true;
                ScrollsBy(held.rows.front(), state, std::string(CardKey(conquest.heading)) + "/" + held.name.text);
                conquest.tabs.push_back(std::move(held));
            }
            conquest.tab = Picked(state, conquest);
            return conquest;
        }

        // The Conquest card, Travel and the Calendar in a column beside it, and Guilds under them all.
        PopupContent WorldPopup(const BarState& state)
        {
            PopupContent popup;
            popup.title = ListOf(Text("World Info"));
            popup.sections.push_back(ConquestSection(state));
            {
                // A tab for each kind of ship, its routes together in the data.
                PopupSection travel{"Travel", {}};
                travel.beside = true;
                const std::span<const TravelRoute> routes = TravelRoutes();
                for (size_t first = 0, last = 0; first < routes.size(); first = last)
                {
                    // The route, whether its ship arrives or departs next against how long until it does.
                    constexpr int kColumns = 3;
                    PopupTab kind{Text(routes[first].group), {}};
                    PopupRow grid{RowKind::Grid, kColumns, {}};
                    for (last = first; last < routes.size() && std::string_view(routes[last].group) == routes[first].group; ++last)
                    {
                        const Sailing next = NextSailing(routes[last], state.unixMs);
                        PopupCell stage    = Cell(Text(next.boarding ? "Departs in" : "Arrives in", Tone::Dim));
                        stage.align        = Align::Right;
                        grid.cells.push_back(Cell(Text(routes[last].name)));
                        grid.cells.push_back(std::move(stage));
                        grid.cells.push_back(Cell(Text(Countdown(next.boarding ? next.leaveIn : next.boardIn))));
                    }
                    grid.nested = true;
                    kind.rows.push_back(std::move(grid));
                    ScrollsBy(kind.rows.front(), state, travel.heading + "/" + kind.name.text);
                    travel.tabs.push_back(std::move(kind));
                }
                travel.tab = Picked(state, travel);
                popup.sections.push_back(std::move(travel));
            }
            {
                const Rse now = RseAt(state.unixMs), next = RseAt(state.unixMs, 1);
                const uint32_t today = VanaAt(state.unixMs).day;
                PopupSection calendar{"Calendar", {}};
                calendar.under = true;
                // Each line's name and who on the left; where and when on the right, the time last.
                const auto race = [](const char* label, const Rse& rse) {
                    PopupRow row = Labeled(label, Text(rse.place));
                    row.cells.front().text.push_back(Text(std::string(rse.race) + " "));
                    return row;
                };
                calendar.rows.push_back(race("RSE", now));
                PopupRow coming = race("Next RSE", next);
                coming.cells.back().text.push_back(Text(kDot, Tone::Dim));
                coming.cells.back().text.push_back(Text(Countdown(now.endsIn)));
                calendar.rows.push_back(std::move(coming));
                const auto until = [](const char* label, double seconds) { return Labeled(label, Text(Countdown(seconds))); };
                calendar.rows.push_back(until("Full Moon", SecondsToDay(state.unixMs, NextMoonDay(today, true))));
                calendar.rows.push_back(until("New Moon", SecondsToDay(state.unixMs, NextMoonDay(today, false))));
                popup.sections.push_back(std::move(calendar));
            }
            {
                // A tab for each guild, its shops together in the data: each shop's hours in columns, open or closed
                // against when that changes and where, then a craft's turn-ins.
                PopupSection guilds{"Guilds", {}};
                const std::span<const GuildShop> shops = GuildShops();
                for (size_t first = 0, last = 0; first < shops.size(); first = last)
                {
                    for (last = first; last < shops.size() && std::string_view(shops[last].guild) == shops[first].guild; ++last) {}
                    PopupTab guild{Text(shops[first].guild), {}};
                    guild.rows.push_back(LineOf("Shop Hours", Tone::Heading));
                    PopupRow grid = Grid(kWorldGuildColumns, (last - first) * kWorldGuildColumns);
                    grid.nested   = true;
                    for (size_t i = first; i < last; ++i)
                    {
                        const GuildHours hours = GuildAt(shops[i], state.unixMs);
                        PopupCell now = Cell(Text(hours.open ? "Open" : hours.holiday ? "Holiday" : "Closed",
                            hours.open ? Tone::Normal : Tone::Dim));
                        now.align = Align::Right;
                        PopupCell change = Cell(Text((hours.open ? "closes in " : "opens in ") + Countdown(hours.changeIn)));
                        change.align = Align::Center;
                        grid.cells.push_back(std::move(now));
                        grid.cells.push_back(std::move(change));
                        grid.cells.push_back(Cell(Text(std::string(shops[i].shops) + ", " + shops[i].zone, Tone::Dim)));
                    }
                    ScrollsBy(grid, state, guilds.heading + "/" + shops[first].guild);
                    guild.rows.push_back(std::move(grid));
                    if (const std::optional<int> craft = CraftGuildNamed(shops[first].guild))
                        for (PopupRow& row : TurnIns(state, *craft, shops[first].guild))
                            guild.rows.push_back(std::move(row));
                    guilds.tabs.push_back(std::move(guild));
                }
                guilds.tab = Picked(state, guilds);
                popup.sections.push_back(std::move(guilds));
            }
            return popup;
        }

        // Calls each with the options of every copy of a widget that Shows.
        template <typename Each>
        void EachShownCopy(const Settings& settings, BarWidget widget, const Each& each)
        {
            for (int copy = 0; copy < kMostCopies; ++copy)
                if (const PanelWidget w{widget, copy}; Shows(settings, w)) each(settings.Widget(w));
        }

        // Red when full; dim for a bag with no room yet, which is not full.
        Span BagFill(const BagCount& bag)
        {
            return Text(std::to_string(bag.used) + " / " + std::to_string(bag.max),
                bag.max == 0 ? Tone::Dim : bag.used >= bag.max ? Tone::Resist : Tone::Normal);
        }

        std::vector<PartContent> TargetParts(const BarState& state, const Settings& settings, const WidgetOptions& o,
            WidgetContent& base)
        {
            if ((o.targetShows & TargetShowBit(TargetShowOf(state.target))) == 0)
            {
                base.shown = false;
                return {};
            }
            if (state.target == nullptr) return {{WidgetPart::TargetName, ListOf(Text("No Target", Tone::Dim))}};
            std::vector<PartContent> parts;
            if (TargetShowOf(state.target) != TargetShow::Mobs)
            {
                parts.push_back({WidgetPart::TargetName, ListOf(Colored(state.target->name,
                                                             ToArgb(settings.palette.type[static_cast<int>(TypeOf(state.target->state))])))});
                if (state.target->kind == EntityKind::Player)
                {
                    // What is known of their jobs, the newest first; a check alone has a popup to show.
                    const std::optional<PlayerJobs>& jobs = state.target->jobs;
                    std::string said;
                    if (jobs && jobs->mainJob != 0)
                        said = JobsText(jobs->mainJob, jobs->mainLevel, jobs->subJob, jobs->subLevel);
                    else if (state.target->status && state.target->status->anonymous)
                        said = kAnonymousJobs;
                    else if (state.targetCheck)
                        said = CheckedJobs(state.targetCheck->general);
                    if (!said.empty()) parts.push_back({WidgetPart::TargetPlayer, ListOf(Text(said))});
                    base.hasPopup = state.targetCheck.has_value();
                }
                base.hasPopup = base.hasPopup || NpcOf(state.target) != nullptr;
            }
            else
            {
                const IconSet aggro = IconsFor(state.target->mob, DetectionBit(DetectionIcon::Aggro));
                const IconSet senses = IconsFor(state.target->mob, o.targetDetection & ~DetectionBit(DetectionIcon::Aggro));
                parts.push_back({WidgetPart::TargetAggro, Marks(aggro.icons, aggro.count)});
                parts.push_back({WidgetPart::TargetDetection, Marks(senses.icons, senses.count)});
                parts.push_back({WidgetPart::TargetName, ListOf(Colored(state.target->name, NameColor(*state.target, settings)))});
                if (HasLevel(*state.target))
                    parts.push_back({WidgetPart::TargetLevel,
                        ListOf(Colored(state.target->level.text, ShadeColor(*state.target, settings)))});
                if (IsMob(state.target))
                {
                    // Weaknesses by SDT or rank, as the badges find them by default.
                    std::vector<Span> weak;
                    const InfoIconSet found = InfoIconsFor(&ResistancesOf(*state.target->mob), BadgeRules{});
                    for (int i = 0; i < found.count; ++i)
                        if (found.icons[i].badge == BadgeKind::Weak || found.icons[i].badge == BadgeKind::WeakRank)
                            weak.push_back(Mark(found.icons[i].icon));
                    parts.push_back({WidgetPart::TargetWeak, std::move(weak)});
                }
                base.hasPopup = IsMob(state.target); // a mob the data lacks gets its name, level and con, no popup
            }
            if (state.targetDistance && std::isfinite(*state.targetDistance))
                parts.push_back({WidgetPart::TargetDistance, ListOf(Text("(" + Printed("%.1f", *state.targetDistance) + ")", Tone::Dim))});
            return parts;
        }

        // What each part of a widget shows now, base taking whether it shows at all, its popup and its figures.
        std::vector<PartContent> PartsNow(PanelWidget widget, const BarState& state, const Settings& settings,
            WidgetContent& base)
        {
            const WidgetOptions& o = settings.Widget(widget);
            switch (widget.kind)
            {
            case BarWidget::Zone:
            {
                const ZoneRecord* zone = KnownZone(state.zone);
                const std::string_view region = ZoneRegion(zone);
                std::vector<PartContent> parts{{WidgetPart::ZoneName, ListOf(Text(ZoneTitle(state.zone)))},
                    {WidgetPart::ZoneRegion, region.empty() ? std::vector<Span>{} : ListOf(Text(std::string(region)))}};
                // Who holds its region, by their crest and by name.
                if (const Nation owner = OwnerOf(zone, state); owner != Nation::None)
                {
                    if (const GamePicture crest = NationCrest(owner); crest != GamePicture::None)
                        parts.push_back({WidgetPart::ZoneCrest, ListOf(Mark(crest))});
                    parts.push_back({WidgetPart::ZoneNation, ListOf(Colored(std::string(NationName(owner)), NationColor(owner)))});
                }
                base.hasPopup = state.zone != 0;
                return parts;
            }
            case BarWidget::Weather:
            {
                // The weather now, or the day's likeliest before it is known; Vana'diel's date, day, time and moon.
                const VanaTime t = VanaAt(state.unixMs);
                const std::optional<WeatherOdds> today = OddsOn(state, t.day);
                std::vector<PartContent> parts{{WidgetPart::WeatherIcon, {}}, {WidgetPart::WeatherName, {}},
                    {WidgetPart::VanaDate, ListOf(Text(VanaDate(t)))}, {WidgetPart::VanaDay, ListOf(WeekdayText(t))},
                    {WidgetPart::VanaTime, ListOf(Text(HourMinute(t.hour, t.minute)))},
                    {WidgetPart::VanaMoon, ListOf(Text(MoonText(t) + " " + std::to_string(t.moonPercent) + "%"))}};
                if (state.weather || today)
                {
                    const Weather now = state.weather ? *state.weather : today->normal;
                    for (const Icon icon : WeatherIcons(now, o.weatherIcons))
                        parts[0].spans.push_back(Mark(icon));
                    parts[1].spans = ListOf(Text(WeatherName(now)));
                }
                base.hasPopup = true;
                return parts;
            }
            case BarWidget::Target: return TargetParts(state, settings, o, base);
            case BarWidget::Currency:
            {
                // The picked currency; gil with its coin.
                base.hasPopup = true;
                if (o.currency == kGilCurrency)
                {
                    Span coin = Text("");
                    coin.coin = true;
                    return {{WidgetPart::CurrencyCoin, ListOf(coin)},
                        {WidgetPart::CurrencyAmount, ListOf(Text(state.gil ? Grouped(*state.gil) : "-"))}};
                }
                const std::optional<uint32_t>& amount = state.currencies[o.currency];
                return {{WidgetPart::CurrencyAmount, ListOf(Text(amount ? Grouped(*amount) : "-"))},
                    {WidgetPart::CurrencyName, ListOf(Text(kCurrencyTable[o.currency].name))}};
            }
            case BarWidget::Bags:
            {
                const std::optional<BagCount>& bag = state.bags[o.bag];
                base.hasPopup = true;
                return {{WidgetPart::BagIcon, ListOf(Mark(Icon::Bag))}, {WidgetPart::BagCount, ListOf(bag ? BagFill(*bag) : Text("-"))},
                    {WidgetPart::BagName, ListOf(Text(ContainerName(o.bag)))}};
            }
            case BarWidget::EarthTime:
                base.hasPopup = true;
                return {{WidgetPart::ClockIcon, ListOf(Mark(Icon::Clock))}, {WidgetPart::ClockTime,
                            ListOf(Text(o.clockZone ? ZoneClockText(ZoneAt(*o.clockZone, state.unixMs), o.clock24, o.clockSeconds)
                                                    : ClockText(state.earthHour, state.earthMinute, o.clock24,
                                                          o.clockSeconds ? state.earthSecond : -1)))},
                    {WidgetPart::ClockServer,
                        ListOf(Text(o.clockSecondZone ? ZoneClockText(ZoneAt(*o.clockSecondZone, state.unixMs), o.clock24, false)
                                                      : LocalZoneName(state.unixMs, state.earthYear, state.earthMonth,
                                                            state.earthDay, state.earthHour, state.earthMinute) +
                                                            " " + ClockText(state.earthHour, state.earthMinute, o.clock24, -1)))},
                    {WidgetPart::ClockVana, ListOf(Text(VanaClock(state.unixMs)))}};
            case BarWidget::GameHelp: return {};
            case BarWidget::Performance: return PerformanceParts(state, base);
            case BarWidget::Job: return JobParts(state.job, o, base);
            case BarWidget::Party: return PartyParts(state.party, base);
            case BarWidget::StatusTimers:
                base.hasPopup = true;
            {
                std::vector<PartContent> parts;
                for (size_t rank = 0; rank < kMostStatusPins; ++rank)
                    parts.push_back({static_cast<WidgetPart>(static_cast<size_t>(WidgetPart::StatusPin1) + rank),
                        PinnedStatus(state.statuses, settings.statusPins, rank)});
                return parts;
            }
            case BarWidget::World: return WorldParts(state, o, base);
            }
            return {};
        }

        WidgetContent ContentOf(PanelWidget widget, const BarState& state, const Settings& settings)
        {
            WidgetContent base;
            base.shown = Shows(settings, widget);
            const WidgetOptions& o = settings.Widget(widget);
            if (widget.kind == BarWidget::GameHelp) return WidgetContent{}; // its own bar, never a widget
            if (!base.shown) return base;
            std::vector<PartContent> parts = PartsNow(widget, state, settings, base);
            if (!base.shown) return base;
            // The widget's icon goes beside both lines, where its Icon setting puts it.
            if (const std::optional<WidgetPart> icon = IconPartOf(widget.kind))
                if (const auto part = std::ranges::find(parts, *icon, &PartContent::part); part != parts.end())
                {
                    if (o.iconSide != IconSide::Hide)
                        for (const Span& s : part->spans)
                        {
                            if (s.icon) base.icons.push_back(*s.icon);
                            if (s.coin) base.coin = true;
                            if (s.picture != GamePicture::None) base.picture = s.picture;
                        }
                    base.iconsRight = o.iconSide == IconSide::Right;
                    parts.erase(part);
                }
            // The pins decide which Pinned Status buttons show, not their switches.
            WidgetDesign design = o.design;
            if (widget.kind == BarWidget::StatusTimers)
                for (std::vector<DesignChip>& line : design.lines)
                    for (DesignChip& chip : line)
                        chip.on = true;
            WidgetContent w = ComposeContent(design, std::move(parts), std::move(base));
            if (w.title.empty() && widget.kind == BarWidget::StatusTimers) w.title = ListOf(Text("Status Timers", Tone::Dim));
            if (w.title.empty() && w.icons.empty() && !w.coin && w.picture == GamePicture::None) w.title = ListOf(Text("-"));
            return w;
        }
    }

    uint32_t ElementColor(Element element) { return kElementColors[static_cast<int>(element)]; }

    std::optional<Span> StripBagFill(const BarState& state)
    {
        return state.stripBag ? std::optional<Span>(BagFill(*state.stripBag)) : std::nullopt;
    }

    std::optional<uint32_t> GilOf(uint16_t item, uint32_t count)
    {
        return item == kGilItem ? std::optional<uint32_t>(count) : std::nullopt;
    }

    std::optional<BagCount> BagOf(uint32_t used, uint32_t max)
    {
        return max > 0 ? std::optional<BagCount>(BagCount{used, max}) : std::nullopt;
    }

    const char* ContainerName(int container) { return kContainerNames[std::clamp(container, 0, kContainerCount - 1)]; }

    const std::vector<int>& BagChoices()
    {
        static const std::vector<int> choices = [] {
            std::vector<int> all;
            for (int c = 0; c < kContainerCount; ++c)
                if (c != kTemporaryItems) all.push_back(c);
            return all;
        }();
        return choices;
    }

    std::optional<int> ContainerNamed(const std::string& caption)
    {
        for (int c = 0; c < kContainerCount; ++c)
            if (caption == kContainerNames[c]) return c;
        // The Items menu opens on the inventory under its own name.
        if (caption == kItemsMenu) return static_cast<int>(kInventory);
        for (const ShortCaption& shortened : kShortCaptions)
            if (caption == shortened.caption) return shortened.container;
        return std::nullopt;
    }

    PressResult ApplyPopupPress(Settings& settings, std::string_view press, Nation nation)
    {
        // A status's star or pin; a status taken off the favorites leaves the panel too, as only they have pins.
        for (const std::string_view kind : {kStatusFavoritePress, kStatusPinPress})
        {
            if (!press.starts_with(kind)) continue;
            const std::string number(press.substr(kind.size()));
            char* end           = nullptr;
            const unsigned long id = std::strtoul(number.c_str(), &end, 10);
            if (number.empty() || *end != '\0' || id > UINT16_MAX || *StatusName(static_cast<uint16_t>(id)) == '\0')
                return PressResult::None;
            const auto status = static_cast<uint16_t>(id);
            std::vector<uint16_t>& list = kind == kStatusPinPress ? settings.statusPins : settings.statusFavorites;
            if (const auto at = std::ranges::find(list, status); at != list.end())
            {
                if (settings.statusPins == std::vector<uint16_t>{status}) return PressResult::Refused;
                list.erase(at);
                if (kind == kStatusFavoritePress) std::erase(settings.statusPins, status);
            }
            else if (kind == kStatusPinPress && list.size() >= kMostStatusPins)
                return PressResult::Refused;
            else
                list.push_back(status);
            return PressResult::Changed;
        }
        if (press.starts_with(kPopupResetPress))
        {
            const std::string number(press.substr(std::string_view(kPopupResetPress).size()));
            char* end = nullptr;
            const long slot = std::strtol(number.c_str(), &end, 10);
            if (number.empty() || *end != '\0' || slot < 0 || slot >= kPanelSlots) return PressResult::None;
            settings.widgets[static_cast<size_t>(slot)].popupSpot.reset();
            return PressResult::Changed;
        }
        if (press.starts_with(kCollapsePress))
        {
            const std::string key(press.substr(std::string_view(kCollapsePress).size()));
            if (key.empty() || key.find(' ') != std::string::npos) return PressResult::None;
            if (const auto at = std::ranges::find(settings.collapsedSections, key); at != settings.collapsedSections.end())
                settings.collapsedSections.erase(at);
            else
                settings.collapsedSections.push_back(key);
            return PressResult::Changed;
        }
        if (!press.starts_with(kFavoritePress)) return PressResult::None;
        const std::string key(press.substr(std::string_view(kFavoritePress).size()));
        if (!CurrencyOfKey(key)) return PressResult::None;
        // Your nation's points by their own key once it is known, so its star takes them off.
        std::vector<std::string> favorites;
        for (const std::string& held : settings.currencyFavorites)
        {
            const std::optional<int> yours = held == kYourConquestPoints ? NationPoints(nation) : std::nullopt;
            std::string named = yours ? kCurrencyTable[*yours].key : held;
            if (std::ranges::find(favorites, named) == favorites.end()) favorites.push_back(std::move(named));
        }
        if (const auto at = std::ranges::find(favorites, key); at != favorites.end())
            favorites.erase(at);
        else
            favorites.push_back(key);
        settings.currencyFavorites = std::move(favorites);
        return PressResult::Changed;
    }

    bool StandingsShown(const Settings& settings)
    {
        bool shown = AnyCopyShown(settings, BarWidget::Zone);
        EachShownCopy(settings, BarWidget::World,
            [&](const WidgetOptions& o) {
                for (int n = 0; n < kContenderNations; ++n)
                    shown = shown || PartOn(o.design, static_cast<WidgetPart>(static_cast<int>(WidgetPart::WorldSanDoria) + n));
            });
        return shown;
    }

    bool PopupShows(const Settings& settings, const std::vector<PanelWidget>& shown, BarWidget widget)
    {
        return std::ranges::any_of(shown, [&](PanelWidget open) {
            return open.kind == widget ||
                   std::ranges::any_of(Members(Panel(settings), open), [&](PanelWidget w) { return w.kind == widget; });
        });
    }

    BarNeeds NeedsOf(const Settings& settings, const std::vector<PanelWidget>& showing)
    {
        const auto shown  = [&](BarWidget widget) { return AnyCopyShown(settings, widget); };
        const auto opened = [&](BarWidget widget) { return PopupShows(settings, showing, widget); };
        BarNeeds needs;
        // A Clock in a zone of its own goes by the time alone, as its server line and its popup's resets do; in the
        // computer's zone, by what Windows makes of the time.
        bool zonedClock = false, localClock = false, serverClock = false;
        EachShownCopy(settings, BarWidget::EarthTime, [&](const WidgetOptions& o) {
            (o.clockZone ? zonedClock : localClock) = true;
            // The computer's zone is named by comparing its time with the time alone.
            const bool second = PartOn(o.design, WidgetPart::ClockServer);
            serverClock = serverClock || second || PartOn(o.design, WidgetPart::ClockVana);
            localClock  = localClock || (second && !o.clockSecondZone);
        });
        needs.unixTime   = shown(BarWidget::Weather) || zonedClock || serverClock ||
                           shown(BarWidget::World) || opened(BarWidget::EarthTime) ||
                           opened(BarWidget::Zone); // the Zone popup counts down to the tally
        needs.localTime  = localClock;
        needs.zone       = shown(BarWidget::Zone) || shown(BarWidget::Weather);
        needs.target     = shown(BarWidget::Target);
        needs.gil        = shown(BarWidget::Currency);
        needs.strip      = shown(BarWidget::GameHelp);
        // The Performance widget reads what its copies show on the panel, and everything for its popup.
        uint8_t perf = 0;
        EachShownCopy(settings, BarWidget::Performance, [&](const WidgetOptions& o) {
            for (int p = 0; p < kPerfPartCount; ++p)
                if (PartOn(o.design, kPerfWidgetParts[p])) perf |= PerfBit(static_cast<PerfPart>(p));
        });
        const bool perfPopup = opened(BarWidget::Performance);
        needs.conquest   = opened(BarWidget::World) || opened(BarWidget::Currency);
        needs.frames     = (perf & PerfBit(PerfPart::Fps)) != 0 || perfPopup;
        needs.job        = shown(BarWidget::Job);
        needs.connection = (perf & kConnectionParts) != 0 || perfPopup;
        needs.party      = shown(BarWidget::Party);
        needs.statuses   = (shown(BarWidget::StatusTimers) && !settings.statusPins.empty()) || opened(BarWidget::StatusTimers);
        needs.memory     = (perf & PerfBit(PerfPart::Memory)) != 0 || perfPopup;
        needs.cpu        = perfPopup;
        needs.linuxxi    = perfPopup;
        // Ashita counts a container's items by walking its slots, so the others wait for the popup that lists them.
        if (shown(BarWidget::Bags))
        {
            needs.containers = opened(BarWidget::Bags) ? kContainerCount : 1;
            EachShownCopy(settings, BarWidget::Bags,
                [&](const WidgetOptions& o) { needs.containers = std::max(needs.containers, o.bag + 1); });
        }
        return needs;
    }

    WidgetContent BarWidgetContent(PanelWidget widget, const BarState& state, const Settings& settings)
    {
        return ContentOf(widget, state, settings);
    }

    std::array<PartMark, kWidgetPartCount> PartMarksNow(const std::vector<PanelWidget>& widgets, const BarState& state,
        const Settings& settings)
    {
        std::array<PartMark, kWidgetPartCount> marks;
        for (int p = 0; p < kWidgetPartCount; ++p)
            marks[static_cast<size_t>(p)] = DefaultMark(static_cast<WidgetPart>(p));
        for (const PanelWidget widget : widgets)
        {
            WidgetContent base;
            for (const PartContent& part : PartsNow(widget, state, settings, base))
                if (const std::optional<PartMark> mark = MarkOf(part.spans)) marks[static_cast<size_t>(part.part)] = *mark;
        }
        return marks;
    }

    namespace
    {
        // Every popup of a pill's widgets, joined: a title or card shown already, as two copies of a widget have, is
        // left out.
        PopupContent JoinedPopupContent(PanelWidget head, const BarState& state, const Settings& settings)
        {
            const std::vector<PanelWidget> pill = Members(Panel(settings), head);
            if (pill.size() <= 1) return BarPopupContent(head, state, settings);
            PopupContent joined;
            joined.oneColumn = true;
            std::vector<std::vector<Span>> titles;
            for (const PanelWidget w : pill)
            {
                if (!ContentOf(w, state, settings).hasPopup) continue;
                PopupContent own = BarPopupContent(w, state, settings);
                if (!own.title.empty() && std::ranges::find(titles, own.title) == titles.end())
                {
                    if (joined.title.empty())
                        joined.icon = own.icon;
                    else
                        joined.title.push_back(Text(kDot, Tone::Dim));
                    joined.title.insert(joined.title.end(), own.title.begin(), own.title.end());
                    titles.push_back(std::move(own.title));
                    if (!joined.subtitle.empty() && !own.subtitle.empty()) joined.subtitle.push_back(Text(kDot, Tone::Dim));
                    joined.subtitle.insert(joined.subtitle.end(), own.subtitle.begin(), own.subtitle.end());
                }
                for (PopupSection& section : own.sections)
                    if (std::ranges::find(joined.sections, section) == joined.sections.end())
                        joined.sections.push_back(std::move(section));
                joined.oneColumn = joined.oneColumn && own.oneColumn;
            }
            return joined;
        }
    }

    std::vector<Icon> WeatherIcons(Weather weather, CustomIcons use)
    {
        const std::optional<Icon> game = WeatherIcon(weather);
        if (use == CustomIcons::Always || (use == CustomIcons::NoVanilla && !game))
            return {kCustomWeather[std::clamp(static_cast<int>(weather), 0, kWeatherCount - 1)]};
        if (!game) return {};
        return std::vector<Icon>(IsDoubleWeather(weather) ? 2 : 1, *game);
    }

    std::optional<PanelWidget> ApplyWidgetPress(Settings& settings, std::string_view press)
    {
        for (const std::string_view kind : {kWidgetClonePress, kWidgetRemovePress})
        {
            if (!press.starts_with(kind)) continue;
            const std::string number(press.substr(kind.size()));
            char* end         = nullptr;
            const long slot   = std::strtol(number.c_str(), &end, 10);
            if (number.empty() || *end != '\0' || slot < 0 || slot >= kPanelSlots) return std::nullopt;
            const PanelWidget widget = WidgetInSlot(static_cast<int>(slot));
            if (kind == kWidgetClonePress) return HasCopies(widget.kind) ? CloneWidget(settings, widget) : std::nullopt;
            if (widget.copy == 0 || !Placed(settings.barSections, widget)) return std::nullopt;
            RemoveWidgetCopy(settings, widget);
            return PanelWidget{widget.kind};
        }
        return std::nullopt;
    }

    std::string CollapseKey(PanelWidget head, std::string_view heading)
    {
        heading = CardKey(heading);
        std::string key = PanelWidgetName(head) + ".";
        for (const char c : heading)
            key += c == ' ' ? '-' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return key;
    }

    PopupContent PanelPopupContent(PanelWidget head, const BarState& state, const Settings& settings)
    {
        PopupContent content = JoinedPopupContent(head, state, settings);
        for (PopupSection& section : content.sections)
        {
            if (section.heading.empty()) continue;
            const std::string key = CollapseKey(head, section.heading);
            section.collapsePress = kCollapsePress + key;
            section.collapsed     = std::ranges::find(settings.collapsedSections, key) != settings.collapsedSections.end();
        }
        if (HasCopies(head.kind)) content.clonePress = kWidgetClonePress + std::to_string(SlotOf(head));
        if (head.copy > 0) content.removePress = kWidgetRemovePress + std::to_string(SlotOf(head));
        if (settings.Widget(head).popupSpot) content.resetPress = kPopupResetPress + std::to_string(SlotOf(head));
        return content;
    }

    PopupContent BarPopupContent(PanelWidget widget, const BarState& state, const Settings& settings)
    {
        PopupContent popup;
        const WidgetOptions& o = settings.Widget(widget);
        switch (widget.kind)
        {
        case BarWidget::Zone:
        {
            const ZoneRecord* zone = KnownZone(state.zone);
            const std::string_view region = ZoneRegion(zone);
            popup.title = ListOf(Text(ZoneTitle(state.zone)));
            // A zone past the data has its number for a title already.
            if (zone != nullptr) popup.title.push_back(Text(" (Zone #" + std::to_string(state.zone) + ")", Tone::Dim));
            if (!region.empty()) popup.subtitle = ListOf(Text(std::string(region), Tone::Dim));
            if (InConquest(zone) && state.shares)
            {
                PopupSection influence{"Influence", {}};
                const Nation owner = OwnerOf(zone, state);
                for (const RegionShare& share : *state.shares)
                {
                    PopupRow row = Labeled(Colored(NationName(share.nation), NationColor(share.nation)),
                        Text(std::to_string(share.percent) + "%"));
                    row.cells[0].star    = share.nation == owner;
                    row.cells[0].picture = NationCrest(share.nation);
                    influence.rows.push_back(std::move(row));
                }
                influence.rows.push_back(Labeled("Tally in", Text(SpanText(SecondsToTally(state.unixMs)))));
                popup.sections.push_back(std::move(influence));
            }
            break;
        }
        case BarWidget::Weather:
        {
            const VanaTime t = VanaAt(state.unixMs);
            const std::optional<WeatherOdds> odds = OddsOn(state, t.day);
            const WeatherChances today = odds ? Chances(*odds) : WeatherChances{};
            const auto iconOf = [&](Weather weather) {
                const std::vector<Icon> icons = WeatherIcons(weather, o.weatherIcons);
                return icons.empty() ? std::nullopt : std::optional(icons.front());
            };
            const auto chanceOf = [](const WeatherChance& c) {
                return std::string(WeatherName(c.weather)) + " " + std::to_string(c.percent) + "%";
            };
            // The weather now for a title, with its icon and today's chance of it.
            if (state.weather || odds)
            {
                const Weather now = state.weather ? *state.weather : odds->normal;
                std::string title = WeatherName(now);
                for (const WeatherChance& c : today)
                    if (c.weather == now) title += " (" + std::to_string(c.percent) + "%)";
                popup.title = ListOf(Text(title));
                popup.icon  = iconOf(now);
            }
            // Today, by Vana'diel's date: the day and time, the zone's weathers today, then the moon, each with what is next.
            PopupSection day{"Today (" + VanaDate(t) + ")", ListOf(Line(Cell(ListOf(WeekdayText(t),
                Text(" " + HourMinute(t.hour, t.minute)),
                Text(" (Next Day in " + Countdown(SecondsToNextDay(state.unixMs)) + ")", Tone::Dim)))))};
            for (const WeatherChance& c : today)
                day.rows.push_back(Line(Cell(Text(chanceOf(c)), iconOf(c.weather))));
            day.rows.push_back(Line(Cell(ListOf(Text(MoonText(t) + ", " + std::to_string(t.moonPercent) + "%"),
                Text(" (Next Phase in " + Countdown(SecondsToNextPhase(state.unixMs)) + ")", Tone::Dim)))));
            // The days after it, beside it: each day and its chances, likeliest first.
            PopupRow grid = Grid(kForecastColumns, (kForecastDays - 1) * kForecastColumns);
            for (int d = 1; d < kForecastDays; ++d)
            {
                const std::optional<WeatherOdds> later = OddsOn(state, t.day + static_cast<uint32_t>(d));
                const WeatherChances chances = later ? Chances(*later) : WeatherChances{};
                const int weekday = (t.weekday + d) % kVanaDaysPerWeek;
                const std::string name = WeekdayName(weekday);
                PopupCell named = Cell(Colored(d == 1 ? "Tomorrow (" + name + ")" : name, ElementColor(WeekdayElement(weekday))));
                named.align = Align::Right;
                grid.cells.push_back(std::move(named));
                for (int c = 0; c < kForecastColumns - 1; ++c)
                {
                    PopupCell weather;
                    if (c < chances.count) weather = Cell(Text(chanceOf(chances.chances[c])), iconOf(chances.chances[c].weather));
                    weather.align = Align::Center;
                    grid.cells.push_back(std::move(weather));
                }
            }
            // Zone 0 is the data's "unknown": the heading names no zone for it.
            const ZoneRecord* zone = state.zone != 0 ? ZoneById(state.zone) : nullptr;
            PopupSection forecast{zone != nullptr ? "Weather Forecast (" + std::string(zone->name) + ")" : "Weather Forecast",
                ListOf(std::move(grid))};
            forecast.beside = true;
            popup.sections  = ListOf(std::move(day), std::move(forecast), ByElement(state, t.day));
            popup.oneColumn = true;
            break;
        }
        case BarWidget::Target: return TargetPopup(state, settings);
        case BarWidget::Currency: return CurrencyPopup(state, settings);
        case BarWidget::Bags:
        {
            popup.title   = ListOf(Text("Inventory"));
            PopupRow grid = Grid(kBagColumns, kBagColumns * kContainerCount);
            for (const int c : BagChoices())
            {
                // Storage shows before its furniture gives it room. The server gives Mog Safe 2 its room before the
                // second floor is open: only what is in it says it can be used.
                const std::optional<BagCount> bag = c == kStorage && !state.bags[c] ? std::optional(BagCount{}) : state.bags[c];
                if (!bag || (c == kMogSafe2 && bag->used == 0)) continue;
                grid.cells.push_back(Cell(Text(kContainerNames[c])));
                grid.cells.push_back(Cell(BagFill(*bag)));
            }
            popup.sections = ListOf(PopupSection{"", ListOf(std::move(grid))});
            break;
        }
        case BarWidget::EarthTime:
        {
            int year = state.earthYear, month = state.earthMonth, day = state.earthDay;
            if (o.clockZone)
            {
                const ZoneTime there = ZoneAt(*o.clockZone, state.unixMs);
                year = there.year, month = there.month, day = there.day;
            }
            popup.title =
                ListOf(Text(std::to_string(year) + "-" + Printed("%02d", month) + "-" + Printed("%02d", day)));
            popup.sections = ListOf(ServerTimeRows(state.unixMs));
            break;
        }
        case BarWidget::GameHelp:
            return GameHelpPopup(state.strip, StripBagFill(state));
        case BarWidget::Performance:
            return PerformancePopup(state);
        case BarWidget::Job:
            return JobPopup(state.job);
        case BarWidget::Party:
            return PartyPopup(state.party);
        case BarWidget::StatusTimers:
            return StatusTimersPopup(state.statuses, settings.statusFavorites, settings.statusPins,
                ScrolledBy(state, kStatusListKey), state.pinRefused);
        case BarWidget::World:
            return WorldPopup(state);
        }
        return popup;
    }
}

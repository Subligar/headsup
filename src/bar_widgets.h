#pragma once

#include "bar_connection.h"
#include "bar_content.h"
#include "bar_fps.h"
#include "bar_job.h"
#include "bar_memory.h"
#include "bar_party.h"
#include "bar_statuses.h"
#include "bar_types.h"
#include "game_packets.h"
#include "game_state.h"
#include "outposts.h"
#include "icons.h"
#include "settings.h"
#include "strip_text.h"
#include "tracker.h"
#include "weather.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace headsup
{
    struct BagCount
    {
        uint32_t used = 0, max = 0;
    };
    constexpr uint32_t kInventory = 0, kGilSlot = 0; // gil is an item in the inventory's first slot
    constexpr int kMogSafe = 1, kStorage = 2, kTemporaryItems = 3, kMogSafe2 = 9;
    // That slot's item read as gil: its count when the item is gil, nullopt for anything else there.
    std::optional<uint32_t> GilOf(uint16_t item, uint32_t count);
    // A container as Ashita counts it: nullopt for one the character has no room in (locked, or not bought).
    std::optional<BagCount> BagOf(uint32_t used, uint32_t max);
    // The container whose name, as the Bags popup gives it, the caption is: the game's caption for a bag list, or
    // "Items" for the inventory as that menu opens.
    std::optional<int> ContainerNamed(const std::string& caption);
    // A container's name wherever HeadsUp shows one (the widget, its pop-out, the Bag dropdowns): the game's without
    // "Mog", a number in brackets, "Wardrobe (3)".
    std::string ContainerName(int container);
    // The containers the Bags widget may show, in its dropdowns' order: every one but the temporary items.
    const std::vector<int>& BagChoices();

    // What the widgets show, gathered by the plugin each frame.
    struct BarState
    {
        int64_t unixMs = 0; // Earth time, UTC, for the Vana'diel clock
        int earthYear = 0, earthMonth = 0, earthDay = 0, earthHour = 0, earthMinute = 0, earthSecond = 0; // local
        uint16_t zone = 0; // 0 while unknown
        const ActorInfo* target = nullptr;
        std::optional<float> targetDistance;   // yalms, with the target
        // The game's name for an item ID, for a mob's drops; nullptr, or an empty name, when it has none.
        const char* (*itemName)(uint16_t item) = nullptr;
        // And for a spell ID, for what a mob casts.
        const char* (*spellName)(uint16_t spell) = nullptr;
        // Whether you have learned a spell, for the blue magic among a mob's TP moves.
        bool (*knowsSpell)(uint16_t spell) = nullptr;
        std::optional<double> targetPhSeconds; // the target placeholder's countdown, when one runs
        std::optional<PlayerCheck> targetCheck; // the targeted player's last check, gathered with the target
        std::optional<Caster> caster;           // your magic accuracy, gathered with the target for its chances
        std::optional<uint32_t> gil;           // nullopt while the inventory reports nothing
        CurrencyValues currencies{};           // the server's, as last sent; gil is the inventory's, above
        std::array<std::optional<BagCount>, kContainerCount> bags{}; // nullopt for a container without room
        std::optional<StripText> strip;    // nullopt while the strip cannot be read
        std::optional<BagCount> stripBag;  // the bag the caption names
        std::optional<Weather> weather; // the latest from the server; nullopt before one since loading or zoning
        std::optional<RegionOwners> owners; // nullopt before a conquest update
        std::optional<RegionShares> shares; // nullopt before a conquest update since the last zone-in
        std::optional<Nation> nation;       // yours, once in game
        const OutpostRuns* outposts = nullptr; // your character's supply runs, as overseers told them
        std::optional<FrameReport> frames; // nullopt before the first frame
        std::optional<JobReport> job; // nullopt while the game reports no job
        std::optional<ConnectionReport> connection;
        Party party{}; // empty slots for no one
        std::vector<ActiveStatus> statuses; // yours
        std::optional<MemoryReport> memory; // nullopt before the first whole walk of the space
        std::optional<double> cpuPercent;   // of one core; nullopt before two samples
        LinuxXIState linuxxi;
        const std::map<std::string, std::string>* pickedTabs = nullptr; // the tab the player picked in a card, by its heading
        const std::map<std::string, size_t>* scrolled        = nullptr; // lines a part of a popup is scrolled past, by its key
        bool pinRefused = false; // Status Timers kept its last pin since its popup opened
        const GuildPatterns* turnIns = nullptr; // today's guild point turn-ins, once fetched
        bool turnInsFailed = false;             // the last fetch got no answer
        std::optional<CraftRanks> crafts;       // yours, once the server sent them
    };

    // What the panel reads of your character, which the game empties or fills partway while you change zones.
    struct CharacterReadings
    {
        uint16_t zone = 0;
        std::optional<Weather> weather;
        std::optional<JobReport> job;
        std::optional<uint32_t> gil;
        std::array<std::optional<BagCount>, kContainerCount> bags{};
        std::vector<ActiveStatus> statuses;
        int64_t unixMs = 0; // when the statuses were read
    };
    // Loaded, held takes state's readings; while loading, state shows held's instead, their status timers run down
    // since.
    void HoldWhileLoading(BarState& state, CharacterReadings& held, bool loaded);

    // The parts of BarState that the shown widgets and the open popup read, for the plugin to gather only those; it
    // always copies the weather, the owners and the shares, which GameState keeps from packets.
    struct BarNeeds
    {
        bool unixTime   = false; // unixMs
        bool localTime  = false; // the earth date and time
        bool zone       = false;
        bool target     = false; // with its placeholder's countdown and a player's check
        bool gil        = false;
        bool strip      = false; // the game's top-strip text
        bool frames     = false;
        bool job        = false;
        bool connection = false;
        bool party      = false;
        bool statuses   = false; // read with the clock, which their timers count from
        bool memory     = false;
        bool cpu        = false;
        bool linuxxi    = false;
        bool conquest   = false; // your nation and supply runs, for World Info's conquest and Currency's points
        int containers = 0; // bags from 0 up to this
    };
    BarNeeds NeedsOf(const Settings& settings, const std::vector<PanelWidget>& showing); // showing: the popups shown
    // Does to the settings what a press on a popup's pressable cell told (PopupCell::press): a currency's or a
    // status's star adds it to the favorites or takes it off, and a status's pin shows it on the panel or takes it
    // off. The last pin stays, by its pin or its star: Refused.
    enum class PressResult : uint8_t { None, Changed, Refused };
    // A weather's icons as Use Custom Icons picks them: HeadsUp's own picture, or the game's element icon, twice for a
    // double weather; none for a weather that has none of the kind picked.
    std::vector<Icon> WeatherIcons(Weather weather, CustomIcons use);
    // What a popup's clone and remove chips tell, then the widget's slot.
    constexpr const char* kWidgetClonePress = "widget clone ", *kWidgetRemovePress = "widget remove ";
    constexpr const char* kCollapsePress = "collapse "; // and the card's CollapseKey
    constexpr const char* kPopupResetPress = "popup reset "; // and its pill's slot: back under its pill
    // A pop-out card's name in Settings::collapsedSections: its pill's and its heading's without what is in brackets,
    // so the Today card is one card whatever the date. One word: lower case, a '-' for each space.
    std::string CollapseKey(PanelWidget head, std::string_view heading);
    // Clones the widget a press names or takes its copy off: the widget whose settings show next, the clone or the
    // first copy; nothing for any other press, or when nothing changed.
    std::optional<PanelWidget> ApplyWidgetPress(Settings& settings, std::string_view press);
    PressResult ApplyPopupPress(Settings& settings, std::string_view press, Nation nation);
    // Whether the conquest standings show: the Zone widget, a World Info showing a nation, or a popup in showing with
    // the Zone's or World Info's cards.
    bool StandingsShown(const Settings& settings, const std::vector<PanelWidget>& showing);
    // Whether a popup shown shows a widget's: its own, a linked pill's with that widget in it, or one whose design
    // holds one of that widget's cards.
    bool PopupShows(const Settings& settings, const std::vector<PanelWidget>& shown, BarWidget widget);

    // The picture each part's chip in the Widget Designer shows: the one these widgets show now, else its stand-in.
    std::array<PartMark, kWidgetPartCount> PartMarksNow(const std::vector<PanelWidget>& widgets, const BarState& state,
        const Settings& settings);

    // Game help is never shown on the bar: it has a bar of its own, under the top bar.
    WidgetContent BarWidgetContent(PanelWidget widget, const BarState& state, const Settings& settings);
    // Whether a widget whose pill is pill is laid out on the bar: shown, and not idle unless its settings show.
    bool TakesRoom(const WidgetContent& content, PanelWidget pill, std::optional<PanelWidget> settingsShown);
    // The used and total slots of the bag the game's caption names, as the Bags widget writes them.
    std::optional<Span> StripBagFill(const BarState& state);
    PopupContent BarPopupContent(PanelWidget widget, const BarState& state, const Settings& settings);
    // The popup a pill opens, named by its first widget: that widget's, or for a combined pill each of its widgets'
    // that is shown and has one, their titles joined and their cards one after another.
    PopupContent PanelPopupContent(PanelWidget head, const BarState& state, const Settings& settings);
    // A widget's currency's picture (kCurrencyTable's place, or kYourConquestCurrency) as a span of no text: gil's coin,
    // a nation's crest for its conquest points, or the game's item; one with none has no mark (HasMark).
    Span CurrencyMark(int currency, Nation yours);
    // The element colors of the weekdays and weather, from the spec.
    uint32_t ElementColor(Element element);
}

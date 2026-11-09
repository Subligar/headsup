#include "bar_statuses.h"

#include "bar_look.h"      // kStarColor
#include "player_status.h" // kStatusIconSlots
#include "status_icons.h"
#include "vanadiel.h"      // kVanaEpochUnixMs

#include <algorithm>
#include <iterator>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr StatusRecord kStatuses[] = {
#include "generated/statuses.inc"
        };
        constexpr int16_t kNoStatus = 0xFF; // an empty buff slot, as the game's packets have it; Ashita's is -1

        // A status's name for the popup and the panel: Phoenix's, or its number for one Phoenix lacks.
        std::string NameOf(uint16_t id)
        {
            const char* name = StatusName(id);
            return *name != '\0' ? std::string(name) : "Status " + std::to_string(id);
        }

        // Whether you have a status of this name, and the most seconds any of them has left: nothing when one of them
        // never runs out.
        struct Held
        {
            bool held = false;
            std::optional<double> secondsLeft;
        };
        // The statuses you have by name, named once.
        struct Named
        {
            std::string name;
            std::optional<double> secondsLeft;
        };
        std::vector<Named> NamedAll(const std::vector<ActiveStatus>& active)
        {
            std::vector<Named> out;
            for (const ActiveStatus& status : active)
                out.push_back(Named{NameOf(status.id), status.secondsLeft});
            return out;
        }
        Held HeldAs(const std::vector<Named>& active, const std::string& name)
        {
            bool endless = false;
            double most  = 0.0;
            Held out;
            for (const Named& status : active)
                if (status.name == name)
                {
                    out.held = true;
                    endless  = endless || !status.secondsLeft;
                    most     = std::max(most, status.secondsLeft.value_or(0.0));
                }
            if (out.held && !endless) out.secondsLeft = most;
            return out;
        }

        // Its time left, as the game counts it down; "-" for one that never runs out, and 0 for one you do not have.
        Span TimeOf(const Held& held)
        {
            if (!held.held) return Text(SpanText(0.0), Tone::Dim);
            return held.secondsLeft ? Text(SpanText(*held.secondsLeft)) : Text("-", Tone::Dim);
        }

        PopupCell Pressable(Icon icon, Span tint, std::string press)
        {
            PopupCell cell;
            cell.icon  = icon;
            cell.tint  = std::move(tint);
            cell.press = std::move(press);
            return cell;
        }

        StatusMark MarkOf(uint16_t id, const BarPalette& theme) { return StatusMark{id, theme.statusIcons, theme.statusBack}; }

        PopupCell Right(Span text)
        {
            PopupCell cell = Cell(std::move(text));
            cell.align     = Align::Right;
            return cell;
        }

        // Every status Phoenix has once by name, sorted by name, each with its name's first id.
        using NamedId = std::pair<std::string, uint16_t>;
        const std::vector<NamedId>& ByName()
        {
            static const std::vector<NamedId> byName = [] {
                std::vector<NamedId> out;
                for (const StatusRecord& record : StatusRecords())
                    out.emplace_back(NameOf(record.id), record.id);
                std::ranges::stable_sort(out, {}, &NamedId::first);
                const auto repeats = std::ranges::unique(out, {}, &NamedId::first);
                out.erase(repeats.begin(), repeats.end());
                return out;
            }();
            return byName;
        }
    }

    std::optional<double> StatusSecondsLeft(uint32_t raw, int64_t unixMs)
    {
        if (raw == kNoExpiry) return std::nullopt;
        const int64_t now = (unixMs - kVanaEpochUnixMs) * kStatusTicksPerSecond / kMsPerSecond;
        const auto left   = static_cast<int32_t>(raw - static_cast<uint32_t>(now));
        return left > 0 ? static_cast<double>(left) / kStatusTicksPerSecond : 0.0;
    }

    std::span<const StatusRecord> StatusRecords() { return kStatuses; }

    const char* StatusName(uint16_t id)
    {
        const auto* it = std::lower_bound(std::begin(kStatuses), std::end(kStatuses), id,
            [](const StatusRecord& r, uint16_t want) { return r.id < want; });
        return it != std::end(kStatuses) && it->id == id ? it->name : "";
    }

    std::vector<ActiveStatus> ActiveStatuses(const int16_t* buffs, const int16_t* icons, const uint32_t* timers,
        int64_t unixMs)
    {
        std::vector<ActiveStatus> out;
        if (buffs == nullptr) return out;
        for (int slot = 0; slot < kStatusIconSlots; ++slot)
        {
            const int16_t id = buffs[slot];
            if (id < 0 || id == kNoStatus) continue;
            const auto status = static_cast<uint16_t>(id);
            if (std::ranges::find(out, status, &ActiveStatus::id) != out.end()) continue;
            ActiveStatus held{status, std::nullopt};
            for (int i = 0; icons != nullptr && timers != nullptr && i < kStatusIconSlots; ++i)
                if (icons[i] == id)
                {
                    held.secondsLeft = StatusSecondsLeft(timers[i], unixMs);
                    break;
                }
            out.push_back(held);
        }
        return out;
    }

    std::vector<Span> PinnedStatus(const std::vector<ActiveStatus>& active, const std::vector<uint16_t>& pins, size_t rank,
        StatusDisplay display, const BarPalette& theme)
    {
        if (rank >= pins.size()) return {};
        const std::string name = NameOf(pins[rank]);
        std::vector<Span> out;
        if (display != StatusDisplay::Name)
        {
            Span mark   = Text("");
            mark.status = MarkOf(pins[rank], theme);
            out.push_back(std::move(mark));
        }
        if (display != StatusDisplay::Icon) out.push_back(Text(out.empty() ? name : " " + name));
        out.push_back(Text(" " + TimeOf(HeldAs(NamedAll(active), name)).text, Tone::Dim));
        return out;
    }

    PopupContent StatusTimersPopup(const std::vector<ActiveStatus>& active, const std::vector<uint16_t>& favorites,
        const std::vector<uint16_t>& pins, const BarPalette& theme, size_t buffsScrolled, size_t debuffsScrolled, bool refused)
    {
        PopupContent popup;
        popup.title     = ListOf(Text("Status Timers"));
        if (refused)
            popup.subtitle = ListOf(Colored(pins.size() >= kMostStatusPins ? "You can not pin more than 6 status effects"
                                                                             : "Must have at least one status pinned.",
                kErrorColor));
        const std::vector<Named> named = NamedAll(active);
        const auto nameCell = [&](uint16_t id, const std::string& name) {
            PopupCell cell = Cell(Text(name));
            cell.status    = MarkOf(id, theme);
            return cell;
        };
        // Favorites: a pin, a star, the icon and name, and the time, matched by name as the panel's are.
        constexpr int kFavoriteColumns = 4, kListColumns = 3;
        PopupSection top = Section(CardKind::StatusFavorites, "Favorites");
        std::vector<std::string> favoriteNames;
        if (favorites.empty())
            top.rows.push_back(LineOf("Click a status's star to keep it here, then its pin to show it on the panel.", Tone::Dim));
        else
        {
            PopupRow grid = Grid(kFavoriteColumns, favorites.size() * kFavoriteColumns);
            for (const uint16_t id : favorites)
            {
                const std::string name = NameOf(id);
                favoriteNames.push_back(name);
                const bool pinned = std::ranges::find(pins, id) != pins.end();
                grid.cells.push_back(Pressable(Icon::Pinned, Text("", pinned ? Tone::Heading : Tone::Dim),
                    kStatusPinPress + std::to_string(id)));
                grid.cells.push_back(Pressable(Icon::Star, Colored("", kStarColor), kStatusFavoritePress + std::to_string(id)));
                grid.cells.push_back(nameCell(id, name));
                grid.cells.push_back(Right(TimeOf(HeldAs(named, name))));
            }
            // Its times against the right edge, as far in as the lists' below.
            grid.spread = grid.inset = true;
            top.rows.push_back(std::move(grid));
        }
        // Every other status once by name, the buffs in one list and the debuffs in another, those you have first,
        // each part by name.
        struct Entry
        {
            uint16_t id;
            std::string name;
            Held held;
        };
        std::vector<Entry> held;
        const auto listed = [&](const std::string& name) {
            return std::ranges::find(favoriteNames, name) != favoriteNames.end() ||
                   std::ranges::find(held, name, &Entry::name) != held.end();
        };
        for (const ActiveStatus& status : active)
            if (std::string name = NameOf(status.id); !listed(name)) held.push_back(Entry{status.id, name, HeldAs(named, name)});
        std::ranges::sort(held, {}, &Entry::name);
        popup.sections.push_back(std::move(top));
        for (const bool debuffs : {false, true})
        {
            PopupRow list = Grid(kListColumns, 0);
            const auto add = [&](uint16_t id, const std::string& name, const Held& time) {
                if (IsDebuff(id) != debuffs) return;
                list.cells.push_back(Pressable(Icon::StarEmpty, Text("", Tone::Dim), kStatusFavoritePress + std::to_string(id)));
                list.cells.push_back(nameCell(id, name));
                list.cells.push_back(Right(TimeOf(time)));
            };
            for (const Entry& entry : held)
                add(entry.id, entry.name, entry.held);
            for (const auto& [name, id] : ByName())
                if (!listed(name)) add(id, name, Held{});
            list.nested   = true;
            list.spread   = true;
            list.scroll   = debuffs ? kDebuffsKey : kBuffsKey;
            list.scrolled = debuffs ? debuffsScrolled : buffsScrolled;
            PopupSection section = Section(debuffs ? CardKind::StatusDebuffs : CardKind::StatusBuffs, list.scroll);
            section.rows.push_back(std::move(list));
            popup.sections.push_back(std::move(section));
        }
        return popup;
    }
}

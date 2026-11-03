#include "bar_statuses.h"

#include "bar_look.h"      // kStarColor
#include "player_status.h" // kStatusIconSlots
#include "vanadiel.h"      // kVanaEpochUnixMs

#include <algorithm>
#include <iterator>

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

        PopupCell Right(Span text)
        {
            PopupCell cell = Cell(std::move(text));
            cell.align     = Align::Right;
            return cell;
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

    WidgetContent StatusTimersWidget(const std::vector<ActiveStatus>& active, const std::vector<uint16_t>& pins)
    {
        WidgetContent w;
        w.shown    = true;
        w.hasPopup = true;
        const std::vector<Named> named = NamedAll(active);
        for (const uint16_t pin : pins)
        {
            const std::string name = NameOf(pin);
            AddPart(w.title, ListOf(Text(name), Text(" " + TimeOf(HeldAs(named, name)).text, Tone::Dim)));
        }
        if (w.title.empty()) w.title = ListOf(Text("Status Timers", Tone::Dim));
        return w;
    }

    PopupContent StatusTimersPopup(const std::vector<ActiveStatus>& active, const std::vector<uint16_t>& favorites,
        const std::vector<uint16_t>& pins, size_t scrolled)
    {
        PopupContent popup;
        popup.title = ListOf(Text("Status Timers"));
        const std::vector<Named> named = NamedAll(active);
        // Favorites: a pin, a star, the name and the time, matched by name as the panel's are.
        constexpr int kFavoriteColumns = 4, kListColumns = 3;
        PopupSection top;
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
                grid.cells.push_back(Pressable(pinned ? Icon::Pinned : Icon::Pin, Text("", pinned ? Tone::Heading : Tone::Dim),
                    kStatusPinPress + std::to_string(id)));
                grid.cells.push_back(Pressable(Icon::Star, Colored("", kStarColor), kStatusFavoritePress + std::to_string(id)));
                grid.cells.push_back(Cell(Text(name)));
                grid.cells.push_back(Right(TimeOf(HeldAs(named, name))));
            }
            top.rows.push_back(std::move(grid));
        }
        // Every other status once by name, those you have first, each part by name.
        struct Entry
        {
            uint16_t id;
            std::string name;
            Held held;
        };
        std::vector<Entry> entries;
        const auto add = [&](uint16_t id) {
            std::string name = NameOf(id);
            if (std::ranges::find(favoriteNames, name) != favoriteNames.end() ||
                std::ranges::find(entries, name, &Entry::name) != entries.end())
                return;
            const Held held = HeldAs(named, name);
            entries.push_back(Entry{id, std::move(name), held});
        };
        for (const ActiveStatus& status : active)
            add(status.id);
        for (const StatusRecord& record : StatusRecords())
            add(record.id);
        std::ranges::sort(entries, [](const Entry& a, const Entry& b) {
            return a.held.held != b.held.held ? a.held.held : a.name < b.name;
        });
        PopupRow list = Grid(kListColumns, entries.size() * kListColumns);
        for (const Entry& entry : entries)
        {
            list.cells.push_back(Pressable(Icon::StarEmpty, Text("", Tone::Dim), kStatusFavoritePress + std::to_string(entry.id)));
            list.cells.push_back(Cell(Text(entry.name)));
            list.cells.push_back(Right(TimeOf(entry.held)));
        }
        list.nested   = true;
        list.scroll   = kStatusListKey;
        list.scrolled = scrolled;
        PopupSection all;
        all.rows.push_back(std::move(list));
        popup.sections = ListOf(std::move(top), std::move(all));
        return popup;
    }
}

#include "status_track.h"

#include "action_bits.h"
#include "bar_statuses.h"
#include "check.h"
#include "combat_popup.h"
#include "plate_settings.h"
#include "status_icons.h"

#include <algorithm>
#include <iterator>

namespace headsup
{
    namespace
    {
        // The server's msg.lua: a status on the actor rather than the target (ADD_EFFECT_SELFBUFF, STATUS_SPIKES),
        // and those whose value is a status taken off the target: removes, Erase, an item, an additional effect's
        // dispel, a steal.
        constexpr uint32_t kSelfBuffMessage = 166, kSpikesStatusMessage = 374;
        constexpr uint32_t kLossMessages[] = {83, 123, 168, 321, 341, 378, 453};
        constexpr uint16_t kIsMessage = 203, kNoLongerMessage = 204, kGainsMessage = 205, kWearsOffMessage = 206;
        // Phoenix's ActionCategory starts: a cast, a weapon skill's or an ability's readying, an item's use, a ranged
        // aim. A start or an interrupt can come from an entity just as a sleep lands on it.
        constexpr uint32_t kNoCategory = 0, kSpellCategory = 4;
        constexpr uint32_t kStartCategories[] = {7, 8, 9, 10, 12};
        // Statuses that stop all action, so one who acts has none of them.
        constexpr uint16_t kSleepIIStatus = 19, kLullabyStatus = 193, kStunStatus = 10, kTerrorStatus = 28;
        constexpr uint16_t kStillStatuses[] = {
            kSleepStatus, kSleepIIStatus, kLullabyStatus, kPetrificationStatus, kStunStatus, kTerrorStatus};

        struct SpellDuration
        {
            uint16_t spell, seconds;
        };
        constexpr SpellDuration kSpellDurations[] = {
#include "generated/spell_durations.inc"
        };

        bool IsLossMessage(uint32_t message) { return std::ranges::find(kLossMessages, message) != std::end(kLossMessages); }

        uint16_t SpellSeconds(uint32_t category, uint32_t spell)
        {
            if (category != kSpellCategory) return 0;
            const auto* it = std::ranges::lower_bound(kSpellDurations, spell, {}, &SpellDuration::spell);
            return it != std::end(kSpellDurations) && it->spell == spell ? it->seconds : 0;
        }

        // A result part's value and message as a change, when they name a status.
        void PutChange(std::vector<StatusChange>& changes, uint32_t target, uint32_t actor, uint32_t value, uint32_t message,
                       uint16_t seconds)
        {
            const auto status = static_cast<uint16_t>(value);
            if (IsStatusMessage(message))
                changes.push_back(
                    {message == kSelfBuffMessage || message == kSpikesStatusMessage ? actor : target, status, true, seconds});
            else if (IsLossMessage(message))
                changes.push_back({target, status, false});
        }
    }

    std::optional<ActionStatuses> ParseActionStatuses(const uint8_t* data, uint32_t size)
    {
        const std::optional<ParsedAction> parsed = ParseAction(data, size);
        if (!parsed) return std::nullopt;
        const uint16_t seconds = SpellSeconds(parsed->category, parsed->argument);
        const bool started = std::ranges::find(kStartCategories, parsed->category) != std::end(kStartCategories);
        ActionStatuses action{parsed->actor, parsed->category != kNoCategory && !started, {}};
        for (const ParsedResult& r : parsed->results)
        {
            PutChange(action.changes, r.target, parsed->actor, r.value, r.message, seconds);
            PutChange(action.changes, r.target, parsed->actor, r.addedValue, r.addedMessage, seconds);
            PutChange(action.changes, r.target, parsed->actor, r.spikesValue, r.spikesMessage, seconds);
        }
        return action;
    }

    std::optional<StatusChange> ParseStatusMessage(const uint8_t* data, uint32_t size)
    {
        const std::optional<BasicMessage> m = ParseBasicMessage(data, size);
        if (!m) return std::nullopt;
        const bool gained = m->message == kIsMessage || m->message == kGainsMessage;
        if (!gained && m->message != kNoLongerMessage && m->message != kWearsOffMessage) return std::nullopt;
        return StatusChange{m->target, static_cast<uint16_t>(m->param), gained};
    }

    void StatusTrack::Apply(const StatusChange& change, double now)
    {
        if (*StatusName(change.status) == '\0') return;
        std::vector<Held>& held = m_Held[change.entity];
        std::erase_if(held, [&](const Held& h) { return h.status == change.status; });
        const double cap = IsDebuff(change.status) ? kTrackedDebuffSeconds : kTrackedBuffSeconds;
        if (change.gained) held.push_back({change.status, now + (change.seconds > 0 ? change.seconds : cap)});
        if (held.empty()) m_Held.erase(change.entity);
    }

    void StatusTrack::Acted(uint32_t actor)
    {
        const auto it = m_Held.find(actor);
        if (it == m_Held.end()) return;
        std::erase_if(it->second, [](const Held& h) { return std::ranges::find(kStillStatuses, h.status) != std::end(kStillStatuses); });
    }

    void StatusTrack::Keep(std::span<const uint32_t> living)
    {
        std::erase_if(m_Held, [&](const auto& entry) { return std::ranges::find(living, entry.first) == living.end(); });
    }

    StatusList StatusTrack::Statuses(uint32_t entity, double now) const
    {
        StatusList list;
        if (const auto it = m_Held.find(entity); it != m_Held.end())
            for (const Held& h : it->second)
                if (now < h.until) list.Add(h.status);
        return list;
    }
}

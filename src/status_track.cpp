#include "status_track.h"

#include "action_bits.h"
#include "bar_statuses.h"
#include "check.h"
#include "combat_popup.h"
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
        constexpr uint32_t kNoCategory = 0;
        constexpr uint32_t kStartCategories[] = {7, 8, 9, 10, 12};
        // Statuses that stop all action, so one who acts has none of them: Sleep, Sleep II, Lullaby, Petrification,
        // Stun, Terror.
        constexpr uint16_t kStillStatuses[] = {2, 19, 193, 7, 10, 28};

        bool IsLossMessage(uint32_t message) { return std::ranges::find(kLossMessages, message) != std::end(kLossMessages); }

        // A result part's value and message as a change, when they name a status.
        void PutChange(std::vector<StatusChange>& changes, uint32_t target, uint32_t actor, uint32_t value, uint32_t message)
        {
            const auto status = static_cast<uint16_t>(value);
            if (IsStatusMessage(message))
                changes.push_back({message == kSelfBuffMessage || message == kSpikesStatusMessage ? actor : target, status, true});
            else if (IsLossMessage(message))
                changes.push_back({target, status, false});
        }
    }

    std::optional<ActionStatuses> ParseActionStatuses(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr) return std::nullopt;
        BitReader bits(data, size);
        const std::optional<uint32_t> actor = bits.Read(kActorBits), targets = bits.Read(kTargetsBits);
        bits.Skip(kResultsSumBits);
        const std::optional<uint32_t> category = bits.Read(kCommandBits);
        if (!actor || !targets || !category) return std::nullopt;
        bits.Skip(kArgumentBits + kInfoBits);
        const bool started = std::ranges::find(kStartCategories, *category) != std::end(kStartCategories);
        ActionStatuses action{*actor, *category != kNoCategory && !started, {}};
        for (uint32_t t = 0; t < *targets; ++t)
        {
            const std::optional<uint32_t> target = bits.Read(kTargetIdBits), results = bits.Read(kResultsBits);
            if (!target || !results) return std::nullopt;
            for (uint32_t r = 0; r < *results; ++r)
            {
                bits.Skip(kResultLeadBits);
                const std::optional<uint32_t> value = bits.Read(kValueBits), message = bits.Read(kMessageBits);
                bits.Skip(kModifierBits);
                const std::optional<uint32_t> added = bits.Read(1);
                std::optional<uint32_t> addedValue = 0, addedMessage = 0;
                if (added && *added)
                {
                    bits.Skip(kAddedLeadBits);
                    addedValue   = bits.Read(kValueBits);
                    addedMessage = bits.Read(kMessageBits);
                }
                const std::optional<uint32_t> spikes = bits.Read(1);
                std::optional<uint32_t> spikesValue = 0, spikesMessage = 0;
                if (spikes && *spikes)
                {
                    bits.Skip(kSpikesLeadBits);
                    spikesValue   = bits.Read(kSpikesValueBits);
                    spikesMessage = bits.Read(kMessageBits);
                }
                if (!value || !message || !addedValue || !addedMessage || !spikes || !spikesValue || !spikesMessage)
                    return std::nullopt;
                PutChange(action.changes, *target, *actor, *value, *message);
                PutChange(action.changes, *target, *actor, *addedValue, *addedMessage);
                PutChange(action.changes, *target, *actor, *spikesValue, *spikesMessage);
            }
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
        if (change.gained)
            held.push_back({change.status, now + (IsDebuff(change.status) ? kTrackedDebuffSeconds : kTrackedBuffSeconds)});
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

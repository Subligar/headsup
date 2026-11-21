#pragma once

#include "player_status.h"

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // A status an entity (by server id) gained or lost, as a packet said.
    struct StatusChange
    {
        uint32_t entity = 0;
        uint16_t status = 0;
        bool gained     = false;
        bool operator==(const StatusChange&) const = default;
    };

    // What an action packet (0x028) says of statuses: who acted, whether the action was done (not a start or an
    // interrupt) and each status put on or taken off a target or the actor.
    struct ActionStatuses
    {
        uint32_t actor = 0;
        bool finished  = false;
        std::vector<StatusChange> changes;
    };
    std::optional<ActionStatuses> ParseActionStatuses(const uint8_t* data, uint32_t size);
    // A 0x029 that says a status began or ended: "is", "gains", "is no longer", "wears off".
    std::optional<StatusChange> ParseStatusMessage(const uint8_t* data, uint32_t size);

    // shortcut: one cap per kind since no packet tells a bystander an end; per-status durations if icons linger
    constexpr double kTrackedDebuffSeconds = 180.0, kTrackedBuffSeconds = 1800.0;

    // The statuses of entities HeadsUp has no list of (alliance members, strangers, mobs), from what the packets
    // said. Phoenix tells a wear-off only to the one who wore it and its caster: every other status ends when the
    // entity acts (those that stop all action), dies, leaves or reaches its kind's cap.
    class StatusTrack
    {
    public:
        // A gain again restarts its cap; a status Phoenix has no name for is not kept.
        void Apply(const StatusChange& change, double now);
        void Acted(uint32_t actor);
        // Every entity not in living loses all it had.
        void Keep(std::span<const uint32_t> living);
        // In the order gained, those not past their cap.
        StatusList Statuses(uint32_t entity, double now) const;

    private:
        struct Held
        {
            uint16_t status;
            double until;
        };
        std::unordered_map<uint32_t, std::vector<Held>> m_Held;
    };
}

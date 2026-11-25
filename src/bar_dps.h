#pragma once

#include "bar_content.h"
#include "bar_design.h"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace headsup
{
    constexpr int kMinDpsSeconds = 3, kMaxDpsSeconds = 60, kDefaultDpsSeconds = 10;

    struct Fighter
    {
        uint32_t id = 0, pet = 0; // server ids; pet 0 for none
        std::string name;
    };
    struct DpsRow
    {
        std::string name;
        double dps = 0.0;
        bool you   = false;
        bool operator==(const DpsRow&) const = default;
    };

    // Your party's damage of the last kMaxDpsSeconds, by who dealt it or whose pet did.
    class DamageMeter
    {
    public:
        void SetFighters(std::vector<Fighter> fighters); // your party, you first; none while no DPS widget shows
        bool InParty() const { return m_Fighters.size() > 1; }
        // A fighter's or a fighter's pet's damage; anyone else's is dropped.
        void Add(double now, uint32_t actor, uint32_t damage);
        // Each fighter with damage under seconds old, that damage over seconds, highest first; a pet's only with pets.
        std::vector<DpsRow> Rows(double now, int seconds, bool pets) const;

    private:
        struct Hit
        {
            double at;
            uint32_t fighter; // the owner's id, a pet's too
            uint32_t damage;
            bool pet;
        };
        std::vector<Fighter> m_Fighters;
        std::deque<Hit> m_Hits;
    };

    // Your DPS, and the party's while you are in one; base takes its pop-out while anyone dealt damage.
    std::vector<PartContent> DpsParts(const std::vector<DpsRow>& rows, bool party, WidgetContent& base);
    PopupContent DpsPopup(const std::vector<DpsRow>& rows);
}

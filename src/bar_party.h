#pragma once

#include "argb.h"
#include "bar_content.h"
#include "bar_design.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace headsup
{
    constexpr int kPartySize          = 6;    // you and five others; the alliance's other parties are not counted
    constexpr uint32_t kWeaponSkillTp = 1000; // TP a weapon skill needs
    constexpr uint32_t kReadyColor    = kBarGreen;

    struct PartyMember
    {
        std::string name;
        uint8_t mainJob = 0, mainLevel = 0, subJob = 0, subLevel = 0;
        uint32_t hp = 0, mp = 0, tp = 0;
        uint8_t hpPercent = 0, mpPercent = 0;
        uint16_t zone = 0;
        bool here     = true; // in your zone; the game's figures for a member elsewhere are stale
        bool operator==(const PartyMember&) const = default;
    };
    using Party = std::array<std::optional<PartyMember>, kPartySize>; // by party slot, you first

    // Whether the job has MP of its own, as Phoenix's grades.yaml gives it.
    bool JobHasMp(uint8_t job);
    // From the main job, or a subjob with levels.
    bool UsesMp(const PartyMember& member);

    // "Party 3" or "Solo", and the lowest HP and MP of the members here; base takes its popup in a party.
    std::vector<PartContent> PartyParts(const Party& party, WidgetContent& base);
    PopupContent PartyPopup(const Party& party);
}

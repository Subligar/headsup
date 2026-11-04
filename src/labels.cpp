#include "labels.h"

#include <cstdio>
#include <optional>

namespace headsup
{
    namespace
    {
        constexpr LabelShade kConShades[kConCount] = {LabelShade::TooWeak, LabelShade::EasyPrey, LabelShade::EasyPrey,
            LabelShade::DecentChallenge, LabelShade::EvenMatch, LabelShade::Tough, LabelShade::VeryTough,
            LabelShade::IncrediblyTough};

        // The con a check gave, else the cons of the mob's level range against yours; none while either is unknown.
        struct ConRange
        {
            Con easiest, hardest;
        };
        std::optional<ConRange> ConsOf(const MobRecord* mob, const CheckResult* checked, int playerLevel)
        {
            if (checked != nullptr && checked->level > 0) return ConRange{checked->con, checked->con};
            if (mob == nullptr || mob->maxLevel == 0 || playerLevel <= 0) return std::nullopt;
            return ConRange{Difficulty(playerLevel, mob->minLevel + mob->expLevelMod),
                Difficulty(playerLevel, mob->maxLevel + mob->expLevelMod)};
        }

        // One con's name, or the two ends' with between them.
        std::string RangeText(const ConRange& cons, const char* (*name)(Con), const char* between)
        {
            return cons.easiest == cons.hardest ? name(cons.hardest) : std::string(name(cons.easiest)) + between + name(cons.hardest);
        }
    }

    std::string IdText(uint32_t serverId)
    {
        char text[16];
        std::snprintf(text, sizeof(text), "[%03X]", static_cast<unsigned>(serverId & kTargetIndexBits));
        return text;
    }

    std::string ServerIdText(uint32_t serverId)
    {
        char text[16];
        std::snprintf(text, sizeof(text), "[%u]", static_cast<unsigned>(serverId));
        return text;
    }

    LabelShade ShadeFor(Con con)
    {
        const auto i = static_cast<unsigned>(con);
        return i < kConCount ? kConShades[i] : LabelShade::Unknown;
    }

    std::string LevelText(const MobRecord* mob, const CheckResult* checked)
    {
        if (checked != nullptr && checked->level > 0) return std::to_string(checked->level);
        if (mob == nullptr || mob->maxLevel == 0) return "?";
        if (mob->minLevel == mob->maxLevel) return std::to_string(mob->maxLevel);
        return std::to_string(mob->minLevel) + "-" + std::to_string(mob->maxLevel);
    }

    std::string ConText(const MobRecord* mob, const CheckResult* checked, int playerLevel)
    {
        const std::optional<ConRange> cons = ConsOf(mob, checked, playerLevel);
        return cons ? RangeText(*cons, Abbrev, "-") : "??";
    }

    Label MakeLabel(const MobRecord* mob, const CheckResult* checked, int playerLevel)
    {
        Label label{};
        const std::optional<ConRange> cons = ConsOf(mob, checked, playerLevel);
        label.shade             = cons ? ShadeFor(cons->hardest) : LabelShade::Unknown;
        const std::string level = LevelText(mob, checked);
        const std::string text  = "Lv " + level + " " + ConText(mob, checked, playerLevel);
        std::snprintf(label.text, sizeof(label.text), "%s", text.c_str());
        std::snprintf(label.level, sizeof(label.level), "%s", level.c_str());
        std::snprintf(label.con, sizeof(label.con), "%s", cons ? RangeText(*cons, ConName, " to ").c_str() : "");
        return label;
    }
}

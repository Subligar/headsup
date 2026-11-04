#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // Phoenix's daily guild point turn-ins, from its website's API. The server picks one of 8 patterns at random when
    // the day changes in Japan (guildutils.cpp UpdateGuildPointsPattern), so only the server or the site knows it.
    constexpr const wchar_t* kGuildPatternsHost = L"api.phoenix-xi.com";
    constexpr const wchar_t* kGuildPatternsPath = L"/api/v1/server/guild-patterns";
    // The craft guilds, by the site's guild id: Fishing to Cooking, whose craft skills are kFirstCraftSkill on.
    constexpr int kCraftGuilds = 9, kFirstCraftSkill = 48;
    constexpr uint8_t kCraftRankCount = 10;
    constexpr const char* kCraftRankNames[kCraftRankCount] = {"Amateur", "Recruit", "Initiate", "Novice", "Apprentice",
        "Journeyman", "Craftsman", "Artisan", "Adept", "Veteran"};

    struct GuildTurnIn
    {
        uint8_t rank = 0;      // the site's: the rank of the guild's table, not of who is asked for it
        uint16_t item = 0;
        std::string itemName;  // empty when the site has none
        int points = 0, maxPoints = 0; // each, and the most a day
        bool operator==(const GuildTurnIn&) const = default;
    };
    struct GuildPatterns
    {
        int pattern = 0;
        std::array<std::vector<GuildTurnIn>, kCraftGuilds> guilds; // by guild id
        bool operator==(const GuildPatterns&) const = default;
    };

    // The guild the shops name "Fishing" ... "Cooking".
    std::optional<int> CraftGuildNamed(std::string_view name);
    // The site's answer, or nothing for any other: a guild whose name is not its id's is left out.
    std::optional<GuildPatterns> ReadGuildPatterns(std::string_view json);

    // The answer kept on disk: the day in Japan it was fetched on (JstDay), then the site's text as it came.
    struct SavedGuildPatterns
    {
        int64_t day = 0;
        std::string json;
    };
    std::string GuildPatternsFile(int64_t day, std::string_view json);
    std::optional<SavedGuildPatterns> ReadGuildPatternsFile(std::string_view text);
    // The table's rank the guild asks a crafter of this rank for today. Phoenix steps each rank's daily through the
    // ranks up to its own (guild.cpp updateGuildPointsPattern), and counts those below Novice as Novice.
    uint8_t AskedRank(uint8_t craftRank, int64_t unixMs);
    // Fetched again only on a day in Japan it was not: once a day, and just after the reset.
    bool GuildPatternsDue(std::optional<int64_t> savedDay, int64_t unixMs);
}

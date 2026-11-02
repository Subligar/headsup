#include "profiles.h"

#include <algorithm>

namespace headsup
{
    namespace
    {
        constexpr std::string_view kLastKey = "*last"; // no character's name starts with a mark
        constexpr char kEquals = '=';

        std::string_view Trimmed(std::string_view text)
        {
            const size_t start = text.find_first_not_of(" \t\r");
            if (start == std::string_view::npos) return {};
            return text.substr(start, text.find_last_not_of(" \t\r") - start + 1);
        }
    }

    CharacterProfiles ReadCharacterProfiles(std::string_view text)
    {
        CharacterProfiles profiles;
        while (!text.empty())
        {
            const size_t end           = text.find('\n');
            const std::string_view row = text.substr(0, end);
            text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
            const size_t equals = row.find(kEquals);
            if (equals == std::string_view::npos) continue;
            const std::string_view key = Trimmed(row.substr(0, equals)), value = Trimmed(row.substr(equals + 1));
            if (key.empty() || value.empty()) continue;
            if (key == kLastKey)
                profiles.last = value;
            else
                profiles.byCharacter[std::string(key)] = value;
        }
        return profiles;
    }

    std::string CharacterProfilesText(const CharacterProfiles& profiles)
    {
        std::string text;
        if (!profiles.last.empty()) text += std::string(kLastKey) + kEquals + profiles.last + "\n";
        for (const auto& [character, profile] : profiles.byCharacter)
            text += character + kEquals + profile + "\n";
        return text;
    }

    std::vector<std::string> ProfileNames(const std::vector<std::string>& files)
    {
        const std::string_view suffix = kProfileSuffix;
        std::vector<std::string> names;
        for (const std::string& file : files)
            if (file.size() > suffix.size() && file.ends_with(suffix)) names.push_back(file.substr(0, file.size() - suffix.size()));
        std::ranges::sort(names);
        return names;
    }

    void RenameProfile(CharacterProfiles& profiles, const std::string& from, const std::string& to)
    {
        for (auto& [character, profile] : profiles.byCharacter)
            if (profile == from) profile = to;
        if (profiles.last == from) profiles.last = to;
    }

    void ForgetProfile(CharacterProfiles& profiles, const std::string& name)
    {
        std::erase_if(profiles.byCharacter, [&](const auto& entry) { return entry.second == name; });
        if (profiles.last == name) profiles.last.clear();
    }
}

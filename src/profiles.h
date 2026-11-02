#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // Settings profiles: named files in config/headsup/profiles, each character using one. characters.ini says which,
    // a "Name=profile" line a character, and which was used last, for the title screen before anyone logs in.
    constexpr const char* kProfilesFolder  = "profiles";
    constexpr const char* kProfileSuffix   = ".ini";
    constexpr const char* kCharactersFile  = "characters.ini";
    struct CharacterProfiles
    {
        std::map<std::string, std::string> byCharacter;
        std::string last;
        bool operator==(const CharacterProfiles&) const = default;
    };
    CharacterProfiles ReadCharacterProfiles(std::string_view text);
    std::string CharacterProfilesText(const CharacterProfiles& profiles);
    // The profiles a folder's file names hold, sorted, each without its suffix.
    std::vector<std::string> ProfileNames(const std::vector<std::string>& files);
    // A profile renamed or deleted: every character that used it uses the new name, or none, and picks again at login.
    void RenameProfile(CharacterProfiles& profiles, const std::string& from, const std::string& to);
    void ForgetProfile(CharacterProfiles& profiles, const std::string& name);
}

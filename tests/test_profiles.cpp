#include "profiles.h"
#include "test.h"

using namespace headsup;

TEST(characters_ini_says_each_characters_profile_and_the_last_used)
{
    const CharacterProfiles read = ReadCharacterProfiles("*last=Fishing\r\nCarrott = whatever\nGodai=Godai\n\nnonsense\n=x\n");
    CHECK_EQ(read.last, "Fishing");
    CHECK((read.byCharacter == std::map<std::string, std::string>{{"Carrott", "whatever"}, {"Godai", "Godai"}}));
    CHECK(ReadCharacterProfiles(CharacterProfilesText(read)) == read);
    CHECK(ReadCharacterProfiles("") == CharacterProfiles{});
}

TEST(a_profiles_folder_lists_its_ini_files_by_name)
{
    CHECK((ProfileNames({"whatever.ini", "Carrott.ini", "notes.txt", ".ini", "a.ini.new"}) ==
           std::vector<std::string>{"Carrott", "whatever"}));
}

TEST(renaming_or_deleting_a_profile_moves_every_character_that_used_it)
{
    CharacterProfiles p{{{"Carrott", "whatever"}, {"Godai", "whatever"}, {"Lia", "Lia"}}, "whatever"};
    RenameProfile(p, "whatever", "Fishing");
    CHECK((p.byCharacter == std::map<std::string, std::string>{{"Carrott", "Fishing"}, {"Godai", "Fishing"}, {"Lia", "Lia"}}));
    CHECK_EQ(p.last, "Fishing");
    ForgetProfile(p, "Fishing");
    CHECK((p.byCharacter == std::map<std::string, std::string>{{"Lia", "Lia"}}));
    CHECK(p.last.empty());
}

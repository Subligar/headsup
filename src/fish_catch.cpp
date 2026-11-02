#include "fish_catch.h"

#include "plate_template.h"
#include "settings.h"

#include <algorithm>
#include <iterator>

namespace headsup
{
    namespace
    {
        // Phoenix's FISHMESSAGEOFFSET_ numbers (fishingutils.h), and what the bar says for each: change the words here.
        struct SizeWords
        {
            uint16_t message;
            FishSize size;
            const char* words;
        };
        constexpr SizeWords kSizes[] = {{0x08, FishSize::Small, "Small Fish"}, {0x32, FishSize::Large, "Large Fish"},
            {0x33, FishSize::Item, "Item"}, {0x34, FishSize::Monster, "Monster"}};
        struct FeelingWords
        {
            uint16_t message;
            FishFeeling feeling;
            const char* words;
        };
        // The keen angler's sense (0x35) comes in place of a good feeling and names the fish, which HeadsUp never says.
        constexpr FeelingWords kFeelings[] = {{0x29, FishFeeling::Good, "Good Feeling"},
            {0x35, FishFeeling::Good, "Good Feeling"}, {0x2A, FishFeeling::Bad, "Bad Feeling"},
            {0x2B, FishFeeling::Terrible, "Terrible Feeling"}, {0x2C, FishFeeling::LowSkill, "Low Skill"},
            {0x2D, FishFeeling::VeryLowSkill, "Very Low Skill"},
            {0x2E, FishFeeling::ExtremelyLowSkill, "Extremely Low Skill"}, {0x36, FishFeeling::Epic, "Epic Catch"}};
        constexpr const char* kFishWord = "Fish", *kLineSnapWords = "Line Break", *kRodBreakWords = "Rod Break";

        // Phoenix warns of a line that will snap only with a bad feeling, and of a rod that will break only with a
        // terrible one (CalculateFishSense).
        bool LineMaySnap(const FishCatch& fish) { return fish.feeling == FishFeeling::Bad; }
        bool RodMayBreak(const FishCatch& fish) { return fish.feeling == FishFeeling::Terrible; }

        std::string TagWords(Tag tag, const FishCatch& fish)
        {
            switch (tag)
            {
            case Tag::Fish: return kFishWord;
            case Tag::FishSize:
                for (const SizeWords& s : kSizes)
                    if (fish.size == s.size) return s.words;
                return {};
            case Tag::Feeling:
                for (const FeelingWords& f : kFeelings)
                    if (fish.feeling == f.feeling) return f.words;
                return {};
            case Tag::LineSnap: return LineMaySnap(fish) ? kLineSnapWords : "";
            case Tag::RodBreak: return RodMayBreak(fish) ? kRodBreakWords : "";
            default: return {};
            }
        }
    }

    void ReadFishMessage(FishCatch& fish, uint16_t message)
    {
        if (const auto s = std::ranges::find(kSizes, message, &SizeWords::message); s != std::end(kSizes))
            fish = FishCatch{s->size, std::nullopt};
        else if (const auto f = std::ranges::find(kFeelings, message, &FeelingWords::message); f != std::end(kFeelings))
            fish.feeling = f->feeling;
    }

    std::vector<Tag> FishBarTags() { return {Tag::Fish, Tag::FishSize, Tag::Feeling, Tag::LineSnap, Tag::RodBreak}; }

    std::string FishLine(std::string_view line, const FishCatch& fish)
    {
        std::string text;
        for (const Atom& atom : ParseTemplate(line))
        {
            if (atom.slot.empty())
            {
                text += atom.text;
                continue;
            }
            for (const Tag tag : atom.slot)
                if (std::string words = TagWords(tag, fish); !words.empty())
                {
                    text += words;
                    break;
                }
        }
        return text;
    }

    std::optional<uint32_t> FishFill(const Settings& s, const FishCatch& fish)
    {
        if (!s.fishColorByCatch || !fish.size) return std::nullopt;
        if (RodMayBreak(fish)) return ToArgb(s.fishRodColor);
        if (LineMaySnap(fish)) return ToArgb(s.fishLineColor);
        switch (*fish.size)
        {
        case FishSize::Small: return ToArgb(s.fishSmallColor);
        case FishSize::Large: return ToArgb(s.fishLargeColor);
        case FishSize::Item: return ToArgb(s.fishItemColor);
        case FishSize::Monster: return ToArgb(s.fishMonsterColor);
        }
        return std::nullopt;
    }
}

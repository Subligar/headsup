#include "icon_adjust.h"

#include "game_font.h"
#include "markup.h"

namespace headsup
{
    namespace
    {
        // Every crest read larger than the icons beside it.
        constexpr float kCrestScale = 0.85f;

        struct FileAdjust
        {
            std::string_view file;
            IconAdjust adjust;
        };
        // The one table of adjusted icons, by file name; any icon not here is drawn as it is.
        constexpr FileAdjust kAdjusts[] = {
            {"bag.png", {0.0f, 1.0f, 1.0f}},
            {"clock.png", {0.0f, 0.0f, 0.8f}},
            {kSanDoriaCrestChunk, {0.0f, 0.0f, kCrestScale}},
            {kBastokCrestChunk, {0.0f, 0.0f, kCrestScale}},
            {kWindurstCrestChunk, {0.0f, 0.0f, kCrestScale}},
            {kBeastmenCrestChunk, {0.0f, 0.0f, kCrestScale}},
            {kJeunoCrestChunk, {0.0f, 0.0f, kCrestScale}},
            {kAhtUrhganCrestChunk, {0.0f, 0.0f, kCrestScale}},
        };

        // A game picture's file name; PlayOnline's icons share one picture, so they have none of their own.
        std::string_view FileOf(GamePicture picture)
        {
            switch (picture)
            {
            case GamePicture::SanDoriaCrest: return kSanDoriaCrestChunk;
            case GamePicture::BastokCrest: return kBastokCrestChunk;
            case GamePicture::WindurstCrest: return kWindurstCrestChunk;
            case GamePicture::BeastmenCrest: return kBeastmenCrestChunk;
            case GamePicture::JeunoCrest: return kJeunoCrestChunk;
            case GamePicture::AhtUrhganCrest: return kAhtUrhganCrestChunk;
            case GamePicture::None:
            case GamePicture::PlayOnlineMail:
            case GamePicture::PlayOnlineMessenger: return {};
            }
            return {};
        }
    }

    IconAdjust AdjustOf(std::string_view file)
    {
        for (const FileAdjust& entry : kAdjusts)
            if (entry.file == file) return entry.adjust;
        return {};
    }

    IconAdjust AdjustOf(Icon icon) { return AdjustOf(kIconFiles[static_cast<size_t>(icon)]); }

    IconAdjust AdjustOf(GamePicture picture) { return AdjustOf(FileOf(picture)); }

    std::string AdjustRules(const IconAdjust& adjust)
    {
        std::string transform;
        if (adjust.x != 0.0f || adjust.y != 0.0f) transform += "translate(" + Px(adjust.x) + "," + Px(adjust.y) + ")";
        if (adjust.scale != 1.0f) transform += (transform.empty() ? "" : " ") + std::string("scale(") + RcssNumber(adjust.scale) + ")";
        return transform.empty() ? std::string() : "transform:" + transform + ";";
    }

    std::string AdjustStyle(const IconAdjust& adjust)
    {
        const std::string rules = AdjustRules(adjust);
        return rules.empty() ? rules : " style=\"" + rules + "\"";
    }
}

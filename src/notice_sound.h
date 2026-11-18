#pragma once

#include "friend_notices.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace headsup
{
    struct Sound
    {
        std::vector<int16_t> stereo; // left and right in turn, as WaveOut takes them
        int rate = 0;
    };
    // An 8-bit mono PCM WAV as 16-bit stereo; nothing for any other kind of file or one cut short.
    std::optional<Sound> SoundFromWav(std::span<const uint8_t> wav);
    std::vector<int16_t> AtVolume(const std::vector<int16_t>& samples, int percent);
    // The WAV a notice plays; empty for a kind with no sound.
    std::span<const uint8_t> NoticeWav(NoticeKind kind);
}

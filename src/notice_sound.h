#pragma once

#include "settings.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
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
    constexpr uint32_t kFirstCallSound = 17; // <call0> is sound effect 17, <call21> is 38
    // The game's file of a sound choice, under the game's folder ("sound\\win\\se\\se000\\se000031.spw" for
    // <call14>); nothing for None and the chime.
    std::optional<std::string> NoticeSoundFile(int choice);
    // One of the game's sound effects (.spw, "SeWave") as 16-bit stereo; nothing for a file that is not 16-bit PCM
    // of one or two channels, or is cut short.
    std::optional<Sound> SoundFromSpw(std::span<const uint8_t> spw);
    // FFXIFriendList's chime, built in.
    std::span<const uint8_t> ChimeWav();
}

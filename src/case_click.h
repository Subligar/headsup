#pragma once

#include <cstdint>
#include <vector>

namespace headsup
{
    constexpr int kClickRate = 44100;
    // A case's sound: a click at each moment, its loudest there, as 16-bit samples, left and right in turn, `seconds`
    // long.
    std::vector<int16_t> ClickTrack(const std::vector<float>& clicks, float seconds);
}

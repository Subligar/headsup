#include "notice_sound.h"

#include "bytes.h"

#include <cstring>

namespace headsup
{
    namespace
    {
        const unsigned char kOnlineWav[] = {
#embed "../resources/sounds/online.wav"
        };

        constexpr size_t kChunkHeader = 8, kWaveStart = 12, kFormatBytes = 16;
        constexpr uint16_t kPcm = 1, kMono = 1, kEightBit = 8;
        constexpr int kEightBitMiddle = 128, kEightBitToSixteen = 256, kWholeVolume = 100;

        bool IsId(std::span<const uint8_t> bytes, size_t at, const char* id) { return std::memcmp(&bytes[at], id, 4) == 0; }
    }

    std::optional<Sound> SoundFromWav(std::span<const uint8_t> wav)
    {
        if (wav.size() < kWaveStart || !IsId(wav, 0, "RIFF") || !IsId(wav, kChunkHeader, "WAVE")) return std::nullopt;
        std::optional<int> rate;
        for (size_t at = kWaveStart; at + kChunkHeader <= wav.size();)
        {
            const uint32_t size = ReadAt<uint32_t>(wav.data(), at + 4);
            const size_t body   = at + kChunkHeader;
            if (size > wav.size() - body) return std::nullopt;
            if (IsId(wav, at, "fmt "))
            {
                if (size < kFormatBytes || ReadAt<uint16_t>(wav.data(), body) != kPcm ||
                    ReadAt<uint16_t>(wav.data(), body + 2) != kMono || ReadAt<uint16_t>(wav.data(), body + 14) != kEightBit)
                    return std::nullopt;
                rate = static_cast<int>(ReadAt<uint32_t>(wav.data(), body + 4));
            }
            else if (IsId(wav, at, "data"))
            {
                if (!rate) return std::nullopt;
                Sound sound{{}, *rate};
                for (const uint8_t sample : wav.subspan(body, size))
                {
                    const auto wide = static_cast<int16_t>((sample - kEightBitMiddle) * kEightBitToSixteen);
                    sound.stereo.insert(sound.stereo.end(), {wide, wide});
                }
                return sound;
            }
            at = body + size + size % 2;
        }
        return std::nullopt;
    }

    std::vector<int16_t> AtVolume(const std::vector<int16_t>& samples, int percent)
    {
        std::vector<int16_t> scaled;
        for (const int16_t sample : samples) scaled.push_back(static_cast<int16_t>(sample * percent / kWholeVolume));
        return scaled;
    }

    std::span<const uint8_t> NoticeWav(NoticeKind kind)
    {
        if (kind == NoticeKind::Online) return kOnlineWav;
        return {};
    }
}

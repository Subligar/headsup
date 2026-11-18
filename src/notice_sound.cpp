#include "notice_sound.h"

#include "bytes.h"

#include <cstring>
#include <string>

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

        // A game sound effect's header, little endian. Its size field (0x08) is not trusted.
        constexpr char kSeWave[] = "SeWave\0";      // eight bytes with the zero after it
        constexpr size_t kSeWaveBytes = sizeof(kSeWave);
        constexpr size_t kSpwHeader = 0x30, kSpwCodecAt = 0x0C, kSpwCountAt = 0x14, kSpwRateAt = 0x1C,
                         kSpwRateAddAt = 0x20, kSpwStartAt = 0x24, kSpwChannelsAt = 0x2A;
        constexpr uint32_t kSpwPcm16 = 1;
        constexpr uint32_t kSpwRateMask = 0x7FFFFFFF; // the rate is two words' sum, its top bit dropped
        constexpr uint8_t kStereo = 2;
        constexpr const char* kCallFolder = "sound\\win\\se\\se000\\se"; // every call is in se000
        constexpr int kSoundIdDigits = 6;
    }

    std::optional<Sound> SoundFromSpw(std::span<const uint8_t> spw)
    {
        if (spw.size() < kSpwHeader || std::memcmp(spw.data(), kSeWave, kSeWaveBytes) != 0 ||
            ReadAt<uint32_t>(spw.data(), kSpwCodecAt) != kSpwPcm16)
            return std::nullopt;
        const uint8_t channels = spw[kSpwChannelsAt];
        if (channels != kMono && channels != kStereo) return std::nullopt;
        const uint64_t samples = uint64_t{ReadAt<uint32_t>(spw.data(), kSpwCountAt)} * channels;
        const uint32_t start   = ReadAt<uint32_t>(spw.data(), kSpwStartAt);
        if (start > spw.size() || samples * sizeof(int16_t) > spw.size() - start) return std::nullopt;
        const uint32_t rate = (ReadAt<uint32_t>(spw.data(), kSpwRateAt) + ReadAt<uint32_t>(spw.data(), kSpwRateAddAt)) &
                              kSpwRateMask;
        Sound sound{{}, static_cast<int>(rate)};
        for (size_t i = 0; i < static_cast<size_t>(samples); ++i)
        {
            const auto sample = ReadAt<int16_t>(spw.data(), start + i * sizeof(int16_t));
            sound.stereo.insert(sound.stereo.end(), channels == kMono ? kStereo : kMono, sample);
        }
        return sound;
    }

    std::optional<std::string> NoticeSoundFile(int choice)
    {
        if (choice < kFirstCallChoice || choice >= kNoticeSoundCount) return std::nullopt;
        std::string id = std::to_string(kFirstCallSound + static_cast<uint32_t>(choice - kFirstCallChoice));
        return kCallFolder + std::string(kSoundIdDigits - id.size(), '0') + id + ".spw";
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

    std::span<const uint8_t> ChimeWav() { return kOnlineWav; }
}

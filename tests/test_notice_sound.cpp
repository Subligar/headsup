#include "notice_sound.h"
#include "test.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    void Add32(std::vector<uint8_t>& bytes, uint32_t value)
    {
        for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }

    void Add16(std::vector<uint8_t>& bytes, uint16_t value)
    {
        bytes.push_back(static_cast<uint8_t>(value));
        bytes.push_back(static_cast<uint8_t>(value >> 8));
    }

    std::vector<uint8_t> Chunk(const char* id, const std::vector<uint8_t>& body)
    {
        std::vector<uint8_t> bytes(id, id + 4);
        Add32(bytes, static_cast<uint32_t>(body.size()));
        bytes.insert(bytes.end(), body.begin(), body.end());
        if (body.size() % 2 != 0) bytes.push_back(0); // a chunk of odd size is padded to even
        return bytes;
    }

    // A RIFF/WAVE file written by hand: fmt (16 bytes), the extra chunk when given, then data.
    std::vector<uint8_t> Wav(uint16_t format, uint16_t channels, uint32_t rate, uint16_t bits,
        const std::vector<uint8_t>& data, const std::vector<uint8_t>& extra = {})
    {
        std::vector<uint8_t> fmt;
        Add16(fmt, format);
        Add16(fmt, channels);
        Add32(fmt, rate);
        Add32(fmt, rate * channels * bits / 8);
        Add16(fmt, static_cast<uint16_t>(channels * bits / 8));
        Add16(fmt, bits);
        std::vector<uint8_t> body = {'W', 'A', 'V', 'E'};
        for (const std::vector<uint8_t>& part : {Chunk("fmt ", fmt), extra, Chunk("data", data)})
            body.insert(body.end(), part.begin(), part.end());
        return Chunk("RIFF", body);
    }
}

TEST(an_eight_bit_mono_wav_becomes_sixteen_bit_stereo)
{
    // The addon's files hold a LIST chunk between fmt and data; an odd-sized chunk is followed by a pad byte.
    const std::vector<uint8_t> wav = Wav(1, 1, 22050, 8, {0x80, 0xFF, 0x00}, Chunk("LIST", {1, 2, 3}));
    const std::optional<Sound> sound = SoundFromWav(wav);
    CHECK(sound && sound->rate == 22050);
    // 8-bit is unsigned around 128: (0x80-128)*256 = 0, (0xFF-128)*256 = 32512, (0-128)*256 = -32768.
    CHECK((sound->stereo == std::vector<int16_t>{0, 0, 32512, 32512, -32768, -32768}));
}

TEST(a_file_that_is_no_eight_bit_mono_pcm_or_is_cut_short_gives_no_sound)
{
    CHECK(!SoundFromWav(Wav(1, 1, 22050, 16, {0, 0}))); // 16-bit
    CHECK(!SoundFromWav(Wav(1, 2, 22050, 8, {0, 0})));  // stereo
    CHECK(!SoundFromWav(Wav(3, 1, 22050, 8, {0, 0})));  // not PCM
    std::vector<uint8_t> cut = Wav(1, 1, 22050, 8, {1, 2, 3, 4});
    cut.resize(cut.size() - 2); // data says 4 bytes and 2 are there
    CHECK(!SoundFromWav(cut));
    std::vector<uint8_t> none = Wav(1, 1, 22050, 8, {});
    none.resize(36); // RIFF, WAVE and fmt alone
    CHECK(!SoundFromWav(none));
    CHECK(!SoundFromWav({}) && !SoundFromWav(std::vector<uint8_t>(64, 0)));
}

TEST(the_volume_scales_every_sample)
{
    CHECK((AtVolume({32512, -32768}, 50) == std::vector<int16_t>{16256, -16384}));
    CHECK((AtVolume({32512, -32768}, 100) == std::vector<int16_t>{32512, -32768}));
    CHECK((AtVolume({32512, -32768}, 0) == std::vector<int16_t>{0, 0}));
}

TEST(the_online_sound_is_in_the_build)
{
    // online.wav's data chunk is 25402 bytes at 22050 Hz (read from the file), two samples a byte.
    const std::optional<Sound> online = SoundFromWav(NoticeWav(NoticeKind::Online));
    CHECK(online && online->rate == 22050 && online->stereo.size() == size_t{50804});
    CHECK(NoticeWav(NoticeKind::Offline).empty());
}

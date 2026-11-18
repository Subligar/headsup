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
    const std::optional<Sound> online = SoundFromWav(ChimeWav());
    CHECK(online && online->rate == 22050 && online->stereo.size() == size_t{50804});
}

namespace
{
    // se000038.spw's first 0x30 bytes, as the game ships it.
    const std::vector<uint8_t> kCall21Header = {
        0x53, 0x65, 0x57, 0x61, 0x76, 0x65, 0x00, 0x00, 0x30, 0x9a, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x26, 0x00, 0x00, 0x00, 0x00, 0xcd, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x91, 0xbf, 0x73, 0x8a,
        0xef, 0xfb, 0x8c, 0x75, 0x30, 0x00, 0x00, 0x00, 0x00, 0x04, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00};
    // Its samples 1536 to 1551 (file offset 0xC30): -1 x7, -2, -2, -1, 0, 0, -1, -2, -2, -2.
    const std::vector<uint8_t> kCall21Samples = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
        0xfe, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff};
    constexpr size_t kCountAt = 0x14, kCodecAt = 0x0C, kChannelsAt = 0x2A, kStartAt = 0x24;

    void Put(std::vector<uint8_t>& file, size_t at, uint32_t value)
    {
        for (size_t i = 0; i < 4; ++i) file[at + i] = static_cast<uint8_t>(value >> (8 * i));
    }

    // The header with a count of its own, then the 32 bytes.
    std::vector<uint8_t> Call21(uint32_t count)
    {
        std::vector<uint8_t> file = kCall21Header;
        Put(file, kCountAt, count);
        file.insert(file.end(), kCall21Samples.begin(), kCall21Samples.end());
        return file;
    }
}

TEST(a_call_sound_becomes_sixteen_bit_stereo_at_its_rate)
{
    const std::optional<Sound> sound = SoundFromSpw(Call21(16));
    CHECK(sound && sound->rate == 48000);
    const std::vector<int16_t> mono = {-1, -1, -1, -1, -1, -1, -1, -2, -2, -1, 0, 0, -1, -2, -2, -2};
    std::vector<int16_t> doubled;
    for (const int16_t sample : mono) doubled.insert(doubled.end(), {sample, sample});
    CHECK(sound && sound->stereo == doubled);
}

TEST(a_sound_file_cut_short_or_of_another_codec_gives_no_sound)
{
    CHECK(!SoundFromSpw(Call21(52480))); // the count as shipped, with only 32 bytes after the header
    std::vector<uint8_t> file = Call21(16);
    file.pop_back();
    CHECK(!SoundFromSpw(file));
    for (const uint32_t codec : {0u, 3u})
    {
        file = Call21(16);
        Put(file, kCodecAt, codec);
        CHECK(!SoundFromSpw(file));
    }
    for (const uint8_t channels : {uint8_t{0}, uint8_t{3}})
    {
        file              = Call21(16);
        file[kChannelsAt] = channels;
        CHECK(!SoundFromSpw(file));
    }
    file = Call21(16);
    Put(file, kStartAt, 0x1000);
    CHECK(!SoundFromSpw(file));
    file    = Call21(16);
    file[5] = 'f';
    CHECK(!SoundFromSpw(file));
    CHECK(!SoundFromSpw(std::vector<uint8_t>(file.begin(), file.begin() + 10)));
    CHECK(!SoundFromSpw({}));
}

TEST(a_two_channel_sound_keeps_its_left_and_right)
{
    std::vector<uint8_t> file = Call21(8);
    file[kChannelsAt]         = 2;
    const std::optional<Sound> sound = SoundFromSpw(file);
    CHECK(sound && (sound->stereo == std::vector<int16_t>{-1, -1, -1, -1, -1, -1, -1, -2, -2, -1, 0, 0, -1, -2, -2, -2}));
}

TEST(each_call_has_its_file_in_the_games_folder)
{
    CHECK(NoticeSoundFile(kFirstCallChoice) == std::optional<std::string>("sound\\win\\se\\se000\\se000017.spw"));
    CHECK(NoticeSoundFile(kFirstCallChoice + 21) == std::optional<std::string>("sound\\win\\se\\se000\\se000038.spw"));
    CHECK(!NoticeSoundFile(kNoSound) && !NoticeSoundFile(kChimeSound) && !NoticeSoundFile(kNoticeSoundCount));
}

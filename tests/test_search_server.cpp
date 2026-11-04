#include "search_cipher.h"
#include "search_server.h"
#include "test.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    std::vector<uint8_t> Bytes(const std::string& hex)
    {
        std::vector<uint8_t> out;
        for (size_t i = 0; i + 1 < hex.size(); i += 2)
            out.push_back(static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16)));
        return out;
    }
    std::string Hex(const std::array<uint8_t, 16>& bytes)
    {
        std::string out;
        for (const uint8_t b : bytes)
        {
            constexpr const char* kDigits = "0123456789abcdef";
            out += kDigits[b >> 4];
            out += kDigits[b & 0xF];
        }
        return out;
    }

    // Recorded on the test server (2026-10-23) by ~/.cache/headsup-feedback-7/player-lookup/fixtures/record.cpp, which
    // builds a request through the server's own blowfish and md5 with the nonce 0x12345678: Headsup (65001, RDM 20 /
    // WHM 10 in West Ronfaure), then Headsup anonymous with two ids of no one online.
    const std::string kHeadsupRequest = "2c00000049584646d56a8e3f09a4af618f1aa3e83c0201014540ed79e104de7fec1547d2c5ca210378563412";
    const std::string kHeadsupKey     = "30733d6d3c31495a327a424363387b7e7856341200000000";
    const std::string kHeadsupAnswer  = "50000000495846464a4858e4042a6d70d3f69548939e732eb67987e2cb99df179d21a5cb2385affb9f610ae8caa7616708887ee7e16a928354d8779eb00258d866cfcadffd25efba3273b9e978563412";
    const std::string kAnonRequest    = "3400000049584646d56a8e3f09a4af6151cd5a899fb6276e045c3dae0fa377c85fcb1ca5d20b976e644852ba593556b478563412";
    const std::string kAnonKey        = "30733d6d3c31495a327a424363387b7e785634120f000000";
    const std::string kAnonAnswer     = "48000000495846464ecc1dc544824bb9b7f9370b1d79ada20b41177e8d8ea80ce9e48a5619be422ddbeae0fe13ab5d206d2dacd504f0a7145dd28210907d86c19df7554b78563412";
    // Recorded the same way (2026-10-24): Headsupfour (65004, THF 20 / WAR 10 in Valkurm Dunes), whose entry ends in
    // 5 bits of padding, room for a field's code but not for the field.
    const std::string kHeadsupfourAnswer = "54000000495846461881566de37def87d3f69548939e732e7321e252fd4bbe971d5291feedd1137aac5fbdb211cac1e44ed814931164c5ed42c0cfd263bfe2eb8108ef5b3a76567bb5d5c3bbf88162d278563412";
    constexpr uint32_t kNonce = 0x12345678;
    constexpr uint32_t kHeadsup = 65001;

    std::array<uint8_t, kAnswerKeyBytes> Key(const std::string& hex)
    {
        std::array<uint8_t, kAnswerKeyBytes> key{};
        const std::vector<uint8_t> bytes = Bytes(hex);
        std::copy(bytes.begin(), bytes.end(), key.begin());
        return key;
    }

    // search_list.cpp's field codes and widths in bits.
    constexpr uint32_t kNameCode = 0x00, kJobCode = 0x03, kLevelCode = 0x04, kFlags1Code = 0x06, kIdCode = 0x08;
    constexpr uint32_t kUnknownCode = 0x1F, kAnonymousFlag = 0x4000; // and flags1's anonymous bit
    constexpr int kCodeWidth = 5, kNameLengthWidth = 4, kNameCharWidth = 7, kJobWidth = 5, kLevelWidth = 8,
                  kFlags1Width = 16, kIdWidth = 20, kByteWidth = 8;

    // An entry's fields as LandSandBoat's search_list.cpp packs them: each value's bits, most significant first.
    struct EntryBits
    {
        std::vector<uint8_t> bytes;
        int bit = 0;
        EntryBits& Put(uint32_t value, int width)
        {
            for (int i = width - 1; i >= 0; --i, ++bit)
            {
                if (bit % 8 == 0) bytes.push_back(0);
                if (value >> i & 1u) bytes.back() |= static_cast<uint8_t>(0x80u >> (bit % 8));
            }
            return *this;
        }
        EntryBits& Name(const std::string& name)
        {
            Put(kNameCode, kCodeWidth).Put(static_cast<uint32_t>(name.size()), kNameLengthWidth);
            for (const char c : name)
                Put(static_cast<uint8_t>(c), kNameCharWidth);
            return *this;
        }
    };

    // The last answer packet of this one entry, sealed as the server seals it: its entries' end at 0x08, 0x80 at
    // 0x0A, the entry from 0x18, the md5 of 8 up to the hash, 4 key bytes, then enciphered from 8 in pairs of words
    // under the md5 of the answer's key, the last 12 bytes left as they are.
    std::vector<uint8_t> AnswerOf(const EntryBits& entry, const std::array<uint8_t, kAnswerKeyBytes>& key)
    {
        constexpr size_t kEntriesAt = 0x18, kTrailer = 20, kPlainTail = 12, kFrom = 8;
        std::vector<uint8_t> p(kEntriesAt);
        p.push_back(static_cast<uint8_t>(entry.bytes.size()));
        p.insert(p.end(), entry.bytes.begin(), entry.bytes.end());
        const size_t end = p.size();
        p.resize((end + kTrailer + 7) / 8 * 8);
        p[0] = static_cast<uint8_t>(p.size());
        p[1] = static_cast<uint8_t>(p.size() >> 8);
        std::copy_n("IXFF", 4, p.begin() + 4);
        p[8]    = static_cast<uint8_t>(end);
        p[9]    = static_cast<uint8_t>(end >> 8);
        p[0x0A] = 0x80;
        const size_t hashAt = p.size() - kTrailer;
        const std::array<uint8_t, kMd5Bytes> hash = Md5(std::span<const uint8_t>(p).subspan(kFrom, hashAt - kFrom));
        std::copy(hash.begin(), hash.end(), p.begin() + static_cast<std::ptrdiff_t>(hashAt));
        const FfxiBlowfish fish(Md5(key));
        size_t words = (p.size() - kPlainTail) / 4;
        words -= words % 2;
        for (size_t w = 0; w < words; w += 2)
        {
            uint32_t left = 0, right = 0;
            std::memcpy(&left, p.data() + kFrom + w * 4, 4);
            std::memcpy(&right, p.data() + kFrom + w * 4 + 4, 4);
            fish.Encipher(left, right);
            std::memcpy(p.data() + kFrom + w * 4, &left, 4);
            std::memcpy(p.data() + kFrom + w * 4 + 4, &right, 4);
        }
        return p;
    }
}

TEST(md5_is_rfc_1321s)
{
    CHECK_EQ(Hex(Md5({})), std::string("d41d8cd98f00b204e9800998ecf8427e"));
    const std::string abc = "abc";
    CHECK_EQ(Hex(Md5({reinterpret_cast<const uint8_t*>(abc.data()), abc.size()})), std::string("900150983cd24fb0d6963f7d28e17f72"));
    // Past one 64-byte block: RFC 1321's 80 digits.
    std::string digits;
    for (int i = 0; i < 8; ++i)
        digits += "1234567890";
    CHECK_EQ(Hex(Md5({reinterpret_cast<const uint8_t*>(digits.data()), digits.size()})), std::string("57edf4a22be3c955ac49da2e2107b67a"));
}

TEST(an_id_list_request_is_the_bytes_the_server_builds)
{
    const std::array<uint32_t, 1> one{kHeadsup};
    const SearchRequest request = IdListRequest(one, kNonce);
    CHECK(request.bytes == Bytes(kHeadsupRequest));
    CHECK(request.answerKey == Key(kHeadsupKey));
    const std::array<uint32_t, 3> three{kHeadsup, kHeadsup + 1, 999999};
    const SearchRequest more = IdListRequest(three, kNonce);
    CHECK(more.bytes == Bytes(kAnonRequest) && more.answerKey == Key(kAnonKey));
}

TEST(an_answer_gives_each_players_jobs_and_levels)
{
    const std::vector<uint8_t> answer = Bytes(kHeadsupAnswer);
    CHECK(SearchPacketSize(answer) == answer.size());
    const std::optional<SearchPacket> packet = ReadSearchPacket(answer, Key(kHeadsupKey));
    CHECK(packet && packet->last && packet->players.size() == 1u);
    const SearchedPlayer& p = packet->players[0];
    constexpr uint8_t kRdm = 5, kWhm = 3;
    CHECK_EQ(p.id, kHeadsup & kSearchIdMask);
    CHECK_EQ(p.name, std::string("Headsup"));
    CHECK(p.mainJob == kRdm && p.mainLevel == 20 && p.subJob == kWhm && p.subLevel == 10);
}

TEST(an_entry_whose_padding_could_start_a_field_still_gives_its_player)
{
    const std::optional<SearchPacket> packet = ReadSearchPacket(Bytes(kHeadsupfourAnswer), Key(kHeadsupKey));
    CHECK(packet && packet->last && packet->players.size() == 1u);
    const SearchedPlayer& p = packet->players[0];
    constexpr uint8_t kThf = 6, kWar = 1;
    CHECK(p.id == 65004u && p.name == std::string("Headsupfour"));
    CHECK(p.mainJob == kThf && p.mainLevel == 20 && p.subJob == kWar && p.subLevel == 10);
}

TEST(an_anonymous_player_answers_without_jobs_and_no_one_offline_answers)
{
    const std::optional<SearchPacket> packet = ReadSearchPacket(Bytes(kAnonAnswer), Key(kAnonKey));
    CHECK(packet && packet->last && packet->players.size() == 1u);
    const SearchedPlayer& p = packet->players[0];
    CHECK(p.name == std::string("Headsup") && p.mainJob == 0 && p.mainLevel == 0);
}

TEST(a_field_of_no_known_code_ends_an_entry_that_still_hides_an_anonymous_players_jobs_and_needs_an_id)
{
    const std::array<uint8_t, kAnswerKeyBytes> key = Key(kHeadsupKey);
    constexpr uint8_t kRdm = 5, kWhm = 3;
    EntryBits anonymous;
    anonymous.Name("Headsup").Put(kJobCode, kCodeWidth).Put(kRdm, kJobWidth).Put(kWhm, kJobWidth);
    anonymous.Put(kLevelCode, kCodeWidth).Put(20, kLevelWidth).Put(10, kLevelWidth);
    anonymous.Put(kFlags1Code, kCodeWidth).Put(kAnonymousFlag, kFlags1Width).Put(kIdCode, kCodeWidth).Put(kHeadsup, kIdWidth);
    anonymous.Put(kUnknownCode, kCodeWidth).Put(UINT8_MAX, kByteWidth);
    const std::optional<SearchPacket> packet = ReadSearchPacket(AnswerOf(anonymous, key), key);
    CHECK(packet && packet->players.size() == 1u);
    const SearchedPlayer& p = packet->players[0];
    CHECK(p.id == kHeadsup && p.name == std::string("Headsup"));
    CHECK(p.mainJob == 0 && p.subJob == 0 && p.mainLevel == 0 && p.subLevel == 0);
    EntryBits idless;
    idless.Name("Headsup").Put(kFlags1Code, kCodeWidth).Put(0, kFlags1Width).Put(kUnknownCode, kCodeWidth).Put(UINT8_MAX, kByteWidth);
    const std::optional<SearchPacket> noId = ReadSearchPacket(AnswerOf(idless, key), key);
    CHECK(noId && noId->players.empty());
}

TEST(a_packet_cut_short_or_under_another_key_is_no_answer)
{
    const std::vector<uint8_t> answer = Bytes(kHeadsupAnswer);
    CHECK(!ReadSearchPacket(std::span(answer).first(answer.size() - 8), Key(kHeadsupKey)));
    CHECK(!ReadSearchPacket(answer, Key(kAnonKey)));
    CHECK(!ReadSearchPacket({}, Key(kHeadsupKey)));
    const std::vector<uint8_t> tiny = {4, 0, 0, 0};
    CHECK(!SearchPacketSize(tiny).has_value());
}

#include "test.h"

#include "currencies.h"
#include "game_state.h"

#include <cstring>
#include <vector>

using namespace headsup;

namespace
{
    void Put32(std::vector<uint8_t>& p, uint32_t at, int32_t value) { std::memcpy(p.data() + at, &value, 4); }
    void Put16(std::vector<uint8_t>& p, uint32_t at, uint16_t value) { std::memcpy(p.data() + at, &value, 2); }
}

TEST(currencies_are_read_from_phoenixs_two_currency_packets_where_its_structs_put_them)
{
    // GP_SERV_COMMAND_CURRENCIES_1, packed: 0xFC bytes with the header, its unknownCD at 0xCD.
    std::vector<uint8_t> first(0xFC, 0);
    Put32(first, 0x04, 1500);  // conquest_points_sandoria
    Put16(first, 0x10, 42);    // beastmans_seals_stored
    Put32(first, 0x7C, 9001);  // imperial_standing
    Put32(first, 0xA4, -5);    // allied_notes, as no one has
    Put16(first, 0xF8, 7);     // deeds, the last
    CurrencyValues values{};
    ReadCurrencies(values, kCurrencies1Packet, first.data(), static_cast<uint32_t>(first.size()));
    CHECK(values[*CurrencyOfKey("cp_sandoria")] == 1500u && values[*CurrencyOfKey("beastmen_seals")] == 42u);
    CHECK(values[*CurrencyOfKey("imperial_standing")] == 9001u && values[*CurrencyOfKey("allied_notes")] == 0u);
    CHECK(values[*CurrencyOfKey("deeds")] == 7u);
    CHECK(!values[kGilCurrency] && !values[*CurrencyOfKey("bayld")]); // the inventory's, and the other packet's

    // GP_SERV_COMMAND_CURRENCIES_2, aligned: apollyon_units last at 0x9C.
    std::vector<uint8_t> second(0xA0, 0);
    Put32(second, 0x04, 250);  // bayld
    Put32(second, 0x9C, 33);   // apollyon_units
    ReadCurrencies(values, kCurrencies2Packet, second.data(), static_cast<uint32_t>(second.size()));
    CHECK(values[*CurrencyOfKey("bayld")] == 250u && values[*CurrencyOfKey("apollyon_units")] == 33u);

    CurrencyValues untouched{};
    ReadCurrencies(untouched, kCurrencies1Packet, first.data(), 0xF9); // too short for deeds
    ReadCurrencies(untouched, kWeatherPacket, first.data(), static_cast<uint32_t>(first.size()));
    CHECK(untouched == CurrencyValues{});
}

TEST(every_currency_has_a_key_of_its_own_and_gil_comes_first)
{
    CHECK(CurrencyOfKey("gil") == kGilCurrency && !CurrencyOfKey("dollars") && !CurrencyOfKey(""));
    for (int c = 0; c < kCurrencyCount; ++c)
        CHECK(CurrencyOfKey(kCurrencyTable[c].key) == c);
}

TEST(currencies_are_asked_for_once_after_logging_in_and_not_again_on_zoning)
{
    GameState game;
    CHECK(!game.CurrencyRequestDue(0.0, false)); // the widget off
    CHECK(game.CurrencyRequestDue(0.0, true));  // after loading
    game.CurrencyRequested(0.0);
    CHECK(!game.CurrencyRequestDue(120.0, true));
    std::vector<uint8_t> zoneIn(0x100, 0);
    game.Received(kZoneInPacket, zoneIn.data(), static_cast<uint32_t>(zoneIn.size()));
    CHECK(!game.CurrencyRequestDue(300.0, true));
    game.LoggedIn();
    CHECK(game.CurrencyRequestDue(400.0, true));
}

// GP_SERV_COMMAND_CONQUEST: ConquestPoints (your nation's) at 0x90 and ImperialStanding at 0xB0, with the header.
TEST(your_conquest_points_and_imperial_standing_follow_each_conquest_packet_until_the_currencies_come_again)
{
    GameState game;
    std::vector<uint8_t> first(0xFC, 0);
    Put32(first, 0x04, 1500);  // conquest_points_sandoria
    Put32(first, 0x08, 200);   // conquest_points_bastok
    Put32(first, 0x7C, 9001);  // imperial_standing
    game.Received(kCurrencies1Packet, first.data(), static_cast<uint32_t>(first.size()));
    const int sandoria = *CurrencyOfKey("cp_sandoria"), bastok = *CurrencyOfKey("cp_bastok");
    const int standing = *CurrencyOfKey("imperial_standing");
    CHECK(game.Currencies(Nation::SanDoria)[sandoria] == 1500u);
    std::vector<uint8_t> conquest(0xB4, 0);
    Put32(conquest, 0x90, 1730);
    Put32(conquest, 0xB0, 9100);
    game.Received(kConquestPacket, conquest.data(), static_cast<uint32_t>(conquest.size()));
    CHECK(game.Currencies(Nation::SanDoria)[sandoria] == 1730u && game.Currencies(Nation::SanDoria)[bastok] == 200u);
    CHECK(game.Currencies(Nation::SanDoria)[standing] == 9100u);
    CHECK(game.Currencies(Nation::Bastok)[bastok] == 1730u && game.Currencies(Nation::Bastok)[sandoria] == 1500u);
    // With no nation known yet, the points are no nation's.
    CHECK(game.Currencies(Nation::None)[sandoria] == 1500u);
    // The currencies' own packet is newer than the conquest packet before it.
    Put32(first, 0x04, 1800);
    game.Received(kCurrencies1Packet, first.data(), static_cast<uint32_t>(first.size()));
    CHECK(game.Currencies(Nation::SanDoria)[sandoria] == 1800u);
    // A conquest packet too short for the points changes nothing.
    game.Received(kConquestPacket, conquest.data(), 0x93);
    CHECK(game.Currencies(Nation::SanDoria)[sandoria] == 1800u);
}

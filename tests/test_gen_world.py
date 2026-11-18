import unittest

from tooling import load

gen = load('tools/gen_world.py')

TRAVEL = ('group\tname\tevery\tdepartures\nAirships\tJeuno to San d\'Oria\t864\t28:182\n'
          'Barge\tCentral to South Landing\t3456\t678:752,2790:2864\n')
GUILDS = ('guild\tshops\tzone\topen\tclose\tholiday\nAlchemy\tWahraga, Gathweeda\tAht Urhgan Whitegate\t8\t23\t5\n'
          "Woodworking\tChaupire\tNorthern San d'Oria\t6\t21\t0\n")
HIGH = 'item\n16928\n16713\n'
# Regions as data/phoenix_zones.tsv has them: Port Jeuno is Jeuno's (22), Carpenters' Landing Norvallen's (2), Bibiki Bay
# Kolshushu's (6), Whitegate Aht Urhgan's (28), Northern San d'Oria San d'Oria's (19).
ZONES = ('id\tname\tregion\tcalendar\tfishing\n2\tCarpenters\' Landing\t2\t1\t\n4\tBibiki Bay\t6\t2\t\n'
         '50\tAht Urhgan Whitegate\t28\t14\t\n231\tNorthern San d\'Oria\t19\t8\t\n246\tPort Jeuno\t22\t59\t\n'
         '220\tShip bound for Selbina\t255\t\t\n')


def generated(travel=TRAVEL, guilds=GUILDS, zones=ZONES):
    return gen.generate(gen.parse_travel(travel), gen.parse_guilds(guilds), gen.parse_high_quality(HIGH),
                        gen.zone_regions(zones))


class Generate(unittest.TestCase):
    def test_routes_and_shops(self):
        text = generated()
        # Each route also says how it is taken and where to, and the region it leaves from.
        self.assertIn('    {"Airships", "Jeuno to San d\'Oria", 864, 1, {{28, 182}}, "Airship to San d\'Oria", 22},\n', text)
        self.assertIn('    {"Barge", "Central to South Landing", 3456, 2, {{678, 752}, {2790, 2864}}, "Barge to South Landing", 2},\n',
                      text)
        self.assertIn('    {"Alchemy", "Wahraga, Gathweeda", "Aht Urhgan Whitegate", 8, 23, 5, 28},\n', text)
        # The high quality turn-ins sorted, for a binary search.
        self.assertIn('const uint16_t kHighQualityTurnIns[] = {16713, 16928};\n', text)

    def test_two_routes_of_a_region_to_one_place_keep_their_own_names(self):
        text = generated(TRAVEL + 'Barge\tNorth to South Landing\t3456\t2415:2514\n')
        self.assertIn('"Barge: Central to South Landing", 2},', text)
        self.assertIn('"Barge: North to South Landing", 2},', text)
        self.assertIn('"Airship to San d\'Oria", 22},', text)

    def test_a_tour_goes_nowhere_but_back(self):
        text = generated(TRAVEL + 'Manaclipper\tBibiki Bay tour\t1728\t43:120\n')
        self.assertIn('"Manaclipper tour", 6},', text)

    def test_a_ships_shop_is_in_no_region(self):
        text = generated(guilds=GUILDS + 'Fishing\tMain\tShip bound for Selbina\t1\t23\t7\n')
        self.assertIn('"Ship bound for Selbina", 1, 23, 7, 255},', text)

    def test_a_route_or_shop_the_zones_cannot_place_is_an_error(self):
        for travel, guilds in [(TRAVEL + 'Airships\tAtlantis to Jeuno\t864\t28:182\n', GUILDS),
                               (TRAVEL + 'Rafts\tJeuno to Bastok\t864\t28:182\n', GUILDS),
                               (TRAVEL, GUILDS + 'Cooking\tKopopo\tWindurst Waters\t5\t20\t7\n')]:
            with self.assertRaises(gen.DataError):
                generated(travel, guilds)

    def test_a_high_quality_list_that_is_not_item_ids_is_an_error(self):
        with self.assertRaises(gen.DataError):
            gen.parse_high_quality('item\nHellfire Axe\n')
        with self.assertRaises(gen.DataError):
            gen.parse_high_quality('item\n70000\n')

    def test_a_file_without_its_header_is_an_error(self):
        with self.assertRaises(gen.DataError):
            gen.parse_travel(TRAVEL.replace('departures', 'times'))
        with self.assertRaises(gen.DataError):
            gen.parse_guilds('')

    def test_a_departure_outside_its_cycle_is_an_error(self):
        with self.assertRaises(gen.DataError) as raised:
            gen.parse_travel(TRAVEL.replace('28:182', '28:864'))
        self.assertIn('line 2', str(raised.exception))

    def test_more_departures_than_a_route_holds_is_an_error(self):
        with self.assertRaises(gen.DataError):
            gen.parse_travel(TRAVEL.replace('28:182', '28:182,300:400,500:600'))

    def test_hours_and_holidays_out_of_range_are_errors(self):
        for row in ['Cooking\tKopopo\tWindurst Waters\t20\t5\t7\n', 'Cooking\tKopopo\tWindurst Waters\t5\t25\t7\n',
                    'Cooking\tKopopo\tWindurst Waters\t5\t20\t8\n', 'Cooking\tKopopo\tWindurst Waters\t5\t20\n']:
            with self.assertRaises(gen.DataError):
                gen.parse_guilds(GUILDS + row)

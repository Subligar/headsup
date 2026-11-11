import pathlib
import struct
import tempfile
import unittest

from tooling import load

fishing = load('tools/phoenix/fishing.py')

REAL = pathlib.Path.home() / '.cache' / 'headsup-phoenix' / 'server'


def fish_row(item, name, skill=11, size=0, ranking=7, log=255, quest=255, flags=0, legendary=0, is_item=0, rarity=1000,
             key_item=0, quest_only=0, disabled=0):
    return (f"INSERT INTO `fishing_fish` VALUES ({item},'{name}',{skill},10,7,13,1,2,{ranking},{size},0,{log},{quest},0,"
            f"{flags},0,0,0,{legendary},0,{is_item},1,{rarity},{key_item},'',0,{quest_only},0,{disabled});")


def bounds(points):
    return '0x' + b''.join(struct.pack('<fff', x, 0.0, z) for x, z in points).hex().upper()


TABLES = {
    'fishing_fish': [fish_row(4401, 'Moat Carp'), fish_row(90, "Rusty Bucket", skill=1, flags=9, is_item=1, rarity=900),
                     fish_row(4305, 'Ryugu Titan', skill=150, size=1, legendary=1, rarity=700),
                     fish_row(591, 'Ripped Cap', log=0, quest=10, is_item=1),
                     fish_row(5000, 'Gone Fish', disabled=1), fish_row(5001, 'Contest Fish', ranking=99)],
    'fishing_area': ["INSERT INTO `fishing_area` VALUES (100,1,'Knightwell',0,0,0,'',0.000,0.000,0.000);",
                     "INSERT INTO `fishing_area` VALUES (4,2,'Ring',1,20,300,'',-360.000,0.000,-390.000);",
                     "INSERT INTO `fishing_area` VALUES (4,1,'Beach',2,20,0," +
                     bounds([(-440, -860), (-260, -860), (-260, -980), (-450, -980), (0, 0)]) + ",0.000,0.000,0.000);"],
    'fishing_catch': ["INSERT INTO `fishing_catch` VALUES (100,1,17);", "INSERT INTO `fishing_catch` VALUES (4,1,3);"],
    'fishing_group': ["INSERT INTO `fishing_group` VALUES (17,4401,1000,1000,36);",
                      "INSERT INTO `fishing_group` VALUES (17,90,900,300,9);",
                      "INSERT INTO `fishing_group` VALUES (3,4305,1000,1000,36);"],
    'fishing_bait': ["INSERT INTO `fishing_bait` VALUES (17396,'Little Worm',0,1,1,0,0,0);",
                     "INSERT INTO `fishing_bait` VALUES (17405,'Fly Lure',1,1,0,64,0,0);"],
    'fishing_bait_affinity': ["INSERT INTO `fishing_bait_affinity` VALUES (17396,4401,2); -- Little Worm, Moat Carp"],
    'fishing_rod': ["INSERT INTO `fishing_rod` VALUES (17391,'Willow Fishing Rod',0,0,0,1,30,100,50,50,30,10,2,1,1,0,3,0,0,0,0,1);",
                    "INSERT INTO `fishing_rod` VALUES (17011,'Ebisu Fishing Rod',1,1,4,1,30,100,50,50,30,10,2,1,1,0,3,0,0,0,1,15);"],
}


def checkout(root: pathlib.Path, tables=None) -> pathlib.Path:
    (root / 'sql').mkdir(parents=True, exist_ok=True)
    for table, rows in (TABLES if tables is None else tables).items():
        (root / 'sql' / f'{table}.sql').write_text('\n'.join(rows) + '\n', encoding='utf-8')
    return root


class Build(unittest.TestCase):
    def setUp(self):
        self.root = pathlib.Path(tempfile.mkdtemp())
        self.data = fishing.build(checkout(self.root))

    def test_only_fish_the_server_loads_are_kept(self):
        self.assertEqual(sorted(f['item'] for f in self.data['fish']), [90, 591, 4305, 4401])

    def test_a_fish_row_reads_the_servers_columns(self):
        fish = {f['item']: f for f in self.data['fish']}
        self.assertEqual(fish[4401], {'item': 4401, 'skill': 11, 'size': 0, 'is_item': 0, 'legendary': 0, 'shellfish': 0,
                                      'quest': 0, 'quest_only': 0, 'rarity': 1000, 'key_item': 0})
        self.assertEqual((fish[90]['is_item'], fish[90]['shellfish'], fish[90]['rarity']), (1, 1, 900))
        self.assertEqual((fish[4305]['legendary'], fish[4305]['size']), (1, 1))
        self.assertEqual(fish[591]['quest'], 1)

    def test_an_areas_radius_is_the_byte_the_server_reads(self):
        ring = next(a for a in self.data['areas'] if (a['zone'], a['area']) == (4, 2))
        self.assertEqual((ring['type'], ring['height'], ring['radius'], ring['x'], ring['z']), (1, 20, 44, -360.0, -390.0))

    def test_a_polygons_points_come_from_its_blob(self):
        beach = next(a for a in self.data['areas'] if (a['zone'], a['area']) == (4, 1))
        self.assertEqual(beach['points'], [(-440.0, -860.0), (-260.0, -860.0), (-260.0, -980.0), (-450.0, -980.0)])

    def test_areas_go_in_the_order_the_server_tries_them_with_their_catch_group(self):
        self.assertEqual([(a['zone'], a['area'], a['group']) for a in self.data['areas']], [(4, 1, 3), (4, 2, 0), (100, 1, 17)])

    def test_a_bait_carries_its_kind_and_affinities(self):
        baits = {b['bait']: b for b in self.data['baits']}
        self.assertEqual((baits[17396]['lure'], baits[17396]['shellfish'], baits[17396]['affinities']), (0, 0, [(4401, 2)]))
        self.assertEqual((baits[17405]['lure'], baits[17405]['shellfish'], baits[17405]['affinities']), (1, 1, []))

    def test_groups_and_rods_are_read(self):
        self.assertEqual(self.data['groups'], [(3, 4305), (17, 90), (17, 4401)])
        self.assertEqual(self.data['rods'], [(17011, 1, 1), (17391, 0, 0)])

    def test_a_missing_table_fails_and_writes_nothing(self):
        tables = dict(TABLES)
        del tables['fishing_fish']
        root = checkout(pathlib.Path(tempfile.mkdtemp()), tables)
        out = pathlib.Path(tempfile.mkdtemp()) / 'out'
        self.assertEqual(fishing.main(['fishing.py', str(root), str(out)]), 1)
        self.assertFalse(out.exists())

    def test_more_than_the_servers_32_points_is_refused(self):
        tables = dict(TABLES)
        tables['fishing_area'] = ["INSERT INTO `fishing_area` VALUES (4,1,'Beach',2,20,0," + bounds([(1, 1)] * 33) +
                                  ",0.000,0.000,0.000);"]
        with self.assertRaises(fishing.FishingDataError):
            fishing.build(checkout(pathlib.Path(tempfile.mkdtemp()), tables))


@unittest.skipUnless((REAL / 'sql' / 'fishing_fish.sql').exists(), 'no Phoenix checkout')
class RealCheckout(unittest.TestCase):
    def test_the_real_tables_read_as_the_server_loads_them(self):
        data = fishing.build(REAL)
        self.assertEqual(len(data['fish']), 128)
        areas = {(a['zone'], a['area']): a for a in data['areas']}
        self.assertEqual(areas[(100, 1)]['group'], 17)
        self.assertEqual([areas[(2, n)]['group'] for n in (2, 3, 4)], [29, 29, 29])
        bibiki = areas[(4, 1)]['points']
        self.assertEqual(len(bibiki), 4)
        self.assertEqual(bibiki[0], (-440.0, -860.0))
        worm = next(b for b in data['baits'] if b['bait'] == 17396)
        self.assertEqual((worm['lure'], len(worm['affinities'])), (0, 24))  # 26 rows, two of them fish the server leaves out
        self.assertIn((4401, 2), worm['affinities'])


if __name__ == '__main__':
    unittest.main()

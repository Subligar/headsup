import pathlib
import re
import tempfile
import unittest

from tooling import ROOT, load

gen = load('tools/gen_fishing.py')

FILES = {
    'phoenix_fish.tsv': 'item\tskill\tsize\tis_item\tlegendary\tshellfish\tquest\tquest_only\trarity\tkey_item\n'
                        '90\t1\t0\t1\t0\t1\t0\t0\t900\t0\n4401\t11\t0\t0\t0\t0\t0\t0\t1000\t0\n',
    'phoenix_fish_areas.tsv': 'zone\tarea\ttype\theight\tradius\tx\ty\tz\tgroup\tpoints\n'
                              '4\t1\t2\t20\t0\t0\t0\t0\t17\t-440:-860 -260:-860 -260:-980\n'
                              '100\t1\t0\t0\t0\t0\t0\t0\t17\t\n',
    'phoenix_fish_groups.tsv': 'group\titem\n17\t90\n17\t4401\n',
    'phoenix_fish_baits.tsv': 'bait\tlure\tshellfish\taffinities\n17396\t0\t0\t4401:2\n',
    'phoenix_fish_rods.tsv': 'rod\tsize\tlegendary\n17391\t0\t0\n',
}


def folder(changes=None) -> pathlib.Path:
    root = pathlib.Path(tempfile.mkdtemp())
    for name, text in {**FILES, **(changes or {})}.items():
        (root / name).write_text(text, encoding='utf-8')
    return root


def rows(text: str, array: str) -> list:
    """Each row of a generated array as its numbers."""
    body = text.split(array + '[] = {')[1].split('};')[0]
    return [[float(v) if '.' in v else int(v) for v in re.findall(r'-?[\d.]+', row)] for row in re.findall(r'\{([^{}]*)\}', body)]


class Generate(unittest.TestCase):
    def test_the_tables_become_arrays_the_pool_can_search(self):
        text = gen.generate(gen.load_folder(folder()))
        self.assertEqual(rows(text, 'kFish'), [[90, 1, 0, 1, 0, 1, 0, 0, 900, 0], [4401, 11, 0, 0, 0, 0, 0, 0, 1000, 0]])
        self.assertEqual(rows(text, 'kFishAreas'), [[4, 1, 2, 20, 0, 0.0, 0.0, 0.0, 17, 0, 3], [100, 1, 0, 0, 0, 0.0, 0.0, 0.0, 17, 3, 0]])
        self.assertEqual(rows(text, 'kAreaPoints'), [[-440.0, -860.0], [-260.0, -860.0], [-260.0, -980.0]])
        self.assertEqual(rows(text, 'kGroupFish'), [[17, 90], [17, 4401]])
        self.assertEqual(rows(text, 'kBaits'), [[17396, 0, 0, 0, 1]])
        self.assertEqual(rows(text, 'kAffinities'), [[4401, 2]])
        self.assertEqual(rows(text, 'kRods'), [[17391, 0, 0]])

    def test_a_wrong_header_is_refused(self):
        with self.assertRaises(gen.DataError):
            gen.load_folder(folder({'phoenix_fish_rods.tsv': 'rod\tsize\n17391\t0\n'}))

    def test_a_group_naming_a_fish_the_server_does_not_load_is_refused(self):
        with self.assertRaises(gen.DataError):
            gen.load_folder(folder({'phoenix_fish_groups.tsv': 'group\titem\n17\t5000\n'}))

    def test_a_number_past_its_fields_size_is_refused(self):
        with self.assertRaises(gen.DataError):
            gen.load_folder(folder({'phoenix_fish_rods.tsv': 'rod\tsize\tlegendary\n70000\t0\t0\n'}))

    def test_the_committed_data_passes(self):
        gen.load_folder(ROOT / 'data')


if __name__ == '__main__':
    unittest.main()

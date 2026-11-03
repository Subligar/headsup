import pathlib
import tempfile
import unittest

from tooling import load

statuses = load('tools/phoenix/statuses.py')

YAML = ('# yaml-language-server: $schema=schemas/status_effects.schema.json\n\nmeta:\n  enum:\n    - cpp:\n'
        '        underlying: uint16_t\n\nstatus_effects:\n  ko:\n    id: 0\n    flags:\n      - no_cancel\n\n'
        '  sleep_ii:\n    id:   19\n    name: sleep\n    element:             dark\n\n'
        '  # Zombie\n  curse_ii:\n    id: 20\n    name: curse\n\n'
        '  str_boost_ii:\n    id: 119\n    name: str_boost\n\n  protect:\n    id: 40\n')
REAL = pathlib.Path.home() / '.cache' / 'headsup-phoenix' / 'server'


def checkout(root: pathlib.Path, text: str = YAML) -> pathlib.Path:
    path = root / 'data' / 'status_effects.yaml'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8')
    return root


class DisplayNames(unittest.TestCase):
    def test_identifiers_become_the_names_players_know(self):
        for identifier, shown in [('protect', 'Protect'), ('str_boost', 'STR Boost'), ('ko', 'KO'),
                                  ('hailstorm_ii', 'Hailstorm II'), ('auto-regen', 'Auto-Regen'),
                                  ('double-up_chance', 'Double-Up Chance'), ('mark_of_the_beast', 'Mark of the Beast'),
                                  ('max_hp_boost', 'Max HP Boost')]:
            self.assertEqual(statuses.display_name(identifier), shown)


class Build(unittest.TestCase):
    def test_statuses_by_number_named_as_the_game_names_them(self):
        with tempfile.TemporaryDirectory() as tmp:
            rows = statuses.build(checkout(pathlib.Path(tmp)))
        self.assertEqual(rows, [(0, 'KO'), (19, 'Sleep'), (20, 'Curse'), (40, 'Protect'), (119, 'STR Boost')])

    def test_a_repeated_number_or_a_missing_one_is_an_error(self):
        for text in [YAML.replace('id: 40', 'id: 19'), YAML.replace('    id: 40\n', '    element: light\n'),
                     YAML.replace('id: 40', 'id: forty'), 'meta:\n  enum: []\n', 'status_effects:\n']:
            with tempfile.TemporaryDirectory() as tmp:
                with self.assertRaises(statuses.StatusDataError):
                    statuses.build(checkout(pathlib.Path(tmp), text))

    def test_writes_the_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = pathlib.Path(tmp) / 'out'
            statuses.write(out, [(0, 'KO'), (40, 'Protect')])
            self.assertEqual((out / 'phoenix_statuses.tsv').read_text(encoding='utf-8'), 'id\tname\n0\tKO\n40\tProtect\n')

    @unittest.skipUnless((REAL / 'data' / 'status_effects.yaml').is_file(), 'no Phoenix checkout')
    def test_the_real_checkout(self):
        by_id = dict(statuses.build(REAL))
        self.assertEqual((by_id[253], by_id[256], by_id[2], by_id[19]), ('Signet', 'Sanction', 'Sleep', 'Sleep'))
        self.assertEqual(by_id[233], 'Auto-Regen')

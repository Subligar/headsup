import pathlib
import shutil
import subprocess
import tempfile
import unittest

from tooling import ROOT

REAL = pathlib.Path.home() / '.cache' / 'headsup-phoenix' / 'server'
SCRIPT = ROOT / 'tools' / 'phoenix' / 'durations.lua'
LIVE_SETTINGS = ROOT / 'tools' / 'phoenix' / 'live_settings.lua'


@unittest.skipUnless(shutil.which('luajit') and (REAL / 'scripts' / 'globals' / 'spells').is_dir(),
                     'no LuaJIT or no Phoenix checkout')
class RealCheckout(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run(['luajit', str(SCRIPT), str(REAL), str(LIVE_SETTINGS), tmp], check=True, capture_output=True)
            lines = (pathlib.Path(tmp) / 'phoenix_spell_durations.tsv').read_text(encoding='utf-8').splitlines()
        cls.header = lines[0]
        cls.seconds = {int(spell): int(s) for spell, s in (line.split('\t') for line in lines[1:])}

    def test_the_header(self):
        self.assertEqual(self.header, 'spell\tseconds')

    def test_a_table_duration(self):
        self.assertEqual(self.seconds[253], 60)  # Sleep, enfeebling_spell.lua
        self.assertEqual(self.seconds[259], 90)  # Sleep II
        self.assertEqual(self.seconds[43], 1800)  # Protect, enhancing_spell.lua
        self.assertEqual(self.seconds[463], 30)  # Foe Lullaby, enfeebling_song.lua
        self.assertEqual(self.seconds[394], 120)  # Valor Minuet, enhancing_song.lua's flat 120
        self.assertEqual(self.seconds[347], 180)  # Kurayami: Ichi

    def test_phoenix_overrides_and_random_durations_at_their_longest(self):
        self.assertEqual(self.seconds[254], 300)  # Blind: randomInt(80, 300) over core's 180
        self.assertEqual(self.seconds[58], 120)  # Paralyze: randomInt(30, 120)
        self.assertEqual(self.seconds[220], 30)  # Poison: 30 over core's 90
        self.assertEqual(self.seconds[353], 180)  # Tonko: Ichi, the era's 180 over 420
        self.assertEqual(self.seconds[137], 300)  # Sneak: randomInt(30, 300)
        self.assertEqual(self.seconds[258], 60)  # Bind: core's randomInt(13, 60)

    def test_spells_with_their_own_code_are_left_out(self):
        self.assertNotIn(230, self.seconds)  # Bio
        self.assertNotIn(23, self.seconds)  # Dia
        self.assertNotIn(281, self.seconds)  # Pyrohelix, in enfeebling_spell.lua's table but its script's own
        self.assertNotIn(338, self.seconds)  # Utsusemi: Ichi, whose table gives 0


if __name__ == '__main__':
    unittest.main()

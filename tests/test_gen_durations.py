import contextlib
import io
import pathlib
import tempfile
import unittest

from tooling import ROOT, load

gen = load('tools/gen_durations.py')

DURATIONS = 'spell\tseconds\n43\t1800\n58\t120\n'


class Generate(unittest.TestCase):
    def test_spells_in_order(self):
        self.assertIn('    {43, 1800},\n    {58, 120},\n', gen.generate(gen.parse(DURATIONS)))

    def test_a_file_without_its_header_is_an_error(self):
        with self.assertRaises(gen.DataError):
            gen.parse(DURATIONS.replace('spell\tseconds', 'id\tseconds'))
        with self.assertRaises(gen.DataError):
            gen.parse('')

    def test_a_spell_out_of_order_or_again_is_an_error(self):
        with self.assertRaises(gen.DataError) as raised:
            gen.parse(DURATIONS + '44\t1800\n')
        self.assertIn('line 4', str(raised.exception))
        with self.assertRaises(gen.DataError):
            gen.parse(DURATIONS + '58\t60\n')

    def test_a_row_the_plugin_cannot_hold_is_an_error(self):
        for row in ['x\t60\n', '59\n', '59\t\n', '59\t60\t1\n', '59\t0\n', '59\t65536\n', '65536\t60\n']:
            with self.assertRaises(gen.DataError, msg=row):
                gen.parse(DURATIONS + row)


class Check(unittest.TestCase):
    """refresh.sh checks a refresh's durations with the generator's own rules before they reach data/."""

    def check(self, text: str) -> int:
        generated = ROOT / 'src' / 'generated' / 'spell_durations.inc'
        before = generated.read_bytes() if generated.is_file() else None
        with tempfile.TemporaryDirectory() as tmp:
            (pathlib.Path(tmp) / 'phoenix_spell_durations.tsv').write_text(text, encoding='utf-8')
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                status = gen.main(['gen_durations.py', '--check', tmp])
        self.assertEqual(generated.read_bytes() if generated.is_file() else None, before)  # writes nothing
        return status

    def test_a_good_file_passes_and_a_bad_one_fails(self):
        self.assertEqual(self.check(DURATIONS), 0)
        self.assertEqual(self.check(DURATIONS + '1\t60\n'), 1)  # out of order


if __name__ == '__main__':
    unittest.main()

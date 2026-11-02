import pathlib
import tempfile
import unittest

from tooling import load

blue = load('tools/phoenix/blue_magic.py')

SQL = ("INSERT INTO `blue_spell_list` VALUES (513,4,1,0,0,0,0,0,NULL); -- Venom Shell\n"
       "INSERT INTO `blue_spell_list` VALUES (622,256,4,0,0,0,0,0,NULL);\n"
       "INSERT INTO `blue_spell_list` VALUES (519,256,4,0,0,0,0,0,NULL);\n"
       "-- INSERT INTO `blue_spell_list` VALUES (723,0,0,0,0,12,10,0,NULL); -- Saurian Slide\n")
REAL = pathlib.Path.home() / '.cache' / 'headsup-phoenix' / 'server'


class Parse(unittest.TestCase):
    def test_each_skill_teaches_its_lowest_spell(self):
        self.assertEqual(blue.parse(SQL), {4: 513, 256: 519})

    def test_a_file_with_no_rows_is_an_error(self):
        with self.assertRaises(blue.BlueMagicError):
            blue.parse('DROP TABLE IF EXISTS `blue_spell_list`;\n')

    def test_writes_the_table_by_skill(self):
        with tempfile.TemporaryDirectory() as tmp:
            blue.write(pathlib.Path(tmp), {256: 519, 4: 513})
            self.assertEqual((pathlib.Path(tmp) / 'phoenix_blue_magic.tsv').read_text(encoding='utf-8'),
                             'skill\tspell\n4\t513\n256\t519\n')

    @unittest.skipUnless((REAL / 'sql' / 'blue_spell_list.sql').is_file(), 'no Phoenix checkout')
    def test_the_real_checkout(self):
        spells = blue.parse((REAL / 'sql' / 'blue_spell_list.sql').read_text(encoding='utf-8'))
        self.assertEqual(spells[264], 584)  # Sheep Song teaches Sheep Song


if __name__ == '__main__':
    unittest.main()

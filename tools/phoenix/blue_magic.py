#!/usr/bin/env python3
"""Read which TP moves teach which blue magic from a Phoenix checkout into data/phoenix_blue_magic.tsv.

Usage: blue_magic.py SERVER_DIR OUT_DIR. sql/blue_spell_list.sql pairs each blue spell with the mob skills a blue mage
learns it from. A skill that teaches two spells keeps the lower one.
"""
import pathlib
import re
import sys

ROW = re.compile(r"^INSERT INTO `blue_spell_list` VALUES \((\d+),(\d+),", re.MULTILINE)  # not one commented out


class BlueMagicError(ValueError):
    pass


def parse(text: str) -> dict:
    """Each skill's spell, by skill."""
    spells = {}
    for spell, skill in ROW.findall(text):
        spells[int(skill)] = min(int(spell), spells.get(int(skill), int(spell)))
    if not spells:
        raise BlueMagicError('blue_spell_list.sql lists no blue magic')
    return spells


def write(out: pathlib.Path, spells: dict) -> None:
    out.mkdir(parents=True, exist_ok=True)
    rows = ['skill\tspell'] + [f'{skill}\t{spell}' for skill, spell in sorted(spells.items())]
    (out / 'phoenix_blue_magic.tsv').write_text('\n'.join(rows) + '\n', encoding='utf-8')


def main(argv) -> int:
    if len(argv) != 3:
        print('usage: blue_magic.py SERVER_DIR OUT_DIR', file=sys.stderr)
        return 2
    try:
        spells = parse((pathlib.Path(argv[1]) / 'sql' / 'blue_spell_list.sql').read_text(encoding='utf-8'))
    except (OSError, BlueMagicError) as error:
        print(f'blue_magic: {error}', file=sys.stderr)
        return 1
    write(pathlib.Path(argv[2]), spells)
    print(f'blue magic: {len(spells)} TP moves -> {argv[2]}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))

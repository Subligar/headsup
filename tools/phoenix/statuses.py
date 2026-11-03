#!/usr/bin/env python3
"""Read status effect names from a Phoenix checkout into data/.

Usage: statuses.py SERVER_DIR OUT_DIR. data/status_effects.yaml keys each status by an identifier and gives the game's
name in a `name:` line where the two differ ("sleep_ii" is the game's "sleep"); each name is that in words.
"""
import pathlib
import re
import sys

SMALL_WORDS = {'of', 'the', 'and', 'in', 'to', 'a', 'an', 'for', 'on', 'at', 'by', 'with', 'from'}
# Stats, pools and numerals the game writes in capitals.
CAPITALS = {'str', 'dex', 'vit', 'agi', 'int', 'mnd', 'chr', 'hp', 'mp', 'tp', 'ko', 'ii', 'iii', 'iv', 'v', 'vi'}
SECTION = 'status_effects:'


class StatusDataError(ValueError):
    pass


def display_name(identifier: str) -> str:
    def word(i: int, w: str) -> str:
        if w in CAPITALS:
            return w.upper()
        if i > 0 and w in SMALL_WORDS:
            return w
        return '-'.join(part.capitalize() for part in w.split('-'))
    return ' '.join(word(i, w) for i, w in enumerate(identifier.lower().split('_')))


def parse_statuses(text: str) -> list:
    """(id, name) for each status, in id order."""
    if f'\n{SECTION}\n' not in f'\n{text}':
        raise StatusDataError(f'status_effects.yaml has no {SECTION} section')
    body = f'\n{text}'.split(f'\n{SECTION}\n', 1)[1]
    rows, seen, key, fields = [], set(), None, {}

    def close():
        if key is None:
            return
        if 'id' not in fields:
            raise StatusDataError(f'status_effects.yaml gives {key} no id')
        status = int(fields['id'])
        if status in seen:
            raise StatusDataError(f'status_effects.yaml gives status {status} again, to {key}')
        seen.add(status)
        rows.append((status, display_name(fields.get('name', key))))

    for line in body.splitlines():
        if not line.strip() or line.lstrip().startswith('#'):
            continue
        if not line.startswith(' '):
            break  # the next top-level section
        if m := re.fullmatch(r'  ([A-Za-z0-9_-]+):\s*', line):
            close()
            key, fields = m.group(1), {}
        elif m := re.fullmatch(r'    (id|name):\s+(\S+)\s*', line):
            if m.group(1) == 'id' and not m.group(2).isdigit():
                raise StatusDataError(f'status_effects.yaml gives {key} the id {m.group(2)}, not a number')
            fields[m.group(1)] = m.group(2)
    close()
    if not rows:
        raise StatusDataError('status_effects.yaml lists no statuses')
    return sorted(rows)


def build(server: pathlib.Path) -> list:
    return parse_statuses((server / 'data' / 'status_effects.yaml').read_text(encoding='utf-8'))


def write(out: pathlib.Path, rows) -> None:
    out.mkdir(parents=True, exist_ok=True)
    lines = ['id\tname'] + [f'{status}\t{name}' for status, name in rows]
    (out / 'phoenix_statuses.tsv').write_text('\n'.join(lines) + '\n', encoding='utf-8')


def main(argv) -> int:
    if len(argv) != 3:
        print('usage: statuses.py SERVER_DIR OUT_DIR', file=sys.stderr)
        return 2
    try:
        rows = build(pathlib.Path(argv[1]))
    except (OSError, StatusDataError) as error:
        print(f'statuses: {error}', file=sys.stderr)
        return 1
    write(pathlib.Path(argv[2]), rows)
    print(f'statuses: {len(rows)} statuses -> {argv[2]}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))

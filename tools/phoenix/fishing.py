#!/usr/bin/env python3
"""Read Phoenix's fishing tables from a checkout into data/.

Usage: fishing.py SERVER_DIR OUT_DIR. Reads sql/fishing_*.sql as the map server loads them (fishingutils.cpp's
Load functions) and writes phoenix_fish.tsv, phoenix_fish_areas.tsv, phoenix_fish_groups.tsv, phoenix_fish_baits.tsv
and phoenix_fish_rods.tsv.
"""
import pathlib
import re
import struct
import sys

TABLES = ('fishing_fish', 'fishing_area', 'fishing_catch', 'fishing_group', 'fishing_bait', 'fishing_bait_affinity',
          'fishing_rod')
NO_QUEST = 255          # a fish's log and quest, when no quest asks for it
DROPPED_RANKING = 99    # the server loads only fish ranked below this
SHELLFISH_FISH = 0x01   # FISHFLAG_SHELLFISH
SHELLFISH_BAIT = 0x40   # BAITFLAG_SHELLFISH_AFFINITY
LURE = 1                # FISHINGBAITTYPE_LURE
MAX_POINTS = 32         # LoadFishingAreas' MAX_BOUNDS
POINT = struct.Struct('<fff')
RADIUS_BITS = 0xFF      # the server reads bound_radius into a uint8

HEADERS = {
    'phoenix_fish.tsv': 'item\tskill\tsize\tis_item\tlegendary\tshellfish\tquest\tquest_only\trarity\tkey_item',
    'phoenix_fish_areas.tsv': 'zone\tarea\ttype\theight\tradius\tx\ty\tz\tgroup\tpoints',
    'phoenix_fish_groups.tsv': 'group\titem',
    'phoenix_fish_baits.tsv': 'bait\tlure\tshellfish\taffinities',
    'phoenix_fish_rods.tsv': 'rod\tsize\tlegendary',
}


class FishingDataError(ValueError):
    pass


def values(text: str, table: str) -> list:
    """Each INSERT row of the table as its fields: numbers, strings without quotes, blobs as bytes, NULL as None."""
    rows = []
    field = r"'(?:[^'\\]|\\.)*'|0x[0-9A-Fa-f]+|NULL|-?[\d.]+"
    for body in re.findall(rf'^INSERT INTO `{table}` VALUES \((.*)\);', text, re.M):
        fields = []
        for token in re.findall(field, body):
            if token.startswith("'"):
                fields.append(token[1:-1].replace("\\'", "'"))
            elif token.startswith('0x'):
                fields.append(bytes.fromhex(token[2:]))
            elif token == 'NULL':
                fields.append(None)
            else:
                fields.append(float(token) if '.' in token else int(token))
        rows.append(fields)
    return rows


def polygon(blob) -> list:
    if not isinstance(blob, bytes):
        return []
    points = [POINT.unpack_from(blob, at) for at in range(0, len(blob) - POINT.size + 1, POINT.size)]
    while points and points[-1] == (0.0, 0.0, 0.0):
        points.pop()
    if len(points) > MAX_POINTS:
        raise FishingDataError(f'an area has {len(points)} points; the server reads {MAX_POINTS}')
    return [(x, z) for x, _, z in points]


def build(server: pathlib.Path) -> dict:
    texts = {}
    for table in TABLES:
        path = server / 'sql' / f'{table}.sql'
        if not path.exists():
            raise FishingDataError(f'{path} is missing')
        texts[table] = path.read_text(encoding='utf-8')
    fish = []
    for f in values(texts['fishing_fish'], 'fishing_fish'):
        if f[28] != 0 or f[8] >= DROPPED_RANKING:
            continue
        fish.append({'item': f[0], 'skill': f[2], 'size': f[9], 'is_item': f[20], 'legendary': int(f[18] != 0),
                     'shellfish': int(f[14] & SHELLFISH_FISH != 0), 'quest': int(f[11] < NO_QUEST and f[12] < NO_QUEST),
                     'quest_only': f[26], 'rarity': f[22], 'key_item': f[23]})
    loaded = {f['item'] for f in fish}
    # The server keeps one group per area: a later row for the same area replaces the earlier.
    groups_of = {(c[0], c[1]): c[2] for c in values(texts['fishing_catch'], 'fishing_catch')}
    areas = {}
    for a in values(texts['fishing_area'], 'fishing_area'):
        areas[(a[0], a[1])] = {'zone': a[0], 'area': a[1], 'type': a[3], 'height': a[4], 'radius': a[5] & RADIUS_BITS,
                               'x': float(a[7]), 'y': float(a[8]), 'z': float(a[9]), 'group': groups_of.get((a[0], a[1]), 0),
                               'points': polygon(a[6])}
    groups = sorted({(g[0], g[1]) for g in values(texts['fishing_group'], 'fishing_group') if g[1] in loaded})
    affinities = {}
    for b in values(texts['fishing_bait_affinity'], 'fishing_bait_affinity'):
        if b[1] in loaded:
            affinities.setdefault(b[0], []).append((b[1], b[2]))
    baits = [{'bait': b[0], 'lure': int(b[2] == LURE), 'shellfish': int(b[5] & SHELLFISH_BAIT != 0),
              'affinities': sorted(affinities.get(b[0], []))} for b in values(texts['fishing_bait'], 'fishing_bait')]
    rods = sorted((r[0], r[3], r[20]) for r in values(texts['fishing_rod'], 'fishing_rod'))
    return {'fish': sorted(fish, key=lambda f: f['item']), 'areas': [areas[key] for key in sorted(areas)],
            'groups': groups, 'baits': sorted(baits, key=lambda b: b['bait']), 'rods': rods}


def number(value) -> str:
    return f'{value:g}' if isinstance(value, float) else str(value)


def write(out: pathlib.Path, data: dict) -> None:
    out.mkdir(parents=True, exist_ok=True)
    tables = {
        'phoenix_fish.tsv': [[f[k] for k in HEADERS['phoenix_fish.tsv'].split('\t')] for f in data['fish']],
        'phoenix_fish_areas.tsv': [[a['zone'], a['area'], a['type'], a['height'], a['radius'], a['x'], a['y'], a['z'],
                                    a['group'], ' '.join(f'{number(x)}:{number(z)}' for x, z in a['points'])]
                                   for a in data['areas']],
        'phoenix_fish_groups.tsv': [list(g) for g in data['groups']],
        'phoenix_fish_baits.tsv': [[b['bait'], b['lure'], b['shellfish'], ' '.join(f'{i}:{p}' for i, p in b['affinities'])]
                                   for b in data['baits']],
        'phoenix_fish_rods.tsv': [list(r) for r in data['rods']],
    }
    for name, rows in tables.items():
        lines = [HEADERS[name]] + ['\t'.join(number(v) if not isinstance(v, str) else v for v in row) for row in rows]
        (out / name).write_text('\n'.join(lines) + '\n', encoding='utf-8')


def main(argv) -> int:
    if len(argv) != 3:
        print('usage: fishing.py SERVER_DIR OUT_DIR', file=sys.stderr)
        return 2
    try:
        data = build(pathlib.Path(argv[1]))
    except (OSError, FishingDataError) as error:
        print(f'fishing: {error}', file=sys.stderr)
        return 1
    write(pathlib.Path(argv[2]), data)
    print(f'fishing: {len(data["fish"])} catches, {len(data["areas"])} areas, {len(data["baits"])} baits -> {argv[2]}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))

#!/usr/bin/env python3
"""Generate src/generated/icons.inc and icon_ids.h from the MobDB icons in third_party/mobdb-icons (MIT License,
ThornyFFXI), XIUI's player icons and gil in third_party/xiui-icons (MIT License, tirem) and HeadsUp's own in resources/icons,
src/generated/status_packs.h, status_packs.inc and status_pack_N.z from the status icon packs in resources/icons/status,
and src/generated/pointer.inc from the chocobo mouse pointer in third_party/playonline-chocobo.

Each PNG is decoded here, cut to its visible pixels on a square, and compiled into the plugin as raw pixels, ready to
copy into a Direct3D texture. A status pack's icons are kept as they are drawn, a pack's pixels compressed together,
so a pack takes memory only once it is picked. The animated cursor file is embedded byte for byte; src/cursor_file.cpp
reads its frames.
"""
import pathlib
import re
import struct
import sys
import zlib

# The icons in headsup::Icon order, which this list alone sets: MobDB's detection icons, XIUI's player icons, MobDB's
# damage and status icons, then HeadsUp's own, white to be tinted; for each the enum name, the PNG from the repository's
# root, and what the icon means when the name does not say.
ICONS = [(name, f'third_party/mobdb-icons/{name}.png', '') for name in
         ['AggroNQ', 'AggroHQ', 'PassiveNQ', 'PassiveHQ', 'Link', 'Sight', 'TrueSight', 'Sound', 'Scent', 'Magic']] + [
    ('Ability', 'third_party/mobdb-icons/JA.png', 'job abilities and weapon skills'),
    ('Blood', 'third_party/mobdb-icons/Blood.png', 'low HP'),
    ('Invite', 'third_party/xiui-icons/invite_icon.png', 'seeking a party'),
    ('Bazaar', 'third_party/xiui-icons/bazaar_icon.png', ''),
    ('Linkshell', 'third_party/xiui-icons/linkshell_icon.png', ''),
    ('Away', 'third_party/xiui-icons/away_icon.png', ''),
    ('Mentor', 'third_party/xiui-icons/mentor_icon.png', ''),
    ('NewAdventurer', 'third_party/xiui-icons/newadventurer_icon.png', ''),
    ('Gm', 'third_party/xiui-icons/gm_icon.png', ''),
    ('LevelSync', 'third_party/xiui-icons/levelsync_icon.png', ''),
] + [(name, f'third_party/mobdb-icons/{name}.png', '') for name in
     ['Slashing', 'Piercing', 'Impact', 'H2H', 'Fire', 'Ice', 'Wind', 'Earth', 'Lightning', 'Water', 'Light', 'Dark']] + [
    ('Sleep', 'third_party/mobdb-icons/ImmuneSleep.png', 'light and dark sleep'),
] + [(name, f'third_party/mobdb-icons/Immune{name}.png', '') for name in
     ['LightSleep', 'DarkSleep', 'Gravity', 'Bind', 'Stun', 'Silence', 'Paralyze', 'Blind', 'Slow', 'Poison', 'Elegy',
      'Requiem', 'Petrify']] + [
    ('Pinned', 'resources/icons/pinned.png', 'a pin, grayed until pinned'),
    ('Star', 'resources/icons/star_filled.png', 'a favorite'),
    ('StarEmpty', 'resources/icons/star_unfilled.png', 'not a favorite'),
    ('Clone', 'resources/icons/copy.png', 'clones a widget'),
    ('Delete', 'resources/icons/delete.png', "takes a widget's copy off"),
    ('Pencil', 'resources/icons/pencil.png', 'renames a custom theme'),
    ('Flag', 'resources/icons/flag.png', 'a region with no supply run yet'),
    ('Bag', 'resources/icons/bag.png', "the Inventory widget's bag"),
    ('Clock', 'resources/icons/clock.png', "the Clock widget's clock"),
] + [(f'Weather{name}', f'resources/icons/weather/{file}.png', '') for name, file in [
    ('Sunny', 'sunny'), ('Fair', 'fair'), ('Cloudy', 'cloudy'), ('Foggy', 'foggy'), ('Thunder', 'thunder'),
    ('Thunderstorms', 'thunderstorms'), ('Aurora', 'aurora'), ('Gloom', 'gloom'), ('Snow', 'snow'),
    ('Blizzard', 'blizzard'), ('Windy', 'windy'), ('Gales', 'gales'), ('Rain', 'rain'), ('Squalls', 'squalls'),
    ('DustStorm', 'dust_storm'), ('HeatSpell', 'heat_spell')]] + [
    ('Reset', 'resources/icons/reset.png', 'puts a moved pop-out back in its place'),
    ('BuffBack', 'resources/icons/status/BuffIcon.png', "behind a buff's status icon"),
    ('DebuffBack', 'resources/icons/status/DebuffIcon.png', "behind a debuff's"),
    ('UnknownStatus', 'resources/icons/status/XIView/unknown.png', 'a status with no icon in its pack or the game'),
    ('Gil', 'third_party/xiui-icons/gil.png', "gil's coins"),
]
# Each folder here is a pack of status icons, each named by its status id, as XIUI's are.
STATUS_PACKS = 'resources/icons/status'
STATUS_FILE = re.compile(r'(\d+)\.png')
PACK_NAME = re.compile(r'[A-Za-z0-9 _-]+')  # said in the settings window and written into C++ as it is
# An icon is drawn at about a quarter of its pixels with linear filtering and no smaller copies, which mixes the clear
# pixels two out from its edge into it: those take the colors beside them, so the edge keeps its own.
BLEED_RINGS = 2
POINTER = 'playonline-chocobo/chocobo.ani'  # under third_party
PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'
RGBA_8_BIT = (8, 6)     # bit depth, color type
RGB_8_BIT = (8, 2)
PALETTE_8_BIT = (8, 3)
OPAQUE = 255
BYTES_PER_PIXEL = 4
MAX_SIDE = 64
BYTES_PER_LINE = 16
# Adam7 interlacing: each pass's first column, first row, column step and row step.
ADAM7 = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]


class IconError(ValueError):
    pass


def paeth(left: int, up: int, up_left: int) -> int:
    p = left + up - up_left
    pa, pb, pc = abs(p - left), abs(p - up), abs(p - up_left)
    return left if pa <= pb and pa <= pc else up if pb <= pc else up_left


def unfilter(raw: bytes, pos: int, width: int, height: int, bpp: int) -> tuple[list[bytearray], int]:
    stride = width * bpp
    rows, prev = [], bytearray(stride)
    for _ in range(height):
        if pos + 1 + stride > len(raw):
            raise IconError('image data ends early')
        kind, row = raw[pos], bytearray(raw[pos + 1:pos + 1 + stride])
        pos += 1 + stride
        for i in range(stride):
            left = row[i - bpp] if i >= bpp else 0
            up_left = prev[i - bpp] if i >= bpp else 0
            predictions = (0, left, prev[i], (left + prev[i]) // 2, paeth(left, prev[i], up_left))
            if kind >= len(predictions):
                raise IconError(f'unknown row filter {kind}')
            row[i] = (row[i] + predictions[kind]) & 0xFF
        rows.append(row)
        prev = row
    return rows, pos


def decode_bgra(data: bytes) -> tuple[int, int, bytes]:
    """Width, height and the pixels of an 8-bit RGBA or palette PNG, rows top to bottom, each pixel B, G, R, A as
    D3DFMT_A8R8G8B8 keeps it in memory."""
    if not data.startswith(PNG_SIGNATURE):
        raise IconError('not a PNG')
    header, compressed, palette, alphas, pos = None, b'', None, b'', len(PNG_SIGNATURE)
    while pos + 8 <= len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', body)
        elif kind == b'IDAT':
            compressed += body
        elif kind == b'PLTE':
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body) - 2, 3)]
        elif kind == b'tRNS':
            alphas = body
        elif kind == b'IEND':
            break
    if header is None:
        raise IconError('no IHDR chunk')
    width, height, depth, color, _, _, interlace = header
    if (depth, color) not in (RGBA_8_BIT, RGB_8_BIT, PALETTE_8_BIT):
        raise IconError(f'bit depth {depth} and color type {color}; only 8-bit RGBA, RGB and palette are supported')
    if (depth, color) == PALETTE_8_BIT and not palette:
        raise IconError('a palette PNG without its palette')
    if not 0 < width <= MAX_SIDE or not 0 < height <= MAX_SIDE:
        raise IconError(f'{width}x{height} pixels; at most {MAX_SIDE}x{MAX_SIDE}')
    try:
        raw = zlib.decompress(compressed)
    except zlib.error as error:
        raise IconError(f'image data does not decompress: {error}') from None

    rgb = (depth, color) == RGB_8_BIT
    bpp = 1 if palette is not None else 3 if rgb else BYTES_PER_PIXEL
    rgba, pos = bytearray(width * height * BYTES_PER_PIXEL), 0
    for x0, y0, dx, dy in ADAM7 if interlace else [(0, 0, 1, 1)]:
        columns, lines = range(x0, width, dx), range(y0, height, dy)
        if not columns or not lines:
            continue
        rows, pos = unfilter(raw, pos, len(columns), len(lines), bpp)
        for y, row in zip(lines, rows):
            for i, x in enumerate(columns):
                at = (y * width + x) * BYTES_PER_PIXEL
                if palette is None:
                    rgba[at:at + BYTES_PER_PIXEL] = row[i * bpp:(i + 1) * bpp] + (bytes([OPAQUE]) if rgb else b'')
                    continue
                index = row[i]
                if index >= len(palette):
                    raise IconError(f'palette index {index} past its {len(palette)} colors')
                rgba[at:at + BYTES_PER_PIXEL] = bytes(palette[index]) + bytes([alphas[index] if index < len(alphas) else 255])
    bgra = bytearray(rgba)
    bgra[0::4], bgra[2::4] = rgba[2::4], rgba[0::4]
    return width, height, bytes(bgra)


def trim_square(width: int, height: int, bgra: bytes) -> tuple[int, int, bytes]:
    """The pixels cut to the box of every pixel not fully transparent and centered on a square as wide as its longer
    side, so icons whose images carry different margins fill the same size on screen."""
    visible = [(x, y) for y in range(height) for x in range(width) if bgra[(y * width + x) * BYTES_PER_PIXEL + 3] != 0]
    if not visible:
        raise IconError('no visible pixels')
    left, top = min(x for x, _ in visible), min(y for _, y in visible)
    w, h = max(x for x, _ in visible) - left + 1, max(y for _, y in visible) - top + 1
    side = max(w, h)
    dx, dy = (side - w) // 2, (side - h) // 2
    out = bytearray(side * side * BYTES_PER_PIXEL)
    for y in range(h):
        start = ((top + y) * width + left) * BYTES_PER_PIXEL
        at = ((dy + y) * side + dx) * BYTES_PER_PIXEL
        out[at:at + w * BYTES_PER_PIXEL] = bgra[start:start + w * BYTES_PER_PIXEL]
    return side, side, bytes(out)


def bled(width: int, height: int, bgra: bytes) -> bytes:
    """The pixels with each fully clear one within BLEED_RINGS of a visible one given the mean color of those beside
    it, a ring at a time, its alpha left 0."""
    out = bytearray(bgra)
    filled = [out[i * BYTES_PER_PIXEL + 3] != 0 for i in range(width * height)]
    for _ in range(BLEED_RINGS):
        ring = []
        for y in range(height):
            for x in range(width):
                if filled[y * width + x]:
                    continue
                beside = [(nx, ny) for ny in range(y - 1, y + 2) for nx in range(x - 1, x + 2)
                          if 0 <= nx < width and 0 <= ny < height and filled[ny * width + nx]]
                if beside:
                    starts = [(ny * width + nx) * BYTES_PER_PIXEL for nx, ny in beside]
                    count = len(beside)
                    ring.append((y * width + x, [(sum(out[s + c] for s in starts) + count // 2) // count for c in range(3)]))
        for index, color in ring:
            out[index * BYTES_PER_PIXEL:index * BYTES_PER_PIXEL + 3] = bytes(color)
            filled[index] = True
    return bytes(out)


def c_bytes(name: str, data: bytes) -> str:
    lines = [f'const unsigned char {name}[] = {{']
    for start in range(0, len(data), BYTES_PER_LINE):
        lines.append('    ' + ' '.join(f'0x{b:02x},' for b in data[start:start + BYTES_PER_LINE]))
    lines.append('};')
    return '\n'.join(lines)


def c_bitmap(name: str, width: int, height: int, bgra: bytes) -> str:
    return c_bytes(f'kBgra{name}', bgra) + f'\nconst IconBitmap kIcon{name} = {{kBgra{name}, {width}, {height}}};'


def generate(root: pathlib.Path) -> str:
    folders = ', '.join(sorted({relative.rsplit('/', 1)[0] for _, relative, _ in ICONS}))
    parts = [f'// Generated by tools/gen_icons.py from {folders}. Do not edit.']
    for name, relative, _ in ICONS:
        path = pathlib.Path(root) / relative
        if not path.is_file():
            raise IconError(f'missing icon {path}')
        try:
            width, height, bgra = trim_square(*decode_bgra(path.read_bytes()))
            bgra = bled(width, height, bgra)
            parts.append(c_bitmap(name, width, height, bgra))
        except IconError as error:
            raise IconError(f'{path}: {error}') from None
    parts.append('const IconBitmap kIcons[] = {' + ', '.join(f'kIcon{name}' for name, _, _ in ICONS) + '};')
    return '\n'.join(parts) + '\n'


def square(width: int, height: int, bgra: bytes) -> tuple[int, bytes]:
    """The pixels centered on a square as wide as their longer side, clear around them."""
    side = max(width, height)
    dx, dy = (side - width) // 2, (side - height) // 2
    out = bytearray(side * side * BYTES_PER_PIXEL)
    for y in range(height):
        at = ((dy + y) * side + dx) * BYTES_PER_PIXEL
        out[at:at + width * BYTES_PER_PIXEL] = bgra[y * width * BYTES_PER_PIXEL:(y + 1) * width * BYTES_PER_PIXEL]
    return side, bytes(out)


def status_packs(root: pathlib.Path) -> list[tuple[str, list[tuple[int, int, bytes]]]]:
    """Each pack's folder name in name order, and its icons in id order: the status id, the side and the pixels."""
    packs = []
    for folder in sorted(path for path in (pathlib.Path(root) / STATUS_PACKS).iterdir() if path.is_dir()):
        if not PACK_NAME.fullmatch(folder.name):
            raise IconError(f'{folder}: a pack name of letters, digits, spaces, _ and - only')
        icons = []
        for path in folder.iterdir():
            if not (named := STATUS_FILE.fullmatch(path.name)):
                continue
            try:
                icons.append((int(named[1]), *square(*decode_bgra(path.read_bytes()))))
            except IconError as error:
                raise IconError(f'{path}: {error}') from None
        packs.append((folder.name, sorted(icons)))
    return packs


def generate_status(packs: list[tuple[str, list[tuple[int, int, bytes]]]]) -> tuple[str, str, dict[str, bytes]]:
    """status_packs.h (the packs' names), status_packs.inc (where each icon's pixels start in its pack) and each pack's
    pixels, compressed."""
    names = ['// Generated by tools/gen_icons.py. Do not edit.', '#pragma once', '', '#include <string_view>', '',
             'namespace headsup', '{', f'    // The folders of {STATUS_PACKS}, each a pack of status icons.',
             '    constexpr std::string_view kStatusPackNames[] = {' + ', '.join(f'"{name}"' for name, _ in packs) + '};',
             f'    constexpr int kStatusPackCount = {len(packs)};', '}', '']
    table, blobs, entries = [f'// Generated by tools/gen_icons.py from {STATUS_PACKS}. Do not edit.'], {}, []
    for index, (_, icons) in enumerate(packs):
        blob, places, at = f'status_pack_{index}.z', [], 0
        for status, side, pixels in icons:
            places.append(f'{{{status}, {side}, {at}}}')
            at += len(pixels)
        blobs[blob] = zlib.compress(b''.join(pixels for _, _, pixels in icons), 9)
        table += [f'const unsigned char kStatusPack{index}[] = {{', f'#embed "{blob}"', '};',
                  f'const PackedIcon kStatusPackIcons{index}[] = {{' + ', '.join(places) + '};']
        entries.append(f'{{kStatusPack{index}, sizeof(kStatusPack{index}), {at}, kStatusPackIcons{index}, {len(icons)}}}')
    table.append('const StatusPack kStatusPacks[] = {' + ', '.join(entries) + '};')
    return '\n'.join(names), '\n'.join(table) + '\n', blobs


def generate_ids(icons: list[tuple[str, str, str]] = ICONS) -> str:
    files = [relative.rsplit('/', 1)[1] for _, relative, _ in icons]
    if len(set(files)) != len(files):
        raise IconError('two icons share a file name, which the icon adjustments name them by')
    lines = ['// Generated by tools/gen_icons.py. Do not edit.', '#pragma once', '', '#include <cstdint>', '#include <string_view>', '',
             'namespace headsup', '{',
             "    // The MobDB detection icons XIUI shows, its player icons, MobDB's damage and status icons, then HeadsUp's own.", '    enum class Icon : uint8_t', '    {']
    lines += [f'        {name},' + (f' // {meaning}' if meaning else '') for name, _, meaning in icons]
    lines += ['    };', f'    constexpr int kIconCount = {len(icons)};',
              "    // Each icon's file name, which the icon adjustments name it by.",
              '    constexpr std::string_view kIconFiles[] = {' + ', '.join(f'"{file}"' for file in files) + '};', '}', '']
    return '\n'.join(lines)


def generate_pointer(path: pathlib.Path) -> str:
    if not path.is_file():
        raise IconError(f'missing pointer {path}')
    return (f'// Generated by tools/gen_icons.py from third_party/{POINTER}. Do not edit.\n'
            + c_bytes('kChocoboPointerFile', path.read_bytes()) + '\n')


def main() -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    generated = root / 'src' / 'generated'
    try:
        icons = generate(root)
        ids = generate_ids()
        pointer = generate_pointer(root / 'third_party' / POINTER)
        packs = status_packs(root)
        pack_names, pack_table, pack_blobs = generate_status(packs)
    except IconError as error:
        print(f'gen_icons: {error}', file=sys.stderr)
        return 1
    generated.mkdir(parents=True, exist_ok=True)
    (generated / 'icons.inc').write_text(icons, encoding='utf-8')
    (generated / 'icon_ids.h').write_text(ids, encoding='utf-8')
    (generated / 'pointer.inc').write_text(pointer, encoding='utf-8')
    (generated / 'status_packs.h').write_text(pack_names, encoding='utf-8')
    (generated / 'status_packs.inc').write_text(pack_table, encoding='utf-8')
    for blob, data in pack_blobs.items():
        (generated / blob).write_bytes(data)
    print(f'icons: {len(ICONS)}, {len(packs)} status packs and the chocobo pointer -> {generated}')
    return 0


if __name__ == '__main__':
    sys.exit(main())

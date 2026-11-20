#!/usr/bin/env python3
"""Generate src/generated/world.inc from data/phoenix_travel.tsv, data/phoenix_guilds.tsv and data/phoenix_turnin_hq.tsv,
with each route's and guild shop's region from data/phoenix_zones.tsv.

With --check DIR, check DIR's four files by the same rules and write nothing: refresh.sh does, before it copies a
Phoenix refresh's files into data/.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from gen_mobdata import c_string  # noqa: E402
from gen_zones import UNKNOWN_REGION, parse_zones  # noqa: E402

TRAVEL_HEADER = 'group\tname\tevery\tdepartures'
GUILDS_HEADER = 'guild\tshops\tzone\topen\tclose\tholiday'
HIGH_HEADER = 'item'
MOST_ITEM = 0xFFFF
MOST_DEPARTURES = 2  # a route's runs, as the plugin's table holds them
MOST_SECONDS = 0xFFFF
HOURS_A_DAY, DAYS_A_WEEK = 24, 8
# A route's group, as Phoenix's zone data names it, and how a player takes it.
MODES = {'Airships': 'Airship', 'Boats': 'Boat', 'Manaclipper': 'Manaclipper', 'Barge': 'Barge'}
# The place a route leaves from, as its name says it, and the zone it leaves from: "Jeuno to Bastok" leaves Port Jeuno.
ROUTE_PLACES = {
    'Bastok': 'Port Bastok', 'Jeuno': 'Port Jeuno', "San d'Oria": "Port San d'Oria", 'Windurst': 'Port Windurst',
    'Kazham': 'Kazham', 'Mhaura': 'Mhaura', 'Selbina': 'Selbina', 'Nashmau': 'Nashmau',
    'Whitegate': 'Aht Urhgan Whitegate', 'Bibiki Bay': 'Bibiki Bay', 'Purgonorgo Isle': 'Bibiki Bay',
    'Central': "Carpenters' Landing", 'North': "Carpenters' Landing", 'South': "Carpenters' Landing",
}
TOUR = ' tour'  # a route that comes back where it left, "Bibiki Bay tour"


class DataError(ValueError):
    pass


def rows(text: str, header: str, name: str):
    lines = text.splitlines()
    if not lines or lines[0] != header:
        raise DataError(f'{name}: the header must be ' + header.replace('\t', ', '))
    for number, line in enumerate(lines[1:], start=2):
        yield number, line.split('\t')


def parse_travel(text: str) -> list:
    routes = []
    for number, fields in rows(text, TRAVEL_HEADER, 'phoenix_travel.tsv'):
        where = f'phoenix_travel.tsv line {number}'
        if len(fields) != 4 or not fields[0] or not fields[1] or not fields[2].isdigit():
            raise DataError(f'{where}: a group, a name, a cycle and departures')
        every = int(fields[2])
        departures = []
        for pair in fields[3].split(','):
            board, _, leave = pair.partition(':')
            if not board.isdigit() or not leave.isdigit() or int(board) >= every or int(leave) >= every:
                raise DataError(f'{where}: departure {pair} is not two times inside the {every} s cycle')
            departures.append((int(board), int(leave)))
        if not 0 < every <= MOST_SECONDS or not 0 < len(departures) <= MOST_DEPARTURES:
            raise DataError(f'{where}: the plugin holds cycles to {MOST_SECONDS} s and {MOST_DEPARTURES} departures')
        routes.append((fields[0], fields[1], every, departures))
    return routes


def parse_guilds(text: str) -> list:
    shops = []
    for number, fields in rows(text, GUILDS_HEADER, 'phoenix_guilds.tsv'):
        where = f'phoenix_guilds.tsv line {number}'
        if len(fields) != 6 or not all(fields[:3]) or not all(field.isdigit() for field in fields[3:]):
            raise DataError(f'{where}: a guild, its shops, a zone, hours and a holiday')
        opens, closes, holiday = (int(field) for field in fields[3:])
        if not opens < closes <= HOURS_A_DAY or holiday >= DAYS_A_WEEK:
            raise DataError(f'{where}: open {opens} to {closes} on all but day {holiday} is no week of hours')
        shops.append((fields[0], fields[1], fields[2], opens, closes, holiday))
    return shops


def parse_high_quality(text: str) -> list:
    items = []
    for number, fields in rows(text, HIGH_HEADER, 'phoenix_turnin_hq.tsv'):
        if len(fields) != 1 or not fields[0].isdigit() or not 0 < int(fields[0]) <= MOST_ITEM:
            raise DataError(f'phoenix_turnin_hq.tsv line {number}: an item id')
        items.append(int(fields[0]))
    return items


def zone_regions(text: str) -> dict:
    """Each zone's name and its region; a name two zones share must have one region."""
    regions = {}
    for zone in parse_zones(text):
        if regions.setdefault(zone['name'], zone['region']) != zone['region']:
            raise DataError(f'phoenix_zones.tsv: {zone["name"]} is in two regions')
    return regions


def leg(group: str, name: str, regions: dict) -> tuple:
    """How the route is taken and where to ("Airship to Bastok"), and the region it leaves from."""
    if group not in MODES:
        raise DataError(f'phoenix_travel.tsv: {group} ({name}) is no group gen_world.py knows')
    place, to, where = (name[:-len(TOUR)], '', 'tour') if name.endswith(TOUR) else (*name.split(' to ', 1), '')
    zone = ROUTE_PLACES.get(place)
    if zone is None or regions.get(zone, UNKNOWN_REGION) == UNKNOWN_REGION:
        raise DataError(f'phoenix_travel.tsv: {name} leaves from no zone gen_world.py can place')
    return f'{MODES[group]} {where or "to " + to}', regions[zone]


def generate(routes: list, shops: list, high: list, regions: dict) -> str:
    lines = ['// Generated by tools/gen_world.py from data/phoenix_travel.tsv, data/phoenix_guilds.tsv, '
             'data/phoenix_turnin_hq.tsv and data/phoenix_zones.tsv. Do not edit.',
             f'static_assert(kMostDepartures == {MOST_DEPARTURES}, "gen_world.py writes {MOST_DEPARTURES} a route");',
             'const TravelRoute kTravelRoutes[] = {']
    legs = [leg(group, name, regions) for group, name, _, _ in routes]
    for (group, name, every, departures), (taken, region) in zip(routes, legs):
        times = ', '.join(f'{{{board}, {leave}}}' for board, leave in departures)
        # Two from one region to one place would read alike in its list ("Barge to Central Landing" from two landings).
        if legs.count((taken, region)) > 1:
            taken = f'{MODES[group]}: {name}'
        lines.append(f'    {{{c_string(group)}, {c_string(name)}, {every}, {len(departures)}, {{{times}}}, '
                     f'{c_string(taken)}, {region}}},')
    lines += ['};', 'const GuildShop kGuildShops[] = {']
    for guild, names, zone, opens, closes, holiday in shops:
        if zone not in regions:
            raise DataError(f'phoenix_guilds.tsv: {guild} ({names}) is in {zone}, which phoenix_zones.tsv lacks')
        lines.append(f'    {{{c_string(guild)}, {c_string(names)}, {c_string(zone)}, {opens}, {closes}, {holiday}, '
                     f'{regions[zone]}}},')
    lines.append('};')
    lines.append('const uint16_t kHighQualityTurnIns[] = {' + ', '.join(str(item) for item in sorted(high)) + '};')
    return '\n'.join(lines) + '\n'


def main(argv) -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    check = argv[1:2] == ['--check']
    if len(argv) != (3 if check else 1):
        print('usage: gen_world.py [--check DATA_DIR]', file=sys.stderr)
        return 2
    data = pathlib.Path(argv[2]) if check else root / 'data'
    try:
        routes = parse_travel((data / 'phoenix_travel.tsv').read_text(encoding='utf-8'))
        shops = parse_guilds((data / 'phoenix_guilds.tsv').read_text(encoding='utf-8'))
        high = parse_high_quality((data / 'phoenix_turnin_hq.tsv').read_text(encoding='utf-8'))
        regions = zone_regions((data / 'phoenix_zones.tsv').read_text(encoding='utf-8'))
        text = generate(routes, shops, high, regions)
    except (OSError, DataError, ValueError) as error:
        print(f'gen_world: {error}', file=sys.stderr)
        return 1
    if check:
        print(f'world: {len(routes)} routes, {len(shops)} guild shops in {data} pass')
        return 0
    out = root / 'src' / 'generated' / 'world.inc'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text, encoding='utf-8')
    print(f'world: {len(routes)} routes, {len(shops)} guild shops -> {out}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))

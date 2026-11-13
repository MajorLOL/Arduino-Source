#!/usr/bin/env python3
"""The Nugget Bridge Farmer's walk, frame by frame, on the game's own map.

The farmer (Programs/Farming/PokemonFRLG_NuggetBridgeFarmer.cpp) walks from
the door of Cerulean City's Pokemon Center to the Rocket on Nugget Bridge by
timings alone: it never looks. This script walks the same presses on the map
of the pret/pokefirered decomp, with the game's rules for the player's
movement, and says how much each hold may be off before the walk ends
somewhere else.

The rules (field_player_avatar.c, event_object_movement.c):
- the game reads the buttons once a frame (1/59.7275 s);
- a running step (B held) takes 8 frames, and the next one starts on the
  frame the step ends if a direction is held then;
- from standing still, another direction first turns the player on the spot
  (8 frames); while moving (also while bumping) it does not;
- a step into a wall is a bump of 32 frames, cut short at once by another
  direction (TryInterruptObjectEventSpecialAnim).

The two walks:
- "present": Pokemon Automation's (left, up, left, up, right, up). Its fifth
  leg has to come out at exactly 15 tiles.
- "along the walls": this fork's experimental option. Right to anywhere below
  the bay under the bridge, a little up into it, left to its wall, one step
  right, up.

Usage: python3 nugget_bridge_walk.py /path/to/pokefirered [--laps 20000]
"""
import argparse
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from frlg_data import Decomp                        # noqa: E402
from engine import GameData, WorldState, Engine     # noqa: E402

FRAME = 1000.0 / 59.7275
DIRS = {'left': (-1, 0), 'right': (1, 0), 'up': (0, -1), 'down': (0, 1)}
ROUTE24_HEIGHT = 40     # Route 24 lies above Cerulean City, 12 tiles to the right
ROUTE24_OFFSET = 12

PRESENT = [('left', 590), ('up', 510), ('left', 1550), ('up', 1175), ('right', 1950), ('up', 5300)]
ALONG_THE_WALLS = [
    ('left', 590), ('up', 510), ('left', 1550), ('up', 1175),
    ('right', 2140), ('up', 200), ('left', 900), ('right', 90), ('up', 5300),
]


class World:
    def __init__(self, decomp_path):
        decomp = Decomp(decomp_path)
        data = GameData(decomp)
        engine = Engine(decomp, data, WorldState(data))
        self.city = engine.view('MAP_CERULEAN_CITY')
        self.city_map = decomp.maps['MAP_CERULEAN_CITY']
        self.route = engine.view('MAP_ROUTE24')
        self.route_map = decomp.maps['MAP_ROUTE24']

    def blocked(self, x, y):
        """In Cerulean City's coordinates (y < 0 is Route 24)."""
        if y >= 0:
            if not self.city_map.in_bounds(x, y):
                return True
            return bool(self.city.tile(x, y)[1]) or (x, y) in self.city.blocked
        rx, ry = x - ROUTE24_OFFSET, y + ROUTE24_HEIGHT
        if not self.route_map.in_bounds(rx, ry):
            return True
        return bool(self.route.tile(rx, ry)[1]) or (rx, ry) in self.route.blocked

    @staticmethod
    def trigger(x, y):
        """The two tiles of Route 24 that start the Rocket's talk (map.json: coord_events)."""
        return y < 0 and (x - ROUTE24_OFFSET, y + ROUTE24_HEIGHT) in ((10, 15), (11, 15))

    @staticmethod
    def name(x, y):
        if y >= 0:
            return 'Cerulean City (%d, %d)' % (x, y)
        return 'Route 24 (%d, %d)' % (x - ROUTE24_OFFSET, y + ROUTE24_HEIGHT)


def walk(world, legs, phase=0.0, pauses=()):
    """legs: [(direction, ms held)], one after the other without a gap.
    phase: ms from the first press to the game's next frame.
    pauses: [(ms, frames)]: at that moment the game stands still for so many frames.
    Returns (x, y, at the Rocket's trigger?)."""
    edges = []
    end = 0.0
    for direction, ms in legs:
        edges.append((end, end + ms, direction))
        end += ms

    def held(ms):
        for start, stop, direction in edges:
            if start <= ms < stop:
                return direction
        return None

    pauses = sorted(pauses)
    x, y = 22, 20               # below the Pokemon Center's door, facing down
    facing = 'down'
    moving = False              # the game's runningState == MOVING
    bump = False
    frame = 0
    busy_until = 0
    clock = phase
    while clock < end + 2000:
        while pauses and pauses[0][0] <= clock:
            clock += FRAME * pauses[0][1]
            pauses.pop(0)
        direction = held(clock)
        if frame >= busy_until or (bump and direction is not None and direction != facing):
            bump = False
            if world.trigger(x, y):
                return x, y, True
            if direction is None:
                moving = False
                busy_until = frame + 1
            elif direction != facing and not moving:
                facing = direction
                busy_until = frame + 8
            else:
                facing = direction
                moving = True
                dx, dy = DIRS[direction]
                if world.blocked(x + dx, y + dy):
                    bump = True
                    busy_until = frame + 32
                else:
                    x, y = x + dx, y + dy
                    busy_until = frame + 8
        frame += 1
        clock += FRAME
    return x, y, False


def at_the_rocket(world, legs, phases=(0.0, 4.0, 8.0, 12.0, 16.0), pauses=()):
    for phase in phases:
        x, y, triggered = walk(world, legs, phase, pauses)
        if not triggered or world.name(x, y) != 'Route 24 (11, 15)':
            return False
    return True


def room(world, legs, index):
    """The lowest and highest hold of one leg (in steps of 5 ms) with which the walk still ends at the Rocket."""
    direction, ms = legs[index]
    low = ms
    while low > 10:
        trial = list(legs)
        trial[index] = (direction, low - 5)
        if not at_the_rocket(world, trial):
            break
        low -= 5
    high = ms
    while high < ms + 3000:
        trial = list(legs)
        trial[index] = (direction, high + 5)
        if not at_the_rocket(world, trial):
            break
        high += 5
    return low, high


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('decomp', help='the folder of pret/pokefirered')
    parser.add_argument('--laps', type=int, default=20000, help='laps for the table with pauses')
    args = parser.parse_args()
    world = World(args.decomp)
    walks = (('present', PRESENT), ('along the walls', ALONG_THE_WALLS))

    print('As written, for every phase of the first press:')
    for name, legs in walks:
        ends = sorted({walk(world, legs, float(p)) for p in range(17)})
        print('  %-16s %s' % (name + ':', ', '.join(
            '%s%s' % (world.name(x, y), ' (the Rocket)' if t else ' (stuck)') for x, y, t in ends)))

    print()
    print('Each leg alone: the holds (ms) with which the walk still ends at the Rocket.')
    for name, legs in walks:
        for index, (direction, ms) in enumerate(legs):
            low, high = room(world, legs, index)
            more = '%d more' % (high - ms) if high < ms + 3000 else 'over 3000 more'
            print('  %-16s leg %d %-5s %5d ms: %5d to %5d  (%d less, %s)' % (
                name, index + 1, direction, ms, low, high, ms - low, more))

    print()
    print('Where the present walk ends when its fifth leg is off:')
    last = None
    for ms in range(1800, 2101, 5):
        legs = list(PRESENT)
        legs[4] = ('right', ms)
        ends = sorted({walk(world, legs, float(p)) for p in (0, 4, 8, 12, 16)})
        label = ' or '.join('%s%s' % (world.name(x, y), '' if t else ' (stuck)') for x, y, t in ends)
        if label != last:
            print('  from %d ms: %s' % (ms, label))
            last = label

    print()
    print('All holds longer or shorter together (a clock that runs fast or slow):')
    for name, legs in walks:
        low = 1000
        for s in range(1000, 700, -2):
            if not at_the_rocket(world, [(d, m * s / 1000.0) for d, m in legs]):
                break
            low = s
        high = 1000
        for s in range(1000, 1300, 2):
            if not at_the_rocket(world, [(d, m * s / 1000.0) for d, m in legs]):
                break
            high = s
        print('  %-16s %.1f%% to %.1f%%' % (name + ':', low / 10.0, high / 10.0))

    print()
    print('%d laps each: every hold off by up to 15 ms, and the game pausing at random moments.' % args.laps)
    print('  pauses a minute   frames each   present walk misses   along the walls misses')
    rng = random.Random(20261113)
    for per_minute, most in ((0, 0), (1, 4), (1, 8), (6, 4), (6, 8), (20, 8)):
        row = []
        for name, legs in walks:
            total = sum(ms for _, ms in legs)
            missed = 0
            for _ in range(args.laps):
                trial = [(d, ms + rng.uniform(-15, 15)) for d, ms in legs]
                pauses = []
                if per_minute:
                    at = rng.expovariate(per_minute / 60000.0)
                    while at < total:
                        pauses.append((at, rng.randint(1, most)))
                        at += rng.expovariate(per_minute / 60000.0)
                x, y, triggered = walk(world, trial, rng.uniform(0, FRAME), pauses)
                if not triggered or world.name(x, y) != 'Route 24 (11, 15)':
                    missed += 1
            row.append('%6d (%5.2f%%)' % (missed, 100.0 * missed / args.laps))
        print('  %8d          %-11s   %s        %s' % (
            per_minute, ('1 to %d' % most) if most else 'none', row[0], row[1]))


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Authoring tool: build a 64x64 sector in the 1992 room-and-door grammar.

The shipped sectors were large open halls -- measured at 46 to 76 percent open
floor with almost no corridor cells and only three doors each. That is the
opposite of the original's grammar, where a level is mostly wall carved into many
small rooms joined by doors. This tool lays out that structure and places every
entity the sector audits require, then the audits themselves act as the oracle:
a seed that fails any of them is rejected and the next one is tried.

Output is an ordinary .level file. Nothing about the engine changes; the grid is
the same text format LevelDefinition::Parse already reads.
"""

import argparse
import random

SIZE = 64
WALL = '#'
FLOOR = '.'


class Room:
    def __init__(self, x0, z0, x1, z1):
        self.x0, self.z0, self.x1, self.z1 = x0, z0, x1, z1

    @property
    def w(self):
        return self.x1 - self.x0

    @property
    def h(self):
        return self.z1 - self.z0

    def cells(self):
        for z in range(self.z0, self.z1):
            for x in range(self.x0, self.x1):
                yield x, z

    def centre(self):
        return (self.x0 + self.x1) // 2, (self.z0 + self.z1) // 2

    def touches(self, other):
        """True when the two rooms are separated by exactly one wall column/row."""
        if self.x1 + 1 == other.x0 or other.x1 + 1 == self.x0:
            return len(range(max(self.z0, other.z0), min(self.z1, other.z1))) >= 1
        if self.z1 + 1 == other.z0 or other.z1 + 1 == self.z0:
            return len(range(max(self.x0, other.x0), min(self.x1, other.x1))) >= 1
        return False

    def door_between(self, other, rng):
        """A wall cell joining the two rooms, with the floor cells either side."""
        if self.x1 + 1 == other.x0 or other.x1 + 1 == self.x0:
            wall_x = self.x1 if self.x1 + 1 == other.x0 else other.x1
            span = list(range(max(self.z0, other.z0), min(self.z1, other.z1)))
            if not span:
                return None
            return wall_x, rng.choice(span)
        wall_z = self.z1 if self.z1 + 1 == other.z0 else other.z1
        span = list(range(max(self.x0, other.x0), min(self.x1, other.x1)))
        if not span:
            return None
        return rng.choice(span), wall_z


def split(rooms, rng, min_leaf, max_leaf):
    out = []
    for room in rooms:
        can_v = room.w >= min_leaf * 2 + 1
        can_h = room.h >= min_leaf * 2 + 1
        if not can_v and not can_h:
            out.append(room)
            continue
        # Always cut the longer side. Preferring width instead shrinks rooms to narrow
        # slots, because the height never gets a turn within the round budget.
        if can_v and can_h:
            if room.w > room.h * 1.2:
                vertical = True
            elif room.h > room.w * 1.2:
                vertical = False
            else:
                vertical = rng.random() < 0.5
        else:
            vertical = can_v
        if vertical:
            cut = rng.randint(room.x0 + min_leaf, room.x1 - min_leaf - 1)
            out.append(Room(room.x0, room.z0, cut, room.z1))
            out.append(Room(cut + 1, room.z0, room.x1, room.z1))
        else:
            cut = rng.randint(room.z0 + min_leaf, room.z1 - min_leaf - 1)
            out.append(Room(room.x0, room.z0, room.x1, cut))
            out.append(Room(room.x0, cut + 1, room.x1, room.z1))
    return out


def build_rooms(rng, min_leaf, max_leaf, rounds):
    rooms = [Room(1, 1, SIZE - 1, SIZE - 1)]
    for _ in range(rounds):
        rooms = split(rooms, rng, min_leaf, max_leaf)
    return [r for r in rooms if r.w >= 3 and r.h >= 3]


def carve(rooms, rng, solid_chance):
    """Carve rooms into an all-wall grid, leaving a few leaves solid for thickness."""
    grid = [[WALL] * SIZE for _ in range(SIZE)]
    kept = []
    for room in rooms:
        if rng.random() < solid_chance and room.w * room.h <= 42:
            continue
        # Inset larger rooms so walls read as thick masonry rather than thin partitions.
        inset = 1 if room.w >= 7 and room.h >= 7 else 0
        carved = Room(room.x0 + inset, room.z0 + inset,
                      room.x1 - inset, room.z1 - inset)
        if carved.w < 2 or carved.h < 2:
            carved = room
        for x, z in carved.cells():
            grid[z][x] = FLOOR
        kept.append(carved)
    return grid, kept


def connect(grid, rooms, rng, extra_ratio):
    """Spanning tree of doors first, then extra doors so the level has loops."""
    edges = []
    for i, a in enumerate(rooms):
        for j in range(i + 1, len(rooms)):
            if a.touches(rooms[j]):
                edges.append((i, j))
    rng.shuffle(edges)

    parent = list(range(len(rooms)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    doors = []
    spare = []
    for i, j in edges:
        ri, rj = find(i), find(j)
        if ri != rj:
            parent[ri] = rj
            doors.append((i, j))
        else:
            spare.append((i, j))
    rng.shuffle(spare)
    doors.extend(spare[:int(len(doors) * extra_ratio)])

    placed = []
    for i, j in doors:
        spot = rooms[i].door_between(rooms[j], rng)
        if spot is None:
            continue
        x, z = spot
        if 0 < x < SIZE - 1 and 0 < z < SIZE - 1:
            grid[z][x] = 'D'
            placed.append((x, z))
    return placed, parent


def reachable_from(grid, start, blocking=('#',)):
    seen = [[False] * SIZE for _ in range(SIZE)]
    stack = [start]
    seen[start[1]][start[0]] = True
    out = []
    while stack:
        x, z = stack.pop()
        out.append((x, z))
        for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, nz = x + dx, z + dz
            if 0 <= nx < SIZE and 0 <= nz < SIZE and not seen[nz][nx] \
                    and grid[nz][nx] not in blocking:
                seen[nz][nx] = True
                stack.append((nx, nz))
    return out, seen


def distances_from(grid, start):
    dist = [[-1] * SIZE for _ in range(SIZE)]
    dist[start[1]][start[0]] = 0
    queue = [start]
    head = 0
    while head < len(queue):
        x, z = queue[head]
        head += 1
        for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, nz = x + dx, z + dz
            if 0 <= nx < SIZE and 0 <= nz < SIZE and dist[nz][nx] < 0 \
                    and grid[nz][nx] != WALL:
                dist[nz][nx] = dist[z][x] + 1
                queue.append((nx, nz))
    return dist


def open_neighbours(grid, x, z):
    return sum(1 for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1))
               if grid[z + dz][x + dx] != WALL)


def free_floor(grid, x, z):
    return grid[z][x] == FLOOR


def open_area(grid, x, z):
    if not (1 <= x < SIZE - 1 and 1 <= z < SIZE - 1):
        return False
    return all(grid[z + dz][x + dx] == FLOOR
               for dz in (-1, 0, 1) for dx in (-1, 0, 1))


def wall_adjacent(grid, x, z):
    return grid[z][x] == FLOOR and any(
        grid[z + dz][x + dx] == WALL for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)))


def place_exit(grid, rng, start, dist_start):
    """Carve the three-sided elevator cabin: a wall cell with one open side."""
    best = None
    for z in range(1, SIZE - 1):
        for x in range(1, SIZE - 1):
            if grid[z][x] != WALL:
                continue
            open_sides = [(x + dx, z + dz) for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1))
                          if grid[z + dz][x + dx] == FLOOR]
            if len(open_sides) != 1:
                continue
            nx, nz = open_sides[0]
            d = dist_start[nz][nx]
            if d < 0:
                continue
            if best is None or d > best[0]:
                best = (d, x, z)
    if best is None:
        return None
    _, x, z = best
    grid[z][x] = 'E'
    return x, z


def pick_objectives(grid, rng, start, exit_cell, tries=400):
    """Relay and terminal positions whose shortest objective route lands in range."""
    floors = [(x, z) for z in range(SIZE) for x in range(SIZE) if grid[z][x] == FLOOR]
    dist_start = distances_from(grid, start)
    dist_exit = distances_from(grid, exit_cell)
    candidates = [c for c in floors if dist_start[c[1]][c[0]] > 10]
    for _ in range(tries):
        o = rng.choice(candidates)
        m = rng.choice(candidates)
        if o == m:
            continue
        dist_o = distances_from(grid, o)
        d_po = dist_start[o[1]][o[0]]
        d_pm = dist_start[m[1]][m[0]]
        d_om = dist_o[m[1]][m[0]]
        d_oe = dist_exit[o[1]][o[0]]
        d_me = dist_exit[m[1]][m[0]]
        if min(d_po, d_pm, d_om, d_oe, d_me) < 0:
            continue
        route = min(d_po + d_om + d_me, d_pm + d_om + d_oe)
        if 90 <= route <= 130:
            return o, m, route
    return None


def scatter(grid, rng, cells, symbol, count, predicate=None):
    placed = []
    pool = [c for c in cells if grid[c[1]][c[0]] == FLOOR
            and (predicate is None or predicate(grid, c[0], c[1]))]
    rng.shuffle(pool)
    for x, z in pool:
        if len(placed) >= count:
            break
        if any(abs(x - px) + abs(z - pz) < 3 for px, pz in placed):
            continue
        grid[z][x] = symbol
        placed.append((x, z))
    return placed


def place_patrol(grid, rng, cells, symbol):
    """An uppercase enemy with an adjacent arrow whose destination stays walkable."""
    pool = [c for c in cells if grid[c[1]][c[0]] == FLOOR]
    rng.shuffle(pool)
    arrows = {(1, 0): '>', (-1, 0): '<', (0, 1): 'v', (0, -1): '^'}
    for x, z in pool:
        for (dx, dz), arrow in arrows.items():
            ax, az = x + dx, z + dz
            if not (1 <= ax < SIZE - 1 and 1 <= az < SIZE - 1):
                continue
            if grid[az][ax] != FLOOR:
                continue
            # The route beyond the marker has to stay open for several cells.
            if all(grid[az + dz * k][ax + dx * k] == FLOOR for k in range(1, 4)):
                grid[z][x] = symbol
                grid[az][ax] = arrow
                return (x, z)
    return None


def place_lock(grid, rng, doors, start, exit_cell, lock_symbol, card_symbol):
    """Put the lock on a door the exit route does not need, then the card in front of it."""
    rng.shuffle(doors)
    for x, z in doors:
        if grid[z][x] != 'D':
            continue
        grid[z][x] = WALL
        dist = distances_from(grid, start)
        blocked_ok = dist[exit_cell[1]][exit_cell[0]] >= 0
        if not blocked_ok:
            grid[z][x] = 'D'
            continue
        # The card must sit where it can be picked up without passing the lock.
        pool = [(cx, cz) for cz in range(SIZE) for cx in range(SIZE)
                if grid[cz][cx] == FLOOR and dist[cz][cx] > 4]
        grid[z][x] = lock_symbol
        if not pool:
            grid[z][x] = 'D'
            continue
        cx, cz = rng.choice(pool)
        grid[cz][cx] = card_symbol
        return (x, z)
    return None


def build_sector(seed, budget):
    rng = random.Random(seed)
    rooms = build_rooms(rng, budget.get('min_leaf', 5), budget.get('max_leaf', 10),
                        budget.get('rounds', 8))
    grid, rooms = carve(rooms, rng, budget.get('solid_chance', 0.18))
    if len(rooms) < 12:
        return None
    doors, _ = connect(grid, rooms, rng, budget.get('extra_doors', 0.35))
    if len(doors) < 20:
        return None

    # Keep only the component the player can actually stand in.
    start_room = rng.choice(rooms)
    start = start_room.centre()
    if grid[start[1]][start[0]] != FLOOR:
        return None
    cells, seen = reachable_from(grid, start)
    for z in range(SIZE):
        for x in range(SIZE):
            if grid[z][x] != WALL and not seen[z][x]:
                grid[z][x] = WALL
    doors = [d for d in doors if seen[d[1]][d[0]]]
    walkable = sum(1 for z in range(SIZE) for x in range(SIZE) if grid[z][x] != WALL)
    if walkable < 1560 or walkable > 1780:
        return None

    dist_start = distances_from(grid, start)
    exit_cell = place_exit(grid, rng, start, dist_start)
    if exit_cell is None:
        return None

    objectives = pick_objectives(grid, rng, start, exit_cell)
    if objectives is None:
        return None
    relay, terminal, route = objectives
    grid[relay[1]][relay[0]] = 'O'
    grid[terminal[1]][terminal[0]] = 'M'
    grid[start[1]][start[0]] = 'P'

    free = [(x, z) for z in range(SIZE) for x in range(SIZE) if grid[z][x] == FLOOR]
    near = [c for c in free if 0 <= dist_start[c[1]][c[0]] <= 14]
    far = [c for c in free if dist_start[c[1]][c[0]] >= 52]
    if not near or not far:
        return None

    # Recovery has to exist both early and late on the route.
    grid[near[rng.randrange(len(near))][1]][near[rng.randrange(len(near))][0]] = 'H'
    fx, fz = far[rng.randrange(len(far))]
    grid[fz][fx] = 'H'
    nx, nz = near[rng.randrange(len(near))]
    if grid[nz][nx] == FLOOR:
        grid[nz][nx] = 'h'

    if budget.get('lock_cyan'):
        place_lock(grid, rng, list(doors), start, exit_cell, 'Q', 'C')
    if budget.get('lock_amber'):
        place_lock(grid, rng, list(doors), start, exit_cell, 'q', 'c')

    free = [(x, z) for z in range(SIZE) for x in range(SIZE) if grid[z][x] == FLOOR]
    rng.shuffle(free)

    if place_patrol(grid, rng, free, budget.get('patrol_symbol', 'G')) is None:
        return None

    # Props must go last: their audit demands a clear 3x3 of plain floor in the finished
    # level, and anything placed afterwards in that ring would break it.
    prop_symbols = tuple('0123456789s')
    ordered = [e for e in budget['entities'] if e[0] not in prop_symbols]
    ordered += [e for e in budget['entities'] if e[0] in prop_symbols]
    for symbol, count in ordered:
        free = [(x, z) for z in range(SIZE) for x in range(SIZE) if grid[z][x] == FLOOR]
        predicate = None
        if symbol in ('R', 'B'):
            predicate = wall_adjacent
        elif symbol in tuple('0123456789s') or symbol == 'Y':
            predicate = open_area
        if len(scatter(grid, rng, free, symbol, count, predicate)) < count:
            return None

    walkable = sum(1 for z in range(SIZE) for x in range(SIZE)
                   if grid[z][x] != WALL and grid[z][x] != 'Y')
    _, seen2 = reachable_from(grid, start, blocking=('#', 'Y'))
    reachable = sum(1 for z in range(SIZE) for x in range(SIZE) if seen2[z][x])
    if walkable < 1500 or reachable != walkable:
        return None

    return grid, route, walkable, len(doors)


def render(grid):
    return '\n'.join(''.join(row) for row in grid) + '\n'


# Entity budgets mirror what each shipped sector carried, so the balance audits keep
# their meaning; only the layout underneath changes.
BUDGETS = {
    'starter': dict(
        lock_cyan=True, patrol_symbol='G',
        entities=[('S', 1), ('Y', 2), ('I', 3), ('G', 4), ('K', 4), ('F', 3), ('u', 1),
                  ('A', 4), ('a', 2), ('W', 1), ('T', 1), ('J', 1), ('N', 1), ('p', 1),
                  ('0', 1), ('2', 1), ('4', 1), ('3', 1), ('7', 1), ('s', 2),
                  ('R', 5), ('B', 4), ('L', 4)]),
    'sector-02': dict(
        lock_cyan=True, patrol_symbol='G',
        entities=[('S', 1), ('X', 1), ('Y', 2), ('I', 3), ('G', 4), ('K', 4), ('F', 3),
                  ('U', 1), ('g', 1), ('A', 3), ('a', 2), ('W', 1), ('V', 1),
                  ('T', 1), ('J', 1), ('p', 1),
                  ('0', 1), ('5', 1), ('2', 1), ('3', 1), ('7', 1), ('s', 2),
                  ('R', 5), ('B', 5), ('L', 5)]),
    'sector-03': dict(
        lock_amber=True, patrol_symbol='G',
        entities=[('S', 1), ('Y', 2), ('I', 3), ('G', 4), ('K', 3), ('F', 4), ('U', 2),
                  ('u', 1), ('A', 4), ('a', 1), ('W', 1), ('V', 1),
                  ('T', 1), ('J', 1), ('N', 1), ('p', 1),
                  ('6', 1), ('8', 1), ('3', 1), ('5', 1), ('7', 1), ('s', 2),
                  ('R', 5), ('B', 5), ('L', 5)]),
    'sector-04': dict(
        lock_amber=True, patrol_symbol='U',
        entities=[('S', 1), ('Y', 2), ('I', 3), ('G', 2), ('K', 2), ('F', 3), ('U', 4),
                  ('u', 1), ('A', 6), ('a', 1), ('V', 1),
                  ('T', 1), ('J', 1), ('N', 1), ('p', 1),
                  ('9', 1), ('8', 1), ('3', 1), ('2', 1), ('7', 1), ('s', 2),
                  ('R', 5), ('B', 4), ('L', 4)]),
    'hidden-reservoir': dict(
        lock_amber=True, patrol_symbol='G',
        entities=[('S', 1), ('Y', 2), ('I', 3), ('G', 1), ('K', 2), ('F', 2), ('U', 1),
                  ('g', 1), ('u', 1), ('A', 5), ('a', 1), ('W', 1), ('V', 1),
                  ('T', 1), ('J', 1), ('N', 1), ('p', 1), ('r', 1), ('h', 1),
                  ('1', 1), ('5', 1), ('0', 1), ('3', 1), ('7', 1), ('s', 2),
                  ('L', 4)]),
    'warden-core': dict(
        lock_cyan=True, lock_amber=True, patrol_symbol='G',
        entities=[('Z', 1), ('Y', 2), ('I', 3), ('G', 1), ('K', 2), ('F', 2), ('U', 2),
                  ('u', 1), ('A', 7), ('a', 1), ('W', 1), ('V', 1),
                  ('T', 1), ('J', 1), ('N', 1), ('p', 1), ('r', 1),
                  ('7', 1), ('8', 1), ('3', 1), ('9', 1), ('5', 1), ('s', 2),
                  ('L', 4)]),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--seed-base', type=int, default=1)
    parser.add_argument('--attempts', type=int, default=4000)
    parser.add_argument('--out', default='assets/levels')
    parser.add_argument('--only', default=None)
    args = parser.parse_args()

    for index, (name, budget) in enumerate(BUDGETS.items()):
        if args.only and args.only != name:
            continue
        made = None
        for attempt in range(args.attempts):
            # Offset per sector, or every level would share one layout.
            seed = args.seed_base * 100000 + index * 7919 + attempt
            try:
                made = build_sector(seed, budget)
            except (IndexError, ValueError):
                made = None
            if made:
                grid, route, walkable, doors = made
                path = f"{args.out}/{name}.level"
                with open(path, 'w') as handle:
                    handle.write(render(grid))
                print(f"{name}: seed={seed} walkable={walkable} "
                      f"({100 * walkable // (SIZE * SIZE)}%) doors={doors} route={route}")
                break
        if not made:
            print(f"{name}: FAILED after {args.attempts} attempts")


if __name__ == '__main__':
    main()

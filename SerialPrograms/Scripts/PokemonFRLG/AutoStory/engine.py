"""
FRLG Autostory route engine.

World model + global router over every map, following the movement rules of
the pokefirered decomp:

  walking         GetCollisionAtCoords(), IsElevationMismatchAt(),
                  directional impassable behaviors, map connections
  ledges          ShouldJumpLedge(): 2 tiles, 32 frames (JUMP_DISTANCE_FAR)
  warps           doors (TryDoorWarp), mats and stairs (TryArrowWarp),
                  step-on warps (TryStartWarpEventScript), arrival rules
                  from field_fadetransition.c
  spinners        MB_SPIN_*: forced slide at 8 frames/tile until
                  MB_STOP_SPINNING or an obstacle (TryUpdatePlayerSpinDirection)
  surfing         8 frames/tile (PlayerWalkFast), water = elevation 1 tiles
  cut trees       OBJ_EVENT_GFX_CUT_TREE objects, removed by Cut
  coord triggers  active when their var equals var_value
  trainers        sight lines from trainer_sight_or_berry_tree_id

The router returns a list of "edges". story.py turns them into legs and
actions for the C++ program.
"""

import copy
import heapq
import json
import os
import re

from frlg_data import DIRS, OPPOSITE

#  ----------------------------------------------------------------- costs
COST_STEP = 1.0
COST_TURN = 0.35
COST_LAND_ENCOUNTER = 18.0     # ~1.5 s check per step plus the odds of a wild battle
REPEL_MARGIN = 10              # the last steps of a REPEL count as without it
COST_WATER_ENCOUNTER = 3.0
COST_NPC_RANGE = 25.0
COST_TRAINER_SIGHT = 400.0     # an unplanned trainer battle
#  From a tile 2+ away in the line of a trainer that turns around, a step
#  toward it: if it saw the player there, it walks up and stands right on
#  that tile.
COST_TOWARD_TRAINER = 2000.0
COST_WARP = 6.0
COST_CUT = 12.0
COST_SURF_START = 10.0
COST_LEDGE = 2.0
COST_SPIN = 0.5
#  Thin ice that breaks: the floor the player falls to (warphole).
ICE_FALL_BELOW = {"MAP_FOUR_ISLAND_ICEFALL_CAVE_1F": "MAP_FOUR_ISLAND_ICEFALL_CAVE_B1F"}
COST_ITEM = 20.0               # pick up an item ball that is in the way

DIR_LIST = ["up", "down", "left", "right"]

FACING_FOR_MOVEMENT = {
    "MOVEMENT_TYPE_FACE_UP": ["up"],
    "MOVEMENT_TYPE_FACE_DOWN": ["down"],
    "MOVEMENT_TYPE_FACE_LEFT": ["left"],
    "MOVEMENT_TYPE_FACE_RIGHT": ["right"],
    "MOVEMENT_TYPE_FACE_DOWN_AND_UP": ["down", "up"],
    "MOVEMENT_TYPE_FACE_LEFT_AND_RIGHT": ["left", "right"],
    "MOVEMENT_TYPE_FACE_UP_AND_LEFT": ["up", "left"],
    "MOVEMENT_TYPE_FACE_UP_AND_RIGHT": ["up", "right"],
    "MOVEMENT_TYPE_FACE_DOWN_AND_LEFT": ["down", "left"],
    "MOVEMENT_TYPE_FACE_DOWN_AND_RIGHT": ["down", "right"],
    "MOVEMENT_TYPE_FACE_DOWN_UP_AND_LEFT": ["down", "up", "left"],
    "MOVEMENT_TYPE_FACE_DOWN_UP_AND_RIGHT": ["down", "up", "right"],
    "MOVEMENT_TYPE_FACE_UP_LEFT_AND_RIGHT": ["up", "left", "right"],
    "MOVEMENT_TYPE_FACE_DOWN_LEFT_AND_RIGHT": ["down", "left", "right"],
}


COST_TRAINER_AREA = 300.0      # where a walking trainer may stand (and stay after a battle)


def trainer_walk_area(m, o, walkable):
    """
    Where a trainer that walks around can be, and which ways it faces
    there: {(x, y): set(directions)}. None for a trainer that stays put.
    event_object_movement.c: wanderers pick a direction at random inside
    their movement range; walkers go back and forth on a line; a walk
    sequence goes round in fixed directions (MoveNextDirectionInSequence).
    """
    mv = o["movement"]
    x0, y0, rx, ry = o["x"], o["y"], o["rx"], o["ry"]

    def inside(x, y):
        return (rx == 0 or abs(x - x0) <= rx) and (ry == 0 or abs(y - y0) <= ry) and m.in_bounds(x, y)

    area = {}
    if "WALK_SEQUENCE_" in mv:
        seq = [w.lower() for w in mv.split("WALK_SEQUENCE_")[1].split("_")]
        if len(seq) != 4 or any(d not in DIRS for d in seq):
            return None
        #  The direction that turns back to the start line (MovementType_
        #  WalkSequence*_Step1): the second if it is the opposite of the
        #  first, otherwise the third.
        k = 1 if seq[1] == OPPOSITE[seq[0]] else 2
        x, y, idx = x0, y0, 0
        area[(x, y)] = {seq[0]}
        for _ in range(400):
            if idx == k and ((seq[k] in ("up", "down") and y == y0) or (seq[k] in ("left", "right") and x == x0)):
                idx = k + 1
            if idx == 3 and (x, y) == (x0, y0):
                idx = 0
            d = seq[idx]
            dx, dy = DIRS[d]
            if not inside(x + dx, y + dy):
                idx = (idx + 1) % 4
                d = seq[idx]
                dx, dy = DIRS[d]
            #  It turns where it stands, then walks (or walks in place).
            area.setdefault((x, y), set()).add(d)
            if inside(x + dx, y + dy) and walkable(x + dx, y + dy):
                x, y = x + dx, y + dy
            area.setdefault((x, y), set()).add(d)
        return area
    if "WANDER" in mv or "WALK_" in mv:
        if "UP_AND_DOWN" in mv or "DOWN_AND_UP" in mv:
            dirs = ["up", "down"]
        elif "LEFT_AND_RIGHT" in mv or "RIGHT_AND_LEFT" in mv:
            dirs = ["left", "right"]
        elif "WANDER_AROUND" in mv:
            dirs = list(DIR_LIST)
        else:
            return None
        #  Every tile it can walk to inside its range.
        todo = [(x0, y0)]
        area[(x0, y0)] = set(dirs)
        while todo:
            x, y = todo.pop()
            for d in dirs:
                dx, dy = DIRS[d]
                n = (x + dx, y + dy)
                if n not in area and inside(*n) and walkable(*n):
                    area[n] = set(dirs)
                    todo.append(n)
        return area
    return None


def fixed_facing(movement):
    """The one direction a trainer always faces, or None if it turns."""
    dirs = FACING_FOR_MOVEMENT.get(movement)
    if dirs and len(dirs) == 1:
        return dirs[0]
    return None


def trainer_directions(movement):
    if movement in FACING_FOR_MOVEMENT:
        return FACING_FOR_MOVEMENT[movement]
    if "WALK" in movement or "WANDER" in movement:
        return list(DIR_LIST)
    #  LOOK_AROUND, ROTATE_*, etc.
    return list(DIR_LIST)


class EngineError(Exception):
    pass


#  ----------------------------------------------------------------- game data
class GameData:
    """Extra decomp data on top of frlg_data.Decomp."""

    def __init__(self, decomp):
        self.d = decomp
        root = decomp.root
        #  Maps with wild encounters.
        with open(os.path.join(root, "src/data/wild_encounters.json")) as f:
            groups = json.load(f)["wild_encounter_groups"]
        self.land_encounters = set()
        self.water_encounters = set()
        self.wild_species = {}
        #  The Kanto species in the grass (cave) of a map, FireRed and
        #  LeafGreen together (slugs as the program reads them: nidoran-f).
        self.land_slugs = {}
        #  The highest level of a wild Pokemon in the grass (or cave) of a map.
        self.wild_max_level = {}
        for g in groups:
            for e in g["encounters"]:
                if "land_mons" in e:
                    self.land_encounters.add(e["map"])
                    sp = self.wild_species.setdefault((e["map"], e.get("base_label", "")), [])
                    for mon in e["land_mons"]["mons"]:
                        sp.append(mon["species"])
                        self.land_slugs.setdefault(e["map"], set()).add(mon["species"][8:].lower().replace("_", "-"))
                        self.wild_max_level[e["map"]] = max(self.wild_max_level.get(e["map"], 0), mon["max_level"])
                if "water_mons" in e:
                    self.water_encounters.add(e["map"])
        #  Flags set by EventScript_ResetAllMapFlags (new game).
        text = open(os.path.join(root, "data/event_scripts.s")).read()
        m = re.search(r"EventScript_ResetAllMapFlags::(.*?)\n\tend", text, re.S)
        self.initial_flags = set(re.findall(r"setflag (FLAG_\w+)", m.group(1))) if m else set()
        #  Coordinate trigger scripts that only set vars/flags and return at
        #  once: walking over them does nothing visible.
        self.harmless_scripts = set()
        quiet = {"lockall", "releaseall", "lock", "release", "setvar", "setflag", "clearflag",
                 "addvar", "end", "return", "subvar", "copyvar"}
        for mid, mp in decomp.maps.items():
            p = os.path.join(root, "data/maps", mp.folder, "scripts.inc")
            if not os.path.exists(p):
                continue
            label = None
            ok = True
            for line in open(p):
                line = line.split("@")[0].rstrip()
                if not line.strip():
                    continue
                if not line.startswith((" ", "\t")) and line.endswith(":"):
                    if label and ok:
                        self.harmless_scripts.add(label)
                    label = line.rstrip(":")
                    ok = True
                    continue
                cmd = line.strip().split()[0]
                if cmd not in quiet:
                    ok = False
            if label and ok:
                self.harmless_scripts.add(label)
        self.metatile_ids = {}
        for line in open(os.path.join(root, "include/constants/metatile_labels.h")):
            mm = re.match(r"#define\s+(METATILE_\w+)\s+(0x[0-9A-Fa-f]+|\d+)", line)
            if mm:
                self.metatile_ids[mm.group(1)] = int(mm.group(2), 0)
        self.setmetatile_labels = {}
        for fn in sorted(os.listdir(os.path.join(root, "data/scripts"))):
            if fn.endswith(".inc"):
                self._parse_setmetatile(os.path.join(root, "data/scripts", fn))
        self.silph_doors = self._parse_silph_doors(decomp)
        #  Pokemon Mansion: one switch flag opens/closes barriers on all floors
        #  (data/scripts/pokemon_mansion.inc, ON_LOAD of each floor).
        self.mansion = {}
        for floor in ("1F", "2F", "3F", "B1F"):
            mid = "MAP_POKEMON_MANSION_" + floor
            if mid in decomp.maps:
                mp = decomp.maps[mid]
                self.mansion[mid] = {
                    "press": self.metatile_tiles(mp, "PokemonMansion_EventScript_PressSwitch_" + floor),
                    "reset": self.metatile_tiles(mp, "PokemonMansion_EventScript_ResetSwitch_" + floor),
                    "statues": [(e["x"], e["y"]) for e in mp.bg_events if e.get("script", "").endswith("_Statue")],
                }
        #  Maps that are dark without Flash.
        self.dark_maps = set()
        for mid, mp in decomp.maps.items():
            p = os.path.join(root, "data/maps", mp.folder, "map.json")
            with open(p) as f:
                if json.load(f).get("requires_flash"):
                    self.dark_maps.add(mid)


    def _parse_setmetatile(self, path):
        label = None
        for line in open(path):
            t = line.split("@")[0].strip()
            if t.endswith("::"):
                label = t[:-2]
                continue
            mm = re.match(r"setmetatile\s+(\d+),\s*(\d+),\s*(\w+),\s*(\w+)", t)
            if mm and label:
                self.setmetatile_labels.setdefault(label, []).append(
                    (int(mm.group(1)), int(mm.group(2)), mm.group(3), mm.group(4) in ("1", "TRUE")))

    def metatile_tiles(self, m, label):
        """{(x,y): (collision, behavior)} for a setmetatile label on map m."""
        out = {}
        for x, y, name, imp in self.setmetatile_labels.get(label, []):
            mid = self.metatile_ids[name]
            beh = self.d.metatile_attributes(m.layout, mid) & 0x1FF
            out[(x, y)] = (1 if imp else 0, beh)
        return out

    def _parse_silph_doors(self, decomp):
        """Silph Co. card key doors (data/scripts/silphco_doors.inc):
        map_id -> list of {flag, open, close, bg:set((x,y))}."""
        root = decomp.root
        text = open(os.path.join(root, "data/scripts/silphco_doors.inc")).read()
        script_door = {}
        for mm in re.finditer(r"(SilphCo_\w+_EventScript_Door\d*)::\s*lockall\s*setvar VAR_TEMP_1, (\d+)\s*setvar VAR_0x8004, (FLAG_\w+)", text):
            script_door[mm.group(1)] = (int(mm.group(2)), mm.group(3))
        open_label = {}
        for mm in re.finditer(r"call_if_eq VAR_TEMP_1, (\d+), (EventScript_Open\w+)", text):
            open_label[int(mm.group(1))] = mm.group(2)
        doors = {}
        for mid, mp in decomp.maps.items():
            if not mp.folder.startswith("SilphCo_"):
                continue
            p = os.path.join(root, "data/maps", mp.folder, "scripts.inc")
            close_for_flag = {}
            if os.path.exists(p):
                for mm in re.finditer(r"call_if_unset (FLAG_\w+), (EventScript_Close\w+)", open(p).read()):
                    close_for_flag[mm.group(1)] = mm.group(2)
            by_flag = {}
            for ev in mp.bg_events:
                sc = ev.get("script", "")
                if sc not in script_door:
                    continue
                k, flag = script_door[sc]
                d = by_flag.setdefault(flag, {"flag": flag, "bg": set(),
                                              "open": self.metatile_tiles(mp, open_label[k]),
                                              "close": self.metatile_tiles(mp, close_for_flag[flag])})
                d["bg"].add((ev["x"], ev["y"]))
            if by_flag:
                doors[mid] = list(by_flag.values())
        return doors


#  ----------------------------------------------------------------- world state
class WorldState:
    """
    Everything about the game that changes as the story goes on and matters
    for walking. Updated by the story spec ("set" steps) and by the router
    (trainers battled on the way).
    """

    def __init__(self, gd):
        self.gd = gd
        self.flags = set(gd.initial_flags)
        self.vars = {}
        self.defeated = set()               # (map_id, object key)
        self.object_overrides = {}          # map_id -> key -> dict
        self.tile_overrides = {}            # map_id -> (x,y) -> {"collision": 0/1}
        self.abilities = {"cut": False, "surf": False, "strength": False, "rock_smash": False}
        self.extra_blocked = {}             # map_id -> set((x,y)) (e.g. a trainer that moved)
        self.cut_done = {}                  # map_id -> set((x,y)) trees cut during this visit
        self.smash_done = {}                # map_id -> set((x,y)) rocks smashed during this visit
        self.ice_cracked = {}               # map_id -> set((x,y)) thin ice walked on during this visit
        self.map_vars = {}                  # map_id -> {VAR_TEMP_x: value} set by ON_TRANSITION
        #  Consumables the story bought: item -> count (ESCAPE_ROPE,
        #  MAX_REPEL); used by the router.
        self.items = {}
        #  Where an ESCAPE ROPE goes (UpdateEscapeWarp(): the last warp from
        #  outdoors into a building or a cave): (map, x, y).
        self.escape = None
        #  Steps the REPEL used still lasts (VAR_REPEL_STEP_COUNT): no wild
        #  battles meanwhile (the team leads by far after the Hall of Fame).
        self.repel_left = 0

    def clone(self):
        return copy.deepcopy(self)

    #  ------------------------------------------------------------ updates
    def apply(self, change):
        for f in change.get("flags", []):
            self.flags.add(f)
        for f in change.get("clear_flags", []):
            self.flags.discard(f)
        for k, v in change.get("vars", {}).items():
            self.vars[k] = int(v)
        for t in change.get("defeated", []):
            self.defeated.add((t[0], t[1]))
        for mid in change.get("reset_objects", []):
            #  The map loaded again: its objects are where the map has them.
            self.object_overrides.pop(mid, None)
        for mid, objs in change.get("objects", {}).items():
            mo = self.object_overrides.setdefault(mid, {})
            for key, ov in objs.items():
                mo.setdefault(key, {}).update(ov)
        for mid, tiles in change.get("tiles", {}).items():
            mt = self.tile_overrides.setdefault(mid, {})
            for xy, ov in tiles.items():
                x, y = (int(v) for v in xy.split(","))
                mt[(x, y)] = ov
        for k, v in change.get("abilities", {}).items():
            self.abilities[k] = bool(v)
        for mid, tiles in change.get("blocked", {}).items():
            #  Something standing on a tile until we leave the map.
            self.extra_blocked.setdefault(mid, set()).update((int(x), int(y)) for x, y in tiles)
        for mid, vs in change.get("map_vars", {}).items():
            self.map_vars.setdefault(mid, {}).update({k: int(v) for k, v in vs.items()})
        if change.get("clear_moved_trainers"):
            self.extra_blocked = {}

    #  ------------------------------------------------------------ objects
    @staticmethod
    def object_key(m, index):
        o = m.objects[index]
        return o.get("local_id") or str(index)

    def objects(self, m):
        """
        Returns a list of dicts for the objects present on map m:
          key, x, y, graphics, movement, rx, ry, trainer, sight, raw
        """
        out = []
        overrides = self.object_overrides.get(m.id, {})
        for i, o in enumerate(m.objects):
            if "x" not in o or "graphics_id" not in o:
                continue
            key = o.get("local_id") or str(i)
            ov = overrides.get(key, {})
            if ov.get("hidden"):
                continue
            flag = o.get("flag", "0") or "0"
            if flag != "0" and flag in self.flags and not ov.get("shown"):
                continue
            x = ov.get("x", o["x"])
            y = ov.get("y", o["y"])
            if not m.in_bounds(x, y):
                continue
            movement = ov.get("movement_type", o.get("movement_type", ""))
            try:
                sight = int(str(o.get("trainer_sight_or_berry_tree_id", "0")), 0)
            except ValueError:
                sight = 0
            out.append({
                "key": key, "index": i, "x": x, "y": y,
                "graphics": o.get("graphics_id", ""),
                "movement": movement,
                "rx": int(ov.get("movement_range_x", o.get("movement_range_x") or 0) or 0),
                "ry": int(ov.get("movement_range_y", o.get("movement_range_y") or 0) or 0),
                "trainer": o.get("trainer_type", "TRAINER_TYPE_NONE") not in ("TRAINER_TYPE_NONE", "", None),
                "sight": sight,
                "script": o.get("script", ""),
                "raw": o,
            })
        return out

    def is_defeated(self, m, key):
        return (m.id, key) in self.defeated


class MapView:
    """Precomputed walking info for one map under one world state."""

    def __init__(self, engine, m):
        self.m = m
        st = engine.state
        B = engine.B
        self.blocked = set()
        self.cut_trees = set()
        self.smash_rocks = set()
        self.boulders = set()
        self.items = {}         # (x,y) -> flag of an item ball
        self.soft = {}
        self.sight = {}         # (x,y) -> list of (trainer key, predictable, trainer_dir)
        objs = st.objects(m)
        self.objs = objs
        for o in objs:
            g = o["graphics"]
            xy = (o["x"], o["y"])
            if g.endswith("CUT_TREE"):
                if xy not in st.cut_done.get(m.id, set()):
                    self.cut_trees.add(xy)
                continue
            if g.endswith("ROCK_SMASH_ROCK"):
                if xy not in getattr(st, "smash_done", {}).get(m.id, set()):
                    self.smash_rocks.add(xy)
                continue
            if g.endswith("PUSHABLE_BOULDER"):
                self.boulders.add(xy)
                continue
            if g.endswith("ITEM_BALL") and o["raw"].get("flag", "0") not in ("0", "", None):
                self.items[xy] = o["raw"]["flag"]
                continue
            self.blocked.add(xy)
            mv = o["movement"]
            if "WANDER" in mv or "WALK_" in mv:
                #  Both orders: WALK_RIGHT_AND_LEFT (Hugh on Kindle Road) had
                #  no range at all.
                horiz = "AROUND" in mv or any(s in mv for s in ("LEFT_AND_RIGHT", "RIGHT_AND_LEFT", "LEFT_RIGHT", "RIGHT_LEFT"))
                vert = "AROUND" in mv or any(s in mv for s in ("UP_AND_DOWN", "DOWN_AND_UP", "UP_DOWN", "DOWN_UP"))
                xs = range(o["x"] - o["rx"], o["x"] + o["rx"] + 1) if horiz else [o["x"]]
                ys = range(o["y"] - o["ry"], o["y"] + o["ry"] + 1) if vert else [o["y"]]
                #  One that walks back and forth (not around) passes the
                #  tiles inside its line both ways, the two ends once: an
                #  end is half as often in the way (Kindle Road: the two
                #  BLACK BELTs on row 64, crossed at the middle of the path).
                back_and_forth = "AROUND" not in mv and (horiz != vert)
                for tx in xs:
                    for ty in ys:
                        if (tx, ty) != xy:
                            end = (horiz and abs(tx - o["x"]) == o["rx"]) or (vert and abs(ty - o["y"]) == o["ry"])
                            cost = COST_NPC_RANGE if (end or not back_and_forth) else 2 * COST_NPC_RANGE
                            self.soft[(tx, ty)] = self.soft.get((tx, ty), 0) + cost
            if o["trainer"] and o["sight"] > 0:
                walk_area = trainer_walk_area(
                    m, o, lambda x, y: m.tile(x, y)[1] == 0 or (x, y) in st.tile_overrides.get(m.id, {})
                )
            else:
                walk_area = None
            if walk_area is not None:
                #  A trainer that walks around: it may stand anywhere on its
                #  way (and stays there after a battle), and sees from there.
                for txy in walk_area:
                    if txy != xy:
                        self.soft[txy] = self.soft.get(txy, 0) + COST_TRAINER_AREA
                if not st.is_defeated(m, o["key"]):
                    seen = set()
                    for (ax, ay), facings in walk_area.items():
                        for d in facings:
                            dx, dy = DIRS[d]
                            for k in range(1, o["sight"] + 1):
                                tx, ty = ax + dx * k, ay + dy * k
                                if not m.in_bounds(tx, ty):
                                    break
                                if m.tile(tx, ty)[1] and (tx, ty) not in st.tile_overrides.get(m.id, {}):
                                    break
                                if (tx, ty, d, k) in seen:
                                    continue
                                seen.add((tx, ty, d, k))
                                self.sight.setdefault((tx, ty), []).append((o["key"], False, d, k))
            elif o["trainer"] and o["sight"] > 0 and not st.is_defeated(m, o["key"]):
                fixed = fixed_facing(mv)
                for d in trainer_directions(mv):
                    dx, dy = DIRS[d]
                    for k in range(1, o["sight"] + 1):
                        tx, ty = o["x"] + dx * k, o["y"] + dy * k
                        if not m.in_bounds(tx, ty):
                            break
                        #  Line of sight stops at collision and at objects.
                        if m.tile(tx, ty)[1] and (tx, ty) not in st.tile_overrides.get(m.id, {}):
                            break
                        self.sight.setdefault((tx, ty), []).append((o["key"], fixed is not None, d, k))
        for xy in st.extra_blocked.get(m.id, set()):
            self.blocked.add(xy)
        #  Coordinate triggers active under the current vars.
        self.triggers = {}
        for ev in m.coord_events:
            if ev.get("type", "trigger") != "trigger":
                continue
            var = ev.get("var", "")
            try:
                want = int(str(ev.get("var_value", "0")), 0)
            except ValueError:
                want = 0
            if var.startswith("VAR_TEMP_"):
                have = st.map_vars.get(m.id, {}).get(var, 0)
            else:
                have = st.vars.get(var, 0)
            if have == want and ev.get("script", "") not in engine.gd.harmless_scripts:
                self.triggers[(ev["x"], ev["y"])] = ev.get("script", "")
        self.tile_ov = dict(st.tile_overrides.get(m.id, {}))
        self.B = engine.B
        mansion = engine.gd.mansion.get(m.id)
        if mansion:
            on = "FLAG_POKEMON_MANSION_SWITCH_STATE" in st.flags
            for xy, (col, beh) in (mansion["press"] if on else mansion["reset"]).items():
                self.tile_ov[xy] = {"collision": col, "behavior": beh}
        #  Silph Co. card key doors: closed until their flag is set.
        self.doors = {}
        for door in engine.gd.silph_doors.get(m.id, []):
            opened = door["flag"] in st.flags
            for xy, (col, beh) in (door["open"] if opened else door["close"]).items():
                self.tile_ov[xy] = {"collision": col, "behavior": beh}
            if not opened:
                for xy in door["bg"]:
                    self.doors[xy] = door
        self.land_enc = m.id in engine.gd.land_encounters
        self.water_enc = m.id in engine.gd.water_encounters
        self.dark = m.id in engine.gd.dark_maps

    def tile(self, x, y):
        t = list(self.m.tile(x, y))
        ov = self.tile_ov.get((x, y))
        if ov is not None:
            if "collision" in ov:
                t[1] = int(ov["collision"])
            if "elevation" in ov:
                t[2] = int(ov["elevation"])
            if "behavior" in ov:
                b = ov["behavior"]
                t[3] = self.B.by_name[b] if isinstance(b, str) else int(b)
            if "encounter" in ov:
                t[4] = int(ov["encounter"])
        return tuple(t)


class MapGoals:
    """Route goal: any tile of these maps (the first one reached)."""

    def __init__(self, maps):
        self.maps = set(maps)

    def __contains__(self, t):
        return t[0] in self.maps


#  ----------------------------------------------------------------- engine
class Engine:
    def __init__(self, decomp, gamedata, state):
        self.d = decomp
        self.gd = gamedata
        self.B = decomp.behaviors
        self.state = state
        self._views = {}
        B = self.B
        self.water_behaviors = {
            B.MB_POND_WATER, B.MB_FAST_WATER, B.MB_DEEP_WATER, B.MB_OCEAN_WATER,
            B.MB_SEMI_DEEP_WATER if "MB_SEMI_DEEP_WATER" in B.by_name else -1,
            B.by_name.get("MB_WATERFALL", -1), B.by_name.get("MB_UNUSED_WATER", -1),
            B.by_name.get("MB_CYCLING_ROAD_WATER", -1),
        }
        self.step_on_warps = {
            B.MB_LADDER, B.MB_UP_ESCALATOR, B.MB_DOWN_ESCALATOR, B.MB_CAVE_DOOR,
            B.MB_FALL_WARP, B.MB_REGULAR_WARP, B.MB_LAVARIDGE_1F_WARP, B.MB_UNION_ROOM_WARP,
            B.by_name.get("MB_WARP_PAD", -1),
        }
        self.arrow_warps = {
            B.MB_SOUTH_ARROW_WARP: "down", B.MB_NORTH_ARROW_WARP: "up",
            B.MB_WEST_ARROW_WARP: "left", B.MB_EAST_ARROW_WARP: "right",
        }
        self.stair_warps = {
            B.MB_UP_LEFT_STAIR_WARP: "left", B.MB_DOWN_LEFT_STAIR_WARP: "left",
            B.MB_UP_RIGHT_STAIR_WARP: "right", B.MB_DOWN_RIGHT_STAIR_WARP: "right",
        }
        self.spin = {
            B.MB_SPIN_RIGHT: "right", B.MB_SPIN_LEFT: "left", B.MB_SPIN_UP: "up", B.MB_SPIN_DOWN: "down",
        }
        self.jump = {
            "down": B.MB_JUMP_SOUTH, "up": B.MB_JUMP_NORTH, "left": B.MB_JUMP_WEST, "right": B.MB_JUMP_EAST,
        }
        self.blocked_dirs = {
            "down": {B.MB_IMPASSABLE_SOUTH, B.MB_IMPASSABLE_SOUTHEAST, B.MB_IMPASSABLE_SOUTHWEST},
            "up": {B.MB_IMPASSABLE_NORTH, B.MB_IMPASSABLE_NORTHEAST, B.MB_IMPASSABLE_NORTHWEST},
            "left": {B.MB_IMPASSABLE_WEST, B.MB_IMPASSABLE_NORTHWEST, B.MB_IMPASSABLE_SOUTHWEST},
            "right": {B.MB_IMPASSABLE_EAST, B.MB_IMPASSABLE_NORTHEAST, B.MB_IMPASSABLE_SOUTHEAST},
        }

    def invalidate(self):
        self._views = {}

    def view(self, map_id):
        v = self._views.get(map_id)
        if v is None:
            v = MapView(self, self.d.maps[map_id])
            self._views[map_id] = v
        return v

    #  ------------------------------------------------------------ helpers
    def is_water(self, beh):
        return beh in self.water_behaviors

    def neighbor_tile(self, m, x, y, d):
        dx, dy = DIRS[d]
        nx, ny = x + dx, y + dy
        if m.in_bounds(nx, ny):
            return m, nx, ny
        t = self.d.connection_target(m, nx, ny)
        return t

    def warp_info(self, m, wx, wy):
        """How to take the warp at (wx,wy) on map m: (stand_xy, push, kind)."""
        v = self.view(m.id)
        beh = v.tile(wx, wy)[3]
        B = self.B
        if beh == B.MB_WARP_DOOR:
            return (wx, wy + 1), "up", "door"
        if beh in self.arrow_warps:
            return (wx, wy), self.arrow_warps[beh], "arrow"
        if beh in self.stair_warps:
            return (wx, wy), self.stair_warps[beh], "stairs"
        if beh in self.step_on_warps:
            return (wx, wy), None, "step"
        return None

    def warp_arrival(self, m, wx, wy, moving_dir):
        _, w = m.warp_at(wx, wy)
        if w is None or w["dest_map"] not in self.d.maps:
            return None
        dest = self.d.maps[w["dest_map"]]
        try:
            dw = dest.warps[int(w["dest_warp_id"])]
        except (ValueError, IndexError):
            return None
        dx, dy = dw["x"], dw["y"]
        beh = self.view(dest.id).tile(dx, dy)[3]
        B = self.B
        if beh == B.MB_WARP_DOOR:
            return dest.id, dx, dy + 1, "down"
        #  overworld.c GetAdjustedInitialDirection() + field_fadetransition.c
        #  Task_ExitNonAnimDoor(): on a cave door the player faces south and
        #  walks one step.
        if beh == B.MB_CAVE_DOOR:
            return dest.id, dx, dy + 1, "down"
        if beh == B.MB_SOUTH_ARROW_WARP:
            return dest.id, dx, dy, "up"
        if beh == B.MB_NORTH_ARROW_WARP:
            return dest.id, dx, dy, "down"
        if beh == B.MB_WEST_ARROW_WARP:
            return dest.id, dx, dy, "right"
        if beh == B.MB_EAST_ARROW_WARP:
            return dest.id, dx, dy, "left"
        if beh in (B.MB_UP_RIGHT_STAIR_WARP, B.MB_DOWN_RIGHT_STAIR_WARP):
            return dest.id, dx, dy, "left"
        if beh in (B.MB_UP_LEFT_STAIR_WARP, B.MB_DOWN_LEFT_STAIR_WARP):
            return dest.id, dx, dy, "right"
        return dest.id, dx, dy, moving_dir

    #  ------------------------------------------------------------ moves
    def edges(self, node, goals, allow_trainers=()):
        """
        node = (map_id, x, y, elev, surfing)
        yields (next_node, edge) where edge is a dict:
          kind: walk | jump | spin | warp | stepwarp | surf_start | dismount | cut
          dir, cost, risk flags ...
        """
        map_id, x, y, elev, surfing = node
        m = self.d.maps[map_id]
        v = self.view(map_id)
        cur = v.tile(x, y)
        cur_beh = cur[3]
        cur_elev_tile = cur[2]

        #  Warps you take standing on/next to them by pressing a direction.
        if not surfing:
            #  Mats and stairs: standing on the warp tile.
            wi, w = m.warp_at(x, y)
            if wi is not None:
                info = self.warp_info(m, x, y)
                if info and info[2] in ("arrow", "stairs"):
                    arr = self.warp_arrival(m, x, y, info[1])
                    if arr:
                        dm, ax, ay, af = arr
                        if self.can_stand(dm, ax, ay, True):
                            yield self.node_at(dm, ax, ay), {
                                "kind": "warp", "dir": info[1], "cost": COST_WARP,
                                "warp": (m.id, x, y), "arrive_facing": af, "warp_kind": info[2],
                            }
            #  Doors: standing below the door, pressing up.
            up = self.neighbor_tile(m, x, y, "up")
            if up and up[0].id == m.id:
                _, ux, uy = up
                wi, w = m.warp_at(ux, uy)
                if wi is not None and v.tile(ux, uy)[3] == self.B.MB_WARP_DOOR:
                    arr = self.warp_arrival(m, ux, uy, "up")
                    if arr:
                        dm, ax, ay, af = arr
                        if self.can_stand(dm, ax, ay, True):
                            yield self.node_at(dm, ax, ay), {
                                "kind": "warp", "dir": "up", "cost": COST_WARP,
                                "warp": (m.id, ux, uy), "arrive_facing": af, "warp_kind": "door",
                            }

        for d in DIR_LIST:
            if cur_beh in self.blocked_dirs[d]:
                continue
            t = self.neighbor_tile(m, x, y, d)
            if t is None:
                continue
            tm, nx, ny = t
            tv = self.view(tm.id)
            _, col, telev, tbeh, enc = tv.tile(nx, ny)
            goal = (tm.id, nx, ny) in goals

            #  Cut trees.
            if (nx, ny) in tv.cut_trees and not surfing:
                if self.state.abilities.get("cut"):
                    nn = (tm.id, nx, ny, elev, False)
                    yield nn, {"kind": "cut", "dir": d, "cost": COST_CUT + COST_STEP,
                               "risk": self.tile_risk(tv, nx, ny, False)}
                continue

            #  Rock Smash rocks (EventScript_RockSmash; a wild battle may
            #  follow, RockSmashWildEncounter).
            if (nx, ny) in tv.smash_rocks and not surfing:
                if self.state.abilities.get("rock_smash"):
                    nn = (tm.id, nx, ny, elev, False)
                    yield nn, {"kind": "smash", "dir": d, "cost": COST_CUT + COST_STEP + 6.0,
                               "risk": self.tile_risk(tv, nx, ny, False)}
                continue

            #  Card key doors: interact with the closed door, then walk in.
            if (nx, ny) in tv.doors and not surfing and "FLAG_HIDE_SILPH_CO_5F_CARD_KEY" in self.state.flags:
                door = tv.doors[(nx, ny)]
                ocol, obeh = door["open"].get((nx, ny), (0, None))
                if not ocol:
                    nn = (tm.id, nx, ny, elev if telev in (0, 15) else telev, False)
                    yield nn, {"kind": "door", "dir": d, "cost": 15.0 + COST_STEP, "flag": door["flag"],
                               "risk": set()}
                    continue

            #  Item balls in the way: pick them up. Not from the water: no
            #  landing on one either (Cerulean Cave 1F: the FULL RESTORE at
            #  (11, 16) was taken for free shore, and the dismount stopped).
            if (nx, ny) in tv.items and surfing:
                continue
            if (nx, ny) in tv.items:
                nn = (tm.id, nx, ny, elev if telev in (0, 15) else telev, False)
                yield nn, {"kind": "item", "dir": d, "cost": COST_ITEM + COST_STEP,
                           "flag": tv.items[(nx, ny)], "risk": self.tile_risk(tv, nx, ny, False)}
                continue

            #  Ledges.
            if tbeh == self.jump[d] and not surfing:
                t2 = self.neighbor_tile(tm, nx, ny, d)
                if t2 is None:
                    continue
                lm, lx, ly = t2
                lv = self.view(lm.id)
                if lv.tile(lx, ly)[1] or (lx, ly) in lv.blocked:
                    continue
                le = lv.tile(lx, ly)[2]
                new_elev = elev if le in (0, 15) else le
                yield (lm.id, lx, ly, new_elev, False), {
                    "kind": "jump", "dir": d, "cost": 2 * COST_STEP + COST_LEDGE + lv.soft.get((lx, ly), 0),
                    "risk": self.tile_risk(lv, lx, ly, False),
                }
                continue
            if tbeh in self.jump.values():
                continue

            #  Waterfalls (HM07, FLAG_BADGE07_GET): surfing north at the
            #  foot, A (EventScript_Waterfall), FLDEFF_USE_WATERFALL takes
            #  the player up to the water above. Onto one any other way the
            #  current pushes back south.
            if tbeh == self.B.by_name.get("MB_WATERFALL", -1):
                if surfing and d == "up" and self.state.abilities.get("waterfall"):
                    cm, cx, cy, tiles = tm, nx, ny, 1
                    while True:
                        t2 = self.neighbor_tile(cm, cx, cy, "up")
                        if t2 is None:
                            break
                        cm, cx, cy = t2
                        if self.view(cm.id).tile(cx, cy)[3] != self.B.by_name.get("MB_WATERFALL", -1):
                            break
                        tiles += 1
                    if self.can_stand(cm.id, cx, cy) or self.is_water(self.view(cm.id).tile(cx, cy)[3]):
                        yield (cm.id, cx, cy, 1, True), {"kind": "waterfall", "dir": "up", "tiles": tiles,
                                                         "cost": COST_SURF_START + 2 * COST_STEP * tiles,
                                                         "risk": set()}
                continue

            if tbeh in self.blocked_dirs[OPPOSITE[d]]:
                continue
            if (nx, ny) in tv.blocked or (nx, ny) in tv.boulders:
                continue

            water = self.is_water(tbeh)
            if surfing:
                if water and not col:
                    if telev not in (0, 15, 1):
                        continue
                    yield (tm.id, nx, ny, 1, True), self.walk_edge(tv, nx, ny, d, True, goal)
                    continue
                #  Leaving the water: COLLISION_ELEVATION_MISMATCH + CanStopSurfing.
                if not col and telev == 3:
                    yield (tm.id, nx, ny, 3, False), {
                        "kind": "dismount", "dir": d, "cost": COST_STEP + 2,
                        "risk": self.tile_risk(tv, nx, ny, False),
                    }
                continue

            if col:
                continue
            #  Starting to surf: facing water from land, and only from
            #  elevation 3 (IsPlayerFacingSurfableFishableWater:
            #  PlayerGetElevation() == 3). Cerulean Cave 1F: from the raised
            #  floor (elevation 4) A in front of the water does nothing.
            if water and telev == 1 and elev != 1:
                if elev != 3:
                    continue
                if self.state.abilities.get("surf"):
                    yield (tm.id, nx, ny, 1, True), {"kind": "surf_start", "dir": d, "cost": COST_SURF_START,
                                                    "risk": self.tile_risk(tv, nx, ny, True)}
                continue
            if elev != 0 and telev not in (0, 15) and telev != elev:
                continue
            if not goal and (nx, ny) in tv.triggers:
                continue
            wi, w = tm.warp_at(nx, ny)
            if wi is not None and tbeh in self.step_on_warps:
                arr = self.warp_arrival(tm, nx, ny, d)
                if arr:
                    dm, ax, ay, af = arr
                    if self.can_stand(dm, ax, ay, True) or goal:
                        yield self.node_at(dm, ax, ay), {
                            "kind": "stepwarp", "dir": d, "cost": COST_WARP,
                            "warp": (tm.id, nx, ny), "arrive_facing": af,
                        }
                continue
            #  Ice (MB_ICE, Icefall Cave B1F): ForcedMovement_Slip on and on
            #  the way the player went, until a tile that is not ice, or
            #  one in the way (then on the ice, free to turn).
            if tbeh == self.B.by_name.get("MB_ICE", -1):
                sm, sx, sy, sdir, tiles = self.ice_slide(tm, nx, ny, d)
                se = self.view(sm).tile(sx, sy)[2]
                yield (sm, sx, sy, elev if se in (0, 15) else se, False), {
                    "kind": "spin", "dir": d, "slide_dir": sdir, "slide_tiles": tiles,
                    "cost": COST_STEP + COST_SPIN * tiles,
                    "risk": self.tile_risk(tv, nx, ny, False),
                }
                continue
            #  Thin ice (Icefall Cave 1F, IcefallCaveIcePerStepCallback):
            #  it cracks when walked on; walked on again it breaks, and the
            #  player falls through to the floor below at the same place
            #  (FourIsland_IcefallCave_1F_EventScript_FallDownHole).
            if tbeh == self.B.by_name.get("MB_THIN_ICE", -1):
                if (nx, ny) in self.state.ice_cracked.get(tm.id, set()):
                    below = ICE_FALL_BELOW.get(tm.id)
                    if below and self.can_stand(below, nx, ny, True):
                        yield self.node_at(below, nx, ny), {
                            "kind": "stepwarp", "dir": d, "cost": COST_WARP + 10.0,
                            "warp": (tm.id, nx, ny), "arrive_facing": d,
                        }
                    continue
                yield (tm.id, nx, ny, elev if telev in (0, 15) else telev, False), dict(
                    self.walk_edge(tv, nx, ny, d, False, goal), cost=COST_STEP * 3)
                continue

            #  Spinners.
            if tbeh in self.spin:
                slide = self.slide(tm, nx, ny, self.spin[tbeh])
                if slide is None:
                    continue
                sm, sx, sy, sdir, tiles = slide
                se = self.view(sm).tile(sx, sy)[2]
                yield (sm, sx, sy, elev if se in (0, 15) else se, False), {
                    "kind": "spin", "dir": d, "slide_dir": sdir, "slide_tiles": tiles,
                    "cost": COST_STEP + COST_SPIN * tiles,
                    "risk": self.tile_risk(tv, nx, ny, False),
                }
                continue
            if telev == 15 or cur_elev_tile == 15:
                new_elev = elev
            else:
                new_elev = telev
            edge = self.walk_edge(tv, nx, ny, d, False, goal)
            if not goal and tm.id == map_id:
                for key, predictable, td, dist in v.sight.get((x, y), []):
                    if not predictable and dist >= 2 and d == OPPOSITE[td]:
                        edge["cost"] += COST_TOWARD_TRAINER
                        break
            yield (tm.id, nx, ny, new_elev, False), edge

    def walk_edge(self, tv, x, y, d, surfing, goal):
        cost = COST_STEP + tv.soft.get((x, y), 0)
        risk = self.tile_risk(tv, x, y, surfing)
        if "encounter" in risk:
            cost += COST_WATER_ENCOUNTER if surfing else COST_LAND_ENCOUNTER
        if "trainer" in risk:
            cost += COST_TRAINER_SIGHT
        return {"kind": "surf" if surfing else "walk", "dir": d, "cost": cost, "risk": risk}

    def tile_risk(self, tv, x, y, surfing, repel=None):
        risk = set()
        enc = tv.tile(x, y)[4]
        if repel is None:
            #  (The last steps of it as without: the count may be off a bit.)
            repel = self.state.repel_left > REPEL_MARGIN
        if repel:
            pass
        elif surfing:
            if enc == 2 and tv.water_enc:
                risk.add("encounter")
        else:
            if enc == 1 and tv.land_enc:
                risk.add("encounter")
        if (x, y) in tv.sight:
            risk.add("trainer")
        return risk

    def ice_slide(self, m, x, y, d):
        """From the ice tile (x,y) entered going d: on until a tile that is
        not ice, or the next one is in the way. Returns (map, x, y, d, tiles)."""
        ice = self.B.by_name.get("MB_ICE", -1)
        tiles = 0
        while tiles < 100:
            v = self.view(m.id)
            if v.tile(x, y)[3] != ice:
                break
            t = self.neighbor_tile(m, x, y, d)
            if t is None:
                break
            tm, nx, ny = t
            tv = self.view(tm.id)
            _, col, _, tbeh, _ = tv.tile(nx, ny)
            if col or (nx, ny) in tv.blocked or (nx, ny) in tv.boulders or tbeh in self.blocked_dirs[OPPOSITE[d]]:
                break
            m, x, y = tm, nx, ny
            tiles += 1
        return m.id, x, y, d, tiles

    def slide(self, m, x, y, d):
        """Follow a spinner slide from (x,y) going d. Returns (map, x, y, last_dir, tiles)."""
        B = self.B
        tiles = 0
        seen = set()
        while True:
            key = (x, y, d)
            if key in seen or tiles > 200:
                return None
            seen.add(key)
            v = self.view(m.id)
            beh = v.tile(x, y)[3]
            if beh == B.MB_STOP_SPINNING:
                return m.id, x, y, d, tiles
            if beh in self.spin:
                d = self.spin[beh]
            t = self.neighbor_tile(m, x, y, d)
            if t is None:
                return m.id, x, y, d, tiles
            tm, nx, ny = t
            tv = self.view(tm.id)
            _, col, _, tbeh, _ = tv.tile(nx, ny)
            if col or (nx, ny) in tv.blocked or tbeh in self.blocked_dirs[OPPOSITE[d]]:
                return m.id, x, y, d, tiles
            m, x, y = tm, nx, ny
            tiles += 1

    def can_stand(self, map_id, x, y, warp_target=False):
        m = self.d.maps.get(map_id)
        if m is None or not m.in_bounds(x, y):
            return False
        v = self.view(map_id)
        if warp_target and m.warp_at(x, y)[0] is not None:
            #  A warp puts the player on its tile even when that tile has
            #  collision (e.g. the Elite Four room entrances).
            return (x, y) not in v.blocked
        return not v.tile(x, y)[1] and (x, y) not in v.blocked

    def node_at(self, map_id, x, y):
        v = self.view(map_id)
        e = v.tile(x, y)[2]
        if e in (0, 15):
            e = 3
        return (map_id, x, y, e, False)

    #  ------------------------------------------------------------ search
    def route(self, start, goals, start_facing=None, max_nodes=2_000_000, allowed_maps=None):
        """
        Dijkstra (with turn costs) from start node to any goal (map,x,y).
        goals may also contain (map,x,y,"surf") for goals that need surfing.
        Returns a list of (edge, node_after).
        """
        #  A goal test of its own (MapGoals: any tile of some maps), or a set.
        goal_set = goals if isinstance(goals, MapGoals) else set((g[0], g[1], g[2]) for g in goals)
        start_state = (start, start_facing)
        dist = {start_state: 0.0}
        prev = {}
        heap = [(0.0, 0, start_state)]
        counter = 0
        found = None
        expanded = 0
        while heap:
            cost, _, state = heapq.heappop(heap)
            if cost > dist.get(state, float("inf")):
                continue
            node, facing = state
            if (node[0], node[1], node[2]) in goal_set:
                found = state
                break
            expanded += 1
            if expanded > max_nodes:
                break
            for nxt, edge in self.edges(node, goal_set):
                if allowed_maps is not None and nxt[0] not in allowed_maps:
                    continue
                d = edge["dir"]
                turn = COST_TURN if (facing is not None and d != facing) else 0.0
                nf = edge.get("arrive_facing") or edge.get("slide_dir") or d
                ns = (nxt, nf)
                nc = cost + edge["cost"] + turn
                if nc < dist.get(ns, float("inf")):
                    dist[ns] = nc
                    prev[ns] = (state, edge)
                    counter += 1
                    heapq.heappush(heap, (nc, counter, ns))
        if found is None:
            return None
        out = []
        s = found
        while s in prev:
            p, edge = prev[s]
            out.append((edge, s[0]))
            s = p
        out.reverse()
        return out

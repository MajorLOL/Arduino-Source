"""
Story builder: turns the story spec into concrete steps for the C++ program.

A spec step is a dict with one of these keys:

  set        state change (flags, vars, defeated trainers, objects, tiles, abilities)
  pos        the player position after scripted movement {map,x,y,facing}
  goto       walk to {map, tiles:[[x,y],..]} (any of them), across warps
             options: trigger (end on an active trigger), surf_goal
  talk       walk next to {map, object | xy} and face it, then an action:
             mode = talk | yes | battle | heal | plain
  cutscene   [min_ms, max_ms]
  battle     a trainer battle that starts on its own (after a trigger/talk)
  rival1     the first rival battle (scripted, Oak's tutorial)
  heal       talk to the nurse (must be standing below the nurse)
  grind      {map, tiles, pc, level, every}
  save       {dex: bool}
  teach      {hm, party_slot, move}
  catch      {species, map, tiles}
  wait       ms
  puzzle     name
  press      {button, times, wait}
  only       {variants:[..], steps:[..]}   steps for some starters only

The builder keeps the world state and the player position across segments,
and plans trainer battles that the route cannot avoid.
"""

import copy

from frlg_data import DIRS, OPPOSITE
from engine import Engine, WorldState, fixed_facing, EngineError

#  Battle-check windows are in the C++; here we only classify runs.
RUN_WALK = "WALK"
RUN_CHECKED = "CHECKED"
RUN_CHECKED_TRAINER = "CHECKED_TRAINER"
RUN_JUMP = "JUMP"
RUN_SPIN = "SPIN"
RUN_SURF = "SURF"
RUN_SURF_CHECKED = "SURF_CHECKED"
RUN_DISMOUNT = "DISMOUNT"
#  The last moves up to a trainer that turns around, inside its line of
#  sight: if it sees the player there, it walks up and the battle happens
#  on the way (the rest of the leg and the talk are then skipped).
RUN_APPROACH = "APPROACH"
#  Back out of that line to where the approach started: as many moves as
#  the approach really made (none if the trainer came all the way).
RUN_BACK = "BACK"


class StoryError(Exception):
    pass


class Builder:
    def __init__(self, decomp, gamedata, variant):
        self.d = decomp
        self.gd = gamedata
        self.variant = variant
        self.state = WorldState(gamedata)
        self.engine = Engine(decomp, gamedata, self.state)
        self.pos = None          # (map_id, x, y, facing)
        self.surfing = False
        self.legs = []           # all legs (dicts)
        #  Strength stays on until the player leaves the map by a warp
        #  (ClearTempFieldEventData()): (map, number of legs then) when it
        #  was used.
        self.strength_used = None
        self.segments = []
        self.leg_counter = 0
        self.warnings = []
        #  map -> trainers that turn around and may have seen the player on
        #  the way (CHECKED_TRAINER), until we leave the map: such a trainer
        #  may stand elsewhere now, and may be beaten already.
        self.maybe_came = {}
        #  map -> {tile: trainer} where such a trainer would stand then.
        self.maybe_at = {}
        self.segment_id = None

    # ------------------------------------------------------------ helpers
    def set_state(self, change):
        change = dict(change)
        if "script_tiles" in change:
            tiles = {k: dict(v) for k, v in change.get("tiles", {}).items()}
            for map_id, label in change.pop("script_tiles"):
                if label.startswith("@"):
                    #  A label in data/scripts/*.inc.
                    m = self.d.maps[map_id]
                    tt = {"%d,%d" % xy: {"collision": c, "behavior": b}
                          for xy, (c, b) in self.gd.metatile_tiles(m, label[1:]).items()}
                    if not tt:
                        raise StoryError("No setmetatile in %s" % label)
                    tiles.setdefault(map_id, {}).update(tt)
                else:
                    tiles.setdefault(map_id, {}).update(self.script_tiles(map_id, label))
            change["tiles"] = tiles
        self.state.apply(change)
        self.engine.invalidate()

    _metatile_ids = None

    def script_tiles(self, map_id, label):
        """Tile overrides from the setmetatile commands of a script label:
        {"x,y": {"collision": 0/1, "behavior": b}}."""
        import os
        import re
        if Builder._metatile_ids is None:
            ids = {}
            p = os.path.join(self.d.root, "include/constants/metatile_labels.h")
            for line in open(p):
                mm = re.match(r"#define\s+(METATILE_\w+)\s+(0x[0-9A-Fa-f]+|\d+)", line)
                if mm:
                    ids[mm.group(1)] = int(mm.group(2), 0)
            Builder._metatile_ids = ids
        m = self.d.maps[map_id]
        p = os.path.join(self.d.root, "data/maps", m.folder, "scripts.inc")
        out = {}
        inside = False
        for line in open(p):
            t = line.strip()
            if t == label + "::":
                inside = True
                continue
            if not inside:
                continue
            if t.endswith("::"):
                break
            mm = re.match(r"setmetatile\s+(\d+),\s*(\d+),\s*(\w+),\s*(\w+)", t)
            if mm:
                x, y, name, imp = int(mm.group(1)), int(mm.group(2)), mm.group(3), mm.group(4)
                mid = Builder._metatile_ids[name]
                beh = self.d.metatile_attributes(m.layout, mid) & 0x1FF
                col = 1 if imp in ("1", "TRUE") else 0
                out["%d,%d" % (x, y)] = {"collision": col, "behavior": beh}
            if t in ("return", "end"):
                break
        if not out:
            raise StoryError("No setmetatile in %s" % label)
        return out

    def node(self):
        mid, x, y, _ = self.pos
        v = self.engine.view(mid)
        e = v.tile(x, y)[2]
        if self.surfing:
            e = 1
        elif e in (0, 15):
            e = 3
        return (mid, x, y, e, self.surfing)

    def new_leg(self, label):
        self.leg_counter += 1
        mid, x, y, f = self.pos
        return {
            "id": len(self.legs), "name": "%s" % label,
            "start": [mid, x, y, f], "runs": [], "push": None, "face": None,
            "end": "STOP", "dark": mid in self.gd.dark_maps, "dest_dark": False, "preview": None,
            "start_facing": f, "points": [(mid, x, y)], "warp_to": None,
            "encounter_steps": 0, "trainer_steps": 0,
        }

    def add_run(self, leg, d, kind, count=1):
        r = leg["runs"]
        if r and r[-1][0] == d and r[-1][2] == kind and kind not in (RUN_SPIN, RUN_DISMOUNT, RUN_JUMP):
            r[-1][1] += count
        else:
            r.append([d, count, kind])
        if kind in (RUN_CHECKED, RUN_SURF_CHECKED):
            leg["encounter_steps"] += count
        if kind in (RUN_CHECKED_TRAINER, RUN_APPROACH):
            leg["trainer_steps"] += count

    def can_run(self, leg):
        """The leg may be run (B held, twice as fast): the Running Shoes are
        in the bag, every map of it allows running, none is dark (the video
        cannot follow the steps there) and no tile on the way stops running
        (rock stairs slow it down, MB_RUNNING_DISALLOWED)."""
        if "FLAG_SYS_B_DASH" not in self.state.flags:
            return False
        for mid, x, y in leg["points"]:
            m = self.d.maps[mid]
            if not m.allow_running or mid in self.gd.dark_maps:
                return False
            if not m.in_bounds(x, y):
                continue
            name = self.d.behaviors.name(self.engine.view(mid).tile(x, y)[3])
            if name in ("MB_ROCK_STAIRS", "MB_RUNNING_DISALLOWED"):
                return False
        return True

    def close_leg(self, leg, out):
        leg["run"] = self.can_run(leg)
        if leg["runs"] or leg["push"] or leg["face"] or leg["end"] != "STOP":
            self.legs.append(leg)
            out.append({"type": "WALK", "label": leg["name"], "leg": leg["id"]})

    # ------------------------------------------------------------ routing
    def predicted_battle_index(self, route):
        """Index of the first route edge that walks into the sight of a
        fixed-facing, undefeated trainer (the battle will start there)."""
        for i, (edge, node) in enumerate(route):
            if edge["kind"] not in ("walk", "jump", "dismount", "cut", "spin", "item", "door"):
                continue
            mid, x, y = node[0], node[1], node[2]
            v = self.engine.view(mid)
            for key, predictable, d, dist in v.sight.get((x, y), []):
                if predictable:
                    return i, mid, key, d, dist
        return None

    def walk_up_conflict(self, route):
        """
        A tile this route walks on after it was in the line of sight (2+
        tiles away) of a trainer that turns around, where that trainer would
        stand if it saw the player there: it would be in the way then.
        (The last tile is left out: next to a trainer to talk to, see
        mark_approach.)
        """
        pending = {}
        for i, (edge, node) in enumerate(route):
            mid, x, y = node[0], node[1], node[2]
            if i + 1 < len(route) and (x, y) in pending.get(mid, ()):
                return (mid, x, y)
            if "trainer" in edge.get("risk", ()):
                v = self.engine.view(mid)
                for key, predictable, td, dist in v.sight.get((x, y), []):
                    if not predictable and dist > 1:
                        tdx, tdy = DIRS[td]
                        pending.setdefault(mid, set()).add((x - tdx, y - tdy))
        return None

    def avoid_walk_up_tiles(self, start, goals, route):
        """Route again around such tiles (see walk_up_conflict), if there is
        another way."""
        added = []
        try:
            for _ in range(6):
                c = self.walk_up_conflict(route)
                if c is None:
                    break
                blocked = self.state.extra_blocked.setdefault(c[0], set())
                if (c[1], c[2]) in blocked:
                    break
                blocked.add((c[1], c[2]))
                added.append(c)
                self.engine.invalidate()
                again = self.engine.route(start, goals, start_facing=self.pos[3])
                if again is None:
                    break
                route = again
        finally:
            for mid, x, y in added:
                self.state.extra_blocked[mid].discard((x, y))
            if added:
                self.engine.invalidate()
        c = self.walk_up_conflict(route)
        if c is not None:
            self.warnings.append("%s: a trainer that turns around may stand in the way at %s (%d,%d)" % (
                self.segment_id, c[0], c[1], c[2]))
        return route

    def walk_to(self, goals, label, out, end_mode="STOP", face=None, allow_trigger=False, face_map=None):
        """Route to any of goals [(map,x,y)], emitting legs and actions."""
        guard = 0
        while True:
            guard += 1
            if guard > 30:
                raise StoryError("Too many planned battles on the way to %s" % label)
            start = self.node()
            route = self.engine.route(start, goals, start_facing=self.pos[3])
            if route is None:
                raise StoryError("No route for '%s' from %s to %s" % (label, self.pos, sorted(goals)[:4]))
            route = self.avoid_walk_up_tiles(start, goals, route)
            pb = self.predicted_battle_index(route)
            if pb is not None:
                i, mid, key, tdir, dist = pb
                part = route[: i + 1]
                self.emit_route(part, label + "_ToTrainer", out, "STOP")
                m = self.d.maps[mid]
                out.append({"type": "BATTLE", "label": "Trainer %s on %s" % (key, m.name), "p0": 15000})
                #  The trainer walks up to the player and stays next to them.
                #  The player does not turn to it in FRLG (trainer_see.c
                #  TrainerSeeFunc_PrepareToEngage(): commented out).
                self.state.defeated.add((mid, key))
                px, py = self.pos[1], self.pos[2]
                dx, dy = DIRS[tdir]
                self.state.extra_blocked.setdefault(mid, set()).add((px - dx, py - dy))
                self.engine.invalidate()
                if (self.pos[0], self.pos[1], self.pos[2]) in goals:
                    if face_map:
                        leg = self.new_leg(label + "_Face")
                        leg["face"] = face_map[(self.pos[0], self.pos[1], self.pos[2])]
                        leg["end"] = "INTERACT"
                        self.close_leg(leg, out)
                        self.pos = (self.pos[0], self.pos[1], self.pos[2], leg["face"])
                    return
                continue
            if face_map:
                end = route[-1][1] if route else self.node()
                face = face_map[(end[0], end[1], end[2])]
            self.emit_route(route, label, out, end_mode, face)
            return

    def emit_route(self, route, label, out, end_mode, face=None):
        leg = self.new_leg(label)
        part = 0
        for edge, node in route:
            k = edge["kind"]
            d = edge["dir"]
            risk = edge.get("risk", set())
            prev_map = self.pos[0]
            if node[0] != prev_map and k not in ("warp", "stepwarp"):
                #  Crossed a map connection: whatever stood around on the
                #  old map is back at its normal place next time.
                self.leave_map(prev_map)
            at = self.maybe_at.get(node[0], {}).get((node[1], node[2]))
            if at is not None and k not in ("warp",):
                self.warnings.append("%s: %s walks where trainer %s may stand (%d,%d)" % (
                    self.segment_id, label, at, node[1], node[2]))
            if k in ("walk", "surf"):
                if k == "surf":
                    kind = RUN_SURF_CHECKED if ("encounter" in risk or "trainer" in risk) else RUN_SURF
                else:
                    if "trainer" in risk:
                        kind = RUN_CHECKED_TRAINER
                        v = self.engine.view(node[0])
                        for key, predictable, td, dist in v.sight.get((node[1], node[2]), []):
                            if predictable:
                                continue
                            self.maybe_came.setdefault(node[0], set()).add(key)
                            if dist > 1:
                                tdx, tdy = DIRS[td]
                                self.maybe_at.setdefault(node[0], {})[(node[1] - tdx, node[2] - tdy)] = key
                    elif "encounter" in risk:
                        kind = RUN_CHECKED
                    else:
                        kind = RUN_WALK
                self.add_run(leg, d, kind)
                self.pos = (node[0], node[1], node[2], d)
                leg["points"].append((node[0], node[1], node[2]))
            elif k == "jump":
                self.add_run(leg, d, RUN_JUMP)
                self.pos = (node[0], node[1], node[2], d)
                leg["points"].append((node[0], node[1], node[2]))
            elif k == "spin":
                self.add_run(leg, d, RUN_CHECKED if "encounter" in risk else RUN_WALK)
                self.add_run(leg, edge["slide_dir"], RUN_SPIN, edge["slide_tiles"])
                self.pos = (node[0], node[1], node[2], edge["slide_dir"])
                leg["points"].append((node[0], node[1], node[2]))
            elif k == "dismount":
                self.add_run(leg, d, RUN_DISMOUNT)
                self.surfing = False
                self.pos = (node[0], node[1], node[2], d)
                leg["points"].append((node[0], node[1], node[2]))
            elif k == "cut":
                self.state.cut_done.setdefault(node[0], set()).add((node[1], node[2]))
                self.engine.invalidate()
                leg["face"] = d
                leg["end"] = "INTERACT"
                self.close_leg(leg, out)
                out.append({"type": "TALK_YES", "label": "Cut tree"})
                out.append({"type": "WAIT", "label": "Cut animation", "p0": 2500})
                part += 1
                leg = self.new_leg("%s_%d" % (label, part))
                self.add_run(leg, d, RUN_CHECKED if "encounter" in risk else RUN_WALK)
                self.pos = (node[0], node[1], node[2], d)
                leg["points"].append((node[0], node[1], node[2]))
            elif k == "door" and edge["flag"] in self.state.flags:
                #  Opened by the previous edge of this route (doors are 2 wide).
                self.add_run(leg, d, RUN_WALK)
                self.pos = (node[0], node[1], node[2], d)
                leg["points"].append((node[0], node[1], node[2]))
            elif k == "door":
                self.state.flags.add(edge["flag"])
                self.engine.invalidate()
                leg["face"] = d
                leg["end"] = "INTERACT"
                self.close_leg(leg, out)
                out.append({"type": "TALK", "label": "Card Key door (%s)" % edge["flag"]})
                part += 1
                leg = self.new_leg("%s_%d" % (label, part))
                self.add_run(leg, d, RUN_WALK)
                self.pos = (node[0], node[1], node[2], d)
                leg["points"].append((node[0], node[1], node[2]))
            elif k == "item":
                self.state.flags.add(edge["flag"])
                self.engine.invalidate()
                leg["face"] = d
                leg["end"] = "INTERACT"
                self.close_leg(leg, out)
                out.append({"type": "TALK", "label": "Pick up item (%s)" % edge["flag"]})
                part += 1
                leg = self.new_leg("%s_%d" % (label, part))
                self.add_run(leg, d, RUN_CHECKED if "encounter" in risk else RUN_WALK)
                self.pos = (node[0], node[1], node[2], d)
                leg["points"].append((node[0], node[1], node[2]))
            elif k == "surf_start":
                leg["face"] = d
                leg["end"] = "INTERACT"
                self.close_leg(leg, out)
                out.append({"type": "TALK_YES", "label": "Surf"})
                out.append({"type": "WAIT", "label": "Surf start", "p0": 2000})
                self.surfing = True
                self.pos = (node[0], node[1], node[2], d)
                part += 1
                leg = self.new_leg("%s_%d" % (label, part))
            elif k in ("warp", "stepwarp"):
                if k == "warp":
                    leg["push"] = d
                    leg["end"] = "WARP_PUSH"
                else:
                    self.add_run(leg, d, RUN_WALK)
                    leg["end"] = "WARP_STEP"
                leg["dest_dark"] = node[0] in self.gd.dark_maps
                leg["warp_to"] = node[0]
                leg["preview"] = self.d.warp_preview(edge["warp"][0], node[0])
                self.close_leg(leg, out)
                self.pos = (node[0], node[1], node[2], edge.get("arrive_facing") or d)
                #  Leaving a map resets the trainers that walked up to us.
                self.leave_map(edge["warp"][0])
                part += 1
                leg = self.new_leg("%s_%d" % (label, part))
            else:
                raise StoryError("Unknown edge kind %s" % k)
        if end_mode == "TRIGGER":
            leg["end"] = "TRIGGER"
        if face:
            leg["face"] = face
            leg["end"] = "INTERACT"
            self.pos = (self.pos[0], self.pos[1], self.pos[2], face)
        self.close_leg(leg, out)

    def mark_approach(self, out, m, target_xy, target_key):
        """
        The leg just emitted ends next to a trainer that turns around, and
        its last moves walk straight at the trainer inside its line of
        sight. If the trainer turns to the player on the way, it walks up
        and stands in the way of the rest of the leg. Mark those moves (and
        the move into the line) APPROACH, so the C++ ends the leg at such a
        battle. Returns (p, face, back) for emit_back(), or None.
        """
        if not out or out[-1]["type"] != "WALK":
            return None
        leg = self.legs[out[-1]["leg"]]
        face = leg.get("face")
        if leg["end"] != "INTERACT" or not face:
            return None
        g = leg["points"][-1]
        if g[0] != m.id:
            return None
        dx, dy = DIRS[face]
        if (g[1] + dx, g[2] + dy) != tuple(target_xy):
            return None
        moves = []
        for d, count, kind in leg["runs"]:
            moves.extend([d, kind] for _ in range(count))
        if len(moves) != len(leg["points"]) - 1:
            return None
        trailing = 0
        while trailing < len(moves) and moves[-1 - trailing][0] == face:
            trailing += 1
        v = self.engine.view(m.id)

        def seen_by(xy):
            return {key for key, predictable, d, dist in v.sight.get(xy, []) if not predictable}

        back = 0
        while back < trailing and seen_by((g[1] - dx * (back + 1), g[2] - dy * (back + 1))) == {target_key}:
            back += 1
        if back == 0:
            return None
        if seen_by((g[1], g[2])) - {target_key}:
            return None
        n = min(back + 1, len(moves))
        for i in range(len(moves) - n, len(moves)):
            if moves[i][1] not in (RUN_WALK, RUN_CHECKED_TRAINER):
                return None
            moves[i][1] = RUN_APPROACH
        runs = []
        for d, kind in moves:
            if runs and runs[-1][0] == d and runs[-1][2] == kind:
                runs[-1][1] += 1
            else:
                runs.append([d, 1, kind])
        leg["runs"] = runs
        p = (m.id, g[1] - dx * back, g[2] - dy * back)
        return p, face, back

    def emit_back(self, approach, label, out):
        """Back to where the approach entered the trainer's line (see
        mark_approach). The trainer may stand anywhere on that line now
        until the map reloads."""
        p, face, back = approach
        mid = p[0]
        dx, dy = DIRS[face]
        leg = self.new_leg(label + "_Back")
        leg["runs"].append([OPPOSITE[face], back, RUN_BACK])
        leg["points"].append(p)
        self.close_leg(leg, out)
        self.pos = (mid, p[1], p[2], OPPOSITE[face])
        blocked = self.state.extra_blocked.setdefault(mid, set())
        for k in range(1, back + 1):
            blocked.add((p[1] + dx * k, p[2] + dy * k))
        self.engine.invalidate()

    def leave_map(self, map_id):
        """Temporary changes of a map end when we leave it (map reload)."""
        self.state.extra_blocked.pop(map_id, None)
        self.state.cut_done.pop(map_id, None)
        self.maybe_came.pop(map_id, None)
        self.maybe_at.pop(map_id, None)
        self.engine.invalidate()

    # ------------------------------------------------------------ targets
    def object_xy(self, m, key):
        for o in self.state.objects(m):
            if o["key"] == key:
                return o["x"], o["y"]
        #  Try by index.
        idx, o = m.object_by_local_id(key)
        if o is not None:
            ov = self.state.object_overrides.get(m.id, {}).get(key, {})
            return ov.get("x", o["x"]), ov.get("y", o["y"])
        raise StoryError("Object %s not on %s" % (key, m.id))

    def sign_dirs(self, m, tx, ty):
        """The way the player must face to read the sign at (tx,ty), if it
        only works from one side (BG_EVENT_PLAYER_FACING_*), else None. The
        Mansion statues only work facing north: from below."""
        facing = {"BG_EVENT_PLAYER_FACING_NORTH": "up", "BG_EVENT_PLAYER_FACING_SOUTH": "down",
                  "BG_EVENT_PLAYER_FACING_EAST": "right", "BG_EVENT_PLAYER_FACING_WEST": "left"}
        for e in m.bg_events:
            if e.get("type") == "sign" and (e["x"], e["y"]) == (tx, ty):
                d = facing.get(e.get("player_facing_dir", ""))
                if d:
                    return [d]
        return None

    def interact_goals(self, m, tx, ty, only_dirs=None, counter=False):
        """Tiles next to (tx,ty) to stand on, and the facing for each.
        counter=True: also stand 2 tiles away across a counter (nurse, clerk)."""
        goals = {}
        for d, (dx, dy) in DIRS.items():
            if only_dirs and d not in only_dirs:
                continue
            ax, ay = tx - dx, ty - dy
            if m.in_bounds(ax, ay):
                goals[(m.id, ax, ay)] = d
            if counter:
                ax2, ay2 = tx - 2 * dx, ty - 2 * dy
                if m.in_bounds(ax2, ay2):
                    beh = self.d.behaviors.name(m.tile(ax, ay)[3])
                    if beh == "MB_COUNTER":
                        goals[(m.id, ax2, ay2)] = d
        return goals

    # ------------------------------------------------------------ steps
    def run_step(self, step, out):
        if "only" in step:
            if self.variant in step["only"]["variants"]:
                for s in step["only"]["steps"]:
                    self.run_step(s, out)
            return
        if "set" in step:
            self.set_state(step["set"])
            return
        if "pos" in step:
            p = step["pos"]
            self.pos = (p["map"], p["x"], p["y"], p.get("facing"))
            self.surfing = bool(p.get("surfing", False))
            return
        if "goto" in step:
            g = step["goto"]
            goals = {(g["map"], x, y) for x, y in g["tiles"]}
            self.walk_to(goals, step.get("label", "Goto"), out,
                         end_mode="TRIGGER" if g.get("trigger") else "STOP")
            if g.get("trigger"):
                #  Mark the last emitted leg as ending on a trigger.
                pass
            return
        if "talk" in step:
            t = step["talk"]
            m = self.d.map(t["map"])
            if "object" in t:
                tx, ty = self.object_xy(m, t["object"])
            else:
                tx, ty = t["xy"]
            dirs = t.get("dirs") or ("object" not in t and self.sign_dirs(m, tx, ty)) or None
            goals_face = self.interact_goals(m, tx, ty, dirs, t.get("counter", False))
            goals = set(goals_face.keys())
            label = step.get("label", "Talk")
            target_key = None
            for o in self.state.objects(m):
                if (o["x"], o["y"]) == (tx, ty):
                    target_key = o["key"]
            was_defeated = target_key is not None and self.state.is_defeated(m, target_key)
            mode = t.get("mode", "talk")
            turning = None
            if mode == "battle" and target_key is not None and not was_defeated:
                for o in self.state.objects(m):
                    if o["key"] == target_key and o["trainer"] and o["sight"] > 0 and fixed_facing(o["movement"]) is None:
                        turning = o
            if turning is not None and target_key in self.maybe_came.get(m.id, set()):
                self.warnings.append("%s: %s may have come to the player earlier on this map" % (self.segment_id, label))
            #  Route to the adjacent tile, then face the target.
            self.walk_to(goals, label, out, face_map=goals_face)
            approach = None
            if mode == "battle" and target_key is not None:
                if not was_defeated and self.state.is_defeated(m, target_key):
                    #  The trainer saw us on the way: that was the battle.
                    return
                if turning is not None:
                    approach = self.mark_approach(out, m, (tx, ty), target_key)
                self.state.defeated.add((m.id, target_key))
                self.engine.invalidate()
            kind = {"talk": "TALK", "yes": "TALK_YES", "no": "TALK_NO", "battle": "TALK", "heal": "HEAL",
                    "plain": None}[mode]
            if kind:
                out.append({"type": kind, "label": label})
            if mode == "battle":
                out.append({"type": "BATTLE", "label": label, "p0": 30000})
            if approach is not None:
                self.emit_back(approach, label, out)
            return
        if "cutscene" in step:
            c = step["cutscene"]
            out.append({"type": "CUTSCENE", "label": step.get("label", "Cutscene"), "p0": c[0], "p1": c[1],
                        "text": "YES" if step.get("answer") == "yes" else ""})
            return
        if "battle" in step:
            out.append({"type": "BATTLE", "label": step.get("label", "Battle"), "p0": step["battle"],
                        "p1": 2 if step.get("legendary") else (1 if step.get("flee") else 0)})
            return
        if "hall_of_fame" in step:
            #  The game saves itself in the Hall of Fame and starts in Pallet
            #  Town from then on (the next segment says where).
            out.append({"type": "RESET", "label": step.get("label", "Hall of Fame"), "p0": step["hall_of_fame"]})
            return
        if "rival1" in step:
            out.append({"type": "SCRIPTED_BATTLE", "label": step.get("label", "Rival")})
            return
        if "heal" in step:
            h = step["heal"]
            pc = self.d.map(h)
            nx, ny = self.nurse_xy(pc)
            goals_face = self.interact_goals(pc, nx, ny, ["up"], counter=True)
            goals = set(goals_face.keys())
            self.walk_to(goals, step.get("label", "To nurse"), out)
            out.append({"type": "HEAL", "label": "Heal at " + pc.name})
            self.pos = (self.pos[0], self.pos[1], self.pos[2], "up")
            return
        if "grind" in step:
            self.grind(step, out)
            return
        if "save" in step:
            out.append({"type": "SAVE", "label": "Save", "p0": 1 if step["save"].get("dex") else 0})
            return
        if "teach" in step:
            t = step["teach"]
            out.append({"type": "TEACH_HM", "label": "Teach %s" % t["move"], "p0": t["hm_rank"],
                        "p1": t["party_slot"], "p2": t.get("forget", -1), "text": t["move"]})
            return
        if "catch" in step:
            self.catch(step, out)
            return
        if "elevator" in step:
            e = step["elevator"]
            m = self.d.map(e["map"])
            gf = self.interact_goals(m, e["panel"][0], e["panel"][1])
            self.walk_to(set(gf.keys()), step.get("label", "Elevator") + "_Panel", out, face_map=gf)
            out.append({"type": "ELEVATOR", "label": step.get("label", "Elevator"),
                        "p0": e["from"], "p1": e["to"]})
            ex, ey = e["exit"]
            self.walk_to({(m.id, ex, ey)}, step.get("label", "Elevator") + "_Exit", out)
            leg = self.new_leg(step.get("label", "Elevator") + "_Out")
            leg["push"] = e["push"]
            leg["end"] = "WARP_PUSH"
            dm, dx, dy, df = e["dest"]
            leg["warp_to"] = dm
            leg["dest_dark"] = dm in self.gd.dark_maps
            leg["preview"] = self.d.warp_preview(m.id, dm)
            self.close_leg(leg, out)
            self.leave_map(m.id)
            self.pos = (dm, dx, dy, df)
            return
        if "probe" in step:
            self.probe(step, out)
            return
        if "trigger_var" in step:
            tv = step["trigger_var"]
            m = self.d.map(tv["map"])
            tiles = [(m.id, e["x"], e["y"]) for e in m.coord_events
                     if e.get("var") == tv["var"] and int(str(e.get("var_value", "0")), 0) == tv["value"]]
            if not tiles:
                raise StoryError("No trigger %s=%d on %s" % (tv["var"], tv["value"], m.id))
            self.walk_to(set(tiles), step.get("label", "Trigger"), out, end_mode="TRIGGER")
            return
        if "team" in step:
            out.append({"type": "TEAM", "label": step.get("label", "Team"), "p0": step["team"]})
            return
        if "use_item" in step:
            u = step["use_item"]
            out.append({"type": "USE_ITEM", "label": step.get("label", "Use item"), "p0": u["slot"], "text": u["words"]})
            return
        if "buy" in step:
            b = step["buy"]
            out.append({"type": "BUY", "label": step.get("label", "Buy"), "p0": b["index"], "p1": b["quantity"]})
            return
        if "heal_items" in step:
            out.append({"type": "HEAL_ITEMS", "label": step.get("label", "Heal with items")})
            return
        if "boulder" in step:
            self.boulder_puzzle(step, out)
            return
        if "switch_puzzle" in step:
            self.switch_puzzle(step, out)
            return
        if "oscillate" in step:
            #  Walk back and forth on two plain tiles until a script starts
            #  (e.g. the Safari Zone step counter runs out).
            o = step["oscillate"]
            m = self.d.map(o["map"])
            v = self.engine.view(m.id)
            spots = {}
            for x, y in o["tiles"]:
                t = v.tile(x, y)
                if t[1] or t[4] or (x, y) in v.blocked or (x, y) in v.sight or (x, y) in v.triggers:
                    continue
                near_warp = False
                for ddx, ddy in [(0, 0), (0, 1), (0, -1), (1, 0), (-1, 0), (0, 2), (0, -2), (2, 0), (-2, 0)]:
                    if m.warp_at(x + ddx, y + ddy)[0] is not None:
                        near_warp = True
                if near_warp:
                    continue
                for dname, (dx, dy) in DIRS.items():
                    nx, ny = x + dx, y + dy
                    if not m.in_bounds(nx, ny):
                        continue
                    tn = v.tile(nx, ny)
                    if tn[1] or tn[4] or (nx, ny) in v.blocked or (nx, ny) in v.sight or (nx, ny) in v.triggers:
                        continue
                    if tn[3] != t[3] or tn[2] != t[2]:
                        continue
                    spots[(m.id, x, y)] = dname
                    break
            if not spots:
                raise StoryError("No place to walk back and forth on %s" % m.id)
            self.walk_to(set(spots.keys()), step.get("label", "Oscillate") + "_ToSpot", out)
            here = (self.pos[0], self.pos[1], self.pos[2])
            out.append({"type": "OSCILLATE", "label": step.get("label", "Oscillate"), "text": spots[here],
                        "p0": o.get("max_ms", 600000)})
            return
        if "wait" in step:
            out.append({"type": "WAIT", "label": step.get("label", "Wait"), "p0": step["wait"]})
            return
        if "puzzle" in step:
            out.append({"type": "PUZZLE", "label": step["puzzle"], "text": step["puzzle"]})
            return
        if "press" in step:
            p = step["press"]
            out.append({"type": "PRESS", "label": step.get("label", "Press"), "text": p["button"],
                        "p0": p.get("times", 1), "p1": p.get("wait", 800)})
            return
        raise StoryError("Unknown step %s" % step)

    def nurse_xy(self, pc):
        for o in self.state.objects(pc):
            if o["graphics"].endswith("NURSE"):
                return o["x"], o["y"]
        raise StoryError("No nurse on " + pc.id)

    def grass_tiles(self, map_id, tiles):
        """The tiles of the list that are walkable grass with another such
        tile next to them (to walk back and forth on)."""
        v = self.engine.view(map_id)
        m = self.d.maps[map_id]
        #  Not next to a warp: a step the count gets wrong would take the
        #  player off the map (the Mansion's door: out and in again).
        warps = {(w["x"], w["y"]) for w in m.warps}
        ok = set()
        for x, y in tiles:
            if not m.in_bounds(x, y):
                continue
            t = v.tile(x, y)
            if t[1] or t[4] != 1 or (x, y) in v.blocked or (x, y) in v.sight or (x, y) in v.triggers:
                continue
            if (x, y) in warps or any((x + dx, y + dy) in warps for dx, dy in DIRS.values()):
                continue
            ok.add((x, y))
        out = set()
        for x, y in ok:
            for dx, dy in DIRS.values():
                if (x + dx, y + dy) in ok:
                    out.add((map_id, x, y))
                    break
        if not out:
            raise StoryError("No usable grass on %s" % map_id)
        return out

    #  The longest straight line of grass a training spot is walked along
    #  (end to end, the position checked on the video against the first
    #  tile, up to that many tiles away): 5 across, 4 up and down (the
    #  screen is 15 by 10 tiles).
    STRIP_MAX = {"left": 5, "right": 5, "up": 4, "down": 4}

    def grass_strips(self, map_id, tiles):
        """Straight lines of training grass (grass_tiles) to walk end to
        end: start tile -> (direction, length). Off the ways of people
        walking around and of trainers, without boulders, item balls or
        trees, all at one height and of one kind (no ledge). In the dark
        (nothing on the video to check the position with) two tiles only."""
        grass = self.grass_tiles(map_id, tiles)
        v = self.engine.view(map_id)
        dark = map_id in self.gd.dark_maps

        def free(x, y):
            return ((map_id, x, y) in grass and (x, y) not in v.soft and (x, y) not in v.blocked
                    and (x, y) not in v.boulders and (x, y) not in v.items and (x, y) not in v.cut_trees
                    and not v.tile(x, y)[1])

        strips = {}
        for (_, x, y) in sorted(grass):
            if not free(x, y):
                continue
            ref = v.tile(x, y)
            for dname in ("left", "right", "up", "down"):
                dx, dy = DIRS[dname]
                cap = 2 if dark else self.STRIP_MAX[dname]
                n = 1
                while n < cap:
                    tx, ty = x + dx * n, y + dy * n
                    t = v.tile(tx, ty) if self.d.maps[map_id].in_bounds(tx, ty) else None
                    if t is None or not free(tx, ty) or t[2] != ref[2] or t[3] != ref[3]:
                        break
                    n += 1
                if n >= 2 and n > strips.get((map_id, x, y), (None, 0))[1]:
                    strips[(map_id, x, y)] = (dname, n)
        return strips

    def strip_goal(self, map_id, tiles):
        """The tiles to walk to for training: the starts of the longest
        grass lines there (4 tiles or more count the same, the nearest is
        taken), and those lines. Without any: the plain grass tiles."""
        strips = self.grass_strips(map_id, tiles)
        if not strips:
            return self.grass_tiles(map_id, tiles), {}
        best = min(4, max(n for _, n in strips.values()))
        return {k for k, (_, n) in strips.items() if n >= best}, strips

    def osc_dir(self, grass, label):
        """Direction to the second grass tile to walk back and forth on."""
        grass_pos = self.pos
        for dname in ([grass_pos[3]] if grass_pos[3] else []) + ["left", "right", "up", "down"]:
            dx, dy = DIRS[dname]
            nx, ny = grass_pos[1] + dx, grass_pos[2] + dy
            if (grass_pos[0], nx, ny) in grass:
                v = self.engine.view(grass_pos[0])
                t = v.tile(nx, ny)
                if not t[1] and (nx, ny) not in v.blocked and t[4] == 1 and t[3] == v.tile(*grass_pos[1:3])[3]:
                    return dname
        raise StoryError("No second grass tile next to %s for '%s'" % (grass_pos, label))

    def grind(self, step, out):
        """
        Grind at a grass tile until the lead reaches a level, healing at the
        Pokemon Center every few battles. Emits three legs:
          nurse -> grass, grass -> nurse, nurse -> grass
        and a GRIND step that uses them. Ends at the nurse, healed.

        The GRIND step starts in front of its nurse. After a reset it starts
        again from a save made there (each heal while training saves), and a
        party that faints is brought back there. So from anywhere else, the
        walk to the nurse and a heal (which saves) come first, as steps of
        their own.
        """
        g = step["grind"]
        label = step.get("label", "Grind")
        grass = self.grass_tiles(g["map"], g["tiles"])
        pc = self.d.map(g["pc"])
        nx, ny = self.nurse_xy(pc)
        nurse_goals = set(self.interact_goals(pc, nx, ny, ["up"], counter=True).keys())

        if self.pos[:3] not in nurse_goals:
            self.walk_to(nurse_goals, label + "_FirstToNurse", out)
            out.append({"type": "HEAL", "label": "Heal at " + pc.name})
        self.pos = (self.pos[0], self.pos[1], self.pos[2], "up")

        sub = []
        goal, strips = self.strip_goal(g["map"], g["tiles"])
        self.walk_to(goal, label + "_ToGrass", sub)
        legs_to_grass = [s["leg"] for s in sub if s["type"] == "WALK"]
        if any(s["type"] != "WALK" for s in sub):
            raise StoryError("Grind route to grass must be plain walking: " + label)
        grass_pos = self.pos
        if tuple(grass_pos[:3]) in strips:
            osc, length = strips[tuple(grass_pos[:3])]
        else:
            osc, length = self.osc_dir(grass, label), 2

        sub2 = []
        self.walk_to(nurse_goals, label + "_ToNurse", sub2)
        legs_to_nurse = [s["leg"] for s in sub2 if s["type"] == "WALK"]
        if any(s["type"] != "WALK" for s in sub2):
            raise StoryError("Grind route to nurse must be plain walking: " + label)
        nurse_pos = (self.pos[0], self.pos[1], self.pos[2], "up")
        self.pos = nurse_pos

        #  Back onto the same tile: the one the training goes from, and the
        #  walk to the nurse starts from.
        sub3 = []
        self.walk_to({tuple(grass_pos[:3])}, label + "_BackToGrass", sub3)
        legs_back = [s["leg"] for s in sub3 if s["type"] == "WALK"]
        if any(s["type"] != "WALK" for s in sub3):
            raise StoryError("Grind route back to grass must be plain walking: " + label)

        out.append({
            "type": "GRIND", "label": label, "p0": g["level"], "p1": g.get("every", 6), "dir": osc,
            "p2": g.get("slot", -1), "text": g.get("line", ""),
            #  A team member below the level of the strongest wild Pokemon
            #  here only takes part (shared experience) until it gets there.
            "p3": self.gd.wild_max_level.get(g["map"], 0) if g.get("slot") else 0,
            #  The grass line walked along: this many tiles from the start;
            #  0x100: Rare Candies first.
            "p4": length | (0x100 if g.get("candies") else 0),
            "legs_to_grass": legs_to_grass, "legs_to_nurse": legs_to_nurse, "legs_back": legs_back,
        })
        #  The grind ends healed at the nurse.
        self.pos = nurse_pos

    def probe(self, step, out):
        """
        Puzzle helper: walk to a hub, then legs hub -> each target (ending
        facing it) and target -> hub. The C++ decides which targets to try.
        Emits PUZZLE with legs_to_grass = to-target legs, legs_to_nurse =
        back-to-hub legs (one leg each). Ends at the hub.
        """
        pr = step["probe"]
        label = step.get("label", pr["name"])
        m = self.d.map(pr["map"])
        hub = (m.id, pr["hub"][0], pr["hub"][1])
        self.walk_to({hub}, label + "_ToHub", out)
        hub_pos = self.pos
        to_t, back = [], []
        for i, (tx, ty) in enumerate(pr["targets"]):
            self.pos = hub_pos
            gf = self.interact_goals(m, tx, ty)
            sub = []
            self.walk_to(set(gf.keys()), "%s_To%d" % (label, i + 1), sub, face_map=gf)
            legs = [s_["leg"] for s_ in sub if s_["type"] == "WALK"]
            if len(legs) != 1 or len(sub) != 1:
                raise StoryError("Probe %s target %d needs exactly one plain leg" % (label, i + 1))
            to_t.append(legs[0])
            sub = []
            self.walk_to({hub}, "%s_Back%d" % (label, i + 1), sub)
            legs = [s_["leg"] for s_ in sub if s_["type"] == "WALK"]
            if len(legs) > 1 or len(sub) != len(legs):
                raise StoryError("Probe %s back %d needs one plain leg" % (label, i + 1))
            back.append(legs[0] if legs else -1)
        self.pos = hub_pos
        out.append({"type": "PUZZLE", "label": label, "text": pr["name"],
                    "legs_to_grass": to_t, "legs_to_nurse": back, "legs_back": []})

    def switch_puzzle(self, step, out):
        """
        Pokemon Mansion: find the shortest sequence of statue presses (each
        toggles FLAG_POKEMON_MANSION_SWITCH_STATE) after which the goal
        becomes reachable, then emit it. step["switch_puzzle"] = {goal: talk
        spec}. The goal step itself is run afterwards.
        """
        sp = step["switch_puzzle"]
        flag = "FLAG_POKEMON_MANSION_SWITCH_STATE"
        if "goal_tiles" in sp:
            goal_tiles = {(g[0], g[1], g[2]) for g in sp["goal_tiles"]}
        else:
            gt = sp["goal"]["talk"]
            gm = self.d.map(gt["map"])
            gx, gy = gt["xy"] if "xy" in gt else self.object_xy(gm, gt["object"])
            goal_tiles = set(self.interact_goals(gm, gx, gy).keys())
        statues = []
        for mid, info in self.gd.mansion.items():
            for (x, y) in info["statues"]:
                statues.append((mid, x, y))

        def reachable(node, facing, flag_on, goals):
            had = flag in self.state.flags
            if flag_on:
                self.state.flags.add(flag)
            else:
                self.state.flags.discard(flag)
            self.engine.invalidate()
            r = self.engine.route(node, goals, start_facing=facing, allowed_maps=set(self.gd.mansion.keys()))
            if had:
                self.state.flags.add(flag)
            else:
                self.state.flags.discard(flag)
            self.engine.invalidate()
            return r

        start = (self.node(), self.pos[3], flag in self.state.flags)
        #  BFS over (statue pressed last, flag) states.
        from collections import deque
        q = deque([(start, [])])
        seen = set()
        found = None
        while q:
            (node, facing, on), seq = q.popleft()
            if reachable(node, facing, on, goal_tiles) is not None:
                found = seq
                break
            if len(seq) >= 6:
                continue
            for st in statues:
                m = self.d.maps[st[0]]
                gf = self.interact_goals(m, st[1], st[2], self.sign_dirs(m, st[1], st[2]))
                r = reachable(node, facing, on, set(gf.keys()))
                if r is None:
                    continue
                end = r[-1][1] if r else node
                key = (st, not on)
                if key in seen:
                    continue
                seen.add(key)
                endf = gf[(end[0], end[1], end[2])]
                q.append(((end, endf, not on), seq + [st]))
        if found is None:
            raise StoryError("Mansion switch puzzle has no solution")
        for st in found:
            self.run_step({"talk": {"map": st[0], "xy": [st[1], st[2]], "mode": "yes"},
                           "label": "MansionSwitch_%s_%d_%d" % (self.d.maps[st[0]].name, st[1], st[2])}, out)
            out.append({"type": "CUTSCENE", "label": "Switch", "p0": 1000, "p1": 15000, "text": ""})
            if flag in self.state.flags:
                self.set_state({"clear_flags": [flag]})
            else:
                self.set_state({"flags": [flag]})

    # ------------------------------------------------------------ boulders
    def boulder_puzzle(self, step, out):
        """
        Strength puzzle on the current map: push a boulder onto a target
        tile (a floor switch, or a hole it falls through). BFS over boulder
        positions; the player walks with the normal movement rules.
        step["boulder"] = {target: [x,y], hole: bool}
        """
        from collections import deque
        bp = step["boulder"]
        label = step.get("label", "Boulder")
        mid = self.pos[0]
        m = self.d.maps[mid]
        tx, ty = bp["target"]
        hole = bool(bp.get("hole"))
        v0 = self.engine.view(mid)
        keys = {}
        for o in self.state.objects(m):
            if o["graphics"].endswith("PUSHABLE_BOULDER"):
                keys[(o["x"], o["y"])] = o["key"]
        start_cfg = frozenset(keys.keys())

        #  Walk graph of this map without boulders (computed once).
        v = self.engine.view(mid)
        saved = v.boulders
        v.boulders = set()
        graph = {}
        todo = deque([self.node()])
        graph_seen = {self.node()}
        while todo:
            n = todo.popleft()
            lst = []
            for nn, ed in self.engine.edges(n, set()):
                if nn[0] != mid or ed["kind"] not in ("walk", "jump"):
                    continue
                lst.append(nn)
                if nn not in graph_seen:
                    graph_seen.add(nn)
                    todo.append(nn)
            graph[n] = lst
        v.boulders = saved

        def region(cfg, start_node):
            seen = {start_node}
            q = deque([start_node])
            while q:
                n = q.popleft()
                for nn in graph.get(n, ()):
                    if (nn[1], nn[2]) in cfg or nn in seen:
                        continue
                    seen.add(nn)
                    q.append(nn)
            return seen

        def can_push_to(b, d, cfg):
            dx, dy = DIRS[d]
            nx, ny = b[0] + dx, b[1] + dy
            if not m.in_bounds(nx, ny):
                return None
            t = v0.tile(nx, ny)
            beh = self.d.behaviors.name(t[3])
            if (nx, ny) in cfg or (nx, ny) in v0.blocked:
                return None
            if beh == "MB_FALL_WARP":
                return (nx, ny)
            if t[1] or beh == "MB_CAVE_DOOR" or m.warp_at(nx, ny)[0] is not None:
                return None
            be = v0.tile(b[0], b[1])[2]
            if t[2] not in (0, 15, be) and be not in (0, 15):
                return None
            return (nx, ny)

        import heapq

        def h(cfg):
            return min(abs(b[0] - tx) + abs(b[1] - ty) for b in cfg) if cfg else 99

        def search(movable, limit):
            """A* over boulder positions; only the boulders that started at a
            position in 'movable' may be pushed."""
            counter = 0
            heap = [(0, 0, counter, start_cfg, self.node(), [], tuple(sorted((b, b) for b in start_cfg)))]
            seen = set()
            expanded = 0
            while heap:
                _, g, _, cfg, pnode, plan, origin_map = heapq.heappop(heap)
                reg = region(cfg, pnode)
                sig = (cfg, min((n[1], n[2], n[3]) for n in reg))
                if sig in seen:
                    continue
                seen.add(sig)
                expanded += 1
                if expanded > limit or len(plan) > 80:
                    continue
                reg_xy = {}
                for n in reg:
                    reg_xy.setdefault((n[1], n[2]), n)
                origin = dict(origin_map)
                for b in sorted(cfg):
                    if origin.get(b) not in movable:
                        continue
                    for d, (dx, dy) in DIRS.items():
                        pxy = (b[0] - dx, b[1] - dy)
                        if pxy not in reg_xy:
                            continue
                        nb = can_push_to(b, d, cfg)
                        if nb is None:
                            continue
                        ncfg = set(cfg)
                        ncfg.discard(b)
                        fell = self.d.behaviors.name(v0.tile(nb[0], nb[1])[3]) == "MB_FALL_WARP"
                        if not fell:
                            ncfg.add(nb)
                        nplan = plan + [(b, d, nb)]
                        if nb == (tx, ty):
                            return nplan
                        if fell:
                            continue
                        no = dict(origin)
                        no[nb] = no.pop(b)
                        fz = frozenset(ncfg)
                        hh = min(abs(x - tx) + abs(y - ty) for (x, y) in fz if no.get((x, y)) in movable)
                        counter += 1
                        heapq.heappush(heap, (g + 1 + hh, g + 1, counter, fz, reg_xy[pxy], nplan,
                                              tuple(sorted(no.items()))))
            return None

        found = None
        for b in sorted(start_cfg, key=lambda q: abs(q[0] - tx) + abs(q[1] - ty)):
            found = search({b}, 20000)
            if found:
                break
        if found is None:
            found = search(set(start_cfg), 60000)
        if found is None:
            raise StoryError("No boulder solution for %s on %s" % (label, mid))
        #  Emit: walk behind the boulder, (first time) use Strength, push.
        #  Still on from a puzzle before on this map (no warp since), a
        #  boulder only says "Strength made it possible...", without the
        #  YES/NO (EventScript_AlreadyUsedStrength).
        cur = dict(keys)        # position -> key
        strength_on = False
        if self.strength_used is not None and self.strength_used[0] == mid:
            strength_on = all(
                L["start"][0] == mid and L["end"] not in ("WARP_PUSH", "WARP_STEP")
                for L in self.legs[self.strength_used[1]:]
            )
        for i, (b, d, nb) in enumerate(found):
            key = cur.pop(b)
            dx, dy = DIRS[d]
            pxy = (b[0] - dx, b[1] - dy)
            gf = {(mid, pxy[0], pxy[1]): d}
            self.walk_to(set(gf.keys()), "%s_Push%d" % (label, i + 1), out, face_map=gf)
            if not strength_on:
                out.append({"type": "TALK_YES", "label": "Use Strength"})
                out.append({"type": "CUTSCENE", "label": "Strength", "p0": 1500, "p1": 20000, "text": ""})
                strength_on = True
                self.strength_used = (mid, len(self.legs))
            out.append({"type": "PUSH", "label": "Push boulder %s" % d, "text": d})
            fell = self.d.behaviors.name(v0.tile(nb[0], nb[1])[3]) == "MB_FALL_WARP"
            if fell:
                self.set_state({"objects": {mid: {key: {"hidden": True}}}})
            else:
                self.set_state({"objects": {mid: {key: {"x": nb[0], "y": nb[1]}}}})
                cur[nb] = key
            self.pos = (self.pos[0], self.pos[1], self.pos[2], d)

    def catch(self, step, out):
        c = step["catch"]
        label = step.get("label", "Catch")
        grass = self.grass_tiles(c["map"], c["tiles"])
        goal, strips = self.strip_goal(c["map"], c["tiles"])
        sub = []
        self.walk_to(goal, label + "_ToGrass", sub)
        out.extend(sub)
        if tuple(self.pos[:3]) in strips:
            osc, length = strips[tuple(self.pos[:3])]
        else:
            osc, length = self.osc_dir(grass, label), 2
        out.append({"type": "CATCH", "label": label, "text": ",".join(c["species"]), "p0": c.get("balls", 5),
                    "dir": osc, "p4": length})

    # ------------------------------------------------------------ segments
    def build(self, spec):
        if "initial_state" in spec:
            self.set_state(spec["initial_state"])
        for seg in spec["segments"]:
            if "variants" in seg and self.variant not in seg["variants"]:
                continue
            if "start" in seg:
                s = seg["start"]
                self.pos = (s["map"], s["x"], s["y"], s.get("facing"))
                self.surfing = False
            if "state" in seg:
                self.set_state(seg["state"])
            out = []
            start_pos = self.pos
            self.segment_id = seg["id"]
            import os as _os
            import time as _time
            _t0 = _time.time()
            try:
                for step in seg["steps"]:
                    self.run_step(step, out)
            except (StoryError, EngineError) as e:
                raise StoryError("[%s / %s] %s" % (self.variant, seg["id"], e))
            if _os.environ.get("STORY_TIMING"):
                import sys as _sys
                print("  %-28s %6.1fs" % (seg["id"], _time.time() - _t0), file=_sys.stderr)
            self.segments.append({
                "id": seg["id"], "title": seg.get("title", ""), "chapter": seg.get("chapter", 0),
                "saves": any(s["type"] == "SAVE" for s in out),
                "has_dex": seg.get("has_dex", True),
                "steps": out, "start": list(start_pos), "end": list(self.pos),
                "notes": seg.get("notes", ""),
            })
        return self.segments

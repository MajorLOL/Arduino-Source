"""
Story builder: turns the story spec into concrete steps for the C++ program.

A spec step is a dict with one of these keys:

  set        state change (flags, vars, defeated trainers, objects, tiles, abilities)
  pos        the player position after scripted movement {map,x,y,facing}
  goto       walk to {map, tiles:[[x,y],..]} (any of them), across warps
             options: trigger (end on an active trigger), surf_goal,
             bump (the last straight stretch, from the water on, ends where
             the way is shut: walked until the player stands still)
  talk       walk next to {map, object | xy} and face it, then an action:
             mode = talk | yes | battle | heal | plain
             options: given (mode yes: the question may not come, what it
             gives was given before), patience (seconds: the one talked to
             walks around, xy is a tile it comes by)
  cutscene   [min_ms, max_ms]; gift: a Pokemon is handed over in it
  battle     a trainer battle that starts on its own (after a trigger/talk)
  rival1     the first rival battle (scripted, Oak's tutorial)
  heal       talk to the nurse (must be standing below the nurse)
  grind      {map, tiles, pc, level, every}
  save       {dex: bool}
  teach      {hm, party_slot, move}
  catch      {species, map, tiles}
  dex        for the POKeDEX: {map, tiles, species, idle, minutes, collect};
             fish: a rod (0 OLD, 1 GOOD, 2 SUPER ROD) cast from the shore of
             the map (with safari: in a Safari Game, and exit: the map of
             the gate, where the program leaves it when the visit is over);
             laps + ground (water | rocks): steps that end where
             they start, played again and again, catching on the way
  smash      {map, xy}: ROCK SMASH on that rock (for what is under it)
  rod_wanted {rod, steps}: the way to a fishing rod, skipped when nothing
             is fished for with it or the bag has it
  unless_owned {species, count, steps}: steps for a Pokemon that is there
             once, skipped when the POKeDEX table has it
  walk_exact {map, tiles, face}: a walk over exactly these tiles, one step
             each (the first next to the player), then turned to "face" (build
             54: the stone of Birth Island, where the steps between two
             presses of A are counted)
  unless_item {words, pocket, steps}: the way to an item (build 54, the LAX
             INCENSE), skipped when the bag has it already; the steps end
             where they start
  got        species: the player was just given it (after cutscene gift)
  wait       ms
  puzzle     name
  press      {button, times, wait}
  only       {variants:[..], steps:[..]}   steps for some starters only

The builder keeps the world state and the player position across segments,
and plans trainer battles that the route cannot avoid.
"""

import copy
import json
import os
import re

from frlg_data import DIRS, OPPOSITE
from engine import MapGoals, Engine, WorldState, fixed_facing, EngineError, REPEL_MARGIN

#  Battle-check windows are in the C++; here we only classify runs.
RUN_WALK = "WALK"
RUN_CHECKED = "CHECKED"
RUN_CHECKED_TRAINER = "CHECKED_TRAINER"
RUN_JUMP = "JUMP"
RUN_SPIN = "SPIN"
RUN_SURF = "SURF"
RUN_SURF_CHECKED = "SURF_CHECKED"
RUN_DISMOUNT = "DISMOUNT"
#  From the water on until the way is shut (a rock, a wall, someone who
#  stands still), over the shore and the land behind it if they come first:
#  the tiles are not counted (see make_bump() and AutoStoryBump in the C++).
RUN_SURF_BUMP = "SURF_BUMP"
#  The last moves up to a trainer that turns around, inside its line of
#  sight: if it sees the player there, it walks up and the battle happens
#  on the way (the rest of the leg and the talk are then skipped).
RUN_APPROACH = "APPROACH"
#  Back out of that line to where the approach started: as many moves as
#  the approach really made (none if the trainer came all the way).
RUN_BACK = "BACK"


_FRAME_SCENES = None


def frame_scenes(decomp):
    """{map id: [(var, value, script)]}: frame scripts that move the player
    a step (not only turn it), outside temporary variables."""
    global _FRAME_SCENES
    if _FRAME_SCENES is not None:
        return _FRAME_SCENES
    import re
    out = {}
    root = os.path.join(decomp.root, "data", "maps")
    steps = re.compile(r"\b(walk|jump|slide|walk_slow|walk_fast|player_run)_(up|down|left|right)\b")
    for name in os.listdir(root):
        path = os.path.join(root, name, "scripts.inc")
        mj = os.path.join(root, name, "map.json")
        if not os.path.exists(path) or not os.path.exists(mj):
            continue
        text = open(path).read()
        labels = {}
        for m in re.finditer(r"^(\w+)::\n(.*?)(?=^\w+::|\Z)", text, re.S | re.M):
            labels[m.group(1)] = m.group(2)
        map_id = json.load(open(mj))["id"]
        for m in re.finditer(r"map_script_2 (\w+), (\d+), (\w+)", text):
            var, value, script = m.group(1), int(m.group(2)), m.group(3)
            if var.startswith("VAR_TEMP"):
                continue
            body = labels.get(script, "")
            moves = re.findall(r"applymovement (?:LOCALID_PLAYER|OBJ_EVENT_ID_PLAYER), (\w+)", body)
            if any(steps.search(labels.get(mv, "")) for mv in moves):
                out.setdefault(map_id, []).append((var, value, script))
    _FRAME_SCENES = out
    return out


class StoryError(Exception):
    pass


_TRADE_HELD = None


def trade_holds_mail(root, species):
    """True when the Pokemon an in-game trade of FireRed gives (src/data/ingame_trades.h)
    holds a MAIL: the PC's box view will not take it (and will not even close
    the party with it in the hand: "Please remove the MAIL.") until the MAIL
    is taken off it in the party menu. Jynx comes with a FAB MAIL."""
    global _TRADE_HELD
    if _TRADE_HELD is None:
        with open(os.path.join(root, "src/data/ingame_trades.h")) as f:
            text = f.read()
        held = {}
        for body in re.findall(r"\[INGAME_TRADE_\w+\]\s*=\s*\{(.*?)\n    \}", text, re.S):
            if "#if defined(FIRERED)" in body:
                #  The FireRed side: what is before the #if, and the first branch.
                before, rest = body.split("#if defined(FIRERED)", 1)
                first = rest.split("#elif defined(LEAFGREEN)")[0]
                after = rest.split("#endif", 1)[1] if "#endif" in rest else ""
                body = before + first + after
            sp = re.search(r"\.species\s*=\s*SPECIES_(\w+)", body)
            item = re.search(r"\.heldItem\s*=\s*(ITEM_\w+)", body)
            if sp and item:
                held[sp.group(1)] = item.group(1)
        _TRADE_HELD = held
    #  (The game's names have no dash: FARFETCHD, MR_MIME.)
    key = species.upper().replace("-", "")
    item = None
    for name, held_item in _TRADE_HELD.items():
        if name.replace("_", "") == key:
            item = held_item
    if item is None:
        raise StoryError("trade: %s is not given by any in-game trade of FireRed" % species)
    return item.endswith("_MAIL")


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
        #  After the Hall of Fame (REPEL from then on).
        self.postgame = False
        #  (items, repel steps) as the last save has them: a reload goes back to it.
        self.saved_consumables = None
        #  Where the shortcuts were taken (for the report).
        self.shortcut_log = []
        self.segment_no_repel = False
        #  No REPEL was ever on (see REPEL_AFTER).
        self.state.repel_left = -Builder.REPEL_AFTER

    # ------------------------------------------------------------ helpers
    def layout_tiles(self, map_id, layout_id):
        """Tile overrides for a map whose layout a script switches
        (setmaplayoutindex): every tile that differs from the map's own
        layout, {"x,y": {collision, elevation, behavior, encounter}}."""
        m = self.d.maps[map_id]
        old, new = m.layout, self.d.layouts[layout_id]
        if (old.width, old.height) != (new.width, new.height):
            raise StoryError("Layout %s does not fit %s" % (layout_id, map_id))
        out = {}
        for y in range(new.height):
            for x in range(new.width):
                b = new.block(x, y)
                if b == old.block(x, y):
                    continue
                attrs = self.d.metatile_attributes(new, b & 0x3FF)
                out["%d,%d" % (x, y)] = {"collision": (b >> 10) & 3, "elevation": (b >> 12) & 15,
                                         "behavior": attrs & 0x1FF, "encounter": (attrs >> 24) & 7}
        return out

    def set_state(self, change):
        change = dict(change)
        if "layouts" in change:
            tiles = {k: dict(v) for k, v in change.get("tiles", {}).items()}
            for map_id, layout_id in change.pop("layouts").items():
                tiles.setdefault(map_id, {}).update(self.layout_tiles(map_id, layout_id))
            change["tiles"] = tiles
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
        self.note_visited(leg)
        #  Thin ice walked on cracks (it breaks under the next step there).
        thin = self.d.behaviors.by_name.get("MB_THIN_ICE", -1)
        for mid, x, y in leg["points"]:
            m = self.d.maps[mid]
            if m.in_bounds(x, y) and self.engine.view(mid).tile(x, y)[3] == thin:
                self.state.ice_cracked.setdefault(mid, set()).add((x, y))
                self.engine.invalidate()
        leg["run"] = self.can_run(leg)
        if leg["runs"] or leg["push"] or leg["face"] or leg["end"] != "STOP":
            self.legs.append(leg)
            out.append({"type": "WALK", "label": leg["name"], "leg": leg["id"]})

    # ------------------------------------------------------------ routing
    def predicted_battle_index(self, route):
        """Index of the first route edge that walks into the sight of a
        fixed-facing, undefeated trainer (the battle will start there)."""
        for i, (edge, node) in enumerate(route):
            if edge["kind"] not in ("walk", "jump", "dismount", "cut", "smash", "spin", "item", "door"):
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

    def check_scenes_on_the_way(self, start, route, label):
        """A map entered on the way whose frame script moves the player
        (map_script_2 with the state's variable at its value): the rival on
        Four Island and in Six Island's Pokemon Center made the player step
        aside, and the route went on a tile off. Such a scene is modelled by
        a walk that ends where it starts, then cutscene() and pos()."""
        scenes = frame_scenes(self.d)
        prev = start[0]
        for i, item in enumerate(route):
            node = item[1]
            if node[0] != prev:
                for var, value, script in scenes.get(node[0], []):
                    have = self.state.vars.get(var, 0)
                    if have == value and i + 1 < len(route):
                        self.warnings.append("%s: %s: entering %s starts %s (%s == %d), which moves the player" % (
                            self.segment_id, label, node[0], script, var, value))
                prev = node[0]

    def walk_to(self, goals, label, out, end_mode="STOP", face=None, allow_trigger=False, face_map=None,
                shortcuts=False):
        """Route to any of goals [(map,x,y)], emitting legs and actions.
        shortcuts: first an ESCAPE ROPE out of a cave or building, Fly, and
        on the way a MAX REPEL, where they pay (not in legs the program
        walks again and again)."""
        if shortcuts:
            self.shortcuts(goals, label, out)
        guard = 0
        parts = {}
        while True:
            guard += 1
            if guard > 30:
                raise StoryError("Too many planned battles and REPELs on the way to %s" % label)
            start = self.node()
            route = self.engine.route(start, goals, start_facing=self.pos[3])
            if route is None:
                raise StoryError("No route for '%s' from %s to %s" % (label, self.pos, sorted(goals)[:4]))
            route = self.avoid_walk_up_tiles(start, goals, route)
            pb = self.predicted_battle_index(route)
            rp = self.repel_plan(route, shortcuts)
            if rp is not None and (pb is None or rp[1] <= pb[0]):
                #  The walk up to where a MAX REPEL is used, or up to where
                #  the one that is on ends; the rest is planned from there.
                what, cut, saved = rp
                if cut > 0:
                    name = label + ("_ToRepel" if what == "use" else "_RepelEnds")
                    parts[name] = parts.get(name, 0) + 1
                    if parts[name] > 1:
                        name += str(parts[name])
                    self.check_scenes_on_the_way(start, route[:cut], label)
                    self.emit_route(route[:cut], name, out, "STOP")
                if what == "use":
                    self.use_repel(label, out, saved)
                continue
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
            self.check_scenes_on_the_way(start, route, label)
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
            if k in Builder.REPEL_STEP_KINDS and self.state.repel_left > -Builder.REPEL_AFTER:
                #  "REPEL's effect wore off..." where it gets to 0 (the next
                #  press of the d-pad closes that text, see REPEL_STEP_KINDS);
                #  wild battles from then on.
                self.state.repel_left -= 1
                if self.state.repel_left == REPEL_MARGIN:
                    self.engine.invalidate()
            if k in ("walk", "surf") and self.state.repel_left <= REPEL_MARGIN and "encounter" not in risk:
                #  Planned with the REPEL, which has worn off by now.
                if self.engine.tile_risk(self.engine.view(node[0]), node[1], node[2], k == "surf", repel=False) & {"encounter"}:
                    risk = set(risk) | {"encounter"}
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
            elif k == "smash":
                self.state.smash_done.setdefault(node[0], set()).add((node[1], node[2]))
                self.engine.invalidate()
                leg["face"] = d
                leg["end"] = "INTERACT"
                self.close_leg(leg, out)
                #  "This rock appears to be breakable..." YES. A wild battle
                #  may come after it: the scene step fights it.
                out.append({"type": "TALK_YES", "label": "Rock Smash"})
                out.append({"type": "WAIT", "label": "Rock Smash animation", "p0": 2500})
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
            elif k == "waterfall":
                #  EventScript_Waterfall: "Would you like to use WATERFALL?"
                leg["face"] = d
                leg["end"] = "INTERACT"
                self.close_leg(leg, out)
                out.append({"type": "TALK_YES", "label": "Waterfall"})
                out.append({"type": "WAIT", "label": "Waterfall climb", "p0": 2500 + 500 * edge["tiles"]})
                self.pos = (node[0], node[1], node[2], "up")
                part += 1
                leg = self.new_leg("%s_%d" % (label, part))
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
                #  From outdoors into a building or a cave: where an ESCAPE
                #  ROPE goes from now on (UpdateEscapeWarp(): the warp tile,
                #  one below it unless the player faced down; not out of
                #  Viridian Forest).
                src = edge["warp"][0]
                if (self.d.maps[src].map_type in Builder.OUTDOOR_TYPES
                        and self.d.maps[node[0]].map_type not in Builder.OUTDOOR_TYPES
                        and src != "MAP_VIRIDIAN_FOREST"):
                    self.state.escape = (src, edge["warp"][1], edge["warp"][2] + (0 if d == "down" else 1))
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

    #  What takes the player on when a direction is held (the others need A).
    HELD_EDGE_KINDS = ("walk", "surf", "dismount", "jump", "spin", "stepwarp", "warp")

    def make_bump(self, label):
        """The leg just emitted ends where the way is shut: its last
        straight stretch, which starts on the water, becomes one run that
        goes on until the player stands still (RUN_SURF_BUMP).

        Over open water the video cannot count tiles, and a run timed for
        30 came out one short every time between Five Island and Resort
        Gorgeous (two map borders on the way). Against a rock, a wall or
        someone who stands still, the count does not matter."""
        leg = self.legs[-1]
        runs = leg["runs"]
        if leg["end"] != "STOP" or not runs:
            raise StoryError("bump: %s does not end with a plain run (%s, %s)" % (label, leg["end"], runs))
        d = runs[-1][0]
        first = len(runs)
        while first > 0 and runs[first - 1][0] == d and runs[first - 1][2] in (
                RUN_SURF, RUN_SURF_CHECKED, RUN_DISMOUNT, RUN_WALK, RUN_CHECKED):
            first -= 1
        stretch = runs[first:]
        if not stretch or stretch[0][2] not in (RUN_SURF, RUN_SURF_CHECKED):
            raise StoryError("bump: the last stretch of %s does not start on the water: %s" % (label, runs))
        for nxt, edge in self.engine.edges(self.node(), set()):
            if edge["dir"] == d and edge["kind"] in Builder.HELD_EDGE_KINDS:
                raise StoryError("bump: %s: the way goes on %s from %s (%s to %s)" % (
                    label, d, self.pos, edge["kind"], nxt[:3]))
        tiles = sum(r[1] for r in stretch)
        leg["runs"] = runs[:first] + [[d, tiles, RUN_SURF_BUMP]]

    # ------------------------------------------------------------ rock stairs
    #  A step up from rock stairs, or down onto them, is walked slowly
    #  (PlayerNotOnBikeMoving(): PlayerIsMovingOnRockStairs() ->
    #  PlayerWalkSlow(); UpdateWalkSlowAnim() moves a pixel in two frames
    #  of three: 23 frames a tile instead of 16). Sideways they are walked
    #  as any tile. An input timed at 16 frames a tile ends 7 frames early
    #  for each such step, and it has 8 to spare: a tile short from two of
    #  them on, and from one when anything else on the way takes a few
    #  frames (a map border). The video sees that and the walk makes it up,
    #  where it can follow. On 16 November 2026 it could not on the way
    #  south from Seven Island (the sea all around the path): the stairs at
    #  (8, 14) and the border after them made the run of 19 one short, and
    #  the grass at the Canyon Entrance was walked along a row too high,
    #  for half an hour. (The logs up to then, where the video followed a
    #  plain run to its end: with two steps on stairs 676 of 680 a tile
    #  short, with one 2 of 276; without stairs, over a map border, 1 of
    #  1,434.)
    WALKED_KINDS = (RUN_WALK, RUN_CHECKED, RUN_CHECKED_TRAINER, RUN_APPROACH, RUN_BACK)
    SLOW_BITS = 64      # the steps of a run the data can mark (StoryRun::slow)

    def mark_slow_steps(self):
        """leg["slow"]: for each run of each leg, the steps of it the game
        walks slowly, as bits (bit i = step i of the run; the C++ holds
        the input 7 frames longer for each). A run with marks that is
        longer than the bits reach is cut in two (or more)."""
        stairs = self.d.behaviors.by_name.get("MB_ROCK_STAIRS", -1)

        def is_stairs(m, x, y):
            return m.in_bounds(x, y) and self.engine.view(m.id).tile(x, y)[3] == stairs

        for leg in self.legs:
            leg["slow"] = [0] * len(leg["runs"])
            #  Up from one or down onto one: either way the leg stands on it.
            if not any(is_stairs(self.d.maps[mid], x, y) for mid, x, y in leg["points"]):
                continue
            m = self.d.maps[leg["start"][0]]
            x, y = leg["start"][1], leg["start"][2]
            before = (m.id, x, y)
            runs, slow = [], []
            for d, count, kind in leg["runs"]:
                if kind == RUN_SPIN:
                    #  (Spinners turn the player on the way: not a straight line.)
                    raise StoryError("rock stairs on a leg with spinner tiles: %s" % leg["name"])
                steps = []
                for i in range(count * (2 if kind == RUN_JUMP else 1)):
                    t = self.engine.neighbor_tile(m, x, y, d)
                    if t is None:
                        raise StoryError("rock stairs: %s leaves the map at %s (%d,%d) going %s" % (
                            leg["name"], m.id, x, y, d))
                    m2, x2, y2 = t
                    if kind in Builder.WALKED_KINDS and (
                            (d == "up" and is_stairs(m, x, y)) or (d == "down" and is_stairs(m2, x2, y2))):
                        steps.append(i)
                    before = (m.id, x, y)
                    m, x, y = m2, x2, y2
                base = 0
                while True:
                    part = count - base
                    #  (A part with a mark is at most SLOW_BITS long: the
                    #  C++ holds the marks of a run in one word. The long
                    #  walk down Navel Rock's Fork has the stairs in its
                    #  first steps and 94 tiles in all.)
                    if part > Builder.SLOW_BITS and any(base <= s < base + part for s in steps):
                        part = Builder.SLOW_BITS
                    runs.append([d, part, kind])
                    slow.append(sum(1 << (s - base) for s in steps if base <= s < base + part))
                    base += part
                    if base >= count:
                        break
            #  (The step onto a warp tile is a run of the leg, not a point of
            #  its way: the way goes on behind the warp.)
            at = before if leg["end"] == "WARP_STEP" else (m.id, x, y)
            end = leg["points"][-1]
            if at != (end[0], end[1], end[2]):
                raise StoryError("rock stairs: the runs of %s end at %s, its way at %s" % (
                    leg["name"], (at,), (end,)))
            leg["runs"] = runs
            leg["slow"] = slow

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

    # ------------------------------------------------------------ fly
    #  KantoFlyLocation (PokemonFRLG_Navigation.h) in its order: the heal
    #  location Fly lands on, and the flag that makes it a destination on
    #  the Fly map (region_map.c, GetMapsecType()).
    KANTO_FLY = [
        ("HEAL_LOCATION_PALLET_TOWN", "FLAG_WORLD_MAP_PALLET_TOWN"),
        ("HEAL_LOCATION_VIRIDIAN_CITY", "FLAG_WORLD_MAP_VIRIDIAN_CITY"),
        ("HEAL_LOCATION_PEWTER_CITY", "FLAG_WORLD_MAP_PEWTER_CITY"),
        ("HEAL_LOCATION_ROUTE4", "FLAG_WORLD_MAP_ROUTE4_POKEMON_CENTER_1F"),
        ("HEAL_LOCATION_CERULEAN_CITY", "FLAG_WORLD_MAP_CERULEAN_CITY"),
        ("HEAL_LOCATION_VERMILION_CITY", "FLAG_WORLD_MAP_VERMILION_CITY"),
        ("HEAL_LOCATION_ROUTE10", "FLAG_WORLD_MAP_ROUTE10_POKEMON_CENTER_1F"),
        ("HEAL_LOCATION_LAVENDER_TOWN", "FLAG_WORLD_MAP_LAVENDER_TOWN"),
        ("HEAL_LOCATION_CELADON_CITY", "FLAG_WORLD_MAP_CELADON_CITY"),
        ("HEAL_LOCATION_SAFFRON_CITY", "FLAG_WORLD_MAP_SAFFRON_CITY"),
        ("HEAL_LOCATION_FUCHSIA_CITY", "FLAG_WORLD_MAP_FUCHSIA_CITY"),
        ("HEAL_LOCATION_CINNABAR_ISLAND", "FLAG_WORLD_MAP_CINNABAR_ISLAND"),
        ("HEAL_LOCATION_INDIGO_PLATEAU", "FLAG_WORLD_MAP_INDIGO_PLATEAU_EXTERIOR"),
    ]
    #  Opening the Fly map, the flight and the landing: about 27 s on the
    #  Switch, where a tile of the route costs about half a second (the runs
    #  measured on the video, the turns, the doors: 69 tiles running out of
    #  Cerulean Cave took 35 s). Too dear before: the program walked where
    #  flying is faster.
    COST_FLY = 60.0
    #  Fly only when it saves more than that.
    FLY_MARGIN = 20.0
    FLY_MAP_TYPES = ("MAP_TYPE_TOWN", "MAP_TYPE_CITY", "MAP_TYPE_ROUTE", "MAP_TYPE_OCEAN_ROUTE")
    _fly_data = None
    _worldmap_flags = None

    def fly_data(self):
        """[(heal location dict, flag)] in KantoFlyLocation order."""
        if Builder._fly_data is None:
            import json
            import os
            with open(os.path.join(self.d.root, "src/data/heal_locations.json")) as f:
                hl = {h["id"]: h for h in json.load(f)["heal_locations"]}
            Builder._fly_data = [(hl[h], flag) for h, flag in Builder.KANTO_FLY]
        return Builder._fly_data

    def worldmap_flags(self):
        """map id -> the FLAG_WORLD_MAP_* its scripts set when entered
        (setworldmapflag, a destination on the Fly map from then on)."""
        if Builder._worldmap_flags is None:
            import os
            import re
            out = {}
            for mid, m in self.d.maps.items():
                p = os.path.join(self.d.root, "data/maps", m.folder, "scripts.inc")
                if not os.path.exists(p):
                    continue
                mm = re.search(r"setworldmapflag\s+(FLAG_WORLD_MAP_\w+)", open(p).read())
                if mm:
                    out[mid] = mm.group(1)
            Builder._worldmap_flags = out
        return Builder._worldmap_flags

    def note_visited(self, leg):
        flags = self.worldmap_flags()
        for mid, _, _ in leg["points"]:
            f = flags.get(mid)
            if f is not None:
                self.state.flags.add(f)

    def fly_destination(self, pc_map_id=None, index=None):
        """Index (KantoFlyLocation) of the Fly destination for a Pokemon
        Center, if Fly can go there now."""
        if not self.state.abilities.get("fly") or "FLAG_BADGE03_GET" not in self.state.flags:
            return None
        for i, (h, flag) in enumerate(self.fly_data()):
            if (index is not None and i == index) or (pc_map_id is not None and h["respawn_map"] == pc_map_id):
                return i if flag in self.state.flags else None
        return None

    _kanto_mapsecs = None

    def in_kanto(self, map_id):
        """The map is in Kanto (its section is on the Kanto region map): the
        Fly map there is Kanto's. From the Sevii Islands it only shows the
        islands (REGIONMAP_TYPE_FLY has no SWITCH button)."""
        if Builder._kanto_mapsecs is None:
            import os
            import re
            p = os.path.join(self.d.root, "src/data/region_map/region_map_layout_kanto.h")
            Builder._kanto_mapsecs = set(re.findall(r"MAPSEC_\w+", open(p).read())) - {"MAPSEC_NONE"}
        return self.d.maps[map_id].mapsec in Builder._kanto_mapsecs

    def route_cost(self, goals):
        route = self.engine.route(self.node(), goals, start_facing=self.pos[3])
        if route is None:
            return None
        return sum(e["cost"] for e, _ in route)

    def fly(self, index, label, out, goals=None, force=False):
        """Fly to KantoFlyLocation 'index' (out of a building first). With
        'goals' (where the walk goes on to): only when it saves time.
        force: also to the town the player is in (the Fly map takes it).
        Returns whether it flew."""
        if self.surfing or not self.in_kanto(self.pos[0]):
            return False
        h, _ = self.fly_data()[index]
        if self.pos[0] == h["map"] and not force:
            return False
        outdoor = [mid for mid, m in self.d.maps.items() if m.map_type in Builder.FLY_MAP_TYPES]
        here = self.d.maps[self.pos[0]].map_type in Builder.FLY_MAP_TYPES
        exit_cost = 0.0 if here else self.route_cost(MapGoals(outdoor))
        if exit_cost is None:
            return False
        if goals is not None:
            walk = self.route_cost(goals)
            saved = self.pos
            self.pos = (h["map"], h["x"], h["y"], "down")
            after = self.route_cost(goals)
            self.pos = saved
            #  (No way on foot at all: Fly. The Indigo Plateau after Victory
            #  Road was reset.)
            if after is None or (walk is not None and walk <= exit_cost + Builder.COST_FLY + after + Builder.FLY_MARGIN):
                return False
        if not here:
            self.walk_to(MapGoals(outdoor), label + "_Out", out)
        out.append({"type": "FLY", "label": label + "_Fly", "p0": index, "p1": 1,
                    "text": h["map"]})
        self.leave_map(self.pos[0])
        self.pos = (h["map"], h["x"], h["y"], "down")
        self.surfing = False
        return True

    # ------------------------------------------------------------ shortcuts
    OUTDOOR_TYPES = ("MAP_TYPE_TOWN", "MAP_TYPE_CITY", "MAP_TYPE_ROUTE", "MAP_TYPE_UNDERWATER", "MAP_TYPE_OCEAN_ROUTE")
    #  The bag (the rope looked for by its description), the spin out and
    #  in: about 25 s.
    COST_ROPE = 60.0
    ROPE_MARGIN = 30.0
    #  A MAX REPEL (250 steps, after the Hall of Fame) where it keeps at
    #  least this many wild battles away, as the maps' encounter rates have
    #  it: a battle takes about half a minute and wears the team down on
    #  the way to a legendary Pokemon; the bag takes 25 s, and the REPEL
    #  costs 700.
    REPEL_MIN_BATTLES = 2.0
    REPEL_STEPS = 250
    #  The edges that are a step for the game's count (UpdateRepelCounter(),
    #  from TryStartStepBasedScript(), for every step the player takes: a
    #  ledge jump is one). Not the step onto a warp tile (the warp comes
    #  first there), not getting on or off SURF, a waterfall or a spin (the
    #  game moves the player), nor the way in through a door.
    #  Where the count gets to 0 the game says "REPEL's effect wore off..."
    #  (EventScript_RepelWoreOff: a sign's text box). Its wait for a button
    #  is over with A or B, and just as well with a direction held other
    #  than the one the player faced at the last talk (WaitForAorBPress(),
    #  with the script about to end): the walk's next press that way closes
    #  it. So the program hardly ever sees that text (no run logged it), and
    #  the count here has to say where the REPEL ends.
    REPEL_STEP_KINDS = ("walk", "surf", "jump", "cut", "smash", "item", "door")
    #  While a REPEL is on the game refuses another one ("...the effects of
    #  a REPEL lingered from earlier"): the next one this many steps after
    #  the one before ended by the count here (WorldState.repel_left goes
    #  on below 0 that far). The real walk has more steps than the count (a
    #  step back after one too far), rarely fewer: the step that starts a
    #  trainer's battle or a scene is none for the game.
    REPEL_AFTER = 5

    def cost_from(self, pos, goals, surfing=False):
        saved = (self.pos, self.surfing)
        self.pos = pos
        self.surfing = surfing
        try:
            return self.route_cost(goals)
        finally:
            self.pos, self.surfing = saved

    def fly_option(self, goals):
        """(cost, KantoFlyLocation) of the best Fly from here to the goals:
        out of the building, the flight, the walk from the landing."""
        if (self.surfing or not self.state.abilities.get("fly") or "FLAG_BADGE03_GET" not in self.state.flags
                or not self.in_kanto(self.pos[0])):
            return None
        if self.d.maps[self.pos[0]].map_type in Builder.FLY_MAP_TYPES:
            exit_cost = 0.0
        else:
            outdoor = [mid for mid, m in self.d.maps.items() if m.map_type in Builder.FLY_MAP_TYPES]
            exit_cost = self.route_cost(MapGoals(outdoor))
            if exit_cost is None:
                return None
        #  Only the towns nearest the goals (in maps on the way: the routes
        #  from all thirteen took minutes per walk).
        hops = self.map_hops(goals)
        candidates = [(hops.get(h["map"], 99), i) for i, (h, flag) in enumerate(self.fly_data())
                      if flag in self.state.flags and h["map"] != self.pos[0]]
        if not candidates:
            return None
        nearest = min(c[0] for c in candidates)
        candidates = sorted(c for c in candidates if c[0] <= nearest + 1)[:3]
        best = None
        for _, i in candidates:
            h, _flag = self.fly_data()[i]
            after = self.cost_from((h["map"], h["x"], h["y"], "down"), goals)
            if after is None:
                continue
            total = exit_cost + Builder.COST_FLY + after
            if best is None or total < best[0]:
                best = (total, i)
        return best

    def rope_option(self, goals):
        """Cost of an ESCAPE ROPE from here (the walk, or Fly, after it)."""
        import os
        if self.state.items.get("ESCAPE_ROPE", 0) <= 0 and not os.environ.get("STORY_ROPES_UNLIMITED"):
            return None
        if not self.d.maps[self.pos[0]].allow_escaping or self.state.escape is None:
            return None
        #  Only to leave: not to a goal in a cave or building of that kind
        #  (back to the entrance and in again).
        goal_maps = goals.maps if isinstance(goals, MapGoals) else {g[0] for g in goals}
        if any(self.d.maps[m].allow_escaping for m in goal_maps if m in self.d.maps):
            return None
        em, ex, ey = self.state.escape
        landing = (em, ex, ey, "down")
        walk = self.cost_from(landing, goals)
        saved = (self.pos, self.surfing)
        self.pos, self.surfing = landing, False
        try:
            fo = self.fly_option(goals)
        finally:
            self.pos, self.surfing = saved
        options = [c for c in (walk, fo[0] + Builder.FLY_MARGIN if fo else None) if c is not None]
        if not options:
            return None
        return Builder.COST_ROPE + min(options)

    def shortcuts(self, goals, label, out):
        """Before a walk: an ESCAPE ROPE out of a cave or a building, Fly
        (from anywhere in Kanto, to the town nearest the goal); each only
        where it pays clearly. (The MAX REPEL: repel_plan().)"""
        walk = self.route_cost(goals)
        least = min(Builder.COST_FLY + Builder.FLY_MARGIN, Builder.COST_ROPE + Builder.ROPE_MARGIN)
        if walk is None or walk > least:
            fo = self.fly_option(goals)
            ro = self.rope_option(goals)
            inf = float("inf")
            w = walk if walk is not None else inf
            fly_total = fo[0] + Builder.FLY_MARGIN if fo else inf
            rope_total = ro + Builder.ROPE_MARGIN if ro is not None else inf
            if rope_total < w and rope_total <= fly_total:
                self.use_rope(label, out)
                self.shortcut_log.append((self.segment_id, label, "ESCAPE ROPE", w - ro))
                #  Fly from where it went?
                self.shortcuts(goals, label, out)
                return
            if fly_total < w:
                before = self.pos
                if self.fly(fo[1], label, out):
                    self.shortcut_log.append((self.segment_id, label, "FLY from %s" % before[0], w - fo[0]))

    _map_graph = None

    def map_hops(self, goals):
        """map -> how many maps away from the goals' maps (connections and
        warps both ways)."""
        if Builder._map_graph is None:
            g = {}
            for mid, m in self.d.maps.items():
                for c in m.connections:
                    other = c.get("map")
                    if other in self.d.maps:
                        g.setdefault(mid, set()).add(other)
                        g.setdefault(other, set()).add(mid)
                for w in m.warps:
                    other = w.get("dest_map")
                    if other in self.d.maps:
                        g.setdefault(mid, set()).add(other)
                        g.setdefault(other, set()).add(mid)
            Builder._map_graph = g
        if isinstance(goals, MapGoals):
            start = set(goals.maps) if hasattr(goals, "maps") else set()
        else:
            start = {gl[0] for gl in goals}
        dist = {m: 0 for m in start}
        frontier = list(start)
        while frontier:
            nxt = []
            for m in frontier:
                for o in Builder._map_graph.get(m, ()):
                    if o not in dist:
                        dist[o] = dist[m] + 1
                        nxt.append(o)
            frontier = nxt
        return dist

    #  Steps the way out on foot may have (no battles: the story must go
    #  on the same after the rope).
    WALK_OUT_STEPS = ("WALK", "WAIT", "TALK_YES", "TALK")

    def use_rope(self, label, out):
        em, ex, ey = self.state.escape
        #  The way out on foot to where the rope goes, for a bag without an
        #  ESCAPE ROPE (not enough money at the Poke Mart): the program
        #  walks it then, else skips it. Planned on a copy of the state: the
        #  story goes on as after the rope.
        alt = []
        saved = (self.state, self.pos, self.surfing, copy.deepcopy(self.maybe_came),
                 copy.deepcopy(self.maybe_at), self.strength_used)
        self.state = self.state.clone()
        self.engine.state = self.state
        self.engine.invalidate()
        try:
            self.walk_to({(em, ex, ey)}, label + "_WalkOut", alt)
            ok = all(s["type"] in Builder.WALK_OUT_STEPS and
                     (s["type"] != "TALK" or s["label"].startswith(("Pick up item", "Card Key door"))) for s in alt)
        except (StoryError, EngineError):
            ok = False
        (self.state, self.pos, self.surfing, self.maybe_came, self.maybe_at, self.strength_used) = saved
        self.engine.state = self.state
        self.engine.invalidate()
        if not ok:
            alt = []
        out.append({"type": "ESCAPE_ROPE", "label": label + "_EscapeRope", "p0": len(alt)})
        out.extend(alt)
        self.state.items["ESCAPE_ROPE"] = self.state.items.get("ESCAPE_ROPE", 0) - 1
        self.leave_map(self.pos[0])
        em, ex, ey = self.state.escape
        #  The spin in ends facing down (ResetInitialPlayerAvatarState()).
        self.pos = (em, ex, ey, "down")
        self.surfing = False

    def wild_odds(self, edge, node):
        """The chance of a wild battle on this edge of a route without a
        REPEL: the encounter rate of its map on grass, cave floor and
        water; for a rock ROCK SMASH breaks, also the rate of the rocks
        (RockSmashWildEncounter(), which a REPEL stops as well)."""
        k = edge["kind"]
        if k not in Builder.REPEL_STEP_KINDS:
            return 0.0
        odds = 0.0
        if "encounter" in self.engine.tile_risk(self.engine.view(node[0]), node[1], node[2], k == "surf", repel=False):
            odds = (self.gd.water_rate if k == "surf" else self.gd.land_rate).get(node[0], 0) / 100.0
        if k == "smash":
            odds += self.gd.rock_rate.get(node[0], 0) / 100.0
        return odds

    def repel_plan(self, route, may_use):
        """What a MAX REPEL changes about this route. None: nothing.
        ("ends", i, 0): the one that is on ends after route[:i] with wild
        Pokemon still ahead: the rest is planned again, without it.
        ("use", i, battles): one is used after route[:i], and keeps about
        that many wild battles away.

        It is used as late as it can be: right before the first tile with
        wild Pokemon, not at the start of the walk. (Used in the Pokemon
        Center of One Island, it was over four steps before the rock at
        the foot of Mt. Ember's Summit Path: a wild Pokemon came out of the
        rock, and every one of the cave after it. The same on the way to
        Zapdos, Articuno, the Ruby and Mewtwo: town and sea first, the cave
        last.)
        A walk that ends in a cave or on a route with grass goes on there
        (boulders to push, the way to the legendary Pokemon): only its land
        counts then, the REPEL is not spent on the sea before it. With more
        than 250 steps to cover: a second one where the first ends if the
        bag has it, else the 250 steps that keep the most battles away."""
        import os
        n = len(route)
        left = self.state.repel_left
        counted = [1 if e["kind"] in Builder.REPEL_STEP_KINDS else 0 for e, _ in route]

        def standing(c):
            #  After route[:c] the player stands on land (the bag can be
            #  opened, a leg can end), not in the sight of a trainer that
            #  turns around.
            if c == 0:
                return not self.surfing
            e, nd = route[c - 1]
            return (e["kind"] in ("walk", "jump", "dismount", "cut", "smash", "item", "door")
                    and not nd[4] and "trainer" not in e.get("risk", ()))

        if left > REPEL_MARGIN:
            steps = 0
            for i in range(n):
                steps += counted[i]
                if steps >= left - REPEL_MARGIN and standing(i + 1):
                    #  (On the water the walk goes on to the shore first:
                    #  nothing to plan differently there.)
                    if any(self.wild_odds(e, nd) > 0 for e, nd in route[i + 1:]):
                        return ("ends", i + 1, 0)
                    break
            return None
        have = 99 if os.environ.get("STORY_REPELS_UNLIMITED") else self.state.items.get("MAX_REPEL", 0)
        if not may_use or not self.postgame or self.segment_no_repel or have <= 0 or n == 0:
            return None
        land_only = route[-1][1][0] in self.gd.land_encounters
        odds = [0.0 if (land_only and e["kind"] == "surf") else self.wild_odds(e, nd) for e, nd in route]
        steps_before = [0] * (n + 1)
        odds_before = [0.0] * (n + 1)
        for i in range(n):
            steps_before[i + 1] = steps_before[i] + counted[i]
            odds_before[i + 1] = odds_before[i] + odds[i]
        #  The steps it keeps the wild Pokemon away, as the router plans
        #  them (the last ones count as without it).
        lasts = Builder.REPEL_STEPS - REPEL_MARGIN - 1

        def saved(c):
            end = c
            while end < n and steps_before[end + 1] - steps_before[c] <= lasts:
                end += 1
            return odds_before[end] - odds_before[c]

        #  Not before the one that is on (its last steps) is over for sure.
        wait = max(0, left + Builder.REPEL_AFTER)
        spots = [c for c in range(n) if steps_before[c] >= wait and standing(c)]
        if not spots:
            return None
        save = {c: saved(c) for c in spots}
        most = max(save.values())
        if most < Builder.REPEL_MIN_BATTLES:
            return None
        risky = [i for i in range(n) if odds[i] > 0 and i >= spots[0]]
        first = max(c for c in spots if c <= risky[0])
        beyond = (odds_before[n] - odds_before[first]) - save[first]
        if save[first] >= Builder.REPEL_MIN_BATTLES and (beyond < 1.0 or (have >= 2 and beyond >= Builder.REPEL_MIN_BATTLES)):
            return ("use", first, save[first])
        best = max(c for c in spots if save[c] >= most - 1e-9)
        return ("use", best, save[best])

    def use_repel(self, label, out, saved):
        out.append({"type": "USE_REPEL", "label": label + "_MaxRepel"})
        self.state.items["MAX_REPEL"] = self.state.items.get("MAX_REPEL", 0) - 1
        self.state.repel_left = Builder.REPEL_STEPS
        self.engine.invalidate()
        self.shortcut_log.append((self.segment_id, label, "MAX REPEL", saved))

    def no_repel_here(self, what):
        """Catching needs the wild Pokemon: no REPEL may still be on."""
        if self.state.repel_left > 0:
            raise StoryError("A REPEL is still on (%d steps) at a %s step: no REPEL in the segment(s) "
                             "before it (\"no_repel\")" % (self.state.repel_left, what))

    def leave_map(self, map_id):
        """Temporary changes of a map end when we leave it (map reload)."""
        self.state.extra_blocked.pop(map_id, None)
        self.state.cut_done.pop(map_id, None)
        self.state.smash_done.pop(map_id, None)
        self.state.ice_cracked.pop(map_id, None)
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

    def talk_alt(self, m, main_xy, alt_xy, label):
        """Where the one talked to may stand instead (some saves have it
        there: Celio at his machine or by the stools). Without a text box
        the program tries from there: one step ("dir", stepped back before
        the next step that moves) to a tile next to alt_xy, facing it (p3: 1
        up, 2 down, 3 left, 4 right; p2 = 1 marks the step)."""
        mid, px, py, _ = self.pos
        codes = {"up": 1, "down": 2, "left": 3, "right": 4}
        ax, ay = alt_xy

        def facing_to(x, y):
            for d, (dx, dy) in DIRS.items():
                if (x + dx, y + dy) == (ax, ay):
                    return d
            return None
        f = facing_to(px, py)
        if f is not None:
            return {"p2": 1, "p3": codes[f]}
        start = self.node()
        for d, (dx, dy) in DIRS.items():
            nx, ny = px + dx, py + dy
            if (nx, ny) in (main_xy, alt_xy) or not m.in_bounds(nx, ny):
                continue
            f = facing_to(nx, ny)
            if f is None:
                continue
            route = self.engine.route(start, {(mid, nx, ny)}, start_facing=self.pos[3])
            if route is not None and len(route) == 1:
                return {"p2": 1, "p3": codes[f], "dir": d}
        raise StoryError("%s: no tile next to %s one step from %s" % (label, alt_xy, (px, py)))

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
        if "fly" in step:
            #  Fly there (where the Fly map has it); without Fly, walk.
            f = step["fly"]
            index = [h for h, _ in Builder.KANTO_FLY].index(f)
            fi = self.fly_destination(index=index)
            #  force: also from the town itself (the player may be elsewhere:
            #  the POKeDEX collection's segments start from wherever the
            #  segment before ended, or chapter 20 goes round again).
            if fi is None or not self.fly(fi, step.get("label", "Fly"), out, force=bool(step.get("force"))):
                h, _ = self.fly_data()[index]
                if self.pos[0] != h["map"]:
                    self.walk_to({(h["map"], h["x"], h["y"])}, step.get("label", "Fly") + "_Walk", out, shortcuts=True)
            return
        if "unless_dex" in step:
            #  Steps the program skips when it has caught enough species
            #  (text: the ones caught for sure). They end where they start.
            u = step["unless_dex"]
            sub = []
            start = self.pos
            for s in u["steps"]:
                self.run_step(s, sub)
            if self.pos[:3] != start[:3]:
                raise StoryError("unless_dex: the steps end at %s, not %s" % (self.pos, start))
            #  p3: the count when the game's own count is known (no margin
            #  for species the program's list may have wrong).
            out.append({"type": "SKIP_IF_DEX", "label": step.get("label", "Enough species?"), "p0": len(sub),
                        "p1": u["count"], "p3": u.get("game_count", u["count"]), "text": ",".join(u["owned"])})
            out.extend(sub)
            return
        if "unless_collected" in step or "collect_segment" in step:
            #  Steps the program skips when its POKeDEX table wants none of
            #  the species of their places (text). unless_collected: they
            #  end where they start. collect_segment: a whole segment, which
            #  starts with FLY and ends outdoors (skipped, the player stays
            #  where the segment before ended: the next FLY starts there).
            whole = "collect_segment" in step
            u = step["collect_segment"] if whole else step["unless_collected"]
            sub = []
            start = self.pos
            for s in u["steps"]:
                self.run_step(s, sub)
            if not whole and self.pos[:3] != start[:3]:
                raise StoryError("unless_collected: the steps end at %s, not %s" % (self.pos, start))
            #  The species of those places, each as often as at the place
            #  where it comes most ("slug:FireRed:LeafGreen", as in a DEX
            #  step). The program looks at the places themselves (which of
            #  them is worth a visit for a species); this is for reading.
            species = []
            share = {}
            for s in sub:
                if s["type"] == "DEX" and (s.get("p2", 0) & 2):
                    for item in s["text"].split(","):
                        slug, fire_red, leaf_green = item.split(":")
                        if slug not in species:
                            species.append(slug)
                        best = share.setdefault(slug, [0, 0])
                        best[0] = max(best[0], int(fire_red))
                        best[1] = max(best[1], int(leaf_green))
            if not species:
                raise StoryError("%s: no place to catch at in these steps" % step.get("label", "?"))
            out.append({"type": "SKIP_IF_COLLECTED", "label": step.get("label", "Anything to catch?"),
                        "p0": len(sub),
                        "text": ",".join("%s:%d:%d" % (slug, share[slug][0], share[slug][1]) for slug in species)})
            out.extend(sub)
            return
        if "rod_wanted" in step:
            #  The way to a fishing rod (a whole segment: FLY, the house of
            #  the one who gives it, out again). Skipped when the POKeDEX
            #  table wants nothing of the places fished at with that rod
            #  (SKIP_IF_COLLECTED with p1 = rod + 1: the places are all the
            #  DEX steps of the story that fish with it), and when the rod
            #  is in the bag already (SKIP_IF_ITEM: the KEY ITEMS pocket is
            #  looked through once in a run).
            u = step["rod_wanted"]
            rod = u["rod"]
            sub = []
            for s in u["steps"]:
                self.run_step(s, sub)
            label = step.get("label", "The %s?" % Builder.RODS[rod])
            out.append({"type": "SKIP_IF_COLLECTED", "label": label, "p0": len(sub) + 1, "p1": rod + 1,
                        "text": Builder.RODS[rod]})
            out.append({"type": "SKIP_IF_ITEM", "label": "%s in the bag?" % Builder.RODS[rod], "p0": len(sub),
                        "p1": 1, "p2": rod + 1, "text": Builder.RODS[rod]})
            out.extend(sub)
            return
        if "walk_exact" in step:
            #  A walk over the tiles given, no route of the router's: DEOXYS'
            #  stone on Birth Island moves when A is pressed in front of it,
            #  and the next press must come within so many steps of the last
            #  (src/field_specials.c, sDeoxysStepCaps), so the way is the
            #  shortest one, and the tiles are those the solver found. The
            #  stone is an object the router does not know to be moving.
            u = step["walk_exact"]
            label = step.get("label", "Walk")
            if self.pos[0] != u["map"]:
                raise StoryError("walk_exact: the player is on %s, not on %s (%s)" % (self.pos[0], u["map"], label))
            m = self.d.maps[u["map"]]
            leg = self.new_leg(label)
            x, y = self.pos[1], self.pos[2]
            names = {(0, -1): "up", (0, 1): "down", (-1, 0): "left", (1, 0): "right"}
            for nx, ny in u["tiles"]:
                d = names.get((nx - x, ny - y))
                if d is None or not m.in_bounds(nx, ny) or m.tile(nx, ny)[1] != 0:
                    raise StoryError("walk_exact: (%d,%d) is not a free tile next to (%d,%d) (%s)" % (nx, ny, x, y, label))
                self.add_run(leg, d, RUN_WALK)
                leg["points"].append((u["map"], nx, ny))
                x, y = nx, ny
                self.pos = (u["map"], x, y, d)
            if u.get("face"):
                leg["face"] = u["face"]
                leg["end"] = "INTERACT"
                self.pos = (u["map"], x, y, u["face"])
            self.close_leg(leg, out)
            return
        if "unless_item" in step:
            #  The way to an item that is not a fishing rod (build 54: the
            #  LAX INCENSE of the Lost Cave, for the egg of WYNAUT): skipped
            #  when the bag has it (SKIP_IF_ITEM with no rod: text = words
            #  of its description, "a|b" one of them enough; p1 = the pocket).
            #  The steps end where they start (FLY again at the end), so the
            #  steps after it are the same whether they ran or not.
            u = step["unless_item"]
            sub = []
            start = self.pos
            for s in u["steps"]:
                self.run_step(s, sub)
            if self.pos[:3] != start[:3]:
                raise StoryError("unless_item: the steps end at %s, not %s" % (self.pos, start))
            out.append({"type": "SKIP_IF_ITEM", "label": step.get("label", "In the bag already?"),
                        "p0": len(sub), "p1": u.get("pocket", 0), "text": u["words"]})
            out.extend(sub)
            return
        if "unless_owned" in step:
            #  Steps for a Pokemon that is there once (a gift, a Pokemon
            #  that stands on the map): skipped when the program's POKeDEX
            #  table has "count" of the species (text) already. "segment":
            #  a whole segment, which starts with FLY and ends outdoors;
            #  else they end where they start.
            u = step["unless_owned"]
            sub = []
            start = self.pos
            for s in u["steps"]:
                self.run_step(s, sub)
            if not u.get("segment") and self.pos[:3] != start[:3]:
                raise StoryError("unless_owned: the steps end at %s, not %s" % (self.pos, start))
            out.append({"type": "SKIP_IF_OWNED", "label": step.get("label", "Owned already?"), "p0": len(sub),
                        "p1": u.get("count", 1), "text": u["species"]})
            out.extend(sub)
            return
        if "unless_boxes" in step:
            #  The steps that read the PC's boxes (a whole segment: it starts
            #  with FLY and ends outdoors with its save): skipped when the
            #  program read the boxes in this run and caught nothing for the
            #  POKeDEX since.
            u = step["unless_boxes"]
            sub = []
            for s in u["steps"]:
                self.run_step(s, sub)
            if not sub or sub[0]["type"] != "FLY" or sub[-1]["type"] != "SAVE":
                raise StoryError("unless_boxes: the steps start with FLY and end with a save (%s)"
                                 % step.get("label", "?"))
            if sum(1 for s in sub if s["type"] == "BOXES") != 1 or any(s["type"] == "SAVE" for s in sub[:-1]):
                raise StoryError("unless_boxes: one look at the boxes, and no save before the last step (%s)"
                                 % step.get("label", "?"))
            out.append({"type": "SKIP_IF_BOXES", "label": step.get("label", "The PC's boxes read?"),
                        "p0": len(sub)})
            out.extend(sub)
            return
        if "boxes" in step:
            #  The PC's boxes are read (nothing in them is changed). The
            #  player stands in front of the PC of a Pokemon Center and faces
            #  it: the walk before this step ended so (talk(..., mode="plain")).
            m = self.d.maps[self.pos[0]]
            x, y, facing = self.pos[1], self.pos[2], self.pos[3]
            if facing != "up" or not m.in_bounds(x, y - 1) or \
                    self.d.behaviors.name(m.tile(x, y - 1)[3]) != "MB_PC":
                raise StoryError("boxes: the player does not face a PC (%s, at %s)"
                                 % (step.get("label", "?"), self.pos))
            if not out or out[-1]["type"] != "WALK":
                raise StoryError("boxes: not after the walk to the PC (%s)" % step.get("label", "?"))
            out.append({"type": "BOXES", "label": step.get("label", "The PC's boxes")})
            return
        if "unless_no_stones" in step:
            #  The steps of the evolution stones (a whole segment: it starts
            #  with FLY and ends outdoors with its save): skipped when the
            #  boxes have nothing that evolves with a stone of the bag or of
            #  the Department Store.
            #  With "fetch" ("Moon Stone in Mt. Moon": the stone, then where
            #  the steps pick it up) the segment is for that one stone: it is
            #  skipped when nothing in the boxes evolves with it, and only
            #  that stone is used at its PC step.
            u = step["unless_no_stones"]
            sub = []
            for s in u["steps"]:
                self.run_step(s, sub)
            if not sub or sub[0]["type"] != "FLY" or sub[-1]["type"] != "SAVE":
                raise StoryError("unless_no_stones: the steps start with FLY and end with a save (%s)"
                                 % step.get("label", "?"))
            if sum(1 for s in sub if s["type"] == "STONES") != 1:
                raise StoryError("unless_no_stones: one step for the stones (%s)" % step.get("label", "?"))
            check = {"type": "SKIP_IF_NO_STONES", "label": step.get("label", "Anything to evolve with a stone?"),
                     "p0": len(sub)}
            fetch = u.get("fetch")
            if fetch:
                if " in " not in fetch or fetch.split(" in ")[0] not in ("Moon Stone", "Sun Stone"):
                    raise StoryError("unless_no_stones: fetch is \"Moon Stone in <place>\" or \"Sun Stone in "
                                     "<place>\" (%s: %s)" % (step.get("label", "?"), fetch))
                check["text"] = fetch
            out.append(check)
            out.extend(sub)
            return
        if "unless_no_levels" in step:
            #  The steps of one Pokemon that evolves by a level (a whole
            #  segment: it starts with FLY and ends outdoors with its save):
            #  skipped when the boxes have nothing left that evolves by a
            #  level into a species the POKeDEX table wants.
            u = step["unless_no_levels"]
            sub = []
            for s in u["steps"]:
                self.run_step(s, sub)
            if not sub or sub[0]["type"] != "FLY" or sub[-1]["type"] != "SAVE":
                raise StoryError("unless_no_levels: the steps start with FLY and end with a save (%s)"
                                 % step.get("label", "?"))
            #  "exp" (build 46): a round of the Elite Four with the EXP. SHARE
            #  on the Pokemon of the PC that has the most levels to go (p1 = 2):
            #  no training (no RAISE), the League's steps in place of it.
            exp = bool(u.get("exp"))
            for kind in ("LEVEL_OUT", "RAISE", "LEVEL_IN"):
                if sum(1 for s in sub if s["type"] == kind) != (0 if exp and kind == "RAISE" else 1):
                    raise StoryError("unless_no_levels: %s %s step (%s)"
                                     % ("no" if exp and kind == "RAISE" else "one", kind, step.get("label", "?")))
            #  "friendship" (build 45): the Pokemon that evolves by a level up
            #  with high friendship (p1 = 1), walked with first (one PACE).
            friendship = bool(u.get("friendship"))
            if exp and friendship:
                raise StoryError("unless_no_levels: exp and friendship do not go together (%s)"
                                 % step.get("label", "?"))
            if sum(1 for s in sub if s["type"] == "PACE") != (1 if friendship else 0):
                raise StoryError("unless_no_levels: %s PACE step (%s)" % ("one" if friendship else "no",
                                                                           step.get("label", "?")))
            out.append({"type": "SKIP_IF_NO_LEVELS", "label": step.get("label", "Anything to evolve by a level?"),
                        "p0": len(sub), "p1": 2 if exp else 1 if friendship else 0})
            out.extend(sub)
            return
        if "level_trip" in step:
            #  In front of the PC of a Pokemon Center, facing it: the Pokemon
            #  of a box with the fewest levels to go comes into the party's
            #  sixth place (LEVEL_OUT), the steps train it (RAISE, a nurse on
            #  the way: the game is saved there), and back at the same PC it
            #  goes into its cell (LEVEL_IN).
            b = step["level_trip"]
            m = self.d.maps[self.pos[0]]
            x, y, facing = self.pos[1], self.pos[2], self.pos[3]
            if facing != "up" or not m.in_bounds(x, y - 1) or \
                    self.d.behaviors.name(m.tile(x, y - 1)[3]) != "MB_PC":
                raise StoryError("level_trip: the player does not face a PC (%s, at %s)"
                                 % (step.get("label", "?"), self.pos))
            if not out or out[-1]["type"] != "WALK":
                raise StoryError("level_trip: not after the walk to the PC (%s)" % step.get("label", "?"))
            start = (self.pos[0], self.pos[1], self.pos[2], self.pos[3])
            sub = []
            for s in b["steps"]:
                self.run_step(s, sub)
            if (self.pos[0], self.pos[1], self.pos[2], self.pos[3]) != start:
                raise StoryError("level_trip: the steps do not end in front of the PC they start at (%s: %s, not %s)"
                                 % (step.get("label", "?"), self.pos, start))
            #  "exp" (build 46): the Pokemon rides through the League (the
            #  EXP. SHARE on it) instead of a training: no RAISE, and saves
            #  and a FLY (to the Indigo Plateau again, after Oak's rating) are
            #  part of the way.
            exp = bool(b.get("exp"))
            if exp:
                if not sub or sub[-1]["type"] != "WALK" or any(s["type"] == "RAISE" for s in sub) \
                        or not any(s["type"] == "HEAL" for s in sub) or not any(s["type"] == "FLY" for s in sub):
                    raise StoryError("level_trip (exp): no RAISE, a nurse (the loss of a room starts over after "
                                     "the first), a FLY, the last step a walk to the PC (%s)"
                                     % step.get("label", "?"))
            elif not sub or sub[-1]["type"] != "WALK" or sum(1 for s in sub if s["type"] == "RAISE") != 1 \
                    or any(s["type"] in ("SAVE", "FLY") for s in sub):
                raise StoryError("level_trip: one RAISE, the last step a walk to the PC, no save (the nurse "
                                 "saves) or FLY (%s)" % step.get("label", "?"))
            out.append({"type": "LEVEL_OUT", "label": step.get("label", "Out of the box"), "p0": len(sub) + 1,
                        "p1": 2 if exp else 1 if b.get("friendship") else 0})
            out.extend(sub)
            out.append({"type": "LEVEL_IN", "label": b.get("in_label", "Into the box")})
            return
        if "unless_stones_in_bag" in step:
            #  The way to the stones that are sold (prize money, the
            #  Department Store): skipped when the bag has them. The steps
            #  end outdoors, and a FLY follows them: skipped or not, the
            #  player is where that FLY lands.
            u = step["unless_stones_in_bag"]
            sub = []
            for s in u["steps"]:
                self.run_step(s, sub)
            if sum(1 for s in sub if s["type"] == "BUY_STONES") != 1:
                raise StoryError("unless_stones_in_bag: one purchase of stones (%s)" % step.get("label", "?"))
            if self.d.maps[self.pos[0]].map_type not in Builder.FLY_MAP_TYPES:
                raise StoryError("unless_stones_in_bag: the steps do not end outdoors (%s, at %s)"
                                 % (step.get("label", "?"), self.pos))
            out.append({"type": "SKIP_IF_STONES_IN_BAG", "label": step.get("label", "Stones to buy?"),
                        "p0": len(sub)})
            out.extend(sub)
            return
        if "buy_stones" in step:
            #  In front of the clerk who sells the stones (the walk before
            #  this step ended there: talk(..., mode="plain", counter=True)).
            if not out or out[-1]["type"] != "WALK":
                raise StoryError("buy_stones: not after the walk to the clerk (%s)" % step.get("label", "?"))
            out.append({"type": "BUY_STONES", "label": step.get("label", "Buy stones")})
            return
        if "stones" in step:
            #  In front of the PC of a Pokemon Center, facing it (as "boxes").
            m = self.d.maps[self.pos[0]]
            x, y, facing = self.pos[1], self.pos[2], self.pos[3]
            if facing != "up" or not m.in_bounds(x, y - 1) or \
                    self.d.behaviors.name(m.tile(x, y - 1)[3]) != "MB_PC":
                raise StoryError("stones: the player does not face a PC (%s, at %s)"
                                 % (step.get("label", "?"), self.pos))
            if not out or out[-1]["type"] != "WALK":
                raise StoryError("stones: not after the walk to the PC (%s)" % step.get("label", "?"))
            out.append({"type": "STONES", "label": step.get("label", "Stones")})
            return
        if "daycare_trip" in step:
            #  (Build 48, experimental.) The Four Island Day Care, one egg per
            #  trip. In front of the PC of a Pokemon Center, facing it:
            #  DAYCARE_OUT brings a DITTO and a Pokemon of the baby's family
            #  out of the boxes into the party's fifth and sixth place (the
            #  ones that were there lie in their cells); the steps are the
            #  trip to Four Island and the Day Care, the walk and the egg,
            #  and they end in front of the same PC again (the program skips
            #  all of them when it has no parents for the baby).
            b = step["daycare_trip"]
            m = self.d.maps[self.pos[0]]
            x, y, facing = self.pos[1], self.pos[2], self.pos[3]
            if facing != "up" or not m.in_bounds(x, y - 1) or \
                    self.d.behaviors.name(m.tile(x, y - 1)[3]) != "MB_PC":
                raise StoryError("daycare_trip: the player does not face a PC (%s, at %s)"
                                 % (step.get("label", "?"), self.pos))
            if not out or out[-1]["type"] != "WALK":
                raise StoryError("daycare_trip: not after the walk to the PC (%s)" % step.get("label", "?"))
            start = (self.pos[0], self.pos[1], self.pos[2], self.pos[3])
            sub = []
            for s in b["steps"]:
                self.run_step(s, sub)
            if (self.pos[0], self.pos[1], self.pos[2], self.pos[3]) != start:
                raise StoryError("daycare_trip: the steps do not end in front of the PC they start at (%s: %s, not %s)"
                                 % (step.get("label", "?"), self.pos, start))
            if not sub or sub[-1]["type"] != "WALK" or any(s["type"] in ("SAVE", "HEAL", "FLY") for s in sub):
                raise StoryError("daycare_trip: walks and the Day Care's steps only, the last one a walk to the PC; "
                                 "no save, nurse or FLY (%s)" % step.get("label", "?"))
            for kind, count in (("DAYCARE_GIVE", 1), ("DAYCARE_STORE", 1), ("DAYCARE_TAKE", 1), ("HATCH", 1)):
                if sum(1 for s in sub if s["type"] == kind) != count:
                    raise StoryError("daycare_trip: %d %s step(s) (%s)" % (count, kind, step.get("label", "?")))
            #  "skip_if_boxes_have" (build 64): the trip is skipped too when the
            #  boxes already hold that many of the baby's species (DAYCARE_OUT p1):
            #  the 4th TYROGUE of C20_79C is bred only when none is left to train.
            out.append({"type": "DAYCARE_OUT", "label": step.get("label", "Parents out of the boxes"),
                        "text": b["baby"], "p0": len(sub) + 1, "p1": int(b.get("skip_if_boxes_have", 0))})
            out.extend(sub)
            out.append({"type": "DAYCARE_END", "label": b.get("end_label", "The parents are back")})
            return
        if "daycare_wait" in step:
            #  The rounds that wait for the egg (each one: to the line, along
            #  it for a few hundred steps, to the man, DAYCARE_MAN). The man
            #  gives the egg in one of them: the rest are skipped (p1 of
            #  DAYCARE_MAN = the steps that are left in this block).
            sub = []
            for s in step["daycare_wait"]["steps"]:
                self.run_step(s, sub)
            men = [i for i, s in enumerate(sub) if s["type"] == "DAYCARE_MAN"]
            if not men or sub[-1]["type"] != "DAYCARE_MAN" or \
                    any(s["type"] == "DAYCARE_MAN" and s.get("p0") != 0 for s in sub):
                raise StoryError("daycare_wait: rounds that end with the man who gives the egg (%s)"
                                 % step.get("label", "?"))
            for i in men:
                sub[i]["p1"] = len(sub) - i - 1
            out.extend(sub)
            return
        if "daycare_man" in step:
            #  In front of the Day Care man outside, who gives the egg: the
            #  player stands below the tile he has the egg at (16, 14) and
            #  faces it. His usual tile is one more up: without a text box
            #  the program takes one step up (and back before the next walk).
            #  mode "reject": an egg that is waiting is not wanted.
            b = step["daycare_man"]
            label = step.get("label", "The Day Care man")
            self.run_step({"talk": {"map": b["map"], "xy": b["xy"], "mode": "plain", "counter": False,
                                    "dirs": ["up"]}, "label": label}, out)
            if self.pos[3] != "up":
                raise StoryError("daycare_man: the player does not face up at %s (%s)" % (self.pos, label))
            ts = {"type": "DAYCARE_MAN", "label": label, "p0": 1 if b.get("mode") == "reject" else 0,
                  "p2": 1, "p3": 1, "dir": "up"}
            out.append(ts)
            if b.get("mode") == "reject":
                #  The man stays where he is until the map is loaded again:
                #  on the tile of the egg (16, 14) when an egg was waiting
                #  (it was in all 13 trips of build 49, the Ditto made the
                #  next egg while the first one was walked), on his usual
                #  tile above it when none was. The way on (to the woman)
                #  goes around both: a step up from the line was "The player
                #  does not move" in front of him.
                bx, by = b["xy"]
                blocked = self.state.extra_blocked.setdefault(b["map"], set())
                blocked.add((bx, by))
                blocked.add((bx, by - 1))
                self.engine.invalidate()
            return
        if "daycare_give" in step or "daycare_take" in step:
            #  In front of the Day Care's woman (across the counter): the two
            #  parents are given, or taken back.
            kind = "DAYCARE_GIVE" if "daycare_give" in step else "DAYCARE_TAKE"
            if not out or out[-1]["type"] != "WALK":
                raise StoryError("%s: not after the walk to the woman (%s)" % (kind.lower(), step.get("label", "?")))
            out.append({"type": kind, "label": step.get("label", kind)})
            return
        if "daycare_store" in step:
            #  In front of the PC of a Pokemon Center, facing it: the Pokemon
            #  that hatched (the party's last) goes into an empty cell.
            m = self.d.maps[self.pos[0]]
            x, y, facing = self.pos[1], self.pos[2], self.pos[3]
            if facing != "up" or not m.in_bounds(x, y - 1) or \
                    self.d.behaviors.name(m.tile(x, y - 1)[3]) != "MB_PC":
                raise StoryError("daycare_store: the player does not face a PC (%s, at %s)"
                                 % (step.get("label", "?"), self.pos))
            if not out or out[-1]["type"] != "WALK":
                raise StoryError("daycare_store: not after the walk to the PC (%s)" % step.get("label", "?"))
            out.append({"type": "DAYCARE_STORE", "label": step.get("label", "The baby into a box")})
            return
        if "egg_room" in step or "egg_back" in step:
            #  (Build 47, experimental.) In front of the PC of a Pokemon
            #  Center, facing it. EGG_ROOM: the party has six Pokemon, an egg
            #  needs room: the sixth (not one of the team) goes into an empty
            #  cell of the boxes. EGG_BACK: what the sixth place holds then
            #  (the Pokemon that hatched) goes into that cell and the one
            #  that was put away comes back.
            kind = "EGG_ROOM" if "egg_room" in step else "EGG_BACK"
            m = self.d.maps[self.pos[0]]
            x, y, facing = self.pos[1], self.pos[2], self.pos[3]
            if facing != "up" or not m.in_bounds(x, y - 1) or \
                    self.d.behaviors.name(m.tile(x, y - 1)[3]) != "MB_PC":
                raise StoryError("%s: the player does not face a PC (%s, at %s)"
                                 % (kind.lower(), step.get("label", "?"), self.pos))
            if not out or out[-1]["type"] != "WALK":
                raise StoryError("%s: not after the walk to the PC (%s)" % (kind.lower(), step.get("label", "?")))
            out.append({"type": kind, "label": step.get("label", "Room for the egg" if kind == "EGG_ROOM"
                                                          else "The hatched one into the box")})
            return
        if "box_trip" in step:
            #  In front of the PC of a Pokemon Center, facing it: a Pokemon
            #  of a box comes into the party's sixth place, the steps do
            #  something with it (a trade), and what is in that place then
            #  goes into the box at the same PC. The game is not saved in
            #  between (a nurse saves too): a reset undoes all of it.
            b = step["box_trip"]
            m = self.d.maps[self.pos[0]]
            x, y, facing = self.pos[1], self.pos[2], self.pos[3]
            if facing != "up" or not m.in_bounds(x, y - 1) or \
                    self.d.behaviors.name(m.tile(x, y - 1)[3]) != "MB_PC":
                raise StoryError("box_trip: the player does not face a PC (%s, at %s)"
                                 % (step.get("label", "?"), self.pos))
            if not out or out[-1]["type"] != "WALK":
                raise StoryError("box_trip: not after the walk to the PC (%s)" % step.get("label", "?"))
            start = (self.pos[0], self.pos[1], self.pos[2], self.pos[3])
            sub = []
            for s in b["steps"]:
                self.run_step(s, sub)
            if (self.pos[0], self.pos[1], self.pos[2], self.pos[3]) != start:
                raise StoryError("box_trip: the steps do not end in front of the PC they start at (%s: %s, not %s)"
                                 % (step.get("label", "?"), self.pos, start))
            if not sub or sub[-1]["type"] != "WALK" or any(s["type"] in ("SAVE", "HEAL", "FLY") for s in sub):
                raise StoryError("box_trip: walks only, the last one to the PC; no save, nurse or FLY (%s)"
                                 % step.get("label", "?"))
            out.append({"type": "BOX_OUT", "label": step.get("label", "Out of the box: " + b["give"]),
                        "text": b["give"], "p0": len(sub) + 1})
            out.extend(sub)
            out.append({"type": "BOX_IN", "label": b.get("in_label", "Into the box")})
            return
        if "coins" in step:
            #  The Celadon Game Corner (build 56, experimental): the walk before
            #  this step ended facing the coins' clerk (across the counter), a
            #  slot machine, or the clerk of the prizes. "buy": p1 packs of 500
            #  COINS, then p2 packs of 50; "play": p1 games with the bet of 3;
            #  "prize": the Pokemon (text) for 9999 COINS, when the case has so
            #  many. Build 57: "look" (facing the coins' clerk: the number in
            #  its window, and how many games of one COIN that asks for) and
            #  "settle" (facing a slot machine: those games).
            c = step["coins"]
            if not out or out[-1]["type"] != "WALK":
                raise StoryError("coins: not after the walk to the clerk or the machine (%s)" % step.get("label", "?"))
            modes = {"buy": 0, "play": 1, "prize": 2, "look": 3, "settle": 4}
            if c["mode"] not in modes:
                raise StoryError("coins: mode is buy, play, prize, look or settle (%s)" % step.get("label", "?"))
            if c["mode"] == "prize" and not c.get("species"):
                raise StoryError("coins: the prize needs a species (%s)" % step.get("label", "?"))
            out.append({"type": "COINS", "label": step.get("label", "Coins: " + c["mode"]), "p0": modes[c["mode"]],
                        "p1": int(c.get("p1", 0)), "p2": int(c.get("p2", 0)), "text": c.get("species", "")})
            return
        if "trade" in step:
            #  Facing someone who trades in the game (the walk before this
            #  step ended there): the species the trade gives.
            if not out or out[-1]["type"] != "WALK":
                raise StoryError("trade: not after the walk to the trader (%s)" % step.get("label", "?"))
            #  p0 = 1: what it gives holds a MAIL (taken off it in the party menu
            #  before it goes into a box).
            out.append({"type": "TRADE", "label": step.get("label", "Trade for " + step["trade"]),
                        "text": step["trade"], "p0": 1 if trade_holds_mail(self.d.root, step["trade"]) else 0})
            return
        if "got" in step:
            #  The player has this Pokemon now (a gift): one more in the
            #  program's POKeDEX table.
            if not out or out[-1]["type"] != "CUTSCENE" or out[-1].get("p4") != 1:
                raise StoryError("got: not after the scene that hands the Pokemon over (cutscene(..., gift=True)): %s"
                                 % step.get("label", step["got"]))
            out.append({"type": "GOT", "label": step.get("label", "Got " + step["got"]), "text": step["got"]})
            return
        if "smash" in step:
            #  ROCK SMASH on one rock (for the wild Pokemon under it; the
            #  rocks on the way of a walk are smashed by the walk): next to
            #  it, facing it, YES. The scene after it fights or catches
            #  what comes out.
            sm = step["smash"]
            m = self.d.map(sm["map"])
            xy = tuple(sm["xy"])
            if xy not in self.engine.view(m.id).smash_rocks:
                raise StoryError("smash: no Rock Smash rock (left) at %s %s" % (m.id, xy))
            if not self.state.abilities.get("rock_smash"):
                raise StoryError("smash: no ROCK SMASH yet (%s)" % step.get("label", "?"))
            goals_face = self.interact_goals(m, xy[0], xy[1])
            self.walk_to(set(goals_face.keys()), step.get("label", "Rock") + "_ToRock", out, face_map=goals_face)
            out.append({"type": "TALK_YES", "label": "Rock Smash"})
            out.append({"type": "WAIT", "label": "Rock Smash animation", "p0": 2500})
            self.state.smash_done.setdefault(m.id, set()).add(xy)
            self.engine.invalidate()
            return
        if "outside" in step:
            #  Out of the building, onto the first tile outdoors.
            outdoor = [mid for mid, m in self.d.maps.items() if m.map_type in Builder.FLY_MAP_TYPES]
            if self.d.maps[self.pos[0]].map_type not in Builder.FLY_MAP_TYPES:
                self.walk_to(MapGoals(outdoor), step.get("label", "Out"), out)
            return
        if "joined" in step:
            #  Steps that only follow when the scene before them took the
            #  player along (the Safari Zone's desk: the game costs 500, and
            #  with less the attendant sends the player one step back, as
            #  after a NO). The CUTSCENE gets their count (p3): the program
            #  looks where the player is after the scene, and skips them
            #  when it is still the small building. They end where the
            #  scene leaves a player who was not taken along.
            sub = []
            for s in step["joined"]["steps"]:
                self.run_step(s, sub)
            if not out or out[-1]["type"] != "CUTSCENE":
                raise StoryError("joined: not after a cutscene (%s)" % step.get("label", "?"))
            out[-1]["p3"] = len(sub)
            out.extend(sub)
            return
        if "rematch" in step:
            #  Prize money from a trainer who battles again with the VS
            #  SEEKER. The player stands next to the trainer, facing it. A
            #  walk of 100 steps or more that ends there again (the VS
            #  SEEKER charges with the steps: VsSeekerResetInBagStepCounter,
            #  100 to use it), the VS SEEKER, A on the trainer: a battle (7
            #  times in 10, Task_VsSeeker_2) or a line of text. p0: battles
            #  to win, p1: minutes at most, p2: the prize of one battle.
            r = step["rematch"]
            label = step.get("label", "Rematch")
            spot = self.pos
            if spot[3] is None:
                raise StoryError("rematch: the facing at %s is not known (%s)" % (spot, label))
            far = tuple(r["far"])
            here = tuple(spot[:3])
            sub = []
            for k in range(r["loops"]):
                self.walk_to({far}, "%s_Walk%d" % (label, k + 1), sub)
                self.walk_to({here}, "%s_Back%d" % (label, k + 1), sub, face_map={here: spot[3]})
            if any(s["type"] != "WALK" for s in sub):
                raise StoryError("rematch: the walk must be plain walking: " + label)
            legs = [s["leg"] for s in sub]
            walked = sum(run[1] for leg in legs for run in self.legs[leg]["runs"])
            if walked < 110:
                raise StoryError("rematch: the walk has %d steps, the VS SEEKER needs 100: %s" % (walked, label))
            self.pos = spot
            out.append({"type": "REMATCH", "label": label, "p0": r["wins"], "p1": r["minutes"], "p2": r["prize"],
                        "legs_to_grass": legs, "at": list(spot)})
            return
        if "seagallop" in step:
            #  On the pier's trigger in Vermilion: the sailor's text, the
            #  menus (the tickets first, on the Switch), the row p0 of the
            #  Sevii Islands list.
            out.append({"type": "SEAGALLOP", "label": step.get("label", "Seagallop"), "p0": step["seagallop"]})
            return
        if "field_move" in step:
            #  A field move from the party menu, facing "face" (Cut on the
            #  braille of the Dotted Hole: CutMoveRuinValleyCheck).
            out.append({"type": "FIELD_MOVE", "label": step.get("label", "Field move"), "p0": step["slot"],
                        "text": step["field_move"], "dir": step.get("face")})
            if step.get("face"):
                self.pos = (self.pos[0], self.pos[1], self.pos[2], step["face"])
            return
        if "stop" in step:
            out.append({"type": "STOP", "label": step.get("label", "Stop"), "text": step["stop"]})
            return
        if "require_dex" in step:
            #  Enough species for what comes next, or the program stops.
            r = step["require_dex"]
            out.append({"type": "SKIP_IF_DEX", "label": step.get("label", "Enough species"), "p0": 0,
                        "p1": r["count"], "p2": 1, "text": ",".join(r["owned"])})
            return
        if "if_left" in step:
            #  Steps only for a legendary Pokemon the program left where it
            #  was (too few balls in the bag; BATTLE with p2 & 4), by its
            #  step's label: skipped otherwise. They end where they start.
            #  What they buy and a REPEL still on at their end do not count
            #  for what comes after: skipped, there is neither.
            u = step["if_left"]
            sub = []
            start = self.pos
            items_before = dict(self.state.items)
            for s in u["steps"]:
                self.run_step(s, sub)
            if self.pos[:3] != start[:3]:
                raise StoryError("if_left: the steps end at %s, not %s" % (self.pos, start))
            for item, count in list(self.state.items.items()):
                self.state.items[item] = min(count, items_before.get(item, 0))
            self.state.repel_left = 0
            self.engine.invalidate()
            out.append({"type": "SKIP_UNLESS_LEFT", "label": step.get("label", "Left for later?"), "p0": len(sub),
                        "text": ",".join(u["labels"])})
            out.extend(sub)
            return
        if "unless_fly" in step:
            #  Steps the program skips when the Fly user knows FLY already
            #  (a save from before this was in the story). They end where
            #  they start.
            sub = []
            start = self.pos
            for s in step["unless_fly"]:
                self.run_step(s, sub)
            if self.pos[:3] != start[:3]:
                raise StoryError("unless_fly: the steps end at %s, not %s" % (self.pos, start))
            out.append({"type": "SKIP_IF_FLY", "label": step.get("label", "Fly known?"), "p0": len(sub), "p1": 1})
            out.extend(sub)
            return
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
            legs_before = len(self.legs)
            self.walk_to(goals, step.get("label", "Goto"), out,
                         end_mode="TRIGGER" if g.get("trigger") else "STOP", shortcuts=True)
            if g.get("trigger"):
                #  Mark the last emitted leg as ending on a trigger.
                pass
            if g.get("bump"):
                if len(self.legs) == legs_before:
                    raise StoryError("bump: no walk to %s" % step.get("label", "Goto"))
                self.make_bump(step.get("label", "Goto"))
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
            self.walk_to(goals, label, out, face_map=goals_face, shortcuts=True)
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
                ts = {"type": kind, "label": label}
                if t.get("alt_xy") is not None and kind in ("TALK", "TALK_YES", "TALK_NO"):
                    ts.update(self.talk_alt(m, (tx, ty), tuple(t["alt_xy"]), label))
                #  p0 & 1: the question may not come (given before). p1: the
                #  one talked to walks around, A for that many seconds.
                if t.get("given"):
                    if kind != "TALK_YES":
                        raise StoryError("talk: \"given\" is for mode \"yes\" (%s)" % label)
                    ts["p0"] = 1
                if t.get("patience"):
                    if kind not in ("TALK", "TALK_YES") or "p2" in ts:
                        raise StoryError("talk: \"patience\" is for a plain talk or a YES (%s)" % label)
                    ts["p1"] = int(t["patience"])
                out.append(ts)
            if mode == "battle":
                out.append({"type": "BATTLE", "label": label, "p0": 30000})
            if approach is not None:
                self.emit_back(approach, label, out)
            return
        if "cutscene" in step:
            c = step["cutscene"]
            cs = {"type": "CUTSCENE", "label": step.get("label", "Cutscene"), "p0": c[0], "p1": c[1],
                  "text": "YES" if step.get("answer") == "yes" else ""}
            if step.get("battles"):
                cs["p2"] = step["battles"]
            if step.get("gift"):
                #  A Pokemon is handed over in this scene: the program looks
                #  for the words of it ("received", "nickname", "transferred"),
                #  and the GOT step after it counts the Pokemon only then.
                cs["p4"] = 1
            out.append(cs)
            return
        if "battle" in step:
            b = {"type": "BATTLE", "label": step.get("label", "Battle"), "p0": step["battle"],
                 "p1": 2 if step.get("legendary") else (1 if step.get("flee") else 0)}
            #  p2: 1 = weaken it first, 4 = the story comes back for it
            #  (left where it is when the bag has too few balls).
            flags = (1 if step.get("weaken") else 0) | (4 if step.get("later") else 0)
            if flags:
                b["p2"] = flags
            out.append(b)
            return
        if "hall_of_fame" in step:
            #  The game saves itself in the Hall of Fame and starts in Pallet
            #  Town from then on (the next segment says where).
            out.append({"type": "RESET", "label": step.get("label", "Hall of Fame"), "p0": step["hall_of_fame"]})
            self.postgame = True
            self.saved_consumables = (dict(self.state.items), self.state.repel_left)
            return
        if "reload" in step:
            #  Load the save just made, the same whether the step before
            #  needed one try (Strength still on) or several (loaded again).
            #  Loading a save ends Strength (Overworld_ResetStateOnContinue),
            #  but the map's objects come back where the save has them
            #  (SpawnObjectEventsOnReturnToField): pushed boulders stay
            #  pushed, smashed rocks smashed (their temporary flags are only
            #  cleared by a warp).
            out.append({"type": "RESET", "label": step.get("label", "Reload"), "p0": 0})
            self.strength_used = None
            if self.saved_consumables is not None:
                self.state.items = dict(self.saved_consumables[0])
                self.state.repel_left = self.saved_consumables[1]
                self.engine.invalidate()
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
            self.walk_to(goals, step.get("label", "To nurse"), out, shortcuts=True)
            self.pos = (self.pos[0], self.pos[1], self.pos[2], "up")
            out.append({"type": "HEAL", "label": "Heal at " + pc.name, "at": list(self.pos)})
            return
        if "grind" in step:
            self.no_repel_here("grind")
            self.grind(step, out)
            return
        if "save" in step:
            #  "at": where the player stands (for the guide of start positions).
            out.append({"type": "SAVE", "label": "Save", "p0": 1 if step["save"].get("dex") else 0,
                        "at": list(self.pos)})
            self.saved_consumables = (dict(self.state.items), self.state.repel_left)
            return
        if "teach" in step:
            t = step["teach"]
            out.append({"type": "TEACH_HM", "label": "Teach %s" % t["move"], "p0": t["hm_rank"],
                        "p1": t["party_slot"], "p2": t.get("forget", -1), "text": t["move"]})
            return
        if "catch" in step:
            self.no_repel_here("catch")
            self.catch(step, out)
            return
        if "dex" in step:
            self.no_repel_here("dex")
            self.dex(step, out)
            return
        if "roam" in step:
            self.no_repel_here("roam")
            self.roam(step, out)
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
            out.append({"type": "TEAM", "label": step.get("label", "Team"), "p0": step["team"],
                        "at": list(self.pos)})
            return
        if "use_item" in step:
            u = step["use_item"]
            ui = {"type": "USE_ITEM", "label": step.get("label", "Use item"), "p0": u["slot"], "text": u["words"]}
            if u.get("give"):
                #  GIVE (to hold) in place of USE.
                ui["p1"] = 1
            out.append(ui)
            return
        if "buy" in step:
            b = step["buy"]
            bought = {"type": "BUY", "label": step.get("label", "Buy"), "p0": b["index"], "p1": b["quantity"]}
            #  p4: only when the money trip won this many battles (REMATCH):
            #  what they paid covers it, and the fee of the Safari Games stays.
            if b.get("wins"):
                bought["p4"] = b["wins"]
            #  p2: a healing item of the League, for the program's count of
            #  the bag (1 REVIVE, 2 FULL RESTORE or MAX POTION); p3 1: only
            #  what is missing up to the quantity.
            if b.get("kind"):
                bought["p2"] = {"revive": 1, "restore": 2}[b["kind"]]
                if b.get("up_to"):
                    bought["p3"] = 1
            out.append(bought)
            if b.get("item"):
                self.state.items[b["item"]] = self.state.items.get(b["item"], 0) + b["quantity"]
            return
        if "heal_items" in step:
            #  p0 1: after a legendary Pokemon (REVIVE and FULL RESTORE on
            #  party slots 1 and 2, whatever is known of them). 2, 3: after
            #  a room of the League again (3: before the Champion).
            modes = {"after_legendary": 1, "league_again": 2, "league_again_last": 3}
            out.append({"type": "HEAL_ITEMS", "label": step.get("label", "Heal with items"),
                        "p0": modes.get(step["heal_items"], 0)})
            return
        if "boulder" in step:
            self.boulder_puzzle(step, out)
            return
        if "switch_puzzle" in step:
            self.switch_puzzle(step, out)
            return
        if "oscillate" in step:
            #  Walk back and forth until a script starts (the Safari Zone
            #  step counter runs out). The longest straight line of plain
            #  tiles there with a wall at both ends: run along it holding
            #  the direction a little longer than the line, so each way ends
            #  bumping into the wall (no drift, many steps per second). No
            #  such line: two tiles, one step at a time.
            o = step["oscillate"]
            m = self.d.map(o["map"])
            v = self.engine.view(m.id)

            def plain(x, y):
                if not m.in_bounds(x, y):
                    return False
                t = v.tile(x, y)
                if t[1] or t[4] or (x, y) in v.blocked or (x, y) in v.sight or (x, y) in v.triggers:
                    return False
                for ddx, ddy in [(0, 0), (0, 1), (0, -1), (1, 0), (-1, 0), (0, 2), (0, -2), (2, 0), (-2, 0)]:
                    if m.warp_at(x + ddx, y + ddy)[0] is not None:
                        return False
                return True

            def wall(x, y):
                #  Collision, not a ledge or another floor: walking into it
                #  only bumps.
                if not m.in_bounds(x, y):
                    return True
                return bool(v.tile(x, y)[1]) and (x, y) not in v.triggers

            max_len = o.get("max_len", 16)
            best = None
            seen = set()
            for tx, ty in o["tiles"]:
                if not plain(tx, ty):
                    continue
                t0 = v.tile(tx, ty)

                def same(x, y):
                    if not plain(x, y):
                        return False
                    tn = v.tile(x, y)
                    return tn[2] == t0[2] and tn[3] == t0[3]

                for dname in ("right", "down"):
                    dx, dy = DIRS[dname]
                    #  The whole line through this tile (also beyond the area).
                    x, y = tx, ty
                    while same(x - dx, y - dy):
                        x, y = x - dx, y - dy
                    if (x, y, dname) in seen:
                        continue
                    seen.add((x, y, dname))
                    n = 1
                    while same(x + dx * n, y + dy * n):
                        n += 1
                    if n < 3 or n > max_len or not wall(x - dx, y - dy) or not wall(x + dx * n, y + dy * n):
                        continue
                    ends = {(m.id, x, y): dname, (m.id, x + dx * (n - 1), y + dy * (n - 1)): OPPOSITE[dname]}
                    if self.route_cost(set(ends.keys())) is None:
                        continue
                    if best is None or n > best[0]:
                        best = (n, ends)
            if best is not None:
                n, ends = best
                self.walk_to(set(ends.keys()), step.get("label", "Oscillate") + "_ToLine", out)
                here = (self.pos[0], self.pos[1], self.pos[2])
                if o.get("hatch"):
                    #  HATCH (build 47): the same run along the line with an
                    #  egg in the party, until it hatches: the steps it
                    #  wants ((cycles + 1) * 255, UpdateEggSteps / ShouldEggHatch
                    #  of the game's daycare.c) and a margin are the most
                    #  the walk goes on. text: the species that comes of it.
                    h = o["hatch"]
                    out.append({"type": "HATCH", "label": step.get("label", "Hatch"), "dir": ends[here],
                                "p0": (h["cycles"] + 1) * 255 + h.get("margin", 800), "p1": n,
                                "text": h["species"],
                                "at": [self.pos[0], self.pos[1], self.pos[2], OPPOSITE[ends[here]]]})
                    self.pos = (self.pos[0], self.pos[1], self.pos[2], OPPOSITE[ends[here]])
                    return
                if o.get("daycare"):
                    #  DAYCARE_WALK (build 48): the same run along the line,
                    #  for a number of steps (the Day Care's egg is made
                    #  every 256 steps of the parents there). The player
                    #  ends on the tile it started from.
                    out.append({"type": "DAYCARE_WALK", "label": step.get("label", "Walk"), "dir": ends[here],
                                "p0": o["daycare"]["steps"], "p1": n,
                                "at": [self.pos[0], self.pos[1], self.pos[2], OPPOSITE[ends[here]]]})
                    self.pos = (self.pos[0], self.pos[1], self.pos[2], OPPOSITE[ends[here]])
                    return
                if o.get("pace"):
                    #  PACE (build 45): the same run along the line, for a
                    #  number of steps the program counts (friendship), not
                    #  until a scene starts. The player ends on the tile it
                    #  started from, in the direction of the wall behind it.
                    out.append({"type": "PACE", "label": step.get("label", "Pace"), "dir": ends[here],
                                "p1": n, "at": [self.pos[0], self.pos[1], self.pos[2], OPPOSITE[ends[here]]]})
                    self.pos = (self.pos[0], self.pos[1], self.pos[2], OPPOSITE[ends[here]])
                    return
                out.append({"type": "OSCILLATE", "label": step.get("label", "Oscillate"), "dir": ends[here],
                            "p0": o.get("max_ms", 600000), "p1": n})
                return
            if o.get("pace") or o.get("hatch") or o.get("daycare"):
                raise StoryError("pace: no straight line with a wall at both ends on %s (%s)"
                                 % (m.id, step.get("label", "Pace")))
            spots = {}
            for x, y in o["tiles"]:
                if not plain(x, y):
                    continue
                t = v.tile(x, y)
                for dname, (dx, dy) in DIRS.items():
                    nx, ny = x + dx, y + dy
                    if not plain(nx, ny):
                        continue
                    tn = v.tile(nx, ny)
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
            out.append({"type": "HEAL", "label": "Heal at " + pc.name,
                        "at": [self.pos[0], self.pos[1], self.pos[2], "up"]})
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
            #  "raise": the Pokemon the program took out of a box for its
            #  evolution (RAISE: the level is chosen by the program).
            "type": "RAISE" if g.get("raise") else "GRIND", "label": label, "p0": g["level"],
            "p1": g.get("every", 6), "dir": osc,
            "p2": g.get("slot", -1), "text": g.get("line", ""),
            #  A team member below the level of the strongest wild Pokemon
            #  here only takes part (shared experience) until it gets there.
            "p3": self.gd.wild_max_level.get(g["map"], 0) if (g.get("slot") or g.get("raise")) else 0,
            #  The grass line walked along: this many tiles from the start;
            #  0x100: Rare Candies first.
            "p4": length | (0x100 if g.get("candies") else 0),
            "legs_to_grass": legs_to_grass, "legs_to_nurse": legs_to_nurse, "legs_back": legs_back,
            "at": list(nurse_pos),
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
        step["boulder"] = {target: [x,y], hole: bool, from: [x,y]}
        "from": the boulder that must get there (the one at that position
        when the step starts); the others may be pushed out of its way.
        Without it, any boulder.
        {open_to: [[x,y], ...]} instead of target: push boulders until the
        player can walk to one of those tiles."""
        from collections import deque
        bp = step["boulder"]
        label = step.get("label", "Boulder")
        mid = self.pos[0]
        m = self.d.maps[mid]
        open_goals = set(tuple(p) for p in bp.get("open_to", []))
        tx, ty = bp["target"] if "target" in bp else (None, None)
        hole = bool(bp.get("hole"))
        goal_origin = tuple(bp["from"]) if bp.get("from") else None
        v0 = self.engine.view(mid)
        keys = {}
        balls = set()
        for o in self.state.objects(m):
            if o["graphics"].endswith("PUSHABLE_BOULDER"):
                keys[(o["x"], o["y"])] = o["key"]
            elif o["graphics"].endswith("ITEM_BALL"):
                #  An object on the tile: a boulder is not pushed onto it
                #  (TryPushBoulder: GetCollisionAtCoords). The Sun Stone of
                #  Ruin Valley lies at the end of a dead end of boulders.
                balls.add((o["x"], o["y"]))
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
                #  (Rock Smash rocks are walked through: the walk smashes them.)
                if nn[0] != mid or ed["kind"] not in ("walk", "jump", "smash", "cut"):
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
            if (nx, ny) in cfg or (nx, ny) in v0.blocked or (nx, ny) in balls:
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
            position in 'movable' may be pushed. With goal_origin, only that
            boulder counts at the target."""
            def dist(fz, origin):
                if open_goals:
                    return 0
                return min((abs(x - tx) + abs(y - ty) for (x, y) in fz
                            if (origin.get((x, y)) == goal_origin if goal_origin else origin.get((x, y)) in movable)),
                           default=99)
            counter = 0
            heap = [(0, 0, counter, start_cfg, self.node(), [], tuple(sorted((b, b) for b in start_cfg)))]
            seen = set()
            expanded = 0
            while heap:
                _, g, _, cfg, pnode, plan, origin_map = heapq.heappop(heap)
                reg = region(cfg, pnode)
                if open_goals and any((n[1], n[2]) in open_goals for n in reg):
                    return plan
                gpos = next((p for p, o in origin_map if o == goal_origin), None) if goal_origin else None
                sig = (cfg, min((n[1], n[2], n[3]) for n in reg), gpos)
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
                        if tx is not None and nb == (tx, ty) and (goal_origin is None or origin.get(b) == goal_origin):
                            return nplan
                        if fell:
                            continue
                        no = dict(origin)
                        no[nb] = no.pop(b)
                        fz = frozenset(ncfg)
                        hh = dist(fz, no)
                        counter += 1
                        heapq.heappush(heap, (g + 1 + hh, g + 1, counter, fz, reg_xy[pxy], nplan,
                                              tuple(sorted(no.items()))))
            return None

        found = None
        if open_goals:
            found = search(set(start_cfg), 60000)
        elif goal_origin is not None:
            if goal_origin not in start_cfg:
                raise StoryError("No boulder at %s for %s on %s" % (goal_origin, label, mid))
            found = search({goal_origin}, 20000) or search(set(start_cfg), 200000)
        else:
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
            if bp.get("wait_on_button") and self.d.behaviors.name(v0.tile(nb[0], nb[1])[3]) == "MB_STRENGTH_BUTTON":
                #  A boulder that lands on a floor switch runs the switch's
                #  script (the Tanoby Key's: lockall, the screen shakes, a
                #  delay of 60 frames, and after the seventh one a message):
                #  the player cannot walk for a while. B closes a message
                #  and does nothing else.
                out.append({"type": "PRESS", "label": "%s_Switch%d" % (label, i + 1), "text": "B",
                            "p0": 1, "p1": int(bp["wait_on_button"])})
            fell = self.d.behaviors.name(v0.tile(nb[0], nb[1])[3]) == "MB_FALL_WARP"
            if fell:
                self.set_state({"objects": {mid: {key: {"hidden": True}}}})
            else:
                self.set_state({"objects": {mid: {key: {"x": nb[0], "y": nb[1]}}}})
                cur[nb] = key
            self.pos = (self.pos[0], self.pos[1], self.pos[2], d)

    def roam(self, step, out):
        """The roaming legendary: to the grass of a route, a ROAM step that
        walks back and forth there, with legs into a building next to it
        (legs_to_nurse) and back onto the first grass tile (legs_to_grass):
        each warp sends the roamer to another route
        (RoamerMoveToOtherLocationSet)."""
        r = step["roam"]
        label = step.get("label", "Roam")
        building = self.d.map(r["building"])
        goal, strips = self.strip_goal(r["map"], r["tiles"])
        self.walk_to(goal, label + "_ToGrass", out)
        grass_pos = self.pos
        if tuple(grass_pos[:3]) in strips:
            osc, length = strips[tuple(grass_pos[:3])]
        else:
            osc, length = self.osc_dir(self.grass_tiles(r["map"], r["tiles"]), label), 2
        sub = []
        self.walk_to(MapGoals([building.id]), label + "_In", sub)
        legs_in = [s["leg"] for s in sub if s["type"] == "WALK"]
        if any(s["type"] != "WALK" for s in sub):
            raise StoryError("Roam route into the building must be plain walking: " + label)
        sub = []
        self.walk_to({tuple(grass_pos[:3])}, label + "_Out", sub)
        legs_out = [s["leg"] for s in sub if s["type"] == "WALK"]
        if any(s["type"] != "WALK" for s in sub):
            raise StoryError("Roam route back to the grass must be plain walking: " + label)
        out.append({"type": "ROAM", "label": label, "text": ",".join(r["species"]),
                    "p0": r.get("per_cycle", 3), "p1": r.get("minutes", 300), "dir": osc, "p4": length,
                    "legs_to_grass": legs_out, "legs_to_nurse": legs_in})
        self.pos = grass_pos

    def dex(self, step, out):
        """For the POKeDEX: to grass (a cave) of the place, then a DEX step
        that walks back and forth there catching the species listed (the
        program skips those it caught already)."""
        c = step["dex"]
        label = step.get("label", "Dex")
        if c.get("fish") is not None:
            self.dex_fish(step, out)
            return
        if c.get("laps") is not None:
            self.dex_laps(step, out)
            return
        have = self.gd.land_slugs.get(c["map"], set())
        #  No list (the POKeDEX collection): every species of the place,
        #  both games.
        species = c["species"] if c["species"] is not None else sorted(have)
        if not species:
            raise StoryError("%s: no wild Pokemon in the grass of %s" % (label, c["map"]))
        missing = [s for s in species if s not in have]
        if missing:
            raise StoryError("%s: not in the grass of %s: %s" % (label, c["map"], missing))
        goal, strips = self.strip_goal(c["map"], c["tiles"])
        if c.get("collect") and self.route_cost(goal) is None:
            #  The longest grass lines out of reach (behind ledges, water):
            #  the shorter ones, any grass.
            for other in (set(strips.keys()), self.grass_tiles(c["map"], c["tiles"])):
                if other and self.route_cost(other) is not None:
                    goal = other
                    break
        self.walk_to(goal, label + "_ToGrass", out)
        if tuple(self.pos[:3]) in strips:
            osc, length = strips[tuple(self.pos[:3])]
        else:
            osc, length = self.osc_dir(self.grass_tiles(c["map"], c["tiles"]), label), 2
        #  The list with how often each species comes here, of 100 wild
        #  Pokemon, in FireRed and in LeafGreen ("slug:FireRed:LeafGreen", 0:
        #  not here in that game): the program looks for what comes here in
        #  its game, and does not stay for a species that is rare here and
        #  common at another place of the collection.
        share = self.gd.land_share.get(c["map"], {})
        text = ",".join("%s:%d:%d" % (slug, share[slug][0], share[slug][1]) for slug in species)
        #  p3: no catching here once the game counts that many species.
        #  p2 & 2: what the program's POKeDEX table still wants of the list
        #  (the game and starter chosen, extra ones to evolve).
        out.append({"type": "DEX", "label": label, "text": text,
                    "p0": c.get("idle", 25), "p1": c.get("minutes", 30),
                    "p2": (1 if c.get("safari") else 0) | (2 if c.get("collect") else 0),
                    "p3": c.get("enough", 0), "dir": osc, "p4": length})

    #  The rods, in the order the data counts them (a DEX step that fishes:
    #  p4; SKIP_IF_ITEM and SKIP_IF_COLLECTED: the rod + 1).
    RODS = ("OLD ROD", "GOOD ROD", "SUPER ROD")
    FISH_EDGE = 1

    def fish_spots(self, map_id, tiles):
        """Where a rod can be cast on a map: {(map, x, y): facing}. Land at
        the height the game fishes from, with water in front that could be
        surfed onto from there (FieldUseFunc_Rod -> CanFish ->
        IsPlayerFacingSurfableFishableWater: the tile in front has the
        water terrain, the way onto it is shut by its height alone, and
        the player stands at height 3). Not in the grass, not where a
        trainer looks, someone walks, a script waits or a warp is next
        door: the player stands there for the whole visit."""
        v = self.engine.view(map_id)
        m = self.d.maps[map_id]
        warps = {(w["x"], w["y"]) for w in m.warps}
        spots = {}
        #  Not on the map's last row or column either: a walk that ends a
        #  tile short would stand on the next map, which has wild Pokemon
        #  of its own (Five Isle Meadow's QWILFISH, the town's SHELLDER).
        edge = Builder.FISH_EDGE
        for x, y in tiles:
            if not (edge <= x < m.width - edge and edge <= y < m.height - edge):
                continue
            t = v.tile(x, y)
            if t[1] or t[2] != 3 or t[4] or m.terrain(x, y):
                continue
            if (x, y) in v.blocked or (x, y) in v.sight or (x, y) in v.triggers or (x, y) in v.soft:
                continue
            if (x, y) in warps or any((x + dx, y + dy) in warps for dx, dy in DIRS.values()):
                continue
            for dname in ("up", "down", "left", "right"):
                if t[3] in self.engine.blocked_dirs[dname]:
                    continue
                dx, dy = DIRS[dname]
                wx, wy = x + dx, y + dy
                #  (The water of the map itself: the wild Pokemon are those
                #  of the map the player stands on.)
                if not m.in_bounds(wx, wy) or (wx, wy) in v.blocked:
                    continue
                wt = v.tile(wx, wy)
                if wt[1] or wt[2] in (0, 3, 15) or m.terrain(wx, wy) != 2:
                    continue
                if wt[3] in self.engine.blocked_dirs[OPPOSITE[dname]]:
                    continue
                spots[(map_id, x, y)] = dname
                break
        return spots

    def dex_fish(self, step, out):
        """For the POKeDEX, with a rod: to the water of the place, then a
        DEX step that casts the rod there again and again (p2 & 4; p4 the
        rod, dir the way to the water) catching what the list has."""
        c = step["dex"]
        label = step.get("label", "Fish")
        rod = c["fish"]
        share = self.gd.fish_share.get(c["map"], {}).get(rod, {})
        species = c["species"] if c["species"] is not None else sorted(share)
        if not species:
            raise StoryError("%s: nothing bites the %s on %s" % (label, Builder.RODS[rod], c["map"]))
        missing = [s for s in species if s not in share]
        if missing:
            raise StoryError("%s: not on the %s of %s: %s" % (label, Builder.RODS[rod], c["map"], missing))
        spots = self.fish_spots(c["map"], c["tiles"])
        if not spots:
            raise StoryError("%s: no tile to fish from on %s" % (label, c["map"]))
        self.walk_to(set(spots.keys()), label + "_ToWater", out, face_map=spots)
        here = tuple(self.pos[:3])
        if here not in spots or self.pos[3] != spots[here] or self.surfing or self.node()[3] != 3:
            raise StoryError("%s: the walk to the water ends at %s (%s)" % (label, self.pos, spots.get(here)))
        text = ",".join("%s:%d:%d" % (slug, share[slug][0], share[slug][1]) for slug in species)
        step_out = {"type": "DEX", "label": label, "text": text,
                    "p0": c.get("idle", 25), "p1": c.get("minutes", 30),
                    "p2": 4 | (2 if c.get("collect") else 0) | (1 if c.get("safari") else 0),
                    "p3": c.get("enough", 0), "dir": spots[here], "p4": rod}
        if c.get("safari"):
            #  In a Safari Game (build 52): a cast costs no step and no ball
            #  unless something is caught, so the game does not end by
            #  itself. The program leaves it when the visit is over: the
            #  way out to the gate (legs_to_nurse, as ROAM's way into a
            #  building), where the attendant asks whether to leave early
            #  (FuchsiaCity_SafariZone_Entrance_EventScript_ExitEarly).
            gate = self.d.map(c["exit"])
            start = (self.pos, self.surfing)
            sub = []
            self.walk_to(MapGoals([gate.id]), label + "_Leave", sub)
            if any(s["type"] != "WALK" for s in sub) or not sub:
                raise StoryError("%s: the way out of the Safari Zone must be plain walking" % label)
            step_out["legs_to_nurse"] = [s["leg"] for s in sub]
            self.pos, self.surfing = start
        out.append(step_out)

    def dex_laps(self, step, out):
        """For the POKeDEX, on the way: a DEX step (p2 & 8) and after it
        the steps of a lap (p4: how many), which the program walks again
        and again from where the player stands, catching what the list has
        of the wild Pokemon that come on the way: over the water of the
        place ("ground": "water"), or smashing its rocks ("rocks"). The lap
        ends where it starts."""
        c = step["dex"]
        label = step.get("label", "Laps")
        ground = c.get("ground", "water")
        share = (self.gd.water_share if ground == "water" else self.gd.rock_share).get(c["map"], {})
        species = c["species"] if c["species"] is not None else sorted(share)
        if not species:
            raise StoryError("%s: no wild Pokemon on the %s of %s" % (label, ground, c["map"]))
        missing = [s for s in species if s not in share]
        if missing:
            raise StoryError("%s: not on the %s of %s: %s" % (label, ground, c["map"], missing))
        start = self.pos
        surfing = self.surfing
        sub = []
        for s in c["laps"]:
            self.run_step(s, sub)
        if self.pos[:3] != start[:3] or self.surfing != surfing:
            raise StoryError("%s: the lap ends at %s, not %s" % (label, self.pos, start))
        allowed = ("WALK", "TALK_YES", "WAIT")
        other = [s["type"] for s in sub if s["type"] not in allowed]
        if other or not sub:
            raise StoryError("%s: a lap is walks, YES answers and waits, not %s" % (label, other or "nothing"))
        if ground == "rocks":
            #  The rocks come back when the map is entered again by a warp
            #  (their flags are temporary): the lap must leave it.
            if self.state.smash_done.get(c["map"]):
                raise StoryError("%s: the lap ends with rocks still smashed on %s" % (label, c["map"]))
        text = ",".join("%s:%d:%d" % (slug, share[slug][0], share[slug][1]) for slug in species)
        out.append({"type": "DEX", "label": label, "text": text,
                    "p0": c.get("idle", 25), "p1": c.get("minutes", 30),
                    "p2": 8 | (2 if c.get("collect") else 0),
                    "p3": c.get("enough", 0), "p4": len(sub)})
        out.extend(sub)

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
            self.segment_no_repel = bool(seg.get("no_repel"))
            if self.segment_no_repel and self.state.repel_left > 0:
                #  Its walks are planned without the REPEL that is still on
                #  from the segment before (its last steps): they are walked
                #  again later, when none is.
                self.state.repel_left = 0
                self.engine.invalidate()
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
        self.mark_slow_steps()
        return self.segments

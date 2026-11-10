"""Which Pokemon one save of FireRed or LeafGreen can register as caught,
without trading with another game, for each starter.

From pret/pokefirered: wild encounters (grass and caves, surfing, fishing,
Rock Smash, Safari Zone), gifts, one-time battles (legendaries, Snorlax, ...),
the roaming legendary, Game Corner prizes, in-game trades with people in the
game, the Togepi egg, breeding at the Four Island Day Care, and evolutions
with what each needs.

python3 dex_availability.py <pokefirered> <out.json>; python3 dex_report.py <out.json> Pokedex-Availability.md
"""
import json
import os
import re
import sys

ROOT = sys.argv[1]
OUT = sys.argv[2]

VERSIONS = ["FireRed", "LeafGreen"]
STARTERS = ["BULBASAUR", "CHARMANDER", "SQUIRTLE"]
ROAMER = {"BULBASAUR": "ENTEI", "CHARMANDER": "SUICUNE", "SQUIRTLE": "RAIKOU"}

STORY, POST = 0, 1          # Before the Hall of Fame / after it.
PHASE_NAME = {STORY: "story", POST: "post-game"}


def read(path):
    return open(os.path.join(ROOT, path), encoding="utf-8").read()


# ---- Species: national dex numbers and names --------------------------------
dex_enum = re.findall(r"^\s*NATIONAL_DEX_(\w+),", read("include/constants/pokedex.h"), re.M)
NATDEX = {name: i + 1 for i, name in enumerate([n for n in dex_enum if n != "NONE"])}
NAMES = {}
for sp, nm in re.findall(r"\[SPECIES_(\w+)\]\s*=\s*_\(\"([^\"]*)\"\)", read("src/data/text/species_names.h")):
    NAMES[sp] = nm
KANTO_END = 151


def nat(sp):
    return NATDEX.get(sp, 9999)


def pretty(sp):
    n = NAMES.get(sp, sp)
    if n == "HO-OH":
        return "Ho-Oh"
    return " ".join(w[:1] + w[1:].lower() for w in n.split(" "))


# ---- Species info: egg groups and gender -----------------------------------
INFO = {}
text = read("src/data/pokemon/species_info.h")
for m in re.finditer(r"\[SPECIES_(\w+)\]\s*=\s*\{(.*?)\n    \}", text, re.S):
    body = m.group(2)
    eg = re.search(r"\.eggGroups\s*=\s*\{(EGG_GROUP_\w+),\s*(EGG_GROUP_\w+)\}", body)
    gr = re.search(r"\.genderRatio\s*=\s*([\w()]+)", body)
    INFO[m.group(1)] = {
        "egg": [eg.group(1)[10:], eg.group(2)[10:]] if eg else [],
        "gender": gr.group(1) if gr else "",
    }

# ---- Evolutions --------------------------------------------------------------
EVOS = {}       # species -> [(method, param, target)]
PRE = {}        # target -> (species, method, param)
for sp, body in re.findall(r"\[SPECIES_(\w+)\]\s*=\s*\{(\{.*?\})\},?\n", read("src/data/pokemon/evolution.h"), re.S):
    for meth, param, tgt in re.findall(r"\{(EVO_\w+),\s*(\w+),\s*SPECIES_(\w+)\}", body):
        EVOS.setdefault(sp, []).append((meth, param, tgt))
        PRE[tgt] = (sp, meth, param)

STONES = {"ITEM_FIRE_STONE": "Fire Stone", "ITEM_WATER_STONE": "Water Stone",
          "ITEM_THUNDER_STONE": "Thunder Stone", "ITEM_LEAF_STONE": "Leaf Stone",
          "ITEM_MOON_STONE": "Moon Stone", "ITEM_SUN_STONE": "Sun Stone"}
# The one Sun Stone: an item ball in Ruin Valley (Six Island), after the Hall
# of Fame. Trade items do not matter without trading.
POST_ITEMS = {"ITEM_SUN_STONE"}


def evo_need(meth, param, tgt):
    """(possible, phase, text) of one evolution in FireRed/LeafGreen."""
    post = nat(tgt) > KANTO_END   # Into a non-Kanto species: National Dex first.
    phase = POST if post else STORY
    if meth == "EVO_LEVEL":
        return True, phase, "level %s" % param
    if meth == "EVO_FRIENDSHIP":
        return True, phase, "level up with high friendship (220+)"
    if meth in ("EVO_LEVEL_ATK_GT_DEF", "EVO_LEVEL_ATK_EQ_DEF", "EVO_LEVEL_ATK_LT_DEF"):
        rel = {"EVO_LEVEL_ATK_GT_DEF": "Attack > Defense", "EVO_LEVEL_ATK_EQ_DEF": "Attack = Defense",
               "EVO_LEVEL_ATK_LT_DEF": "Attack < Defense"}[meth]
        return True, phase, "level %s with %s" % (param, rel)
    if meth == "EVO_ITEM":
        if param in STONES:
            if param in POST_ITEMS:
                return True, POST, STONES[param] + " (the only one: Ruin Valley, Six Island)"
            return True, phase, STONES[param]
        return False, phase, "%s (not in FireRed/LeafGreen)" % param[5:].replace("_", " ").title()
    if meth == "EVO_TRADE":
        return False, phase, "trade (needs another game)"
    if meth == "EVO_TRADE_ITEM":
        return False, phase, "trade holding %s (needs another game)" % param[5:].replace("_", " ").title()
    if meth in ("EVO_FRIENDSHIP_DAY", "EVO_FRIENDSHIP_NIGHT"):
        return False, phase, "friendship by %s: no clock in FireRed/LeafGreen" % ("day" if meth.endswith("DAY") else "night")
    return False, phase, meth


if "--evos" in sys.argv:
    for sp, lst in EVOS.items():
        for e in lst:
            print(sp, e, evo_need(*e))
    sys.exit(0)

# ---- Places ------------------------------------------------------------------
POST_PREFIXES = ("FOUR_ISLAND", "FIVE_ISLAND", "SIX_ISLAND", "SEVEN_ISLAND", "CERULEAN_CAVE")


def map_phase(m):
    return POST if m[4:].startswith(POST_PREFIXES) else STORY


def map_name(m):
    s = m[4:]
    s = re.sub(r"ROUTE(\d+)", r"ROUTE \1", s)
    words = s.split("_")
    out = []
    for w in words:
        if w in ("1F", "2F", "3F", "B1F", "B2F", "B3F", "B4F"):
            out.append(w)
        elif w.startswith("ROOM"):
            out.append("Room " + w[4:])
        elif w == "SSANNE":
            out.append("S.S. Anne")
        else:
            out.append(w.capitalize())
    s = " ".join(out)
    for a, b in (("Mt ", "Mt. "), ("Pokemon", "Pokémon")):
        s = s.replace(a, b)
    return s


# ---- Sources -----------------------------------------------------------------
# A source: kind, where, phase, needs (list of text), version (None: both),
# starter (None: all), requested (species given in a trade), choice (text).
SOURCES = {}   # species -> [source]


def add(sp, **src):
    src.setdefault("version", None)
    src.setdefault("starter", None)
    src.setdefault("needs", [])
    src.setdefault("choice", None)
    src.setdefault("requested", None)
    SOURCES.setdefault(sp, []).append(src)


wild = json.loads(read("src/data/wild_encounters.json"))
group = wild["wild_encounter_groups"][0]
fish_groups = None
for f in group["fields"]:
    if f["type"] == "fishing_mons":
        fish_groups = f["groups"]
for e in group["encounters"]:
    label = e["base_label"]
    version = "FireRed" if label.endswith("_FireRed") else "LeafGreen" if label.endswith("_LeafGreen") else None
    m = e["map"]
    #  Altering Cave: its other tables only by Mystery Gift.
    if "AlteringCave_" in label and re.search(r"AlteringCave_\d", label):
        continue
    where = map_name(m)
    phase = map_phase(m)
    safari = m.startswith("MAP_SAFARI_ZONE")
    base_needs = ["Safari Zone"] if safari else []
    if m.startswith("MAP_POKEMON_TOWER"):
        base_needs = ["Silph Scope"]
    if m.startswith("MAP_POWER_PLANT"):
        base_needs = ["Surf"]
    for field, method, needs in (("land_mons", "grass/cave", []), ("water_mons", "surfing", ["Surf"]),
                                 ("rock_smash_mons", "Rock Smash", ["HM06 Rock Smash"])):
        if field not in e:
            continue
        for slot in e[field]["mons"]:
            add(slot["species"][8:], kind="wild", method=method, where=where, phase=phase,
                needs=base_needs + needs, version=version,
                levels=(slot["min_level"], slot["max_level"]))
    if "fishing_mons" in e:
        mons = e["fishing_mons"]["mons"]
        for rod, idx in fish_groups.items():
            rod_name = {"old_rod": "Old Rod", "good_rod": "Good Rod", "super_rod": "Super Rod"}[rod]
            for i in idx:
                slot = mons[i]
                add(slot["species"][8:], kind="wild", method="fishing (%s)" % rod_name, where=where, phase=phase,
                    needs=base_needs + [rod_name], version=version,
                    levels=(slot["min_level"], slot["max_level"]))

# One-time battles (catchable). The Pokemon Tower's Marowak is a ghost: it
# cannot be caught.
for sp, where, phase, needs, lvl in (
    ("ARTICUNO", "Seafoam Islands B4F", STORY, ["Surf", "Strength"], 50),
    ("ZAPDOS", "Power Plant", STORY, ["Surf"], 50),
    ("MOLTRES", "Mt. Ember summit", STORY, ["Rock Smash", "Strength"], 50),
    ("MEWTWO", "Cerulean Cave B1F", POST, ["Surf", "after the Sapphire is given to Celio"], 70),
    ("SNORLAX", "Route 12", STORY, ["Poké Flute"], 30),
    ("SNORLAX", "Route 16", STORY, ["Poké Flute"], 30),
    ("ELECTRODE", "Power Plant (2 Poké Ball look-alikes)", STORY, ["Surf"], 34),
    ("HYPNO", "Berry Forest, Three Island", STORY, [], 30),
):
    add(sp, kind="static", method="one-time battle", where=where, phase=phase, needs=needs, levels=(lvl, lvl))
#  The Switch version gives the Mystic Ticket and the Aurora Ticket after the
#  Hall of Fame (on a GBA they came only from events): the ferry from
#  Vermilion goes to Navel Rock and Birth Island.
for sp, where, ticket, lvl in (("LUGIA", "Navel Rock (base)", "Mystic Ticket", 70),
                               ("HO_OH", "Navel Rock (summit)", "Mystic Ticket", 70),
                               ("DEOXYS", "Birth Island", "Aurora Ticket", 30)):
    add(sp, kind="static", method="one-time battle", where=where, phase=POST,
        needs=["%s (Switch version: given after the Hall of Fame)" % ticket], levels=(lvl, lvl))
for st, sp in ROAMER.items():
    add(sp, kind="roaming", method="roams Kanto's routes", where="Kanto (after the Sapphire is given to Celio)",
        phase=POST, needs=["Master Ball advised"], starter=st, levels=(50, 50))

# Gifts.
for st in STARTERS:
    add(st, kind="gift", method="starter", where="Professor Oak's Lab", phase=STORY, starter=st, levels=(5, 5))
add("MAGIKARP", kind="gift", method="bought (¥500)", where="Route 4 Pokémon Center", phase=STORY, levels=(5, 5))
add("EEVEE", kind="gift", method="gift", where="Celadon Condominiums roof", phase=STORY, levels=(25, 25))
add("LAPRAS", kind="gift", method="gift", where="Silph Co. 7F", phase=STORY, levels=(25, 25))
add("HITMONLEE", kind="gift", method="gift", where="Saffron Fighting Dojo", phase=STORY,
    choice="Hitmonlee or Hitmonchan at the Dojo", levels=(25, 25))
add("HITMONCHAN", kind="gift", method="gift", where="Saffron Fighting Dojo", phase=STORY,
    choice="Hitmonlee or Hitmonchan at the Dojo", levels=(25, 25))
add("OMANYTE", kind="gift", method="Helix Fossil revived", where="Cinnabar Lab", phase=STORY,
    choice="Helix Fossil or Dome Fossil at Mt. Moon", levels=(5, 5))
add("KABUTO", kind="gift", method="Dome Fossil revived", where="Cinnabar Lab", phase=STORY,
    choice="Helix Fossil or Dome Fossil at Mt. Moon", levels=(5, 5))
add("AERODACTYL", kind="gift", method="Old Amber revived", where="Cinnabar Lab", phase=STORY, levels=(5, 5))
add("TOGEPI", kind="egg", method="egg (gift)", where="Water Labyrinth, Five Island", phase=POST,
    needs=["a Pokémon with high friendship to show"], levels=(5, 5))

# Game Corner prizes (Celadon), per version.
for version, lst in (("FireRed", [("ABRA", 9), ("CLEFAIRY", 8), ("DRATINI", 18), ("SCYTHER", 25), ("PORYGON", 26)]),
                     ("LeafGreen", [("ABRA", 7), ("CLEFAIRY", 12), ("PINSIR", 18), ("DRATINI", 24), ("PORYGON", 18)])):
    for sp, lvl in lst:
        add(sp, kind="prize", method="Game Corner prize", where="Celadon Game Corner", phase=STORY,
            needs=["coins"], version=version, levels=(lvl, lvl))

# In-game trades with people in the game (no other game needed).
trades = read("src/data/ingame_trades.h")
TRADE_PLACE = {"MR_MIME": "Route 2 house", "JYNX": "Cerulean City house", "NIDORAN": "Underground Path north entrance",
               "FARFETCHD": "Vermilion City house", "NIDORINOA": "Route 11 gate 2F", "LICKITUNG": "Route 18 gate 2F",
               "ELECTRODE": "Cinnabar Lab lounge", "TANGELA": "Cinnabar Lab lounge", "SEEL": "Cinnabar Lab experiment room"}
for key, body in re.findall(r"\[INGAME_TRADE_(\w+)\]\s*=\s*\{(.*?)\n    \}", trades, re.S):
    parts = {}
    if "#if defined(FIRERED)" in body:
        fr = body.split("#if defined(FIRERED)")[1].split("#elif defined(LEAFGREEN)")
        lg = fr[1].split("#endif")[0]
        common = body.split("#if defined(FIRERED)")[0] + fr[1].split("#endif")[1] if "#endif" in fr[1] else ""
        parts["FireRed"] = body.split("#if defined(FIRERED)")[0] + fr[0] + common
        parts["LeafGreen"] = body.split("#if defined(FIRERED)")[0] + lg + common
    else:
        parts["FireRed"] = parts["LeafGreen"] = body
    for version, b in parts.items():
        sp = re.search(r"\.species\s*=\s*SPECIES_(\w+)", b).group(1)
        req = re.search(r"\.requestedSpecies\s*=\s*SPECIES_(\w+)", b).group(1)
        add(sp, kind="trade", method="in-game trade (give %s)" % pretty(req), where=TRADE_PLACE.get(key, key),
            phase=STORY, version=version, requested=req)


# ---- Breeding ----------------------------------------------------------------
def lowest(sp):
    while sp in PRE:
        sp = PRE[sp][0]
    return sp


INCENSE = {"AZURILL": ("MARILL", "Sea Incense"), "WYNAUT": ("WOBBUFFET", "Lax Incense")}


def egg_species(parent):
    """[(egg species, what the parent holds or None)]."""
    base = lowest(parent)
    #  Azurill and Wynaut only when a parent holds an incense (both in the
    #  Lost Cave, Five Island); else the egg is Marill / Wobbuffet.
    if base in INCENSE:
        return [(INCENSE[base][0], None), (base, INCENSE[base][1])]
    return [(base, None)]


def can_breed(sp):
    eg = INFO.get(sp, {}).get("egg", [])
    return bool(eg) and "UNDISCOVERED" not in eg


# ---- Closure per version and starter -----------------------------------------
def solve(version, starter):
    """species -> {"phase", "how": [text], "via"} for what this save can register."""
    got = {}

    def offer(sp, phase, how, via):
        cur = got.get(sp)
        changed = False
        if cur is None:
            got[sp] = cur = {"phase": phase, "how": [], "via": via, "direct": None, "direct_how": [], "evolve": []}
            changed = True
        elif phase < cur["phase"]:
            cur["phase"] = phase
            cur["via"] = via
            changed = True
        if via == "evolution":
            if how not in cur["evolve"]:
                cur["evolve"].append(how)
        else:
            if cur["direct"] is None or phase < cur["direct"]:
                cur["direct"] = phase
                changed = True
            if how not in cur["direct_how"]:
                cur["direct_how"].append(how)
        if how not in cur["how"]:
            cur["how"].append(how)
        return changed

    changed = True
    while changed:
        changed = False
        for sp, srcs in SOURCES.items():
            for s in srcs:
                if s["version"] not in (None, version) or s["starter"] not in (None, starter):
                    continue
                phase = s["phase"]
                if s["requested"]:
                    if s["requested"] not in got:
                        continue
                    phase = max(phase, got[s["requested"]]["phase"])
                lv = s.get("levels")
                how = "%s: %s" % (s["method"], s["where"])
                if s["needs"]:
                    how += " (needs %s)" % ", ".join(s["needs"])
                if s["choice"]:
                    how += " [choice: %s]" % s["choice"]
                changed |= offer(sp, phase, how, s["kind"])
        #  Evolutions.
        for sp in list(got):
            for meth, param, tgt in EVOS.get(sp, []):
                ok, phase, need = evo_need(meth, param, tgt)
                if not ok:
                    continue
                phase = max(phase, got[sp]["phase"])
                changed |= offer(tgt, phase, "evolve %s: %s" % (pretty(sp), need), "evolution")
        #  Breeding at the Four Island Day Care (with Ditto or a partner).
        if "DITTO" in got:
            for sp in list(got):
                if not can_breed(sp):
                    continue
                phase = max(POST, got[sp]["phase"])
                for egg, item in egg_species(sp):
                    how = "breed %s at the Four Island Day Care" % pretty(sp)
                    if item:
                        how = "breed %s holding the %s (Lost Cave) at the Four Island Day Care" % (pretty(sp), item)
                    changed |= offer(egg, phase, how, "breeding")
    return got


RESULTS = {}
for version in VERSIONS:
    for starter in STARTERS:
        RESULTS[(version, starter)] = solve(version, starter)

ALL = sorted(set(sp for r in RESULTS.values() for sp in r), key=nat)


# ---- Why the rest cannot be registered --------------------------------------
def line_of(sp):
    base = lowest(sp)
    out, todo = [], [base]
    while todo:
        s = todo.pop()
        out.append(s)
        todo += [t for _, _, t in EVOS.get(s, [])]
    return out


def why_not(sp, version, starter):
    got = RESULTS[(version, starter)]
    if sp in STARTERS or PRE.get(sp, ("",))[0] in STARTERS or lowest(sp) in STARTERS:
        return "another starter"
    other = [v for v in VERSIONS if v != version][0]
    line = line_of(sp)
    here = any(s["version"] in (None, version) for m in line for s in SOURCES.get(m, []) if s["kind"] != "trade")
    there = any(s["version"] == other for m in line for s in SOURCES.get(m, []))
    if there and not here and not any(m in got for m in line):
        return "%s only" % other
    if sp in ROAMER.values():
        return "the roaming legendary of another starter"
    if sp in PRE:
        pre, meth, param = PRE[sp]
        ok, _, need = evo_need(meth, param, sp)
        if not ok:
            return "evolves from %s by %s" % (pretty(pre), need)
        if pre not in got:
            return "its pre-evolution %s cannot be registered" % pretty(pre)
    for s in SOURCES.get(sp, []):
        if s["version"] and s["version"] != version:
            return "%s only" % s["version"]
    if sp in ("MEW", "CELEBI", "JIRACHI"):
        return "event only (Mystery Gift)"
    if nat(sp) > 251:
        return "not in FireRed/LeafGreen (Hoenn: Ruby/Sapphire/Emerald)"
    return "not in FireRed/LeafGreen (Johto: Gold/Silver/Crystal)"


out = {"versions": VERSIONS, "starters": STARTERS, "combos": {}}
for (version, starter), got in RESULTS.items():
    key = "%s_%s" % (version, starter)
    out["combos"][key] = {
        "count": len(got),
        "story": sum(1 for v in got.values() if v["phase"] == STORY),
        "species": {sp: {"dex": nat(sp), "name": pretty(sp), "phase": PHASE_NAME[v["phase"]],
                         "via": v["via"], "how": v["how"],
                         "direct": None if v["direct"] is None else PHASE_NAME[v["direct"]],
                         "direct_how": v["direct_how"], "evolve": v["evolve"]}
                    for sp, v in sorted(got.items(), key=lambda x: nat(x[0]))},
        "missing": {sp: why_not(sp, version, starter) for sp in sorted(NATDEX, key=nat) if sp not in got},
    }
out["evolutions"] = {sp: [{"to": t, "method": m, "param": p, "possible": evo_need(m, p, t)[0],
                           "need": evo_need(m, p, t)[2], "phase": PHASE_NAME[evo_need(m, p, t)[1]]}
                          for m, p, t in lst] for sp, lst in EVOS.items()}
out["names"] = {sp: pretty(sp) for sp in NATDEX}
out["dex"] = {sp: nat(sp) for sp in NATDEX}
out["sources"] = SOURCES
out["pre"] = {t: {"from": f, "method": m, "param": p} for t, (f, m, p) in PRE.items()}
json.dump(out, open(OUT, "w"), indent=1, ensure_ascii=False)
for k, v in out["combos"].items():
    print(k, v["count"], "story", v["story"])

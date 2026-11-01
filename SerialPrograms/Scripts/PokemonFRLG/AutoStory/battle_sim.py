#!/usr/bin/env python3
"""
Monte Carlo battle simulation for the Autostory's level targets.

The Autostory trains only its lead (the starter) and fights the story's big
battles with it alone. This simulates those battles with the Gen 3 rules
from the pret/pokefirered decomp (stats, damage formula, crits, accuracy,
the common move effects, trainer items) and the Autostory's own move choice
(the one in PokemonFRLG_AutoStoryBattle.cpp), against a trainer AI that
mostly picks its strongest move, finishes off when it can and heals when low.

For each fight it prints the win rate by level, and the lowest level that
wins at least the wanted share of the battles.

Usage: python3 battle_sim.py --decomp /path/to/pokefirered [--runs 2000]
"""

import argparse
import os
import random
import re
import sys

STATS = ["hp", "atk", "df", "spe", "spa", "spd"]
PHYSICAL = {"NORMAL", "FIGHTING", "FLYING", "POISON", "GROUND", "ROCK", "BUG", "GHOST", "STEEL"}

NATURES = [  # (raised, lowered); same = neutral
    ("atk", "atk"), ("atk", "df"), ("atk", "spe"), ("atk", "spa"), ("atk", "spd"),
    ("df", "atk"), ("df", "df"), ("df", "spe"), ("df", "spa"), ("df", "spd"),
    ("spe", "atk"), ("spe", "df"), ("spe", "spe"), ("spe", "spa"), ("spe", "spd"),
    ("spa", "atk"), ("spa", "df"), ("spa", "spe"), ("spa", "spa"), ("spa", "spd"),
    ("spd", "atk"), ("spd", "df"), ("spd", "spe"), ("spd", "spa"), ("spd", "spd"),
]

STAGE_RATIO = [(10, 40), (10, 35), (10, 30), (10, 25), (10, 20), (10, 15), (10, 10),
               (15, 10), (20, 10), (25, 10), (30, 10), (35, 10), (40, 10)]
ACC_RATIO = [(33, 100), (36, 100), (43, 100), (50, 100), (60, 100), (75, 100), (1, 1),
             (133, 100), (166, 100), (2, 1), (233, 100), (133, 50), (3, 1)]
CRIT_CHANCE = [16, 8, 4, 3, 2]

ITEM_HEAL = {"ITEM_POTION": 20, "ITEM_SUPER_POTION": 50, "ITEM_HYPER_POTION": 200,
             "ITEM_MAX_POTION": 9999, "ITEM_FULL_RESTORE": 9999}


# ---------------------------------------------------------------- decomp data

class Data:
    def __init__(self, root):
        self.root = root
        self.species = self._species()
        self.moves = self._moves()
        self.types = self._type_chart()
        self.learnsets = self._learnsets()
        self.evolutions = self._evolutions()
        self.parties = self._parties()
        self.trainers = self._trainers()

    def read(self, path):
        with open(os.path.join(self.root, path)) as f:
            return f.read()

    def _species(self):
        text = self.read("src/data/pokemon/species_info.h")
        out = {}
        for m in re.finditer(r"\[(SPECIES_\w+)\]\s*=\s*\n\s*\{(.*?)\n    \}", text, re.S):
            body = m.group(2)
            def f(k):
                mm = re.search(r"\." + k + r"\s*=\s*(\d+)", body)
                return int(mm.group(1)) if mm else None
            if f("baseHP") is None:
                continue
            t = re.search(r"\.types\s*=\s*\{TYPE_(\w+),\s*TYPE_(\w+)\}", body)
            out[m.group(1)] = {
                "hp": f("baseHP"), "atk": f("baseAttack"), "df": f("baseDefense"),
                "spe": f("baseSpeed"), "spa": f("baseSpAttack"), "spd": f("baseSpDefense"),
                "types": (t.group(1), t.group(2)),
            }
        return out

    def _moves(self):
        text = self.read("src/data/battle_moves.h")
        out = {}
        for m in re.finditer(r"\[(MOVE_\w+)\]\s*=\s*\{(.*?)\n    \}", text, re.S):
            body = m.group(2)
            def f(k, default=None):
                mm = re.search(r"\." + k + r"\s*=\s*([^,\n]+)", body)
                return mm.group(1).strip() if mm else default
            out[m.group(1)] = {
                "name": m.group(1),
                "effect": f("effect"),
                "power": int(f("power", "0")),
                "type": f("type", "TYPE_NORMAL")[5:],
                "accuracy": int(f("accuracy", "0")),
                "pp": int(f("pp", "0")),
                "chance": int(f("secondaryEffectChance", "0")),
                "priority": int(f("priority", "0")),
            }
        return out

    def _type_chart(self):
        text = self.read("src/battle_main.c")
        body = text[text.index("gTypeEffectiveness["):]
        body = body[:body.index("};")]
        tokens = re.findall(r"TYPE_\w+", body)
        chart = {}
        i = 0
        while i + 2 < len(tokens):
            a, d, mul = tokens[i], tokens[i + 1], tokens[i + 2]
            if a in ("TYPE_FORESIGHT", "TYPE_ENDTABLE"):
                i += 1
                continue
            value = {"TYPE_MUL_SUPER_EFFECTIVE": 2.0, "TYPE_MUL_NOT_EFFECTIVE": 0.5,
                     "TYPE_MUL_NO_EFFECT": 0.0}.get(mul)
            if value is None:
                i += 1
                continue
            chart[(a[5:], d[5:])] = value
            i += 3
        return chart

    def _learnsets(self):
        text = self.read("src/data/pokemon/level_up_learnsets.h")
        arrays = {}
        for m in re.finditer(r"static const u16 (\w+)\[\]\s*=\s*\{(.*?)\};", text, re.S):
            arrays[m.group(1)] = [(int(a), b) for a, b in re.findall(r"LEVEL_UP_MOVE\(\s*(\d+),\s*(MOVE_\w+)\)", m.group(2))]
        ptrs = self.read("src/data/pokemon/level_up_learnset_pointers.h")
        out = {}
        for sp, arr in re.findall(r"\[(SPECIES_\w+)\]\s*=\s*(\w+)", ptrs):
            out[sp] = arrays.get(arr, [])
        return out

    def _evolutions(self):
        text = self.read("src/data/pokemon/evolution.h")
        out = {}
        for sp, lvl, to in re.findall(r"\[(SPECIES_\w+)\]\s*=\s*\{\{EVO_LEVEL,\s*(\d+),\s*(SPECIES_\w+)\}", text):
            out[sp] = (int(lvl), to)
        return out

    def _parties(self):
        text = self.read("src/data/trainer_parties.h")
        out = {}
        for m in re.finditer(r"static const struct (\w+) (sParty_\w+)\[\]\s*=\s*\{(.*?)\n\};", text, re.S):
            mons = []
            for mm in re.finditer(r"\{(.*?)\n    \}", m.group(3), re.S):
                body = mm.group(1)
                iv = re.search(r"\.iv\s*=\s*(\d+)", body)
                lvl = re.search(r"\.lvl\s*=\s*(\d+)", body)
                sp = re.search(r"\.species\s*=\s*(SPECIES_\w+)", body)
                mv = re.search(r"\.moves\s*=\s*\{([^}]*)\}", body)
                if not (lvl and sp):
                    continue
                mons.append({
                    "iv": int(iv.group(1)) if iv else 0,
                    "level": int(lvl.group(1)),
                    "species": sp.group(1),
                    "moves": [x.strip() for x in mv.group(1).split(",") if x.strip() and x.strip() != "MOVE_NONE"] if mv else None,
                })
            out[m.group(2)] = mons
        return out

    def _trainers(self):
        text = self.read("src/data/trainers.h")
        out = {}
        for m in re.finditer(r"\[(TRAINER_\w+)\]\s*=\s*\{(.*?)\n    \}", text, re.S):
            body = m.group(2)
            party = re.search(r"\.party\s*=\s*\w+\((sParty_\w+)\)", body) or re.search(r"=\s*(sParty_\w+)", body)
            items = re.search(r"\.items\s*=\s*\{([^}]*)\}", body)
            out[m.group(1)] = {
                "smart": "AI_SCRIPT_TRY_TO_FAINT" in body,
                "party": party.group(1) if party else None,
                "items": [x.strip() for x in items.group(1).split(",") if x.strip() and x.strip() != "ITEM_NONE"] if items else [],
            }
        return out

    def effectiveness(self, move_type, defender_types):
        e = 1.0
        for t in set(defender_types):
            e *= self.types.get((move_type, t), 1.0)
        return e

    def default_moves(self, species, level):
        known = []
        for lvl, mv in self.learnsets.get(species, []):
            if lvl > level:
                break
            if mv in known:
                continue
            if len(known) == 4:
                known.pop(0)
            known.append(mv)
        return known


# ------------------------------------------------- the Autostory's move value

AVOID = {"EFFECT_OHKO", "EFFECT_EXPLOSION", "EFFECT_DREAM_EATER", "EFFECT_SNORE", "EFFECT_FOCUS_PUNCH",
         "EFFECT_SPIT_UP", "EFFECT_COUNTER", "EFFECT_MIRROR_COAT", "EFFECT_BIDE", "EFFECT_FUTURE_SIGHT",
         "EFFECT_BEAT_UP"}
HALF = {"EFFECT_RECHARGE", "EFFECT_TWO_TURNS_ATTACK", "EFFECT_SOLARBEAM", "EFFECT_SKY_ATTACK",
        "EFFECT_SKULL_BASH", "EFFECT_RAZOR_WIND", "EFFECT_SEMI_INVULNERABLE"}
FIXED = {"EFFECT_LEVEL_DAMAGE": 70, "EFFECT_PSYWAVE": 70, "EFFECT_DRAGON_RAGE": 45,
         "EFFECT_SONICBOOM": 25, "EFFECT_SUPER_FANG": 25}


def autostory_base_damage(move):
    e = move["effect"]
    if e in FIXED:
        return FIXED[e]
    if e in AVOID:
        return 0
    if move["power"] <= 1 and e not in ("EFFECT_FLAIL", "EFFECT_RETURN", "EFFECT_FRUSTRATION",
                                        "EFFECT_LOW_KICK", "EFFECT_PRESENT", "EFFECT_MAGNITUDE"):
        return 0
    if e == "EFFECT_FALSE_SWIPE":
        return move["power"] * 0.5
    if e in HALF:
        return move["power"] * 0.5
    if e == "EFFECT_MULTI_HIT":
        return move["power"] * 3.0
    if e in ("EFFECT_DOUBLE_HIT", "EFFECT_TWINEEDLE", "EFFECT_TRIPLE_KICK"):
        return move["power"] * 2.0
    if e in ("EFFECT_RECOIL", "EFFECT_DOUBLE_EDGE", "EFFECT_RECOIL_IF_MISS"):
        return move["power"] * 0.85
    if e in ("EFFECT_FLAIL", "EFFECT_RETURN", "EFFECT_FRUSTRATION", "EFFECT_LOW_KICK",
             "EFFECT_PRESENT", "EFFECT_MAGNITUDE"):
        return 50
    return move["power"]


def autostory_value(data, move_id, lead_types, opponent_types=None):
    move = data.moves[move_id]
    score = autostory_base_damage(move)
    if score <= 0:
        return 0
    fixed = move["effect"] in FIXED
    eff = data.effectiveness(move["type"], opponent_types) if opponent_types else 1.0
    if not fixed:
        if move["type"] in lead_types:
            score *= 1.5
        score *= eff
    elif eff == 0:
        score = 0
    if move["accuracy"] > 0:
        score *= move["accuracy"] / 100.0
    return score


def lead_moves(data, starter, level, cut_at=None):
    """The lead's moves at "level", learned the way the Autostory does."""
    species = starter
    moves = data.default_moves(starter, 5)

    def types():
        return data.species[species]["types"]

    def learn(mv, force=False):
        if mv in moves:
            return
        if len(moves) < 4:
            moves.append(mv)
            return
        values = [autostory_value(data, m, types()) for m in moves]
        worst = min(range(4), key=lambda i: values[i])
        new = autostory_value(data, mv, types())
        if force or new > values[worst] * 1.1 + 1:
            moves[worst] = mv

    for lvl in range(6, level + 1):
        for l, mv in data.learnsets.get(species, []):
            if l == lvl:
                learn(mv)
        evo = data.evolutions.get(species)
        if evo and lvl >= evo[0]:
            species = evo[1]
            for l, mv in data.learnsets.get(species, []):
                if l == lvl:
                    learn(mv)
        if cut_at is not None and lvl == cut_at:
            learn("MOVE_CUT", force=True)
    return species, moves


# ------------------------------------------------------------------ battling

class Mon:
    def __init__(self, data, species, level, moves, ivs, nature=None, evs=None):
        self.data = data
        self.species = species
        self.level = level
        base = data.species[species]
        self.types = base["types"]
        evs = evs or {s: 0 for s in STATS}
        self.stats = {}
        for s in STATS:
            v = (2 * base[s] + ivs[s] + evs[s] // 4) * level // 100
            if s == "hp":
                v += level + 10
            else:
                v += 5
                if nature and nature[0] != nature[1]:
                    if nature[0] == s:
                        v = v * 110 // 100
                    elif nature[1] == s:
                        v = v * 90 // 100
            self.stats[s] = v
        self.max_hp = self.stats["hp"]
        self.hp = self.max_hp
        self.moves = list(moves)
        self.pp = {m: data.moves[m]["pp"] for m in moves}
        self.reset_volatile()
        self.status = None      # "slp", "psn", "tox", "brn", "par", "frz"
        self.sleep = 0
        self.toxic = 0

    def reset_volatile(self):
        self.stage = {s: 0 for s in ["atk", "df", "spe", "spa", "spd", "acc", "eva"]}
        self.confused = 0
        self.flinch = False
        self.recharge = False
        self.charging = None
        self.invulnerable = False
        self.trapped = 0
        self.seeded = False
        self.crit_stage = 0
        self.protect = False
        self.reflect = 0
        self.light_screen = 0

    def stat(self, s):
        num, den = STAGE_RATIO[self.stage[s] + 6]
        v = self.stats[s] * num // den
        if s == "spe" and self.status == "par":
            v //= 4
        return v


def damage(data, att, dfn, move, rng, crit, power=None):
    power = power if power is not None else move["power"]
    physical = move["type"] in PHYSICAL
    a_s, d_s = ("atk", "df") if physical else ("spa", "spd")
    a_stage, d_stage = att.stage[a_s], dfn.stage[d_s]
    if crit:
        a_stage = max(a_stage, 0)
        d_stage = min(d_stage, 0)
    an, ad = STAGE_RATIO[a_stage + 6]
    dn, dd = STAGE_RATIO[d_stage + 6]
    attack = att.stats[a_s] * an // ad
    defense = max(1, dfn.stats[d_s] * dn // dd)
    if move["effect"] == "EFFECT_EXPLOSION":
        defense = max(1, defense // 2)
    dmg = attack * power * (2 * att.level // 5 + 2) // defense // 50
    if physical and att.status == "brn":
        dmg //= 2
    if not crit and ((physical and dfn.reflect) or (not physical and dfn.light_screen)):
        dmg //= 2
    if physical and dmg == 0:
        dmg = 1
    dmg += 2
    if crit:
        dmg *= 2
    if move["type"] in att.types:
        dmg = dmg * 15 // 10
    for t in set(dfn.types):
        mul = data.types.get((move["type"], t), 1.0)
        dmg = int(dmg * mul)
    if dmg:
        dmg = dmg * (100 - rng.randrange(16)) // 100
        dmg = max(dmg, 1)
    return dmg


def apply_status(dfn, status, rng):
    if dfn.status is not None or dfn.hp <= 0:
        return False
    if status == "par" and "ELECTRIC" in dfn.types:
        return False
    if status in ("psn", "tox") and ("POISON" in dfn.types or "STEEL" in dfn.types):
        return False
    if status == "brn" and "FIRE" in dfn.types:
        return False
    if status == "frz" and "ICE" in dfn.types:
        return False
    dfn.status = status
    if status == "slp":
        dfn.sleep = rng.randint(2, 5)
    if status == "tox":
        dfn.toxic = 1
    return True


def change_stage(mon, stat, n):
    before = mon.stage[stat]
    mon.stage[stat] = max(-6, min(6, before + n))
    return mon.stage[stat] != before


STAT_EFFECTS = {
    "ATTACK_UP": ("self", "atk", 1), "DEFENSE_UP": ("self", "df", 1), "SPEED_UP": ("self", "spe", 1),
    "SPECIAL_ATTACK_UP": ("self", "spa", 1), "SPECIAL_DEFENSE_UP": ("self", "spd", 1),
    "ACCURACY_UP": ("self", "acc", 1), "EVASION_UP": ("self", "eva", 1),
    "ATTACK_UP_2": ("self", "atk", 2), "DEFENSE_UP_2": ("self", "df", 2), "SPEED_UP_2": ("self", "spe", 2),
    "SPECIAL_ATTACK_UP_2": ("self", "spa", 2), "SPECIAL_DEFENSE_UP_2": ("self", "spd", 2),
    "EVASION_UP_2": ("self", "eva", 2), "DEFENSE_CURL": ("self", "df", 1), "MINIMIZE": ("self", "eva", 1),
    "ATTACK_DOWN": ("foe", "atk", -1), "DEFENSE_DOWN": ("foe", "df", -1), "SPEED_DOWN": ("foe", "spe", -1),
    "SPECIAL_ATTACK_DOWN": ("foe", "spa", -1), "SPECIAL_DEFENSE_DOWN": ("foe", "spd", -1),
    "ACCURACY_DOWN": ("foe", "acc", -1), "EVASION_DOWN": ("foe", "eva", -1),
    "ATTACK_DOWN_2": ("foe", "atk", -2), "DEFENSE_DOWN_2": ("foe", "df", -2), "SPEED_DOWN_2": ("foe", "spe", -2),
    "SPECIAL_DEFENSE_DOWN_2": ("foe", "spd", -2), "ACCURACY_DOWN_2": ("foe", "acc", -2),
}
HIT_SECONDARY = {
    "EFFECT_POISON_HIT": ("status", "psn"), "EFFECT_BURN_HIT": ("status", "brn"),
    "EFFECT_FREEZE_HIT": ("status", "frz"), "EFFECT_PARALYZE_HIT": ("status", "par"),
    "EFFECT_CONFUSE_HIT": ("confuse", None), "EFFECT_FLINCH_HIT": ("flinch", None),
    "EFFECT_ATTACK_DOWN_HIT": ("stage", ("atk", -1)), "EFFECT_DEFENSE_DOWN_HIT": ("stage", ("df", -1)),
    "EFFECT_SPEED_DOWN_HIT": ("stage", ("spe", -1)), "EFFECT_SPECIAL_ATTACK_DOWN_HIT": ("stage", ("spa", -1)),
    "EFFECT_SPECIAL_DEFENSE_DOWN_HIT": ("stage", ("spd", -1)), "EFFECT_ACCURACY_DOWN_HIT": ("stage", ("acc", -1)),
    "EFFECT_TWINEEDLE": ("status", "psn"), "EFFECT_THUNDER": ("status", "par"),
    "EFFECT_BLAZE_KICK": ("status", "brn"), "EFFECT_POISON_FANG": ("status", "tox"),
    "EFFECT_SECRET_POWER": ("status", "par"), "EFFECT_FLINCH_MINIMIZE_HIT": ("flinch", None),
}
NOTHING = "nothing"


def move_power(att, dfn, move, rng):
    e = move["effect"]
    if e in ("EFFECT_FLAIL",):
        r = att.hp * 48 // att.max_hp
        return 200 if r <= 1 else 150 if r <= 4 else 100 if r <= 9 else 80 if r <= 16 else 40 if r <= 32 else 20
    if e in ("EFFECT_RETURN", "EFFECT_FRUSTRATION"):
        return 70
    if e == "EFFECT_LOW_KICK":
        return 60
    if e == "EFFECT_MAGNITUDE":
        return rng.choice([10, 30, 50, 70, 70, 90, 110, 150])
    if e == "EFFECT_PRESENT":
        return rng.choice([40, 80, 120])
    if e == "EFFECT_FACADE" and att.status in ("psn", "tox", "brn", "par"):
        return move["power"] * 2
    return move["power"]


def can_move(mon, rng):
    """Status checks before a move. Returns False if the turn is lost."""
    if mon.recharge:
        mon.recharge = False
        return False
    if mon.status == "slp":
        mon.sleep -= 1
        if mon.sleep > 0:
            return False
        mon.status = None
    if mon.status == "frz":
        if rng.random() < 0.2:
            mon.status = None
        else:
            return False
    if mon.flinch:
        mon.flinch = False
        return False
    if mon.confused:
        mon.confused -= 1
        if mon.confused and rng.random() < 0.5:
            fake = {"type": "MYSTERY", "power": 40, "effect": "EFFECT_HIT"}
            attack = mon.stat("atk")
            defense = max(1, mon.stat("df"))
            dmg = attack * 40 * (2 * mon.level // 5 + 2) // defense // 50 + 2
            dmg = dmg * (100 - rng.randrange(16)) // 100
            mon.hp -= max(1, dmg)
            return False
    if mon.status == "par" and rng.random() < 0.25:
        return False
    return True


def hits(att, dfn, move, rng):
    if move["accuracy"] == 0:
        return True
    if dfn.invulnerable:
        return False
    stage = max(0, min(12, att.stage["acc"] + 6 - dfn.stage["eva"]))
    num, den = ACC_RATIO[stage]
    acc = move["accuracy"] * num // den
    return rng.randrange(100) + 1 <= acc


def use_move(data, att, dfn, move_id, rng, first):
    move = data.moves[move_id]
    e = move["effect"]
    if move_id in att.pp:
        att.pp[move_id] -= 1
    # Two-turn moves.
    if e in ("EFFECT_TWO_TURNS_ATTACK", "EFFECT_SOLARBEAM", "EFFECT_SKY_ATTACK", "EFFECT_SKULL_BASH",
             "EFFECT_RAZOR_WIND", "EFFECT_SEMI_INVULNERABLE"):
        if att.charging != move_id:
            att.charging = move_id
            att.invulnerable = e == "EFFECT_SEMI_INVULNERABLE"
            return
        att.charging = None
        att.invulnerable = False
    if dfn.protect and e not in ("EFFECT_PROTECT",):
        return
    power = move["power"]
    is_damaging = power > 1 or e in FIXED or e in ("EFFECT_FLAIL", "EFFECT_RETURN", "EFFECT_FRUSTRATION",
                                                    "EFFECT_LOW_KICK", "EFFECT_MAGNITUDE", "EFFECT_PRESENT")
    if is_damaging:
        if e == "EFFECT_OHKO":
            if att.level < dfn.level:
                return
            if rng.randrange(100) < move["accuracy"] + att.level - dfn.level and data.effectiveness(move["type"], dfn.types) > 0:
                dfn.hp = 0
            return
        if e == "EFFECT_DREAM_EATER" and dfn.status != "slp":
            return
        if not hits(att, dfn, move, rng):
            if e == "EFFECT_RECOIL_IF_MISS":
                att.hp -= att.max_hp // 2
            if e == "EFFECT_EXPLOSION":
                att.hp = 0
            return
        eff = data.effectiveness(move["type"], dfn.types)
        if e in FIXED:
            if eff == 0:
                return
            if e == "EFFECT_LEVEL_DAMAGE":
                dmg = att.level
            elif e == "EFFECT_PSYWAVE":
                dmg = max(1, att.level * rng.randint(5, 15) // 10)
            elif e == "EFFECT_DRAGON_RAGE":
                dmg = 40
            elif e == "EFFECT_SONICBOOM":
                dmg = 20
            else:
                dmg = max(1, dfn.hp // 2)
            dfn.hp -= dmg
            return
        if eff == 0:
            return
        hits_n = 1
        if e == "EFFECT_MULTI_HIT":
            hits_n = rng.choice([2, 2, 2, 3, 3, 3, 4, 5])
        elif e in ("EFFECT_DOUBLE_HIT", "EFFECT_TWINEEDLE"):
            hits_n = 2
        elif e == "EFFECT_TRIPLE_KICK":
            hits_n = 3
        total = 0
        for k in range(hits_n):
            stage = min(4, att.crit_stage + (1 if e in ("EFFECT_HIGH_CRITICAL", "EFFECT_BLAZE_KICK",
                                                         "EFFECT_SKY_ATTACK") else 0))
            crit = rng.randrange(CRIT_CHANCE[stage]) == 0
            p = move_power(att, dfn, move, rng)
            if e == "EFFECT_TRIPLE_KICK":
                p = 10 * (k + 1)
            dmg = damage(data, att, dfn, move, rng, crit, p)
            if e == "EFFECT_FALSE_SWIPE":
                dmg = min(dmg, dfn.hp - 1)
            dfn.hp -= dmg
            total += dmg
            if dfn.hp <= 0:
                break
        if e == "EFFECT_ABSORB":
            att.hp = min(att.max_hp, att.hp + max(1, total // 2))
        if e in ("EFFECT_RECOIL",):
            att.hp -= max(1, total // 4)
        if e == "EFFECT_DOUBLE_EDGE":
            att.hp -= max(1, total // 3)
        if e == "EFFECT_EXPLOSION":
            att.hp = 0
        if e == "EFFECT_RECHARGE":
            att.recharge = True
        if e == "EFFECT_TRAP":
            dfn.trapped = rng.randint(2, 5)
        if e == "EFFECT_RAPID_SPIN":
            att.trapped = 0
            att.seeded = False
        if e == "EFFECT_TRI_ATTACK" and rng.randrange(100) < 20:
            apply_status(dfn, rng.choice(["brn", "par", "frz"]), rng)
        sec = HIT_SECONDARY.get(e)
        chance = move["chance"] or (100 if e in ("EFFECT_SPEED_DOWN_HIT",) else 0)
        if sec and dfn.hp > 0 and rng.randrange(100) < (chance or 0):
            kind, arg = sec
            if kind == "status":
                apply_status(dfn, arg, rng)
            elif kind == "confuse":
                if not dfn.confused:
                    dfn.confused = rng.randint(2, 5)
            elif kind == "flinch":
                if first:
                    dfn.flinch = True
            elif kind == "stage":
                change_stage(dfn, arg[0], arg[1])
        if e == "EFFECT_SUPERPOWER":
            change_stage(att, "atk", -1)
            change_stage(att, "df", -1)
        if e == "EFFECT_OVERHEAT":
            change_stage(att, "spa", -2)
        if e == "EFFECT_ATTACK_UP_HIT" and rng.randrange(100) < (move["chance"] or 0):
            change_stage(att, "atk", 1)
        if e == "EFFECT_DEFENSE_UP_HIT" and rng.randrange(100) < (move["chance"] or 0):
            change_stage(att, "df", 1)
        return
    # Status moves.
    name = e[len("EFFECT_"):] if e else ""
    if name in STAT_EFFECTS:
        who, stat, n = STAT_EFFECTS[name]
        if who == "foe":
            if not hits(att, dfn, move, rng):
                return
            change_stage(dfn, stat, n)
        else:
            change_stage(att, stat, n)
        return
    if e in ("EFFECT_SLEEP", "EFFECT_POISON", "EFFECT_TOXIC", "EFFECT_PARALYZE", "EFFECT_WILL_O_WISP"):
        if e == "EFFECT_PARALYZE" and data.effectiveness(move["type"], dfn.types) == 0:
            return
        if not hits(att, dfn, move, rng):
            return
        apply_status(dfn, {"EFFECT_SLEEP": "slp", "EFFECT_POISON": "psn", "EFFECT_TOXIC": "tox",
                           "EFFECT_PARALYZE": "par", "EFFECT_WILL_O_WISP": "brn"}[e], rng)
        return
    if e in ("EFFECT_CONFUSE", "EFFECT_SWAGGER", "EFFECT_FLATTER"):
        if not hits(att, dfn, move, rng):
            return
        if e == "EFFECT_SWAGGER":
            change_stage(dfn, "atk", 2)
        if e == "EFFECT_FLATTER":
            change_stage(dfn, "spa", 1)
        if not dfn.confused:
            dfn.confused = rng.randint(2, 5)
        return
    if e in ("EFFECT_RESTORE_HP", "EFFECT_SOFTBOILED", "EFFECT_MORNING_SUN", "EFFECT_SYNTHESIS",
             "EFFECT_MOONLIGHT"):
        att.hp = min(att.max_hp, att.hp + att.max_hp // 2)
        return
    if e == "EFFECT_REST":
        att.hp = att.max_hp
        att.status = "slp"
        att.sleep = 3
        return
    if e == "EFFECT_LEECH_SEED":
        if "GRASS" not in dfn.types and hits(att, dfn, move, rng):
            dfn.seeded = True
        return
    if e == "EFFECT_REFLECT":
        att.reflect = 5
        return
    if e == "EFFECT_LIGHT_SCREEN":
        att.light_screen = 5
        return
    if e == "EFFECT_HAZE":
        for s in att.stage:
            att.stage[s] = 0
            dfn.stage[s] = 0
        return
    if e == "EFFECT_FOCUS_ENERGY":
        att.crit_stage = 2
        return
    if e == "EFFECT_PROTECT":
        att.protect = rng.random() < 0.5
        return
    if e == "EFFECT_BELLY_DRUM":
        if att.hp > att.max_hp // 2:
            att.hp -= att.max_hp // 2
            att.stage["atk"] = 6
        return
    if e == "EFFECT_CURSE":
        change_stage(att, "atk", 1)
        change_stage(att, "df", 1)
        change_stage(att, "spe", -1)
        return
    if e == "EFFECT_SUPER_FANG":
        return
    # Anything else: no effect in this simulation.


def end_of_turn(mon, foe):
    if mon.hp <= 0:
        return
    if mon.status == "psn" or mon.status == "brn":
        mon.hp -= max(1, mon.max_hp // 8)
    elif mon.status == "tox":
        mon.hp -= max(1, mon.max_hp * mon.toxic // 16)
        mon.toxic += 1
    if mon.seeded and mon.hp > 0:
        d = max(1, mon.max_hp // 8)
        mon.hp -= d
        if foe.hp > 0:
            foe.hp = min(foe.max_hp, foe.hp + d)
    if mon.trapped and mon.hp > 0:
        mon.trapped -= 1
        mon.hp -= max(1, mon.max_hp // 16)
    mon.protect = False
    if mon.reflect:
        mon.reflect -= 1
    if mon.light_screen:
        mon.light_screen -= 1


def est_damage(data, att, dfn, move_id):
    move = data.moves[move_id]
    e = move["effect"]
    if e in FIXED:
        if data.effectiveness(move["type"], dfn.types) == 0:
            return 0
        return {"EFFECT_LEVEL_DAMAGE": att.level, "EFFECT_PSYWAVE": att.level,
                "EFFECT_DRAGON_RAGE": 40, "EFFECT_SONICBOOM": 20}.get(e, dfn.hp // 2)
    if move["power"] <= 1 or e in ("EFFECT_DREAM_EATER",) and dfn.status != "slp":
        return 0
    if e == "EFFECT_OHKO":
        return 0
    mult = {"EFFECT_MULTI_HIT": 3, "EFFECT_DOUBLE_HIT": 2, "EFFECT_TWINEEDLE": 2}.get(e, 1)
    if e in HALF:
        mult = 0.5
    rng = random.Random(0)
    d = damage(data, att, dfn, move, rng, False)
    acc = move["accuracy"] / 100.0 if move["accuracy"] else 1.0
    return d * mult * acc


def useless(data, att, dfn, move_id):
    move = data.moves[move_id]
    e = move["effect"]
    if att.pp.get(move_id, 1) <= 0:
        return True
    name = e[len("EFFECT_"):] if e else ""
    if name in STAT_EFFECTS:
        who, stat, n = STAT_EFFECTS[name]
        mon = dfn if who == "foe" else att
        return mon.stage[stat] >= 6 if n > 0 else mon.stage[stat] <= -6 or (who == "foe" and mon.stage[stat] <= -2)
    if e in ("EFFECT_SLEEP", "EFFECT_POISON", "EFFECT_TOXIC", "EFFECT_PARALYZE", "EFFECT_WILL_O_WISP"):
        return dfn.status is not None or (e == "EFFECT_PARALYZE" and data.effectiveness(move["type"], dfn.types) == 0)
    if e in ("EFFECT_CONFUSE", "EFFECT_SWAGGER"):
        return dfn.confused > 0
    if e in ("EFFECT_RESTORE_HP", "EFFECT_SOFTBOILED", "EFFECT_MORNING_SUN", "EFFECT_SYNTHESIS",
             "EFFECT_MOONLIGHT", "EFFECT_REST"):
        return att.hp * 2 > att.max_hp
    if e == "EFFECT_LEECH_SEED":
        return dfn.seeded or "GRASS" in dfn.types
    if e == "EFFECT_REFLECT":
        return att.reflect > 0
    if e == "EFFECT_LIGHT_SCREEN":
        return att.light_screen > 0
    if e == "EFFECT_FOCUS_ENERGY":
        return att.crit_stage > 0
    if move["power"] > 1 and data.effectiveness(move["type"], dfn.types) == 0:
        return True
    if move["power"] <= 1 and e not in FIXED and e not in (
            "EFFECT_HAZE", "EFFECT_PROTECT", "EFFECT_BELLY_DRUM", "EFFECT_CURSE", "EFFECT_FLATTER") and \
            name not in STAT_EFFECTS:
        return True     # not simulated: the AI would rarely pick it
    return False


#  Moves the Gen 3 AI leaves out when it looks for its most powerful move
#  (sDiscouragedPowerfulMoveEffects), and never uses to finish off.
DISCOURAGED = {"EFFECT_EXPLOSION", "EFFECT_DREAM_EATER", "EFFECT_RAZOR_WIND", "EFFECT_SKY_ATTACK",
               "EFFECT_RECHARGE", "EFFECT_SKULL_BASH", "EFFECT_SOLARBEAM", "EFFECT_SPIT_UP",
               "EFFECT_FOCUS_PUNCH", "EFFECT_SUPERPOWER", "EFFECT_ERUPTION", "EFFECT_OVERHEAT"}
HEALING = {"EFFECT_RESTORE_HP", "EFFECT_SOFTBOILED", "EFFECT_MORNING_SUN", "EFFECT_SYNTHESIS",
           "EFFECT_MOONLIGHT", "EFFECT_REST"}


def trainer_choice(data, me, foe, rng, smart=True):
    """The trainer AI, roughly: AI_SCRIPT_CHECK_BAD_MOVE leaves out moves that
    would do nothing; AI_SCRIPT_TRY_TO_FAINT ("smart") favors the most
    powerful move (+4 when it can faint the target, -1 for the others);
    AI_SCRIPT_CHECK_VIABILITY adds some randomness (healing when low,
    exploding when low)."""
    options = [m for m in me.moves if not useless(data, me, foe, m)]
    if not options:
        return me.moves[0]
    if not smart:
        return rng.choice(options)
    healing = [m for m in options if data.moves[m]["effect"] in HEALING]
    if healing and me.hp * 2 <= me.max_hp and rng.random() < 0.7:
        return rng.choice(healing)
    dmg = {m: est_damage(data, me, foe, m) for m in options}
    strong = [m for m in options if dmg[m] > 0 and data.moves[m]["effect"] not in DISCOURAGED]
    if strong:
        best = max(strong, key=lambda m: dmg[m])
        if dmg[best] >= foe.hp:
            return best
        if rng.random() < 0.8:
            return best
    boom = [m for m in options if data.moves[m]["effect"] == "EFFECT_EXPLOSION"]
    if boom and me.hp * 10 < me.max_hp * 3 and rng.random() < 0.5:
        return boom[0]
    rest = [m for m in options if data.moves[m]["effect"] != "EFFECT_EXPLOSION"]
    return rng.choice(rest or options)


def lead_choice(data, me, foe):
    best, best_score = None, -1
    for m in me.moves:
        if me.pp.get(m, 0) <= 0:
            continue
        s = autostory_value(data, m, me.types, foe.types)
        if s > best_score:
            best, best_score = m, s
    return best or "MOVE_STRUGGLE"


def battle(data, lead, party, items, rng, max_turns=200, smart=True):
    """lead against the trainer's party. True if the lead wins."""
    items = list(items)
    for spec in party:
        foe = spec
        lead.reset_volatile()
        for turn in range(max_turns):
            # Trainer items: heal when low.
            foe_move = None
            heal_item = None
            for it in items:
                amount = ITEM_HEAL.get(it)
                if amount and (foe.hp * 4 < foe.max_hp or foe.max_hp - foe.hp > amount) and foe.hp > 0:
                    heal_item = it
                    break
            if heal_item is None:
                foe_move = foe.charging or trainer_choice(data, foe, lead, rng, smart)
            lead_move = lead.charging or lead_choice(data, lead, foe)
            if lead_move == "MOVE_STRUGGLE":
                lead_move = lead.moves[0]
            # Order.
            lp = data.moves[lead_move]["priority"]
            fp = data.moves[foe_move]["priority"] if foe_move else 6
            if fp != lp:
                lead_first = lp > fp
            else:
                ls, fs = lead.stat("spe"), foe.stat("spe")
                lead_first = ls > fs or (ls == fs and rng.random() < 0.5)
            if heal_item:
                items.remove(heal_item)
                foe.hp = foe.max_hp if ITEM_HEAL[heal_item] > 1000 else min(foe.max_hp, foe.hp + ITEM_HEAL[heal_item])
                if heal_item == "ITEM_FULL_RESTORE":
                    foe.status = None
                    foe.confused = 0
                lead_first = True
            order = [(lead, foe, lead_move), (foe, lead, foe_move)] if lead_first else \
                [(foe, lead, foe_move), (lead, foe, lead_move)]
            for k, (a, d, mv) in enumerate(order):
                if mv is None or a.hp <= 0 or d.hp <= 0:
                    continue
                if can_move(a, rng):
                    use_move(data, a, d, mv, rng, k == 0)
                if a.hp <= 0 or d.hp <= 0:
                    break
            if lead.hp > 0:
                end_of_turn(lead, foe)
            if foe.hp > 0:
                end_of_turn(foe, lead)
            lead.flinch = foe.flinch = False
            if lead.hp <= 0:
                return False
            if foe.hp <= 0:
                break
        else:
            return False
    return True


def random_ivs(rng):
    return {s: rng.randint(0, 31) for s in STATS}


def trainer_mon(data, spec):
    iv = spec["iv"] * 31 // 255
    moves = spec["moves"] or data.default_moves(spec["species"], spec["level"])
    return Mon(data, spec["species"], spec["level"], moves, {s: iv for s in STATS})


# ------------------------------------------------------------- the fights

RIVAL = {"CHARMANDER": "SQUIRTLE", "BULBASAUR": "CHARMANDER", "SQUIRTLE": "BULBASAUR"}

#  The story's big fights, in order, with the level target that covers them
#  (the last training before the fight).
FIGHTS = [
    ("BROCK", "Brock", "TRAINER_LEADER_BROCK"),
    ("RIVAL2", "Rival (Cerulean)", "TRAINER_RIVAL_CERULEAN_{rival}"),
    ("MISTY", "Misty", "TRAINER_LEADER_MISTY"),
    ("MISTY", "Rival (S.S. Anne)", "TRAINER_RIVAL_SS_ANNE_{rival}"),
    ("SURGE", "Lt. Surge", "TRAINER_LEADER_LT_SURGE"),
    ("ERIKA", "Erika", "TRAINER_LEADER_ERIKA"),
    ("ERIKA", "Giovanni (Hideout)", "TRAINER_BOSS_GIOVANNI"),
    ("ERIKA", "Rival (Pokemon Tower)", "TRAINER_RIVAL_POKEMON_TOWER_{rival}"),
    ("KOGA", "Koga", "TRAINER_LEADER_KOGA"),
    ("KOGA", "Rival (Silph Co.)", "TRAINER_RIVAL_SILPH_{rival}"),
    ("KOGA", "Giovanni (Silph Co.)", "TRAINER_BOSS_GIOVANNI_2"),
    ("SABRINA", "Sabrina", "TRAINER_LEADER_SABRINA"),
    ("BLAINE", "Blaine", "TRAINER_LEADER_BLAINE"),
    ("GIOVANNI", "Giovanni (Gym)", "TRAINER_LEADER_GIOVANNI"),
    ("GIOVANNI", "Rival (Route 22)", "TRAINER_RIVAL_ROUTE22_LATE_{rival}"),
    ("LEAGUE", "Lorelei", "TRAINER_ELITE_FOUR_LORELEI"),
    ("LEAGUE", "Bruno", "TRAINER_ELITE_FOUR_BRUNO"),
    ("LEAGUE", "Agatha", "TRAINER_ELITE_FOUR_AGATHA"),
    ("LEAGUE", "Lance", "TRAINER_ELITE_FOUR_LANCE"),
    ("LEAGUE", "Champion", "TRAINER_CHAMPION_FIRST_{rival}"),
]
CUT_AT_KEY = "SURGE"    # Cut is taught right before the Lt. Surge training


def win_rate(data, starter, level, trainer, runs, seed, cut):
    t = data.trainers[trainer]
    party_specs = data.parties[t["party"]]
    species, moves = lead_moves(data, "SPECIES_" + starter, level, cut_at=level if cut else None)
    rng = random.Random(seed)
    wins = 0
    for _ in range(runs):
        lead = Mon(data, species, level, moves, random_ivs(rng), rng.choice(NATURES))
        party = [trainer_mon(data, p) for p in party_specs]
        if battle(data, lead, party, t["items"], rng, smart=t["smart"]):
            wins += 1
    return wins / runs, species, moves


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decomp", required=True)
    ap.add_argument("--runs", type=int, default=1000)
    ap.add_argument("--want", type=float, default=0.9)
    ap.add_argument("--starter", default=None)
    ap.add_argument("--fight", default=None)
    args = ap.parse_args()
    data = Data(args.decomp)
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from story_spec import LEVELS

    keys_order = [k for k, _, _ in FIGHTS]
    for starter in ["CHARMANDER", "BULBASAUR", "SQUIRTLE"]:
        if args.starter and args.starter != starter:
            continue
        print("=" * 70)
        print(starter)
        for key, label, trainer in FIGHTS:
            if args.fight and args.fight.lower() not in label.lower():
                continue
            trainer = trainer.format(rival=RIVAL[starter])
            planned = LEVELS[key][starter]
            cut = keys_order.index(key) >= keys_order.index(CUT_AT_KEY)
            row = []
            need = None
            for level in range(max(5, planned - 4), min(100, planned + 21)):
                rate, species, moves = win_rate(data, starter, level, trainer, args.runs, level, cut)
                row.append((level, rate))
                if need is None and rate >= args.want:
                    need = level
                if rate >= 0.99 and level >= planned:
                    break
            party = data.parties[data.trainers[trainer]["party"]]
            foes = ", ".join("%s %d" % (p["species"][8:].title(), p["level"]) for p in party)
            print("%-22s planned %2d  needed %s   [%s]" % (label, planned, need, foes))
            print("    " + "  ".join("%d:%d%%" % (l, round(r * 100)) for l, r in row))
            sp, mv = lead_moves(data, "SPECIES_" + starter, need or planned, cut_at=(need or planned) if cut else None)
            print("    at %s: %s %s" % (need or planned, sp[8:].title(), ", ".join(m[5:].title() for m in mv)))


if __name__ == "__main__":
    main()

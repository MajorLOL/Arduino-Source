#!/usr/bin/env python3
"""
Move audit: at every training spot of the Charmander story, the wild
Pokemon there (by how often they appear), which move the Autostory picks
against each of them, and which one does the most damage (the Gen 3
damage formula of battle_sim.py). Then the C++ rule (damage_estimate() in
PokemonFRLG_AutoStoryBattle.cpp): how often its pick is not the best, and
how often the lead comes in because the member needs 4 turns or more.

The moves of each Pokemon come from team_sim.py (learned the old way:
power and type only, so a Voltorb forgets SONICBOOM for SWIFT).

Usage: python3 move_audit.py --decomp /path/to/pokefirered
"""
import argparse, os, sys, json, math, collections
import battle_sim as B
import team_sim as T

ap = argparse.ArgumentParser()
ap.add_argument("--decomp", required=True)
args = ap.parse_args()
data = B.Data(args.decomp)
W = json.load(open(os.path.join(args.decomp, 'src/data/wild_encounters.json')))
group = [g for g in W['wild_encounter_groups'] if g.get('for_maps')][0]
RATES = [20, 20, 10, 10, 10, 10, 5, 5, 4, 4, 1, 1]
def wild(map_id):
    for e in group['encounters']:
        if e.get('map') == map_id and 'FireRed' in e.get('base_label', '') or (e.get('map') == map_id and 'LeafGreen' not in e.get('base_label', '')):
            if e.get('map') == map_id and 'land_mons' in e:
                c = collections.Counter()
                for r, m in zip(RATES, e['land_mons']['mons']):
                    c[(m['species'], (m['min_level'] + m['max_level']) // 2)] += r
                return c
    return None

IV = {s: 15 for s in B.STATS}
EV = {s: 60 for s in B.STATS}
def mon(species, level, moves, ours=True):
    return B.Mon(data, species, level, moves, IV, None, EV if ours else None)

def vaporeon(level):
    mv = data.default_moves("SPECIES_EEVEE", 25)
    mv[0] = "MOVE_WATER_PULSE"
    return T.grow(data, "SPECIES_VAPOREON", 25, level, mv)

def trained(name, level):
    if name == "LEAD":
        return T.candidate(data, "STARTER", level, "CHARMANDER")
    if name == "VOLTORB":
        return T.candidate(data, "ELECTRODE", level)
    if name == "EEVEE":
        return vaporeon(level)
    if name == "LAPRAS":
        return T.candidate(data, "LAPRAS", level)

SPOTS = [  # (key, map, [(who, from, to)])
    ("BROCK", "MAP_ROUTE2", [("LEAD", 9, 18)]),
    ("RIVAL2", "MAP_ROUTE4", [("LEAD", 18, 25)]),
    ("MISTY", "MAP_ROUTE24", [("LEAD", 25, 34)]),
    ("ERIKA", "MAP_ROUTE7", [("VOLTORB", 16, 25), ("EEVEE", 25, 30), ("LEAD", 34, 36)]),
    ("KOGA", "MAP_ROUTE15", [("VOLTORB", 25, 33), ("EEVEE", 30, 35), ("LEAD", 36, 50)]),
    ("SABRINA", "MAP_ROUTE7", [("LAPRAS", 25, 32), ("LEAD", 50, 50)]),
    ("BLAINE", "MAP_POKEMON_MANSION_1F", [("VOLTORB", 33, 42), ("EEVEE", 35, 42), ("LAPRAS", 32, 42), ("LEAD", 50, 52)]),
    ("GIOVANNI", "MAP_POKEMON_MANSION_1F", [("VOLTORB", 42, 47), ("EEVEE", 42, 47), ("LAPRAS", 42, 47), ("LEAD", 52, 66)]),
    ("LEAGUE", "MAP_VICTORY_ROAD_2F", [("VOLTORB", 47, 53), ("EEVEE", 47, 53), ("LAPRAS", 47, 53), ("LEAD", 66, 74)]),
]

def short(m): return m.replace("MOVE_", "")
def sp(s): return s.replace("SPECIES_", "")

for key, map_id, who_list in SPOTS:
    enc = wild(map_id)
    if not enc:
        print(key, map_id, "no land encounters"); continue
    total = sum(enc.values())
    print("=" * 100)
    print("%s  %s  wild: %s" % (key, map_id, ", ".join("%s %d (%d%%)" % (sp(s), l, r * 100 // total) for (s, l), r in enc.most_common())))
    for who, lo, hi in who_list:
        level = (lo + hi) // 2
        species, moves = trained(who, level)
        me = mon(species, level, moves)
        print("  %-8s %s L%d  moves: %s" % (who, sp(species), level, " / ".join(short(m) for m in moves)))
        t_choice = t_best = 0.0
        rows = []
        for (fs, fl), r in enc.items():
            foe = mon(fs, fl, data.default_moves(fs, fl), ours=False)
            est = {m: B.est_damage(data, me, foe, m) for m in moves}
            choice = max(moves, key=lambda m: B.autostory_value(data, m, me.types, foe.types))
            best = max(moves, key=lambda m: est[m])
            tc = math.ceil(foe.max_hp / est[choice]) if est[choice] > 0 else 99
            tb = math.ceil(foe.max_hp / est[best]) if est[best] > 0 else 99
            t_choice += r * min(tc, 20); t_best += r * min(tb, 20)
            rows.append((r, fs, fl, choice, best, tc, tb, foe.max_hp, est))
        print("     average turns to KO: picked %.1f, best %.1f" % (t_choice / total, t_best / total))
        for r, fs, fl, choice, best, tc, tb, hp, est in sorted(rows, key=lambda x: -x[0]):
            flag = ""
            if tc > tb: flag += "  <-- better: %s" % short(best)
            if tb >= 4: flag += "  [slow: %d turns]" % tb
            print("     %3d%% %-10s L%-2d HP%3d  picks %-12s %2d turns (%s)%s" % (
                r * 100 // total, sp(fs), fl, hp, short(choice), tc,
                ", ".join("%s %d" % (short(m), est[m]) for m in moves), flag))

# ---------------------------------------------------------------- the new C++ rule
print()
print("#" * 100)
print("New rule (C++ damage_estimate: IVs 15, no EVs, foe at the member's level)")
FIX = {"EFFECT_SONICBOOM": 20, "EFFECT_DRAGON_RAGE": 40, "EFFECT_LEVEL_DAMAGE": None}
def st(b, L): return (2 * b + 15) * L // 100 + 5
def hpv(b, L): return (2 * b + 15) * L // 100 + L + 10
def cpp_est(me_sp, L, move, foe_sp):
    mv = data.moves[move]; e = mv["effect"]
    eff = data.effectiveness(mv["type"], data.species[foe_sp]["types"])
    if e in FIX:
        d = 0 if eff == 0 else (FIX[e] if FIX[e] else L)
    else:
        base = B.autostory_base_damage(mv)
        if base <= 0: return 0
        phys = mv["type"] in B.PHYSICAL
        A = st(data.species[me_sp]["atk" if phys else "spa"], L)
        D = st(data.species[foe_sp]["df" if phys else "spd"], L)
        d = ((2.0 * L / 5 + 2) * base * A / D) / 50 + 2
        if mv["type"] in data.species[me_sp]["types"]: d *= 1.5
        d *= eff
    if mv["accuracy"]: d *= mv["accuracy"] / 100.0
    return d
for key, map_id, who_list in SPOTS:
    enc = wild(map_id); total = sum(enc.values())
    for who, lo, hi in who_list:
        if who == "LEAD": continue
        level = (lo + hi) // 2
        species, moves = trained(who, level)
        me = mon(species, level, moves)
        worse = 0; shared = 0; t_new = 0
        for (fs, fl), r in enc.items():
            foe = mon(fs, fl, data.default_moves(fs, fl), ours=False)
            est = {m: B.est_damage(data, me, foe, m) for m in moves}
            new = max(moves, key=lambda m: cpp_est(species, level, m, fs))
            best = max(moves, key=lambda m: est[m])
            tb = math.ceil(foe.max_hp / est[best]) if est[best] > 0 else 99
            tn = math.ceil(foe.max_hp / est[new]) if est[new] > 0 else 99
            turns = max(cpp_est(species, level, m, fs) for m in moves)
            turns = 99 if turns <= 0 else math.ceil(hpv(data.species[fs]["hp"], level) / turns)
            if tn > tb: worse += r
            if turns >= 4: shared += r
            t_new += r * min(tn, 20)
        print("%-9s %-8s L%d: new pick worse than best in %d%%, lead comes in for %d%% of the battles, avg turns %.1f" % (
            key, who, level, worse * 100 // total, shared * 100 // total, t_new / total))

"""
Team battles for the end of the game (Elite Four, Champion), built on
battle_sim.py: which Pokemon to catch next to the starter, and how far to
train them.

The team's battle policy is what the Autostory can do in FRLG:
  - before the battle, the best answer to the trainer's first Pokemon leads
    (the party is reordered in the overworld);
  - when the trainer is about to send the next Pokemon, the game asks
    "Will PLAYER change POKeMON?" and names it: switch to the best answer for
    free;
  - when one of ours faints, the best answer comes in.
Each member uses its strongest move (the Autostory's move value).

The trainer AI is battle_sim's (no switching, items when low).
"""

import argparse
import itertools
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import battle_sim as B


#  Stone evolutions (battle_sim reads only the level ones).
STONE = {
    "SPECIES_PIKACHU": "SPECIES_RAICHU",
    "SPECIES_EEVEE_THUNDER": "SPECIES_JOLTEON",
    "SPECIES_EEVEE_WATER": "SPECIES_VAPOREON",
    "SPECIES_EEVEE_FIRE": "SPECIES_FLAREON",
    "SPECIES_GLOOM": "SPECIES_VILEPLUME",
    "SPECIES_EXEGGCUTE": "SPECIES_EXEGGUTOR",
    "SPECIES_GROWLITHE": "SPECIES_ARCANINE",
}


def grow(data, species, start_level, level, start_moves=None, teach=(), stone_at=None, stone_to=None,
         no_level_evo=False):
    """Species and moves at "level" for a Pokemon that joined at start_level,
    learning moves the way the Autostory does. teach = [(level, move)]
    (TMs and HMs, forced in)."""
    moves = list(start_moves) if start_moves else data.default_moves(species, start_level)

    def learn(mv, force=False):
        if mv in moves:
            return
        if len(moves) < 4:
            moves.append(mv)
            return
        types = data.species[species]["types"]
        values = [B.autostory_value(data, m, types) for m in moves]
        worst = min(range(4), key=lambda i: values[i])
        new = B.autostory_value(data, mv, types)
        if force or new > values[worst] * 1.1 + 1:
            moves[worst] = mv

    for lvl in range(start_level + 1, level + 1):
        for l, mv in data.learnsets.get(species, []):
            if l == lvl:
                learn(mv)
        evo = None if no_level_evo else data.evolutions.get(species)
        if evo and lvl >= evo[0]:
            species = evo[1]
            for l, mv in data.learnsets.get(species, []):
                if l == lvl:
                    learn(mv)
        if stone_at is not None and lvl == stone_at:
            species = stone_to
        for l, mv in teach:
            if l == lvl:
                learn(mv, force=True)
    for l, mv in teach:
        if l <= start_level:
            learn(mv, force=True)
    return species, moves


#  Ways to get each candidate: (where, how, base species, level got,
#  evolution handling). "level got" is when it joins.
def candidate(data, name, level, starter=None):
    """Species and moves of a candidate at "level"."""
    if name == "STARTER":
        sp, mv = B.lead_moves(data, "SPECIES_" + starter, level, cut_at=30)
        return sp, mv
    if name == "LAPRAS":
        #  Silph Co. gift at 25; Surf over slot 1, Strength over slot 4.
        mv = data.default_moves("SPECIES_LAPRAS", 25)
        mv[0] = "MOVE_SURF"
        if len(mv) >= 4:
            mv[3] = "MOVE_STRENGTH"
        else:
            mv.append("MOVE_STRENGTH")
        return grow(data, "SPECIES_LAPRAS", 25, level, mv)
    if name == "RAICHU":
        #  Pikachu (Viridian Forest), Thunderbolt at 26, then the Thunder Stone.
        return grow(data, "SPECIES_PIKACHU", 5, level, stone_at=27, stone_to="SPECIES_RAICHU")
    if name == "JOLTEON":
        #  Eevee (Celadon gift, 25) + Thunder Stone, Shock Wave (TM34).
        return grow(data, "SPECIES_JOLTEON", 25, level, data.default_moves("SPECIES_EEVEE", 25),
                    teach=[(25, "MOVE_SHOCK_WAVE")])
    if name == "FLAREON":
        return grow(data, "SPECIES_FLAREON", 25, level, data.default_moves("SPECIES_EEVEE", 25))
    if name == "VAPOREON":
        return grow(data, "SPECIES_VAPOREON", 25, level, data.default_moves("SPECIES_EEVEE", 25))
    if name == "ELECTRODE":
        return grow(data, "SPECIES_VOLTORB", 16, level)
    if name == "KADABRA":
        return grow(data, "SPECIES_ABRA", 10, level)
    if name == "MACHOKE_RT":
        return grow(data, "SPECIES_MACHOP", 17, level)
    if name == "MACHOKE_VR":
        return grow(data, "SPECIES_MACHOKE", 46, level)
    if name == "MAROWAK_VR":
        return grow(data, "SPECIES_MAROWAK", 46, level)
    if name == "ONIX_VR":
        return grow(data, "SPECIES_ONIX", 46, level)
    if name == "GOLBAT_VR":
        return grow(data, "SPECIES_GOLBAT", 44, level)
    if name == "PRIMEAPE":
        return grow(data, "SPECIES_PRIMEAPE", 42, level)
    if name == "FEAROW":
        return grow(data, "SPECIES_FEAROW", 42, level)
    if name == "ARBOK":
        return grow(data, "SPECIES_ARBOK", 44, level)
    if name == "VILEPLUME":
        return grow(data, "SPECIES_ODDISH", 14, level, stone_at=32, stone_to="SPECIES_VILEPLUME")
    if name == "GYARADOS":
        return grow(data, "SPECIES_MAGIKARP", 5, level)
    if name == "MAROWAK_T":
        return grow(data, "SPECIES_CUBONE", 17, level)
    if name == "HAUNTER":
        return grow(data, "SPECIES_GASTLY", 18, level)
    if name == "GRAVELER":
        return grow(data, "SPECIES_GEODUDE", 17, level)
    if name == "DUGTRIO":
        return grow(data, "SPECIES_DUGTRIO", 30, level)
    if name == "ARCANINE":
        return grow(data, "SPECIES_GROWLITHE", 18, level, stone_at=35, stone_to="SPECIES_ARCANINE")
    raise KeyError(name)


#  Effort values of our Pokemon: hundreds of wild battles on the way give
#  each about this much per stat (510 in all at most).
EVS = 60


def make(data, name, level, rng, starter=None):
    sp, mv = candidate(data, name, level, starter)
    return B.Mon(data, sp, level, mv, B.random_ivs(rng), rng.choice(B.NATURES), {s: EVS for s in B.STATS})


def matchup(data, me, foe):
    """Higher is better for "me" against "foe": the share of the foe's HP we
    take per turn, minus the share of ours it takes."""
    mine = max([B.est_damage(data, me, foe, m) for m in me.moves if me.pp.get(m, 1) > 0] or [0])
    theirs = max([B.est_damage(data, foe, me, m) for m in foe.moves] or [0])
    faster = me.stat("spe") >= foe.stat("spe")
    turns_me = foe.hp / mine if mine > 0 else 99
    turns_foe = me.hp / theirs if theirs > 0 else 99
    #  Who goes down first, and by how much.
    return (turns_foe + (0.5 if faster else 0)) - turns_me


def pick(data, team, foe, exclude=None):
    alive = [m for m in team if m.hp > 0 and m is not exclude]
    if not alive:
        return None
    return max(alive, key=lambda m: matchup(data, m, foe))


def team_battle(data, team, foes, items, rng, smart_ai=True, policy="smart", max_turns=300, heals=None):
    """heals: the player's healing items (shared, used up), for the one out
    when its HP is below a third (battle_sim.player_item())."""
    items = list(items)
    heals = heals if heals is not None else {}
    if policy == "smart":
        cur = pick(data, team, foes[0])
    else:
        cur = team[0]
    for i, foe in enumerate(foes):
        if i > 0 and policy == "smart":
            #  "Will PLAYER change POKeMON?": free switch.
            best = pick(data, team, foe)
            if best is not None and best is not cur and matchup(data, best, foe) > matchup(data, cur, foe) + 0.5:
                cur = best
        cur.reset_volatile()
        for turn in range(max_turns):
            foe_move = None
            heal_item = None
            for it in items:
                amount = B.ITEM_HEAL.get(it)
                if amount and foe.hp * 4 < foe.max_hp and foe.hp > 0:
                    heal_item = it
                    break
            if heal_item is None:
                foe_move = foe.charging or B.trainer_choice(data, foe, cur, rng, smart_ai)
            my_item = None if cur.charging or cur.recharge else B.player_item(cur, [], heals)
            my_move = None if my_item else (cur.charging or B.lead_choice(data, cur, foe))
            if my_move == "MOVE_STRUGGLE":
                my_move = cur.moves[0]
            if my_item:
                B.use_player_item(cur, my_item, [], heals)
            lp = data.moves[my_move]["priority"] if my_move else 6
            fp = data.moves[foe_move]["priority"] if foe_move else 6
            if fp != lp:
                me_first = lp > fp
            else:
                ms, fs = cur.stat("spe"), foe.stat("spe")
                me_first = ms > fs or (ms == fs and rng.random() < 0.5)
            if heal_item:
                items.remove(heal_item)
                foe.hp = foe.max_hp if B.ITEM_HEAL[heal_item] > 1000 else min(foe.max_hp, foe.hp + B.ITEM_HEAL[heal_item])
                if heal_item == "ITEM_FULL_RESTORE":
                    foe.status = None
                    foe.confused = 0
                me_first = True
            order = [(cur, foe, my_move), (foe, cur, foe_move)] if me_first else [(foe, cur, foe_move), (cur, foe, my_move)]
            for a, d, mv in order:
                if mv is None or a.hp <= 0 or d.hp <= 0:
                    continue
                if B.can_move(a, rng):
                    B.use_move(data, a, d, mv, rng, a is order[0][0])
                if a.hp <= 0 or d.hp <= 0:
                    break
            if cur.hp > 0:
                B.end_of_turn(cur, foe)
            if foe.hp > 0:
                B.end_of_turn(foe, cur)
            cur.flinch = foe.flinch = False
            if foe.hp <= 0:
                break
            if cur.hp <= 0:
                nxt = pick(data, team, foe) if policy == "smart" else next((m for m in team if m.hp > 0), None)
                if nxt is None:
                    return False
                cur = nxt
                cur.reset_volatile()
        else:
            return False
    return True


RIVAL = B.RIVAL

E4 = [
    ("Lorelei", "TRAINER_ELITE_FOUR_LORELEI"),
    ("Bruno", "TRAINER_ELITE_FOUR_BRUNO"),
    ("Agatha", "TRAINER_ELITE_FOUR_AGATHA"),
    ("Lance", "TRAINER_ELITE_FOUR_LANCE"),
    ("Champion", "TRAINER_CHAMPION_FIRST_{rival}"),
]


def rates(data, starter, members, runs, seed=1, policy="smart", gauntlet=False, heals=None):
    """members = [(name, level)]. Win rate per E4 battle (full heal between).
    "gauntlet": the rooms one after the other as in the game, where between
    them only HP and status come back (Full Restore, Revive), not PP; the
    rate of a room is among the runs that got there."""
    if gauntlet:
        rooms = [(label, tname.format(rival=RIVAL[starter])) for label, tname in E4]
        reached = [0] * len(rooms)
        won = [0] * len(rooms)
        rng = random.Random(seed)
        for _ in range(runs):
            team = [make(data, n, l, rng, starter) for n, l in members]
            bag = dict(heals or {})
            for i, (label, tname) in enumerate(rooms):
                reached[i] += 1
                t = data.trainers[tname]
                foes = [B.trainer_mon(data, p) for p in data.parties[t["party"]]]
                if not team_battle(data, team, foes, t["items"], rng, t["smart"], policy, heals=bag):
                    break
                won[i] += 1
                for m in team:
                    m.hp = m.max_hp
                    m.status = None
                    m.sleep = 0
                    m.toxic = 0
        return [won[i] / reached[i] if reached[i] else 0.0 for i in range(len(rooms))]
    out = []
    for label, tname in E4:
        tname = tname.format(rival=RIVAL[starter])
        t = data.trainers[tname]
        specs = data.parties[t["party"]]
        rng = random.Random(seed)
        wins = 0
        for _ in range(runs):
            team = [make(data, n, l, rng, starter) for n, l in members]
            foes = [B.trainer_mon(data, p) for p in specs]
            if team_battle(data, team, foes, t["items"], rng, t["smart"], policy):
                wins += 1
        out.append(wins / runs)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decomp", required=True)
    ap.add_argument("--runs", type=int, default=300)
    ap.add_argument("--starter", default="CHARMANDER")
    ap.add_argument("--team", default=None, help="NAME:LEVEL,NAME:LEVEL")
    ap.add_argument("--policy", default="smart")
    ap.add_argument("--search", action="store_true")
    ap.add_argument("--level", type=int, default=55)
    ap.add_argument("--lead", type=int, default=70)
    ap.add_argument("--size", type=int, default=2)
    ap.add_argument("--gauntlet", action="store_true", help="PP carried from room to room, as in the game")
    ap.add_argument("--pool", default="VAPOREON,JOLTEON,FLAREON,ONIX_VR,MACHOKE_VR,MAROWAK_VR,GOLBAT_VR,ARBOK,FEAROW,PRIMEAPE,ELECTRODE,KADABRA")
    args = ap.parse_args()
    data = B.Data(args.decomp)

    def show(members):
        r = rates(data, args.starter, members, args.runs, policy=args.policy, gauntlet=args.gauntlet)
        total = 1.0
        for x in r:
            total *= x
        desc = ", ".join("%s %d" % (n.title(), l) for n, l in members)
        print("%-60s %s  all: %3d%%" % (desc, " ".join("%3d%%" % round(x * 100) for x in r), round(total * 100)))
        return r, total

    if args.team:
        members = [(p.split(":")[0], int(p.split(":")[1])) for p in args.team.split(",")]
        show(members)
        for n, l in members:
            sp, mv = candidate(data, n, l, args.starter)
            print("   %s %d: %s %s" % (n, l, sp[8:].title(), ", ".join(m[5:].title() for m in mv)))
        return
    if args.search:
        pool = args.pool.split(",")
        eevee = {"VAPOREON", "JOLTEON", "FLAREON"}
        base = [("STARTER", args.lead), ("LAPRAS", args.level)]
        late = {"ONIX_VR": 46, "MACHOKE_VR": 46, "MAROWAK_VR": 46, "GOLBAT_VR": 45, "ARBOK": 44,
                "FEAROW": 42, "PRIMEAPE": 42}
        results = []
        for combo in itertools.combinations(pool, args.size):
            if len(eevee & set(combo)) > 1:
                continue
            members = base + [(c, late.get(c, args.level)) for c in combo]
            r, total = show(members)
            results.append((total, [round(x, 2) for x in r], combo))
        results.sort(reverse=True)
        for x in results[:10]:
            print("best:", x)

if __name__ == "__main__":
    main()


#  The C++ policy (team_score in PokemonFRLG_AutoStoryBattle.cpp): types
#  only. name -> (types, attack types, strength).
CPP_TEAM = {
    "STARTER_CHARMANDER": (("FIRE", "FLYING"), ["FIRE", "FLYING", "NORMAL"], 2.0),
    "STARTER_BULBASAUR": (("GRASS", "POISON"), ["GRASS", "NORMAL"], 2.0),
    "STARTER_SQUIRTLE": (("WATER", "WATER"), ["WATER", "DARK", "NORMAL"], 2.0),
    "ELECTRODE": (("ELECTRIC", "ELECTRIC"), ["ELECTRIC", "NORMAL"], 1.0),
    "VAPOREON": (("WATER", "WATER"), ["WATER", "ICE", "DARK", "NORMAL"], 1.0),
    "JOLTEON": (("ELECTRIC", "ELECTRIC"), ["ELECTRIC", "FIGHTING", "NORMAL"], 1.0),
    "FLAREON": (("FIRE", "FIRE"), ["FIRE", "DARK", "NORMAL"], 1.0),
    "LAPRAS": (("WATER", "ICE"), ["WATER", "ICE", "NORMAL"], 1.0),
}


def cpp_score(data, key, foe):
    types, attacks, strength = CPP_TEAM[key]
    offense = 0.25
    for t in attacks:
        e = data.effectiveness(t, foe.types)
        if t in types:
            e *= 1.5
        offense = max(offense, e)
    defense = max(data.effectiveness(ft, types) for ft in set(foe.types))
    return strength * offense / max(0.25, defense)

#!/usr/bin/env python3
"""
The League again (chapters 13 and 15: for prize money, and for the Hall of
Fame the National POKeDEX wants), played as the Autostory plays it, built on
battle_sim.py and team_sim.py:
  - the team member for each foe is chosen by the C++ type score
    (team_score() in PokemonFRLG_AutoStoryBattle.cpp): a switch at the
    battle menu that costs the turn, a free one at "Will PLAYER change
    POKeMON?", the best one left after a faint, the legendary birds once the
    team is down;
  - one bag for all five battles: REVIVEs and potions that restore all HP
    (MAX POTION), bought at the start of a round as far as the money goes
    and used between the rooms by the rules of story_heal_with_items();
  - a loss is either played again from the save in front of the room (free)
    or left standing: the game takes 4 x 30 x the highest level of the
    money, heals the team, and the League starts over from Lorelei (the
    rooms pay again).
What does a policy cost, what does it bring, and how long does it take until
the Champion is beaten?

  python3 league_again_sim.py --decomp /path/to/pokefirered

Charizard 78 with Electrode, Vaporeon and Lapras at 62 (about the levels of
the playthrough that got stuck in front of the Champion with four of six
down), 150 campaigns each, 15000 to start with. "First round": the Champion
beaten without a loss left standing.

  no items                                  first round   3%; the Champion
                                            beaten within 8 rounds: 25%
  4 REVIVE                                  first round  52%, 87 minutes
  4 REVIVE + 4 potions, below 60% always    first round  87%, 39 minutes
  4 REVIVE + 4 potions, below 40% (before
    the Champion 60%)                       first round  96%, 34 minutes
  the same, potions in battle as well       first round  64%, 59 minutes
  the program: the strongest below 50%
    (before the Champion 70%), the others
    below 25% (50%)                         first round  98%, 33 minutes
  the same, one try a save                  first round  81%
  the same, 3 REVIVE + 3 potions            first round  95%
  the same, nothing to buy them with        first round   3%, 77 minutes (the
                                            first round's rooms pay for them)

The program's rules use about 2.4 REVIVEs and 2.6 potions a round: some
19000 of the 29100 the five battles pay are left. With the others at 60 the
first round is won 87% of the time, at 64 99%.
"""

import argparse
import copy
import random
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import battle_sim as B
import team_sim as T

data = None

MOVES = {
    "STARTER": ["MOVE_CUT", "MOVE_FLAMETHROWER", "MOVE_FLY", "MOVE_WING_ATTACK"],
    "ELECTRODE": ["MOVE_SPARK", "MOVE_TACKLE", "MOVE_ROLLOUT", "MOVE_SONIC_BOOM"],
    "VAPOREON": ["MOVE_WATER_PULSE", "MOVE_BITE", "MOVE_AURORA_BEAM", "MOVE_HYDRO_PUMP"],
    "LAPRAS": ["MOVE_SURF", "MOVE_ROCK_SMASH", "MOVE_ICE_BEAM", "MOVE_STRENGTH"],
}
BIRDS = [
    ("SPECIES_ZAPDOS", 50, ["MOVE_THUNDER_WAVE", "MOVE_AGILITY", "MOVE_DETECT", "MOVE_DRILL_PECK"]),
    ("SPECIES_ARTICUNO", 50, ["MOVE_MIST", "MOVE_AGILITY", "MOVE_MIND_READER", "MOVE_ICE_BEAM"]),
]
#  team_for() in PokemonFRLG_AutoStory.cpp: types, attack types, strength.
CPP = {
    "STARTER": (("FIRE", "FLYING"), ["FIRE", "FLYING", "NORMAL"], 2.0),
    "ELECTRODE": (("ELECTRIC", "ELECTRIC"), ["ELECTRIC", "NORMAL"], 1.0),
    "VAPOREON": (("WATER", "WATER"), ["WATER", "ICE", "DARK", "NORMAL"], 1.0),
    "LAPRAS": (("WATER", "ICE"), ["WATER", "ICE", "NORMAL"], 1.0),
}
LEAGUE_ATTACKS = {
    "aerodactyl": ["FLYING", "NORMAL", "ROCK"], "alakazam": ["PSYCHIC"], "arbok": ["DARK", "POISON", "STEEL"],
    "arcanine": ["DARK", "FIRE", "NORMAL"], "blastoise": ["DARK", "NORMAL", "WATER"],
    "charizard": ["FIRE", "FLYING", "NORMAL"], "cloyster": ["WATER"], "dewgong": ["ICE", "WATER"],
    "dragonair": ["DRAGON", "NORMAL"], "dragonite": ["DRAGON", "FLYING", "NORMAL"],
    "exeggutor": ["GRASS", "NORMAL"], "gengar": ["GHOST", "POISON"], "golbat": ["DARK", "FLYING", "POISON"],
    "gyarados": ["DARK", "DRAGON", "NORMAL", "WATER"], "hitmonchan": ["FIGHTING", "ROCK"],
    "hitmonlee": ["FIGHTING", "NORMAL"], "jynx": ["ICE", "NORMAL"], "lapras": ["ICE", "NORMAL", "WATER"],
    "machamp": ["FIGHTING", "ROCK"], "onix": ["GROUND", "NORMAL", "ROCK", "STEEL"], "pidgeot": ["FLYING"],
    "rhydon": ["GROUND", "NORMAL", "ROCK"], "slowbro": ["ICE", "WATER"], "venusaur": ["GRASS"],
}
PRIZE = [5400, 5600, 5800, 6000, 6300]
ROOM_MIN = [6.0, 5.5, 4.0, 9.0, 5.5]      # minutes a room takes (battle, the heal, the save, the walk)
WHITEOUT = 9360
PRICE = {"rev": 1500, "pot": 2500}


def score(key, foe, strength=None):
    types, attacks, s = CPP[key]
    if strength is not None and key == "STARTER":
        s = strength
    off = 0.25
    for t in attacks:
        e = data.effectiveness(t, foe.types)
        if t in types:
            e *= 1.5
        off = max(off, e)
    slug = foe.species[8:].lower()
    foe_attacks = LEAGUE_ATTACKS.get(slug, list(foe.types))
    dfn = 0.25
    for t in foe_attacks:
        e = data.effectiveness(t, types)
        if t in foe.types:
            e *= 1.5
        dfn = max(dfn, e)
    return s * off / dfn


def make_team(levels, rng):
    team = []
    for name, level in levels:
        sp, _ = T.candidate(data, name, level, "CHARMANDER")
        m = B.Mon(data, sp, level, MOVES[name], B.random_ivs(rng), rng.choice(B.NATURES), {s: T.EVS for s in B.STATS})
        m.key = name
        team.append(m)
    birds = []
    for sp, level, moves in BIRDS:
        m = B.Mon(data, sp, level, moves, B.random_ivs(rng), rng.choice(B.NATURES), {s: 0 for s in B.STATS})
        m.key = None
        birds.append(m)
    return team, birds


def full_heal(mons):
    for m in mons:
        m.hp = m.max_hp
        m.status = None
        m.sleep = 0
        m.toxic = 0
        m.pp = {mv: data.moves[mv]["pp"] for mv in m.moves}


def battle(team, birds, foes, items, rng, cfg, bag):
    """One room. True if won. The program's switching: at the battle menu
    for a new foe a switch that costs the turn (1.5 x + 0.5 better), at the
    question before the next foe a free one (1.25 x + 0.25), the best one
    after a faint; the birds once the team is down."""
    items = list(items)
    strength = cfg.get("strength")
    alive = [m for m in team if m.hp > 0]
    cur = alive[0] if alive else next((b for b in birds if b.hp > 0), None)
    if cur is None:
        return False

    def best(foe, exclude):
        c = [m for m in team if m.hp > 0 and m is not exclude]
        if not c:
            return None
        return max(c, key=lambda m: score(m.key, foe, strength))

    def sc(m, foe):
        return score(m.key, foe, strength) if m.key else 0.0

    for i, foe in enumerate(foes):
        switching = False
        if i > 0:
            b = best(foe, cur)
            if b is not None and sc(b, foe) >= sc(cur, foe) * 1.25 + 0.25:
                cur = b
                cur.reset_volatile()
        else:
            b = best(foe, cur)
            if b is not None and sc(b, foe) >= sc(cur, foe) * 1.5 + 0.5:
                cur = b
                switching = True
        cur.new_foe() if not switching else cur.reset_volatile()
        for turn in range(300):
            foe_move = None
            heal_item = None
            for it in items:
                amount = B.ITEM_HEAL.get(it)
                if amount and foe.hp * 4 < foe.max_hp and foe.hp > 0:
                    heal_item = it
                    break
            if heal_item is None:
                foe_move = foe.charging or B.trainer_choice(data, foe, cur, rng, True)
            my_item = None
            if cfg.get("in_battle") and not switching and not cur.charging and not cur.recharge:
                if cur.hp * 3 < cur.max_hp and bag["pot"] > 0:
                    my_item = "pot"
            my_move = None if (my_item or switching) else (cur.charging or B.lead_choice(data, cur, foe))
            if my_move == "MOVE_STRUGGLE":
                my_move = cur.moves[0]
            if my_item:
                bag["pot"] -= 1
                bag["used_pot"] += 1
                cur.hp = cur.max_hp
            lp = data.moves[my_move]["priority"] if my_move else 6
            fp = data.moves[foe_move]["priority"] if foe_move else 6
            if fp != lp:
                me_first = lp > fp
            else:
                ms, fs = cur.stat("spe"), foe.stat("spe")
                me_first = ms > fs or (ms == fs and rng.random() < 0.5)
            if heal_item:
                items.remove(heal_item)
                foe.hp = foe.max_hp
                foe.status = None
                foe.confused = 0
                me_first = True
            switching = False
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
                nxt = best(foe, cur)
                if nxt is None:
                    nxt = next((b for b in birds if b.hp > 0), None)
                if nxt is None:
                    return False
                cur = nxt
                cur.reset_volatile()
        else:
            return False
    return True


def heal_between(team, bag, cfg, last):
    """After a room: REVIVE for who is down, a potion below the threshold
    (the strongest first)."""
    for m in team:
        if m.hp <= 0 and bag["rev"] > 0:
            bag["rev"] -= 1
            bag["used_rev"] += 1
            m.hp = m.max_hp // 2
            m.status = None
            m.sleep = 0
            m.toxic = 0
    below = cfg["last_below"] if last else cfg["below"]
    lead_below = cfg.get("lead_last_below", below) if last else cfg.get("lead_below", below)
    for k, m in enumerate(team):
        limit = lead_below if k == 0 else below
        if m.hp > 0 and m.hp < m.max_hp * limit and bag["pot"] > 0:
            bag["pot"] -= 1
            bag["used_pot"] += 1
            m.hp = m.max_hp
            m.status = None
            m.sleep = 0
            m.toxic = 0


def campaign(levels, cfg, rng, money0):
    """Rounds of the League until the Champion is beaten (or the rounds run
    out). Returns (won, minutes, money, rounds, whiteouts)."""
    team, birds = make_team(levels, rng)
    rooms = ["TRAINER_ELITE_FOUR_LORELEI", "TRAINER_ELITE_FOUR_BRUNO", "TRAINER_ELITE_FOUR_AGATHA",
             "TRAINER_ELITE_FOUR_LANCE", "TRAINER_CHAMPION_FIRST_SQUIRTLE"]
    money = money0
    bag = {"rev": cfg.get("rev0", 0), "pot": cfg.get("pot0", 0), "used_rev": 0, "used_pot": 0}
    minutes = 0.0
    whiteouts = 0
    for rnd in range(cfg.get("rounds", 8)):
        full_heal(team + birds)
        #  The kit: top up as far as the money goes, REVIVEs first.
        for kind in ("rev", "pot"):
            want = cfg[kind] - bag[kind]
            while want > 0 and money >= PRICE[kind]:
                money -= PRICE[kind]
                bag[kind] += 1
                want -= 1
        minutes += 1.5
        room = 0
        tries = 0
        while room < 5:
            saved = copy.deepcopy((team, birds, bag))
            fit = sum(1 for m in team if m.hp > 0)
            t = data.trainers[rooms[room]]
            foes = [B.trainer_mon(data, p) for p in data.parties[t["party"]]]
            c = dict(cfg)
            if room == 4 and cfg.get("in_battle_champion"):
                c["in_battle"] = True
            ok = battle(team, birds, foes, t["items"], rng, c, bag)
            minutes += ROOM_MIN[room]
            if ok:
                money += PRIZE[room]
                if room < 4:
                    heal_between(team, bag, cfg, room == 3)
                room += 1
                tries = 0
                continue
            tries += 1
            worn = fit * 2 <= len(team)
            if not worn and tries < cfg.get("tries", 3):
                team, birds, bag = saved
                minutes += 0.5
                continue
            break
        if room >= 5:
            return True, minutes, money, rnd + 1, whiteouts, bag
        whiteouts += 1
        money -= min(money, WHITEOUT)
        minutes += 1.0
    return False, minutes, money, cfg.get("rounds", 8), whiteouts, bag


def run(name, levels, cfg, n=300, money0=15000, seed=7):
    rng = random.Random(seed)
    won = 0
    mins = []
    monies = []
    rounds = []
    wo = []
    ur = []
    up = []
    for _ in range(n):
        ok, minutes, money, r, w, bag = campaign(levels, cfg, rng, money0)
        won += ok
        mins.append(minutes)
        monies.append(money + bag["rev"] * 0 - money0)
        rounds.append(r)
        wo.append(w)
        ur.append(bag["used_rev"])
        up.append(bag["used_pot"])
    avg = lambda x: sum(x) / len(x)
    first = sum(1 for r, w in zip(rounds, wo) if w == 0) / n
    print("%-44s done %3d%%  first round %3d%%  %5.0f min  money %+7.0f  per hour %+6.0f  rounds %.2f  whiteouts %.2f  rev %.1f pot %.1f" % (
        name, round(100 * won / n), round(100 * first), avg(mins), avg(monies), avg(monies) / avg(mins) * 60, avg(rounds), avg(wo), avg(ur), avg(up)), flush=True)


def main():
    global data
    ap = argparse.ArgumentParser()
    ap.add_argument("--decomp", required=True)
    ap.add_argument("--runs", type=int, default=150)
    ap.add_argument("--lead", type=int, default=78)
    ap.add_argument("--level", type=int, default=62, help="level of the three others")
    ap.add_argument("--money", type=int, default=15000)
    args = ap.parse_args()
    data = B.Data(args.decomp)
    n = args.runs
    levels = [("STARTER", args.lead), ("ELECTRODE", args.level), ("VAPOREON", args.level), ("LAPRAS", args.level)]
    print("Charizard %d, the others %d; %d campaigns each; %d to start with" % (args.lead, args.level, n, args.money))
    base = {"rev": 0, "pot": 0, "below": 0.0, "last_below": 0.0, "tries": 3}
    #  story_heal_with_items(), modes 2 and 3.
    program = dict(base, rev=4, pot=4, below=0.25, last_below=0.5, lead_below=0.5, lead_last_below=0.7)
    run("no items, 3 tries a save", levels, dict(base), n, args.money)
    run("4 REVIVE", levels, dict(base, rev=4), n, args.money)
    run("4 REVIVE + 4 pot (<60% always)", levels, dict(base, rev=4, pot=4, below=0.6, last_below=0.6), n, args.money)
    run("4 REVIVE + 4 pot (<40%, last <60%)", levels, dict(base, rev=4, pot=4, below=0.4, last_below=0.6), n, args.money)
    run("  the same, in battle as well", levels,
        dict(base, rev=4, pot=4, below=0.4, last_below=0.6, in_battle=True), n, args.money)
    run("the program: 4 + 4, lead <50/70, others <25/50", levels, dict(program), n, args.money)
    run("  the same, 1 try a save", levels, dict(program, tries=1), n, args.money)
    run("  the same, 3 + 3", levels, dict(program, rev=3, pot=3), n, args.money)
    run("  the same, starter strength 3", levels, dict(program, strength=3.0), n, args.money)
    run("  the same, no money to start with", levels, dict(program), n, 0)


if __name__ == "__main__":
    main()

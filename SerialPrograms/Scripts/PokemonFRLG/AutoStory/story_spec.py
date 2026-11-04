"""
FRLG Autostory: the main story as a list of segments.

Every position, flag, var and trigger here comes from the event scripts in
pret/pokefirered (data/maps/<Map>/scripts.inc and map.json). Comments name
the script that does the thing.

Each segment starts where the previous one saved. Variant-specific steps use
only(["SQUIRTLE"], [...]).
"""

STARTERS = ["BULBASAUR", "SQUIRTLE", "CHARMANDER"]


# ---------------------------------------------------------------- helpers
def goto(map_id, tiles, label=None, trigger=False):
    s = {"goto": {"map": map_id, "tiles": tiles, "trigger": trigger}}
    if label:
        s["label"] = label
    return s


def talk(map_id, obj=None, xy=None, mode="talk", label=None, dirs=None, counter=False):
    t = {"map": map_id, "mode": mode, "counter": counter}
    if obj is not None:
        t["object"] = obj
    if xy is not None:
        t["xy"] = xy
    if dirs:
        t["dirs"] = dirs
    s = {"talk": t}
    if label:
        s["label"] = label
    return s


def cutscene(min_ms, max_ms, label=None, answer=None):
    s = {"cutscene": [min_ms, max_ms]}
    if answer:
        s["answer"] = answer
    if label:
        s["label"] = label
    return s


def battle(label, timeout=30000, flee=False):
    s = {"battle": timeout, "label": label}
    if flee:
        s["flee"] = True
    return s


def setst(**kw):
    return {"set": kw}


def pos(map_id, x, y, facing=None, surfing=False):
    return {"pos": {"map": map_id, "x": x, "y": y, "facing": facing, "surfing": surfing}}


def heal(pc_map, label=None):
    s = {"heal": pc_map}
    if label:
        s["label"] = label
    return s


def save(dex=True):
    return {"save": {"dex": dex}}


def only(variants, steps):
    return {"only": {"variants": variants, "steps": steps}}


def grind(map_id, tiles, pc, level, label, every=6, slot=None, line=None, candies=False):
    """Train until a level. slot/line: a team member (party slot, its
    evolution line as slugs) trains in front instead of the lead. candies:
    first the Rare Candies in the bag, as many as the level needs."""
    g = {"map": map_id, "tiles": tiles, "pc": pc, "level": level, "every": every}
    if slot is not None:
        g["slot"] = slot
        g["line"] = line
    if candies:
        g["candies"] = True
    return {"grind": g, "label": label}


def teach(hm_rank, party_slot, move, label=None, forget=-1):
    """Teach an HM from the TM Case. forget = move slot (0-3) to replace if the
    Pokemon already knows 4 moves; -1 = let the program decide (the lead)."""
    return {"teach": {"hm_rank": hm_rank, "party_slot": party_slot, "move": move, "forget": forget},
            "label": label or ("Teach " + move)}


def catch(map_id, tiles, species, label, balls=5):
    return {"catch": {"map": map_id, "tiles": tiles, "species": species, "balls": balls}, "label": label}


def probe(map_id, hub, targets, name, label=None):
    """Legs from a hub tile to interact with each target and back (puzzles)."""
    return {"probe": {"map": map_id, "hub": hub, "targets": targets, "name": name}, "label": label or name}


def elevator(map_id, panel, from_idx, to_idx, exit_xy, push, dest, label="Elevator"):
    """Ride an elevator: talk to the panel, pick a floor, walk out."""
    return {"elevator": {"map": map_id, "panel": panel, "from": from_idx, "to": to_idx,
                         "exit": exit_xy, "push": push, "dest": dest}, "label": label}


def candy(map_id, xy, flag, label):
    """Pick up a Rare Candy near the way: an item ball, or a hidden item (A
    facing its tile). They are used at the League training (grind with
    candies=True), where a level costs the most battles."""
    return [talk(map_id, xy=xy, mode="talk", label=label), setst(flags=[flag])]


def talk_no(map_id, xy, label):
    return talk(map_id, xy=xy, mode="no", label=label)


def buy(map_id, clerk_xy, item_index, quantity, label):
    """Buy from a Poke Mart clerk: item_index in the mart's list (decomp)."""
    return [talk(map_id, xy=clerk_xy, mode="plain", label=label, counter=True),
            {"buy": {"index": item_index, "quantity": quantity}, "label": label}]


def use_item(words, party_slot, label):
    """Use an item from the bag's ITEMS pocket (found by words of its
    description) on a party slot: an evolution stone."""
    return {"use_item": {"words": words, "slot": party_slot}, "label": label}


def team(on=True):
    """From now on trainer battles are fought by the whole team (the best
    answer to each opponent comes in)."""
    return {"team": 1 if on else 0, "label": "Team play" if on else "Lead alone"}


def wait(ms, label="Wait"):
    return {"wait": ms, "label": label}


def puzzle(name):
    return {"puzzle": name}


def press(button, times=1, wait_ms=800, label=None):
    return {"press": {"button": button, "times": times, "wait": wait_ms}, "label": label or ("Press " + button)}


def defeated(map_id, *keys):
    return [[map_id, k] for k in keys]


# ---------------------------------------------------------------- chapter 0
#  Bedroom -> Pokedex. (Same story as v0.1, now with automatic routing.)
CHAPTER0 = [
    {
        "id": "C0_01_BEDROOM_TO_LAB", "chapter": 0, "has_dex": False,
        "title": "Bedroom -> Oak stops you -> lab",
        "start": {"map": "MAP_PALLET_TOWN_PLAYERS_HOUSE_2F", "x": 6, "y": 6, "facing": "up"},
        "state": {
            #  PalletTown_EventScript_SetSignLadyPos: before the starter she
            #  stands at (5,15) facing up.
            "objects": {"MAP_PALLET_TOWN": {"LOCALID_PALLET_SIGN_LADY": {"x": 5, "y": 15, "movement_type": "MOVEMENT_TYPE_FACE_UP"}}},
        },
        "steps": [
            #  PalletTown coord events (12,1)/(13,1): VAR_MAP_SCENE_PALLET_TOWN_OAK == 0.
            goto("MAP_PALLET_TOWN", [[13, 1], [12, 1]], "BedroomToOakTrigger", trigger=True),
            cutscene(25000, 180000, "Oak takes you to his lab"),
            #  PalletTown_EventScript_OakTrigger + ChooseStarterScene.
            setst(vars={"VAR_MAP_SCENE_PALLET_TOWN_OAK": 1, "VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB": 2},
                  clear_flags=["FLAG_HIDE_OAK_IN_HIS_LAB"]),
            pos("MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB", 6, 4, "up"),
        ],
    },
    {
        "id": "C0_02_CHOOSE_STARTER", "chapter": 0, "has_dex": False,
        "title": "Choose the starter",
        "steps": [
            only(["BULBASAUR"], [talk("MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB", "LOCALID_BULBASAUR_BALL", mode="yes", label="StarterBall")]),
            only(["SQUIRTLE"], [talk("MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB", "LOCALID_SQUIRTLE_BALL", mode="yes", label="StarterBall")]),
            only(["CHARMANDER"], [talk("MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB", "LOCALID_CHARMANDER_BALL", mode="yes", label="StarterBall")]),
            cutscene(10000, 120000, "Starter received, rival picks"),
            #  ChoseStarter: removeobject (sets the ball's flag), RivalTakesStarter.
            setst(vars={"VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB": 3},
                  flags=["FLAG_PALLET_LADY_NOT_BLOCKING_SIGN"]),
            only(["BULBASAUR"], [setst(flags=["FLAG_HIDE_BULBASAUR_BALL", "FLAG_HIDE_CHARMANDER_BALL"],
                                       objects={"MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB": {"LOCALID_OAKS_LAB_RIVAL": {"x": 10, "y": 5}}})]),
            only(["SQUIRTLE"], [setst(flags=["FLAG_HIDE_SQUIRTLE_BALL", "FLAG_HIDE_BULBASAUR_BALL"],
                                      objects={"MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB": {"LOCALID_OAKS_LAB_RIVAL": {"x": 8, "y": 5}}})]),
            only(["CHARMANDER"], [setst(flags=["FLAG_HIDE_CHARMANDER_BALL", "FLAG_HIDE_SQUIRTLE_BALL"],
                                        objects={"MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB": {"LOCALID_OAKS_LAB_RIVAL": {"x": 9, "y": 5}}})]),
            save(dex=False),
        ],
    },
    {
        "id": "C0_03_RIVAL_BATTLE", "chapter": 0, "has_dex": False,
        "title": "First rival battle",
        "steps": [
            #  RivalBattleTrigger{Left,Mid,Right} at (5..7,8) when the lab var is 3.
            goto("MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB", [[6, 8], [7, 8], [5, 8]], "ToRivalTrigger", trigger=True),
            {"rival1": True, "label": "Rival (lab)"},
            cutscene(8000, 120000, "Rival leaves the lab"),
            #  EndRivalBattle: removeobject rival, var 4.
            setst(vars={"VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB": 4},
                  flags=["FLAG_HIDE_RIVAL_IN_LAB", "FLAG_BEAT_RIVAL_IN_OAKS_LAB", "FLAG_OPENED_START_MENU"]),
            save(dex=False),
        ],
    },
    {
        "id": "C0_04_TO_VIRIDIAN_MART", "chapter": 0, "has_dex": False,
        "title": "Route 1 -> Viridian City -> Poke Mart (Oak's Parcel)",
        "state": {
            #  The sign lady wanders around her map.json spot once the start
            #  menu has been opened (PalletTown_EventScript_SetSignLadyDone).
            "objects": {
                "MAP_PALLET_TOWN": {"LOCALID_PALLET_SIGN_LADY": {"x": 3, "y": 10, "movement_type": "MOVEMENT_TYPE_WANDER_AROUND"}},
                #  ViridianCity_EventScript_SetOldManBlockingRoad (old man var 0).
                "MAP_VIRIDIAN_CITY": {"LOCALID_TUTORIAL_MAN": {"x": 21, "y": 11, "movement_type": "MOVEMENT_TYPE_FACE_DOWN"}},
            },
        },
        "steps": [
            #  Walking into the Mart starts ViridianCity_Mart_EventScript_ParcelScene.
            goto("MAP_VIRIDIAN_CITY_MART", [[4, 7]], "LabToViridianMart"),
            cutscene(6000, 90000, "Oak's Parcel"),
            setst(vars={"VAR_MAP_SCENE_VIRIDIAN_CITY_MART": 1, "VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB": 5}),
            pos("MAP_VIRIDIAN_CITY_MART", 4, 3, "left"),
            save(dex=False),
        ],
    },
    {
        "id": "C0_05_PARCEL_TO_OAK", "chapter": 0, "has_dex": False,
        "title": "Parcel to Oak -> Pokedex",
        "steps": [
            talk("MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB", "LOCALID_OAKS_LAB_PROF_OAK", mode="talk", label="ToOak"),
            cutscene(20000, 180000, "Pokedex scene"),
            #  ReceiveDexScene.
            setst(vars={"VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB": 6, "VAR_MAP_SCENE_VIRIDIAN_CITY_MART": 2,
                        "VAR_MAP_SCENE_VIRIDIAN_CITY_OLD_MAN": 1, "VAR_MAP_SCENE_PALLET_TOWN_RIVALS_HOUSE": 1,
                        "VAR_MAP_SCENE_ROUTE22": 1},
                  flags=["FLAG_SYS_POKEDEX_GET", "FLAG_HIDE_POKEDEX"],
                  objects={"MAP_VIRIDIAN_CITY": {"LOCALID_TUTORIAL_MAN": {"x": 21, "y": 8, "movement_type": "MOVEMENT_TYPE_LOOK_AROUND"}}}),
            save(dex=True),
        ],
    },
]

#  Level targets for the lead before each big fight (grinding). The C++ adds
#  the user's "extra levels" option on top.
#
#  From battle_sim.py: the lowest level at which the lead alone wins about
#  70% of the simulated battles of every big fight up to the next training
#  (the simulation is on the safe side: no EVs, no AI switching; a lost
#  fight is retried from the save before it). Never below the first plan.
#  Bulbasaur and Squirtle from Koga on, and every starter in the Elite Four,
#  cannot win alone at any sensible level (poison, rock moves, a weak
#  moveset): those values are stopgaps until the lead gets help (Lapras,
#  items in battle).
LEVELS = {
    "BROCK":   {"BULBASAUR": 12, "SQUIRTLE": 12, "CHARMANDER": 18},
    "FOREST":  {"BULBASAUR": 8,  "SQUIRTLE": 8,  "CHARMANDER": 9},
    "RIVAL2":  {"BULBASAUR": 27, "SQUIRTLE": 23, "CHARMANDER": 25},
    "MISTY":   {"BULBASAUR": 32, "SQUIRTLE": 29, "CHARMANDER": 34},
    "SURGE":   {"BULBASAUR": 28, "SQUIRTLE": 36, "CHARMANDER": 30},
    "ERIKA":   {"BULBASAUR": 40, "SQUIRTLE": 44, "CHARMANDER": 36},
    "KOGA":    {"BULBASAUR": 55, "SQUIRTLE": 55, "CHARMANDER": 50},
    "SABRINA": {"BULBASAUR": 58, "SQUIRTLE": 55, "CHARMANDER": 47},
    "BLAINE":  {"BULBASAUR": 60, "SQUIRTLE": 55, "CHARMANDER": 52},
    "GIOVANNI": {"BULBASAUR": 62, "SQUIRTLE": 58, "CHARMANDER": 66},
    "LEAGUE":  {"BULBASAUR": 70, "SQUIRTLE": 70, "CHARMANDER": 74},
}


def grind_all(key, map_id, tiles, pc, label, every=5, candies=False):
    """The team members' training (TEAM_TRAINING), then a grind step per
    starter with its own level target, at the same spot. The members first:
    while one is too weak for the wild Pokemon there, the lead fights for it
    and gets half the experience, which counts towards its own target."""
    steps = []
    for member, level in TEAM_TRAINING.get(key, {}).items():
        slots, lines = TEAM_MEMBERS[member]
        steps += [only([v], [grind(map_id, tiles, pc, level, label + "_" + member.title(), every,
                                   slot=slots[v], line=lines[v], candies=candies)]) for v in STARTERS]
    steps += [only([v], [grind(map_id, tiles, pc, LEVELS[key][v], label, every, candies=candies)]) for v in STARTERS]
    return steps


def rect(x0, y0, x1, y1):
    return [[x, y] for x in range(x0, x1 + 1) for y in range(y0, y1 + 1)]


PC = {
    "VIRIDIAN": "MAP_VIRIDIAN_CITY_POKEMON_CENTER_1F",
    "PEWTER": "MAP_PEWTER_CITY_POKEMON_CENTER_1F",
    "ROUTE4": "MAP_ROUTE4_POKEMON_CENTER_1F",
    "CERULEAN": "MAP_CERULEAN_CITY_POKEMON_CENTER_1F",
    "VERMILION": "MAP_VERMILION_CITY_POKEMON_CENTER_1F",
    "ROUTE10": "MAP_ROUTE10_POKEMON_CENTER_1F",
    "LAVENDER": "MAP_LAVENDER_TOWN_POKEMON_CENTER_1F",
    "CELADON": "MAP_CELADON_CITY_POKEMON_CENTER_1F",
    "FUCHSIA": "MAP_FUCHSIA_CITY_POKEMON_CENTER_1F",
    "SAFFRON": "MAP_SAFFRON_CITY_POKEMON_CENTER_1F",
    "CINNABAR": "MAP_CINNABAR_ISLAND_POKEMON_CENTER_1F",
    "INDIGO": "MAP_INDIGO_PLATEAU_POKEMON_CENTER_1F",
}

#  The team for the Elite Four (team_sim.py: the starter alone cannot win
#  against Lorelei, Lance and the Champion), in the order the Pokemon join:
#    Charmander: Voltorb (Route 10), Eevee -> Vaporeon (Celadon), Lapras (Silph Co.)
#    Bulbasaur:  Voltorb, Eevee -> Jolteon, Lapras
#    Squirtle:   Cut helper (Route 24), Voltorb, Eevee -> Flareon, Lapras
#  The party slots below follow from that order (the C++ has the same team
#  in team_for()).
VOLTORB_SLOT = {"BULBASAUR": 2, "SQUIRTLE": 3, "CHARMANDER": 2}
EEVEE_SLOT = {"BULBASAUR": 3, "SQUIRTLE": 4, "CHARMANDER": 3}
LAPRAS_SLOT = {"BULBASAUR": 4, "SQUIRTLE": 5, "CHARMANDER": 4}
TEAM_MEMBERS = {
    "VOLTORB": (VOLTORB_SLOT, {v: "voltorb,electrode" for v in ["BULBASAUR", "SQUIRTLE", "CHARMANDER"]}),
    "EEVEE": (EEVEE_SLOT, {"BULBASAUR": "eevee,jolteon", "SQUIRTLE": "eevee,flareon", "CHARMANDER": "eevee,vaporeon"}),
    "LAPRAS": (LAPRAS_SLOT, {v: "lapras" for v in ["BULBASAUR", "SQUIRTLE", "CHARMANDER"]}),
}
#  Levels the members train to, after the lead, at the lead's training spots.
#  The League: the rooms come one after the other, and between them only HP
#  comes back (Full Restore, Revive), not PP. Played that way (a gauntlet in
#  team_sim's battles), Charizard 74 with the members at 53 beats the
#  Champion one time in three; with the members at 58 three times in four.
#  A loss in the League starts it over with more training (AutoStory.cpp).
TEAM_TRAINING = {
    "ERIKA":    {"VOLTORB": 25, "EEVEE": 30},
    "KOGA":     {"VOLTORB": 33, "EEVEE": 35},
    "SABRINA":  {"LAPRAS": 32},
    "BLAINE":   {"VOLTORB": 42, "EEVEE": 42, "LAPRAS": 42},
    "GIOVANNI": {"VOLTORB": 47, "EEVEE": 47, "LAPRAS": 47},
    "LEAGUE":   {"VOLTORB": 58, "EEVEE": 58, "LAPRAS": 58},
}
#  Eevee's evolution has no attack of its type until late (Water Gun and
#  Thunder Shock come at 16, before it joins at 25): a TM right away.
#  (hm_rank: HMs come first in the TM Case, then the TMs by number. In
#  Celadon: HM01, TM03, TM28, TM34, TM39.)
EEVEE_TM = {"CHARMANDER": (2, "WATER PULSE"), "BULBASAUR": (4, "SHOCK WAVE")}
#  Evolution stone for Eevee: index in the Celadon Dept. Store 4F list
#  (Poke Doll, Retro Mail, Fire, Thunder, Water, Leaf Stone).
EEVEE_STONE = {"BULBASAUR": (3, "Thunder Stone"), "SQUIRTLE": (2, "Fire Stone"), "CHARMANDER": (4, "Water Stone")}

#  Party slot that learns Cut (Squirtle cannot: its helper in slot 2 does).
CUT_SLOT = {"BULBASAUR": 1, "SQUIRTLE": 2, "CHARMANDER": 1}


# ---------------------------------------------------------------- chapter 1
#  Pokedex -> Viridian Forest -> Pewter City -> Boulder Badge.
CHAPTER1 = [
    {
        "id": "C1_01_VIRIDIAN_PC", "chapter": 1,
        "title": "To the Viridian City Pokemon Center",
        "steps": [
            heal(PC["VIRIDIAN"], "LabToViridianPC"),
            save(),
        ],
    },
    {
        "id": "C1_02_GRIND_ROUTE2", "chapter": 1,
        "title": "Old man's catching tutorial, train on Route 2 (south)",
        "steps": [
            #  With VAR_MAP_SCENE_VIRIDIAN_CITY_OLD_MAN == 1 the old man stands at
            #  (21,8) between two triggers: the only way north.
            #  ViridianCity_EventScript_TutorialTriggerRight -> DoTutorialBattle.
            goto("MAP_VIRIDIAN_CITY", [[22, 8]], "ToOldManTutorial", trigger=True),
            cutscene(20000, 150000, "Old man catching tutorial"),
            #  Var 2: the old man goes back to his map.json spot (21,6) the next
            #  time the map loads; right now he still stands at (21,8).
            setst(vars={"VAR_MAP_SCENE_VIRIDIAN_CITY_OLD_MAN": 2},
                  objects={"MAP_VIRIDIAN_CITY": {"LOCALID_TUTORIAL_MAN": {"x": 21, "y": 6, "movement_type": "MOVEMENT_TYPE_LOOK_AROUND"}}},
                  blocked={"MAP_VIRIDIAN_CITY": [[21, 8]]}),
            pos("MAP_VIRIDIAN_CITY", 22, 8, "left"),
        ] + grind_all("FOREST", "MAP_ROUTE2", rect(6, 56, 12, 60), PC["VIRIDIAN"], "GrindRoute2South") + [save()],
    },
    {
        "id": "C1_03_VIRIDIAN_FOREST", "chapter": 1,
        "title": "Through Viridian Forest to Pewter City",
        "steps": [
            heal(PC["PEWTER"], "ViridianToPewterPC"),
            save(),
        ],
    },
    {
        "id": "C1_04_GRIND_BROCK", "chapter": 1,
        "title": "Train on Route 2 (north) for Brock",
        "steps": grind_all("BROCK", "MAP_ROUTE2", rect(2, 2, 8, 8), PC["PEWTER"], "GrindForBrock") + [save()],
    },
    {
        "id": "C1_05_BROCK", "chapter": 1,
        "title": "Defeat Brock (Boulder Badge)",
        "steps": [
            #  PewterCity_Gym_EventScript_Brock. The camper (Liam) can be avoided.
            talk("MAP_PEWTER_CITY_GYM", xy=[6, 5], mode="battle", dirs=["up"], label="Brock"),
            cutscene(3000, 60000, "Boulder Badge + TM39"),
            #  Save at once: a failure on the way out must not cost the badge.
            save(),
            #  PewterCity_Gym_EventScript_DefeatedBrock.
            setst(flags=["FLAG_DEFEATED_BROCK", "FLAG_BADGE01_GET", "FLAG_HIDE_PEWTER_CITY_GYM_GUIDE",
                         "FLAG_GOT_TM39_FROM_BROCK"],
                  clear_flags=["FLAG_HIDE_PEWTER_CITY_RUNNING_SHOES_GUY"],
                  vars={"VAR_MAP_SCENE_PEWTER_CITY": 1}),
            heal(PC["PEWTER"], "BrockToPewterPC"),
            save(),
        ],
    },
]


# ---------------------------------------------------------------- chapter 2
#  Running Shoes -> Route 3 -> Mt. Moon -> Cerulean -> Nugget Bridge -> Bill
#  -> Cascade Badge.
CHAPTER2 = [
    {
        "id": "C2_01_RUNNING_SHOES_ROUTE3", "chapter": 2,
        "title": "Running Shoes, Route 3 to the Pokemon Center at Mt. Moon",
        "steps": [
            #  PewterCity_EventScript_RunningShoesAideTriggerTop (VAR_MAP_SCENE_PEWTER_CITY == 1).
            goto("MAP_PEWTER_CITY", [[46, 21]], "ToRunningShoesAide", trigger=True),
            cutscene(8000, 90000, "Running Shoes"),
            #  PewterCity_EventScript_AideGiveRunningShoes.
            setst(flags=["FLAG_SYS_B_DASH", "FLAG_HIDE_PEWTER_CITY_RUNNING_SHOES_GUY"],
                  vars={"VAR_MAP_SCENE_PEWTER_CITY": 2}),
            pos("MAP_PEWTER_CITY", 46, 21, None),
            heal(PC["ROUTE4"], "Route3ToMtMoonPC"),
            save(),
        ],
    },
    {
        "id": "C2_02_MT_MOON", "chapter": 2,
        "title": "Mt. Moon: take a fossil, on to Cerulean City",
        "steps": [
            #  The only way to the fossils passes Miguel's trigger (14,11)
            #  (VAR_MAP_SCENE_MT_MOON_B2F == 0): MtMoon_B2F_EventScript_MiguelTrigger.
        ] + candy("MAP_MT_MOON_1F", [42, 35], "FLAG_HIDE_MT_MOON_1F_RARE_CANDY", "RareCandyMtMoon") + [
            goto("MAP_MT_MOON_B2F", [[14, 11]], "ToMiguel", trigger=True),
            battle("Super Nerd Miguel", 30000),
            setst(vars={"VAR_MAP_SCENE_MT_MOON_B2F": 1}),
            pos("MAP_MT_MOON_B2F", 14, 11, "left"),
            #  The fossils block the way to the B1F ladder that leads to the
            #  exit. MtMoon_B2F_EventScript_DomeFossil.
            talk("MAP_MT_MOON_B2F", "LOCALID_DOME_FOSSIL", mode="yes", dirs=["up"], label="DomeFossil"),
            cutscene(4000, 60000, "Miguel takes the Helix Fossil"),
            setst(flags=["FLAG_HIDE_DOME_FOSSIL", "FLAG_HIDE_HELIX_FOSSIL", "FLAG_GOT_DOME_FOSSIL",
                         "FLAG_GOT_FOSSIL_FROM_MT_MOON"],
                  objects={"MAP_MT_MOON_B2F": {"LOCALID_MIGUEL": {"x": 14, "y": 8}}}),
            heal(PC["CERULEAN"], "MtMoonToCeruleanPC"),
            save(),
        ],
    },
    {
        "id": "C2_03_GRIND_RIVAL", "chapter": 2,
        "title": "Train on Route 4 for the rival",
        "steps": grind_all("RIVAL2", "MAP_ROUTE4", rect(75, 11, 87, 16), PC["CERULEAN"], "GrindRoute4") + [save()],
    },
    {
        "id": "C2_04_RIVAL_NUGGET_BRIDGE", "chapter": 2,
        "title": "Rival in Cerulean, Nugget Bridge and the Rocket",
        "steps": [
            #  CeruleanCity_EventScript_RivalTriggerMid (VAR_MAP_SCENE_CERULEAN_CITY_RIVAL == 0).
            goto("MAP_CERULEAN_CITY", [[23, 6]], "ToCeruleanRival", trigger=True),
            battle("Rival (Cerulean)", 40000),
            cutscene(6000, 90000, "Rival leaves, Fame Checker"),
            setst(vars={"VAR_MAP_SCENE_CERULEAN_CITY_RIVAL": 1, "VAR_MAP_SCENE_ROUTE22": 2},
                  flags=["FLAG_GOT_FAME_CHECKER", "FLAG_HIDE_CERULEAN_RIVAL"]),
            pos("MAP_CERULEAN_CITY", 23, 6, "up"),
            #  Route24_EventScript_RocketTriggerRight (VAR_MAP_SCENE_ROUTE24 == 0):
            #  the Rocket only motions, he does not walk.
            goto("MAP_ROUTE24", [[11, 15]], "NuggetBridge", trigger=True),
            cutscene(3000, 30000, "Nugget"),
            battle("Rocket (Nugget Bridge)", 30000),
            cutscene(1000, 20000, "After the Rocket"),
            setst(vars={"VAR_MAP_SCENE_ROUTE24": 1}),
            pos("MAP_ROUTE24", 11, 15, "right"),
            heal(PC["CERULEAN"], "BridgeToCeruleanPC"),
            save(),
        ],
    },
    {
        "id": "C2_05_BILL", "chapter": 2,
        "title": "Help Bill (S.S. Ticket)",
        "steps": [
            #  Route25_SeaCottage_EventScript_Bill: any answer means yes.
            talk("MAP_ROUTE25_SEA_COTTAGE", "LOCALID_BILL_CLEFAIRY", mode="yes", label="BillClefairy"),
            cutscene(4000, 60000, "Bill enters the teleporter"),
            setst(flags=["FLAG_HIDE_BILL_CLEFAIRY", "BILL_IN_TELEPORTER"]),
            #  Route25_SeaCottage_EventScript_Computer -> RunCellSeparator.
            talk("MAP_ROUTE25_SEA_COTTAGE", xy=[4, 5], mode="talk", dirs=["up"], label="BillsPC"),
            cutscene(8000, 90000, "Cell separation"),
            setst(clear_flags=["BILL_IN_TELEPORTER", "FLAG_HIDE_BILL_HUMAN_SEA_COTTAGE"],
                  flags=["FLAG_HELPED_BILL_IN_SEA_COTTAGE"],
                  objects={"MAP_ROUTE25_SEA_COTTAGE": {"LOCALID_BILL_HUMAN": {"x": 7, "y": 6}}}),
            talk("MAP_ROUTE25_SEA_COTTAGE", "LOCALID_BILL_HUMAN", mode="talk", label="BillTicket"),
            cutscene(4000, 60000, "S.S. Ticket"),
            setst(flags=["FLAG_GOT_SS_TICKET", "FLAG_HIDE_NUGGET_BRIDGE_ROCKET", "FLAG_SYS_NOT_SOMEONES_PC"]),
            heal(PC["CERULEAN"], "BillToCeruleanPC"),
            save(),
        ],
    },
    {
        "id": "C2_06_GRIND_MISTY", "chapter": 2,
        "title": "Train on Route 24 for Misty (Squirtle: also catch a Cut user)",
        "steps": [
            #  Poke Balls for the team (Voltorb on Route 10).
        ] + buy("MAP_CERULEAN_CITY_MART", [2, 3], 0, 15, "BuyPokeBalls") + [
            #  Squirtle cannot learn Cut: catch Oddish (FireRed) or Bellsprout
            #  (LeafGreen) here, it goes to party slot 2.
            only(["SQUIRTLE"], [catch("MAP_ROUTE24", rect(3, 19, 5, 31), ["oddish", "bellsprout"], "CatchCutHelper", balls=5)]),
        ] + grind_all("MISTY", "MAP_ROUTE24", rect(3, 19, 5, 31), PC["CERULEAN"], "GrindRoute24") + [save()],
    },
    {
        "id": "C2_07_MISTY", "chapter": 2,
        "title": "Defeat Misty (Cascade Badge)",
        "steps": [
            talk("MAP_CERULEAN_CITY_GYM", xy=[8, 6], mode="battle", label="Misty"),
            cutscene(3000, 60000, "Cascade Badge + TM03"),
            #  Save at once: a failure on the way out must not cost the badge.
            save(),
            setst(flags=["FLAG_DEFEATED_MISTY", "FLAG_BADGE02_GET", "FLAG_GOT_TM03_FROM_MISTY"]),
            heal(PC["CERULEAN"], "MistyToCeruleanPC"),
            save(),
        ],
    },
]

# ---------------------------------------------------------------- chapter 3
#  Underground Path -> Vermilion -> S.S. Anne (HM01 Cut) -> Thunder Badge.
VERMILION_CANS = [[x, y] for y in (10, 12, 14) for x in (1, 3, 5, 7, 9)]

CHAPTER3 = [
    {
        "id": "C3_01_TO_VERMILION", "chapter": 3,
        "title": "Rocket behind the house, Route 5, Underground Path, Route 6 to Vermilion City",
        "steps": [
            #  The only way south leads through the burgled house (back door at
            #  (4,1)) and past the Rocket: CeruleanCity_EventScript_GruntTriggerBottom.
        ] + candy("MAP_CERULEAN_CITY", [18, 7], "FLAG_HIDDEN_ITEM_CERULEAN_CITY_RARE_CANDY", "RareCandyCerulean") + [
            goto("MAP_CERULEAN_CITY", [[33, 7]], "ToCeruleanRocket", trigger=True),
            cutscene(2000, 30000, "Rocket intro"),
            battle("Rocket (Cerulean)", 30000),
            cutscene(4000, 60000, "TM28, Rocket leaves"),
            #  CeruleanCity_EventScript_GruntDefeated.
            setst(vars={"VAR_MAP_SCENE_CERULEAN_CITY_ROCKET": 1},
                  flags=["FLAG_HIDE_CERULEAN_ROCKET", "FLAG_GOT_TM28_FROM_ROCKET"]),
            pos("MAP_CERULEAN_CITY", 33, 7, "up"),
        ] + candy("MAP_ROUTE6", [19, 5], "FLAG_HIDDEN_ITEM_ROUTE6_RARE_CANDY", "RareCandyRoute6") + [
            heal(PC["VERMILION"], "CeruleanToVermilionPC"),
            save(),
        ],
    },
    {
        "id": "C3_02_SS_ANNE", "chapter": 3,
        "title": "S.S. Anne: rival and the captain (HM01 Cut)",
        "steps": [
            #  VermilionCity_EventScript_CheckTicket (the (22,32)/(23,32)
            #  ExitedTicketCheck triggers are harmless).
            goto("MAP_VERMILION_CITY", [[23, 33]], "ToTicketCheck", trigger=True),
            cutscene(2000, 30000, "Ticket check"),
            setst(vars={"VAR_VERMILION_CITY_TICKET_CHECK_TRIGGER": 1}),
            pos("MAP_VERMILION_CITY", 23, 33, "right"),
            #  SSAnne_2F_Corridor_EventScript_RivalTriggerMid (var 0).
            goto("MAP_SSANNE_2F_CORRIDOR", [[31, 6]], "ToSSAnneRival", trigger=True),
            battle("Rival (S.S. Anne)", 40000),
            cutscene(3000, 60000, "Rival leaves"),
            setst(vars={"VAR_MAP_SCENE_S_S_ANNE_2F_CORRIDOR": 1}, flags=["FLAG_HIDE_SS_ANNE_RIVAL"]),
            pos("MAP_SSANNE_2F_CORRIDOR", 31, 6, "up"),
            talk("MAP_SSANNE_CAPTAINS_OFFICE", "LOCALID_SS_ANNE_CAPTAIN", mode="talk", label="Captain"),
            cutscene(8000, 90000, "HM01 Cut"),
            setst(flags=["FLAG_GOT_HM01"], vars={"VAR_MAP_SCENE_VERMILION_CITY": 1}),
            #  SSAnne_Exterior_ExitSSAnne runs as soon as we step outside.
            goto("MAP_SSANNE_EXTERIOR", [[32, 14]], "LeaveSSAnne"),
            cutscene(20000, 120000, "The S.S. Anne leaves"),
            setst(vars={"VAR_MAP_SCENE_VERMILION_CITY": 3, "VAR_VERMILION_CITY_TICKET_CHECK_TRIGGER": 0},
                  flags=["FLAG_HIDE_SS_ANNE"]),
            pos("MAP_VERMILION_CITY", 23, 32, "up"),
            heal(PC["VERMILION"], "DockToVermilionPC"),
            save(),
        ],
    },
    {
        "id": "C3_03_CUT_AND_GRIND", "chapter": 3,
        "title": "Teach Cut, train on Route 11 for Lt. Surge",
        "steps": [only([v], [teach(1, CUT_SLOT[v], "CUT", forget=-1 if CUT_SLOT[v] == 1 else 0)]) for v in STARTERS]
        + [setst(abilities={"cut": True})]
        + grind_all("SURGE", "MAP_ROUTE11", rect(18, 2, 29, 7), PC["VERMILION"], "GrindRoute11") + [save()],
    },
    {
        "id": "C3_04_SURGE", "chapter": 3,
        "title": "Lt. Surge: trainers, trash can puzzle, Thunder Badge",
        "steps": [
            #  Beat the three gym trainers first so they do not interrupt
            #  the trash can search. Dwayne and Baily turn around: each is
            #  beaten before the way crosses its line of sight elsewhere
            #  (it would come up to the player and stand somewhere else).
            talk("MAP_VERMILION_CITY_GYM", xy=[8, 13], mode="battle", label="Sailor Dwayne"),
            talk("MAP_VERMILION_CITY_GYM", xy=[2, 11], mode="battle", label="Super Nerd Baily"),
            talk("MAP_VERMILION_CITY_GYM", xy=[7, 8], mode="battle", label="Gentleman Tucker"),
            setst(defeated=defeated("MAP_VERMILION_CITY_GYM", "4", "1", "2")),
            #  VermilionCity_Gym_EventScript_TrashCan: switch 1 is random, switch 2
            #  is next to it (SetVermilionTrashCans). The C++ searches by reading
            #  the dialog.
            probe("MAP_VERMILION_CITY_GYM", [5, 15], VERMILION_CANS, "VERMILION_TRASH_CANS"),
            #  VermilionCity_Gym_EventScript_SetBeamsOff.
            setst(flags=["FLAG_FOUND_BOTH_VERMILION_GYM_SWITCHES"],
                  script_tiles=[["MAP_VERMILION_CITY_GYM", "VermilionCity_Gym_EventScript_SetBeamsOff"]]),
            talk("MAP_VERMILION_CITY_GYM", xy=[5, 2], mode="battle", label="Lt. Surge"),
            cutscene(3000, 60000, "Thunder Badge + TM34"),
            #  Save at once: a failure on the way out must not cost the badge.
            save(),
            #  VermilionCity_Gym_EventScript_DefeatedLtSurge also brings Oak's Aide
            #  to Vermilion City (25,7), right in the way to the Pokemon Center.
            setst(flags=["FLAG_DEFEATED_LT_SURGE", "FLAG_BADGE03_GET", "FLAG_GOT_TM34_FROM_SURGE"],
                  clear_flags=["FLAG_HIDE_VERMILION_CITY_OAKS_AIDE"]),
            heal(PC["VERMILION"], "SurgeToVermilionPC"),
            save(),
        ],
    },
]

# ---------------------------------------------------------------- chapter 4
#  Route 9 -> Rock Tunnel (dark) -> Lavender -> Celadon -> Rainbow Badge.
CHAPTER4 = [
    {
        "id": "C4_01_ROUTE9", "chapter": 4,
        "title": "Back to Cerulean, Route 9 (Cut) to the Pokemon Center at Rock Tunnel",
        "steps": candy("MAP_ROUTE9", [63, 2], "FLAG_HIDDEN_ITEM_ROUTE9_RARE_CANDY", "RareCandyRoute9") + [
            heal(PC["ROUTE10"], "VermilionToRoute10PC"),
            save(),
        ],
    },
    {
        "id": "C4_02_ROCK_TUNNEL", "chapter": 4,
        "title": "Catch Voltorb on Route 10, Rock Tunnel (without Flash) to Lavender Town",
        "steps": [
            #  Voltorb -> Electrode: the Electric member of the team.
            #  A Poke Ball catches it 1 time in 4 (catch rate 190, full HP):
            #  15 bought + 5 from Oak miss all with a chance of 1 in 300. A
            #  reset gives the balls back.
            catch("MAP_ROUTE10", rect(6, 6, 16, 10), ["voltorb"], "CatchVoltorb", balls=20),
            heal(PC["ROUTE10"], "VoltorbToRoute10PC"),
            save(),
            #  PokeManiac Cooper (B1F) looks around next to the only way to
            #  the last ladder: whether he sees the player is chance, and in
            #  the dark the steps cannot be checked. Talk to him instead.
            talk("MAP_ROCK_TUNNEL_B1F", xy=[2, 6], mode="battle", label="PokeManiac Cooper"),
            heal(PC["LAVENDER"], "RockTunnelToLavenderPC"),
            save(),
        ],
    },
    {
        "id": "C4_03_TO_CELADON", "chapter": 4,
        "title": "Route 8, Underground Path, Route 7 to Celadon City",
        "steps": [
            heal(PC["CELADON"], "LavenderToCeladonPC"),
            save(),
        ],
    },
    {
        "id": "C4_04_TEA_AND_GRIND", "chapter": 4,
        "title": "Get the Tea (for the Saffron gate guards), train on Route 7",
        "steps": [
            #  CeladonCity_Condominiums_1F_EventScript_TeaWoman.
            talk("MAP_CELADON_CITY_CONDOMINIUMS_1F", xy=[2, 9], mode="talk", label="TeaWoman"),
            cutscene(2000, 30000, "Tea"),
            setst(flags=["FLAG_GOT_TEA"]),
            #  CeladonCity_Condominiums_RoofRoom_EventScript_EeveeBall: Eevee (25)
            #  for the team; NO to the nickname.
            talk("MAP_CELADON_CITY_CONDOMINIUMS_ROOF_ROOM", "LOCALID_EEVEE_POKEBALL", mode="no", label="Eevee"),
            cutscene(1000, 20000, "Got Eevee"),
            setst(flags=["FLAG_HIDE_EEVEE_BALL"]),
        ] + [only([v], buy("MAP_CELADON_CITY_DEPARTMENT_STORE_4F", [3, 13], EEVEE_STONE[v][0], 1, "Buy" + EEVEE_STONE[v][1].replace(" ", "")))
             for v in STARTERS] + [
            only([v], [use_item("evolve", EEVEE_SLOT[v], "Evolve Eevee (" + EEVEE_STONE[v][1] + ")")]) for v in STARTERS
        ] + [
            #  It knows Helping Hand, Sand-Attack, Growl, Quick Attack.
            only([v], [teach(EEVEE_TM[v][0], EEVEE_SLOT[v], EEVEE_TM[v][1], forget=0)]) for v in EEVEE_TM
        ] + grind_all("ERIKA", "MAP_ROUTE7", rect(11, 2, 18, 6), PC["CELADON"], "GrindRoute7") + [save()],
    },
    {
        "id": "C4_05_ERIKA", "chapter": 4,
        "title": "Defeat Erika (Rainbow Badge)",
        "steps": [
            talk("MAP_CELADON_CITY_GYM", xy=[6, 4], mode="battle", dirs=["up"], label="Erika"),
            cutscene(3000, 60000, "Rainbow Badge + TM19"),
            #  Save at once: a failure on the way out must not cost the badge.
            save(),
            setst(flags=["FLAG_DEFEATED_ERIKA", "FLAG_BADGE04_GET", "FLAG_GOT_TM19_FROM_ERIKA"]),
            heal(PC["CELADON"], "ErikaToCeladonPC"),
            save(),
        ],
    },
]

# ---------------------------------------------------------------- chapter 5
#  Rocket Hideout (Silph Scope) -> Pokemon Tower (Poke Flute).
CHAPTER5 = [
    {
        "id": "C5_01_HIDEOUT_LIFT_KEY", "chapter": 5,
        "title": "Game Corner: Rocket at the poster, into the hideout, get the Lift Key",
        "steps": [
            #  CeladonCity_GameCorner_EventScript_RocketGrunt guards the poster.
            talk("MAP_CELADON_CITY_GAME_CORNER", "LOCALID_GAME_CORNER_GRUNT", mode="battle", label="GameCornerRocket"),
            cutscene(2000, 30000, "Rocket leaves"),
            setst(flags=["FLAG_HIDE_GAME_CORNER_ROCKET"]),
            #  CeladonCity_GameCorner_EventScript_Poster -> OpenRocketHideout.
            talk("MAP_CELADON_CITY_GAME_CORNER", xy=[11, 1], mode="talk", dirs=["up"], label="Poster"),
            cutscene(2000, 30000, "Switch behind the poster"),
            setst(flags=["FLAG_OPENED_ROCKET_HIDEOUT"],
                  script_tiles=[["MAP_CELADON_CITY_GAME_CORNER", "CeladonCity_GameCorner_EventScript_OpenRocketHideout"]]),
            #  RocketHideout_B4F_EventScript_Grunt1 drops the Lift Key.
        ] + candy("MAP_ROCKET_HIDEOUT_B3F", [12, 12], "FLAG_HIDE_ROCKET_HIDEOUT_B3F_RARE_CANDY", "RareCandyHideout") + [
            talk("MAP_ROCKET_HIDEOUT_B4F", xy=[4, 2], mode="battle", label="LiftKeyRocket"),
            cutscene(1000, 20000, "Lift Key drops"),
            setst(clear_flags=["FLAG_HIDE_LIFT_KEY"]),
            talk("MAP_ROCKET_HIDEOUT_B4F", "LOCALID_LIFT_KEY", mode="talk", label="LiftKey"),
            cutscene(1000, 20000, "Lift Key"),
            setst(flags=["FLAG_CAN_USE_ROCKET_HIDEOUT_LIFT", "FLAG_HIDE_LIFT_KEY"]),
            heal(PC["CELADON"], "LiftKeyToCeladonPC"),
            save(),
        ],
    },
    {
        "id": "C5_02_HIDEOUT_GIOVANNI", "chapter": 5,
        "title": "Elevator to B4F, defeat Giovanni (Silph Scope)",
        "steps": [
            goto("MAP_ROCKET_HIDEOUT_ELEVATOR", [[2, 5]], "ToHideoutElevator"),
            #  RocketHideout_Elevator_EventScript_FloorSelect: B1F, B2F, B4F, EXIT;
            #  the cursor starts on the current floor (B2F).
            elevator("MAP_ROCKET_HIDEOUT_ELEVATOR", [0, 2], 1, 2, [2, 5], "down",
                     ["MAP_ROCKET_HIDEOUT_B4F", 20, 24, "down"], "HideoutElevatorToB4F"),
            #  The door grunts; when both are beaten the barrier goes
            #  (RocketHideout_B4F_EventScript_RemoveBarrier).
            talk("MAP_ROCKET_HIDEOUT_B4F", xy=[19, 14], mode="battle", label="DoorRocketRight"),
            talk("MAP_ROCKET_HIDEOUT_B4F", xy=[16, 14], mode="battle", label="DoorRocketLeft"),
            setst(script_tiles=[["MAP_ROCKET_HIDEOUT_B4F", "RocketHideout_B4F_EventScript_RemoveBarrier"]]),
            talk("MAP_ROCKET_HIDEOUT_B4F", "LOCALID_HIDEOUT_GIOVANNI", mode="battle", label="Giovanni (Hideout)"),
            cutscene(3000, 60000, "Giovanni leaves"),
            setst(flags=["FLAG_HIDE_HIDEOUT_GIOVANNI", "FLAG_HIDE_CELADON_ROCKETS"],
                  clear_flags=["FLAG_HIDE_SILPH_SCOPE"]),
            talk("MAP_ROCKET_HIDEOUT_B4F", "LOCALID_SILPH_SCOPE", mode="talk", label="SilphScope"),
            cutscene(1000, 20000, "Silph Scope"),
            setst(flags=["FLAG_HIDE_SILPH_SCOPE"]),
            #  Back up with the lift (B4F is cursor position 2) to B1F, where
            #  the Rocket behind the barrier guards the way out.
            goto("MAP_ROCKET_HIDEOUT_ELEVATOR", [[2, 5]], "BackToHideoutElevator"),
            elevator("MAP_ROCKET_HIDEOUT_ELEVATOR", [0, 2], 2, 0, [2, 5], "down",
                     ["MAP_ROCKET_HIDEOUT_B1F", 24, 26, "down"], "HideoutElevatorToB1F"),
            #  RocketHideout_B1F_EventScript_Grunt5 -> RemoveBarrier.
            talk("MAP_ROCKET_HIDEOUT_B1F", xy=[21, 27], mode="battle", label="BarrierRocket"),
            setst(script_tiles=[["MAP_ROCKET_HIDEOUT_B1F", "RocketHideout_B1F_EventScript_RemoveBarrier"]]),
            heal(PC["CELADON"], "HideoutToCeladonPC"),
            save(),
        ],
    },
    {
        "id": "C5_03_TO_LAVENDER", "chapter": 5,
        "title": "Back to Lavender Town",
        #  Balls for a shiny (the option "Catch shiny Pokemon"): the best on
        #  sale on the way, as many as the money allows (LavenderTown_Mart_Items).
        "steps": buy("MAP_LAVENDER_TOWN_MART", [2, 3], 0, 5, "BuyGreatBalls") + [
            heal(PC["LAVENDER"], "CeladonToLavenderPC"),
            save(),
        ],
    },
    {
        "id": "C5_04_POKEMON_TOWER", "chapter": 5,
        "title": "Pokemon Tower: Marowak ghost, Rockets, Mr. Fuji (Poke Flute)",
        "steps": [
            #  PokemonTower_2F_EventScript_RivalTriggerDown: both ways west pass
            #  the rival (VAR_MAP_SCENE_POKEMON_TOWER_2F == 0).
            goto("MAP_POKEMON_TOWER_2F", [[16, 6]], "ToTowerRival", trigger=True),
            battle("Rival (Pokemon Tower)", 40000),
            cutscene(3000, 60000, "Rival leaves"),
            setst(vars={"VAR_MAP_SCENE_POKEMON_TOWER_2F": 1}, flags=["FLAG_HIDE_TOWER_RIVAL"]),
            pos("MAP_POKEMON_TOWER_2F", 16, 6, "up"),
            #  PokemonTower_6F_EventScript_MarowakGhost: the only way to the 7F
            #  stairs; it has to be beaten (running away sends you back).
        ] + candy("MAP_POKEMON_TOWER_6F", [5, 15], "FLAG_HIDE_POKEMON_TOWER_6F_RARE_CANDY", "RareCandyTower") + [
            goto("MAP_POKEMON_TOWER_6F", [[12, 16], [11, 15]], "ToMarowakGhost", trigger=True),
            cutscene(1000, 15000, "Be gone, intruders"),
            battle("Ghost Marowak", 40000),
            cutscene(2000, 30000, "Marowak's spirit is calmed"),
            setst(vars={"VAR_MAP_SCENE_POKEMON_TOWER_6F": 1}),
            #  PokemonTower_7F_EventScript_MrFuji warps us to his house.
            talk("MAP_POKEMON_TOWER_7F", xy=[11, 4], mode="talk", dirs=["up"], label="MrFujiTower"),
            cutscene(5000, 60000, "Mr. Fuji takes you home"),
            setst(flags=["FLAG_HIDE_TOWER_FUJI", "FLAG_RESCUED_MR_FUJI"], clear_flags=["FLAG_HIDE_POKEHOUSE_FUJI"]),
            pos("MAP_LAVENDER_TOWN_VOLUNTEER_POKEMON_HOUSE", 4, 7, "up"),
            #  LavenderTown_VolunteerPokemonHouse_EventScript_MrFuji.
            talk("MAP_LAVENDER_TOWN_VOLUNTEER_POKEMON_HOUSE", xy=[3, 3], mode="talk", label="MrFujiFlute"),
            cutscene(3000, 60000, "Poke Flute"),
            setst(flags=["FLAG_GOT_POKE_FLUTE"]),
            heal(PC["LAVENDER"], "FujiToLavenderPC"),
            save(),
        ],
    },
]

# ---------------------------------------------------------------- chapter 6
#  Snorlax -> Routes 12-15 -> Fuchsia: Safari Zone (Surf, Gold Teeth),
#  Warden (Strength), Soul Badge.
CHAPTER6 = [
    {
        "id": "C6_01_SNORLAX_TO_FUCHSIA", "chapter": 6,
        "title": "Wake Snorlax (Poke Flute), Routes 12-15 to Fuchsia City",
        "steps": [
            #  Route12_EventScript_Snorlax: "use the Poke Flute?" YES -> wild battle.
            #  Running away also removes it (B_OUTCOME_RAN).
            talk("MAP_ROUTE12", xy=[14, 70], mode="yes", dirs=["down"], label="Snorlax"),
            cutscene(3000, 30000, "Poke Flute"),
            battle("Snorlax", 40000, flee=True),
            cutscene(1000, 15000, "Snorlax gone"),
            setst(flags=["FLAG_HIDE_ROUTE_12_SNORLAX", "FLAG_WOKE_UP_ROUTE_12_SNORLAX"]),
        ] + candy("MAP_ROUTE12", [9, 116], "FLAG_HIDDEN_ITEM_ROUTE12_RARE_CANDY", "RareCandyRoute12") + buy("MAP_FUCHSIA_CITY_MART", [2, 3], 0, 5, "BuyUltraBalls") + [
            heal(PC["FUCHSIA"], "Route12ToFuchsiaPC"),
            save(),
        ],
    },
    {
        "id": "C6_02_SAFARI_ZONE", "chapter": 6,
        "title": "Safari Zone: HM03 Surf and the Gold Teeth, the Warden gives HM04 Strength",
        "steps": [
            #  FuchsiaCity_SafariZone_Entrance_EventScript_EntryTriggerMid: YES, 500.
            goto("MAP_FUCHSIA_CITY_SAFARI_ZONE_ENTRANCE", [[4, 3]], "ToSafariDesk", trigger=True),
            cutscene(4000, 40000, "Join the Safari Game", answer="yes"),
            setst(vars={"VAR_MAP_SCENE_FUCHSIA_CITY_SAFARI_ZONE_ENTRANCE": 2}),
            pos("MAP_SAFARI_ZONE_CENTER", 26, 30, "up"),
            talk("MAP_SAFARI_ZONE_WEST", xy=[28, 14], mode="talk", label="GoldTeeth"),
            cutscene(1000, 15000, "Gold Teeth"),
            setst(flags=["FLAG_HIDE_SAFARI_ZONE_WEST_GOLD_TEETH"]),
            talk("MAP_SAFARI_ZONE_SECRET_HOUSE", xy=[6, 5], mode="talk", label="SecretHouse"),
            cutscene(3000, 30000, "HM03 Surf"),
            setst(flags=["FLAG_GOT_HM03"]),
            #  Walking back would take more than the 600 steps, so use them up
            #  outside the grass: SafariZone_EventScript_TimesUp warps us to the
            #  entrance (4,1), we step out to (4,2) and ExitWarpIn walks 2 down.
            {"oscillate": {"map": "MAP_SAFARI_ZONE_WEST", "tiles": rect(10, 6, 16, 12)}, "label": "SafariUseUpSteps"},
            cutscene(4000, 40000, "Time's up"),
            setst(vars={"VAR_MAP_SCENE_FUCHSIA_CITY_SAFARI_ZONE_ENTRANCE": 0}),
            pos("MAP_FUCHSIA_CITY_SAFARI_ZONE_ENTRANCE", 4, 4, "down"),
            #  FuchsiaCity_WardensHouse_EventScript_Warden -> GiveGoldTeeth.
            talk("MAP_FUCHSIA_CITY_WARDENS_HOUSE", xy=[3, 5], mode="talk", label="Warden"),
            cutscene(3000, 40000, "HM04 Strength"),
            setst(flags=["FLAG_GOT_HM04"]),
            #  (The Rare Candy here is behind boulders: Strength comes later.)
            heal(PC["FUCHSIA"], "WardenToFuchsiaPC"),
            save(),
        ],
    },
    {
        "id": "C6_03_GRIND_KOGA", "chapter": 6,
        "title": "Train on Route 15 for Koga",
        "steps": grind_all("KOGA", "MAP_ROUTE15", rect(0, 0, 70, 20), PC["FUCHSIA"], "GrindRoute15") + [save()],
    },
    {
        "id": "C6_04_KOGA", "chapter": 6,
        "title": "Defeat Koga (Soul Badge)",
        "steps": [
            talk("MAP_FUCHSIA_CITY_GYM", xy=[7, 13], mode="battle", label="Koga"),
            cutscene(3000, 60000, "Soul Badge + TM06"),
            #  Save at once: a failure on the way out must not cost the badge.
            save(),
            setst(flags=["FLAG_DEFEATED_KOGA", "FLAG_BADGE05_GET", "FLAG_GOT_TM06_FROM_KOGA"]),
            heal(PC["FUCHSIA"], "KogaToFuchsiaPC"),
            save(),
        ],
    },
]

# ---------------------------------------------------------------- chapter 7
#  Tea for the gate guard -> Saffron: Silph Co. (Card Key, rival, Lapras,
#  Giovanni) -> Surf and Strength on Lapras -> Marsh Badge.
CHAPTER7 = [
    {
        "id": "C7_01_TO_SAFFRON", "chapter": 7,
        "title": "To Saffron City (the gate guard gets the Tea)",
        "steps": [
            #  Route8_WestEntrance_EventScript_GuardTriggerMid with FLAG_GOT_TEA:
            #  one var opens all four gates. The player turns up to the guard
            #  and walks up one tile to him (Route8_WestEntrance_Movement_
            #  WalkToGuardMid): the scene ends at (6, 4) facing up. Only
            #  (1, 5) of the west doorway is the exit (an arrow warp).
            goto("MAP_ROUTE8_WEST_ENTRANCE", [[6, 5]], "ToSaffronGate", trigger=True),
            cutscene(4000, 40000, "The guard drinks the tea"),
            setst(vars={"VAR_MAP_SCENE_ROUTE5_ROUTE6_ROUTE7_ROUTE8_GATES": 1}),
            pos("MAP_ROUTE8_WEST_ENTRANCE", 6, 4, "up"),
            heal(PC["SAFFRON"], "GateToSaffronPC"),
            save(),
        ],
    },
    {
        "id": "C7_02_SILPH_CARD_KEY", "chapter": 7,
        "title": "Silph Co.: get the Card Key on 5F",
        "steps": [
            talk("MAP_SILPH_CO_5F", xy=[22, 21], mode="talk", label="CardKey"),
            cutscene(1000, 20000, "Card Key"),
            setst(flags=["FLAG_HIDE_SILPH_CO_5F_CARD_KEY"]),
            heal(PC["SAFFRON"], "CardKeyToSaffronPC"),
            save(),
        ],
    },
    {
        "id": "C7_03_SILPH_GIOVANNI", "chapter": 7,
        "title": "Silph Co.: rival on 7F, Lapras, Giovanni on 11F",
        "steps": [
            #  SilphCo_7F_EventScript_RivalTriggerBottom (VAR_MAP_SCENE_SILPH_CO_7F == 0).
            goto("MAP_SILPH_CO_7F", [[2, 5], [2, 4]], "ToSilphRival", trigger=True),
            battle("Rival (Silph Co.)", 40000),
            cutscene(3000, 60000, "Rival leaves"),
            setst(vars={"VAR_MAP_SCENE_SILPH_CO_7F": 1}, flags=["FLAG_HIDE_SILPH_RIVAL"]),
            #  SilphCo_7F_EventScript_LaprasGuy: "nickname?" -> NO (B).
            talk("MAP_SILPH_CO_7F", xy=[0, 7], mode="talk", label="LaprasGuy"),
            cutscene(4000, 40000, "Lapras"),
            setst(flags=["FLAG_GOT_LAPRAS_FROM_SILPH"]),
            #  SilphCo_11F_EventScript_GiovanniTriggerRight.
            goto("MAP_SILPH_CO_11F", [[6, 15], [5, 15]], "ToSilphGiovanni", trigger=True),
            cutscene(2000, 30000, "Giovanni"),
            battle("Giovanni (Silph Co.)", 40000),
            cutscene(3000, 60000, "Giovanni leaves"),
            setst(vars={"VAR_MAP_SCENE_SILPH_CO_11F": 1},
                  flags=["FLAG_HIDE_SAFFRON_ROCKETS", "FLAG_HIDE_SILPH_ROCKETS"],
                  clear_flags=["FLAG_HIDE_SAFFRON_CIVILIANS"]),
            heal(PC["SAFFRON"], "SilphToSaffronPC"),
            save(),
        ],
    },
    {
        "id": "C7_04_HMS_AND_GRIND", "chapter": 7,
        "title": "Teach Surf and Strength to Lapras, train for Sabrina",
        "steps": [only([v], [
            #  Lapras (Lv. 25) knows Mist, Body Slam, Confuse Ray, Perish Song.
            teach(2, LAPRAS_SLOT[v], "SURF", forget=0), teach(3, LAPRAS_SLOT[v], "STRENGTH", forget=3)]) for v in STARTERS]
        + [setst(abilities={"surf": True, "strength": True})]
        + grind_all("SABRINA", "MAP_ROUTE7", rect(11, 2, 18, 6), PC["CELADON"], "GrindRoute7Sabrina") + [save()],
    },
    {
        "id": "C7_05_SABRINA", "chapter": 7,
        "title": "Defeat Sabrina (Marsh Badge)",
        "steps": [
            talk("MAP_SAFFRON_CITY_GYM", xy=[14, 11], mode="battle", label="Sabrina"),
            cutscene(3000, 60000, "Marsh Badge + TM04"),
            #  Save at once: a failure on the way out must not cost the badge.
            save(),
            setst(flags=["FLAG_DEFEATED_SABRINA", "FLAG_BADGE06_GET", "FLAG_GOT_TM04_FROM_SABRINA"]),
            heal(PC["SAFFRON"], "SabrinaToSaffronPC"),
            save(),
        ],
    },
]

# ---------------------------------------------------------------- chapter 8
#  Surf to Cinnabar -> Pokemon Mansion (Secret Key) -> Volcano Badge.
#  CinnabarIsland_Gym_EventScript_Quiz1..6: the YES/NO answer that opens
#  each door (a wrong answer starts a trainer battle instead).
CINNABAR_QUIZ = [
    ([22, 10], "yes"), ([15, 2], "no"), ([13, 10], "no"),
    ([13, 17], "no"), ([1, 18], "yes"), ([1, 10], "no"),
]


def cinnabar_quiz_steps():
    out = []
    for i, (xy, answer) in enumerate(CINNABAR_QUIZ):
        n = i + 1
        out.append(talk("MAP_CINNABAR_ISLAND_GYM", xy=xy, mode=answer, label="Quiz%d" % n))
        out.append(cutscene(2000, 30000, "Quiz %d answer" % n))
        out.append(setst(flags=["FLAG_CINNABAR_GYM_QUIZ_%d" % n],
                         script_tiles=[["MAP_CINNABAR_ISLAND_GYM", "CinnabarIsland_Gym_EventScript_OpenDoor%d" % n]]))
    return out


CHAPTER8 = [
    {
        "id": "C8_01_TO_CINNABAR", "chapter": 8,
        "title": "Via Pallet Town, surf Route 21 to Cinnabar Island",
        "steps": [
            heal(PC["CINNABAR"], "SaffronToCinnabarPC"),
            save(),
        ],
    },
    {
        "id": "C8_02_MANSION", "chapter": 8,
        "title": "Pokemon Mansion: switches, the Secret Key",
        "steps": [
            goto("MAP_POKEMON_MANSION_1F", [[8, 33]], "ToMansion"),
            {"switch_puzzle": {"goal": talk("MAP_POKEMON_MANSION_B1F", xy=[5, 7])}, "label": "MansionSwitches"},
            talk("MAP_POKEMON_MANSION_B1F", xy=[5, 7], mode="talk", label="SecretKey"),
            cutscene(1000, 20000, "Secret Key"),
            #  CinnabarIsland_EventScript_CheckUnlockGym: VAR_TEMP_1 = 1 from now on.
            setst(flags=["FLAG_HIDE_POKEMON_MANSION_B1F_SECRET_KEY"],
                  map_vars={"MAP_CINNABAR_ISLAND": {"VAR_TEMP_1": 1}}),
            #  The way back to the front door may need other switch states.
            {"switch_puzzle": {"goal_tiles": [["MAP_POKEMON_MANSION_1F", 8, 32], ["MAP_POKEMON_MANSION_1F", 8, 33],
                                             ["MAP_POKEMON_MANSION_1F", 34, 33], ["MAP_POKEMON_MANSION_1F", 34, 32]]},
             "label": "MansionSwitchesBack"},
            heal(PC["CINNABAR"], "MansionToCinnabarPC"),
            save(),
        ],
    },
    {
        "id": "C8_03_GRIND_BLAINE", "chapter": 8,
        "title": "Train in the Pokemon Mansion for Blaine",
        "steps": grind_all("BLAINE", "MAP_POKEMON_MANSION_1F", rect(1, 27, 15, 32), PC["CINNABAR"], "GrindMansion") + [save()],
    },
    {
        "id": "C8_04_BLAINE", "chapter": 8,
        "title": "Quiz and Blaine (Volcano Badge); decline Bill's invitation",
        "steps": cinnabar_quiz_steps() + [
            talk("MAP_CINNABAR_ISLAND_GYM", xy=[5, 4], mode="battle", label="Blaine"),
            cutscene(3000, 60000, "Volcano Badge + TM38"),
            #  Save at once: a failure on the way out must not cost the badge.
            save(),
            setst(flags=["FLAG_DEFEATED_BLAINE", "FLAG_BADGE07_GET", "FLAG_GOT_TM38_FROM_BLAINE"],
                  clear_flags=["FLAG_HIDE_CINNABAR_BILL"],
                  vars={"VAR_MAP_SCENE_CINNABAR_ISLAND": 1}),
            #  Stepping outside starts CinnabarIsland_EventScript_BillScene: say NO
            #  (B), Bill waits in the Pokemon Center; the Sevii Islands are
            #  not needed for the League.
            goto("MAP_CINNABAR_ISLAND", [[20, 5]], "LeaveCinnabarGym"),
            cutscene(4000, 60000, "Bill asks to go to One Island (no)"),
            setst(vars={"VAR_MAP_SCENE_CINNABAR_ISLAND": 2}, flags=["FLAG_HIDE_CINNABAR_BILL"],
                  clear_flags=["FLAG_HIDE_CINNABAR_POKECENTER_BILL"]),
            heal(PC["CINNABAR"], "BlaineToCinnabarPC"),
            save(),
        ],
    },
]

CHAPTER8.append({
    "id": "C8_05_GRIND_GIOVANNI", "chapter": 8,
    "title": "Train in the Pokemon Mansion for Giovanni",
    "steps": grind_all("GIOVANNI", "MAP_POKEMON_MANSION_1F", rect(1, 27, 15, 32), PC["CINNABAR"], "GrindMansion2") + [save()],
})


# ---------------------------------------------------------------- chapter 9
#  Viridian Gym (Earth Badge) -> rival on Route 22 -> badge guards on
#  Route 23 -> Victory Road (Strength puzzles) -> Indigo Plateau.
ROUTE23_GUARDS = list(range(1, 8))

CHAPTER9 = [
    {
        "id": "C9_01_TO_VIRIDIAN", "chapter": 9,
        "title": "Back to Viridian City",
        "state": {
            #  ViridianCity_EventScript_TryUnlockGym: badges 2-7 unlock the gym door.
            "vars": {"VAR_MAP_SCENE_VIRIDIAN_CITY_GYM_DOOR": 1},
        },
        "steps": [
            heal(PC["VIRIDIAN"], "CinnabarToViridianPC"),
            save(),
        ],
    },
    {
        "id": "C9_02_GIOVANNI", "chapter": 9,
        "title": "Giovanni in the Viridian Gym (Earth Badge)",
        "steps": [
            #  Takashi (10,2) guards the only way to Giovanni. If he spots us
            #  from below he walks into that corridor and stays there, so talk
            #  to him from the right (reached over the spinners) instead.
            talk("MAP_VIRIDIAN_CITY_GYM", xy=[10, 2], mode="battle", dirs=["left"], label="Black Belt Takashi"),
            talk("MAP_VIRIDIAN_CITY_GYM", "LOCALID_VIRIDIAN_GIOVANNI", mode="battle", label="Giovanni (Gym)"),
            cutscene(3000, 60000, "Earth Badge + TM26"),
            #  Save at once: a failure on the way out must not cost the badge.
            save(),
            #  ViridianCity_Gym_EventScript_DefeatedGiovanni.
            setst(flags=["FLAG_DEFEATED_LEADER_GIOVANNI", "FLAG_BADGE08_GET", "FLAG_HIDE_MISC_KANTO_ROCKETS",
                         "FLAG_GOT_TM26_FROM_GIOVANNI"],
                  vars={"VAR_MAP_SCENE_ROUTE22": 3}),
            heal(PC["VIRIDIAN"], "GiovanniToViridianPC"),
            save(),
        ],
    },
    {
        "id": "C9_03_ROUTE22_ROUTE23", "chapter": 9,
        "title": "Rival on Route 22, the badge guards on Route 23",
        "steps": [
            #  Route22_EventScript_LateRivalTriggerMid (VAR_MAP_SCENE_ROUTE22 == 3).
            {"trigger_var": {"map": "MAP_ROUTE22", "var": "VAR_MAP_SCENE_ROUTE22", "value": 3}, "label": "ToRoute22Rival"},
            battle("Rival (Route 22)", 40000),
            cutscene(3000, 60000, "Rival leaves"),
            setst(vars={"VAR_MAP_SCENE_ROUTE22": 4}),
            #  Heal after the rival (the save there means a failure later
            #  on does not mean fighting him again), and save on Route 23
            #  below: a failure in Victory Road then starts again at its
            #  door, not at the rival.
            heal(PC["VIRIDIAN"], "Route22RivalToViridianPC"),
            #  Route22_NorthEntrance_EventScript_BoulderBadgeGuardTrigger and the
            #  Route 23 guards: each one bumps VAR_MAP_SCENE_ROUTE23.
            {"trigger_var": {"map": "MAP_ROUTE22_NORTH_ENTRANCE", "var": "VAR_MAP_SCENE_ROUTE23", "value": 0}, "label": "BoulderBadgeGuard"},
            cutscene(3000, 30000, "Boulder Badge"),
            setst(vars={"VAR_MAP_SCENE_ROUTE23": 1}),
        ] + [x for k in ROUTE23_GUARDS for x in (
            {"trigger_var": {"map": "MAP_ROUTE23", "var": "VAR_MAP_SCENE_ROUTE23", "value": k}, "label": "BadgeGuard%d" % k},
            cutscene(2000, 30000, "Badge %d" % (k + 1)),
            setst(vars={"VAR_MAP_SCENE_ROUTE23": k + 1}),
        )] + [save()],
    },
    {
        "id": "C9_04_VICTORY_ROAD", "chapter": 9,
        "title": "Victory Road: boulders onto the switches, to Indigo Plateau",
        "state": {
            #  Route23_OnTransition resets the Victory Road puzzles.
            "flags": ["FLAG_HIDE_VICTORY_ROAD_2F_BOULDER"],
            "clear_flags": ["FLAG_HIDE_VICTORY_ROAD_3F_BOULDER"],
            "vars": {"VAR_MAP_SCENE_VICTORY_ROAD_1F": 0, "VAR_MAP_SCENE_VICTORY_ROAD_2F_BOULDER1": 0,
                     "VAR_MAP_SCENE_VICTORY_ROAD_2F_BOULDER2": 0, "VAR_MAP_SCENE_VICTORY_ROAD_3F": 0},
        },
        "steps": [
            goto("MAP_VICTORY_ROAD_1F", [[11, 19]], "ToVictoryRoad"),
            #  VictoryRoad_1F_EventScript_FloorSwitch opens the rock barrier.
            {"boulder": {"target": [20, 16]}, "label": "VR1F_Boulder"},
            setst(vars={"VAR_MAP_SCENE_VICTORY_ROAD_1F": 100},
                  script_tiles=[["MAP_VICTORY_ROAD_1F", "VictoryRoad_1F_EventScript_FloorSwitch"]]),
            goto("MAP_VICTORY_ROAD_2F", [[1, 10], [2, 9], [1, 8]], "VR1FTo2F"),
            {"boulder": {"target": [2, 19]}, "label": "VR2F_Boulder1"},
            setst(vars={"VAR_MAP_SCENE_VICTORY_ROAD_2F_BOULDER1": 100},
                  script_tiles=[["MAP_VICTORY_ROAD_2F", "VictoryRoad_2F_EventScript_FloorSwitch1"]]),
            goto("MAP_VICTORY_ROAD_3F", [[5, 3], [6, 2], [4, 2]], "VR2FTo3F"),
            {"boulder": {"target": [7, 7]}, "label": "VR3F_Boulder"},
            setst(vars={"VAR_MAP_SCENE_VICTORY_ROAD_3F": 100},
                  script_tiles=[["MAP_VICTORY_ROAD_3F", "VictoryRoad_3F_EventScript_FloorSwitch"]]),
            #  A boulder dropped through the hole at (34,18) appears on 2F
            #  (GetBoulderRevealFlagByLocalIdAndMap: FLAG_HIDE_VICTORY_ROAD_2F_BOULDER).
            {"boulder": {"target": [34, 18], "hole": True}, "label": "VR3F_Hole"},
            setst(clear_flags=["FLAG_HIDE_VICTORY_ROAD_2F_BOULDER"], flags=["FLAG_HIDE_VICTORY_ROAD_3F_BOULDER"]),
            goto("MAP_VICTORY_ROAD_2F", [[32, 19], [31, 19], [34, 20], [35, 19]], "VR3FTo2FBoulder"),
            {"boulder": {"target": [14, 19]}, "label": "VR2F_Boulder2"},
            setst(vars={"VAR_MAP_SCENE_VICTORY_ROAD_2F_BOULDER2": 100},
                  script_tiles=[["MAP_VICTORY_ROAD_2F", "VictoryRoad_2F_EventScript_FloorSwitch2"]]),
            heal(PC["INDIGO"], "VictoryRoadToIndigoPC"),
            save(),
        ],
    },
]


# ---------------------------------------------------------------- chapter 10
#  Training, items, Elite Four, Champion, Hall of Fame.
LEAGUE_ROOMS = [
    ("MAP_POKEMON_LEAGUE_LORELEIS_ROOM", "Lorelei", "FLAG_DEFEATED_LORELEI", (6, 7), "PokemonLeague_EventScript_SetDoorOpen"),
    ("MAP_POKEMON_LEAGUE_BRUNOS_ROOM", "Bruno", "FLAG_DEFEATED_BRUNO", (6, 7), "PokemonLeague_EventScript_SetDoorOpen"),
    ("MAP_POKEMON_LEAGUE_AGATHAS_ROOM", "Agatha", "FLAG_DEFEATED_AGATHA", (6, 7), "PokemonLeague_EventScript_SetDoorOpen"),
    ("MAP_POKEMON_LEAGUE_LANCES_ROOM", "Lance", "FLAG_DEFEATED_LANCE", (6, 10), "PokemonLeague_EventScript_SetDoorOpenLance"),
]
LEAGUE_LEADER_XY = {"Lorelei": [6, 5], "Bruno": [6, 5], "Agatha": [6, 5], "Lance": [6, 8]}


def league_room_steps():
    out = []
    for i, (room, name, flag, entry_xy, door_label) in enumerate(LEAGUE_ROOMS):
        #  Arriving starts PokemonLeague_*Room_EventScript_EnterRoom (walk in,
        #  the entrance closes behind us).
        out.append(goto(room, [[6, 12]] if name != "Lance" else [[23, 13]], "To" + name))
        out.append(cutscene(2500, 30000, "Enter %s's room" % name))
        out.append(setst(vars={"VAR_MAP_SCENE_POKEMON_LEAGUE": i + 1}))
        out.append(pos(room, entry_xy[0], entry_xy[1], "up" if name != "Lance" else "left"))
        out.append(talk(room, xy=LEAGUE_LEADER_XY[name], mode="battle", label=name))
        out.append(cutscene(3000, 60000, "%s: the door opens" % name))
        out.append(setst(flags=[flag], script_tiles=[[room, "@" + door_label]]))
        out.append({"heal_items": True, "label": "Heal with items after " + name})
        out.append(save())
    return out


CHAPTER10 = [
    {
        "id": "C10_01_GRIND_LEAGUE", "chapter": 10,
        "title": "Train in Victory Road for the Elite Four",
        #  Every walk to the training spot crosses Route 23, whose
        #  Route23_OnTransition resets the Victory Road puzzles: the rock
        #  barriers are back (VictoryRoad_*F_OnLoad), and the boulder that
        #  fell to 2F is gone again. (The barrier at 2F (33, 17) stood in the
        #  route to the grass.)
        "state": {
            "flags": ["FLAG_HIDE_VICTORY_ROAD_2F_BOULDER"],
            "clear_flags": ["FLAG_HIDE_VICTORY_ROAD_3F_BOULDER"],
            "vars": {"VAR_MAP_SCENE_VICTORY_ROAD_1F": 0, "VAR_MAP_SCENE_VICTORY_ROAD_2F_BOULDER1": 0,
                     "VAR_MAP_SCENE_VICTORY_ROAD_2F_BOULDER2": 0, "VAR_MAP_SCENE_VICTORY_ROAD_3F": 0},
            "script_tiles": [
                ["MAP_VICTORY_ROAD_1F", "VictoryRoad_1F_EventScript_SetRockBarrier"],
                ["MAP_VICTORY_ROAD_2F", "VictoryRoad_2F_EventScript_SetRockBarrier1"],
                ["MAP_VICTORY_ROAD_2F", "VictoryRoad_2F_EventScript_SetRockBarrier2"],
                ["MAP_VICTORY_ROAD_3F", "VictoryRoad_3F_EventScript_SetRockBarrier"],
            ],
        },
        #  Near the way out to Route 23: the barriers shut the rest of 2F off.
        "steps": grind_all("LEAGUE", "MAP_VICTORY_ROAD_2F", rect(39, 10, 46, 11), PC["INDIGO"], "GrindVictoryRoad",
                           candies=True) + [save()],
    },
    {
        "id": "C10_02_ITEMS", "chapter": 10,
        "title": "Buy Full Restores, Revives and Ultra Balls",
        #  Between the rooms everyone who fought gets a Full Restore (and a
        #  Revive first if they fainted).
        "steps": buy("MAP_INDIGO_PLATEAU_POKEMON_CENTER_1F", [0, 7], 2, 15, "BuyFullRestore")
        + buy("MAP_INDIGO_PLATEAU_POKEMON_CENTER_1F", [0, 7], 4, 8, "BuyRevive")
        #  With the money left: Ultra Balls for a shiny.
        + buy("MAP_INDIGO_PLATEAU_POKEMON_CENTER_1F", [0, 7], 0, 5, "BuyUltraBalls")
        + [heal(PC["INDIGO"], "ShopToIndigoNurse"), save()],
    },
    {
        "id": "C10_03_ELITE_FOUR", "chapter": 10,
        "title": "Elite Four: Lorelei, Bruno, Agatha, Lance",
        "steps": [team()] + league_room_steps(),
    },
    {
        "id": "C10_04_CHAMPION", "chapter": 10,
        "title": "Champion: the rival, Hall of Fame",
        "steps": [
            team(),
            #  PokemonLeague_ChampionsRoom_EventScript_EnterRoom does everything:
            #  walk in, rival battle, Oak, Hall of Fame.
            goto("MAP_POKEMON_LEAGUE_CHAMPIONS_ROOM", [[6, 19]], "ToChampion"),
            cutscene(6000, 60000, "Champion intro"),
            battle("Champion (rival)", 60000),
            #  Oak, the Hall of Fame (the game saves itself there: Continue
            #  starts in Pallet Town, HEAL_LOCATION_PALLET_TOWN), the credits,
            #  then the game resets to the title screen. A through it all,
            #  then load that save.
            {"hall_of_fame": 600000, "label": "Oak, Hall of Fame, credits"},
        ],
    },
]


def legendary(map_id, xy, flags, label):
    """A legendary Pokemon after the Hall of Fame: A in front of it
    ("Gyaoo!"), a wild battle in which it gets the best ball in the bag
    (the Master Ball too) until it is caught. Not caught (it fainted, the
    balls ran out): the step fails, and the save before it is tried again."""
    return [talk(map_id, xy=xy, mode="talk", label=label),
            {"battle": 60000, "label": label, "legendary": True},
            setst(flags=flags)]


#  After the Hall of Fame: the legendary Pokemon. Articuno (Seafoam Islands:
#  water currents, boulders) and Moltres (Mt. Ember: the boat to the Sevii
#  Islands) are still to come; Mewtwo needs the whole Sevii Islands story.
CHAPTER11 = [
    {
        "id": "C11_01_MASTER_BALL", "chapter": 11,
        "title": "After the Hall of Fame: the Master Ball from the Silph Co. president",
        #  The game was loaded at the Hall of Fame save: Pallet Town, in front
        #  of the house. PalletTown_EventScript_OakRatingScene (ON_FRAME,
        #  VAR_MAP_SCENE_PALLET_TOWN_OAK == 2): Oak comes and talks.
        "state": {"vars": {"VAR_MAP_SCENE_PALLET_TOWN_OAK": 2}},
        "steps": [
            pos("MAP_PALLET_TOWN", 6, 8, "down"),
            cutscene(3000, 90000, "Oak checks the Pokedex"),
            setst(vars={"VAR_MAP_SCENE_PALLET_TOWN_OAK": 3}),
            #  SilphCo_11F_EventScript_President: the Master Ball, any time after Giovanni.
            talk("MAP_SILPH_CO_11F", xy=[9, 9], mode="talk", label="SilphPresident"),
            cutscene(1000, 30000, "Master Ball"),
            setst(flags=["FLAG_GOT_MASTER_BALL_FROM_SILPH"]),
            heal(PC["SAFFRON"], "SilphToSaffronPC"),
            save(),
        ],
    },
    {
        "id": "C11_02_ZAPDOS", "chapter": 11,
        "title": "Zapdos in the Power Plant (Master Ball)",
        "steps": [
            heal(PC["ROUTE10"], "SaffronToRoute10PC"),
            save(),
        ] + legendary("MAP_POWER_PLANT", [5, 11], ["FLAG_FOUGHT_ZAPDOS", "FLAG_HIDE_ZAPDOS"], "Zapdos") + [
            heal(PC["ROUTE10"], "PowerPlantToRoute10PC"),
            save(),
        ],
    },
]

SEGMENTS = (list(CHAPTER0) + CHAPTER1 + CHAPTER2 + CHAPTER3 + CHAPTER4 + CHAPTER5 + CHAPTER6 + CHAPTER7
            + CHAPTER8 + CHAPTER9 + CHAPTER10 + CHAPTER11)


#  Metatiles that map scripts change when a map loads (ON_LOAD setmetatile),
#  in their state at the start of the game.
INITIAL_STATE = {
    "script_tiles": [
        ["MAP_CELADON_CITY_GAME_CORNER", "CeladonCity_GameCorner_EventScript_HideRocketHideout"],
        ["MAP_ROCKET_HIDEOUT_B1F", "RocketHideout_B1F_EventScript_SetBarrier"],
        ["MAP_ROCKET_HIDEOUT_B4F", "RocketHideout_B4F_EventScript_SetBarrier"],
        #  Victory Road: a rock barrier on each floor until a boulder is
        #  pushed onto its floor switch (VAR_MAP_SCENE_VICTORY_ROAD_*). The
        #  3F one at (12, 12) stood in the route to that floor's boulder.
        ["MAP_VICTORY_ROAD_1F", "VictoryRoad_1F_EventScript_SetRockBarrier"],
        ["MAP_VICTORY_ROAD_2F", "VictoryRoad_2F_EventScript_SetRockBarrier1"],
        ["MAP_VICTORY_ROAD_2F", "VictoryRoad_2F_EventScript_SetRockBarrier2"],
        ["MAP_VICTORY_ROAD_3F", "VictoryRoad_3F_EventScript_SetRockBarrier"],
    ],
}


def spec():
    return {"segments": SEGMENTS, "initial_state": INITIAL_STATE}

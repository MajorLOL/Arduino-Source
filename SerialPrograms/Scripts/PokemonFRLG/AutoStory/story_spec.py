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


#  Where Celio stands in One Island's Pokemon Center from the Ruby on.
CELIO_AT_MACHINE = [15, 6]
CELIO_BY_STOOLS = [14, 8]


def talk(map_id, obj=None, xy=None, mode="talk", label=None, dirs=None, counter=False, alt_xy=None):
    t = {"map": map_id, "mode": mode, "counter": counter}
    if obj is not None:
        t["object"] = obj
    if xy is not None:
        t["xy"] = xy
    if dirs:
        t["dirs"] = dirs
    if alt_xy is not None:
        #  Where it may stand instead: tried when no text box comes up.
        t["alt_xy"] = alt_xy
    s = {"talk": t}
    if label:
        s["label"] = label
    return s


def cutscene(min_ms, max_ms, label=None, answer=None, battles=0):
    s = {"cutscene": [min_ms, max_ms]}
    if answer:
        s["answer"] = answer
    if battles:
        #  The scene is a failure with fewer battles (a NO to its question).
        s["battles"] = battles
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


def buy(map_id, clerk_xy, item_index, quantity, label, item=None, kind=None, up_to=False):
    """Buy from a Poke Mart clerk: item_index in the mart's list (decomp).
    item: ESCAPE_ROPE or MAX_REPEL, counted for the router (story.py,
    Builder.shortcuts). Bought before the balls "as many as the money
    allows" of the same visit.
    kind: "revive" or "restore" (FULL RESTORE, MAX POTION), the healing
    items of the League: the program keeps count of them. up_to: only what
    is missing up to the quantity."""
    b = {"index": item_index, "quantity": quantity}
    if item:
        b["item"] = item
    if kind:
        b["kind"] = kind
        if up_to:
            b["up_to"] = True
    return [talk(map_id, xy=clerk_xy, mode="plain", label=label, counter=True),
            {"buy": b, "label": label}]


#  ESCAPE ROPEs and MAX REPELs (story.py, Builder.shortcuts: the router uses
#  them where they pay). Their place in each Poke Mart's list.
ROPE_INDEX = {"MAP_LAVENDER_TOWN_MART": 7, "MAP_SAFFRON_CITY_MART": 4, "MAP_PEWTER_CITY_MART": 6,
              "MAP_FOUR_ISLAND_MART": 6}
MAX_REPEL_INDEX = {"MAP_FUCHSIA_CITY_MART": 5, "MAP_THREE_ISLAND_MART": 5, "MAP_FOUR_ISLAND_MART": 7}


def ropes(mart, count, label="BuyEscapeRopes"):
    return buy(mart, [2, 3], ROPE_INDEX[mart], count, label, item="ESCAPE_ROPE")


def max_repels(mart, count, label="BuyMaxRepels"):
    return buy(mart, [2, 3], MAX_REPEL_INDEX[mart], count, label, item="MAX_REPEL")


def use_item(words, party_slot, label):
    """Use an item from the bag's ITEMS pocket (found by words of its
    description) on a party slot: an evolution stone."""
    return {"use_item": {"words": words, "slot": party_slot}, "label": label}


def give_item(words, party_slot, label):
    """Give an item from the bag's ITEMS pocket (found by words of its
    description) to a party slot to hold. If it holds one already, YES: the
    two are switched (the old one goes to the bag)."""
    return {"use_item": {"words": words, "slot": party_slot, "give": True}, "label": label}


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

#  Fly (HM02, the house on Route 16): only Charizard in these teams can
#  learn it, the starter in party slot 1. From then on a trip to a Pokemon
#  Center flies when that is clearly faster (story.py, Builder.fly), and the
#  HMs after it in the TM Case move down one.
FLY_VARIANTS = ["CHARMANDER"]


def get_fly(label="Fly"):
    """HM02 from the woman in the Route 16 house (Route16_House_EventScript_Woman),
    taught to the starter (party slot 1) in place of its weakest move."""
    return [talk("MAP_ROUTE16_HOUSE", xy=[4, 2], mode="talk", label=label + "HM02"),
            cutscene(1500, 30000, "HM02"),
            setst(flags=["FLAG_GOT_HM02"]),
            teach(2, 1, "FLY", label="Teach FLY"),
            setst(abilities={"fly": True})]


def fly(heal_location, label=None, force=False):
    """Fly to a town (HEAL_LOCATION_*): where the Fly map has it now; else walk there.
    force: also from that town itself."""
    s = {"fly": heal_location, "label": label or "Fly"}
    if force:
        s["force"] = True
    return s


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
            #  Charizard (Lv. 36 for Erika) learns Fly.
            only(FLY_VARIANTS, get_fly("Route16House") + [save()]),
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
        #  Three ESCAPE ROPEs first: out of Silph Co. (twice) and the Pokemon
        #  Mansion.
        "steps": ropes("MAP_LAVENDER_TOWN_MART", 3) + buy("MAP_LAVENDER_TOWN_MART", [2, 3], 0, 5, "BuyGreatBalls") + [
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
            #  Three of its pages end a message (no advance arrow): 90 s.
            goto("MAP_FUCHSIA_CITY_SAFARI_ZONE_ENTRANCE", [[4, 3]], "ToSafariDesk", trigger=True),
            cutscene(4000, 90000, "Join the Safari Game", answer="yes"),
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
            only(FLY_VARIANTS, [fly("HEAL_LOCATION_LAVENDER_TOWN", "ToLavender")]),
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
            #  (With HM02 Fly in the TM Case the HMs after it move down one.)
            teach(2 + (v in FLY_VARIANTS), LAPRAS_SLOT[v], "SURF", forget=0),
            teach(3 + (v in FLY_VARIANTS), LAPRAS_SLOT[v], "STRENGTH", forget=3)]) for v in STARTERS]
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
            only(FLY_VARIANTS, [fly("HEAL_LOCATION_PALLET_TOWN", "ToPallet")]),
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


def league_room_steps(again=False):
    """again: the League again for prize money. Between its rooms the
    healing items are used sparingly (a REVIVE for who is down, a potion
    only for who is low), before the Champion less so."""
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
        mode = True if not again else "league_again_last" if name == "Lance" else "league_again"
        out.append({"heal_items": mode, "label": "Heal with items after " + name})
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
        "steps": buy("MAP_INDIGO_PLATEAU_POKEMON_CENTER_1F", [0, 7], 2, 15, "BuyFullRestore", kind="restore")
        + buy("MAP_INDIGO_PLATEAU_POKEMON_CENTER_1F", [0, 7], 4, 8, "BuyRevive", kind="revive")
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


def legendary(map_id, xy, flags, label, weaken=False, heal=True, later=False):
    """A legendary Pokemon after the Hall of Fame: A in front of it
    ("Gyaoo!"), a wild battle in which it gets the best ball in the bag
    (not the Master Ball: that is kept for the roaming legendary) until it
    is caught. Not caught (it fainted, the balls ran out): the step fails,
    and the save before it is tried again. weaken: its HP is brought down
    first (catch rate 3: a ball catches it about three times as often in
    the red). Caught: saved on the spot, so that nothing on the way back
    (a walk that goes wrong, the tired team fainting) loses it again
    (Cerulean Cave: Mewtwo caught eleven times, lost on the way out each
    time). heal: then the tired team is healed from the bag (REVIVE, FULL
    RESTORE on the first two), after the save, so that every try of the way
    back starts with it (Cerulean Cave: a WOBBUFFET, which cannot be run
    from, made the team faint). later: the story comes back for it
    (moltres_again): with too few balls in the bag (they ran out before a
    try had a fair chance) it is left where it is, the save in front of it
    loaded once more, and the steps after it go on (Moltres with one ULTRA
    BALL and two POKe BALLs: 19 tries in an hour)."""
    return [talk(map_id, xy=xy, mode="talk", label=label),
            {"battle": 60000, "label": label, "legendary": True, "weaken": weaken, "later": later},
            setst(flags=flags),
            save()] + ([heal_after_legendary(label)] if heal else [])


def heal_after_legendary(label):
    return {"heal_items": "after_legendary", "label": "Heal with items after " + label}


#  Seafoam Islands: the holes. A boulder pushed into one falls to the floor
#  below; the player is kept off them (the floor below has currents: falling
#  on B3F rides the current to B4F, SeafoamIslands_B3F_EventScript_EnterByFalling).
SEAFOAM_HOLES = {
    "MAP_SEAFOAM_ISLANDS_1F": [[21, 8], [30, 8]],
    "MAP_SEAFOAM_ISLANDS_B1F": [[23, 8], [28, 8]],
    "MAP_SEAFOAM_ISLANDS_B2F": [[24, 8], [27, 8]],
    "MAP_SEAFOAM_ISLANDS_B3F": [[6, 18], [9, 18]],
}


#  After the Hall of Fame: the legendary Pokemon. Zapdos and Articuno
#  (Seafoam Islands: boulders stop the current) are weakened, then Ultra
#  Balls; the Master Ball is kept for the roaming legendary (it flees at
#  once; it only starts roaming after the Sapphire is given to Celio, the end
#  of the Sevii Islands story). Moltres (Mt. Ember: the boat to the Sevii
#  Islands) comes in chapter 12; Mewtwo needs the whole Sevii Islands story
#  (chapter 19).
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
            #  A save from a build before Fly was in the story: HM02 now (the
            #  program skips this when the starter knows FLY), then back here.
            only(FLY_VARIANTS, [{"unless_fly": get_fly("PostgameRoute16House") + [
                fly("HEAL_LOCATION_PALLET_TOWN", "BackToPallet")], "label": "Knows FLY?"}]),
            fly("HEAL_LOCATION_SAFFRON_CITY", "ToSaffron"),
            #  SilphCo_11F_EventScript_President: the Master Ball, any time after Giovanni.
            talk("MAP_SILPH_CO_11F", xy=[9, 9], mode="talk", label="SilphPresident"),
            cutscene(1000, 30000, "Master Ball"),
            setst(flags=["FLAG_GOT_MASTER_BALL_FROM_SILPH"]),
            #  An ESCAPE ROPE: out of the Seafoam Islands after Articuno.
        ] + ropes("MAP_SAFFRON_CITY_MART", 1) + [
            heal(PC["SAFFRON"], "SilphToSaffronPC"),
            save(),
        ],
    },
    {
        "id": "C11_02_ZAPDOS", "chapter": 11,
        "title": "Zapdos in the Power Plant (Ultra Balls; the Master Ball is kept for the roaming legendary)",
        "steps": [
            #  Ultra Balls in Fuchsia City, as many as the money allows
            #  (quantity 0): for Zapdos, what is left for Articuno.
            heal(PC["FUCHSIA"], "SaffronToFuchsiaPC"),
            #  MAX REPELs first: the Power Plant, the Seafoam Islands.
        ] + max_repels("MAP_FUCHSIA_CITY_MART", 2) + buy("MAP_FUCHSIA_CITY_MART", [2, 3], 0, 0, "BuyUltraBalls") + [
            heal(PC["ROUTE10"], "FuchsiaToRoute10PC"),
            save(),
        ] + legendary("MAP_POWER_PLANT", [5, 11], ["FLAG_FOUGHT_ZAPDOS", "FLAG_HIDE_ZAPDOS"], "Zapdos",
                      weaken=True) + [
            heal(PC["ROUTE10"], "PowerPlantToRoute10PC"),
            save(),
        ],
    },
]

CHAPTER11.append({
    "id": "C11_03_ARTICUNO", "chapter": 11,
    "title": "Articuno in the Seafoam Islands: two boulders stop the current on B4F (Ultra Balls)",
    #  Route20_OnTransition puts every boulder back while the currents are
    #  not stopped: as at the start of the game.
    "state": {
        "tiles": {mid: {"%d,%d" % (x, y): {"collision": 1} for x, y in holes} for mid, holes in SEAFOAM_HOLES.items()},
    },
    "steps": [
        #  Fuchsia City: Route 19 and 20 by Surf to the east entrance. The
        #  Ultra Balls were bought before Zapdos.
        heal(PC["FUCHSIA"], "Route10ToFuchsiaPC"),
        save(),
        #  B3F: boulders 3 and 5 into the holes. Each comes down on B4F in
        #  front of the current there (the flag in its trainer_type field,
        #  HandleBoulderFallThroughHole). Boulders 4 and 6 only stand in the way.
        goto("MAP_SEAFOAM_ISLANDS_B3F", [[8, 17]], "ToSeafoamB3F"),
        {"boulder": {"target": [6, 18], "hole": True, "from": [6, 17]}, "label": "SeafoamB3F_Boulder3"},
        setst(flags=["FLAG_HIDE_SEAFOAM_B3F_BOULDER_3"], clear_flags=["FLAG_HIDE_SEAFOAM_B4F_BOULDER_1"]),
        {"boulder": {"target": [9, 18], "hole": True, "from": [12, 16]}, "label": "SeafoamB3F_Boulder5"},
        setst(flags=["FLAG_HIDE_SEAFOAM_B3F_BOULDER_5"], clear_flags=["FLAG_HIDE_SEAFOAM_B4F_BOULDER_2"]),
        #  SeafoamIslands_B4F_OnTransition: two boulders there, the current
        #  stops (setmaplayoutindex LAYOUT_SEAFOAM_ISLANDS_B4F_CURRENT_STOPPED).
        setst(flags=["FLAG_STOPPED_SEAFOAM_B4F_CURRENT"],
              layouts={"MAP_SEAFOAM_ISLANDS_B4F": "LAYOUT_SEAFOAM_ISLANDS_B4F_CURRENT_STOPPED"}),
        talk("MAP_SEAFOAM_ISLANDS_B4F", xy=[22, 19], mode="talk", label="SeafoamUltraBall"),
        setst(flags=["FLAG_HIDE_SEAFOAM_ISLANDS_B4F_ULTRA_BALL"]),
        goto("MAP_SEAFOAM_ISLANDS_B4F", [[9, 3]], "ToArticuno"),
        save(),
    ] + legendary("MAP_SEAFOAM_ISLANDS_B4F", [9, 2], ["FLAG_FOUGHT_ARTICUNO", "FLAG_HIDE_ARTICUNO"], "Articuno",
                  weaken=True) + [
        heal(PC["FUCHSIA"], "SeafoamToFuchsiaPC"),
        save(),
    ],
})

# ---------------------------------------------------------------- chapter 12
#  The Sevii Islands with Bill (the first time: Islands 1-3). The way to the
#  National Pokedex, the Ruby and the Sapphire, and so to the roaming
#  legendary. On this first visit the ferry only goes between the islands
#  (EventScript_ChooseDestFromOneIsland: Kanto once VAR_MAP_SCENE_CINNABAR_
#  ISLAND >= 4), and Fly there only has the islands' map: back to Kanto only
#  with Bill, once Lostelle is found and the Meteorite delivered.


def ferry(harbor_map, option, dest_harbor, label):
    """The Seagallop from a Sevii harbor: the sailor (8,6), the destination
    menu (option: its row), the ferry, then the harbor it arrives at (8,5)."""
    steps = [talk(harbor_map, xy=[8, 6], mode="plain", label=label),
             press("A", 1, 2500, label + "_Talk")]
    if option:
        steps.append(press("DOWN", option, 500, label + "_Menu"))
    return steps + [press("A", 1, 1500, label + "_Pick"),
                    cutscene(8000, 90000, "Ferry"),
                    pos(dest_harbor, 8, 5)]


#  HM06 Rock Smash: Lapras learns it in place of HYDRO PUMP (Surf hits as
#  Water already); Charizard keeps its moves. HMs in the TM Case: 01, (02),
#  03, 04, 06.
ROCK_SMASH_RANK = {v: 4 + (v in FLY_VARIANTS) for v in STARTERS}

CHAPTER12 = [
    {
        "id": "C12_01_ONE_ISLAND", "chapter": 12,
        "title": "With Bill to One Island: Celio, the Tri-Pass",
        "steps": [
            fly("HEAL_LOCATION_CINNABAR_ISLAND", "ToCinnabar"),
            #  CinnabarIsland_PokemonCenter_1F_EventScript_Bill: YES. Then one
            #  scene after the other: out to the harbor, the boat, the ferry,
            #  One Island (setworldmapflag FLAG_WORLD_MAP_ONE_ISLAND), Bill
            #  walks to the Pokemon Center, Celio (MeetCelioScene): the
            #  Meteorite, the Tri-Pass. The player stands at (14,7) then.
            talk("MAP_CINNABAR_ISLAND_POKEMON_CENTER_1F", xy=[11, 5], mode="yes", label="BillSevii"),
            cutscene(150000, 480000, "With Bill to One Island, Celio"),
            setst(flags=["FLAG_HIDE_CINNABAR_POKECENTER_BILL", "FLAG_HIDE_CINNABAR_BILL", "FLAG_HIDE_CINNABAR_SEAGALLOP",
                         "FLAG_HIDE_ONE_ISLAND_BILL", "FLAG_SYS_SEVII_MAP_123", "FLAG_SYS_PC_STORAGE_DISABLED",
                         "FLAG_WORLD_MAP_ONE_ISLAND"],
                  vars={"VAR_MAP_SCENE_CINNABAR_ISLAND_2": 2, "VAR_MAP_SCENE_CINNABAR_ISLAND": 2,
                        "VAR_MAP_SCENE_ONE_ISLAND_HARBOR": 3, "VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F": 1}),
            pos("MAP_ONE_ISLAND_POKEMON_CENTER_1F", 14, 7, "up"),
            heal("MAP_ONE_ISLAND_POKEMON_CENTER_1F", "OneIslandNurse"),
            save(),
        ],
    },
    {
        "id": "C12_02_ROCK_SMASH", "chapter": 12,
        "title": "Kindle Road to the Ember Spa: HM06 Rock Smash for Lapras",
        "steps": [
            talk("MAP_ONE_ISLAND_KINDLE_ROAD_EMBER_SPA", xy=[10, 5], mode="talk", label="RockSmashMan"),
            cutscene(1500, 30000, "HM06"),
            setst(flags=["FLAG_GOT_HM06"]),
        ] + [only([v], [{"teach": {"hm_rank": ROCK_SMASH_RANK[v], "party_slot": LAPRAS_SLOT[v],
                                    "move": "ROCK SMASH|HYDRO PUMP", "forget": -1}, "label": "Teach ROCK SMASH"}])
             for v in STARTERS] + [
            setst(abilities={"rock_smash": True}),
            heal("MAP_ONE_ISLAND_POKEMON_CENTER_1F", "SpaToOneIslandPC"),
            save(),
        ],
    },
    {
        "id": "C12_03_TWO_ISLAND", "chapter": 12,
        "title": "Two Island: Lostelle is missing",
        "steps": ferry("MAP_ONE_ISLAND_HARBOR", 0, "MAP_TWO_ISLAND_HARBOR", "FerryToTwoIsland") + [
            #  TwoIsland_JoyfulGameCorner_EventScript_HelpFindLostelleScene
            #  (on entering): Lostelle's daddy, a biker. The player ends
            #  next to the door at (4,8), the daddy at (6,8).
            goto("MAP_TWO_ISLAND_JOYFUL_GAME_CORNER", [[5, 8]], "ToJoyfulGameCorner"),
            cutscene(5000, 120000, "Lostelle is missing"),
            setst(vars={"VAR_MAP_SCENE_TWO_ISLAND_JOYFUL_GAME_CORNER": 1, "VAR_MAP_SCENE_THREE_ISLAND": 2},
                  clear_flags=["FLAG_HIDE_THREE_ISLAND_LONE_BIKER"],
                  objects={"MAP_TWO_ISLAND_JOYFUL_GAME_CORNER": {"LOCALID_JOYFUL_GAME_CORNER_DADDY": {"x": 6, "y": 8}}}),
            pos("MAP_TWO_ISLAND_JOYFUL_GAME_CORNER", 4, 8, "right"),
            heal("MAP_TWO_ISLAND_POKEMON_CENTER_1F", "GameCornerToTwoIslandPC"),
            save(),
        ],
    },
    {
        "id": "C12_04_THREE_ISLAND", "chapter": 12,
        "title": "Three Island: the bikers, Ultra Balls",
        "steps": ferry("MAP_TWO_ISLAND_HARBOR", 1, "MAP_THREE_ISLAND_HARBOR", "FerryToThreeIsland") + [
            #  ThreeIsland_EventScript_BikerBossIntroTrigger (row 27), then
            #  ThreeIsland_EventScript_BattleBikersScene (row 26): YES, three
            #  bikers and Cue Ball Paxton one after the other.
            goto("MAP_THREE_ISLAND", [[9, 27]], "BikerBossIntro", trigger=True),
            cutscene(3000, 120000, "Paxton and the islanders"),
            setst(vars={"VAR_MAP_SCENE_THREE_ISLAND": 3}),
            pos("MAP_THREE_ISLAND", 9, 27, "up"),
            #  Healed and saved before the bikers: a NO to "You wanna make
            #  something of it" (ThreeIsland_EventScript_LeaveBikersAlone)
            #  steps the player back and the bikers stay. The scene then
            #  fails (no battle) and goes again from this save.
            heal("MAP_THREE_ISLAND_POKEMON_CENTER_1F", "ToThreeIslandPC"),
            save(),
            goto("MAP_THREE_ISLAND", [[9, 26]], "BattleBikers", trigger=True),
            cutscene(3000, 600000, "The bikers", answer="yes", battles=1),
            setst(vars={"VAR_MAP_SCENE_THREE_ISLAND": 4},
                  flags=["FLAG_HIDE_THREE_ISLAND_BIKERS", "FLAG_HIDE_THREE_ISLAND_LONE_BIKER"]),
            pos("MAP_THREE_ISLAND", 9, 26, "up"),
            heal("MAP_THREE_ISLAND_POKEMON_CENTER_1F", "BikersToThreeIslandPC"),
            #  The money went into Ultra Balls in Fuchsia City before
            #  Zapdos; the trainers since then paid for three more (the
            #  second playthrough: "10 Ultra Balls" here were 1, and
            #  Moltres got 1 Ultra Ball and 2 Poke Balls a try). With
            #  Charmander none are bought here: Moltres gets the balls left
            #  from Articuno, and with too few it is left for chapter 13
            #  (moltres_again, after the League's prize money). With the
            #  others the story ends after this chapter: 11 Ultra Balls, or
            #  as many as the money allows.
        ] + max_repels("MAP_THREE_ISLAND_MART", 2) + [
            only([v for v in STARTERS if v not in FLY_VARIANTS],
                 buy("MAP_THREE_ISLAND_MART", [2, 3], 0, 11, "BuyUltraBalls")),
            heal("MAP_THREE_ISLAND_POKEMON_CENTER_1F", "MartToThreeIslandPC"),
            save(),
        ],
    },
    {
        "id": "C12_05_LOSTELLE", "chapter": 12,
        "title": "Bond Bridge, Berry Forest: Lostelle; the Meteorite",
        "steps": [
            #  ThreeIsland_BerryForest_EventScript_Lostelle: a wild Hypno
            #  (run), then everyone is back in the Joyful Game Corner
            #  (6,6), facing up (OnWarp), Lostelle and her daddy talk.
            talk("MAP_THREE_ISLAND_BERRY_FOREST", xy=[4, 8], mode="talk", label="Lostelle"),
            battle("Hypno", 30000, flee=True),
            cutscene(3000, 120000, "Lostelle found"),
            setst(flags=["FLAG_RESCUED_LOSTELLE", "FLAG_HIDE_LOSTELLE_IN_BERRY_FOREST"],
                  clear_flags=["FLAG_HIDE_TWO_ISLAND_GAME_CORNER_LOSTELLE"],
                  vars={"VAR_MAP_SCENE_TWO_ISLAND_JOYFUL_GAME_CORNER": 3}),
            pos("MAP_TWO_ISLAND_JOYFUL_GAME_CORNER", 6, 6, "up"),
            #  TwoIsland_JoyfulGameCorner_EventScript_GiveDaddyMeteorite.
            talk("MAP_TWO_ISLAND_JOYFUL_GAME_CORNER", xy=[5, 5], mode="talk", label="LostellesDaddy"),
            cutscene(1500, 60000, "The Meteorite, a Moon Stone"),
            setst(flags=["FLAG_GOT_MOON_STONE_FROM_JOYFUL_GAME_CORNER"],
                  vars={"VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F": 2}),
            heal("MAP_TWO_ISLAND_POKEMON_CENTER_1F", "GameCornerToTwoIslandPC2"),
            save(),
        ],
    },
    {
        "id": "C12_06_MOLTRES", "chapter": 12,
        "title": "Mt. Ember: Moltres at the summit (weakened, Ultra Balls)",
        "steps": ferry("MAP_TWO_ISLAND_HARBOR", 0, "MAP_ONE_ISLAND_HARBOR", "FerryToOneIsland") + [
            #  Mt. Ember: boulders (Strength) and rocks (Rock Smash) on the way
            #  to the Summit Path (14,24); at the summit four boulders before
            #  Moltres (9,6).
            goto("MAP_MT_EMBER_EXTERIOR", [[28, 47]], "ToMtEmber"),
            {"boulder": {"open_to": [[14, 25]]}, "label": "MtEmberBoulders"},
            goto("MAP_MT_EMBER_SUMMIT", [[9, 14]], "ToMtEmberSummit"),
            {"boulder": {"open_to": [[9, 7], [8, 6], [10, 6]]}, "label": "SummitBoulders"},
            goto("MAP_MT_EMBER_SUMMIT", [[9, 7]], "ToMoltres"),
            save(),
            #  With Charmander the story comes back for it (chapter 13)
            #  when the bag has too few balls; with the others it ends
            #  after this chapter.
        ] + [only([v], legendary("MAP_MT_EMBER_SUMMIT", [9, 6], ["FLAG_FOUGHT_MOLTRES", "FLAG_HIDE_MOLTRES"], "Moltres",
                                 weaken=True, heal=False, later=v in FLY_VARIANTS)) for v in STARTERS] + [
            #  Caught (and saved): loaded again, so Strength is off whether
            #  it took one try or several. The boulders stay where they were
            #  pushed on the way up (loading a save keeps them there): the
            #  way down is through them again.
            {"reload": True, "label": "Reload after Moltres"},
            heal_after_legendary("Moltres"),
            {"boulder": {"open_to": [[9, 14]]}, "label": "SummitBouldersBack"},
            #  Mt. Ember loads again on the way down: its boulders are back.
            setst(reset_objects=["MAP_MT_EMBER_EXTERIOR"]),
            heal("MAP_ONE_ISLAND_POKEMON_CENTER_1F", "SummitToOneIslandPC"),
            save(),
        ],
    },
    {
        "id": "C12_07_BACK_TO_KANTO", "chapter": 12,
        "title": "Back to Kanto with Bill",
        "steps": [
            #  OneIsland_PokemonCenter_1F_EventScript_LeaveOneIslandScene
            #  (x 12, VAR 2): Bill and Celio, the ferry to Cinnabar, then
            #  CinnabarIsland_EventScript_ReturnFromSeviiIslands. The ferry
            #  lands at (21,7) (sSeag).
            goto("MAP_ONE_ISLAND_POKEMON_CENTER_1F", [[12, 8]], "BillLeaves", trigger=True),
            cutscene(60000, 300000, "Back to Kanto"),
            setst(flags=["FLAG_HIDE_ONE_ISLAND_POKECENTER_BILL", "FLAG_HIDE_TWO_ISLAND_GAME_CORNER_LOSTELLE"],
                  clear_flags=["FLAG_HIDE_LOSTELLE_IN_HER_HOME", "FLAG_SYS_PC_STORAGE_DISABLED"],
                  vars={"VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F": 3, "VAR_MAP_SCENE_CINNABAR_ISLAND": 4}),
            pos("MAP_CINNABAR_ISLAND", 21, 7),
            heal(PC["CINNABAR"], "HarborToCinnabarPC"),
            save(),
        ],
    },
]


# ---------------------------------------------------------------- chapter 13
#  The National POKeDEX: Oak gives it in Pallet Town after a Hall of Fame
#  (PalletTown_EventScript_OakRatingScene, VAR_MAP_SCENE_PALLET_TOWN_OAK 2)
#  once 60 Kanto species are caught (GetKantoPokedexCount) and One Island
#  was visited. Celio asks for the Ruby only then; with the Ruby and the
#  Sapphire (two gems, the key items of the Sevii Islands story) in his
#  Network Machine, the roaming legendary (Suicune for the Charmander
#  player) starts roaming Kanto.
#  The balls for some 50 species cost more than the story leaves (some 300
#  POKe BALLs, 60000): the Elite Four three times more first, for the prize
#  money (29100 a run, some 19000 of it left after the REVIVEs and MAX
#  POTIONs it takes: league_kit()); Oak's rating after each Hall of Fame
#  says it is not 60 yet.

#  Species caught for sure by the program in the story: the starter's line,
#  the team (Voltorb -> Electrode, Eevee and its evolution, Lapras) and the
#  three legendary birds.
STARTER_LINE = {"BULBASAUR": ["bulbasaur", "ivysaur", "venusaur"],
                "SQUIRTLE": ["squirtle", "wartortle", "blastoise"],
                "CHARMANDER": ["charmander", "charmeleon", "charizard"]}


def dex_owned(v):
    owned = list(STARTER_LINE[v])
    for slots, lines in TEAM_MEMBERS.values():
        owned += lines[v].split(",")
    return owned + ["zapdos", "articuno", "moltres"]


LEAGUE_FLAGS = ["FLAG_DEFEATED_LORELEI", "FLAG_DEFEATED_BRUNO", "FLAG_DEFEATED_AGATHA",
                "FLAG_DEFEATED_LANCE", "FLAG_DEFEATED_CHAMP"]


#  The healing items of a League run for prize money, from the Indigo
#  Plateau's Poke Mart (IndigoPlateau_PokemonCenter_1F_Items: ULTRA BALL,
#  GREAT BALL, FULL RESTORE, MAX POTION, REVIVE, ...): REVIVEs (1500) and MAX
#  POTIONs (2500: all the HP like a FULL RESTORE, 500 cheaper). Without any,
#  the team reaches the Champion worn out and loses (the second League
#  again: four of six down in front of the Champion, 21 tries lost). With
#  four of each, used sparingly between the rooms (league_room_steps(again)),
#  the League is won at the first go nearly every time, for about 2.5 REVIVEs
#  and 2.5 MAX POTIONs: some 19000 of the 29100 the five battles pay are
#  left (league_again_sim.py, on team_sim.py). Only what is missing up to
#  four is bought (the program counts what it uses), as far as the money
#  goes, the REVIVEs first.
LEAGUE_KIT = 4


def league_kit():
    mart = "MAP_INDIGO_PLATEAU_POKEMON_CENTER_1F"
    return (buy(mart, [0, 7], 4, LEAGUE_KIT, "BuyRevives", kind="revive", up_to=True)
            + buy(mart, [0, 7], 3, LEAGUE_KIT, "BuyMaxPotions", kind="restore", up_to=True))


def league_again(prefix, chapter, why):
    """The Elite Four and the Champion again (EventScript_ResetEliteFour at
    each Hall of Fame), from the Indigo Plateau, then Oak's rating in Pallet
    Town.
    The nurse first, then the Poke Mart's counter in the same room: a loss
    the program leaves standing (the save in front of a room worn out, or
    lost three times) puts the player in front of that nurse, and the run
    goes on from the step after her, with healing items bought from what
    the rooms paid."""
    return [
        {
            "id": prefix + "_ELITE_FOUR", "chapter": chapter,
            "title": "Elite Four again: Lorelei, Bruno, Agatha, Lance (" + why + ")",
            "state": {"clear_flags": LEAGUE_FLAGS, "vars": {"VAR_MAP_SCENE_POKEMON_LEAGUE": 0}},
            "steps": [heal(PC["INDIGO"], "ToIndigoPlateau")] + league_kit() + [save(), team()]
            + league_room_steps(again=True),
        },
        {
            "id": prefix + "_CHAMPION", "chapter": chapter,
            "title": "Champion again, Hall of Fame",
            "steps": [
                team(),
                goto("MAP_POKEMON_LEAGUE_CHAMPIONS_ROOM", [[6, 19]], "ToChampion"),
                cutscene(6000, 60000, "Champion intro"),
                battle("Champion (rival)", 60000),
                {"hall_of_fame": 600000, "label": "Oak, Hall of Fame, credits"},
            ],
        },
    ]


def oak_rating(label):
    """In front of the house after the Hall of Fame: Oak comes, looks at the
    POKeDEX (OakRatingScene)."""
    return [pos("MAP_PALLET_TOWN", 6, 8, "down"),
            cutscene(3000, 90000, label),
            setst(vars={"VAR_MAP_SCENE_PALLET_TOWN_OAK": 3})]


def vermilion_ferry(option, dest_harbor, label):
    """The Seagallop from Vermilion: the ticket check by the pier (x 22-23,
    y 33). VermilionCity_EventScript_ChooseSeagallopDestTriPass (and
    ...RainbowPass): "Ah, you have a TRI-PASS." first, A to the next page,
    then "Would you like to board a SEAGALLOP ferry?" with the destinations
    (One, Two, Three Island). The cutscene's B on that menu is a cancel: the
    player walks back up the pier (the first try of C14_10)."""
    steps = [goto("MAP_VERMILION_CITY", [[23, 33]], label, trigger=True),
             wait(2500, label + "_Pass"),
             press("A", 1, 2000, label + "_Menu")]
    if option:
        steps.append(press("DOWN", option, 500, label + "_Down"))
    return steps + [press("A", 1, 1500, label + "_Pick"),
                    cutscene(8000, 90000, "Ferry"),
                    pos(dest_harbor, 8, 5)]


def sevii_ferry_home(harbor_map, label):
    """Back to Vermilion from a Sevii harbor: Vermilion is the first of the
    destinations now (EventScript_ChooseDest*VermilionAllowed). The ferry
    lands by the pier (23, 32)."""
    return [talk(harbor_map, xy=[8, 6], mode="plain", label=label),
            press("A", 1, 2500, label + "_Talk"),
            press("A", 1, 1500, label + "_Pick"),
            cutscene(8000, 90000, "Ferry"),
            pos("MAP_VERMILION_CITY", 23, 32)]


def moltres_again():
    """Moltres, left on Mt. Ember in chapter 12 with too few balls in the bag
    (legendary(later=True)): back for it with balls from the League's prize
    money. 21 GREAT BALLs (12600): about one try in four catches it. The
    POKeDEX needs that money as well, so not ULTRA BALLs (twice the price
    for a third more catches); and not POKe BALLs, cheaper still, because a
    try has room for some 25 to 35 balls only: Moltres wears the team down
    turn after turn (FIRE SPIN's trap, FLAMETHROWER). Balls thrown in a try
    that fails come back with the save. From Pallet Town (after Oak's
    rating) and back there by FLY; the program skips all of it unless it
    left Moltres there.
    Moltres is on the summit again and the boulders are back where the game
    puts them, as at the start of C12_06; the trainers on the way are
    beaten."""
    return [
        setst(clear_flags=["FLAG_FOUGHT_MOLTRES", "FLAG_HIDE_MOLTRES"],
              reset_objects=["MAP_MT_EMBER_SUMMIT", "MAP_MT_EMBER_EXTERIOR"]),
        heal(PC["FUCHSIA"], "MoltresToFuchsiaPC"),
        #  Fuchsia City: MAX REPELs for Mt. Ember, 21 GREAT BALLs (the second
        #  item of its list; one purchase: RIGHT twice).
    ] + max_repels("MAP_FUCHSIA_CITY_MART", 2) + buy("MAP_FUCHSIA_CITY_MART", [2, 3], 1, 21, "BuyGreatBalls") + [
        heal(PC["VERMILION"], "FuchsiaToVermilionPC"),
        save(),
    ] + vermilion_ferry(0, "MAP_ONE_ISLAND_HARBOR", "FerryToOneIsland") + [
        heal("MAP_ONE_ISLAND_POKEMON_CENTER_1F", "HarborToOneIslandPC"),
        save(),
        goto("MAP_MT_EMBER_EXTERIOR", [[28, 47]], "ToMtEmber"),
        {"boulder": {"open_to": [[14, 25]]}, "label": "MtEmberBoulders"},
        goto("MAP_MT_EMBER_SUMMIT", [[9, 14]], "ToMtEmberSummit"),
        {"boulder": {"open_to": [[9, 7], [8, 6], [10, 6]]}, "label": "SummitBoulders"},
        goto("MAP_MT_EMBER_SUMMIT", [[9, 7]], "ToMoltres"),
        save(),
    ] + legendary("MAP_MT_EMBER_SUMMIT", [9, 6], ["FLAG_FOUGHT_MOLTRES", "FLAG_HIDE_MOLTRES"], "Moltres",
                  weaken=True, heal=False) + [
        {"reload": True, "label": "Reload after Moltres"},
        heal_after_legendary("Moltres"),
        {"boulder": {"open_to": [[9, 14]]}, "label": "SummitBouldersBack"},
        setst(reset_objects=["MAP_MT_EMBER_EXTERIOR"]),
        heal("MAP_ONE_ISLAND_POKEMON_CENTER_1F", "SummitToOneIslandPC"),
        save(),
    ] + sevii_ferry_home("MAP_ONE_ISLAND_HARBOR", "MoltresFerryHome") + [
        fly("HEAL_LOCATION_PALLET_TOWN", "MoltresBackToPallet"),
    ]


CHAPTER13 = league_again("C13_01", 13, "prize money for Poke Balls") + [
    {
        "id": "C13_01_OAK", "chapter": 13,
        "title": "Oak's POKeDEX rating (not 60 species yet)",
        "state": {"vars": {"VAR_MAP_SCENE_PALLET_TOWN_OAK": 2}},
        "steps": oak_rating("Oak: not 60 species yet") + [save()],
    },
    {
        "id": "C13_01_MOLTRES", "chapter": 13,
        "title": "Moltres, if it was left on Mt. Ember (Great Balls from the prize money)",
        "steps": [{"if_left": {"labels": ["Moltres"], "steps": moltres_again()}, "label": "Moltres left?"},
                  save()],
    },
] + league_again("C13_02", 13, "more prize money") + [
    {
        "id": "C13_02_OAK", "chapter": 13,
        "title": "Oak's POKeDEX rating (not 60 species yet)",
        "state": {"vars": {"VAR_MAP_SCENE_PALLET_TOWN_OAK": 2}},
        "steps": oak_rating("Oak: not 60 species yet") + [save()],
    },
] + league_again("C13_03", 13, "prize money once more") + [
    {
        "id": "C13_03_OAK", "chapter": 13,
        "title": "Oak's POKeDEX rating (not 60 species yet)",
        "state": {"vars": {"VAR_MAP_SCENE_PALLET_TOWN_OAK": 2}},
        "steps": oak_rating("Oak: not 60 species yet") + [save()],
    },
]


# ---------------------------------------------------------------- chapter 14
#  Catching for the POKeDEX. Each place: back and forth on its grass (cave
#  floor), every species of its list not caught yet gets POKe BALLs (the
#  cheapest catch for the money: at full HP, the team would knock it out),
#  up to 20 a battle; the others are run from. A place is left once all are
#  caught, 25 battles in a row brought none, or its time is up. Species of
#  both games are listed (FireRed and LeafGreen differ in some).


#  Species the National POKeDEX needs (OakRatingScene): a DEX step catches
#  nothing once the game counts that many (the load menu's POKeDEX number).
DEX_NEEDED = 60


def dex(map_id, tiles, species, label, idle=25, minutes=30, safari=False):
    return {"dex": {"map": map_id, "tiles": tiles, "species": species, "idle": idle,
                    "minutes": minutes, "safari": safari, "enough": DEX_NEEDED}, "label": label}


def safari_game(area_map, tiles, species, label):
    """One Safari Game (500): SAFARI BALLs at every species of the list until
    it is caught or flees, until the 30 balls or the 600 steps run out; the
    PA's text, then the gate (FuchsiaCity_SafariZone_Entrance_EventScript_
    ExitWarpIn / ExitWalkIn: both end at (4, 4))."""
    return [
        goto("MAP_FUCHSIA_CITY_SAFARI_ZONE_ENTRANCE", [[4, 3]], label + "_Desk", trigger=True),
        cutscene(4000, 90000, "Join the Safari Game", answer="yes"),
        setst(vars={"VAR_MAP_SCENE_FUCHSIA_CITY_SAFARI_ZONE_ENTRANCE": 2}),
        pos("MAP_SAFARI_ZONE_CENTER", 26, 30, "up"),
        dex(area_map, tiles, species, label, safari=True),
        cutscene(3000, 40000, "The Safari Game is over"),
        setst(vars={"VAR_MAP_SCENE_FUCHSIA_CITY_SAFARI_ZONE_ENTRANCE": 0}),
        pos("MAP_FUCHSIA_CITY_SAFARI_ZONE_ENTRANCE", 4, 4, "down"),
    ]


def poke_balls(mart, label):
    """As many POKe BALLs as the money allows (99 at most)."""
    return buy(mart, [2, 3], 0, 0, label)


WHOLE = rect(0, 0, 120, 120)

CHAPTER14 = [
    {
        "id": "C14_01_VIRIDIAN_FOREST", "chapter": 14,
        "title": "POKeDEX: Poke Balls in Viridian City, Viridian Forest",
        #  81 POKe BALLs (one purchase: RIGHT eight times), not as many as
        #  the money allows: the ESCAPE ROPEs in Pewter City come first
        #  there.
        "steps": [heal(PC["VIRIDIAN"], "ToViridianPC")] + buy("MAP_VIRIDIAN_CITY_MART", [2, 3], 0, 81, "BuyPokeBalls") + [
            dex("MAP_VIRIDIAN_FOREST", rect(3, 55, 42, 61), ["caterpie", "weedle", "kakuna", "metapod", "pikachu"],
                "DexViridianForest"),
            heal(PC["PEWTER"], "ForestToPewterPC"),
            save(),
        ],
    },
    {
        "id": "C14_02_ROUTE3_MT_MOON", "chapter": 14,
        "title": "POKeDEX: more Poke Balls in Pewter City, Route 3, a Magikarp, Mt. Moon",
        #  ESCAPE ROPEs first: out of Mt. Moon, the Pokemon Tower, the
        #  Seafoam Islands, Berry Forest and Ruby Path.
        "steps": ropes("MAP_PEWTER_CITY_MART", 5) + poke_balls("MAP_PEWTER_CITY_MART", "BuyPokeBalls") + [
            dex("MAP_ROUTE3", WHOLE, ["spearow", "pidgey", "nidoran-m", "nidoran-f", "mankey", "jigglypuff"],
                "DexRoute3"),
            heal(PC["ROUTE4"], "Route3ToMtMoonPC"),
            save(),
            #  Route4_PokemonCenter_1F_EventScript_MagikarpSalesman: YES
            #  (500), NO to the nickname.
            talk(PC["ROUTE4"], xy=[1, 3], mode="yes", label="MagikarpSalesman"),
            cutscene(2000, 30000, "Magikarp"),
            setst(flags=["FLAG_BOUGHT_MAGIKARP"]),
            #  B2F: CLEFAIRY there 6% of the time (1F: 1%).
            dex("MAP_MT_MOON_B2F", WHOLE, ["zubat", "geodude", "paras", "clefairy"], "DexMtMoon"),
            heal(PC["ROUTE4"], "MtMoonToPC"),
            save(),
        ],
    },
    {
        "id": "C14_03_POKEMON_TOWER", "chapter": 14,
        "title": "POKeDEX: Pokemon Tower",
        "steps": [
            heal(PC["LAVENDER"], "ToLavenderPC"),
            dex("MAP_POKEMON_TOWER_7F", WHOLE, ["gastly", "haunter", "cubone"], "DexPokemonTower"),
            heal(PC["LAVENDER"], "TowerToLavenderPC"),
            save(),
        ],
    },
    {
        "id": "C14_04_POWER_PLANT", "chapter": 14,
        "title": "POKeDEX: Power Plant",
        "steps": [
            heal(PC["ROUTE10"], "ToRoute10PC"),
            dex("MAP_POWER_PLANT", WHOLE, ["magnemite", "pikachu", "magneton"], "DexPowerPlant"),
            heal(PC["ROUTE10"], "PowerPlantToRoute10PC"),
            save(),
        ],
    },
    {
        "id": "C14_05_SAFARI_ZONE", "chapter": 14,
        "title": "POKeDEX: two Safari Games (center, east)",
        "steps": [heal(PC["FUCHSIA"], "ToFuchsiaPC"), save()]
        + safari_game("MAP_SAFARI_ZONE_CENTER", WHOLE,
                      ["rhyhorn", "nidoran-m", "nidoran-f", "exeggcute", "venonat", "nidorino", "nidorina",
                       "parasect", "scyther", "chansey", "pinsir"], "DexSafariCenter")
        + [heal(PC["FUCHSIA"], "SafariToFuchsiaPC"), save()]
        + safari_game("MAP_SAFARI_ZONE_EAST", WHOLE,
                      ["doduo", "paras", "nidoran-f", "nidoran-m", "exeggcute", "nidorino", "nidorina",
                       "parasect", "kangaskhan", "scyther", "pinsir"], "DexSafariEast")
        + [heal(PC["FUCHSIA"], "SafariToFuchsiaPC2"), save()],
    },
    {
        "id": "C14_06_SEAFOAM", "chapter": 14,
        "title": "POKeDEX: Seafoam Islands",
        "steps": [
            dex("MAP_SEAFOAM_ISLANDS_B3F", WHOLE, ["seel", "psyduck", "golbat", "golduck", "dewgong", "zubat",
                                                  "slowpoke", "slowbro"], "DexSeafoam"),
            heal(PC["FUCHSIA"], "SeafoamToFuchsiaPC"),
            save(),
        ],
    },
    {
        "id": "C14_07_MANSION", "chapter": 14,
        "title": "POKeDEX: Pokemon Mansion",
        "steps": [
            heal(PC["CINNABAR"], "ToCinnabarPC"),
            #  1F, where the story trained (B1F is behind the switches).
            dex("MAP_POKEMON_MANSION_1F", rect(1, 27, 15, 32), ["koffing", "raticate", "growlithe", "grimer", "rattata",
                                                               "weezing", "vulpix"], "DexMansion"),
            heal(PC["CINNABAR"], "MansionToCinnabarPC"),
            save(),
        ],
    },
    {
        "id": "C14_08_ROUTE11_DIGLETT", "chapter": 14,
        "title": "POKeDEX: Poke Balls in Vermilion City, Route 11, Diglett's Cave",
        "steps": [heal(PC["VERMILION"], "ToVermilionPC")] + poke_balls("MAP_VERMILION_CITY_MART", "BuyPokeBalls") + [
            dex("MAP_ROUTE11", WHOLE, ["ekans", "drowzee", "spearow", "sandshrew"], "DexRoute11"),
            dex("MAP_DIGLETTS_CAVE_B1F", WHOLE, ["diglett", "dugtrio"], "DexDiglettsCave"),
            heal(PC["VERMILION"], "DiglettToVermilionPC"),
            save(),
        ],
    },
    {
        "id": "C14_09_VICTORY_ROAD", "chapter": 14,
        "title": "POKeDEX: Victory Road",
        #  Route23_OnTransition resets the Victory Road puzzles (see C10_01).
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
        "steps": [
            heal(PC["INDIGO"], "ToIndigoPC"),
            dex("MAP_VICTORY_ROAD_2F", rect(39, 10, 46, 11), ["machop", "machoke", "onix", "marowak", "primeape",
                                                             "arbok", "golbat", "geodude", "zubat", "sandslash"],
                "DexVictoryRoad"),
            heal(PC["INDIGO"], "VictoryRoadToIndigoPC"),
            save(),
        ],
    },
    {
        "id": "C14_10_KINDLE_ROAD", "chapter": 14,
        "title": "POKeDEX: Poke Balls, the ferry to One Island, Kindle Road",
        "steps": [heal(PC["VERMILION"], "ToVermilionPC")] + poke_balls("MAP_VERMILION_CITY_MART", "BuyPokeBalls")
        + vermilion_ferry(0, "MAP_ONE_ISLAND_HARBOR", "FerryToOneIsland") + [
            dex("MAP_ONE_ISLAND_KINDLE_ROAD", WHOLE, ["ponyta", "meowth", "fearow", "persian", "rapidash", "psyduck",
                                                     "geodude", "spearow", "slowpoke"], "DexKindleRoad"),
            heal("MAP_ONE_ISLAND_POKEMON_CENTER_1F", "KindleRoadToOneIslandPC"),
            save(),
        ],
    },
    {
        "id": "C14_11_BOND_BRIDGE", "chapter": 14,
        "title": "POKeDEX: Three Island, Bond Bridge (Berry Forest if still short)",
        "steps": ferry("MAP_ONE_ISLAND_HARBOR", 2, "MAP_THREE_ISLAND_HARBOR", "FerryToThreeIsland") + [
            heal("MAP_THREE_ISLAND_POKEMON_CENTER_1F", "HarborToThreeIslandPC"),
            save(),
        ] + [only([v], [{"unless_dex": {"count": 64, "game_count": DEX_NEEDED, "owned": dex_owned(v), "steps": [
            dex("MAP_THREE_ISLAND_BOND_BRIDGE", WHOLE, ["pidgeotto", "gloom", "oddish", "meowth", "persian", "venonat",
                                                       "psyduck", "pidgey", "bellsprout", "weepinbell", "slowpoke"],
                "DexBondBridge"),
            heal("MAP_THREE_ISLAND_POKEMON_CENTER_1F", "BondBridgeToThreeIslandPC"),
            save(),
        ]}, "label": "Enough species?"}]) for v in STARTERS] + [
        ] + [only([v], [{"unless_dex": {"count": 64, "game_count": DEX_NEEDED, "owned": dex_owned(v), "steps": [
            dex("MAP_THREE_ISLAND_BERRY_FOREST", WHOLE, ["hypno", "drowzee", "exeggcute", "venomoth", "gloom",
                                                        "pidgeotto", "oddish", "venonat", "weepinbell"],
                "DexBerryForest"),
            heal("MAP_THREE_ISLAND_POKEMON_CENTER_1F", "BerryForestToThreeIslandPC"),
            save(),
        ]}, "label": "Enough species?"}]) for v in STARTERS] + [
        ] + sevii_ferry_home("MAP_THREE_ISLAND_HARBOR", "FerryToVermilion") + [
            heal(PC["VERMILION"], "PierToVermilionPC"),
            save(),
        ],
    },
]


# ---------------------------------------------------------------- chapter 15
#  The League once more: then Oak takes the player to the lab
#  (OakRatingScene with 60 or more: "follow me"), the National POKeDEX.
CHAPTER15 = [
    {
        "id": "C15_01_ENOUGH", "chapter": 15,
        "title": "60 species? To the Indigo Plateau",
        #  (The healing items for the Elite Four: league_again().)
        "steps": [only([v], [{"require_dex": {"count": 60, "owned": dex_owned(v)}, "label": "60 species caught"}])
                  for v in STARTERS] + [heal(PC["INDIGO"], "ToIndigoPC"), save()],
    },
] + league_again("C15_02", 15, "for the National POKeDEX") + [
    {
        "id": "C15_03_NATIONAL_DEX", "chapter": 15,
        "title": "Oak: 60 species, the National POKeDEX",
        #  OakRatingScene with 60 or more ("Impressive! Follow me!"): to the
        #  lab, PalletTown_ProfessorOaksLab_EventScript_EnterForNationalDexScene
        #  (8 steps up to (6, 4), Oak at (6, 3)), the rival, the upgrade
        #  (EnableNationalPokedex).
        "state": {"vars": {"VAR_MAP_SCENE_PALLET_TOWN_OAK": 2}},
        "steps": [
            pos("MAP_PALLET_TOWN", 6, 8, "down"),
            cutscene(15000, 300000, "Oak: the National POKeDEX"),
            setst(vars={"VAR_MAP_SCENE_PALLET_TOWN_OAK": 3, "VAR_MAP_SCENE_PALLET_TOWN_PROFESSOR_OAKS_LAB": 8},
                  flags=["FLAG_HIDE_OAK_IN_PALLET_TOWN", "FLAG_SYS_NATIONAL_DEX"],
                  clear_flags=["FLAG_HIDE_OAK_IN_HIS_LAB"],
                  objects={"MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB": {"LOCALID_OAKS_LAB_PROF_OAK": {"x": 6, "y": 3}}}),
            pos("MAP_PALLET_TOWN_PROFESSOR_OAKS_LAB", 6, 4, "up"),
            save(),
        ],
    },
    {
        "id": "C15_04_EXP_SHARE", "chapter": 15,
        "title": "The EXP. SHARE from Oak's aide on Route 15 (50 species caught)",
        #  Route15_WestEntrance_2F_EventScript_Aide: "...if you've caught at
        #  least 50 kinds of POKeMON" (YES/NO), GetPokedexCount, then the
        #  EXP. SHARE (FLAG_GOT_EXP_SHARE_FROM_OAKS_AIDE). Charizard holds it
        #  ("...a share of EXP. points without having to battle").
        "steps": [
            heal(PC["FUCHSIA"], "LabToFuchsiaPC"),
            save(),
            #  MAX REPELs for Ruby Path (Mt. Ember).
        ] + max_repels("MAP_FUCHSIA_CITY_MART", 2) + [
            talk("MAP_ROUTE15_WEST_ENTRANCE_2F", xy=[5, 5], mode="talk", label="OaksAideExpShare"),
            cutscene(3000, 60000, "The EXP. SHARE", answer="yes"),
            setst(flags=["FLAG_GOT_EXP_SHARE_FROM_OAKS_AIDE"]),
            give_item("share of", 1, "ExpShareToCharizard"),
            heal(PC["FUCHSIA"], "Route15ToFuchsiaPC"),
            save(),
        ],
    },
]

# ---------------------------------------------------------------- chapter 16
#  The Ruby (a gem: Celio's Network Machine for trading with the Hoenn games
#  needs it and the Sapphire). With the National POKeDEX Celio asks for it
#  (OneIsland_PokemonCenter_1F_EventScript_CelioRequestRuby): the cave of
#  the Ruby Path opens on Mt. Ember (MtEmber_Exterior_EventScript_OpenCave),
#  two Rockets in front of it tell the first password of the Rocket
#  Warehouse and fight. The Ruby lies on B5F. Celio gives the Rainbow Pass
#  for it (all seven islands) and asks for the Sapphire.
CHAPTER16 = [
    {
        "id": "C16_01_CELIO_RUBY", "chapter": 16,
        "title": "One Island: Celio asks for the Ruby (a gem of the Sevii Islands)",
        #  Celio stands where OneIsland_PokemonCenter_1F_EventScript_
        #  SetBillCelioReadyToLeavePos put him (setobjectxyperm: for good).
        "state": {"objects": {"MAP_ONE_ISLAND_POKEMON_CENTER_1F": {"LOCALID_CELIO": {"x": 14, "y": 8}}}},
        "steps": [heal(PC["VERMILION"], "LabToVermilionPC"), save()]
        + vermilion_ferry(0, "MAP_ONE_ISLAND_HARBOR", "FerryToOneIsland") + [
            heal("MAP_ONE_ISLAND_POKEMON_CENTER_1F", "HarborToOneIslandPC"),
            #  Celio stood at his machine (15, 6) in the game tested, not by
            #  the stools (14, 8) where SetBillCelioReadyToLeavePos puts him:
            #  talked to at the machine first (no step down to the stools),
            #  by the stools if no text box comes up.
            talk("MAP_ONE_ISLAND_POKEMON_CENTER_1F", xy=CELIO_AT_MACHINE, mode="talk", label="CelioRuby",
                 alt_xy=CELIO_BY_STOOLS),
            cutscene(3000, 60000, "Celio: the Ruby"),
            setst(vars={"VAR_MAP_SCENE_MT_EMBER_EXTERIOR": 1, "VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F": 4},
                  script_tiles=[["MAP_MT_EMBER_EXTERIOR", "MtEmber_Exterior_EventScript_OpenCave"]]),
            heal("MAP_ONE_ISLAND_POKEMON_CENTER_1F", "CelioToNurse"),
            save(),
        ],
    },
    {
        "id": "C16_02_RUBY_PATH", "chapter": 16,
        "title": "Mt. Ember: the Rockets' password, the Ruby Path, the Ruby",
        "steps": [
            #  MtEmber_Exterior_EventScript_RocketPasswordScene (x 37-39, y 43).
            goto("MAP_MT_EMBER_EXTERIOR", [[37, 43], [38, 43], [39, 43]], "RocketPassword", trigger=True),
            cutscene(3000, 60000, "The Rockets' password"),
            setst(vars={"VAR_MAP_SCENE_MT_EMBER_EXTERIOR": 2}),
            talk("MAP_MT_EMBER_EXTERIOR", "LOCALID_MT_EMBER_GRUNT1", mode="battle", label="MtEmberGrunt1"),
            cutscene(1500, 30000, "Grunt 1"),
            talk("MAP_MT_EMBER_EXTERIOR", "LOCALID_MT_EMBER_GRUNT2", mode="battle", label="MtEmberGrunt2"),
            #  MtEmber_Exterior_EventScript_DefeatedBothGrunts: they go.
            cutscene(1500, 30000, "Grunt 2, the Rockets leave"),
            setst(flags=["FLAG_HIDE_MT_EMBER_EXTERIOR_ROCKETS"]),
            save(),
            #  Boulders (Strength) and rocks (Rock Smash) on B2F and B3F.
            goto("MAP_MT_EMBER_RUBY_PATH_B2F", [[2, 7]], "ToRubyPathB2F"),
            {"boulder": {"open_to": [[13, 8], [14, 9]]}, "label": "RubyPathB2F"},
            goto("MAP_MT_EMBER_RUBY_PATH_B3F", [[2, 4]], "ToRubyPathB3F"),
            {"boulder": {"open_to": [[5, 18], [4, 19], [6, 18]]}, "label": "RubyPathB3F"},
            talk("MAP_MT_EMBER_RUBY_PATH_B5F", "LOCALID_RUBY", mode="talk", label="Ruby"),
            cutscene(1500, 30000, "The Ruby"),
            #  B3F and B2F load again on the way up: their boulders are back.
            setst(flags=["FLAG_GOT_RUBY", "FLAG_HIDE_RUBY"],
                  reset_objects=["MAP_MT_EMBER_RUBY_PATH_B3F", "MAP_MT_EMBER_RUBY_PATH_B2F", "MAP_MT_EMBER_EXTERIOR"]),
            #  Up again: B3F from the stairs of B4F, its boulders out of the
            #  way to the stairs up (19, 19).
            goto("MAP_MT_EMBER_RUBY_PATH_B3F", [[5, 18]], "BackToRubyPathB3F"),
            {"boulder": {"open_to": [[19, 18], [20, 19]]}, "label": "RubyPathB3FBack"},
            heal("MAP_ONE_ISLAND_POKEMON_CENTER_1F", "RubyToOneIslandPC"),
            save(),
        ],
    },
    {
        "id": "C16_03_RAINBOW_PASS", "chapter": 16,
        "title": "Celio: the Ruby in the machine, the Rainbow Pass; the Sapphire next",
        "steps": [
            #  OneIsland_PokemonCenter_1F_EventScript_GiveCelioRuby: "May I
            #  ask one more favor?" YES.
            talk("MAP_ONE_ISLAND_POKEMON_CENTER_1F", xy=CELIO_AT_MACHINE, mode="yes", label="CelioGiveRuby",
                 alt_xy=CELIO_BY_STOOLS),
            cutscene(3000, 90000, "The Rainbow Pass"),
            setst(vars={"VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F": 5}, flags=["FLAG_SYS_SEVII_MAP_4567"],
                  script_tiles=[["MAP_ONE_ISLAND_POKEMON_CENTER_1F", "OneIsland_PokemonCenter_1F_EventScript_SetRubyMetatile"]]),
            heal("MAP_ONE_ISLAND_POKEMON_CENTER_1F", "CelioToNurse2"),
            save(),
        ],
    },
]


# ---------------------------------------------------------------- chapter 17
#  The Sapphire (the other gem). The way to it: Four Island, Icefall Cave
#  (HM07 Waterfall there; Lorelei and the poachers in the back: only then
#  the scientist steps away from the door of the Dotted Hole), Six Island
#  (Ruin Valley: Cut on the braille of the door, the Dotted Hole; Gideon
#  takes the Sapphire to the Rocket Warehouse, his password), Five Island
#  (the warehouse: spinners, the admins, Gideon), and back to Celio. With
#  both gems in the Network Machine the roaming legendary starts roaming
#  (OneIsland_PokemonCenter_1F_EventScript_GiveCelioSapphire, InitRoamer).


def rainbow_ferry(harbor_map, page0, page1, dest_harbor, label):
    """The Seagallop with the Rainbow Pass (DrawSeagallopDestinationMenu):
    page 0 lists four destinations (Vermilion, One... Four, without the
    island the ferry leaves from), then "Other"; page 1 three of Four...
    Seven. page0/page1: the row to pick (page1 None: picked on page 0)."""
    steps = [talk(harbor_map, xy=[8, 6], mode="plain", label=label),
             press("A", 1, 2500, label + "_Talk")]
    if page0:
        steps.append(press("DOWN", page0, 500, label + "_Menu"))
    steps.append(press("A", 1, 1500, label + "_Pick"))
    if page1 is not None:
        if page1:
            steps.append(press("DOWN", page1, 500, label + "_Menu2"))
        steps.append(press("A", 1, 1500, label + "_Pick2"))
    return steps + [cutscene(8000, 90000, "Ferry"), pos(dest_harbor, 8, 5)]


WATERFALL_RANK = {v: 5 + (v in FLY_VARIANTS) for v in STARTERS}

CHAPTER17 = [
    {
        "id": "C17_01_ICEFALL_CAVE", "chapter": 17,
        "title": "Four Island, Icefall Cave: thin ice, HM07 Waterfall for Vaporeon",
        "steps": rainbow_ferry("MAP_ONE_ISLAND_HARBOR", 3, None, "MAP_FOUR_ISLAND_HARBOR", "FerryToFourIsland") + [
            #  The first step on Four Island: the rival comes, talks and
            #  leaves (FourIsland_EventScript_RivalScene, VAR 0), the player
            #  steps left to watch him go (PlayerWatchRivalExit): one tile
            #  left of the arrival (10, 28), facing right. Unmodelled, the
            #  walk to the Pokemon Center ended a tile left of its door.
            goto("MAP_FOUR_ISLAND", [[10, 28]], "HarborToFourIsland"),
            cutscene(3000, 60000, "The rival on Four Island"),
            pos("MAP_FOUR_ISLAND", 9, 28, "right"),
            setst(vars={"VAR_MAP_SCENE_FOUR_ISLAND": 1}, flags=["FLAG_HIDE_FOUR_ISLAND_RIVAL"]),
            #  ESCAPE ROPEs: out of Icefall Cave and Cerulean Cave.
        ] + ropes("MAP_FOUR_ISLAND_MART", 2) + [
            heal("MAP_FOUR_ISLAND_POKEMON_CENTER_1F", "HarborToFourIslandPC"),
            save(),
            #  1F: over the thin ice (8, 14), (8, 10), (8, 9) up to the thin
            #  ice (8, 3), a step back, onto it again: it breaks, the fall
            #  to B1F (FourIsland_IcefallCave_1F_EventScript_FallDownHole),
            #  on the strip that leads onto the ice there.
            goto("MAP_FOUR_ISLAND_ICEFALL_CAVE_1F", [[8, 3]], "ToThinIce"),
            goto("MAP_FOUR_ISLAND_ICEFALL_CAVE_1F", [[8, 4]], "OffTheThinIce"),
            #  B1F: the ladder (12, 3) up to the north-east of 1F, the thin
            #  ice (16, 9) there twice: the fall onto the ice of B1F (16, 9),
            #  from where it slides to the ladder up to the HM07 corner.
            goto("MAP_FOUR_ISLAND_ICEFALL_CAVE_1F", [[16, 9]], "ToThinIce2"),
            goto("MAP_FOUR_ISLAND_ICEFALL_CAVE_1F", [[16, 8]], "OffTheThinIce2"),
            talk("MAP_FOUR_ISLAND_ICEFALL_CAVE_1F", xy=[12, 16], mode="talk", label="HM07"),
            cutscene(1000, 20000, "HM07"),
            setst(flags=["FLAG_HIDE_FOUR_ISLAND_ICEFALL_CAVE_1F_HM07"]),
        ] + [only([v], [{"teach": {"hm_rank": WATERFALL_RANK[v], "party_slot": EEVEE_SLOT[v],
                                    "move": "WATERFALL", "forget": -1}, "label": "Teach WATERFALL"}])
             for v in STARTERS] + [
            setst(abilities={"waterfall": True}),
            save(),
        ],
    },
    {
        "id": "C17_02_LORELEI", "chapter": 17,
        "title": "Icefall Cave: up the waterfall, Lorelei and the poachers",
        "steps": [
            #  FourIsland_IcefallCave_Back_EventScript_LoreleiRocketsScene
            #  (x 11-13, y 18): from (12, 18) the player walks up 2, right,
            #  up (PlayerToRockets); a Rocket fights.
            goto("MAP_FOUR_ISLAND_ICEFALL_CAVE_BACK", [[12, 18]], "LoreleiScene", trigger=True),
            cutscene(5000, 300000, "Lorelei and the poachers"),
            setst(flags=["FLAG_HIDE_RUIN_VALLEY_SCIENTIST", "FLAG_HIDE_ICEFALL_CAVE_ROCKETS"],
                  clear_flags=["FLAG_HIDE_LORELEI_IN_HER_HOUSE"],
                  vars={"VAR_MAP_SCENE_ICEFALL_CAVE_BACK": 1}),
            pos("MAP_FOUR_ISLAND_ICEFALL_CAVE_BACK", 13, 15, "left"),
            heal("MAP_FOUR_ISLAND_POKEMON_CENTER_1F", "IcefallToFourIslandPC"),
            save(),
        ],
    },
    {
        "id": "C17_03_DOTTED_HOLE", "chapter": 17,
        "title": "Six Island: Ruin Valley, the Dotted Hole, the Sapphire (Gideon takes it)",
        "steps": rainbow_ferry("MAP_FOUR_ISLAND_HARBOR", 4, 1, "MAP_SIX_ISLAND_HARBOR", "FerryToSixIsland") + [
            #  The first step into Six Island's Pokemon Center: the rival
            #  again (SixIsland_PokemonCenter_1F_EventScript_RivalScene, VAR
            #  0), the player steps left to let him go: (6, 8), facing
            #  right. Unmodelled, the walk to the nurse ended a tile left.
            goto("MAP_SIX_ISLAND_POKEMON_CENTER_1F", [[7, 8]], "HarborToSixIslandPC"),
            cutscene(3000, 60000, "The rival in Six Island's Pokemon Center"),
            pos("MAP_SIX_ISLAND_POKEMON_CENTER_1F", 6, 8, "right"),
            setst(vars={"VAR_MAP_SCENE_SIX_ISLAND_POKEMON_CENTER_1F": 1, "VAR_MAP_SCENE_FOUR_ISLAND": 1},
                  flags=["FLAG_HIDE_SIX_ISLAND_POKECENTER_RIVAL"]),
            heal("MAP_SIX_ISLAND_POKEMON_CENTER_1F", "ToSixIslandNurse"),
            save(),
            #  CutMoveRuinValleyCheck: CUT from the party menu at (24, 25),
            #  facing north; the door opens (CutMoveOpenDottedHoleDoor).
            goto("MAP_SIX_ISLAND_RUIN_VALLEY", [[24, 25]], "ToDottedHoleDoor"),
            {"field_move": "CUT", "slot": 1, "face": "up", "label": "Cut the braille"},
            cutscene(2000, 20000, "The door opens"),
            setst(flags=["FLAG_USED_CUT_ON_RUIN_VALLEY_BRAILLE"],
                  script_tiles=[["MAP_SIX_ISLAND_RUIN_VALLEY", "SixIsland_RuinValley_EventScript_OpenDottedHoleDoor"]]),
            #  SixIsland_DottedHole_SapphireRoom_EventScript_Sapphire.
            talk("MAP_SIX_ISLAND_DOTTED_HOLE_SAPPHIRE_ROOM", "LOCALID_SAPPHIRE", mode="talk", label="Sapphire"),
            cutscene(5000, 120000, "Gideon takes the Sapphire"),
            setst(flags=["FLAG_HIDE_SAPPHIRE", "FLAG_HIDE_DOTTED_HOLE_SCIENTIST", "FLAG_LEARNED_YES_NAH_CHANSEY"]),
            heal("MAP_SIX_ISLAND_POKEMON_CENTER_1F", "DottedHoleToSixIslandPC"),
            save(),
        ],
    },
    {
        "id": "C17_04_ROCKET_WAREHOUSE", "chapter": 17,
        "title": "Five Island: the Rocket Warehouse, Gideon, the Sapphire back",
        "steps": rainbow_ferry("MAP_SIX_ISLAND_HARBOR", 4, 1, "MAP_FIVE_ISLAND_HARBOR", "FerryToFiveIsland") + [
            heal("MAP_FIVE_ISLAND_POKEMON_CENTER_1F", "HarborToFiveIslandPC"),
            save(),
            #  FiveIsland_Meadow_EventScript_WarehouseDoor: both passwords.
            talk("MAP_FIVE_ISLAND_MEADOW", xy=[12, 21], mode="talk", label="WarehouseDoor"),
            cutscene(2000, 20000, "The door opens"),
            setst(flags=["FLAG_UNLOCKED_ROCKET_WAREHOUSE"],
                  script_tiles=[["MAP_FIVE_ISLAND_MEADOW", "FiveIsland_Meadow_EventScript_SetWarehouseDoorUnlocked"]]),
            #  Through the spinners to FiveIsland_RocketWarehouse_EventScript_
            #  Admin2Trigger (x 22-24, y 11): the admin turns, sees the
            #  player, fights; then all the Rockets leave
            #  (FiveIsland_RocketWarehouse_EventScript_DefeatedAdmin2).
            goto("MAP_FIVE_ISLAND_ROCKET_WAREHOUSE", [[22, 11], [23, 11], [24, 11]], "ToTheAdmins", trigger=True),
            cutscene(3000, 300000, "The admin fights, the Rockets leave"),
            setst(flags=["FLAG_HIDE_FIVE_ISLAND_ROCKETS", "FLAG_DEFEATED_ROCKETS_IN_WAREHOUSE"],
                  vars={"VAR_MAP_SCENE_ROCKET_WAREHOUSE": 1}),
            #  Gideon (27, 4): FiveIsland_RocketWarehouse_EventScript_DefeatedGideon.
            talk("MAP_FIVE_ISLAND_ROCKET_WAREHOUSE", xy=[27, 4], mode="battle", label="Gideon"),
            cutscene(2000, 60000, "The Sapphire"),
            setst(flags=["FLAG_RECOVERED_SAPPHIRE"]),
            heal("MAP_FIVE_ISLAND_POKEMON_CENTER_1F", "WarehouseToFiveIslandPC"),
            save(),
        ],
    },
    {
        "id": "C17_05_NETWORK_MACHINE", "chapter": 17,
        "title": "One Island: the Sapphire for Celio; the roaming legendary is out",
        "steps": rainbow_ferry("MAP_FIVE_ISLAND_HARBOR", 1, None, "MAP_ONE_ISLAND_HARBOR", "FerryToOneIsland") + [
            heal("MAP_ONE_ISLAND_POKEMON_CENTER_1F", "HarborToOneIslandPC"),
            talk("MAP_ONE_ISLAND_POKEMON_CENTER_1F", xy=CELIO_AT_MACHINE, mode="talk", label="CelioSapphire",
                 alt_xy=CELIO_BY_STOOLS),
            cutscene(5000, 120000, "The Network Machine works"),
            setst(vars={"VAR_MAP_SCENE_ONE_ISLAND_POKEMON_CENTER_1F": 6},
                  flags=["FLAG_SYS_CAN_LINK_WITH_RS", "FLAG_HIDE_CERULEAN_CAVE_GUARD", "FLAG_HIDE_LORELEI_IN_HER_HOUSE"]),
            heal("MAP_ONE_ISLAND_POKEMON_CENTER_1F", "CelioToNurse3"),
            save(),
        ],
    },
]


# ---------------------------------------------------------------- chapter 18
#  The roaming legendary (Suicune for the Charmander player; Entei,
#  Raikou with the other starters): on one of the Kanto routes, it moves to
#  another one with every map change (RoamerMove), to any of them at every
#  warp (RoamerMoveToOtherLocationSet). On its route a wild battle is it one
#  time in four (TryStartRoamerEncounter). It flees on its first turn: the
#  MASTER BALL at once (a ball goes before any move).
CHAPTER18 = [
    {
        "id": "C18_01_SUICUNE", "chapter": 18,
        "title": "The roaming Suicune: Route 10, the MASTER BALL",
        "steps": rainbow_ferry("MAP_ONE_ISLAND_HARBOR", 0, None, "MAP_VERMILION_CITY", "FerryToVermilion")[:-1] + [
            pos("MAP_VERMILION_CITY", 23, 32),
            heal(PC["ROUTE10"], "ToRoute10PC"),
            save(),
            #  The grass north of the Pokemon Center; in and out of it every
            #  3 battles.
            {"roam": {"map": "MAP_ROUTE10", "tiles": rect(6, 6, 16, 10), "species": ["suicune", "entei", "raikou"],
                      "building": PC["ROUTE10"], "per_cycle": 3, "minutes": 360}, "label": "RoamingLegendary"},
            heal(PC["ROUTE10"], "SuicuneToRoute10PC"),
            save(),
        ],
    },
]

# ---------------------------------------------------------------- chapter 19
#  Mewtwo. Cerulean Cave opens when Celio has the Sapphire (C17_05:
#  FLAG_HIDE_CERULEAN_CAVE_GUARD). Level 70, catch rate 3, the Master Ball
#  went to Suicune: weakened, then Ultra Balls (Fuchsia City, as many as the
#  money allows); not caught, or the team fainted: the save in front of it
#  again (CeruleanCave_B1F_EventScript_Mewtwo).
CHAPTER19 = [
    {
        "id": "C19_01_MEWTWO", "chapter": 19,
        "title": "Mewtwo in Cerulean Cave (weakened, Ultra Balls)",
        "steps": [
            heal(PC["FUCHSIA"], "ToFuchsiaPC"),
            #  MAX REPELs first: the way to Mewtwo, some 290 steps through
            #  the cave (the second one where the first ends).
        ] + max_repels("MAP_FUCHSIA_CITY_MART", 2) + buy("MAP_FUCHSIA_CITY_MART", [2, 3], 0, 0, "BuyUltraBalls") + [
            heal(PC["CERULEAN"], "FuchsiaToCeruleanPC"),
            save(),
            goto("MAP_CERULEAN_CAVE_B1F", [[7, 13]], "ToMewtwo"),
            save(),
        ] + legendary("MAP_CERULEAN_CAVE_B1F", [7, 12], ["FLAG_FOUGHT_MEWTWO", "FLAG_HIDE_MEWTWO"], "Mewtwo",
                      weaken=True) + [
            heal(PC["CERULEAN"], "CaveToCeruleanPC"),
            save(),
        ],
    },
]



# ---------------------------------------------------------------- chapter 20
#  The POKeDEX collection, after Mewtwo: every place with grass, a cave floor
#  or the Safari Zone where a species lives that the program's POKeDEX table
#  (the option "Pokedex collection", for the game and the starter chosen)
#  still wants: one of each species that can be registered, and a second
#  (third) one of those whose evolution cannot be caught (to evolve). The
#  lists here are of both games (all species of the place, story.py): the
#  program only catches what the table wants of its game, and skips a place
#  (a town) where it wants nothing.
#
#  Each place is a round trip from the town's Pokemon Center (from the nurse
#  back to the nurse, then a save). Each town is a segment of its own that
#  starts with FLY (from anywhere outdoors in Kanto, also from that town:
#  the Fly map takes the town the player is in) and ends outdoors in front
#  of the Pokemon Center: a town skipped leaves the player where the town
#  before ended, and the next FLY starts from there. With "shiny only" the
#  program goes round the towns again and again (C20_02 after the last).
#  Surfing, fishing, Rock Smash, gifts, trades and breeding: later. Not yet
#  either: Mt. Ember's grass (behind Rock Smash rocks and Strength boulders
#  the routes do not move: MAGMAR in LeafGreen), the Tanoby Chambers (UNOWN
#  only once the Tanoby Key puzzle is solved), Three Isle Port (DUNSPARCE:
#  through the Dunsparce Tunnel, which the routes do not pass).

COLLECT_IDLE = 30       # Battles in a row without a catch: the visit ends.
COLLECT_MINUTES = 30    # The longest visit.
#  Visits of each place (the later ones skipped when nothing is wanted
#  there any more): a visit also ends when the lead is low on HP (catching
#  at full HP, the wild Pokemon hit back) or fainted.
COLLECT_VISITS = 2


def collect(map_id, tiles, label, species=None, safari=False):
    """For the POKeDEX table: back and forth there catching what the table
    still wants (species None: every species of the place, both games)."""
    return {"dex": {"map": map_id, "tiles": tiles, "species": species, "idle": COLLECT_IDLE,
                    "minutes": COLLECT_MINUTES, "safari": safari, "enough": 0, "collect": True}, "label": label}


def collect_trip(pc_map, steps, label):
    """A round trip from the nurse of pc_map, skipped when the table wants
    none of the species of its places."""
    return {"unless_collected": {"steps": steps + [heal(pc_map, label + "_BackToNurse"), save()]},
            "label": label + "?"}


def collect_place(pc_map, map_id, tiles, label, species=None, visits=COLLECT_VISITS):
    """The place's visits: each a round trip from the nurse, skipped when
    nothing of the place is wanted (any more)."""
    return [collect_trip(pc_map, [collect(map_id, tiles, label + ("" if v == 0 else "_Visit%d" % (v + 1)), species)],
                         label + ("" if v == 0 else "_Visit%d" % (v + 1)))
            for v in range(visits)]


def collect_safari(area_map, label):
    """A Safari Game in one area (C14_05: the walk in, the PA's text at the
    end, the gate)."""
    steps = safari_game(area_map, WHOLE, None, label)
    steps = [collect(area_map, WHOLE, label, safari=True) if "dex" in s else s for s in steps]
    return collect_trip(PC["FUCHSIA"], steps, label)


def collect_town(seg_id, title, pc_map, places, fly_to, mart=None, state=None):
    """One town: FLY there, the nurse, POKe BALLs (mart), the places, out of
    the Pokemon Center. Skipped as a whole when the table wants none of the
    species of its places."""
    steps = [fly(fly_to, seg_id + "_Fly", force=True)]
    steps.append(heal(pc_map, seg_id + "_ToNurse"))
    if mart:
        steps += poke_balls(mart, seg_id + "_BuyPokeBalls") + [heal(pc_map, seg_id + "_MartToNurse")]
    for place in places:
        steps += place if isinstance(place, list) else [place]
    out = {"outside": True, "label": seg_id + "_Out"}
    body = [{"collect_segment": {"steps": steps + [out]}, "label": "Anything to catch here?"}]
    seg = {"id": seg_id, "chapter": 20, "title": "POKeDEX collection: " + title, "no_repel": True,
           "steps": body}
    if state:
        seg["state"] = state
    return seg


def sevii_island(harbor, pc_map, places, label):
    """One island: from the harbor to the nurse, the places, back to the
    harbor (where the ferry leaves). Skipped when nothing of it is wanted."""
    steps = [heal(pc_map, label + "_ToNurse")]
    for place in places:
        steps += place if isinstance(place, list) else [place]
    return {"unless_collected": {"steps": steps + [goto(harbor, [[8, 5]], label + "_ToHarbor")]},
            "label": label + "?"}


ONE_PC = "MAP_ONE_ISLAND_POKEMON_CENTER_1F"
TWO_PC = "MAP_TWO_ISLAND_POKEMON_CENTER_1F"
THREE_PC = "MAP_THREE_ISLAND_POKEMON_CENTER_1F"
FOUR_PC = "MAP_FOUR_ISLAND_POKEMON_CENTER_1F"
FIVE_PC = "MAP_FIVE_ISLAND_POKEMON_CENTER_1F"
SIX_PC = "MAP_SIX_ISLAND_POKEMON_CENTER_1F"
SEVEN_PC = "MAP_SEVEN_ISLAND_POKEMON_CENTER_1F"

VICTORY_ROAD_RESET = {
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
}


def sevii_tour():
    """Vermilion -> One ... Seven Island -> Vermilion (the Seagallop with the
    Rainbow Pass), the places of each island. Starts and ends at the nurse
    of the Vermilion Pokemon Center."""
    return [
        goto("MAP_VERMILION_CITY", [[23, 33]], "ToTheSeagallop", trigger=True),
        #  The Switch version puts the MYSTIC and AURORA TICKETs in the bag
        #  after the Hall of Fame: the sailor talks about them (the first
        #  time) and asks SEVII ISLANDS / NAVEL ROCK / BIRTH ISLAND first.
        {"seagallop": 0, "label": "SeagallopToOneIsland"},
        cutscene(8000, 90000, "Ferry"),
        pos("MAP_ONE_ISLAND_HARBOR", 8, 5),
        sevii_island("MAP_ONE_ISLAND_HARBOR", ONE_PC, [
            collect_place(ONE_PC, "MAP_ONE_ISLAND_KINDLE_ROAD", WHOLE, "CollectKindleRoad"),
            collect_place(ONE_PC, "MAP_ONE_ISLAND_TREASURE_BEACH", WHOLE, "CollectTreasureBeach"),
            collect_place(ONE_PC, "MAP_MT_EMBER_RUBY_PATH_B1F", WHOLE, "CollectRubyPath"),
        ], "OneIsland"),
    ] + rainbow_ferry("MAP_ONE_ISLAND_HARBOR", 1, None, "MAP_TWO_ISLAND_HARBOR", "FerryToTwoIsland") + [
        sevii_island("MAP_TWO_ISLAND_HARBOR", TWO_PC, [
            collect_place(TWO_PC, "MAP_TWO_ISLAND_CAPE_BRINK", WHOLE, "CollectCapeBrink"),
        ], "TwoIsland"),
    ] + rainbow_ferry("MAP_TWO_ISLAND_HARBOR", 2, None, "MAP_THREE_ISLAND_HARBOR", "FerryToThreeIsland") + [
        sevii_island("MAP_THREE_ISLAND_HARBOR", THREE_PC, [
            collect_place(THREE_PC, "MAP_THREE_ISLAND_BOND_BRIDGE", WHOLE, "CollectBondBridge"),
            collect_place(THREE_PC, "MAP_THREE_ISLAND_BERRY_FOREST", WHOLE, "CollectBerryForest"),
        ], "ThreeIsland"),
    ] + rainbow_ferry("MAP_THREE_ISLAND_HARBOR", 3, None, "MAP_FOUR_ISLAND_HARBOR", "FerryToFourIsland") + [
        sevii_island("MAP_FOUR_ISLAND_HARBOR", FOUR_PC, [
            collect_place(FOUR_PC, "MAP_FOUR_ISLAND_ICEFALL_CAVE_1F", WHOLE, "CollectIcefallCave"),
        ], "FourIsland"),
    ] + rainbow_ferry("MAP_FOUR_ISLAND_HARBOR", 4, 0, "MAP_FIVE_ISLAND_HARBOR", "FerryToFiveIsland") + [
        sevii_island("MAP_FIVE_ISLAND_HARBOR", FIVE_PC, [
            collect_place(FIVE_PC, "MAP_FIVE_ISLAND_MEADOW", WHOLE, "CollectFiveIsleMeadow"),
            collect_place(FIVE_PC, "MAP_FIVE_ISLAND_LOST_CAVE_ROOM1", WHOLE, "CollectLostCave"),
        ], "FiveIsland"),
    ] + rainbow_ferry("MAP_FIVE_ISLAND_HARBOR", 4, 1, "MAP_SIX_ISLAND_HARBOR", "FerryToSixIsland") + [
        sevii_island("MAP_SIX_ISLAND_HARBOR", SIX_PC, [
            collect_place(SIX_PC, "MAP_SIX_ISLAND_RUIN_VALLEY", WHOLE, "CollectRuinValley"),
            collect_place(SIX_PC, "MAP_SIX_ISLAND_PATTERN_BUSH", WHOLE, "CollectPatternBush"),
            collect_place(SIX_PC, "MAP_SIX_ISLAND_WATER_PATH", WHOLE, "CollectWaterPath"),
        ], "SixIsland"),
    ] + rainbow_ferry("MAP_SIX_ISLAND_HARBOR", 4, 2, "MAP_SEVEN_ISLAND_HARBOR", "FerryToSevenIsland") + [
        sevii_island("MAP_SEVEN_ISLAND_HARBOR", SEVEN_PC, [
            collect_place(SEVEN_PC, "MAP_SEVEN_ISLAND_SEVAULT_CANYON_ENTRANCE", WHOLE, "CollectCanyonEntrance"),
            collect_place(SEVEN_PC, "MAP_SEVEN_ISLAND_SEVAULT_CANYON", WHOLE, "CollectSevaultCanyon"),
        ], "SevenIsland"),
    ] + rainbow_ferry("MAP_SEVEN_ISLAND_HARBOR", 0, None, "MAP_VERMILION_CITY", "FerryToVermilion")[:-1] + [
        pos("MAP_VERMILION_CITY", 23, 32),
        heal(PC["VERMILION"], "PierToVermilionNurse"),
        save(),
    ]


CHAPTER20 = [
    #  Mewtwo's segment ends at the nurse in Cerulean City: out first, so that
    #  every town segment starts outdoors (and the loop of "shiny only"
    #  comes back to C20_02 from anywhere).
    {
        "id": "C20_01_START", "chapter": 20, "no_repel": True,
        "title": "POKeDEX collection: out of the Cerulean City Pokemon Center",
        "steps": [{"outside": True, "label": "C20_01_Out"}],
    },
    collect_town("C20_02_CERULEAN", "Route 24, Route 4, Cerulean Cave", PC["CERULEAN"], [
        collect_place(PC["CERULEAN"], "MAP_ROUTE24", WHOLE, "CollectRoute24"),
        collect_place(PC["CERULEAN"], "MAP_ROUTE4", WHOLE, "CollectRoute4"),
        collect_place(PC["CERULEAN"], "MAP_CERULEAN_CAVE_B1F", WHOLE, "CollectCeruleanCaveB1F", visits=3),
        collect_place(PC["CERULEAN"], "MAP_CERULEAN_CAVE_1F", WHOLE, "CollectCeruleanCave1F"),
    ], fly_to="HEAL_LOCATION_CERULEAN_CITY", mart="MAP_CERULEAN_CITY_MART"),
    collect_town("C20_03_MT_MOON", "Mt. Moon, Route 3", PC["ROUTE4"], [
        collect_place(PC["ROUTE4"], "MAP_MT_MOON_B2F", WHOLE, "CollectMtMoon"),
        collect_place(PC["ROUTE4"], "MAP_ROUTE3", WHOLE, "CollectRoute3"),
    ], fly_to="HEAL_LOCATION_ROUTE4"),
    collect_town("C20_04_VIRIDIAN", "Viridian Forest, Route 22, Route 23", PC["VIRIDIAN"], [
        collect_place(PC["VIRIDIAN"], "MAP_VIRIDIAN_FOREST", rect(3, 55, 42, 61), "CollectViridianForest"),
        collect_place(PC["VIRIDIAN"], "MAP_ROUTE22", WHOLE, "CollectRoute22"),
        collect_place(PC["VIRIDIAN"], "MAP_ROUTE23", WHOLE, "CollectRoute23"),
    ], fly_to="HEAL_LOCATION_VIRIDIAN_CITY", mart="MAP_VIRIDIAN_CITY_MART"),
    collect_town("C20_05_VICTORY_ROAD", "Victory Road", PC["INDIGO"], [
        collect_place(PC["INDIGO"], "MAP_VICTORY_ROAD_2F", rect(39, 10, 46, 11), "CollectVictoryRoad"),
    ], fly_to="HEAL_LOCATION_INDIGO_PLATEAU", state=VICTORY_ROAD_RESET),
    collect_town("C20_06_ROUTE10", "Power Plant, Rock Tunnel", PC["ROUTE10"], [
        collect_place(PC["ROUTE10"], "MAP_POWER_PLANT", WHOLE, "CollectPowerPlant"),
        collect_place(PC["ROUTE10"], "MAP_ROCK_TUNNEL_1F", WHOLE, "CollectRockTunnel"),
    ], fly_to="HEAL_LOCATION_ROUTE10"),
    collect_town("C20_07_LAVENDER", "Pokemon Tower, Route 8, Route 12", PC["LAVENDER"], [
        collect_place(PC["LAVENDER"], "MAP_POKEMON_TOWER_7F", WHOLE, "CollectPokemonTower"),
        collect_place(PC["LAVENDER"], "MAP_ROUTE8", WHOLE, "CollectRoute8"),
        collect_place(PC["LAVENDER"], "MAP_ROUTE12", WHOLE, "CollectRoute12"),
    ], fly_to="HEAL_LOCATION_LAVENDER_TOWN"),
    collect_town("C20_08_CELADON", "Route 16", PC["CELADON"], [
        collect_place(PC["CELADON"], "MAP_ROUTE16", WHOLE, "CollectRoute16"),
    ], fly_to="HEAL_LOCATION_CELADON_CITY"),
    #  The Safari Zone: each area twice (its rare ones: CHANSEY, TAUROS,
    #  KANGASKHAN, SCYTHER/PINSIR 1-4%); the second game only when the
    #  first left some.
    collect_town("C20_09_FUCHSIA", "Safari Zone, Routes 14, 15, 18, Seafoam Islands", PC["FUCHSIA"], [
        collect_safari("MAP_SAFARI_ZONE_CENTER", "CollectSafariCenter"),
        collect_safari("MAP_SAFARI_ZONE_EAST", "CollectSafariEast"),
        collect_safari("MAP_SAFARI_ZONE_NORTH", "CollectSafariNorth"),
        collect_safari("MAP_SAFARI_ZONE_WEST", "CollectSafariWest"),
        collect_safari("MAP_SAFARI_ZONE_CENTER", "CollectSafariCenter2"),
        collect_safari("MAP_SAFARI_ZONE_EAST", "CollectSafariEast2"),
        collect_safari("MAP_SAFARI_ZONE_NORTH", "CollectSafariNorth2"),
        collect_safari("MAP_SAFARI_ZONE_WEST", "CollectSafariWest2"),
        collect_place(PC["FUCHSIA"], "MAP_ROUTE14", WHOLE, "CollectRoute14"),
        collect_place(PC["FUCHSIA"], "MAP_ROUTE15", WHOLE, "CollectRoute15"),
        collect_place(PC["FUCHSIA"], "MAP_ROUTE18", WHOLE, "CollectRoute18"),
        collect_place(PC["FUCHSIA"], "MAP_SEAFOAM_ISLANDS_B3F", WHOLE, "CollectSeafoam"),
    ], fly_to="HEAL_LOCATION_FUCHSIA_CITY"),
    collect_town("C20_10_CINNABAR", "Pokemon Mansion, Route 21", PC["CINNABAR"], [
        collect_place(PC["CINNABAR"], "MAP_POKEMON_MANSION_1F", rect(1, 27, 15, 32), "CollectMansion"),
        collect_place(PC["CINNABAR"], "MAP_ROUTE21_NORTH", WHOLE, "CollectRoute21"),
    ], fly_to="HEAL_LOCATION_CINNABAR_ISLAND"),
    collect_town("C20_11_VERMILION_SEVII", "Route 11, Diglett's Cave, Route 6, the Sevii Islands", PC["VERMILION"], [
        collect_place(PC["VERMILION"], "MAP_ROUTE11", WHOLE, "CollectRoute11"),
        collect_place(PC["VERMILION"], "MAP_DIGLETTS_CAVE_B1F", WHOLE, "CollectDiglettsCave"),
        collect_place(PC["VERMILION"], "MAP_ROUTE6", WHOLE, "CollectRoute6"),
        {"unless_collected": {"steps": sevii_tour()}, "label": "The Sevii Islands?"},
    ], fly_to="HEAL_LOCATION_VERMILION_CITY", mart="MAP_VERMILION_CITY_MART"),
]


def fly_only(segments):
    """From here on the story flies (the Indigo Plateau is out of reach on
    foot once Victory Road is reset, the places are far apart): with
    another starter (no Pokemon of its team learns FLY) these segments only
    stop the program."""
    others = [v for v in STARTERS if v not in FLY_VARIANTS]
    out = []
    for seg in segments:
        seg = dict(seg)
        seg["steps"] = [only(FLY_VARIANTS, seg["steps"]),
                        only(others, [{"stop": "The story after the Sevii Islands (the National POKeDEX) is only "
                                               "played with Charmander so far (it needs FLY).",
                                       "label": "Not with this starter"}])]
        out.append(seg)
    return out


SEGMENTS = (list(CHAPTER0) + CHAPTER1 + CHAPTER2 + CHAPTER3 + CHAPTER4 + CHAPTER5 + CHAPTER6 + CHAPTER7
            + CHAPTER8 + CHAPTER9 + CHAPTER10 + CHAPTER11 + CHAPTER12
            + fly_only(CHAPTER13 + CHAPTER14 + CHAPTER15 + CHAPTER16 + CHAPTER17 + CHAPTER18 + CHAPTER19
                       + CHAPTER20))


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

#!/usr/bin/env python3
"""
Generate PokemonFRLG_AutoStoryDex.h: move, species and type data for the
Autostory battle logic, taken from the pret/pokefirered decomp:

  src/data/battle_moves.h          power, type, accuracy, pp, effect
  src/data/text/move_names.h       in-game move names
  src/data/pokemon/species_info.h  species types
  src/data/text/species_names.h    species names (-> Pokemon Automation slugs)
  src/battle_main.c                gTypeEffectiveness

Usage: python3 gen_dex.py --decomp /path/to/pokefirered --out PokemonFRLG_AutoStoryDex.h
"""

import argparse
import os
import re

TYPES = ["NORMAL", "FIGHTING", "FLYING", "POISON", "GROUND", "ROCK", "BUG", "GHOST", "STEEL",
         "MYSTERY", "FIRE", "WATER", "GRASS", "ELECTRIC", "PSYCHIC", "ICE", "DRAGON", "DARK"]

#  How the battle logic values a move (see AutoStoryDex::MoveClass).
EFFECT_CLASS = {
    "EFFECT_LEVEL_DAMAGE": "LEVEL_DAMAGE",
    "EFFECT_PSYWAVE": "LEVEL_DAMAGE",
    "EFFECT_DRAGON_RAGE": "FIXED_40",
    "EFFECT_SONICBOOM": "FIXED_20",
    "EFFECT_SUPER_FANG": "FIXED_20",
    "EFFECT_OHKO": "AVOID",
    "EFFECT_EXPLOSION": "AVOID",
    "EFFECT_DREAM_EATER": "AVOID",
    "EFFECT_SNORE": "AVOID",
    "EFFECT_FOCUS_PUNCH": "AVOID",
    "EFFECT_SPIT_UP": "AVOID",
    "EFFECT_COUNTER": "AVOID",
    "EFFECT_MIRROR_COAT": "AVOID",
    "EFFECT_BIDE": "AVOID",
    "EFFECT_FUTURE_SIGHT": "AVOID",
    "EFFECT_FALSE_SWIPE": "WEAK",
    "EFFECT_RECHARGE": "HALF",
    "EFFECT_TWO_TURNS_ATTACK": "HALF",
    "EFFECT_SOLARBEAM": "HALF",
    "EFFECT_SKY_ATTACK": "HALF",
    "EFFECT_SKULL_BASH": "HALF",
    "EFFECT_RAZOR_WIND": "HALF",
    "EFFECT_SEMI_INVULNERABLE": "HALF",
    "EFFECT_MULTI_HIT": "TIMES_3",
    "EFFECT_DOUBLE_HIT": "TIMES_2",
    "EFFECT_TWINEEDLE": "TIMES_2",
    "EFFECT_TRIPLE_KICK": "TIMES_2",
    "EFFECT_BEAT_UP": "AVOID",
    "EFFECT_RECOIL": "RECOIL",
    "EFFECT_DOUBLE_EDGE": "RECOIL",
    "EFFECT_RECOIL_IF_MISS": "RECOIL",
    "EFFECT_FLAIL": "VARIABLE",
    "EFFECT_RETURN": "VARIABLE",
    "EFFECT_FRUSTRATION": "VARIABLE",
    "EFFECT_LOW_KICK": "VARIABLE",
    "EFFECT_PRESENT": "VARIABLE",
    "EFFECT_MAGNITUDE": "VARIABLE",
    "EFFECT_HIDDEN_POWER": "VARIABLE",
    "EFFECT_ENDEAVOR": "AVOID",
    "EFFECT_ERUPTION": "VARIABLE",
    "EFFECT_WEATHER_BALL": "VARIABLE",
    "EFFECT_REVENGE": "VARIABLE",
    "EFFECT_SMELLINGSALT": "VARIABLE",
    "EFFECT_FACADE": "VARIABLE",
    "EFFECT_ROLLOUT": "VARIABLE",
    "EFFECT_FURY_CUTTER": "VARIABLE",
    "EFFECT_PURSUIT": "VARIABLE",
}

HM_MOVES = {"MOVE_CUT", "MOVE_FLY", "MOVE_SURF", "MOVE_STRENGTH", "MOVE_FLASH", "MOVE_ROCK_SMASH",
            "MOVE_WATERFALL", "MOVE_DIVE"}


def parse_blocks(text, prefix):
    """[PREFIX_X] = { ... } blocks -> {PREFIX_X: body}"""
    out = {}
    for m in re.finditer(r"\[(%s\w+)\]\s*=\s*\{(.*?)\n\s*\}," % prefix, text, re.S):
        out[m.group(1)] = m.group(2)
    return out


def field(body, name):
    m = re.search(r"\.%s\s*=\s*([^,\n]+)" % name, body)
    return m.group(1).strip() if m else None


def slug_of(name):
    s = name.lower()
    s = s.replace("♀", "-f").replace("♂", "-m")
    s = s.replace("{female}", "-f").replace("{male}", "-m")
    s = s.replace(". ", "-").replace(".", "").replace("'", "").replace(" ", "-")
    return s


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decomp", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    root = args.decomp

    moves_text = open(os.path.join(root, "src/data/battle_moves.h")).read()
    names_text = open(os.path.join(root, "src/data/text/move_names.h"), encoding="utf-8").read()
    move_names = dict(re.findall(r"\[(MOVE_\w+)\]\s*=\s*_\(\"([^\"]*)\"\)", names_text))
    blocks = parse_blocks(moves_text, "MOVE_")
    moves = []
    for key, body in blocks.items():
        if key == "MOVE_NONE" or key not in move_names:
            continue
        effect = field(body, "effect")
        power = int(field(body, "power") or 0)
        mtype = field(body, "type").replace("TYPE_", "")
        acc = int(field(body, "accuracy") or 0)
        pp = int(field(body, "pp") or 0)
        cls = EFFECT_CLASS.get(effect, "DAMAGE" if power > 0 else "STATUS")
        if power == 0 and cls in ("DAMAGE",):
            cls = "STATUS"
        moves.append((move_names[key], mtype, power, acc, pp, cls, key in HM_MOVES))

    species_text = open(os.path.join(root, "src/data/pokemon/species_info.h")).read()
    sp_names_text = open(os.path.join(root, "src/data/text/species_names.h"), encoding="utf-8").read()
    sp_names = dict(re.findall(r"\[(SPECIES_\w+)\]\s*=\s*_\(\"([^\"]*)\"\)", sp_names_text))
    species = []
    #  The body may not run into the next species: SPECIES_NONE has no types,
    #  and its match took BULBASAUR's (Bulbasaur was missing).
    for m in re.finditer(r"\[(SPECIES_\w+)\]\s*=\s*\{((?:(?!\[SPECIES_).)*?)\.types\s*=\s*\{\s*TYPE_(\w+)\s*,\s*TYPE_(\w+)\s*\}", species_text, re.S):
        key, t1, t2 = m.group(1), m.group(3), m.group(4)
        if key not in sp_names or key == "SPECIES_NONE" or key.startswith("SPECIES_OLD_UNOWN"):
            continue
        name = sp_names[key]
        if name in ("??????????", "?"):
            continue
        base = {}
        for stat in ("HP", "Attack", "Defense", "SpAttack", "SpDefense", "Speed"):
            mm = re.search(r"\.base%s\s*=\s*(\d+)" % stat, m.group(2))
            base[stat] = int(mm.group(1)) if mm else 0
        species.append((slug_of(name), name, t1, t2,
                        (base["HP"], base["Attack"], base["Defense"], base["SpAttack"], base["SpDefense"], base["Speed"])))

    bm = open(os.path.join(root, "src/battle_main.c")).read()
    m = re.search(r"gTypeEffectiveness\[\d+\]\s*=\s*\{(.*?)\};", bm, re.S)
    chart = {}
    for a, d, mul in re.findall(r"TYPE_(\w+),\s*TYPE_(\w+),\s*TYPE_MUL_(\w+)", m.group(1)):
        if a in ("FORESIGHT", "ENDTABLE"):
            continue
        val = {"NO_EFFECT": 0, "NOT_EFFECTIVE": 5, "SUPER_EFFECTIVE": 20, "NORMAL": 10}[mul]
        chart[(a, d)] = val

    L = []
    L.append("/*  Pokemon FRLG Autostory Dex")
    L.append(" *")
    L.append(" *  From: https://github.com/PokemonAutomation/")
    L.append(" *")
    L.append(" *  GENERATED by SerialPrograms/Scripts/PokemonFRLG/AutoStory/gen_dex.py from")
    L.append(" *  the pret/pokefirered decomp (battle_moves.h, move_names.h, species_info.h,")
    L.append(" *  battle_main.c).")
    L.append(" *  Do not edit by hand.")
    L.append(" *")
    L.append(" */")
    L.append("")
    L.append("#ifndef PokemonAutomation_PokemonFRLG_AutoStoryDex_H")
    L.append("#define PokemonAutomation_PokemonFRLG_AutoStoryDex_H")
    L.append("")
    L.append("#include <stdint.h>")
    L.append("#include <vector>")
    L.append("")
    L.append("namespace PokemonAutomation{")
    L.append("namespace NintendoSwitch{")
    L.append("namespace PokemonFRLG{")
    L.append("namespace AutoStoryDex{")
    L.append("")
    L.append("enum class Type : uint8_t{")
    for t in TYPES:
        L.append("    %s," % t)
    L.append("};")
    L.append("")
    L.append("//  How a move is valued when choosing one.")
    L.append("enum class MoveClass : uint8_t{")
    for c in ["DAMAGE", "STATUS", "LEVEL_DAMAGE", "FIXED_40", "FIXED_20", "AVOID", "WEAK", "HALF",
              "TIMES_2", "TIMES_3", "RECOIL", "VARIABLE"]:
        L.append("    %s," % c)
    L.append("};")
    L.append("")
    L.append("struct MoveInfo{")
    L.append("    const char* name;       //  as shown in the game (English)")
    L.append("    Type type;")
    L.append("    uint8_t power;")
    L.append("    uint8_t accuracy;       //  0 = never misses")
    L.append("    uint8_t pp;")
    L.append("    MoveClass move_class;")
    L.append("    bool hm;                //  HM moves cannot be forgotten")
    L.append("};")
    L.append("")
    L.append("struct SpeciesInfo{")
    L.append("    const char* slug;       //  Pokemon Automation slug")
    L.append("    const char* name;       //  as shown in the game (English)")
    L.append("    Type type1;")
    L.append("    Type type2;")
    L.append("    //  Base stats.")
    L.append("    uint8_t hp = 0;")
    L.append("    uint8_t attack = 0;")
    L.append("    uint8_t defense = 0;")
    L.append("    uint8_t sp_attack = 0;")
    L.append("    uint8_t sp_defense = 0;")
    L.append("    uint8_t speed = 0;")
    L.append("};")
    L.append("")
    L.append("inline const std::vector<MoveInfo>& moves(){")
    L.append("    static const std::vector<MoveInfo> data{")
    for name, t, p, a, pp, cls, hm in moves:
        L.append("        {%s, Type::%s, %d, %d, %d, MoveClass::%s, %s}," % (
            cstr(name), t, p, a, pp, cls, "true" if hm else "false"))
    L.append("    };")
    L.append("    return data;")
    L.append("}")
    L.append("")
    L.append("inline const std::vector<SpeciesInfo>& species(){")
    L.append("    static const std::vector<SpeciesInfo> data{")
    for slug, name, t1, t2, b in species:
        L.append("        {%s, %s, Type::%s, Type::%s, %d, %d, %d, %d, %d, %d}," % (
            (cstr(slug), cstr(name.encode("ascii", "replace").decode()), t1, t2) + b))
    L.append("    };")
    L.append("    return data;")
    L.append("}")
    L.append("")
    L.append("//  Damage multiplier x10 (0, 5, 10, 20) for one attacking and one")
    L.append("//  defending type (gTypeEffectiveness).")
    L.append("inline uint8_t type_multiplier_x10(Type attack, Type defense){")
    L.append("    static const uint8_t table[%d][%d]{" % (len(TYPES), len(TYPES)))
    for a in TYPES:
        row = ", ".join("%2d" % chart.get((a, d), 10) for d in TYPES)
        L.append("        {%s},   //  %s" % (row, a))
    L.append("    };")
    L.append("    return table[(size_t)attack][(size_t)defense];")
    L.append("}")
    L.append("")
    L.append("}")
    L.append("}")
    L.append("}")
    L.append("}")
    L.append("#endif")
    with open(args.out, "w", newline="\r\n") as f:
        f.write("\n".join(L) + "\n")
    print("moves %d, species %d, chart entries %d" % (len(moves), len(species), len(chart)))


if __name__ == "__main__":
    main()

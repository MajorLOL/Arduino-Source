"""The wiki page Pokedex-Availability.md from dex_availability.py's json:
python3 dex_report.py avail.json Pokedex-Availability.md"""
import json
import re
import sys
from collections import OrderedDict

d = json.load(open(sys.argv[1]))
OUT = sys.argv[2]
V = d["versions"]
ST = d["starters"]
COMBOS = [(v, s) for v in V for s in ST]
C = d["combos"]
NAME = d["names"]
DEX = d["dex"]
PRE = d["pre"]
EVOS = d["evolutions"]
SHORT = {"FireRed": "FR", "LeafGreen": "LG", "BULBASAUR": "Bulb.", "CHARMANDER": "Char.", "SQUIRTLE": "Squi."}


def key(v, s):
    return "%s_%s" % (v, s)


def name(sp):
    return NAME.get(sp, sp)


POST_WORDS = ("Four Island", "Five Island", "Six Island", "Seven Island", "Cerulean Cave", "Water Labyrinth")


def summarize(hows, limit=3, groups_max=3):
    """Group "method: place (needs ...)" texts by method; places before the
    Hall of Fame first; breeding only when nothing else."""
    groups = OrderedDict()
    breed = []
    for h in hows:
        mb = re.match(r"^breed (.*?)(?: holding the .*)? at the Four Island Day Care$", h)
        if mb:
            breed.append(mb.group(1))
            continue
        m = re.match(r"^(.*?): (.*?)( \(needs .*?\))?( \[choice: .*\])?$", h)
        if not m:
            groups.setdefault(h, [])
            continue
        method, place, needs, choice = m.group(1), m.group(2), m.group(3) or "", m.group(4) or ""
        needs = needs[len(" (needs "):-1] if needs else ""
        needs = ", ".join(x for x in needs.split(", ") if x and x not in method)
        label = method[:1].upper() + method[1:] + (" (%s)" % needs if needs else "") + choice
        g = groups.setdefault(label, [])
        if place not in g:
            g.append(place)
    parts = []
    for method, places in groups.items():
        places = sorted(places, key=lambda p: 2 if any(w in p for w in POST_WORDS) else
                        1 if any(w in p for w in ("One Island", "Two Island", "Three Island", "Mt. Ember")) else 0)
        if not places:
            parts.append(method)
            continue
        shown = ", ".join(places[:limit]) + (" +%d more" % (len(places) - limit) if len(places) > limit else "")
        parts.append("%s: %s" % (method, shown))
    if len(parts) > groups_max:
        parts = parts[:groups_max] + ["…"]
    if not parts and breed:
        inc = [h for h in hows if "holding the" in h]
        if inc:
            parts.append(inc[0][:1].upper() + inc[0][1:])
        else:
            parts.append("Breed %s at the Four Island Day Care" % " or ".join(sorted(set(breed))))
    return "; ".join(parts)


def evo_text(sp):
    p = PRE.get(sp)
    if not p:
        return ""
    for e in EVOS.get(p["from"], []):
        if e["to"] == sp:
            return "from %s: %s%s" % (name(p["from"]), e["need"], " (National Dex)" if e["phase"] == "post-game" and e["possible"] else "")
    return ""


L = []
L.append("# Pokédex availability: FireRed and LeafGreen")
L.append("")
L.append("Which Pokémon one save can register as **caught**, for both games and all three starters. "
         "Worked out from the game's own data (pret/pokefirered: wild encounter tables, event scripts, "
         "in-game trades, evolutions, breeding), by `SerialPrograms/Scripts/PokemonFRLG/AutoStory/dex_availability.py`.")
L.append("")
L.append("**Counted as obtainable:** wild Pokémon (grass and caves, surfing, the three rods, Rock Smash, the Safari Zone), "
         "gifts, one-time battles (legendaries, Snorlax, the Power Plant's Electrode, Hypno in Berry Forest), the roaming "
         "legendary, Game Corner prizes, the in-game trades with people in the game (they need no other game), the Togepi "
         "egg, breeding at the Four Island Day Care, and evolutions that one game can do.")
L.append("")
L.append("**Not counted:** trading with another game (also every trade evolution), Mystery Gift events "
         "(Mew, Celebi, Jirachi, the other Altering Cave tables), and Pokémon that are not in these games at all.")
L.append("")
L.append("**Switch version:** it puts the Mystic Ticket and the Aurora Ticket in the bag after the Hall of Fame (on a GBA "
         "they came only from events), so Lugia, Ho-Oh (Navel Rock) and Deoxys (Birth Island) are counted. On a GBA the "
         "counts below are 3 lower. On Switch the roaming legendary also no longer disappears after it uses Roar.")
L.append("")

# ---- Summary ------------------------------------------------------------------
L.append("## Summary")
L.append("")
L.append("| Game | Starter | Registrable | Before the Hall of Fame | Only after it | Roaming legendary |")
L.append("|---|---|---|---|---|---|")
ROAMER = {"BULBASAUR": "Entei", "CHARMANDER": "Suicune", "SQUIRTLE": "Raikou"}
for v, s in COMBOS:
    c = C[key(v, s)]
    L.append("| %s | %s | **%d** | %d | %d | %s |" % (v, s.capitalize(), c["count"] - 2, c["story"] - 2,
                                                      c["count"] - c["story"], ROAMER[s]))
L.append("")
L.append("The numbers count one fossil: the Helix Fossil (Omanyte, Omastar) **or** the Dome Fossil (Kabuto, Kabutops) "
         "at Mt. Moon; the other is taken and never comes back. Hitmonlee and Hitmonchan are both counted: the Dojo gives "
         "one, the other (and Hitmontop) comes from breeding a Tyrogue after the Hall of Fame.")
L.append("")

# ---- Version differences -------------------------------------------------------
fr = set(C[key("FireRed", "CHARMANDER")]["species"])
lg = set(C[key("LeafGreen", "CHARMANDER")]["species"])
L.append("## 1. FireRed and LeafGreen")
L.append("")
L.append("Apart from these, both games can register exactly the same Pokémon (checked for every species).")
L.append("")
L.append("| Only in FireRed | How | Only in LeafGreen | How |")
L.append("|---|---|---|---|")
fro = sorted(fr - lg, key=lambda s: DEX[s])
lgo = sorted(lg - fr, key=lambda s: DEX[s])
for i in range(max(len(fro), len(lgo))):
    a = fro[i] if i < len(fro) else None
    b = lgo[i] if i < len(lgo) else None

    def cell(sp, v):
        if sp is None:
            return "", ""
        e = C[key(v, "CHARMANDER")]["species"][sp]
        how = summarize(e["direct_how"], 2, 2) if e["direct_how"] else ", ".join(x.replace("evolve ", "evolve ") for x in e["evolve"])
        return "#%03d %s" % (DEX[sp], name(sp)), how
    a1, a2 = cell(a, "FireRed")
    b1, b2 = cell(b, "LeafGreen")
    L.append("| %s | %s | %s | %s |" % (a1, a2, b1, b2))
L.append("")
L.append("Version differences that are not exclusives:")
L.append("")
L.append("- **Game Corner prizes:** FireRed has Scyther, LeafGreen Pinsir (Abra, Clefairy, Dratini and Porygon in both, at other levels).")
L.append("- **In-game trades:** Underground Path: FireRed gives Nidoran♀ for Nidoran♂, LeafGreen Nidoran♂ for Nidoran♀. "
         "Route 11 gate: FireRed gives Nidorina for Nidorino, LeafGreen Nidorino for Nidorina. Route 18 gate (Lickitung): "
         "FireRed asks for Golduck, LeafGreen for Slowbro. The others (Mr. Mime for Abra, Jynx for Poliwhirl, Farfetch'd for "
         "Spearow, Electrode for Raichu, Tangela for Venonat, Seel for Ponyta) are the same.")
L.append("- **Breeding babies follow their parents:** Elekid only in FireRed (Electabuzz), Magby only in LeafGreen (Magmar).")
L.append("")

# ---- Starters -----------------------------------------------------------------------
L.append("## 2. The starter")
L.append("")
L.append("The starter changes exactly two things about what one save can register (checked for every species):")
L.append("")
L.append("| Starter | Its line (the other two lines need another game) | Roaming legendary (the other two never appear) |")
L.append("|---|---|---|")
for s, line, r in (("Bulbasaur", "Bulbasaur, Ivysaur, Venusaur", "Entei"), ("Charmander", "Charmander, Charmeleon, Charizard", "Suicune"),
                   ("Squirtle", "Squirtle, Wartortle, Blastoise", "Raikou")):
    L.append("| %s | %s | %s |" % (s, line, r))
L.append("")
L.append("The roaming legendary starts roaming Kanto after the Sapphire is given to Celio (One Island), after the Hall of Fame.")
L.append("")
L.append("**HMs.** What the starter's line can learn, and what the program's team does about the rest:")
L.append("")
L.append("| Starter line | Cut | Fly | Surf | Strength | Flash | Rock Smash | Waterfall |")
L.append("|---|---|---|---|---|---|---|---|")
L.append("| Bulbasaur | ✓ | – | – | ✓ | ✓ | ✓ | – |")
L.append("| Charmander | ✓ | ✓ (Charizard) | – | ✓ | – | ✓ | – |")
L.append("| Squirtle | – | – | ✓ | ✓ | – | ✓ | ✓ |")
L.append("")
L.append("- **Cut** is needed before Lt. Surge: with Squirtle the program catches a helper on Route 24 — Oddish in FireRed, "
         "Bellsprout in LeafGreen (each game has only one of them there).")
L.append("- **Surf and Strength:** Lapras (Silph Co. gift) in all three teams. **Fly:** only Charizard, so only the Charmander "
         "run flies. Flash is not needed by the route (Rock Tunnel is walked in the dark).")
L.append("- The story's team: Voltorb (Route 10) and Lapras for all starters; Eevee becomes Vaporeon (Charmander), Jolteon "
         "(Bulbasaur) or Flareon (Squirtle).")
L.append("")

# ---- Full list ----------------------------------------------------------------
L.append("## 3. Every registrable Pokémon")
L.append("")
L.append("✓ = can be registered in that game with that starter. *Post* = only after the Hall of Fame. "
         "*Evolve* = never met like this: only by evolving (see section 4).")
L.append("")
hdr = " | ".join("%s %s" % (SHORT[v], SHORT[s]) for v, s in COMBOS)
L.append("| # | Pokémon | %s | When | How |" % hdr)
L.append("|---|---|%s|---|---|" % "|".join("---" for _ in COMBOS))
allsp = sorted(set(sp for c in C.values() for sp in c["species"]), key=lambda s: DEX[s])
for sp in allsp:
    marks = []
    phases = set()
    entry = None
    for v, s in COMBOS:
        e = C[key(v, s)]["species"].get(sp)
        marks.append("✓" if e else "–")
        if e:
            phases.add(e["phase"])
            entry = entry or e
    when = "story" if "story" in phases else "post"
    if entry["direct_how"]:
        how = summarize(entry["direct_how"])
        if entry["direct"] == "post-game" and entry["phase"] == "story":
            how = "Evolve (%s); %s" % (evo_text(sp), how)
    else:
        how = "Evolve " + evo_text(sp)
    if sp in ("OMANYTE", "OMASTAR", "KABUTO", "KABUTOPS"):
        how += " — *one fossil per save*"
    L.append("| %03d | %s | %s | %s | %s |" % (DEX[sp], name(sp), " | ".join(marks), when, how.replace("|", "/")))
L.append("")

# ---- Evolution catch list -------------------------------------------------------
L.append("## 4. Pokémon to evolve, and how many to catch")
L.append("")
L.append("Some Pokémon are never met in their evolved forms. Catching the first form registers only that form, so the "
         "catch list has **an extra copy for each form that must be evolved**: one copy is kept as it is, the other is "
         "trained and evolved (which registers every form on the way). A Pokémon that splits into several evolutions "
         "(Eevee, Tyrogue) needs a copy per evolution.")
L.append("")
L.append("Requirements: *level N* = level up to N; stones from the Celadon Dept. Store 4F (Fire, Water, Thunder, Leaf; "
         "¥2,100 each); Moon Stones: Mt. Moon 1F and B2F (hidden), Rocket Hideout B2F, Pokémon Mansion 1F (hidden), Lostelle's "
         "father (Two Island) — 5 for the 4 needed (Nidorina, Nidorino, Clefairy, Jigglypuff); the one Sun Stone "
         "(Gloom → Bellossom) is in Ruin Valley, Six Island, after the Hall of Fame. "
         "*High friendship* = 220+ (walking with it, battles, vitamins). An evolution into a non-Kanto Pokémon "
         "(Crobat, Blissey, ...) needs the National Dex first (after the Hall of Fame).")
L.append("")


def plan(version, starter):
    c = C[key(version, starter)]["species"]
    rows = []
    #  Evolution-only species, by the nearest form that can be caught/got.
    need = {}
    for sp, e in c.items():
        if e["direct"] is not None:
            continue
        chain = [sp]
        cur = sp
        while cur in PRE and c.get(PRE[cur]["from"], {}).get("direct") is None:
            cur = PRE[cur]["from"]
            chain.append(cur)
        src = PRE[cur]["from"]
        need.setdefault(src, []).append(sp)
    #  Leaves only: a copy evolved to the last form registers the ones between.
    for src, lst in need.items():
        leaves = [x for x in lst if not any(PRE.get(y, {}).get("from") == x for y in lst)]
        steps = []
        for leaf in sorted(leaves, key=lambda s: DEX[s]):
            path = [leaf]
            while PRE[path[-1]]["from"] != src:
                path.append(PRE[path[-1]]["from"])
            path.reverse()
            txt = []
            prev = src
            for x in path:
                for ev in EVOS.get(prev, []):
                    if ev["to"] == x:
                        txt.append("%s (%s)" % (name(x), ev["need"]))
                prev = x
            steps.append(" → ".join(txt))
        if src == "TYROGUE":
            steps.append("Hitmonlee or Hitmonchan, whichever the Dojo did not give (level 20 with Attack > Defense / Attack < Defense)")
            leaves.append("HITMON_OTHER")
        rows.append((src, 1 + len(leaves), steps, c[src]))
    return sorted(rows, key=lambda r: DEX[r[0]])


for v in V:
    L.append("### %s" % v)
    L.append("")
    L.append("| Catch / get | Copies | Evolve the extra copies into | Where the first form comes from |")
    L.append("|---|---|---|---|")
    base = plan(v, "CHARMANDER")
    for src, copies, steps, e in base:
        if src in ("CHARMANDER", "CHARMELEON"):
            continue
        where = summarize(e["direct_how"], 2)
        if src == "EEVEE":
            where += " — one gift: the extra copies by breeding it (after the Hall of Fame)"
        if src == "TYROGUE":
            where += " (with Ditto)"
        L.append("| %s | %d | %s | %s |" % (name(src), copies, "<br>".join(steps), where.replace("|", "/")))
    L.append("| *the starter* | 1 | it evolves in the story (Ivysaur/Venusaur, Charmeleon/Charizard, Wartortle/Blastoise) | "
             "Professor Oak; a second copy to keep unevolved only by breeding after the Hall of Fame |")
    L.append("")

# ---- Not registrable ----------------------------------------------------------
L.append("## 5. Not registrable without another game")
L.append("")
L.append("Kanto and Johto Pokémon (#001–#251) that one save cannot register, and why. Of the Hoenn Pokémon "
         "(#252–#386) only Azurill (LeafGreen), Wynaut (breeding with an incense from the Lost Cave) and Deoxys "
         "(Switch: Aurora Ticket) can be registered; the others are not in these games.")
L.append("")
L.append("| # | Pokémon | FireRed | LeafGreen |")
L.append("|---|---|---|---|")
for sp in sorted(DEX, key=lambda s: DEX[s]):
    if DEX[sp] > 251:
        continue
    rf = C[key("FireRed", "CHARMANDER")]["missing"].get(sp)
    rl = C[key("LeafGreen", "CHARMANDER")]["missing"].get(sp)
    if sp in ("BULBASAUR", "IVYSAUR", "VENUSAUR", "SQUIRTLE", "WARTORTLE", "BLASTOISE", "ENTEI", "RAIKOU"):
        rf = rl = "only with another starter"
    if sp in ("CHARMANDER", "CHARMELEON", "CHARIZARD", "SUICUNE"):
        continue
    if rf is None and rl is None:
        continue
    L.append("| %03d | %s | %s | %s |" % (DEX[sp], name(sp), rf or "✓", rl or "✓"))
L.append("")
open(OUT, "w").write("\n".join(L) + "\n")
print("written", OUT, len(L), "lines")


# ---- C++ data for the program's Pokedex collection table ----------------------
#  python3 dex_report.py avail.json Pokedex-Availability.md PokemonFRLG_AutoStoryDexCollection.h
def slug_of(n):
    s = n.lower()
    s = s.replace("♀", "-f").replace("♂", "-m")
    s = s.replace(". ", "-").replace(".", "").replace("'", "").replace(" ", "-")
    return s


def cstr(s):
    s = s.replace("é", "e").replace("♀", " F").replace("♂", " M").replace("→", "->").replace("—", "-").replace("…", "...")
    s = s.encode("ascii", "replace").decode()
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


if len(sys.argv) > 3:
    #  Bits of the program's Starter enum order: BULBASAUR 0, SQUIRTLE 1,
    #  CHARMANDER 2; FireRed bits 0-2, LeafGreen bits 3-5.
    S_BIT = {"BULBASAUR": 0, "SQUIRTLE": 1, "CHARMANDER": 2}
    plans = {v: {r[0]: r for r in plan(v, "CHARMANDER")} for v in V}
    rows = []
    for sp in allsp:
        mask = 0
        for gi, v in enumerate(V):
            for s in ST:
                if sp in C[key(v, s)]["species"]:
                    mask |= 1 << (gi * 3 + S_BIT[s])
        copies, how, evolve = [], [], []
        post = True
        for v in V:
            e = None
            for s in ST:
                e = e or C[key(v, s)]["species"].get(sp)
            if e is None:
                copies.append(0)
                how.append("")
                evolve.append("")
                continue
            post = post and e["phase"] == "post-game"
            if sp in ("CHARMANDER", "BULBASAUR", "SQUIRTLE"):
                copies.append(1)
                how.append("Professor Oak (the starter)")
                evolve.append("it evolves in the story")
                continue
            if e["direct_how"]:
                h = summarize(e["direct_how"], 2, 2)
                if sp in plans[v]:
                    _, n, steps, _ = plans[v][sp]
                    copies.append(n)
                    evolve.append("; ".join(steps))
                else:
                    copies.append(1)
                    evolve.append("")
                how.append(h)
            else:
                copies.append(0)
                how.append("evolve " + evo_text(sp))
                evolve.append("")
        rows.append((DEX[sp], slug_of(name(sp)), name(sp), mask, post, copies, how, evolve))
    H = []
    H.append("/*  Pokemon FRLG Autostory Dex Collection")
    H.append(" *")
    H.append(" *  From: https://github.com/PokemonAutomation/")
    H.append(" *")
    H.append(" *  GENERATED by SerialPrograms/Scripts/PokemonFRLG/AutoStory/dex_report.py from")
    H.append(" *  dex_availability.py (pret/pokefirered). Do not edit by hand.")
    H.append(" *")
    H.append(" *  Every Pokemon one save can register as caught without trading with")
    H.append(" *  another game (Switch version: the Mystic and Aurora Tickets).")
    H.append(" *")
    H.append(" */")
    H.append("")
    H.append("#ifndef PokemonAutomation_PokemonFRLG_AutoStoryDexCollection_H")
    H.append("#define PokemonAutomation_PokemonFRLG_AutoStoryDexCollection_H")
    H.append("")
    H.append("#include <stdint.h>")
    H.append("")
    H.append("namespace PokemonAutomation{")
    H.append("namespace NintendoSwitch{")
    H.append("namespace PokemonFRLG{")
    H.append("namespace AutoStoryDex{")
    H.append("")
    H.append("")
    H.append("struct CollectionEntry{")
    H.append("    uint16_t dex;           //  National POKeDEX number")
    H.append("    const char* slug;       //  Pokemon Automation slug")
    H.append("    const char* name;")
    H.append("    //  In which game with which starter: bit game * 3 + starter, game 0")
    H.append("    //  FireRed, 1 LeafGreen; starter 0 Bulbasaur, 1 Squirtle, 2 Charmander.")
    H.append("    uint8_t available;")
    H.append("    bool post_game;         //  only after the Hall of Fame")
    H.append("    //  Per game (FireRed, LeafGreen): copies to catch or get (1, or more")
    H.append("    //  to evolve the extra ones; 0: only by evolving), where and how, and")
    H.append("    //  what the extra copies are evolved into.")
    H.append("    uint8_t copies[2];")
    H.append("    const char* how[2];")
    H.append("    const char* evolve[2];")
    H.append("};")
    H.append("")
    H.append("inline const CollectionEntry* collection(size_t& count){")
    H.append("    static const CollectionEntry data[] = {")
    for dex, slug, nm, mask, post, cp, how, ev in rows:
        H.append("        {%d, %s, %s, 0x%02x, %s, {%d, %d}, {%s, %s}, {%s, %s}}," % (
            dex, cstr(slug), cstr(nm), mask, "true" if post else "false", cp[0], cp[1],
            cstr(how[0]), cstr(how[1]), cstr(ev[0]), cstr(ev[1])))
    H.append("    };")
    H.append("    count = sizeof(data) / sizeof(data[0]);")
    H.append("    return data;")
    H.append("}")
    H.append("")
    H.append("")
    H.append("}")
    H.append("}")
    H.append("}")
    H.append("}")
    H.append("#endif")
    open(sys.argv[3], "w", newline="\r\n").write("\n".join(H) + "\n")
    print("written", sys.argv[3], len(rows), "species")

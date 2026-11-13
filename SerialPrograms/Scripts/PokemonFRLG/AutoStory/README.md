# FRLG Autostory generator

These scripts write the data headers of the FireRed/LeafGreen Autostory
program (`SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/`):

| Header | Made by | What is in it |
|---|---|---|
| `PokemonFRLG_AutoStoryData.h` | `generate.py` | The story for each starter: segments, steps, and every walk as straight runs |
| `PokemonFRLG_AutoStoryDex.h` | `gen_dex.py` | Moves, species and the type chart, for the battle logic |
| `PokemonFRLG_AutoStoryDexCollection.h` | `dex_availability.py`, `dex_report.py` | Every species one save can register per game and starter (no trading), how many to catch, where and how: the program's Pokedex collection table |

The headers are generated output. Do not edit them by hand: change the
scripts and generate them again.

## What to read

The story itself is `story_spec.py` (about 1,200 lines): each segment as a
list of steps such as `talk(...)`, `heal(...)`, `grind_all(...)`,
`catch(...)`, `teach(...)` and `buy(...)`, with map names and tile
positions from the decompilation. Comments name the event script behind
each step.

The 14,000 lines of `PokemonFRLG_AutoStoryData.h` are what that spec turns
into once every walk is worked out tile by tile:

| File | Role |
|---|---|
| `story_spec.py` | The story: segments and their steps, team members, level targets (`LEVELS`). |
| `story.py` | Turns each step into C++ steps and walking legs. It keeps track of where the player stands, which way they face, and what the story has changed (flags, defeated trainers, moved people). |
| `engine.py` | The map model and the path search: collision, elevation, ledges, warps and map connections, people and how they move, trainer sight lines, wild-encounter tiles. Tiles a trainer can see, and grass, cost more, so routes go around them when they can. |
| `frlg_data.py` | Reads the decompilation: map layouts, metatile behaviors, events, connections. |
| `generate.py` | Builds the story for every starter and writes the header, `story.json` (everything, for inspection) and `STORY.md` (an overview per segment, optionally with every leg drawn on its maps). |
| `render.py` | Draws the maps for those images (needs Pillow). |
| `gen_dex.py` | Writes `PokemonFRLG_AutoStoryDex.h`. |
| `battle_sim.py`, `team_sim.py` | Simulate the big fights with the decompilation's damage formula. They were used to choose the level targets in `story_spec.LEVELS` and the team; the generator does not run them. |
| `move_audit.py` | For every training spot: the wild Pokemon there, the move the program picks against each, and the one that does the most damage. |
| `log_video_check.py` | Reads logs of runs (`SerialPrograms.log`): whether the picture falls behind while a video stream runs, and how many frames a minute came in. No decompilation needed. |
| `nugget_bridge_walk.py` | Not for the AutoStory: the Nugget Bridge Farmer's walk, frame by frame on the game's map, and how much each of its holds may be off. |

A walk is written as runs of one direction and a kind. `WALK`: walked in
one go. `CHECKED`: one step at a time, watching for a wild battle (grass,
caves, water). `CHECKED_TRAINER` and `APPROACH`: watching for a trainer who
may see the player. The C++ types are in `PokemonFRLG_AutoStoryScript.h`.

## Running it

You need Python 3 (tested with 3.13) and a checkout of
[pret/pokefirered](https://github.com/pret/pokefirered). The headers in the
repository were made from revision `ce6406d`. The decompilation does not
need to be built. Clone it outside this repository, then run from this
folder:

```
git clone https://github.com/pret/pokefirered /path/to/pokefirered
git -C /path/to/pokefirered checkout ce6406d

python3 generate.py --decomp /path/to/pokefirered --out out --no-images
python3 gen_dex.py --decomp /path/to/pokefirered --out out/PokemonFRLG_AutoStoryDex.h
python3 dex_availability.py /path/to/pokefirered out/dex.json
python3 dex_report.py out/dex.json out/Pokedex-Availability.md out/PokemonFRLG_AutoStoryDexCollection.h
```

`generate.py` takes about 3 minutes. Leave out `--no-images` to also draw
every leg into `out/images/` (needs `pip install pillow`; this takes much
longer). Warnings are printed for spots the route cannot fully avoid,
such as a trainer who walks across the path.

Then copy the headers over the ones in
`SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/`.

The output is the same on every run. With nothing changed, the copied
headers are identical to the ones in the repository (CRLF line endings, as
written). After a change to the spec, `git diff` on the header shows what
the change did to the routes.

## Changing the story

1. Edit `story_spec.py`. For example, to talk to a trainer on purpose
   before healing:
   `talk("MAP_ROCK_TUNNEL_B1F", xy=[2, 6], mode="battle", label="PokeManiac Cooper")`.
2. Run `generate.py` and read the step list of the segment in `out/STORY.md`.
3. Copy the header, build, and test the segment on the console.

Step numbers inside a segment are what the program's "Start at step"
option counts. Adding or moving steps changes them for that segment.

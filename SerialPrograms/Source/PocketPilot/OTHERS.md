# Other controller programs and their help forum, and what PocketPilot has of them

Read on 13 November 2026, for ideas and for faults that others ran into and that could be here too:

- the help forum of AutoController on Discord (`#autocontroller-help` of the server "Pokémon Auto Controller"): its 21 posts, December 2025 to August 2026, each to its last message;
- [AutoController2](https://github.com/brianuuu/AutoController2) (version 0.6.0 of September 2026): its programs, their manuals and how a program is built;
- five more projects that a second review named: [AutoController_swsh](https://github.com/brianuuu/AutoController_swsh), [EasyCon](https://github.com/EasyConNS/EasyCon), [Poke-Controller](https://github.com/KawaSwitch/Poke-Controller) with [Poke-Controller-Modified](https://github.com/Moi-poke/Poke-Controller-Modified), [bdsp-automation](https://github.com/Brikwerk/bdsp-automation) and [AutoTableTurf](https://github.com/fga401/AutoTableTurf): their code and the fixes in their history;
- Pokémon Automation's own FireRed and LeafGreen fixes since this fork was made.

The forum is the help forum of another program. None of its posts is about Pokémon Automation or PocketPilot, so none of them is a fault report for this fork; what they show is what goes wrong for people who run such programs. Nothing of these projects was run and no code was taken from them: AutoController2, EasyCon and AutoTableTurf are under the GPL, AutoController_swsh has no license at all, and the three under the MIT license are written in Python.

## What came out of it

- **Fix 20: the Nugget Bridge Farmer's waits have an end** ([FIXES.md](FIXES.md), 20). In the forum AutoController2's Nugget Farmer "runs 1 extra step to the right sometimes" and is then stuck below the first trainer of the bridge. Pokémon Automation's Nugget Bridge Farmer walks by timings alone too, has one leg that must come out at an exact number of tiles, and had no limit on any of its waits: stuck there, it would mash B until someone looked. In build 10.
- **A walk along the walls for that farmer** (an experimental option, off by default, build 10). AutoController2's author solved the extra step by making the walk "hit the bridge wall to normalize". The same idea, worked out anew for Pokémon Automation's route and walked frame by frame on the game's own map: `SerialPrograms/Scripts/PokemonFRLG/AutoStory/nugget_bridge_walk.py`. Not run on a console.
- **Measured from this fork's own logs: the picture does not fall behind, and the frames keep coming** (below). The forum's two most common causes are not there on the computer this fork is tested on. The measurement is a script now: `log_video_check.py` in the same folder.
- **Checked in the code:** every open-ended wait of this fork's own FRLG programs has a limit; none of them queues button presses in a loop that goes by the clock (the fault Pokémon Automation fixed in its own FRLG code in October, #1684 and #1770); the video pipeline keeps the newest frame only.
- **Pokémon Automation's `main` of 13 November merged in** (four commits since the 11th, none about the AutoStory).
- **Ideas kept for the roadmap**, not built: Nugget Bridge as the story's source of money, a second sign of a shiny next to its sound, and the story's own screen readers tested on stored pictures in every build ([ROADMAP.md](../../../ROADMAP.md), section 3).

## The forum: 21 posts by cause

| Cause | Posts | What happened there | PocketPilot |
|---|---|---|---|
| The picture falls behind while the program runs | 2, and one withdrawn | Splatoon 3's Tableturf: "Unable to detect turn start" after about an hour; the delay was 200 to 300 ms after starting and 600 to 700 ms when it failed, and back to 200 to 300 after refreshing the camera. The Nugget Farmer: 794 ms, and 1,128 ms after a little less than an hour. Sending the picture through OBS's virtual camera solved the first. (The withdrawn post, "outdated", was the first one's earlier title.) | **Not here.** The video pipeline keeps the newest frame only, there is no line of frames to fall behind in. Measured: see below. |
| Frames dropped: the picture at 15 to 20 frames a second | 2 | The Yes/No box of the nickname question was on the screen for too few frames to be seen, and the program typed "AAAAAAAAAA" as the name. A static reset took a normal Zapdos for a shiny one: it times the start of the battle. A capture card on USB 2. | **Not here on the test computer.** Measured: see below. The AutoStory hears a shiny instead of timing it. |
| A camera delay longer than the program's fixed wait | 1 | After running from a battle the program took the next one as started (delay 626 ms on a Switch 1); the author made the wait longer. | The AutoStory looks for the end of a battle instead of waiting a fixed time. Where it pressed without looking and a late battle could swallow the press (getting off the water), it got a look in build 8. |
| Timings that are right on one console and not on another | 2 | The Nugget Farmer's extra step on a Switch 1. The Donut Maker of Legends: Z-A: waits of 3 seconds too short on a Switch 1 (the map, saving, restarting the game). | The AutoStory counts every walk in the picture and corrects it. **Fix 20** and the option "Walk along the walls" for Pokémon Automation's Nugget Bridge Farmer, the one FRLG program here that walks without looking. |
| Colours | 2 | Sword and Shield's egg boxes not recognised: too much colour, solved by the console's RGB range (Full to Limited, Switch 2). A starter reset of Brilliant Diamond: a cheap capture card's colours. In other posts: a green and purple tint from a 4K capture card, and the Switch 2's dark theme being another colour than the Switch 1's. | Not tried on another capture card. Pokémon Automation's detectors go mostly by the ratio between colours, not by fixed levels. Four readers of the AutoStory do go by fixed levels: the ball mark of a caught species in a battle, the load menu (is there a save, and its POKéDEX count) and the naming screen of a new game. They are the first to check on another capture card. |
| The TV size not at 100% | 1 | The nickname question not passed. (AutoController 1 checked the border; version 2 does not yet.) | **Here.** Checked at the start of the AutoStory and of the FRLG farmers (the black border of the picture). |
| The program's own mistakes and files | 6 | A mistake in a release (the Rocket's talk not seen), a gift reset that went to the naming screen after 10,000 resets, a file missing from a test version, the program unpacked in the Downloads folder, Japanese text data not downloaded, a sandwich that worked on the next try. | Not about this fork. |
| The board, its firmware, wiring and driver | 5 | A chip that had to come off an Arduino Uno, TX and RX wired the wrong way round, the wrong COM port, which .hex file. | Not about this fork: Pokémon Automation's own controller firmware and setup. |

## Measured from the logs of this fork's test runs

All the logs of the AutoStory's test runs, 27 October to 13 November 2026 (`log_video_check.py`):

- **The delay.** For every walk the log has the press and the moment the walk was seen finished in the picture. For equal walks the time between the two is the same, plus how late the picture is. Per video stream and per hour since it started, against the stream's own median: 30 streams, 162 hours together, the longest 17.4 hours, 15,881 walks. Of the 149 hourly medians 138 are within 5 ms and 143 within 10 ms; the other six are 13 to 48 ms off (one look of the walk check is 50 ms), three of them on the first day. No stream gets later with the hours. In the forum the delay grew by 300 ms and more within an hour.
- **The frames.** The log reports the frames of a minute now and then: 3,600 is 60 a second. From 29 October on: 1,854 minutes, the lowest 3,555, and 16 of them under 3,590. On 27 and 28 October, the first two days, 121 of 779 minutes had fewer than 3,500, the lowest 2,717 (45 a second).

What the logs cannot say is how late the picture is in milliseconds, only that it does not change. The program "Input Timing Test" (experimental) measures that; it has not been run yet.

## AutoController2

A Qt program like this one, for an Arduino or Teensy on a serial converter; Windows only. Its FRLG programs all have a counterpart here:

| AutoController2 | Here |
|---|---|
| Gift Reset, Starter Reset | Gift Reset (starters are one of its targets) |
| Static Reset | Legendary Reset, Legendary Run Away |
| Overworld Shiny (also fishing) | Shiny Hunt - Overworld, Shiny Hunt - Fishing |
| Prize Corner Reset, Pickup Farmer, Nugget Farmer | Prize Corner Reset, Pickup Farmer, Nugget Bridge Farmer |
| RNG Manipulation | RNG Helper and the Starter, Gift, Static, Wild, Roaming Legendary and Egg RNG programs |
| Camera Checker | The check of the TV size at a program's start, and the Input Timing Test (experimental, FRLG only) |
| Command Recorder, Custom Command | Record Keyboard Controller, Turbo Macro |
| Donut Maker, Respawn Reset (Legends: Z-A) | Pokémon Automation's Z-A programs |
| Mega Man Star Force and Splatoon programs | Nothing |

What it does that is worth knowing:

- **The states of its programs have a time limit and a message of their own.** Its Nugget Farmer stops with "Cannot detect dialog box with Team Rocket grunt", "Unable to detect battle start for too long, did you maxed out nuggets?" or "Unable to detect returning to PC for too long". Pokémon Automation's had neither; fix 20 gives it both.
- **It tells a shiny by the sound and, as a second sign, by the time the battle takes to start** (measured in the first battle, compared in the later ones). Its manual warns that a Pokémon with Intimidate gives a false alarm, and in the forum a slow picture did. A second sign is a good idea; this one is not good enough to take over.
- **Its button sequences can differ per console** (a "Switch1" and a "Switch2" version of a command). Needed because it waits fixed times; a program that looks does not need it.
- **A camera check at first start**: the console's type and theme, the colours, and the delay (over a second: "some programs may not work properly").
- Its manuals ask for: TV size 100%, RGB range Full, both HDR settings off on a Switch 2, text speed Fast, and for the shiny sound the game's sound on Mono. Mono is the game's default; whether Stereo matters for the sound detector here has not been tried.

## The five other projects

| Project | What it is | What it shows | Here |
|---|---|---|---|
| AutoController_swsh | AutoController2's predecessor (no license) | The same way of building a program: a state, a button sequence, a wait for a picture with a time limit. | As above. |
| EasyCon (GPL) | A script language for a controller board, with image search: a label (a picture and where to look for it) gives a score from 0 to 100 | Scripts wait in loops such as "while the score is under 90: look again", which has no end when the picture never comes. Fixes of 2026: the program froze when a script printed very often, and when the capture card stopped answering while the program closed. | The AutoStory's steps are a table made from the game's own map data, and each step has its check and its limit. |
| Poke-Controller, Poke-Controller-Modified (MIT, Python) | Template matching in grey, on the whole picture or a region, with a score of 0.7 to pass | Its camera fix of March 2025: reading the camera now always gives the newest frame (a short line of frames could hand out older ones). In 2020: with two direction keys at once, the order in which they were sent could make the character stop. | The video pipeline hands out the newest frame only. |
| bdsp-automation (MIT, Python) | One script: a starter reset for Brilliant Diamond | Every stage has a time limit, and past it the game is closed and started again; a crash of the game is noticed by the HOME menu showing. Advice: take the console offline for long runs, an update prompt derails them. | During a run the AutoStory resets inside the game and does not pass the HOME menu, where such prompts come. Its waits have limits and its failed steps start over from the last save. |
| AutoTableTurf (GPL, Python) | Splatoon 3's card game: recognises the board, picks the move | Recognition and game flow are tested apart, on stored pictures. Its boxes are in pixels of one resolution and its colours tuned on one capture card; after a change of the game's screens "all detection box positions must be updated". One of its fixes: a loop without an end got 20 tries. | Boxes here are fractions of the picture. Stored pictures for the AutoStory's own readers: on the roadmap. |

## Pokémon Automation's own FRLG fixes

Pokémon Automation's plan for an FRLG AutoStory of its own ([#1827](https://github.com/PokemonAutomation/Arduino-Source/issues/1827)) has, in `main`, a shared list for choosing where a story starts and ends (#1828) and no story yet. Its FRLG fixes of October and November 2026 are all in the code this fork is built on:

- **#1684**: after an evolution the program pressed B every 2 seconds without waiting for the presses, so that a minute of presses was still to come when its 30 seconds were over. **#1770**: the same in a loop that spun in the grass until a time. Looked for in this fork's own programs: none of their loops goes by the clock while queueing presses. (Two of Pokémon Automation's egg routines still do, with limits of 10 and 45 minutes, where some seconds more do not matter.)
- **#1771**: the bag was not recognised once its list was scrolled, because a red arrow is drawn over the strip that was checked.
- **#1791**: an option for consoles that check the game's license online at its start (the game goes back to the HOME menu for some seconds). It is for the RNG programs, which close and start the game; the AutoStory does not.

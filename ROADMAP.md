# PocketPilot: roadmap

What the first builds have, what could make the FRLG AutoStory faster, what could be added and what could be modernized. Written on 12 November 2026; sections 6 and 7, the numbers of section 2 and the last three rows of section 3 are of 13 November.

## 1. What the first build has

- Its own name, icon, "About" window, window title and build number; the links to Pokémon Automation's Discord and its check for a new version are gone.
- A new interface: see [README.md](README.md).
- Pokémon Automation's `main` of 11 November 2026 merged in (149 commits since the fork was made): among others gamepad support, the FRLG Box Sorter and Living Dex sorter, a reworked audio pipeline and device names for the dropdowns. Gamepad support (`PA_SDL`) is on in the builds. Build 10 has its `main` of 13 November (four commits more).
- 20 bug fixes from a review of the whole code, of Pokémon Automation's open issues and of what goes wrong in other controller programs: [FIXES.md](SerialPrograms/Source/PocketPilot/FIXES.md).
- A faster build: Ninja and ccache as in Pokémon Automation's own workflow. A build takes about 4 minutes when little changed, 35 minutes from nothing (it was 35 to 40 minutes each time).
- A test of the interface in every build: the build starts on the build machine as on a computer that never ran it, goes through its pages, themes and lists and saves a picture of each. A build that fails is not published. It found its first bug in the first build: a crash at the first start without saved settings (fixed in build 2; build 1 should not be used).

Not tested yet: the AutoStory on this new base. The branch `frlg-autostory` (build 108) stays the one for the test run with Charmander until a PocketPilot build has played a few segments.

Builds 4 and 5 add the experimental programs and options of sections 6 and 7. Build 10 adds fix 20 and an experimental walk for the Nugget Bridge Farmer (section 6).

## 2. Can the AutoStory match a speedrunner?

Not today, and not by a small change. The numbers:

| | Time |
|---|---|
| World record, Any% Glitchless (pokeguy, with Squirtle) | 1 h 59 min 34 s |
| AutoStory, second playthrough with Charmander, new game to the Hall of Fame (measured from the logs of 8 to 13 November, 34 runs; the Champion fell on 13 November at 01:49) | 56.9 hours of play |
| of which training levels in the grass (11 segments) | 45.8 hours |
| of which the route itself: walking, talking, the trainers on the way, the gyms, shopping, retries after a failure | 11.2 hours |
| of which the Elite Four and the Champion, each beaten at the first try | 0.9 hours |

So a whole story takes 57 hours, some 29 times the record. Four fifths of that is training:

| Training for | Hours | Levels asked for |
|---|---|---|
| Brock (Routes 1 and 2) | 3.3 | Charmander 18 |
| The rival in Cerulean | 1.3 | 25 |
| Misty | 4.3 | 34 |
| Erika (with Electrode and Vaporeon) | 8.3 | Charizard 36, Electrode 25, Vaporeon 30 |
| Koga | 4.4 | 50, 33, 35 |
| Sabrina (with Lapras) | 1.7 | 47, Lapras 32 |
| Blaine | 7.0 | 52, the team 42 |
| Giovanni | 5.6 | 66, the team 47 |
| The League | 9.8 (11.3 in the first playthrough) | 74, the team 58 |

A speedrunner does not train at all. The published routes ([Squirtle](https://www.speedrun.com/pkmnfrlg/guide/rps95), [Charmander](https://www.speedrun.com/pkmnfrlg/guides/wardh)) take their levels from the trainers on the way and a handful of Rare Candies, and win the hard fights with items bought on the 5th floor of the Celadon Department Store (the Charmander route: 22 X Special, 5 X Defend, 1 to 3 X Speed, 2 X Accuracy, 3 Guard Spec). The Charmander route meets Erika at level 34, Giovanni in Silph Co. at 43 and leaves Fuchsia at 48. They also reset the run when a fight goes wrong; the program cannot count on luck, so it levels until the fight is safe.

What would bring the time down, in the order of what it gains for the work it takes:

1. **X items instead of levels.** Buy the route's X items in Celadon, use them at the start of the gym leaders', Giovanni's, the rival's and the League's fights, and ask for the route's levels instead of today's. Save before each of those fights (the program does); after a loss, try again, and only after a second loss train three levels. Estimate: 5 to 10 hours of training left instead of 46.
2. **Only the lead trains.** Electrode, Vaporeon and Lapras are there for the League's match-ups. With X items the lead can carry the League; the team then only needs to exist for Surf, Strength and Fly.
3. **Running by default.** The option "Running (experimental)" halves the walking time on the maps that allow it. It needs a run from start to end without getting lost before it can be the default.
4. **The Switch versions' fanfare skip.** Opening and closing the help menu with L at the right moment skips a fanfare (2.68 seconds each, up to a minute and a half in a run, found by iamClemi in March 2026). Small, but free once the timing is known.
5. **A leaner route.** Leave out the trainers and item detours the speedrun routes leave out, and replace screen checks by fixed timing where a mistake cannot happen.

With 1 to 3 the story should come under 20 hours; with 4 and 5 perhaps under 12. A separate speed mode that walks by the clock, like a tool-assisted run, could come near 3 or 4 hours, but every step of it would have to be tested on the console, and it would stop at the first dropped button press. The record itself is the best of thousands of human attempts with the fastest starter; a program that has to finish every time will stay behind it.

All of these change how the story plays, so each needs test runs. The first step, the X items, is the one to start with.

## 3. What could be added

From what the Switch versions and other tools offer:

| What | Why | Size |
|---|---|---|
| **Navel Rock and Birth Island** (Lugia, Ho-Oh, Deoxys) as chapters of the AutoStory | The Switch versions put the Mystic Ticket and the Aurora Ticket in the bag after the Hall of Fame; on the Game Boy Advance they were given out at events only. | Medium: two ferry destinations, two caves, the triangle puzzle |
| **A complete Pokédex** as a goal | Pokémon HOME 4.1.0 (October 2026) takes whole boxes from the Switch versions, one way, and gives a special Celebi in the mobile app for a complete FRLG Pokédex. The AutoStory's Pokédex collection already catches what one cartridge can. | Large: needs the next row too |
| **Trading between two Switches** (local wireless): trade evolutions (Alakazam, Machamp, Golem, Gengar), the other version's Pokémon, the other starters | The only way to the Pokémon one cartridge cannot get. Pokémon Automation can drive two consoles at once. | Large, and needs two consoles and two games |
| **Egg hatcher and breeder** (Four Island day care) | Pokémon Automation has these for Sword/Shield, Brilliant Diamond/Shining Pearl and Scarlet/Violet; for FRLG only an Egg RNG tool in development. | Medium |
| **The fanfare skip** in the existing farmers and shiny hunters | See above. | Small |
| **Counts per species and a video clip of every shiny** | pokebot-gen3 (for emulators) reports encounters, "phases" and milestones to Discord. Pokémon Automation has notifications and a stream history to build on. | Medium |
| **A status page for the phone** on the home network (what runs, the last screenshot, the stats) | pokebot-gen3 has an HTTP API; Pokémon Automation's latest code has the start of an HTTP server. | Medium |
| **A report of where the time went** at the end of a run | The numbers of section 2 came from reading logs by hand. | Small |
| **Nugget Bridge as the story's source of money** | The Rocket at the end of Nugget Bridge gives a Nugget (sold for 5,000) every time until he is beaten, and a lost battle costs 16 or 24 per level of the party's highest Pokémon at that point of the story. Pokémon Automation's farmer makes about 100 Nuggets in two hours; twenty are 100,000, and in the run of 13 November money cut the story's purchases short. Needs a party that loses (the others in the PC) and selling at a Mart. | Medium |
| **A second sign of a shiny**, next to its sound | The AutoStory and Pokémon Automation's FRLG hunts of wild Pokémon go by the sound alone, and say so when no sound comes in. AutoController2 also times the start of the battle, which a Pokémon with Intimidate or a slow picture sets off. A sign from the picture would be better: the sparkles, or the colours of the species against its two palettes. | Medium |
| **The AutoStory's own screen readers tested on stored pictures** in every build | AutoTableTurf tests recognition apart from the game's flow. The build's test already starts the program and saves pictures of its pages; four readers of the story go by fixed colour levels and are the first to fail on another capture card ([OTHERS.md](SerialPrograms/Source/PocketPilot/OTHERS.md)). | Small |

Left out on purpose: automating online battles (Pokémon Champions, released in April 2026, is built around them). That is unfair to the people on the other side.

pokebot-gen3 is under the GPL, which Pokémon Automation's license terms do not accept, so only its ideas can be used, not its code.

## 4. What could be modernized or replaced

Done in the first build: the interface and its themes, the build (Ninja, ccache, a test of the interface), the video frame conversion, readable colors, the bug fixes.

Candidates:

| What | Why |
|---|---|
| **The stream history's frames** | It still converts every frame with `QVideoFrame::toImage()` after mapping it; the memory test measured 13 KB of growth per picture for that path too. The converter of [VideoFrameConversion.h](SerialPrograms/Source/CommonFramework/VideoPipeline/Backends/VideoFrameConversion.h) can replace it. |
| **The Qt version** | The builds use Qt 6.10.2 like Pokémon Automation's own workflow, while its build guide still names 6.8.3 (later versions hang when threads end, which the program works around by exiting hard). Worth one build with 6.8.3 to see whether the memory growth of `toImage()` is there too. |
| **An update check of its own** | The one for Pokémon Automation's releases is off. PocketPilot could look at its own releases. |
| **The program's file names** | The program is still `SerialPrograms.exe` and its settings `SerialPrograms-Settings.json`. Renaming needs a step that takes over the old settings. |
| **The settings page** | One long page. Groups with a search box, as in the side bar, would make it easier to use. |
| **The log window** | A filter for errors and a search. |
| **`catch (...)` in the programs** | Several programs catch every exception in a retry loop, also the one that stops the program. Each should let that one through. |
| **Texts with a color of their own** | About 30 texts of the programs set their color in HTML (`<font color="red">`, `"orange"`, `"blue"`) instead of through `html_color_text()`, which picks a shade that can be read on the theme's background. Blue on a dark background and orange on a light one are hard to read, as they are in Pokémon Automation. |
| **Colored texts after a change of theme** | A program that is open while the theme changes keeps the shades of the old theme in its colored texts until it is opened again. |
| **Pokémon Automation's tests in this fork's workflow** | Its workflow runs the command-line tests after the build; this fork's workflow only takes the pictures. |
| **A Dutch interface** | Everything is in English. |

## 5. Keeping up with Pokémon Automation

The branch takes Pokémon Automation's changes by merging its `main`. PocketPilot's own code sits in one folder to keep those merges small; the files it changes elsewhere are the main window, the program list, the theme option, the global names and the handful of files of the fixes.

The fixes and the video frame conversion are separate commits, so that they can be offered to Pokémon Automation one at a time (its rules: new contributors at most two open pull requests, opened and answered by a person).

## 6. What people ask for, and what builds 4 and 5 have of it (experimental)

Where the requests come from: Pokémon Automation's request tracker ([ComputerControl issues](https://github.com/PokemonAutomation/ComputerControl/issues), 53 open on 12 November 2026), its own plan for an FRLG AutoStory ([Arduino-Source #1827](https://github.com/PokemonAutomation/Arduino-Source/issues/1827)), and what [pokebot-gen3](https://github.com/hanzi/pokebot-gen3/blob/main/wiki/Readme.md) (for emulators) offers that Pokémon Automation's FRLG list does not.

Both of Pokémon Automation's lists were read to the end on 13 November 2026, three pages each: the 53 open entries of the request tracker and the 59 open [issues of Arduino-Source](https://github.com/PokemonAutomation/Arduino-Source/issues), which are mostly the developers' own to-do list. Ten entries carry the label "bug" and three more report a fault; none of them is about FireRed and LeafGreen. Two are already solved in the code this fork is built on: the egg programs of Scarlet and Violet that asked for the IV judge when no filter needed it ([#232](https://github.com/PokemonAutomation/ComputerControl/issues/232)), and the row of the Living Dex sorter ([#1875](https://github.com/PokemonAutomation/Arduino-Source/issues/1875)). The others need a game, a console or a capture card this fork cannot be tested with (Sword and Shield, Scarlet and Violet, a Switch 2, an AVerMedia GC573), or lie in Qt ([#570](https://github.com/PokemonAutomation/Arduino-Source/issues/570), [#189](https://github.com/PokemonAutomation/ComputerControl/issues/189), [#125](https://github.com/PokemonAutomation/ComputerControl/issues/125)). Every issue of Arduino-Source was opened: what each one asks, and where this fork stands on it, is in [ISSUES.md](SerialPrograms/Source/PocketPilot/ISSUES.md). Two fixes came out of it ([FIXES.md](SerialPrograms/Source/PocketPilot/FIXES.md), 18 and 19): a wrong test in the move scores of the Dynamax Adventures, found next to [#1072](https://github.com/PokemonAutomation/Arduino-Source/issues/1072), and the audio input that settings of an older build lost, found by following [#1032](https://github.com/PokemonAutomation/Arduino-Source/issues/1032).

Also read on 13 November: the help forum of AutoController (another controller program; 21 posts, each to its end), AutoController2 itself and five more projects of that kind: [OTHERS.md](SerialPrograms/Source/PocketPilot/OTHERS.md). None of it is about this fork, and nothing was taken over as code (three of them are under the GPL). What goes wrong there with the picture, one that falls behind while the program runs and a capture card that drops frames, was measured in the logs of this fork's test runs and is not there: over 162 hours of video the delay does not grow, and since 29 October every minute had 59 frames a second or more. One fault of theirs could happen here too, in Pokémon Automation's Nugget Bridge Farmer: fix 20, in build 10, and with it an experimental option, "Walk along the walls", worked out on the game's own map and not run on a console.

| Asked for | Where | In PocketPilot |
|---|---|---|
| An FRLG AutoStory | Request [#259](https://github.com/PokemonAutomation/ComputerControl/issues/259) of March 2026; Pokémon Automation started its own in November ([#1827](https://github.com/PokemonAutomation/Arduino-Source/issues/1827): the start of the game is coded, from the first gym on it is not) | There since the first build. It finished a whole game on 13 November 2026: from a new game to the Hall of Fame in 57 hours of play, over 34 runs. |
| The program's main messages in the log on the video, not only in the log window | Request [#264](https://github.com/PokemonAutomation/ComputerControl/issues/264) | **Build 4, experimental:** the option "Progress on the video" of the AutoStory and of the Level Trainer: the step, each battle's result and HP, the level while training, heals, saves, failed steps. |
| A loop counter for the Turbo Macro | Request [#249](https://github.com/PokemonAutomation/ComputerControl/issues/249) | **Build 4, experimental:** the Turbo Macro counts its loops under the video. |
| Training a Pokémon to a level | pokebot-gen3's "Level Grind" mode. Pokémon Automation has an EV Trainer and an Elite Four Farmer for FRLG, nothing with a level to reach. | **Build 4, experimental:** the program "Level Trainer". It trains the first Pokémon of the party with the AutoStory's battles: where the player stands (no nurse: it stops when low on HP or out of PP), or at one of the nine training places of the story, from the nurse to the grass and back to heal. |
| Faster battles | This fork's own measurement: of the 31 seconds of a training battle, 8 are the wait after its last text (6 seconds for nothing of a battle to show, then a look at the start menu); the game needs about 2. | **Build 4, experimental:** the option "Faster battles" of the AutoStory and the Level Trainer. The game fades to black at the end of every battle; the program looks at the start menu when the picture is back. Off by default. |
| Entering ACE codes (box names) | Request [#260](https://github.com/PokemonAutomation/ComputerControl/issues/260): "a program that renames boxes to ACE codes the user enters into it" | **Build 5, experimental:** the program "Box Namer". It types a list of names into the PC's boxes and reads every character back from the screen. See below. |
| Day care and eggs, Sweet Scent, Rock Smash, the Safari Zone as a hunting ground, puzzles | pokebot-gen3 modes | Not built. See section 3 for the day care. |

"Experimental" is a new standing of a program, next to Pokémon Automation's "beta" and "3rd party": the program has the tag in the list and a note under its title. It means: compiled and checked by the build's interface test, but never run on a console. The experimental options of the AutoStory are off unless turned on, so the AutoStory plays as it did.

Also new in build 4, for section 7: the program "Input Timing Test" (experimental). It opens the start menu a few hundred times and reports how long the game takes to show its reaction to a press, how much that varies, and whether a press can be aimed at a frame.

### The Box Namer (build 5)

An ACE code for these games is a list of box names: the game is made to run them as instructions, so one wrong character is a wrong code. The program takes the list the way the ACE notes publish it (`Box  6: _ F o H I o o r [ FoHIoor]`) or as plain names, and does for each box what a person does: NAME in the box's menu, the name on the naming screen, OK, on to the next box.

What it does to be sure of every character:

- After every character it reads the name back from the screen and compares it with the game's own font, pixel by pixel: the characters typed so far, nothing after them, and the place the next one goes to (which is what tells a space). A character only counts when it is exactly its shape: `c` and `o` differ in one pixel, `.` and `,` in two.
- A space in the eighth place looks the same as an empty eighth place. It is typed over a character that can be seen, which is read first.
- Nothing is pressed blind where a missed or a doubled press would do harm: the arrow of the box's menu is seen on NAME before A, OK is pressed only on a name that reads as written, and the box is seen to change after RIGHT.
- After OK it looks at the box's title, which must show the name. The Switch versions pass a name through the console's list of words that are not allowed when OK is pressed, and do not take one that is on it: the box keeps the name it had, and nothing on the naming screen says so. That is in the game's source ([pret/pokefirered](https://github.com/pret/pokefirered), `MainState_PressedOKButton` in the revision for the Switch), and the [ACE notes](https://pomeg-letterbombers.github.io/pokemon-ace-notes/frlg-faq/) list codes that cannot be entered on the Switch versions for that reason.

What was tested, all of it off the console:

| What | On what | Result |
|---|---|---|
| Reading the name being typed | A capture of this fork's test Switch (the rival's naming screen with AAAAAAA) | Read right, with the place of the next character. |
| | The same capture scaled down to 1280 x 720, the size of this capture card's video, in five ways | Read right each time. |
| | 3,008 pictures drawn from the game's font: every character of the keyboard in every place, names of 0 to 8 characters, scaled in four ways, blurred, with noise, as JPEG, with the game box off by up to 7 pixels or 1 in 100 in size, at 1080p and 720p | 3,008 read right, none wrong, none unread. |
| Which keys to press | 20,030 names typed on a model of the naming screen written from the game's source (the pages, the cursor and its way around the buttons, the full name), from a screen just opened and from one left anywhere | All came out as written; 23 presses a name on average, 63 at most. |
| The box's title | Pokémon Automation's 29 test pictures of the box: three wallpapers, three video formats, the titles BOX1, BOX2, BOX3, BOX6, BOX9 and BOX14; at 1080p and scaled down to 720p | In 28, the title shows its own name and none of the other names tried (also not BOX8 for BOX3, or BOX13 for BOX14). In the 29th a Pokémon is held in front of the title, and no name fits. |
| The list | The lines of the ACE notes with and without box numbers, brackets and underscores | Read as meant; 15 kinds of mistakes are refused with the line and the reason. |

The tests of the list, the keys to press and the box's title are also among the program's own unit tests (`PokemonFRLG::NamingScreenText`, 31 of them, the typing with 3,027 names); they were run outside the program for this build, with stand-ins for its picture classes.

Not tested: anything on a console. What goes on between the screens (the box's menu, the wait for the naming screen and for the box to come back, the look at the box changing) has only been compiled. The hand on the box's name is found with Pokémon Automation's detector of its Box Sorter, which is beta there. The first run should be watched, with names that do no harm; the default names (BOX1 to BOX14) can be typed back the same way.

What the program does not do: it knows nothing of the code. Which names a code needs on the Switch versions, and whether the console's word list takes them, is for the ACE notes; the program stops and says so when a box does not show its name.

## 7. Can a program of our own beat the world records?

The records:

| | Time | How |
|---|---|---|
| Best run by a person, Any% Glitchless | 1 h 59 min 34 s | pokeguy, on a Game Boy Player with Game Boy Interface. The leaderboard lists GBA, Game Boy Player, GBI and DS; the Switch versions are not on it. |
| Tool-assisted run | 1 h 44 min 17 s | mkdasher, 2013, on the emulator VBA-rr: 373,735 frames of input, with heavy manipulation of the game's random numbers (Squirtle, later a Clefable with Metronome). |

**An official record: no.** speedrun.com's rules say that "the game itself must not be altered or controlled with outside tools" and that turbo controllers "or such with non-native functions" are not allowed. A program pressing the buttons is exactly that. TASVideos publishes input files made on emulators; a run on a console is not one of those.

**The tool-assisted time: no.** That run is perfect to the frame for an hour and three quarters, made with save states and a view into the game's memory. A program that looks at a capture card cannot get there.

**Faster than the best person, unofficially: possible in principle, far away in practice.** What speaks for it:

- A long tool-assisted run has been played back on real hardware: a Pokémon Crystal run of more than 600,000 frames on a GameCube with Game Boy Interface (it lost its place once, after 32 minutes, through the drift of the game's clock, and that was fixed).
- On a console that asks a USB controller for its buttons, as the Switch does, timed playback worked in short stretches when the playback counted the console's video frames (a Wii U and an RP2040 board); over the air it was off by up to two frames.
- Pokémon Automation's RNG programs hit one chosen moment on the retail Switch versions after calibrating: the seed is set by when a button is pressed on the title screen, and its guide's example is 8 advances late and one seed off at the first try, on target at the next.
- A program does not get tired. Where people play thousands of attempts for one lucky run, it can play twelve two-hour attempts a day and keep the best, and it can do what the published route says it leaves out ("not optimized for RNG manipulation").

What stands in the way:

- The gap between a perfect run and the best person is only 15 minutes in two hours. The AutoStory is not near that kind of play: it looks at the screen after nearly everything it does (a walk of four tiles in the grass takes 3.3 seconds, of which one second is walking), it trains for 46 hours, and it takes Charmander.
- It would be another program, not a faster AutoStory: the speedrun route (Squirtle, no training, X items, Rare Candies, Repels, the bicycle), played by the clock in stretches of timed presses, looking at the video only where the game gives a clear sign to find its place again.
- The Switch versions run the game in Nintendo's own emulator: its timing has to be measured, nothing of the Game Boy Advance runs can be assumed.
- Every stretch has to be tested on the console, and a run stops at the first press that falls a frame late.

The order to go about it, each step useful by itself:

1. **Measure** (build 4): the Input Timing Test tells how much the delay between a press and its effect varies on this controller board, console and capture card, and whether a press can be aimed at a frame. Without that, nothing by the clock can be planned.
2. **Take the waits out of the AutoStory** where the game gives a sign: "Faster battles" (build 4), "Shorter looks in the grass" (build 5, see below), running.
3. **X items instead of levels** (section 2): from 57 hours to under 20.
4. **A speedrun mode** for one stretch first, for instance from the bedroom to the first rival battle: timed presses, the video as the check. Compare with the same stretch in a record run's video.
5. **The whole route** stretch by stretch, then the random numbers (the starter's stats from the title screen, as Pokémon Automation's Starter RNG does), then many attempts.

A fair guess: steps 1 to 3 are weeks, and bring a story that plays in less than a day. Steps 4 and 5 are months of test runs, with a first whole run somewhere around 3 to 4 hours. Coming under 1 h 59 min 34 s would take much more than that again, and may not be reachable through a capture card at all; the Input Timing Test is the first thing that can tell.

One thing would be new either way: no program-played run of FireRed or LeafGreen on a console turned up in this search. The first one that finishes at speedrun pace has no time to beat.

### What the logs of the test runs already tell (13 November)

The Input Timing Test has not run yet. Three things can be read from the logs of the AutoStory itself.

**Where the time of a run goes.** The run of 12 to 13 November (9.4 hours: six hours of training for the League, the Elite Four and the Champion, then 26 tries at Zapdos) counted its own time by activity:

| Activity | Minutes | Share |
|---|---|---|
| Battles (415), and reading text, moves and species names | 309 | 55% |
| Walking the route | 92 | 16% |
| Walks along the grass while training (1,614) | 85 | 15% |
| Menus, healing, saving, resets, flying, single steps | 42 | 7% |
| The rest | 36 | 6% |

**The look after a walk in the grass.** After every walk along the grass the program watched 1.5 seconds for a battle to start. Of the 1,614 walks, 1,266 ended without one and waited the whole time: 32 minutes of the run. Of the 348 battles, the video saw 343 start during the walk itself; that look found the other 5, 1.3 seconds after the walk. The game decides on a wild battle in the frame a step ends, and what follows changes the whole picture every frame until the screen is black (two fades to gray and back, then the transition). So a screen that stays the same for 0.3 seconds has no battle starting. **Build 5, experimental:** the option "Shorter looks in the grass" of the AutoStory and the Level Trainer ends the look then. About one second less for each walk without a battle: 23 minutes of that run. With "Faster battles" (about 5 seconds for each battle that is not a catch, some 30 minutes of that run) the two would have taken a little under an hour off the 9.4, if they work as meant; neither has run on a console.

**Walking by the clock.** A walk is one held press, timed to the tile (16 frames a tile), and the video counts the tiles after it. In the logs of 8 to 13 November the video could count 15,009 of them:

| | Walks | Exactly the tiles asked for | Short | A tile too far |
|---|---|---|---|---|
| Along the grass (up to 4 tiles) | 7,330 | 7,329 | 1 | 0 |
| On the route (1 to 41 tiles) | 7,679 | 7,464 | 215 | 0 |

The short ones are not the clock. 100 of the 215 are walks over rock stairs: the steps in front of the Indigo Plateau and the two ends of Nugget Bridge. On those tiles the game itself walks slower, 23 frames a tile instead of 16 (`PlayerIsMovingOnRockStairs` in its source). A walk over four of them came out two tiles short 83 times of 85 and one tile short the other two times; a walk over two of them one tile short, 14 times of 14. One stair tile fits in the spare time of a press (19 walks of 20). The program sees the missing tiles on the video and walks them. A clock that counts the stairs would not have to, and the script's generator knows where they are (it does not let the player run there). Another 22 are one walk in the Power Plant, two tiles short all 22 times: the Max Repel wears off on it, and the game shows "REPEL's effect wore off…" and takes the text away again by itself because a direction is held. And 79 are walks that did not start at all, 58 of them single steps and most of those at the same four places; those have not been looked into. Along the grass a timed walk came out right 7,329 times of 7,330, and no walk went a tile too far.

Between two walks the program takes 0.8 seconds (the median of 6,973 walks that were followed straight by another): it lets the last step end, counts the tiles on the video and waits for a still picture. The game needs up to a quarter of a second of that. In these logs the pauses add up to 87 minutes, next to 163 minutes of held presses.

So for step 4, walking by the clock is not what stands in the way on this controller board and console: the clock has to count the rock stairs and the steps of a Repel, and it can chain the presses where the program now stops to look. What a speed mode has to solve is what is not walking: more than half of a run is battles, and the texts, menus and fades between them have no fixed length yet. That is what the Input Timing Test and "Faster battles" are for.

## Sources

- [speedrun.com: Pokémon FireRed/LeafGreen leaderboard](https://www.speedrun.com/pkmnfrlg)
- [speedrun.com: site rules](https://www.speedrun.com/support/learn/site-rules)
- [TASVideos: GBA Pokémon FireRed by mkdasher in 1:44:17.34](https://tasvideos.org/2520M) and [its submission](https://tasvideos.org/4143S)
- [RetroRGB: a Pokémon Crystal tool-assisted run verified on a console with Game Boy Interface](https://retrorgb.com/pokemon-tas-gbi.html)
- [Doug Brown: tool-assisted playback on a Wii U with an RP2040, wired and wireless](https://www.downtowndougbrown.com/2024/10/are-wireless-gamepads-terrible-mario-maker-tas-playback-with-an-rp2040)
- [Pokémon Automation: FRLG RNG manipulation guide](https://pokemonautomation.github.io/Programs/PokemonFRLG/RngManipulationGuide.html)
- [Pokémon Automation: request tracker](https://github.com/PokemonAutomation/ComputerControl/issues) and [the plan for its own FRLG AutoStory](https://github.com/PokemonAutomation/Arduino-Source/issues/1827)
- [Pokémon Automation: the scrolled-bag fix this fork's test run needed](https://github.com/PokemonAutomation/Arduino-Source/pull/1771)
- [speedrun.com: Any% (Glitchless), JP_Xinnam](https://www.speedrun.com/pkmnfrlg/guide/rps95) and [Any% (Glitchless) with Charmander, Exarion](https://www.speedrun.com/pkmnfrlg/guides/wardh)
- [GamesRadar, 20 March 2026: the fanfare skip in the Switch versions](https://www.gamesradar.com/games/pokemon/the-biggest-time-save-in-nearly-a-decade-of-pokemon-speedrunning-has-been-discovered-thanks-to-the-new-firered-and-leafgreen-switch-ports-and-all-it-takes-is-the-press-of-a-button/)
- [Nintendo Life: Pokémon FireRed and LeafGreen on the Switch eShop](https://www.nintendolife.com/games/switch-eshop/pokemon_firered_and_leafgreen)
- [Video Games Chronicle, 28 February 2026: the event tickets in the Switch versions](https://www.videogameschronicle.com/news/pokemon-firered-and-leafgreen-on-nintendo-switch-lets-players-claim-previously-event-only-items)
- [Bulbagarden: Pokémon HOME 4.1.0 and FireRed/LeafGreen](https://bulbagarden.net/threads/pokemon-home-version-4-1-0-to-receive-october-update-adding-firered-and-leafgreen-connectivity-and-transfers-of-pokemon-in-go-safari-balls.311303/)
- [Pokémon Automation: the list of programs](https://pokemonautomation.github.io/Programs/index.html)
- [pokebot-gen3](https://github.com/40Cakes/pokebot-gen3)
- [pret/pokefirered](https://github.com/pret/pokefirered): the game's source as the pret project took it apart, with the revision of the Switch versions (the naming screen, the PC's boxes, the battle transition, the rock stairs)
- [Pokémon ACE notes: FRLG FAQ](https://pomeg-letterbombers.github.io/pokemon-ace-notes/frlg-faq/) and [a route with its box names](https://pomeg-letterbombers.github.io/pokemon-ace-notes/frlg-non-jpn-pre-e4-route)
- [Nintendo Life, 28 February 2026: the Switch versions replace names that are not allowed](https://www.nintendolife.com/news/2026/02/pokemon-firered-and-leafgreen-switch-update-censors-naughty-and-offensive-names)
- [Pokémon Automation: its test pictures](https://github.com/PokemonAutomation/CommandLineTests)

# PocketPilot: roadmap

What the first builds have, what could make the FRLG AutoStory faster, what could be added and what could be modernized. Written on 12 November 2026.

## 1. What the first build has

- Its own name, icon, "About" window, window title and build number; the links to Pokémon Automation's Discord and its check for a new version are gone.
- A new interface: see [README.md](README.md).
- Pokémon Automation's `main` of 11 November 2026 merged in (149 commits since the fork was made): among others gamepad support, the FRLG Box Sorter and Living Dex sorter, a reworked audio pipeline and device names for the dropdowns. Gamepad support (`PA_SDL`) is on in the builds.
- 17 bug fixes from a review of the whole code: [FIXES.md](SerialPrograms/Source/PocketPilot/FIXES.md).
- A faster build: Ninja and ccache as in Pokémon Automation's own workflow. A build takes about 4 minutes when little changed, 35 minutes from nothing (it was 35 to 40 minutes each time).
- A test of the interface in every build: the build starts on the build machine as on a computer that never ran it, goes through its pages, themes and lists and saves a picture of each. A build that fails is not published. It found its first bug in the first build: a crash at the first start without saved settings (fixed in build 2; build 1 should not be used).

Not tested yet: the AutoStory on this new base. The branch `frlg-autostory` (build 107) stays the one for the test run with Charmander until a PocketPilot build has played a few segments.

## 2. Can the AutoStory match a speedrunner?

Not today, and not by a small change. The numbers:

| | Time |
|---|---|
| World record, Any% Glitchless (pokeguy, with Squirtle) | 1 h 59 min 34 s |
| AutoStory, second playthrough with Charmander, new game to Victory Road (measured from the logs of 8 to 12 November, 33 runs) | 50 hours of play, the League training not finished |
| of which training levels in the grass (11 segments) | 39.8 hours |
| of which the route itself: walking, talking, the trainers on the way, the gyms, shopping, retries after a failure | 10.2 hours |
| The League training in the first playthrough | 11.3 hours |

So a whole story takes about 60 hours, some 30 times the record. Three quarters of that is training:

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
| The League | 3.8 so far (11.3 in the first playthrough) | 74, the team 58 |

A speedrunner does not train at all. The published routes ([Squirtle](https://www.speedrun.com/pkmnfrlg/guide/rps95), [Charmander](https://www.speedrun.com/pkmnfrlg/guides/wardh)) take their levels from the trainers on the way and a handful of Rare Candies, and win the hard fights with items bought on the 5th floor of the Celadon Department Store (the Charmander route: 22 X Special, 5 X Defend, 1 to 3 X Speed, 2 X Accuracy, 3 Guard Spec). The Charmander route meets Erika at level 34, Giovanni in Silph Co. at 43 and leaves Fuchsia at 48. They also reset the run when a fight goes wrong; the program cannot count on luck, so it levels until the fight is safe.

What would bring the time down, in the order of what it gains for the work it takes:

1. **X items instead of levels.** Buy the route's X items in Celadon, use them at the start of the gym leaders', Giovanni's, the rival's and the League's fights, and ask for the route's levels instead of today's. Save before each of those fights (the program does); after a loss, try again, and only after a second loss train three levels. Estimate: 5 to 10 hours of training left instead of about 47.
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

## Sources

- [speedrun.com: Pokémon FireRed/LeafGreen leaderboard](https://www.speedrun.com/pkmnfrlg)
- [speedrun.com: Any% (Glitchless), JP_Xinnam](https://www.speedrun.com/pkmnfrlg/guide/rps95) and [Any% (Glitchless) with Charmander, Exarion](https://www.speedrun.com/pkmnfrlg/guides/wardh)
- [GamesRadar, 20 March 2026: the fanfare skip in the Switch versions](https://www.gamesradar.com/games/pokemon/the-biggest-time-save-in-nearly-a-decade-of-pokemon-speedrunning-has-been-discovered-thanks-to-the-new-firered-and-leafgreen-switch-ports-and-all-it-takes-is-the-press-of-a-button/)
- [Nintendo Life: Pokémon FireRed and LeafGreen on the Switch eShop](https://www.nintendolife.com/games/switch-eshop/pokemon_firered_and_leafgreen)
- [Video Games Chronicle, 28 February 2026: the event tickets in the Switch versions](https://www.videogameschronicle.com/news/pokemon-firered-and-leafgreen-on-nintendo-switch-lets-players-claim-previously-event-only-items)
- [Bulbagarden: Pokémon HOME 4.1.0 and FireRed/LeafGreen](https://bulbagarden.net/threads/pokemon-home-version-4-1-0-to-receive-october-update-adding-firered-and-leafgreen-connectivity-and-transfers-of-pokemon-in-go-safari-balls.311303/)
- [Pokémon Automation: the list of programs](https://pokemonautomation.github.io/Programs/index.html)
- [pokebot-gen3](https://github.com/40Cakes/pokebot-gen3)

# Claude GG: what is different from Pokémon Automation, and why

Claude GG is a fork of [Pokémon Automation's Arduino-Source](https://github.com/PokemonAutomation/Arduino-Source) (the program "SerialPrograms"). This file lists everything that differs from the original, with the reason for each change, and says what was wrong in the original where a change fixes a fault. The programs for the games that are not mentioned here are Pokémon Automation's, as they are.

Written on 26 November 2026, at build 101.

## 1. The base, and how to see the exact difference

The code is Pokémon Automation's `main` as it was on 21 November 2026: commit `743ac0d225d1d7a756d4b861e901c0fc4217eaee` ("run distortion augmentation on test UI (#1949)"). Everything up to that commit is the history of Pokémon Automation, with its authors; everything after it is this fork's. Since then, Pokémon Automation has moved on; this fork takes its changes by merging its `main` (see section 11).

In numbers, at build 101: 326 commits after that base; 387 files changed (268 new, 119 changed); 143,570 lines added and 564 removed. Most of it is new files: the interface, Claude, and the FRLG AutoStory with its data. Of the 119 files that exist in the original, only about 70 hold code that changed in some way other than a name or a list of programs; they are listed in section 6 (fixes) and section 9 (what had to change for the new interface).

To see it yourself:

```
git remote add upstream https://github.com/PokemonAutomation/Arduino-Source.git
git fetch upstream
git diff --stat 743ac0d225d1d7a756d4b861e901c0fc4217eaee HEAD       # what this fork changed
git diff 743ac0d225d1d7a756d4b861e901c0fc4217eaee HEAD -- Common SerialPrograms/Source/CommonFramework   # only the shared code
git log --oneline 743ac0d225d1d7a756d4b861e901c0fc4217eaee..HEAD    # the commits of this fork
```

The history has to stay in the repository: it is what credits the authors of Pokémon Automation, whose license (MIT, Copyright (c) 2021 Alexander J. Yee) asks for its notice to stay (see [GITHUB-SETUP.md](GITHUB-SETUP.md), section 1).

## 2. Why a fork at all

Pokémon Automation is a large program for people who know it: a window with a program list, a side bar and many settings. This fork is for one person's use and has three goals that the original does not:

1. A **simpler way in**: a setup that asks one thing at a time (language, console, controller board, capture card, game) and then shows the games and their programs, with a look of its own.
2. **Claude** built in: a panel to ask about the programs, and a way to have a program built or fixed that ends in a pull request.
3. A program that **plays the whole story of FireRed and LeafGreen** by itself, which the original does not have.

The cost of a fork is that it has to keep up with the original. The design follows from that: Claude GG's own code is in one folder (`SerialPrograms/Source/PocketPilot`), and the files of the original that were changed are few (section 9), so that merging the original's `main` stays small.

## 3. What is new

| What | Where | Why |
|---|---|---|
| **Name and identity**: Claude GG; `ClaudeGG.exe`, `ClaudeGG.log`, `ClaudeGG-Settings.json`; a logo of a Switch with a chat bubble "GG"; an About window; the number of the build in the title | `Globals.cpp`, `IconResource/`, `PocketPilot_About.*`, `PocketPilot_Logo.*`, `PocketPilot_OldSettings.*` | A fork that is not Pokémon Automation's must not look like it: its Discord application, its update check and its name are Pokémon Automation's. Settings of an older build (named `SerialPrograms-Settings.json`) are copied to the new name at the first start; nothing is overwritten or deleted. |
| **A new interface**: the window is the screens of the setup (language, console, controller, capture card, game, program); the games as cartridges; the programs of a game with live picture, checks, output log, timeline and options; Night and Day themes in the color of a starter; search over games, programs and settings | `PocketPilot_Setup.*`, `PocketPilot_Home.*`, `PocketPilot_Theme.*`, `PocketPilot_ProgramList.*`, `PocketPilot_SettingsSearch.*`, `Resources/` | The original's window needs knowledge of its parts before anything works. The new one leads from nothing to a running program. The old window (top bar, side bar, status strip) is gone; its programs and options are the same. The two looks of the original stay as "Classic". |
| **Intro and tutorial films**: a short film when the program starts (two Joy-Cons click into a handheld, "GG" lights up); one film each for the ESP32, ESP32-S3 and Pico boards in the setup; the program's texts in Dutch, German, French and Spanish | `PocketPilot_Intro.*`, `PocketPilot_Language.*`, `Resources/lang/`, `Resources/*.mp4` | The first start of a new user is the part where most people give up. The films show what the words do not; every film can be skipped and the intro can be turned off (`--no-intro`). |
| **Several installations at the same time** (build 101): each a process of its own with its own settings, its own capture card and microcontroller; an overview with a miniature of each; a button back to the overview; "New installation" starts at the setup | `PocketPilot_Instances.*`, `PocketPilot_InstancePaths.*`, changes in `Main.cpp`, `GlobalAutoPaths.cpp`, `VideoSession.*` | The original runs one program on one console, and several consoles only inside one process (the "multi-console" programs). Two consoles with their own capture cards need two settings, two logs and two windows. A process per installation keeps a crash or a stop of one from touching the others. |
| **Claude in the program**: a panel to ask about the programs; it builds or fixes a program on a branch `claude/...`, which becomes a pull request. "Claude Plays" lets Claude play a game by looking at the screen | `PocketPilot/Assistant/`, `PocketPilot/ClaudePlays/` | To make changing a program possible without a development environment. The rules it works under (never to the branch the builds are made from, nothing without a click, no changes to workflows or third-party code) are in [ASSISTANT.md](SerialPrograms/Source/PocketPilot/ASSISTANT.md). Experimental: it has not run against the real API from a build machine, which has no key. |
| **FRLG AutoStory**: plays FireRed or LeafGreen from a new game to the Hall of Fame, and on (legendaries, Sevii Islands, National Pokédex, Mewtwo, a Pokédex collection); a summary of where its time went at the end of a run | `VideoGames/PokemonFRLG/Programs/AutoStory/` (38 files), `SerialPrograms/Scripts/PokemonFRLG/AutoStory/` (the tools that made its data) | The original has FRLG programs for shiny hunting and farming but none that plays the story. See [ROADMAP.md](ROADMAP.md), section 2, for how long a run takes (about 57 hours) and why. |
| **More FRLG programs**, tagged "experimental" because they have not run on a console: Level Trainer, Box Namer, Input Timing Test; options "Faster battles", "Shorter looks in the grass", "Walk along the walls" of the Nugget Bridge Farmer | `VideoGames/PokemonFRLG/Programs/` | Needed by the AutoStory (training, naming) or asked for. "Experimental" is on them so that nobody takes them for tested. |
| **The FRLG RNG target search and plan tracker**, imported from [dayfine's fork](https://github.com/dayfine/Arduino-Source) (pull request #3 of this repository) | `VideoGames/PokemonFRLG/Programs/RngManipulation/` | Work of another contributor that is not in the original (yet). It is a library that is not wired into a program here. Its authorship is kept in the history. |
| **Shiny clips**: a hunt for a shiny in FRLG saves a clip of the last seconds of the video when it finds one, and sends it with the notification | `PocketPilot_ShinyClip.*` | A screenshot does not show how a shiny appeared; the clip does. Only a file under 8 MB is sent. |
| **Search in the Output window, and a filter for warnings and errors** | `CommonFramework/Logging/LoggerWindow.*` | The log of a program that runs for hours cannot be read by eye. |
| **Check for updates**: a button in the About window that looks at this repository's releases | `PocketPilot_Update.*` | The original's check looks at Pokémon Automation's releases and would offer their program in place of this one. This one runs only when the button is pressed. |
| **Tests in every build**: the interface, the intro film, the tutorial films, every language, the setup and the installations are run on the build machine, and a build that fails is not published | `PocketPilot_SelfTest.*`, `.github/workflows/pocketpilot-windows.yml` | The first build had a crash at the first start without saved settings. A build that is found to start only after someone downloaded it costs more than a test on the build machine. |

## 4. What was left out, and why

| What | Why |
|---|---|
| The original's check for a new version | It points at Pokémon Automation's releases. Replaced by the button in the About window. |
| Discord Rich Presence by default | It shows the program as Pokémon Automation, whose Discord application it is. |
| The Discord bot and the Discord Social SDK in the Windows build | Nothing here uses them, and D++ brings OpenSSL 1.1.1k, which is no longer maintained. The Discord webhooks (the notifications) stay. |
| The wallpaper (on until now) | Off until it is turned on: the new interface has its own background. |
| The links to Pokémon Automation's Discord and its error reporting | This is not their release, and their team cannot help with this fork. Error reports are saved on the computer (`ErrorReportsLocal`), not sent. |

## 5. A change to how pictures are made (memory)

Pictures of the screen are made with the program's own converter ([`VideoFrameConversion.h`](SerialPrograms/Source/CommonFramework/VideoPipeline/Backends/VideoFrameConversion.h)) instead of Qt's `QVideoFrame::toImage()` in the stream history. **What was wrong:** on some computers `toImage()` made the program's memory grow by 0.3 to 1.6 GB an hour while a program ran (13 KB for each frame converted). **Why this way:** the snapshots already used a converter of their own; the stream history now uses the same one, and it keeps Qt's conversion where its own has not been matched. The build's test sends a made-up NV12 frame through both.

## 6. Faults in the original that were fixed

A review of the whole code on 12 November 2026 (cppcheck, clang-tidy and reading the core paths), Pokémon Automation's open issues ([ISSUES.md](SerialPrograms/Source/PocketPilot/ISSUES.md)) and the help forum of another controller program ([OTHERS.md](SerialPrograms/Source/PocketPilot/OTHERS.md)) found 20 faults, and a 21st turned up on 26 November. Each was still in Pokémon Automation's code when it was fixed here. The full text of each, with the reasoning, is in [FIXES.md](SerialPrograms/Source/PocketPilot/FIXES.md); they are separate commits so that they can be offered to Pokémon Automation one at a time.

**They were checked by compiling them (GCC and Clang, with the warnings of the real build). None of them has run on a console yet:** they change paths that only show when something goes wrong.

| # | Where | What was wrong | The fix |
|---|---|---|---|
| 1 | `Recording/StreamHistoryTracker_SaveFrames.cpp` | Saving the stream history threw when the video file could not be opened. It is saved while an error report is made, so the exception stopped a program that was only reporting an error it could have recovered from. Frames that could not be converted were stored empty and written later from a null picture. | The failure is reported and the program goes on; a frame that cannot be converted is left out. |
| 2 | `CommonTools/Async/InferenceSession.cpp` | A missing `break`: an external callback fell through into the code for a visual one (a wrong cast, a lookup that finds nothing). | `break`. |
| 3 | `Controllers/ControllerStateTable.cpp` | A saved Turbo Macro row in the old format lost its release time: the test looked at one key only. | The release is kept when either key is there. |
| 4 | `CommonTools/Audio/AudioPerSpectrumDetectorBase.cpp` | A history of scores that nothing reads grew by about 47 entries a second while no sound came close to the one listened for: about 15 MB an hour with a silent or missing audio input. | At most 1,000 entries are kept. |
| 5 | `Common/Cpp/ListenerSet.h` | `run_method()` moved an rvalue argument into the first listener, and the others got what was left. | The arguments are passed as they are, once for each listener. |
| 6 | `Common/Cpp/Logging/LastLogTracker.cpp` | `get_recent(lines)` returned the oldest lines, not the latest. | The last `lines` entries. |
| 7 | `Common/Cpp/ExpressionEvaluator.cpp` | The loop over the parsed expression shortened the vector it was going through. | The `pop_back()` is gone. |
| 8 | `VideoPipeline/UI/VideoSourceSelectorWidget.cpp` | A bounds check with `>` where `>=` was meant. | `>=`. |
| 9 | `AudioPipeline/IO/AudioFileLoader.cpp` | A WAV file with 0 channels divided by zero. | Refused, like one with more than 8 channels. |
| 10 | `PokemonFRLG_Navigation.cpp`, `spam_first_move()` | The counter of failures was tested but never raised: a battle in which nothing was detected went on forever. | It is raised when nothing is detected. |
| 11 | `PokemonFRLG_PickupFarmer.cpp` | The counter of failed encounters was made anew in every round, so "failed 5 times" could never happen. | It lives outside the loop. |
| 12 | `PokemonSV_SinglesMoveTable.cpp` | The table never stored whether it is for a trainer battle, and its first row read the value before the table existed: whatever was in memory decided whether a row offered "Run". | The value is kept in a base class set before the first row. |
| 13 | `PokemonSV_WorldNavigation.cpp`, `jump_off_wall_until_map_open()` | The test for the last failed try was inside the loop, where it could never be true: three failed tries went on as if the map were open. | The failure is reported after the loop. |
| 14 | `PokemonSV_BoxDetection.cpp` | A copied bounds check tested `x` twice and never `y`: a negative row became row 255. | It tests `y`. |
| 15 | `PokemonSV_DialogArrowDetector.cpp` | A counter was used before it was ever set. | It starts at 0. |
| 16 | `PokemonLZA_FastTravelNavigation.cpp` | A "confirmation" after a wait compared the value read before the wait. | The screen is read again. |
| 17 | `PokemonLGPE_AlolanTrade.cpp`, `PokemonLGPE_FossilRevival.cpp`, `PokemonSwSh_DateSpam-PokeJobsFarmer.cpp` | Loop counters of 16 bits counting up to an option of 32 bits: above 65,535 the counter wraps around and the loop never ends. | Counters as wide as the option. |
| 18 | `PokemonSwSh_PkmnLib_Battle.cpp` | The move scores of the Dynamax Adventures counted a terrain for every Pokémon but a Flying type that also has Levitate (`!is_flying \|\| !is_levitate`): Electric moves of a Zapdos got the boost of Electric Terrain. | The test is the one the game uses. |
| 19 | `AudioPipeline/AudioOption.cpp` | Settings written before 29 October 2026 keep the audio input under old keys. After a change of the original (`32f0aaf`, "AudioSourceDescriptor (Part 6)", #1764) the code reads the new keys only, so such settings came up without an audio input and nothing said so: a program that listens for the shiny sound hears nothing. It showed in a test run of 13 November that had no sound. | The old keys are read too. |
| 20 | `PokemonFRLG_NuggetBridgeFarmer.cpp` | None of the program's six waits had an end, and its "Errors" count was never raised. The walk from the Pokémon Center to the Rocket goes by timings alone: five of its six legs end at a wall or at the Rocket's trigger, or may come out a tile short or long. | Every wait has an end and the count is raised; an option "Walk along the walls" (experimental) walks it by the walls. |
| 21 | `Common/Cpp/Concurrency/PeriodicRunner.cpp` | A runnable that was removed before it had run once left its place in the schedule behind (the place was made with one reading of the clock and the entry with another, and the removal looked for the place by the entry's time), and the runner's thread then ran what was not there any more. The program stops its serial port poller on every way out, also when it was made a moment before, so a program that started and stopped at once crashed at exit. Found by the tests of the installations (two processes that live less than a second). | The entry has the time of its place; the removal erases its own place only. |

Also changed for the themes: `SimpleIntegerWidget.cpp` and `FloatingPointWidget.cpp` set a valid number to black text, which cannot be read in a dark theme; `StringSelectWidget.cpp` set an entry without a color to black. They take the theme's text color now. Colored text of the programs (about 30 places write `<font color="blue">` and the like for a white background) is shown in a shade that reads on both themes, by the filter that translates the texts: the programs' own files did not change.

## 7. Faults in this fork's own first builds

These were found by the tests of the builds, and are not in the original:

- **Build 1: a crash at the first start without saved settings.** Found by the interface test of the first build; fixed in build 2. Build 1 must not be used.
- **Build 101, first try: the build did not compile.** The GUI-free core library (`CoreLib`, which has no Qt) compiled `GlobalAutoPaths.cpp`, which now asked for the installation's folder through a header that includes Qt. The paths moved to a header and a file of their own that need nothing of Qt (`PocketPilot_InstancePaths.*`).
- **Build 101, second try: two overviews could start.** The test of the installations showed that on Windows a second local server can listen on the same pipe name, so the overview could not rely on a failed listen to be the only one. It takes a lock file (`Installations/hub.lock`) first, as every installation takes `running.lock`.

## 8. Security (review of 25 November 2026)

[SECURITY.md](SECURITY.md) lists what leaves the program (nothing at start; no telemetry; everything else only when its part is used) and what was done about what the review found:

- Discord webhook addresses and tokens in the settings file are sealed with the Windows account's key (DPAPI). **Why:** they were plain text in a file that people copy, zip and attach to bug reports.
- Text that goes to Claude or to a report is cleaned of the name of the account in a path, webhook addresses, tokens and keys.
- The firmware that the setup flashes is checked against hashes that the build made; esptool is pinned by its version and its SHA-256.
- The workflow was split in two: `build` only reads the repository, and `publish` has the right to write and does nothing but check the zip against the hash and put it on a release. Third-party actions are pinned by their commit, and so is Pokémon Automation's `Packages`. **Why:** a pull request's build runs code from the pull request, and it must not be able to use a token that writes.
- The Discord bot (with OpenSSL 1.1.1k) is out of the Windows build.

## 9. The files of the original that changed, and why

Besides the fixes (section 6), the changes in files that the original has are these, and nothing else:

| Files | Change | Why |
|---|---|---|
| `Main.cpp`, `MainWindow.*`, `PanelLists.*`, the `*_Panels.cpp` of the games, `Panels/*` | The new window and setup start from here; the programs are registered with a description the new screens use; one installation's id is read first | The new interface needs the list of programs with their games, and the options of a program as widgets in its own screen. |
| `Globals.*`, `GlobalAutoPaths.*`, `StaticGlobals.cpp`, `PersistentSettings.*` | Name, home page of the project, the build's number, settings file named after the program, per-installation folders | The identity (section 3). `RUNTIME_BASE_PATH()` is the one place that folders are made from, so the installations changed it only. |
| `GlobalSettingsPanel.*`, `ThemeSelectorOption.*`, `WallpaperOption.cpp` | The new themes, the settings search, the wallpaper off | The interface. |
| `VideoSession.*`, `SnapshotManager.*`, `VideoFrameConversion.*` | The latest frame of a session can be asked for; the frame converter | The miniature of an installation shows the capture card's picture; section 5. |
| `ColoredText.*`, `StringOption.cpp`, `OptionsPanelWidget.cpp`, `PanelElements.cpp` | Colors that read on both themes | Section 6, last paragraph. |
| `ProgramNotifications.cpp`, `DppCommandHandler.cpp`, `DiscordSocial.cpp`, `PokemonBDSP_ShinyEncounterDetector.cpp` | The links in messages point to this project | They said Pokémon Automation's page. |
| `*Session.h` of the panels and consoles | A small hook for the new screens | The program's screen holds the original's panel widget inside it. |
| `PokemonFRLG_*` (inference, navigation, panels, hunts) | Shiny clips; readers that the AutoStory also uses; the fixes 10, 11 and 20 | The AutoStory is built on them. |
| `.github/workflows/cpp-ci-serial-programs-base.yml` | `SerialPrograms.exe` is `ClaudeGG.exe` | The program was renamed. |
| `IconResource/*`, `README.md`, `.gitignore`, `SerialPrograms/CMakeLists.txt`, `cmake/SourceFiles.cmake` | Icons, the readme, the generated build number is ignored, new files are listed | |

## 10. The build, and the workflow that makes it

[`.github/workflows/pocketpilot-windows.yml`](.github/workflows/pocketpilot-windows.yml) (new). Every push to the branch `pocketpilot` makes a Windows build, tests it and publishes it as a pre-release `pocketpilot-build-N` with its SHA-256. A pull request into `pocketpilot` is built and tested but not published. It follows the original's workflow for Windows (Ninja and ccache, Qt 6.10.2), with two additions: the tests of section 3 and the two-job split of section 8. A build takes about 4 minutes when little changed, 35 minutes from nothing (an empty cache). The original's workflows are still in the repository and still work for the original's own checks (see [GITHUB-SETUP.md](GITHUB-SETUP.md), section 5, on whether to keep them running).

## 11. Keeping up with Pokémon Automation

By merging its `main` into `pocketpilot`; merge, build, and read the interface test's pictures. The first merge (149 commits, 11 November 2026) brought gamepad support, the FRLG Box Sorter and the Living Dex sorter, a reworked audio pipeline and device names for the dropdowns; the second brought the state of 21 November. Pokémon Automation's rules for contributions (new contributors have at most two open pull requests, opened and answered by a person) decide how the fixes of section 6 can be offered to it.

## 12. What is not known yet

- The AutoStory has not played a story on this base; the test run with Charmander continues from the branch `frlg-autostory`.
- The fixes of section 6 and the experimental programs have not run on a console.
- Claude (the panel, and Claude Plays) has not run against the real API from a build machine, which has no key; it has been tested against made-up answers.
- Several installations have been tested as processes on the build machine; two real capture cards at once have not been tried.

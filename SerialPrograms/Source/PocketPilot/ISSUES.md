# Pokémon Automation's open issues, and what PocketPilot has of them

The 59 open issues of [PokemonAutomation/Arduino-Source](https://github.com/PokemonAutomation/Arduino-Source/issues) (three pages), each one opened and read on 13 November 2026. A sixtieth, [#1875](https://github.com/PokemonAutomation/Arduino-Source/issues/1875) (the Living Dex sorter moved a leftover Pokémon from the wrong row), was closed that day; its fix is in the code this fork is built on.

Most of the list is the developers' own to-do list: more than half of the issues are a title with one line of text or none, and some twenty are about Pokémon Legends: Z-A. Three carry the label "bug". None reports a fault in a FireRed and LeafGreen program.

The last column says where this fork stands:

- **Here**: PocketPilot has it, or the code it is built on has it.
- **Could be built here**: it needs nothing this fork cannot be tested with (FireRed or LeafGreen on a Switch, a capture card, an ESP32 controller).
- **Not here**: it needs another game, a console, hardware or data this fork cannot be tested with.
- **Theirs**: it is about how Pokémon Automation's developers work (tests, tools for training models, cleanups, libraries).

## What came out of reading them

- **Fix 19: the audio input of older settings.** Following [#1032](https://github.com/PokemonAutomation/Arduino-Source/issues/1032) (a profile whose device fails to load says nothing) led to the reason this fork's own test run played 3.8 hours without sound on 13 November. Since 29 October 2026 (`32f0aaf`, "AudioSourceDescriptor (Part 6)", #1764) Pokémon Automation keeps the audio input under `"InputSourceType"` and no longer reads `"InputDevice"` and `"InputFile"`, so settings written by an older build come up without an audio input, and nothing says so. PocketPilot now reads the old keys when the new one is absent ([FIXES.md](FIXES.md), 19). It is in the branch and comes with the build after build 8.
- **Fix 18: terrains in the move scores of the Dynamax Adventures**, found while reading the code for [#1072](https://github.com/PokemonAutomation/Arduino-Source/issues/1072), which it does not explain ([FIXES.md](FIXES.md), 18).
- **Two things worth building here**, neither built yet: the log of the last run as one file or on the clipboard ([#954](https://github.com/PokemonAutomation/Arduino-Source/issues/954)), and a notice on the program's page when a saved capture card, audio input or controller is not found ([#1032](https://github.com/PokemonAutomation/Arduino-Source/issues/1032)).

## The program as a whole, and FireRed and LeafGreen

| Issue | What it asks or reports | PocketPilot |
|---|---|---|
| [#1827](https://github.com/PokemonAutomation/Arduino-Source/issues/1827) [Tracker] FRLG AutoStory | Pokémon Automation's plan for an AutoStory of its own; the start of the game is coded, from the first gym on it is not. | **Here.** This fork's AutoStory played a whole game, from a new game to the Hall of Fame; the story after it is being tested. |
| [#1870](https://github.com/PokemonAutomation/Arduino-Source/issues/1870) Box Sorters - All Games - Roadmap | One standard for the box programs of all games, Pokémon Home first. For FireRed and LeafGreen: reading a Pokémon's Poké Ball and types in the box. Nothing on the list is ticked off yet. | **Theirs to finish.** Their FRLG Box Sorter is in this fork as it is; the rest comes with a merge of their `main`. |
| [#954](https://github.com/PokemonAutomation/Arduino-Source/issues/954) Improve log reporting | A place in the interface that opens the log folder, and a button that copies the log of the last program session. | **Half here.** Settings has "Runtime Base Folder: Open Folder", and `SerialPrograms.log` is in that folder. The button for the last run's log **could be built here**. |
| [#1032](https://github.com/PokemonAutomation/Arduino-Source/issues/1032) Display something when something fails to load | A profile with an item that fails to load (an unplugged or broken device) should warn the user. | **Could be built here.** Not built. Fix 19 takes away one way to get there. |
| [#1035](https://github.com/PokemonAutomation/Arduino-Source/issues/1035) Is the audio reset every 5 s when a reset did not fix it | A question to look into, without text. | **The code says yes:** `AudioSession::on_watchdog_timeout()` resets the audio every "Audio Auto-Reset" seconds (5 by default) for as long as no audio comes in, with two lines in the log each time. With no audio input selected it does nothing. Nothing changed here. |
| [#570](https://github.com/PokemonAutomation/Arduino-Source/issues/570) Qt 6.9.0 hangs when stopping a program during inference (bug) | From Qt 6.9 on, a thread that ends can be taken over by another thread's handle, and joining it hangs: "Stopping program" forever. Qt 6.8.3 did not have it; 6.9.1 still did. | **Here, as upstream has it:** their workaround is in the code (`Qt6.9ThreadBugWorkaround.h`, and the program leaves with `quick_exit()` instead of joining its threads). The builds use Qt 6.10.2, as theirs do. In this fork's run of 13 November a stop by hand took 0.02 s. |
| [#1077](https://github.com/PokemonAutomation/Arduino-Source/issues/1077) Investigate alternatives to Qt | Because of #570, which they call existential; "no obvious alternative to Qt". | **Theirs.** |
| [#909](https://github.com/PokemonAutomation/Arduino-Source/issues/909) Use PaddleOCR for OCR, [#620](https://github.com/PokemonAutomation/Arduino-Source/issues/620) Investigate ONNX-based OCR | A better text reader than Tesseract, which is poor at Asian languages. | **Here.** The code reads text with PaddleOCR on ONNX Runtime by default (Settings, "OCR library"); the AutoStory uses it. The issues are still open upstream. |
| [#1018](https://github.com/PokemonAutomation/Arduino-Source/issues/1018), [#144](https://github.com/PokemonAutomation/Arduino-Source/issues/144), [#145](https://github.com/PokemonAutomation/Arduino-Source/issues/145) Tesseract | A list of allowed characters, a newer version, control over which CPU instructions it uses. | **Theirs.** Tesseract is no longer the default reader. |
| [#617](https://github.com/PokemonAutomation/Arduino-Source/issues/617) Lazy UI loading | A page with many options, or a program with several videos, takes a noticeable moment to open; add the options bit by bit or show that it is loading. | **Could be built here.** Not built. |
| [#1876](https://github.com/PokemonAutomation/Arduino-Source/issues/1876) Remap gamepad controls | A mapping table for a gamepad on the computer, like the one for the keyboard. Assigned to one of their developers. | **Not here:** it cannot be tested without a gamepad. |
| [#11](https://github.com/PokemonAutomation/Arduino-Source/issues/11) Several programs in one window | Run several programs in one instance instead of several copies of the program. Their note: what blocked it is gone. | **Not built.** |
| [#619](https://github.com/PokemonAutomation/Arduino-Source/issues/619) Move videos off the main Qt thread, [#67](https://github.com/PokemonAutomation/Arduino-Source/issues/67) OpenCV camera widget | Drawing the video on the interface's thread costs speed with several videos or a slow computer; Qt's video uses much CPU. An earlier try failed. | **Not here.** With one Switch at 720p a picture takes 9 ms on this fork's test computer. |
| [#150](https://github.com/PokemonAutomation/Arduino-Source/issues/150) ARM for M1 Macs, [#64](https://github.com/PokemonAutomation/Arduino-Source/issues/64) Linux | Builds for other systems. The code has ARM kernels and workflows for Linux and macOS by now. | **Not here:** this fork is built for Windows only. |

## Other games and hardware

All **not here**: each needs a game or hardware this fork cannot be tested with.

| Issue | What it asks or reports |
|---|---|
| [#1072](https://github.com/PokemonAutomation/Arduino-Source/issues/1072) Refrigerate type change (bug) | Sword and Shield, Dynamax Adventures: an Aurorus with Refrigerate never picked Body Slam. No cause found by them or here; the conversion to Ice is in the code. Fix 18 was found next to it. |
| [#1315](https://github.com/PokemonAutomation/Arduino-Source/issues/1315) ESP32 slow to pair with a Switch 2 as a Joy-Con (bug) | Several minutes to pair, wireless, as a Joy-Con only. Their answer: probably that one board. |
| [#586](https://github.com/PokemonAutomation/Arduino-Source/issues/586) Implement Gyro | Motion input for catching in Let's Go Pikachu and Eevee. The controller code has experimental gyro calls. |
| [#1455](https://github.com/PokemonAutomation/Arduino-Source/issues/1455) Home Box Sorter: a faster way to sort | Pokémon Home: fewer moves (swaps along cycles, the page view), and a simulator to compare ways. |
| [#996](https://github.com/PokemonAutomation/Arduino-Source/issues/996), [#984](https://github.com/PokemonAutomation/Arduino-Source/issues/984), [#983](https://github.com/PokemonAutomation/Arduino-Source/issues/983), [#911](https://github.com/PokemonAutomation/Arduino-Source/issues/911) Box sorters | The Legends: Z-A sorter moving by distance without looking; sorting by form and by gender; the Sword and Shield sorter brought up to date with shared code. |
| [#999](https://github.com/PokemonAutomation/Arduino-Source/issues/999), [#997](https://github.com/PokemonAutomation/Arduino-Source/issues/997), [#987](https://github.com/PokemonAutomation/Arduino-Source/issues/987), [#986](https://github.com/PokemonAutomation/Arduino-Source/issues/986), [#985](https://github.com/PokemonAutomation/Arduino-Source/issues/985) Donut Maker (Legends: Z-A) | Saving what was read of the flavors, reading berry names by their fixed order, counting the berries, recognizing flavor powers and berries by their icons. |
| [#981](https://github.com/PokemonAutomation/Arduino-Source/issues/981), [#903](https://github.com/PokemonAutomation/Arduino-Source/issues/903), [#982](https://github.com/PokemonAutomation/Arduino-Source/issues/982) Hyperspace (Legends: Z-A) | A shuttle run with a calorie reader and a game reset, rerolling the spawns of a hole, a correction to the labelled pictures. |
| [#988](https://github.com/PokemonAutomation/Arduino-Source/issues/988), [#827](https://github.com/PokemonAutomation/Arduino-Source/issues/827) Wild Zones (Legends: Z-A) | Avoiding the change of day and night at the Wild Zone Cafe, and hunting what appears only by day or only by night. |
| [#840](https://github.com/PokemonAutomation/Arduino-Source/issues/840), [#762](https://github.com/PokemonAutomation/Arduino-Source/issues/762), [#761](https://github.com/PokemonAutomation/Arduino-Source/issues/761) New programs (Legends: Z-A) | An endless Battle Royale, stat resets for the two legendary Pokémon, battling one trainer of the main story over and over. |
| [#768](https://github.com/PokemonAutomation/Arduino-Source/issues/768), [#767](https://github.com/PokemonAutomation/Arduino-Source/issues/767), [#764](https://github.com/PokemonAutomation/Arduino-Source/issues/764) Sounds to listen for | Being targeted by a wild Pokémon, a Pokémon fainting, an alpha's noise. |
| [#945](https://github.com/PokemonAutomation/Arduino-Source/issues/945), [#944](https://github.com/PokemonAutomation/Arduino-Source/issues/944) Auto catcher | Making sure the right ball is thrown, and listening for the sound of a catch that held or failed. |
| [#857](https://github.com/PokemonAutomation/Arduino-Source/issues/857), [#834](https://github.com/PokemonAutomation/Arduino-Source/issues/834), [#771](https://github.com/PokemonAutomation/Arduino-Source/issues/771) | Following the player by the minimap; an option to leave out the shiny sound in some Z-A programs; the code entry of Scarlet and Violet for mystery gifts. |

## The developers' own work

All **theirs**.

| Issue | What it is |
|---|---|
| [#1863](https://github.com/PokemonAutomation/Arduino-Source/issues/1863), [#1013](https://github.com/PokemonAutomation/Arduino-Source/issues/1013) Tests | Tests that set their own game settings (the picture's place on the screen is one setting for all of them); a crash of the unit tests when a thread is left running. |
| [#979](https://github.com/PokemonAutomation/Arduino-Source/issues/979), [#978](https://github.com/PokemonAutomation/Arduino-Source/issues/978), [#956](https://github.com/PokemonAutomation/Arduino-Source/issues/956), [#955](https://github.com/PokemonAutomation/Arduino-Source/issues/955), [#791](https://github.com/PokemonAutomation/Arduino-Source/issues/791), [#790](https://github.com/PokemonAutomation/Arduino-Source/issues/790) Tools for training models | Trying two newer models, labelling pictures with a model's first guess, the thickness of a label's border, picking up a long computation after a crash, exporting a set of pictures. |
| [#858](https://github.com/PokemonAutomation/Arduino-Source/issues/858), [#651](https://github.com/PokemonAutomation/Arduino-Source/issues/651) Cleanups | Shorter namespace lines; a logger class that both contains and is a logger. |
| [#1033](https://github.com/PokemonAutomation/Arduino-Source/issues/1033) Chinese names | The Chinese names of some Pokémon in the text reader's lists, to match the names shown. |

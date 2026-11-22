# PocketPilot Studio

<img src="IconResource/pocketpilot.png" width="96" alt="PocketPilot Studio's icon: a handheld console with a play arrow on its screen">

Automation programs for Switch games: run them, and build or fix them with Claude.

A program of PocketPilot Studio plays a Pokémon game on a Nintendo Switch by itself. It watches the game through a capture card and presses the buttons through a controller board (for example an ESP32). Claude, in a panel of the main window, answers questions about the programs and can change their source: on a branch of its own, which becomes a pull request.

It is a personal fork of [Pokémon Automation](https://github.com/PokemonAutomation/Arduino-Source)'s "SerialPrograms", with a new interface, a program that plays the whole story of FireRed and LeafGreen, Claude built in, and fixes of its own. The programs for the other games are those of Pokémon Automation. Until build 12 its name was PocketPilot; the files and folders of the builds still have that name.

PocketPilot Studio is not made, endorsed or supported by Nintendo, Game Freak, Creatures or The Pokémon Company, nor by Anthropic or the Pokémon Automation project. Please do not ask the Pokémon Automation team for help with this fork: use the [issues of this repository](https://github.com/MajorLOL/Arduino-Source/issues).

## Getting it

Every push to the branch `pocketpilot` makes a Windows build and tests that it starts. A build that passes is published: the [releases](https://github.com/MajorLOL/Arduino-Source/releases) named "PocketPilot Studio - Windows build N" ("PocketPilot - Windows build N" until build 12). Unzip `PocketPilot-Windows.zip` and run `SerialPrograms.exe`. Each release also has pictures of that build's interface, taken on the build machine.

To keep your settings, close the old program first, then copy its `UserSettings` folder into the new folder. (The program writes its settings when its window closes. Since build 38 the AutoStory also has them written at its start and at each save of the game, so a copy made while it is still open has the options of the run.)

The hardware and its setup are the same as for Pokémon Automation: see [its documentation](https://pokemonautomation.github.io/).

## What is different from Pokémon Automation

| | |
|---|---|
| **Interface** | A top bar, a side bar with the games and a search over the programs, a page with all games as cartridges, a page with the programs of one game and what each does, and a status strip. Two themes of its own (Night and Day) in the color of a starter (Fire, Water or Grass), set under Settings. The two looks of Pokémon Automation are still there ("Classic"). While a program runs, the list of programs is locked instead of switched off: it still scrolls, and a click on another program says why nothing opens. |
| **Claude** | A panel behind the button "Claude" in the top bar: a conversation with Anthropic's Claude, on an API key of your own. It answers questions about the programs (it can read the program list, the options of the open program, the log and the source), and it builds or fixes a program: every change goes to a branch `claude/...` of this repository, after you have seen and approved it, and from there into a pull request that is built and tested before you merge it. In the experimental program **Claude Plays**, which is in the list of every game, Claude plays toward a goal you write down: it gets small pictures of the screen and the text the program reads from it, and works the controller. In FireRed and LeafGreen it has the AutoStory's routines for battles, text and walking on top of that. The list **Other Games** has it for a game without a list of its own. What it needs, what it costs and what it sends: [ASSISTANT.md](SerialPrograms/Source/PocketPilot/ASSISTANT.md). |
| **Search** | The side bar searches the programs of the game that is open; the Games page searches the programs of all games. |
| **FRLG AutoStory** | Plays FireRed or LeafGreen from a new game to the Hall of Fame and on: the legendary Pokémon, the Sevii Islands, the National Pokédex, Mewtwo and a Pokédex collection. See [the program's folder](SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory). |
| **Experimental programs** | Programs and options that are new and have not run on a console yet carry the tag "experimental": the FRLG **Level Trainer** (trains the first Pokémon of the party to a level, where the player stands or at one of the AutoStory's nine training places), the FRLG **Box Namer** (types a list of names into the PC's boxes, as an ACE code asks for, and reads every character back from the screen), the FRLG **Input Timing Test** (how long the game takes to show its reaction to a press, and how much that varies), **Claude Plays** in every list (see "Claude" above), the AutoStory's options "Faster battles", "Shorter looks in the grass" and "Progress on the video", the option "Walk along the walls" of the FRLG Nugget Bridge Farmer, and the next steps of the AutoStory's Pokédex collection (build 32: fishing with the Good Rod and the Super Rod, laps over the water off Five Island and from rock to rock in Cerulean Cave, Hitmonlee, the Snorlax of Route 16, Kabuto and Aerodactyl; build 33: a place of the collection that fails twice in a row is left out for the rest of the run, and the program goes on with the next one; build 34: Dunsparce's grass on Three Isle Port, and the PC's boxes read cell by cell; build 35: the Pokémon of the boxes that evolve with a stone change places with the sixth one of the party at the PC, get their stone and go back, after two battles for the prize money and a visit to the Celadon Department Store; all four ran on a console on 17 and 18 November and worked at the first run, eight evolved with a stone; build 36: the four trades in the game, with the Pokémon to give taken out of a box in the same way; the first of them was made on a console on 18 November, but the program did not see the trader's thanks after it and went on pressing A; build 37 puts that right and starts with a trade that an earlier run left out, build 38 is the same for the trades; on 18 November the end of a trade worked on a console (a Jynx was traded for a Poliwhirl), but the Farfetch'd was not taken for the one in the table (Pokémon Automation reads its name with a dash) and the Jynx came with a FAB MAIL on it, which keeps the PC from closing the party with it in the hand; build 39 has both, and on 18 November it ran: the Farfetch'd and the Jynx were traded and went into their boxes, the Jynx's MAIL into the PC's mailbox; the Mr. Mime and the Lickitung lay in their boxes already, where the list had the Abra and the Golduck, and build 40 takes such a trade as made, which ran on a console on 18 November; build 41, which ran on a console on 18 November: the Moon Stones of the Pokémon Mansion and Mt. Moon and the Sun Stone of Ruin Valley, which the Department Store does not sell, are picked up where the game has them, and used at once: Clefable, Wigglytuff and Bellossom evolved; build 43 and 44: the Pokémon of the boxes that evolve by a level are trained at Victory Road, one in each of nineteen segments, with the Exp. Share on them, and evolve there; on a console on 18 and 19 November 17 of them evolved in 16 hours; build 45: the ones that evolve by friendship, Crobat and Blissey, are walked with back and forth on the Indigo Plateau's plaza for the steps their friendship needs, which the program counts and saves as it goes, and then trained one level at Victory Road; build 46: a Pokémon that needs a lot of experience rides through the Elite Four again with the Exp. Share on it instead, three times as fast as at Victory Road; build 47: the Togepi egg of the Water Labyrinth, with the party's sixth Pokémon put into an empty cell of the boxes to make room, the egg walked until it hatches on the Indigo Plateau's plaza, and the Togepi put into the box, where the friendship segments take it for Togetic; build 48, experimental: the eggs of the Four Island Day Care for the baby Pokémon that only hatch, Pichu, Cleffa, Igglybuff, a second Togepi, Smoochum, Elekid and three Tyrogue; on a console on 20 and 21 November the swaps at the PC, the ferries, the woman taking both parents and the walk on the line worked, but the man's question was never reached, because the program pressed A too few times and the B that closed the box answered NO to the egg; build 49 put that right, and on 21 and 22 November 13 eggs came and all 13 hatched, each baby going into a box at the PC of Four Island, but the walk on to the woman stopped each time at the Day Care man, who stands one tile below his usual one while an egg waits and was in the way; build 50 walks around him, and on 22 November it went through all nine segments without a failure, in 4 hours 44 minutes: nine eggs hatched, the parents came back from the woman, the ferry took the player home and the game was saved each time, the table went from 153 to 159 of 175 species; build 51, experimental: five more level segments after the Day Care train what it hatched that evolves by a level, the three Tyrogue first, each to level 20, where the game makes Hitmonlee, Hitmonchan or Hitmontop of it by its Attack against its Defense, for the table's Hitmonchan). The Turbo Macro counts its loops. What they are for and how far each was tested: [ROADMAP.md](ROADMAP.md), sections 6 and 7. |
| **Video frames** | Pictures of the screen are made without Qt's `QVideoFrame::toImage()`, which on some computers grows the program's memory by 0.3 to 1.6 GB an hour while a program runs. See [VideoFrameConversion.h](SerialPrograms/Source/CommonFramework/VideoPipeline/Backends/VideoFrameConversion.h). |
| **Fixes** | Fixes for bugs found in a review of the whole code: see [the list](SerialPrograms/Source/PocketPilot/FIXES.md). What Pokémon Automation's open issues and other controller programs' users run into, and where this fork stands on each: [ISSUES.md](SerialPrograms/Source/PocketPilot/ISSUES.md), [OTHERS.md](SerialPrograms/Source/PocketPilot/OTHERS.md). |
| **Colors** | Colored text (in the options, the log window, the lists) is shown in a shade that can be read on the theme's background. |
| **Builds** | Each build starts once on the build machine, as on a computer that never ran it, goes through its pages, themes and lists and saves a picture of each. A build that does not start, crashes on the way or cannot take its pictures is not published; one that looks broken shows in its pictures before anyone downloads it. A pull request into `pocketpilot` is built and tested the same way, as it will be once merged, and not published. |
| **What it leaves out** | The check for a new version (it is for Pokémon Automation's releases) and Discord Rich Presence by default (it shows as Pokémon Automation, whose Discord application it is). The wallpaper is off until you turn it on. |

What is planned, what could be made faster and what could be added: [ROADMAP.md](ROADMAP.md).

## Building

PocketPilot Studio builds like Pokémon Automation's SerialPrograms: see [SerialPrograms/BuildInstructions](SerialPrograms/BuildInstructions). The Windows builds are made by [this workflow](.github/workflows/pocketpilot-windows.yml) with Qt 6.10.2.

`SerialPrograms --ui-screenshots <folder>` starts the program, saves a picture of each page, theme and opened list in the folder and exits (nothing is pressed on a console, no setting is saved). It also holds a conversation with a made-up Claude that ends in a commit on a made-up GitHub: no key is used and nothing is sent anywhere. Its exit code is 0 when every picture was taken and every check passed.

## Where the code is

PocketPilot Studio's own code is in [SerialPrograms/Source/PocketPilot](SerialPrograms/Source/PocketPilot): the themes, the frame of the main window, the Games page, the look of the program list, the "About" window and the test of the interface. Claude is in its folder [Assistant](SerialPrograms/Source/PocketPilot/Assistant): the connection to the Claude API, the conversation and its tools, the work on the repository through GitHub, and the panel. The rest of the tree is Pokémon Automation's, with the changes this fork needs. The branch follows Pokémon Automation's `main` by merging it.

## Licenses

- The source code is under the MIT license, like Pokémon Automation's: see [LICENSE](LICENSE) (Copyright (c) 2021 Alexander J. Yee). PocketPilot Studio's own changes and additions are under the same license.
- The builds bring along precompiled libraries and the resources of Pokémon Automation ([Packages](https://github.com/PokemonAutomation/Packages)), which that project gives for non-commercial use only. Use the builds for non-commercial purposes.
- The typeface of the name and the headings is Jersey 20 by Sarah Cadigan-Fried, under the [SIL Open Font License 1.1](SerialPrograms/Source/PocketPilot/Resources/Jersey20-OFL.txt).
- The names of the games belong to their owners and are used only to say which game a program is for.

Dependencies, as in Pokémon Automation:

| **Dependency** | **License** |
| --- | --- |
| Qt6 | LGPLv3 |
| [QDarkStyleSheet](https://github.com/ColinDuquesnoy/QDarkStyleSheet) | MIT |
| [Qt Wav Reader](https://code.qt.io/cgit/qt/qtmultimedia.git/tree/examples/multimedia/spectrum/app/wavfile.cpp?h=5.15) | BSD |
| [nlohmann json](https://github.com/nlohmann/json) | MIT |
| [D++](https://github.com/brainboxdotcc/DPP) | Apache 2.0 |
| [Tesseract](https://github.com/tesseract-ocr/tesseract) | Apache 2.0 |
| [Tesseract for Windows](https://github.com/peirick/Tesseract-OCR_for_Windows) | Apache 2.0 |
| [OpenCV](https://github.com/opencv/opencv) | Apache 2.0 |
| [ONNX](https://github.com/microsoft/onnxruntime) | MIT |
| [sdbus-c++](https://github.com/Kistler-Group/sdbus-cpp) | LGPLv2.1 |
| [SDL](https://github.com/libsdl-org/SDL) | zlib |

A tiny part of Pokémon Automation is not open source (telemetry and internal experiments, by its own README). It is not in this repository.

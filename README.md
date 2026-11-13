# PocketPilot

<img src="IconResource/pocketpilot.png" width="96" alt="PocketPilot's icon: a handheld console with a play arrow on its screen">

PocketPilot plays Pokémon games on a Nintendo Switch by itself. It watches the game through a capture card and presses the buttons through a controller board (for example an ESP32).

It is a personal fork of [Pokémon Automation](https://github.com/PokemonAutomation/Arduino-Source)'s "SerialPrograms", with a new interface, a program that plays the whole story of FireRed and LeafGreen, and fixes of its own. The programs for the other games are those of Pokémon Automation.

PocketPilot is not made, endorsed or supported by Nintendo, Game Freak, Creatures or The Pokémon Company, nor by the Pokémon Automation project. Please do not ask the Pokémon Automation team for help with this fork: use the [issues of this repository](https://github.com/MajorLOL/Arduino-Source/issues).

## Getting it

Every push to the branch `pocketpilot` makes a Windows build and tests that it starts. A build that passes is published: the [releases](https://github.com/MajorLOL/Arduino-Source/releases) named "PocketPilot - Windows build N". Unzip `PocketPilot-Windows.zip` and run `SerialPrograms.exe`. Each release also has pictures of that build's interface, taken on the build machine.

To keep your settings, close the old program first, then copy its `UserSettings` folder into the new folder.

The hardware and its setup are the same as for Pokémon Automation: see [its documentation](https://pokemonautomation.github.io/).

## What is different from Pokémon Automation

| | |
|---|---|
| **Interface** | A top bar, a side bar with the games and a search over the programs, a page with all games as cartridges, a page with the programs of one game and what each does, and a status strip. Two themes of its own (Night and Day) in the color of a starter (Fire, Water or Grass), set under Settings. The two looks of Pokémon Automation are still there ("Classic"). |
| **Search** | The side bar searches the programs of the game that is open; the Games page searches the programs of all games. |
| **FRLG AutoStory** | Plays FireRed or LeafGreen from a new game to the Hall of Fame and on: the legendary Pokémon, the Sevii Islands, the National Pokédex, Mewtwo and a Pokédex collection. See [the program's folder](SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory). |
| **Experimental programs** | Programs and options that are new and have not run on a console yet carry the tag "experimental": the FRLG **Level Trainer** (trains the first Pokémon of the party to a level, where the player stands or at one of the AutoStory's nine training places), the FRLG **Input Timing Test** (how long the game takes to show its reaction to a press, and how much that varies), and the AutoStory's options "Faster battles" and "Progress on the video". The Turbo Macro counts its loops. What they are for: [ROADMAP.md](ROADMAP.md), sections 6 and 7. |
| **Video frames** | Pictures of the screen are made without Qt's `QVideoFrame::toImage()`, which on some computers grows the program's memory by 0.3 to 1.6 GB an hour while a program runs. See [VideoFrameConversion.h](SerialPrograms/Source/CommonFramework/VideoPipeline/Backends/VideoFrameConversion.h). |
| **Fixes** | Fixes for bugs found in a review of the whole code: see [the list](SerialPrograms/Source/PocketPilot/FIXES.md). |
| **Colors** | Colored text (in the options, the log window, the lists) is shown in a shade that can be read on the theme's background. |
| **Builds** | Each build starts once on the build machine, as on a computer that never ran it, goes through its pages, themes and lists and saves a picture of each. A build that does not start, crashes on the way or cannot take its pictures is not published; one that looks broken shows in its pictures before anyone downloads it. |
| **What it leaves out** | The check for a new version (it is for Pokémon Automation's releases) and Discord Rich Presence by default (it shows as Pokémon Automation, whose Discord application it is). The wallpaper is off until you turn it on. |

What is planned, what could be made faster and what could be added: [ROADMAP.md](ROADMAP.md).

## Building

PocketPilot builds like Pokémon Automation's SerialPrograms: see [SerialPrograms/BuildInstructions](SerialPrograms/BuildInstructions). The Windows builds are made by [this workflow](.github/workflows/pocketpilot-windows.yml) with Qt 6.10.2.

`SerialPrograms --ui-screenshots <folder>` starts the program, saves a picture of each page, theme and opened list in the folder and exits (nothing is pressed on a console, no setting is saved). Its exit code is 0 when every picture was taken.

## Where the code is

PocketPilot's own code is in [SerialPrograms/Source/PocketPilot](SerialPrograms/Source/PocketPilot): the themes, the frame of the main window, the Games page, the look of the program list, the "About" window and the test of the interface. The rest of the tree is Pokémon Automation's, with the changes this fork needs. The branch follows Pokémon Automation's `main` by merging it.

## Licenses

- The source code is under the MIT license, like Pokémon Automation's: see [LICENSE](LICENSE) (Copyright (c) 2021 Alexander J. Yee). PocketPilot's own changes and additions are under the same license.
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

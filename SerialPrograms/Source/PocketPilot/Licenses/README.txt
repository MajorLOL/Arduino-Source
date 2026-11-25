PocketPilot: licenses
=====================

PocketPilot is a personal fork of Pokemon Automation's "SerialPrograms"
(https://github.com/PokemonAutomation/Arduino-Source). It is not made, endorsed
or supported by Nintendo, Game Freak, Creatures or The Pokemon Company, nor by
the Pokemon Automation project.

This folder holds:

  PokemonAutomation-MIT-License.txt
      The license of Pokemon Automation's source code, which PocketPilot is
      built on: MIT, Copyright (c) 2021 Alexander J. Yee. PocketPilot's own
      changes and additions are under the same license.

  Jersey20-OFL.txt
      The license of the typeface of PocketPilot's name and headings:
      Jersey 20, Copyright 2023 The Soft Type Project Authors, SIL Open Font
      License 1.1.

  Newsreader-OFL.txt, Fredoka-OFL.txt
      The licenses of the typefaces of the setup that follows the intro:
      Newsreader (the headings), Copyright 2020 The Newsreader Project
      Authors, and Fredoka (the other texts), Copyright 2016 The Fredoka
      Project Authors, both SIL Open Font License 1.1.

Use
---

The source code is under the MIT license. The builds bring along precompiled
libraries and the resources of Pokemon Automation, which that project gives
for non-commercial use only. Use the builds for non-commercial purposes.

Libraries the builds use
------------------------

  Qt 6                  LGPL version 3    https://www.qt.io/
                        Used unmodified, as separate libraries (the Qt6*.dll
                        files next to the program), which can be replaced by
                        other builds of the same version of Qt. Its source:
                        https://download.qt.io/official_releases/qt/
  QDarkStyleSheet       MIT               https://github.com/ColinDuquesnoy/QDarkStyleSheet
  Qt Wav Reader         BSD               from the examples of Qt Multimedia
  nlohmann json         MIT               https://github.com/nlohmann/json
  D++                   Apache 2.0        https://github.com/brainboxdotcc/DPP (in the source; not in the Windows build)
  Tesseract             Apache 2.0        https://github.com/tesseract-ocr/tesseract
  OpenCV                Apache 2.0        https://github.com/opencv/opencv
  ONNX Runtime          MIT               https://github.com/microsoft/onnxruntime
  SDL 3                 zlib              https://github.com/libsdl-org/SDL
  miniz                 MIT               https://github.com/richgel999/miniz

The FRLG RNG target search (Source/VideoGames/PokemonFRLG/Programs/RngManipulation:
PokemonFRLG_RngTargetSearch, PokemonFRLG_RngPlan, PokemonFRLG_RngPlanTracker and their
tests) was taken from https://github.com/dayfine/Arduino-Source, a fork of Pokemon
Automation under the same MIT license.

The names of the games belong to their owners and are used only to say which
game a program is for.

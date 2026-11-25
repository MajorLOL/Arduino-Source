/*  PocketPilot Setup
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The menus that come after the intro at the first start, and that can be
 *  opened again from the settings: the language, then the question which
 *  microcontroller is used, then the steps of its setup. Only the intro is a
 *  film; these are screens that wait for the user.
 *
 *  The intro and the menus are one screen: the film plays on the window of
 *  the setup, its last picture stays and fades into the first menu, and when
 *  the setup is over the screen stays up (the menu fades out, the background
 *  stays) until the main window is there. Nothing opens or closes in between.
 *
 *  Each screen starts with a short animation (the controllers in the
 *  background, the heading, the bar with the choice) and then waits for the
 *  choice, with the mouse or the keyboard:
 *
 *      Tab, Shift+Tab    the buttons
 *      Enter, Space      press the button, open the list, pick the row
 *      Up, Down          the rows of an open list
 *      Esc               close the list
 *      Left, Right       the steps
 *
 *  The look is that of the setup film: black, with the outlines of
 *  controllers, the headings in Newsreader, the rest in Fredoka, the color of
 *  the Joy-Cons. The screen is drawn at 1920 x 1080 and scaled to the window.
 *
 *  The boards and the steps are those of the Pokemon Automation Setup Guide
 *  (https://pokemonautomation.github.io/SetupGuide/index.html). The steps
 *  are given in the program for the Raspberry Pi Pico W and Pico 2 W and for
 *  the ESP32 and ESP32-S3, and the installation can be done from them (see
 *  PocketPilot_Firmware.h): the Pico gets its firmware copied once it is in
 *  flash mode, the ESP32 boards are flashed with esptool. The firmware card
 *  shows the version and its download link. "Open the Setup Guide" opens the
 *  guide inside the program, in the same style, and "Watch the video
 *  tutorial" plays a film of the setup of the board (Resources/tutorial-*.mp4,
 *  made by Tools/TutorialFilms.cpp from these same screens).
 *
 *  The last screen after "You are done!" asks which game to start, as
 *  cartridges (the same games as the page with the games has, see
 *  PocketPilot_Home.h). The one that is picked is opened in the main window
 *  when the setup ends (see take_chosen_game() in PocketPilot_Language.h).
 *
 *  The workflow that makes the builds also runs
 *
 *      SerialPrograms --setup-test <folder>
 *      SerialPrograms --tutorial-test
 *
 *  the first goes through all the screens without a person, with the mouse
 *  and the keyboard, and saves a picture of each in the folder; the second
 *  decodes the three tutorial films.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Setup_H
#define PokemonAutomation_PocketPilot_Setup_H

#include <vector>
#include <QString>
#include <QStringList>
#include "PocketPilot_Home.h"

class QApplication;

namespace PokemonAutomation{
namespace PocketPilot{


//  Option on the command line.
extern const char* const SETUP_TEST_OPTION;     //  --setup-test <folder>
extern const char* const TUTORIAL_TEST_OPTION;  //  --tutorial-test


struct SetupRequest{
    //  Which questions to ask: the language, the board.
    bool ask_language = false;
    bool ask_board = false;

    //  The language that is in use now ("en", "nl", ...).
    QString language_code = "en";

    //  The games to choose from at the end, in the order of the program's
    //  list of games (what the page with the games shows too). Empty: there
    //  is no choice at the end, "Start" just ends the setup.
    std::vector<HomeGame> games;

    //  Play the intro film first, on this same screen: its last picture goes
    //  over into the first page, so that no window opens or closes in
    //  between. With no question to ask the screen is just the intro.
    bool play_intro = false;

    //  Keep the screen up when the setup is over (the page fades out) until
    //  release_setup_curtain() is called: the main window is made next, and
    //  the desktop must not show in between.
    bool hold_screen = false;
};

struct SetupResult{
    //  The language was chosen: its code ("en", "nl", "de", "fr", "es" or
    //  "custom" for Other). Otherwise language_code stays as it was.
    bool language_chosen = false;
    QString language_code = "en";

    //  The board question was answered or skipped.
    bool board_done = false;
    //  The board that was chosen: "pico", "pico2", "esp32", "esp32s3", or
    //  "other"; empty when skipped.
    QString board;

    //  The game that was chosen at the end, by its name in the list of games
    //  (HomeGame::name). Empty: none was chosen ("Choose later", the setup
    //  was skipped or closed, or there were no games to choose from).
    QString game;

    //  How the intro ended ("it played to the end", "it was skipped", or what
    //  went wrong); empty when there was no intro.
    QString intro_note;
};


//  Shows the screens full screen and returns when they are done or skipped.
//  Needs a QApplication. The language is tried on the screens as it is
//  picked (see preview_language()); making it final is left to the caller.
SetupResult run_setup(const SetupRequest& request);

//  Ends the screen that was kept up with SetupRequest::hold_screen: it fades
//  away, over the window that was made in the meantime. Does nothing when no
//  screen is kept; it also goes by itself after a while.
void release_setup_curtain();

//  The test: all the screens, driven with the mouse and the keyboard. The
//  pictures go in the folder (when it is not empty). Returns the program's
//  exit code: 0 when all is well.
int run_setup_test(QApplication& application, const QString& screenshots_folder);

//  The test of the tutorial films: each is decoded without a window, has its
//  picture size, its sound and its length, and plays to the end. What was found
//  is in the report, a line each; the last line says passed or FAILED.
int run_tutorial_test(QApplication& application, QStringList& report);



}
}
#endif

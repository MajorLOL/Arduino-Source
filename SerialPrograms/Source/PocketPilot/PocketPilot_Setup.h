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
 *  are given for the Raspberry Pi Pico W and Pico 2 W in USB mode; for the
 *  ESP32 boards the screen points to the guide, with its video.
 *
 *  The workflow that makes the builds also runs
 *
 *      SerialPrograms --setup-test <folder>
 *
 *  which goes through all the screens without a person, with the mouse and
 *  the keyboard, and saves a picture of each in the folder.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Setup_H
#define PokemonAutomation_PocketPilot_Setup_H

#include <QString>

class QApplication;

namespace PokemonAutomation{
namespace PocketPilot{


//  Option on the command line.
extern const char* const SETUP_TEST_OPTION;     //  --setup-test <folder>


struct SetupRequest{
    //  Which questions to ask: the language, the board.
    bool ask_language = false;
    bool ask_board = false;

    //  The language that is in use now ("en", "nl", ...).
    QString language_code = "en";
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
};


//  Shows the screens full screen and returns when they are done or skipped.
//  Needs a QApplication. The language is tried on the screens as it is
//  picked (see preview_language()); making it final is left to the caller.
SetupResult run_setup(const SetupRequest& request);

//  The test: all the screens, driven with the mouse and the keyboard. The
//  pictures go in the folder (when it is not empty). Returns the program's
//  exit code: 0 when all is well.
int run_setup_test(QApplication& application, const QString& screenshots_folder);



}
}
#endif

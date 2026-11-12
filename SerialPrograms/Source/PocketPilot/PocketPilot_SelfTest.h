/*  PocketPilot Self Test
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  A test of the interface for the builds: the program started with
 *
 *      SerialPrograms --ui-screenshots <folder>
 *
 *  opens its main window, goes through its pages and themes, opens the
 *  lists of two dropdowns, saves a picture of each in the folder and exits.
 *  Nothing is pressed on a console and no setting is saved. What it is
 *  about to do goes to the program's log first. The workflow that makes
 *  the builds runs it on a machine without settings, keeps the pictures
 *  with the build and does not publish a build that fails it: every build
 *  shows what it looks like and that it starts.
 *
 */

#ifndef PokemonAutomation_PocketPilot_SelfTest_H
#define PokemonAutomation_PocketPilot_SelfTest_H

#include <string>

class QApplication;

namespace PokemonAutomation{

class MainWindow;

namespace PocketPilot{


//  The option's name on the command line.
extern const char* const UI_SCREENSHOTS_OPTION;

//  Takes the pictures. Returns the program's exit code: 0 when all of them
//  were saved.
int run_ui_screenshots(QApplication& application, MainWindow& window, const std::string& folder);



}
}
#endif

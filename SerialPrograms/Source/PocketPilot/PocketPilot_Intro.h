/*  PocketPilot Intro
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The short film that plays first when the program starts: two Joy-Cons
 *  click into a handheld, a chat bubble with "GG" lights up on its screen.
 *  It is in the program (Resources/intro.mp4: picture and sound in one
 *  file) and plays full screen on a black background.
 *
 *  It plays on the screen of the setup (see PocketPilot_Setup.h): one window
 *  from the film to the first question, whose last picture fades into it, and
 *  to the main window after it. Nothing opens or closes in between. When
 *  there is nothing to ask, the last picture of the film stays until the main
 *  window is there.
 *
 *  It never gets in the way:
 *    - a click or any key skips it;
 *    - Settings > "Play the intro at startup" turns it off, and so does
 *      --no-intro on the command line;
 *    - if it cannot be played (no decoder, no file, no picture within a
 *      few seconds) the program goes on at once;
 *    - the tests (--ui-screenshots, --command-line-test-mode) skip it.
 *
 *  The workflow that makes the builds also runs
 *
 *      SerialPrograms --intro-test
 *
 *  which decodes the film without showing it and exits with 0 when the
 *  picture and the sound of the whole film came through.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Intro_H
#define PokemonAutomation_PocketPilot_Intro_H

class QApplication;

namespace PokemonAutomation{
namespace PocketPilot{


//  Options on the command line.
extern const char* const NO_INTRO_OPTION;       //  --no-intro
extern const char* const INTRO_TEST_OPTION;     //  --intro-test

//  Decodes the intro without a window and without sound output. Returns the
//  program's exit code: 0 when the whole film came through with its sound.
int run_intro_test(QApplication& application);



}
}
#endif

/*  PocketPilot Setup
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The menus that come after the intro at the first start, and that can be
 *  opened again from the settings: the language, then the console (one of the
 *  consoles that the program supports now), then the question which
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
 *  After the installation ("You are done!") comes a new screen of its own that
 *  asks which game to start, as cartridges with the logo of each game (the
 *  same games as the page with the games has, see PocketPilot_Home.h). It is
 *  no part of the installation. It is also shown alone, at each start, when
 *  the setting "Ask which game to start" is on. The one that is picked is
 *  opened in the main window (see take_chosen_game() in
 *  PocketPilot_Language.h).
 *
 *  The same screens are also the main window (see AppView below): the games,
 *  then the programs of one game, then the program itself, with the buttons
 *  of the old top bar (settings, Claude, the output log, the button diagram,
 *  about) along the top. The program's own page (its console, Start and Stop,
 *  its options) is still the old Qt widget, held inside the screen under a
 *  header that has the way back and the state. Nothing in it ends the screen:
 *  "Start" opens the program, it does not leave. (This is the first version
 *  of "one interface"; what each button does is listed in ROADMAP.md.)
 *
 *  More than one installation of the program can run (see
 *  PocketPilot_Instances.h). Their overview, as miniatures, is one more page of
 *  these screens, in a window of its own (SetupRequest::overview); every other
 *  window has a button that goes back to it (the one in the corner of the
 *  setup's screens, "Installations" along the top of the main window).
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

#include <functional>
#include <memory>
#include <vector>
#include <QImage>
#include <QString>
#include <QStringList>
#include "PocketPilot_Home.h"
#include "PocketPilot_Instances.h"

class QApplication;
class QWidget;

namespace PokemonAutomation{
namespace PocketPilot{


//  Option on the command line.
extern const char* const SETUP_TEST_OPTION;     //  --setup-test <folder>
extern const char* const TUTORIAL_TEST_OPTION;  //  --tutorial-test

//  One line of the output log of the program, for the log panel of the screen
//  with the programs.
struct SetupLogLine{
    //  Counts up from 1; the panel asks for the lines after the last it has.
    quint64 serial = 0;
    //  When it was written (milliseconds since the epoch).
    qint64 msec = 0;
    //  0 information, 1 warning, 2 error.
    int level = 0;
    //  The line without its time.
    QString text;
};

//  The options of one program as the widgets that its own page has, for the
//  panel that slides open next to the list of programs. The options are those
//  of the program, kept where its page keeps them: what is changed here is
//  what the page shows later. The setup deletes it when another program is
//  picked or the screen goes away (its widget goes with it).
class SetupProgramOptions{
public:
    virtual ~SetupProgramOptions() = default;

    //  The widget with the options, a child of the parent that it was made
    //  for. Null: the program has no options. It lives as long as this does.
    virtual QWidget* widget() = 0;

    //  True while something was changed that is not kept yet.
    virtual bool changed() const = 0;

    //  Keeps the options (and only them: not the console that the page of the
    //  program also keeps).
    virtual void save() = 0;
};


struct SetupRequest{
    //  Which questions to ask: the language, the board.
    bool ask_language = false;
    bool ask_board = false;

    //  Ask which console is used: a screen of its own between the language
    //  and the board, in the same style, with the consoles that are supported
    //  now. It belongs to the setup, so it goes with ask_board.
    bool ask_console = false;

    //  The console that was chosen before ("switch" or "switch2"), shown as
    //  picked; empty: none yet.
    QString console_code;

    //  The language that is in use now ("en", "nl", ...).
    QString language_code = "en";

    //  The games to choose from, in the order of the program's list of games
    //  (what the page with the games shows too). Empty: there is no choice,
    //  "Start" just ends the setup.
    std::vector<HomeGame> games;

    //  The programs of those games (HomeProgram::game is the index into
    //  "games"). When the game that was chosen has some, a screen of its own
    //  follows the choice of the game: which program to start. Empty: none.
    std::vector<HomeProgram> programs;

    //  Fills "out" with the lines of the output log that came after "after"
    //  (oldest first; the first call, with 0, gets what is kept). Empty: the
    //  log panel stays empty.
    std::function<void(quint64 after, std::vector<SetupLogLine>& out)> log_poll;

    //  The options of a program (an index into "games" and the name of the
    //  program in it), made as children of "parent". Null when it cannot be
    //  made. Empty: the panel with the options is not offered.
    std::function<std::unique_ptr<SetupProgramOptions>(QWidget* parent, int game, const QString& program)> make_options;

    //  Which program this is ("PocketPilot 1.2.3, build 76"), for the report
    //  that "Copy for Claude" makes.
    QString build;

    //  Ask which game to start, as a screen of its own: after the questions
    //  that are asked (the language), or alone. (After the installation of
    //  the board the choice always follows when there are games.)
    bool ask_game = false;

    //  Play the intro film first, on this same screen: its last picture goes
    //  over into the first page, so that no window opens or closes in
    //  between. With no question to ask the screen is just the intro.
    bool play_intro = false;

    //  Keep the screen up when the setup is over (the page fades out) until
    //  release_setup_curtain() is called: the main window is made next, and
    //  the desktop must not show in between.
    bool hold_screen = false;

    //  ---- the installations (see PocketPilot_Instances.h)

    //  This screen is the overview of the installations, in a window of its own,
    //  instead of the setup. It needs the four functions that follow.
    bool overview = false;
    //  The installations, with what they tell (called about once a second).
    std::function<std::vector<InstanceInfo>()> instance_list;
    //  A miniature was pressed: open that installation (bring its window
    //  forward, or start it).
    std::function<void(const QString& id)> instance_open;
    //  "New installation" was pressed: makes one and starts it; returns its id
    //  (empty: it could not be made).
    std::function<QString()> instance_create;
    //  "Remove" was pressed twice: deletes the installation's folder. False:
    //  it was not deleted.
    std::function<bool(const QString& id)> instance_remove;

    //  The button in the corner of every other screen that is a window of its
    //  own: it shows the overview (the screen puts its own window away). Empty:
    //  the button is not drawn.
    std::function<void()> show_overview;
};

struct SetupResult{
    //  The language was chosen: its code ("en", "nl", "de", "fr", "es" or
    //  "custom" for Other). Otherwise language_code stays as it was.
    bool language_chosen = false;
    QString language_code = "en";

    //  The console was chosen: its code ("switch" or "switch2").
    bool console_chosen = false;
    QString console_code;

    //  The board question was answered or skipped.
    bool board_done = false;
    //  The board that was chosen: "pico", "pico2", "esp32", "esp32s3", or
    //  "other"; empty when skipped.
    QString board;

    //  The game that was chosen at the end, by its name in the list of games
    //  (HomeGame::name). Empty: none was chosen ("Choose later", the setup
    //  was skipped or closed, or there were no games to choose from).
    QString game;
    //  The program that was chosen in that game, by its name (HomeProgram::name).
    //  Empty: none was ("Choose later", or the game has none to choose from).
    QString program;

    //  How the intro ended ("it played to the end", "it was skipped", or what
    //  went wrong); empty when there was no intro.
    QString intro_note;
};


//  What the main window does for the screens when they are its own screen.
//  Every action may be empty: the button is drawn, and does nothing.
struct AppActions{
    //  Opens a program of a game (an index into SetupRequest::games and the
    //  name of the program in it). The window answers with AppView::host()
    //  before it returns. False: it could not be opened (a program runs).
    std::function<bool(int game, const QString& program)> open_program;

    //  The program that is open is closed (the way back). False: it cannot
    //  be closed now (a program runs).
    std::function<bool()> close_program;

    //  The buttons along the top.
    std::function<void()> settings;
    std::function<void()> about;
    std::function<void()> output;       //  the output log
    std::function<void()> buttons;      //  the diagram of the buttons
    //  Claude's panel: shown or hidden (the window calls AppView::show_claude()).
    std::function<void(bool show)> claude;

    //  Another console was picked on the screen with the consoles.
    std::function<void(const QString& code)> console_chosen;

    //  "Keep the screen on" was switched.
    std::function<void(bool on)> keep_screen_on;

    //  "Installations": shows the overview of the installations (the window is
    //  put away by the screen). Empty: the button is not drawn.
    std::function<void()> installations;
};

//  The screens as the main window's own screen: a child of "parent", it stays
//  until the window is closed. The window owns it through its parent, as it
//  owns any widget.
class AppView{
public:
    virtual ~AppView() = default;

    virtual QWidget* widget() = 0;

    //  The games, or the programs of one of them (an index into the games).
    virtual void show_games() = 0;
    virtual void show_programs(int game) = 0;

    //  A program (or the settings: game -1) is open: its widget goes under a
    //  header with the way back. "content" is the window's own widget; it
    //  stays the window's: unhost() gives it back before it is deleted.
    virtual void host(QWidget* content, int game, const QString& game_name, const QString& title) = 0;
    virtual void unhost(QWidget* content) = 0;
    virtual bool hosting() const = 0;

    //  The program of the last time ("Continue with ..." on the games): the
    //  game's name and the program's. Empty: none.
    virtual void set_resume(const QString& game, const QString& program) = 0;

    //  A program runs (or not): the state in the header, and no way back.
    virtual void set_state(bool busy, const QString& text) = 0;
    //  What is said about the computer's sleep, beside its switch.
    virtual void set_sleep_text(const QString& text) = 0;
    //  The switch "Keep the screen on" is shown as set (or not), without the
    //  action: for the state that was kept from the last time.
    virtual void set_keep_on(bool on) = 0;

    //  Claude's panel, over the right of the screen when it is shown. The
    //  panel stays the window's.
    virtual void set_claude(QWidget* panel) = 0;
    virtual void show_claude(bool show) = 0;
    virtual bool claude_shown() const = 0;

    //  ---- for the test, which has no event loop of its own: time passes (with
    //  the pictures drawn), a press on a button by its name, what page it is.
    virtual void wait(double seconds) = 0;
    virtual bool click(const QString& id) = 0;
    virtual bool has_button(const QString& id) const = 0;
    virtual QString page() const = 0;
    virtual QImage picture() = 0;
};

//  Makes the screens as the main window's own screen. Starts on the games.
//  "test": no clock of its own (time only passes in wait()), and nothing
//  opens outside the program.
AppView* make_app_view(QWidget* parent, const SetupRequest& request, AppActions actions, bool test = false);

//  Shows the screens full screen and returns when they are done or skipped.
//  Needs a QApplication. The language is tried on the screens as it is
//  picked (see preview_language()); making it final is left to the caller.
SetupResult run_setup(const SetupRequest& request);

//  The overview of the installations as a window of its own, full screen
//  (SetupRequest::overview): returns when it is closed. With a folder it makes
//  no window on the screen, goes through the overview with the mouse and the
//  keyboard, saves pictures in the folder and returns 0 when all was well.
int run_instances_overview(const SetupRequest& request, const QString& screenshots_folder);
//  The overview comes forward, also when it was minimized.
void bring_overview_forward();

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

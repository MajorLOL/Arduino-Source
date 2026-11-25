/*  PocketPilot Language
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The language of the menus, the settings and the explanations. English is
 *  the language of the program and stays the default; Dutch, German, French
 *  and Spanish come with it, and "Other" reads a file of your own.
 *
 *  How it works
 *  ------------
 *  The English text stays in the source, as it always was. A language is a
 *  file (Resources/lang/<code>.json, in the program) that maps an English
 *  text to its translation. Two things look the texts up:
 *
 *    - tr_ui("English text") for text the program puts together itself
 *      (with %1, %2 ... for the parts that change);
 *    - a filter on the application that looks at the labels, buttons,
 *      group boxes, tabs, menus, tool tips, placeholders and window titles
 *      of every window as it is shown, and swaps the text for its
 *      translation when the dictionary has it. That is how the texts of
 *      Pokemon Automation's own options and panels are translated without
 *      changing their files, so merging its updates stays easy.
 *
 *  What the dictionary does not have stays English: a missing translation
 *  never breaks anything. Drop-down lists of the programs are left alone on
 *  purpose (the programs read their choice back from the text in them).
 *
 *  Your own language
 *  -----------------
 *  "Other" in the list reads Languages/custom.json next to the program. The
 *  same file format works for any language; files of the four languages
 *  that come with the program can be overridden the same way
 *  (Languages/nl.json, ...): what is in the file wins. With a language
 *  other than English, the texts that were looked up and not found are
 *  written to Languages/missing-<code>.json when the program closes, a
 *  list to start from.
 *
 *  The language is set in Settings ("Language"), takes effect at once, and is
 *  asked once, after the intro, at the first start.
 *
 *  The workflow that makes the builds also runs
 *
 *      SerialPrograms --language-test
 *
 *  which loads every language, checks that each translation keeps the
 *  parts that must not change (%1, tags, line breaks), and translates a few
 *  windows on screen.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Language_H
#define PokemonAutomation_PocketPilot_Language_H

#include <stddef.h>
#include <functional>
#include <memory>
#include <vector>
#include <QString>
#include <QStringList>
#include "PocketPilot_Home.h"
#include "PocketPilot_Setup.h"

class QApplication;
class QObject;

namespace PokemonAutomation{
namespace PocketPilot{


//  Options on the command line.
extern const char* const NO_LANGUAGE_PROMPT_OPTION;     //  --no-language-prompt
extern const char* const LANGUAGE_TEST_OPTION;          //  --language-test

//  The code of a language ("en", "nl", ...) and the name in that language.
struct LanguageInfo{
    const char* code;
    const char* native_name;
};
const LanguageInfo* language_list(size_t& count);


//  The translation of an English text, or the text itself when the current
//  language has none (and always when the language is English). Safe to call
//  from any thread.
QString tr_ui(const QString& english);
QString tr_ui(const char* english);                     //  UTF-8

//  The current language's code.
QString current_language_code();

//  Puts the language of the settings into use: loads its dictionary and
//  translates every window that is open. Only on the main thread, with a
//  QApplication.
void apply_language();

//  The texts that set a color for themselves (<font color="blue">) are shown
//  in a shade that reads on the theme in use: the filter does it as a window
//  is shown. This does it again for every window that is open, after the theme
//  changed. Only on the main thread, with a QApplication.
void refresh_text_colors();

//  Calls "callback" on the main thread every time the language changed, after
//  the windows that are open were translated. For the texts a window makes
//  itself with tr_ui (that the filter cannot know about): it builds them
//  again. It goes away with "owner".
void on_language_changed(QObject* owner, std::function<void()> callback);

//  Installs the filter on the application and makes the language of the
//  settings the current one. Call once, after the settings were read. Also
//  follows the setting from then on.
void install_language_support(QApplication& application);

//  Tries a language without keeping it: the dictionary is loaded and the
//  windows that are open are translated, the setting stays as it was. The
//  setup uses this while the user looks at the list. apply_language() puts
//  the language of the settings back.
void preview_language(const QString& code);

//  The setup at the first start (and when it is opened from the settings):
//  the language, then the microcontroller, full screen and in the style of
//  the intro. Keeps what was answered (the language in the settings, the
//  questions that were asked are not asked again) and writes the settings.
//  Returns when it is done. Does nothing when both questions were answered.
void ask_setup_if_needed();

//  What comes on the screen when the program starts: the intro (when
//  "play_intro"), then the setup (when "ask_setup", and there is a question
//  left to ask). All on one screen, so that nothing opens or closes in
//  between: the film's last picture goes over into the first question, and
//  when the questions are over the screen stays up until the main window is
//  there (see release_setup_curtain() in PocketPilot_Setup.h, which Main
//  calls). With neither, nothing is shown. When "ask_setup", and the setting
//  "Ask which game to start" is on, a screen of its own with the games
//  follows (alone, when the language and the board have been asked before).
void show_start_screens(bool play_intro, bool ask_setup);

//  How many games the setup would offer (what the lister returns), and the
//  same for the log: the games must be there before the main window is.
size_t count_games_for_setup();
//  And how many programs it has to choose from after a game.
size_t count_programs_for_setup();
//  And for how many games the options of their first program can be made as
//  widgets, as the panel next to the list of programs does (see
//  set_options_maker()). The test of the interface checks it.
size_t count_options_for_setup();

//  The same, whatever the settings say: "Open the setup" in the settings.
void open_setup();

//  The screens of the setup as the main window's own screen (see AppView):
//  the games and the programs given, the output log, the options of the
//  programs and the build as the setup has them, and the console that is
//  picked on the way kept in the settings. "test": see make_app_view().
AppView* make_main_view(
    QWidget* parent,
    std::vector<HomeGame> games, std::vector<HomeProgram> programs,
    AppActions actions, bool test = false
);

//  The setup ends with the choice of a game to start. The games to choose
//  from are listed by this function (the main window does not exist yet at
//  the first start): it fills the first list with the games, their pictures
//  and their number of programs, and the second with all their programs (each
//  knows its game by the index into the first). Set it before the setup is
//  asked; without it there is no choice at the end.
void set_game_lister(std::function<void(std::vector<HomeGame>&, std::vector<HomeProgram>&)> lister);

//  Makes the options of a program (a game: an index into the games of the
//  lister, and the name of the program in it) as the widgets that its own page
//  has, as children of "parent", for the panel next to the list of programs.
//  They are kept where the page keeps them. Without it the panel is not
//  offered.
void set_options_maker(std::function<std::unique_ptr<SetupProgramOptions>(QWidget* parent, int game, const QString& program)> maker);

//  What the main window does with a game that was chosen when the setup is
//  opened from its settings (the window is there then): shows the game, as a
//  click on its cartridge does, and opens the program that was chosen in it
//  (empty: none was). The window sets it while it lives.
void set_game_opener(std::function<void(const QString& game, const QString& program)> opener);

//  The game that was chosen at the end of the setup, when there was no main
//  window yet to open it (the first start). Returns it once; empty when none
//  was chosen. The main window asks for it when it is made.
QString take_chosen_game();
//  The program that was chosen in that game (empty: none). Take it after the
//  game; returns it once.
QString take_chosen_program();

//  The folder with the languages of the user (Languages, next to the
//  program), made if it is not there yet.
QString user_language_folder();

//  The test: loads every language, checks its translations and the
//  filter. Returns the program's exit code: 0 when all is well.
int run_language_test(QApplication& application);

//  For the test: the problems of one dictionary file's content, as lines of
//  text (empty when it is fine). "english" is the key, "translated" its
//  translation.
QStringList check_translation(const QString& english, const QString& translated);



}
}
#endif

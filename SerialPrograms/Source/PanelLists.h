/*  Program Tabs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_ProgramTabs_H
#define PokemonAutomation_ProgramTabs_H

#include <map>
#include <QWidget>
#include "CommonFramework/Panels/PanelList.h"
#include "PocketPilot/PocketPilot_Home.h"
#include "PocketPilot/PocketPilot_Setup.h"

class QComboBox;
class QLabel;
class QLineEdit;

namespace PokemonAutomation{


// The games of the program with their pictures and the number of programs
// each has, in the order of the side bar. Needs no window: the setup uses it
// to ask which game to start before the main window is made. Empty when the
// lists could not be made (then "error" says why, when it is given). With
// "programs", the programs of all the games come too, in the order of their
// lists (HomeProgram::game is the index into "games"), with their sections,
// descriptions and tags: the setup asks which program to start with them.
void describe_all_games(
    std::vector<PocketPilot::HomeGame>& games,
    std::string* error = nullptr,
    std::vector<PocketPilot::HomeProgram>* programs = nullptr
);


// The options of one program of one game (an index into the games of
// describe_all_games(), and the name of the program in it) as the widgets that
// the page of the program has, as children of "parent". The setup shows them
// next to its list of programs, before the main window is there. They are
// loaded from and kept in the settings of the program's page, and only the
// options: the console of the page is left alone (no camera or serial port is
// opened). Null when there is no such program or it has no options.
std::unique_ptr<PocketPilot::SetupProgramOptions> make_setup_options(
    QWidget* parent, int game, const QString& program
);


// The program selection UI on the left side of the program window.
// It has a dropdown menu to select which Switch game's program list to show, a
// box to search that list, and a display list window to show the current active
// game's program list.
// This class owns the a vector of PanelListDescriptor. A PanelListDescriptor is
// like a generator for the programs and their UIs for a game.
class ProgramSelect : public QWidget{
public:
    ProgramSelect(QWidget& parent, PanelHolder& holder);

    void lock();
    void unlock();

    // Load the panel specified in the persistent setting.
    void load_persistent_panel();

    // The game and the program of the persistent setting. Empty: none.
    void persistent_panel(std::string& game, std::string& program) const;

    // The games and all their programs, for the page that shows them.
    void describe(
        std::vector<PocketPilot::HomeGame>& games,
        std::vector<PocketPilot::HomeProgram>& programs
    ) const;

    // The name of the game whose list is shown.
    const std::string& current_game() const;

    // Shows a game's list. "game" is an index into the games of describe().
    void open_game(int game);
    // Shows a game's list and opens one of its programs.
    void open_program(int game, const std::string& program);
    // The same by the game's name. False if there is no such game.
    bool open_program(const std::string& game, const std::string& program);

    // No program is open anymore: no row of the list is marked.
    void clear_current();

    virtual QSize sizeHint() const override;

private:
    void add(std::unique_ptr<PanelListDescriptor> list);
    void change_list(int index);
    void apply_filter();

private:
    PanelHolder& m_holder;

    std::vector<std::unique_ptr<PanelListDescriptor>> m_lists;
    std::map<std::string, int> m_tab_map;

    QComboBox* m_dropdown;
    QLineEdit* m_search;
    QLabel* m_nothing_found;

    bool m_locked = false;

    int m_active_index = -1;
    PanelListWidget* m_active_list = nullptr;
};



}
#endif

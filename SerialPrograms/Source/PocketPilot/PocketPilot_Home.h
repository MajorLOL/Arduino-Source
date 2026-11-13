/*  PocketPilot Home
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The page the main window shows when no program is open: the games as
 *  cartridges, the programs of one game with what each of them does, a
 *  search over the programs of all games, and a way back to the program
 *  that was open last.
 *
 *  It knows nothing of the rest of the program: the main window hands it
 *  the games and the programs, and what to do on a click.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Home_H
#define PokemonAutomation_PocketPilot_Home_H

#include <functional>
#include <vector>
#include <QPixmap>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace PokemonAutomation{
namespace PocketPilot{


struct HomeGame{
    QString name;           //  As in the list of games.
    QPixmap icon;           //  May be empty.
    int programs = 0;
    bool enabled = true;
};

struct HomeProgram{
    int game = 0;           //  Index into the games.
    QString section;        //  The title of its section in the game's list. May be empty.
    QString name;
    QString description;
};


//  The name of a game without the series in front of it:
//  "Pokemon FireRed and LeafGreen" becomes "FireRed and LeafGreen".
QString short_game_name(const QString& name);

//  The two colors of a game's cartridge label. Games that come as a pair of
//  versions get the colors of both; the others the same color twice.
void game_label_colors(const QString& name, QColor& first, QColor& second);


class HomePage : public QWidget{
public:
    struct Actions{
        //  A game was picked: show its list in the side bar.
        std::function<void(int game)> open_game;
        std::function<void(int game, const QString& program)> open_program;
        //  Back to the program that was open last.
        std::function<void()> resume;
    };

    HomePage(
        QWidget& parent,
        std::vector<HomeGame> games,
        std::vector<HomeProgram> programs,
        Actions actions
    );

    //  The program that was open last, to go back to. An empty program:
    //  nothing to go back to.
    void set_resume(const QString& game, const QString& program);

    //  All games, as cartridges.
    void show_games();
    //  The programs of one game, each with what it does.
    void show_game(int game);
    //  The same by the game's name. False if there is no such game.
    bool show_game(const QString& name);
    //  What a click on a game's cartridge does: its programs here and its
    //  list in the side bar. False if there is no such game.
    bool pick_game(const QString& name);

    void set_search_text(const QString& text);
    //  The cursor in the search box, so that typing searches at once.
    void focus_search();

protected:
    virtual void changeEvent(QEvent* event) override;

private:
    class GameTile;
    class TileGrid;

    void pick_game(int game);
    void refresh();
    void add_program_row(int index, const QString& second_column);
    void open_result(QTreeWidgetItem* item);

private:
    std::vector<HomeGame> m_games;
    std::vector<HomeProgram> m_programs;
    Actions m_actions;

    //  The game whose programs are shown. -1: all games.
    int m_game = -1;
    QString m_resume_program;

    //  Built: the widgets below are there.
    bool m_ready = false;

    QPushButton* m_back = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_lead = nullptr;
    QLineEdit* m_search = nullptr;
    QTreeWidget* m_results = nullptr;
    QLabel* m_no_results = nullptr;
    QWidget* m_resume_row = nullptr;
    QPushButton* m_resume_button = nullptr;
    QLabel* m_resume_label = nullptr;
    TileGrid* m_grid = nullptr;
    QWidget* m_filler = nullptr;
};



}
}
#endif

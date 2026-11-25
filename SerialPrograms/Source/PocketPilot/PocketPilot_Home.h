/*  PocketPilot Home
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  What the screens of the window show of the games: the games as the list
 *  of programs has them (the cartridges) and the programs of each, the name
 *  of a game as it is shown, and the colors of its cartridge label. (The
 *  screens themselves are in PocketPilot_Setup.h.)
 *
 */

#ifndef PokemonAutomation_PocketPilot_Home_H
#define PokemonAutomation_PocketPilot_Home_H

#include <QColor>
#include <QPixmap>
#include <QString>

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
    QString tag;            //  "beta", "rough", ...; empty for a regular program.
    QString tag_meaning;
};


//  The name of a game without the series in front of it:
//  "Pokemon FireRed and LeafGreen" becomes "FireRed and LeafGreen".
QString short_game_name(const QString& name);

//  The two colors of a game's cartridge label. Games that come as a pair of
//  versions get the colors of both; the others the same color twice.
void game_label_colors(const QString& name, QColor& first, QColor& second);



}
}
#endif

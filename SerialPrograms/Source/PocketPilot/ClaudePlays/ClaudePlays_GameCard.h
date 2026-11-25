/*  PocketPilot Claude Plays: the card of a game
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  A game that has a list of its own but no picture file among the
 *  resources (these come with the packages, one file for each game) gets a
 *  card the program draws: the game's name on a ground of one color, in the
 *  size and shape of the pictures of the other lists. It is not the game's
 *  logo: the program carries none.
 *
 */

#ifndef PokemonAutomation_PocketPilot_ClaudePlays_GameCard_H
#define PokemonAutomation_PocketPilot_ClaudePlays_GameCard_H

#include <string>
#include <QImage>

namespace PokemonAutomation{
namespace PocketPilot{
namespace ClaudePlays{


//  256 x 128. The name is cut into lines at its spaces, as many as it needs
//  (three at the most); without a GUI application (no fonts) only the ground
//  is drawn.
QImage make_game_card(const std::string& name, QRgb ground);



}
}
}
#endif

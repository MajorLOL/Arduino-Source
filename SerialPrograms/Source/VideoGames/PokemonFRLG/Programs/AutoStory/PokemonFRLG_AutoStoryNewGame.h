/*  Pokemon FRLG Autostory New Game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A new game from the title screen: NEW GAME, Professor Oak's intro, the
 *  player's and the rival's names typed on the naming screen, and the
 *  first save in the bedroom, where segment C0_01 starts.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryNewGame_H
#define PokemonAutomation_PokemonFRLG_AutoStoryNewGame_H

#include <string>
#include "PokemonFRLG_AutoStoryTools.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Empty if the naming screen can type the name (1 to 7 of A-Z, a-z, 0-9,
//  space . , ! ? / - '), else what is wrong with it.
std::string story_name_problem(const std::string& name);

//  Soft reset, NEW GAME, the intro with the two names, and a save in the
//  bedroom (over the save there was). Returns whether there was a save on
//  the cartridge before (its game options are kept; without one the game
//  has its default options).
bool story_new_game(StoryRuntime& rt, const std::string& player, const std::string& rival);


}
}
}
#endif

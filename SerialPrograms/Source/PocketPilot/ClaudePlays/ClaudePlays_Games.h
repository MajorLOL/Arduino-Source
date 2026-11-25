/*  PocketPilot Claude Plays: the games
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  What the program "Claude Plays" of each game's list tells Claude about
 *  its game, and "Other Games" for whatever else is on the screen.
 *  (FireRed and LeafGreen have a Claude Plays of their own, with the
 *  routines of the AutoStory: VideoGames/PokemonFRLG/Programs/ClaudePlays.)
 *
 *  Every game that is added to the program gets a list with a Claude Plays
 *  in it: a GameId and a profile here, and either a list of its own in
 *  VideoGames/ (like ZeldaTotK) or, while it has no programs, an entry in
 *  has_only_claude_plays() (the list is then made by GameOnlyList). The
 *  notes of a profile say only what is certain.
 *
 *  No Qt in this header.
 *
 */

#ifndef PokemonAutomation_PocketPilot_ClaudePlays_Games_H
#define PokemonAutomation_PocketPilot_ClaudePlays_Games_H

#include <string>
#include <vector>

namespace PokemonAutomation{
namespace PocketPilot{
namespace ClaudePlays{


enum class GameId{
    OTHER_GAMES,
    POKEMON_HOME,
    LETS_GO,
    SWORD_SHIELD,
    BRILLIANT_DIAMOND_SHINING_PEARL,
    LEGENDS_ARCEUS,
    SCARLET_VIOLET,
    LEGENDS_ZA,
    POKOPIA,
    RUBY_SAPPHIRE_EMERALD,
    ZELDA_TOTK,
    OCTOPATH_TRAVELER_0,
    MARIO_KART_8_DELUXE,
    SPLATOON_3,
    ANIMAL_CROSSING_NEW_HORIZONS,
};

//  The controller the game's programs use.
enum class PadKind{
    PRO_CONTROLLER,
    RIGHT_JOYCON,
};

struct GameProfile{
    GameId id;
    //  The program's identifier: "PokemonSV:ClaudePlays".
    std::string identifier;
    //  The category of the game's programs: "Pokemon SV" (with the accent).
    std::string category;
    //  For Claude: "Pokemon Scarlet or Violet". Empty for OTHER_GAMES.
    std::string name;
    //  For the user, in the program's description: "Pokemon Scarlet and
    //  Violet".
    std::string title;
    //  For Claude: the controls and what else helps, as far as they are
    //  certain. The buttons on the screen are what counts.
    std::string notes;
    PadKind pad = PadKind::PRO_CONTROLLER;
    //  The game's other programs have a routine that saves the game (the
    //  program wires it to the tool save_game).
    bool save_routine = false;
};

const GameProfile& game_profile(GameId id);
//  All of them, in the order of the list of games.
const std::vector<GameId>& all_games();

//  The games that have a list of their own with nothing in it but Claude
//  Plays (no program for the game itself yet): the list is made by
//  GameOnlyList (ClaudePlays_Program.h).
bool has_only_claude_plays(GameId id);

//  The name of the list of games "Other Games".
extern const char* const OTHER_GAMES_LIST;



}
}
}
#endif

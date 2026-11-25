/*  PocketPilot Claude Plays: the games
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The notes say only what is certain: the controls as the games' manuals
 *  and the routines of this program's other programs have them. Claude is
 *  told to go by the buttons the screen shows.
 *
 */

#include <map>
#include "Pokemon/Pokemon_Strings.h"
#include "ClaudePlays_Games.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace ClaudePlays{


const char* const OTHER_GAMES_LIST = "Other Games";


namespace{

std::map<GameId, GameProfile> make_profiles(){
    const std::string& POKEMON = Pokemon::STRING_POKEMON;
    std::map<GameId, GameProfile> profiles;
    auto add = [&](GameProfile profile){
        profiles[profile.id] = std::move(profile);
    };

    add(GameProfile{
        GameId::OTHER_GAMES,
        "OtherGames:ClaudePlays",
        OTHER_GAMES_LIST,
        "",
        "whatever game is on the screen",
        "",
        PadKind::PRO_CONTROLLER,
        false,
    });
    add(GameProfile{
        GameId::POKEMON_HOME,
        "PokemonHome:ClaudePlays",
        POKEMON + " Home",
        "Pokemon HOME, the app that keeps Pokemon in boxes and moves them between games",
        POKEMON + " HOME",
        "It is an app, not a game: the stick or the d-pad moves the cursor, A picks, B goes back. What is "
        "moved or released there is final once the app has saved it: do only what the goal names.",
        PadKind::PRO_CONTROLLER,
        false,
    });
    add(GameProfile{
        GameId::LETS_GO,
        "PokemonLGPE:ClaudePlays",
        POKEMON + " LGPE",
        "Pokemon Let's Go, Pikachu! or Let's Go, Eevee!",
        POKEMON + " Let's Go, Pikachu! and Let's Go, Eevee!",
        "The stick walks and moves the cursor, A confirms and talks, B cancels, X opens the menu. Catching a "
        "wild Pokemon takes a throwing motion of the controller, which the program cannot make: an encounter "
        "can only be left again, with the button the screen shows for running away.",
        PadKind::RIGHT_JOYCON,
        false,
    });
    add(GameProfile{
        GameId::SWORD_SHIELD,
        "PokemonSwSh:ClaudePlays",
        POKEMON + " SwSh",
        "Pokemon Sword or Shield",
        POKEMON + " Sword and Shield",
        "The left stick walks, the right stick turns the camera in the Wild Area. A confirms and talks, B "
        "cancels, X opens the menu, Y opens Y-Comm, PLUS gets on and off the bike.",
        PadKind::PRO_CONTROLLER,
        true,
    });
    add(GameProfile{
        GameId::BRILLIANT_DIAMOND_SHINING_PEARL,
        "PokemonBDSP:ClaudePlays",
        POKEMON + " BDSP",
        "Pokemon Brilliant Diamond or Shining Pearl",
        POKEMON + " Brilliant Diamond and Shining Pearl",
        "The left stick or the d-pad walks, and B held runs. A confirms and talks, B cancels, X opens the "
        "menu.",
        PadKind::PRO_CONTROLLER,
        true,
    });
    add(GameProfile{
        GameId::LEGENDS_ARCEUS,
        "PokemonLA:ClaudePlays",
        POKEMON + " LA",
        "Pokemon Legends: Arceus",
        POKEMON + " Legends: Arceus",
        "The left stick walks (pressed in: run), the right stick turns the camera. A talks, picks up and "
        "confirms, B crouches and goes back in menus, Y rolls out of the way. X switches between items and "
        "Pokemon to throw, L and R pick which one, ZR aims while held and throws when let go, ZL locks on. "
        "PLUS calls the ride Pokemon, MINUS opens the map, UP on the d-pad opens the satchel and the menu.",
        PadKind::PRO_CONTROLLER,
        true,
    });
    add(GameProfile{
        GameId::SCARLET_VIOLET,
        "PokemonSV:ClaudePlays",
        POKEMON + " SV",
        "Pokemon Scarlet or Violet",
        POKEMON + " Scarlet and Violet",
        "The left stick walks, the right stick turns the camera, L puts the camera behind the player. A "
        "talks, picks up and confirms, B cancels (on foot it crouches, on the ride Pokemon it jumps), X opens "
        "the main menu, Y the map, PLUS gets on and off the ride Pokemon, MINUS opens the Pokedex. ZR throws "
        "the first Pokemon's ball (at a wild Pokemon: a battle), R sends it out to fight on its own, ZL "
        "locks on.",
        PadKind::PRO_CONTROLLER,
        true,
    });
    add(GameProfile{
        GameId::LEGENDS_ZA,
        "PokemonLZA:ClaudePlays",
        POKEMON + " LZA",
        "Pokemon Legends: Z-A",
        POKEMON + " Legends: Z-A",
        "The left stick walks, the right stick turns the camera, B held runs. X opens the main menu, PLUS "
        "the map. Battles happen in real time, where the player stands.",
        PadKind::PRO_CONTROLLER,
        true,
    });
    add(GameProfile{
        GameId::POKOPIA,
        "PokemonPokopia:ClaudePlays",
        POKEMON + " Pokopia",
        "Pokemon Pokopia",
        POKEMON + " Pokopia",
        "A life simulation with Pokemon, not a game of battles.",
        PadKind::PRO_CONTROLLER,
        false,
    });
    add(GameProfile{
        GameId::RUBY_SAPPHIRE_EMERALD,
        "PokemonRSE:ClaudePlays",
        POKEMON + " RSE",
        "Pokemon Ruby, Sapphire or Emerald",
        POKEMON + " Ruby, Sapphire, and Emerald",
        "A Game Boy Advance game: the d-pad walks, A confirms and talks, B cancels, START is PLUS and SELECT "
        "is MINUS on this controller. The overworld is a grid of tiles. Leave ZL and ZR alone: they are not "
        "the game's.",
        PadKind::PRO_CONTROLLER,
        false,
    });
    add(GameProfile{
        GameId::ZELDA_TOTK,
        "ZeldaTotK:ClaudePlays",
        "Zelda: TotK",
        "The Legend of Zelda: Tears of the Kingdom",
        "The Legend of Zelda: Tears of the Kingdom",
        "The left stick walks, the right stick turns the camera. A talks, picks up and confirms, B cancels, "
        "puts the weapon away and, held, sprints. X jumps, Y strikes, ZL raises the shield and locks on, ZR "
        "draws the bow, R throws, L uses the chosen ability. PLUS opens the inventory, MINUS the map.",
        PadKind::PRO_CONTROLLER,
        false,
    });
    add(GameProfile{
        GameId::OCTOPATH_TRAVELER_0,
        "OctopathTraveler0:ClaudePlays",
        "Octopath Traveler 0",
        "Octopath Traveler 0",
        "Octopath Traveler 0",
        "A turn-based role-playing game. The left stick walks, B dashes. A talks, examines and confirms, B "
        "cancels, X opens the menu, Y uses the character's Path Action (on the people of a town), L and R "
        "switch between the tabs of a menu, ZL opens the map of the area.",
        PadKind::PRO_CONTROLLER,
        false,
    });
    add(GameProfile{
        GameId::MARIO_KART_8_DELUXE,
        "MarioKart8Deluxe:ClaudePlays",
        "Mario Kart 8 DX",
        "Mario Kart 8 Deluxe",
        "Mario Kart 8 Deluxe",
        "The left stick steers. A accelerates, B brakes and, held, reverses. L or ZL uses the item, R or ZR "
        "hops and, held while steering, drifts. X looks behind, PLUS pauses. The game has assists for "
        "steering and acceleration: whether they are on shows in its options. A race goes on while Claude "
        "thinks, so one decision is a long time on the track: prefer holding a button for a moment over "
        "many short presses.",
        PadKind::PRO_CONTROLLER,
        false,
    });
    add(GameProfile{
        GameId::SPLATOON_3,
        "Splatoon3:ClaudePlays",
        "Splatoon 3",
        "Splatoon 3",
        "Splatoon 3",
        "The left stick moves, the right stick turns the camera and aims. B jumps. ZR fires the main weapon, "
        "R uses the sub weapon, ZL turns into a squid and back (swimming in the player's own ink refills "
        "it), pressing the right stick in uses the special weapon when it is charged. Y resets the camera, X "
        "opens the menu. The game can also aim by the motion of the controller, which the program cannot "
        "make: aim with the right stick.",
        PadKind::PRO_CONTROLLER,
        false,
    });
    add(GameProfile{
        GameId::ANIMAL_CROSSING_NEW_HORIZONS,
        "AnimalCrossingNH:ClaudePlays",
        "Animal Crossing NH",
        "Animal Crossing: New Horizons",
        "Animal Crossing: New Horizons",
        "A life simulation that runs on the Switch's clock, and saves by itself now and then. The left stick "
        "walks, and B held runs. A talks, picks up, uses the tool in hand and enters houses, B cancels, X "
        "opens the pockets, Y picks up an item that lies on the ground, ZL opens the NookPhone.",
        PadKind::PRO_CONTROLLER,
        false,
    });
    return profiles;
}

}   //  namespace


const GameProfile& game_profile(GameId id){
    //  On the first call: the names of the series are strings of another
    //  file, which have to be there first.
    static const std::map<GameId, GameProfile> profiles = make_profiles();
    return profiles.at(id);
}

bool has_only_claude_plays(GameId id){
    switch (id){
    case GameId::OCTOPATH_TRAVELER_0:
    case GameId::MARIO_KART_8_DELUXE:
    case GameId::SPLATOON_3:
    case GameId::ANIMAL_CROSSING_NEW_HORIZONS:
        return true;
    default:
        return false;
    }
}

const std::vector<GameId>& all_games(){
    static const std::vector<GameId> games{
        GameId::POKEMON_HOME,
        GameId::LETS_GO,
        GameId::SWORD_SHIELD,
        GameId::BRILLIANT_DIAMOND_SHINING_PEARL,
        GameId::LEGENDS_ARCEUS,
        GameId::SCARLET_VIOLET,
        GameId::LEGENDS_ZA,
        GameId::POKOPIA,
        GameId::RUBY_SAPPHIRE_EMERALD,
        GameId::ZELDA_TOTK,
        GameId::OCTOPATH_TRAVELER_0,
        GameId::MARIO_KART_8_DELUXE,
        GameId::SPLATOON_3,
        GameId::ANIMAL_CROSSING_NEW_HORIZONS,
        GameId::OTHER_GAMES,
    };
    return games;
}



}
}
}

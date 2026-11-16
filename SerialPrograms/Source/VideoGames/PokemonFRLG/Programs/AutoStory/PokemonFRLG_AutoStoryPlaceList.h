/*  Pokemon FRLG Autostory Place List
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The species of a place where the program catches for the POKeDEX (the
 *  text of a DEX step), with how often each comes there in each game:
 *
 *      doduo:35:35,raticate:5:5,nidoran-m:20:0
 *
 *  A name, then how many of 100 wild Pokemon there are that species in
 *  FireRed, then in LeafGreen. 0: it does not come there in that game. The
 *  two games do not have the same species everywhere: the center of the
 *  Safari Zone has NIDORAN M in FireRed and NIDORAN F in LeafGreen, and a
 *  list of both together made the program wait in FireRed for a NIDORAN F
 *  that only comes in the east and west areas.
 *
 *  And which place is worth a wait for a species: where it comes 1 in 10
 *  or more, or at least half as often as at the best place of the
 *  collection. On Route 16 a RATICATE is 1 in 20; in the Pokemon Mansion
 *  and on Route 18 it is 3 in 10. On 15 November the program stayed on
 *  Route 16 for 35 battles and 18 minutes for one.
 *
 *  This part has no dependencies on the rest of the program so it can be
 *  tested on its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlaceList_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlaceList_H

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The species of a place's list for a game (0 FireRed, 1 LeafGreen; any
//  other number: either game): the ones that come there in it, with how
//  often of 100. A name without numbers (a list that has none): it comes
//  there (100).
inline std::map<std::string, int> story_place_species(const char* text, int game){
    std::map<std::string, int> species;
    const std::string list = text == nullptr ? "" : text;
    size_t start = 0;
    while (start <= list.size()){
        const size_t comma = list.find(',', start);
        const std::string item = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        //  "slug", or "slug:F:L".
        const size_t first = item.find(':');
        const std::string slug = item.substr(0, first);
        int share[2] = {100, 100};
        if (first != std::string::npos){
            const size_t second = item.find(':', first + 1);
            const size_t length = second == std::string::npos ? std::string::npos : second - first - 1;
            share[0] = std::atoi(item.substr(first + 1, length).c_str());
            share[1] = second == std::string::npos ? share[0] : std::atoi(item.substr(second + 1).c_str());
        }
        const int here = game == 0 ? share[0] : game == 1 ? share[1] : std::max(share[0], share[1]);
        if (!slug.empty() && here > 0){
            species[slug] = here;
        }
        if (comma == std::string::npos){
            break;
        }
        start = comma + 1;
    }
    return species;
}

//  A place where a species comes "here" times of 100 is worth a wait for
//  it, when the best place of the collection has it "best" times of 100
//  (0: not known).
inline bool story_place_worth(int here, int best){
    return here > 0 && (here >= 10 || best <= 0 || 2 * here >= best);
}

//  Places the table still wants something from, but the collection is
//  short of what they take: a Safari Game is 500 ("short_of_money": one
//  was not joined), the other places take POKe BALLs ("short_of_balls":
//  none was left for a wanted species). Are they left for now?
//  "safari_places", "other_places": how many of each kind the steps have.
//  "trip_open": the money trip can still be made. Then every place waits
//  for it: the trip brings both, and the towns are gone round again after
//  it. Without it: only steps that have nothing but places of a kind that
//  cannot be done.
inline bool story_places_wait(
    int safari_places, int other_places,
    bool short_of_money, bool short_of_balls, bool trip_open
){
    if ((!short_of_money && !short_of_balls) || safari_places + other_places <= 0){
        return false;
    }
    if (trip_open){
        return true;
    }
    return (safari_places <= 0 || short_of_money) && (other_places <= 0 || short_of_balls);
}

//  The last town of the collection is through. Round the towns once more?
//  (The run of 16 November 2026 ended with one catch left that the route
//  has a place for, the second CHANSEY, after a round that had brought
//  four catches; the program stopped there, and to go on the game had to
//  be saved by hand in front of a Pokemon Center.)
//    "left": catches the table still wants at places where the species is
//        looked for.
//    "visited", "caught": the round that just ended went to a place; it
//        brought a catch.
//    "start_round_used": a round was already started without a catch
//        before it.
//    "rounds": rounds so far in this run.
//    "short": a Safari Game was refused or the balls ran out, and the money
//        trip did not make up for it.
//  Yes while the rounds bring something: after a round with a catch. And
//  once for a run that starts at the end (it has been to no place yet): so
//  a program started again where the last run stopped goes round for what
//  is left, and stops after a round that brought nothing.
const int STORY_MOST_ROUNDS = 8;
inline bool story_another_round(
    size_t left, bool visited, bool caught, bool start_round_used, int rounds, bool short_of_something
){
    if (left == 0 || short_of_something || rounds >= STORY_MOST_ROUNDS){
        return false;
    }
    return caught || (!visited && !start_round_used);
}


}
}
}
#endif

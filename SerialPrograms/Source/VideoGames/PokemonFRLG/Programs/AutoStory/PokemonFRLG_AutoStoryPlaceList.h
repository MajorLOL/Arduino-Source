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
#include <set>
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

//  A place of the collection is one trip from a nurse and back: its check
//  ("FishViridian?", "FishViridian_Visit2?"), the way there, the place, the
//  way back, the nurse, a save. The place a check is for: its label without
//  the question mark and without the number of the visit.
inline std::string story_trip_place(const std::string& label){
    std::string place = label;
    while (!place.empty() && (place.back() == '?' || place.back() == ' ')){
        place.pop_back();
    }
    const std::string visit = "_Visit";
    const size_t at = place.rfind(visit);
    if (at != std::string::npos && at > 0 && at + visit.size() < place.size()){
        bool digits = true;
        for (size_t k = at + visit.size(); k < place.size(); k++){
            digits = digits && place[k] >= '0' && place[k] <= '9';
        }
        if (digits){
            place.resize(at);
        }
    }
    return place;
}

//  The segment of a Pokemon that is there once (a gift, one that stands on
//  the map) counts as a place too: "the segment for HITMONLEE".
inline std::string story_once_place(const std::string& slug){
    std::string name = "the segment for ";
    for (char ch : slug){
        name += ch >= 'a' && ch <= 'z' ? (char)(ch - 'a' + 'A') : ch;
    }
    return name;
}

//  The segment that reads the PC's boxes counts as a place, too.
inline std::string story_boxes_place(){
    return "the PC's boxes";
}

//  And the segment of the evolution stones.
inline std::string story_stones_place(){
    return "the evolution stones";
}

//  The segments that evolve by level (build 43, experimental): all of them
//  count as one place when a run starts in the middle of one, and the one
//  Pokemon that is trained counts as a place of its own ("raising PIDGEOT"):
//  if that fails twice in a row, the next segment takes another one.
inline std::string story_levels_place(){
    return "the evolving by level";
}
inline std::string story_level_place(const std::string& slug){
    std::string name = "raising ";
    for (char ch : slug){
        name += ch >= 'a' && ch <= 'z' ? (char)(ch - 'a' + 'A') : ch;
    }
    return name;
}

//  The same for the segments that evolve by friendship (build 45,
//  experimental): the Pokemon walked with is a place of its own
//  ("walking for CROBAT").
inline std::string story_friends_place(){
    return "the evolving by friendship";
}
inline std::string story_friend_place(const std::string& slug){
    std::string name = "walking for ";
    for (char ch : slug){
        name += ch >= 'a' && ch <= 'z' ? (char)(ch - 'a' + 'A') : ch;
    }
    return name;
}

//  The same for the rounds of the Elite Four for experience (build 46,
//  experimental): the Pokemon that rides along is a place of its own
//  ("the Elite Four for PUPITAR").
inline std::string story_exp_rounds_place(){
    return "the Elite Four for experience";
}
inline std::string story_exp_place(const std::string& slug){
    std::string name = "the Elite Four for ";
    for (char ch : slug){
        name += ch >= 'a' && ch <= 'z' ? (char)(ch - 'a' + 'A') : ch;
    }
    return name;
}

//  The segment that fetches one evolution stone where the game has it
//  (build 41, experimental): the text of its first step is "Moon Stone in
//  Mt. Moon". The stone is the part before " in " (empty when the text has
//  no such part: the segment of all the stones), and the whole text is its
//  place for the failures of a run: "the Moon Stone in Mt. Moon".
inline std::string story_stone_fetch_stone(const std::string& text){
    const size_t at = text.find(" in ");
    return at == std::string::npos || at == 0 ? std::string() : text.substr(0, at);
}
inline std::string story_stone_fetch_place(const std::string& text){
    return "the " + text;
}

//  A TALK step for an item that may be gone (taken in an earlier
//  playthrough): no text after A is then no error, the walk goes on ("Pick
//  up item" on the way, "Pick up stone" for a stone that is fetched).
inline bool story_item_may_be_gone(const std::string& label){
    return label.rfind("Pick up item", 0) == 0 || label.rfind("Pick up stone", 0) == 0;
}

//  A trip that fails is tried again from the last save. After this many
//  failures in a row at the same place, that place is left out for the rest
//  of the run and the program goes on with the next one. (16 November 2026:
//  a place that could not be reached failed four times the same way, two
//  hours and a quarter, and the program stopped with the rest of the night
//  not used. And a new kind of place that does not work should not keep the
//  run from the other kinds.) The same for the way to a fishing rod, and
//  for the segment of a Pokemon that is there once.
const int STORY_PLACE_FAILURES = 2;
inline bool story_place_fails_out(int failures_in_a_row){
    return failures_in_a_row >= STORY_PLACE_FAILURES;
}

//  Fishing is the same at every place (the bag, the rod, the text a cast
//  ends with): with this many fishing places left out for failures, the
//  other fishing places of the run are left out too.
const int STORY_FISHING_PLACES_FAILED = 2;
inline bool story_fishing_off(int fishing_places_left_out){
    return fishing_places_left_out >= STORY_FISHING_PLACES_FAILED;
}

//  Places left out are for a run in which most places work. With more than
//  this many left out, something else is wrong (and each one costs two
//  failures): the next failure stops the program, as the failures of one
//  place did before. (All that build 32 added can fail and stay under it:
//  two ways to a rod, two fishing places, two places of laps, four segments
//  of Pokemon that are there once.)
const int STORY_MOST_PLACES_LEFT_OUT = 12;

//  The failures of a run by place.
struct StoryPlaceFailures{
    std::map<std::string, int> in_a_row;
    std::set<std::string> left_out;
    int fishing_left_out = 0;

    //  A trip to "place" (fished at or not) failed. Is the place left out
    //  by this failure?
    bool failed(const std::string& place, bool fishing){
        const int count = ++in_a_row[place];
        if (!story_place_fails_out(count) || !left_out.insert(place).second){
            return false;
        }
        if (fishing){
            fishing_left_out++;
        }
        return true;
    }
    //  A trip to "place" ended well.
    void went_well(const std::string& place){
        in_a_row.erase(place);
    }
    int failures(const std::string& place) const{
        auto iter = in_a_row.find(place);
        return iter == in_a_row.end() ? 0 : iter->second;
    }
    //  Left out for its own failures; and a place fished at, for those of
    //  fishing.
    bool place_out(const std::string& place) const{
        return left_out.count(place) != 0;
    }
    bool fishing_out() const{
        return story_fishing_off(fishing_left_out);
    }
    //  Too many places left out for the run to go on like this.
    bool too_many() const{
        return left_out.size() > (size_t)STORY_MOST_PLACES_LEFT_OUT;
    }
};


}
}
}
#endif

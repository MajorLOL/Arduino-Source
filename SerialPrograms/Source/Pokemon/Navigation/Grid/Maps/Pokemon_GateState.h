/*  Gate State
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which of the maps' gates (MapData::gates) are open: the card-key doors
 *  the player has opened, and the state of each switch. A switch is shared
 *  by every map that lists it as a floor (Pokemon Mansion's statue
 *  switch), and every gate that follows it is open or closed with it.
 *
 *  The state is plain data, with no map in it; the story keeps it
 *  (Pokemon_StoryCheckpoint.h). What it makes of a map's tiles is
 *  Pokemon_GateTiles.h, and the gated planner (Pokemon_GatedPlanner.h)
 *  searches over it.
 *
 */

#ifndef PokemonAutomation_Pokemon_GateState_H
#define PokemonAutomation_Pokemon_GateState_H

#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <string>

namespace PokemonAutomation{
namespace Pokemon{


//  Which gates are open and which switches are on.
struct GateState{
    //  The card-key gates opened, each as gate_key(map, gate id).
    std::set<std::string> opened = {};
    //  The switches (by id) that are on. One not here is off, as at a new game.
    std::set<std::string> switches_on = {};

    bool operator==(const GateState& other) const = default;

    bool switch_on(const std::string& id) const;
};

//  What a step does to a GateState: gates it opens, switches it sets.
struct GateChange{
    std::set<std::string> opened = {};
    //  The switch's new state, on or off.
    std::map<std::string, bool> switches = {};

    bool operator==(const GateChange& other) const = default;
};

//  Names a gate across maps: "SilphCo_5F/door_1".
std::string gate_key(const std::string& map, const std::string& gate_id);

//  Puts `change` on `state`: its gates are opened and its switches set.
void apply_gate_change(GateState& state, const GateChange& change);

//  The change that takes `before` to `after`: the gates `after` has opened
//  that `before` has not, and the switches whose state differs. A card-key
//  gate does not close again, so one `before` has opened and `after` has not
//  is ignored.
GateChange gate_change_between(const GateState& before, const GateState& after);



} // namespace Pokemon
} // namespace PokemonAutomation


//  A hash of GateState, for planners that keep it in the search state.
template <>
struct std::hash<PokemonAutomation::Pokemon::GateState>{
    size_t operator()(const PokemonAutomation::Pokemon::GateState& state) const;
};

#endif

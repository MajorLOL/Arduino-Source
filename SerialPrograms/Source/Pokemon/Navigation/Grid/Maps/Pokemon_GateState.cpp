/*  Gate State
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_GateState.h"

namespace PokemonAutomation{
namespace Pokemon{


bool GateState::switch_on(const std::string& id) const{
    return switches_on.contains(id);
}

std::string gate_key(const std::string& map, const std::string& gate_id){
    return map + "/" + gate_id;
}

void apply_gate_change(GateState& state, const GateChange& change){
    state.opened.insert(change.opened.begin(), change.opened.end());
    for (const auto& [id, on] : change.switches){
        if (on){
            state.switches_on.insert(id);
        }else{
            state.switches_on.erase(id);
        }
    }
}

GateChange gate_change_between(const GateState& before, const GateState& after){
    GateChange change;
    for (const std::string& key : after.opened){
        if (!before.opened.contains(key)){
            change.opened.insert(key);
        }
    }
    for (const std::string& id : after.switches_on){
        if (!before.switch_on(id)){
            change.switches[id] = true;
        }
    }
    for (const std::string& id : before.switches_on){
        if (!after.switch_on(id)){
            change.switches[id] = false;
        }
    }
    return change;
}



} // namespace Pokemon
} // namespace PokemonAutomation


size_t std::hash<PokemonAutomation::Pokemon::GateState>::operator()(const PokemonAutomation::Pokemon::GateState& state) const{
    size_t hash = 0;
    for (const std::string& key : state.opened){
        hash = hash * 31 + std::hash<std::string>()(key);
    }
    for (const std::string& id : state.switches_on){
        hash = hash * 37 + std::hash<std::string>()(id);
    }
    return hash;
}

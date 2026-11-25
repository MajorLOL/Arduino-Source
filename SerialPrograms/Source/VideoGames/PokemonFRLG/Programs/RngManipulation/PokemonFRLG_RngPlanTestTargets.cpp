/*  RNG Plan Test Targets
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_RngPlanTestTargets.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;


namespace RngPlanTestTargets{


std::string join(const std::vector<uint16_t>& values){
    std::string text;
    for (uint16_t value : values){
        text += (text.empty() ? "" : ",") + std::to_string(value);
    }
    return "{" + text + "}";
}


//  SIDs 100, 200 and 300 weigh 3, 2 and 1.
const std::vector<SidCandidate> CANDIDATES = {{100, 3}, {200, 2}, {300, 1}};

//  A target reached "seconds" after the reset, shiny for the candidates at
//  "shiny" (indices into CANDIDATES).
RngTarget make_target(uint32_t state, uint64_t seconds, std::vector<size_t> shiny){
    RngTarget target{};
    target.state = state;
    target.seed = {uint16_t(state), seconds * 1000, SeedButton::A, BlackoutButton::None};
    target.advances = 0;
    target.pokemon = method1_pokemon(state);
    for (size_t candidate : shiny){
        target.shiny_for.push_back({candidate, AdvShinyType::Star});
    }
    return target;
}

//  By expected wait per shiny over all three (weight 6): 1 = 50 s / (5/6) = 60,
//  2 = 20 s / (1/6) = 120, 3 = 40 s / (3/6) = 80. Then over SID 300 alone, 2 is
//  the only one shiny for it. 4 is the quickest and shiny for none.
const std::vector<RngTarget> TARGETS = {
    make_target(1, 50, {0, 1}),
    make_target(2, 20, {2}),
    make_target(3, 40, {0}),
    make_target(4, 5, {}),
};


}


}
}
}

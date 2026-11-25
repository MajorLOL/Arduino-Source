/*  RNG Plan Test Targets
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Made-up SID candidates and targets the RNG plan tests share.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_RngPlanTestTargets_H
#define PokemonAutomation_PokemonFRLG_RngPlanTestTargets_H

#include <cstdint>
#include <string>
#include <vector>
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngTargetSearch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace RngPlanTestTargets{


std::string join(const std::vector<uint16_t>& values);

//  SIDs 100, 200 and 300 weigh 3, 2 and 1.
extern const std::vector<SidCandidate> CANDIDATES;

//  A target reached "seconds" after the reset, shiny for the candidates at
//  "shiny" (indices into CANDIDATES).
RngTarget make_target(uint32_t state, uint64_t seconds, std::vector<size_t> shiny);

//  By expected wait per shiny over all three (weight 6): 1 = 50 s / (5/6) = 60,
//  2 = 20 s / (1/6) = 120, 3 = 40 s / (3/6) = 80. Then over SID 300 alone, 2 is
//  the only one shiny for it. 4 is the quickest and shiny for none.
extern const std::vector<RngTarget> TARGETS;


}


}
}
}
#endif

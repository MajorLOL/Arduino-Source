/*  RNG Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_BlindNavigation.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngCalibration.h"
#include "PokemonFRLG_RngPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;


std::vector<SidCandidate> sid_helper_candidates(uint16_t tid, uint32_t target_advances, uint8_t count){
    //  As SidHelper's get_sid_messages(): from "count / 2" steps before the target.
    const int start = -int(count) / 2;
    const int end = start + int(count);
    const int farthest = std::max(-start, end - 1);

    std::vector<SidCandidate> candidates;
    AdvRngSearcher searcher(tid, uint64_t(int64_t(target_advances) + 2 * start));
    for (int i = start; i < end; i++){
        candidates.push_back({uint16_t(searcher.state.s0 >> 16), double(farthest + 1 - std::abs(i))});
        searcher.advance_state();
        searcher.advance_state();
    }
    return candidates;
}


namespace{

//  "targets" with only the shiny candidates still "left", and their share of
//  the weight left.
std::vector<RngTarget> rescore(
    const std::vector<RngTarget>& targets,
    const std::vector<SidCandidate>& candidates,
    const std::vector<bool>& left
){
    double total = 0;
    for (size_t c = 0; c < candidates.size(); c++){
        if (left[c]){
            total += candidates[c].weight;
        }
    }
    std::vector<RngTarget> scored;
    scored.reserve(targets.size());
    for (const RngTarget& target : targets){
        RngTarget copy = target;
        copy.shiny_for.clear();
        double weight = 0;
        for (const ShinyCandidate& shiny : target.shiny_for){
            if (left[shiny.candidate]){
                copy.shiny_for.push_back(shiny);
                weight += candidates[shiny.candidate].weight;
            }
        }
        copy.shiny_probability = total > 0 ? weight / total : 0;
        scored.emplace_back(std::move(copy));
    }
    return scored;
}

RngAttempt make_attempt(const RngTarget& target, const std::vector<SidCandidate>& candidates){
    RngAttempt attempt{target, {}};
    for (const ShinyCandidate& shiny : target.shiny_for){
        attempt.sids.push_back(candidates[shiny.candidate].sid);
    }
    return attempt;
}

}


std::vector<RngAttempt> plan_rng_attempts(
    const std::vector<RngTarget>& targets,
    const std::vector<SidCandidate>& candidates,
    bool shiny_required,
    size_t max_attempts
){
    std::vector<RngAttempt> attempts;
    if (targets.empty() || max_attempts == 0){
        return attempts;
    }
    std::vector<bool> left(candidates.size(), true);

    if (!shiny_required){
        //  With no shiny chance, rank_rng_targets() sorts by time alone.
        std::vector<RngTarget> by_time = targets;
        for (RngTarget& target : by_time){
            target.shiny_probability = 0;
        }
        rank_rng_targets(by_time);
        const uint32_t quickest = by_time.front().state;
        for (const RngTarget& target : rescore(targets, candidates, left)){
            if (target.state == quickest){
                attempts.push_back(make_attempt(target, candidates));
                break;
            }
        }
        return attempts;
    }

    while (attempts.size() < max_attempts){
        std::vector<RngTarget> scored = rescore(targets, candidates, left);
        rank_rng_targets(scored);
        const RngTarget& best = scored.front();
        if (best.shiny_for.empty()){
            break;
        }
        for (const ShinyCandidate& shiny : best.shiny_for){
            left[shiny.candidate] = false;
        }
        attempts.push_back(make_attempt(best, candidates));
        if (std::find(left.begin(), left.end(), true) == left.end()){
            break;
        }
    }
    return attempts;
}


int64_t starter_rng_seed_wait_ms(const InitialSeedEntry& seed){
    return int64_t(seed.seed_time_ms) + STARTER_RNG_FIXED_SEED_OFFSET_MS;
}

bool starter_rng_can_wait_for(const InitialSeedEntry& seed){
    return starter_rng_seed_wait_ms(seed) >= STARTER_RNG_MIN_SEED_WAIT_MS;
}

int64_t starter_rng_ingame_ms(uint64_t advances){
    const int64_t frames = int64_t(advances) - int64_t(STARTER_RNG_CONTINUE_SCREEN_FRAMES)
        + fixed_advances_offset(PokemonFRLG_RngTarget::starters);
    return int64_t(frames * FRLG_FRAME_DURATION / 2);
}

std::vector<RngAttempt> plan_starter_rng(const StarterRngPlanInputs& inputs){
    const SeedsDatabase database(seeds_json_path(inputs.firered, inputs.language));
    //  Only the English lists hold the blackout button's seeds (initial_seed_entries()).
    const bool english_list = inputs.language != Language::Japanese;
    std::vector<InitialSeedEntry> seeds = initial_seed_entries(database, inputs.sound, "Help", english_list);
    std::erase_if(seeds, [](const InitialSeedEntry& seed){ return !starter_rng_can_wait_for(seed); });

    RngTargetFilters filters = inputs.filters;
    filters.min_advances = std::max(filters.min_advances, STARTER_RNG_PLAN_MIN_ADVANCES);
    const std::vector<RngTarget> targets = search_rng_targets(seeds, inputs.tid, inputs.candidates, filters);
    return plan_rng_attempts(targets, inputs.candidates, filters.shiny_required, inputs.max_attempts);
}


}
}
}

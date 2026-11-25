/*  RNG Plan Tracker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The step after each attempt of an RNG plan (PokemonFRLG_RngPlan.h),
 *  from what came out of it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_RngPlanTracker_H
#define PokemonAutomation_PokemonFRLG_RngPlanTracker_H

#include <cstdint>
#include <optional>
#include <vector>
#include "Pokemon/Pokemon_AdvRng.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What came out of one attempt.
enum class RngAttemptOutcome{
    //  Shiny, with the target's nature and IVs.
    SHINY,
    //  Shiny, but not the target, or not known to be.
    SHINY_OFF_TARGET,
    //  The target's nature and IVs, not shiny.
    HIT_NOT_SHINY,
    //  Another nature or other IVs: not the target.
    MISSED,
    //  The attempt couldn't run (Starter RNG refused its timings). It rules
    //  nothing out; the next attempt runs instead.
    UNUSABLE,
};

struct RngAttemptResult{
    bool shiny;
    Pokemon::AdvNature nature;
    Pokemon::AdvIVs ivs;
};
RngAttemptOutcome classify_attempt(const RngTarget& target, const RngAttemptResult& result);


enum class RngPlanStep{
    //  Run current_attempt(): the first, the next, or the same again after a miss.
    RUN_ATTEMPT,
    //  Shiny, the target hit. remaining_sids() are the SIDs it can be.
    GOT_SHINY,
    //  Shiny, not the target: the SIDs are as they were, none confirmed.
    GOT_SHINY_OFF_TARGET,
    //  Shiny not required, and the target was hit.
    GOT_TARGET,
    //  Every attempt was hit and none was shiny.
    OUT_OF_ATTEMPTS,
    //  The last attempt still shiny for a SID left was missed more often
    //  than the retries allow, or couldn't run. (Missing an earlier one moves
    //  on to the next.)
    MISSED_EVERY_ATTEMPT_LEFT,
};

//  Follows the plan through the attempts' outcomes.
class RngPlanTracker{
public:
    RngPlanTracker(
        std::vector<RngAttempt> attempts,
        const std::vector<SidCandidate>& candidates,
        bool shiny_required,
        size_t max_retries
    );

    RngPlanStep step() const{ return m_step; }
    size_t attempt_index() const{ return m_attempt; }
    size_t attempt_count() const{ return m_attempts.size(); }
    const RngAttempt& current_attempt() const{ return m_attempts[m_attempt]; }
    //  The SID candidates not ruled out, in the order given.
    const std::vector<uint16_t>& remaining_sids() const{ return m_remaining; }
    //  The SID, once a shiny leaves one candidate.
    std::optional<uint16_t> confirmed_sid() const;

    //  The next step after the current attempt came out as "outcome". Once
    //  the plan has ended, the step stays as it is.
    RngPlanStep record(RngAttemptOutcome outcome);

private:
    bool is_ruled_out(uint16_t sid) const;
    //  Moves to the next attempt still shiny for a SID left (with shiny
    //  required; else any next one). False if there is none.
    bool move_to_next_attempt();

    std::vector<RngAttempt> m_attempts;
    bool m_shiny_required;
    size_t m_max_retries;
    std::vector<uint16_t> m_remaining;
    size_t m_attempt = 0;
    size_t m_retries = 0;
    RngPlanStep m_step;
};


//  What the loop does after an attempt's result.
enum class RngPlanNext{
    RETRY,
    NEXT_ATTEMPT,
    //  Shiny and the target: go on with the story.
    CONFIRM,
    STOP,
};
//  The step after `index` came out as the tracker's `step` and its attempt
//  is now `next_index`.
RngPlanNext rng_plan_next(RngPlanStep step, size_t index, size_t next_index);


}
}
}
#endif

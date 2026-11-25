/*  RNG Plan Tracker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_RngPlanTracker.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;


RngAttemptOutcome classify_attempt(const RngTarget& target, const RngAttemptResult& result){
    const AdvIVs& want = target.pokemon.ivs;
    const AdvIVs& got = result.ivs;
    const bool same_ivs = want.hp == got.hp && want.attack == got.attack && want.defense == got.defense
        && want.spatk == got.spatk && want.spdef == got.spdef && want.speed == got.speed;
    const bool hit = result.nature == target.pokemon.nature && same_ivs;
    if (result.shiny){
        return hit ? RngAttemptOutcome::SHINY : RngAttemptOutcome::SHINY_OFF_TARGET;
    }
    return hit ? RngAttemptOutcome::HIT_NOT_SHINY : RngAttemptOutcome::MISSED;
}


RngPlanTracker::RngPlanTracker(
    std::vector<RngAttempt> attempts,
    const std::vector<SidCandidate>& candidates,
    bool shiny_required,
    size_t max_retries
)
    : m_attempts(std::move(attempts))
    , m_shiny_required(shiny_required)
    , m_max_retries(max_retries)
    , m_step(m_attempts.empty() ? RngPlanStep::OUT_OF_ATTEMPTS : RngPlanStep::RUN_ATTEMPT)
{
    for (const SidCandidate& candidate : candidates){
        m_remaining.push_back(candidate.sid);
    }
}

std::optional<uint16_t> RngPlanTracker::confirmed_sid() const{
    if (m_step == RngPlanStep::GOT_SHINY && m_remaining.size() == 1){
        return m_remaining.front();
    }
    return std::nullopt;
}

bool RngPlanTracker::is_ruled_out(uint16_t sid) const{
    return std::find(m_remaining.begin(), m_remaining.end(), sid) == m_remaining.end();
}

bool RngPlanTracker::move_to_next_attempt(){
    for (size_t next = m_attempt + 1; next < m_attempts.size(); next++){
        const std::vector<uint16_t>& next_sids = m_attempts[next].sids;
        if (!m_shiny_required || !std::all_of(next_sids.begin(), next_sids.end(), [&](uint16_t sid){ return is_ruled_out(sid); })){
            m_attempt = next;
            m_retries = 0;
            return true;
        }
    }
    return false;
}

RngPlanStep RngPlanTracker::record(RngAttemptOutcome outcome){
    if (m_step != RngPlanStep::RUN_ATTEMPT){
        return m_step;
    }
    const std::vector<uint16_t>& sids = m_attempts[m_attempt].sids;
    switch (outcome){
    case RngAttemptOutcome::SHINY:
        //  The SID is one the target is shiny for. None of them: it wasn't
        //  among the candidates.
        std::erase_if(m_remaining, [&](uint16_t sid){ return std::find(sids.begin(), sids.end(), sid) == sids.end(); });
        m_step = RngPlanStep::GOT_SHINY;
        return m_step;
    case RngAttemptOutcome::SHINY_OFF_TARGET:
        m_step = RngPlanStep::GOT_SHINY_OFF_TARGET;
        return m_step;
    case RngAttemptOutcome::HIT_NOT_SHINY:
        if (!m_shiny_required){
            m_step = RngPlanStep::GOT_TARGET;
            return m_step;
        }
        m_remaining.erase(
            std::remove_if(m_remaining.begin(), m_remaining.end(), [&](uint16_t sid){
                return std::find(sids.begin(), sids.end(), sid) != sids.end();
            }),
            m_remaining.end()
        );
        if (!move_to_next_attempt()){
            m_step = RngPlanStep::OUT_OF_ATTEMPTS;
        }
        return m_step;
    case RngAttemptOutcome::MISSED:
        //  A miss rules nothing out. Too many: try the next attempt instead.
        if (++m_retries <= m_max_retries){
            return m_step;
        }
        [[fallthrough]];
    case RngAttemptOutcome::UNUSABLE:
        if (!move_to_next_attempt()){
            m_step = RngPlanStep::MISSED_EVERY_ATTEMPT_LEFT;
        }
        return m_step;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown RngAttemptOutcome.");
}


RngPlanNext rng_plan_next(RngPlanStep step, size_t index, size_t next_index){
    switch (step){
    case RngPlanStep::RUN_ATTEMPT:
        return next_index == index ? RngPlanNext::RETRY : RngPlanNext::NEXT_ATTEMPT;
    case RngPlanStep::GOT_SHINY:
        return RngPlanNext::CONFIRM;
    case RngPlanStep::GOT_SHINY_OFF_TARGET:
    case RngPlanStep::GOT_TARGET:
    case RngPlanStep::OUT_OF_ATTEMPTS:
    case RngPlanStep::MISSED_EVERY_ATTEMPT_LEFT:
        return RngPlanNext::STOP;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown RngPlanStep.");
}


}
}
}

/*  PokemonLZA AutoStory - Catch plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include "PokemonLZA_AutoStoryCatchPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

std::string_view LockTarget::caught_text() const{
    if (!caught){
        return "caught mark unread";
    }
    return *caught ? "caught" : "not caught";
}

namespace{

//  Nearer Pokemon stand lower on the screen, and their plates with them:
//  first-throw catches had plates at 0.49 and 0.50, a Mareep missed 11
//  times at about 15 m at 0.40. Plate width and bracket height did not
//  tell them apart.
constexpr double NEAR_PLATE_Y = 0.45;

constexpr int STEPS_BEFORE_FIRST_THROW = 4;
constexpr int STEPS_AFTER_MISS = 2;

} // namespace

bool step_toward_target(const LockTarget& target, int misses, int steps){
    if (target.y >= NEAR_PLATE_Y){
        return false;
    }
    return steps < (misses == 0 ? STEPS_BEFORE_FIRST_THROW : STEPS_AFTER_MISS);
}

bool same_lock_target(const LockTarget& a, const LockTarget& b){
    return std::abs(a.x - b.x) <= 0.03 && std::abs(a.y - b.y) <= 0.03;
}

std::optional<size_t> nearest_uncaught(const std::vector<LockTarget>& targets){
    std::optional<size_t> best;
    for (size_t i = 0; i < targets.size(); i++){
        if (targets[i].caught == true){
            continue;
        }
        if (!best || targets[i].y > targets[*best].y){
            best = i;
        }
    }
    return best;
}


LockTargetCycle::LockTargetCycle(size_t max_views)
    : m_max_views(max_views)
{}

bool LockTargetCycle::add(const LockTarget& target){
    if (m_done){
        return false;
    }
    if (!m_targets.empty() && same_lock_target(target, m_targets.back())){
        //  The flick found nothing further that way.
        m_done = true;
        return false;
    }
    if (m_targets.size() > 1 && same_lock_target(target, m_targets.front())){
        //  Round to the first again.
        m_current = 0;
        m_done = true;
        return false;
    }
    m_targets.push_back(target);
    m_current = m_targets.size() - 1;
    m_done = m_targets.size() >= m_max_views;
    return !m_done;
}

int LockTargetCycle::flicks_to(size_t index) const{
    return (int)index - (int)m_current;
}


std::optional<int> settled_party_slots(MoveOnlyFunction<std::optional<int>()> read_look){
    //  A catch's new member joins the bar seconds after its banner: 03.08 read
    //  4 of 5 slots and failed, and its retry at once read 5 (console B,
    //  2026-11-18). Hold one count for 3 s; none if unread in 2 s; cap at 8 s.
    //  Looks are 250 ms apart.
    constexpr int FIRST_READ_LOOKS = 8;
    constexpr int HELD_LOOKS = 13;
    constexpr int MAX_LOOKS = 32;
    std::optional<int> count;
    int held = 0;
    for (int look = 0; look < MAX_LOOKS; look++){
        const std::optional<int> now = read_look();
        if (!now){
            if (!count && look + 1 >= FIRST_READ_LOOKS){
                return std::nullopt;
            }
            continue;
        }
        held = now == count ? held + 1 : 1;
        count = now;
        if (held >= HELD_LOOKS){
            return count;
        }
    }
    return count;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

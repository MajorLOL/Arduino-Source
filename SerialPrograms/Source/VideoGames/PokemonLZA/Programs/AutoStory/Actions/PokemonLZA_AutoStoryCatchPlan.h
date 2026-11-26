/*  PokemonLZA AutoStory - Catch plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure target selection, lock-on cycling, and party-bar settling for
 *  Wild Zone 1 catches.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatchPlan_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatchPlan_H

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Locked-on wild Pokemon name-plate geometry and caught-mark reading.
struct LockTarget{
    double x = 0;
    double y = 0;
    std::optional<bool> caught;

    std::string_view caught_text() const;
};

//  True when the player should step closer to `target` given prior `misses` and `steps`.
bool step_toward_target(const LockTarget& target, int misses, int steps);

//  True when two lock-on plates refer to the same target within position tolerance.
bool same_lock_target(const LockTarget& a, const LockTarget& b);

//  Index of the nearest uncaught target in `targets`, or std::nullopt if all are caught.
std::optional<size_t> nearest_uncaught(const std::vector<LockTarget>& targets);

//  Records targets seen while cycling right under ZL until wrap-around or `max_views`.
class LockTargetCycle{
public:
    explicit LockTargetCycle(size_t max_views);

    //  Records `target`; returns true if cycling should continue.
    bool add(const LockTarget& target);

    const std::vector<LockTarget>& targets() const{
        return m_targets;
    }

    //  Signed flick count from current target to `index` (positive = right).
    int flicks_to(size_t index) const;

private:
    size_t m_max_views;
    std::vector<LockTarget> m_targets;
    size_t m_current = 0;
    bool m_done = false;
};

//  Polls `read_look` until the party bar's filled-slot count settles, or returns std::nullopt if unread.
std::optional<int> settled_party_slots(MoveOnlyFunction<std::optional<int>()> read_look);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

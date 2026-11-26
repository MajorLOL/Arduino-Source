/*  Screen Wait
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Polling loops over a StoryBackend with cancellation, HOME-menu failure,
 *  confirmation streaks/holds, and wall-clock plus look-count bounds.
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenWait_H
#define PokemonAutomation_Pokemon_ScreenWait_H

#include <chrono>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenTimers.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Timing, confirmation, and HOME-menu policy for wait_for() and poll_until().
struct WaitRule{
    Milliseconds poll = Milliseconds(250);
    Milliseconds timeout = Seconds(30);
    size_t confirm_looks = 1;
    Milliseconds confirm_hold = Milliseconds(0);
    bool fail_on_home = true;
};

//  The predicate was confirmed before timing out.
struct WaitMatched{
    ScreenObservation look = {};
    size_t looks = 0;
    Milliseconds elapsed = Milliseconds(0);
};

//  The timeout or maximum look count elapsed before confirmation.
struct WaitTimeout{
    ScreenObservation last_look = {};
    size_t looks = 0;
    Milliseconds elapsed = Milliseconds(0);
};

//  Outcome of wait_for() and poll_until(): matched, timed out, or failed.
using WaitOutcome = std::variant<WaitMatched, WaitTimeout, StoryFailure>;

namespace detail{

template <typename OnLook>
void invoke_on_look(OnLook&& on_look, const ScreenObservation& look){
    if constexpr (!std::is_same_v<std::decay_t<OnLook>, std::nullptr_t>){
        if constexpr (std::is_convertible_v<OnLook, bool>){
            if (on_look){
                on_look(look);
            }
        }else{
            on_look(look);
        }
    }
}

} // namespace detail

//  Observes `backend` until `done(look)` is confirmed per `rule`, calling
//  `on_not_done(look)` after each unconfirmed look and `on_look(look)` on every
//  look when provided.
template <typename Predicate, typename Act, typename OnLook = std::nullptr_t>
WaitOutcome poll_until(
    StoryBackend& backend,
    Predicate&& done,
    WaitRule rule,
    Act&& on_not_done,
    OnLook&& on_look = nullptr
){
    const size_t max_looks = (rule.timeout > Milliseconds(0) && rule.poll > Milliseconds(0))
        ? static_cast<size_t>(rule.timeout / rule.poll) + 1
        : 1;
    ConfirmStreak streak(rule.confirm_looks);
    HoldTimer hold(rule.confirm_hold);
    std::optional<WallClock> start = std::nullopt;
    size_t looks = 0;

    while (true){
        backend.scope().throw_if_cancelled();
        ScreenObservation look = backend.observe();
        looks++;
        if (!start.has_value()){
            start = look.when;
        }
        const Milliseconds elapsed = look.when > *start
            ? std::chrono::duration_cast<Milliseconds>(look.when - *start)
            : Milliseconds(0);

        detail::invoke_on_look(on_look, look);

        if (rule.fail_on_home && look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            return game_not_running_failure();
        }

        const bool matched = static_cast<bool>(done(look));
        const bool streak_ok = streak.update(matched);
        const bool hold_ok = rule.confirm_hold > Milliseconds(0) ? hold.update(matched, look.when) : matched;
        if (streak_ok && hold_ok){
            return WaitMatched{.look = std::move(look), .looks = looks, .elapsed = elapsed};
        }
        if (elapsed >= rule.timeout || looks >= max_looks){
            return WaitTimeout{.last_look = std::move(look), .looks = looks, .elapsed = elapsed};
        }

        on_not_done(look);
    }
}

//  Observes `backend` until `predicate(look)` is confirmed per `rule`, waiting
//  `rule.poll` between unconfirmed looks.
template <typename Predicate, typename OnLook = std::nullptr_t>
WaitOutcome wait_for(
    StoryBackend& backend,
    Predicate&& predicate,
    WaitRule rule = {},
    OnLook&& on_look = nullptr
){
    return poll_until(
        backend,
        std::forward<Predicate>(predicate),
        rule,
        [&](const ScreenObservation&){
            backend.wait(rule.poll);
        },
        std::forward<OnLook>(on_look)
    );
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

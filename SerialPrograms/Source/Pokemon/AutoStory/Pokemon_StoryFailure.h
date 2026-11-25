/*  Story Failure
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Why a story action, check or checkpoint failed.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryFailure_H
#define PokemonAutomation_Pokemon_StoryFailure_H

#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace PokemonAutomation{
namespace Pokemon{


//  A failed action, check or checkpoint; std::nullopt in its place is success.
struct StoryFailure{
    //  A name the game owns, which recoveries match.
    std::string code = {};
    std::string detail = {};
    //  Another attempt would fail the same way, so the engine does not reload.
    bool repeats = false;
    //  The checkpoint goes on at its next step with this action, when it has
    //  one, instead of failing (a trainer's battle on screen before the plan's
    //  walk to them: on to the battle step).
    std::optional<std::string> resume_at_action = std::nullopt;
};

//  The code of a step that found the Switch's HOME menu instead of the game.
inline constexpr std::string_view GAME_NOT_RUNNING = "game_not_running";
//  GAME_NOT_RUNNING: the run stops, with no reload.
inline StoryFailure game_not_running_failure(){
    return StoryFailure{
        .code = std::string(GAME_NOT_RUNNING),
        .detail = "the game is not running: the Switch HOME menu is showing",
        .repeats = true,
    };
}



} // namespace Pokemon
} // namespace PokemonAutomation


//  "probe_exhausted: 4 rows", or the code alone when there is no detail.
template <>
struct std::formatter<PokemonAutomation::Pokemon::StoryFailure> : std::formatter<std::string_view>{
    auto format(const PokemonAutomation::Pokemon::StoryFailure& failure, std::format_context& context) const{
        if (failure.detail.empty()){
            return std::format_to(context.out(), "{}", failure.code);
        }
        return std::format_to(context.out(), "{}: {}", failure.code, failure.detail);
    }
};

#endif

/*  Grid Story Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_GridStoryActions.h"

namespace PokemonAutomation{
namespace Pokemon{


StoryFailure grid_moves_unsupported(std::string_view step){
    return StoryFailure{
        .code = "unsupported_step",
        .detail = std::format("{}: backend has no tile moves", step),
        .repeats = true,
    };
}

GridLegsArgs parse_grid_legs_args(const JsonObject& args){
    reject_unknown_keys(args, {"legs", "run"});
    GridLegsArgs result;
    result.legs = read_story_legs(args.get_value_throw("legs"), "legs");
    if (const JsonValue* run = args.get_value("run")){
        result.pace = run->to_boolean_throw("run") ? StoryPace::RUN : StoryPace::WALK;
    }
    return result;
}

Direction parse_grid_face_args(const JsonObject& args){
    reject_unknown_keys(args, {"direction"});
    return read_direction(args.get_string_throw("direction"), "direction");
}



} // namespace Pokemon
} // namespace PokemonAutomation

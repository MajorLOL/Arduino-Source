/*  Grid Story Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story actions every tile game (FRLG, BDSP) shares: "legs", straight
 *  legs walked as given, and "face", a turn or a single step. Each parser
 *  reads its args at plan load (Pokemon_GridStoryArgs.h) and returns a
 *  step run that moves the player through the game's backend, which adds
 *  move_leg() and face() to StoryBackend (GridMoveBackend): how a tile is
 *  crossed (a D-pad tap, a held stick) and how sure the pose stays is the
 *  game's.
 *
 */

#ifndef PokemonAutomation_Pokemon_GridStoryActions_H
#define PokemonAutomation_Pokemon_GridStoryActions_H

#include <concepts>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_Direction.h"
#include "Pokemon_GridStoryArgs.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A tile game's backend: a StoryBackend that also walks `tiles` tiles in a
//  straight line and turns in place.
template <typename Backend>
concept GridMoveBackend = std::derived_from<Backend, StoryBackend>
    && requires(Backend& backend, Direction direction, int tiles, StoryPace pace){
        backend.move_leg(direction, tiles, pace);
        backend.face(direction);
    };

//  The failure of a grid action run on a backend that has no tile moves.
StoryFailure grid_moves_unsupported(std::string_view step);

//  Parsed args for the "legs" action.
struct GridLegsArgs{
    std::vector<StoryLeg> legs = {};
    StoryPace pace = StoryPace::WALK;
};

//  Parses "legs": {"legs": [{"direction": "north", "tiles": 3}, ...],
//  "run": false}.
GridLegsArgs parse_grid_legs_args(const JsonObject& args);

//  Parses "face": {"direction": "north"}.
Direction parse_grid_face_args(const JsonObject& args);

//  Action parser for "legs" on `Backend`.
template <GridMoveBackend Backend>
StoryActionParser grid_legs_action();

//  Action parser for "face" on `Backend`.
template <GridMoveBackend Backend>
StoryActionParser grid_face_action();

//  Registers "legs" and "face" on `actions` for `Backend`. A step run on a
//  backend of another type fails as "unsupported_step" before any input.
template <GridMoveBackend Backend>
void add_grid_story_actions(StoryActions& actions);



template <GridMoveBackend Backend>
StoryActionParser grid_legs_action(){
    return [](const JsonObject& args) -> StoryStepRun{
        GridLegsArgs parsed = parse_grid_legs_args(args);
        return [parsed = std::move(parsed)](StoryBackend& backend, RunState& state) -> std::optional<StoryFailure>{
            Backend* grid = dynamic_cast<Backend*>(&backend);
            if (grid == nullptr){
                return grid_moves_unsupported(state.step);
            }
            for (const StoryLeg& leg : parsed.legs){
                backend.scope().throw_if_cancelled();
                grid->move_leg(leg.direction, leg.tiles, parsed.pace);
            }
            return std::nullopt;
        };
    };
}

template <GridMoveBackend Backend>
StoryActionParser grid_face_action(){
    return [](const JsonObject& args) -> StoryStepRun{
        const Direction direction = parse_grid_face_args(args);
        return [direction](StoryBackend& backend, RunState& state) -> std::optional<StoryFailure>{
            Backend* grid = dynamic_cast<Backend*>(&backend);
            if (grid == nullptr){
                return grid_moves_unsupported(state.step);
            }
            grid->face(direction);
            return std::nullopt;
        };
    };
}

template <GridMoveBackend Backend>
void add_grid_story_actions(StoryActions& actions){
    actions.emplace("legs", grid_legs_action<Backend>());
    actions.emplace("face", grid_face_action<Backend>());
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

/*  Story Step
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One step of a checkpoint: an action of the game's, named by the plan,
 *  with its args bound, ready to run. A checkpoint (StoryCheckpoint) is the
 *  steps between two saves; the engine runs them in order, and the first
 *  step that fails fails the checkpoint.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStep_H
#define PokemonAutomation_Pokemon_StoryStep_H

#include <functional>
#include <optional>
#include <set>
#include <string>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a run knows; a reload empties it.
struct RunState{
    //  The step under way, "<checkpoint>/<step>", for its log lines.
    std::string step = {};
    //  The actions of the checkpoint's steps after the one under way.
    std::set<std::string, std::less<>> actions_ahead = {};
};

//  A step's action with its args bound.
using StoryStepRun = MoveOnlyFunction<std::optional<StoryFailure>(StoryBackend& backend, RunState& state)>;

struct StoryStep{
    std::string name = {};
    std::string why = {};
    //  The plan's "action": what kind of step it is.
    std::string action = {};
    StoryStepRun run = {};
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

/*  Story Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Runs a range of story checkpoints on a game's backend.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEngine_H
#define PokemonAutomation_Pokemon_StoryEngine_H

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon_StoryBackend.h"
#include "Pokemon_StoryGame.h"

namespace PokemonAutomation{
namespace Pokemon{


struct CheckpointFailure{
    std::string checkpoint_id = {};
    //  0-based; none when the checkpoint failed outside its steps (a
    //  reload).
    std::optional<size_t> event_index = std::nullopt;
    StoryFailure failure = {};
};

//  Runs `checkpoints` in order from `state`. Returns std::nullopt when every
//  checkpoint passed, else the failure that ended the run.
//  After a failure that does not repeat, game.reload says which checkpoint's
//  start the game is at and the run goes on from there, until a checkpoint
//  fails game.attempts_per_checkpoint times. A step that throws is logged and
//  the exception passes through.
std::optional<CheckpointFailure> run_checkpoints(
    StoryBackend& backend, std::span<StoryCheckpoint> checkpoints,
    StoryGame& game, RunState& state
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

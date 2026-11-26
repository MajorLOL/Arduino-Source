/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA StoryGame factory and chained checkpoint runner.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryGame_H

#include <optional>
#include <set>
#include <span>
#include <string>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryPlan.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Builds a StoryGame using autosave_policy() over `sightings` (which must outlive it), with no reload hook.
Pokemon::StoryGame plza_story_game(const MarkedCheckpoints& marked, AutosaveSightings& sightings);

//  Reloads the latest save for `checkpoint`.
//  Fails as "no_reload" if `game` has no reload hook, or "reload_failed" if reload does not return a start.
std::optional<Pokemon::StoryFailure> load_checkpoint_start(
    Pokemon::StoryGame& game, PlzaBackend& backend, const Pokemon::StoryCheckpoint& checkpoint
);

//  Reads the live screen for the fields specified by `start`.
using StartReader = MoveOnlyFunction<StartSignature(const StartSignature& start)>;

//  Options controlling retries, chained reloads, and start-signature verification.
struct CheckpointRunOptions{
    size_t attempts_per_checkpoint = 1;
    std::set<std::string, std::less<>> reload_when_chained = {};
    std::span<const CheckpointStart> starts = {};
    StartReader read_start = {};
};

//  Runs `checkpoints` in order; returns std::nullopt when all pass.
//  Between checkpoints, may fail with chain codes "reload_off_start", "reload_unchecked", or "chain_off_start".
std::optional<Pokemon::CheckpointFailure> run_plza_checkpoints(
    Pokemon::StoryGame& game, std::span<Pokemon::StoryCheckpoint> checkpoints, CheckpointRunOptions& options,
    PlzaBackend& backend, Pokemon::RunState& state, AutosaveSightings& sightings
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

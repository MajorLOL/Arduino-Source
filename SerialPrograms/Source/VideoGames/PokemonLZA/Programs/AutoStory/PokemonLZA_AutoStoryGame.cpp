/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryWalks.h"
#include "PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryPlan.h"
#include "PokemonLZA_AutoStoryGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  01.01's first step waits for the platform; a new game has no save to check.
bool first_step_checks_start(const Pokemon::StoryCheckpoint& checkpoint){
    return checkpoint_first_action(checkpoint) == action_name(PlzaAction::WAIT_FOR_PLATFORM) || checkpoint_starts_without_save(checkpoint);
}


//  "[Boundary] <id> start: <pinned>": what pins the start ("pinned by
//  minimap", "unchecked"), the evidence for which start checks a run needs.
void log_start_boundary(PlzaBackend& backend, std::string_view id, std::string_view pinned){
    backend.log(std::format("[Boundary] {} start: {}", id, pinned));
}

} // namespace


Pokemon::StoryGame plza_story_game(const MarkedCheckpoints& marked, AutosaveSightings& sightings){
    return Pokemon::StoryGame{.save = autosave_policy(marked, sightings)};
}

std::optional<Pokemon::StoryFailure> load_checkpoint_start(Pokemon::StoryGame& game, PlzaBackend& backend, const Pokemon::StoryCheckpoint& checkpoint){
    if (!game.reload){
        return Pokemon::StoryFailure{.code = "no_reload", .detail = "this game has no reload"};
    }
    backend.log(std::format("[Save] {}: loading the latest save", checkpoint.id));
    if (!game.reload(backend, checkpoint)){
        return Pokemon::StoryFailure{.code = "reload_failed", .detail = "the reload is at no checkpoint's start"};
    }
    return std::nullopt;
}

namespace{

//  What run_plza_checkpoints()'s steps between two checkpoints (the chain)
//  share. These steps serve only that run: split up to keep each short.
struct CheckpointRun{
    Pokemon::StoryGame& game;
    PlzaBackend& backend;
    CheckpointRunOptions& options;
    AutosaveSightings& sightings;
};

std::optional<Pokemon::StoryFailure> check_live_start(CheckpointRun& run, const Pokemon::StoryCheckpoint& checkpoint, bool seen){
    PlzaBackend& backend = run.backend;
    CheckpointRunOptions& options = run.options;
    const CheckpointStart* start = find_start(options.starts, checkpoint.id);
    if (start == nullptr || !options.read_start){
        backend.log(std::format(
            "[Checkpoint] {}: START UNCHECKED: {}; on from the live screen (autosave before it {})",
            checkpoint.id, start == nullptr ? "its plan names no start signature" : "nothing reads the screen", seen ? "seen" : "not seen"
        ));
        log_start_boundary(backend, checkpoint.id, seen ? "pinned by sighting" : "unchecked");
        return std::nullopt;
    }
    const StartSignature read = options.read_start(start->signature);
    const std::string screen = std::format("the screen reads ({}), its start is ({})", to_string(read), to_string(start->signature));
    if (start_confirmed(start->signature, read)){
        backend.log(std::format("[Checkpoint] {}: START CHECKED: {}; on from the live screen", checkpoint.id, screen));
        log_start_boundary(backend, checkpoint.id, "pinned by minimap");
        return std::nullopt;
    }
    backend.log(std::format("[Checkpoint] {}: START CHECK FAILED: {}; nothing run from there", checkpoint.id, screen));
    return Pokemon::StoryFailure{.code = "chain_off_start", .detail = std::format("the run is not at {}'s start: {}", checkpoint.id, screen)};
}

//  `seen`: whether the checkpoint before's autosave showed.
std::optional<Pokemon::StoryFailure> check_chain_start(CheckpointRun& run, const Pokemon::StoryCheckpoint& checkpoint, bool seen){
    if (first_step_checks_start(checkpoint)){
        run.backend.log(std::format("[Checkpoint] {}: start check passed; on from the live screen", checkpoint.id));
        log_start_boundary(run.backend, checkpoint.id, "pinned by its first step");
        return std::nullopt;
    }
    return check_live_start(run, checkpoint, seen);
}

std::optional<Pokemon::StoryFailure> reload_into(
    CheckpointRun& run, const Pokemon::StoryCheckpoint& previous, const Pokemon::StoryCheckpoint& checkpoint
){
    PlzaBackend& backend = run.backend;
    CheckpointRunOptions& options = run.options;
    backend.log(std::format(
        "[Checkpoint] {}: reloads its start save when chained, whether or not {}'s autosave showed at its end ({})",
        checkpoint.id, previous.id, run.sightings.last_end_seen() ? "seen" : "not seen"
    ));
    const std::optional<Pokemon::StoryFailure> control = check_can_walk(backend, std::format("the reload into {}", checkpoint.id));
    if (control){
        backend.log(std::format("[Checkpoint] {}: control never came back after {}: no reload", checkpoint.id, previous.id));
        return control;
    }
    backend.log(std::format("[Checkpoint] {}: control is back after {}'s last scene; reloading the latest save", checkpoint.id, previous.id));
    const std::optional<Pokemon::StoryFailure> loaded = load_checkpoint_start(run.game, backend, checkpoint);
    if (loaded){
        return loaded;
    }
    const CheckpointStart* start = find_start(options.starts, checkpoint.id);
    if (start == nullptr || !options.read_start){
        const std::string why = start == nullptr ? "its plan names no start signature" : "nothing reads the screen";
        backend.log(std::format("[Checkpoint] {}: where the reload landed is not checked ({}); nothing pressed after the reload", checkpoint.id, why));
        return Pokemon::StoryFailure{.code = "reload_unchecked", .detail = std::format(
            "{}: where the reload landed is not checked: {}; nothing pressed after the reload", checkpoint.id, why
        )};
    }
    const StartSignature read = options.read_start(start->signature);
    if (!checkpoint_at(std::span<const Pokemon::StoryCheckpoint>(&checkpoint, 1), std::span<const CheckpointStart>(start, 1), /*asked=*/0, read)){
        const std::string where = std::format(
            "the screen reads ({}), the start is ({}); nothing pressed after the reload", to_string(read), to_string(start->signature)
        );
        backend.log(std::format("[Checkpoint] {}: the reload landed somewhere other than its start: {}", checkpoint.id, where));
        return Pokemon::StoryFailure{.code = "reload_off_start", .detail = std::format(
            "the reload into {} landed somewhere other than its start: {}", checkpoint.id, where
        )};
    }
    backend.log(std::format("[Checkpoint] {}: the reload landed at its start ({})", checkpoint.id, to_string(read)));
    log_start_boundary(backend, checkpoint.id, "pinned by minimap");
    return std::nullopt;
}

//  A reload starts a fresh `state`.
std::optional<Pokemon::StoryFailure> chain_into(
    CheckpointRun& run, const Pokemon::StoryCheckpoint& previous, const Pokemon::StoryCheckpoint& checkpoint, Pokemon::RunState& state
){
    if (!run.options.reload_when_chained.contains(checkpoint.id)){
        return check_chain_start(run, checkpoint, run.sightings.last_end_seen());
    }
    const std::optional<Pokemon::StoryFailure> loaded = reload_into(run, previous, checkpoint);
    if (!loaded){
        state = Pokemon::RunState{};
    }
    return loaded;
}

} // namespace

std::optional<Pokemon::CheckpointFailure> run_plza_checkpoints(
    Pokemon::StoryGame& game, std::span<Pokemon::StoryCheckpoint> checkpoints, CheckpointRunOptions& options,
    PlzaBackend& backend, Pokemon::RunState& state, AutosaveSightings& sightings
){
    CheckpointRun run{.game = game, .backend = backend, .options = options, .sightings = sightings};
    for (size_t index = 0; index < checkpoints.size(); index++){
        Pokemon::StoryCheckpoint& checkpoint = checkpoints[index];
        const bool start_saved = index == 0 || options.reload_when_chained.contains(checkpoint.id) || sightings.last_end_seen();
        if (index > 0){
            if (std::optional<Pokemon::StoryFailure> chained = chain_into(run, checkpoints[index - 1], checkpoint, state)){
                backend.log(std::format("[Event] {}/0 chain: failed: {}", checkpoint.id, *chained));
                return Pokemon::CheckpointFailure{.checkpoint_id = checkpoint.id, .failure = std::move(*chained)};
            }
        }
        if (!start_saved && options.attempts_per_checkpoint > 1){
            backend.log(std::format("[Checkpoint] {}: no autosave seen at its start, so no reload if it fails", checkpoint.id));
        }
        sightings.restart();
        Pokemon::StoryGame retry{
            .save = [&game](PlzaBackend& on, const Pokemon::StoryCheckpoint& passed) -> std::optional<Pokemon::StoryFailure>{
                return game.save ? game.save(on, passed) : std::nullopt;
            },
            .attempts_per_checkpoint = start_saved ? options.attempts_per_checkpoint : 1,
        };
        if (game.reload){
            retry.reload = reloading_only_to_start(game, sightings);
        }
        if (std::optional<Pokemon::CheckpointFailure> failure = Pokemon::run_checkpoints(backend, checkpoints.subspan(index, 1), retry, state)){
            log_moved_save(backend, sightings, checkpoint.id);
            return failure;
        }
    }
    return std::nullopt;
}

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

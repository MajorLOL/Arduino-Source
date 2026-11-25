/*  Story Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <exception>
#include <format>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon_StoryEngine.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  The text of the exception being handled.
std::string current_exception_text(){
    try{
        throw;
    }catch (const Exception& e){
        return e.message();
    }catch (std::exception& e){
        return e.what();
    }catch (...){
        return "unknown exception";
    }
}

//  Runs `step`, step `number` (from 1) of checkpoint `checkpoint_id`; a
//  throw is logged and passes through.
std::optional<StoryFailure> run_step(
    std::string_view checkpoint_id, size_t number, StoryBackend& backend, StoryStep& step, RunState& state
){
    backend.log(event_line(checkpoint_id, number, step.name, "start"));
    state.step = std::format("{}/{}", checkpoint_id, step.name);
    std::optional<StoryFailure> failure;
    try{
        backend.scope().throw_if_cancelled();
        failure = step.run(backend, state);
    }catch (...){
        const std::string what = current_exception_text();
        backend.log(event_line(checkpoint_id, number, step.name, std::format("failed: {}", what)));
        backend.log(checkpoint_failed_line(checkpoint_id, number, step.name, what));
        throw;
    }
    backend.log(event_line(
        checkpoint_id, number, step.name, failure ? std::format("failed: {}", *failure) : "done"
    ));
    return failure;
}

std::set<std::string, std::less<>> actions_after(const StoryCheckpoint& checkpoint, size_t index){
    std::set<std::string, std::less<>> actions;
    for (size_t c = index + 1; c < checkpoint.steps.size(); c++){
        actions.insert(checkpoint.steps[c].action);
    }
    return actions;
}

//  The step after `index` that `failure` resumes at; none: the checkpoint fails.
//  Logs the jump.
std::optional<size_t> resume_step(StoryBackend& backend, const StoryCheckpoint& checkpoint, size_t index, const StoryFailure& failure){
    if (!failure.resume_at_action){
        return std::nullopt;
    }
    std::string skipped;
    for (size_t c = index + 1; c < checkpoint.steps.size(); c++){
        const StoryStep& step = checkpoint.steps[c];
        if (step.action == *failure.resume_at_action){
            backend.log(event_jump_line(checkpoint.id, failure.detail, checkpoint.steps[index].name, step.name, skipped));
            return c;
        }
        skipped += (skipped.empty() ? "" : ", ") + step.name;
    }
    return std::nullopt;
}

//  One attempt: the steps in order until one fails, then the save when
//  they passed.
std::optional<CheckpointFailure> run_once(
    StoryBackend& backend, StoryCheckpoint& checkpoint, StoryGame& game, RunState& state
){
    const std::string& id = checkpoint.id;
    backend.log(checkpoint_start_line(id, checkpoint.steps.size()));
    for (size_t c = 0; c < checkpoint.steps.size(); c++){
        state.actions_ahead = actions_after(checkpoint, c);
        std::optional<StoryFailure> failure = run_step(id, c + 1, backend, checkpoint.steps[c], state);
        if (failure){
            if (const std::optional<size_t> resume = resume_step(backend, checkpoint, c, *failure)){
                c = *resume - 1;
                continue;
            }
            backend.log(checkpoint_failed_line(id, c + 1, checkpoint.steps[c].name, std::format("{}", *failure)));
            return CheckpointFailure{.checkpoint_id = id, .event_index = c, .failure = std::move(*failure)};
        }
    }
    backend.log(checkpoint_done_line(id, checkpoint.steps.size()));
    if (game.save){
        if (const std::optional<StoryFailure> failure = game.save(backend, checkpoint)){
            backend.log(save_failed_line(id, std::format("{}", *failure)));
        }
    }
    return std::nullopt;
}

//  Empties `state` and reloads after `checkpoints[failed]` failed; returns
//  the index of the checkpoint up to `failed` whose start the game is at,
//  none when the game is at no such start.
std::optional<size_t> reload(
    StoryBackend& backend, std::span<const StoryCheckpoint> checkpoints, size_t failed, StoryGame& game, RunState& state
){
    state = RunState{};
    const std::optional<std::string> start = game.reload(backend, checkpoints[failed]);
    for (size_t c = 0; c <= failed; c++){
        if (start == checkpoints[c].id){
            return c;
        }
    }
    return std::nullopt;
}


} // namespace


std::optional<CheckpointFailure> run_checkpoints(
    StoryBackend& backend, std::span<StoryCheckpoint> checkpoints,
    StoryGame& game, RunState& state
){
    //  Failed runs of each checkpoint so far.
    std::vector<size_t> failures(checkpoints.size(), 0);
    size_t index = 0;
    while (index < checkpoints.size()){
        StoryCheckpoint& checkpoint = checkpoints[index];
        std::optional<CheckpointFailure> failure = run_once(backend, checkpoint, game, state);
        if (!failure){
            backend.log(checkpoint_pass_line(checkpoint.id, failures[index] + 1));
            index++;
            continue;
        }
        const size_t failed = ++failures[index];
        if (failure->failure.repeats || !game.reload || failed >= game.attempts_per_checkpoint){
            return failure;
        }
        backend.log(checkpoint_reset_line(checkpoint.id, failed + 1, game.attempts_per_checkpoint));
        const std::optional<size_t> start = reload(backend, checkpoints, index, game, state);
        if (!start){
            backend.log(checkpoint_no_start_line(checkpoint.id));
            return CheckpointFailure{
                .checkpoint_id = checkpoint.id,
                .failure = {.code = "start_unknown", .detail = "the reload is at no start of this run"},
            };
        }
        index = *start;
    }
    return std::nullopt;
}



} // namespace Pokemon
} // namespace PokemonAutomation

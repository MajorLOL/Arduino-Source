/*  Story Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The log lines every game's AutoStory and the story engine write.
 *
 */

// code-health: allow header_symbols -- every story log line format in one place, as review asked; each symbol is one line format

#ifndef PokemonAutomation_Pokemon_StoryLog_H
#define PokemonAutomation_Pokemon_StoryLog_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace PokemonAutomation{
namespace Pokemon{


//  The lines below start with "[Checkpoint] " and then `checkpoint`. A game
//  whose checkpoint text already says it is a checkpoint (SV's
//  "Checkpoint:: 004 - 02 ...") passes that text as `checkpoint` with
//  AS_GIVEN, and the line starts with the text alone:
//      Checkpoint:: 004 - 02 ...: start
enum class CheckpointPrefix{
    ADD,
    AS_GIVEN,
};

//  A checkpoint starts; the story engine also says it has `events` events:
//      [Checkpoint] 00.04: start
//      [Checkpoint] 00.04: start, 3 events
std::string checkpoint_start_line(
    std::string_view checkpoint,
    std::optional<size_t> events = std::nullopt,
    CheckpointPrefix prefix_kind = CheckpointPrefix::ADD
);

//  An attempt at a checkpoint starts, counting from 1, out of the
//  program's limit of `attempts` if it has one:
//      [Checkpoint] 00.04: attempt 2
//      [Checkpoint] 00.04: attempt 2/3
std::string checkpoint_attempt_line(
    std::string_view checkpoint,
    size_t attempt,
    std::optional<size_t> attempts = std::nullopt,
    CheckpointPrefix prefix_kind = CheckpointPrefix::ADD
);

//  A checkpoint passed on attempt `attempt`, counting from 1; every attempt
//  before it ended in a reset:
//      [Checkpoint] 00.03: passed on attempt 1 (one-shot)
//      [Checkpoint] 00.04: passed on attempt 3 (2 resets)
std::string checkpoint_pass_line(
    std::string_view checkpoint,
    size_t attempt,
    CheckpointPrefix prefix_kind = CheckpointPrefix::ADD
);

//  An attempt failed and the game resets to the checkpoint's save for
//  attempt `next_attempt`, out of the limit of `attempts` if it has one:
//      [Checkpoint] 00.04: failed, resetting for attempt 2
//      [Checkpoint] 00.04: failed, resetting for attempt 2/3
std::string checkpoint_reset_line(
    std::string_view checkpoint,
    size_t next_attempt,
    std::optional<size_t> attempts = std::nullopt,
    CheckpointPrefix prefix_kind = CheckpointPrefix::ADD
);

//  The story engine's lines; `number` counts a checkpoint's steps from 1.
//  A step starts or ends, with its status:
//      [Event] 00.04/2 to_lab: start
//      [Event] 00.04/2 to_lab: failed: no_fade: no white fade in 3 slices
std::string event_line(std::string_view checkpoint, size_t number, std::string_view event, std::string_view status);
//  A checkpoint's step `number`, named `event`, failed for `reason`:
//      [Checkpoint] 00.04: failed at 2 to_lab: no_fade: no white fade in 3 slices
std::string checkpoint_failed_line(std::string_view checkpoint, size_t number, std::string_view event, std::string_view reason);
//  A step ended for `reason` and the checkpoint goes on at step `to`,
//  past the steps `skipped` (", "-joined, empty for none):
//      [Event] 03.02b: battle on screen during plaza_scene: jumping to battle_totodile, skipping to_taunie, taunie_battle_start
std::string event_jump_line(
    std::string_view checkpoint, std::string_view reason, std::string_view event, std::string_view to, std::string_view skipped
);
//  Every one of a checkpoint's `events` steps passed:
//      [Checkpoint] 00.04: done, 3 run
std::string checkpoint_done_line(std::string_view checkpoint, size_t events);
//  A reload after the checkpoint failed came back at no checkpoint start of
//  the run:
//      [Checkpoint] 00.04: reloaded to no start of this run
std::string checkpoint_no_start_line(std::string_view checkpoint);
//  The save after a passed checkpoint failed for `reason`:
//      [Save] 00.04: save failed: save_error: the disk is full
std::string save_failed_line(std::string_view checkpoint, std::string_view reason);

//  `line` without its leading "[Tag] ", for the video overlay; a line with
//  no tag is returned unchanged.
std::string story_short_line(std::string_view line);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

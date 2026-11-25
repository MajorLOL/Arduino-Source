/*  Story Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_StoryLog.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

std::string resets_text(size_t attempt){
    if (attempt <= 1){
        return "one-shot";
    }
    return std::format("{} reset{}", attempt - 1, attempt == 2 ? "" : "s");
}

//  "[Checkpoint] 00.04", or the game's own checkpoint text unchanged.
std::string checkpoint_prefix(std::string_view checkpoint, CheckpointPrefix prefix_kind){
    if (prefix_kind == CheckpointPrefix::AS_GIVEN){
        return std::string(checkpoint);
    }
    return std::format("[Checkpoint] {}", checkpoint);
}

} // namespace


std::string checkpoint_start_line(
    std::string_view checkpoint, std::optional<size_t> events, CheckpointPrefix prefix_kind
){
    const std::string prefix = checkpoint_prefix(checkpoint, prefix_kind);
    if (!events){
        return std::format("{}: start", prefix);
    }
    return std::format("{}: start, {} events", prefix, *events);
}

std::string checkpoint_attempt_line(
    std::string_view checkpoint, size_t attempt, std::optional<size_t> attempts, CheckpointPrefix prefix_kind
){
    const std::string prefix = checkpoint_prefix(checkpoint, prefix_kind);
    if (!attempts){
        return std::format("{}: attempt {}", prefix, attempt);
    }
    return std::format("{}: attempt {}/{}", prefix, attempt, *attempts);
}

std::string checkpoint_pass_line(std::string_view checkpoint, size_t attempt, CheckpointPrefix prefix_kind){
    return std::format(
        "{}: passed on attempt {} ({})",
        checkpoint_prefix(checkpoint, prefix_kind), attempt, resets_text(attempt)
    );
}

std::string checkpoint_reset_line(
    std::string_view checkpoint, size_t next_attempt, std::optional<size_t> attempts, CheckpointPrefix prefix_kind
){
    const std::string prefix = checkpoint_prefix(checkpoint, prefix_kind);
    if (!attempts){
        return std::format("{}: failed, resetting for attempt {}", prefix, next_attempt);
    }
    return std::format("{}: failed, resetting for attempt {}/{}", prefix, next_attempt, *attempts);
}

std::string event_line(std::string_view checkpoint, size_t number, std::string_view event, std::string_view status){
    return std::format("[Event] {}/{} {}: {}", checkpoint, number, event, status);
}

std::string checkpoint_failed_line(std::string_view checkpoint, size_t number, std::string_view event, std::string_view reason){
    return std::format("[Checkpoint] {}: failed at {} {}: {}", checkpoint, number, event, reason);
}

std::string event_jump_line(
    std::string_view checkpoint, std::string_view reason, std::string_view event, std::string_view to, std::string_view skipped
){
    if (skipped.empty()){
        return std::format("[Event] {}: {} during {}: jumping to {}", checkpoint, reason, event, to);
    }
    return std::format("[Event] {}: {} during {}: jumping to {}, skipping {}", checkpoint, reason, event, to, skipped);
}

std::string checkpoint_done_line(std::string_view checkpoint, size_t events){
    return std::format("[Checkpoint] {}: done, {} run", checkpoint, events);
}

std::string checkpoint_no_start_line(std::string_view checkpoint){
    return std::format("[Checkpoint] {}: reloaded to no start of this run", checkpoint);
}

std::string save_failed_line(std::string_view checkpoint, std::string_view reason){
    return std::format("[Save] {}: save failed: {}", checkpoint, reason);
}

std::string story_short_line(std::string_view line){
    if (!line.starts_with('[')){
        return std::string(line);
    }
    const size_t end = line.find("] ");
    if (end == std::string_view::npos){
        return std::string(line);
    }
    return std::string(line.substr(end + 2));
}



} // namespace Pokemon
} // namespace PokemonAutomation

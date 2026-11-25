/*  PokemonLZA AutoStory - Checkpoint starts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryZone1.h"
#include "PokemonLZA_AutoStoryStarts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using Pokemon::StoryCheckpoint;


std::string to_string(const StartSignature& signature){
    std::vector<std::string> parts;
    if (signature.talk_prompt){
        parts.push_back(*signature.talk_prompt ? "talk prompt" : "no talk prompt");
    }
    if (signature.wild_zone){
        parts.push_back(*signature.wild_zone ? "in a wild zone" : "not in a wild zone");
    }
    if (signature.party){
        parts.push_back(std::format("party {}", *signature.party));
    }
    if (signature.minimap){
        parts.push_back(std::format("minimap {}", *signature.minimap));
    }
    if (parts.empty()){
        return "nothing read";
    }
    std::string out = parts[0];
    for (size_t index = 1; index < parts.size(); index++){
        out += std::format(", {}", parts[index]);
    }
    return out;
}

const CheckpointStart* find_start(std::span<const CheckpointStart> starts, std::string_view id){
    for (const CheckpointStart& start : starts){
        if (start.checkpoint == id){
            return &start;
        }
    }
    return nullptr;
}


namespace{

//  None when one of them is missing.
template <typename Type>
std::optional<bool> field_agrees(const std::optional<Type>& known, const std::optional<Type>& read){
    if (!known || !read){
        return std::nullopt;
    }
    return *known == *read;
}

bool start_matches(const StartSignature& known, const StartSignature& read){
    if (known.minimap && known.minimap != read.minimap){
        return false;
    }
    const std::optional<bool> fields[] = {
        field_agrees(known.talk_prompt, read.talk_prompt),
        field_agrees(known.wild_zone, read.wild_zone),
        field_agrees(known.party, read.party),
        field_agrees(known.minimap, read.minimap),
    };
    bool shared = false;
    for (const std::optional<bool>& agrees : fields){
        if (agrees && !*agrees){
            return false;
        }
        shared = shared || agrees.has_value();
    }
    return shared;
}

bool checkpoint_matches(const StoryCheckpoint& checkpoint, std::span<const CheckpointStart> starts, const StartSignature& read){
    const CheckpointStart* start = find_start(starts, checkpoint.id);
    return start != nullptr && start_matches(start->signature, read);
}

} // namespace


std::optional<size_t> checkpoint_at(
    std::span<const StoryCheckpoint> checkpoints, std::span<const CheckpointStart> starts,
    size_t asked, const StartSignature& read
){
    if (asked >= checkpoints.size()){
        return std::nullopt;
    }
    if (checkpoint_matches(checkpoints[asked], starts, read)){
        return asked;
    }
    std::optional<size_t> found;
    for (size_t index = 0; index < asked; index++){
        if (!checkpoint_matches(checkpoints[index], starts, read)){
            continue;
        }
        if (found){
            return std::nullopt;
        }
        found = index;
    }
    return found;
}

bool start_confirmed(const StartSignature& known, const StartSignature& read){
    const auto agrees = [](const auto& known_field, const auto& read_field){
        return !known_field || known_field == read_field;
    };
    return agrees(known.talk_prompt, read.talk_prompt) && agrees(known.wild_zone, read.wild_zone)
        && agrees(known.party, read.party) && agrees(known.minimap, read.minimap)
        && (known.talk_prompt || known.wild_zone || known.party || known.minimap);
}

std::optional<bool> wild_zone_from_map(const std::optional<MapPoint>& pin){
    if (!pin){
        return std::nullopt;
    }
    return pin_in_zone_1(*pin);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  PokemonLZA AutoStory - Checkpoint starts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Start-signature matching used to identify or confirm which checkpoint
 *  the live game is positioned at before execution.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryStarts_H
#define PokemonAutomation_PokemonLZA_AutoStoryStarts_H

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Observable screen fields identifying a checkpoint start; std::nullopt means unspecified or unread.
struct StartSignature{
    std::optional<bool> talk_prompt = std::nullopt;
    std::optional<bool> wild_zone = std::nullopt;
    std::optional<size_t> party = std::nullopt;
    std::optional<std::string> minimap = std::nullopt;

    bool operator==(const StartSignature&) const = default;
};

//  Formats `signature` for logging, or "nothing read" if all fields are unset.
std::string to_string(const StartSignature& signature);

//  Associates a checkpoint id with its declared StartSignature.
struct CheckpointStart{
    std::string checkpoint;
    StartSignature signature = {};
};

//  Returns the entry in `starts` for `id`, or nullptr if none exists.
const CheckpointStart* find_start(std::span<const CheckpointStart> starts, std::string_view id);

//  Returns `asked` if its signature matches `read`, else the unique earlier matching checkpoint index.
//  Returns std::nullopt if no checkpoint or multiple earlier checkpoints match.
std::optional<size_t> checkpoint_at(
    std::span<const Pokemon::StoryCheckpoint> checkpoints, std::span<const CheckpointStart> starts,
    size_t asked, const StartSignature& read
);

//  True when every field specified in `known` is present in `read` and equals it.
bool start_confirmed(const StartSignature& known, const StartSignature& read);

//  Returns whether `pin` lies inside Wild Zone 1, or std::nullopt if `pin` is unset.
std::optional<bool> wild_zone_from_map(const std::optional<MapPoint>& pin);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

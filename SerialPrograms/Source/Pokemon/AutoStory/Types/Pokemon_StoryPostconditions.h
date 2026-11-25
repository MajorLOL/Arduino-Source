/*  Story Postconditions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What must hold once a checkpoint is done, as plain data from its plan:
 *  where the player stands, what the Bag holds, which story flags moved.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryPostconditions_H
#define PokemonAutomation_Pokemon_StoryPostconditions_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Grid/Pokemon_GridStoryArgs.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The end state a checkpoint's plan promises: position, Bag items and flags.
struct StoryPostconditions{
    //  Where the checkpoint ends; unset, it is the last step's target.
    std::optional<GridPose> position = std::nullopt;
    //  Each item's count in the Bag, exactly, by the game's item id
    //  ("LIFT_KEY", "RARE_CANDY").
    std::vector<StoryItem> items = {};
    //  Story flags that must be set: a trainer beaten, a door open.
    std::vector<std::string> flags = {};

    bool operator==(const StoryPostconditions& other) const = default;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

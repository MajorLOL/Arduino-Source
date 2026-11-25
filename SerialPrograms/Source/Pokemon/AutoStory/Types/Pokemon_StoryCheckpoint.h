/*  Story Checkpoint
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checkpoint of a game's story.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryCheckpoint_H
#define PokemonAutomation_Pokemon_StoryCheckpoint_H

#include <string>
#include <vector>
#include "Pokemon_StoryStep.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The stretch of story between two saves: its steps.
struct StoryCheckpoint{
    std::string id = {};
    std::string title = {};
    std::vector<StoryStep> steps = {};
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

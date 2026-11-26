/*  Story Checkpoint JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads a story checkpoint file into steps ready to run.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryCheckpointJson_H
#define PokemonAutomation_Pokemon_StoryCheckpointJson_H

#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Parses a step's "args" into its typed args and returns the step's run
//  with them bound; throws ParseException on a bad arg.
using StoryActionParser = MoveOnlyFunction<StoryStepRun(const JsonObject& args)>;

//  A game's actions by the name a step's "action" gives.
using StoryActions = std::map<std::string, StoryActionParser, std::less<>>;

//  Throws ParseException naming the first key of `object` not in `keys`.
//  Only an action knows its args, so each parser checks its own with this.
void reject_unknown_keys(const JsonObject& object, std::initializer_list<std::string_view> keys);

//  The checkpoint of a checkpoint file of `game`:
//      {"format": "pokesim-authored-checkpoint/1", "game": ...,
//       "checkpoint": {"id", "title", "events": [steps]}, "execution": {...}}
//  Each step is parsed by its action among `actions`. Throws ParseException
//  naming `source` on anything unknown, mistyped or missing. "execution" is
//  the game's to read from `file`; the checkpoint's "start" and "metadata"
//  are notes for the plan's readers and are not read.
StoryCheckpoint read_story_checkpoint(
    const JsonValue& file, std::string_view game, std::string_view source, StoryActions& actions
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

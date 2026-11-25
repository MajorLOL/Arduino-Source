/*  AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The unit tests of the pieces every AutoStory program shares.
 *  add_tests_AutoStory() is the one entry point the unit test runner calls;
 *  a new shared AutoStory suite registers there.
 *
 */

#ifndef PokemonAutomation_Pokemon_AutoStory_Tests_H
#define PokemonAutomation_Pokemon_AutoStory_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Data/Pokemon_StoryCheckpointJson_Tests.h"
#include "Engine/Pokemon_ScreenMatch_Tests.h"
#include "Engine/Pokemon_StoryBackend_Tests.h"
#include "Engine/Pokemon_StoryEngine_Tests.h"
#include "Grid/Pokemon_GridStoryActions_Tests.h"
#include "Options/Pokemon_StoryRangeOptions_Tests.h"
#include "Pokemon_AutoStorySelect_Tests.h"
#include "Pokemon_StoryLog_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers every shared AutoStory suite, for the unit test runner.
inline void add_tests_AutoStory(UnitTestDatabase& database){
    add_tests_AutoStorySelect(database);
    add_tests_StoryRangeOptions(database);
    add_tests_StoryLog(database);
    add_tests_StoryCheckpointJson(database);
    add_tests_ScreenMatch(database);
    add_tests_StoryBackend(database);
    add_tests_StoryEngine(database);
    add_tests_GridStoryActions(database);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

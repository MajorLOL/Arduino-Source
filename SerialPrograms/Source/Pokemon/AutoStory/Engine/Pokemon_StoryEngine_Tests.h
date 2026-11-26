/*  Story Engine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEngine_Tests_H
#define PokemonAutomation_Pokemon_StoryEngine_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the story engine on a scripted backend: runs, lint, retries and saves.
void add_tests_StoryEngine(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

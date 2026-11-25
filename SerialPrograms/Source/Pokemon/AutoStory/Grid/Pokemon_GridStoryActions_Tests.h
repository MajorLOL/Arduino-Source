/*  Grid Story Actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_GridStoryActions_Tests_H
#define PokemonAutomation_Pokemon_GridStoryActions_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the shared tile actions: legs and face read from JSON and run on a scripted backend.
void add_tests_GridStoryActions(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

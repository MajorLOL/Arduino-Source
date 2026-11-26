/*  Story Backend Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryBackend_Tests_H
#define PokemonAutomation_Pokemon_StoryBackend_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the backend's pieces: a look from screen events, zone pose matching and the scripted backend.
void add_tests_StoryBackend(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

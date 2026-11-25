/*  Screen Match Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenMatch_Tests_H
#define PokemonAutomation_Pokemon_ScreenMatch_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Registers unit tests for ScreenMatch, screen timers, and wait_for()/poll_until().
void add_tests_ScreenMatch(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

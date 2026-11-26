/*  PokemonLZA AutoStory - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStory_Tests_H
#define PokemonAutomation_PokemonLZA_AutoStory_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Pure logic suites (no test images needed).
void add_tests_AutoStoryActions(UnitTestDatabase& database);
void add_tests_AutoStoryGame(UnitTestDatabase& database);

//  Full-build suites (image/screenshot detectors and new-game validation).
void add_tests_AutoStoryNewGame(UnitTestDatabase& database);
void add_tests_AutoStoryScreenEvents(UnitTestDatabase& database);

inline void add_tests_AutoStoryLogic(UnitTestDatabase& database){
    add_tests_AutoStoryActions(database);
    add_tests_AutoStoryGame(database);
}

inline void add_tests_AutoStoryImage(UnitTestDatabase& database){
    add_tests_AutoStoryNewGame(database);
    add_tests_AutoStoryScreenEvents(database);
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  PokemonLZA AutoStory - Console actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Console-only action handlers and story battle runner.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryConsoleActions_H
#define PokemonAutomation_PokemonLZA_AutoStoryConsoleActions_H

#include <chrono>
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryConsoleBackend.h"
#include "PokemonLZA_AutoStoryActionTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Builds the console-only action map for PlzaConsoleHooks::console.
//  Each handler catches OperationFailedException as a failed step; `env` must outlive it.
PlzaConsoleActions plza_console_actions(StoryEnv& env);

//  Fights a story battle until a post-battle dialogue box appears.
//  Throws OperationFailedException if the battle does not finish within `timeout`.
void fight_story_battle(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, std::chrono::seconds timeout
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

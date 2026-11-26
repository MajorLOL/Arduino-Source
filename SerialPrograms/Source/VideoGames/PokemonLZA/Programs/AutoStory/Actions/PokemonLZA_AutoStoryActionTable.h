/*  PokemonLZA AutoStory - Action table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Registry binding PLZA plan action names to pure and console-hooked handlers.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryActionTable_H
#define PokemonAutomation_PokemonLZA_AutoStoryActionTable_H

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryAutosaves.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStoryWalks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Console-only action callback invoked with the step's timeout budget.
using PlzaConsoleAction = MoveOnlyFunction<std::optional<Pokemon::StoryFailure>(PlzaBackend& backend, Milliseconds timeout)>;
using PlzaConsoleActions = std::map<std::string, PlzaConsoleAction, std::less<>>;

//  Callback that fights the active story battle until completion or failure.
using PlzaBattleRunner = MoveOnlyFunction<std::optional<Pokemon::StoryFailure>(PlzaBackend& backend)>;

//  Optional console-specific hooks composed into plza_story_actions().
//  Unset hooks cause console-only steps to fail with "console_only".
struct PlzaConsoleHooks{
    PlzaConsoleActions console = {};
    std::optional<MinimapArrowReader> arrow = std::nullopt;
    std::optional<LockOnWatch> lock_on = std::nullopt;
    PlzaBattleRunner battle = {};
    AutosaveSightings* sightings = nullptr;
    std::optional<StoryMove> starter_move = std::nullopt;
};

//  Returns the action parser table for PLZA plans bound to `hooks`, which must outlive parsed steps.
Pokemon::StoryActions plza_story_actions(PlzaConsoleHooks& hooks);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

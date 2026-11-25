/*  PokemonLZA AutoStory - New game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  New-game launch, language/look/name entry (01.00a), and Confirm Choices
 *  verification and confirmation (01.00b).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryNewGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryNewGame_H

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Languages in the new-game language list's order (left column, then right).
enum class NewGameLanguage{
    English,
    SpanishSpain,
    SpanishLatinAmerica,
    French,
    German,
    Italian,
    Japanese,
    Korean,
    ChineseSimplified,
    ChineseTraditional,
};

//  User-selected new-game intro choices.
struct NewGameChoices{
    NewGameLanguage language = NewGameLanguage::English;
    size_t look = 1;
    std::string name = "Kosmos";
};

//  Throws UserSetupError if Switch HOME is not on screen (wakes with at most one B).
void require_new_game_home(ConsoleHandle& console, ProControllerContext& context);

//  Returns a user-facing error if `rows` cannot be played with `choices`, or std::nullopt.
std::optional<std::string> new_game_run_problem(std::span<const Pokemon::StoryCheckpoint> rows, const NewGameChoices& choices);

//  Plays 01.00a from HOME through language, look, and name entry, stopping on Confirm Choices.
//  Throws UserSetupError on invalid setup/choices or OperationFailedException if a screen fails.
void play_new_game_intro(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices);

//  Returns the highlighted row on Confirm Choices without pressing any button.
//  Throws UserSetupError if Confirm Choices is not on screen.
size_t require_confirm_choices(ConsoleHandle& console, ProControllerContext& context);

//  Verifies name and language on Confirm Choices against `choices`, then selects "Ready!".
//  Throws UserSetupError if not on Confirm Choices, or OperationFailedException on mismatch/loss.
void confirm_new_game_choices(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices);

//  Closes any running title from HOME or in-game and launches PLZA up to its opening black screen.
//  Throws UserSetupError without launching anything if PLZA's tile is not found on HOME.
void launch_plza_from_home(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

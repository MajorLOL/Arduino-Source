/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure story scene helpers, platform waits, choice selection, and line-mash
 *  utilities operating through PlzaBackend.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScenes_H
#define PokemonAutomation_PokemonLZA_AutoStoryScenes_H

#include <cstddef>
#include <optional>
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Advances Taunie's first map sequence until the party bar holds with no text box.
//  Fails as "map_talk_timeout" after 120 s.
std::optional<Pokemon::StoryFailure> talk_through_map_screen(PlzaBackend& backend);

//  Steps along the wall until the interact prompt appears, then climbs the ladder.
//  Fails as "ladder_not_found" if no prompt appears.
std::optional<Pokemon::StoryFailure> climb_ladder(PlzaBackend& backend);

//  Advances dialogue until a choice box appears, then selects row `index` (0 = top).
//  Fails as "no_choice", "no_such_choice", or "choice_not_reached".
std::optional<Pokemon::StoryFailure> choose_row(PlzaBackend& backend, int index);

//  Passively waits (no button presses) until the station platform is live with free control.
//  Fails as "platform_not_seen" after `timeout`, or "game_not_running" on HOME.
std::optional<Pokemon::StoryFailure> wait_for_platform(PlzaBackend& backend, Milliseconds timeout);

//  Precondition: new-game opening was just confirmed.
//  Advances and skips the opening until the arrival autosave, then waits for platform control.
//  Fails as "platform_not_seen" after `timeout`, or "game_not_running" on HOME.
std::optional<Pokemon::StoryFailure> opening_to_platform(PlzaBackend& backend, Milliseconds timeout);

//  Summary of what press_a_through_lines() observed.
struct LinesPassed{
    bool text = false;
    bool card = false;
    bool timed_out = false;
};

//  Presses A while text or a tutorial card is visible until neither shows for `quiet`, up to `limit`.
LinesPassed press_a_through_lines(PlzaBackend& backend, Milliseconds quiet, Milliseconds limit);

//  Presses B until the party bar or a text box appears; returns false if neither shows within `presses`.
bool press_b_out_of_menu(PlzaBackend& backend, size_t presses);

//  Opens the main menu options, sets text speed to Fast, and returns to the overworld.
//  Fails as "text_speed_timeout" on timeout or "game_not_running" on HOME.
std::optional<Pokemon::StoryFailure> set_text_speed_fast(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

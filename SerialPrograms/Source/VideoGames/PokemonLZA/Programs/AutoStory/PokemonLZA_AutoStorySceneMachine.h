/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Scene progression state machine and battle-on-screen check for PLZA AutoStory.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H
#define PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  How long a Plus press is held to skip a cutscene.
constexpr Milliseconds CUTSCENE_SKIP_HOLD = Milliseconds(2000);

//  Holds ZL up to `hold`; true once the move-effectiveness symbols show (locked on).
using LockOnWatch = std::function<bool(Milliseconds hold)>;

//  Configuration for run_scene_machine(): terminal conditions and input overrides.
struct SceneMachineConfig{
    bool stop_at_choice = false;
    bool stop_at_battle = false;
    bool end_at_battle = false;
    std::optional<LockOnWatch> lock_on = std::nullopt;
    Button button = BUTTON_A;
    std::optional<Milliseconds> start_window = std::nullopt;
    bool end_on_black_fade = false;
};

//  True when a scene is active (text, choice, battle, tutorial card, or non-HUD fade/motion).
bool scene_under_way(const ScreenObservation& look);

//  Formats `look`'s active events and readings as a comma-separated string, or "nothing".
std::string describe_scene_look(const ScreenObservation& look);

//  Advances scenes according to `config` until a terminal screen is reached or `timeout` elapses.
//  Returns failure codes: "scene_never_started", "battle_on_screen", "scene_ended_in_control",
//  "game_not_running", or "scene_timeout".
std::optional<Pokemon::StoryFailure> run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout
);

//  Returns the "battle_on_screen" failure that redirects execution to the checkpoint's next story_battle.
Pokemon::StoryFailure battle_on_screen_failure();

//  Polls for a confirmed battle HUD; returns false immediately if the first look does not show it.
bool battle_showing(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

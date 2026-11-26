/*  PokemonLZA AutoStory - Console actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_MenuNavigation.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStoryCatches.h"
#include "PokemonLZA_AutoStoryNewGame.h"
#include "PokemonLZA_AutoStoryWalks.h"
#include "PokemonLZA_AutoStoryScenes.h"
#include "PokemonLZA_AutoStoryMapLegs.h"
#include "PokemonLZA_AutoStoryWorldWalks.h"
#include "PokemonLZA_AutoStoryZone1.h"
#include "PokemonLZA_AutoStoryConsoleActions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_StoryDialogDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;

namespace{

//  Runs at the far gate, each planned on one pin read.
constexpr int GATE_RUNS = 3;


//  Wraps `run(backend, timeout)` so an OperationFailedException becomes a StoryFailure with `error_code`.
template <typename Run>
PlzaConsoleAction catch_operation_failed(std::string_view name, std::string error_code, Run run){
    return [name = std::string(name), error_code = std::move(error_code), run = std::move(run)](
        PlzaBackend& backend, Milliseconds timeout
    ) -> std::optional<Pokemon::StoryFailure>{
        try{
            return run(backend, timeout);
        }catch (OperationFailedException& e){
            const std::string message = e.message();
            backend.log(std::format("[PLZA] {} failed: {}", name, message));
            return Pokemon::StoryFailure{.code = error_code, .detail = message};
        }
    };
}


//  Runs once more after a day/night change, which stops the run.
std::optional<Pokemon::StoryFailure> run_to_gate(StoryEnv& env, Milliseconds timeout){
    //  0: the A prompt, 1: a day/night change, else no prompt.
    int result = 1;
    for (size_t runs = 0; runs < 2 && result == 1; runs++){
        result = run_towards_gate_with_A_button(
            env.env.console, env.context, /*run_direction_x=*/0, /*run_direction_y=*/1, timeout
        );
    }
    if (result != 0){
        return Pokemon::StoryFailure{.code = "gate_not_reached", .detail = std::format("no gate A prompt in {} ms of running", timeout.count())};
    }
    return std::nullopt;
}

//  From `from` to `to` by dead reckoning, B held: no map read until the
//  gate's run has missed.
std::optional<Pokemon::StoryFailure> run_exit_leg(StoryEnv& env, PlzaBackend& backend, MapPoint from, MapPoint to){
    const MapLegMove leg = MapLeg(to, /*stop_radius=*/0).next(from);
    if (leg.step != MapLegStep::WALK){
        return std::nullopt;
    }
    backend.log(std::format(
        "[PLZA] run_out_of_zone_1: to ({:.0f}, {:.0f}) first, heading {:.0f}, hold {} ms",
        to.x, to.y, leg.heading, leg.hold.count()
    ));
    const std::optional<Pokemon::StoryFailure> faced = face_heading(backend, minimap_arrow(env), leg.heading);
    if (faced){
        return faced;
    }
    backend.stick(/*degrees=*/0, leg.hold, /*release=*/300ms, Pokemon::StoryPace::RUN);
    return std::nullopt;
}

struct ExitRun{
    //  None: at the gate's A prompt.
    std::optional<Pokemon::StoryFailure> failure = std::nullopt;
    //  A run that missed the prompt, worth planning again from a new read.
    bool missed = false;
};

//  zone_1_exit_legs() from `pin`, the last until the gate's A prompt.
ExitRun run_exit_legs(StoryEnv& env, PlzaBackend& backend, MapPoint pin, Milliseconds timeout){
    const std::vector<MapPoint> legs = zone_1_exit_legs(pin);
    MapPoint from = pin;
    for (size_t leg = 0; leg + 1 < legs.size(); from = legs[leg++]){
        const std::optional<Pokemon::StoryFailure> ran_leg = run_exit_leg(env, backend, from, legs[leg]);
        if (ran_leg){
            return {.failure = ran_leg};
        }
    }
    const std::optional<Pokemon::StoryFailure> faced = face_heading(backend, minimap_arrow(env), bearing(from, legs.back()));
    if (faced){
        return {.failure = faced};
    }
    const std::optional<Pokemon::StoryFailure> ran = run_to_gate(env, timeout);
    return {.failure = ran, .missed = ran.has_value()};
}

//  One pin read, then held-B runs: onto the gate's line when off it, and at
//  the far gate until its A prompt. A wild battle on does not stop it:
//  leaving the zone ends the battle. A run with no prompt (a pillar) reads
//  the pin again and plans from there.
std::optional<Pokemon::StoryFailure> run_out_of_zone_1(StoryEnv& env, PlzaBackend& backend, Milliseconds timeout){
    ExitRun run{.missed = true};
    for (int runs = 0; runs < GATE_RUNS && run.missed; runs++){
        const std::optional<MapPoint> pin = read_pin_on_map(env, backend);
        if (!pin){
            return Pokemon::StoryFailure{.code = "map_pin_not_read", .detail = "no map pin before the run to the far gate"};
        }
        if (runs > 0){
            backend.log(std::format("[PLZA] run_out_of_zone_1: no prompt on run {}, again from ({:.0f}, {:.0f})", runs, pin->x, pin->y));
        }
        run = run_exit_legs(env, backend, *pin, timeout);
    }
    return run.failure;
}


//  Mable's Research is three rows down the main menu; it opens on Rewards
//  under her lines. No line at all: not in her office.
std::optional<Pokemon::StoryFailure> claim_research_reward(StoryEnv& env, PlzaBackend& backend){
    overworld_to_main_menu(env.env.console, env.context);
    for (int i = 0; i < 3; i++){
        backend.press(BUTTON_DOWN, /*hold=*/160ms, /*release=*/400ms);
    }
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/2840ms);
    const LinesPassed passed = press_a_through_lines(backend, /*quiet=*/3s, /*limit=*/60s);
    if (passed.timed_out){
        return Pokemon::StoryFailure{.code = "reward_lines_endless", .detail = "Mable's lines did not end within 60 s"};
    }
    if (!passed.text){
        return Pokemon::StoryFailure{.code = "no_reward_lines", .detail = "no dialog after opening Mable's Research: no reward was claimed"};
    }
    if (!press_b_out_of_menu(backend, /*presses=*/60)){
        return Pokemon::StoryFailure{.code = "menu_not_closed", .detail = "no party bar or talk after 60 B presses out of the menu"};
    }
    return std::nullopt;
}

using ConsoleStep = std::optional<Pokemon::StoryFailure> (*)(StoryEnv& env, PlzaBackend& backend);

const std::map<std::string_view, ConsoleStep>& console_steps(){
    static const std::map<std::string_view, ConsoleStep> steps{
        {action_name(PlzaAction::CATCH_TUTORIAL), [](StoryEnv& env, PlzaBackend& backend){
            return catch_tutorial(env, backend, /*max_throws=*/6);
        }},
        {action_name(PlzaAction::BATTLE_THEN_CATCH), [](StoryEnv& env, PlzaBackend& backend){
            return battle_then_catch(env, backend, /*max_rounds=*/6);
        }},
        {action_name(PlzaAction::CATCH_IN_ZONE), [](StoryEnv& env, PlzaBackend& backend){
            return catch_in_zone(env, backend, /*max_throws=*/12);
        }},
        {action_name(PlzaAction::CHECK_OUT_OF_ZONE_1), [](StoryEnv& env, PlzaBackend& backend){
            return check_out_of_zone_1(backend, [&env, &backend]{
                return read_pin_on_map(env, backend);
            });
        }},
        {action_name(PlzaAction::CLAIM_MABLE_RESEARCH_REWARD), claim_research_reward},
    };
    return steps;
}

} // namespace


PlzaConsoleActions plza_console_actions(StoryEnv& env){
    PlzaConsoleActions actions;
    const auto add_new_game = [&](PlzaAction action, void (*step)(ConsoleHandle&, ProControllerContext&, const NewGameChoices&)){
        const std::string_view name = action_name(action);
        actions.emplace(name, catch_operation_failed(name, std::format("{}_failed", name), [&env, step](PlzaBackend&, Milliseconds) -> std::optional<Pokemon::StoryFailure>{
            step(env.env.console, env.context, env.new_game);
            return std::nullopt;
        }));
    };
    add_new_game(PlzaAction::NEW_GAME_INTRO, play_new_game_intro);
    add_new_game(PlzaAction::CONFIRM_NEW_GAME, confirm_new_game_choices);
    actions.emplace(action_name(PlzaAction::RUN_TO_GATE), catch_operation_failed(
        action_name(PlzaAction::RUN_TO_GATE), "gate_not_reached",
        [&env](PlzaBackend&, Milliseconds timeout){ return run_to_gate(env, timeout); }
    ));
    actions.emplace(action_name(PlzaAction::RUN_OUT_OF_ZONE_1), catch_operation_failed(
        action_name(PlzaAction::RUN_OUT_OF_ZONE_1), "gate_not_reached",
        [&env](PlzaBackend& backend, Milliseconds timeout){ return run_out_of_zone_1(env, backend, timeout); }
    ));
    for (const auto& [name, step] : console_steps()){
        actions.emplace(name, catch_operation_failed(
            name, "operation_failed",
            [&env, step](PlzaBackend& backend, Milliseconds){ return step(env, backend); }
        ));
    }
    return actions;
}


void fight_story_battle(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, std::chrono::seconds timeout
){
    TrainerBattleState battle_state(battle_ai);
    StoryDialogDetector any_dialog;
    const WallClock deadline = current_time() + timeout;
    bool attacked = false;
    while (true){
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        const bool dialog = any_dialog.detect(screen);
        if (dialog && attacked){
            return;
        }
        if (current_time() > deadline){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "fight_story_battle(): no dialogue after the battle within the timeout.",
                console
            );
        }
        if (dialog || !battle_state.attempt_one_attack(env, console, context)){
            if (!attacked){
                pbf_press_button(context, BUTTON_A, /*hold=*/160ms, /*release=*/400ms);
            }
            continue;
        }
        attacked = true;
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

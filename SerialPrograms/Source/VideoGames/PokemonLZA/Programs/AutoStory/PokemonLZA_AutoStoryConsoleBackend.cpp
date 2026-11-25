/*  PokemonLZA AutoStory - Console backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include "Common/Cpp/Color.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_MoveEffectivenessSymbol.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogChoiceBox.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_TutorialCardDetector.h"
#include "VideoGames/PokemonLZA/PokemonLZA_Settings.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_GameEntry.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryConsoleActions.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryNewGame.h"
#include "PokemonLZA_AutoStoryConsoleBackend.h"
#include "PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


PlzaConsoleBackend::PlzaConsoleBackend(StoryEnv& env)
    : m_env(env)
    , m_home(env.env.console, COLOR_RED)
{}

void PlzaConsoleBackend::press(Button button, Milliseconds hold, Milliseconds release){
    m_env.cached_map_pin.reset();
    m_run_end.reset();
    pbf_press_button(m_env.context, button, hold, release);
}

//  Console B 2026-11-19, measured on the full map at 03.04's start: walking
//  covers 16.6 px/s and running 27.0 px/s, linear from 0.4 s to 4 s.
constexpr double RUN_SPEEDUP = 1.65;

void PlzaConsoleBackend::stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace){
    m_env.cached_map_pin.reset();
    const double radians = degrees * std::numbers::pi / 180.0;
    const JoystickPosition position{std::sin(radians), std::cos(radians)};
    if (pace != Pokemon::StoryPace::RUN || m_env.walk_only){
        m_run_end.reset();
        pbf_move_left_joystick(m_env.context, position, duration, release);
        return;
    }
    const Milliseconds run(static_cast<int64_t>(duration.count() / RUN_SPEEDUP));
    const WallClock now = current_time();
    const WallClock start = m_run_end && *m_run_end > now ? *m_run_end : now;
    pbf_controller_state(m_env.context, BUTTON_B, DPAD_NONE, position, /*right_joystick=*/{}, run);
    if (release > 0ms){
        pbf_wait(m_env.context, release);
        m_run_end.reset();
    }else{
        m_run_end = start + run;
    }
}

void PlzaConsoleBackend::wait(Milliseconds duration){
    m_env.context.wait_for_all_requests();
    m_run_end.reset();
    m_env.context.wait_for(duration);
}

void PlzaConsoleBackend::wait_for_inputs(){
    //  Look before the queued run ends, with time left to queue the next slice.
    if (m_run_end && *m_run_end - 80ms > current_time()){
        m_env.context.wait_until(*m_run_end - 80ms);
    }else if (!m_run_end || *m_run_end <= current_time()){
        m_env.context.wait_for_all_requests();
        m_run_end.reset();
    }
}

ScreenObservation PlzaConsoleBackend::observe(){
    wait_for_inputs();
    VideoSnapshot frame = m_env.env.console.video().snapshot();
    ScreenObservation look;
    if (frame && m_home.detect(*frame.frame)){
        look.events.set(ScreenEvent::SWITCH_HOME_MENU, true);
    }else if (frame){
        look.events = m_observer.read(*frame.frame);
        if (look.events.has(ScreenEvent::CHOICE_BOX)){
            look.events.set(ScreenEvent::DIALOG_BOX, true);
        }
        if (m_autosave.detect(*frame.frame)){
            look.readings[std::string(AUTOSAVE_READING)] = 1;
        }
        if (m_controls_hint.detect(*frame.frame)){
            look.readings[std::string(CONTROLS_HINT_READING)] = 1;
        }
        if (m_advance_arrow.detect(*frame.frame)){
            look.readings[std::string(ADVANCE_ARROW_READING)] = 1;
        }
        if (m_mission_complete.detect(*frame.frame)){
            look.readings[std::string(MISSION_COMPLETE_READING)] = 1;
        }
        if (tutorial_card_shows(*frame.frame)){
            look.readings[std::string(TUTORIAL_CARD_READING)] = 1;
        }
        if (look.events.has(ScreenEvent::CHOICE_BOX)){
            const std::optional<DialogChoiceRows> rows = read_dialog_choice_rows(*frame.frame);
            if (rows){
                look.readings[std::string(CHOICE_ROWS_READING)] = (int)rows->count;
                look.readings[std::string(CHOICE_SELECTED_READING)] = (int)rows->selected;
            }
        }
        if (same_screen(m_previous, frame)){
            look.events.set(ScreenEvent::SCREEN_STILL, true);
        }
    }
    look.when = current_time();
    m_previous = frame;
    look.frame = std::move(frame);
    return look;
}

void PlzaConsoleBackend::log(std::string_view line){
    m_env.env.console.log(std::string(line), COLOR_BLUE);
    m_env.env.console.overlay().add_log(std::string(line));
}

void PlzaConsoleBackend::log_detail(std::string_view line){
    m_env.env.console.log(std::string(line), COLOR_BLUE);
}

CancellableScope& PlzaConsoleBackend::scope(){
    return m_env.context;
}


MinimapArrowReader minimap_arrow(StoryEnv& env){
    return [&env](){
        return get_facing_direction(env.env.console, env.context);
    };
}

LockOnWatch move_symbols_lock_on(StoryEnv& env){
    return [&env](Milliseconds hold){
        MoveEffectivenessSymbolWatcher symbols(COLOR_RED, &env.env.console.overlay(), /*hold_duration=*/100ms);
        env.context.wait_for_all_requests();
        const int fired = run_until<ProControllerContext>(
            env.env.console, env.context,
            [hold](ProControllerContext& context){
                pbf_press_button(context, BUTTON_ZL, hold, /*release=*/0ms);
            },
            {symbols}
        );
        env.context.wait_for_all_requests();
        return fired == 0;
    };
}

namespace{

//  The wait reset_game_from_home() takes by default.
constexpr Milliseconds RELOAD_SETTLE = 1s;

//  Launches PLZA's own tile, whichever game ran. Fails as "launch_failed".
std::optional<Pokemon::StoryFailure> reload_latest_save(StoryEnv& env){
    env.cached_map_pin.reset();
    ConsoleHandle& console = env.env.console;
    launch_plza_from_home(console, env.context);
    const bool loaded =
        openedgame_to_gamemenu(console, env.context, GameSettings::instance().START_GAME_WAIT) &&
        gamemenu_to_ingame(console, env.context, GameSettings::instance().ENTER_GAME_WAIT0);
    if (!loaded){
        return Pokemon::StoryFailure{.code = "launch_failed", .detail = "the game did not reach the overworld after the reload"};
    }
    env.context.wait_for(RELOAD_SETTLE);
    return std::nullopt;
}

} // namespace

PlzaConsoleHooks plza_console_hooks(StoryEnv& env, AutosaveSightings& sightings, std::optional<StoryMove> starter_move){
    return PlzaConsoleHooks{
        .console = plza_console_actions(env),
        .arrow = minimap_arrow(env),
        .lock_on = move_symbols_lock_on(env),
        .battle = [&env](PlzaBackend&) -> std::optional<Pokemon::StoryFailure>{
            try{
                fight_story_battle(env.env, env.env.console, env.context, env.battle_ai, /*timeout=*/120s);
            }catch (OperationFailedException& e){
                return Pokemon::StoryFailure{.code = "battle_failed", .detail = e.message()};
            }
            return std::nullopt;
        },
        .sightings = &sightings,
        .starter_move = std::move(starter_move),
    };
}

Pokemon::StoryGame make_plza_story_game(StoryEnv& env, const MarkedCheckpoints& marked, AutosaveSightings& sightings){
    Pokemon::StoryGame game = plza_story_game(marked, sightings);
    game.reload = [&env, &sightings](PlzaBackend& backend, const Pokemon::StoryCheckpoint& failed) -> std::optional<std::string>{
        sightings.restart();
        if (const std::optional<Pokemon::StoryFailure> failure = reload_latest_save(env)){
            backend.log(std::format("[Save] {}: the reload failed: {}", failed.id, *failure));
            return std::nullopt;
        }
        return failed.id;
    };
    return game;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

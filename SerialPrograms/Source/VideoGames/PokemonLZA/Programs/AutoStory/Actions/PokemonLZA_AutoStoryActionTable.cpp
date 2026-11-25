/*  PokemonLZA AutoStory - Action table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenMatch.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenWait.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStoryWalks.h"
#include "PokemonLZA_AutoStoryScenes.h"
#include "PokemonLZA_AutoStoryActionTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::RunState;
using Pokemon::StoryActionParser;
using Pokemon::StoryActions;
using Pokemon::StoryFailure;
using Pokemon::StoryStepRun;
using Pokemon::reject_unknown_keys;


namespace{

//  A console action run with no console: every attempt would fail the same way.
constexpr std::string_view CONSOLE_ONLY = "console_only";

//  A walk "end" that holds the last leg until a scene takes control.
constexpr std::string_view WALK_UNTIL_SCENE = "scene";

//  A walk "end" that presses A on the first look of an A prompt and ends
//  once the talk starts (walk_into_talk()).
constexpr std::string_view WALK_INTO_TALK = "talk";

//  The other walk "end"s: held until this event.
constexpr std::pair<std::string_view, ScreenEvent> WALK_ENDS[] = {
    {"fade_white", ScreenEvent::FADE_WHITE},
    {"fade_black", ScreenEvent::FADE_BLACK},
    {"dialog", ScreenEvent::DIALOG_BOX},
    {"prompt", ScreenEvent::INTERACT_PROMPT},
};

//  The event a walk's "end" arg names; none for "scene" or an unknown word.
std::optional<ScreenEvent> walk_end_event(std::string_view end){
    for (const auto& [word, event] : WALK_ENDS){
        if (word == end){
            return event;
        }
    }
    return std::nullopt;
}


std::optional<Milliseconds> optional_ms(const JsonObject& args, const std::string& key){
    const JsonValue* value = args.get_value(key);
    if (value == nullptr){
        return std::nullopt;
    }
    return Milliseconds(value->to_integer_throw());
}

//  A story battle later in the checkpoint: a battle on screen before it
//  ends the step, and the run goes on there.
bool battle_ahead(const RunState& state){
    return state.actions_ahead.contains(action_name(PlzaAction::STORY_BATTLE));
}

SceneMachineConfig ending_at_battle(SceneMachineConfig config, const RunState& state){
    config.end_at_battle = battle_ahead(state);
    return config;
}

Milliseconds ms_or(const JsonObject& args, const std::string& key, Milliseconds fallback){
    return optional_ms(args, key).value_or(fallback);
}

bool flag(const JsonObject& args, const std::string& key){
    const JsonValue* value = args.get_value(key);
    return value != nullptr && value->to_boolean_throw();
}

Button scene_button(const JsonObject& args){
    const JsonValue* value = args.get_value("button");
    if (value == nullptr || value->to_string_throw() == "A"){
        return BUTTON_A;
    }
    if (value->to_string_throw() == "B"){
        return BUTTON_B;
    }
    throw ParseException("\"button\" must be \"A\" or \"B\"");
}

//  An action with no args: `run` as the step's run.
template <typename Run>
Pokemon::StoryActionParser no_args(Run run){
    return [run](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, {});
        return run;
    };
}


using PlatformStep = std::optional<StoryFailure> (*)(PlzaBackend&, Milliseconds);

Pokemon::StoryActionParser platform_action(PlatformStep step, Milliseconds fallback){
    return [step, fallback](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, {"timeout_ms"});
        const Milliseconds timeout = ms_or(args, "timeout_ms", fallback);
        return [step, timeout](PlzaBackend& backend, RunState&){
            return step(backend, timeout);
        };
    };
}



//  A walk step's args.
struct WalkArgs{
    StoryMove move = {};
    //  None: the legs alone. Set: held until this event, or a scene for none.
    std::optional<std::optional<ScreenEvent>> end = std::nullopt;
    Milliseconds end_after = 0ms;
    std::optional<double> heading = std::nullopt;
    //  "end": "talk": the walk starts the talk at the prompt it reaches.
    bool into_talk = false;
    //  The chosen starter's walk replaces the legs, if it has one.
    bool to_starter = false;
};

//  [[turn degrees, walk ms], ...]
std::vector<StoryWalkLeg> read_legs(const JsonArray& rows){
    std::vector<StoryWalkLeg> legs;
    for (const JsonValue& row : rows){
        const JsonArray& pair = row.to_array_throw();
        if (pair.size() != 2){
            throw ParseException("each leg is [turn degrees, walk ms]");
        }
        legs.push_back({.turn = (double)pair[0].to_integer_throw(), .walk = Milliseconds(pair[1].to_integer_throw())});
    }
    return legs;
}

WalkArgs read_walk_args(const JsonObject& args){
    reject_unknown_keys(args, {"legs", "end", "held", "wait_ms", "end_after_ms", "heading", "to_starter"});
    WalkArgs walk;
    walk.move.legs = read_legs(args.get_array_throw("legs"));
    walk.to_starter = flag(args, "to_starter");
    if (walk.to_starter && walk.move.legs.empty()){
        throw ParseException("\"to_starter\" needs the legs of the starter with no walk of its own");
    }
    if (flag(args, "held")){
        walk.move.style = StoryWalkStyle::HELD;
    }
    walk.move.wait_first = ms_or(args, "wait_ms", 0ms);
    if (const JsonValue* end = args.get_value("end")){
        const std::string& word = end->to_string_throw();
        const std::optional<ScreenEvent> event = walk_end_event(word);
        walk.into_talk = word == WALK_INTO_TALK;
        if (word != WALK_UNTIL_SCENE && !walk.into_talk && !event){
            throw ParseException("\"end\" must be \"fade_white\", \"fade_black\", \"dialog\", \"prompt\", \"talk\" or \"scene\"");
        }
        if (!walk.into_talk){
            walk.end = event;
        }
    }
    if (const std::optional<Milliseconds> end_after = optional_ms(args, "end_after_ms")){
        if (!walk.end){
            throw ParseException("\"end_after_ms\" needs an \"end\" other than \"talk\"");
        }
        walk.end_after = *end_after;
    }
    if (const JsonValue* heading = args.get_value("heading")){
        walk.heading = (double)heading->to_integer_throw();
    }
    return walk;
}

//  With "to_starter", hooks.starter_move replaces the legs.
WalkArgs aimed_at_starter(PlzaConsoleHooks& hooks, WalkArgs walk){
    if (walk.to_starter && hooks.starter_move){
        const Milliseconds wait_first = walk.move.wait_first;
        walk.move = *hooks.starter_move;
        walk.move.wait_first = wait_first;
    }
    return walk;
}

//  Starts only with free control. "heading" needs the arrow, else CONSOLE_ONLY.
std::optional<StoryFailure> walk_step(
    PlzaConsoleHooks& hooks, const WalkArgs& walk, PlzaBackend& backend, std::string_view name, bool stop_on_battle
){
    if (walk.move.wait_first > 0ms){
        backend.wait(walk.move.wait_first);
    }
    if (std::optional<StoryFailure> busy = check_can_walk(backend, name)){
        return busy;
    }
    if (walk.heading){
        if (!hooks.arrow){
            return StoryFailure{.code = std::string(CONSOLE_ONLY), .detail = std::format("{}: its heading needs the minimap arrow", name), .repeats = true};
        }
        if (std::optional<StoryFailure> faced = face_heading(backend, *hooks.arrow, *walk.heading)){
            return faced;
        }
    }
    if (walk.into_talk){
        return walk_into_talk(backend, name, walk.move, {.after_legs = walk.to_starter, .stop_on_battle = stop_on_battle});
    }
    if (walk.end){
        return walk_until(backend, name, walk.move, {.end = *walk.end, .end_after = walk.end_after, .stop_on_battle = stop_on_battle});
    }
    run_story_move(backend, walk.move);
    finish_walk(backend, name);
    return std::nullopt;
}


//  Preserve the old scene actions' bounds: battle start 90 s; a choice,
//  start window or B mash 120 s; the general story loop 360 s.
Milliseconds scene_timeout(const SceneMachineConfig& config){
    if (config.stop_at_battle){
        return 90s;
    }
    if (config.stop_at_choice || config.start_window || config.button == BUTTON_B){
        return 120s;
    }
    return 360s;
}

Pokemon::StoryActionParser scene_action(PlzaConsoleHooks& hooks){
    return [&hooks](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, {
            "button", "press_a_first", "stop_at_choice", "start_window_ms", "end_on_black_fade", "battle_start",
        });
        const Button button = scene_button(args);
        const bool stop_at_choice = flag(args, "stop_at_choice");
        const bool battle_start = flag(args, "battle_start");
        const SceneMachineConfig config{
            .stop_at_choice = stop_at_choice,
            .stop_at_battle = battle_start,
            .lock_on = battle_start ? hooks.lock_on : std::nullopt,
            .button = button,
            .start_window = optional_ms(args, "start_window_ms"),
            .end_on_black_fade = flag(args, "end_on_black_fade"),
        };
        return [press_a_first = flag(args, "press_a_first"), battle_start, config](PlzaBackend& backend, RunState& state){
            if (press_a_first){
                press_through_prompt(backend);
            }
            const SceneMachineConfig run = battle_start ? config : ending_at_battle(config, state);
            return run_scene_machine(backend, "scene", run, /*timeout=*/scene_timeout(config));
        };
    };
}


constexpr Milliseconds PLUS_SKIP_POLL = 250ms;
//  Console B, 2026-11-19: Plus 3.0-3.4 s after the opening fade ended
//  skipped 01.01's ad and 02.02's street cutscene; the skip's black fade
//  showed 2.2-2.8 s after the press started.
constexpr Milliseconds PLUS_SKIP_AFTER_FADE = 2500ms;
constexpr Milliseconds PLUS_SKIP_FADE_WITHIN = 5000ms;
constexpr Milliseconds PLUS_SKIP_FADE_END_WAIT = 15s;

const Pokemon::ScreenMatch NOT_BLACK_FADE{
    .no_events = {ScreenEvent::FADE_BLACK},
};
const Pokemon::ScreenMatch BLACK_FADE{
    .all_events = {ScreenEvent::FADE_BLACK},
};

//  From a cutscene's opening black fade: once it ends, Plus, then the
//  skip's black fade. Fails as "fade_not_ended" or "cutscene_not_skipped".
std::optional<StoryFailure> plus_skip_step(PlzaBackend& backend){
    const Pokemon::WaitOutcome fade_end = Pokemon::wait_for(
        backend,
        NOT_BLACK_FADE,
        Pokemon::WaitRule{.poll = PLUS_SKIP_POLL, .timeout = PLUS_SKIP_FADE_END_WAIT}
    );
    if (const auto* failure = std::get_if<StoryFailure>(&fade_end)){
        return *failure;
    }
    if (!std::holds_alternative<Pokemon::WaitMatched>(fade_end)){
        return StoryFailure{.code = "fade_not_ended", .detail = "plus_skip: the black fade did not end within 15 s"};
    }
    backend.wait(PLUS_SKIP_AFTER_FADE);
    backend.press(BUTTON_PLUS, CUTSCENE_SKIP_HOLD, /*release=*/0ms);
    const Pokemon::WaitOutcome skipped = Pokemon::wait_for(
        backend,
        BLACK_FADE,
        Pokemon::WaitRule{.poll = PLUS_SKIP_POLL, .timeout = PLUS_SKIP_FADE_WITHIN}
    );
    if (const auto* failure = std::get_if<StoryFailure>(&skipped)){
        return *failure;
    }
    if (const auto* matched = std::get_if<Pokemon::WaitMatched>(&skipped)){
        backend.log(std::format("[PLZA] plus_skip: a black fade {} ms after the hold.", matched->elapsed.count()));
        return std::nullopt;
    }
    const std::string detail = std::format("plus_skip: no black fade within {} ms after the hold.", PLUS_SKIP_FADE_WITHIN.count());
    backend.log(std::format("[PLZA] {}", detail));
    return StoryFailure{.code = "cutscene_not_skipped", .detail = detail};
}

//  As long as the start's lock-on holds ZL.
constexpr Milliseconds BATTLE_LOCK_ON_HOLD = 2s;
constexpr Milliseconds BATTLE_START_POLL = 250ms;
//  Fails the step in seconds, not after 2 min of looks on the overworld.
constexpr Seconds BATTLE_START_WAIT = 10s;

//  A start that locked on and let go of ZL can leave the HUD down until ZL
//  is held again, so `lock_on` also counts the move symbols under ZL.
std::optional<StoryFailure> wait_for_battle_start(PlzaBackend& backend, const std::optional<LockOnWatch>& lock_on, Milliseconds within){
    const Milliseconds step = lock_on ? BATTLE_LOCK_ON_HOLD : BATTLE_START_POLL;
    std::optional<WallClock> start;
    for (Milliseconds waited = 0ms; ; waited += step){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            return Pokemon::game_not_running_failure();
        }
        if (look.events.has(ScreenEvent::BATTLE_STARTED)){
            return std::nullopt;
        }
        start = start.value_or(look.when);
        if (waited >= within || (look.when > *start && look.when - *start >= within)){
            break;
        }
        if (lock_on){
            if ((*lock_on)(BATTLE_LOCK_ON_HOLD)){
                backend.log("[PLZA] story_battle: no battle HUD; ZL held: the move symbols showed.");
                return std::nullopt;
            }
        }else{
            backend.wait(BATTLE_START_POLL);
        }
    }
    return StoryFailure{
        .code = "battle_never_started",
        .detail = std::format(
            "no battle HUD{} within {} s", lock_on ? " and no move symbols under ZL" : "",
            std::chrono::duration_cast<Seconds>(within).count()
        ),
    };
}

std::optional<StoryFailure> battle_step(PlzaConsoleHooks& hooks, PlzaBackend& backend){
    if (std::optional<StoryFailure> failure = wait_for_battle_start(backend, hooks.lock_on, BATTLE_START_WAIT)){
        return failure;
    }
    if (!hooks.battle){
        return StoryFailure{.code = std::string(CONSOLE_ONLY), .detail = "no battle code off a console", .repeats = true};
    }
    return hooks.battle(backend);
}


//  A console action: the console's code when `hooks` has it, else
//  CONSOLE_ONLY. `timeout`: the step's "timeout_ms" fallback; none: no args.
StoryActionParser console_action(PlzaConsoleHooks& hooks, std::string_view name, std::optional<Milliseconds> timeout = std::nullopt){
    return [&hooks, name = std::string(name), timeout](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, timeout ? std::initializer_list<std::string_view>{"timeout_ms"} : std::initializer_list<std::string_view>{});
        const Milliseconds limit = ms_or(args, "timeout_ms", timeout.value_or(0ms));
        return [&hooks, name, limit](PlzaBackend& backend, RunState&) -> std::optional<StoryFailure>{
            const auto action = hooks.console.find(name);
            if (action == hooks.console.end()){
                return StoryFailure{.code = std::string(CONSOLE_ONLY), .detail = std::format("{} runs only on a console", name), .repeats = true};
            }
            return action->second(backend, limit);
        };
    };
}

StoryActionParser walk_action(PlzaConsoleHooks& hooks){
    return [&hooks](const JsonObject& args) -> StoryStepRun{
        //  With a story battle ahead, the battle HUD before the walk or after
        //  a failed one ends it with battle_on_screen_failure().
        return [&hooks, walk = read_walk_args(args)](PlzaBackend& backend, RunState& state) -> std::optional<StoryFailure>{
            const bool battle = battle_ahead(state);
            if (battle && battle_showing(backend)){
                return battle_on_screen_failure();
            }
            std::optional<StoryFailure> walked = walk_step(hooks, aimed_at_starter(hooks, walk), backend, "walk", battle);
            if (walked && battle && battle_showing(backend)){
                return battle_on_screen_failure();
            }
            return walked;
        };
    };
}

//  The actions that need no console.
void add_backend_actions(StoryActions& actions, PlzaConsoleHooks& hooks){
    std::pair<std::string_view, StoryActionParser> table[] = {
        {action_name(PlzaAction::WAIT_FOR_PLATFORM), platform_action(wait_for_platform, /*fallback=*/180s)},
        {action_name(PlzaAction::OPENING_TO_PLATFORM), platform_action(opening_to_platform, /*fallback=*/240s)},
        {action_name(PlzaAction::WALK), walk_action(hooks)},
        {action_name(PlzaAction::TALK_THROUGH_MAP_SCREEN), no_args([](PlzaBackend& backend, RunState&){ return talk_through_map_screen(backend); })},
        {action_name(PlzaAction::CLIMB_LADDER), no_args([](PlzaBackend& backend, RunState&){ return climb_ladder(backend); })},
        {action_name(PlzaAction::CHOOSE_0), no_args([](PlzaBackend& backend, RunState&){ return choose_row(backend, 0); })},
        {action_name(PlzaAction::CHOOSE_1), no_args([](PlzaBackend& backend, RunState&){ return choose_row(backend, 1); })},
        {action_name(PlzaAction::SCENE), scene_action(hooks)},
        {action_name(PlzaAction::PLUS_SKIP), no_args([](PlzaBackend& backend, RunState&){ return plus_skip_step(backend); })},
        {action_name(PlzaAction::STORY_BATTLE), no_args([&hooks](PlzaBackend& backend, RunState&){ return battle_step(hooks, backend); })},
        {action_name(PlzaAction::SET_TEXT_SPEED_FAST), no_args([](PlzaBackend& backend, RunState&){ return set_text_speed_fast(backend); })},
    };
    for (auto& [name, parser] : table){
        actions.emplace(name, std::move(parser));
    }
}

//  Wild catches always trigger an in-game autosave, even when the badge flashes
//  before the post-throw animation finishes.
StoryActionParser catch_in_zone_action(PlzaConsoleHooks& hooks){
    return [&hooks, run_console = console_action(hooks, action_name(PlzaAction::CATCH_IN_ZONE))](
        const JsonObject& args
    ) mutable -> StoryStepRun{
        return [&hooks, step = run_console(args)](PlzaBackend& backend, RunState& state) mutable -> std::optional<StoryFailure>{
            std::optional<StoryFailure> failure = step(backend, state);
            if (!failure && hooks.sightings != nullptr){
                hooks.sightings->saved(backend, "autosave on catch");
            }
            return failure;
        };
    };
}

//  The actions that run only on a console, bound to `hooks.console`.
void add_console_actions(StoryActions& actions, PlzaConsoleHooks& hooks){
    std::pair<std::string_view, StoryActionParser> table[] = {
        {action_name(PlzaAction::RUN_TO_GATE), console_action(hooks, action_name(PlzaAction::RUN_TO_GATE), /*timeout=*/8s)},
        {action_name(PlzaAction::RUN_OUT_OF_ZONE_1), console_action(hooks, action_name(PlzaAction::RUN_OUT_OF_ZONE_1), /*timeout=*/5s)},
        {action_name(PlzaAction::NEW_GAME_INTRO), console_action(hooks, action_name(PlzaAction::NEW_GAME_INTRO))},
        {action_name(PlzaAction::CONFIRM_NEW_GAME), console_action(hooks, action_name(PlzaAction::CONFIRM_NEW_GAME))},
        {action_name(PlzaAction::CATCH_TUTORIAL), console_action(hooks, action_name(PlzaAction::CATCH_TUTORIAL))},
        {action_name(PlzaAction::BATTLE_THEN_CATCH), console_action(hooks, action_name(PlzaAction::BATTLE_THEN_CATCH))},
        {action_name(PlzaAction::CATCH_IN_ZONE), catch_in_zone_action(hooks)},
        {action_name(PlzaAction::CHECK_OUT_OF_ZONE_1), console_action(hooks, action_name(PlzaAction::CHECK_OUT_OF_ZONE_1))},
        {action_name(PlzaAction::CLAIM_MABLE_RESEARCH_REWARD), console_action(hooks, action_name(PlzaAction::CLAIM_MABLE_RESEARCH_REWARD))},
    };
    for (auto& [name, parser] : table){
        actions.emplace(name, std::move(parser));
    }
}

} // namespace


StoryActions plza_story_actions(PlzaConsoleHooks& hooks){
    StoryActions actions;
    add_backend_actions(actions, hooks);
    add_console_actions(actions, hooks);
    return actions;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

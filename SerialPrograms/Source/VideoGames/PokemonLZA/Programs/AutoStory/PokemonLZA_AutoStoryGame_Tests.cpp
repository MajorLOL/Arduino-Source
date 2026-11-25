/*  PokemonLZA AutoStory - Game Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "Actions/PokemonLZA_AutoStoryActionTable.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryPlan.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStory_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;

namespace{

using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;

std::string joined(const std::vector<std::string>& texts, std::string_view separator){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : std::string(separator)) + part;
    }
    return text;
}

ScreenObservation look(ScreenEvents events, std::vector<std::string_view> readings, Milliseconds at = 0ms){
    ScreenObservation observation;
    observation.when = WallClock::min() + at;
    observation.events = events;
    for (std::string_view reading : readings){
        observation.readings[std::string(reading)] = 1;
    }
    return observation;
}

ScreenObservation event_look(ScreenEvent event, Milliseconds at = 0ms){ return look({event}, /*readings=*/{}, at); }
ScreenObservation reading_look(std::string_view reading, Milliseconds at = 0ms){ return look(/*events=*/{}, {reading}, at); }
ScreenObservation quiet_look(Milliseconds at = 0ms){ return look(/*events=*/{}, /*readings=*/{}, at); }
ScreenObservation party_bar(Milliseconds at){ return event_look(ScreenEvent::PLAYER_CAN_MOVE, at); }

ScreenObservation choice(int selected, int rows = 2, Milliseconds at = 0ms){
    ScreenObservation observation = look({ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX}, /*readings=*/{}, at);
    observation.readings[std::string(CHOICE_ROWS_READING)] = rows;
    observation.readings[std::string(CHOICE_SELECTED_READING)] = selected;
    return observation;
}

const ScreenObservation PLATFORM = reading_look(CONTROLS_HINT_READING);
const ScreenObservation OWN_AUTOSAVE = reading_look(AUTOSAVE_READING, 2000ms);
const ScreenObservation CUTSCENE = quiet_look();
const ScreenObservation BLACK_FADE = event_look(ScreenEvent::FADE_BLACK);
const ScreenObservation DIALOG = event_look(ScreenEvent::DIALOG_BOX);
const ScreenObservation IN_CONTROL = event_look(ScreenEvent::PLAYER_CAN_MOVE);
const ScreenObservation A_PROMPT = look({ScreenEvent::INTERACT_PROMPT, ScreenEvent::PLAYER_CAN_MOVE}, /*readings=*/{});
const ScreenObservation PROMPT_ONLY = event_look(ScreenEvent::INTERACT_PROMPT);
const ScreenObservation PLATFORM_SAVING = look(/*events=*/{}, {CONTROLS_HINT_READING, AUTOSAVE_READING}, 2000ms);

std::vector<ScreenObservation> concat(std::vector<ScreenObservation> first, const std::vector<ScreenObservation>& then){
    first.insert(first.end(), then.begin(), then.end());
    return first;
}

StoryCheckpoint named_step_row(std::string_view id, std::string_view step_name, PlzaConsoleHooks& tools){
    for (StoryCheckpoint& row : load_plza_story(tools).checkpoints){
        if (row.id == id){
            std::erase_if(row.steps, [step_name](const StoryStep& step){ return step.name != step_name; });
            return std::move(row);
        }
    }
    return {};
}

StoryCheckpoint sliced_row(std::string_view id, size_t first_step, size_t steps, PlzaConsoleHooks& tools){
    for (StoryCheckpoint& row : load_plza_story(tools).checkpoints){
        if (row.id == id){
            row.steps.erase(row.steps.begin(), row.steps.begin() + (std::ptrdiff_t)first_step);
            if (steps < row.steps.size()){
                row.steps.resize(steps);
            }
            return std::move(row);
        }
    }
    return {};
}

std::string verdict_of(const std::optional<Pokemon::CheckpointFailure>& failure, std::span<const StoryCheckpoint> rows, bool with_checkpoint){
    if (!failure){
        return "done";
    }
    std::string event = "chain";
    for (const StoryCheckpoint& checkpoint : rows){
        if (checkpoint.id == failure->checkpoint_id && failure->event_index){
            event = checkpoint.steps[*failure->event_index].name;
        }
    }
    return with_checkpoint
        ? std::format("failed at {} {}: {}", failure->checkpoint_id, event, failure->failure.code)
        : std::format("failed at {}: {}", event, failure->failure.code);
}

UnitTestResult verify_outcome(
    const std::string& verdict, std::string_view expected_verdict,
    const Pokemon::FakeStoryBackend& backend,
    const std::optional<std::string>& expected_inputs, std::string_view expected_line
){
    if (verdict != expected_verdict){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", expected_verdict, verdict, joined(backend.lines(), "\n"));
    }
    const std::string inputs = joined(backend.inputs(), ", ");
    if (expected_inputs && inputs != *expected_inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", *expected_inputs, inputs);
    }
    if (!expected_line.empty() && std::find(backend.lines().begin(), backend.lines().end(), expected_line) == backend.lines().end()){
        return std::format("expected the line \"{}\" in:\n{}", expected_line, joined(backend.lines(), "\n"));
    }
    return true;
}

StoryStep plan_step(std::string name, std::string_view action, const JsonObject& args, PlzaConsoleHooks& tools){
    return StoryStep{.name = std::move(name), .action = std::string(action), .run = plza_story_actions(tools).at(std::string(action))(args)};
}

StoryCheckpoint one_step_row(std::string id, std::string action, const JsonObject& args, PlzaConsoleHooks& tools){
    StoryCheckpoint row{.id = std::move(id), .title = "Test"};
    row.steps.push_back(plan_step(action, action, args, tools));
    return row;
}

template <typename Case, UnitTestResult (*check)(const Case&, CancellableScope&)>
class CaseTest : public UnitTest{
public:
    CaseTest(std::string_view kind, const Case& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame {}{}", kind, test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check(m_case, scope);
    }

private:
    const Case& m_case;
};

//  --- Built-in StoryData plans & scene args ---

struct BuiltinStoryCase{
    std::string name;
    struct Input{};
    Input input;
    struct Expectation{ size_t plan_count; };
    Expectation expectation;
};

const BuiltinStoryCase BUILTIN_STORY_CASES[] = {
    {.name = "all 27 StoryData plans parse and bind cleanly", .input = {}, .expectation = {.plan_count = 27}},
};

UnitTestResult check_builtin_story(const BuiltinStoryCase& test, CancellableScope&){
    PlzaConsoleHooks tools;
    const StoryData story = load_plza_story(tools);
    const size_t embedded = autostory_checkpoint_json().size();
    if (embedded != test.expectation.plan_count || story.checkpoints.size() != embedded){
        return std::format("expected {} plans (embedded {}), loaded {}", test.expectation.plan_count, embedded, story.checkpoints.size());
    }
    for (const StoryCheckpoint& checkpoint : story.checkpoints){
        if (checkpoint.id.empty() || checkpoint.title.empty() || checkpoint.steps.empty()){
            return std::format("checkpoint \"{}\" is missing id, title, or steps", checkpoint.id);
        }
        for (const StoryStep& step : checkpoint.steps){
            if (!step.run){
                return std::format("checkpoint {} step \"{}\" has no bound action", checkpoint.id, step.name);
            }
        }
    }
    return true;
}

struct SceneArgsCase{
    std::string name;
    struct Input{ std::string key; };
    Input input;
    struct Expectation{ bool rejected; };
    Expectation expectation;
};

const SceneArgsCase SCENE_ARGS_CASES[] = {
    {.name = "unknown key is rejected", .input = {.key = "advance_with_a"}, .expectation = {.rejected = true}},
};

UnitTestResult check_scene_args(const SceneArgsCase& test, CancellableScope&){
    PlzaConsoleHooks tools;
    JsonObject args;
    args[test.input.key] = true;
    try{
        plza_story_actions(tools).at("scene")(args);
        return test.expectation.rejected ? UnitTestResult(UnitTestResult::FAILED, "expected ParseException") : UnitTestResult(true);
    }catch (const ParseException&){
        return test.expectation.rejected ? UnitTestResult(true) : UnitTestResult(UnitTestResult::FAILED, "unexpected ParseException");
    }
}

//  --- Autosave sightings and step validation ---

struct SightingLook{
    std::string step;
    bool badge = false;
    Milliseconds at = 0ms;
};

struct SightingsCase{
    std::string name;
    struct Input{ std::vector<SightingLook> looks; };
    Input input;
    struct Expectation{ std::optional<std::string> seen_in; };
    Expectation expectation;
};

const SightingsCase SIGHTINGS_CASES[] = {
    {
        .name = "a badge still up from the last checkpoint's autosave is not this checkpoint's",
        .input = {.looks = {
            {.step = "check_in", .badge = true, .at = 93500ms}, {.step = ""},
            {.step = "to_lobby_elevator", .badge = true, .at = 94700ms}, {.step = "to_lobby_elevator", .badge = true, .at = 95000ms},
            {.step = "to_lobby_elevator", .badge = false, .at = 95500ms}, {.step = "floor_question", .badge = false, .at = 106000ms},
        }},
        .expectation = {.seen_in = std::nullopt},
    },
    {
        .name = "a new badge after the last checkpoint's has gone is this checkpoint's",
        .input = {.looks = {
            {.step = "check_in", .badge = true, .at = 93500ms}, {.step = ""},
            {.step = "to_lobby_elevator", .badge = true, .at = 94700ms}, {.step = "to_lobby_elevator", .badge = false, .at = 95500ms},
            {.step = "floor_question", .badge = true, .at = 106000ms},
        }},
        .expectation = {.seen_in = "floor_question"},
    },
    {
        .name = "a badge on a checkpoint's first look with none seen before it is the last checkpoint's save",
        .input = {.looks = {
            {.step = "first_evening", .badge = false, .at = 0ms}, {.step = ""},
            {.step = "out_of_room_202", .badge = true, .at = 1300ms}, {.step = "out_of_room_202", .badge = true, .at = 1700ms},
            {.step = "out_of_room_202", .badge = false, .at = 3000ms}, {.step = "leave_room_202", .badge = false, .at = 9000ms},
        }},
        .expectation = {.seen_in = std::nullopt},
    },
};

UnitTestResult check_sightings(const SightingsCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    Pokemon::FakeStoryBackend backend({.looks = {}}, scope);
    for (const SightingLook& entry : test.input.looks){
        if (entry.step.empty()){
            sightings.finish("");
            sightings.restart();
            continue;
        }
        sightings.enter(entry.step);
        sightings.note(backend, entry.badge ? reading_look(AUTOSAVE_READING, entry.at) : quiet_look(entry.at));
    }
    if (sightings.seen_in() != test.expectation.seen_in){
        return std::format("expected seen in \"{}\", got \"{}\"", test.expectation.seen_in.value_or("none"), sightings.seen_in().value_or("none"));
    }
    return true;
}

struct AutosaveStepCase{
    std::string name;
    struct Input{ std::vector<std::string> steps; bool marked_autosave = false; std::optional<std::string> autosave_step = std::nullopt; };
    Input input;
    struct Expectation{ std::string error; };
    Expectation expectation;
};

const AutosaveStepCase AUTOSAVE_STEP_CASES[] = {
    {
        .name = "an autosave before steps that move the player fails",
        .input = {.steps = {"wild_zones_tutorial", "to_zone_1_gate", "enter_zone_1"}, .autosave_step = "wild_zones_tutorial"},
        .expectation = {.error =
            "03.06: the game autosaves in wild_zones_tutorial, before to_zone_1_gate: the next checkpoint reloads "
            "that save, so end 03.06 at wild_zones_tutorial and start the next one with to_zone_1_gate"
        },
    },
    {
        .name = "an autosave in the last step passes",
        .input = {.steps = {"toward_zone_1_gate", "wild_zones_tutorial"}, .marked_autosave = true, .autosave_step = "wild_zones_tutorial"},
        .expectation = {.error = ""},
    },
};

UnitTestResult check_autosave_step(const AutosaveStepCase& test, CancellableScope&){
    StoryCheckpoint checkpoint{.id = "03.06", .title = "Test"};
    for (const std::string& name : test.input.steps){
        checkpoint.steps.push_back(StoryStep{.name = name});
    }
    const std::string got = autosave_step_error(checkpoint, test.input.marked_autosave, test.input.autosave_step).value_or("");
    return got == test.expectation.error ? UnitTestResult(true) : UnitTestResult(UnitTestResult::FAILED, std::format("expected \"{}\", got \"{}\"", test.expectation.error, got));
}

//  --- Core built-in step runs ---

struct RunCase{
    std::string name;
    struct Input{ std::string checkpoint; std::string step; std::vector<ScreenObservation> looks; };
    Input input;
    struct Expectation{ std::string verdict; std::optional<std::string> inputs = std::nullopt; std::string line = {}; };
    Expectation expectation;
};

const RunCase RUN_CASES[] = {
    {
        .name = "01.00b's opening fails with no autosave",
        .input = {.checkpoint = "01.00b", .step = "opening", .looks = {PLATFORM}},
        .expectation = {.verdict = "failed at opening: platform_not_seen"},
    },
    {
        //  console B, 2026-11-19: a Plus during the confirm fade did nothing.
        .name = "01.00b holds Plus 0.75 s after the opening fades in and ends on its autosave",
        .input = {.checkpoint = "01.00b", .step = "opening", .looks = {BLACK_FADE, CUTSCENE, quiet_look(750ms), OWN_AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {.verdict = "done", .inputs = "wait 300ms, wait 300ms, press +, wait 300ms, wait 300ms, wait 300ms", .line = "[Save] 01.00b: the game autosaved (seen)"},
    },
    {
        .name = "01.01's platform check is passive on dialog and ignores a hint under dialog",
        .input = {.checkpoint = "01.01", .step = "on_platform", .looks = {
            look({ScreenEvent::DIALOG_BOX}, {ADVANCE_ARROW_READING}), look({ScreenEvent::DIALOG_BOX}, {CONTROLS_HINT_READING}), PLATFORM, PLATFORM,
        }},
        .expectation = {.verdict = "done", .inputs = "wait 300ms, wait 300ms, wait 300ms"},
    },
    {
        .name = "01.01's walk does not start in a text box",
        .input = {.checkpoint = "01.01", .step = "platform_to_plaza", .looks = {DIALOG}},
        .expectation = {.verdict = "failed at platform_to_plaza: walk_not_free"},
    },
    {
        .name = "01.02's choice fails when the highlight does not move",
        .input = {.checkpoint = "01.02", .step = "battle_with_pokemon", .looks = {choice(0)}},
        .expectation = {.verdict = "failed at battle_with_pokemon: choice_not_reached"},
    },
    {
        //  01.03 on console B 2026-11-19 & 2026-11-20: hold legs past Taunie's mid-walk prompt, press A after.
        .name = "01.03's walk to the starter ignores a mid-walk prompt and talks after its legs",
        .input = {.checkpoint = "01.03", .step = "approach_starter", .looks = concat(
            std::vector<ScreenObservation>(5, PROMPT_ONLY), concat(std::vector<ScreenObservation>(4, A_PROMPT), {DIALOG})
        )},
        .expectation = {.verdict = "done"},
    },
};

UnitTestResult check_run(const RunCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    PlzaConsoleHooks tools{.sightings = &sightings};
    tools.battle = [](PlzaBackend&) -> std::optional<Pokemon::StoryFailure>{ return std::nullopt; };
    Pokemon::StoryGame game = plza_story_game(builtin_story_data(), sightings);
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    Pokemon::RunState state;
    std::vector<StoryCheckpoint> rows;
    rows.push_back(named_step_row(test.input.checkpoint, test.input.step, tools));
    if (rows[0].steps.size() != 1){
        return std::format("no step {} in {}", test.input.step, test.input.checkpoint);
    }
    note_autosaves(rows, sightings);
    const std::string verdict = verdict_of(Pokemon::run_checkpoints(backend, rows, game, state), rows, /*with_checkpoint=*/false);
    return verify_outcome(verdict, test.expectation.verdict, backend, test.expectation.inputs, test.expectation.line);
}

//  --- Scene machine & plus_skip behavior invariants ---

struct SceneBehaviorCase{
    std::string name;
    struct Input{
        std::string action = "scene";
        std::string button = "A";
        bool stop_at_choice = false;
        bool battle_start = false;
        std::optional<int64_t> start_window_ms = std::nullopt;
        std::optional<size_t> lock_on_hits_at = std::nullopt;
        std::vector<ScreenObservation> looks;
    };
    Input input;
    struct Expectation{
        std::string verdict;
        std::optional<std::string> inputs = std::nullopt;
        size_t lock_on_watches = 0;
    };
    Expectation expectation;
};

const SceneBehaviorCase SCENE_CASES[] = {
    {
        .name = "B on text, passive on fades, A on tutorial card, done on held control",
        .input = {.button = "B", .looks = {
            event_look(ScreenEvent::DIALOG_BOX, 0ms), event_look(ScreenEvent::FADE_WHITE, 400ms),
            reading_look(TUTORIAL_CARD_READING, 800ms), event_look(ScreenEvent::FADE_BLACK, 1200ms),
            party_bar(1600ms), party_bar(2700ms),
        }},
        .expectation = {.verdict = "done", .inputs = "press B, wait 250ms, press A, wait 250ms, wait 250ms"},
    },
    {
        //  02.05 elevator, console B 2026-11-19: stop_at_choice presses nothing on quiet frames or choice.
        .name = "stop_at_choice advances text, waits through quiet frames, and stops at choice",
        .input = {.button = "B", .stop_at_choice = true, .looks = {
            event_look(ScreenEvent::DIALOG_BOX, 0ms), quiet_look(400ms), choice(/*selected=*/0, /*rows=*/2, /*at=*/800ms),
        }},
        .expectation = {.verdict = "done", .inputs = "press B, wait 250ms"},
    },
    {
        .name = "2-row choice moves Up to row 0 and confirms with A, never B",
        .input = {.button = "B", .looks = {
            choice(/*selected=*/1, /*rows=*/2, /*at=*/0ms), choice(/*selected=*/0, /*rows=*/2, /*at=*/400ms),
            party_bar(800ms), party_bar(1900ms),
        }},
        .expectation = {.verdict = "done", .inputs = "press Up, press A, wait 250ms"},
    },
    {
        .name = "HUD held past start_window with no scene fails with scene_never_started",
        .input = {.start_window_ms = 45000, .looks = {
            look({ScreenEvent::PLAYER_CAN_MOVE}, {CONTROLS_HINT_READING}, 0ms),
            look({ScreenEvent::PLAYER_CAN_MOVE}, {CONTROLS_HINT_READING}, 45000ms),
        }},
        .expectation = {.verdict = "failed at scene: scene_never_started", .inputs = "wait 250ms"},
    },
    {
        .name = "A prompt held ends without pressing A, and later text switches to B",
        .input = {.looks = {
            event_look(ScreenEvent::DIALOG_BOX, 0ms), event_look(ScreenEvent::INTERACT_PROMPT, 400ms),
            event_look(ScreenEvent::DIALOG_BOX, 800ms), event_look(ScreenEvent::INTERACT_PROMPT, 1200ms),
            event_look(ScreenEvent::INTERACT_PROMPT, 2300ms),
        }},
        .expectation = {.verdict = "done", .inputs = "press A, wait 250ms, press B, wait 250ms"},
    },
    {
        .name = "battle_start off-camera recovers via lock-on turn, L, and second ZL watch",
        .input = {.battle_start = true, .lock_on_hits_at = 2, .looks = {
            look({ScreenEvent::DIALOG_BOX}, {ADVANCE_ARROW_READING}, 0ms), event_look(ScreenEvent::FADE_BLACK, 400ms),
            quiet_look(1000ms), party_bar(2000ms), party_bar(3100ms), party_bar(5200ms), party_bar(7500ms),
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "press A, wait 250ms, wait 250ms, wait 250ms, stick 90deg 150ms walk, release 100ms, press L",
            .lock_on_watches = 2,
        },
    },
    {
        .name = "battle_start ending in control for 5 s with no lock-on watch fails",
        .input = {.battle_start = true, .looks = {
            event_look(ScreenEvent::FADE_BLACK, 0ms), party_bar(1000ms), party_bar(3000ms), party_bar(6100ms),
        }},
        .expectation = {.verdict = "failed at scene: scene_ended_in_control"},
    },
    {
        //  console B, 2026-11-19: Plus after the opening fade skips 01.01/02.02 cutscenes.
        .name = "plus_skip holds Plus after the opening fade ends and finishes on the skip fade",
        .input = {.action = "plus_skip", .looks = {BLACK_FADE, CUTSCENE, BLACK_FADE}},
        .expectation = {.verdict = "done", .inputs = "wait 250ms, wait 2500ms, press +"},
    },
    {
        .name = "plus_skip fails with cutscene_not_skipped when no black fade follows Plus",
        .input = {.action = "plus_skip", .looks = {
            event_look(ScreenEvent::FADE_BLACK, 0ms), quiet_look(500ms), quiet_look(500ms), quiet_look(6000ms),
        }},
        .expectation = {.verdict = "failed at plus_skip: cutscene_not_skipped", .inputs = "wait 250ms, wait 2500ms, press +, wait 250ms"},
    },
};

JsonObject scene_case_args(const SceneBehaviorCase::Input& input){
    JsonObject args;
    if (input.action != "scene"){
        return args;
    }
    if (input.button != "A"){
        args["button"] = input.button;
    }
    if (input.stop_at_choice){
        args["stop_at_choice"] = true;
    }
    if (input.battle_start){
        args["battle_start"] = true;
    }
    if (input.start_window_ms){
        args["start_window_ms"] = *input.start_window_ms;
    }
    return args;
}

UnitTestResult check_scene_behavior(const SceneBehaviorCase& test, CancellableScope& scope){
    size_t watches = 0;
    AutosaveSightings sightings;
    PlzaConsoleHooks tools{.sightings = &sightings};
    if (test.input.lock_on_hits_at){
        const size_t hit = *test.input.lock_on_hits_at;
        tools.lock_on = [&watches, hit](Milliseconds){ return ++watches >= hit; };
    }
    Pokemon::StoryGame game = plza_story_game(MarkedCheckpoints{}, sightings);
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    Pokemon::RunState state;
    std::vector<StoryCheckpoint> rows;
    rows.push_back(one_step_row("01.01", test.input.action, scene_case_args(test.input), tools));
    const std::string verdict = verdict_of(Pokemon::run_checkpoints(backend, rows, game, state), rows, /*with_checkpoint=*/false);
    const UnitTestResult checked = verify_outcome(verdict, test.expectation.verdict, backend, test.expectation.inputs, "");
    if (checked.result != UnitTestResult::PASSED){
        return checked;
    }
    if (watches != test.expectation.lock_on_watches){
        return std::format("expected {} lock-on watches, got {}", test.expectation.lock_on_watches, watches);
    }
    return true;
}

//  --- Battle-on-screen jump ---

struct BattleJumpCase{
    std::string name;
    struct Input{ size_t first_step; size_t steps; std::vector<ScreenObservation> looks; };
    Input input;
    struct Expectation{ std::string inputs; std::string line; };
    Expectation expectation;
};

const BattleJumpCase BATTLE_JUMP_CASES[] = {
    {
        .name = "a flickering battle HUD in the plaza scene jumps to the first story battle",
        .input = {.first_step = 1, .steps = 4, .looks = {
            event_look(ScreenEvent::FADE_BLACK, 0ms), event_look(ScreenEvent::BATTLE_STARTED, 400ms),
            quiet_look(800ms), event_look(ScreenEvent::BATTLE_STARTED, 1200ms),
        }},
        .expectation = {
            .inputs = "press A, press A, press A",
            .line = "[Event] 03.02b: battle on screen during plaza_scene: jumping to battle_totodile, skipping to_taunie, taunie_battle_start",
        },
    },
    {
        .name = "a battle HUD at the walk to Taunie jumps to the story battle with no walk",
        .input = {.first_step = 2, .steps = 3, .looks = {
            event_look(ScreenEvent::BATTLE_STARTED, 0ms), party_bar(400ms), event_look(ScreenEvent::BATTLE_STARTED, 800ms),
        }},
        .expectation = {
            .inputs = "wait 250ms, wait 250ms",
            .line = "[Event] 03.02b: battle on screen during to_taunie: jumping to battle_totodile, skipping taunie_battle_start",
        },
    },
};

UnitTestResult check_battle_jump(const BattleJumpCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    PlzaConsoleHooks hooks{.sightings = &sightings};
    hooks.battle = [](PlzaBackend&) -> std::optional<Pokemon::StoryFailure>{ return std::nullopt; };
    Pokemon::StoryGame game = plza_story_game(builtin_story_data(), sightings);
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    Pokemon::RunState state;
    std::vector<StoryCheckpoint> rows;
    rows.push_back(sliced_row("03.02b", test.input.first_step, test.input.steps, hooks));
    note_autosaves(rows, sightings);
    const std::string verdict = verdict_of(Pokemon::run_checkpoints(backend, rows, game, state), rows, /*with_checkpoint=*/false);
    return verify_outcome(verdict, "done", backend, test.expectation.inputs, test.expectation.line);
}

//  --- Checkpoint chaining & reload verification ---

Pokemon::StoryGame game_with_reload(const MarkedCheckpoints& rows, AutosaveSightings& sightings){
    Pokemon::StoryGame game = plza_story_game(rows, sightings);
    game.reload = [](PlzaBackend& backend, const StoryCheckpoint& failed) -> std::optional<std::string>{
        backend.press(BUTTON_HOME, /*hold=*/80ms, /*release=*/80ms);
        return failed.id;
    };
    return game;
}

JsonObject choice_scene_args(bool stop_at_choice){
    JsonObject args;
    if (stop_at_choice){
        args["button"] = "B";
        args["stop_at_choice"] = true;
    }
    return args;
}

struct ChainCase{
    std::string name;
    struct Input{
        std::vector<ScreenObservation> looks;
        std::vector<std::string> first_actions = {"wait_for_platform"};
        std::string second_action = "scene";
        bool second_stops_at_choice = true;
        size_t attempts = 1;
        bool reload_when_chained = false;
        StartSignature landed = {};
        std::optional<StartSignature> start = std::nullopt;
    };
    Input input;
    struct Expectation{
        std::string verdict;
        size_t reloads = 0;
        std::string line = {};
        bool nothing_pressed_after_reload = false;
    };
    Expectation expectation;
};

const std::vector<ScreenObservation> ROOF_SAVED = {CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, PLATFORM, PLATFORM};
const std::vector<ScreenObservation> AFTER_RELOAD = {DIALOG, party_bar(1000ms), party_bar(3000ms)};
const StartSignature STATION_FRONT{.talk_prompt = false, .party = 1};

const ChainCase CHAIN_CASES[] = {
    {
        .name = "autosave seen: on from the live screen without a reload",
        .input = {.looks = {CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, choice(0)}},
        .expectation = {.verdict = "done", .reloads = 0},
    },
    {
        .name = "an autosave before the last step does not save the row",
        .input = {.looks = {CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, PLATFORM, PLATFORM}, .first_actions = {"wait_for_platform", "wait_for_platform"}, .second_action = "wait_for_platform"},
        .expectation = {.verdict = "done", .reloads = 0, .line = "[Save] 01.01: not saved (the game autosaved in wait_for_platform, before wait_for_platform_again: a reload would skip wait_for_platform_again)"},
    },
    {
        .name = "a run that autosaved and then failed does not reload: the save moved past its start",
        .input = {.looks = {CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, CUTSCENE, reading_look(AUTOSAVE_READING, 10000ms), CUTSCENE}, .second_action = "wait_for_platform", .attempts = 2},
        .expectation = {.verdict = "failed at 01.02 chain: start_unknown", .reloads = 0, .line = "[Save] 01.02: the game autosaved in wait_for_platform before the run failed: the latest save is past 01.02's start"},
    },
    {
        .name = "reload when chained: waits for control after the row before's last scene, reloads, lands at the start and runs",
        .input = {.looks = concat(ROOF_SAVED, concat({BLACK_FADE, BLACK_FADE, IN_CONTROL, IN_CONTROL}, AFTER_RELOAD)), .first_actions = {"wait_for_platform", "wait_for_platform"}, .second_action = "scene", .second_stops_at_choice = false, .reload_when_chained = true, .landed = STATION_FRONT, .start = STATION_FRONT},
        .expectation = {.verdict = "done", .reloads = 1, .line = "[Checkpoint] 01.02: control is back after 01.01's last scene; reloading the latest save"},
    },
    {
        .name = "reload when chained: control never comes back, no reload",
        .input = {.looks = concat(ROOF_SAVED, {BLACK_FADE}), .first_actions = {"wait_for_platform", "wait_for_platform"}, .second_action = "scene", .second_stops_at_choice = false, .reload_when_chained = true, .landed = STATION_FRONT, .start = STATION_FRONT},
        .expectation = {.verdict = "failed at 01.02 chain: walk_not_free", .reloads = 0},
    },
    {
        .name = "reload when chained: the reload lands elsewhere, stops with nothing pressed",
        .input = {.looks = concat(ROOF_SAVED, concat({IN_CONTROL, IN_CONTROL}, AFTER_RELOAD)), .first_actions = {"wait_for_platform", "wait_for_platform"}, .second_action = "scene", .second_stops_at_choice = false, .reload_when_chained = true, .landed = {.talk_prompt = true, .party = 1}, .start = STATION_FRONT},
        .expectation = {.verdict = "failed at 01.02 chain: reload_off_start", .reloads = 1, .line = "[Checkpoint] 01.02: the reload landed somewhere other than its start: the screen reads (talk prompt, party 1), the start is (no talk prompt, party 1); nothing pressed after the reload", .nothing_pressed_after_reload = true},
    },
    {
        .name = "reload when chained: no start signature to check the landing against, stops with nothing pressed",
        .input = {.looks = concat(ROOF_SAVED, concat({IN_CONTROL, IN_CONTROL}, AFTER_RELOAD)), .first_actions = {"wait_for_platform", "wait_for_platform"}, .second_action = "scene", .second_stops_at_choice = false, .reload_when_chained = true, .landed = STATION_FRONT},
        .expectation = {.verdict = "failed at 01.02 chain: reload_unchecked", .reloads = 1, .nothing_pressed_after_reload = true},
    },
};

UnitTestResult check_chain(const ChainCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    Pokemon::StoryGame game = game_with_reload(MarkedCheckpoints{}, sightings);
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    Pokemon::RunState state;
    PlzaConsoleHooks tools{.sightings = &sightings};
    std::vector<StoryCheckpoint> rows;
    rows.push_back(one_step_row("01.01", test.input.first_actions[0], JsonObject(), tools));
    for (size_t i = 1; i < test.input.first_actions.size(); i++){
        rows[0].steps.push_back(plan_step(std::format("{}_again", test.input.first_actions[i]), test.input.first_actions[i], JsonObject(), tools));
    }
    const bool stop_at_choice = test.input.second_action == "scene" && test.input.second_stops_at_choice;
    rows.push_back(one_step_row("01.02", test.input.second_action, choice_scene_args(stop_at_choice), tools));
    note_autosaves(rows, sightings);
    std::vector<CheckpointStart> starts;
    if (test.input.start){
        starts.push_back(CheckpointStart{.checkpoint = "01.02", .signature = *test.input.start});
    }
    CheckpointRunOptions options{.attempts_per_checkpoint = test.input.attempts, .starts = starts};
    options.read_start = [&test](const StartSignature&){ return test.input.landed; };
    if (test.input.reload_when_chained){
        options.reload_when_chained = {"01.02"};
    }
    const std::string verdict = verdict_of(run_plza_checkpoints(game, rows, options, backend, state, sightings), rows, /*with_checkpoint=*/true);
    const UnitTestResult checked = verify_outcome(verdict, test.expectation.verdict, backend, std::nullopt, test.expectation.line);
    if (checked.result != UnitTestResult::PASSED){
        return checked;
    }
    const std::vector<std::string>& inputs = backend.inputs();
    const size_t reloads = (size_t)std::count(inputs.begin(), inputs.end(), "press HOME");
    if (reloads != test.expectation.reloads){
        return std::format("expected {} reloads, got {}: {}", test.expectation.reloads, reloads, joined(inputs, ", "));
    }
    const auto home = std::find(inputs.rbegin(), inputs.rend(), "press HOME");
    if (test.expectation.nothing_pressed_after_reload && std::any_of(inputs.rbegin(), home, [](const std::string& in){ return in.starts_with("press"); })){
        return std::format("expected no press after the reload: {}", joined(inputs, ", "));
    }
    return true;
}

//  --- Chain start signature guard ---

const std::vector<ScreenObservation> CHAIN_LOOKS = {PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE};
const StartSignature OUT_OF_FAR_GATE{.wild_zone = false, .party = 5};

struct ChainStartCase{
    std::string name;
    struct Input{ std::optional<StartSignature> start; StartSignature screen; };
    Input input;
    struct Expectation{ std::string verdict; std::string line = {}; };
    Expectation expectation;
};

const ChainStartCase CHAIN_START_CASES[] = {
    {
        .name = "the live screen is not at the row's start: stops there",
        .input = {.start = OUT_OF_FAR_GATE, .screen = {.talk_prompt = false, .wild_zone = true, .party = 5}},
        .expectation = {.verdict = "failed at 01.02 chain: chain_off_start", .line = "[Checkpoint] 01.02: START CHECK FAILED: the screen reads (no talk prompt, in a wild zone, party 5), its start is (not in a wild zone, party 5); nothing run from there"},
    },
    {
        .name = "a field of the row's start not read: stops there",
        .input = {.start = OUT_OF_FAR_GATE, .screen = {.talk_prompt = false, .party = 5}},
        .expectation = {.verdict = "failed at 01.02 chain: chain_off_start"},
    },
    {
        .name = "the row names no start: on, unchecked",
        .input = {.start = std::nullopt, .screen = {}},
        .expectation = {.verdict = "done", .line = "[Checkpoint] 01.02: START UNCHECKED: its plan names no start signature; on from the live screen (autosave before it not seen)"},
    },
};

UnitTestResult check_chain_start(const ChainStartCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    Pokemon::StoryGame game = plza_story_game(MarkedCheckpoints{}, sightings);
    Pokemon::FakeStoryBackend backend({.looks = CHAIN_LOOKS}, scope);
    PlzaConsoleHooks tools;
    std::vector<StoryCheckpoint> rows;
    rows.push_back(one_step_row("01.01", "wait_for_platform", JsonObject(), tools));
    rows.push_back(StoryCheckpoint{.id = "01.02"});
    std::vector<CheckpointStart> starts;
    if (test.input.start){
        starts.push_back(CheckpointStart{.checkpoint = "01.02", .signature = *test.input.start});
    }
    CheckpointRunOptions options{.starts = starts};
    options.read_start = [&test](const StartSignature&){ return test.input.screen; };
    Pokemon::RunState state;
    const std::string verdict = verdict_of(run_plza_checkpoints(game, rows, options, backend, state, sightings), rows, /*with_checkpoint=*/true);
    return verify_outcome(verdict, test.expectation.verdict, backend, std::nullopt, test.expectation.line);
}

template <typename Case, UnitTestResult (*check)(const Case&, CancellableScope&), size_t N>
void register_cases(UnitTestDatabase& database, std::string_view kind, const Case (&cases)[N]){
    for (const Case& test : cases){
        database.add<CaseTest<Case, check>>(kind, test);
    }
}

} // namespace


void add_tests_AutoStoryGame(UnitTestDatabase& database){
    register_cases<BuiltinStoryCase, check_builtin_story>(database, "builtin plans - ", BUILTIN_STORY_CASES);
    register_cases<SceneArgsCase, check_scene_args>(database, "scene args - ", SCENE_ARGS_CASES);
    register_cases<SightingsCase, check_sightings>(database, "autosave sightings - ", SIGHTINGS_CASES);
    register_cases<AutosaveStepCase, check_autosave_step>(database, "autosave step - ", AUTOSAVE_STEP_CASES);
    register_cases<RunCase, check_run>(database, "run - ", RUN_CASES);
    register_cases<SceneBehaviorCase, check_scene_behavior>(database, "scene - ", SCENE_CASES);
    register_cases<BattleJumpCase, check_battle_jump>(database, "battle on screen - ", BATTLE_JUMP_CASES);
    register_cases<ChainCase, check_chain>(database, "chain - ", CHAIN_CASES);
    register_cases<ChainStartCase, check_chain_start>(database, "chain start - ", CHAIN_START_CASES);
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

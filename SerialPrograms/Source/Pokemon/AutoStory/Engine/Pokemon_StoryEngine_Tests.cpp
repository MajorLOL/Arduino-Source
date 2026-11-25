/*  Story Engine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The engine on a FakeStoryBackend: a test game whose actions answer as
 *  scripted (pass, fail, fail once, press, wait for a white fade in the
 *  scripted looks), so a run's report, inputs and retries are checked with
 *  no console.
 *
 */

#include <format>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryEngine_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

using namespace std::chrono_literals;


//  What the test game's actions share across a run.
struct TestWorld{
    //  How many times "fail_once" ran.
    size_t fail_once_calls = 0;
};

//  Actions exercising jump-ahead and RunState::actions_ahead.
void add_jump_actions(StoryActions& actions){
    actions.emplace("jump_to_press_a", [](const JsonObject&) -> StoryStepRun{
        return [](StoryBackend&, RunState&){
            return std::optional<StoryFailure>(StoryFailure{
                .code = "jumped",
                .detail = "ahead on screen",
                .resume_at_action = "press_a",
            });
        };
    });
    actions.emplace("log_ahead", [](const JsonObject&) -> StoryStepRun{
        return [](StoryBackend& backend, RunState& state){
            std::string text;
            for (const std::string& action : state.actions_ahead){
                if (!text.empty()){
                    text += ", ";
                }
                text += action;
            }
            backend.log(std::format("ahead: {}", text));
            return std::optional<StoryFailure>();
        };
    });
}

//  The test game's actions; none takes args.
StoryActions test_actions(TestWorld& world){
    StoryActions actions;
    actions.emplace("ok", [](const JsonObject&) -> StoryStepRun{
        return [](StoryBackend&, RunState&){
            return std::optional<StoryFailure>();
        };
    });
    actions.emplace("fail", [](const JsonObject&) -> StoryStepRun{
        return [](StoryBackend&, RunState&){
            return std::optional<StoryFailure>(StoryFailure{.code = "scripted_failure"});
        };
    });
    actions.emplace("fail_once", [&world](const JsonObject&) -> StoryStepRun{
        return [&world](StoryBackend&, RunState&){
            return world.fail_once_calls++ == 0 ? std::optional<StoryFailure>(StoryFailure{.code = "scripted_failure"}) : std::nullopt;
        };
    });
    actions.emplace("not_running", [](const JsonObject&) -> StoryStepRun{
        return [](StoryBackend&, RunState&){
            return std::optional<StoryFailure>(game_not_running_failure());
        };
    });
    actions.emplace("press_a", [](const JsonObject&) -> StoryStepRun{
        return [](StoryBackend& backend, RunState&){
            backend.press(NintendoSwitch::BUTTON_A, /*hold=*/80ms, /*release=*/80ms);
            return std::optional<StoryFailure>();
        };
    });
    //  Walks north until a look shows a white fade, at most 3 slices.
    actions.emplace("walk_to_fade", [](const JsonObject&) -> StoryStepRun{
        return [](StoryBackend& backend, RunState&){
            for (size_t slice = 0; slice < 3; slice++){
                backend.stick(/*degrees=*/0, /*duration=*/500ms, /*release=*/0ms, StoryPace::WALK);
                if (backend.observe().events.has(ScreenEvent::FADE_WHITE)){
                    return std::optional<StoryFailure>();
                }
            }
            return std::optional<StoryFailure>(StoryFailure{.code = "no_fade", .detail = "no white fade in 3 slices"});
        };
    });
    add_jump_actions(actions);
    return actions;
}

//  The test game. Its save fails at the checkpoints of `save_failures`; its
//  reload presses HOME and answers `reload_start`.
StoryGame test_game(std::set<std::string> save_failures, std::optional<std::string> reload_start){
    StoryGame game;
    game.save = [save_failures = std::move(save_failures)](StoryBackend&, const StoryCheckpoint& checkpoint){
        if (save_failures.contains(checkpoint.id)){
            return std::optional<StoryFailure>(StoryFailure{.code = "save_error", .detail = "the disk is full"});
        }
        return std::optional<StoryFailure>();
    };
    game.reload = [reload_start = std::move(reload_start)](StoryBackend& backend, const StoryCheckpoint&){
        backend.press(NintendoSwitch::BUTTON_HOME, /*hold=*/80ms, /*release=*/80ms);
        return reload_start;
    };
    return game;
}

//  A checkpoint `id` whose steps are named for their actions.
struct TestCheckpoint{
    std::string id = {};
    std::vector<std::string> actions = {};
};

std::vector<StoryCheckpoint> checkpoints_of(const std::vector<TestCheckpoint>& tests, TestWorld& world){
    StoryActions actions = test_actions(world);
    std::vector<StoryCheckpoint> checkpoints;
    for (const TestCheckpoint& test : tests){
        StoryCheckpoint checkpoint{.id = test.id, .title = "Test", .steps = {}};
        for (const std::string& action : test.actions){
            checkpoint.steps.push_back(StoryStep{.name = action, .why = "", .action = action, .run = actions.find(action)->second(JsonObject())});
        }
        checkpoints.push_back(std::move(checkpoint));
    }
    return checkpoints;
}

//  A look that shows `events`.
ScreenObservation look(ScreenEvents events){
    ScreenObservation observation;
    observation.events = events;
    return observation;
}

//  "a b": the strings, space-separated.
std::string joined(const std::vector<std::string>& texts, std::string_view separator = " "){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : std::string(separator)) + part;
    }
    return text;
}

//  "done" or "failed at 01.01 fail: scripted_failure" (no step name when the
//  checkpoint failed outside its steps).
std::string verdict(const std::optional<CheckpointFailure>& failure, const std::vector<StoryCheckpoint>& checkpoints){
    if (!failure){
        return "done";
    }
    std::string event;
    for (const StoryCheckpoint& checkpoint : checkpoints){
        if (checkpoint.id == failure->checkpoint_id && failure->event_index){
            event = checkpoint.steps[*failure->event_index].name;
        }
    }
    return std::format("failed at {} {}: {}", failure->checkpoint_id, event, failure->failure.code);
}


struct RangeCase{
    std::string name;
    struct Input{
        std::vector<TestCheckpoint> checkpoints = {};
        size_t attempts = 1;
        std::set<std::string> save_failures = {};
        //  What the reload answers.
        std::optional<std::string> reload_start = std::nullopt;
        std::vector<ScreenObservation> looks = {};
    };
    Input input;
    struct Expectation{
        //  verdict() of the run.
        std::string verdict = {};
        //  The backend's inputs, joined by ", ".
        std::string inputs = {};
        //  A log line the run wrote, if any is checked.
        std::string line = {};
    };
    Expectation expectation;
};

const RangeCase RANGE_CASES[] = {
    {
        .name = "two checkpoints pass",
        .input = {.checkpoints = {{"01.00", {"ok", "press_a"}}, {"01.01", {"ok"}}}},
        .expectation = {.verdict = "done", .inputs = "press A", .line = "[Checkpoint] 01.01: passed on attempt 1 (one-shot)"},
    },
    {
        .name = "a save that fails is logged and the run goes on",
        .input = {.checkpoints = {{"01.00", {"ok"}}, {"01.01", {"press_a"}}}, .save_failures = {"01.00"}},
        .expectation = {.verdict = "done", .inputs = "press A", .line = "[Save] 01.00: save failed: save_error: the disk is full"},
    },
    {
        .name = "a failed step stops the run at its event",
        .input = {.checkpoints = {{"01.00", {"ok", "fail", "press_a"}}}},
        .expectation = {.verdict = "failed at 01.00 fail: scripted_failure", .inputs = "", .line = "[Checkpoint] 01.00: failed at 2 fail: scripted_failure"},
    },
    {
        .name = "a failed checkpoint passes after a reload to its start",
        .input = {.checkpoints = {{"01.01", {"fail_once"}}}, .attempts = 3, .reload_start = "01.01"},
        .expectation = {.verdict = "done", .inputs = "press HOME", .line = "[Checkpoint] 01.01: passed on attempt 2 (1 reset)"},
    },
    {
        .name = "a reload to an earlier checkpoint's start replays it",
        .input = {
            .checkpoints = {{"01.00", {"press_a"}}, {"01.01", {"fail_once"}}},
            .attempts = 2,
            .reload_start = "01.00",
        },
        .expectation = {.verdict = "done", .inputs = "press A, press HOME, press A", .line = "[Checkpoint] 01.01: passed on attempt 2 (1 reset)"},
    },
    {
        .name = "a checkpoint stops the run after its last attempt fails",
        .input = {.checkpoints = {{"01.01", {"fail"}}}, .attempts = 2, .reload_start = "01.01"},
        .expectation = {.verdict = "failed at 01.01 fail: scripted_failure", .inputs = "press HOME"},
    },
    {
        .name = "a reload to an unknown start stops the run",
        .input = {.checkpoints = {{"01.01", {"fail"}}}, .attempts = 3},
        .expectation = {.verdict = "failed at 01.01 : start_unknown", .inputs = "press HOME", .line = "[Checkpoint] 01.01: reloaded to no start of this run"},
    },
    {
        .name = "a failure that repeats stops the run with no reload",
        .input = {.checkpoints = {{"01.01", {"not_running"}}}, .attempts = 3, .reload_start = "01.01"},
        .expectation = {.verdict = "failed at 01.01 not_running: game_not_running", .inputs = ""},
    },
    {
        .name = "a walk ends on the scripted white fade",
        .input = {.checkpoints = {{"01.01", {"walk_to_fade"}}}, .looks = {look({}), look({ScreenEvent::FADE_WHITE})}},
        .expectation = {.verdict = "done", .inputs = "stick 0deg 500ms walk, stick 0deg 500ms walk"},
    },
    {
        .name = "a walk whose fade never shows fails",
        .input = {.checkpoints = {{"01.01", {"walk_to_fade"}}}, .looks = {look({ScreenEvent::DIALOG_BOX})}},
        .expectation = {
            .verdict = "failed at 01.01 walk_to_fade: no_fade",
            .inputs = "stick 0deg 500ms walk, stick 0deg 500ms walk, stick 0deg 500ms walk",
            .line = "[Event] 01.01/1 walk_to_fade: failed: no_fade: no white fade in 3 slices",
        },
    },
    {
        .name = "a jump skips intermediate steps to its target action",
        .input = {.checkpoints = {{"01.00", {"jump_to_press_a", "fail", "ok", "press_a"}}}},
        .expectation = {
            .verdict = "done",
            .inputs = "press A",
            .line = "[Event] 01.00: ahead on screen during jump_to_press_a: jumping to press_a, skipping fail, ok",
        },
    },
    {
        .name = "a jump to the immediate next step skips nothing",
        .input = {.checkpoints = {{"01.00", {"jump_to_press_a", "press_a"}}}},
        .expectation = {
            .verdict = "done",
            .inputs = "press A",
            .line = "[Event] 01.00: ahead on screen during jump_to_press_a: jumping to press_a",
        },
    },
    {
        .name = "a jump whose target action is not ahead fails normally",
        .input = {.checkpoints = {{"01.00", {"press_a", "jump_to_press_a", "ok"}}}},
        .expectation = {
            .verdict = "failed at 01.00 jump_to_press_a: jumped",
            .inputs = "press A",
            .line = "[Checkpoint] 01.00: failed at 2 jump_to_press_a: jumped: ahead on screen",
        },
    },
    {
        .name = "actions_ahead lists only future step actions",
        .input = {.checkpoints = {{"01.00", {"press_a", "log_ahead", "ok", "fail"}}}},
        .expectation = {
            .verdict = "failed at 01.00 fail: scripted_failure",
            .inputs = "press A",
            .line = "ahead: fail, ok",
        },
    },
    {
        .name = "actions_ahead is empty on the last step",
        .input = {.checkpoints = {{"01.00", {"press_a", "log_ahead"}}}},
        .expectation = {
            .verdict = "done",
            .inputs = "press A",
            .line = "ahead: ",
        },
    },
};

UnitTestResult check_range(const RangeCase& test, CancellableScope& scope){
    TestWorld world;
    StoryGame game = test_game(test.input.save_failures, test.input.reload_start);
    game.attempts_per_checkpoint = test.input.attempts;
    std::vector<StoryCheckpoint> checkpoints = checkpoints_of(test.input.checkpoints, world);
    FakeStoryBackend backend({.looks = test.input.looks}, scope);
    RunState state;
    const std::optional<CheckpointFailure> failure = run_checkpoints(backend, checkpoints, game, state);
    const std::string got = verdict(failure, checkpoints);
    if (got != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.verdict, got);
    }
    const std::string inputs = joined(backend.inputs(), ", ");
    if (inputs != test.expectation.inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", test.expectation.inputs, inputs);
    }
    if (test.expectation.line.empty()){
        return true;
    }
    for (const std::string& line : backend.lines()){
        if (line == test.expectation.line){
            return true;
        }
    }
    return std::format("expected the line \"{}\" in:\n{}", test.expectation.line, joined(backend.lines(), "\n"));
}


class StoryEngineTest : public UnitTest{
public:
    explicit StoryEngineTest(RangeCase test_case)
        : UnitTest(std::format("Pokemon::StoryEngine - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_range(m_case, scope);
    }

private:
    RangeCase m_case;
};

} // namespace


void add_tests_StoryEngine(UnitTestDatabase& database){
    for (const RangeCase& test : RANGE_CASES){
        database.add<StoryEngineTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation

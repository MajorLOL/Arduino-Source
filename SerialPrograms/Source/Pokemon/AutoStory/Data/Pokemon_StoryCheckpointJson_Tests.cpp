/*  Story Checkpoint JSON Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each case reads a checkpoint file through read_story_checkpoint() with a
 *  test game's actions and runs its steps on a FakeStoryBackend, or expects
 *  the read to fail.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryCheckpointJson.h"
#include "Pokemon_StoryCheckpointJson_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  A "test" game's file around the checkpoint `checkpoint`.
std::string test_file(std::string_view checkpoint){
    return std::format(
        R"({{"format": "pokesim-authored-checkpoint/1", "game": "test", "checkpoint": {}, "execution": {{}}}})",
        checkpoint
    );
}

struct ReadCase{
    std::string name;
    struct Input{
        //  A checkpoint file's JSON, of the "test" game.
        std::string json;
    };
    struct Expectation{
        //  The lines running every step logs, joined by " | "; none: the
        //  read fails.
        std::optional<std::string> log = std::nullopt;
    };
    Input input;
    Expectation expectation;
};

const ReadCase CASES[] = {
    {
        .name = "a parsed step runs its handler with typed args",
        .input = {.json = test_file(R"({
            "id": "x", "title": "X",
            "events": [{"id": "buy", "action": "mart", "args": {"map": "M", "budget": 3000}}]
        })")},
        .expectation = {.log = "mart M 3000"},
    },
    {
        .name = "steps run in plan order",
        .input = {.json = test_file(R"({
            "id": "x", "title": "X",
            "events": [
                {"id": "a", "action": "talk"},
                {"id": "b", "action": "mart", "args": {"map": "M", "budget": 1}}
            ]
        })")},
        .expectation = {.log = "talk | mart M 1"},
    },
    {
        .name = "start and metadata are notes, not read",
        .input = {.json = test_file(R"({
            "id": "x", "title": "X",
            "start": {"tiles": [[1, 2]]},
            "metadata": {"phase": "tested"},
            "events": [{"id": "a", "action": "talk"}]
        })")},
        .expectation = {.log = "talk"},
    },
    {
        .name = "the execution object is left to the game",
        .input = {.json = R"({
            "format": "pokesim-authored-checkpoint/1",
            "game": "test",
            "checkpoint": {"id": "x", "title": "X", "events": [{"id": "a", "action": "talk"}]},
            "execution": {"zero": 0, "false": false, "null": null}
        })"},
        .expectation = {.log = "talk"},
    },
    {
        .name = "an unknown arg key is rejected",
        .input = {.json = test_file(R"({
            "id": "x", "title": "X",
            "events": [{"id": "a", "action": "mart", "args": {"map": "M", "budget": 1, "budgte": 1}}]
        })")},
        .expectation = {},
    },
    {
        .name = "a missing required arg is rejected",
        .input = {.json = test_file(R"({
            "id": "x", "title": "X",
            "events": [{"id": "a", "action": "mart", "args": {"map": "M"}}]
        })")},
        .expectation = {},
    },
    {
        .name = "an unknown action is rejected",
        .input = {.json = test_file(R"({"id": "x", "title": "X", "events": [{"id": "a", "action": "dance"}]})")},
        .expectation = {},
    },
    {
        .name = "a step without an action is rejected",
        .input = {.json = test_file(R"({"id": "x", "title": "X", "events": [{"id": "a"}]})")},
        .expectation = {},
    },
    {
        .name = "the old kind field is rejected",
        .input = {.json = test_file(R"({
            "id": "x", "title": "X",
            "events": [{"id": "a", "kind": "talk", "action": "talk"}]
        })")},
        .expectation = {},
    },
    {
        .name = "args that are not an object are rejected",
        .input = {.json = test_file(R"({
            "id": "x", "title": "X",
            "events": [{"id": "a", "action": "mart", "args": ["map"]}]
        })")},
        .expectation = {},
    },
    {
        .name = "duplicate step ids are rejected",
        .input = {.json = test_file(R"({
            "id": "x", "title": "X",
            "events": [{"id": "a", "action": "talk"}, {"id": "a", "action": "talk"}]
        })")},
        .expectation = {},
    },
    {
        .name = "another game's file is rejected",
        .input = {.json = R"({
            "format": "pokesim-authored-checkpoint/1",
            "game": "frlg",
            "checkpoint": {"id": "x", "title": "X", "events": []},
            "execution": {}
        })"},
        .expectation = {},
    },
    {
        .name = "an unknown format is rejected",
        .input = {.json = R"({
            "format": "future",
            "game": "test",
            "checkpoint": {"id": "x", "title": "X", "events": []},
            "execution": {}
        })"},
        .expectation = {},
    },
    {
        .name = "a file without execution is rejected",
        .input = {.json = R"({
            "format": "pokesim-authored-checkpoint/1",
            "game": "test",
            "checkpoint": {"id": "x", "title": "X", "events": []}
        })"},
        .expectation = {},
    },
    {
        .name = "an extra file field is rejected",
        .input = {.json = R"({
            "format": "pokesim-authored-checkpoint/1",
            "game": "test",
            "checkpoint": {"id": "x", "title": "X", "events": []},
            "execution": {},
            "extra": 0
        })"},
        .expectation = {},
    },
    {
        .name = "an execution that is not an object is rejected",
        .input = {.json = R"({
            "format": "pokesim-authored-checkpoint/1",
            "game": "test",
            "checkpoint": {"id": "x", "title": "X", "events": []},
            "execution": false
        })"},
        .expectation = {},
    },
};


//  A test game's actions: "talk" takes no args; "mart" needs a map and a
//  budget. Each logs its name and args.
StoryActions test_actions(){
    StoryActions actions;
    actions.emplace("talk", [](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, {});
        return [](StoryBackend& backend, RunState&){
            backend.log("talk");
            return std::optional<StoryFailure>();
        };
    });
    actions.emplace("mart", [](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, {"map", "budget"});
        return [map = args.get_string_throw("map"), budget = args.get_integer_throw("budget")](StoryBackend& backend, RunState&){
            backend.log(std::format("mart {} {}", map, budget));
            return std::optional<StoryFailure>();
        };
    });
    return actions;
}

//  The lines running every step of the file `json` logs, joined by " | ";
//  none when the read fails.
std::optional<std::string> run_log(std::string_view json, CancellableScope& scope){
    StoryActions actions = test_actions();
    StoryCheckpoint checkpoint;
    try{
        checkpoint = read_story_checkpoint(parse_json(std::string(json)), "test", "test.json", actions);
    }catch (const ParseException&){
        return std::nullopt;
    }
    FakeStoryBackend backend({}, scope);
    RunState state;
    for (StoryStep& step : checkpoint.steps){
        step.run(backend, state);
    }
    std::string log;
    for (const std::string& line : backend.lines()){
        log += (log.empty() ? "" : " | ") + line;
    }
    return log;
}


class StoryCheckpointJsonTest : public UnitTest{
public:
    explicit StoryCheckpointJsonTest(ReadCase test)
        : UnitTest(std::format("Pokemon::StoryCheckpointJson - {}", test.name))
        , m_case(std::move(test))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::optional<std::string> log = run_log(m_case.input.json, scope);
        if (log == m_case.expectation.log){
            return true;
        }
        return std::format(
            "expected {}, got {}",
            m_case.expectation.log ? std::format("\"{}\"", *m_case.expectation.log) : "a rejected read",
            log ? std::format("\"{}\"", *log) : "a rejected read"
        );
    }

private:
    ReadCase m_case;
};


} // namespace


void add_tests_StoryCheckpointJson(UnitTestDatabase& database){
    for (const ReadCase& test : CASES){
        database.add<StoryCheckpointJsonTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation

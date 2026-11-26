/*  Grid Story Actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each case reads one step from JSON through read_story_checkpoint() with
 *  the grid actions registered, runs it on a scripted backend, and checks
 *  the moves it made or the failure / parse error.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "Pokemon_GridStoryActions.h"
#include "Pokemon_GridStoryActions_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  The scripted backend with tile moves: records each move in order.
class FakeGridBackend : public FakeStoryBackend{
public:
    using FakeStoryBackend::FakeStoryBackend;

    void move_leg(Direction direction, int tiles, StoryPace pace){
        m_moves.push_back(std::format(
            "leg {} {} {}", direction_name(direction), tiles, pace == StoryPace::RUN ? "run" : "walk"
        ));
    }
    void face(Direction direction){
        m_moves.push_back(std::format("face {}", direction_name(direction)));
    }

    const std::vector<std::string>& moves() const{
        return m_moves;
    }

private:
    std::vector<std::string> m_moves = {};
};

//  Which backend a step runs on.
enum class TestBackend{
    GRID,
    PLAIN,  //  the scripted backend with no tile moves
};

struct GridActionCase{
    std::string name = {};
    struct Input{
        //  One step, as a checkpoint's event.
        std::string_view step_json = {};
        TestBackend backend = TestBackend::GRID;
    };
    struct Expectation{
        //  The moves made, then the outcome ("leg north 3 walk / ok");
        //  std::nullopt when loading the step must throw ParseException.
        std::optional<std::string> text = std::nullopt;
    };
    Input input = {};
    Expectation expectation = {};
};

const GridActionCase ACTION_CASES[] = {
    {
        .name = "legs walk each leg in order",
        .input = {.step_json = R"({"id":"w","action":"legs","args":{"legs":[
            {"direction":"north","tiles":3},{"direction":"east","tiles":2}]}})"},
        .expectation = {.text = "leg north 3 walk, leg east 2 walk / ok"},
    },
    {
        .name = "legs with run run every leg",
        .input = {.step_json = R"({"id":"w","action":"legs","args":{"run":true,"legs":[{"direction":"south","tiles":1}]}})"},
        .expectation = {.text = "leg south 1 run / ok"},
    },
    {
        .name = "face turns to the direction",
        .input = {.step_json = R"({"id":"f","action":"face","args":{"direction":"west"}})"},
        .expectation = {.text = "face west / ok"},
    },
    {
        .name = "legs on a backend with no tile moves fail before any input",
        .input = {
            .step_json = R"({"id":"w","action":"legs","args":{"legs":[{"direction":"north","tiles":1}]}})",
            .backend = TestBackend::PLAIN,
        },
        .expectation = {.text = " / unsupported_step"},
    },
    {
        .name = "face rejects unknown arg key at load",
        .input = {.step_json = R"({"id":"f","action":"face","args":{"direction":"west","extra":1}})"},
        .expectation = {.text = std::nullopt},
    },
    {
        .name = "legs rejects bad direction at load",
        .input = {.step_json = R"({"id":"w","action":"legs","args":{"legs":[{"direction":"up","tiles":1}]}})"},
        .expectation = {.text = std::nullopt},
    },
};

struct GridArgParseCase{
    std::string name = {};
    struct Input{
        std::string_view kind = {};
        std::string_view json = {};
    };
    struct Expectation{
        bool parses = false;
    };
    Input input = {};
    Expectation expectation = {};
};

const GridArgParseCase ARG_CASES[] = {
    {
        .name = "grid pose with tiles and facing",
        .input = {.kind = "pose", .json = R"({"map":"PalletTown","tiles":[[3,4],[5,6]],"facing":"north"})"},
        .expectation = {.parses = true},
    },
    {
        .name = "grid pose rejects unknown field",
        .input = {.kind = "pose", .json = R"({"map":"PalletTown","unknown":1})"},
        .expectation = {.parses = false},
    },
    {
        .name = "tile declared accepts map only",
        .input = {.kind = "declared", .json = R"({"map":"Route1"})"},
        .expectation = {.parses = true},
    },
    {
        .name = "story items parses non-negative quantities",
        .input = {.kind = "items", .json = R"([{"item":"Potion","quantity":2}])"},
        .expectation = {.parses = true},
    },
    {
        .name = "story items rejects negative quantity",
        .input = {.kind = "items", .json = R"([{"item":"Potion","quantity":-1}])"},
        .expectation = {.parses = false},
    },
};

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += std::format("{}{}", text.empty() ? "" : ", ", line);
    }
    return text;
}

std::string run_action_case(const GridActionCase::Input& input, CancellableScope& scope){
    StoryActions actions;
    add_grid_story_actions<FakeGridBackend>(actions);
    const std::string json = std::format(
        R"({{"format":"pokesim-authored-checkpoint/1","game":"test","checkpoint":{{"id":"x","title":"X","events":[{}]}},"execution":{{}}}})",
        input.step_json
    );
    StoryCheckpoint checkpoint = read_story_checkpoint(parse_json(json), "test", "test", actions);
    StoryStep& step = checkpoint.steps.at(0);
    RunState state{.step = std::format("x/{}", step.name)};

    FakeGridBackend grid({}, scope);
    FakeStoryBackend plain({}, scope);
    const bool on_grid = input.backend == TestBackend::GRID;
    const std::optional<StoryFailure> failure = on_grid ? step.run(grid, state) : step.run(plain, state);
    return std::format(
        "{} / {}", joined(on_grid ? grid.moves() : plain.inputs()), failure ? failure->code : "ok"
    );
}

class GridStoryActionTest : public UnitTest{
public:
    explicit GridStoryActionTest(GridActionCase test_case)
        : UnitTest(std::format("Pokemon::GridStoryActions - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        try{
            const std::string got = run_action_case(m_case.input, scope);
            if (!m_case.expectation.text){
                return std::format("expected ParseException, got \"{}\"", got);
            }
            if (got != *m_case.expectation.text){
                return std::format("expected \"{}\", got \"{}\"", *m_case.expectation.text, got);
            }
        }catch (const ParseException& error){
            if (m_case.expectation.text){
                return std::format("expected \"{}\", the step did not read: {}", *m_case.expectation.text, error.message());
            }
        }
        return true;
    }

private:
    GridActionCase m_case;
};

class GridStoryArgParseTest : public UnitTest{
public:
    explicit GridStoryArgParseTest(GridArgParseCase test_case)
        : UnitTest(std::format("Pokemon::GridStoryArgs - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        bool parsed = true;
        try{
            const JsonValue value = parse_json(std::string(m_case.input.json));
            if (m_case.input.kind == "pose"){
                read_grid_pose(value, "test");
            }else if (m_case.input.kind == "declared"){
                read_tile_declared(value, "test");
            }else{
                read_story_items(value, "test");
            }
        }catch (const ParseException&){
            parsed = false;
        }
        if (parsed != m_case.expectation.parses){
            return std::format("expected parses={}, got {}", m_case.expectation.parses, parsed);
        }
        return true;
    }

private:
    GridArgParseCase m_case;
};


} // namespace


void add_tests_GridStoryActions(UnitTestDatabase& database){
    for (const GridActionCase& test : ACTION_CASES){
        database.add<GridStoryActionTest>(test);
    }
    for (const GridArgParseCase& test : ARG_CASES){
        database.add<GridStoryArgParseTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation

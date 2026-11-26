/*  Story Log Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryLog.h"
#include "Pokemon_StoryLog_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

// One formatter call and the line it must make.
struct StoryLogTestCase{
    //  The formatter call, made when the test runs.
    struct Input{
        std::string (*format)();
    };

    //  The exact line: log readers and the engine tests match these.
    struct Expectation{
        std::string line;
    };

    std::string name = {};
    Input input = {};
    Expectation expectation = {};
};

class Test_StoryLog : public UnitTest{
public:
    Test_StoryLog(StoryLogTestCase test_case)
        : UnitTest(std::format("Pokemon::StoryLog - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string line = m_case.input.format();
        if (line != m_case.expectation.line){
            return std::format("Expected \"{}\", got \"{}\"", m_case.expectation.line, line);
        }
        return true;
    }

private:
    const StoryLogTestCase m_case;
};

//  SV's text for a checkpoint, which already says "Checkpoint::".
constexpr std::string_view SV_TEXT = "Checkpoint:: 004 - 02. Start point: X";

const StoryLogTestCase STORY_LOG_CASES[] = {
    {
        .name = "checkpoint start",
        .input = {.format = []{ return checkpoint_start_line("00.04"); }},
        .expectation = {.line = "[Checkpoint] 00.04: start"},
    },
    {
        .name = "checkpoint start with its events",
        .input = {.format = []{ return checkpoint_start_line("00.04", /*events=*/3); }},
        .expectation = {.line = "[Checkpoint] 00.04: start, 3 events"},
    },
    {
        .name = "attempt",
        .input = {.format = []{ return checkpoint_attempt_line("00.04", 2); }},
        .expectation = {.line = "[Checkpoint] 00.04: attempt 2"},
    },
    {
        .name = "attempt out of a limit",
        .input = {.format = []{ return checkpoint_attempt_line("00.04", 2, /*attempts=*/3); }},
        .expectation = {.line = "[Checkpoint] 00.04: attempt 2/3"},
    },
    {
        .name = "one-shot pass",
        .input = {.format = []{ return checkpoint_pass_line("00.03", 1); }},
        .expectation = {.line = "[Checkpoint] 00.03: passed on attempt 1 (one-shot)"},
    },
    {
        .name = "pass after one reset",
        .input = {.format = []{ return checkpoint_pass_line("00.04", 2); }},
        .expectation = {.line = "[Checkpoint] 00.04: passed on attempt 2 (1 reset)"},
    },
    {
        .name = "pass after resets",
        .input = {.format = []{ return checkpoint_pass_line("00.04", 3); }},
        .expectation = {.line = "[Checkpoint] 00.04: passed on attempt 3 (2 resets)"},
    },
    {
        .name = "reset",
        .input = {.format = []{ return checkpoint_reset_line("00.04", 2); }},
        .expectation = {.line = "[Checkpoint] 00.04: failed, resetting for attempt 2"},
    },
    {
        .name = "reset out of a limit",
        .input = {.format = []{ return checkpoint_reset_line("00.04", 2, /*attempts=*/3); }},
        .expectation = {.line = "[Checkpoint] 00.04: failed, resetting for attempt 2/3"},
    },
    {
        .name = "start without the checkpoint prefix",
        .input = {.format = []{ return checkpoint_start_line(SV_TEXT, /*events=*/std::nullopt, CheckpointPrefix::AS_GIVEN); }},
        .expectation = {.line = "Checkpoint:: 004 - 02. Start point: X: start"},
    },
    {
        .name = "attempt without the checkpoint prefix",
        .input = {.format = []{ return checkpoint_attempt_line(SV_TEXT, /*attempt=*/2, /*attempts=*/100, CheckpointPrefix::AS_GIVEN); }},
        .expectation = {.line = "Checkpoint:: 004 - 02. Start point: X: attempt 2/100"},
    },
    {
        .name = "pass without the checkpoint prefix",
        .input = {.format = []{ return checkpoint_pass_line(SV_TEXT, /*attempt=*/2, CheckpointPrefix::AS_GIVEN); }},
        .expectation = {.line = "Checkpoint:: 004 - 02. Start point: X: passed on attempt 2 (1 reset)"},
    },
    {
        .name = "reset without the checkpoint prefix",
        .input = {.format = []{ return checkpoint_reset_line(SV_TEXT, /*next_attempt=*/2, /*attempts=*/std::nullopt, CheckpointPrefix::AS_GIVEN); }},
        .expectation = {.line = "Checkpoint:: 004 - 02. Start point: X: failed, resetting for attempt 2"},
    },
    {
        .name = "short checkpoint start",
        .input = {.format = []{ return story_short_line("[Checkpoint] 12.03: start, 18 events"); }},
        .expectation = {.line = "12.03: start, 18 events"},
    },
    {
        .name = "short line with another tag",
        .input = {.format = []{ return story_short_line("[Tag] 12.03: some text"); }},
        .expectation = {.line = "12.03: some text"},
    },
    {
        .name = "short line without the checkpoint prefix",
        .input = {.format = []{ return story_short_line(
            checkpoint_start_line(SV_TEXT, /*events=*/std::nullopt, CheckpointPrefix::AS_GIVEN)
        ); }},
        .expectation = {.line = "Checkpoint:: 004 - 02. Start point: X: start"},
    },
    {
        .name = "short line with an unclosed tag",
        .input = {.format = []{ return story_short_line("[Tag 12.03"); }},
        .expectation = {.line = "[Tag 12.03"},
    },
    {
        .name = "short empty line",
        .input = {.format = []{ return story_short_line(""); }},
        .expectation = {.line = ""},
    },
    {
        .name = "event jump with skipped steps",
        .input = {.format = []{ return event_jump_line("01.02", "battle on screen", "walk_to_trainer", "trainer_battle", "approach, talk"); }},
        .expectation = {.line = "[Event] 01.02: battle on screen during walk_to_trainer: jumping to trainer_battle, skipping approach, talk"},
    },
    {
        .name = "event jump without skipped steps",
        .input = {.format = []{ return event_jump_line("01.02", "battle on screen", "walk_to_trainer", "trainer_battle", ""); }},
        .expectation = {.line = "[Event] 01.02: battle on screen during walk_to_trainer: jumping to trainer_battle"},
    },
};


} // namespace


void add_tests_StoryLog(UnitTestDatabase& database){
    for (const StoryLogTestCase& test_case : STORY_LOG_CASES){
        database.add<Test_StoryLog>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation

/*  PokemonLZA AutoStory - New game Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryPlan.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStory_Tests.h"
#include "PokemonLZA_AutoStoryNewGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  6 characters of 3 UTF-8 bytes each.
const std::string HANGUL_6 = "\xec\xbd\x94\xec\x8a\xa4\xeb\xaa\xa8\xec\x8a\xa4\xec\xbd\x94\xec\x8a\xa4";


struct RunProblemCase{
    std::string name;
    struct Input{
        std::vector<std::string> rows;
        NewGameChoices choices;
    };
    Input input;
    struct Expectation{
        bool refused = false;
    };
    Expectation expectation;
};

const RunProblemCase RUN_PROBLEM_CASES[] = {
    {.name = "the defaults from 01.00a", .input = {.rows = {"01.00a"}, .choices = {}}, .expectation = {.refused = false}},
    {
        .name = "13 characters in English from 01.00a",
        .input = {.rows = {"01.00a"}, .choices = {.name = "Abcdefghijklm"}},
        .expectation = {.refused = true},
    },
    {
        .name = "7 characters in Japanese from 01.00a",
        .input = {.rows = {"01.00a"}, .choices = {.language = NewGameLanguage::Japanese, .name = "Kosmoss"}},
        .expectation = {.refused = true},
    },
    {.name = "an empty name from 01.00a", .input = {.rows = {"01.00a"}, .choices = {.name = ""}}, .expectation = {.refused = true}},
    {.name = "look 9 from 01.00a", .input = {.rows = {"01.00a"}, .choices = {.look = 9}}, .expectation = {.refused = true}},
    {
        .name = "Korean from 01.00a",
        .input = {.rows = {"01.00a", "01.00b"}, .choices = {.language = NewGameLanguage::Korean, .name = HANGUL_6}},
        .expectation = {.refused = true},
    },
    {
        .name = "Korean from 01.00b",
        .input = {.rows = {"01.00b", "01.01"}, .choices = {.language = NewGameLanguage::Korean, .name = HANGUL_6}},
        .expectation = {.refused = false},
    },
    {
        .name = "digits and the first page's symbols from 01.00a",
        .input = {.rows = {"01.00a"}, .choices = {.name = "a1-/:',.?!"}},
        .expectation = {.refused = false},
    },
    {.name = "an accented letter from 01.00a", .input = {.rows = {"01.00a"}, .choices = {.name = "Jos\xc3\xa9"}}, .expectation = {.refused = true}},
    {.name = "a symbol of the second page from 01.00a", .input = {.rows = {"01.00a"}, .choices = {.name = "a#"}}, .expectation = {.refused = true}},
};

std::vector<Pokemon::StoryCheckpoint> builtin_rows(const std::vector<std::string>& ids){
    std::vector<Pokemon::StoryCheckpoint> rows;
    for (const std::string& id : ids){
        for (const Pokemon::StoryCheckpoint& row : builtin_story_data().checkpoints){
            if (row.id == id){
                //  The first step's action says whether the row starts a new game.
                Pokemon::StoryCheckpoint copy{.id = row.id, .title = row.title};
                copy.steps.push_back(Pokemon::StoryStep{.name = row.steps.front().name, .action = row.steps.front().action});
                rows.push_back(std::move(copy));
            }
        }
    }
    return rows;
}

class Test_NewGameRunProblem : public UnitTest{
public:
    explicit Test_NewGameRunProblem(const RunProblemCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - new game choices by run: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<Pokemon::StoryCheckpoint> rows = builtin_rows(m_test.input.rows);
        if (rows.size() != m_test.input.rows.size()){
            return std::string("A row of the case is not in the built-in story.");
        }
        const std::optional<std::string> problem = new_game_run_problem(rows, m_test.input.choices);
        if (problem.has_value() != m_test.expectation.refused){
            return std::format(
                "Expected {}, got {}.",
                m_test.expectation.refused ? "a refusal" : "no problem",
                problem ? *problem : std::string("no problem")
            );
        }
        return true;
    }

private:
    const RunProblemCase& m_test;
};


} // namespace


void add_tests_AutoStoryNewGame(UnitTestDatabase& database){
    for (const RunProblemCase& test : RUN_PROBLEM_CASES){
        database.add<Test_NewGameRunProblem>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

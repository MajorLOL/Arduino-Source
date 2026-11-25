/*  Story Backend Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <string>
#include <utility>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryBackend_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

using namespace std::chrono_literals;


//  A look at one piece: what it gives as text.
struct BackendCase{
    std::string name = {};
    struct Input{
        std::function<std::string(CancellableScope& scope)> read;
    };
    Input input = {};
    struct Expectation{
        std::string text;
    };
    Expectation expectation = {};
};

//  The scripted backend's inputs and looks.
std::string fake_text(CancellableScope& scope){
    ScreenObservation fade;
    fade.events = {ScreenEvent::FADE_WHITE};
    FakeStoryBackend backend({.looks = {ScreenObservation{}, fade}}, scope);
    backend.press(NintendoSwitch::BUTTON_A, /*hold=*/80ms, /*release=*/80ms);
    backend.stick(/*degrees=*/0, /*duration=*/400ms, /*release=*/0ms, StoryPace::RUN);
    backend.stick(/*degrees=*/90, /*duration=*/150ms, /*release=*/300ms, StoryPace::WALK);
    backend.wait(100ms);
    std::string looks;
    for (size_t c = 0; c < 3; c++){
        looks += std::format("{}{}", looks.empty() ? "" : " / ", backend.observe().events);
    }
    std::string text;
    for (const std::string& input : backend.inputs()){
        text += std::format("{}, ", input);
    }
    return text + looks;
}

const BackendCase CASES[] = {
    {
        .name = "the scripted backend records inputs and repeats its last look",
        .input = {.read = fake_text},
        .expectation = {.text = "press A, stick 0deg 400ms run, stick 90deg 150ms walk, release 300ms, wait 100ms, "
            "nothing / white fade / white fade"},
    },
};


class StoryBackendTest : public UnitTest{
public:
    explicit StoryBackendTest(BackendCase test_case)
        : UnitTest(std::format("Pokemon::StoryBackend - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string got = m_case.input.read(scope);
        if (got != m_case.expectation.text){
            return std::format("expected \"{}\", got \"{}\"", m_case.expectation.text, got);
        }
        return true;
    }

private:
    BackendCase m_case;
};

} // namespace


void add_tests_StoryBackend(UnitTestDatabase& database){
    for (const BackendCase& test : CASES){
        database.add<StoryBackendTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation

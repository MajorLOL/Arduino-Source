/*  Screen Events Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The shared event vocabulary, fades and the still-screen test.
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "ScreenEventTypes.h"
#include "ScreenFades.h"
#include "ScreenEvents.h"
#include "ScreenEvents_Tests.h"

namespace PokemonAutomation{

namespace{

UnitTestResult expect_equal(const std::string& what, const std::string& got, const std::string& expected){
    if (got == expected){
        return true;
    }
    return std::format("{}: expected \"{}\", got \"{}\"", what, expected, got);
}


UnitTestResult test_set_and_describe(){
    ScreenEvents events{ScreenEvent::DIALOG_BOX, ScreenEvent::PLAYER_CAN_MOVE};
    if (!events.has(ScreenEvent::DIALOG_BOX) || events.has(ScreenEvent::CHOICE_BOX)){
        return "has() disagrees with the constructor's events";
    }
    events.set(ScreenEvent::DIALOG_BOX, false);
    events.merge(fade_events(WarpFade::WHITE));
    events.merge(fade_events(WarpFade::NONE));
    if (fade_in(events) != WarpFade::WHITE){
        return "fade_events(WHITE) did not give a white fade";
    }
    if (std::format("{}", ScreenEvents()) != "nothing"){
        return "an empty set does not print as nothing";
    }
    return expect_equal("std::format()", std::format("{}", events), "white fade, player can move");
}

//  A screen filled with one colour, or a checkerboard of two.
ImageRGB32 screen_of(uint32_t a, uint32_t b){
    ImageRGB32 out(64, 36);
    for (size_t y = 0; y < out.height(); y++){
        for (size_t x = 0; x < out.width(); x++){
            out.pixel(x, y) = (x + y) % 2 == 0 ? a : b;
        }
    }
    return out;
}

UnitTestResult test_fade_of_screen(){
    struct Row{
        std::string name = {};
        uint32_t a = 0;
        uint32_t b = 0;
        WarpFade fade = WarpFade::NONE;
    };
    const std::vector<Row> rows = {
        {"black", 0xff000000, 0xff000000, WarpFade::BLACK},
        {"a cave mouth's dim fade (60 a channel)", 0xff3c3c3c, 0xff3c3c3c, WarpFade::BLACK},
        {"white", 0xffffffff, 0xffffffff, WarpFade::WHITE},
        {"grey, neither", 0xff787878, 0xff787878, WarpFade::NONE},
        {"a dark textured floor", 0xff101010, 0xff505050, WarpFade::NONE},
    };
    for (const Row& row : rows){
        const WarpFade got = fade_of_screen(screen_of(row.a, row.b));
        if (got != row.fade){
            return std::format("{}: expected {}, got {}", row.name, WARP_FADE_NAMES().get_string(row.fade), WARP_FADE_NAMES().get_string(got));
        }
    }
    return true;
}

UnitTestResult test_same_screen(){
    const WallClock now = WallClock::min();
    const VideoSnapshot black(screen_of(0xff000000, 0xff000000), now);
    const VideoSnapshot black_again(screen_of(0xff000000, 0xff000000), now);
    const VideoSnapshot white(screen_of(0xffffffff, 0xffffffff), now);
    ImageRGB32 small(32, 18);
    small.fill(0xff000000);
    const VideoSnapshot black_small(std::move(small), now);
    if (!same_screen(black, black_again)){
        return "two black frames are not the same screen";
    }
    if (same_screen(black, white)){
        return "black and white frames are the same screen";
    }
    if (same_screen(black, black_small)){
        return "frames of different sizes are the same screen";
    }
    if (same_screen(black, VideoSnapshot())){
        return "a missing frame is the same screen";
    }
    return true;
}


struct ScreenEventsCase{
    struct Input{
        UnitTestResult (*body)() = nullptr;
    };
    struct Expectation{
    };

    std::string name = {};
    Input input = {};
    Expectation expectation = {};
};

class Test_ScreenEvents : public UnitTest{
public:
    explicit Test_ScreenEvents(ScreenEventsCase test_case)
        : UnitTest(std::format("CommonTools::ScreenEvents - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.body();
    }

private:
    ScreenEventsCase m_case;
};

} // namespace


const ScreenEventsCase SCREEN_EVENTS_CASES[] = {
    {
        .name = "a set of events, its fade and its log text",
        .input = {.body = test_set_and_describe},
        .expectation = {},
    },
    {
        .name = "a fade is uniform, dark or white; a textured floor is none",
        .input = {.body = test_fade_of_screen},
        .expectation = {},
    },
    {
        .name = "the same screen: equal frames of one size",
        .input = {.body = test_same_screen},
        .expectation = {},
    },
};

void add_tests_ScreenEvents(UnitTestDatabase& database){
    for (const auto& item : SCREEN_EVENTS_CASES){
        database.add<Test_ScreenEvents>(item);
    }
}



}  // namespace PokemonAutomation

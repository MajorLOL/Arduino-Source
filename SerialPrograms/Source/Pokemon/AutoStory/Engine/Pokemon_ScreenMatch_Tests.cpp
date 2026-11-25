/*  Screen Match Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "Pokemon_ScreenMatch.h"
#include "Pokemon_ScreenMatch_Tests.h"
#include "Pokemon_ScreenTimers.h"
#include "Pokemon_ScreenWait.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

using namespace std::chrono_literals;

const WallClock T0 = WallClock(1000s);


struct ScreenMatchCase{
    std::string name = {};
    struct Input{
        std::function<std::string(CancellableScope& scope)> run;
    };
    Input input = {};
    struct Expectation{
        std::string text;
    };
    Expectation expectation = {};
};

ScreenObservation make_look(
    Milliseconds offset,
    ScreenEvents events = {},
    std::initializer_list<std::pair<std::string_view, int>> readings = {}
){
    ScreenObservation look;
    look.when = T0 + offset;
    look.events = events;
    for (const auto& [key, value] : readings){
        look.readings.emplace(std::string(key), value);
    }
    return look;
}

std::string outcome_text(const WaitOutcome& outcome){
    if (const auto* matched = std::get_if<WaitMatched>(&outcome)){
        return std::format("matched looks={} elapsed={}ms", matched->looks, matched->elapsed.count());
    }
    if (const auto* timed_out = std::get_if<WaitTimeout>(&outcome)){
        return std::format("timeout looks={} elapsed={}ms", timed_out->looks, timed_out->elapsed.count());
    }
    const auto& failure = std::get<StoryFailure>(outcome);
    return std::format("failure code={} repeats={}", failure.code, failure.repeats ? "true" : "false");
}

std::string test_screen_match_all_and_no(CancellableScope&){
    const ScreenMatch rule{
        .all_events = {ScreenEvent::DIALOG_BOX, ScreenEvent::PLAYER_CAN_MOVE},
        .no_events = {ScreenEvent::CHOICE_BOX},
        .all_readings = {"hp"},
        .no_readings = {"ko"},
    };
    const bool ok = rule.matches(make_look(0ms, {ScreenEvent::DIALOG_BOX, ScreenEvent::PLAYER_CAN_MOVE}, {{"hp", 10}}));
    const bool missing_event = rule(make_look(0ms, {ScreenEvent::DIALOG_BOX}, {{"hp", 10}}));
    const bool forbidden_event = rule(make_look(0ms, {ScreenEvent::DIALOG_BOX, ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::CHOICE_BOX}, {{"hp", 10}}));
    const bool missing_reading = rule(make_look(0ms, {ScreenEvent::DIALOG_BOX, ScreenEvent::PLAYER_CAN_MOVE}));
    const bool forbidden_reading = rule(make_look(0ms, {ScreenEvent::DIALOG_BOX, ScreenEvent::PLAYER_CAN_MOVE}, {{"hp", 10}, {"ko", 1}}));
    return std::format("{} {} {} {} {}", ok, missing_event, forbidden_event, missing_reading, forbidden_reading);
}

std::string test_screen_match_any_disjunction(CancellableScope&){
    const ScreenMatch rule{
        .any_events = {ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX},
        .any_readings = {"heading", "hp"},
    };
    const bool neither = rule(make_look(0ms, {ScreenEvent::FADE_BLACK}, {{"other", 1}}));
    const bool event_only = rule(make_look(0ms, {ScreenEvent::CHOICE_BOX}));
    const bool reading_only = rule(make_look(0ms, {}, {{"hp", 5}}));
    const bool both = rule(make_look(0ms, {ScreenEvent::DIALOG_BOX}, {{"heading", 90}}));
    return std::format("{} {} {} {}", neither, event_only, reading_only, both);
}

std::string test_screen_match_empty(CancellableScope&){
    constexpr ScreenMatch empty_rule{};
    const bool on_empty = empty_rule(ScreenObservation{});
    const bool on_full = empty_rule(make_look(0ms, {ScreenEvent::BATTLE_STARTED}, {{"hp", 1}}));
    return std::format("{} {}", on_empty, on_full);
}

std::string test_confirm_streak(CancellableScope&){
    ConfirmStreak streak(2);
    const bool a = streak.update(true);
    const size_t c1 = streak.count();
    const bool b = streak.update(false);
    const size_t c2 = streak.count();
    const bool c = streak.update(true);
    const bool d = streak.update(true);
    const size_t c3 = streak.count();
    streak.reset();
    return std::format("{}({}) {}({}) {} {}({}) reset={}", a, c1, b, c2, c, d, c3, streak.count());
}

std::string test_hold_timer(CancellableScope&){
    HoldTimer timer(500ms);
    const bool before = timer.held(T0);
    const bool u1 = timer.update(true, T0);
    const bool u2 = timer.update(true, T0 + 250ms);
    const bool u3 = timer.update(true, T0 + 500ms);
    const bool h3 = timer.held(T0 + 500ms);
    const bool u4 = timer.update(false, T0 + 600ms);
    const bool h4 = timer.held(T0 + 600ms);
    HoldTimer instant(0ms);
    const bool inst = instant.update(true, T0);
    return std::format("{} {} {} {} {} {} {} {}", before, u1, u2, u3, h3, u4, h4, inst);
}

std::string test_edge_trigger(CancellableScope&){
    EdgeTrigger edge;
    const bool r1 = edge.rising(true);
    const bool r2 = edge.rising(true);
    const bool s1 = edge.state();
    const bool f1 = edge.falling(false);
    const bool f2 = edge.falling(false);
    edge.rising(true);
    edge.reset();
    return std::format("{} {} {} {} {} {}", r1, r2, s1, f1, f2, edge.state());
}

std::string test_flicker_filter(CancellableScope&){
    FlickerFilter filter(/*hits=*/2, /*window=*/2000ms, /*off_after=*/3000ms);
    const bool on1 = filter.on_with(true, T0);
    filter.record(true, T0);
    const bool on2 = filter.on_with(true, T0 + 400ms);
    filter.record(true, T0 + 400ms);
    const bool gap = filter.on_with(false, T0 + 2000ms);
    filter.record(false, T0 + 2000ms);
    const bool expired = filter.on_with(false, T0 + 3400ms);
    filter.reset();
    const bool after_reset = filter.on_with(true, T0 + 4000ms);
    return std::format("{} {} {} {} {}", on1, on2, gap, expired, after_reset);
}

std::string test_wait_for_confirm_looks(CancellableScope& scope){
    const ScreenMatch target{.all_events = {ScreenEvent::DIALOG_BOX}};
    FakeStoryBackend backend({.looks = {
        make_look(0ms, {ScreenEvent::DIALOG_BOX}),
        make_look(250ms, {}),
        make_look(500ms, {ScreenEvent::DIALOG_BOX}),
        make_look(750ms, {ScreenEvent::DIALOG_BOX}),
    }}, scope);
    const WaitOutcome outcome = wait_for(backend, target, {
        .poll = 250ms,
        .timeout = 5000ms,
        .confirm_looks = 2,
    });
    return std::format("{} waits={}", outcome_text(outcome), backend.inputs().size());
}

std::string test_wait_for_confirm_hold(CancellableScope& scope){
    const ScreenMatch target{.all_events = {ScreenEvent::PLAYER_CAN_MOVE}};
    FakeStoryBackend backend({.looks = {
        make_look(0ms, {ScreenEvent::PLAYER_CAN_MOVE}),
        make_look(250ms, {ScreenEvent::PLAYER_CAN_MOVE}),
        make_look(500ms, {ScreenEvent::PLAYER_CAN_MOVE}),
    }}, scope);
    const WaitOutcome outcome = wait_for(backend, target, {
        .poll = 250ms,
        .timeout = 5000ms,
        .confirm_looks = 1,
        .confirm_hold = 500ms,
    });
    return outcome_text(outcome);
}

std::string test_wait_for_clock_timeout(CancellableScope& scope){
    const ScreenMatch target{.all_events = {ScreenEvent::BATTLE_STARTED}};
    FakeStoryBackend backend({.looks = {
        make_look(0ms),
        make_look(500ms),
        make_look(1000ms),
    }}, scope);
    const WaitOutcome outcome = wait_for(backend, target, {
        .poll = 250ms,
        .timeout = 1000ms,
    });
    return std::format("{} waits={}", outcome_text(outcome), backend.inputs().size());
}

std::string test_wait_for_constant_clock_bound(CancellableScope& scope){
    const ScreenMatch target{.all_events = {ScreenEvent::BATTLE_STARTED}};
    FakeStoryBackend backend({.looks = {ScreenObservation{}}}, scope);
    const WaitOutcome outcome = wait_for(backend, target, {
        .poll = 250ms,
        .timeout = 1000ms,
    });
    return std::format("{} waits={}", outcome_text(outcome), backend.inputs().size());
}

std::string test_wait_for_home_menu(CancellableScope& scope){
    const ScreenMatch target{.all_events = {ScreenEvent::DIALOG_BOX}};
    FakeStoryBackend fail_backend({.looks = {
        make_look(0ms, {ScreenEvent::SWITCH_HOME_MENU}),
    }}, scope);
    const WaitOutcome failed = wait_for(fail_backend, target, {
        .poll = 250ms,
        .timeout = 1000ms,
        .fail_on_home = true,
    });

    FakeStoryBackend ignore_backend({.looks = {
        make_look(0ms, {ScreenEvent::SWITCH_HOME_MENU}),
        make_look(250ms, {ScreenEvent::DIALOG_BOX}),
    }}, scope);
    const WaitOutcome ignored = wait_for(ignore_backend, target, {
        .poll = 250ms,
        .timeout = 1000ms,
        .fail_on_home = false,
    });
    return std::format("{} / {}", outcome_text(failed), outcome_text(ignored));
}

std::string test_wait_for_on_look_and_poll_until(CancellableScope& scope){
    const ScreenMatch target{.all_events = {ScreenEvent::DIALOG_BOX}};
    FakeStoryBackend backend({.looks = {
        make_look(0ms),
        make_look(250ms),
        make_look(500ms, {ScreenEvent::DIALOG_BOX}),
    }}, scope);
    size_t seen = 0;
    size_t acted = 0;
    const WaitOutcome outcome = poll_until(
        backend,
        target,
        {.poll = 250ms, .timeout = 2000ms},
        [&](const ScreenObservation&){
            acted++;
            backend.press(NintendoSwitch::BUTTON_A, /*hold=*/80ms, /*release=*/80ms);
        },
        [&](const ScreenObservation&){
            seen++;
        }
    );
    return std::format("{} seen={} acted={} inputs={}", outcome_text(outcome), seen, acted, backend.inputs().size());
}

const ScreenMatchCase CASES[] = {
    {
        .name = "ScreenMatch checks all_events, no_events, all_readings, and no_readings",
        .input = {.run = test_screen_match_all_and_no},
        .expectation = {.text = "true false false false false"},
    },
    {
        .name = "ScreenMatch treats any_events and any_readings as a single disjunction",
        .input = {.run = test_screen_match_any_disjunction},
        .expectation = {.text = "false true true true"},
    },
    {
        .name = "empty ScreenMatch matches any observation",
        .input = {.run = test_screen_match_empty},
        .expectation = {.text = "true true"},
    },
    {
        .name = "ConfirmStreak counts consecutive matches and resets on miss",
        .input = {.run = test_confirm_streak},
        .expectation = {.text = "false(1) false(0) false true(2) reset=0"},
    },
    {
        .name = "HoldTimer requires sustained match duration and resets on miss",
        .input = {.run = test_hold_timer},
        .expectation = {.text = "false false false true true false false true"},
    },
    {
        .name = "EdgeTrigger detects rising and falling transitions",
        .input = {.run = test_edge_trigger},
        .expectation = {.text = "true false true true false false"},
    },
    {
        .name = "FlickerFilter latches on hits within window and clears after off_after",
        .input = {.run = test_flicker_filter},
        .expectation = {.text = "false true true false false"},
    },
    {
        .name = "wait_for requires confirm_looks consecutive matches",
        .input = {.run = test_wait_for_confirm_looks},
        .expectation = {.text = "matched looks=4 elapsed=750ms waits=3"},
    },
    {
        .name = "wait_for requires confirm_hold sustained duration",
        .input = {.run = test_wait_for_confirm_hold},
        .expectation = {.text = "matched looks=3 elapsed=500ms"},
    },
    {
        .name = "wait_for times out when WallClock elapsed reaches timeout",
        .input = {.run = test_wait_for_clock_timeout},
        .expectation = {.text = "timeout looks=3 elapsed=1000ms waits=2"},
    },
    {
        .name = "wait_for bounds constant-clock backends by max_looks",
        .input = {.run = test_wait_for_constant_clock_bound},
        .expectation = {.text = "timeout looks=5 elapsed=0ms waits=4"},
    },
    {
        .name = "wait_for fails on SWITCH_HOME_MENU unless fail_on_home is false",
        .input = {.run = test_wait_for_home_menu},
        .expectation = {.text = "failure code=game_not_running repeats=true / matched looks=2 elapsed=250ms"},
    },
    {
        .name = "poll_until calls on_look every look and on_not_done only while unmatched",
        .input = {.run = test_wait_for_on_look_and_poll_until},
        .expectation = {.text = "matched looks=3 elapsed=500ms seen=3 acted=2 inputs=2"},
    },
};


class ScreenMatchTest : public UnitTest{
public:
    explicit ScreenMatchTest(ScreenMatchCase test_case)
        : UnitTest(std::format("Pokemon::ScreenMatch - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string got = m_case.input.run(scope);
        if (got != m_case.expectation.text){
            return std::format("expected \"{}\", got \"{}\"", m_case.expectation.text, got);
        }
        return true;
    }

private:
    ScreenMatchCase m_case;
};

} // namespace


void add_tests_ScreenMatch(UnitTestDatabase& database){
    for (const ScreenMatchCase& test : CASES){
        database.add<ScreenMatchTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation

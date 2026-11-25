/*  RNG Plan Tracker Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The outcome of an attempt, and the step after each, on made-up targets.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_RngPlanTestTargets.h"
#include "PokemonFRLG_RngPlanTracker.h"
#include "PokemonFRLG_RngPlanTracker_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;
using namespace RngPlanTestTargets;


namespace{

class Test_RngPlanTracker : public UnitTest{
public:
    Test_RngPlanTracker(std::string name, std::function<UnitTestResult(Logger&)> body)
        : UnitTest("PokemonFRLG::RngPlanTracker - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult(Logger&)> m_body;
};


UnitTestResult test_classify(Logger&){
    const RngTarget hit = TARGETS[0];
    AdvIVs other_ivs = hit.pokemon.ivs;
    other_ivs.speed = uint8_t((other_ivs.speed + 1) % 32);
    const AdvNature other_nature = hit.pokemon.nature == AdvNature::Hardy ? AdvNature::Lonely : AdvNature::Hardy;

    struct Case{
        std::string name;
        RngAttemptResult result;
        RngAttemptOutcome expected;
    };
    const Case cases[] = {
        {.name = "shiny, the target", .result = {.shiny = true, .nature = hit.pokemon.nature, .ivs = hit.pokemon.ivs}, .expected = RngAttemptOutcome::SHINY},
        {.name = "shiny, not the target", .result = {.shiny = true, .nature = other_nature, .ivs = other_ivs}, .expected = RngAttemptOutcome::SHINY_OFF_TARGET},
        {.name = "same nature and IVs", .result = {.shiny = false, .nature = hit.pokemon.nature, .ivs = hit.pokemon.ivs}, .expected = RngAttemptOutcome::HIT_NOT_SHINY},
        {.name = "other IVs", .result = {.shiny = false, .nature = hit.pokemon.nature, .ivs = other_ivs}, .expected = RngAttemptOutcome::MISSED},
        {.name = "other nature", .result = {.shiny = false, .nature = other_nature, .ivs = hit.pokemon.ivs}, .expected = RngAttemptOutcome::MISSED},
    };
    for (const Case& item : cases){
        if (classify_attempt(hit, item.result) != item.expected){
            return item.name + ": classified wrong.";
        }
    }
    return true;
}


const char* step_name(RngPlanStep step){
    switch (step){
    case RngPlanStep::RUN_ATTEMPT:     return "RUN_ATTEMPT";
    case RngPlanStep::GOT_SHINY:       return "GOT_SHINY";
    case RngPlanStep::GOT_SHINY_OFF_TARGET: return "GOT_SHINY_OFF_TARGET";
    case RngPlanStep::GOT_TARGET:      return "GOT_TARGET";
    case RngPlanStep::OUT_OF_ATTEMPTS: return "OUT_OF_ATTEMPTS";
    case RngPlanStep::MISSED_EVERY_ATTEMPT_LEFT: return "MISSED_EVERY_ATTEMPT_LEFT";
    }
    return "?";
}

struct TrackerCase{
    std::string name;
    bool shiny_required;
    std::vector<RngAttemptOutcome> outcomes;
    //  The step after each outcome, and the attempt run next.
    std::vector<RngPlanStep> steps;
    std::vector<size_t> attempts;
    std::vector<uint16_t> remaining;
    std::optional<uint16_t> confirmed;
};
std::vector<TrackerCase> tracker_cases(){
    using enum RngAttemptOutcome;
    using enum RngPlanStep;
    return {
        {
            .name = "shiny at once: one of two SIDs",
            .shiny_required = true, .outcomes = {SHINY},
            .steps = {GOT_SHINY}, .attempts = {0},
            .remaining = {100, 200}, .confirmed = std::nullopt,
        },
        {
            .name = "hit, then shiny: SID confirmed",
            .shiny_required = true, .outcomes = {HIT_NOT_SHINY, SHINY},
            .steps = {RUN_ATTEMPT, GOT_SHINY}, .attempts = {1, 1},
            .remaining = {300}, .confirmed = uint16_t(300),
        },
        {
            .name = "a miss retries the same target",
            .shiny_required = true, .outcomes = {MISSED, MISSED, HIT_NOT_SHINY, MISSED},
            .steps = {RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT}, .attempts = {0, 0, 1, 1},
            .remaining = {300}, .confirmed = std::nullopt,
        },
    };
}
//  Misses: retried, then the next attempt, mixed with hits and shinies.
std::vector<TrackerCase> tracker_retry_cases(){
    using enum RngAttemptOutcome;
    using enum RngPlanStep;
    return {
        {
            .name = "retries used up, then the next attempt",
            .shiny_required = true, .outcomes = {MISSED, MISSED, MISSED, SHINY},
            .steps = {RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, GOT_SHINY}, .attempts = {0, 0, 1, 1},
            .remaining = {300}, .confirmed = uint16_t(300),
        },
        {
            .name = "every attempt left missed too often",
            .shiny_required = true, .outcomes = {MISSED, MISSED, MISSED, MISSED, MISSED, MISSED},
            .steps = {RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, MISSED_EVERY_ATTEMPT_LEFT},
            .attempts = {0, 0, 1, 1, 1, 1},
            .remaining = {100, 200, 300}, .confirmed = std::nullopt,
        },
        {
            .name = "shiny off the target confirms nothing",
            .shiny_required = true, .outcomes = {HIT_NOT_SHINY, SHINY_OFF_TARGET, SHINY},
            .steps = {RUN_ATTEMPT, GOT_SHINY_OFF_TARGET, GOT_SHINY_OFF_TARGET}, .attempts = {1, 1, 1},
            .remaining = {300}, .confirmed = std::nullopt,
        },
    };
}
//  Attempts that can't run.
std::vector<TrackerCase> tracker_unusable_cases(){
    using enum RngAttemptOutcome;
    using enum RngPlanStep;
    return {
        {
            .name = "an attempt that can't run moves on",
            .shiny_required = true, .outcomes = {MISSED, UNUSABLE, SHINY},
            .steps = {RUN_ATTEMPT, RUN_ATTEMPT, GOT_SHINY}, .attempts = {0, 1, 1},
            .remaining = {300}, .confirmed = uint16_t(300),
        },
        {
            .name = "the last attempt can't run",
            .shiny_required = true, .outcomes = {UNUSABLE, UNUSABLE},
            .steps = {RUN_ATTEMPT, MISSED_EVERY_ATTEMPT_LEFT}, .attempts = {1, 1},
            .remaining = {100, 200, 300}, .confirmed = std::nullopt,
        },
    };
}
std::vector<TrackerCase> tracker_mixed_cases(){
    using enum RngAttemptOutcome;
    using enum RngPlanStep;
    return {
        {
            .name = "miss, hit, misses, then shiny",
            .shiny_required = true, .outcomes = {MISSED, HIT_NOT_SHINY, MISSED, MISSED, SHINY},
            .steps = {RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, GOT_SHINY}, .attempts = {0, 1, 1, 1, 1},
            .remaining = {300}, .confirmed = uint16_t(300),
        },
        {
            .name = "misses move on, then a hit ends the plan",
            .shiny_required = true, .outcomes = {MISSED, MISSED, MISSED, MISSED, HIT_NOT_SHINY},
            .steps = {RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, OUT_OF_ATTEMPTS}, .attempts = {0, 0, 1, 1, 1},
            .remaining = {100, 200}, .confirmed = std::nullopt,
        },
        {
            .name = "every attempt hit, none shiny",
            .shiny_required = true, .outcomes = {HIT_NOT_SHINY, HIT_NOT_SHINY},
            .steps = {RUN_ATTEMPT, OUT_OF_ATTEMPTS}, .attempts = {1, 1},
            .remaining = {}, .confirmed = std::nullopt,
        },
        {
            .name = "shiny not required: a hit ends it",
            .shiny_required = false, .outcomes = {HIT_NOT_SHINY},
            .steps = {GOT_TARGET}, .attempts = {0},
            .remaining = {100, 200, 300}, .confirmed = std::nullopt,
        },
    };
}

UnitTestResult check_tracker_case(const TrackerCase& item){
    RngPlanTracker tracker(plan_rng_attempts(TARGETS, CANDIDATES, true, 5), CANDIDATES, item.shiny_required, 2);
    if (tracker.step() != RngPlanStep::RUN_ATTEMPT){
        return item.name + ": expected to start with RUN_ATTEMPT, got " + step_name(tracker.step()) + ".";
    }
    for (size_t c = 0; c < item.outcomes.size(); c++){
        const RngPlanStep step = tracker.record(item.outcomes[c]);
        if (step != item.steps[c] || tracker.attempt_index() != item.attempts[c]){
            return std::format(
                "{}: after outcome {}, expected {} at attempt {}, got {} at attempt {}.",
                item.name, c, step_name(item.steps[c]), item.attempts[c], step_name(step), tracker.attempt_index()
            );
        }
    }
    if (tracker.remaining_sids() != item.remaining || tracker.confirmed_sid() != item.confirmed){
        return std::format(
            "{}: expected SIDs {} (confirmed {}), got {} (confirmed {}).",
            item.name, join(item.remaining), item.confirmed ? std::to_string(*item.confirmed) : "none",
            join(tracker.remaining_sids()), tracker.confirmed_sid() ? std::to_string(*tracker.confirmed_sid()) : "none"
        );
    }
    return true;
}

UnitTestResult test_tracker(Logger&){
    std::vector<TrackerCase> cases = tracker_cases();
    for (auto* more : {tracker_retry_cases, tracker_unusable_cases, tracker_mixed_cases}){
        for (TrackerCase& item : more()){
            cases.push_back(std::move(item));
        }
    }
    for (const TrackerCase& item : cases){
        const UnitTestResult result = check_tracker_case(item);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    RngPlanTracker empty({}, CANDIDATES, true, 2);
    if (empty.step() != RngPlanStep::OUT_OF_ATTEMPTS){
        return std::string("An empty plan: expected OUT_OF_ATTEMPTS, got ") + step_name(empty.step()) + ".";
    }
    //  A shiny hit for a SID that is no candidate: none left, none confirmed.
    RngPlanTracker outside({RngAttempt{make_target(1, 50, {}), {999}}}, CANDIDATES, true, 2);
    outside.record(RngAttemptOutcome::SHINY);
    if (outside.step() != RngPlanStep::GOT_SHINY || !outside.remaining_sids().empty() || outside.confirmed_sid()){
        return std::format(
            "Shiny for no candidate: expected GOT_SHINY with no SID left, got {} with SIDs {}.",
            step_name(outside.step()), join(outside.remaining_sids())
        );
    }
    return true;
}

}


void add_tests_RngPlanTracker(UnitTestDatabase& database){
    database.add<Test_RngPlanTracker>("attempt outcome", test_classify);
    database.add<Test_RngPlanTracker>("step after each outcome", test_tracker);
}



}
}
}

/*  PokemonLZA AutoStory - Actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure logic tests for AutoStory actions: heading turns, Wild Zone 1 exit
 *  and map legs, catch-plan target cycling, and menu text-speed setup.
 *
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStory_Tests.h"
#include "PokemonLZA_AutoStoryCatchPlan.h"
#include "PokemonLZA_AutoStoryMapLegs.h"
#include "PokemonLZA_AutoStoryScenes.h"
#include "PokemonLZA_AutoStoryWalks.h"
#include "PokemonLZA_AutoStoryZone1.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

template <typename Case, UnitTestResult (*check)(const Case&, CancellableScope&)>
class ActionCaseTest : public UnitTest{
public:
    ActionCaseTest(std::string_view prefix, const Case& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - {}: {}", prefix, test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check(m_case, scope);
    }

private:
    const Case& m_case;
};


//  --- face_heading ---

//  Below this, a turn lands `small_turn_scale` times as far as asked.
constexpr double SMALL_TURN = 15;
//  Faced: the player really faces the heading, not only the arrow read.
constexpr double FACED_TOLERANCE = 4;

class TurningGame : public Pokemon::FakeStoryBackend{
public:
    TurningGame(double facing, bool turns_take, size_t stale_reads, double small_turn_scale, CancellableScope& scope)
        : FakeStoryBackend({}, scope)
        , m_facing(facing)
        , m_shown(facing)
        , m_turns_take(turns_take)
        , m_stale_reads(stale_reads)
        , m_small_turn_scale(small_turn_scale)
    {}

    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace) override{
        if (m_turns_take){
            m_queued_turn += std::fabs(degrees) < SMALL_TURN ? degrees * m_small_turn_scale : degrees;
        }
        FakeStoryBackend::stick(degrees, duration, release, pace);
    }
    virtual ScreenObservation observe() override{
        if (m_queued_turn != 0){
            m_shown = m_facing;
            m_stale_left = m_stale_reads;
        }
        m_facing = std::fmod(std::fmod(m_facing + m_queued_turn, 360) + 360, 360);
        m_queued_turn = 0;
        return FakeStoryBackend::observe();
    }

    //  The queued turn not in yet; the old facing while the feed lags.
    double arrow(){
        if (m_stale_left > 0){
            m_stale_left--;
            return m_shown;
        }
        return m_facing;
    }

    double facing() const{
        return m_facing;
    }

private:
    double m_facing;
    double m_shown;
    const bool m_turns_take;
    const size_t m_stale_reads;
    const double m_small_turn_scale;
    size_t m_stale_left = 0;
    double m_queued_turn = 0;
};

double heading_error(double facing, double heading){
    const double error = std::fmod(std::fabs(facing - heading), 360.0);
    return std::min(error, 360 - error);
}

struct FaceHeadingCase{
    std::string name;
    struct Input{
        double facing;
        double heading;
        bool turns_take = true;
        size_t stale_reads = 0;
        double small_turn_scale = 1;
    };
    Input input;
    struct Expectation{
        std::string failure;
    };
    Expectation expectation;
};

const FaceHeadingCase FACE_HEADING_CASES[] = {
    {
        .name = "run 10 of 03.08: from 135 to west",
        .input = {.facing = 135, .heading = 270},
        .expectation = {.failure = ""},
    },
    {
        .name = "across north",
        .input = {.facing = 350, .heading = 20},
        .expectation = {.failure = ""},
    },
    {
        .name = "8 off is not facing it: 03.09 read 272 for 264 and walked right of the gate",
        .input = {.facing = 272, .heading = 264, .turns_take = false},
        .expectation = {.failure = "heading_missed"},
    },
    {
        //  Korean 03.08 catch_more_species read 161, 163, 269, 9, 9 for 265:
        //  each reading one turn late (console B video, 2026-11-17 23:58).
        .name = "a lagging feed: from 161 to 265",
        .input = {.facing = 161, .heading = 265, .stale_reads = 1},
        .expectation = {.failure = ""},
    },
    {
        //  CHS 03.08 read 248.9, 259.1, 249.1, 259.3 for 254: each 5 deg
        //  correction turned 10 (console B, 2026-11-18 02:03).
        .name = "small turns overshoot: from 249 to 254",
        .input = {.facing = 249, .heading = 254, .small_turn_scale = 2},
        .expectation = {.failure = ""},
    },
};

UnitTestResult check_face_heading(const FaceHeadingCase& test, CancellableScope& scope){
    TurningGame game(test.input.facing, test.input.turns_take, test.input.stale_reads, test.input.small_turn_scale, scope);
    const std::optional<Pokemon::StoryFailure> outcome = face_heading(game, [&game](){ return game.arrow(); }, test.input.heading);
    if ((outcome ? outcome->code : std::string()) != test.expectation.failure){
        return std::format(
            "expected failure \"{}\"; got \"{}\", facing {:.0f}",
            test.expectation.failure, outcome ? outcome->code : std::string(), game.facing()
        );
    }
    if (!outcome && heading_error(game.facing(), test.input.heading) > FACED_TOLERANCE){
        return std::format("faced: the player faces {:.0f}, not {:.0f}", game.facing(), test.input.heading);
    }
    return true;
}


//  --- Wild Zone 1 exit, zone check, and map legs ---

MoveOnlyFunction<std::optional<MapPoint>()> pins_of(const std::vector<MapPoint>& pins, size_t& reads){
    if (pins.empty()){
        return nullptr;
    }
    return [pins, &reads]() -> std::optional<MapPoint>{
        const MapPoint pin = pins[std::min(reads, pins.size() - 1)];
        reads++;
        return pin;
    };
}

constexpr MapPoint OUT_OF_ZONE_PIN{.x = 401, .y = 225};
constexpr MapPoint IN_ZONE_PIN{.x = 386, .y = 252};
constexpr MapPoint OFF_CENTRE_OUT_PIN{.x = 420, .y = 262};
constexpr double GATE_HEADING = 270;
constexpr double GATE_HEADING_TOLERANCE = 10;

struct ExitCase{
    std::string name;
    struct Input{
        MapPoint start;
    };
    Input input;
    struct Expectation{
        std::optional<MapPoint> waypoint;
    };
    Expectation expectation;
};

const ExitCase EXIT_CASES[] = {
    {
        .name = "on the gate's line: one straight run",
        .input = {.start = {.x = 379, .y = 250}},
        .expectation = {.waypoint = std::nullopt},
    },
    {
        .name = "north-east of the gate: diagonal straight to the gate run-up",
        .input = {.start = {.x = 399, .y = 231}},
        .expectation = {.waypoint = MapPoint{.x = 375, .y = 252}},
    },
    {
        .name = "west of the gate: onto the line east of the gate",
        .input = {.start = {.x = 340, .y = 240}},
        .expectation = {.waypoint = MapPoint{.x = 375, .y = 252}},
    },
};

UnitTestResult check_exit(const ExitCase& test, CancellableScope&){
    const std::vector<MapPoint> legs = zone_1_exit_legs(test.input.start);
    const std::optional<MapPoint> waypoint = legs.size() == 2 ? std::optional<MapPoint>(legs.front()) : std::nullopt;
    const auto same = [](std::optional<MapPoint> a, std::optional<MapPoint> b){
        return a.has_value() == b.has_value() && (!a || (a->x == b->x && a->y == b->y));
    };
    if (legs.empty() || legs.size() > 2 || !same(waypoint, test.expectation.waypoint)){
        const auto shown = [](std::optional<MapPoint> point){
            return point ? std::format("({:.0f}, {:.0f}) first", point->x, point->y) : std::string("a straight run");
        };
        return std::format("Expected {}, got {} in {} legs.", shown(test.expectation.waypoint), shown(waypoint), legs.size());
    }
    const double heading = bearing(legs.size() == 1 ? test.input.start : legs[legs.size() - 2], legs.back());
    if (std::abs(heading - GATE_HEADING) > GATE_HEADING_TOLERANCE){
        return std::format("The run at the gate heads {:.0f}, not along its line ({:.0f}).", heading, GATE_HEADING);
    }
    return true;
}

struct OutOfZoneCase{
    std::string name;
    struct Input{
        std::vector<MapPoint> pins;
    };
    Input input;
    struct Expectation{
        std::optional<std::string> failure;
        size_t reads = 0;
    };
    Expectation expectation;
};

const OutOfZoneCase OUT_OF_ZONE_CASES[] = {
    {
        .name = "out at the map's centre: one map read",
        .input = {.pins = {OUT_OF_ZONE_PIN}},
        .expectation = {.failure = std::nullopt, .reads = 1},
    },
    {
        .name = "in the zone: no save there",
        .input = {.pins = {IN_ZONE_PIN}},
        .expectation = {.failure = "in_wild_zone", .reads = 1},
    },
    {
        .name = "out off the map's centre, in on the next read: in the zone",
        .input = {.pins = {OFF_CENTRE_OUT_PIN, IN_ZONE_PIN}},
        .expectation = {.failure = "in_wild_zone", .reads = 2},
    },
    {
        .name = "no map pin reader: not known to be out",
        .input = {.pins = {}},
        .expectation = {.failure = "zone_unread", .reads = 0},
    },
};

UnitTestResult check_out_of_zone(const OutOfZoneCase& test, CancellableScope& scope){
    Pokemon::FakeStoryBackend backend(Pokemon::FakeStoryScript{.looks = {}}, scope);
    size_t reads = 0;
    const std::optional<Pokemon::StoryFailure> outcome = check_out_of_zone_1(backend, pins_of(test.input.pins, reads));
    const std::optional<std::string> failure = !outcome ? std::nullopt : std::optional<std::string>(outcome->code);
    if (failure != test.expectation.failure){
        return std::format("Expected {}, got {}.", test.expectation.failure.value_or("a pass"), (outcome ? std::format("{}", *outcome) : std::string("ok")));
    }
    if (reads != test.expectation.reads){
        return std::format("Expected {} map reads, got {}.", test.expectation.reads, reads);
    }
    return true;
}

constexpr MapWaypoint SPOT{.name = "the 100 spot", .pin = {.x = 432, .y = 238}, .stop_radius = CATCH_STOP_RADIUS};
constexpr MapWaypoint SECOND_SPOT{.name = "the second catch spot", .pin = {.x = 386, .y = 252}, .stop_radius = CATCH_STOP_RADIUS};

std::string describe_move(const MapLegMove& move){
    switch (move.step){
    case MapLegStep::ARRIVED:
        return "arrived";
    case MapLegStep::WALK:
        return std::format(
            "{}walk {:.0f} deg",
            move.correction > 0 ? std::format("correction {}: ", move.correction) : std::string(),
            move.heading
        );
    case MapLegStep::READ_AGAIN:
        return "read again";
    case MapLegStep::OFF_TARGET:
        return "off target";
    case MapLegStep::PIN_NOT_READ:
        return "pin not read";
    }
    return "?";
}

struct LegCase{
    std::string name;
    struct Input{
        MapWaypoint waypoint;
        std::vector<std::optional<MapPoint>> reads;
    };
    Input input;
    struct Expectation{
        std::string moves;
    };
    Expectation expectation;
};

const LegCase LEG_CASES[] = {
    {
        .name = "a walk that ends short gets a correction, then arrives",
        .input = {.waypoint = SPOT, .reads = {MapPoint{.x = 456, .y = 231}, MapPoint{.x = 445, .y = 234}, MapPoint{.x = 431, .y = 238}}},
        .expectation = {.moves = "walk 254 deg, correction 1: walk 253 deg, arrived"},
    },
    {
        .name = "still off after the leg and two corrections: stops",
        .input = {.waypoint = SPOT, .reads = {
            MapPoint{.x = 456, .y = 231}, MapPoint{.x = 420, .y = 220},
            MapPoint{.x = 445, .y = 250}, MapPoint{.x = 420, .y = 225},
        }},
        .expectation = {.moves = "walk 254 deg, correction 1: walk 146 deg, correction 2: walk 313 deg, off target"},
    },
    {
        .name = "a missing read gets a second look",
        .input = {.waypoint = SPOT, .reads = {std::nullopt, MapPoint{.x = 433, .y = 238}}},
        .expectation = {.moves = "read again, arrived"},
    },
    {
        .name = "two missing reads in a row stop",
        .input = {.waypoint = SPOT, .reads = {std::nullopt, std::nullopt}},
        .expectation = {.moves = "read again, pin not read"},
    },
    {
        .name = "a missing read after a walk is not a correction",
        .input = {.waypoint = SPOT, .reads = {MapPoint{.x = 456, .y = 231}, std::nullopt, MapPoint{.x = 445, .y = 234}}},
        .expectation = {.moves = "walk 254 deg, read again, correction 1: walk 253 deg"},
    },
    {
        .name = "a walk past a catch spot on its way, within its stop area, arrives",
        .input = {.waypoint = SECOND_SPOT, .reads = {MapPoint{.x = 431, .y = 236}, MapPoint{.x = 380, .y = 247}}},
        .expectation = {.moves = "walk 250 deg, arrived"},
    },
    {
        .name = "a walk that ends off to the side of a catch spot, out of its stop area, gets a correction",
        .input = {.waypoint = SECOND_SPOT, .reads = {MapPoint{.x = 431, .y = 236}, MapPoint{.x = 384, .y = 265}}},
        .expectation = {.moves = "walk 250 deg, correction 1: walk 9 deg"},
    },
    {
        .name = "a first read within a catch spot's stop area arrives without a walk",
        .input = {.waypoint = SECOND_SPOT, .reads = {MapPoint{.x = 380, .y = 247}}},
        .expectation = {.moves = "arrived"},
    },
};

UnitTestResult check_map_leg(const LegCase& test, CancellableScope&){
    MapLeg leg(test.input.waypoint.pin, test.input.waypoint.stop_radius);
    std::string moves;
    for (const std::optional<MapPoint>& read : test.input.reads){
        const MapLegMove move = leg.next(read);
        if (move.step == MapLegStep::WALK && move.hold <= Milliseconds(0)){
            return std::format("Expected a positive walk hold, got {} ms.", move.hold.count());
        }
        if (!moves.empty()){
            moves += ", ";
        }
        moves += describe_move(move);
    }
    if (moves != test.expectation.moves){
        return std::format("Expected \"{}\", got \"{}\".", test.expectation.moves, moves);
    }
    return true;
}


//  --- Catch plan ---

struct Plate{
    double x;
    double y;
    std::optional<bool> caught = false;
};

LockTarget lock_target(const Plate& plate){
    LockTarget target;
    target.x = plate.x;
    target.y = plate.y;
    target.caught = plate.caught;
    return target;
}

struct SettleCase{
    std::string name;
    struct Input{
        std::vector<std::optional<int>> reads;
    };
    Input input;
    struct Expectation{
        std::optional<int> slots;
    };
    Expectation expectation;
};

const SettleCase SETTLE_CASES[] = {
    {
        //  03.08 read 4 of 5 and failed; its retry at once read 5 (console B, 2026-11-18).
        .name = "the last catch joining the bar a second after the first read counts",
        .input = {.reads = {4, 4, 4, 4, 5}},
        .expectation = {.slots = 5},
    },
};

UnitTestResult check_settled_party_slots(const SettleCase& test, CancellableScope&){
    const std::vector<std::optional<int>>& reads = test.input.reads;
    size_t look = 0;
    const std::optional<int> got = settled_party_slots([&]{
        return reads[std::min(look++, reads.size() - 1)];
    });
    if (got != test.expectation.slots){
        return std::format("Expected {}, got {}.", test.expectation.slots.value_or(-1), got.value_or(-1));
    }
    return true;
}

struct CycleCase{
    std::string name;
    struct Input{
        size_t max_views;
        std::vector<Plate> views;
        size_t flick_target;
    };
    Input input;
    struct Expectation{
        size_t views_added;
        size_t targets;
        int flicks;
    };
    Expectation expectation;
};

const CycleCase CYCLE_CASES[] = {
    {
        .name = "stops where a flick finds no other, the nearest back left",
        .input = {.max_views = 4, .views = {{0.50, 0.25}, {0.52, 0.40}, {0.48, 0.30}, {0.49, 0.31}}, .flick_target = 1},
        .expectation = {.views_added = 4, .targets = 3, .flicks = -1},
    },
    {
        .name = "stops when it comes round to the first, the second a flick right",
        .input = {.max_views = 4, .views = {{0.50, 0.25}, {0.50, 0.40}, {0.51, 0.26}}, .flick_target = 1},
        .expectation = {.views_added = 3, .targets = 2, .flicks = 1},
    },
    {
        .name = "is bounded by its views, the lock on the last",
        .input = {
            .max_views = 4,
            .views = {{0.0, 0.20}, {0.1, 0.25}, {0.2, 0.30}, {0.3, 0.35}, {0.4, 0.40}, {0.5, 0.45}, {0.6, 0.50}},
            .flick_target = 3,
        },
        .expectation = {.views_added = 4, .targets = 4, .flicks = 0},
    },
};

UnitTestResult check_lock_target_cycle(const CycleCase& test, CancellableScope&){
    LockTargetCycle cycle(test.input.max_views);
    size_t added = 0;
    for (const Plate& view : test.input.views){
        added++;
        if (!cycle.add(lock_target(view))){
            break;
        }
    }
    const size_t targets = cycle.targets().size();
    const int flicks = cycle.flicks_to(test.input.flick_target);
    const CycleCase::Expectation& expected = test.expectation;
    if (added != expected.views_added || targets != expected.targets || flicks != expected.flicks){
        return std::format(
            "Expected {} views added, {} targets and {} flicks; got {}, {} and {}.",
            expected.views_added, expected.targets, expected.flicks, added, targets, flicks
        );
    }
    return true;
}


//  --- set_text_speed_fast ---

ScreenObservation look(ScreenEvents events, std::vector<std::string_view> readings, Milliseconds at){
    ScreenObservation observation;
    observation.when = WallClock::min() + at;
    observation.events = events;
    for (std::string_view reading : readings){
        observation.readings[std::string(reading)] = 1;
    }
    return observation;
}

ScreenObservation overworld(Milliseconds at){
    return look({ScreenEvent::PLAYER_CAN_MOVE}, /*readings=*/{}, at);
}

ScreenObservation card(Milliseconds at){
    return look(/*events=*/{}, {TUTORIAL_CARD_READING}, at);
}

ScreenObservation platform(Milliseconds at){
    return look(/*events=*/{}, {CONTROLS_HINT_READING}, at);
}

ScreenObservation options(Milliseconds at){
    return look(/*events=*/{}, /*readings=*/{}, at);
}

std::string non_poll_inputs(const std::vector<std::string>& texts){
    std::string text;
    for (const std::string& part : texts){
        if (part == "wait 250ms"){
            continue;
        }
        if (!text.empty()){
            text += ", ";
        }
        text += part;
    }
    return text;
}

struct TextSpeedCase{
    std::string name;
    struct Input{
        std::vector<ScreenObservation> looks;
    };
    Input input;
    struct Expectation{
        std::string failure;
        std::string inputs;
    };
    Expectation expectation;
};

constexpr std::string_view TEXT_SPEED_ROWS = "press L, press Up, press A, press Down, press Right";
constexpr std::string_view TEXT_SPEED_OPEN = "press X, wait 2000ms";

const TextSpeedCase TEXT_SPEED_CASES[] = {
    {
        //  console B, 2026-11-19, 01.01 3/3: a second A on a fading card opened the bag.
        .name = "a tutorial card fading out after its A gets no second A",
        .input = {.looks = {
            card(Milliseconds(0)), options(Milliseconds(1500)),
            options(Milliseconds(2500)), platform(Milliseconds(3500)), platform(Milliseconds(3750)),
        }},
        .expectation = {
            .failure = "",
            .inputs = std::format("{}, press A, wait 1500ms, {}, press B", TEXT_SPEED_OPEN, TEXT_SPEED_ROWS),
        },
    },
    {
        .name = "a menu with no tutorial card gets the rows at once",
        .input = {.looks = {options(Milliseconds(0)), overworld(Milliseconds(1000)), overworld(Milliseconds(1250))}},
        .expectation = {
            .failure = "",
            .inputs = std::format("{}, {}", TEXT_SPEED_OPEN, TEXT_SPEED_ROWS),
        },
    },
    {
        .name = "a screen no detector reads after the rows, like a stray bag, gets B until the overworld",
        .input = {.looks = {
            options(Milliseconds(0)), options(Milliseconds(1000)), options(Milliseconds(2000)),
            options(Milliseconds(3000)), overworld(Milliseconds(4000)), overworld(Milliseconds(4250)),
        }},
        .expectation = {
            .failure = "",
            .inputs = std::format("{}, {}, press B, press B, press B", TEXT_SPEED_OPEN, TEXT_SPEED_ROWS),
        },
    },
    {
        .name = "never back on the overworld: the step times out",
        .input = {.looks = {options(Milliseconds(0)), options(Milliseconds(1000)), options(Milliseconds(31500))}},
        .expectation = {
            .failure = "text_speed_timeout",
            .inputs = std::format("{}, {}, press B", TEXT_SPEED_OPEN, TEXT_SPEED_ROWS),
        },
    },
};

UnitTestResult check_text_speed(const TextSpeedCase& test, CancellableScope& scope){
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    const std::optional<Pokemon::StoryFailure> outcome = set_text_speed_fast(backend);
    if ((outcome ? outcome->code : std::string()) != test.expectation.failure){
        return std::format("expected failure \"{}\", got \"{}\"", test.expectation.failure, (outcome ? std::format("{}", *outcome) : std::string("ok")));
    }
    const std::string inputs = non_poll_inputs(backend.inputs());
    if (inputs != test.expectation.inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", test.expectation.inputs, inputs);
    }
    return true;
}

} // namespace


void add_tests_AutoStoryActions(UnitTestDatabase& database){
    for (const FaceHeadingCase& test : FACE_HEADING_CASES){
        database.add<ActionCaseTest<FaceHeadingCase, check_face_heading>>("face_heading", test);
    }
    for (const ExitCase& test : EXIT_CASES){
        database.add<ActionCaseTest<ExitCase, check_exit>>("zone_1_exit_legs", test);
    }
    for (const OutOfZoneCase& test : OUT_OF_ZONE_CASES){
        database.add<ActionCaseTest<OutOfZoneCase, check_out_of_zone>>("check_out_of_zone_1", test);
    }
    for (const LegCase& test : LEG_CASES){
        database.add<ActionCaseTest<LegCase, check_map_leg>>("Map legs", test);
    }
    for (const SettleCase& test : SETTLE_CASES){
        database.add<ActionCaseTest<SettleCase, check_settled_party_slots>>("Catches: settled party bar", test);
    }
    for (const CycleCase& test : CYCLE_CASES){
        database.add<ActionCaseTest<CycleCase, check_lock_target_cycle>>("Catches: target cycle", test);
    }
    for (const TextSpeedCase& test : TEXT_SPEED_CASES){
        database.add<ActionCaseTest<TextSpeedCase, check_text_speed>>("set_text_speed_fast", test);
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

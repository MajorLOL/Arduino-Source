/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_RunFromBattleDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_AlertEyeDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_CaughtMarkReader.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_PartyBarDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BallThrow.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_LockOnSweep.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "PokemonLZA_AutoStoryCatchPlan.h"
#include "PokemonLZA_AutoStoryScenes.h"
#include "PokemonLZA_AutoStoryWorldWalks.h"
#include "PokemonLZA_AutoStoryZone1.h"
#include "PokemonLZA_AutoStoryCatches.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  The road below the rocks. Going straight from the rocks to the 100 spot
//  stops on an obstacle at about (467, 226) 3 runs in 5 (2026-11-18/19);
//  through here the heading to the spot keeps south of it.
constexpr MapWaypoint ZONE1_ROAD_ENTRY{.name = "the road entry", .pin = {.x = 456, .y = 231}};

//  Via the road, past the rock on its right.
constexpr MapWaypoint ROCKS_TO_SPOT_100[] = {ZONE1_ROAD_ENTRY, ZONE1_SPOT_100};

//  How long the party bar must show the new member before declaring caught.
constexpr Milliseconds PARTY_CAUGHT_HOLD(250);

//  The new member joins the party bar ~6-7 s after a successful throw.
const BallThrowOptions STORY_THROW{
    .lock_on = Milliseconds(500),
    .aim = Milliseconds(1500),
    .after_throw = Milliseconds(8000),
};

constexpr Milliseconds LINES_QUIET(3000);
constexpr Milliseconds LINES_LIMIT(30000);

//  Right, toward the grass right of the road where the catches were.
constexpr double SWEEP_STICK_X = 0.5;
constexpr int SWEEP_ROUNDS = 8;

//  The Run prompt, or the alert bar with the eye while a wild Pokemon
//  attacks before the partner is out.
bool wild_battle_on(StoryEnv& env){
    env.context.wait_for_all_requests();
    const VideoSnapshot screen = env.env.console.video().snapshot();
    if (!screen){
        return false;
    }
    RunFromBattleDetector run_prompt(COLOR_RED, nullptr);
    AlertEyeDetector alert(COLOR_RED, nullptr);
    return run_prompt.detect(screen) || alert.detect(screen);
}

//  Attacks until 3 land (12 tries at most); two lock-ons in a row with no
//  move symbols press D-pad up (twice at most): the partner may not be out.
//  The target stays the one in front: TrainerBattleState turns the camera
//  after 3 misses in a row, which lost the attacker for ~2 s (console B,
//  2026-11-19 22:46:34), so it starts afresh after every 2.
int weaken_target(StoryEnv& env, PlzaBackend& backend){
    std::optional<TrainerBattleState> attacker(std::in_place, env.battle_ai);
    int landed = 0;
    int misses = 0;
    int sends = 0;
    for (int tries = 0; tries < 12 && landed < 3; tries++){
        if (attacker->attempt_one_attack(env.env, env.env.console, env.context)){
            landed++;
            misses = 0;
            continue;
        }
        if (++misses < 2){
            continue;
        }
        if (sends < 2){
            backend.log("[PLZA] weaken_target: no lock-on twice: D-pad up to send the partner out");
            backend.press(BUTTON_UP, /*hold=*/100ms, /*release=*/2000ms);
            sends++;
        }
        attacker.emplace(env.battle_ai);
        misses = 0;
    }
    return landed;
}

} // namespace


std::optional<Pokemon::StoryFailure> catch_tutorial(StoryEnv& env, PlzaBackend& backend, int max_throws){
    for (int throws = 1; throws <= max_throws; throws++){
        throw_ball(env.context, STORY_THROW);
        if (press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT).card){
            backend.log(std::format("[PLZA] catch_tutorial: caught on throw {}", throws));
            return std::nullopt;
        }
        if (throws < max_throws && wild_battle_on(env)){
            const int landed = weaken_target(env, backend);
            backend.log(std::format("[PLZA] catch_tutorial: throw {} broke out into a battle: {} attacks landed", throws, landed));
            press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        }
    }
    return Pokemon::StoryFailure{.code = "no_catch", .detail = std::format("no tutorial card after {} throws", max_throws)};
}

std::optional<Pokemon::StoryFailure> battle_then_catch(StoryEnv& env, PlzaBackend& backend, int max_rounds){
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/840ms);
    press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
    backend.press(BUTTON_UP, /*hold=*/100ms, /*release=*/2000ms);
    for (int round = 0; round < max_rounds; round++){
        const int landed = weaken_target(env, backend);
        if (landed == 0 && round > 0){
            if (!wild_battle_on(env)){
                backend.log(std::format("[PLZA] battle_then_catch: nothing to lock on in round {}: caught", round + 1));
                return std::nullopt;
            }
            backend.log(std::format("[PLZA] battle_then_catch: nothing to lock on in round {}, still in battle: sweeping camera", round + 1));
            if (!lock_on_by_sweep(env.env.console, env.context, SWEEP_STICK_X, SWEEP_ROUNDS)){
                continue;
            }
        }
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        throw_ball(env.context, STORY_THROW);
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        backend.log(std::format("[PLZA] battle_then_catch: round {}: {} attacks landed, then a throw", round + 1, landed));
    }
    return Pokemon::StoryFailure{.code = "still_a_target", .detail = std::format("a target still locks on after {} rounds", max_rounds)};
}


namespace{

//  The lock shows over 1.5 s after ZL at times.
constexpr Milliseconds LOCK_ON_WAIT(3000);
constexpr double TARGET_FLICK = 0.8;
//  So a target that keeps dodging does not take all of the stop's throws.
constexpr int THROWS_AT_ONE_TARGET = 3;

struct CatchResult{
    int throws = 0;
    bool caught = false;
    bool party_reached = false;
};

class PartyCountDetector : public StaticScreenDetector{
public:
    explicit PartyCountDetector(int target_slots)
        : m_target_slots(target_slots)
    {}
    virtual void make_overlays(VideoOverlaySet& items) const override{
        m_bar.make_overlays(items);
    }
    virtual bool detect(const ImageViewRGB32& screen) override{
        return m_bar.detect(screen) && m_bar.filled_slots() >= m_target_slots;
    }

private:
    int m_target_slots;
    PartyBarDetector m_bar;
};


//  After a ZL lock-on the camera looks steeply down: down 1500 ms pins the
//  pitch at its lowest, then up 600 ms gives the walking pitch.
void level_camera(StoryEnv& env){
    pbf_move_right_joystick(env.context, /*position=*/{0, -1}, /*hold=*/Milliseconds(1500), /*release=*/Milliseconds(100));
    pbf_move_right_joystick(env.context, /*position=*/{0, 1}, /*hold=*/Milliseconds(600), /*release=*/Milliseconds(300));
}

//  Throws a ball and returns immediately once the party bar reaches `target_party`.
bool throw_ball_watched(StoryEnv& env, PlzaBackend& backend, int target_party){
    ConsoleHandle& console = env.env.console;
    DetectorToFinder<PartyCountDetector> party_reached("PartyCountWatcher", PARTY_CAUGHT_HOLD, target_party);
    const int fired = run_until<ProControllerContext>(
        console, env.context,
        [](ProControllerContext& context){
            throw_ball(context, STORY_THROW);
        },
        {party_reached}
    );
    const bool seen = fired >= 0;
    backend.log(std::format("[PLZA] catch_in_zone: party bar reached {}: {}", target_party, seen ? "yes" : "no"));
    env.context.wait_for_all_requests();
    return seen;
}

//  Ends once the party bar reaches `target_party`, a round after the first
//  finds nothing to lock on, or `max_throws` throws.
CatchResult weaken_then_throw(StoryEnv& env, PlzaBackend& backend, int max_throws, int target_party){
    CatchResult out;
    while (out.throws < max_throws){
        const int landed = weaken_target(env, backend);
        if (landed == 0 && out.throws > 0){
            backend.log(std::format("[PLZA] catch_in_zone: nothing to lock on in round {}: gone", out.throws + 1));
            return out;
        }
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        const bool reached = throw_ball_watched(env, backend, target_party);
        out.throws++;
        backend.log(std::format("[PLZA] catch_in_zone: round {}: {} attacks landed, then a throw", out.throws, landed));
        if (reached){
            out.caught = true;
            out.party_reached = true;
            return out;
        }
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
    }
    return out;
}

//  Each hold is queued before the last one ends, so ZL is never let go from
//  the lock to the throw (throw_ball() queues its own ZL first).
class HeldZl{
public:
    explicit HeldZl(StoryEnv& env)
        : m_env(env)
        , m_end(current_time())
    {}

    //  `left_y` forward when positive.
    void hold(Milliseconds time, double right_x, double left_y){
        pbf_controller_state(
            m_env.context, BUTTON_ZL, DPAD_NONE,
            /*left_joystick=*/{0, left_y}, /*right_joystick=*/{right_x, 0}, time
        );
        m_end = std::max(m_end, current_time()) + time;
    }

    //  Reads 0.6 s after what was queued, with 0.4 s more of ZL queued.
    std::optional<LockTarget> read(){
        hold(Milliseconds(1000), /*right_x=*/0, /*left_y=*/0);
        const WallClock at = m_end - Milliseconds(400);
        const WallClock now = current_time();
        if (at > now){
            m_env.context.wait_for(std::chrono::duration_cast<Milliseconds>(at - now));
        }
        const VideoSnapshot frame = m_env.env.console.video().snapshot();
        if (!frame){
            return std::nullopt;
        }
        const std::optional<ImageFloatBox> bar = lock_target_plate_bar(frame);
        if (!bar){
            return std::nullopt;
        }
        LockTarget target;
        target.x = bar->x + bar->width / 2;
        target.y = bar->y + bar->height / 2;
        target.caught = lock_target_caught(frame);
        return target;
    }

    std::optional<LockTarget> wait_for_plate(Milliseconds limit){
        const WallClock end = current_time() + limit;
        while (true){
            const std::optional<LockTarget> target = read();
            if (target || current_time() >= end){
                return target;
            }
        }
    }

    std::optional<LockTarget> flick(double x){
        hold(Milliseconds(150), x, /*left_y=*/0);
        return read();
    }

private:
    StoryEnv& m_env;
    WallClock m_end;
};

void log_target(PlzaBackend& backend, std::string_view what, const LockTarget& target){
    backend.log(std::format(
        "[PLZA] catch_in_zone: {}: plate at ({:.2f}, {:.2f}), {}",
        what, target.x, target.y, target.caught_text()
    ));
}

struct TargetChoice{
    //  Nothing when no uncaught target was chosen.
    std::optional<LockTarget> target;
    bool saw_plates = false;
};

//  Reads up to 4 targets by flicking right, then flicks back to the nearest
//  one not caught: the closer one is much more likely to be caught.
TargetChoice choose_target(PlzaBackend& backend, HeldZl& held){
    LockTargetCycle cycle(4);
    std::optional<LockTarget> view = held.wait_for_plate(LOCK_ON_WAIT);
    if (!view){
        backend.log(std::format("[PLZA] catch_in_zone: ZL held {} ms, but no name plate: no lock on", LOCK_ON_WAIT.count()));
    }
    for (int flicks = 0; view && flicks < 4; flicks++){
        log_target(backend, std::format("target {}", cycle.targets().size()), *view);
        if (!cycle.add(*view)){
            break;
        }
        view = held.flick(TARGET_FLICK);
    }
    const std::vector<LockTarget>& targets = cycle.targets();
    TargetChoice choice{.saw_plates = !targets.empty()};
    const std::optional<size_t> best = nearest_uncaught(targets);
    if (!best){
        backend.log(std::format("[PLZA] catch_in_zone: {} plates read, none of a species not caught", targets.size()));
        return choice;
    }
    const int flicks = cycle.flicks_to(*best);
    backend.log(std::format("[PLZA] catch_in_zone: choosing target {} of {}: {} flicks {}", *best, targets.size(), std::abs(flicks), flicks < 0 ? "left" : "right"));
    std::optional<LockTarget> now = targets[*best];
    for (int i = 0; i < std::abs(flicks); i++){
        now = held.flick(flicks < 0 ? -TARGET_FLICK : TARGET_FLICK);
    }
    if (now && !same_lock_target(*now, targets[*best])){
        log_target(backend, "the lock moved on to another plate", *now);
    }
    choice.target = now ? now : targets[*best];
    return choice;
}

//  The lock keeps the camera on the target while stepping toward it.
LockTarget approach_target(LockTarget target, int misses, PlzaBackend& backend, HeldZl& held){
    int steps = 0;
    for (; step_toward_target(target, misses, steps); steps++){
        held.hold(Milliseconds(500), /*right_x=*/0, /*left_y=*/1);
        const std::optional<LockTarget> view = held.read();
        if (!view){
            backend.log(std::format("[PLZA] catch_in_zone: the plate went after {} steps toward the target", steps + 1));
            return target;
        }
        target = *view;
    }
    log_target(backend, std::format("after {} steps toward the target", steps), target);
    return target;
}

TargetChoice aim_choose_approach(StoryEnv& env, PlzaBackend& backend){
    HeldZl held(env);
    TargetChoice choice = choose_target(backend, held);
    if (choice.target){
        choice.target = approach_target(*choice.target, /*misses=*/0, backend, held);
    }
    return choice;
}

struct TargetThrows{
    CatchResult result;
    bool move_on = false;
};

void record_breakout_weaken(StoryEnv& env, PlzaBackend& backend, CatchResult& result, int max_throws, int target_party){
    if (result.throws >= max_throws){
        return;
    }
    backend.log("[PLZA] catch_in_zone: the throw broke out: battle, weakening");
    const CatchResult weakened = weaken_then_throw(env, backend, max_throws - result.throws, target_party);
    result.throws += weakened.throws;
    result.caught = weakened.caught;
    result.party_reached = weakened.party_reached;
}

//  Each throw is logged with its plate's height, the distance signal, to
//  calibrate step_toward_target().
TargetThrows throw_at_target(StoryEnv& env, PlzaBackend& backend, const LockTarget& target, int max_throws, int target_party){
    TargetThrows out;
    LockTarget aimed = target;
    while (out.result.throws < max_throws){
        const bool reached = throw_ball_watched(env, backend, target_party);
        out.result.throws++;
        if (reached){
            backend.log(std::format("[PLZA] catch_in_zone: throw {} at plate y {:.2f}: caught (party {})", out.result.throws, aimed.y, target_party));
            out.result.caught = true;
            out.result.party_reached = true;
            return out;
        }
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        const bool battle = wild_battle_on(env);
        std::optional<HeldZl> held;
        std::optional<LockTarget> relocked;
        if (!battle){
            held.emplace(env);
            relocked = held->wait_for_plate(LOCK_ON_WAIT);
        }
        const bool caught = aimed.caught == false && !battle && !relocked;
        backend.log(std::format("[PLZA] catch_in_zone: throw {} at plate y {:.2f}: {}", out.result.throws, aimed.y, caught ? "caught" : "miss"));
        if (caught){
            out.result.caught = true;
            return out;
        }
        if (battle){
            record_breakout_weaken(env, backend, out.result, max_throws, target_party);
            return out;
        }
        if (relocked && relocked->caught != true){
            if (out.result.throws >= THROWS_AT_ONE_TARGET){
                log_target(backend, std::format("{} misses at one lock: stopping at this spot", out.result.throws), *relocked);
                out.move_on = true;
                return out;
            }
            log_target(backend, "missed, locked on again", *relocked);
            aimed = approach_target(*relocked, out.result.throws, backend, *held);
            continue;
        }
        return out;
    }
    return out;
}

//  Returns whether ZL locked on (the move symbols showed); when not, the
//  sweep's L put the camera back behind the player.
bool sweep_for_lock_on(StoryEnv& env, PlzaBackend& backend, std::string_view spot_name){
    env.context.wait_for_all_requests();
    const WallClock start = current_time();
    const bool locked = lock_on_by_sweep(env.env.console, env.context, SWEEP_STICK_X, SWEEP_ROUNDS);
    const long long elapsed = std::chrono::duration_cast<Milliseconds>(current_time() - start).count();
    if (locked){
        backend.log(std::format("[PLZA] catch_in_zone: at {}: the move symbols showed {} ms into the lock-on sweep: locked on", spot_name, elapsed));
        return true;
    }
    backend.log(std::format("[PLZA] catch_in_zone: at {}: no move symbols in {} sweep rounds ({} ms): L", spot_name, SWEEP_ROUNDS, elapsed));
    return false;
}

//  For a species already caught: no throws. 3 rounds at most.
void fight_off(StoryEnv& env, PlzaBackend& backend){
    for (int round = 1; round <= 3; round++){
        const int landed = weaken_target(env, backend);
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        const bool still_on = wild_battle_on(env);
        backend.log(std::format("[PLZA] catch_in_zone: fighting off, round {}: {} attacks landed, battle {}", round, landed, still_on ? "on" : "over"));
        if (landed == 0 || !still_on){
            return;
        }
    }
}

CatchResult fight_back(StoryEnv& env, PlzaBackend& backend, int max_throws, int target_party){
    const std::optional<LockTarget> attacker = HeldZl(env).wait_for_plate(LOCK_ON_WAIT);
    if (attacker){
        log_target(backend, "a wild battle at the stop, locked on to", *attacker);
    }
    if (attacker && attacker->caught == true){
        backend.log("[PLZA] catch_in_zone: a species already caught: fighting it off, no throws");
        fight_off(env, backend);
        return {};
    }
    backend.log("[PLZA] catch_in_zone: a wild battle at the stop: weakening and throwing");
    return weaken_then_throw(env, backend, max_throws, target_party);
}

CatchResult catch_at_stop(StoryEnv& env, PlzaBackend& backend, std::string_view spot_name, int max_throws, int target_party){
    CatchResult out;
    bool pitch_tipped = false;
    for (int tries = 0; tries < 4 && !out.caught && out.throws < max_throws; tries++){
        backend.scope().throw_if_cancelled();
        if (pitch_tipped){
            level_camera(env);
            pitch_tipped = false;
        }
        if (wild_battle_on(env)){
            const CatchResult fought = fight_back(env, backend, max_throws - out.throws, target_party);
            out.throws += fought.throws;
            out.caught = out.caught || fought.caught;
            out.party_reached = out.party_reached || fought.party_reached;
            pitch_tipped = true;
            continue;
        }
        const bool locked = sweep_for_lock_on(env, backend, spot_name);
        pitch_tipped = true;
        if (!locked){
            backend.log(std::format("[PLZA] catch_in_zone: no lock-on at {} in the sweep", spot_name));
            break;
        }
        const TargetChoice choice = aim_choose_approach(env, backend);
        if (!choice.saw_plates){
            continue;
        }
        if (!choice.target){
            backend.log("[PLZA] catch_in_zone: only species already caught lock on: looking further right");
            pbf_move_right_joystick(env.context, /*position=*/{0.5, 0}, /*hold=*/Milliseconds(400), /*release=*/Milliseconds(300));
            continue;
        }
        const TargetThrows thrown = throw_at_target(env, backend, *choice.target, max_throws - out.throws, target_party);
        out.throws += thrown.result.throws;
        out.caught = out.caught || thrown.result.caught;
        out.party_reached = out.party_reached || thrown.result.party_reached;
        if (thrown.move_on){
            break;
        }
    }
    if (out.caught){
        backend.log(std::format("[PLZA] catch_in_zone: caught at {}", spot_name));
    }else if (pitch_tipped){
        level_camera(env);
    }
    return out;
}

//  See settled_party_slots().
std::optional<int> read_party_slots(StoryEnv& env, PlzaBackend& backend){
    PartyBarDetector bar;
    int looks = 0;
    const std::optional<int> slots = settled_party_slots([&]() -> std::optional<int>{
        if (looks++ > 0){
            env.context.wait_for(Milliseconds(250));
        }
        env.context.wait_for_all_requests();
        const VideoSnapshot frame = env.env.console.video().snapshot();
        if (frame && bar.detect(frame) && bar.filled_slots() > 0){
            return bar.filled_slots();
        }
        return std::nullopt;
    });
    if (!slots){
        backend.log("[PLZA] catch_in_zone: the party bar did not read within 2 s");
        return std::nullopt;
    }
    backend.log(std::format("[PLZA] catch_in_zone: the party bar shows {} filled slots, steady after {} looks", *slots, looks));
    return slots;
}

//  The pin decides whether the player started at the rocks.
std::optional<Pokemon::StoryFailure> go_to_zone_spot(StoryEnv& env, PlzaBackend& backend){
    constexpr double ROCKS_START_RADIUS = 8;
    backend.log(std::format(
        "[PLZA] catch_in_zone: at the start, {} is put at ({:.0f}, {:.0f}) on the map",
        ZONE1_ROCKS.name, ZONE1_ROCKS.pin.x, ZONE1_ROCKS.pin.y
    ));
    const std::optional<MapPoint> start_pin = read_pin_on_map(env, backend);
    const std::string pin_text = start_pin ? std::format("({:.0f}, {:.0f})", start_pin->x, start_pin->y) : std::string("not read");
    const bool at_rocks = start_pin
        && std::hypot(start_pin->x - ZONE1_ROCKS.pin.x, start_pin->y - ZONE1_ROCKS.pin.y) <= ROCKS_START_RADIUS;
    if (!at_rocks){
        backend.log(std::format(
            "[PLZA] catch_in_zone: pin at the start {}, not within {:.0f} of ({:.0f}, {:.0f}): stop",
            pin_text, ROCKS_START_RADIUS, ZONE1_ROCKS.pin.x, ZONE1_ROCKS.pin.y
        ));
        return Pokemon::StoryFailure{.code = "rocks_not_left", .detail = std::format(
            "the pin at the start was {}, not within {:.0f} of ({:.0f}, {:.0f})",
            pin_text, ROCKS_START_RADIUS, ZONE1_ROCKS.pin.x, ZONE1_ROCKS.pin.y
        )};
    }
    backend.log(std::format("[PLZA] catch_in_zone: pin at the start {}: off the rocks to the spot", pin_text));
    backend.press(BUTTON_L, /*hold=*/Milliseconds(100), /*release=*/Milliseconds(150));
    pbf_move_left_joystick(env.context, /*position=*/{-1, 0}, /*hold=*/Milliseconds(900), /*release=*/Milliseconds(300));
    return walk_map_legs(env, backend, ROCKS_TO_SPOT_100);
}

} // namespace


std::optional<Pokemon::StoryFailure> catch_in_zone(StoryEnv& env, PlzaBackend& backend, int max_throws){
    std::optional<int> party_slots = read_party_slots(env, backend);
    if (party_slots && *party_slots >= 5){
        backend.log("[PLZA] catch_in_zone: the party already holds 5 species: nothing to catch");
        return std::nullopt;
    }
    const bool for_fifth = party_slots && *party_slots >= 4;
    const int target_party = for_fifth ? 5 : 4;
    const std::string_view spot_name = for_fifth ? ZONE1_SPOT_80.name : ZONE1_SPOT_100.name;
    std::optional<Pokemon::StoryFailure> walk_failed;
    if (for_fifth){
        //  03.08a: already caught the fourth species; walk straight to the 80 spot for the fifth.
        backend.log(std::format("[PLZA] catch_in_zone: party holds {} species: walking to {} for 5", *party_slots, spot_name));
        walk_failed = walk_map_legs(env, backend, std::span<const MapWaypoint>(&ZONE1_SPOT_80, 1));
    }else{
        //  03.08: catch the fourth species from Taunie's rocks so its autosave
        //  becomes 03.08a's clean retry boundary.
        walk_failed = go_to_zone_spot(env, backend);
    }
    if (walk_failed){
        return walk_failed;
    }
    const CatchResult result = catch_at_stop(env, backend, spot_name, max_throws, target_party);
    if (result.party_reached){
        party_slots = target_party;
    }else if (result.throws > 0){
        party_slots = read_party_slots(env, backend);
    }
    const bool done = party_slots ? (*party_slots >= target_party) : result.caught;
    backend.log(std::format(
        "[PLZA] catch_in_zone: {} throws at {}, caught={}, party_slots={}/{}: {}",
        result.throws, spot_name, result.caught,
        party_slots ? std::to_string(*party_slots) : std::string("unread"),
        target_party, done ? "done" : "short"
    ));
    if (done){
        return std::nullopt;
    }
    return Pokemon::StoryFailure{
        .code = "short_of_species",
        .detail = party_slots
            ? std::format("the party holds {} species of {} ({} throws at {})", *party_slots, target_party, result.throws, spot_name)
            : std::format("no catch at {} in {} throws", spot_name, result.throws),
    };
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

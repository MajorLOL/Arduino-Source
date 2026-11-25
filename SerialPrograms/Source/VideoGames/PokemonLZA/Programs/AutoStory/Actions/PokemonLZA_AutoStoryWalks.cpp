/*  PokemonLZA AutoStory - Walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>
#include <algorithm>
#include <iterator>
#include <optional>
#include <variant>
#include "PokemonLZA_AutoStoryWalks.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenTimers.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenWait.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenPredicates.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;

namespace{

//  Turns land within 2; at 8 off, a 6.5 s leg misses a gate.
constexpr double HEADING_TOLERANCE = 4;

constexpr int HEADING_TURNS = 4;

//  The lab feed has run 0.4 s behind the game: covers that lag.
constexpr int TURN_READS = 10;
constexpr Milliseconds TURN_READ_GAP = 100ms;

//  In (-180, 180].
double turn_to(double now, double heading){
    double turn = std::fmod(heading - now, 360.0);
    if (turn > 180){
        turn -= 360;
    }else if (turn <= -180){
        turn += 360;
    }
    return turn;
}

//  The last read when the arrow never moves half the turn: the turn did not take.
double read_after_turn(PlzaBackend& backend, const MinimapArrowReader& read_arrow, double before, double turn){
    backend.observe();
    double now = read_arrow();
    for (int reads = 1; reads < TURN_READS; reads++){
        if (std::abs(turn_to(before, now)) >= std::abs(turn) / 2){
            break;
        }
        backend.wait(TURN_READ_GAP);
        backend.observe();
        now = read_arrow();
    }
    return now;
}

void story_turn(PlzaBackend& backend, double degrees){
    //  The tap the story was recorded with; short releases run legs back to back.
    backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
    backend.stick(degrees, /*duration=*/150ms, /*release=*/100ms, Pokemon::StoryPace::WALK);
}

} // namespace

std::optional<Pokemon::StoryFailure> face_heading(PlzaBackend& backend, const MinimapArrowReader& read_arrow, double heading){
    backend.observe();
    double now = read_arrow();
    //  Small turns overshoot (5 asked, 10 turned): after each overshoot the
    //  next turn asks for half as much.
    double gain = 1;
    double last_turn = 0;
    for (int turns = 0; turns <= HEADING_TURNS; turns++){
        const double turn = turn_to(now, heading);
        backend.log_detail(std::format("[PLZA] face_heading: the arrow reads {:.1f} deg, {:.0f} wanted: {:.1f} off", now, heading, turn));
        if (std::abs(turn) <= HEADING_TOLERANCE){
            backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
            return std::nullopt;
        }
        if (turns < HEADING_TURNS){
            if (last_turn * turn < 0){
                gain /= 2;
            }
            last_turn = turn;
            const double asked = turn * gain;
            story_turn(backend, asked);
            now = read_after_turn(backend, read_arrow, now, asked);
        }
    }
    return Pokemon::StoryFailure{.code = "heading_missed", .detail = std::format("not facing {:.0f} deg after {} turns", heading, HEADING_TURNS)};
}


namespace{

//  A dropped turn or hold leaves the screen still. A walk into a wall reads
//  still too, and plays its leg twice.
bool screen_still(PlzaBackend& backend){
    return backend.observe().events.has(ScreenEvent::SCREEN_STILL);
}

//  L since the walk goes where the camera looks.
void turn_and_recenter(PlzaBackend& backend, double degrees){
    story_turn(backend, degrees);
    backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
}

template <typename Input>
void play_checked(PlzaBackend& backend, std::string_view what, Input&& input){
    input();
    if (screen_still(backend)){
        backend.log(std::format("[PLZA] walk: the screen stayed still after {}; once more", what));
        input();
    }
}

void recenter_before_held_leg(PlzaBackend& backend, size_t index, const StoryWalkLeg& leg){
    if (index > 0 && leg.turn != 0){
        backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
    }
}

void run_held_legs(PlzaBackend& backend, const std::vector<StoryWalkLeg>& legs){
    for (size_t index = 0; index < legs.size(); index++){
        const StoryWalkLeg& leg = legs[index];
        recenter_before_held_leg(backend, index, leg);
        if (leg.walk > 0ms){
            backend.stick(leg.turn, leg.walk, /*release=*/0ms, Pokemon::StoryPace::RUN);
        }
    }
}

//  "leg 2/5 turn 90 walk 1200 ms"
std::string story_walk_leg_label(size_t index, size_t count, const StoryWalkLeg& leg){
    std::string label = std::format("leg {}/{} turn {:g}", index + 1, count, leg.turn);
    if (leg.walk > Milliseconds(0)){
        label += std::format(" walk {} ms", leg.walk.count());
    }
    return label;
}

} // namespace


void run_story_move(PlzaBackend& backend, const StoryMove& move){
    if (move.style == StoryWalkStyle::HELD){
        run_held_legs(backend, move.legs);
        return;
    }
    //  The look the first input is checked against.
    backend.observe();
    for (size_t index = 0; index < move.legs.size(); index++){
        const StoryWalkLeg& leg = move.legs[index];
        const std::string label = story_walk_leg_label(index, move.legs.size(), leg);
        if (leg.turn != 0){
            play_checked(backend, std::format("the turn of {}", label), [&]{
                turn_and_recenter(backend, leg.turn);
            });
        }
        if (leg.walk > Milliseconds(0)){
            play_checked(backend, std::format("the walk of {}", label), [&]{
                backend.stick(/*degrees=*/0, leg.walk, /*release=*/0ms, Pokemon::StoryPace::RUN);
            });
        }
    }
}


namespace{

void run_story_move_but_last_walk(PlzaBackend& backend, const StoryMove& move){
    if (move.legs.empty()){
        return;
    }
    StoryMove head = move;
    if (move.style == StoryWalkStyle::HELD){
        head.legs.pop_back();
        run_held_legs(backend, head.legs);
        recenter_before_held_leg(backend, head.legs.size(), move.legs.back());
        return;
    }
    head.legs.back().walk = 0ms;
    run_story_move(backend, head);
}

void hold_last_leg(PlzaBackend& backend, const StoryMove& move, Milliseconds slice){
    if (move.style == StoryWalkStyle::HELD && !move.legs.empty()){
        backend.stick(move.legs.back().turn, slice, /*release=*/0ms, Pokemon::StoryPace::RUN);
    }else{
        backend.stick(/*degrees=*/0, slice, /*release=*/0ms, Pokemon::StoryPace::RUN);
    }
}

//  How long each slice of the held last leg is, between looks for its end.
constexpr Milliseconds HOLD_SLICE = 250ms;

//  How long the last leg has been held. The leg is a time budget for its
//  end, not a distance: overshoot is harmless, and falls and fades take
//  game time. A run slice takes less time on a console than its length (the
//  backend runs it faster), so the leg is held until its time has passed on
//  the looks' clock too. A clock that stops (a script's repeated last look)
//  leaves the slices' lengths to end it.
class LastLegHold{
public:
    explicit LastLegHold(Milliseconds hold)
        : m_hold(hold)
    {}

    bool holding() const{
        return m_sliced < m_hold || (m_clock_runs && m_elapsed < m_hold);
    }
    //  The slices' lengths: the logs' and end_after's time.
    Milliseconds sliced() const{
        return m_sliced;
    }
    //  By the looks' clock, from the first slice; at least the budget once done.
    Milliseconds elapsed() const{
        return std::max(m_elapsed, m_hold);
    }

    void held_slice(Milliseconds slice, WallClock look_at){
        m_sliced += slice;
        if (!m_first_look){
            m_first_look = look_at;
            m_first_slice = slice;
        }else{
            m_clock_runs = look_at > m_last_look;
        }
        m_last_look = look_at;
        m_elapsed = m_first_slice + std::chrono::duration_cast<Milliseconds>(look_at - *m_first_look);
    }

private:
    Milliseconds m_hold;
    Milliseconds m_sliced = 0ms;
    Milliseconds m_elapsed = 0ms;
    Milliseconds m_first_slice = 0ms;
    std::optional<WallClock> m_first_look = std::nullopt;
    WallClock m_last_look = WallClock::min();
    bool m_clock_runs = false;
};

constexpr Milliseconds FREE_CONTROL_WAIT = 30000ms;
constexpr Milliseconds FREE_CONTROL_LOOK = 250ms;
//  Nothing busy this long, with no sign of control, is control where no
//  sign shows (Hotel Z's halls). Longer than the ~7 s from the end of
//  01.01's scenes to control.
constexpr Milliseconds QUIET_IS_CONTROL = 10000ms;

//  An A prompt shows only in the overworld, so it is control where no party
//  bar shows (01.03). Held a second, so a prompt left up under a late fade
//  after A at a door is not taken.
constexpr Milliseconds PROMPT_IS_CONTROL = 1000ms;

//  A first glimpse of the prompt can be it fading in or the player still
//  moving (02.05's elevator in German).
constexpr Milliseconds PROMPT_STEADY_GAP = 300ms;

bool prompt_taken(const ScreenObservation& look){
    return PROMPT_TAKEN.matches(look);
}

//  What keeps control from being free; none when it is.
std::optional<std::string_view> not_free_control(const ScreenObservation& look){
    if (look.readings.contains(ADVANCE_ARROW_READING)){
        return "text waiting for A";
    }
    if (look.events.has(ScreenEvent::DIALOG_BOX)){
        return "a text box";
    }
    if (FADE_SHOWS.matches(look)){
        return "a fade";
    }
    return std::nullopt;
}

bool prompt_steady(PlzaBackend& backend){
    backend.wait(PROMPT_STEADY_GAP);
    return backend.observe().events.has(ScreenEvent::INTERACT_PROMPT);
}

constexpr Milliseconds TALK_POLL = 250ms;

struct AtPrompt{
    //  What shows the game took A, and on how many looks in a row.
    bool (*until)(const ScreenObservation& look);
    size_t until_looks;
    //  As in all; how long after an A the prompt must still be up for the next.
    size_t retries;
    Milliseconds retry_gap;
    Milliseconds release;
    std::string_view name;
};

enum class AtPromptEnd{
    TAKEN,
    PROMPT_STAYS,
    NOTHING,
};
struct AtPromptResult{
    AtPromptEnd end;
    size_t presses;
    Milliseconds since_press;
    ScreenObservation look;
};

//  TAKEN: `at.until` shows on `at.until_looks` looks in a row.
//  PROMPT_STAYS: still up after `at.retries` As. NOTHING: the prompt gone at
//  the end of an A's `retry_gap`.
AtPromptResult a_at_prompt(PlzaBackend& backend, const AtPrompt& at){
    backend.press(BUTTON_A, /*hold=*/160ms, at.release);
    size_t presses = 1;
    Milliseconds since_press(0);
    size_t until_looks = 0;
    while (true){
        backend.scope().throw_if_cancelled();
        ScreenObservation look = backend.observe();
        until_looks = at.until(look) ? until_looks + 1 : 0;
        if (until_looks >= at.until_looks){
            return {AtPromptEnd::TAKEN, presses, since_press, std::move(look)};
        }
        if (since_press + TALK_POLL >= at.retry_gap){
            backend.wait(TALK_POLL);
            since_press += TALK_POLL;
            const bool prompt_stays = until_looks == 0 && look.events.has(ScreenEvent::INTERACT_PROMPT);
            if (!prompt_stays){
                return {AtPromptEnd::NOTHING, presses, since_press, std::move(look)};
            }
            if (presses >= at.retries){
                return {AtPromptEnd::PROMPT_STAYS, presses, since_press, std::move(look)};
            }
            backend.log(std::format(
                "[PLZA] {}: the A prompt still up {} ms after A {} with nothing open; A again", at.name, since_press.count(), presses
            ));
            backend.press(BUTTON_A, /*hold=*/160ms, at.release);
            presses++;
            since_press = Milliseconds(0);
            continue;
        }
        backend.wait(TALK_POLL);
        since_press += TALK_POLL;
    }
}

//  A late fade starts 1.8 s after the A.
const AtPrompt THROUGH_PROMPT{
    .until = prompt_taken,
    .until_looks = 1,
    .retries = 3,
    .retry_gap = 3000ms,
    .release = 400ms,
    .name = "press_through_prompt",
};

//  A walk into a talk: no wait for the scene after the prompt goes, the walk
//  goes on instead. Talks start within 250 ms of the A (2026-11-19 logs).
const AtPrompt WALK_TALK{
    .until = scene_under_way,
    .until_looks = 2,
    .retries = 3,
    .retry_gap = 1000ms,
    .release = 240ms,
    .name = "walk",
};

//  How long a walk into a talk looks for the prompt after its last leg.
constexpr Milliseconds TALK_TAIL = 1000ms;

//  Sideways from the facing: 5 left, then 11 right, back past the start.
//  03.03b's ladder showed after 9 (console B, 2026-11-19).
constexpr double PROMPT_SIDESTEPS[] = {-90, -90, -90, -90, -90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90};

//  Only a walk of one straight leg to the first A prompt ahead; one that
//  turns, has earlier legs, or an `end_after` goes to a prompt elsewhere.
bool ends_before_walking(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, ScreenEvent end, Milliseconds end_after
){
    const bool straight_to_prompt = end == ScreenEvent::INTERACT_PROMPT && end_after == 0ms &&
        move.legs.size() == 1 && move.legs[0].turn == 0;
    if (!straight_to_prompt || !backend.observe().events.has(end) || !prompt_steady(backend)){
        return false;
    }
    backend.log(std::format("[PLZA] {}: the A prompt shows before the walk: no walk.", walk));
    return true;
}

//  None: walk on. Set: the walk ends, with its failure if any.
std::optional<std::optional<Pokemon::StoryFailure>> end_event_look(
    PlzaBackend& backend, std::string_view walk, const ScreenObservation& look,
    ScreenEvent end, Milliseconds held, Milliseconds end_after
){
    if (look.events.has(end) && held < end_after){
        backend.log(std::format(
            "[PLZA] {}: the end event after {} ms of the last leg, before its {} ms: passed on the way, on.",
            walk, held.count(), end_after.count()
        ));
        return std::nullopt;
    }
    if (look.events.has(end) && end == ScreenEvent::INTERACT_PROMPT && !prompt_steady(backend)){
        backend.log(std::format("[PLZA] {}: the A prompt after {} ms of the last leg went again: on.", walk, held.count()));
        return std::nullopt;
    }
    if (look.events.has(end)){
        backend.log(std::format("[PLZA] {}: the end event after {} ms of the last leg.", walk, held.count()));
        backend.log_detail(std::format("[PLZA] {}: ended on: {}", walk, describe_scene_look(look)));
        return std::optional<Pokemon::StoryFailure>();
    }
    if (look.events.has(ScreenEvent::FADE_BLACK)){
        backend.log(std::format("[PLZA] {}: a black fade after {} ms of the last leg: the scene takes over.", walk, held.count()));
        return std::optional<Pokemon::StoryFailure>();
    }
    return std::nullopt;
}

//  A on the prompt a walk into a talk reached. Set: the walk ends, with its
//  failure if any. None: the prompt went with nothing started.
std::optional<std::optional<Pokemon::StoryFailure>> talk_at_walk_prompt(PlzaBackend& backend, std::string_view walk, Milliseconds held){
    const AtPromptResult result = a_at_prompt(backend, WALK_TALK);
    switch (result.end){
    case AtPromptEnd::TAKEN:
        backend.log(std::format("[PLZA] {}: A on the A prompt after {} ms of the last leg started a talk.", walk, held.count()));
        backend.log_detail(std::format("[PLZA] {}: started on: {}", walk, describe_scene_look(result.look)));
        return std::optional<Pokemon::StoryFailure>();
    case AtPromptEnd::PROMPT_STAYS:
        return std::optional<Pokemon::StoryFailure>(Pokemon::StoryFailure{.code = "talk_no_scene", .detail = std::format(
            "{}: the A prompt still up with nothing open after {} As: the game took none", walk, result.presses
        )});
    case AtPromptEnd::NOTHING:
        break;
    }
    backend.log(std::format("[PLZA] {}: the A prompt went with nothing started after A: on.", walk));
    return std::nullopt;
}

struct FreeControlWatch{
    std::string_view last_busy = "nothing";
    Pokemon::ConfirmStreak control_streak{2};
    Pokemon::HoldTimer quiet_hold{QUIET_IS_CONTROL};
    Pokemon::HoldTimer prompt_hold{PROMPT_IS_CONTROL};
    size_t poll_looks = 0;
};

bool check_free_control(PlzaBackend& backend, std::string_view walk, const ScreenObservation& look, FreeControlWatch& watch){
    const WallClock poll_time = WallClock::min() + watch.poll_looks * FREE_CONTROL_LOOK;
    watch.poll_looks++;
    const std::optional<std::string_view> busy = not_free_control(look);
    if (busy){
        watch.last_busy = *busy;
    }
    const bool quiet_ready = watch.quiet_hold.update(!busy.has_value(), poll_time);
    if (watch.control_streak.update(OVERWORLD_FREE.matches(look))){
        return true;
    }
    if (watch.prompt_hold.update(!busy && look.events.has(ScreenEvent::INTERACT_PROMPT), poll_time)){
        backend.log(std::format("[PLZA] {}: the A prompt held {} ms with nothing busy: control.", walk, PROMPT_IS_CONTROL.count()));
        return true;
    }
    if (quiet_ready){
        backend.log(std::format(
            "[PLZA] {}: nothing busy for {} ms and no sign of control; taken as control.", walk, QUIET_IS_CONTROL.count()
        ));
        return true;
    }
    return false;
}

struct LastLegOutcome{
    std::optional<std::optional<Pokemon::StoryFailure>> ended;
    Milliseconds sliced = 0ms;
    Milliseconds elapsed = 0ms;
    Pokemon::ConfirmStreak battle_streak{2};
};

//  Runs all legs but the last walk, then slices the last leg until `on_look` ends it,
//  a battle HUD holds (if `stop_on_battle`), or the hold budget elapses.
template <typename OnLook>
LastLegOutcome walk_last_leg(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, bool stop_on_battle, OnLook&& on_look
){
    const Milliseconds hold = move.legs.empty() ? 0ms : move.legs.back().walk;
    run_story_move_but_last_walk(backend, move);
    LastLegHold leg(hold);
    LastLegOutcome outcome;
    //  The slices stop the stick only as long as a look takes.
    while (leg.holding()){
        backend.scope().throw_if_cancelled();
        const Milliseconds slice = leg.sliced() < hold ? std::min(HOLD_SLICE, hold - leg.sliced()) : HOLD_SLICE;
        hold_last_leg(backend, move, slice);
        const ScreenObservation look = backend.observe();
        leg.held_slice(slice, look.when);
        if (stop_on_battle && outcome.battle_streak.update(look.events.has(ScreenEvent::BATTLE_STARTED))){
            backend.log(std::format("[PLZA] {}: battle HUD showed after {} ms of the last leg: stopping walk.", walk, leg.sliced().count()));
            outcome.ended = std::optional<Pokemon::StoryFailure>(battle_on_screen_failure());
            return outcome;
        }
        if (std::optional<std::optional<Pokemon::StoryFailure>> ended = on_look(look, leg.sliced())){
            outcome.ended = std::move(ended);
            return outcome;
        }
    }
    outcome.sliced = leg.sliced();
    outcome.elapsed = leg.elapsed();
    return outcome;
}

} // namespace


std::optional<Pokemon::StoryFailure> check_can_walk(PlzaBackend& backend, std::string_view walk){
    FreeControlWatch watch;
    const Pokemon::WaitOutcome outcome = Pokemon::wait_for(
        backend,
        [&](const ScreenObservation& look){
            return check_free_control(backend, walk, look, watch);
        },
        Pokemon::WaitRule{.poll = FREE_CONTROL_LOOK, .timeout = FREE_CONTROL_WAIT}
    );
    if (const auto* failure = std::get_if<Pokemon::StoryFailure>(&outcome)){
        return *failure;
    }
    if (std::holds_alternative<Pokemon::WaitMatched>(outcome)){
        return std::nullopt;
    }
    return Pokemon::StoryFailure{.code = "walk_not_free", .detail =
        std::format("{}: no free control within {} ms, last on {}", walk, FREE_CONTROL_WAIT.count(), watch.last_busy)
    };
}

void finish_walk(PlzaBackend& backend, std::string_view walk){
    const ScreenObservation look = backend.observe();
    backend.log(std::format("[PLZA] {}: legs done, screen now: {}", walk, look.events));
}

std::optional<Pokemon::StoryFailure> walk_until(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, const WalkStopOptions& options
){
    if (options.end && ends_before_walking(backend, walk, move, *options.end, options.end_after)){
        return std::nullopt;
    }
    size_t scene_looks = 0;
    const LastLegOutcome walked = walk_last_leg(
        backend, walk, move, options.stop_on_battle,
        [&](const ScreenObservation& look, Milliseconds held) -> std::optional<std::optional<Pokemon::StoryFailure>>{
            if (!options.end){
                const std::optional<std::string_view> busy = not_free_control(look);
                scene_looks = busy ? scene_looks + 1 : 0;
                if (busy && scene_looks >= 2){
                    backend.log(std::format(
                        "[PLZA] {}: a scene ({}) took control after {} ms of the last leg: the walk stops.", walk, *busy, held.count()
                    ));
                    return std::optional<Pokemon::StoryFailure>();
                }
                return std::nullopt;
            }
            return end_event_look(backend, walk, look, *options.end, held, options.end_after);
        }
    );
    if (walked.ended){
        return *walked.ended;
    }
    if (!options.end){
        return Pokemon::StoryFailure{.code = "walk_no_scene", .detail = std::format("{}: control still free after {} ms of the last leg: no scene took it", walk, walked.elapsed.count())};
    }
    return Pokemon::StoryFailure{.code = "walk_no_end", .detail = std::format("{}: no end event within {} ms of the last leg", walk, walked.elapsed.count())};
}

std::optional<Pokemon::StoryFailure> walk_into_talk(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, const WalkStopOptions& options
){
    if (ends_before_walking(backend, walk, move, ScreenEvent::INTERACT_PROMPT, /*end_after=*/0ms)){
        if (const std::optional<std::optional<Pokemon::StoryFailure>> ended = talk_at_walk_prompt(backend, walk, 0ms)){
            return *ended;
        }
    }
    LastLegOutcome walked = walk_last_leg(
        backend, walk, move, options.stop_on_battle,
        [&](const ScreenObservation& look, Milliseconds held) -> std::optional<std::optional<Pokemon::StoryFailure>>{
            if (options.after_legs || !look.events.has(ScreenEvent::INTERACT_PROMPT)){
                return std::nullopt;
            }
            return talk_at_walk_prompt(backend, walk, held);
        }
    );
    if (walked.ended){
        return *walked.ended;
    }
    for (Milliseconds tail = 0ms; tail < TALK_TAIL; ){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        if (options.stop_on_battle && walked.battle_streak.update(look.events.has(ScreenEvent::BATTLE_STARTED))){
            backend.log(std::format("[PLZA] {}: battle HUD showed after {} ms of the last leg: stopping walk.", walk, walked.sliced.count()));
            return battle_on_screen_failure();
        }
        if (!look.events.has(ScreenEvent::INTERACT_PROMPT)){
            backend.wait(TALK_POLL);
            tail += TALK_POLL;
            continue;
        }
        if (const std::optional<std::optional<Pokemon::StoryFailure>> ended = talk_at_walk_prompt(backend, walk, walked.sliced)){
            return *ended;
        }
        tail += WALK_TALK.retry_gap;
    }
    return Pokemon::StoryFailure{.code = "walk_no_end", .detail = std::format(
        "{}: no talk started within {} ms after the last leg", walk, TALK_TAIL.count()
    )};
}

std::optional<size_t> step_along_until_prompt(PlzaBackend& backend, double facing_degrees){
    for (size_t steps = 0; ; steps++){
        backend.scope().throw_if_cancelled();
        if (backend.observe().events.has(ScreenEvent::INTERACT_PROMPT)){
            return steps;
        }
        if (steps == std::size(PROMPT_SIDESTEPS)){
            return std::nullopt;
        }
        backend.stick(facing_degrees + PROMPT_SIDESTEPS[steps], /*duration=*/250ms, /*release=*/100ms, Pokemon::StoryPace::WALK);
        backend.stick(facing_degrees, /*duration=*/120ms, /*release=*/500ms, Pokemon::StoryPace::WALK);
    }
}

void press_through_prompt(PlzaBackend& backend){
    const AtPromptResult result = a_at_prompt(backend, THROUGH_PROMPT);
    if (result.end == AtPromptEnd::PROMPT_STAYS){
        backend.log(std::format(
            "[PLZA] press_through_prompt: the prompt is still up {} ms after A {}", result.since_press.count(), result.presses
        ));
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

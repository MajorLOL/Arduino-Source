/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <chrono>
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenWait.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenPredicates.h"
#include "PokemonLZA_AutoStoryScenes.h"
#include "PokemonLZA_AutoStoryWalks.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenTimers.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::StoryFailure;

namespace{

//  Up the 03.03 ladder to the roof: about 6 s by hand on 2026-11-19.
constexpr Milliseconds LADDER_CLIMB = 6500ms;
constexpr Milliseconds MAP_TALK_TIMEOUT = 120s;
constexpr Milliseconds MAP_TALK_CONTROL_HOLD = 1000ms;
constexpr Milliseconds MAP_TALK_HOLD = 160ms;
constexpr Milliseconds MAP_TALK_RELEASE = 840ms;
constexpr Milliseconds MAP_TALK_POLL = 250ms;

//  A on a tutorial card (B turns its pages back) and on text before it; B
//  on the rest: on the map A would place a pin.
enum class MapTalkPart{BEFORE_CARD, AFTER_CARD};
Button map_talk_button(const ScreenObservation& look, MapTalkPart part){
    if (look.readings.contains(TUTORIAL_CARD_READING)){
        return BUTTON_A;
    }
    return TEXT_SHOWS.matches(look) && part == MapTalkPart::BEFORE_CARD ? BUTTON_A : BUTTON_B;
}


constexpr Milliseconds CHOICE_WAIT(3000);
constexpr Milliseconds CHOICE_POLL(250);

//  (highlighted row, row count).
std::optional<std::pair<int, int>> choice_rows(const ScreenObservation& look){
    if (!CHOICE_WITH_ROWS.matches(look)){
        return std::nullopt;
    }
    return std::make_pair(
        look.readings.find(CHOICE_SELECTED_READING)->second,
        look.readings.find(CHOICE_ROWS_READING)->second
    );
}

Pokemon::WaitOutcome wait_for_choice_rows(PlzaBackend& backend){
    return Pokemon::wait_for(
        backend,
        CHOICE_WITH_ROWS,
        Pokemon::WaitRule{.poll = CHOICE_POLL, .timeout = CHOICE_WAIT}
    );
}

//  A presses through a question's text (e.g. "Which floor?") before its choices show.
constexpr int CHOICE_TEXT_PRESSES = 3;

} // namespace


std::optional<Pokemon::StoryFailure> talk_through_map_screen(PlzaBackend& backend){
    MapTalkPart part = MapTalkPart::BEFORE_CARD;
    const Pokemon::WaitOutcome outcome = Pokemon::poll_until(
        backend,
        PARTY_BAR_FREE,
        Pokemon::WaitRule{.poll = MAP_TALK_POLL, .timeout = MAP_TALK_TIMEOUT, .confirm_hold = MAP_TALK_CONTROL_HOLD},
        [&](const ScreenObservation& look){
            if (PARTY_BAR_FREE.matches(look)){
                backend.wait(MAP_TALK_POLL);
                return;
            }
            backend.press(map_talk_button(look, part), MAP_TALK_HOLD, MAP_TALK_RELEASE);
            if (look.readings.contains(TUTORIAL_CARD_READING)){
                part = MapTalkPart::AFTER_CARD;
            }
        }
    );
    if (const auto* failure = std::get_if<Pokemon::StoryFailure>(&outcome)){
        return *failure;
    }
    if (std::holds_alternative<Pokemon::WaitMatched>(outcome)){
        backend.log("[PLZA] talk_through_map_screen: the party bar held: back in the overworld");
        return std::nullopt;
    }
    return Pokemon::StoryFailure{.code = "map_talk_timeout", .detail = std::format(
        "no party bar held {} ms within {} ms of A on the talk and the card, B on the map",
        MAP_TALK_CONTROL_HOLD.count(), MAP_TALK_TIMEOUT.count()
    )};
}

std::optional<Pokemon::StoryFailure> climb_ladder(PlzaBackend& backend){
    //  The run-pace side street leg stops left or right of the ladder
    //  (2026-11-19): a fixed sidestep missed it, so step along the wall
    //  until the prompt shows. "Climb Up" shows only facing the ladder.
    backend.stick(/*degrees=*/0, /*duration=*/150ms, /*release=*/500ms, Pokemon::StoryPace::WALK);
    const std::optional<size_t> steps = step_along_until_prompt(backend, /*facing_degrees=*/0);
    if (!steps){
        return Pokemon::StoryFailure{.code = "ladder_not_found", .detail = "no Climb Up prompt stepping along the wall"};
    }
    backend.log(std::format("[PLZA] climb_ladder: Climb Up after {} steps along the wall", *steps));
    press_through_prompt(backend);
    backend.stick(/*degrees=*/0, /*duration=*/LADDER_CLIMB, /*release=*/500ms, Pokemon::StoryPace::WALK);
    return std::nullopt;
}

std::optional<Pokemon::StoryFailure> choose_row(PlzaBackend& backend, int index){
    Pokemon::WaitOutcome outcome = wait_for_choice_rows(backend);
    for (int press = 0; std::holds_alternative<Pokemon::WaitTimeout>(outcome) && press < CHOICE_TEXT_PRESSES; press++){
        backend.log(std::format("[PLZA] choose_{}: no choice yet: A through the text", index));
        backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
        outcome = wait_for_choice_rows(backend);
    }
    if (const auto* failure = std::get_if<Pokemon::StoryFailure>(&outcome)){
        return *failure;
    }
    const auto* matched = std::get_if<Pokemon::WaitMatched>(&outcome);
    if (matched == nullptr){
        return Pokemon::StoryFailure{.code = "no_choice", .detail = std::format("no choice box with rows that read after {} A presses", CHOICE_TEXT_PRESSES)};
    }
    const auto [selected, count] = *choice_rows(matched->look);
    if (index >= count){
        return Pokemon::StoryFailure{.code = "no_such_choice", .detail = std::format("row {} of a choice of {} rows", index, count)};
    }
    backend.log(std::format("[PLZA] choose_{}: row {} of {} highlighted", index, selected, count));
    const int moves = index - selected;
    for (int moved = 0; moved < std::abs(moves); moved++){
        backend.press(moves > 0 ? BUTTON_DOWN : BUTTON_UP, /*hold=*/160ms, /*release=*/240ms);
    }
    if (moves != 0){
        const std::optional<std::pair<int, int>> now = choice_rows(backend.observe());
        if (!now || now->first != index){
            return Pokemon::StoryFailure{.code = "choice_not_reached", .detail = std::format(
                "the highlight is on row {}, not {}, after {} presses from row {}",
                now ? std::to_string(now->first) : "none", index, std::abs(moves), selected
            )};
        }
    }
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
    return std::nullopt;
}


namespace{

//  The autosave badge shows for 1-3 s: at least 2 looks a second.
constexpr Milliseconds PLATFORM_POLL(300);
constexpr size_t PLATFORM_LOOKS = 2;

//  A Plus held while the confirm screen still fades out does nothing (console
//  B, 2026-11-19: held 4 ms into the step). The opening's "hold to skip"
//  takes one this long after its picture fades in.
constexpr Milliseconds OPENING_SKIP_AFTER_FADE_IN = 750ms;

const Pokemon::ScreenMatch AUTOSAVE_SHOWS{
    .all_readings = {AUTOSAVE_READING},
};

constexpr Milliseconds TEXT_SPEED_POLL = 250ms;
constexpr Milliseconds MENU_OPEN_WAIT = 2000ms;
constexpr Milliseconds TEXT_SPEED_LIMIT = 30000ms;
constexpr size_t TEXT_SPEED_HELD_LOOKS = 2;

//  From the main menu to the options' text speed, set to Fast; each with
//  its release.
constexpr std::pair<Button, Milliseconds> TEXT_SPEED_PRESSES[] = {
    {BUTTON_L, 600ms},
    {BUTTON_UP, 300ms},
    {BUTTON_A, 1100ms},
    {BUTTON_DOWN, 300ms},
    {BUTTON_RIGHT, 300ms},
};

//  The save file's first menu opens under a tutorial card. A closes it; the
//  card fades out over ~1 s and reads as a card meanwhile, so a second A
//  too soon lands on the main menu and opens the bag (console B,
//  2026-11-19: 01.01 failed 3/3 that way).
constexpr Milliseconds CARD_CLOSE_WAIT = 1500ms;
constexpr size_t CARD_PAGES_MAX = 3;

//  B until the overworld holds; B on the overworld does nothing.
std::optional<StoryFailure> back_to_overworld(PlzaBackend& backend){
    const Pokemon::WaitOutcome outcome = Pokemon::poll_until(
        backend,
        OVERWORLD_FREE,
        Pokemon::WaitRule{.poll = TEXT_SPEED_POLL, .timeout = TEXT_SPEED_LIMIT, .confirm_looks = TEXT_SPEED_HELD_LOOKS},
        [&](const ScreenObservation& look){
            if (OVERWORLD_FREE.matches(look)){
                backend.wait(TEXT_SPEED_POLL);
            }else{
                backend.press(BUTTON_B, /*hold=*/160ms, /*release=*/840ms);
            }
        }
    );
    if (const auto* failure = std::get_if<StoryFailure>(&outcome)){
        return *failure;
    }
    if (const auto* matched = std::get_if<Pokemon::WaitMatched>(&outcome)){
        backend.log(std::format("[PLZA set_text_speed_fast] text speed set, back on the overworld at {:.1f}s", matched->elapsed.count() / 1000.0));
        return std::nullopt;
    }
    return StoryFailure{.code = "text_speed_timeout", .detail = std::format(
        "not back on the overworld in {} ms of B after the text speed rows", TEXT_SPEED_LIMIT.count()
    )};
}

} // namespace


std::optional<StoryFailure> wait_for_platform(PlzaBackend& backend, Milliseconds timeout){
    const Pokemon::WaitOutcome outcome = Pokemon::wait_for(
        backend,
        PLATFORM_HINT,
        Pokemon::WaitRule{.poll = PLATFORM_POLL, .timeout = timeout, .confirm_looks = PLATFORM_LOOKS}
    );
    if (const auto* failure = std::get_if<StoryFailure>(&outcome)){
        return *failure;
    }
    if (const auto* matched = std::get_if<Pokemon::WaitMatched>(&outcome)){
        backend.log(std::format("[PLZA] platform wait: on the platform after {} looks", matched->looks));
        return std::nullopt;
    }
    return StoryFailure{.code = "platform_not_seen", .detail = std::format(
        "No controls hint on {} looks in a row within {} ms.",
        PLATFORM_LOOKS, timeout.count()
    )};
}

std::optional<StoryFailure> opening_to_platform(PlzaBackend& backend, Milliseconds timeout){
    Pokemon::EdgeTrigger text_waiting;
    Pokemon::EdgeTrigger fade_shown;
    std::optional<WallClock> faded_in;
    const Pokemon::WaitOutcome outcome = Pokemon::poll_until(
        backend,
        AUTOSAVE_SHOWS,
        Pokemon::WaitRule{.poll = PLATFORM_POLL, .timeout = timeout},
        [&](const ScreenObservation& look){
            if (text_waiting.rising(TEXT_WAITING_FOR_A.matches(look))){
                backend.log("[PLZA] platform wait: text waiting for A; A");
                backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/400ms);
            }
            const bool fade = FADE_SHOWS.matches(look);
            if (fade){
                faded_in.reset();
            }
            if (fade_shown.falling(fade)){
                faded_in = look.when;
            }
            if (faded_in &&
                look.when > *faded_in &&
                look.when - *faded_in >= OPENING_SKIP_AFTER_FADE_IN &&
                !TEXT_SHOWS.matches(look) &&
                !look.readings.contains(CONTROLS_HINT_READING)
            ){
                backend.log("[PLZA] platform wait: the opening plays; Plus held to skip it");
                backend.press(BUTTON_PLUS, CUTSCENE_SKIP_HOLD, /*release=*/400ms);
                faded_in.reset();
            }
            backend.wait(PLATFORM_POLL);
        }
    );
    if (const auto* failure = std::get_if<StoryFailure>(&outcome)){
        return *failure;
    }
    if (const auto* matched = std::get_if<Pokemon::WaitMatched>(&outcome)){
        backend.log("[PLZA] platform wait: the autosave badge is on screen");
        backend.wait(PLATFORM_POLL);
        const Milliseconds remaining = matched->elapsed >= timeout ? 0ms : timeout - matched->elapsed;
        return wait_for_platform(backend, remaining);
    }
    return StoryFailure{.code = "platform_not_seen", .detail = std::format(
        "No autosave badge seen within {} ms.", timeout.count()
    )};
}


constexpr Milliseconds LINES_POLL(250);

LinesPassed press_a_through_lines(PlzaBackend& backend, Milliseconds quiet, Milliseconds limit){
    LinesPassed passed;
    //  Bounds the looks too, for a script whose time stands still.
    const size_t max_looks = (size_t)(limit / LINES_POLL) + 1;
    std::optional<WallClock> start;
    Pokemon::HoldTimer quiet_hold(quiet);
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            passed.timed_out = true;
            return passed;
        }
        start = start.value_or(look.when);
        const bool card = look.readings.contains(TUTORIAL_CARD_READING);
        const bool text = TEXT_SHOWS.matches(look);
        if (card || text){
            passed.card = passed.card || card;
            passed.text = passed.text || text;
            quiet_hold.reset();
            backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
        }else if (quiet_hold.update(true, look.when)){
            return passed;
        }
        if (look.when - *start >= limit){
            break;
        }
        //  The next look right after a press: the press's 400 ms is the beat.
        if (!card && !text){
            backend.wait(LINES_POLL);
        }
    }
    passed.timed_out = true;
    return passed;
}

namespace{

bool out_of_menu(const ScreenObservation& look){
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) || look.events.has(ScreenEvent::DIALOG_BOX);
}

} // namespace

bool press_b_out_of_menu(PlzaBackend& backend, size_t presses){
    for (size_t pressed = 0; pressed < presses; pressed++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            return false;
        }
        if (out_of_menu(look)){
            return true;
        }
        backend.press(BUTTON_B, /*hold=*/160ms, /*release=*/840ms);
    }
    const ScreenObservation look = backend.observe();
    if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
        return false;
    }
    return out_of_menu(look);
}

//  Blind but for the first menu's tutorial card and the way back.
std::optional<StoryFailure> set_text_speed_fast(PlzaBackend& backend){
    backend.press(BUTTON_X, /*hold=*/160ms, /*release=*/240ms);
    backend.wait(MENU_OPEN_WAIT);
    for (size_t pages = 0; pages < CARD_PAGES_MAX; pages++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
            return Pokemon::game_not_running_failure();
        }
        if (!look.readings.contains(TUTORIAL_CARD_READING)){
            break;
        }
        backend.log("[PLZA set_text_speed_fast] tutorial card: A");
        backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
        backend.wait(CARD_CLOSE_WAIT);
    }
    for (const auto& [button, release] : TEXT_SPEED_PRESSES){
        backend.press(button, /*hold=*/100ms, release);
    }
    return back_to_overworld(backend);
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

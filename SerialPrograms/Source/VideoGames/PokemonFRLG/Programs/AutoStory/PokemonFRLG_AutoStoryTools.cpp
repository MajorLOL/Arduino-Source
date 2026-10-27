/*  Pokemon FRLG Autostory Tools
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_StartMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattlePokemonDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_SelectionArrowDetector.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_StartMenuNavigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_AutoStoryWalkTracker.h"
#include "PokemonFRLG_AutoStoryTools.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{


//  Gen 3 movement (pokefirered decomp, field_player_avatar.c and
//  event_object_movement.c):
//    - Pressing a direction the player is not moving in, from a standstill,
//      first turns them in place (8 frames). If the direction is still held
//      when the turn is over, they move.
//    - A walking step takes 16 frames, a ledge jump 32 frames (2 tiles), a
//      step while surfing 8 frames (same speed as running), a tile on a
//      spinner 8 frames.
//    - The game reads the d-pad when a step finishes. If a direction is
//      held then, the player keeps going that way without stopping.
constexpr double FRAME_MS = 16.74;
constexpr double TURN_FRAMES = 8;
constexpr double WALK_FRAMES = 16;
constexpr double JUMP_FRAMES = 32;
constexpr double SURF_FRAMES = 8;
constexpr double SPIN_FRAMES = 8;

//  The text of the overworld text box.
const ImageFloatBox DIALOG_TEXT_BOX(0.080, 0.765, 0.840, 0.175);


Milliseconds frames_to_ms(double frames){
    if (frames < 0){
        frames = 0;
    }
    return Milliseconds((int64_t)std::lround(frames * FRAME_MS));
}

const char* dpad_name(DpadPosition direction){
    switch (direction){
    case DPAD_UP:       return "up";
    case DPAD_DOWN:     return "down";
    case DPAD_LEFT:     return "left";
    case DPAD_RIGHT:    return "right";
    default:            return "none";
    }
}


//  One move (a step, a jump, a step on the water) from a standstill,
//  whichever way the player faces.
//
//  Walking (16-frame steps, 32-frame jumps): the press is held 12 frames.
//    - Facing that way: the move starts at once. The input is gone before
//      it ends, so the player stops after one move.
//    - Facing another way: the turn ends at frame 8, the input is still
//      held, so one move starts. The input is gone before it ends.
//  Surfing (8-frame steps): 12 frames would be a second step when already
//  facing that way, so then the press is 4 frames.
void single_move(
    ProControllerContext& context, DpadPosition& facing,
    DpadPosition direction, double move_frames, bool surf
){
    double press = 12;
    if (surf && facing == direction){
        press = 4;
    }
    double total = TURN_FRAMES + move_frames + 6;
    pbf_press_dpad(context, direction, frames_to_ms(press), frames_to_ms(total - press));
    facing = direction;
}


//  The new map is fading in: wait until the player can move.
void wait_after_warp(StoryRuntime& rt, MapPreview preview, const char* leg_name){
    switch (preview){
    case MapPreview::FOREST:
        //  The area picture is drawn over the new map: a fade in, 40 to 120
        //  frames, a cross-fade of about 50 frames. The player cannot move
        //  until it is gone (Task_RunMapPreviewScreenForest).
        rt.console.log(std::string("Area picture, waiting for it to go away: ") + leg_name);
        pbf_wait(rt.context, 5000ms);
        break;
    case MapPreview::CAVE:
        //  A full screen picture comes first. Holding B ends it
        //  (Task_MapPreviewScreen_0), then a flash and the map fades in.
        rt.console.log(std::string("Area picture, skipping it with B: ") + leg_name);
        pbf_press_button(rt.context, BUTTON_B, 1500ms, 3500ms);
        break;
    case MapPreview::NONE:
        //  Coming out of a door, the game opens the door, walks the player
        //  one step out and closes it again (Task_ExitDoor), and input is
        //  ignored until that is done.
        pbf_wait(rt.context, 2000ms);
        break;
    }
    rt.context.wait_for_all_requests();
}

//  Hold "direction" from a standstill until the warp has finished.
//  Doors, mats and stairs all warp while the direction is held:
//    door:   facing the door, holding toward it      (TryDoorWarp)
//    mat:    standing on it, holding the arrow       (TryArrowWarp)
//    stairs: standing on them, holding sideways      (TryArrowWarp)
void take_warp_push(
    StoryRuntime& rt, DpadPosition direction, bool dark, MapPreview preview, const char* leg_name
){
    BlackScreenOverWatcher warp_finished(COLOR_RED);
    rt.context.wait_for_all_requests();
    int ret = run_until<ProControllerContext>(
        rt.console, rt.context,
        [&](ProControllerContext& context){
            pbf_press_dpad(context, direction, 1500ms, 0ms);
            pbf_wait(context, 10000ms);
        },
        { warp_finished }
    );
    if (ret < 0){
        if (dark){
            //  Without Flash most of the screen is black, so the end of the
            //  fade may not be seen. Carry on.
            rt.console.log(std::string("Warp not seen on a dark map: ") + leg_name, COLOR_ORANGE);
        }else{
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::string("take_warp(): No warp at the end of ") + leg_name +
                ". The player is probably not where the route expects (blocked by an NPC?).",
                rt.console
            );
        }
    }
    wait_after_warp(rt, preview, leg_name);
}

//  The last step landed on a warp tile (ladder, hole, warp pad, cave), or
//  a spinner carries the player onto one.
void wait_warp_step(StoryRuntime& rt, bool dark, MapPreview preview, const char* leg_name){
    BlackScreenOverWatcher warp_finished(COLOR_RED);
    int ret = wait_until(rt.console, rt.context, 10000ms, { warp_finished });
    if (ret < 0){
        if (dark){
            rt.console.log(std::string("Warp not seen on a dark map: ") + leg_name, COLOR_ORANGE);
        }else{
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::string("walk_leg(): Stepping onto the warp did not warp: ") + leg_name,
                rt.console
            );
        }
    }
    wait_after_warp(rt, preview, leg_name);
}

//  Take the last step, onto a warp tile. If the step did not happen (the
//  game ignored the input), take it again.
void step_onto_warp(
    StoryRuntime& rt, DpadPosition direction, double frames, bool surf,
    bool dark, MapPreview preview, const char* leg_name
){
    for (int attempt = 0; attempt < 3; attempt++){
        BlackScreenOverWatcher warp_finished(COLOR_RED);
        rt.context.wait_for_all_requests();
        int ret = run_until<ProControllerContext>(
            rt.console, rt.context,
            [&](ProControllerContext& context){
                single_move(context, rt.facing, direction, frames, surf);
                pbf_wait(context, 5000ms);
            },
            { warp_finished }
        );
        rt.facing = direction;
        rt.steps++;
        if (ret == 0){
            wait_after_warp(rt, preview, leg_name);
            return;
        }
        if (dark){
            rt.console.log(std::string("Warp not seen on a dark map: ") + leg_name, COLOR_ORANGE);
            wait_after_warp(rt, preview, leg_name);
            return;
        }
        rt.console.log(std::string("No warp after the last step of ") + leg_name + ". Taking the step again.", COLOR_ORANGE);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::string("walk_leg(): Stepping onto the warp did not warp: ") + leg_name +
        ". The player is probably not where the route expects.",
        rt.console
    );
}


DpadPosition opposite(DpadPosition direction){
    switch (direction){
    case DPAD_UP:       return DPAD_DOWN;
    case DPAD_DOWN:     return DPAD_UP;
    case DPAD_LEFT:     return DPAD_RIGHT;
    case DPAD_RIGHT:    return DPAD_LEFT;
    default:            return DPAD_NONE;
    }
}


//  Wait (a moment at most) until the screen stops changing: menus closed,
//  fades over. A move is measured against the first picture, which must
//  show the map as it is.
void wait_for_still_screen(StoryRuntime& rt){
    StableScreenWatcher stable(100ms);
    rt.context.wait_for_all_requests();
    wait_until(rt.console, rt.context, 1000ms, { {stable, 10ms} });
}

//  What ended an attempt to walk part of a run.
enum class RunEvent{
    NONE,               //  The input was done.
    SCREEN_CHANGED,     //  The video no longer shows the map (battle, fade, menu).
    BATTLE_START,       //  The screen went black: a battle transition.
    BATTLE_TEXT,        //  Battle text with the advance arrow.
    DIALOG,             //  A text box: a trainer saw the player, or a sign.
};

struct RunAttempt{
    int moves = 0;          //  Moves made.
    bool measured = false;  //  "moves" was measured on the video.
    RunEvent event = RunEvent::NONE;
    std::string video;      //  What the video tracker saw.
};

//  From a standstill, try to make "moves" moves (steps, jumps) in
//  "direction" as one held input. Stops early if a battle or a text box
//  shows up. With "track", the video tells how many moves were made.
RunAttempt attempt_moves(
    StoryRuntime& rt, DpadPosition direction, int moves,
    double frames, int tiles_per_move, bool surf, bool dark, bool track
){
    BlackScreenWatcher battle_starting(COLOR_RED);
    AdvanceBattleDialogWatcher battle_text(COLOR_RED);
    WhiteDialogWatcher dialog(COLOR_RED);
    WalkTracker tracker(direction, frames / tiles_per_move);

    std::vector<PeriodicInferenceCallback> callbacks;
    std::vector<RunEvent> events;
    if (track){
        callbacks.emplace_back(tracker, 10ms);
        events.push_back(RunEvent::SCREEN_CHANGED);
    }
    if (!dark){
        //  Without Flash the screen is mostly black anyway.
        callbacks.emplace_back(battle_starting);
        events.push_back(RunEvent::BATTLE_START);
    }
    callbacks.emplace_back(battle_text);
    events.push_back(RunEvent::BATTLE_TEXT);
    callbacks.emplace_back(dialog);
    events.push_back(RunEvent::DIALOG);

    DpadPosition facing = rt.facing;
    if (track){
        wait_for_still_screen(rt);
    }
    rt.context.wait_for_all_requests();
    int ret = run_until<ProControllerContext>(
        rt.console, rt.context,
        [&](ProControllerContext& context){
            if (moves == 1){
                single_move(context, facing, direction, frames, surf);
            }else{
                //  Released halfway through the last move: the player stops
                //  at the end of it. Not knowing which way the player faces,
                //  half a turn: within the last move either way.
                double turn = direction == facing ? 0 : (facing == DPAD_NONE ? TURN_FRAMES / 2 : TURN_FRAMES);
                double hold = turn + frames * moves - frames / 2;
                pbf_press_dpad(context, direction, frames_to_ms(hold), frames_to_ms(frames / 2 + 6));
            }
        },
        callbacks
    );
    rt.facing = direction;

    RunAttempt result;
    result.event = ret < 0 ? RunEvent::NONE : events[(size_t)ret];
    if (!track){
        result.moves = moves;
        return result;
    }
    if (ret < 0){
        //  The video lags behind the game: keep watching until the player
        //  is seen standing still.
        tracker.stop_when_still(150ms);
        int still = wait_until(rt.console, rt.context, 700ms, { tracker });
        if (still == 0 && tracker.tracker().lost()){
            result.event = RunEvent::SCREEN_CHANGED;
        }
    }
    const AutoStoryScroll::Tracker& t = tracker.tracker();
    result.video = t.summary();
    int tiles = t.tiles();
    if (t.reliable() || result.event != RunEvent::NONE){
        //  Interrupted: the video is the best guess there is, even unsure.
        result.moves = (tiles + tiles_per_move / 2) / tiles_per_move;
        result.measured = t.reliable();
        if (result.moves < 0){
            result.moves = 0;
        }
    }else{
        //  The picture has too little detail (open water, darkness): trust
        //  the timing.
        result.moves = moves;
    }
    return result;
}


}   //  namespace



StoryBattleResult story_battle(
    StoryRuntime& rt, StoryBattleKind kind, const std::string& where,
    bool throw_on_loss
){
    rt.console.log("Battle: " + where);
    StoryBattleResult result = run_story_battle(rt.console, rt.context, kind, rt.battle, rt.memory);
    if (result == StoryBattleResult::NO_BATTLE){
        rt.console.log("No battle started: " + where, COLOR_ORANGE);
        return result;
    }
    if (kind == StoryBattleKind::TRAINER){
        rt.trainer_battles++;
    }else{
        rt.wild_battles++;
    }
    switch (result){
    case StoryBattleResult::LEAD_FAINTED:
        rt.lead_fainted = true;
        break;
    case StoryBattleResult::LOST:
        //  The player is back at the last Pokemon Center, healed.
        rt.lead_fainted = false;
        rt.facing = DPAD_UP;
        if (throw_on_loss){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "The party fainted (" + where + "). Reloading the last save. "
                "Train the team more (\"Extra levels\") if this keeps happening.",
                rt.console
            );
        }
        break;
    default:
        break;
    }
    return result;
}



void story_single_step(StoryRuntime& rt, DpadPosition direction, bool surf){
    single_move(rt.context, rt.facing, direction, surf ? SURF_FRAMES : WALK_FRAMES, surf);
    rt.steps++;
    rt.context.wait_for_all_requests();
}

StoryBattleResult story_checked_step(
    StoryRuntime& rt, DpadPosition direction, bool surf, bool trainer,
    StoryBattleKind wild_kind, const std::string& where,
    bool throw_on_loss
){
    Milliseconds check = trainer ? rt.trainer_check : rt.encounter_check;
    double frames = surf ? SURF_FRAMES : WALK_FRAMES;
    for (int attempt = 0; attempt < 6; attempt++){
        BlackScreenWatcher battle_starting(COLOR_RED);
        AdvanceBattleDialogWatcher battle_started(COLOR_RED);
        WhiteDialogWatcher trainer_talking(COLOR_RED);
        WalkTracker tracker(direction, frames);
        std::vector<PeriodicInferenceCallback> callbacks{battle_starting, battle_started, trainer_talking};
        if (rt.verify_walking){
            callbacks.emplace_back(tracker, 10ms);
        }
        DpadPosition& facing = rt.facing;
        if (rt.verify_walking){
            wait_for_still_screen(rt);
        }
        rt.context.wait_for_all_requests();
        int ret = run_until<ProControllerContext>(
            rt.console, rt.context,
            [&](ProControllerContext& context){
                single_move(context, facing, direction, frames, surf);
                pbf_wait(context, check);
            },
            callbacks
        );
        rt.facing = direction;
        const AutoStoryScroll::Tracker& t = tracker.tracker();
        //  Not moved: only if the screen still looks exactly as before the
        //  step. (If it changed some other way, the video cannot tell.)
        const bool unchanged = t.last_mismatch() <= t.noise() + 2.0;
        if (ret < 0 && rt.verify_walking && t.reliable() && t.tiles() == 0 && unchanged){
            //  The step did not happen (the game ignored the input, or
            //  something is in the way). Take it again, so the caller's
            //  idea of where the player is stays right.
            rt.console.log("The step " + std::string(dpad_name(direction)) + " did not happen: " + where + ". Again.", COLOR_ORANGE);
            pbf_wait(rt.context, attempt < 2 ? 300ms : 1500ms);
            continue;
        }
        rt.steps++;
        //  Two steps for one press (the press lasted too long): one back,
        //  after the battle if one started.
        const int extra = rt.verify_walking && t.reliable() ? t.tiles() - 1 : 0;
        auto step_back = [&](){
            for (int i = 0; i < extra; i++){
                rt.console.log("The step " + std::string(dpad_name(direction)) + " went one tile too far: " + where + ". Stepping back.", COLOR_ORANGE);
                attempt_moves(rt, opposite(direction), 1, frames, 1, surf, false, false);
            }
        };
        if (ret < 0){
            step_back();
            return StoryBattleResult::NO_BATTLE;
        }
        //  ret 3: the tracker lost the map, a battle transition.
        StoryBattleKind kind = (ret == 2 || trainer) ? StoryBattleKind::TRAINER : wild_kind;
        StoryBattleResult result = story_battle(
            rt, kind,
            where + " (going " + dpad_name(direction) + ")",
            throw_on_loss
        );
        if (result != StoryBattleResult::LOST){
            step_back();
        }
        return result;
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "The player does not move " + std::string(dpad_name(direction)) + ": " + where +
        ". Something is in the way, or the player is not where the route expects.",
        rt.console
    );
}

void story_face(StoryRuntime& rt, DpadPosition direction){
    if (direction == DPAD_NONE || rt.facing == direction){
        return;
    }
    //  Release before the 8-frame turn is over: the player turns without
    //  stepping. (If they already faced that way, the press would start a
    //  step - but what they face is in the way, so they stay.)
    pbf_press_dpad(rt.context, direction, frames_to_ms(4), frames_to_ms(14));
    rt.context.wait_for_all_requests();
    rt.facing = direction;
}



namespace{

//  Walk one run of a leg, making sure all its moves happen: missing moves
//  (the game ignored the input, an NPC was in the way, a battle stopped the
//  player) are made again, moves too many are walked back.
//  "scene_at_end": the last move lands on a script trigger, so a text box
//  or a change of screen right at the end is the scene starting (the next
//  step takes care of it), not a battle.
//  "warp_at_end": the leg ends by pushing into a door or onto a mat. If the
//  screen fades right at the end, the warp already started (the input
//  lasted a little long). Returns true then.
bool walk_run(
    StoryRuntime& rt, const StoryLeg& leg, const StoryRun& run, uint16_t count,
    bool track, bool scene_at_end, bool warp_at_end, const std::string& where
){
    const bool surf = run.kind == RunKind::SURF || run.kind == RunKind::SURF_CHECKED;
    const double frames = run.kind == RunKind::JUMP ? JUMP_FRAMES : (surf ? SURF_FRAMES : WALK_FRAMES);
    const int tiles_per_move = run.kind == RunKind::JUMP ? 2 : 1;
    const bool encounters = run.kind == RunKind::CHECKED || run.kind == RunKind::SURF_CHECKED || run.kind == RunKind::CHECKED_TRAINER;
    const StoryBattleKind battle_kind = run.kind == RunKind::CHECKED_TRAINER ? StoryBattleKind::TRAINER : StoryBattleKind::WILD;
    const std::string label = where + " (going " + dpad_name(run.direction) + ")";

    if (!track && encounters){
        //  No video check: one step at a time, watching for a battle after each.
        for (uint16_t i = 0; i < count; i++){
            story_checked_step(
                rt, run.direction, surf, run.kind == RunKind::CHECKED_TRAINER, StoryBattleKind::WILD,
                where + " step " + std::to_string(i + 1) + "/" + std::to_string(count)
            );
        }
        return false;
    }

    int remaining = count;
    int not_moving = 0;
    while (remaining > 0){
        //  After a try that did not move at all, one move at a time: if the
        //  video is wrong about it, little harm is done.
        const int moves = not_moving > 0 ? 1 : remaining;
        RunAttempt a = attempt_moves(rt, run.direction, moves, frames, tiles_per_move, surf, leg.dark, track);
        if (track){
            rt.console.log(
                std::string("Walked ") + dpad_name(run.direction) + ": " +
                std::to_string(a.moves) + " of " + std::to_string(moves) +
                (a.measured ? "" : " (video unsure)") + " - " + a.video
            );
        }
        int made = a.moves;
        if (made > moves && a.measured && !scene_at_end){
            //  One too many (the input lasted too long): step back.
            int back = made - moves;
            rt.console.log("Walked " + std::to_string(back) + " too far. Stepping back.", COLOR_ORANGE);
            for (int i = 0; i < back; i++){
                attempt_moves(rt, opposite(run.direction), 1, frames, tiles_per_move, surf, leg.dark, false);
            }
            made = moves;
        }
        made = std::min(made, moves);
        remaining -= made;
        rt.steps += (uint64_t)made * (uint64_t)tiles_per_move;

        if (scene_at_end && remaining == 0){
            if (a.event != RunEvent::NONE){
                rt.console.log("On the trigger: the scene starts.");
            }
            return false;
        }
        if (warp_at_end && remaining == 0 &&
            (a.event == RunEvent::SCREEN_CHANGED || a.event == RunEvent::BATTLE_START)
        ){
            rt.console.log("The warp started at the end of the run.");
            return true;
        }

        switch (a.event){
        case RunEvent::BATTLE_START:
        case RunEvent::BATTLE_TEXT:
            story_battle(rt, battle_kind, label);
            break;
        case RunEvent::DIALOG:
            //  A trainer saw the player (or a sign or person talks).
            story_battle(rt, StoryBattleKind::TRAINER, label);
            break;
        case RunEvent::SCREEN_CHANGED:{
            //  Maybe a battle is starting. Give it a moment to show.
            BlackScreenWatcher battle_starting(COLOR_RED);
            AdvanceBattleDialogWatcher battle_text(COLOR_RED);
            BattleMenuWatcher battle_menu(COLOR_RED);
            WhiteDialogWatcher dialog(COLOR_RED);
            std::vector<PeriodicInferenceCallback> callbacks{battle_text, battle_menu, dialog};
            if (!leg.dark){
                callbacks.emplace_back(battle_starting);
            }
            int ret = wait_until(rt.console, rt.context, 3000ms, callbacks);
            if (ret == 2){
                story_battle(rt, StoryBattleKind::TRAINER, label);
            }else if (ret >= 0){
                story_battle(rt, battle_kind, label);
            }else{
                rt.console.log("The screen changed but no battle came. Walking on.", COLOR_ORANGE);
            }
            break;
        }
        case RunEvent::NONE:
            break;
        }

        if (made == 0 && a.event == RunEvent::NONE){
            not_moving++;
            if (not_moving >= 10){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "walk_leg(): The player does not move: " + label +
                    ", " + std::to_string(remaining) + " of " + std::to_string(count) + " moves left. "
                    "Something is in the way, or the player is not where the route expects.",
                    rt.console
                );
            }
            rt.console.log("Not moving (in the way, or the game is busy). Trying again.", COLOR_ORANGE);
            pbf_wait(rt.context, not_moving <= 2 ? 500ms : 1500ms);
            rt.context.wait_for_all_requests();
        }else{
            not_moving = 0;
        }
    }
    return false;
}


//  With the video check, runs one after the other in the same direction
//  are walked as one (walking onto grass and on through it), without a stop
//  in between. Each kind keeps its own handling otherwise.
std::vector<StoryRun> merge_runs(const StoryRun* runs, uint16_t count, bool merge){
    std::vector<StoryRun> result;
    auto mergeable = [](RunKind a, RunKind b){
        auto walk = [](RunKind k){ return k == RunKind::WALK || k == RunKind::CHECKED; };
        auto surf = [](RunKind k){ return k == RunKind::SURF || k == RunKind::SURF_CHECKED; };
        return (walk(a) && walk(b)) || (surf(a) && surf(b));
    };
    for (uint16_t r = 0; r < count; r++){
        const StoryRun& run = runs[r];
        if (merge && !result.empty() &&
            result.back().direction == run.direction &&
            mergeable(result.back().kind, run.kind)
        ){
            StoryRun& back = result.back();
            back.count = (uint16_t)(back.count + run.count);
            if (run.kind == RunKind::CHECKED || run.kind == RunKind::SURF_CHECKED){
                back.kind = run.kind;
            }
            continue;
        }
        result.push_back(run);
    }
    return result;
}

}


void walk_leg(StoryRuntime& rt, const StoryScript& script, int32_t leg_index){
    const StoryLeg& leg = script.leg(leg_index);
    rt.console.log(std::string("Walking: ") + leg.name);
    const std::vector<StoryRun> runs = merge_runs(script.leg_runs(leg), leg.run_count, rt.verify_walking);

    //  Which way the player faces is what our own last move left. The
    //  route only guesses it (a scene may have turned the player), so it is
    //  used only on the water, where a wrong guess costs a step either way.
    //  Walking, an unknown facing is allowed for (half a turn).
    if (rt.facing == DPAD_NONE && leg.start_facing != DPAD_NONE && !runs.empty() &&
        (runs[0].kind == RunKind::SURF || runs[0].kind == RunKind::SURF_CHECKED)
    ){
        rt.facing = leg.start_facing;
    }

    bool warped = false;
    bool warped_early = false;
    for (size_t r = 0; r < runs.size(); r++){
        const StoryRun& run = runs[r];
        const bool last = r + 1 == runs.size();
        const RunKind next = last ? RunKind::WALK : runs[r + 1].kind;
        switch (run.kind){
        case RunKind::WALK:
        case RunKind::JUMP:
        case RunKind::SURF:
        case RunKind::CHECKED:
        case RunKind::CHECKED_TRAINER:
        case RunKind::SURF_CHECKED:{
            //  A spinner after this run carries the player on: the video
            //  would see that too.
            const bool track = rt.verify_walking && next != RunKind::SPIN;
            const bool onto_warp = last && leg.end == LegEnd::WARP_STEP && run.kind != RunKind::JUMP;
            uint16_t count = onto_warp ? (uint16_t)(run.count - 1) : run.count;
            const bool scene_at_end = last && leg.end == LegEnd::TRIGGER;
            const bool warp_at_end = last && leg.end == LegEnd::WARP_PUSH;
            if (count > 0){
                warped_early = walk_run(rt, leg, run, count, track, scene_at_end, warp_at_end, leg.name);
            }
            if (onto_warp){
                const bool surf = run.kind == RunKind::SURF || run.kind == RunKind::SURF_CHECKED;
                step_onto_warp(
                    rt, run.direction, surf ? SURF_FRAMES : WALK_FRAMES, surf,
                    leg.dark || leg.dest_dark, leg.preview, leg.name
                );
                warped = true;
            }
            break;
        }
        case RunKind::SPIN:
            //  The step onto the spinner was the end of the last run. The
            //  player is carried without input, then stops.
            pbf_wait(rt.context, frames_to_ms(SPIN_FRAMES * run.count + 20) + 300ms);
            rt.context.wait_for_all_requests();
            rt.facing = run.direction;
            break;
        case RunKind::DISMOUNT:
            //  Jump from the water onto the shore (Task_StopSurfingInit).
            //  12 frames works whichever way the player faces, and is over
            //  before the player can move again.
            pbf_press_dpad(rt.context, run.direction, frames_to_ms(12), 1200ms);
            rt.context.wait_for_all_requests();
            rt.facing = run.direction;
            rt.steps++;
            break;
        }
    }

    switch (leg.end){
    case LegEnd::STOP:
        break;
    case LegEnd::WARP_PUSH:
        if (warped_early){
            wait_warp_step(rt, leg.dark || leg.dest_dark, leg.preview, leg.name);
        }else{
            take_warp_push(rt, leg.push, leg.dark || leg.dest_dark, leg.preview, leg.name);
        }
        rt.facing = DPAD_NONE;
        break;
    case LegEnd::WARP_STEP:
        if (!warped){
            wait_warp_step(rt, leg.dark || leg.dest_dark, leg.preview, leg.name);
        }
        rt.facing = DPAD_NONE;
        break;
    case LegEnd::TRIGGER:
        pbf_wait(rt.context, 300ms);
        rt.context.wait_for_all_requests();
        break;
    case LegEnd::INTERACT:
        story_face(rt, leg.face);
        break;
    }
}



bool story_cutscene(
    StoryRuntime& rt,
    Milliseconds min_time, Milliseconds max_time,
    bool answer_yes, const std::string& what
){
    rt.console.log("Cutscene: " + what);
    //  A scene can turn or move the player.
    rt.facing = DPAD_NONE;
    WallClock start = current_time();
    WallClock deadline = start + max_time;
    while (current_time() < deadline){
        SelectionDialogWatcher yes_no(COLOR_RED);
        WhiteDialogWatcher dialog(COLOR_RED);
        BattleMenuWatcher battle_menu(COLOR_RED);
        AdvanceBattleDialogWatcher battle_text(COLOR_RED);
        //  Also the battle text box without the advance arrow (a trainer's
        //  last words in battle): the battle logic handles it.
        BattleDialogWatcher battle_box(COLOR_RED);
        rt.context.wait_for_all_requests();
        int ret = wait_until(
            rt.console, rt.context, 1000ms,
            { yes_no, dialog, battle_menu, battle_text, battle_box }
        );
        switch (ret){
        case 0:{
            bool yes = answer_yes;
            if (yes){
                std::string question = story_read_dialog(rt);
                rt.console.log("Question: " + question);
                if (question.find("nickname") != std::string::npos){
                    yes = false;
                }
            }
            pbf_wait(rt.context, 300ms);
            pbf_press_button(rt.context, yes ? BUTTON_A : BUTTON_B, 160ms, 840ms);
            break;
        }
        case 1:
            //  B advances the text (and never says YES to anything).
            pbf_press_button(rt.context, BUTTON_B, 160ms, 340ms);
            break;
        case 2:
        case 3:
        case 4:
            rt.console.log("A battle is on screen: " + what);
            return true;
        default:
            if (current_time() - start >= min_time){
                //  The start menu only opens when the player is free to move.
                if (in_overworld(rt.console, rt.context)){
                    rt.console.log("Cutscene over: " + what);
                    return false;
                }
            }else if (!answer_yes){
                //  Texts without the usual box ("received X!" fanfares).
                pbf_press_button(rt.context, BUTTON_B, 160ms, 340ms);
            }
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "story_cutscene(): Still not back in the overworld after: " + what,
        rt.console
    );
}


void story_talk_start(StoryRuntime& rt, const std::string& what){
    rt.console.log("Talking: " + what);
    WhiteDialogWatcher dialog(COLOR_RED);
    AdvanceBattleDialogWatcher battle_text(COLOR_RED);
    rt.context.wait_for_all_requests();
    int ret = run_until<ProControllerContext>(
        rt.console, rt.context,
        [](ProControllerContext& context){
            for (int i = 0; i < 5; i++){
                pbf_press_button(context, BUTTON_A, 160ms, 1840ms);
            }
        },
        { dialog, battle_text }
    );
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_talk_start(): No text box after pressing A at: " + what +
            ". The player is probably not where the route expects.",
            rt.console
        );
    }
}


void story_talk_answer(StoryRuntime& rt, bool yes, const std::string& what){
    rt.console.log(std::string(yes ? "Talking and answering YES: " : "Talking and answering NO: ") + what);
    SelectionDialogWatcher yes_no(COLOR_RED);
    rt.context.wait_for_all_requests();

    //  One A starts the conversation, more A advance the text. They are
    //  spaced out so the YES/NO box is seen before an A answers it.
    int ret = run_until<ProControllerContext>(
        rt.console, rt.context,
        [](ProControllerContext& context){
            for (int i = 0; i < 6; i++){
                pbf_press_button(context, BUTTON_A, 160ms, 2840ms);
            }
        },
        { yes_no }
    );
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_talk_answer(): No YES/NO box at: " + what,
            rt.console
        );
    }
    //  The cursor starts on YES. B answers NO.
    pbf_wait(rt.context, 300ms);
    pbf_press_button(rt.context, yes ? BUTTON_A : BUTTON_B, 160ms, 840ms);
    rt.context.wait_for_all_requests();
}


void story_heal(StoryRuntime& rt, const std::string& where){
    rt.console.log("Healing: " + where);
    heal_at_pokecenter(rt.console, rt.context);
    story_cutscene(rt, 0ms, 30s, false, "Nurse at " + where);
    rt.lead_fainted = false;
    rt.facing = DPAD_UP;
}


void story_save(StoryRuntime& rt, bool has_pokedex){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log(has_pokedex ? "Saving the game." : "Saving the game (no Pokedex yet).");
    if (has_pokedex){
        save_game_to_overworld(console, context);
    }else{
        //  Same as save_game_to_overworld(), with the start menu that has no
        //  POKeDEX entry.
        WallClock deadline = current_time() + 120s;
        while (true){
            context.wait_for_all_requests();
            if (current_time() > deadline){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "story_save(): Unable to save the game after 2 minutes.",
                    console
                );
            }
            StartMenuWatcher start_menu(COLOR_RED);
            if (wait_until(console, context, 1000ms, { start_menu }) != 0){
                pbf_press_button(context, BUTTON_PLUS, 200ms, 800ms);
                continue;
            }
            if (!move_cursor_to_position(console, context, SelectionArrowPositionNoDexMenu::SAVE)){
                pbf_mash_button(context, BUTTON_B, 500ms);
                continue;
            }
            pbf_press_button(context, BUTTON_A, 320ms, 400ms);
            context.wait_for_all_requests();

            SelectionArrowWatcher save_arrow(COLOR_RED, &console.overlay(), SelectionArrowPositionConfirmationMenu::YES);
            if (wait_until(console, context, 2000ms, { save_arrow }) != 0){
                pbf_mash_button(context, BUTTON_B, 500ms);
                continue;
            }
            pbf_press_button(context, BUTTON_A, 320ms, 200ms);
            context.wait_for_all_requests();

            //  "There is already a saved file. Is it okay to overwrite it?"
            SelectionArrowWatcher overwrite_arrow(COLOR_RED, &console.overlay(), SelectionArrowPositionConfirmationMenu::YES);
            if (wait_until(console, context, 2000ms, { overwrite_arrow }) == 0){
                pbf_press_button(context, BUTTON_A, 320ms, 200ms);
            }
            break;
        }
        //  Let "SAVING... DON'T TURN OFF THE POWER." finish, then close the
        //  "saved the game" text.
        pbf_wait(context, 4000ms);
        pbf_mash_button(context, BUTTON_B, 1500ms);
        context.wait_for_all_requests();
    }
    story_back_to_overworld(rt, "after saving");
}


bool story_scripted_battle(StoryRuntime& rt, const std::string& opponent){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log("Battle: " + opponent);

    //  Through the pre-battle text and the battle intro.
    BattleMenuWatcher battle_menu(COLOR_RED);
    context.wait_for_all_requests();
    int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, 90000ms);
        },
        { battle_menu }
    );
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_scripted_battle(): The battle menu never appeared against " + opponent,
            console
        );
    }

    //  First turn: FIGHT, first move. In the first rival battle Prof. Oak
    //  explains the battle and dims the screen, which confuses most
    //  detectors, so get through that with B until the menu is back.
    pbf_mash_button(context, BUTTON_A, 1000ms);
    context.wait_for_all_requests();
    ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, 30000ms);
        },
        { battle_menu }
    );
    if (ret < 0){
        console.log("Battle menu not seen after the first turn. Continuing with A.", COLOR_RED);
    }

    //  A on the battle menu picks FIGHT, A on the move list picks the move
    //  under the cursor (the first move), A also advances battle text.
    BattleOpponentFaintWatcher opponent_fainted(COLOR_RED);
    BattleFaintWatcher player_fainted(COLOR_RED);
    context.wait_for_all_requests();
    ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_A, 300000ms);
        },
        { opponent_fainted, player_fainted }
    );
    rt.trainer_battles++;
    switch (ret){
    case 0:
        console.log("Won against " + opponent + ".");
        return true;
    case 1:
        console.log("Lost against " + opponent + ". (The story goes on.)", COLOR_ORANGE);
        return false;
    default:
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_scripted_battle(): Nobody fainted within 5 minutes against " + opponent,
            console
        );
    }
}


std::string story_read_dialog(StoryRuntime& rt){
    return read_story_text(rt.console, DIALOG_TEXT_BOX, false);
}


void story_back_to_overworld(StoryRuntime& rt, const std::string& where){
    for (int attempt = 0; attempt < 4; attempt++){
        if (in_overworld(rt.console, rt.context)){
            return;
        }
        pbf_mash_button(rt.context, BUTTON_B, 3000ms);
        rt.context.wait_for_all_requests();
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "Not back in the overworld: " + where,
        rt.console
    );
}



}
}
}

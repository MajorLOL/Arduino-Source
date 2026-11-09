/*  Pokemon FRLG Autostory Tools
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"
#include "Common/Cpp/MemoryUtilization/MemoryUtilization.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
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
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_AutoStoryWalkTracker.h"
#include "PokemonFRLG_AutoStoryMemory.h"
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
constexpr double RUN_FRAMES = 8;        //  With the Running Shoes (B held).
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
//  Walking (16-frame steps, 32-frame jumps):
//    - Facing that way: the move starts at once. The press is 10 frames,
//      over well before the move ends (frame 16), so the player stops
//      after one move.
//    - Facing another way: the turn ends at frame 8 and the input must
//      still be held then for a move to start. The press is 14 frames;
//      a second move could only start at frame 24.
//    - Not known: 12 frames, which works both ways.
//  Surfing (8-frame steps): already facing that way, 4 frames; otherwise
//  12 (turn, then one step).
void single_move(
    ProControllerContext& context, DpadPosition& facing,
    DpadPosition direction, double move_frames, bool surf
){
    double press;
    if (surf){
        press = facing == direction ? 4 : 12;
    }else if (facing == DPAD_NONE){
        press = 12;
    }else{
        press = facing == direction ? 10 : 14;
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

//  A fade was taken for a warp. A wild battle starts the same way (black,
//  then a new picture), and the step onto the warp tile never brings one
//  (the warp comes first): a battle means the player did not reach the
//  warp. Then it is fought, and the caller tries the warp again.
//  (Victory Road 1F: a battle on the step to the ladder passed for the
//  warp, and the route went on as if on 2F.)
bool battle_instead_of_warp(StoryRuntime& rt, bool dark, const char* leg_name){
    if (dark){
        return false;
    }
    AdvanceBattleDialogWatcher battle_text(COLOR_RED);
    BattleMenuWatcher battle_menu(COLOR_RED);
    rt.context.wait_for_all_requests();
    if (wait_until(rt.console, rt.context, 3000ms, { battle_text, battle_menu }) < 0){
        return false;
    }
    rt.console.log(std::string("A wild battle, not the warp: ") + leg_name + ". The warp again after it.", COLOR_ORANGE);
    story_battle(rt, StoryBattleKind::WILD, std::string(leg_name) + " (before the warp)");
    rt.facing = DPAD_NONE;
    return true;
}

//  Hold "direction" from a standstill until the warp starts, then wait
//  until it has finished. Doors, mats and stairs all warp while the
//  direction is held:
//    door:   facing the door, holding toward it      (TryDoorWarp)
//    mat:    standing on it, holding the arrow       (TryArrowWarp)
//    stairs: standing on them, holding sideways      (TryArrowWarp)
//  Released when the screen turns black: a direction still held when the
//  new map comes up can take the player straight back. Stairs are quick,
//  and the player arrives standing on the other stairs (Task_ExitStairs):
//  holding sideways there goes back down.
void take_warp_push(
    StoryRuntime& rt, DpadPosition direction, bool dark, MapPreview preview, const char* leg_name
){
    int ret = -1;
    for (int attempt = 0; attempt < 3; attempt++){
        BlackScreenOverWatcher warp_finished(COLOR_RED);
        BlackScreenWatcher warp_starting(COLOR_RED);
        std::vector<PeriodicInferenceCallback> callbacks;
        callbacks.emplace_back(warp_finished);
        if (!dark){
            //  (Without Flash the screen is black anyway.)
            callbacks.emplace_back(warp_starting);
        }
        rt.context.wait_for_all_requests();
        ret = run_until<ProControllerContext>(
            rt.console, rt.context,
            [&](ProControllerContext& context){
                pbf_press_dpad(context, direction, dark ? 700ms : 1500ms, 0ms);
            },
            callbacks
        );
        if (ret != 0){
            ret = wait_until(rt.console, rt.context, 10000ms, { warp_finished });
        }
        if (ret < 0 || !battle_instead_of_warp(rt, dark, leg_name)){
            break;
        }
        ret = -1;
    }
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
    if (ret < 0 && !dark){
        //  The fade can be over before this looks for it: the watchers of
        //  the run saw the screen change, and by the time they let go, the
        //  black was gone (a short fade, a gate). Then the new map is up.
        VideoSnapshot frame = rt.console.video().snapshot();
        BlackScreenDetector black(COLOR_RED);
        if (frame && !black.detect(frame)){
            rt.console.log(std::string("The fade was over before it was watched: ") + leg_name, COLOR_ORANGE);
            ret = 0;
        }
    }
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
            if (battle_instead_of_warp(rt, dark, leg_name)){
                continue;
            }
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
    wait_until(rt.console, rt.context, 1000ms, { stable });
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
    int video_moves = 0;    //  Moves the video saw (up to a change of screen).
    int pixels = 0;         //  How far the video saw the map scroll.
    size_t frames = 0;      //  Video frames looked at, and how many of them
    size_t unsure = 0;      //  did not tell the offset well.
    bool measured = false;  //  "moves" was measured on the video.
    bool lost = false;      //  The video lost the map on the way.
    bool lost_early = false;    //  ... so early that no step can have been made then.
    bool on_turn = false;       //  ... on the turn in place before the step.
    RunEvent event = RunEvent::NONE;
    std::string video;      //  What the video tracker saw.
    //  The screen before the run (with the video check), to measure later
    //  how far the run got when something stopped it.
    bool has_start = false;
    AutoStoryScroll::Screen start;
};

//  From a standstill, try to make "moves" moves (steps, jumps) in
//  "direction" as one held input. Stops early if a battle or a text box
//  shows up. With "track", the video tells how many moves were made.
//  "trainer_watch": in the dark, how long to watch after a step where a
//  trainer may see the player.
RunAttempt attempt_moves(
    StoryRuntime& rt, DpadPosition direction, int moves,
    double frames, int tiles_per_move, bool surf, bool dark, bool track,
    bool battles = false, Milliseconds trainer_watch = Milliseconds(0),
    bool run = false
){
    //  Running: B held with the d-pad, 8-frame steps. Single moves walk
    //  (their presses are timed for walking).
    const bool running = run && moves > 1 && !surf && tiles_per_move == 1;
    if (running){
        frames = RUN_FRAMES;
    }
    BlackScreenWatcher battle_starting(COLOR_RED);
    WalkTracker tracker(direction, frames / tiles_per_move);
    //  Text boxes count when they appear with the player standing, not when
    //  the place looks like one (the white bottom of the S.S. Anne
    //  corridors), all the time or as it comes into view.
    const WalkTracker* motion = track ? &tracker : nullptr;
    AdvanceBattleDialogDetector battle_text_detector(COLOR_RED);
    TurnsOnWatcher battle_text("BattleTextAppears", battle_text_detector, 250ms, motion);
    WhiteDialogDetector dialog_detector(COLOR_RED);
    TurnsOnWatcher dialog("DialogAppears", dialog_detector, 250ms, motion);

    std::vector<PeriodicInferenceCallback> callbacks;
    std::vector<RunEvent> events;
    if (track){
        //  At the usual 50 ms: every video frame that is looked at has to be
        //  converted first, and doing that for every frame keeps the CPU
        //  busy enough for the video recording to fall behind and use up
        //  the memory. 3 pixels walking between two looks is plenty.
        //  (It never stops the run itself.)
        callbacks.emplace_back(tracker);
        events.push_back(RunEvent::NONE);
    }
    if (!dark){
        //  Without Flash the screen is mostly black anyway.
        callbacks.emplace_back(battle_starting);
        events.push_back(RunEvent::BATTLE_START);
    }else{
        //  In the dark, the battle text is what shows a battle. Elsewhere
        //  the screen goes black first (and the battle text detector also
        //  takes some overworld places for a text box, like the white
        //  bottom of the S.S. Anne corridors).
        callbacks.emplace_back(battle_text);
        events.push_back(RunEvent::BATTLE_TEXT);
    }
    callbacks.emplace_back(dialog);
    events.push_back(RunEvent::DIALOG);

    DpadPosition facing = rt.facing;
    RunAttempt result;
    if (track){
        wait_for_still_screen(rt);
        VideoSnapshot frame = rt.console.video().snapshot();
        if (frame){
            ImageViewRGB32 game = extract_box_reference(*frame.frame, GameSettings::instance().GAME_BOX);
            AutoStoryScroll::sample_screen(result.start, game.data(), game.bytes_per_row(), game.width(), game.height());
            result.has_start = true;
        }
    }
    rt.context.wait_for_all_requests();
    if (battles && dark && track && moves == 1 && !surf &&
        facing != DPAD_NONE && facing != direction
    ){
        //  Gen 3 also looks for a wild battle when a turn in place is over
        //  (FieldGetPlayerInput(): the player is at a tile center then), not
        //  only at the end of a step. In the dark both look the same on the
        //  video. So turn first, on its own (released before the 8-frame
        //  turn is over), and watch: a battle now means no step.
        WalkTracker turn_tracker(direction, frames / tiles_per_move);
        turn_tracker.stop_when_lost();
        int turned = run_until<ProControllerContext>(
            rt.console, rt.context,
            [&](ProControllerContext& context){
                pbf_press_dpad(context, direction, frames_to_ms(4), frames_to_ms(TURN_FRAMES + 6 - 4));
                pbf_wait(context, 1000ms);
            },
            { turn_tracker }
        );
        rt.facing = direction;
        facing = direction;
        if (turned >= 0 || turn_tracker.tracker().lost()){
            result.video = turn_tracker.tracker().summary();
            result.lost = true;
            result.lost_early = true;
            result.on_turn = true;
            return result;
        }
    }
    //  (single_move() updates "facing" as it presses.)
    const DpadPosition facing_before = facing;
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
                if (running){
                    pbf_controller_state(context, BUTTON_B, direction, {0, 0}, {0, 0}, frames_to_ms(hold));
                    pbf_wait(context, frames_to_ms(frames / 2 + 6));
                }else{
                    pbf_press_dpad(context, direction, frames_to_ms(hold), frames_to_ms(frames / 2 + 6));
                }
            }
        },
        callbacks
    );
    rt.facing = direction;
    //  The player turned first (or may have: facing not known).
    const bool turned = facing_before != direction;

    result.event = ret < 0 ? RunEvent::NONE : events[(size_t)ret];
    if (ret < 0 && battles && track && dark && !tracker.tracker().lost()){
        //  In the dark only a battle starting shows on the video (the whole
        //  screen changes). It must be seen now, at the end of the step
        //  that started it, not in the next try: watch a little longer.
        tracker.stop_when_lost();
        if (trainer_watch > 800ms){
            //  A trainer that saw the player at the end of the step comes
            //  later: the "!", maybe a walk, then its words (a text box
            //  without the advance arrow). Until they show, the player
            //  cannot move, and in the dark a step taken then would look
            //  made.
            if (wait_until(rt.console, rt.context, trainer_watch, { tracker, dialog }) == 1){
                result.event = RunEvent::DIALOG;
            }
        }else{
            wait_until(rt.console, rt.context, 800ms, { tracker });
        }
    }
    //  The picture changed before the video saw any step.
    const bool no_step = tracker.tracker().lost() &&
        std::abs(tracker.tracker().pixels()) < AutoStoryScroll::TILE_PIXELS / 2;
    if (ret < 0 && track && !dark && tracker.tracker().lost() && (battles || (turned && no_step))){
        //  The whole picture changed on the way, where wild battles start:
        //  a battle. It started at the end of a step, and the screen only
        //  goes black after the input is over. The video counted the steps
        //  up to the change.
        //  Or a trainer saw the player: its words come up (a text box
        //  without the advance arrow), and the rest of the run did not
        //  happen.
        //  Or, on a run without grass: the player turned on the grass tile
        //  the run starts from, and Gen 3 also looks for a wild battle when
        //  a turn in place is over (FieldGetPlayerInput()). Then no step
        //  was made.
        BlackScreenWatcher black(COLOR_RED);
        AdvanceBattleDialogWatcher text(COLOR_RED);
        WhiteDialogWatcher words(COLOR_RED);
        if (battles){
            int late = wait_until(rt.console, rt.context, 2500ms, { black, text, words });
            if (late >= 0){
                result.event = late == 0 ? RunEvent::BATTLE_START : (late == 1 ? RunEvent::BATTLE_TEXT : RunEvent::DIALOG);
            }
        }else if (wait_until(rt.console, rt.context, 5000ms, { text }) == 0){
            //  Only the battle text tells: a warp's fade looks like a battle
            //  starting.
            rt.console.log("A wild battle after the turn, before the first step.", COLOR_ORANGE);
            result.event = RunEvent::BATTLE_TEXT;
        }
    }
    if (!track){
        result.moves = moves;
        return result;
    }
    if (ret < 0 && !tracker.tracker().lost()){
        //  The video lags behind the game: keep watching until the player
        //  is seen standing still.
        tracker.stop_when_still(150ms);
        wait_until(rt.console, rt.context, 700ms, { tracker });
    }
    const AutoStoryScroll::Tracker& t = tracker.tracker();
    result.video = t.summary();
    result.pixels = t.pixels();
    result.frames = t.frames();
    result.unsure = t.unsure_frames();
    result.lost = t.lost();
    //  A step takes 16 frames (267 ms) from the press, running 8.
    result.lost_early = t.lost() && tracker.lost_at_ms() >= 0 &&
        tracker.lost_at_ms() < (running ? 100 : 200);
    int tiles = t.tiles();
    result.video_moves = std::max(0, (tiles + tiles_per_move / 2) / tiles_per_move);
    if (result.event != RunEvent::NONE){
        //  A battle or a trainer stopped the player. The video saw up to
        //  that moment: the best guess there is, even unsure.
        result.moves = std::max(0, (tiles + tiles_per_move / 2) / tiles_per_move);
        result.measured = t.reliable();
    }else if (!t.lost() && t.reliable()){
        result.moves = std::max(0, (tiles + tiles_per_move / 2) / tiles_per_move);
        result.measured = true;
    }else{
        //  The picture changed on the way without a battle (a fade, a text
        //  box), or has too little detail (open water, darkness): the run
        //  went on as timed.
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
    rt.whited_out = false;
    rt.battle.shiny_watch = rt.shiny_watch.get();
    //  A shiny takes the next free party slot. Until the whole team is in,
    //  keep the slots the team still needs.
    rt.battle.shiny_catch = rt.party_order_final || (int)rt.team.size() + rt.party_extras < 6;
    StoryBattleResult result = run_story_battle(rt.console, rt.context, kind, rt.battle, rt.memory);
    if (rt.shiny_watch){
        StoryShinyWatch& watch = *rt.shiny_watch;
        watch.consume();
        if (!rt.warned_no_sound && kind != StoryBattleKind::TRAINER &&
            current_time() - watch.started() > 60s && !watch.has_sound()
        ){
            rt.warned_no_sound = true;
            rt.console.log(
                "No sound comes in from the capture card: shiny Pokemon cannot be noticed. "
                "Set the audio input (and turn the game's sound on) to have them caught.",
                COLOR_RED
            );
        }
        if (current_time() - watch.detector_started() > 20min){
            watch.restart();
        }
    }
    if (rt.memory.shiny){
        rt.shinies++;
        const std::string name = rt.memory.shiny_species.empty() ? std::string("Pokemon") : rt.memory.shiny_species;
        if (rt.memory.shiny_caught){
            rt.shinies_caught++;
            rt.shiny_unsaved = true;
            if (!rt.memory.caught.empty()){
                rt.console.log("The shiny " + name + " is the team member to catch here.", COLOR_BLUE);
            }else if (rt.memory.shiny_to_pc){
                rt.console.log("The shiny " + name + " went to the PC (the party is full).", COLOR_BLUE);
            }else{
                rt.party_extras++;
                rt.console.log(
                    "The shiny " + name + " is in the party, behind the team (" +
                    std::to_string(rt.party_extras) + " besides the team).",
                    COLOR_BLUE
                );
            }
        }else{
            rt.console.log("The shiny " + name + " was not caught. Going on.", COLOR_ORANGE);
        }
    }
    //  New to the POKeDEX: the foe's box had no ball mark (the game's
    //  word), or, the mark not read, the POKeDEX page came up.
    const bool new_to_dex = rt.memory.foe_mark == 0 || (rt.memory.foe_mark < 0 && rt.memory.dex_registered);
    if (!rt.memory.owned_by_mark.empty()){
        //  The game had it caught already: wanted no more.
        rt.caught.insert(rt.memory.owned_by_mark);
        if (rt.battle.dex_species.erase(rt.memory.owned_by_mark) != 0){
            rt.dex_owned_by_mark++;
        }
    }
    if (result == StoryBattleResult::CAUGHT && !rt.memory.caught.empty() &&
        rt.memory.caught.rfind("nidoran", 0) == 0 &&
        (rt.battle.dex_species.count("nidoran-f") != 0 || rt.battle.dex_species.count("nidoran-m") != 0)
    ){
        //  The two NIDORANs read alike: only the ball mark (or the POKeDEX
        //  page) tells that the one caught was a wanted one (not the other,
        //  caught before).
        std::string other = rt.memory.caught == "nidoran-f" ? "nidoran-m" : "nidoran-f";
        if (!new_to_dex){
            rt.console.log("Caught a NIDORAN that was in the POKeDEX already: the other one is still wanted.", COLOR_ORANGE);
            rt.memory.caught.clear();
            if (!rt.memory.shiny_to_pc && !rt.memory.shiny){
                rt.party_extras++;
            }
        }else if (rt.battle.dex_species.count(rt.memory.caught) == 0){
            rt.memory.caught = other;
        }
    }
    if (result == StoryBattleResult::CAUGHT && !rt.memory.caught.empty()){
        if (new_to_dex){
            rt.dex_new_since_load.insert(rt.memory.caught);
        }
        rt.caught.insert(rt.memory.caught);
        const bool for_dex = rt.battle.dex_species.erase(rt.memory.caught) != 0 ||
            rt.memory.caught == rt.memory.dex_by_mark;
        if (for_dex){
            if (new_to_dex){
                rt.dex_new++;
            }
            rt.console.log(
                "Caught for the POKeDEX: " + rt.memory.caught +
                (new_to_dex ? " (new)." : " (it was in the POKeDEX already)."),
                COLOR_BLUE
            );
            if (!rt.memory.shiny_to_pc && !rt.memory.shiny){
                rt.party_extras++;
            }
        }else{
            rt.console.log("Caught for the team: " + rt.memory.caught, COLOR_BLUE);
            //  Wanted no more on the way.
            rt.battle.catch_species.clear();
        }
    }
    rt.safari_balls_used += rt.memory.safari_balls;
    if (rt.memory.safari_over || (rt.in_safari && rt.safari_balls_used >= 30)){
        //  The last SAFARI BALL: the game is over, whether it caught or
        //  not (SafariZone_EventScript_OutOfBalls after the battle).
        rt.safari_over = true;
    }
    if (rt.memory.no_balls){
        rt.no_balls = true;
    }
    if (result == StoryBattleResult::NO_BATTLE){
        rt.console.log("No battle started: " + where, COLOR_ORANGE);
        rt.false_battles++;
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
        rt.memory.no_pp = 0;
        rt.facing = DPAD_UP;
        rt.whited_out = true;
        if (rt.battle.catch_legendary){
            //  The team fainted against the legendary Pokemon: one more try
            //  at it (the save is in front of it), not an error of the
            //  route (Mewtwo: 13 of 29 tries ended so, and the program
            //  stopped after too many "errors").
            rt.legendary_missed = true;
        }
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

StoryBattleResult story_grass_run(
    StoryRuntime& rt, DpadPosition direction, int moves,
    StoryBattleKind wild_kind, const std::string& where, bool throw_on_loss,
    int& made, bool& sure, bool run
){
    StoryMemoryScope memory_scope("grass runs");
    const std::string label = where + " (going " + dpad_name(direction) + ")";
    //  Walking, not running: the video counts the tiles, and a battle on
    //  the way stops the player at the end of the step that started it.
    RunAttempt a = attempt_moves(
        rt, direction, moves, WALK_FRAMES, 1, false, false, rt.verify_walking, true,
        Milliseconds(0), run && rt.running && rt.verify_walking
    );
    rt.facing = direction;
    made = a.moves;
    sure = rt.verify_walking && a.measured && !a.lost && a.event == RunEvent::NONE;
    rt.steps += (uint64_t)std::max(0, made);
    if (rt.verify_walking){
        rt.console.log(
            "Grass " + std::string(dpad_name(direction)) + ": " + std::to_string(a.moves) + " of " +
            std::to_string(moves) + (a.measured ? "" : " (video unsure)") + " - " + a.video
        );
    }

    RunEvent event = a.event;
    if (event == RunEvent::NONE && !a.lost){
        //  A battle that starts at the end of the last step shows a moment
        //  later, as after a single step.
        BlackScreenWatcher battle_starting(COLOR_RED);
        AdvanceBattleDialogWatcher battle_started(COLOR_RED);
        WhiteDialogWatcher trainer_talking(COLOR_RED);
        rt.context.wait_for_all_requests();
        int ret = wait_until(
            rt.console, rt.context, rt.encounter_check,
            { battle_starting, battle_started, trainer_talking }
        );
        if (ret == 0){
            event = RunEvent::BATTLE_START;
        }else if (ret == 1){
            event = RunEvent::BATTLE_TEXT;
        }else if (ret == 2){
            event = RunEvent::DIALOG;
        }
    }else if (event == RunEvent::NONE && a.lost){
        event = RunEvent::SCREEN_CHANGED;
    }

    StoryBattleResult result = StoryBattleResult::NO_BATTLE;
    switch (event){
    case RunEvent::BATTLE_START:
    case RunEvent::BATTLE_TEXT:
        result = story_battle(rt, wild_kind, label, throw_on_loss);
        break;
    case RunEvent::DIALOG:
        if (story_close_repel_text(rt)){
            break;
        }
        if (rt.in_safari){
            //  No trainers there: the PA, the Safari Game is over.
            rt.console.log("Text in the Safari Zone: the Safari Game is over.", COLOR_BLUE);
            rt.safari_over = true;
            break;
        }
        result = story_battle(rt, StoryBattleKind::TRAINER, label, throw_on_loss);
        break;
    case RunEvent::SCREEN_CHANGED:{
        //  Maybe a battle is starting (some transitions do not go all black).
        BlackScreenWatcher battle_starting(COLOR_RED);
        AdvanceBattleDialogWatcher battle_text(COLOR_RED);
        BattleMenuWatcher battle_menu(COLOR_RED);
        int ret = wait_until(rt.console, rt.context, 3000ms, { battle_text, battle_menu, battle_starting });
        if (ret >= 0){
            result = story_battle(rt, wild_kind, label, throw_on_loss);
        }else{
            rt.console.log("The screen changed in the grass but no battle came.", COLOR_ORANGE);
        }
        break;
    }
    case RunEvent::NONE:
        break;
    }
    if (result != StoryBattleResult::NO_BATTLE){
        sure = false;
    }
    return result;
}

StoryBattleResult story_checked_step(
    StoryRuntime& rt, DpadPosition direction, bool surf, bool trainer,
    StoryBattleKind wild_kind, const std::string& where,
    bool throw_on_loss, bool* moved
){
    StoryMemoryScope memory_scope("single steps");
    Milliseconds check = trainer ? rt.trainer_check : rt.encounter_check;
    double frames = surf ? SURF_FRAMES : WALK_FRAMES;
    if (moved != nullptr){
        *moved = true;
    }
    const int attempts = moved == nullptr ? 6 : 3;
    for (int attempt = 0; attempt < attempts; attempt++){
        BlackScreenWatcher battle_starting(COLOR_RED);
        AdvanceBattleDialogWatcher battle_started(COLOR_RED);
        WhiteDialogWatcher trainer_talking(COLOR_RED);
        WalkTracker tracker(direction, frames);
        std::vector<PeriodicInferenceCallback> callbacks{battle_starting, battle_started, trainer_talking};
        if (rt.verify_walking){
            callbacks.emplace_back(tracker);
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
        if (rt.verify_walking && ret < 0 && (t.lost() || !t.reliable() || t.tiles() != 1)){
            rt.console.log(
                "Step " + std::string(dpad_name(direction)) + " (" + where + "): " + t.summary() +
                (t.reliable() ? "" : ", unsure"),
                COLOR_ORANGE
            );
        }
        //  Not moved: the map did not scroll. (Animated water or flowers,
        //  or someone walking around, do not make it look scrolled.)
        if (ret < 0 && rt.verify_walking && !t.lost() && t.reliable() && t.tiles() == 0){
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
        const int extra = rt.verify_walking && t.reliable() && !t.lost() ? t.tiles() - 1 : 0;
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
        if (ret == 2 && rt.in_safari){
            rt.console.log("Text in the Safari Zone: the Safari Game is over.", COLOR_BLUE);
            rt.safari_over = true;
            return StoryBattleResult::NO_BATTLE;
        }
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
    if (moved != nullptr){
        *moved = false;
        return StoryBattleResult::NO_BATTLE;
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



bool story_take_picture(StoryRuntime& rt, AutoStoryScroll::Screen& picture){
    if (!rt.verify_walking){
        return false;
    }
    wait_for_still_screen(rt);
    VideoSnapshot frame = rt.console.video().snapshot();
    if (!frame){
        return false;
    }
    ImageViewRGB32 game = extract_box_reference(*frame.frame, GameSettings::instance().GAME_BOX);
    AutoStoryScroll::sample_screen(
        picture,
        game.data(), game.bytes_per_row(),
        game.width(), game.height()
    );
    return true;
}

bool story_offset_from(
    StoryRuntime& rt, const AutoStoryScroll::Screen& picture, const std::string& where,
    int& dx, int& dy, AutoStoryScroll::Screen& now, int max_tiles
){
    if (!story_take_picture(rt, now)){
        return false;
    }
    AutoStoryScroll::TileOffset offset = AutoStoryScroll::tile_offset(picture, now, max_tiles);
    rt.console.log("Position check (" + where + "): " + offset.summary());
    if (!offset.clear){
        return false;
    }
    dx = offset.x;
    dy = offset.y;
    return true;
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
//  A trainer that came up to the player stays where it stopped and no
//  longer wanders (trainer_see.c: SetTrainerMovementType()), and it may
//  stand in the way: a picnicker or bird keeper on the walkways of Route 13.
//  Walk around it: a step aside, past it, and back into line. With one move
//  left (the trainer stands on the tile where the route turns), only a step
//  aside the way the route turns next gets past: then that next run is one
//  shorter ("next_reduce"). Returns the moves of this run made that way (0:
//  no way around).
int walk_around(
    StoryRuntime& rt, DpadPosition direction, int remaining,
    DpadPosition next_direction, uint16_t& next_reduce
){
    const bool across = direction == DPAD_LEFT || direction == DPAD_RIGHT;
    DpadPosition sides[2] = {across ? DPAD_UP : DPAD_LEFT, across ? DPAD_DOWN : DPAD_RIGHT};
    if (next_direction == sides[1]){
        std::swap(sides[0], sides[1]);
    }
    auto moved = [](const RunAttempt& a, int moves){
        return a.event == RunEvent::NONE && !a.lost && a.measured && a.moves == moves;
    };
    for (DpadPosition side : sides){
        if (remaining < 2 && side != next_direction){
            continue;
        }
        RunAttempt aside = attempt_moves(rt, side, 1, WALK_FRAMES, 1, false, false, true);
        if (!moved(aside, 1)){
            if (aside.event != RunEvent::NONE || aside.lost){
                return 0;   //  Something else came up: back to the usual way.
            }
            continue;
        }
        const int ahead = std::min(remaining, 2);
        rt.console.log(
            std::string("Trying to walk around: one step ") + dpad_name(side) + ", then " + dpad_name(direction) + ".",
            COLOR_ORANGE
        );
        RunAttempt past = attempt_moves(rt, direction, ahead, WALK_FRAMES, 1, false, false, true);
        if (moved(past, ahead)){
            if (remaining < 2){
                rt.console.log(
                    std::string("Walked around someone in the way: ") + dpad_name(side) + ", then " +
                    dpad_name(direction) + ". The next run is one shorter.",
                    COLOR_ORANGE
                );
                next_reduce = 1;
                return 1;
            }
            RunAttempt back = attempt_moves(rt, opposite(side), 1, WALK_FRAMES, 1, false, false, true);
            if (moved(back, 1)){
                rt.console.log(
                    std::string("Walked around someone in the way: ") + dpad_name(side) + ", " +
                    dpad_name(direction) + " 2, back.",
                    COLOR_ORANGE
                );
                return 2;
            }
            if (back.event != RunEvent::NONE || back.lost){
                return 0;
            }
            //  The way back onto the route is shut: not someone in the way
            //  but a wall, and the player is not where the route thinks.
            //  Back to where this started, so that stays known.
            rt.console.log("No way back onto the route there. Back to where the player was.", COLOR_ORANGE);
            attempt_moves(rt, opposite(direction), ahead, WALK_FRAMES, 1, false, false, true);
            attempt_moves(rt, opposite(side), 1, WALK_FRAMES, 1, false, false, true);
            return 0;
        }
        //  Not that way: back to where it started.
        if (past.event != RunEvent::NONE || past.lost){
            return 0;
        }
        if (past.moves > 0){
            attempt_moves(rt, opposite(direction), past.moves, WALK_FRAMES, 1, false, false, true);
        }
        attempt_moves(rt, opposite(side), 1, WALK_FRAMES, 1, false, false, true);
    }
    return 0;
}

//  The run before stopped by a battle where the video could not tell how far
//  it got, and this one does not move from its first tile: the player is
//  likely one tile off along the run before (Pokemon Tower 6F, in the fog:
//  "2 of 5" counted when a GASTLY came, one more taken after it, and the run
//  down hit a grave). A step back along it, or one more, and this run's way:
//  the one that moves puts the player back on the route. Returns whether it
//  did (one move of this run made). "short_first": one more along the run
//  before is tried first (it likely ended one short).
bool step_back_onto_route(StoryRuntime& rt, DpadPosition direction, bool short_first){
    const DpadPosition before = rt.last_run_direction;
    auto went = [](const RunAttempt& a){
        return a.event == RunEvent::NONE && !a.lost && a.moves == 1;
    };
    const DpadPosition ways[2] = {
        short_first ? before : opposite(before),
        short_first ? opposite(before) : before,
    };
    for (DpadPosition way : ways){
        RunAttempt aside = attempt_moves(rt, way, 1, WALK_FRAMES, 1, false, false, true);
        if (!went(aside)){
            if (aside.event != RunEvent::NONE || aside.lost){
                return false;
            }
            continue;
        }
        RunAttempt on = attempt_moves(rt, direction, 1, WALK_FRAMES, 1, false, false, true);
        if (went(on)){
            rt.console.log(
                std::string("The run before was one tile off: one step ") + dpad_name(way) +
                ", and now " + dpad_name(direction) + " goes.",
                COLOR_ORANGE
            );
            rt.steps++;
            return true;
        }
        if (on.event != RunEvent::NONE || on.lost){
            return false;
        }
        attempt_moves(rt, opposite(way), 1, WALK_FRAMES, 1, false, false, true);
    }
    return false;
}

bool walk_run(
    StoryRuntime& rt, const StoryLeg& leg, const StoryRun& run, uint16_t count,
    bool track, bool scene_at_end, bool warp_at_end, const std::string& where,
    DpadPosition next_direction, uint16_t& next_reduce
){
    //  For the next run: this one, and whether it may have ended one tile
    //  off (see StoryRuntime::last_run_off).
    const int off_before = rt.last_run_off;
    const DpadPosition direction_before = rt.last_run_direction;
    int off = 0;
    struct LastRun{
        StoryRuntime& rt;
        DpadPosition direction;
        int& off;
        ~LastRun(){
            rt.last_run_direction = direction;
            rt.last_run_off = off;
        }
    } last_run{rt, run.direction, off};
    bool stepped_back = false;
    bool taken_as_made = false;
    //  The last try of this run that moved: how often the video was unsure.
    size_t moved_unsure = 0;
    size_t moved_frames = 0;
    const bool surf = run.kind == RunKind::SURF || run.kind == RunKind::SURF_CHECKED;
    const double frames = run.kind == RunKind::JUMP ? JUMP_FRAMES : (surf ? SURF_FRAMES : WALK_FRAMES);
    const int tiles_per_move = run.kind == RunKind::JUMP ? 2 : 1;
    const bool approach = run.kind == RunKind::APPROACH;
    const bool trainer = run.kind == RunKind::CHECKED_TRAINER || approach;
    const bool encounters = run.kind == RunKind::CHECKED || run.kind == RunKind::SURF_CHECKED || trainer;
    const StoryBattleKind battle_kind = trainer ? StoryBattleKind::TRAINER : StoryBattleKind::WILD;
    const std::string label = where + " (going " + dpad_name(run.direction) + ")";

    //  On an approach, a battle is the trainer to talk to: it saw the
    //  player, walked up, and now stands in the way of the rest of the leg.
    auto trainer_came = [&](StoryBattleResult result){
        if (!approach || result == StoryBattleResult::NO_BATTLE){
            return false;
        }
        rt.console.log("The trainer came to the player. The leg ends here: " + label, COLOR_BLUE);
        rt.trainer_came = true;
        rt.facing = leg.face;
        return true;
    };
    if (!track && encounters){
        //  No video check: one step at a time, watching for a battle after each.
        for (uint16_t i = 0; i < count; i++){
            StoryBattleResult result = story_checked_step(
                rt, run.direction, surf, trainer, StoryBattleKind::WILD,
                where + " step " + std::to_string(i + 1) + "/" + std::to_string(count)
            );
            if (approach){
                rt.approach_made++;
            }
            if (trainer_came(result)){
                return false;
            }
        }
        return false;
    }

    //  In the dark (no Flash) the video cannot see the steps, only that the
    //  screen changes when a battle starts, and the battle text comes long
    //  after the input is over. Where battles can start, one step at a
    //  time then: a battle starts at the end of a step, so the count stays
    //  right.
    const bool one_by_one = leg.dark && encounters && track;

    int remaining = count;
    int not_moving = 0;
    const uint64_t battles_at_start = rt.trainer_battles + rt.wild_battles;
    bool walked_around = false;
    int box_talks = 0;      //  text boxes without the arrow taken for someone talking
    int no_progress = 0;    //  tries in a row that did not move, whatever stopped them
    while (remaining > 0){
        //  After a try that did not move at all, one move at a time: if the
        //  video is wrong about it, little harm is done.
        const int moves = not_moving > 0 || one_by_one ? 1 : remaining;
        //  Running: only with the video check (it measures the run), on the
        //  plain and the encounter tiles of a leg that allows it.
        const bool running = rt.running && track && leg.run && moves > 1 &&
            (run.kind == RunKind::WALK || run.kind == RunKind::CHECKED);
        RunAttempt a = attempt_moves(
            rt, run.direction, moves, frames, tiles_per_move, surf, leg.dark, track, encounters,
            trainer ? rt.trainer_check : Milliseconds(0), running
        );
        if (track){
            rt.console.log(
                std::string(running ? "Ran " : "Walked ") + dpad_name(run.direction) + ": " +
                std::to_string(a.moves) + " of " + std::to_string(moves) +
                (a.measured ? "" : " (video unsure)") + " - " + a.video
            );
        }
        int made = a.moves;
        if (leg.dark && !a.measured && (a.event != RunEvent::NONE || a.lost)){
            //  The step was made: battles and trainers start at its end.
            //  Unless the screen changed before it could be over: then it
            //  was the step before.
            made = a.lost_early ? 0 : moves;
            if (a.on_turn){
                rt.console.log("The screen changed on the turn (a battle can start after a turn too): no step.", COLOR_ORANGE);
            }else if (a.lost_early){
                rt.console.log("The screen changed right away: this step did not happen.", COLOR_ORANGE);
            }
        }
        const bool too_far =
            made > moves && a.measured && !scene_at_end &&
            a.pixels >= (moves * tiles_per_move) * AutoStoryScroll::TILE_PIXELS + AutoStoryScroll::TILE_PIXELS * 3 / 4;
        if (too_far && a.unsure * 10 > a.frames){
            //  A whole tile more, seen on a picture that often did not
            //  tell: a repeating ground fooling the video, not a step too
            //  many (Cinnabar, on the way to the Pokemon Center: 8 of 7 with
            //  11 frames of 52 unsure, and the step back left the player
            //  beside the door).
            rt.console.log("The video says one too far, but it was often unsure on the way. Not stepping back.", COLOR_ORANGE);
            made = moves;
        }else if (too_far){
            //  One too many (the input lasted too long): step back. Only
            //  when the video saw most of that extra step: 89 px for a run
            //  of 5 (80 px) rounds to 6, but on a floor with little detail
            //  (Koga's gym) that is the video being off, and stepping back
            //  then puts the player beside the route.
            int back = made - moves;
            rt.console.log("Walked " + std::to_string(back) + " too far. Stepping back.", COLOR_ORANGE);
            for (int i = 0; i < back; i++){
                attempt_moves(rt, opposite(run.direction), 1, frames, tiles_per_move, surf, leg.dark, false);
            }
            made = moves;
            //  If the video was wrong (172 px for 10 on Pokemon Tower 3F,
            //  and the run after it hit a wall), that left the player one
            //  short.
            off = 1;
        }
        if (made > 0){
            moved_unsure = a.unsure;
            moved_frames = a.frames;
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
            (a.event == RunEvent::BATTLE_START || a.lost)
        ){
            if (encounters){
                //  A wild battle on the last step looks the same as the
                //  warp: see whether battle text comes.
                AdvanceBattleDialogWatcher battle_text(COLOR_RED);
                BattleMenuWatcher battle_menu(COLOR_RED);
                if (wait_until(rt.console, rt.context, 4000ms, { battle_text, battle_menu }) >= 0){
                    rt.console.log("A battle on the last step before the warp.", COLOR_ORANGE);
                    story_battle(rt, battle_kind, label);
                    return false;
                }
            }
            rt.console.log("The warp started at the end of the run.");
            return true;
        }

        if (one_by_one && a.lost && a.event == RunEvent::NONE){
            //  The dark screen changed: a battle is starting.
            rt.console.log("The screen changed in the dark: a battle?", COLOR_ORANGE);
            a.event = RunEvent::BATTLE_START;
        }
        if (a.event == RunEvent::NONE && a.lost && encounters && track && !leg.dark &&
            !(warp_at_end && remaining == 0) && !(scene_at_end && remaining == 0)
        ){
            //  The picture changed where wild battles start, but no battle
            //  showed yet: some battle transitions do not go all black, and
            //  the battle text comes seconds later. The run did not go on
            //  as timed then: the video counted the moves up to the change
            //  (the Mansion: 5 counted as 6, and the route went up the
            //  wrong column).
            const int seen = std::min(a.video_moves, moves);
            if (seen != made){
                rt.console.log(
                    "The screen changed on the way: " + std::to_string(seen) + " moves seen, not " +
                    std::to_string(made) + " as timed.",
                    COLOR_ORANGE
                );
                remaining += made - seen;
                rt.steps -= (uint64_t)(made - seen) * (uint64_t)tiles_per_move;
                made = seen;
            }
            a.event = RunEvent::SCREEN_CHANGED;
        }

        StoryBattleResult battle = StoryBattleResult::NO_BATTLE;
        switch (a.event){
        case RunEvent::BATTLE_START:
        case RunEvent::BATTLE_TEXT:
            battle = story_battle(rt, battle_kind, label);
            break;
        case RunEvent::DIALOG:
            //  The REPEL wore off: the walk goes on.
            if (story_close_repel_text(rt)){
                break;
            }
            //  A trainer saw the player (or a sign or person talks). In
            //  FRLG the player does not turn to the trainer
            //  (TrainerSeeFunc_PrepareToEngage()).
            battle = story_battle(rt, StoryBattleKind::TRAINER, label);
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
            if (ret == 2 && story_close_repel_text(rt)){
                //  (The REPEL wore off.)
            }else if (ret == 2){
                battle = story_battle(rt, StoryBattleKind::TRAINER, label);
            }else if (ret >= 0){
                battle = story_battle(rt, battle_kind, label);
            }else{
                rt.console.log("The screen changed but no battle came. Walking on.", COLOR_ORANGE);
            }
            break;
        }
        case RunEvent::NONE:
            break;
        }

        //  Something stopped the run. The video saw up to that moment, but
        //  the step under way then was still finished. Now that the player
        //  stands still again, compare with the screen before the run.
        bool unsure_stop = a.event != RunEvent::NONE && !a.measured;
        if (a.event != RunEvent::NONE && a.has_start && !leg.dark){
            const int dx = run.direction == DPAD_LEFT ? -1 : (run.direction == DPAD_RIGHT ? 1 : 0);
            const int dy = run.direction == DPAD_UP ? -1 : (run.direction == DPAD_DOWN ? 1 : 0);
            //  The run stopped after "made" moves as seen: the real count
            //  is at most one more, so the check looks only that far. (A
            //  long run stopped early, up or down, was never checked: "2 of
            //  14" on Cerulean Cave B1F was really 1, and the route went on
            //  a row too low into a wall.)
            const int reach = (std::min(moves, made + 1) + 1) * tiles_per_move;
            AutoStoryScroll::Screen now;
            if (reach <= (dx != 0 ? 10 : 6) && story_take_picture(rt, now)){
                AutoStoryScroll::TileOffset offset = AutoStoryScroll::tile_offset_along(a.start, now, dx, dy, reach);
                rt.console.log("After the stop, the run got: " + offset.summary());
                const int tiles = offset.x * dx + offset.y * dy;
                if (offset.clear){
                    unsure_stop = false;
                }
                if (offset.clear && tiles >= 0 && tiles % tiles_per_move == 0 && tiles / tiles_per_move <= moves){
                    const int real = tiles / tiles_per_move;
                    if (real != made){
                        rt.console.log(
                            "Walked " + std::to_string(real) + " instead of " + std::to_string(made) + ".",
                            COLOR_ORANGE
                        );
                        remaining += made - real;
                        made = real;
                    }
                }
            }
        }
        if (unsure_stop){
            //  The rest of the run went on from where the player really
            //  was: likely one too far.
            off = -1;
        }

        if (approach){
            rt.approach_made = (uint16_t)(rt.approach_made + made);
        }
        if (trainer_came(battle)){
            return false;
        }
        if (made == 0){
            no_progress++;
            if (no_progress >= 15){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "walk_leg(): The player does not get any further: " + label +
                    ", " + std::to_string(remaining) + " of " + std::to_string(count) + " moves left.",
                    rt.console
                );
            }
        }else{
            no_progress = 0;
        }

        if (made == 0 && a.event == RunEvent::NONE){
            //  Someone talking stops the player too. A text box that was
            //  there from the start is not noticed while walking.
            VideoSnapshot screen = rt.console.video().snapshot();
            BattleMenuDetector battle_menu(COLOR_RED);
            AdvanceBattleDialogDetector battle_text(COLOR_RED);
            if (screen && (battle_menu.detect(screen) || battle_text.detect(screen))){
                //  A battle that started unseen (in the dark).
                rt.console.log("A battle is on.", COLOR_ORANGE);
                if (trainer_came(story_battle(rt, battle_kind, label))){
                    return false;
                }
                continue;
            }
            AdvanceWhiteDialogDetector talking(COLOR_RED);
            WhiteDialogDetector text_box(COLOR_RED);
            const bool arrow = screen && talking.detect(screen);
            //  A trainer's first words have no advance arrow. (A few tries:
            //  some places look like a text box, like the white bottom of
            //  the S.S. Anne corridors.)
            const bool box = !arrow && box_talks < 3 && screen && text_box.detect(screen);
            if (arrow || box){
                if (box){
                    box_talks++;
                }
                rt.console.log("Someone is talking.", COLOR_ORANGE);
                StoryBattleResult talked = story_battle(rt, StoryBattleKind::TRAINER, label);
                //  The last input came while the player could not move: it
                //  may not have turned them.
                rt.facing = DPAD_NONE;
                if (trainer_came(talked)){
                    return false;
                }
                continue;
            }
            not_moving++;
            if (!taken_as_made && not_moving >= 2 && remaining == 1 && remaining < (int)count &&
                moved_frames > 0 && moved_unsure * 10 >= moved_frames &&
                track && !leg.dark && !surf && run.kind != RunKind::JUMP &&
                !warp_at_end && !scene_at_end
            ){
                //  The last move of the run does not go, and the video was
                //  often unsure on the way: it likely counted one short
                //  (Pokemon Tower 3F: "10 of 11" with 14 of 66 frames unsure,
                //  the player at the end, and the last move into a grave).
                taken_as_made = true;
                rt.console.log(
                    "The last move does not go, and the video was often unsure on the way: taking it as made.",
                    COLOR_ORANGE
                );
                remaining = 0;
                rt.steps += (uint64_t)tiles_per_move;
                off = 1;
                continue;
            }
            if (!stepped_back && not_moving >= 2 && remaining == count && off_before != 0 &&
                direction_before != DPAD_NONE && direction_before != run.direction &&
                direction_before != opposite(run.direction) &&
                track && !leg.dark && !surf && run.kind != RunKind::JUMP
            ){
                stepped_back = true;
                if (step_back_onto_route(rt, run.direction, off_before > 0)){
                    remaining--;
                    rt.steps++;
                    not_moving = 0;
                    no_progress = 0;
                    continue;
                }
            }
            if (!walked_around && not_moving >= 2 &&
                rt.trainer_battles + rt.wild_battles > battles_at_start &&
                track && !leg.dark && !surf && run.kind != RunKind::JUMP
            ){
                //  A battle on this run, and now the way is shut: maybe the
                //  trainer stands in it.
                walked_around = true;
                const int around = walk_around(rt, run.direction, remaining, next_direction, next_reduce);
                if (around > 0){
                    remaining -= around;
                    rt.steps += (uint64_t)around;
                    not_moving = 0;
                    no_progress = 0;
                    continue;
                }
            }
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
    StoryMemoryScope memory_scope("walking");
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
    //  In the dark a turn has to be known to be one (see attempt_moves()).
    //  After a warp the game sets the facing (GetAdjustedInitialDirection():
    //  on a ladder, the way the player went), and the route has it.
    if (rt.facing == DPAD_NONE && rt.arrived_by_warp && leg.dark && leg.start_facing != DPAD_NONE){
        rt.facing = leg.start_facing;
    }
    rt.arrived_by_warp = false;
    rt.last_leg_dark = leg.dark;
    rt.trainer_battles_before_leg = rt.trainer_battles;

    rt.trainer_came = false;
    rt.last_run_direction = DPAD_NONE;
    rt.last_run_off = 0;
    uint16_t approach_total = 0;
    for (const StoryRun& run : runs){
        if (run.kind == RunKind::APPROACH){
            approach_total = (uint16_t)(approach_total + run.count);
        }
    }
    if (approach_total > 0){
        rt.approach_total = approach_total;
        rt.approach_made = 0;
    }
    bool warped = false;
    bool warped_early = false;
    uint16_t reduce_next = 0;   //  a walk around someone made part of the next run
    for (size_t r = 0; r < runs.size() && !rt.trainer_came; r++){
        const StoryRun& run = runs[r];
        const bool last = r + 1 == runs.size();
        const RunKind next = last ? RunKind::WALK : runs[r + 1].kind;
        switch (run.kind){
        case RunKind::WALK:
        case RunKind::JUMP:
        case RunKind::SURF:
        case RunKind::CHECKED:
        case RunKind::CHECKED_TRAINER:
        case RunKind::APPROACH:
        case RunKind::SURF_CHECKED:{
            //  A spinner after this run carries the player on: the video
            //  would see that too.
            const bool track = rt.verify_walking && next != RunKind::SPIN;
            const bool onto_warp = last && leg.end == LegEnd::WARP_STEP && run.kind != RunKind::JUMP;
            uint16_t count = onto_warp ? (uint16_t)(run.count - 1) : run.count;
            count = (uint16_t)(count - std::min(count, reduce_next));
            reduce_next = 0;
            const bool scene_at_end = last && leg.end == LegEnd::TRIGGER;
            const bool warp_at_end = last && leg.end == LegEnd::WARP_PUSH;
            const DpadPosition next_direction = last ? DPAD_NONE : runs[r + 1].direction;
            if (count > 0){
                warped_early = walk_run(
                    rt, leg, run, count, track, scene_at_end, warp_at_end, leg.name,
                    next_direction, reduce_next
                );
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
        case RunKind::BACK:{
            //  Out of the line of the trainer just talked to, back to where
            //  the approach entered it. If the trainer came up to the
            //  player, the approach got that much less far. (The first move
            //  of the approach only went into the line.) Not known (the
            //  program started here): all of it.
            const int into_line = (int)rt.approach_total - (int)run.count;
            const int back = std::max(0, std::min((int)run.count, (int)rt.approach_made - into_line));
            if (back != run.count){
                rt.console.log(
                    "The trainer came " + std::to_string(run.count - back) + " tile(s) closer: walking back " +
                    std::to_string(back) + " of " + std::to_string(run.count) + ".",
                    COLOR_BLUE
                );
            }
            if (back > 0){
                StoryRun plain{run.direction, (uint16_t)back, RunKind::WALK};
                uint16_t no_reduce = 0;
                walk_run(rt, leg, plain, (uint16_t)back, rt.verify_walking, false, false, leg.name, DPAD_NONE, no_reduce);
            }
            rt.approach_total = 0;
            rt.approach_made = 0;
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
            //  The change of screen at the end of the run was a battle whose
            //  text came late (Victory Road's way out: a Marowak, and the
            //  route went on as if on Route 23). The player still stands
            //  at the warp: push again.
            if (battle_instead_of_warp(rt, leg.dark || leg.dest_dark, leg.name)){
                take_warp_push(rt, leg.push, leg.dark || leg.dest_dark, leg.preview, leg.name);
            }
        }else{
            take_warp_push(rt, leg.push, leg.dark || leg.dest_dark, leg.preview, leg.name);
        }
        rt.facing = DPAD_NONE;
        rt.arrived_by_warp = true;
        break;
    case LegEnd::WARP_STEP:
        if (!warped){
            wait_warp_step(rt, leg.dark || leg.dest_dark, leg.preview, leg.name);
        }
        rt.facing = DPAD_NONE;
        rt.arrived_by_warp = true;
        break;
    case LegEnd::TRIGGER:
        pbf_wait(rt.context, 300ms);
        rt.context.wait_for_all_requests();
        break;
    case LegEnd::INTERACT:
        if (rt.trainer_came){
            //  The player already faces the trainer, who stands next to them.
            break;
        }
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
    //  Some places look like a text box all the time (the white bottom of
    //  the S.S. Anne corridors): after a few presses on one, check whether
    //  the player is free to move.
    int dialog_in_row = 0;
    //  Answering YES: since when a text box without the advance arrow is up,
    //  and how many times such a box stayed (A pressed on it).
    WallClock no_arrow_since = WallClock::max();
    int no_arrow_times = 0;
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
        case 1:{
            //  Answering YES, not this check: it presses START and then B,
            //  and B goes to the text (Oak's aide on Route 15: five pages,
            //  the B's of the check went through them and said NO).
            if (!answer_yes &&
                ++dialog_in_row % 5 == 0 && current_time() - start >= min_time &&
                in_overworld(rt.console, rt.context)
            ){
                rt.console.log("Cutscene over: " + what);
                return false;
            }
            if (!answer_yes){
                //  B advances the text (and never says YES to anything).
                pbf_press_button(rt.context, BUTTON_B, 160ms, 340ms);
                break;
            }
            //  A question to say YES to may come: B on its YES/NO box is a NO
            //  (the bikers on Three Island: no battles, and they stay). The
            //  box comes up the moment the text is printed, so B only goes
            //  to a page that waits with its advance arrow.
            VideoSnapshot screen = rt.console.video().snapshot();
            SelectionDialogDetector yes_no_now(COLOR_RED);
            AdvanceWhiteDialogDetector arrow(COLOR_RED);
            if (screen && yes_no_now.detect(screen)){
                //  The YES/NO box: the next round answers it.
                pbf_wait(rt.context, 100ms);
                break;
            }
            if (screen && arrow.detect(screen)){
                no_arrow_since = WallClock::max();
                pbf_press_button(rt.context, BUTTON_B, 160ms, 340ms);
                break;
            }
            WallClock now = current_time();
            if (no_arrow_since == WallClock::max()){
                no_arrow_since = now;
            }
            if (now - no_arrow_since >= 8s){
                //  A box that never shows the arrow: A advances it too (and
                //  says YES to a box the detector does not see). One that
                //  stays after a few A's may be a place that only looks
                //  like a text box: is the player free?
                if (++no_arrow_times >= 3 && current_time() - start >= min_time &&
                    in_overworld(rt.console, rt.context)
                ){
                    rt.console.log("Cutscene over: " + what);
                    return false;
                }
                rt.console.log("Text without the advance arrow: A.", COLOR_ORANGE);
                no_arrow_since = WallClock::max();
                pbf_press_button(rt.context, BUTTON_A, 160ms, 340ms);
                break;
            }
            //  The text is still printing.
            pbf_wait(rt.context, 200ms);
            break;
        }
        case 3:
        case 4:
            //  The battle text detectors also take some overworld places
            //  for a text box.
            if (in_overworld(rt.console, rt.context)){
                if (current_time() - start >= min_time){
                    rt.console.log("Cutscene over: " + what);
                    return false;
                }
                break;
            }
            rt.console.log("A battle is on screen: " + what);
            return true;
        case 2:
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


namespace{
//  Nothing came up on A: the player may not face what to talk to. The tap
//  that turns them can be missed by the game (the Mansion statues, now and
//  then, with the same inputs that work other times). Turn again, with a
//  moment of standing still first. (Facing it already, the tap only
//  bumps into it.)
void face_again(StoryRuntime& rt, const std::string& what){
    if (rt.facing == DPAD_NONE){
        return;
    }
    rt.console.log(
        "No text at " + what + ". Turning " + std::string(dpad_name(rt.facing)) + " again.",
        COLOR_ORANGE
    );
    pbf_wait(rt.context, 500ms);
    pbf_press_dpad(rt.context, rt.facing, frames_to_ms(4), frames_to_ms(14));
    rt.context.wait_for_all_requests();
}
}

namespace{

//  To where the one talked to may stand instead: one step, then turn.
void go_to_talk_alt(StoryRuntime& rt, const StoryTalkAlt& alt, const std::string& what){
    rt.console.log("No text box at " + what + ": trying where it may stand instead.", COLOR_ORANGE);
    if (alt.move != DPAD_NONE){
        bool moved = false;
        story_checked_step(
            rt, alt.move, false, false, StoryBattleKind::WILD, what + " (the other spot)", true, &moved
        );
        if (moved){
            rt.talk_alt_back =
                alt.move == DPAD_UP ? DPAD_DOWN :
                alt.move == DPAD_DOWN ? DPAD_UP :
                alt.move == DPAD_LEFT ? DPAD_RIGHT : DPAD_LEFT;
        }
    }
    story_face(rt, alt.face);
}

}

bool story_talk_start(StoryRuntime& rt, const std::string& what, const StoryTalkAlt& alt, bool may_be_gone){
    rt.console.log("Talking: " + what);
    int ret = -1;
    for (int spot = 0; spot < (alt.on ? 2 : 1) && ret < 0; spot++){
        if (spot == 1){
            go_to_talk_alt(rt, alt, what);
        }
        //  With another spot, one try here: turning again is a step when
        //  no one is there (Celio: two steps down, then the other spot
        //  was tried from the wrong tile).
        //  An item that may be gone: one try (turning again would be a
        //  step onto its tile).
        const int attempts = (alt.on && spot == 0) || may_be_gone ? 1 : 3;
        for (int attempt = 0; attempt < attempts && ret < 0; attempt++){
            if (attempt > 0){
                face_again(rt, what);
            }
            WhiteDialogWatcher dialog(COLOR_RED);
            AdvanceBattleDialogWatcher battle_text(COLOR_RED);
            rt.context.wait_for_all_requests();
            ret = run_until<ProControllerContext>(
                rt.console, rt.context,
                [](ProControllerContext& context){
                    for (int i = 0; i < 2; i++){
                        pbf_press_button(context, BUTTON_A, 160ms, 1840ms);
                    }
                },
                { dialog, battle_text }
            );
        }
    }
    if (ret < 0 && may_be_gone){
        rt.console.log("No text: the item is not there (picked up before). Walking on.", COLOR_ORANGE);
        return false;
    }
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_talk_start(): No text box after pressing A at: " + what +
            ". The player is probably not where the route expects.",
            rt.console
        );
    }
    return true;
}


void story_talk_answer(StoryRuntime& rt, bool yes, const std::string& what, const StoryTalkAlt& alt){
    rt.console.log(std::string(yes ? "Talking and answering YES: " : "Talking and answering NO: ") + what);
    //  One A starts the conversation, more A advance the text. They are
    //  spaced out so the YES/NO box is seen before an A answers it. If no
    //  text comes up at all, the player is turned again.
    int ret = -1;
    for (int spot = 0; spot < (alt.on ? 2 : 1) && ret < 0; spot++){
        if (spot == 1){
            go_to_talk_alt(rt, alt, what);
        }
        //  With another spot, one try here (turning again is a step when
        //  no one is there).
        const int attempts = alt.on && spot == 0 ? 1 : 3;
        for (int attempt = 0; attempt < attempts; attempt++){
            if (attempt > 0){
                face_again(rt, what);
            }
            SelectionDialogWatcher yes_no(COLOR_RED);
            WhiteDialogWatcher text(COLOR_RED);
            rt.context.wait_for_all_requests();
            ret = run_until<ProControllerContext>(
                rt.console, rt.context,
                [](ProControllerContext& context){
                    pbf_press_button(context, BUTTON_A, 160ms, 2840ms);
                },
                { yes_no, text }
            );
            if (ret >= 0){
                break;
            }
        }
    }
    const bool text_seen = ret == 1;
    if (ret == 1){
        //  The text is up: on to the question, the A presses still spaced
        //  out from the first one (the box may come up on its own once the
        //  text is printed).
        SelectionDialogWatcher yes_no(COLOR_RED);
        ret = run_until<ProControllerContext>(
            rt.console, rt.context,
            [](ProControllerContext& context){
                for (int i = 0; i < 5; i++){
                    pbf_wait(context, 2700ms);
                    pbf_press_button(context, BUTTON_A, 160ms, 140ms);
                }
                pbf_wait(context, 2700ms);
            },
            { yes_no }
        );
    }
    if (ret < 0 && text_seen && what == "Use Strength"){
        //  Strength is on already (used on this map before, no warp since):
        //  the boulder only says "STRENGTH made it possible to move boulders
        //  around." (EventScript_AlreadyUsedStrength). Close it and go on.
        rt.console.log("Strength is on already: no question. Going on.", COLOR_ORANGE);
        pbf_mash_button(rt.context, BUTTON_B, 2000ms);
        rt.context.wait_for_all_requests();
        return;
    }
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


void story_heal(StoryRuntime& rt, const std::string& where, bool mid_step){
    StoryMemoryScope memory_scope("healing");
    rt.console.log("Healing: " + where);
    heal_at_pokecenter(rt.console, rt.context);
    story_cutscene(rt, 0ms, 30s, false, "Nurse at " + where);
    rt.lead_fainted = false;
    rt.memory.no_pp = 0;
    rt.facing = DPAD_UP;
    if (!rt.save_after_heal || (!mid_step && rt.next_is_save)){
        return;
    }
    //  In front of the nurse: a good place to come back to.
    story_save(rt, rt.has_dex);
    rt.facing = DPAD_UP;
    story_saved(rt, mid_step ? rt.step_index : rt.step_index + 1);
}

namespace{
std::string gigabytes(size_t bytes){
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    return buffer;
}
}

void story_log_memory(StoryRuntime& rt, bool force){
    WallClock now = current_time();
    if (!force && rt.last_memory_log != WallClock::min() && now - rt.last_memory_log < std::chrono::minutes(5)){
        return;
    }
    rt.last_memory_log = now;
    MemoryUsage memory = process_memory_usage();
    rt.console.log(
        "Memory: program " + gigabytes(memory.process_physical_memory) +
        " (virtual " + gigabytes(memory.process_virtual_memory) + "), system " +
        gigabytes(memory.total_used_system_memory) + " of " + gigabytes(memory.total_system_memory) + " in use."
    );
    rt.console.log(story_memory_summary());
}

std::set<std::string> story_species_list(const char* text){
    std::set<std::string> species;
    std::string list = text == nullptr ? "" : text;
    size_t start = 0;
    while (start <= list.size()){
        size_t comma = list.find(',', start);
        std::string slug = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!slug.empty()){
            species.insert(slug);
        }
        if (comma == std::string::npos){
            break;
        }
        start = comma + 1;
    }
    return species;
}


void story_saved(StoryRuntime& rt, size_t resume_at){
    rt.resume_at = resume_at;
    rt.caught_at_save = rt.caught;
    rt.party_extras_at_save = rt.party_extras;
    rt.party_order_final_at_save = rt.party_order_final;
    if (rt.shiny_unsaved){
        rt.console.log("The shiny Pokemon caught is saved.", COLOR_BLUE);
    }
    rt.shiny_unsaved = false;
    //  Saved at the end of the segment: the next one starts there.
    const bool next = resume_at >= rt.segment_steps && !rt.next_segment_id.empty();
    const std::string resume_segment = next ? rt.next_segment_id : rt.segment_id;
    const size_t resume_step = next ? 1 : resume_at + 1;
    const std::string where = resume_segment + ", step " + std::to_string(resume_step);
    rt.console.log(
        "Saved. To go on from this save later: start at segment " + where + ".",
        COLOR_BLUE
    );
    if (rt.on_saved){
        rt.on_saved(resume_segment, resume_step);
    }
    story_log_memory(rt, true);

    //  The program's memory use keeps growing on some computers (video
    //  pipeline) until it crashes after some hours. Stop here, right after
    //  a save, before that happens.
    //  (The two crashes seen happened with 6.4 and 7.9 GB committed on a
    //  12 GB computer, while it starts with about 2 GB.)
    MemoryUsage memory = process_memory_usage();
    if (story_memory_too_high(memory)){
        throw ProgramFinishedException(
            "Pokemon Automation uses " + gigabytes(memory.process_virtual_memory) + " of the " +
            gigabytes(memory.total_system_memory) + " of memory and could crash soon. The game was "
            "just saved: close Pokemon Automation completely (its main window too), open it again "
            "and start at segment " + where + "."
        );
    }
}

bool story_memory_too_high(const MemoryUsage& memory){
    return memory.total_system_memory > 0 && (
        memory.process_virtual_memory > memory.total_system_memory / 5 * 2 ||
        memory.process_physical_memory > memory.total_system_memory / 2
    );
}

void story_check_memory_at_start(StoryRuntime& rt){
    //  The memory use only goes down when Pokemon Automation is closed:
    //  starting the program again in the same window stops at the first
    //  save. Say so now instead.
    MemoryUsage memory = process_memory_usage();
    if (story_memory_too_high(memory)){
        throw UserSetupError(
            rt.console.logger(),
            "Pokemon Automation already uses " + gigabytes(memory.process_virtual_memory) + " of the " +
            gigabytes(memory.total_system_memory) + " of memory (it starts with less than 1 GB), and "
            "would stop again at the first save. Close Pokemon Automation completely (its main window "
            "too), open it again, then start this program."
        );
    }
}


namespace{

//  After YES to "Would you like to save the game?" (start_menu.c): the game
//  asks to overwrite the save there is, "There is already a saved file. Is
//  it okay to overwrite it?" with YES first, or, the first time after NEW
//  GAME over another game's save (new_game.c: gDifferentSaveFile), "WARNING!
//  There is a different game file that is already saved. ... Are you sure
//  you want to save now and overwrite the other save file?" over four pages
//  and then YES/NO with NO first. B there is NO: nothing saved. Then
//  "SAVING... DON'T TURN OFF THE POWER." and "{PLAYER} saved the game."
//  Returns whether the game saves.
bool story_confirm_save(StoryRuntime& rt){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    auto has = [](const std::string& text, const char* part){
        return text.find(part) != std::string::npos;
    };
    const WallClock deadline = current_time() + 25s;
    bool warned = false;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const std::string text = story_read_dialog(rt);
        if (has(text, "saving") || has(text, "turn off") || has(text, "saved the game")){
            return true;
        }
        SelectionArrowWatcher yes_arrow(COLOR_RED, &console.overlay(), SelectionArrowPositionConfirmationMenu::YES);
        SelectionArrowWatcher no_arrow(COLOR_RED, &console.overlay(), SelectionArrowPositionConfirmationMenu::NO);
        const int ret = wait_until(console, context, 500ms, { yes_arrow, no_arrow });
        if (ret == 0){
            //  Overwrite: YES.
            pbf_press_button(context, BUTTON_A, 160ms, 640ms);
            continue;
        }
        if (ret == 1){
            //  NO first: the question after the WARNING. Up to YES.
            console.log("Overwriting the other game's save (the old game is lost): YES.", COLOR_ORANGE);
            pbf_press_dpad(context, DPAD_UP, 160ms, 340ms);
            continue;
        }
        if (!has(text, "are you sure") && (
            has(text, "warning") || has(text, "different game") ||
            has(text, "if you save now") || has(text, "entirely lost")
        )){
            if (!warned){
                console.log("The game warns of another game's save (the first save after NEW GAME).", COLOR_ORANGE);
                warned = true;
            }
            //  The next page.
            pbf_press_button(context, BUTTON_A, 160ms, 640ms);
            continue;
        }
    }
    return false;
}

}

void story_save(StoryRuntime& rt, bool has_pokedex, bool has_pokemon){
    StoryMemoryScope memory_scope("saving");
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log(
        has_pokedex ? "Saving the game." :
        has_pokemon ? "Saving the game (no Pokedex yet)." :
        "Saving the game (no Pokemon yet)."
    );
    //  The start menu without POKeDEX: POKeMON, BAG, the player, SAVE, OPTION,
    //  EXIT. Before the first Pokemon also without POKeMON: BAG, the player,
    //  SAVE, ... where SAVE is the third entry (the player's place in the
    //  menu with POKeMON; moving to the fourth opened OPTION).
    const SelectionArrowPositionNoDexMenu save_entry = has_pokemon
        ? SelectionArrowPositionNoDexMenu::SAVE
        : SelectionArrowPositionNoDexMenu::TRAINER;
    //  As save_game_to_overworld(), with both start menus, the game's
    //  questions after YES answered (story_confirm_save()), and only done
    //  once the game saves: "SAVING..." or "saved the game" seen. (Before,
    //  a save that did not happen went on as saved: after NEW GAME over
    //  another game's save not one of a whole night's saves happened.)
    WallClock deadline = current_time() + 150s;
    int tries = 0;
    while (true){
        context.wait_for_all_requests();
        if (current_time() > deadline){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_save(): The game did not save (tried for 2 minutes, " + std::to_string(tries) + " times).",
                console
            );
        }
        StartMenuWatcher start_menu(COLOR_RED);
        if (wait_until(console, context, 1000ms, { start_menu }) != 0){
            pbf_press_button(context, BUTTON_PLUS, 200ms, 800ms);
            continue;
        }
        const bool at_save = has_pokedex
            ? move_cursor_to_position(console, context, SelectionArrowPositionStartMenu::SAVE)
            : move_cursor_to_position(console, context, save_entry);
        if (!at_save){
            pbf_mash_button(context, BUTTON_B, 500ms);
            continue;
        }
        pbf_press_button(context, BUTTON_A, 320ms, 400ms);
        context.wait_for_all_requests();

        //  "Would you like to save the game?"
        SelectionArrowWatcher save_arrow(COLOR_RED, &console.overlay(), SelectionArrowPositionConfirmationMenu::YES);
        if (wait_until(console, context, 2000ms, { save_arrow }) != 0){
            console.log("Unable to detect Save Arrow. Attempting to open start menu again.");
            pbf_mash_button(context, BUTTON_B, 500ms);
            continue;
        }
        console.log("Detected Save Arrow. Saving game.");
        pbf_press_button(context, BUTTON_A, 320ms, 200ms);
        tries++;
        if (story_confirm_save(rt)){
            break;
        }
        console.log("The game did not save (no \"SAVING...\" seen). Again.", COLOR_ORANGE);
        pbf_mash_button(context, BUTTON_B, 1500ms);
    }
    //  Let "SAVING... DON'T TURN OFF THE POWER." finish, then close the
    //  "saved the game" text.
    pbf_wait(context, 4000ms);
    pbf_mash_button(context, BUTTON_B, 1500ms);
    context.wait_for_all_requests();
    story_back_to_overworld(rt, "after saving");
}


namespace{

//  The digits of the game's normal font (FONT_NORMAL, latin_normal.png):
//  the main color of glyph rows 2 to 10, 5 pixels (bit 4 = the left one).
//  A digit takes 6 pixels.
const uint8_t LOAD_MENU_DIGITS[10][9] = {
    {14, 17, 17, 17, 17, 17, 17, 17, 14},
    { 4, 12,  4,  4,  4,  4,  4,  4, 14},
    {14, 17, 17,  1,  2,  4,  8, 16, 31},
    {14, 17,  1,  1,  6,  1,  1, 17, 14},
    { 6, 10, 10, 18, 18, 18, 31,  2,  2},
    {31, 16, 16, 16, 30,  1,  1, 17, 14},
    {14, 17, 16, 16, 30, 17, 17, 17, 14},
    {31,  1,  1,  1,  2,  2,  4,  4,  4},
    {14, 17, 17, 17, 14, 17, 17, 17, 14},
    {14, 17, 17, 17, 15,  1,  1, 17, 14},
};

}

int read_load_menu_dex_count(const ImageViewRGB32& screen){
    //  The load menu (main_menu.c): the CONTINUE window at tile (3, 1).
    //  PrintDexCount prints "POKeDEX" at (2, 50) in it and the number at
    //  (62, 50): on the game screen from x = 86, y = 58. Blue text with a
    //  light blue shadow on white; the main color of the glyphs is matched
    //  pixel by pixel (at the centers of the game's pixels).
    ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const size_t width = game.width();
    const size_t height = game.height();
    if (width < 240 || height < 160){
        return -1;
    }
    const double sx = (double)width / 240.0;
    const double sy = (double)height / 160.0;
    auto pixel = [&](int x, int y){
        const size_t px = std::min(width - 1, (size_t)((x + 0.5) * sx));
        const size_t py = std::min(height - 1, (size_t)((y + 0.5) * sy));
        return game.pixel(px, py);
    };
    auto ink = [&](int x, int y){
        const uint32_t p = pixel(x, y);
        const int r = (p >> 16) & 0xff;
        const int b = p & 0xff;
        return r < 160 && b > r + 50;
    };
    auto white = [&](int x, int y){
        const uint32_t p = pixel(x, y);
        return ((p >> 16) & 0xff) > 200 && ((p >> 8) & 0xff) > 200 && (p & 0xff) > 200;
    };

    //  The window: white right above and below the line.
    int white_count = 0;
    for (int x = 24; x < 120; x += 2){
        white_count += white(x, 57) ? 1 : 0;
        white_count += white(x, 71) ? 1 : 0;
    }
    if (white_count < 90){
        return -1;
    }
    //  The label "POKeDEX" is there (it shows only with a POKeDEX).
    int label_ink = 0;
    for (int y = 60; y <= 68; y++){
        for (int x = 26; x < 70; x++){
            label_ink += ink(x, y) ? 1 : 0;
        }
    }
    if (label_ink < 40){
        return -1;
    }

    int value = 0;
    int digits = 0;
    for (int k = 0; k < 3; k++){
        const int x0 = 86 + 6 * k;
        int cell_ink = 0;
        for (int y = 58; y < 72; y++){
            for (int x = x0; x < x0 + 6; x++){
                cell_ink += ink(x, y) ? 1 : 0;
            }
        }
        if (cell_ink <= 1){
            break;      //  The end of the number.
        }
        int best = -1;
        int best_miss = 1000;
        int second_miss = 1000;
        for (int d = 0; d < 10; d++){
            int miss = 0;
            for (int y = 58; y < 72; y++){
                const int row = y - 60;
                const uint8_t bits = row >= 0 && row < 9 ? LOAD_MENU_DIGITS[d][row] : 0;
                for (int c = 0; c < 6; c++){
                    const bool on = c < 5 && ((bits >> (4 - c)) & 1) != 0;
                    miss += on != ink(x0 + c, y) ? 1 : 0;
                }
            }
            if (miss < best_miss){
                second_miss = best_miss;
                best_miss = miss;
                best = d;
            }else if (miss < second_miss){
                second_miss = miss;
            }
        }
        if (best < 0 || best_miss > 4 || second_miss - best_miss < 3){
            return -1;
        }
        value = value * 10 + best;
        digits++;
    }
    return digits == 0 ? -1 : value;
}

void story_soft_reset(StoryRuntime& rt){
    StoryMemoryScope memory_scope("soft reset");
    int count = -1;
    soft_reset(
        rt.console, rt.context,
        [&](ConsoleHandle& console, ProControllerContext& context){
            //  The menu may still be fading in: a few looks.
            for (int attempt = 0; attempt < 6 && count < 0; attempt++){
                if (attempt > 0){
                    pbf_wait(context, 100ms);
                    context.wait_for_all_requests();
                }
                VideoSnapshot screen = console.video().snapshot();
                if (screen){
                    count = read_load_menu_dex_count(screen);
                }
            }
        }
    );
    rt.dex_count_at_load = count;
    rt.dex_new_since_load.clear();
    if (count >= 0){
        rt.console.log("The game's POKeDEX in this save: " + std::to_string(count) + " species caught.", COLOR_BLUE);
    }else{
        rt.console.log("The POKeDEX number of the load menu was not read (no POKeDEX yet?).");
    }
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

bool story_close_repel_text(StoryRuntime& rt){
    const std::string text = story_read_dialog(rt);
    if (text.find("wore off") == std::string::npos && text.find("effect wore") == std::string::npos){
        return false;
    }
    rt.console.log("\"REPEL's effect wore off...\": wild Pokemon again from here.", COLOR_BLUE);
    pbf_press_button(rt.context, BUTTON_B, 160ms, 800ms);
    rt.context.wait_for_all_requests();
    return true;
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

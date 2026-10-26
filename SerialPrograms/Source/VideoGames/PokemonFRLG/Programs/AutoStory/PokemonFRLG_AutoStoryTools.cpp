/*  Pokemon FRLG Autostory Tools
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
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


//  A straight run of moves of the same length in one direction.
struct Move{
    DpadPosition direction;
    uint32_t count;
    double frames;      //  per move
};

//  Walk the moves as one continuous input, starting from a standstill.
//  Direction switches happen halfway through the last move of a run,
//  which gives a tolerance of half a move either way. The last run is
//  released halfway through its last move, so the player stops on the
//  final tile.
void walk_continuous(
    ProControllerContext& context, DpadPosition& facing,
    const std::vector<Move>& moves
){
    if (moves.empty()){
        return;
    }
    double t = moves[0].direction == facing ? 0 : TURN_FRAMES;
    double previous = 0;
    double last_half = 0;
    for (const Move& move : moves){
        t += move.frames * move.count;
        last_half = move.frames / 2;
        double end = t - last_half;
        pbf_press_dpad(context, move.direction, frames_to_ms(end - previous), 0ms);
        previous = end;
    }
    //  Let the last move finish.
    pbf_wait(context, frames_to_ms(last_half + 6));
    facing = moves.back().direction;
}


//  Hold "direction" from a standstill until the warp has finished.
//  Doors, mats and stairs all warp while the direction is held:
//    door:   facing the door, holding toward it      (TryDoorWarp)
//    mat:    standing on it, holding the arrow       (TryArrowWarp)
//    stairs: standing on them, holding sideways      (TryArrowWarp)
void take_warp_push(
    StoryRuntime& rt, DpadPosition direction, bool dark, const char* leg_name
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
    //  The new map is fading in. Coming out of a door, the game opens the
    //  door, walks the player one step out and closes it again
    //  (Task_ExitDoor), and input is ignored until that is done.
    pbf_wait(rt.context, 2000ms);
    rt.context.wait_for_all_requests();
}

//  The last step landed on a warp tile (ladder, hole, warp pad, cave).
void wait_warp_step(StoryRuntime& rt, bool dark, const char* leg_name){
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
    //  Arriving on a cave entrance, the game walks the player one step out.
    pbf_wait(rt.context, 2000ms);
    rt.context.wait_for_all_requests();
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
    BlackScreenWatcher battle_starting(COLOR_RED);
    AdvanceBattleDialogWatcher battle_started(COLOR_RED);
    WhiteDialogWatcher trainer_talking(COLOR_RED);
    Milliseconds check = trainer ? rt.trainer_check : rt.encounter_check;
    double frames = surf ? SURF_FRAMES : WALK_FRAMES;
    DpadPosition& facing = rt.facing;
    rt.context.wait_for_all_requests();
    int ret = run_until<ProControllerContext>(
        rt.console, rt.context,
        [&](ProControllerContext& context){
            single_move(context, facing, direction, frames, surf);
            pbf_wait(context, check);
        },
        { battle_starting, battle_started, trainer_talking }
    );
    rt.steps++;
    rt.facing = direction;
    if (ret < 0){
        return StoryBattleResult::NO_BATTLE;
    }
    StoryBattleKind kind = (ret == 2 || trainer) ? StoryBattleKind::TRAINER : wild_kind;
    return story_battle(
        rt, kind,
        where + " (going " + dpad_name(direction) + ")",
        throw_on_loss
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



void walk_leg(StoryRuntime& rt, const StoryScript& script, int32_t leg_index){
    const StoryLeg& leg = script.leg(leg_index);
    rt.console.log(std::string("Walking: ") + leg.name);
    if (leg.start_facing != DPAD_NONE){
        rt.facing = leg.start_facing;
    }

    std::vector<Move> pending;      //  moves without wild encounters

    auto flush = [&](){
        if (pending.empty()){
            return;
        }
        //  The first move works whichever way the player faces. After it,
        //  the rest is one continuous input.
        Move& first = pending[0];
        single_move(rt.context, rt.facing, first.direction, first.frames, first.frames == SURF_FRAMES);
        rt.steps++;
        first.count--;
        if (first.count == 0){
            pending.erase(pending.begin());
        }
        if (!pending.empty()){
            walk_continuous(rt.context, rt.facing, pending);
            for (const Move& move : pending){
                rt.steps += move.count;
            }
        }
        pending.clear();
        rt.context.wait_for_all_requests();
    };

    const StoryRun* runs = script.leg_runs(leg);
    for (uint16_t r = 0; r < leg.run_count; r++){
        const StoryRun& run = runs[r];
        switch (run.kind){
        case RunKind::WALK:
            pending.push_back({run.direction, run.count, WALK_FRAMES});
            break;
        case RunKind::JUMP:
            pending.push_back({run.direction, run.count, JUMP_FRAMES});
            break;
        case RunKind::SURF:
            pending.push_back({run.direction, run.count, SURF_FRAMES});
            break;
        case RunKind::CHECKED:
        case RunKind::CHECKED_TRAINER:
        case RunKind::SURF_CHECKED:{
            flush();
            bool surf = run.kind == RunKind::SURF_CHECKED;
            bool trainer = run.kind == RunKind::CHECKED_TRAINER;
            for (uint16_t i = 0; i < run.count; i++){
                story_checked_step(
                    rt, run.direction, surf, trainer, StoryBattleKind::WILD,
                    std::string(leg.name) + " step " + std::to_string(i + 1) + "/" + std::to_string(run.count)
                );
            }
            break;
        }
        case RunKind::SPIN:
            //  The step onto the spinner was the last pending move. The
            //  player is carried without input, then stops.
            flush();
            pbf_wait(rt.context, frames_to_ms(SPIN_FRAMES * run.count + 20) + 300ms);
            rt.context.wait_for_all_requests();
            rt.facing = run.direction;
            break;
        case RunKind::DISMOUNT:
            //  Jump from the water onto the shore (Task_StopSurfingInit).
            //  12 frames works whichever way the player faces, and is over
            //  before the player can move again.
            flush();
            pbf_press_dpad(rt.context, run.direction, frames_to_ms(12), 1200ms);
            rt.context.wait_for_all_requests();
            rt.facing = run.direction;
            rt.steps++;
            break;
        }
    }
    flush();

    switch (leg.end){
    case LegEnd::STOP:
        break;
    case LegEnd::WARP_PUSH:
        take_warp_push(rt, leg.push, leg.dark || leg.dest_dark, leg.name);
        rt.facing = DPAD_NONE;
        break;
    case LegEnd::WARP_STEP:
        wait_warp_step(rt, leg.dark || leg.dest_dark, leg.name);
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

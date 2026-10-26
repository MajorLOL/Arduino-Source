/*  Pokemon FRLG Autostory Tools
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Building blocks for the Autostory: walking a generated leg (with wild
 *  and trainer battles on the way), taking warps, getting through
 *  cutscenes and conversations, healing and saving.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTools_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTools_H

#include <stdint.h>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "PokemonFRLG_AutoStoryScript.h"
#include "PokemonFRLG_AutoStoryBattle.h"

namespace PokemonAutomation{

template <typename Type> class ControllerContext;

namespace NintendoSwitch{

class ConsoleHandle;
class ProController;
using ProControllerContext = ControllerContext<ProController>;

namespace PokemonFRLG{


//  Everything the story steps share while the program runs.
struct StoryRuntime{
    StoryRuntime(ConsoleHandle& p_console, ProControllerContext& p_context)
        : console(p_console)
        , context(p_context)
    {}

    ConsoleHandle& console;
    ProControllerContext& context;

    StoryBattleSettings battle;
    StoryBattleMemory memory;

    //  After each step on a wild-encounter tile, how long to watch for a
    //  battle to start before taking the next step.
    Milliseconds encounter_check = std::chrono::milliseconds(1500);
    //  Same, in sight of a trainer who turns around (the trainer first
    //  walks up to the player).
    Milliseconds trainer_check = std::chrono::milliseconds(2500);

    //  Which way the player faces. DPAD_NONE if not known.
    DpadPosition facing = DPAD_NONE;

    //  Set when the lead fainted since the last heal.
    bool lead_fainted = false;

    uint64_t steps = 0;
    uint64_t wild_battles = 0;
    uint64_t trainer_battles = 0;
};


//  Run a battle that just started (or is about to start). Updates the
//  counters. If "throw_on_loss" and the party fainted, throws an
//  OperationFailedException so the program reloads the last save.
StoryBattleResult story_battle(
    StoryRuntime& rt, StoryBattleKind kind, const std::string& where,
    bool throw_on_loss = true
);


//  Walk one generated leg. Handles wild battles and turning trainers on the
//  way. For a warp leg, returns once the new map has faded in. For an
//  interact leg, returns facing the target. For a trigger leg, returns
//  standing on the trigger (a scene starts).
void walk_leg(StoryRuntime& rt, const StoryScript& script, int32_t leg_index);

//  One step, whichever way the player faces. "surf" for 8-frame steps.
void story_single_step(StoryRuntime& rt, DpadPosition direction, bool surf = false);

//  One step on a wild-encounter tile, then watch for a battle (and run it).
//  Returns the battle result, or NO_BATTLE.
StoryBattleResult story_checked_step(
    StoryRuntime& rt, DpadPosition direction, bool surf, bool trainer,
    StoryBattleKind wild_kind, const std::string& where,
    bool throw_on_loss = true
);

//  Turn to face "direction" without walking.
void story_face(StoryRuntime& rt, DpadPosition direction);


//  Get through a scene: B advances the text, YES/NO questions are answered
//  as asked (nickname questions always NO). Ends when the player can move
//  again (at least "min_time" after the start). Returns true if a battle
//  started instead (the caller runs it).
bool story_cutscene(
    StoryRuntime& rt,
    Milliseconds min_time, Milliseconds max_time,
    bool answer_yes, const std::string& what
);

//  Press A to talk to whatever the player faces, until a text box (or a
//  battle) shows up.
void story_talk_start(StoryRuntime& rt, const std::string& what);

//  Press A to talk, read until the YES/NO box and answer it.
void story_talk_answer(StoryRuntime& rt, bool yes, const std::string& what);

//  Talk to the nurse in front of the player (across the counter) and heal.
void story_heal(StoryRuntime& rt, const std::string& where);

//  Save from the overworld. "has_pokedex" picks the start menu layout.
void story_save(StoryRuntime& rt, bool has_pokedex);

//  The first rival battle (Oak explains the battle, so most detectors are
//  confused): use the first move until someone faints.
bool story_scripted_battle(StoryRuntime& rt, const std::string& opponent);


//  The text of the overworld text box (bottom of the screen), lower case.
std::string story_read_dialog(StoryRuntime& rt);

//  Close menus and text boxes until the player can move.
void story_back_to_overworld(StoryRuntime& rt, const std::string& where);


}
}
}
#endif

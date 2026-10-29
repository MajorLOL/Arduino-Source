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
#include <set>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "PokemonFRLG_AutoStoryScript.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryScroll.h"

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

    //  Check on the video how far the player really walked, and make up
    //  for missing steps.
    bool verify_walking = true;

    //  Which way the player faces. DPAD_NONE if not known.
    DpadPosition facing = DPAD_NONE;

    //  The last walked leg ended early: the trainer it went to talk to saw
    //  the player and came, and the battle is over.
    bool trainer_came = false;
    //  Moves of the APPROACH runs: in the leg, and really made.
    uint16_t approach_total = 0;
    uint16_t approach_made = 0;

    //  Team members caught (species slugs): in this run, and at the last
    //  save (a reset goes back to that).
    std::set<std::string> caught;
    std::set<std::string> caught_at_save;

    //  Set when the lead fainted since the last heal.
    bool lead_fainted = false;

    //  Save the game after every heal at a Pokemon Center.
    bool save_after_heal = true;

    //  Where the program is in the story, set for each step.
    std::string segment_id;
    size_t segment_steps = 0;       //  Steps of the running segment.
    std::string next_segment_id;    //  The segment after it ("" if none).
    bool has_dex = true;            //  The start menu has the POKeDEX entry.
    size_t step_index = 0;          //  The running step, in its segment.
    bool next_is_save = false;      //  The next step saves anyway.

    //  Set when the game was saved in the running segment: the step (in
    //  this segment) to continue from after a reset. SIZE_MAX if not.
    size_t resume_at = (size_t)-1;

    //  Memory use of the program, logged now and then.
    WallClock last_memory_log = WallClock::min();

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


//  The species of a catch step ("a,b,c").
std::set<std::string> story_species_list(const char* text);


//  Walk one generated leg. Handles wild battles and turning trainers on the
//  way. Each straight run is checked on the video (verify_walking): missing
//  steps are made again, steps too many walked back. For a warp leg,
//  returns once the new map has faded in (and its area picture is gone).
//  For an interact leg, returns facing the target. For a trigger leg,
//  returns standing on the trigger (a scene starts).
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

//  With the video check: a picture of the map around the player, once the
//  screen stands still, to tell later where the player stands compared to
//  now. False without the video check.
bool story_take_picture(StoryRuntime& rt, AutoStoryScroll::Screen& picture);

//  Where the player stands now compared to when "picture" was taken, in
//  tiles (x to the right, y down), up to 3 tiles away. False if the video
//  does not tell clearly (a text box, someone in the way, too far), or
//  without the video check. "now": the picture taken for this.
bool story_offset_from(
    StoryRuntime& rt, const AutoStoryScroll::Screen& picture, const std::string& where,
    int& dx, int& dy, AutoStoryScroll::Screen& now
);


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
//  Then save (unless the next step saves anyway). "mid_step": the running
//  step goes on afterwards and starts from here again after a reset
//  (training); otherwise a reset continues with the next step.
void story_heal(StoryRuntime& rt, const std::string& where, bool mid_step = false);

//  The game was just saved: after a reset the story continues from step
//  "resume_at" of the running segment. Logs where to start the program to
//  go on from this save, and stops the program here if it uses so much
//  memory that it could crash soon.
void story_saved(StoryRuntime& rt, size_t resume_at);

//  Log the memory the program uses (every few minutes, or now if "force").
void story_log_memory(StoryRuntime& rt, bool force = false);

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

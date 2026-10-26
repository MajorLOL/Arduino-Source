/*  Pokemon FRLG Autostory Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The bigger story steps: menus (party summary, bag, TM Case, Poke Mart,
 *  elevators), Strength boulders, the Vermilion Gym trash cans, grinding
 *  and catching.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryActions_H
#define PokemonAutomation_PokemonFRLG_AutoStoryActions_H

#include <string>
#include "PokemonFRLG_AutoStoryScript.h"
#include "PokemonFRLG_AutoStoryTools.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Read the level of the lead (party slot 1) from its summary.
//  Returns -1 if it could not be read. Starts and ends in the overworld.
int story_read_lead_level(StoryRuntime& rt);

//  Teach an HM from the TM Case. "hm_rank" is the position of the HM in
//  the TM Case (HMs come first), "party_slot" is 1-6. "forget_slot" is the
//  move (0-3) to replace if the Pokemon already knows 4 moves, -1 to pick
//  the weakest move of the lead.
void story_teach_hm(
    StoryRuntime& rt,
    int hm_rank, int party_slot, int forget_slot,
    const std::string& move
);

//  Facing a Poke Mart clerk: buy "quantity" of the item at "index" of the
//  list (as many as the money allows).
void story_buy(StoryRuntime& rt, int index, int quantity, const std::string& what);

//  Full Restore on the first Pokemon (Revive first if it fainted, and a
//  Full Restore on the second one too).
void story_heal_with_items(StoryRuntime& rt);

//  Facing an elevator panel: pick a floor. "from" is where the cursor
//  starts (the current floor), "to" the floor to pick.
void story_elevator(StoryRuntime& rt, int from, int to, const std::string& what);

//  Facing a boulder with Strength on: push it one tile.
void story_push_boulder(StoryRuntime& rt, DpadPosition direction);

//  Walk back and forth until a scene starts (Safari Zone: time's up).
void story_oscillate(StoryRuntime& rt, DpadPosition direction, Milliseconds max_time, const std::string& what);

//  The Vermilion Gym trash cans. Starts and ends at the hub tile.
void story_trash_can_puzzle(StoryRuntime& rt, const StoryScript& script, const StoryStep& step);

//  Grind in the grass until the lead reaches "level", healing every few
//  battles. Ends healed at the nurse (see StepType::GRIND).
void story_grind(
    StoryRuntime& rt, const StoryScript& script, const StoryStep& step,
    int extra_levels
);

//  Walk back and forth in the grass until one of the species is caught.
void story_catch(StoryRuntime& rt, const StoryStep& step);


}
}
}
#endif

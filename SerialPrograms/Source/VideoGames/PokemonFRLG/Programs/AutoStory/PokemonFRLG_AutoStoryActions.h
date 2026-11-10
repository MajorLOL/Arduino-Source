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

//  After a legendary Pokemon (the save right after it): REVIVE on party
//  slots 1 and 2, then FULL RESTORE on both, as far as the bag has them.
//  Who fainted is not known after a reload; on one who did not, a REVIVE
//  has no effect and is not used up.
void story_heal_after_legendary(StoryRuntime& rt);

//  An ESCAPE ROPE from the bag: out of the cave or building, to where it was
//  entered from outdoors (the spin in ends facing down). False if the bag
//  has none (the story walks out instead).
bool story_escape_rope(StoryRuntime& rt);

//  A MAX REPEL (or any REPEL) from the bag. Without one: a warning, the walk
//  goes on with the wild Pokemon.
void story_use_repel(StoryRuntime& rt);

//  The level of a party slot (1-6), from its summary (-1 if not read), and
//  its species (slug, "" if not read).
int story_read_slot(StoryRuntime& rt, int slot, std::string* species);

//  Swap two party slots in the overworld party menu (a < b).
void story_swap_party(StoryRuntime& rt, int a, int b);

//  Shinies caught on the way join at the end of the party, and a team member
//  that joins after them lands behind them. Read the party and swap the
//  team (StoryRuntime::team, in slot order) back into its slots. Only when
//  there are such shinies and the team was not found complete in its slots
//  already, or "force".
void story_fix_party_order(StoryRuntime& rt, bool force);

//  The starter must lead (party slot 1). A run stopped while a team member
//  trained in front, then started again at a later segment, leaves it out
//  of order: find the starter and swap it back to slot 1.
void story_check_lead(StoryRuntime& rt);

//  Use an item from the bag's ITEMS pocket (found by a word of its
//  description) on a party slot (1-6): an evolution stone.
//  give: GIVE (to hold) in place of USE; party slot 1 is the starter then.
void story_use_item(StoryRuntime& rt, int party_slot, const std::string& words, const std::string& what, bool give = false);

//  Use up to "count" Rare Candies from the bag on party slot 1, in the
//  overworld. A move it wants to learn is not learned. Returns how many
//  were used.
int story_use_rare_candies(StoryRuntime& rt, int count);

//  Party slot "party_slot" knows FLY (looked up in its party menu actions).
bool story_knows_fly(StoryRuntime& rt, int party_slot);

//  Outdoors: Fly with the Pokemon in "party_slot" to "destination"
//  (KantoFlyLocation). Lands in front of that town's Pokemon Center.
void story_fly(StoryRuntime& rt, int party_slot, int destination, const std::string& what);

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

//  A field move from the party menu (CUT on the braille of the Dotted Hole),
//  facing "facing" first.
void story_field_move(StoryRuntime& rt, int party_slot, const std::string& move, DpadPosition facing, const std::string& what);

//  The roaming legendary (StepType::ROAM): back and forth in the grass of a
//  route until it comes (the MASTER BALL), into a building and out now and
//  then (it moves to another route at each warp).
void story_roam(StoryRuntime& rt, const StoryScript& script, const StoryStep& step);

//  For the POKeDEX (StepType::DEX): back and forth in the grass (cave,
//  water) of a place, catching the species of the step not caught yet with
//  the cheapest balls; the others are run from.
void story_dex(StoryRuntime& rt, const StoryStep& step);

//  The Seagallop from Vermilion City (StepType::SEAGALLOP), the player on
//  the pier's trigger: through the sailor's text (on the Switch the MYSTIC
//  and AURORA TICKETs first, and their menu: SEVII ISLANDS) to row "row"
//  of the Sevii Islands list (0: One Island). The ferry scene is the next
//  step.
void story_seagallop(StoryRuntime& rt, int row, const std::string& what);


}
}
}
#endif

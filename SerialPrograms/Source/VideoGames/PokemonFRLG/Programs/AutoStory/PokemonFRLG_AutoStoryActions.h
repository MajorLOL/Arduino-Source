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

#include <functional>
#include <string>
#include <utility>
#include <vector>
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
//  "kind": what it is, for the program's count of the bag (1 REVIVE, 2 FULL
//  RESTORE or MAX POTION, 0 anything else). "up_to": only what is missing
//  up to "quantity" of that kind (not known how many the bag has: a look in
//  it; none bought if nothing is missing).
void story_buy(
    StoryRuntime& rt, int index, int quantity, const std::string& what,
    int kind = 0, bool up_to = false
);

//  Between two battles, with items from the bag. The team (team play): a
//  REVIVE for everyone who is down, then FULL RESTOREs (or MAX POTIONs):
//  "mode" 0, the first League: for everyone below 60% of its HP;
//  "mode" 2, the League again (for prize money, which the items cost): for
//  the strongest below 50%, the others below 25%; "mode" 3, the same before
//  the Champion: below 70% and below 50%. The strongest first.
//  Without team play: a Full Restore on the first Pokemon (a Revive first
//  if it fainted) and on the second one.
void story_heal_with_items(StoryRuntime& rt, int mode = 0);

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

//  A party slot's summary, pages 1 and 2 (build 57, HITMONTOP by vitamins):
//  the species (slug), level, nature, and the Attack and the Defense as
//  they stand. Both pages are read twice and must agree. False: not read.
struct StoryStats{
    std::string species;
    std::string nature;
    int level = -1;
    int attack = -1;
    int defense = -1;
};
bool story_read_stats(StoryRuntime& rt, int party_slot, StoryStats& stats);

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
//  (words: "a|b" for one of several.)
//  "tolerant": no such item in the bag is no error (false comes back, the
//  bag is closed); true: the item was used or given.
//  "keep_order": the party is as it is (a Pokemon of the boxes trains in
//  front): it is not put back in the team's order first.
bool story_use_item(
    StoryRuntime& rt, int party_slot, const std::string& words, const std::string& what,
    bool give = false, bool tolerant = false, bool keep_order = false
);
//  The party menu: the item party slot "party_slot" holds goes back to the
//  bag. False: it holds none ("is not holding anything"). An error when the
//  menu is not what it is expected to be.
bool story_take_item(StoryRuntime& rt, int party_slot, const std::string& label);

//  Evolving by level (build 43, experimental; PokemonFRLG_AutoStoryBoxScan.cpp
//  and PokemonFRLG_AutoStoryActions.cpp). The species of the boxes that
//  evolve by a level into one the POKeDEX table still wants, the one with
//  the fewest levels to go first.
std::vector<AutoStoryBoxes::LevelTarget> story_level_targets(StoryRuntime& rt);
//  What a trip of StepType::LEVEL_OUT to LEVEL_IN is for (the step's p1).
enum class LevelMode : int{
    LEVEL = 0,          //  Trained at Victory Road until its level (build 43).
    FRIENDSHIP = 1,     //  Walked with, then trained one level (build 45).
    EXP = 2,            //  Through the Elite Four with the EXP. SHARE on it (build 46).
};
//  In front of a PC, facing it, with a party of six whose last one is not of
//  the team (StepType::LEVEL_OUT): the first of those changes places with the
//  party's sixth one (StoryRuntime::out_*, out_becomes, out_goal say which).
//  "changed" is called once it is done. False, and nothing pressed, when
//  there is none.
//  FRIENDSHIP (build 45): the one of those that evolve by friendship, with
//  the fewest blocks of steps still to walk (story_friend_targets()); its goal
//  is its level plus one.
//  EXP (build 46): the one of those with the most experience points to go
//  (story_exp_targets()), only when it is still that far on its real level.
bool story_level_out(
    StoryRuntime& rt, const std::string& label, LevelMode mode, const std::function<void()>& changed
);
//  The Elite Four for experience (build 46, experimental): the species of the
//  boxes that evolve by a level into one the POKeDEX table still wants and
//  that have AutoStoryBoxes::EXP_ROUND_MIN points to go or more, the one with
//  the most first.
std::vector<AutoStoryBoxes::LevelTarget> story_exp_targets(StoryRuntime& rt);
//  The EXP. SHARE from the party slot "from_slot" to "to_slot" (the lead to
//  the Pokemon that rides through the League, and back). Not essential: when
//  the menus are not what they are expected to be, it is logged and the trip
//  goes on as it is.
void story_exp_share(StoryRuntime& rt, int from_slot, int to_slot, const std::string& label);
//  Evolving by friendship (build 45, experimental): the species of the boxes
//  that evolve by a level up with high friendship, the one with the fewest
//  blocks of steps still to walk first (rt.friend_records says what was
//  walked).
std::vector<AutoStoryBoxes::LevelTarget> story_friend_targets(StoryRuntime& rt);
//  Back at the PC (StepType::LEVEL_IN): what the sixth place holds goes into
//  the cell, the party's own sixth one comes back; an evolved species is one
//  more in the table; the game is saved. "changed" is called after that.
void story_level_in(StoryRuntime& rt, const std::string& label, const std::function<void()>& changed);
//  The training (StepType::RAISE): the one in the party's sixth place
//  trains in front of the lead, with the EXP. SHARE, in the grass of the
//  step (as StepType::GRIND), until it is at StoryRuntime::out_goal. Ends
//  healed at the nurse, the party in its order, the EXP. SHARE on the lead.
void story_raise(StoryRuntime& rt, const StoryScript& script, const StoryStep& step);

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
//  length >= 3: a line of that many tiles with a wall at both ends, run
//  along (B held) each way until bumping into the wall.
void story_oscillate(StoryRuntime& rt, DpadPosition direction, int length, Milliseconds max_time, const std::string& what);
//  StepType::PACE (build 45): the Pokemon out of its box for its friendship
//  (rt.out_*, out_friend) walks back and forth along a line of "length" tiles
//  between two walls, run along as story_oscillate() does, until its blocks
//  of 128 steps are the ones its evolution wants (AutoStoryBoxes::
//  friend_blocks_wanted). The steps are counted, the position checked on the
//  video at each end of a round, the game saved every 40 blocks and at the
//  end; "on_saved" is called after each save.
void story_pace(
    StoryRuntime& rt, DpadPosition direction, int length, const std::string& what,
    const std::function<void()>& on_saved
);

//  How many Pokemon (eggs too) the party has: the party menu, its last
//  occupied slot (build 47).
int story_party_count(StoryRuntime& rt);

//  StepType::HATCH (build 47, experimental): with an egg in the party's last
//  place, back and forth along a line of "length" tiles between two walls
//  (run along, B held, as story_pace() does) until a text box or a black
//  screen comes up: the hatching. The scene is read through with B (the
//  nickname is declined), what the last place holds is looked at (the
//  summary), and the player goes back to where the line starts. Returns the
//  species that hatched (what it reads as). An error (the game is then
//  reset) when the party does not have the Pokemon that EGG_ROOM and the
//  egg's handing over make it, when the walk reaches "max_steps" steps, or
//  when the scene does not come. "species": what is expected (a different
//  one is logged, not an error).
std::string story_hatch(
    StoryRuntime& rt, DpadPosition direction, int length, int max_steps, const std::string& species,
    const std::string& what
);

//  The Celadon Game Corner (build 56, experimental; StepType::COINS, see its
//  steps in PokemonFRLG_AutoStoryScript.h): the packs of COINS at the clerk
//  (A, A, the list: DOWN for the 500, A, A: the same keys whether the pack is
//  bought or the clerk says why not), the games of a slot machine (R for the bet
//  of 3, the reels stopped with A, START for the payout; the machine's own
//  bias decides what comes, whatever the timing is), and the prizes' clerk,
//  who is asked for the species of "text": a YES/NO box of a nickname that
//  comes by itself says that the COINS were enough (the fanfare, "obtained
//  the PORYGON!", the nickname question; NO), else the clerk says that more
//  are needed. When the prize was given: one more in the POKeDEX table.
void story_coins(StoryRuntime& rt, const StoryStep& step, const std::string& label);

//  The Four Island Day Care (build 48, experimental; StepType::DAYCARE_OUT to
//  DAYCARE_END, a trip that has no save in it).
//  story_daycare_walk(): "steps" steps along a line of "length" tiles between
//  two walls (as story_pace()), an even number of legs: it ends where it
//  started. story_daycare_give(): in front of the woman across the counter,
//  with the parents in the party's fifth and sixth place: both are given to
//  her (A, YES, the party menu, STORE; "one more?" YES, again). The party has
//  the team then (an error when it does not, or when a member of the team
//  went in the Day Care). story_daycare_man(): in front of the man outside
//  (in "alt" the other tile he may stand on): the egg when he has one (YES;
//  true, the party has it in its last place); with "reject": an egg that is
//  waiting is not wanted (NO, NO; true when there was one). false: no egg.
//  story_daycare_take(): the woman gives both back (what it costs is paid,
//  the party has the team and the two parents again).
void story_daycare_walk(
    StoryRuntime& rt, DpadPosition direction, int length, int steps, const std::string& what
);
void story_daycare_give(StoryRuntime& rt, const std::string& label);
bool story_daycare_man(StoryRuntime& rt, const std::string& label, bool reject, const StoryTalkAlt& alt);
void story_daycare_take(StoryRuntime& rt, const std::string& label);

//  The Vermilion Gym trash cans. Starts and ends at the hub tile.
void story_trash_can_puzzle(StoryRuntime& rt, const StoryScript& script, const StoryStep& step);

//  Grind in the grass until the lead reaches "level", healing every few
//  battles. Ends healed at the nurse (see StepType::GRIND).
void story_grind(
    StoryRuntime& rt, const StoryScript& script, const StoryStep& step,
    int extra_levels
);

//  Experimental (the Level Trainer): the training of a GRIND step of the
//  story ("place": its routes between the nurse and the grass, its grass
//  line) for whatever Pokemon is in party slot 1, up to "level". Starts in
//  front of that place's nurse and ends there, healed. No team member, no
//  Rare Candies.
void story_train_at(StoryRuntime& rt, const StoryScript& script, const StoryStep& place, int level);

//  Experimental (the Level Trainer): training where the player stands, on a
//  line of wild-encounter tiles from the player's tile. No nurse: it stops
//  when the lead is low on HP, out of PP or fainted.
struct StoryTrainHere{
    DpadPosition forth = DPAD_UP;   //  The line, from the player's tile.
    int length = 3;                 //  Its tiles, the player's included (2 to 5).
    int level = 100;                //  Train party slot 1 up to this level.
    double stop_hp = 0.35;          //  Stop below this part of its HP (0: never).
    uint64_t max_battles = 0;       //  Stop after this many battles (0: no limit).
    bool run = false;               //  The walks may be run (Running Shoes).
};
enum class StoryTrainEnd{
    LEVEL_REACHED,
    LOW_HP,
    OUT_OF_PP,
    LEAD_FAINTED,
    WHITED_OUT,         //  The party fainted: the player is at the last Pokemon Center.
    BATTLE_LIMIT,
};
//  "level": the lead's level at the end (-1 if it was never read).
StoryTrainEnd story_train_here(StoryRuntime& rt, const StoryTrainHere& options, int& level);

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
//  the cheapest balls; the others are run from. With p2 & 4: fishing, the
//  rod p4 cast from where the player stands until the same ends
//  (PokemonFRLG_AutoStoryFishing.h). With p2 & 1 and & 4 (build 52): in a
//  Safari Game; the visit ends as any other, and the game is left early by
//  the walk of legs_b to the gate (a game that ends by itself is read through
//  by the segment's next step).
void story_dex(StoryRuntime& rt, const StoryScript& script, const StoryStep& step);

//  For the POKeDEX, laps (StepType::DEX with p2 & 8): the steps after the
//  step are a lap that ends where it starts (walks over water, rocks
//  smashed), and the segment's loop plays them again and again. What the
//  table wants of the step's list is caught in the wild battles on the way
//  (the others are run from).
//    story_laps_begin(): the list is set up. False: nothing to catch there
//        (the lap's steps are skipped).
//    story_laps_again(): after the last step of a lap: another one? Not
//        when nothing looked for is left, p0 battles in a row brought no
//        catch, p1 minutes are over, the balls ran out, the lead is low.
//    story_laps_end(): the battle settings as before, and what the laps
//        brought ("failed": a step of the lap failed, no summary).
bool story_laps_begin(StoryRuntime& rt, const StoryStep& step);
bool story_laps_again(StoryRuntime& rt);
void story_laps_end(StoryRuntime& rt, bool failed = false);

//  The PC's boxes read (StepType::BOXES, experimental), the player in
//  front of the PC of a Pokemon Center and facing it: BILL's PC, MOVE
//  POKeMON, every cell of the 14 boxes looked at (the species and the level
//  of what is in it, with the readers of Pokemon Automation's Box Sorter),
//  out again. A is not pressed in the box view: nothing in the boxes
//  changes, and the PC is left on the box it was on. What was read is in
//  StoryRuntime::boxes (boxes_readings counts it, also when an error comes
//  after it). False: the boxes were read, but their round did not close (a
//  box may have been passed over or read twice). An error (the game is then
//  reset) when the box view does not open, the hand is lost, the PC would
//  be left on another box, or the player does not get free again.
//  (PokemonFRLG_AutoStoryBoxScan.cpp)
bool story_boxes(StoryRuntime& rt, const std::string& label);

//  Evolution stones (build 35, experimental; PokemonFRLG_AutoStoryBoxScan.cpp).
//  The rows of the POKeDEX table as far as evolving goes, and what can be
//  evolved with a stone: the species the table does not have, whose earlier
//  form lies in a box (StoryRuntime::boxes), without the stones known to be
//  missing (StoryRuntime::stones_out).
std::vector<AutoStoryBoxes::TableRow> story_evolve_table(StoryRuntime& rt);
std::vector<AutoStoryBoxes::StoneTarget> story_stone_targets(StoryRuntime& rt);
//  A look in the bag for each stone they take that is not sold (MOON STONE,
//  SUN STONE): without one, its targets are left out. Returns what stays.
std::vector<AutoStoryBoxes::StoneTarget> story_stones_not_sold(StoryRuntime& rt, const std::string& label);
//  A look in the bag for each stone they take that is sold; what is still
//  to buy (one in the bag is counted as one), and that as text.
std::vector<std::pair<std::string, int>> story_stones_to_buy(StoryRuntime& rt);
std::string story_stones_text(const std::vector<std::pair<std::string, int>>& stones);
//  In front of the clerk of the Celadon Department Store's fourth floor:
//  what is still to buy, as far as the money goes.
void story_buy_stones(StoryRuntime& rt, const std::string& label);
//  In front of the PC of a Pokemon Center, facing it, with a party of six
//  whose last one is not of the team. For each target with its stone in the
//  bag: the Pokemon of the box changes places with the party's sixth, gets
//  the stone, is looked at (it must be the evolved species), changes back,
//  and the game is saved (to go on at this step). "changed" is called after
//  each of them. An error (the game is then reset, nothing of the round
//  under way is saved) when anything is not as expected. Returns how many
//  evolved.
int story_stones(StoryRuntime& rt, const std::string& label, const std::function<void()>& changed);

//  A Pokemon of a box into the party for a while (build 36, experimental).
//  In front of a PC, facing it. story_box_out(): one "give" of the boxes
//  (the lowest level, and not the one the list of what evolves has set
//  aside) changes places with the party's sixth one, which must not be of
//  the team; false, and nothing pressed, when there is none. "trade_get": the
//  species the segment's trade gives. If the cell holds that one (the trade
//  was made before: by hand, or in a run whose list was not kept), nothing is
//  pressed either: the list and the POKeDEX table take it, and false comes
//  back with StoryRuntime::out_made_before set. story_box_in():
//  what the sixth place holds now (StoryRuntime::out_now) goes into that
//  cell, the party's own sixth one comes back. The game must not be saved
//  between the two.
bool story_box_out(
    StoryRuntime& rt, const std::string& label, const std::string& give, const std::string& trade_get
);
void story_box_in(StoryRuntime& rt, const std::string& label);

//  Room for an egg (build 47, experimental). In front of a PC, facing it.
//  story_egg_room(): with fewer than six Pokemon in the party there is room
//  (nothing is pressed); else the sixth one, which must not be of the team,
//  goes into an empty cell of the boxes (from the last box back: new catches
//  fill the first ones; each cell is looked at on the screen first). An error
//  (the game is then reset) when it cannot. story_egg_back(): what the
//  party's sixth place holds now (the Pokemon that hatched, StepType::HATCH)
//  goes into that cell and the one that was put away comes back. Neither
//  may have a save between them.
void story_egg_room(StoryRuntime& rt, const std::string& label);
void story_egg_back(StoryRuntime& rt, const std::string& label);

//  The Day Care (build 48, experimental). In front of a PC, facing it.
//  story_daycare_out(): a DITTO and a Pokemon of the family of "baby" (the
//  ones that give its egg with a DITTO) come into the party's fifth and sixth
//  place, from the boxes (the ones that were there go into their cells;
//  a parent that is there already stays). False (nothing changed): the party
//  does not have six Pokemon, or the boxes lack a parent. story_daycare_store():
//  what hatched (the party's last place, the team and the baby) goes into an
//  empty cell of the boxes.
bool story_daycare_out(StoryRuntime& rt, const std::string& label, const std::string& baby);
void story_daycare_store(StoryRuntime& rt, const std::string& label);
//  Facing someone who trades in the game ("Do you have a SPEAROW? Want to
//  trade it for my FARFETCH'D?"): YES, the party's sixth one, the trade's
//  scene, and a look at what the sixth is now: it must be "get" (one more
//  in the POKeDEX table). "holds_mail": what the trade gives holds a MAIL
//  (the JYNX of Cerulean City: a FAB MAIL), which is taken off it (see
//  story_take_mail()) before it is put into a box. False when the trader
//  asks nothing (the trade was made before) or nothing was taken out of a
//  box for it.
bool story_trade(StoryRuntime& rt, const std::string& label, const std::string& get, bool holds_mail);

//  In the overworld: the MAIL that the Pokemon in party slot "party_slot"
//  holds goes to the PC's mailbox (party menu: the Pokemon's actions, MAIL,
//  TAKE MAIL, YES). The box view of the PC does not take a Pokemon with a
//  MAIL, and does not even close the party with one in the hand ("Please
//  remove the MAIL."). Throws (nothing is saved) when a text on the way
//  is not the one the game shows at that point, or the mailbox is full.
void story_take_mail(StoryRuntime& rt, int party_slot, const std::string& label);

//  Is an item in the bag (StepType::SKIP_IF_ITEM)? Opens the bag, goes
//  through "pocket" (0 ITEMS, 1 KEY ITEMS, 2 POKe BALLS) reading the
//  descriptions, closes it again. "rod" 0 to 2: that fishing rod, by its
//  own words (and StoryRuntime::rods_in_bag keeps it); else "words": one
//  of them enough, "a|b", and "!c": not with c.
bool story_bag_has(StoryRuntime& rt, int pocket, int rod, const std::string& words, const std::string& what);

//  The Seagallop from Vermilion City (StepType::SEAGALLOP), the player on
//  the pier's trigger: through the sailor's text (on the Switch the MYSTIC
//  and AURORA TICKETs first, and their menu: SEVII ISLANDS) to row "row"
//  of the Sevii Islands list (0: One Island), until his text box is gone.
//  His text is blue and not read: for row 0 every page and menu gets A
//  (AutoStoryFerryTalk). The ferry scene is the next step.
//  Build 54 (experimental): "row" 101 and 102 are rows of the tickets' menu
//  itself (NAVEL ROCK, BIRTH ISLAND): the ferry goes there at once.
void story_seagallop(StoryRuntime& rt, int row, const std::string& what);

//  Prize money from a trainer who battles again (StepType::REMATCH), the
//  player next to the trainer and facing it. In turns: the step's walk (100
//  steps or more, back to where it starts: the VS SEEKER charges with the
//  steps), the VS SEEKER from the KEY ITEMS pocket, A on the trainer. That
//  is a battle 7 times in 10 (and the first time, with a trainer not
//  beaten before), a line of text otherwise. Until p0 battles are won, p1
//  minutes are over, or nothing comes of it (no VS SEEKER in the bag; eight
//  uses in a row without a battle). Returns the battles won, which are
//  added to StoryRuntime::trip_wins.
int story_rematch(StoryRuntime& rt, const StoryScript& script, const StoryStep& step);


}
}
}
#endif

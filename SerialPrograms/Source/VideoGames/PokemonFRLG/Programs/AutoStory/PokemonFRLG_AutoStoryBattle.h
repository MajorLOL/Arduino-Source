/*  Pokemon FRLG Autostory Battles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Battles for the Autostory: pick the move that does the most damage
 *  against the opponent (move names and the opponent are read with OCR,
 *  types and power come from the decomp data), decide which moves to learn,
 *  send in the next Pokemon when the lead faints, run, catch.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattle_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattle_H

#include <stdint.h>
#include <set>
#include <string>
#include <vector>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "PokemonFRLG_AutoStoryDex.h"

namespace PokemonAutomation{

template <typename Type> class ControllerContext;

namespace NintendoSwitch{

class ConsoleHandle;
class ProController;
using ProControllerContext = ControllerContext<ProController>;

namespace PokemonFRLG{


enum class StoryBattleKind{
    TRAINER,        //  Fight until it ends.
    WILD,           //  Wild battle: fight or run, as the settings say.
    WILD_FIGHT,     //  Wild battle that has to be won (the Marowak ghost).
    WILD_RUN,       //  Run away (Snorlax).
    CATCH,          //  Catch one of the wanted species, run from the others.
};

enum class StoryBattleResult{
    WON,            //  The battle ended and we did not run.
    RAN,
    CAUGHT,
    LEAD_FAINTED,   //  The lead fainted (the battle may have gone on with the next one).
    NO_BATTLE,      //  No battle showed up.
    LOST,           //  The whole party fainted ("... whited out!").
};

//  The game must be in English: move names, the opponent and the battle text
//  are read with English OCR.
//  A member of the team (party slot order outside of battle). For trainer
//  battles where the team plays together (the Elite Four).
struct StoryTeamMember{
    std::set<std::string> line;     //  slugs of its evolution line
    AutoStoryDex::Type type1;       //  its types (the final evolution's)
    AutoStoryDex::Type type2;
    std::vector<AutoStoryDex::Type> attacks;    //  types its moves hit with
    double strength = 1.0;          //  higher level than the others: more
    bool fights = true;             //  false: only carried along (Cut helper)
};

struct StoryBattleSettings{
    bool wild_run = false;

    //  Types of the lead (same-type attack bonus).
    AutoStoryDex::Type lead_type1 = AutoStoryDex::Type::NORMAL;
    AutoStoryDex::Type lead_type2 = AutoStoryDex::Type::NORMAL;

    //  For StoryBattleKind::CATCH: Pokemon Automation slugs. Also caught
    //  when met in a StoryBattleKind::WILD battle.
    std::set<std::string> catch_species;
    uint16_t catch_balls = 5;

    //  Not empty: trainer battles are fought by the whole team. The best
    //  answer to each opponent comes in (free when the game asks "Will
    //  PLAYER change POKeMON?", and when one faints).
    std::vector<StoryTeamMember> team;

    //  Training a weak team member in front (StoryBattleKind::WILD_FIGHT):
    //  it is sent out first, so it gets half the experience, then the
    //  Pokemon at this battle position (1-5) comes in and fights. 0: no.
    int share_position = 0;
    //  The types of the one that comes in, and the member's evolution line
    //  (slugs: to know when the member is the one learning a move).
    AutoStoryDex::Type share_type1 = AutoStoryDex::Type::NORMAL;
    AutoStoryDex::Type share_type2 = AutoStoryDex::Type::NORMAL;
    std::set<std::string> share_members;
};

//  What the battle logic remembers between battles.
struct StoryBattleMemory{
    //  The lead's moves in menu order, as indexes into AutoStoryDex::moves().
    //  -1 = not known (or empty). Read from the FIGHT menu every turn.
    int moves[4] = {-1, -1, -1, -1};
    uint64_t battles = 0;
    uint64_t lead_faints = 0;
    //  Set when "... grew to LV. X!" was seen. The grinding clears it after
    //  reading the lead's level.
    bool level_up_seen = false;
    //  Moves found to be out of PP (bit per slot): picking them just brings
    //  the move list back ("There's no PP left for this move!"). Cleared
    //  when the party is healed.
    uint8_t no_pp = 0;
    //  The slot picked last, -1 if none in this battle.
    int last_slot = -1;
    //  How many times in a row the move list came back after picking
    //  "last_slot" (a press the game missed brings it back once).
    int list_back = 0;
    //  The species caught in the last battle ("" if none).
    std::string caught;
    //  Shared experience: the moves of the member that stayed out, and the
    //  last "X is trying to learn Y." text (who learns).
    int member_moves[4] = {-1, -1, -1, -1};
    std::string learner;
    //  Team play: the members (team indexes) that fought in the last
    //  battle, and those that fainted.
    std::set<int> team_used;
    std::set<int> team_fainted;
};


//  Run a battle from its start (intro text included) until the player is
//  back in the overworld. Throws OperationFailedException if the screen is
//  not understood for a long time.
StoryBattleResult run_story_battle(
    ConsoleHandle& console, ProControllerContext& context,
    StoryBattleKind kind,
    const StoryBattleSettings& settings,
    StoryBattleMemory& memory
);

//  Decide whether to learn "new_move" instead of one of "moves".
//  Returns the slot (0-3) to forget, or -1 to skip the new move.
//  HM moves are never forgotten.
int choose_move_to_forget(
    const int moves[4], int new_move,
    const StoryBattleSettings& settings
);

//  The value of a move without knowing the opponent (power, accuracy,
//  same-type bonus). Used to decide what to learn.
double move_value(int move, const StoryBattleSettings& settings);

//  Read the (English) text in a box of the game screen with OCR (lower case,
//  lines joined by spaces). "bright": white text on a dark background
//  (battle text), otherwise dark text on a light background.
std::string read_story_text(
    ConsoleHandle& console, const ImageFloatBox& box, bool bright
);


}
}
}
#endif

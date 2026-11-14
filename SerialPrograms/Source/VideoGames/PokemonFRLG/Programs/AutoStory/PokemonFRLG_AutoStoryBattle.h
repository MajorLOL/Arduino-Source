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
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
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
    OUT_OF_BALLS,   //  A legendary Pokemon and no ball left in the bag: the battle is
                    //  still on (the caller loads the save in front of it).
};

//  Listens for the shiny sound the whole time the program runs (an audio
//  inference session of its own). The battle code starts looking only once
//  the battle is noticed, and that can be after the intro: the transition,
//  the wild Pokemon sliding in and its sparkles (in a cave in the dark only
//  the battle text shows a battle).
class StoryShinyWatch{
public:
    StoryShinyWatch(ConsoleHandle& console, ProControllerContext& context);
    ~StoryShinyWatch();

    //  The shiny sound was heard between "from" and "to", and after the last
    //  consume().
    bool heard(WallClock from, WallClock to) const;
    //  What was heard so far belongs to a battle that is over.
    void consume();
    //  Sound came in at all: the capture card's audio input is set.
    bool has_sound() const;
    //  When it started listening, and since when with the same detector.
    WallClock started() const{ return m_started; }
    WallClock detector_started() const{ return m_detector_started; }
    //  Start over with a new detector: while nothing comes close to the
    //  sound, the detector keeps a history of its scores.
    void restart();

private:
    struct Data;
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    WallClock m_started;
    WallClock m_detector_started;
    bool m_had_sound = false;
    mutable std::mutex m_lock;
    std::vector<WallClock> m_heard;     //  the last few times
    WallClock m_consumed = WallClock::min();
    std::unique_ptr<Data> m_data;
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
    //  The legendary Pokemon of a legendary step (its slug): the scene says
    //  which it is, whatever the name reads as ("MEHTHO": MEOWTH as likely
    //  as MEWTWO).
    std::string legendary_species;

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
    //  The member fights alone, but against a foe it needs many turns for
    //  (Electrode against Onix in Victory Road: SPARK does nothing, SWIFT a
    //  twentieth of its HP), the Pokemon at this battle position still
    //  comes in. 0: never.
    int slow_share_position = 0;
    //  The types of the one that comes in, and the member's evolution line
    //  (slugs: to know when the member is the one learning a move).
    AutoStoryDex::Type share_type1 = AutoStoryDex::Type::NORMAL;
    AutoStoryDex::Type share_type2 = AutoStoryDex::Type::NORMAL;
    std::set<std::string> share_members;

    //  The species and level of the one that fights, if known (training):
    //  the moves are then valued by the damage they do with its stats
    //  against the foe's (a Voltorb's TACKLE does less than SONICBOOM),
    //  the foe taken to be about as strong. Not known: by power and type.
    const AutoStoryDex::SpeciesInfo* lead_species = nullptr;
    int lead_level = 0;

    //  Not null: a wild Pokemon whose shiny sound played in the intro is
    //  caught with the best ball in the bag (the Safari Zone: SAFARI BALLs),
    //  until it is caught, gets away or the balls run out. "shiny_catch"
    //  false: only noticed (no room for it in the party yet).
    const StoryShinyWatch* shiny_watch = nullptr;
    bool shiny_catch = true;
    //  A legendary Pokemon: caught with the best ball in the bag until the
    //  balls run out. The Master Ball only with "legendary_master" (a shiny
    //  never gets it): it is kept for the roaming legendary, which flees at
    //  once.
    bool catch_legendary = false;
    bool legendary_master = false;

    //  A scene follows the battle without the overworld coming back (the
    //  Champion: Oak, the Hall of Fame, the credits): the battle is over
    //  once overworld text shows after it. The next step goes through it.
    bool scene_after = false;
    //  No Master Ball for it: its HP is brought down first with the weakest
    //  move, as long as one more hit surely leaves it some (watched on its
    //  HP bar). Catch rate 3: at full HP an ULTRA BALL catches it about 0.8%
    //  of the time, at a fifth of its HP about 2%.
    bool weaken_legendary = false;

    //  For the POKeDEX: a wild Pokemon of these species (slugs; not caught
    //  yet) is caught with the cheapest ball in the bag (POKe BALL first,
    //  then GREAT, then ULTRA; never the Master Ball), up to
    //  "dex_max_throws" balls a battle, at full HP (the team would knock it
    //  out). In the Safari Zone: SAFARI BALLs until it is caught or flees.
    //  The others are run from (StoryBattleKind::CATCH).
    std::set<std::string> dex_species;
    int dex_max_throws = 20;
    //  All species of the DEX step, caught or not as the program knows: the
    //  game's ball mark decides for those met (StoryBattleMemory::foe_mark).
    std::set<std::string> dex_all;
    //  Of dex_species: more than one wanted (the others to evolve): caught
    //  even when the game shows the ball mark (one is caught already).
    std::set<std::string> dex_extra;
    //  Only shiny ones of dex_species (the collection's "Shiny only"): the
    //  others are run from, the ball mark does not matter.
    bool dex_shiny_only = false;

    //  The roaming legendary (StepType::ROAM): met, it gets the MASTER BALL
    //  at once (it flees on its first turn; a ball goes before any move).
    std::set<std::string> roamer_species;

    //  Trainer battles: below a third of its HP, the Pokemon that is out
    //  gets the best healing item of the bag (Full Restore, Max Potion,
    //  Hyper/Super Potion...). In the League, played as one gauntlet (the PP
    //  do not come back between the rooms), the Champion is beaten far more
    //  often (team_sim --gauntlet). Off in the League again (chapters 13
    //  and 15), which has four of them, not fifteen: there they are worth
    //  more between the rooms (league_again_sim.py).
    bool heal_in_battle = true;

    //  Experimental (the option "Faster battles"): the end of a battle is
    //  looked for when the screen fades to black, not only after 6 seconds
    //  without anything of a battle on it. The game fades out at the end of
    //  every battle (battle_main.c: HandleEndTurn_FinishBattle); once the
    //  picture is back and nothing of a battle shows, one press of + tells:
    //  the start menu opens only where the player is free to move. Whatever
    //  else it finds there (an evolution, text, a whiteout) is left to the
    //  loop as before. Not while catching, and not once the party fainted.
    bool fast_end = false;
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
    //  How many times that text was seen (of any Pokemon of the party).
    uint64_t level_ups = 0;
    //  Moves found to be out of PP (bit per slot): picking them just brings
    //  the move list back ("There's no PP left for this move!"). Cleared
    //  when the party is healed.
    uint8_t no_pp = 0;
    //  Moves that do nothing to the foe (bit per slot): an ability absorbs
    //  them (WATER ABSORB, FLASH FIRE; also on a DITTO that took the form of
    //  the player's Pokemon) or the foe is immune. Left out until the foe
    //  changes. A turn without damage alone does not tell: the player's
    //  Pokemon may not have moved (asleep, paralyzed, confused) or missed.
    //  So after such a turn another move is tried once ("probe"); only if
    //  that one (or any other) did damage and the first one again did not,
    //  the first one does nothing.
    uint8_t useless = 0;
    uint8_t probe = 0;              //  left out for the next pick only
    uint8_t no_damage[4] = {0, 0, 0, 0};
    int damage_turns = 0;           //  turns in which the foe lost HP
    int damage_turns_at[4] = {0, 0, 0, 0};  //  ... when a slot first did nothing
    double foe_hp = -1;             //  the foe's HP bar when the last move was picked
    const AutoStoryDex::SpeciesInfo* foe_for_hp = nullptr;

    void reset_foe(){
        useless = 0;
        probe = 0;
        damage_turns = 0;
        for (int i = 0; i < 4; i++){
            no_damage[i] = 0;
            damage_turns_at[i] = 0;
        }
        foe_hp = -1;
        foe_for_hp = nullptr;
    }
    //  Another Pokemon of the player's came in: its moves are not known
    //  yet, and what the slots of the one before did to this foe says
    //  nothing about them. (The Champion's BLASTOISE: VAPOREON's HYDRO PUMP
    //  in slot 4, out of PP, "took none of the foe's HP" twice. Slot 4 stayed
    //  left out for everyone who came in after it: CHARIZARD's WING ATTACK,
    //  LAPRAS's STRENGTH, and ZAPDOS and ARTICUNO, whose only attacks are in
    //  slot 4, used THUNDER WAVE six times and MIST fifteen times.)
    void reset_active(){
        no_pp = 0;
        last_slot = -1;
        list_back = 0;
        for (int& m : moves){
            m = -1;
        }
        reset_foe();
    }
    //  The slot picked last, -1 if none in this battle.
    int last_slot = -1;
    //  How many times in a row the move list came back after picking
    //  "last_slot" (a press the game missed brings it back once).
    int list_back = 0;
    //  The species caught in the last battle ("" if none). A shiny counts
    //  only if it was one of the wanted species.
    std::string caught;
    //  The last battle was against a shiny (its species, "" if not read),
    //  it was caught, and it went to the PC (the party was full).
    bool shiny = false;
    std::string shiny_species;
    bool shiny_caught = false;
    bool shiny_to_pc = false;
    //  The last battle: a ball caught a species new to the POKeDEX (its
    //  page came up), the Safari Game ended in it (the last SAFARI BALL
    //  missed: "You're out of SAFARI BALLS! Game over!"), no ball was left
    //  in the bag for a species wanted for the POKeDEX.
    bool dex_registered = false;
    bool safari_over = false;
    bool no_balls = false;
    //  The last battle, a legendary Pokemon (StoryBattleSettings::
    //  catch_legendary): the balls thrown at it, what they are worth in
    //  POKe BALLs (a GREAT BALL catches 1.5 times as often, an ULTRA BALL
    //  twice), and whether the bag ran out of them.
    int legendary_balls = 0;
    double legendary_ball_worth = 0;
    bool legendary_no_balls = false;
    //  The ball mark of the foe's box in the last (wild) battle: the game
    //  shows it for a species caught before. 1: there, 0: not there, -1:
    //  not seen (a trainer battle, the box not found). A species the
    //  program wanted that had the mark (not caught again), and one it had
    //  as caught that had none (caught for the POKeDEX).
    int foe_mark = -1;
    std::string owned_by_mark;
    std::string dex_by_mark;
    //  SAFARI BALLs thrown in the last battle.
    int safari_balls = 0;
    //  Shared experience: the moves of the member that stayed out, and the
    //  last "X is trying to learn Y." text (who learns).
    int member_moves[4] = {-1, -1, -1, -1};
    std::string learner;
    //  "member_moves" are read and still right: they change only with a
    //  level-up (a move learned on its own, an evolution) or a move learned
    //  in its place (then they are kept up to date).
    bool member_moves_known = false;
    //  The HP bar of the player's Pokemon (the one out) the last time it was
    //  seen in the last battle: 0 to 1. -1 if not seen.
    double player_hp = -1;
    //  Team play: the members (team indexes) that fought in the last
    //  battle, and those that fainted in it.
    std::set<int> team_used;
    std::set<int> team_fainted;
    //  ... and the HP bar of each the last time it was out (0 to 1, -1 not
    //  known). Kept from battle to battle (set to 1 when healed).
    double team_hp[6] = {-1, -1, -1, -1, -1, -1};
    //  The members that are down: fainted in a battle and not revived since
    //  (no REVIVE, no Pokemon Center). Kept from battle to battle: such a
    //  member cannot come in ("X has no energy left to battle!"), and with
    //  the first of the team down the game starts the battle with the next
    //  one. (The League without REVIVEs: ELECTRODE, down since Lorelei, was
    //  the pick against Lance's GYARADOS. The menu stayed open, the program
    //  took that for a faint, and from there on its order of the party was
    //  not the game's: ZAPDOS came in for LAPRAS, CHARIZARD counted as down.)
    //  Also the ones a pick found out about. Not known after the program
    //  was started again: found out then.
    std::set<int> team_down;
    //  Of the last team battle: the members that fight (not the ones only
    //  carried along), and how many of them were down already when it began.
    int team_fighters = 0;
    int team_down_at_start = 0;
    //  No healing item was found in the bag: not looked for again until
    //  items are bought.
    bool no_heal_items = false;
    //  FULL RESTOREs and MAX POTIONs used in the last battle.
    int restores_used = 0;
    //  "Faster battles" (StoryBattleSettings::fast_end): battles whose end
    //  was found by the fade to black, fades after which the player was not
    //  free to move (the loop went on as before), and the time from the
    //  fade to the end of those found, in milliseconds.
    uint64_t fast_ends = 0;
    uint64_t fast_end_misses = 0;
    uint64_t fast_end_ms = 0;
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

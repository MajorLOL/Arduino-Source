/*  Pokemon FRLG Autostory Safari
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Bait, rocks and balls in a Safari Zone battle.
 *
 *  Up to PocketPilot build 30 the AutoStory threw SAFARI BALLs at a species
 *  it wanted and nothing else. The run of 16 November 2026 played two
 *  Safari Games in the north area for the second CHANSEY: six came, and
 *  each fled after one to four balls (12 in all). That is how the game has
 *  it: a CHANSEY flees 45 times in 100 each turn, and a ball catches it 5
 *  times in 100.
 *
 *  Pokemon Automation has a table of what to do instead (its Packages:
 *  Resources/PokemonFRLG/SafariOptimalAction, one file per species, read by
 *  SafariOptimalAction): for each number of SAFARI BALLs left, the actions
 *  turn by turn. For a CHANSEY with 30 balls: bait, bait, three balls,
 *  bait, two balls, bait, three balls and so on, and a rock before the last
 *  ball. The AutoStory follows that table for a species it wants
 *  (run_story_battle()).
 *
 *  This file is the game's side of it, for the build's test: the chance of
 *  a catch for a list of actions, by the game's own rules (FRLG's
 *  battle_main.c, battle_ai_script_commands.c, battle_script_commands.c):
 *    - At the start the catch factor is the catch rate x 100 / 1275, the
 *      escape factor the Safari flee rate x 100 / 1275, at least 2.
 *    - Each turn the wild Pokemon's choice is made before the player's
 *      action is carried out (OpponentHandleChooseMove()): it flees
 *      5 x escape factor times in 100 (Cmd_if_random_safari_flee()). While
 *      it is angry the factor counts double (20 at most), while it eats a
 *      quarter (at least 1).
 *    - A ball (Cmd_handleballthrow()): the catch rate is the catch factor
 *      x 1275 / 100, x 1.5 for the SAFARI BALL, a third of that at full HP;
 *      then four shakes.
 *    - Bait (HandleAction_ThrowBait()): it eats for 2 to 6 turns more (6 at
 *      most) and is no longer angry. The catch factor is halved, but a
 *      factor of 2 or less after that becomes 3, and it stays as it is when
 *      the eating is over. So with a factor of 2 (CHANSEY) bait makes the
 *      balls better for the rest of the battle, with 3 (KANGASKHAN, TAUROS,
 *      SCYTHER, PINSIR, DRATINI, DRAGONAIR) it costs them nothing.
 *    - A rock (HandleAction_ThrowRock()): angry for 2 to 6 turns more (6 at
 *      most), no longer eating, the catch factor doubled (20 at most).
 *    - After the player's action the Pokemon flees or watches
 *      (HandleAction_WatchesCarefully()): a turn less of being angry (after
 *      the last one the catch factor is the species' own again), or else a
 *      turn less of eating.
 *  By these rules the table catches a CHANSEY 19 times in 100 where balls
 *  alone catch it 10 times, the ones with catch rate 45 19 times instead of
 *  16, and it is never worse than balls alone (23 species, 1 to 30 balls).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStorySafari_H
#define PokemonAutomation_PokemonFRLG_AutoStorySafari_H

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStorySafari{


enum class Action{
    BALL,
    BAIT,
    ROCK,
};

const int BALLS_A_GAME = 30;

//  A species of the Safari Zone: its catch rate and its Safari flee rate
//  (the game's species data).
struct Species{
    const char* name;       //  as in Pokemon Automation's table
    int catch_rate;
    int flee_rate;
};
inline const std::vector<Species>& species(){
    static const std::vector<Species> list{
        {"chansey", 30, 125},
        {"doduo", 190, 50},
        {"dragonair", 45, 125},
        {"dratini", 45, 100},
        {"exeggcute", 90, 75},
        {"goldeen", 225, 50},
        {"kangaskhan", 45, 125},
        {"magikarp", 255, 25},
        {"nidoran", 235, 50},
        {"nidorina", 120, 75},
        {"nidorino", 120, 75},
        {"paras", 190, 50},
        {"parasect", 75, 75},
        {"pinsir", 45, 125},
        {"poliwag", 255, 50},
        {"psyduck", 190, 50},
        {"rhyhorn", 120, 75},
        {"scyther", 45, 125},
        {"seaking", 60, 75},
        {"slowpoke", 190, 50},
        {"tauros", 45, 125},
        {"venomoth", 75, 75},
        {"venonat", 190, 50},
    };
    return list;
}

//  The name of a species in Pokemon Automation's table: the two NIDORANs
//  are one there.
inline std::string table_name(const std::string& slug){
    return slug.rfind("nidoran", 0) == 0 ? "nidoran" : slug;
}

inline int catch_factor(int catch_rate){
    return catch_rate * 100 / 1275;
}
inline int escape_factor(int flee_rate){
    return std::max(2, flee_rate * 100 / 1275);
}

//  The game's Sqrt(): the whole part of the root.
inline int whole_root(int value){
    int root = (int)std::sqrt((double)value);
    while (root > 0 && root * root > value){
        root--;
    }
    while ((root + 1) * (root + 1) <= value){
        root++;
    }
    return root;
}

//  The chance that a SAFARI BALL catches at full HP, for a catch factor.
inline double ball_chance(int factor){
    const int rate = std::min(255, factor * 1275 / 100);
    const int odds = rate * 15 / 10 / 3;
    if (odds > 254){
        return 1.0;
    }
    if (odds <= 0){
        return 0.0;
    }
    const int shake = 1048560 / whole_root(whole_root(16711680 / odds));
    const double one = std::min(1.0, shake / 65536.0);
    return one * one * one * one;
}

//  The chance that the wild Pokemon flees in a turn.
inline double flee_chance(int escape, int eating, int angry){
    int rate = escape;
    if (angry > 0){
        rate = std::min(20, escape * 2);
    }else if (eating > 0){
        rate = std::max(1, escape / 4);
    }
    return std::min(100, rate * 5) / 100.0;
}

//  The chance of a catch in one battle with "balls" SAFARI BALLs left: the
//  actions in turn, balls after them. (The Safari Game is over with its
//  last ball.)
inline double catch_odds(int catch_rate, int flee_rate, const std::vector<Action>& actions, int balls){
    const int own = catch_factor(catch_rate);
    const int escape = escape_factor(flee_rate);
    //  How likely each state is at the start of a turn: the catch factor,
    //  the turns left eating, the turns left angry, the balls left.
    using State = std::tuple<int, int, int, int>;
    std::map<State, double> now;
    now[State(own, 0, 0, balls)] = 1.0;
    double caught = 0;
    for (size_t turn = 0; turn < 400 && !now.empty(); turn++){
        const Action action = turn < actions.size() ? actions[turn] : Action::BALL;
        std::map<State, double> next;
        //  The Pokemon did not flee: it watches.
        auto watches = [&](int factor, int eating, int angry, int left, double chance){
            if (angry > 0){
                angry--;
                if (angry == 0){
                    factor = own;
                }
            }else if (eating > 0){
                eating--;
            }
            next[State(factor, eating, angry, left)] += chance;
        };
        for (const auto& entry : now){
            const int factor = std::get<0>(entry.first);
            const int eating = std::get<1>(entry.first);
            const int angry = std::get<2>(entry.first);
            const int left = std::get<3>(entry.first);
            const double chance = entry.second;
            if (left <= 0 || chance < 1e-13){
                continue;
            }
            const double stays = 1.0 - flee_chance(escape, eating, angry);
            switch (action){
            case Action::BALL:{
                const double catches = ball_chance(factor);
                caught += chance * catches;
                if (left > 1){
                    watches(factor, eating, angry, left - 1, chance * (1.0 - catches) * stays);
                }
                break;
            }
            case Action::BAIT:{
                int halved = factor / 2;
                if (halved <= 2){
                    halved = 3;
                }
                for (int more = 2; more <= 6; more++){
                    watches(halved, std::min(6, eating + more), 0, left, chance * stays / 5);
                }
                break;
            }
            case Action::ROCK:{
                const int doubled = std::min(20, factor * 2);
                for (int more = 2; more <= 6; more++){
                    watches(doubled, 0, std::min(6, angry + more), left, chance * stays / 5);
                }
                break;
            }
            }
        }
        now.swap(next);
    }
    return caught;
}


}
}
}
}
#endif

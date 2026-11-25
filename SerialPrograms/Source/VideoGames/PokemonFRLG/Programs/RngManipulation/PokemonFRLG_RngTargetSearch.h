/*  RNG Target Search
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Finds static, gift and starter RNG targets (a Target Seed plus Advances, as
 *  Static/Gift/Starter RNG take them) for a trainer, the way the "Searcher" and
 *  "Initial Seed" tabs of Ten Lines do for FRLG on Switch:
 *      https://lincoln-lm.github.io/ten-lines
 *      https://github.com/Lincoln-LM/ten-lines
 *
 *  How a program can use it to pick its own target when the SID is not known:
 *   1. SID Helper reads the TID and narrows the SID down to a few weighted candidates.
 *   2. search_rng_targets() with those candidates, then rank_rng_targets().
 *   3. Run Static/Gift/Starter RNG with the top target's seed and advances.
 *   4. Read the result:
 *       - Shiny: the SID is one of the candidates the target is shiny for. If that is
 *         one candidate, the SID is confirmed.
 *       - Nature and IVs match, but not shiny: the target was hit, so every candidate it
 *         is shiny for is wrong. Drop them and search again.
 *       - Nature or IVs differ: the target was missed. Retry it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_RngTargetSearch_H
#define PokemonAutomation_PokemonFRLG_RngTargetSearch_H

#include <cstdint>
#include <string>
#include <vector>
#include "Pokemon/Pokemon_AdvRng.h"
#include "PokemonFRLG_BlindNavigation.h"
#include "PokemonFRLG_SeedsDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The Gen 3 LCRNG: state * 0x41C64E6D + 0x6073.
uint32_t lcrng_next(uint32_t state);
uint32_t lcrng_previous(uint32_t state);
//  How many lcrng_next() calls take "from" to "to". The LCRNG has a single cycle of
//  2^32 states, so every pair has exactly one distance.
uint32_t lcrng_distance(uint32_t from, uint32_t to);

//  The static, gift or starter Pokemon (Method 1) generated from "state", the RNG state
//  just before the PID is rolled: the next two calls give the low and high PID halves,
//  the two after that the IVs (HP/Atk/Def, then Spe/SpA/SpD).
Pokemon::AdvPokemonResult method1_pokemon(uint32_t state);

Pokemon::AdvShinyType shiny_type(uint32_t pid, uint16_t tid, uint16_t sid);

struct HiddenPower{
    std::string type;
    uint8_t power;
};
HiddenPower hidden_power(const Pokemon::AdvIVs& ivs);


//  One way to reach an initial seed from the title screen.
struct InitialSeedEntry{
    //  The seed reached, after the blackout offset.
    uint16_t seed;
    uint64_t seed_time_ms;
    SeedButton seed_button;
    BlackoutButton blackout_button;
};

//  The initial seeds reachable with one sound setting and button mode, each by its
//  earliest entry in the seed table. That is the entry Static/Gift/Starter RNG use for
//  a Target Seed (SeedsDatabase::find_seed()).
//
//  Holding a blackout button lowers the seed by BLACKOUT_SEED_OFFSET. Ten Lines lists
//  that only for the English Switch tables in Help mode (HELD_BUTTON_OFFSETS["lg_nx"]
//  and ["fr_nx"] in src/wasm/src/initial_seed.hpp), so pass "allow_blackout" only then.
std::vector<InitialSeedEntry> initial_seed_entries(
    const SeedsDatabase& database,
    SoundSetting sound,
    const std::string& button_mode,
    bool allow_blackout
);


struct SidCandidate{
    uint16_t sid;
    //  Relative likelihood. The weights need not add up to 1.
    double weight;
};

struct RngTargetFilters{
    //  Empty: any nature.
    std::vector<Pokemon::AdvNature> natures;
    Pokemon::AdvIVs min_ivs;
    Pokemon::AdvGender gender = Pokemon::AdvGender::Any;
    int16_t gender_threshold = 126;
    Pokemon::AdvAbility ability = Pokemon::AdvAbility::Any;
    //  Shiny for at least one SID candidate.
    bool shiny_required = true;

    //  Total advances from the initial seed: the "Advances" of Ten Lines and of the RNG
    //  programs. They include the title and continue screens, so fewer of them are
    //  spent in game.
    uint64_t min_advances = 0;
    uint64_t max_advances = 1000000;

    uint64_t min_seed_time_ms = 0;
    //  Skip seed times within "avoided_seed_time_radius_ms" of this.
    uint64_t avoided_seed_time_ms = 0;
    uint64_t avoided_seed_time_radius_ms = 0;
};

struct ShinyCandidate{
    //  Index into the SID candidates.
    size_t candidate;
    Pokemon::AdvShinyType type;
};

struct RngTarget{
    //  The RNG state before the PID is rolled: the "Target" column of Ten Lines.
    uint32_t state;
    InitialSeedEntry seed;
    uint64_t advances;
    Pokemon::AdvPokemonResult pokemon;
    //  The candidates this target is shiny for.
    std::vector<ShinyCandidate> shiny_for;
    //  Their share of the total candidate weight.
    double shiny_probability;
};

//  Every Method 1 result that passes "filters", each with the initial seed that reaches
//  it in the fewest advances, sorted by advances.
std::vector<RngTarget> search_rng_targets(
    const std::vector<InitialSeedEntry>& seeds,
    uint16_t tid,
    const std::vector<SidCandidate>& candidates,
    const RngTargetFilters& filters
);

//  Sorts targets by expected wait per shiny: the time to the target (seed time plus
//  advances at the GBA frame rate) divided by shiny_probability. A target shiny for
//  every candidate beats one shiny for half of them unless it takes twice as long.
//  Targets with no shiny candidate go last, by time.
void rank_rng_targets(std::vector<RngTarget>& targets);



}
}
}
#endif

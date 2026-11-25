/*  RNG Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which Starter RNG targets to ask for, in order, when the SID is
 *  only narrowed down to SID Helper's candidates, and what to do after each
 *  one. The loop is the one PokemonFRLG_RngTargetSearch.h describes:
 *   - Shiny, and the target was hit: the SID is one of those the target is
 *     shiny for. With one left, it is confirmed and later targets can be
 *     searched with it alone.
 *   - Shiny, but not the target (another advance): it says nothing about
 *     the SID. The plan ends with the shiny.
 *   - Nature and IVs match, but not shiny: the target was hit, so the SIDs it
 *     is shiny for are wrong. Go on to the next attempt.
 *   - Nature or IVs differ: the target was missed. Run it again, up to the
 *     retries, then go on to the next attempt.
 *
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_RngPlan_H
#define PokemonAutomation_PokemonFRLG_RngPlan_H

#include <cstdint>
#include <string>
#include <vector>
#include "CommonFramework/Language.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngTargetSearch.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SeedsDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The SIDs SID Helper lists for "tid": "count" of them, 2 advances apart,
//  centered on "target_advances", as its get_sid_messages() lists them. The
//  middle one, where SID Helper aims, weighs the most, and each step away one
//  less, so the outermost weigh 1.
std::vector<SidCandidate> sid_helper_candidates(uint16_t tid, uint32_t target_advances, uint8_t count);


struct RngAttempt{
    RngTarget target;
    //  The SIDs the target is shiny for, of those the attempts before it
    //  leave. Empty when the target isn't shiny for any of them.
    std::vector<uint16_t> sids;
};

//  The attempts to make, in order, for SID candidates "candidates" (the ones
//  "targets" were searched with). Each is the best target by
//  rank_rng_targets() over the candidates the attempts before it leave, so a
//  target shiny for several candidates comes before one shiny for one, unless
//  it takes much longer. Stops after "max_attempts", or once no target is
//  shiny for a candidate left.
//
//  With "shiny_required" off, a hit ends the plan whether it is shiny or not,
//  so the plan is the one quickest target.
std::vector<RngAttempt> plan_rng_attempts(
    const std::vector<RngTarget>& targets,
    const std::vector<SidCandidate>& candidates,
    bool shiny_required,
    size_t max_attempts
);


//  What Starter RNG's targets are searched with.
struct StarterRngPlanInputs{
    bool firered;
    Language language;
    SoundSetting sound;
    uint16_t tid;
    std::vector<SidCandidate> candidates;
    RngTargetFilters filters;
    size_t max_attempts;
};

//  Starter RNG's smallest Advances setting.
inline constexpr uint64_t STARTER_RNG_MIN_ADVANCES = 940;

//  Starter RNG's title screen wait, before calibration: the seed list's
//  delay plus this (FIXED_SEED_OFFSET in StarterRng's attempts).
inline constexpr int64_t STARTER_RNG_FIXED_SEED_OFFSET_MS = -845;
//  The shortest title screen wait the plan uses. Starter RNG refuses waits
//  under 29,650 ms, and its docs (pokemonautomation.github.io,
//  docs/Programs/PokemonFRLG/StarterRng.md, "Avoid low seed delays when
//  possible") warn that waits near 30,500 ms can miss the title screen
//  press. 31,500 ms keeps a second clear of that for the seed calibration.
inline constexpr int64_t STARTER_RNG_MIN_SEED_WAIT_MS = 31500;
//  Starter RNG's title screen wait for `seed`, as prepare_timings() makes it
//  before calibration.
int64_t starter_rng_seed_wait_ms(const InitialSeedEntry& seed);
//  Whether the wait is at least STARTER_RNG_MIN_SEED_WAIT_MS. (Console B,
//  2026-11-02, FireRed English: seeds 11A3 and 405C, at 30,473 and
//  30,488 ms in the table, are 29,628 and 29,643 ms waits, which Starter
//  RNG refuses.)
bool starter_rng_can_wait_for(const InitialSeedEntry& seed);

//  Starter RNG's Continue Screen frames, taken off the advances before the
//  in-game wait (CONTINUE_SCREEN_FRAMES in StarterRng's attempts).
inline constexpr uint64_t STARTER_RNG_CONTINUE_SCREEN_FRAMES = 200;
//  Starter RNG's in-game wait for `advances`, before calibration. It
//  refuses one under 7,500 ms, which STARTER_RNG_MIN_ADVANCES just makes.
int64_t starter_rng_ingame_ms(uint64_t advances);
//  Room for the in-game calibration, in advances (2 per frame of wait): the
//  calibration moves the wait, and it went to -720 after one miss on console
//  B (2026-11-02, seed B579 at 967 advances: an 8 s wait cut to 1.7 s). The
//  plan's targets are at least this many advances past Starter RNG's
//  minimum. To tune from the logs' calibrations.
inline constexpr uint64_t STARTER_RNG_INGAME_MARGIN_ADVANCES = 720;
inline constexpr uint64_t STARTER_RNG_PLAN_MIN_ADVANCES = STARTER_RNG_MIN_ADVANCES + STARTER_RNG_INGAME_MARGIN_ADVANCES;

//  The attempts for Starter RNG, from the seed list of the version and
//  language in Help button mode, the one Starter RNG uses.
std::vector<RngAttempt> plan_starter_rng(const StarterRngPlanInputs& inputs);


}
}
}
#endif

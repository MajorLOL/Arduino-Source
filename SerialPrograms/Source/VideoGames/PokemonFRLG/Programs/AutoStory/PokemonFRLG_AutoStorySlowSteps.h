/*  Pokemon FRLG Autostory Slow Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rock stairs in the timing of a walk.
 *
 *  A run is one held input, timed for its tiles: 16 frames each, let go
 *  halfway through the last one (8 frames to spare either way). A step up
 *  from rock stairs, or down onto them, takes 23 frames (FRLG's
 *  PlayerNotOnBikeMoving(): PlayerIsMovingOnRockStairs() ->
 *  PlayerWalkSlow(); UpdateWalkSlowAnim() moves a pixel in two frames of
 *  three). Sideways the stairs are walked as any tile. So an input over
 *  stairs ended 7 frames early for each of them: a tile short from two on,
 *  and from one when something else on the way takes a few frames (a map
 *  border, where the game loads the next map).
 *
 *  The logs up to 16 November 2026, their plain runs matched to today's
 *  route by the names of the legs, where the video followed the run to its
 *  end: with two steps on stairs 676 of 680 came out one tile short, with
 *  three 8 of 8, with four 217 of 222 two short; with one step 2 of 276
 *  (one of the two the only run with a map border on the way as well).
 *  Without stairs, 1 of 1,434 runs over a map border was a tile short. The
 *  walk then made the missing tile up with a second input. Where the video
 *  cannot follow, it did not: south of Seven Island the path runs between
 *  the sea on both sides, the stairs at (8, 14) and the border to the
 *  Canyon Entrance five tiles on made the run of 19 one short, the video
 *  lost the picture there, and the grass was walked along a row too high
 *  for half an hour.
 *
 *  Measured on the recording that came with that night's report (the walk
 *  back up the same path, 16 tiles, 10 frames a second; the land in the
 *  picture compared frame to frame): 256 pixels in all, the step off the
 *  stairs 7 frames longer than the others, the step over the border 2. So
 *  that run of 19 needed 9 frames more than it was timed for, with 8 to
 *  spare; with the stairs counted it has 6 left for the border.
 *
 *  The route's data marks those steps (generate.py, Builder.
 *  mark_slow_steps()): bit i of StoryRun::slow is step i of the run. The
 *  input lasts 7 frames longer for each: let go 15 frames into a slow last
 *  step with 8 to go, or 8 into an ordinary one as before.
 *
 *  This part has no dependencies on the rest of the program so it can be
 *  tested on its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStorySlowSteps_H
#define PokemonAutomation_PokemonFRLG_AutoStorySlowSteps_H

#include <stdint.h>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStorySlowSteps{


const int STEP_FRAMES = 16;         //  a walking step
const int SLOW_STEP_FRAMES = 23;    //  ... on rock stairs, up or down
const int EXTRA_FRAMES = SLOW_STEP_FRAMES - STEP_FRAMES;
const int BITS = 64;                //  the steps of a run that can be marked

//  Step "step" of a run is slow.
inline bool is_slow(uint64_t slow, int step){
    return step >= 0 && step < BITS && ((slow >> step) & 1) != 0;
}

//  How many of the steps [first, first + count) of a run are slow.
inline int slow_steps(uint64_t slow, int first, int count){
    int n = 0;
    for (int i = 0; i < count; i++){
        n += is_slow(slow, first + i) ? 1 : 0;
    }
    return n;
}

//  The marks of a run from its step "first" on.
inline uint64_t from_step(uint64_t slow, int first){
    if (first <= 0){
        return slow;
    }
    return first >= BITS ? 0 : slow >> first;
}

//  Two runs in the same direction walked as one, the second right after
//  the first ("first_count" steps): the marks of the whole. False: the
//  marks of the second do not fit behind the first (then they are walked
//  one after the other).
inline bool join(uint64_t first, int first_count, uint64_t second, int second_count, uint64_t& joined){
    if (second == 0){
        joined = first;
        return true;
    }
    if (first_count < 0 || second_count < 0 || first_count >= BITS || first_count + second_count > BITS){
        return false;
    }
    joined = first | (second << first_count);
    return true;
}

//  The frames a held input lasts for "moves" moves of "frames" each, of
//  which "slow" are walking steps on rock stairs, after "turn" frames of
//  turning: let go half a move before the next one would start.
inline double hold_frames(double turn, double frames, int moves, int slow){
    return turn + frames * moves + (double)EXTRA_FRAMES * slow - frames / 2;
}

//  The game's side, to check the above: how many steps a held input of
//  "hold" frames makes from step "first" of a run on, after "turn" frames
//  of turning. A step starts when the one before it is over and the input
//  is still held.
inline int steps_made(uint64_t slow, int first, double turn, double hold){
    double start = turn;
    int made = 0;
    while (start < hold && made < 4 * BITS){
        start += is_slow(slow, first + made) ? SLOW_STEP_FRAMES : STEP_FRAMES;
        made++;
    }
    return made;
}


}
}
}
}
#endif

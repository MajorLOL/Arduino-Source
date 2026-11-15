/*  Pokemon FRLG Autostory Bump
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A run that ends where the way is shut.
 *
 *  A run is one held input, timed for its tiles, and the video counts the
 *  tiles as the map scrolls by. Over open water it cannot (water looks the
 *  same a tile further), so there the timing is all there is. In the
 *  night of 15 to 16 November 2026 the run from Five Island up to Resort
 *  Gorgeous, 30 tiles of water over two map borders, came out one tile
 *  short four times out of four. The input is over halfway through its
 *  last tile, so four frames lost on the way (a border where the game
 *  stands still for a moment) are enough. A row too low, the next run
 *  went into the rocks. (Three of the four times the picture also changed
 *  on the way; that passed for a battle starting, the tiles the video had
 *  not seen were walked again, and the player landed on the small island
 *  further up.)
 *
 *  So such a run is laid out to end against something that does not move:
 *  a rock in the water, a wall, someone who stands still. The player goes
 *  on until the video sees nothing move any more, and how many tiles that
 *  took does not matter. Getting off the water on the way is part of it:
 *  the player jumps onto the shore and walks on (CanStopSurfing()).
 *
 *  "Nothing moves": the video followed the map during an input and saw it
 *  stay where it was. Standing next to water, it is sure of that in about
 *  two tries out of three (in the log of that night: 131 of 189 inputs for
 *  two tiles or more that moved nothing); in the others it saw no
 *  movement either, but was not sure (the waves, most likely). So: twice
 *  sure, or four tries in a row without any movement seen. While the
 *  player moves, the video sees the rocks and shores go by, also where it
 *  cannot count them.
 *
 *  This part has no dependencies on the rest of the program so it can be
 *  tested on its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBump_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBump_H

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStoryBump{


//  What a try (one held input) came to.
enum class Seen{
    STOOD,      //  the video is sure that the player did not move
    STILL,      //  it saw no movement, without being sure
    MOVED,      //  the player moved, or the picture changed on the way
    STOPPED,    //  a battle or someone talking stopped it (and is over now)
};

enum class Act{
    WHOLE,      //  an input for the whole run
    PART,       //  an input for a few tiles more
    DONE,       //  the way is shut: arrived
    SHUT,       //  it was shut from the start: the player is not where the run begins
    ENDLESS,    //  the player does not come to a stop
};

const int MORE_TILES = 6;       //  a PART (long enough for the video to be sure)
const int SURE_TO_END = 2;      //  tries without movement of which the video was sure
const int STILL_TO_END = 4;     //  tries in a row without movement, sure or not
const int MOST_TRIES = 40;

struct Run{
    int tries = 0;
    int still = 0;          //  tries in a row without movement seen
    int sure = 0;           //  ... of which the video was sure
    bool moved = false;     //  the player got somewhere, or was stopped on the way
};

//  The first try.
inline Act first(Run& run){
    run = Run();
    run.tries = 1;
    return Act::WHOLE;
}

//  After a try: what next.
inline Act next(Run& run, Seen seen){
    Act act = Act::PART;
    switch (seen){
    case Seen::STOOD:
    case Seen::STILL:
        run.still++;
        run.sure += seen == Seen::STOOD ? 1 : 0;
        if (run.sure >= SURE_TO_END || run.still >= STILL_TO_END){
            return run.moved ? Act::DONE : Act::SHUT;
        }
        break;
    case Seen::MOVED:
        run.still = 0;
        run.sure = 0;
        run.moved = true;
        break;
    case Seen::STOPPED:
        //  Wherever that was along the run: all of it again. What is too
        //  much of it is spent standing against the end. (It may have been
        //  on the last tile: the player is on the way, so standing after
        //  this is the end and not a way shut from the start.)
        run.still = 0;
        run.sure = 0;
        run.moved = true;
        act = Act::WHOLE;
        break;
    }
    if (run.tries >= MOST_TRIES){
        return Act::ENDLESS;
    }
    run.tries++;
    return act;
}


}
}
}
}
#endif

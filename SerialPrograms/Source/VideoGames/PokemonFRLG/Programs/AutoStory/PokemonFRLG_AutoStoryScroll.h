/*  Pokemon FRLG Autostory Scroll Tracking
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Measures how far the player has walked by following the scrolling of the
 *  overworld on the video.
 *
 *  The overworld camera keeps the player in the middle of the screen, so
 *  while the player walks the whole map moves the other way, one GBA pixel
 *  per frame, 16 pixels per tile. Comparing each video frame with an earlier
 *  one at different offsets gives the exact number of pixels walked.
 *
 *  This part has no dependencies on the rest of the program so it can be
 *  tested on its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryScroll_H
#define PokemonAutomation_PokemonFRLG_AutoStoryScroll_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStoryScroll{


constexpr int SCREEN_WIDTH = 240;
constexpr int SCREEN_HEIGHT = 160;
constexpr int TILE_PIXELS = 16;


//  The game screen at GBA resolution, one brightness value per pixel.
//  (On the heap: several of these at once would be a lot for the stack of
//  the program thread.)
struct Screen{
    std::vector<uint8_t> pixels;

    Screen()
        : pixels((size_t)SCREEN_WIDTH * SCREEN_HEIGHT, 0)
    {}

    uint8_t at(int x, int y) const{
        return pixels[(size_t)y * SCREEN_WIDTH + (size_t)x];
    }
};

//  Sample the game screen out of 32-bit pixels (0xAARRGGBB), the part of the
//  video that shows the game, of any size.
void sample_screen(
    Screen& out,
    const uint32_t* data, size_t bytes_per_row,
    size_t width, size_t height
);

//  How different "now" is from "key" when "now" is moved by (dx, dy):
//  now(x, y) against key(x + dx, y + dy), the mean of the capped absolute
//  differences over the part both show, leaving out the player. Every
//  second row is compared; "first_row" 1: the rows between them.
double screen_mismatch(const Screen& key, const Screen& now, int dx, int dy, int first_row = 0);

//  How much the brightness of a picture varies: the mean distance of its
//  pixels from their mean.
double screen_spread(const Screen& screen);


//  Where the player stands in "now" compared to "key", both taken with the
//  player standing still, in whole tiles: x to the right, y down. Looks up
//  to "max_tiles" tiles away each way.
struct TileOffset{
    int x = 0;
    int y = 0;
    double best = 0;        //  The mismatch at that offset, over the whole screen.
    double margin = 0;      //  How much worse the next best offset is, where they differ.
    bool clear = false;     //  The best offset stands out: the answer can be trusted.
    std::string summary() const;
};
TileOffset tile_offset(const Screen& key, const Screen& now, int max_tiles);

//  Same, looking only along (dir_x, dir_y), from 1 tile back to "max_tiles"
//  ahead: how far a straight run got.
TileOffset tile_offset_along(const Screen& key, const Screen& now, int dir_x, int dir_y, int max_tiles);



//  Follows the scrolling during a straight run in one direction.
class Tracker{
public:
    enum class Result{
        TRACKED,        //  The offset was found.
        UNSURE,         //  The picture does not tell (no detail); estimated.
        LOST,           //  The screen changed (battle, fade, menu).
    };

public:
    //  (dir_x, dir_y): the direction of the run, one of (1,0) (-1,0) (0,1) (0,-1).
    //  max_pixels_per_ms: the highest walking speed.
    Tracker(int dir_x, int dir_y, double max_pixels_per_ms);

    //  Add the next video frame, taken "time_ms" after the start.
    Result add(const Screen& screen, double time_ms);

    //  The input of the run is over: from here on nothing moves by itself.
    //  A frame that does not tell where the map is (several offsets fit, or
    //  none fits as the frames before did) is taken as the map standing
    //  still. While the input lasts, such a frame is taken as the walk going
    //  on at its speed, or left out.
    //  (Kindle Road, 16 November 2026, the runs down the grass: the frames
    //  after the last step fit badly and do not tell. Left out since build
    //  28, they kept the run from being seen to stand, and 0.3 seconds later
    //  the speed of the walk put the map a tile or two further: "64 px (4
    //  tiles)" in 27 of 39 runs of three tiles, "80 px" in 8.)
    void input_over(){ m_input_over = true; }

    //  Pixels walked in the direction of the run, and to the side of it.
    int pixels() const{ return m_along; }
    int side_pixels() const{ return m_side; }

    //  round(pixels / 16)
    int tiles() const;

    //  The offset was measured well all along.
    bool reliable() const;

    //  Lost the screen (two frames in a row unlike anything before), and
    //  not found again since.
    bool lost() const{ return m_lost; }

    //  How often the screen was lost and found again further on: something
    //  covered the map for a moment while the player walked on. (A poisoned
    //  Pokemon: every five steps the map is drawn as a mosaic for 10 frames,
    //  FldEffPoison_Start(), and the walk goes on through it.)
    //
    //  Not after a fade: a battle starts with the whole picture fading to
    //  gray and back twice, a warp with a fade to black, and what follows is
    //  not the map. The transition into a wild battle on open ground
    //  (B_TRANSITION_SLICE) slides every second line of the picture to the
    //  left, slowly at first, and the lines between them to the right: on
    //  the rows that are compared, that is the map moving on in a run to the
    //  right. (The run of 16 November 2026 with build 29: 33 runs to the
    //  right in the grass read "167 px (10 tiles) ... lost and found 1x",
    //  up to 308 px for four tiles, when a battle started on the way; the
    //  battle was found a moment later, with the run counted as timed.)
    size_t flashes() const{ return m_flashes; }

    //  The last frames showed no movement for this long.
    double still_ms() const{ return m_still_ms; }

    size_t frames() const{ return m_frames; }
    size_t unsure_frames() const{ return m_unsure; }
    double noise() const{ return m_noise; }
    double last_mismatch() const{ return m_last_best; }

    std::string summary() const;

private:
    void offset_to_shift(int along, int side, int& dx, int& dy) const;

private:
    int m_dir_x;
    int m_dir_y;
    double m_max_speed;

    bool m_has_key = false;
    Screen m_key;
    int m_key_along = 0;
    int m_key_side = 0;

    int m_along = 0;
    int m_side = 0;
    double m_last_time = 0;
    double m_seen_time = 0;     //  the last frame given, used or not
    double m_speed = 0;         //  pixels per ms, recent
    double m_still_ms = 0;
    bool m_input_over = false;

    double m_noise = -1;        //  mismatch of a perfect match: video noise
    double m_last_best = 0;
    double m_typical = -1;      //  recent mismatch of the matches found
    int m_bad_in_row = 0;
    bool m_lost = false;
    bool m_gone = false;        //  lost for good: no longer looked for
    bool m_faded = false;       //  one of the frames that lost the screen was a fade
    double m_lost_since = 0;    //  the first frame unlike anything before
    double m_blurred_since = -1;    //  the first of the frames left out now

    size_t m_frames = 0;
    size_t m_unsure = 0;
    size_t m_lost_frames = 0;
    size_t m_flashes = 0;
};


//  Whether the tiles of a run that nothing stopped (no battle, no one
//  talking) are the video's to count: it followed the map to the end, well,
//  and never lost it on the way. A run that was lost and found again went
//  on as it was timed, and the timing counts it, as it did when a lost map
//  stayed lost. Finding the map again is for a run that something stops
//  later: there the video's count up to that moment is all there is.
//
//  After a gap the count is right on ground with detail, but it can be off
//  where the picture is mostly water, which moves by itself. South of Seven
//  Island the path runs between the sea, and the name of the place slides
//  in at the map border: the picture is lost there. In the recording of 16
//  November 2026 (the walk back up, 16 tiles, 10 frames a second) it was
//  found again two frames later and came to 251 px for the 256 walked:
//  right, with 3 px to spare. The same walk played backwards: 256.
inline bool counts_run(bool lost, bool reliable, size_t flashes){
    return !lost && reliable && flashes == 0;
}



}
}
}
}
#endif

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
//  differences over the part both show, leaving out the player.
double screen_mismatch(const Screen& key, const Screen& now, int dx, int dy);


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

    //  Pixels walked in the direction of the run, and to the side of it.
    int pixels() const{ return m_along; }
    int side_pixels() const{ return m_side; }

    //  round(pixels / 16)
    int tiles() const;

    //  The offset was measured well all along.
    bool reliable() const;

    //  Lost the screen (two frames in a row unlike anything before).
    bool lost() const{ return m_lost; }

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
    double m_speed = 0;         //  pixels per ms, recent
    double m_still_ms = 0;

    double m_noise = -1;        //  mismatch of a perfect match: video noise
    double m_last_best = 0;
    double m_typical = -1;      //  recent mismatch of the matches found
    int m_bad_in_row = 0;
    bool m_lost = false;

    size_t m_frames = 0;
    size_t m_unsure = 0;
    size_t m_lost_frames = 0;
};



}
}
}
}
#endif

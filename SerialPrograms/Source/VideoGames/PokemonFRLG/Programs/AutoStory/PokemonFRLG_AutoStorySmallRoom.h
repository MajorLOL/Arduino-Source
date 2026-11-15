/*  Pokemon FRLG Autostory Small Room
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Is the player in a small building?
 *
 *  The game's picture is 15 tiles wide. A room narrower than that has
 *  black beside it: the gate house of the Safari Zone is 9 tiles wide, and
 *  from its middle three tiles at each side of the picture are black.
 *  Outdoors the map goes on to both sides.
 *
 *  This is how the program tells whether the Safari Game was joined. With
 *  less than 500 the attendant says "Oops! Not enough money!" and sends
 *  the player one step back (the same as after a NO), and the scene is
 *  over just as after a game that was joined: on 15 November 2026 the save
 *  had 146, and the program walked on as if in the Safari Zone, into the
 *  counter, four times. (The text reading did not make out the desk's
 *  texts in that run, so it is not asked.)
 *
 *  Looked at: a strip at each side of the picture, as wide as a tile and a
 *  half, between the money box (top left) and the text box (bottom): black
 *  in the small room whether the player stands in its middle or a tile to
 *  either side. And the middle of the picture, to tell a small room from a
 *  picture that is dark as a whole (a fade, a cave without FLASH).
 *
 *  This part has no dependencies on the rest of the program so it can be
 *  tested on its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStorySmallRoom_H
#define PokemonAutomation_PokemonFRLG_AutoStorySmallRoom_H

#include <stddef.h>
#include <stdint.h>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStorySmallRoom{


enum class Sides{
    OPEN,       //  the map goes on at the sides (or something is drawn there)
    BLACK,      //  black at both sides, a picture between them: a small room
    DARK,       //  black all over
};

//  How much of a part of the picture is black: "x0" to "x1" of its width,
//  "y0" to "y1" of its height.
inline double black_share(
    const uint32_t* pixels, size_t width, size_t height, size_t bytes_per_row,
    double x0, double x1, double y0, double y1
){
    const size_t left = (size_t)(x0 * (double)width);
    const size_t right = (size_t)(x1 * (double)width);
    const size_t top = (size_t)(y0 * (double)height);
    const size_t bottom = (size_t)(y1 * (double)height);
    size_t black = 0;
    size_t all = 0;
    for (size_t y = top; y < bottom && y < height; y++){
        const uint32_t* row = (const uint32_t*)((const char*)pixels + y * bytes_per_row);
        for (size_t x = left; x < right && x < width; x++){
            const uint32_t pixel = row[x];
            const uint32_t r = (pixel >> 16) & 0xff;
            const uint32_t g = (pixel >> 8) & 0xff;
            const uint32_t b = pixel & 0xff;
            if (r < 32 && g < 32 && b < 32){
                black++;
            }
            all++;
        }
    }
    return all == 0 ? 0 : (double)black / (double)all;
}

//  "pixels": the game's picture (the game box of a frame), 0xAARRGGBB,
//  "width" by "height", its rows "bytes_per_row" apart.
inline Sides black_sides(const uint32_t* pixels, size_t width, size_t height, size_t bytes_per_row){
    if (pixels == nullptr || width < 60 || height < 40){
        return Sides::OPEN;
    }
    const double left = black_share(pixels, width, height, bytes_per_row, 0.01, 0.12, 0.45, 0.70);
    const double right = black_share(pixels, width, height, bytes_per_row, 0.88, 0.99, 0.45, 0.70);
    if (left < 0.97 || right < 0.97){
        return Sides::OPEN;
    }
    const double middle = black_share(pixels, width, height, bytes_per_row, 0.35, 0.65, 0.45, 0.70);
    return middle > 0.90 ? Sides::DARK : Sides::BLACK;
}


}
}
}
}
#endif

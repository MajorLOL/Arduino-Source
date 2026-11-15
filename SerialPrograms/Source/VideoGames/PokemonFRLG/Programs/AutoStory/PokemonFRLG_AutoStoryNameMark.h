/*  Pokemon FRLG Autostory Name Mark
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Tells the male mark from the female mark at the end of a name in a
 *  battle's HP box.
 *
 *  The two NIDORANs have the same name but for that mark (the game draws it
 *  as the last sign of the name, in the name's own color, and no gender sign
 *  after it: UpdateNickInHealthbox()). The text reading does not tell the
 *  marks apart: both names read as "NIDORANE" or "NIDORAN", so every NIDORAN
 *  was taken for the female one.
 *
 *  Both marks are a ring and a stem in a glyph of five by seven game pixels
 *  (the game's small font). The male mark has its stem, the arrow, above
 *  the ring; the female one has it below, with a bar across:
 *
 *      . . # . .        . # # . .
 *      . # # # .        # . . # .
 *      # . # . #        # . . # .
 *      . # # . .        . # # . .
 *      # . . # .        . . # . .
 *      # . . # .        # # # # #
 *      . # # . .        . . # . .
 *
 *  So the middle of the glyph is looked at in two places: its second and
 *  third row, where the male mark has its arrow and the female one the hole
 *  of its ring, and its fifth and sixth row, where it is the other way
 *  round. A mark is only told when both places agree, the ring's two sides
 *  are there beside the hole, and the glyph is not a letter.
 *
 *  This part has no dependencies on the rest of the program so it can be
 *  tested on its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryNameMark_H
#define PokemonAutomation_PokemonFRLG_AutoStoryNameMark_H

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStoryNameMark{


enum class Mark{
    NOT_TOLD,
    MALE,
    FEMALE,
};

//  The mark at the end of the name in a picture of the name's box: dark
//  text on the light HP box, the name's last glyph the last thing with ink
//  in it. "pixels": 0xAARRGGBB, "width" by "height", its rows
//  "bytes_per_row" apart. "game_pixel": how many pixels of the picture one
//  pixel of the game is wide (4.33 for a 720p capture).
//
//  "seen", for the log: the name's last glyph as five by seven game pixels,
//  "#" where it is dark, its rows from the top with "/" between them (the
//  male mark: "..#../.###./#.#.#/.##../#..#./#..#./.##.."). Empty if there
//  is no text in the picture.
Mark read_mark(
    const uint32_t* pixels, size_t width, size_t height, size_t bytes_per_row,
    double game_pixel, std::string* seen = nullptr
);

//  For the test of a build: reads names drawn here (a few letters with the
//  male mark, the female mark or neither behind them, as large as in a 720p
//  and in a 1080p capture, sharp and blurred). Returns how many were read
//  wrong, with a line on each in "failures"; "cases": how many were read.
int check_reader(int& cases, std::string& failures);


}
}
}
}
#endif

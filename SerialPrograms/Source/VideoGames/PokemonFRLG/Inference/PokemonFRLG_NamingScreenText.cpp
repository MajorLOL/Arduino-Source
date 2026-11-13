/*  Pokemon FRLG Naming Screen Text
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include "PokemonFRLG_NamingScreenText.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace NamingScreen{


namespace{

const char32_t ELLIPSIS = 0x2026;
const char32_t QUOTE_DOUBLE_LEFT = 0x201C;
const char32_t QUOTE_DOUBLE_RIGHT = 0x201D;
const char32_t QUOTE_LEFT = 0x2018;
const char32_t QUOTE_RIGHT = 0x2019;
const char32_t MALE = 0x2642;
const char32_t FEMALE = 0x2640;

//  naming_screen.c, sKeyboardChars: the rows are filled up to the page's
//  columns with the game's character 0, which is its space, so the empty
//  keys type a space too.
const char32_t* const KEYBOARD[PAGES][ROWS] = {
    {U"ABCDEF .", U"GHIJKL ,", U"MNOPQRS ", U"TUVWXYZ "},
    {U"abcdef .", U"ghijkl ,", U"mnopqrs ", U"tuvwxyz "},
    {U"01234 ", U"56789 ", U"!?\u2642\u2640/-", U"\u2026\u201C\u201D\u2018\u2019 "},
};
const int COLUMNS[PAGES] = {8, 8, 6};

//  The boxes of the PC.
const int BOXES = 14;


//  The game's font (pokefirered, graphics/fonts/latin_normal.png), the
//  characters of the keyboard. Of each character's cell: how wide it is,
//  its dark pixels (rows 0 to 12) and its shadow pixels (rows 0 to 13, the
//  lighter ones to the right of and under the dark ones), one byte a row,
//  the leftmost of its 6 columns in bit 5. No character has a pixel
//  further right or lower. The closest pairs differ in one dark pixel (c
//  and o) or two (. and the comma, D and O, G and O), so on the naming
//  screen a character only counts when it is exactly its glyph.
const int GLYPH_ROWS = 13;
const int SHADOW_ROWS = 14;
const int GLYPH_COLUMNS = 6;
const int SPACE_WIDTH = 6;
struct Glyph{
    char32_t character;
    int width;
    uint8_t rows[GLYPH_ROWS];
    uint8_t shadow[SHADOW_ROWS];
};
const Glyph GLYPHS[] = {
    {U'0', 6,
        {0x00, 0x00, 0x1c, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x02, 0x1d, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {U'1', 6,
        {0x00, 0x00, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x04, 0x04, 0x14, 0x04, 0x04, 0x04, 0x04, 0x04, 0x02, 0x1e, 0x00, 0x00}},
    {U'2', 6,
        {0x00, 0x00, 0x1c, 0x22, 0x22, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3e, 0x00, 0x00},
        {0x00, 0x00, 0x02, 0x1d, 0x11, 0x31, 0x03, 0x06, 0x0c, 0x18, 0x01, 0x3f, 0x00, 0x00}},
    {U'3', 6,
        {0x00, 0x00, 0x1c, 0x22, 0x02, 0x02, 0x0c, 0x02, 0x02, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x02, 0x1d, 0x31, 0x01, 0x02, 0x0d, 0x01, 0x01, 0x23, 0x1e, 0x00, 0x00}},
    {U'4', 6,
        {0x00, 0x00, 0x0c, 0x14, 0x14, 0x24, 0x24, 0x24, 0x3e, 0x04, 0x04, 0x00, 0x00},
        {0x00, 0x00, 0x02, 0x0a, 0x0a, 0x1a, 0x12, 0x12, 0x01, 0x3b, 0x02, 0x06, 0x00, 0x00}},
    {U'5', 6,
        {0x00, 0x00, 0x3e, 0x20, 0x20, 0x20, 0x3c, 0x02, 0x02, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x01, 0x1f, 0x10, 0x10, 0x02, 0x3d, 0x01, 0x01, 0x23, 0x1e, 0x00, 0x00}},
    {U'6', 6,
        {0x00, 0x00, 0x1c, 0x22, 0x20, 0x20, 0x3c, 0x22, 0x22, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x02, 0x1d, 0x13, 0x10, 0x02, 0x1d, 0x11, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {U'7', 6,
        {0x00, 0x00, 0x3e, 0x02, 0x02, 0x02, 0x04, 0x04, 0x08, 0x08, 0x08, 0x00, 0x00},
        {0x00, 0x00, 0x01, 0x3d, 0x01, 0x01, 0x03, 0x02, 0x06, 0x04, 0x04, 0x0c, 0x00, 0x00}},
    {U'8', 6,
        {0x00, 0x00, 0x1c, 0x22, 0x22, 0x22, 0x1c, 0x22, 0x22, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x02, 0x1d, 0x11, 0x11, 0x22, 0x1d, 0x11, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {U'9', 6,
        {0x00, 0x00, 0x1c, 0x22, 0x22, 0x22, 0x1e, 0x02, 0x02, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x02, 0x1d, 0x11, 0x11, 0x01, 0x1d, 0x01, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {U'!', 6,
        {0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x08, 0x00, 0x00},
        {0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04, 0x04, 0x0c, 0x00, 0x00}},
    {U'?', 6,
        {0x00, 0x1c, 0x22, 0x22, 0x22, 0x0c, 0x08, 0x08, 0x00, 0x08, 0x08, 0x00, 0x00},
        {0x00, 0x02, 0x1d, 0x11, 0x11, 0x33, 0x06, 0x04, 0x00, 0x04, 0x04, 0x0c, 0x00, 0x00}},
    {U'.', 5,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x1c, 0x00, 0x00}},
    {U'-', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {0x2026, 6,     //  the ellipsis
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x3f, 0x00, 0x00}},
    {0x201C, 6,     //  the left double quote
        {0x12, 0x24, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x09, 0x12, 0x09, 0x09, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {0x201D, 6,     //  the right double quote
        {0x36, 0x36, 0x12, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x09, 0x09, 0x2d, 0x1b, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {0x2018, 3,     //  the left single quote
        {0x10, 0x20, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x08, 0x10, 0x08, 0x08, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {0x2019, 3,     //  the right single quote
        {0x30, 0x30, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {0x08, 0x08, 0x28, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {0x2642, 6,     //  the male sign
        {0x00, 0x08, 0x1c, 0x2a, 0x08, 0x08, 0x1c, 0x22, 0x22, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x04, 0x02, 0x15, 0x37, 0x04, 0x02, 0x1d, 0x11, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {0x2640, 6,     //  the female sign
        {0x00, 0x1c, 0x22, 0x22, 0x22, 0x1c, 0x08, 0x3e, 0x08, 0x08, 0x08, 0x00, 0x00},
        {0x00, 0x02, 0x1d, 0x11, 0x11, 0x23, 0x16, 0x01, 0x37, 0x04, 0x04, 0x0c, 0x00, 0x00}},
    {U',', 5,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x08, 0x10},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x14, 0x0c, 0x18}},
    {U'/', 6,
        {0x00, 0x02, 0x02, 0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x00, 0x00},
        {0x00, 0x01, 0x01, 0x03, 0x02, 0x06, 0x04, 0x0c, 0x08, 0x18, 0x10, 0x30, 0x00, 0x00}},
    {U'A', 6,
        {0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x22, 0x3e, 0x22, 0x22, 0x22, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x02, 0x1d, 0x11, 0x11, 0x01, 0x1d, 0x11, 0x11, 0x33, 0x00, 0x00}},
    {U'B', 6,
        {0x00, 0x00, 0x00, 0x3c, 0x22, 0x22, 0x3c, 0x22, 0x22, 0x22, 0x3c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x02, 0x1d, 0x11, 0x02, 0x1d, 0x11, 0x11, 0x03, 0x3e, 0x00, 0x00}},
    {U'C', 6,
        {0x00, 0x00, 0x00, 0x1c, 0x22, 0x20, 0x20, 0x20, 0x20, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x02, 0x1d, 0x13, 0x10, 0x10, 0x10, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {U'D', 6,
        {0x00, 0x00, 0x00, 0x3c, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x02, 0x1d, 0x11, 0x11, 0x11, 0x11, 0x11, 0x03, 0x3e, 0x00, 0x00}},
    {U'E', 6,
        {0x00, 0x00, 0x00, 0x3e, 0x20, 0x20, 0x3c, 0x20, 0x20, 0x20, 0x3e, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x01, 0x1f, 0x10, 0x02, 0x1e, 0x10, 0x10, 0x01, 0x3f, 0x00, 0x00}},
    {U'F', 6,
        {0x00, 0x00, 0x00, 0x3e, 0x20, 0x20, 0x3c, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x01, 0x1f, 0x10, 0x02, 0x1e, 0x10, 0x10, 0x10, 0x30, 0x00, 0x00}},
    {U'G', 6,
        {0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x20, 0x26, 0x22, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x02, 0x1d, 0x11, 0x10, 0x11, 0x15, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {U'H', 6,
        {0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x3e, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x01, 0x1d, 0x11, 0x11, 0x11, 0x33, 0x00, 0x00}},
    {U'I', 6,
        {0x00, 0x00, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x3e, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x01, 0x37, 0x04, 0x04, 0x04, 0x04, 0x04, 0x01, 0x3f, 0x00, 0x00}},
    {U'J', 6,
        {0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {U'K', 6,
        {0x00, 0x00, 0x00, 0x22, 0x24, 0x28, 0x30, 0x30, 0x28, 0x24, 0x22, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x11, 0x12, 0x14, 0x08, 0x08, 0x14, 0x12, 0x11, 0x33, 0x00, 0x00}},
    {U'L', 6,
        {0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3e, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x01, 0x3f, 0x00, 0x00}},
    {U'M', 6,
        {0x00, 0x00, 0x00, 0x22, 0x36, 0x36, 0x2a, 0x2a, 0x22, 0x22, 0x22, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x11, 0x09, 0x09, 0x15, 0x15, 0x1d, 0x11, 0x11, 0x33, 0x00, 0x00}},
    {U'N', 6,
        {0x00, 0x00, 0x00, 0x22, 0x32, 0x32, 0x2a, 0x26, 0x26, 0x22, 0x22, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x11, 0x09, 0x09, 0x15, 0x19, 0x11, 0x15, 0x11, 0x33, 0x00, 0x00}},
    {U'O', 6,
        {0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x02, 0x1d, 0x11, 0x11, 0x11, 0x11, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {U'P', 6,
        {0x00, 0x00, 0x00, 0x3c, 0x22, 0x22, 0x22, 0x3c, 0x20, 0x20, 0x20, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x02, 0x1d, 0x11, 0x11, 0x03, 0x1e, 0x10, 0x10, 0x30, 0x00, 0x00}},
    {U'Q', 6,
        {0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x22, 0x22, 0x3a, 0x26, 0x1d, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x02, 0x1d, 0x11, 0x11, 0x11, 0x05, 0x19, 0x22, 0x1d, 0x00, 0x00}},
    {U'R', 6,
        {0x00, 0x00, 0x00, 0x3c, 0x22, 0x22, 0x22, 0x3c, 0x24, 0x22, 0x22, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x02, 0x1d, 0x11, 0x11, 0x03, 0x1a, 0x15, 0x11, 0x33, 0x00, 0x00}},
    {U'S', 6,
        {0x00, 0x00, 0x00, 0x1c, 0x22, 0x20, 0x18, 0x04, 0x02, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x02, 0x1d, 0x10, 0x24, 0x1a, 0x01, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {U'T', 6,
        {0x00, 0x00, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x01, 0x37, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0c, 0x00, 0x00}},
    {U'U', 6,
        {0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {U'V', 6,
        {0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x2b, 0x0a, 0x16, 0x04, 0x0c, 0x00, 0x00}},
    {U'W', 6,
        {0x00, 0x00, 0x00, 0x22, 0x22, 0x2a, 0x2a, 0x2a, 0x36, 0x36, 0x22, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x15, 0x09, 0x09, 0x1d, 0x33, 0x00, 0x00}},
    {U'X', 6,
        {0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x08, 0x14, 0x22, 0x22, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x11, 0x11, 0x0a, 0x16, 0x04, 0x0a, 0x15, 0x11, 0x33, 0x00, 0x00}},
    {U'Y', 6,
        {0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x11, 0x11, 0x2b, 0x0a, 0x16, 0x04, 0x04, 0x04, 0x0c, 0x00, 0x00}},
    {U'Z', 6,
        {0x00, 0x00, 0x00, 0x3e, 0x02, 0x04, 0x08, 0x08, 0x10, 0x20, 0x3e, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x01, 0x3d, 0x03, 0x06, 0x04, 0x0c, 0x18, 0x01, 0x3f, 0x00, 0x00}},
    {U'a', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x22, 0x22, 0x26, 0x1a, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x1d, 0x11, 0x11, 0x25, 0x1b, 0x00, 0x00}},
    {U'b', 6,
        {0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x3c, 0x22, 0x22, 0x22, 0x3c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x02, 0x1d, 0x11, 0x11, 0x03, 0x3e, 0x00, 0x00}},
    {U'c', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x22, 0x20, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1d, 0x10, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {U'd', 6,
        {0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x1e, 0x22, 0x22, 0x22, 0x1e, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x1d, 0x11, 0x11, 0x21, 0x1f, 0x00, 0x00}},
    {U'e', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x22, 0x3e, 0x20, 0x1e, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1d, 0x01, 0x1e, 0x21, 0x1e, 0x00, 0x00}},
    {U'f', 5,
        {0x00, 0x00, 0x00, 0x0c, 0x10, 0x10, 0x3c, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x02, 0x0c, 0x08, 0x02, 0x2e, 0x08, 0x08, 0x08, 0x18, 0x00, 0x00}},
    {U'g', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x22, 0x1e, 0x02, 0x1c},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1d, 0x11, 0x11, 0x21, 0x1d, 0x03, 0x1e}},
    {U'h', 6,
        {0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x3c, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x02, 0x1d, 0x11, 0x11, 0x11, 0x33, 0x00, 0x00}},
    {U'i', 4,
        {0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x08, 0x18, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x18, 0x00, 0x00}},
    {U'j', 6,
        {0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x24, 0x24, 0x18},
        {0x00, 0x00, 0x00, 0x02, 0x06, 0x00, 0x02, 0x02, 0x02, 0x02, 0x12, 0x12, 0x26, 0x1c}},
    {U'k', 5,
        {0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x24, 0x28, 0x30, 0x28, 0x24, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x12, 0x14, 0x08, 0x14, 0x1a, 0x36, 0x00, 0x00}},
    {U'l', 5,
        {0x00, 0x00, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x04, 0x14, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0c, 0x00, 0x00}},
    {U'm', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x2a, 0x2a, 0x2a, 0x2a, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x15, 0x15, 0x15, 0x15, 0x2b, 0x00, 0x00}},
    {U'n', 5,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x24, 0x24, 0x24, 0x24, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x1a, 0x12, 0x12, 0x12, 0x36, 0x00, 0x00}},
    {U'o', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x22, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1d, 0x11, 0x11, 0x23, 0x1e, 0x00, 0x00}},
    {U'p', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x22, 0x22, 0x22, 0x3c, 0x20, 0x20},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1d, 0x11, 0x11, 0x03, 0x1e, 0x10, 0x30}},
    {U'q', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x22, 0x22, 0x22, 0x1e, 0x02, 0x02},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x1d, 0x11, 0x11, 0x21, 0x1d, 0x01, 0x03}},
    {U'r', 5,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x30, 0x20, 0x20, 0x20, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x0e, 0x18, 0x10, 0x10, 0x30, 0x00, 0x00}},
    {U's', 5,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x20, 0x18, 0x04, 0x3c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1e, 0x24, 0x1a, 0x02, 0x3e, 0x00, 0x00}},
    {U't', 5,
        {0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x3c, 0x10, 0x10, 0x10, 0x1c, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x02, 0x2e, 0x08, 0x08, 0x02, 0x1e, 0x00, 0x00}},
    {U'u', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x24, 0x24, 0x24, 0x1a, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x12, 0x12, 0x12, 0x24, 0x1a, 0x00, 0x00}},
    {U'v', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x14, 0x08, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x2b, 0x0a, 0x16, 0x0c, 0x00, 0x00}},
    {U'w', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2a, 0x2a, 0x2a, 0x2a, 0x14, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x15, 0x15, 0x15, 0x2b, 0x1e, 0x00, 0x00}},
    {U'x', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x14, 0x08, 0x14, 0x22, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x2b, 0x14, 0x0a, 0x15, 0x33, 0x00, 0x00}},
    {U'y', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x1e, 0x02, 0x3c},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x21, 0x1d, 0x03, 0x1e}},
    {U'z', 6,
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x04, 0x08, 0x10, 0x3e, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3b, 0x06, 0x0c, 0x01, 0x3f, 0x00, 0x00}},
};

const Glyph* find_glyph(char32_t character){
    for (const Glyph& glyph : GLYPHS){
        if (glyph.character == character){
            return &glyph;
        }
    }
    return nullptr;
}

int bits_set(unsigned value){
    int count = 0;
    while (value != 0){
        count += (int)(value & 1u);
        value >>= 1;
    }
    return count;
}

//  How dark: 0 for white. The text is (98, 98, 98): 157. Its shadow is
//  (213, 213, 205): 42. The underscores are (56, 56, 56) over
//  (115, 115, 115): 199 and 140.
int darkness(uint32_t color){
    const unsigned r = (color >> 16) & 0xff;
    const unsigned g = (color >> 8) & 0xff;
    const unsigned b = color & 0xff;
    return 255 - (int)std::max(r, std::max(g, b));
}
const int DARK = 95;

int presses(int from_x, int from_y, int x, int y){
    const int rows = std::abs(y - from_y);
    return std::abs(x - from_x) + std::min(rows, ROWS - rows);
}


bool decode_utf8(const std::string& text, std::u32string& out){
    out.clear();
    size_t i = 0;
    while (i < text.size()){
        const unsigned char lead = (unsigned char)text[i];
        char32_t code;
        size_t more;
        if (lead < 0x80){
            code = lead;
            more = 0;
        }else if ((lead & 0xe0) == 0xc0){
            code = lead & 0x1fu;
            more = 1;
        }else if ((lead & 0xf0) == 0xe0){
            code = lead & 0x0fu;
            more = 2;
        }else if ((lead & 0xf8) == 0xf0){
            code = lead & 0x07u;
            more = 3;
        }else{
            return false;
        }
        if (i + more >= text.size() + (more == 0 ? 1 : 0)){
            return false;
        }
        for (size_t k = 1; k <= more; k++){
            const unsigned char next = (unsigned char)text[i + k];
            if ((next & 0xc0) != 0x80){
                return false;
            }
            code = (code << 6) | (char32_t)(next & 0x3fu);
        }
        out += code;
        i += more + 1;
    }
    return true;
}

void append_utf8(std::string& out, char32_t code){
    if (code < 0x80){
        out += (char)code;
    }else if (code < 0x800){
        out += (char)(0xc0 | (code >> 6));
        out += (char)(0x80 | (code & 0x3f));
    }else if (code < 0x10000){
        out += (char)(0xe0 | (code >> 12));
        out += (char)(0x80 | ((code >> 6) & 0x3f));
        out += (char)(0x80 | (code & 0x3f));
    }else{
        out += (char)(0xf0 | (code >> 18));
        out += (char)(0x80 | ((code >> 12) & 0x3f));
        out += (char)(0x80 | ((code >> 6) & 0x3f));
        out += (char)(0x80 | (code & 0x3f));
    }
}

bool is_blank(char32_t c){
    return c == U' ' || c == U'\t' || c == 0xA0;
}

std::u32string trimmed(const std::u32string& text){
    size_t from = 0;
    size_t to = text.size();
    while (from < to && is_blank(text[from])){
        from++;
    }
    while (to > from && is_blank(text[to - 1])){
        to--;
    }
    return text.substr(from, to - from);
}

//  "C C U n n R E o": characters with a space between each two.
bool is_spaced_out(const std::u32string& text){
    if (text.size() < 3 || text.size() % 2 == 0){
        return false;
    }
    for (size_t i = 0; i < text.size(); i++){
        if ((i % 2 == 1) != (text[i] == U' ')){
            return false;
        }
    }
    return true;
}

}



int columns(int page){
    return page >= 0 && page < PAGES ? COLUMNS[page] : 0;
}

char32_t key(int page, int x, int y){
    if (page < 0 || page >= PAGES || y < 0 || y >= ROWS || x < 0 || x >= COLUMNS[page]){
        return 0;
    }
    return KEYBOARD[page][y][x];
}

bool can_type(char32_t character){
    return page_for(character, PAGE_UPPER) >= 0;
}

int page_for(char32_t character, int page){
    if (character == 0 || page < 0 || page >= PAGES){
        return -1;
    }
    for (int k = 0; k < PAGES; k++){
        const int candidate = (page + k) % PAGES;
        int x = 0;
        int y = 0;
        if (nearest_key(character, candidate, 0, 0, x, y)){
            return candidate;
        }
    }
    return -1;
}

bool nearest_key(char32_t character, int page, int from_x, int from_y, int& x, int& y){
    if (character == 0 || page < 0 || page >= PAGES){
        return false;
    }
    int best = -1;
    for (int ky = 0; ky < ROWS; ky++){
        for (int kx = 0; kx < COLUMNS[page]; kx++){
            if (KEYBOARD[page][ky][kx] != character){
                continue;
            }
            const int cost = presses(from_x, from_y, kx, ky);
            if (best < 0 || cost < best){
                best = cost;
                x = kx;
                y = ky;
            }
        }
    }
    return best >= 0;
}

bool nearest_visible_key(int page, int from_x, int from_y, int& x, int& y){
    if (page < 0 || page >= PAGES){
        return false;
    }
    int best = -1;
    for (int ky = 0; ky < ROWS; ky++){
        for (int kx = 0; kx < COLUMNS[page]; kx++){
            if (KEYBOARD[page][ky][kx] == U' '){
                continue;
            }
            const int cost = presses(from_x, from_y, kx, ky);
            if (best < 0 || cost < best){
                best = cost;
                x = kx;
                y = ky;
            }
        }
    }
    return best >= 0;
}

std::vector<TypingStep> typing_steps(const std::u32string& name, int max_characters, Keys keys){
    std::vector<TypingStep> steps;
    const int places = std::max(1, std::min(max_characters, MAX_CHARACTERS));
    if (name.empty() || (int)name.size() > places ||
        keys.page < 0 || keys.page >= PAGES || keys.y < 0 || keys.y >= ROWS ||
        keys.x < 0 || keys.x >= COLUMNS[keys.page]
    ){
        return steps;
    }
    std::u32string typed;
    for (size_t i = 0; i < name.size(); i++){
        const char32_t wanted = name[i];
        const bool space_in_the_last_place = wanted == U' ' && (int)i == places - 1;
        TypingStep step;
        int x = 0;
        int y = 0;
        char32_t character = wanted;
        if (space_in_the_last_place){
            if (!nearest_visible_key(keys.page, keys.x, keys.y, x, y)){
                return {};
            }
            character = KEYBOARD[keys.page][y][x];
        }else{
            const int page = page_for(wanted, keys.page);
            if (page < 0){
                return {};
            }
            while (keys.page != page){
                step.presses.push_back(Press::SELECT);
                keys.page = (keys.page + 1) % PAGES;
                //  On a page with fewer columns the cursor moves in to its
                //  last one (MainState_WaitPageSwap).
                keys.x = std::min(keys.x, COLUMNS[keys.page] - 1);
            }
            if (!nearest_key(wanted, keys.page, keys.x, keys.y, x, y)){
                return {};
            }
        }
        //  Along the row first (never around its end: the buttons are
        //  there), then up or down, around the end if that is shorter.
        for (; keys.x < x; keys.x++){
            step.presses.push_back(Press::RIGHT);
        }
        for (; keys.x > x; keys.x--){
            step.presses.push_back(Press::LEFT);
        }
        const int down = ((y - keys.y) % ROWS + ROWS) % ROWS;
        if (down <= ROWS / 2){
            step.presses.insert(step.presses.end(), (size_t)down, Press::DOWN);
        }else{
            step.presses.insert(step.presses.end(), (size_t)(ROWS - down), Press::UP);
        }
        keys.y = y;
        step.presses.push_back(Press::A);
        typed += character;
        step.page = keys.page;
        step.shows = typed;
        steps.push_back(step);

        if (space_in_the_last_place){
            TypingStep space;
            space.presses.push_back(Press::LEFT);
            space.presses.push_back(Press::A);
            typed.back() = U' ';
            space.page = keys.page;
            space.shows = typed;
            steps.push_back(space);
        }
    }
    return steps;
}

std::vector<Press> restart_presses(int max_characters){
    const int places = std::max(1, std::min(max_characters, MAX_CHARACTERS));
    std::vector<Press> out((size_t)places + 1, Press::B);
    out.push_back(Press::START);
    out.push_back(Press::RIGHT);
    return out;
}

std::string to_utf8(const std::u32string& text){
    std::string out;
    for (char32_t c : text){
        append_utf8(out, c);
    }
    return out;
}

std::string in_brackets(const std::u32string& name){
    return "[" + to_utf8(name) + "]";
}

std::string character_name(char32_t character){
    switch (character){
    case 0:                     return "something else";
    case U' ':                  return "a space";
    case ELLIPSIS:              return "the ellipsis";
    case QUOTE_DOUBLE_LEFT:     return "the left double quote";
    case QUOTE_DOUBLE_RIGHT:    return "the right double quote";
    case QUOTE_LEFT:            return "the left single quote";
    case QUOTE_RIGHT:           return "the right single quote";
    case MALE:                  return "the male sign";
    case FEMALE:                return "the female sign";
    case U'.':                  return "a period";
    case U',':                  return "a comma";
    default:                    break;
    }
    std::string out = "\"";
    append_utf8(out, character);
    return out + "\"";
}


bool parse_name_lines(
    const std::string& utf8, int max_characters,
    std::vector<NameLine>& lines, std::string& error
){
    lines.clear();
    error.clear();
    max_characters = std::max(1, std::min(max_characters, MAX_CHARACTERS));

    int line_number = 0;
    size_t at = 0;
    while (at <= utf8.size()){
        size_t end = utf8.find('\n', at);
        if (end == std::string::npos){
            end = utf8.size();
        }
        std::string raw = utf8.substr(at, end - at);
        at = end + 1;
        line_number++;
        while (!raw.empty() && raw.back() == '\r'){
            raw.pop_back();
        }
        const std::string where = "Line " + std::to_string(line_number) + ": ";

        std::u32string text;
        if (!decode_utf8(raw, text)){
            error = where + "this is not text (UTF-8).";
            return false;
        }
        for (char32_t& c : text){
            if (c == U'\t' || c == 0xA0){
                c = U' ';
            }
        }
        if (trimmed(text).empty()){
            continue;
        }

        NameLine entry;
        entry.line = line_number;

        //  "Box 6:" in front. The keyboard has no colon, so a colon is
        //  never part of a name.
        const size_t colon = text.find(U':');
        const size_t bracket = text.find(U'[');
        bool open_ended = false;
        if (colon != std::u32string::npos && (bracket == std::u32string::npos || colon < bracket)){
            const std::u32string head = trimmed(text.substr(0, colon));
            size_t i = 0;
            const bool word =
                head.size() >= 3 &&
                (head[0] == U'B' || head[0] == U'b') &&
                (head[1] == U'O' || head[1] == U'o') &&
                (head[2] == U'X' || head[2] == U'x');
            i = 3;
            while (word && i < head.size() && head[i] == U' '){
                i++;
            }
            int number = 0;
            size_t digits = 0;
            while (word && i < head.size() && head[i] >= U'0' && head[i] <= U'9' && digits < 3){
                number = number * 10 + (int)(head[i] - U'0');
                i++;
                digits++;
            }
            if (word && i < head.size() && head[i] == U'+'){
                open_ended = true;
                i++;
            }
            if (!word || digits == 0 || i != head.size()){
                error = where + "what is in front of the colon is not \"Box\" and a number (a name cannot have a colon).";
                return false;
            }
            if (open_ended){
                continue;       //  "Box 8+: (anything)"
            }
            if (number < 1 || number > BOXES){
                error = where + "there is no box " + std::to_string(number) + " (the PC has boxes 1 to " + std::to_string(BOXES) + ").";
                return false;
            }
            entry.box = number;
            text = text.substr(colon + 1);
        }

        std::u32string name;
        const size_t open = text.find(U'[');
        if (open != std::u32string::npos){
            const size_t close = text.rfind(U']');
            if (close == std::u32string::npos || close < open){
                error = where + "a [ without a ] after it.";
                return false;
            }
            name = text.substr(open + 1, close - open - 1);
        }else{
            name = trimmed(text);
            if (entry.box != 0 && is_spaced_out(name)){
                error = where + "\"" + to_utf8(name) + "\" can be read in two ways (with or without the spaces). "
                    "Write the name between brackets, as in [" + to_utf8(name.substr(0, 1)) + "...].";
                return false;
            }
        }

        for (size_t i = 0; i < name.size(); i++){
            char32_t& c = name[i];
            if (c == U'_'){
                c = U' ';
            }else if (c == U'\''){
                c = QUOTE_RIGHT;        //  (the key the game draws as ')
            }
            if (can_type(c)){
                continue;
            }
            std::string what = "\"";
            append_utf8(what, c);
            what += "\"";
            error = where + "the game's keyboard has no " + what + " (character " + std::to_string(i + 1) + " of the name).";
            if (c == U'"'){
                error += " Its double quotes are the left one and the right one: write the one that is meant.";
            }else if (c == U'[' || c == U']'){
                error += " Brackets go around the whole name only.";
            }
            return false;
        }
        if (name.empty()){
            error = where + "no name.";
            return false;
        }
        if ((int)name.size() > max_characters){
            error = where + in_brackets(name) + " has " + std::to_string(name.size()) + " characters; a name has at most " +
                std::to_string(max_characters) + ".";
            if (is_spaced_out(name)){
                error += " If the spaces are only there to set the characters apart, write the name without them or between brackets.";
            }
            return false;
        }
        if (name.find_first_not_of(U' ') == std::u32string::npos){
            error = where + "a name of spaces only: the game does not keep such a name.";
            return false;
        }
        entry.name = name;
        lines.push_back(entry);
    }

    if (lines.empty()){
        error = "No names: write one name on each line.";
        return false;
    }
    if ((int)lines.size() > BOXES){
        error = std::to_string(lines.size()) + " names, but the PC has " + std::to_string(BOXES) + " boxes.";
        return false;
    }
    size_t numbered = 0;
    for (const NameLine& line : lines){
        numbered += line.box != 0 ? 1 : 0;
    }
    if (numbered != 0 && numbered != lines.size()){
        error = "Some lines say which box they are for (\"Box 3:\") and some do not: write it on all of them or on none.";
        return false;
    }
    for (size_t i = 1; i < lines.size() && numbered != 0; i++){
        if (lines[i].box <= lines[i - 1].box){
            error = "Line " + std::to_string(lines[i].line) + ": box " + std::to_string(lines[i].box) +
                " comes after box " + std::to_string(lines[i - 1].box) + ". Write the boxes in rising order, each once.";
            return false;
        }
    }
    return true;
}



Pixel game_pixels(const uint32_t* data, size_t bytes_per_row, size_t width, size_t height){
    if (data == nullptr || width < 240 || height < 160){
        return [](double, double) -> uint32_t{ return 0; };
    }
    const double sx = (double)width / 240.0;
    const double sy = (double)height / 160.0;
    //  The picture pixel in the middle of the game pixel (6.5 picture
    //  pixels a game pixel at 1080p); from 8 on, the 3 by 3 around it.
    const int reach = (int)(std::min(sx, sy) / 8.0);
    return [=](double x, double y) -> uint32_t{
        const int cx = (int)(x * sx);
        const int cy = (int)(y * sy);
        unsigned r = 0;
        unsigned g = 0;
        unsigned b = 0;
        unsigned count = 0;
        for (int py = cy - reach; py <= cy + reach; py++){
            if (py < 0 || py >= (int)height){
                continue;
            }
            const uint32_t* row = (const uint32_t*)((const char*)data + (size_t)py * bytes_per_row);
            for (int px = cx - reach; px <= cx + reach; px++){
                if (px < 0 || px >= (int)width){
                    continue;
                }
                const uint32_t p = row[px];
                r += (p >> 16) & 0xff;
                g += (p >> 8) & 0xff;
                b += p & 0xff;
                count++;
            }
        }
        if (count == 0){
            return 0;
        }
        return ((r / count) << 16) | ((g / count) << 8) | (b / count);
    };
}

//  The screen (naming_screen.c): the keyboard's window, tiles (3, 10) to
//  (22, 18), filled with the page's own colour of the keyboard's palette
//  (index 13, 14 or 15: sFillValues), the white box with the title and the
//  name above it, the striped yellow background left and right.
int keyboard_page(const Pixel& pixel){
    auto at = [&](int x, int y){
        return pixel(x + 0.5, y + 0.5);
    };
    //  The striped background: at its left and right edges, two rows of
    //  every four are a strong yellow, (215, 206, 83) to (230, 222, 90).
    int yellow = 0;
    int total = 0;
    for (int y = 60; y < 150; y++){
        for (int x : {1, 2, 3, 236, 237, 238}){
            const uint32_t p = at(x, y);
            total++;
            yellow += ((p >> 16) & 0xff) > 190 && ((p >> 8) & 0xff) > 180 && (p & 0xff) < 160 ? 1 : 0;
        }
    }
    if (yellow * 100 < total * 25){
        return -1;
    }
    //  The white box.
    int white = 0;
    total = 0;
    for (int y = 18; y < 52; y += 2){
        for (int x = 50; x < 190; x += 2){
            const uint32_t p = at(x, y);
            total++;
            white += ((p >> 16) & 0xff) > 225 && ((p >> 8) & 0xff) > 225 && (p & 0xff) > 225 ? 1 : 0;
        }
    }
    if (white * 100 < total * 45){
        return -1;
    }
    //  The panel: (123, 172, 197), (213, 156, 115) or (148, 189, 106), with
    //  the keys' white letters and their shadows on it.
    const int PANEL[PAGES][3] = {
        {123, 172, 197},
        {213, 156, 115},
        {148, 189, 106},
    };
    int counts[PAGES] = {0, 0, 0};
    total = 0;
    for (int y = 80; y < 140; y += 2){
        for (int x = 40; x < 160; x += 2){
            const uint32_t p = at(x, y);
            const int r = (int)((p >> 16) & 0xff);
            const int g = (int)((p >> 8) & 0xff);
            const int b = (int)(p & 0xff);
            total++;
            for (int page = 0; page < PAGES; page++){
                if (std::abs(r - PANEL[page][0]) < 20 && std::abs(g - PANEL[page][1]) < 20 && std::abs(b - PANEL[page][2]) < 20){
                    counts[page]++;
                }
            }
        }
    }
    for (int page = 0; page < PAGES; page++){
        if (counts[page] * 10 >= total * 6){
            return page;
        }
    }
    return -1;
}



std::string Entry::summary() const{
    if (!found){
        return "(the name's field is not found)";
    }
    std::u32string text;
    for (int i = 0; i < slots; i++){
        text += characters[i] != 0 ? characters[i] : (char32_t)0xFFFD;
    }
    std::string out = in_brackets(text);
    if (cursor >= 0){
        out += ", the next character at " + std::to_string(cursor + 1);
    }else{
        out += ", the underscores do not tell where the next character goes";
    }
    for (int i = 0; i < slots; i++){
        if (characters[i] == 0){
            out += "; character " + std::to_string(i + 1) + " is " + std::to_string(differences[i]) +
                " pixel" + (differences[i] == 1 ? "" : "s") + " off " + character_name(closest[i]);
        }
    }
    return out;
}

//  The field (naming_screen.c): the name is printed one character every 8
//  pixels from x = (240 - 8 * characters) / 2 + 6 at y = 49, 14 rows high,
//  into a window that ends at y = 63. Under every character is an
//  underscore, a sprite: a bar 6 pixels wide on rows 62 and 63. The
//  underscore of the slot the next character goes to moves up and down 1 to
//  3 rows lower than that (SpriteCB_Underscore), so it never has a dark
//  pixel on row 62; when the name is full, that slot is the last one.
//  (Measured on a capture of the Switch: "RIVAL's NAME?" with AAAAAAA.)
Entry read_entry(const Pixel& pixel, int max_characters){
    Entry entry;
    const int slots = std::max(1, std::min(max_characters, MAX_CHARACTERS));
    entry.slots = slots;
    const int base = (240 - slots * 8) / 2 + 6;
    const int BAR_ROW = 62;
    const int TEXT_ROW = 49;

    auto dark = [&](double x, double y){
        return darkness(pixel(x + 0.5, y + 0.5));
    };

    //  Where the row of underscores is, to an eighth of a pixel: the game
    //  box of the settings can be off by a pixel or two. The bars on their
    //  two rows, nothing between them and nothing right under them (all but
    //  one of them, and that one is nowhere near then).
    //  An offset counts for as much as the worst of itself and the offsets
    //  up to a quarter of a pixel around it: on a picture scaled without
    //  smoothing, an offset that lands on the edge between two rows can
    //  look best (it takes the darker row of the bar twice) while the name
    //  would be read across that edge too.
    const int STEPS = 16;           //  +-2 pixels
    const int SIDE = 2 * STEPS + 1;
    const int AROUND = 2;
    std::vector<int> scores((size_t)SIDE * (size_t)SIDE);
    for (int iy = 0; iy < SIDE; iy++){
        for (int ix = 0; ix < SIDE; ix++){
            const double dx = (ix - STEPS) * 0.125;
            const double dy = (iy - STEPS) * 0.125;
            int score = 0;
            for (int slot = 0; slot < slots; slot++){
                const int x0 = base + slot * 8;
                for (int column = 0; column < 8; column++){
                    const int bar = dark(x0 + column + dx, BAR_ROW + dy) + dark(x0 + column + dx, BAR_ROW + 1 + dy);
                    if (column < 6){
                        score += bar - dark(x0 + column + dx, BAR_ROW + 2 + dy);
                    }else{
                        score -= bar;
                    }
                }
            }
            scores[(size_t)iy * (size_t)SIDE + (size_t)ix] = score;
        }
    }
    std::vector<int> worst((size_t)SIDE * (size_t)SIDE);
    int best = 0;
    for (int iy = 0; iy < SIDE; iy++){
        for (int ix = 0; ix < SIDE; ix++){
            int least = scores[(size_t)iy * (size_t)SIDE + (size_t)ix];
            for (int ny = std::max(0, iy - AROUND); ny <= std::min(SIDE - 1, iy + AROUND); ny++){
                for (int nx = std::max(0, ix - AROUND); nx <= std::min(SIDE - 1, ix + AROUND); nx++){
                    least = std::min(least, scores[(size_t)ny * (size_t)SIDE + (size_t)nx]);
                }
            }
            worst[(size_t)iy * (size_t)SIDE + (size_t)ix] = least;
            best = std::max(best, least);
        }
    }
    //  A bar is 6 pixels of 199 over 6 of 140: 2034.
    if (best < (slots - 1) * 2034 * 5 / 10){
        return entry;
    }
    double sum_dx = 0;
    double sum_dy = 0;
    int ties = 0;
    for (int iy = 0; iy < SIDE; iy++){
        for (int ix = 0; ix < SIDE; ix++){
            if (worst[(size_t)iy * (size_t)SIDE + (size_t)ix] * 100 >= best * 98){
                sum_dx += (ix - STEPS) * 0.125;
                sum_dy += (iy - STEPS) * 0.125;
                ties++;
            }
        }
    }
    entry.found = true;
    entry.dx = sum_dx / ties;
    entry.dy = sum_dy / ties;

    int lowered = 0;
    int lowered_slot = -1;
    bool all_bars = true;
    for (int slot = 0; slot < slots; slot++){
        const int x0 = base + slot * 8;

        //  This slot's underscore: its top row...
        int top = -1;
        for (int row = BAR_ROW; row <= BAR_ROW + 4 && top < 0; row++){
            int count = 0;
            for (int column = 0; column < 6; column++){
                count += dark(x0 + column + entry.dx, row + entry.dy) > DARK ? 1 : 0;
            }
            if (count >= 4){
                top = row;
            }
        }
        if (top < 0){
            all_bars = false;
        }else if (top > BAR_ROW){
            lowered++;
            lowered_slot = slot;
        }

        //  ... and how far left or right it is (a game box that is a little
        //  too wide or narrow shifts the slots at the ends).
        double dx = entry.dx;
        if (top >= 0){
            const int SHIFTS = 13;      //  +-0.75 pixels
            int slot_scores[SHIFTS];
            for (int i = 0; i < SHIFTS; i++){
                const double shift = entry.dx + (i - SHIFTS / 2) * 0.125;
                int score = 0;
                for (int column = -1; column < 7; column++){
                    const int bar = dark(x0 + column + shift, top + entry.dy) + dark(x0 + column + shift, top + 1 + entry.dy);
                    score += column >= 0 && column < 6 ? bar : -bar;
                }
                slot_scores[i] = score;
            }
            int slot_worst[SHIFTS];
            int slot_best = 0;
            for (int i = 0; i < SHIFTS; i++){
                int least = slot_scores[i];
                for (int n = std::max(0, i - AROUND); n <= std::min(SHIFTS - 1, i + AROUND); n++){
                    least = std::min(least, slot_scores[n]);
                }
                slot_worst[i] = least;
                slot_best = std::max(slot_best, least);
            }
            double sum = 0;
            int count = 0;
            for (int i = 0; i < SHIFTS; i++){
                if (slot_best > 0 && slot_worst[i] * 100 >= slot_best * 98){
                    sum += entry.dx + (i - SHIFTS / 2) * 0.125;
                    count++;
                }
            }
            if (count > 0){
                dx = sum / count;
            }
        }

        //  The character: its dark pixels against each glyph's.
        unsigned rows[GLYPH_ROWS];
        for (int row = 0; row < GLYPH_ROWS; row++){
            unsigned value = 0;
            for (int column = 0; column < GLYPH_COLUMNS; column++){
                if (dark(x0 + column + dx, TEXT_ROW + row + entry.dy) > DARK){
                    value |= 1u << (GLYPH_COLUMNS - 1 - column);
                }
            }
            rows[row] = value;
        }
        int least = 0;
        for (int row = 0; row < GLYPH_ROWS; row++){
            least += bits_set(rows[row]);
        }
        char32_t closest = U' ';        //  (no dark pixel at all)
        for (const Glyph& glyph : GLYPHS){
            int differences = 0;
            for (int row = 0; row < GLYPH_ROWS; row++){
                differences += bits_set(rows[row] ^ (unsigned)glyph.rows[row]);
            }
            if (differences < least){
                least = differences;
                closest = glyph.character;
            }
        }
        entry.closest[slot] = closest;
        entry.differences[slot] = least;
        entry.characters[slot] = least == 0 ? closest : (char32_t)0;
    }
    if (all_bars && lowered == 1){
        entry.cursor = lowered_slot;
    }
    return entry;
}

std::string entry_difference(const Entry& entry, const std::u32string& typed){
    if (!entry.found){
        return "the name's field is not found on the screen";
    }
    const int length = (int)typed.size();
    if (length > entry.slots){
        return "the name is longer than the field";
    }
    for (int i = 0; i < entry.slots; i++){
        const char32_t expected = i < length ? typed[(size_t)i] : U' ';
        if (entry.characters[i] == expected){
            continue;
        }
        std::string seen;
        if (entry.characters[i] == U' '){
            seen = "nothing";
        }else if (entry.characters[i] != 0){
            seen = character_name(entry.characters[i]);
        }else{
            seen = "something " + std::to_string(entry.differences[i]) + " pixel" +
                (entry.differences[i] == 1 ? "" : "s") + " off " + character_name(entry.closest[i]);
        }
        if (i < length){
            return "character " + std::to_string(i + 1) + " shows " + seen + ", not " + character_name(expected);
        }
        return "after the " + std::to_string(length) + " typed, place " + std::to_string(i + 1) + " shows " + seen;
    }
    const int due = std::min(length, entry.slots - 1);
    if (entry.cursor != due){
        if (entry.cursor < 0){
            return "the underscores do not tell where the next character goes";
        }
        return "the next character is due at place " + std::to_string(entry.cursor + 1) +
            ", not at " + std::to_string(due + 1) + " (a space too many or too few)";
    }
    return "";
}



//  The box's title on the storage screen (pokemon_storage_system:
//  InitBoxTitle): the name in the same font, each character as wide as its
//  glyph, white with a shadow of (57, 57, 57) on every wallpaper, drawn into
//  two sprites that put its left edge at x = 160 - width / 2 and its top row
//  at y = 22. What is behind it differs by wallpaper and has white and dark
//  parts of its own, so only pixels of characters are looked at: the white
//  ones must be white and the shadow ones dark.
//  (Measured on Pokemon Automation's test pictures of the box, three
//  wallpapers and three video formats: the title's own name has 97 to 100
//  in 100 of its pixels right, another default name 80 to 92.)
namespace{

struct TitlePoint{
    int x;
    int y;
};

bool is_white(uint32_t p){
    const unsigned r = (p >> 16) & 0xff;
    const unsigned g = (p >> 8) & 0xff;
    const unsigned b = p & 0xff;
    return std::min(r, std::min(g, b)) >= 200;
}
bool is_shadow(uint32_t p){
    const unsigned r = (p >> 16) & 0xff;
    const unsigned g = (p >> 8) & 0xff;
    const unsigned b = p & 0xff;
    return std::max(r, std::max(g, b)) <= 120;
}

//  How well the glyph explains what is at (x, y): its pixels that are as
//  they should be, less twice those that are not.
int glyph_evidence(const Pixel& pixel, const Glyph& glyph, double x, double y){
    int right = 0;
    int wrong = 0;
    for (int row = 0; row < SHADOW_ROWS; row++){
        for (int column = 0; column < GLYPH_COLUMNS; column++){
            const unsigned bit = 1u << (GLYPH_COLUMNS - 1 - column);
            const bool white = row < GLYPH_ROWS && (glyph.rows[row] & bit) != 0;
            const bool shadow = !white && (glyph.shadow[row] & bit) != 0;
            if (!white && !shadow){
                continue;
            }
            const uint32_t p = pixel(x + column, y + row);
            if (white ? is_white(p) : is_shadow(p)){
                right++;
            }else{
                wrong++;
            }
        }
    }
    return right - 2 * wrong;
}

}

TitleLook look_at_title(const Pixel& pixel, const std::u32string& name){
    TitleLook look;
    std::vector<TitlePoint> white;
    std::vector<TitlePoint> shadow;
    int width = 0;
    for (char32_t character : name){
        const Glyph* glyph = find_glyph(character);
        width += glyph != nullptr ? glyph->width : SPACE_WIDTH;
    }
    const int LEFT = 160 - width / 2;
    const int TOP = 22;
    int x0 = LEFT;
    for (char32_t character : name){
        const Glyph* glyph = find_glyph(character);
        if (glyph == nullptr){
            x0 += SPACE_WIDTH;
            continue;
        }
        for (int row = 0; row < SHADOW_ROWS; row++){
            for (int column = 0; column < GLYPH_COLUMNS; column++){
                const unsigned bit = 1u << (GLYPH_COLUMNS - 1 - column);
                if (row < GLYPH_ROWS && (glyph->rows[row] & bit) != 0){
                    white.push_back(TitlePoint{x0 + column, TOP + row});
                }else if ((glyph->shadow[row] & bit) != 0){
                    shadow.push_back(TitlePoint{x0 + column, TOP + row});
                }
            }
        }
        x0 += glyph->width;
    }
    if (white.empty() || shadow.empty()){
        return look;
    }

    //  Where the name fits best, within 2 pixels.
    double best_dx = 0;
    double best_dy = 0;
    for (int iy = -8; iy <= 8; iy++){
        for (int ix = -8; ix <= 8; ix++){
            const double dx = ix * 0.25 + 0.5;
            const double dy = iy * 0.25 + 0.5;
            size_t white_seen = 0;
            for (const TitlePoint& point : white){
                white_seen += is_white(pixel(point.x + dx, point.y + dy)) ? 1 : 0;
            }
            const double white_share = (double)white_seen / (double)white.size();
            if (white_share <= look.match){
                continue;
            }
            size_t shadow_seen = 0;
            for (const TitlePoint& point : shadow){
                shadow_seen += is_shadow(pixel(point.x + dx, point.y + dy)) ? 1 : 0;
            }
            const double shadow_share = (double)shadow_seen / (double)shadow.size();
            const double match = std::min(white_share, shadow_share);
            if (match > look.match){
                look.match = match;
                best_dx = dx;
                best_dy = dy;
            }
        }
    }

    //  Character by character, there: no other character of the keyboard
    //  may explain what is in its place clearly better (an 8 where a 3
    //  should be has all of the 3's pixels right too).
    x0 = LEFT;
    for (size_t i = 0; i < name.size(); i++){
        const Glyph* expected = find_glyph(name[i]);
        const int mine = expected != nullptr ? glyph_evidence(pixel, *expected, x0 + best_dx, TOP + best_dy) : 0;
        for (const Glyph& glyph : GLYPHS){
            if (&glyph == expected){
                continue;
            }
            //  (A space: nothing should be there. A character with a few
            //  pixels only can sit right on the wallpaper by chance.)
            const int margin = expected != nullptr ? 3 : 12;
            if (glyph_evidence(pixel, glyph, x0 + best_dx, TOP + best_dy) >= mine + margin){
                if (look.other_at < 0){
                    look.other_at = (int)i;
                    look.other = glyph.character;
                }
                break;
            }
        }
        x0 += expected != nullptr ? expected->width : SPACE_WIDTH;
    }
    return look;
}



}
}
}
}

/*  Pokemon FRLG Autostory Coins
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The number in the coins window of the Celadon Game Corner (PocketPilot
 *  build 57, experimental, not run on a console when written).
 *
 *  The coins' clerk (and the prizes' clerk) shows a window while it talks:
 *  showcoinsbox 0, 5 is ShowCoinsWindow(coins, 0, 5) of src/coins.c, a window
 *  of 8 x 3 tiles at tile (1, 6): GBA pixels (8, 48) to (72, 72). In it
 *  "COINS" and, on the line at y = 12 of the window, "NNNN COINS" in the small
 *  font (FONT_SMALL), right-aligned at x = 64 (PrintCoinsString: the number is
 *  padded with spaces to four places). The line is 49 pixels wide (nine
 *  glyphs of 5 pixels and a space), so the first digit's cell begins at
 *  x = 8 + 64 - 49 = 23, and the glyphs, which are 5 wide and 8 high with
 *  their shadow, begin at y = 48 + 12 + 4 = 64 (graphics/fonts/latin_small.png,
 *  the cell of a glyph holds the glyph from its fifth row).
 *
 *  The text is dark grey (56) with a light grey shadow (216) on white (255).
 *  The program takes a piece of the game screen that holds the four cells
 *  and a little around them, and compares each cell of 5 x 8 GBA pixels, as
 *  the average of what the screen shows there, to the ten digits and to a
 *  blank cell. The place of the window on the picture may be off by a pixel:
 *  the nine places (-1 to 1 in x and y) are tried, and the one with the
 *  fewest differences in all four cells wins. What does not look like a
 *  number (a cell that is none of the ten digits, a blank between digits) is
 *  no reading.
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryCoins_H
#define PokemonAutomation_PokemonFRLG_AutoStoryCoins_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStoryCoins{


//  The piece of the game screen that is read, in GBA pixels (the game screen
//  is 240 x 160): the cells lie inside it with a margin of six to the sides
//  and three above and below (the frame of the window starts at y = 72).
constexpr int CROP_X = 17;
constexpr int CROP_Y = 61;
constexpr int CROP_W = 32;
constexpr int CROP_H = 11;
//  The first cell, the number of cells, their size.
constexpr int DIGIT_X = 23;
constexpr int DIGIT_Y = 64;
constexpr int SLOTS = 4;
constexpr int GLYPH_W = 5;
constexpr int GLYPH_H = 8;

//  The ten digits of FONT_SMALL: '#' the text, 'o' its shadow, '.' the window.
inline const char* const* glyph(int digit){
    static const char* const GLYPHS[10][GLYPH_H] = {
        {".##o.", "#oo#o", "#o.#o", "#o.#o", "#o.#o", "#o.#o", "o##oo", ".ooo."},
        {"..#o.", ".##o.", ".o#o.", "..#o.", "..#o.", "..#o.", ".###o", ".oooo"},
        {".##o.", "#oo#o", "o..#o", "..#o.", ".#o..", "#o...", "####o", "ooooo"},
        {".##o.", "#oo#o", "o..#o", ".##o.", ".oo#o", "#o.#o", "o##oo", ".ooo."},
        {".##o.", "#o#o.", "#o#o.", "#o#o.", "#o#o.", "####o", "oo#oo", "..oo."},
        {"####o", "#oooo", "#o...", "###o.", "ooo#o", "#o.#o", "o##oo", ".ooo."},
        {".##o.", "#oo#o", "#o...", "###o.", "#oo#o", "#o.#o", "o##oo", ".ooo."},
        {"####o", "ooo#o", "...#o", "..#oo", "..#o.", ".#oo.", ".#o..", ".oo.."},
        {".##o.", "#oo#o", "#o.#o", "o##o.", "#oo#o", "#o.#o", "o##oo", ".ooo."},
        {".##o.", "#oo#o", "#o.#o", "o###o", ".oo#o", "#o.#o", "o##oo", ".ooo."},
    };
    return GLYPHS[digit < 0 ? 0 : digit > 9 ? 9 : digit];
}

//  How dark a pixel of a glyph is, 1 for the text, between for its shadow
//  (the screen: (255 - 216) / (255 - 56)).
inline double glyph_darkness(char ch){
    return ch == '#' ? 1.0 : ch == 'o' ? 0.2 : 0.0;
}

struct Reading{
    bool ok = false;
    int value = -1;
    double cost = 1;        //  Mean difference per cell pixel of the four cells (0 = the same).
    double margin = 0;      //  The least gap, over the cells, between the best digit and the next.
    int dx = 0;             //  Where the window was, from where it was expected (GBA pixels).
    int dy = 0;
    std::string why;
};

//  "lum": the luminance (0 to 255) of the piece of the screen, "width" x
//  "height" pixels, row by row, for the GBA rectangle (CROP_X, CROP_Y,
//  CROP_W, CROP_H).
inline Reading read_digits(const std::vector<uint8_t>& lum, int width, int height){
    Reading out;
    if (width < CROP_W || height < CROP_H || lum.size() != (size_t)width * (size_t)height){
        out.why = "the piece of the screen is too small";
        return out;
    }
    int lo = 255;
    int hi = 0;
    for (uint8_t v : lum){
        lo = std::min<int>(lo, v);
        hi = std::max<int>(hi, v);
    }
    if (hi - lo < 80 || hi < 170){
        out.why = "no text on a light window (" + std::to_string(lo) + " to " + std::to_string(hi) + ")";
        return out;
    }
    const double sx = (double)width / CROP_W;
    const double sy = (double)height / CROP_H;
    //  The darkness of one GBA pixel (gx, gy) of the piece, from the middle of
    //  its square on the picture (the edges are blurred); -1 outside.
    auto cell = [&](int gx, int gy) -> double{
        if (gx < 0 || gy < 0 || gx >= CROP_W || gy >= CROP_H){
            return -1;
        }
        const int x0 = (int)std::floor((gx + 0.25) * sx);
        const int x1 = std::max(x0 + 1, (int)std::ceil((gx + 0.75) * sx));
        const int y0 = (int)std::floor((gy + 0.25) * sy);
        const int y1 = std::max(y0 + 1, (int)std::ceil((gy + 0.75) * sy));
        double sum = 0;
        int count = 0;
        for (int y = y0; y < y1 && y < height; y++){
            for (int x = x0; x < x1 && x < width; x++){
                const double v = (double)(hi - lum[(size_t)y * (size_t)width + (size_t)x]) / (double)(hi - lo);
                sum += v;
                count++;
            }
        }
        return count == 0 ? -1 : sum / count;
    };

    struct Choice{
        int digit = -1;     //  -1: a blank cell
        double cost = 1e9;
        double next = 1e9;
    };
    double best_total = 1e18;
    Choice best_choice[SLOTS];
    int best_dx = 0;
    int best_dy = 0;
    bool any = false;
    for (int dy = -1; dy <= 1; dy++){
        for (int dx = -1; dx <= 1; dx++){
            Choice choice[SLOTS];
            double total = 0;
            bool inside = true;
            for (int slot = 0; slot < SLOTS && inside; slot++){
                double seen[GLYPH_H][GLYPH_W];
                for (int r = 0; r < GLYPH_H && inside; r++){
                    for (int c = 0; c < GLYPH_W && inside; c++){
                        seen[r][c] = cell(DIGIT_X - CROP_X + slot * GLYPH_W + c + dx, DIGIT_Y - CROP_Y + r + dy);
                        inside = seen[r][c] >= 0;
                    }
                }
                if (!inside){
                    break;
                }
                for (int candidate = -1; candidate <= 9; candidate++){
                    double diff = 0;
                    for (int r = 0; r < GLYPH_H; r++){
                        for (int c = 0; c < GLYPH_W; c++){
                            const double want = candidate < 0 ? 0.0 : glyph_darkness(glyph(candidate)[r][c]);
                            diff += std::fabs(seen[r][c] - want);
                        }
                    }
                    diff /= GLYPH_H * GLYPH_W;
                    if (diff < choice[slot].cost){
                        choice[slot].next = choice[slot].cost;
                        choice[slot].cost = diff;
                        choice[slot].digit = candidate;
                    }else if (diff < choice[slot].next){
                        choice[slot].next = diff;
                    }
                }
                total += choice[slot].cost;
            }
            if (!inside){
                continue;
            }
            if (total < best_total){
                best_total = total;
                for (int slot = 0; slot < SLOTS; slot++){
                    best_choice[slot] = choice[slot];
                }
                best_dx = dx;
                best_dy = dy;
                any = true;
            }
        }
    }
    if (!any){
        out.why = "no place inside the piece of the screen";
        return out;
    }
    out.dx = best_dx;
    out.dy = best_dy;
    out.cost = best_total / SLOTS;
    double margin = 1e9;
    int value = 0;
    int digits = 0;
    bool blank_after = false;
    for (int slot = 0; slot < SLOTS; slot++){
        const Choice& choice = best_choice[slot];
        if (choice.cost > 0.18){
            out.why = "cell " + std::to_string(slot + 1) + " is no digit (" + std::to_string(choice.cost) + ")";
            return out;
        }
        margin = std::min(margin, choice.next - choice.cost);
        if (choice.digit < 0){
            if (digits > 0){
                blank_after = true;
            }
            continue;
        }
        if (blank_after){
            out.why = "a blank between digits";
            return out;
        }
        value = value * 10 + choice.digit;
        digits++;
    }
    out.margin = margin;
    if (digits == 0){
        out.why = "no digit";
        return out;
    }
    if (margin < 0.03){
        out.why = "a digit that is too close to another (" + std::to_string(margin) + ")";
        return out;
    }
    out.ok = true;
    out.value = value;
    return out;
}


//  What to do about a reading of the coins at the clerk: how many games of
//  the slot machine with the bet of one COIN to play (each costs one COIN,
//  and a win adds some) to bring the COINS to 49 modulo 50, from where the
//  packs of 500 and 50 end at 9999 exactly (the clerk refuses a pack of 500
//  from 9500 and one of 50 from 9950: 49 + 19 x 500 + 9 x 50 = 9999).
//  "at_most": the games of one visit to the machine.
inline int games_to_residue(int coins, int at_most){
    if (coins < 50){
        return 0;       //  Nothing to lose: the packs make 50 first.
    }
    const int need = (coins % 50 + 1) % 50;
    return std::min(need, std::max(0, at_most));
}


}
}
}
}

#endif

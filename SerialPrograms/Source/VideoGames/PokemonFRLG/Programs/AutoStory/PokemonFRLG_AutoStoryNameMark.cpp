/*  Pokemon FRLG Autostory Name Mark
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <string>
#include <vector>
#include "PokemonFRLG_AutoStoryNameMark.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStoryNameMark{


namespace{

//  The picture as how light each pixel is: the lightest of its red, green
//  and blue. (The text is dark grey; its shadow and the box are light.)
struct Lightness{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> value;

    uint8_t at(int x, int y) const{
        return value[(size_t)y * (size_t)width + (size_t)x];
    }
};

//  The last glyph of the name, with one threshold: everything darker than
//  "dark" is ink.
struct Glyph{
    //  "left" to "right" are in the picture: there is a last column with
    //  ink, and five game pixels fit in front of its end.
    bool placed = false;
    int left = 0;
    int right = 0;
    //  Its rows with ink. -1: none.
    int top = -1;
    int bottom = -1;
    //  Ink per column of the picture.
    std::vector<int> column_ink;
};

Glyph last_glyph(const Lightness& picture, double game_pixel, int dark){
    const int w = picture.width;
    const int h = picture.height;
    Glyph glyph;
    //  The last column with ink: the right edge of the name's last glyph.
    //  (Two pixels of ink at least: not a speck.)
    glyph.column_ink.assign((size_t)w, 0);
    for (int y = 0; y < h; y++){
        for (int x = 0; x < w; x++){
            if (picture.at(x, y) < dark){
                glyph.column_ink[(size_t)x]++;
            }
        }
    }
    int right = -1;
    for (int x = w - 1; x >= 0; x--){
        if (glyph.column_ink[(size_t)x] >= 2){
            right = x;
            break;
        }
    }
    //  A mark is five game pixels wide.
    const int left = right - (int)(5.0 * game_pixel + 0.5) + 1;
    if (right < 0 || left < 0){
        return glyph;
    }
    glyph.placed = true;
    glyph.left = left;
    glyph.right = right;
    //  The rows of the glyph: seven game pixels.
    for (int y = 0; y < h; y++){
        bool any = false;
        for (int x = left; x <= right && !any; x++){
            any = picture.at(x, y) < dark;
        }
        if (any){
            if (glyph.top < 0){
                glyph.top = y;
            }
            glyph.bottom = y;
        }
    }
    return glyph;
}

//  The glyph as five by seven game pixels, "#" where more than half is ink:
//  its rows from the top, "/" between them. For the log.
std::string glyph_picture(const Lightness& picture, double game_pixel, int dark){
    const Glyph glyph = last_glyph(picture, game_pixel, dark);
    if (!glyph.placed || glyph.top < 0){
        return std::string();
    }
    const int rows = glyph.bottom - glyph.top + 1;
    std::string ret;
    for (int r = 0; r < 7; r++){
        if (r > 0){
            ret += '/';
        }
        const int y0 = std::min(glyph.bottom, glyph.top + r * rows / 7);
        const int y1 = std::min(glyph.bottom, std::max(y0, glyph.top + (r + 1) * rows / 7 - 1));
        for (int c = 0; c < 5; c++){
            const int x0 = std::min(glyph.right, glyph.left + (int)(c * game_pixel + 0.5));
            const int x1 = std::min(glyph.right, std::max(x0, glyph.left + (int)((c + 1) * game_pixel + 0.5) - 1));
            int count = 0;
            for (int y = y0; y <= y1; y++){
                for (int x = x0; x <= x1; x++){
                    if (picture.at(x, y) < dark){
                        count++;
                    }
                }
            }
            ret += 2 * count > (y1 - y0 + 1) * (x1 - x0 + 1) ? '#' : '.';
        }
    }
    return ret;
}

//  With one threshold: which mark the last glyph is.
Mark tell(const Lightness& picture, double game_pixel, int dark){
    const Glyph glyph = last_glyph(picture, game_pixel, dark);
    if (!glyph.placed){
        return Mark::NOT_TOLD;
    }
    const int left = glyph.left;
    const int right = glyph.right;
    auto ink = [&](int x, int y){
        return picture.at(x, y) < dark;
    };
    //  A column of the glyph, "at" game pixels from its left edge.
    auto column = [&](double at){
        return std::min(right, left + (int)(at * game_pixel + 0.5));
    };

    //  The glyph starts where it should: there is ink in its first column.
    //  (Were the small wing at the right of the male mark not seen, this
    //  would be the shadow beside the letter in front. The same for a name
    //  that ends in a letter: the glyph looked at is a game pixel wider
    //  than a letter.)
    {
        int first = 0;
        for (int x = column(0.1); x <= column(0.7); x++){
            first += glyph.column_ink[(size_t)x];
        }
        if (first < 2){
            return Mark::NOT_TOLD;
        }
    }

    const int top = glyph.top;
    const int bottom = glyph.bottom;
    if (top < 0){
        return Mark::NOT_TOLD;
    }
    const int rows = bottom - top + 1;
    if (rows < 5.5 * game_pixel || rows > 8.5 * game_pixel){
        return Mark::NOT_TOLD;
    }

    //  A letter has a stroke from top to bottom at its left or its right.
    //  The marks have three game pixels of seven at the left and one at the
    //  right.
    auto tallest = [&](double from, double to){
        int most = 0;
        for (int x = column(from); x <= column(to); x++){
            int count = 0;
            for (int y = top; y <= bottom; y++){
                if (ink(x, y)){
                    count++;
                }
            }
            most = std::max(most, count);
        }
        return (double)most / rows;
    };
    if (tallest(0.0, 0.8) > 0.75 || tallest(4.2, 4.95) > 0.7){
        return Mark::NOT_TOLD;
    }

    //  How much of a part of the glyph is ink: columns "from" to "to" (game
    //  pixels from the left edge), rows "high" to "low" (parts of the
    //  glyph's height, from its top).
    auto filled = [&](double from, double to, double high, double low){
        const int x0 = column(from);
        const int x1 = column(to);
        const int y0 = std::min(bottom, top + (int)(high * rows + 0.5));
        const int y1 = std::min(bottom, top + (int)(low * rows + 0.5));
        int count = 0;
        for (int y = y0; y <= y1; y++){
            for (int x = x0; x <= x1; x++){
                if (ink(x, y)){
                    count++;
                }
            }
        }
        const int all = (y1 - y0 + 1) * (x1 - x0 + 1);
        return all <= 0 ? 0.0 : (double)count / all;
    };
    //  The middle of the second and third row, and of the fifth and sixth:
    //  the stem in one, the hole of the ring in the other. (Half a game
    //  pixel is kept from the ring's top and bottom.)
    const double UPPER[2] = {0.24, 0.36};
    const double LOWER[2] = {0.64, 0.78};
    const double upper = filled(2.05, 2.55, UPPER[0], UPPER[1]);
    const double lower = filled(2.05, 2.55, LOWER[0], LOWER[1]);
    Mark mark = Mark::NOT_TOLD;
    const double* ring = nullptr;
    if (upper >= 0.60 && lower <= 0.25){
        mark = Mark::MALE;
        ring = LOWER;
    }else if (upper <= 0.25 && lower >= 0.60){
        mark = Mark::FEMALE;
        ring = UPPER;
    }else{
        return Mark::NOT_TOLD;
    }
    //  Where the hole is, the ring has its two sides.
    if (filled(0.15, 0.85, ring[0], ring[1]) < 0.5 || filled(3.15, 3.85, ring[0], ring[1]) < 0.5){
        return Mark::NOT_TOLD;
    }
    return mark;
}

}   //  namespace



Mark read_mark(
    const uint32_t* pixels, size_t width, size_t height, size_t bytes_per_row,
    double game_pixel, std::string* seen
){
    if (seen != nullptr){
        seen->clear();
    }
    //  A picture too small to tell the marks apart, or none.
    if (pixels == nullptr || width == 0 || height == 0 || game_pixel < 3.5 ||
        width > 100000 || height > 100000
    ){
        return Mark::NOT_TOLD;
    }
    Lightness picture;
    picture.width = (int)width;
    picture.height = (int)height;
    picture.value.resize(width * height);
    int darkest = 255;
    int lightest = 0;
    for (size_t y = 0; y < height; y++){
        const uint32_t* row = (const uint32_t*)((const char*)pixels + y * bytes_per_row);
        for (size_t x = 0; x < width; x++){
            const uint32_t pixel = row[x];
            const int red = (int)((pixel >> 16) & 0xff);
            const int green = (int)((pixel >> 8) & 0xff);
            const int blue = (int)(pixel & 0xff);
            const int value = std::max(red, std::max(green, blue));
            picture.value[y * width + x] = (uint8_t)value;
            darkest = std::min(darkest, value);
            lightest = std::max(lightest, value);
        }
    }
    //  No text on a box.
    if (lightest - darkest < 80){
        return Mark::NOT_TOLD;
    }

    //  A few thresholds between the text and the box. The text's shadow is
    //  a light brown right beside the letters, and a scaled picture mixes
    //  the two: too low a threshold thins the strokes until parts of a mark
    //  are gone, too high a one closes the hole of its ring. What one
    //  threshold tells counts, unless another one tells otherwise.
    const double PARTS[] = {0.53, 0.45, 0.60, 0.37};
    if (seen != nullptr){
        *seen = glyph_picture(picture, game_pixel, darkest + (int)((lightest - darkest) * PARTS[0]));
    }
    Mark told = Mark::NOT_TOLD;
    for (double part : PARTS){
        const Mark mark = tell(picture, game_pixel, darkest + (int)((lightest - darkest) * part));
        if (mark == Mark::NOT_TOLD){
            continue;
        }
        if (told != Mark::NOT_TOLD && told != mark){
            return Mark::NOT_TOLD;
        }
        told = mark;
    }
    return told;
}



//  The check.

namespace{

//  Letters of four and marks of five by seven game pixels ("m": the male
//  mark, "f": the female one).
struct DrawnGlyph{
    char sign;
    int width;
    const char* rows[7];
};
const DrawnGlyph DRAWN_GLYPHS[] = {
    {'N', 4, {"#..#", "##.#", "##.#", "#.##", "#.##", "#..#", "#..#"}},
    {'I', 4, {"###.", ".#..", ".#..", ".#..", ".#..", ".#..", "###."}},
    {'D', 4, {"###.", "#..#", "#..#", "#..#", "#..#", "#..#", "###."}},
    {'O', 4, {".##.", "#..#", "#..#", "#..#", "#..#", "#..#", ".##."}},
    {'R', 4, {"###.", "#..#", "#..#", "###.", "#.#.", "#..#", "#..#"}},
    {'A', 4, {".##.", "#..#", "#..#", "####", "#..#", "#..#", "#..#"}},
    {'m', 5, {"..#..", ".###.", "#.#.#", ".##..", "#..#.", "#..#.", ".##.."}},
    {'f', 5, {".##..", "#..#.", "#..#.", ".##..", "..#..", "#####", "..#.."}},
};

//  A name as the foe's box has it: dark letters with their light brown
//  shadow (right of and below each pixel) on the box, 64 by 12 game pixels
//  with the name from (2, 2), each game pixel "scale" pixels of the picture.
std::vector<uint32_t> drawn_name(const char* name, double scale, bool blurred, size_t& width, size_t& height){
    const int GAME_WIDTH = 64;
    const int GAME_HEIGHT = 12;
    const uint32_t BOX = 0xffffffde;
    const uint32_t INK = 0xff414141;
    const uint32_t SHADOW = 0xffded5b4;
    std::vector<uint8_t> game((size_t)(GAME_WIDTH * GAME_HEIGHT), 0);
    auto game_at = [&](int x, int y) -> uint8_t&{
        return game[(size_t)(y * GAME_WIDTH + x)];
    };
    int at = 2;
    for (const char* c = name; *c != 0; c++){
        for (const DrawnGlyph& glyph : DRAWN_GLYPHS){
            if (glyph.sign != *c){
                continue;
            }
            for (int y = 0; y < 7; y++){
                for (int x = 0; x < glyph.width && at + x < GAME_WIDTH; x++){
                    if (glyph.rows[y][x] == '#'){
                        game_at(at + x, 2 + y) = 1;
                    }
                }
            }
            at += glyph.width + 1;
            break;
        }
    }
    for (int y = GAME_HEIGHT - 1; y > 0; y--){
        for (int x = GAME_WIDTH - 1; x > 0; x--){
            if (game_at(x, y) == 0 && (game_at(x - 1, y) == 1 || game_at(x, y - 1) == 1 || game_at(x - 1, y - 1) == 1)){
                game_at(x, y) = 2;
            }
        }
    }

    width = (size_t)(GAME_WIDTH * scale);
    height = (size_t)(GAME_HEIGHT * scale);
    std::vector<uint32_t> picture(width * height, BOX);
    for (size_t y = 0; y < height; y++){
        for (size_t x = 0; x < width; x++){
            const int gx = std::min(GAME_WIDTH - 1, (int)((double)x / scale));
            const int gy = std::min(GAME_HEIGHT - 1, (int)((double)y / scale));
            const uint8_t value = game_at(gx, gy);
            picture[y * width + x] = value == 1 ? INK : value == 2 ? SHADOW : BOX;
        }
    }
    if (!blurred){
        return picture;
    }
    //  Each pixel with the eight around it.
    const int w = (int)width;
    const int h = (int)height;
    std::vector<uint32_t> soft(width * height, BOX);
    for (int y = 0; y < h; y++){
        for (int x = 0; x < w; x++){
            unsigned sum[3] = {0, 0, 0};
            for (int dy = -1; dy <= 1; dy++){
                for (int dx = -1; dx <= 1; dx++){
                    const int px = std::min(w - 1, std::max(0, x + dx));
                    const int py = std::min(h - 1, std::max(0, y + dy));
                    const uint32_t pixel = picture[(size_t)(py * w + px)];
                    sum[0] += (pixel >> 16) & 0xff;
                    sum[1] += (pixel >> 8) & 0xff;
                    sum[2] += pixel & 0xff;
                }
            }
            soft[(size_t)(y * w + x)] = 0xff000000 | ((sum[0] / 9) << 16) | ((sum[1] / 9) << 8) | (sum[2] / 9);
        }
    }
    return soft;
}

const char* mark_name(Mark mark){
    return mark == Mark::MALE ? "the male mark" : mark == Mark::FEMALE ? "the female mark" : "no mark";
}

}   //  namespace


int check_reader(int& cases, std::string& failures){
    struct Name{
        const char* signs;
        const char* shown;
        Mark expected;
    };
    const Name NAMES[] = {
        {"NIDORANm", "NIDORAN with the male mark", Mark::MALE},
        {"NIDORANf", "NIDORAN with the female mark", Mark::FEMALE},
        {"NIDORAN", "NIDORAN without a mark", Mark::NOT_TOLD},
        {"NIDORINO", "NIDORINO", Mark::NOT_TOLD},
        {"NIDORINA", "NIDORINA", Mark::NOT_TOLD},
        {"", "a box without a name", Mark::NOT_TOLD},
    };
    //  A game pixel in a 720p and in a 1080p capture.
    const double SCALES[] = {1040.0 / 240.0, 1560.0 / 240.0};
    cases = 0;
    failures.clear();
    int wrong = 0;
    auto read_drawn = [&](
        const char* signs, const char* shown, double scale, bool blurred,
        Mark expected, const char* expected_seen
    ){
        size_t width = 0;
        size_t height = 0;
        const std::vector<uint32_t> picture = drawn_name(signs, scale, blurred, width, height);
        std::string seen;
        const Mark mark = read_mark(picture.data(), width, height, width * sizeof(uint32_t), scale, &seen);
        cases++;
        if (mark == expected && (expected_seen == nullptr || seen == expected_seen)){
            return;
        }
        wrong++;
        failures += std::string(shown) + ", a game pixel " + std::to_string(scale) + " wide" +
            (blurred ? ", blurred" : "") + ": read as " + mark_name(mark) + " (seen: " + seen + ")\n";
    };
    for (const Name& name : NAMES){
        for (double scale : SCALES){
            for (bool blurred : {false, true}){
                read_drawn(name.signs, name.shown, scale, blurred, name.expected, nullptr);
            }
        }
    }
    //  What the log gets of a mark.
    read_drawn(
        "NIDORANm", "NIDORAN with the male mark, as the log shows it", SCALES[0], false,
        Mark::MALE, "..#../.###./#.#.#/.##../#..#./#..#./.##.."
    );
    read_drawn(
        "NIDORANf", "NIDORAN with the female mark, as the log shows it", SCALES[0], false,
        Mark::FEMALE, ".##../#..#./#..#./.##../..#../#####/..#.."
    );
    //  Too small a picture to tell the marks apart: not told, not guessed.
    read_drawn("NIDORANm", "NIDORAN with the male mark, too small", 3.0, false, Mark::NOT_TOLD, nullptr);
    return wrong;
}



}
}
}
}

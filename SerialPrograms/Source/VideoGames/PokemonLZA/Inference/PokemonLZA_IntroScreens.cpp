/*  PokemonLZA - New game intro screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include <cctype>
#include <cstdint>
#include <span>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "PokemonLZA_IntroScreens.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr size_t LANGUAGE_LIST_ROWS = 5;
constexpr size_t LANGUAGE_LIST_SIZE = 2 * LANGUAGE_LIST_ROWS;
constexpr size_t LOOK_COLUMNS = 4;
constexpr size_t LOOK_COUNT = 2 * LOOK_COLUMNS;

bool usable(const ImageViewRGB32& frame){
    return frame && frame.width() >= 160 && frame.height() >= 90;
}

double brightness(const ImageStats& stats){
    return stats.average.sum() / 3;
}

ImageStats box_stats(const ImageViewRGB32& frame, const ImageFloatBox& box){
    return image_stats(extract_box_reference(frame, box));
}

//  Plain rows: blue over red by `min_blue_over_red`, channel spreads summing to
//  `max_spread` at most.
struct SlateRows{
    double min_red;
    double max_red;
    double min_green;
    double max_green;
    double min_blue;
    double max_blue;
    double min_blue_over_red;
    double max_spread;
};

enum class RowBox{
    HIGHLIGHTED,
    PLAIN,
    OTHER,
};

RowBox classify_row(const ImageStats& stats, const SlateRows& slate){
    if (stats.count == 0){
        return RowBox::OTHER;
    }
    const FloatPixel& average = stats.average;
    const double spread = stats.stddev.sum();
    if (average.r >= 200 && average.g >= 200 && average.b >= 200 && spread <= 30){
        return RowBox::HIGHLIGHTED;
    }
    const bool plain =
        average.r >= slate.min_red && average.r <= slate.max_red &&
        average.g >= slate.min_green && average.g <= slate.max_green &&
        average.b >= slate.min_blue && average.b <= slate.max_blue &&
        average.b >= average.r + slate.min_blue_over_red &&
        spread <= slate.max_spread;
    return plain ? RowBox::PLAIN : RowBox::OTHER;
}

//  The one white, flat strip (channels 200+, spreads summing <= 30) among `slate` ones;
//  nullopt when none or two are white, or a strip is neither.
std::optional<size_t> highlighted_row(const ImageViewRGB32& frame, std::span<const ImageFloatBox> strips, const SlateRows& slate){
    std::optional<size_t> highlighted;
    for (size_t row = 0; row < strips.size(); row++){
        switch (classify_row(box_stats(frame, strips[row]), slate)){
        case RowBox::HIGHLIGHTED:
            if (highlighted){
                return std::nullopt;
            }
            highlighted = row;
            break;
        case RowBox::PLAIN:
            break;
        case RowBox::OTHER:
            return std::nullopt;
        }
    }
    return highlighted;
}

//  Rows span x 144-742: the choices from y 578 and 662 (68 high), "Ready!" from y 790
//  (112 high). Each strip is 100 wide from x 620, right of the text.
ImageFloatBox confirm_row_strip(size_t row){
    switch (row){
    case 0:
        return ImageFloatBox(620.0 / 1920, 592.0 / 1080, 100.0 / 1920, 40.0 / 1080);
    case 1:
        return ImageFloatBox(620.0 / 1920, 676.0 / 1080, 100.0 / 1920, 40.0 / 1080);
    default:
        return ImageFloatBox(620.0 / 1920, 820.0 / 1080, 100.0 / 1920, 60.0 / 1080);
    }
}

//  Highlighted: white, flat; others slate. The highlight's glow raises the next row's
//  spread up to 20.
constexpr SlateRows CONFIRM_ROWS_SLATE{
    .min_red = 20, .max_red = 55,
    .min_green = 40, .max_green = 80,
    .min_blue = 65, .max_blue = 115,
    .min_blue_over_red = 25,
    .max_spread = 30,
};

//  The dotted background between the choices and "Ready!" and right of all three:
//  (10-13, 29-59, 63-106), spread up to 51 under the highlight's glow. Dark and blue
//  with little red; the intro's other screens are not.
bool confirm_background_dark(const ImageViewRGB32& frame){
    const ImageFloatBox boxes[] = {
        ImageFloatBox(620.0 / 1920, 742.0 / 1080, 100.0 / 1920, 40.0 / 1080),
        ImageFloatBox(770.0 / 1920, 580.0 / 1080, 30.0 / 1920, 320.0 / 1080),
    };
    for (const ImageFloatBox& box : boxes){
        const ImageStats stats = box_stats(frame, box);
        if (stats.count == 0 || brightness(stats) >= 90 || stats.stddev.sum() > 80){
            return false;
        }
        if (stats.average.r > 30 || stats.average.b < stats.average.r + 40){
            return false;
        }
    }
    return true;
}

//  A 65 x 54 strip of entry `index`'s box (576 x 84; columns at x 347 and 996, rows
//  108 apart from y 342), 38 right of its edge and 15 down: left of the text.
ImageFloatBox language_box_strip(size_t index){
    const size_t column = index / LANGUAGE_LIST_ROWS;
    const size_t row = index % LANGUAGE_LIST_ROWS;
    return ImageFloatBox(column == 0 ? 0.2005 : 0.5365, 0.331 + 0.1 * (double)row, 0.034, 0.05);
}

//  Highlighted: flat 255 white; others slate, about (54, 60-76, 82-107), spread under 3.
constexpr SlateRows LANGUAGE_LIST_ROWS_SLATE{
    .min_red = 35, .max_red = 90,
    .min_green = 45, .max_green = 110,
    .min_blue = 65, .max_blue = 140,
    .min_blue_over_red = 15,
    .max_spread = 20,
};

//  The dark blue background between the columns (about (9, 24, 56)).
bool column_gap_dark(const ImageViewRGB32& frame){
    const ImageStats gap = box_stats(frame, ImageFloatBox(0.4896, 0.331, 0.0208, 0.447));
    return gap.count > 0 && brightness(gap) < 70;
}

//  Tiles are 240 x 240: left edges x 373, 685, 997, 1309; tops y 324, 609.
double look_tile_left(size_t look){
    return (373.0 + 312.0 * (double)((look - 1) % LOOK_COLUMNS)) / 1920;
}
double look_tile_top(size_t look){
    return (look <= LOOK_COLUMNS ? 324.0 : 609.0) / 1080;
}

//  12 x 12, 10 in from both edges (0 TL, 1 TR, 2 BL, 3 BR). The highlighted card
//  reaches ~12 past the tile, so the patches stay on it while it is enlarged.
ImageFloatBox look_corner_patch(size_t look, size_t corner){
    const double dx = (corner % 2 == 0 ? 10.0 : 218.0) / 1920;
    const double dy = (corner < 2 ? 10.0 : 218.0) / 1080;
    return ImageFloatBox(look_tile_left(look) + dx, look_tile_top(look) + dy, 12.0 / 1920, 12.0 / 1080);
}

//  60 x 60 from 90 right of the tile's edge and 100 down.
ImageFloatBox look_face_patch(size_t look){
    return ImageFloatBox(look_tile_left(look) + 90.0 / 1920, look_tile_top(look) + 100.0 / 1080, 60.0 / 1920, 60.0 / 1080);
}

enum class LookCorner{
    WHITE,
    DARK,
    OTHER,
};

//  Measured on 67 frames: highlighted card (240-248, 245-250, 246-251), spread under
//  17; other tiles or background (21-34, 29-57, 56-91), spread under 32.
LookCorner classify_look_corner(const ImageStats& stats){
    if (stats.count == 0){
        return LookCorner::OTHER;
    }
    const FloatPixel& average = stats.average;
    const double spread = stats.stddev.sum();
    if (average.r >= 200 && average.g >= 200 && average.b >= 200 && spread <= 40){
        return LookCorner::WHITE;
    }
    if (brightness(stats) < 90 && average.r <= 50 && average.b >= average.r + 20 && spread <= 50){
        return LookCorner::DARK;
    }
    return LookCorner::OTHER;
}

//  Nullopt if a corner is neither white nor dark, or no face is in the middle
//  (faces: brightness 108-196, spread over 117).
std::optional<size_t> look_white_corners(const ImageViewRGB32& frame, size_t look){
    const ImageStats face = box_stats(frame, look_face_patch(look));
    if (face.count == 0 || brightness(face) < 80 || face.stddev.sum() < 60){
        return std::nullopt;
    }
    size_t white = 0;
    for (size_t corner = 0; corner < 4; corner++){
        switch (classify_look_corner(box_stats(frame, look_corner_patch(look, corner)))){
        case LookCorner::WHITE:
            white++;
            break;
        case LookCorner::DARK:
            break;
        case LookCorner::OTHER:
            return std::nullopt;
        }
    }
    return white;
}

//  The highlighted look has 4 white corners while the other 7 have none.
struct LookPickerReading{
    bool shows = false;
    std::optional<size_t> highlighted;
};

LookPickerReading read_look_picker(const ImageViewRGB32& frame){
    if (!usable(frame)){
        return {};
    }
    std::optional<size_t> card;
    size_t lit_tiles = 0;
    for (size_t look = 1; look <= LOOK_COUNT; look++){
        const std::optional<size_t> white = look_white_corners(frame, look);
        if (!white){
            return {};
        }
        if (*white > 0){
            lit_tiles++;
        }
        if (*white == 4){
            card = look;
        }
    }
    LookPickerReading reading{.shows = true};
    if (lit_tiles == 1 && card){
        reading.highlighted = card;
    }
    return reading;
}

//  Pixels with every channel >= `min_channel` are the text.
std::string read_light_text(const ImageViewRGB32& frame, const ImageFloatBox& box, Language language, uint8_t min_channel){
    const uint32_t mins = 0xff000000 | ((uint32_t)min_channel << 16) | ((uint32_t)min_channel << 8) | min_channel;
    const ImageRGB32 filtered = to_blackwhite_rgb32_range(
        extract_box_reference(frame, box), /*in_range_black=*/true, mins, /*maxs=*/0xffffffff
    );
    return OCR::ocr_read(language, filtered);
}

} // namespace


//  Title line y 215-266 from the highlighted tile's left edge (row border from
//  y 277); text about (10, 212, 224) on (241, 241, 241).
bool is_home_tile_plza(const ImageViewRGB32& frame){
    if (!usable(frame)){
        return false;
    }
    const ImageRGB32 filtered = to_blackwhite_rgb32_range(
        extract_box_reference(frame, ImageFloatBox(0.02, 205.0 / 1080, 0.96, 67.0 / 1080)),
        /*in_range_black=*/true, /*mins=*/0xff009696, /*maxs=*/0xff7fffff
    );
    std::string letters;
    for (const char ch : OCR::ocr_read(Language::English, filtered)){
        if (std::isalnum((unsigned char)ch)){
            letters += (char)std::tolower((unsigned char)ch);
        }
    }
    return letters.ends_with("za");
}

std::optional<size_t> read_language_list_cursor(const ImageViewRGB32& frame){
    if (!usable(frame) || !column_gap_dark(frame)){
        return std::nullopt;
    }
    std::array<ImageFloatBox, LANGUAGE_LIST_SIZE> strips;
    for (size_t index = 0; index < LANGUAGE_LIST_SIZE; index++){
        strips[index] = language_box_strip(index);
    }
    return highlighted_row(frame, strips, LANGUAGE_LIST_ROWS_SLATE);
}

std::optional<size_t> read_look_picker_cursor(const ImageViewRGB32& frame){
    return read_look_picker(frame).highlighted;
}

bool is_look_picker(const ImageViewRGB32& frame){
    return read_look_picker(frame).shows;
}

//  Question area (x 1056-1824, y 54-184): flat 27 (spread 3) in all ten languages;
//  other intro screens are 46+ or busy. Light theme: key row y 929-950 averages
//  205-211 (elsewhere <= 160). Dark theme: the area is navy (27, spread 4), and the
//  bar at y 1010-1042 is flat grey 76 (other screens <= 56, HOME's dark theme 51).
bool is_name_keyboard(const ImageViewRGB32& frame){
    if (!usable(frame)){
        return false;
    }
    const ImageStats question = box_stats(frame, ImageFloatBox(0.55, 0.05, 0.40, 0.12));
    if (question.count == 0 || brightness(question) > 40 || question.stddev.sum() > 15){
        return false;
    }
    const ImageStats keys = box_stats(frame, ImageFloatBox(0.03, 0.86, 0.80, 0.02));
    if (keys.count > 0 && brightness(keys) >= 185){
        return true;
    }
    const ImageStats bar = box_stats(frame, ImageFloatBox(0.10, 0.935, 0.35, 0.03));
    return bar.count > 0 && brightness(bar) >= 65 && brightness(bar) <= 90 && bar.stddev.sum() < 10;
}

//  White text from x 345: language row y 400-450, name row y 468-522.
std::string read_confirm_choices_name(const ImageViewRGB32& frame){
    if (!usable(frame)){
        return "";
    }
    const std::string text = read_light_text(
        frame, ImageFloatBox(340.0 / 1920, 468.0 / 1080, 760.0 / 1920, 54.0 / 1080), Language::English, /*min_channel=*/150
    );
    std::string name;
    for (const char ch : text){
        if (ch != ' ' && ch != '\n' && ch != '\r'){
            name += ch;
        }
    }
    return name;
}

std::string read_confirm_choices_language(const ImageViewRGB32& frame, Language language){
    if (!usable(frame)){
        return "";
    }
    return read_light_text(
        frame, ImageFloatBox(340.0 / 1920, 400.0 / 1080, 760.0 / 1920, 50.0 / 1080), language, /*min_channel=*/150
    );
}

//  The language list and look picker fall on the background strips, so neither reads
//  as this screen.
std::optional<size_t> read_confirm_choices_cursor(const ImageViewRGB32& frame){
    if (!usable(frame) || !confirm_background_dark(frame)){
        return std::nullopt;
    }
    const std::array<ImageFloatBox, CONFIRM_CHOICES_READY_ROW + 1> strips{confirm_row_strip(0), confirm_row_strip(1), confirm_row_strip(2)};
    return highlighted_row(frame, strips, CONFIRM_ROWS_SLATE);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

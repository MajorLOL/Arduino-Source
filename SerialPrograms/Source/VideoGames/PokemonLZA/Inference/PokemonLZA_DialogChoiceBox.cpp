/*  PokemonLZA - Dialog choice box
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <vector>
#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonLZA_DialogChoiceBox.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  Rows read from the bottom up before the reader stops looking.
constexpr size_t MAX_CHOICE_ROWS = 6;
//  One white strip alone is not a menu: light pavement reads as a highlighted row.
constexpr size_t MIN_CHOICE_ROWS = 2;

//  At 1920 x 1080 the panel ends at x 1728, y 808 and grows left to fit its longest
//  row (x 1198-1408). The highlight runs from 10 right of the panel's left to x 1715,
//  the bottom row's at y 732-800, each row above 80 higher. The text starts 35 right
//  of the panel's left and ends by x 1690.
constexpr double ROW_PITCH = 80.0 / 1080;
constexpr double STRIP_TOP = 748.0 / 1080;
constexpr double STRIP_HEIGHT = 40.0 / 1080;
constexpr double TEXT_END_X = 1690.0 / 1920;
//  Start of the leftward highlight scan; bands 4 inside the bottom row's highlight's
//  top and bottom, which few glyphs reach.
constexpr double HIGHLIGHT_SCAN_X = 1700.0 / 1920;
constexpr double HIGHLIGHT_TOP_BAND_Y = 736.0 / 1080;
constexpr double HIGHLIGHT_BOTTOM_BAND_Y = 796.0 / 1080;
//  Plain rows measure 0.07-0.2; the elevator's wall above its top row 0.
constexpr double MIN_TEXT_SHARE = 0.005;

//  Right of any row's text, inside the highlight.
ImageFloatBox right_strip(size_t from_bottom){
    return ImageFloatBox(1698.0 / 1920, STRIP_TOP - ROW_PITCH * from_bottom, 14.0 / 1920, STRIP_HEIGHT);
}

//  From 10 right of `highlight_left` (a share of the width) to the text's right limit.
ImageFloatBox text_band(size_t from_bottom, double highlight_left){
    const double left = highlight_left + 10.0 / 1920;
    return ImageFloatBox(left, STRIP_TOP - ROW_PITCH * from_bottom, TEXT_END_X - left, STRIP_HEIGHT);
}

bool bright(uint32_t pixel, uint32_t min_channel){
    const uint32_t red = (pixel >> 16) & 0xff;
    const uint32_t green = (pixel >> 8) & 0xff;
    const uint32_t blue = pixel & 0xff;
    return red >= min_channel && green >= min_channel && blue >= min_channel;
}

enum class ChoiceSlot{
    HIGHLIGHTED,
    PANEL,
    OTHER,
};

//  Highlighted: flat 255 white; plain: panel blue (35-45, 60-73, 108-120), spread 3-7.
//  The elevator's wall above its top row is as blue; `row_has_text` tells it apart.
ChoiceSlot classify_slot(const ImageViewRGB32& frame, size_t from_bottom){
    const ImageStats right = image_stats(extract_box_reference(frame, right_strip(from_bottom)));
    if (right.count == 0 || right.stddev.sum() > 30){
        return ChoiceSlot::OTHER;
    }
    const FloatPixel& average = right.average;
    if (average.r >= 200 && average.g >= 200 && average.b >= 200){
        return ChoiceSlot::HIGHLIGHTED;
    }
    const bool dark_blue =
        average.r <= 70 && average.g <= 100 &&
        average.b >= 80 && average.b <= 150 &&
        average.b >= average.r + 40;
    return dark_blue ? ChoiceSlot::PANEL : ChoiceSlot::OTHER;
}

//  As a share of the width: scanned left while the top or bottom band is white.
double highlight_left(const ImageViewRGB32& frame, size_t from_bottom){
    const double pitch = ROW_PITCH * from_bottom;
    const size_t top_y = (size_t)((HIGHLIGHT_TOP_BAND_Y - pitch) * frame.height());
    const size_t bottom_y = (size_t)((HIGHLIGHT_BOTTOM_BAND_Y - pitch) * frame.height());
    size_t x = (size_t)(HIGHLIGHT_SCAN_X * frame.width());
    while (x > 0 && (bright(frame.pixel(x - 1, top_y), 200) || bright(frame.pixel(x - 1, bottom_y), 200))){
        x--;
    }
    return (double)x / frame.width();
}

bool row_has_text(const ImageViewRGB32& frame, size_t from_bottom, double highlight_left){
    const ImageViewRGB32 band = extract_box_reference(frame, text_band(from_bottom, highlight_left));
    const size_t area = band.width() * band.height();
    if (area == 0){
        return false;
    }
    size_t bright_pixels = 0;
    for (size_t y = 0; y < band.height(); y++){
        for (size_t x = 0; x < band.width(); x++){
            if (bright(band.pixel(x, y), 180)){
                bright_pixels++;
            }
        }
    }
    return (double)bright_pixels / area >= MIN_TEXT_SHARE;
}

} // namespace


//  The arrow sits 13 left to 27 right of the panel's left (x 1185-1225 on French's
//  panel at 1198); from x 960 leaves room for rows of about 54 characters.
const ImageFloatBox& dialog_choice_box(){
    static const ImageFloatBox box{960.0 / 1920, 0.481553, 0.949837 - 960.0 / 1920, 0.312621};
    return box;
}

std::optional<DialogChoiceRows> read_dialog_choice_rows(const ImageViewRGB32& frame){
    if (!frame || frame.width() < 192 || frame.height() < 108){
        return std::nullopt;
    }
    std::vector<ChoiceSlot> slots;
    std::optional<size_t> first_highlighted;
    while (slots.size() < MAX_CHOICE_ROWS){
        const ChoiceSlot slot = classify_slot(frame, slots.size());
        if (slot == ChoiceSlot::OTHER){
            break;
        }
        if (slot == ChoiceSlot::HIGHLIGHTED && !first_highlighted){
            first_highlighted = slots.size();
        }
        slots.push_back(slot);
    }
    if (!first_highlighted){
        return std::nullopt;
    }
    //  The rows end at the first blue slot with no text.
    const double left = highlight_left(frame, *first_highlighted);
    size_t count = 0;
    size_t highlighted = 0;
    for (; count < slots.size(); count++){
        if (slots[count] == ChoiceSlot::HIGHLIGHTED){
            highlighted++;
        }else if (!row_has_text(frame, count, left)){
            break;
        }
    }
    if (highlighted != 1 || count < MIN_CHOICE_ROWS){
        return std::nullopt;
    }
    return DialogChoiceRows{.count = count, .selected = count - 1 - *first_highlighted};
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

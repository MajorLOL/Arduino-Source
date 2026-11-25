/*  PokemonLZA - Caught mark reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <memory>
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/Images/BinaryImage_FilterRgb32.h"
#include "Kernels/Waterfill/Kernels_Waterfill_Session.h"
#include "PokemonLZA_CaughtMarkReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Kernels::Waterfill;


namespace{

//  The widest flat green bar in the upper two thirds, 80+ px at 1080p. The green is
//  tight (bar about 45, 198, 115) so bright grass (68, 160, 80) around the plate keeps
//  it flat. The player's own HP bar is about 63 px wide; plates 105-115.
std::optional<ImagePixelBox> find_plate_bar(const ImageViewRGB32& screen){
    if (screen.width() == 0 || screen.height() == 0){
        return std::nullopt;
    }
    const double scale = screen.height() / 1080.0;
    const ImageViewRGB32 upper = extract_box_reference(screen, ImageFloatBox(0, 0, 1, 0.68));
    PackedBinaryMatrix matrix = compress_rgb32_to_binary_range(
        upper,
        /*min_red=*/0, /*max_red=*/90,
        /*min_green=*/175, /*max_green=*/255,
        /*min_blue=*/90, /*max_blue=*/145
    );
    std::unique_ptr<WaterfillSession> session = make_WaterfillSession(matrix);
    std::unique_ptr<WaterfillIterator> iter = session->make_iterator((size_t)(150 * scale * scale));
    std::optional<ImagePixelBox> best;
    WaterfillObject object;
    while (iter->find_next(object, false)){
        const size_t w = object.width() + 1;
        const size_t h = object.height() + 1;
        const bool flat = w >= 80 * scale && h <= 24 * scale && w >= 4 * h;
        if (!flat || object.area * 2 < w * h){
            continue;
        }
        if (!best || w > best->width()){
            best = ImagePixelBox(object.min_x, object.min_y, object.max_x + 1, object.max_y + 1);
        }
    }
    return best;
}

} // namespace


namespace{

//  Pinkish red and white pixels where the caught mark sits above `bar`.
struct MarkPixels{
    size_t red = 0;
    size_t white = 0;
};

MarkPixels count_mark_pixels(const ImageViewRGB32& screen, const ImagePixelBox& bar){
    const double w = (double)bar.width();
    const size_t min_x = (size_t)(bar.min_x + 0.6 * w);
    const size_t max_x = std::min(screen.width(), (size_t)(bar.min_x + 1.5 * w));
    const size_t min_y = (size_t)std::max(0.0, bar.min_y - 0.85 * w);
    const size_t max_y = (size_t)std::max(0.0, bar.min_y - 0.45 * w);
    MarkPixels counts;
    for (size_t y = min_y; y < max_y; y++){
        for (size_t x = min_x; x < max_x; x++){
            const Color pixel(screen.pixel(x, y));
            const int r = pixel.red();
            //  The ball's red is pinkish (blue about green); the orange gender mark next to it
            //  (green well over blue) is not counted.
            counts.red += r > 190 && r > pixel.green() + 70 && r > pixel.blue() + 50 && pixel.blue() + 15 >= pixel.green();
            counts.white += r > 215 && pixel.green() > 215 && pixel.blue() > 215;
        }
    }
    return counts;
}

} // namespace


std::optional<ImageFloatBox> lock_target_plate_bar(const ImageViewRGB32& screen){
    const std::optional<ImagePixelBox> bar = find_plate_bar(screen);
    if (!bar){
        return std::nullopt;
    }
    return ImageFloatBox(
        (double)bar->min_x / screen.width(), (double)bar->min_y / screen.height(),
        (double)bar->width() / screen.width(), (double)bar->height() / screen.height()
    );
}

std::optional<bool> lock_target_caught(const ImageViewRGB32& screen){
    const std::optional<ImagePixelBox> bar = find_plate_bar(screen);
    if (!bar){
        return std::nullopt;
    }
    const MarkPixels counts = count_mark_pixels(screen, *bar);
    //  37 red and 42 white at 1280x720 on the caught Mareep.
    const double area_scale = (screen.height() / 720.0) * (screen.height() / 720.0);
    return counts.red >= 15 * area_scale && counts.white >= 15 * area_scale;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

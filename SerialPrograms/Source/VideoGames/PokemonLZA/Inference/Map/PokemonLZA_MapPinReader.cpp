/*  PokemonLZA - Map pin reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <memory>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/Images/BinaryImage_FilterRgb32.h"
#include "Kernels/Waterfill/Kernels_Waterfill_Session.h"
#include "PokemonLZA_MapPinReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Kernels::Waterfill;


namespace{

//  The map's coordinates (MapPoint) are the frame scaled to this.
constexpr double MAP_WIDTH = 800;
constexpr double MAP_HEIGHT = 450;

//  Wild Zone 1's window on the map, with a margin round its outline.
constexpr double WINDOW_MIN_X = 240;
constexpr double WINDOW_MAX_X = 540;
constexpr double WINDOW_MIN_Y = 170;
constexpr double WINDOW_MAX_Y = 290;

//  The arrow's blue (0, 160, 255); its white rim and the cone (95, 200, 235) are out.
constexpr uint32_t ARROW_MIN = 0xff0080e1;
constexpr uint32_t ARROW_MAX = 0xff40c0ff;

//  The arrow reads 536 px at 1080p, 465 with a corner under the zone's badge.
constexpr double ARROW_MIN_AREA = 150;

} // namespace


std::optional<MapPoint> read_map_pin(const ImageViewRGB32& screen){
    if (!screen || screen.width() < 16 || screen.height() < 16){
        return std::nullopt;
    }
    const double width = (double)screen.width();
    const double height = (double)screen.height();
    const ImagePixelBox window(
        (size_t)(WINDOW_MIN_X / MAP_WIDTH * width), (size_t)(WINDOW_MIN_Y / MAP_HEIGHT * height),
        (size_t)(WINDOW_MAX_X / MAP_WIDTH * width), (size_t)(WINDOW_MAX_Y / MAP_HEIGHT * height)
    );
    const ImageViewRGB32 view = extract_box_reference(screen, window);
    const double scale = height / 1080.0;
    PackedBinaryMatrix matrix = compress_rgb32_to_binary_range(view, ARROW_MIN, ARROW_MAX);
    std::unique_ptr<WaterfillSession> session = make_WaterfillSession(matrix);
    std::unique_ptr<WaterfillIterator> iter = session->make_iterator((size_t)std::max(1.0, ARROW_MIN_AREA * scale * scale));
    std::optional<MapPoint> best;
    size_t best_area = 0;
    WaterfillObject blob;
    while (iter->find_next(blob, false)){
        if (blob.area <= best_area){
            continue;
        }
        best_area = blob.area;
        best = MapPoint{
            .x = ((double)window.min_x + blob.center_of_gravity_x()) / width * MAP_WIDTH,
            .y = ((double)window.min_y + blob.center_of_gravity_y()) / height * MAP_HEIGHT,
        };
    }
    return best;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

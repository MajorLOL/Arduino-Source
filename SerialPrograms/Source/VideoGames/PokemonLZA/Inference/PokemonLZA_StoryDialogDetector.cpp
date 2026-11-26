/*  PokemonLZA - Story dialog detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <memory>
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTypes/BinaryImage.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Images/BinaryImage_FilterRgb32.h"
#include "Kernels/Waterfill/Kernels_Waterfill_Session.h"
#include "PokemonLZA_StoryDialogDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Kernels::Waterfill;


StoryDialogDetector::StoryDialogDetector(StoryDialogKinds kinds)
    : m_kinds(kinds)
    , m_white(COLOR_RED)
    , m_blue(COLOR_BLUE)
    , m_teal(COLOR_GREEN)
{}

void StoryDialogDetector::make_overlays(VideoOverlaySet& items) const{
    m_white.make_overlays(items);
    m_blue.make_overlays(items);
    m_teal.make_overlays(items);
}

bool StoryDialogDetector::detect(const ImageViewRGB32& screen){
    const bool white = m_kinds.white == WhiteDialogPart::BOX_ONLY ? m_white.detect_box(screen) : m_white.detect(screen);
    return white || m_blue.detect(screen) || m_teal.detect(screen);
}


namespace{

//  `blob`'s overlap with a V over their union; each arm runs from 10% in at the top to
//  the middle at the bottom, 22% of the width across.
double v_match(const WaterfillObject& blob){
    const double width = double(blob.width());
    const double height = double(blob.height());
    size_t both = 0;
    size_t either = 0;
    for (size_t y = blob.min_y; y < blob.max_y; y++){
        const double v = (double(y - blob.min_y) + 0.5) / height;
        for (size_t x = blob.min_x; x < blob.max_x; x++){
            const double u = (double(x - blob.min_x) + 0.5) / width;
            const bool in_v = std::abs(std::abs(u - 0.5) - (0.40 * (1 - v) + 0.03)) <= 0.11;
            const bool in_blob = blob.object->get(x, y);
            if (in_v && in_blob){
                both++;
            }
            if (in_v || in_blob){
                either++;
            }
        }
    }
    return either == 0 ? 0 : double(both) / double(either);
}

//  Pixels of `mask` within `margin` of `blob`'s bounds but outside it, per blob pixel.
double stray_share(const PackedBinaryMatrix& mask, const WaterfillObject& blob, size_t margin){
    const size_t min_x = blob.min_x > margin ? blob.min_x - margin : 0;
    const size_t min_y = blob.min_y > margin ? blob.min_y - margin : 0;
    const size_t max_x = std::min(blob.max_x + margin, mask.width());
    const size_t max_y = std::min(blob.max_y + margin, mask.height());
    size_t in_window = 0;
    for (size_t y = min_y; y < max_y; y++){
        for (size_t x = min_x; x < max_x; x++){
            if (mask.get(x, y)){
                in_window++;
            }
        }
    }
    return double(in_window - blob.area) / double(blob.area);
}

//  32x19 measured on 1080p, 30-33 x 18-20 on every frame with it.
bool chevron_blob(const PackedBinaryMatrix& mask, const WaterfillObject& blob, double scale){
    const double width = double(blob.width()) / scale;
    const double height = double(blob.height()) / scale;
    if (width < 26 || width > 40 || height < 15 || height > 24){
        return false;
    }
    const double aspect = width / height;
    if (aspect < 1.4 || aspect > 2.1){
        return false;
    }
    return stray_share(mask, blob, size_t(4 * scale + 0.5)) <= 0.09 && v_match(blob) >= 0.45;
}

bool chevron_in(const ImageViewRGB32& area, double scale, uint32_t mins, uint32_t maxs){
    const PackedBinaryMatrix mask = compress_rgb32_to_binary_range(area, mins, maxs);
    PackedBinaryMatrix unfilled = mask.copy();
    const std::unique_ptr<WaterfillSession> session = make_WaterfillSession(unfilled);
    const std::unique_ptr<WaterfillIterator> blobs = session->make_iterator(1);
    WaterfillObject blob;
    while (blobs->find_next(blob, true)){
        if (chevron_blob(mask, blob, scale)){
            return true;
        }
    }
    return false;
}

} // namespace


//  The chevron (a V about 32x19 on 1080p) at x 1370-1480, y 930-1050.
AdvanceArrowDetector::AdvanceArrowDetector(Color color)
    : m_color(color)
    , m_area(1370.0 / 1920, 930.0 / 1080, 110.0 / 1920, 120.0 / 1080)
{}

void AdvanceArrowDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_area);
}

//  On JPEG frames the chevron fills the V 0.51-0.71 with at most 6% stray pixels; the
//  closest other shape (a dark blade) 0.48 with 13%, so strays are allowed up to 9%.
//  JPEG halves colour resolution, so white over orange loses blue at its edges: white
//  asks for blue 220 (190-230 all work).
bool AdvanceArrowDetector::detect(const ImageViewRGB32& screen){
    const double scale = double(screen.height()) / 1080;
    const ImageViewRGB32 area = extract_box_reference(screen, m_area);
    return chevron_in(area, scale, /*mins=*/0x00ebebdc, /*maxs=*/0xffffffff) || chevron_in(area, scale, /*mins=*/0x00000000, /*maxs=*/0xff6e6e6e);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

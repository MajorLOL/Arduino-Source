/*  PokemonLZA - Controls hint detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_PixelShares.h"
#include "PokemonLZA_ControlsHintDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The controls hint's sky blue: blue over 200, green over 160, blue over red by 60.
double sky_blue_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.blue() > 200 && pixel.green() > 160 && pixel.blue() > pixel.red() + 60;
    });
}

} // namespace


//  On 1080p: the card at x 946-971, y 868-901, spine x 974-976; "?" at x 953-965, y 876-894.
ControlsHintDetector::ControlsHintDetector(Color color)
    : m_color(color)
    , m_card(947.0 / 1920, 872.0 / 1080, 24.0 / 1920, 28.0 / 1080)
    , m_glyph(955.0 / 1920, 879.0 / 1080, 8.0 / 1920, 7.0 / 1080)
    , m_around{
        {928.0 / 1920, 872.0 / 1080, 14.0 / 1920, 28.0 / 1080},
        {980.0 / 1920, 872.0 / 1080, 14.0 / 1920, 28.0 / 1080},
        {945.0 / 1920, 856.0 / 1080, 30.0 / 1920, 8.0 / 1080},
        {945.0 / 1920, 906.0 / 1080, 30.0 / 1920, 8.0 / 1080},
    }
{}

void ControlsHintDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_card);
    for (const ImageFloatBox& box : m_around){
        items.add(m_color, box);
    }
}

//  With the hint up the card is 88%+ sky blue, the "?" at most 48%, nothing around it.
//  A sky behind comes closest (card 76%, but "?" 100%, around 54%). More rows raise the
//  card (three rows: 16-24 px), so it is searched from 60 px above to 20 px below.
bool ControlsHintDetector::detect(const ImageViewRGB32& screen){
    for (double dy = -60; dy <= 20; dy += 2){
        if (card_at(screen, dy)){
            return true;
        }
    }
    return false;
}

bool ControlsHintDetector::card_at(const ImageViewRGB32& screen, double dy) const{
    if (sky_blue_share(screen, shifted(m_card, /*dx=*/0, dy)) < 0.80 || sky_blue_share(screen, shifted(m_glyph, /*dx=*/0, dy)) > 0.70){
        return false;
    }
    for (const ImageFloatBox& box : m_around){
        if (sky_blue_share(screen, shifted(box, /*dx=*/0, dy)) > 0.10){
            return false;
        }
    }
    return true;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

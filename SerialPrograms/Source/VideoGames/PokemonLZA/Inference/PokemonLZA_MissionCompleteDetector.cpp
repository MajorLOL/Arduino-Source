/*  PokemonLZA - Mission complete detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_MissionCompleteDetector.h"
#include "PokemonLZA_PixelShares.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The mission banner's yellow: red over 200, green over 140, blue under 130, red
//  over blue by 110.
double banner_yellow_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.red() > 200 && pixel.green() > 140 && pixel.blue() < 130 &&
            pixel.red() > pixel.blue() + 110;
    });
}

//  The mission complete green: green over 180, over red by 40 and over blue by 70.
double complete_green_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.green() > 180 && pixel.green() > pixel.red() + 40 && pixel.green() > pixel.blue() + 70;
    });
}

} // namespace


//  On 1080p: big green "COMPLETE" at x 615-1300, y 271-349, none of its green 20 px
//  above or below; the bar is read left and right of the name.
MissionCompleteDetector::MissionCompleteDetector(Color color)
    : m_color(color)
    , m_word(640.0 / 1920, 285.0 / 1080, 640.0 / 1920, 55.0 / 1080)
    , m_above(620.0 / 1920, 245.0 / 1080, 680.0 / 1920, 20.0 / 1080)
    , m_below(620.0 / 1920, 358.0 / 1080, 680.0 / 1920, 20.0 / 1080)
    , m_bar_left(430.0 / 1920, 440.0 / 1080, 390.0 / 1920, 140.0 / 1080)
    , m_bar_right(1100.0 / 1920, 440.0 / 1080, 400.0 / 1920, 140.0 / 1080)
{}

void MissionCompleteDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_word);
    items.add(m_color, m_above);
    items.add(m_color, m_below);
    items.add(m_color, m_bar_left);
    items.add(m_color, m_bar_right);
}

//  On the card: word 70% green, nothing green around, bar boxes 100% yellow. Others:
//  word 11% at most; the mission banners (same bar) 2%.
bool MissionCompleteDetector::detect(const ImageViewRGB32& screen){
    if (complete_green_share(screen, m_word) < 0.40){
        return false;
    }
    if (complete_green_share(screen, m_above) > 0.10 || complete_green_share(screen, m_below) > 0.10){
        return false;
    }
    return banner_yellow_share(screen, m_bar_left) >= 0.6 && banner_yellow_share(screen, m_bar_right) >= 0.6;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  PokemonLZA - Autosave detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <initializer_list>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_PixelShares.h"
#include "PokemonLZA_AutosaveDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The icon, with the strips left, above and below it clear of white and bright cyan,
//  `dx` px right of the Chinese badge's.
bool autosave_icon_at(
    const ImageViewRGB32& screen, const ImageFloatBox& icon,
    std::initializer_list<ImageFloatBox> around, double dx
){
    const ImageFloatBox icon_here = shifted(icon, dx);
    const double white = white_share(screen, icon_here);
    if (white < 0.08 || white > 0.40 || dim_cyan_share(screen, icon_here) < 0.25){
        return false;
    }
    for (const ImageFloatBox& box : around){
        const ImageFloatBox here = shifted(box, dx);
        if (white_share(screen, here) >= 0.05 || cyan_share(screen, here) >= 0.05){
            return false;
        }
    }
    return true;
}

} // namespace


AutosaveDetector::AutosaveDetector(Color color)
    : m_color(color)
    , m_icon(1603.0 / 1920, 73.0 / 1080, 27.0 / 1920, 31.0 / 1080)
    , m_left(1570.0 / 1920, 73.0 / 1080, 28.0 / 1920, 31.0 / 1080)
    , m_above(1603.0 / 1920, 56.0 / 1080, 27.0 / 1920, 14.0 / 1080)
    , m_below(1603.0 / 1920, 108.0 / 1080, 27.0 / 1920, 14.0 / 1080)
{}

void AutosaveDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_icon);
    items.add(m_color, m_left);
    items.add(m_color, m_above);
    items.add(m_color, m_below);
}

//  The icon is 19% white and 48% cyan with nothing bright left of it; a cutscene flash
//  is bright left of it too. Texts shorter than Chinese move the right-aligned icon
//  right, so it is searched up to 240 px right. Glowing scenery glows on past the icon:
//  the strips above and below must stay under 5%. The glow is dim over dark scenes, and
//  only blue-leaning pixels count as cyan (a banner's mint or a green sign's are not).
bool AutosaveDetector::detect(const ImageViewRGB32& screen){
    for (double dx = 0; dx <= 240; dx += 4){
        if (autosave_icon_at(screen, m_icon, {m_left, m_above, m_below}, dx)){
            return true;
        }
    }
    return false;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

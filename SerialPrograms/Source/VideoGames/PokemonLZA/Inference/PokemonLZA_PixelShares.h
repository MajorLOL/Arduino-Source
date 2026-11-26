/*  PokemonLZA - Pixel shares
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Shared pixel-fraction helpers and 1080p-relative box shifting for PokemonLZA detectors.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_PixelShares_H
#define PokemonAutomation_PokemonLZA_PixelShares_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Returns the fraction (0..1) of pixels in `box` satisfying `matches(Color)`.
template <typename Matches>
double share_of(const ImageViewRGB32& screen, const ImageFloatBox& box, Matches&& matches);

//  Returns the fraction of bright cyan-glow pixels in `box`.
double cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  Returns the fraction of dimmer cyan-glow pixels in `box` (over dark or shaded scenes).
double dim_cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  Returns the fraction of bright white pixels in `box`.
double white_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  Returns `box` shifted by `dx` pixels (of 1920) horizontally and `dy` pixels (of 1080) vertically.
ImageFloatBox shifted(ImageFloatBox box, double dx, double dy = 0);


template <typename Matches>
double share_of(const ImageViewRGB32& screen, const ImageFloatBox& box, Matches&& matches){
    const ImageViewRGB32 area = extract_box_reference(screen, box);
    const size_t total = area.width() * area.height();
    if (total == 0){
        return 0;
    }
    size_t count = 0;
    for (size_t y = 0; y < area.height(); y++){
        for (size_t x = 0; x < area.width(); x++){
            if (matches(Color(area.pixel(x, y)))){
                count++;
            }
        }
    }
    return (double)count / (double)total;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

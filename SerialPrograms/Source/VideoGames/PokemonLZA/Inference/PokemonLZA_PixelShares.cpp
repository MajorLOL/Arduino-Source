/*  PokemonLZA - Pixel shares
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_PixelShares.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Cyan glow: blue over red by 50, green over red by 30, blue over 120, blue >= green
//  - 20 (keeps out mint and sign green, which have green far over blue).
double cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.blue() > pixel.red() + 50 && pixel.green() > pixel.red() + 30 && pixel.blue() > 120 &&
            pixel.blue() + 20 >= pixel.green();
    });
}

//  Dimmer cyan glow (red margins 40 and 25, blue over 70): autosave glow over dark scenes.
double dim_cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.blue() > pixel.red() + 40 && pixel.green() > pixel.red() + 25 && pixel.blue() > 70 &&
            pixel.blue() + 20 >= pixel.green();
    });
}

//  Bright white: every channel above 200.
double white_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.red() > 200 && pixel.green() > 200 && pixel.blue() > 200;
    });
}

ImageFloatBox shifted(ImageFloatBox box, double dx, double dy){
    box.x += dx / 1920;
    box.y += dy / 1080;
    return box;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

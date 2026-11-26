/*  PokemonLZA - Tutorial card detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonLZA_TutorialCardDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Measured on 5 cards and 9 other screens: cards' panel 241-246 (text spreads it to
//  40-48), edges 61-76 with spread under 7; others' panel at most 185, or edges
//  brighter or busy.
bool tutorial_card_shows(const ImageViewRGB32& frame){
    const ImageStats panel = image_stats(extract_box_reference(frame, ImageFloatBox(0.32, 0.62, 0.36, 0.20)));
    if (panel.average.sum() / 3 < 225){
        return false;
    }
    for (const ImageFloatBox& side : {ImageFloatBox(0.03, 0.30, 0.15, 0.40), ImageFloatBox(0.82, 0.30, 0.15, 0.40)}){
        const ImageStats edge = image_stats(extract_box_reference(frame, side));
        if (edge.average.sum() / 3 > 100 || edge.stddev.sum() / 3 > 12){
            return false;
        }
    }
    return true;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

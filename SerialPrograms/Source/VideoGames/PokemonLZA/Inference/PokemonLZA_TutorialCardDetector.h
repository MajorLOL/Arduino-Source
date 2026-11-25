/*  PokemonLZA - Tutorial card detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Detects modal tutorial cards shown during story progression.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_TutorialCardDetector_H
#define PokemonAutomation_PokemonLZA_TutorialCardDetector_H

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Returns true when a modal tutorial card is visible on `frame`.
bool tutorial_card_shows(const ImageViewRGB32& frame);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

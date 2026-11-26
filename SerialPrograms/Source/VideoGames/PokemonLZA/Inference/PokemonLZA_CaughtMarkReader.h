/*  PokemonLZA - Caught mark reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads a locked-on wild Pokemon's name plate and caught-species mark.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_CaughtMarkReader_H
#define PokemonAutomation_PokemonLZA_CaughtMarkReader_H

#include <optional>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Returns whether the locked-on name plate shows a caught Poke Ball mark, or std::nullopt if no plate is found.
std::optional<bool> lock_target_caught(const ImageViewRGB32& screen);

//  Returns the locked-on name plate's HP bar bounds (normalized screen coordinates), or std::nullopt if not found.
std::optional<ImageFloatBox> lock_target_plate_bar(const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

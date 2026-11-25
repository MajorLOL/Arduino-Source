/*  PokemonLZA - Map pin reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads the player's position arrow on the full map screen.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_MapPinReader_H
#define PokemonAutomation_PokemonLZA_MapPinReader_H

#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "PokemonLZA_MapPoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Returns the player arrow's position in 800 x 450 map coordinates, or std::nullopt if unread.
std::optional<MapPoint> read_map_pin(const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

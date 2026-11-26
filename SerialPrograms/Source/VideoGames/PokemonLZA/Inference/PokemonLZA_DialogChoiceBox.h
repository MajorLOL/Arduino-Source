/*  PokemonLZA - Dialog choice box
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Choice-arrow search region and choice-row reader for dialogue menus.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_DialogChoiceBox_H
#define PokemonAutomation_PokemonLZA_DialogChoiceBox_H

#include <cstddef>
#include <optional>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Returns the normalized search box for the dialogue selection arrow.
const ImageFloatBox& dialog_choice_box();

//  Row count and 0-based highlighted row index (0 = top).
struct DialogChoiceRows{
    size_t count = 0;
    size_t selected = 0;
};

//  Reads choice-box rows and the single highlighted row from `frame`, or returns std::nullopt.
std::optional<DialogChoiceRows> read_dialog_choice_rows(const ImageViewRGB32& frame);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

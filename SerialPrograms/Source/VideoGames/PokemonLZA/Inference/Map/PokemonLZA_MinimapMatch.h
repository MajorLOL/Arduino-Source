/*  PokemonLZA - Minimap match
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Matches the top-left minimap against embedded ReferenceData cell grids.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_MinimapMatch_H
#define PokemonAutomation_PokemonLZA_MinimapMatch_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/ImageTools/FloatPixel.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The ReferenceData/*.json files built into the program, (name, text) by name.
const std::vector<std::pair<std::string, std::string>>& autostory_reference_json();

//  Cell colors, row by row.
using ReferenceCells = std::vector<FloatPixel>;

//  Parses a {"cols": n, "rows": ["rrggbb...", ...]} grid; throws ParseException
//  naming `source` unless it is `cols` x `rows`.
ReferenceCells read_cell_grid(const JsonObject& grid, size_t cols, size_t rows, std::string_view source);

//  Returns the nearest built-in reference name to `screen`'s minimap ring within
//  the match threshold, or std::nullopt if none matches.
std::optional<std::string> matching_minimap(const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

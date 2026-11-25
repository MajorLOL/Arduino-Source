/*  Grid Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a player stands on a 2D tile grid (FRLG, BDSP): a map, the tiles
 *  they may be on and the way they face. Games that move on a surface or
 *  in 3D (PLZA) need their own type.
 *
 */

#ifndef PokemonAutomation_Pokemon_GridPose_H
#define PokemonAutomation_Pokemon_GridPose_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Grid/Maps/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One of `tiles` of `map`, facing `facing` if set. As a target, any of the
//  tiles will do.
struct GridPose{
    std::string map = {};
    std::vector<TilePos> tiles = {};
    std::optional<Direction> facing = std::nullopt;

    bool operator==(const GridPose& other) const = default;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

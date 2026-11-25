/*  Grid
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


TilePos TilePos::next(Direction direction) const{
    switch (direction){
    case Direction::NORTH:
        return {x, y - 1};
    case Direction::EAST:
        return {x + 1, y};
    case Direction::SOUTH:
        return {x, y + 1};
    case Direction::WEST:
        return {x - 1, y};
    }
    return *this;
}

std::string to_string(const TilePos& pos){
    return std::format("({}, {})", pos.x, pos.y);
}



} // namespace Pokemon
} // namespace PokemonAutomation

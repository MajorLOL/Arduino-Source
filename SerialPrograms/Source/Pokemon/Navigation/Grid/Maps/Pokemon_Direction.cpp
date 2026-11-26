/*  Direction
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_Direction.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string direction_name(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return "north";
    case Direction::EAST:
        return "east";
    case Direction::SOUTH:
        return "south";
    case Direction::WEST:
        return "west";
    }
    return "?";
}



} // namespace Pokemon
} // namespace PokemonAutomation

/*  Direction
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The four cardinal directions the player can face or move in.
 *
 */

#ifndef PokemonAutomation_Pokemon_Direction_H
#define PokemonAutomation_Pokemon_Direction_H

#include <string>

namespace PokemonAutomation{
namespace Pokemon{


//  Row 0 is the top of a map, so SOUTH increases y.
enum class Direction{
    NORTH,
    EAST,
    SOUTH,
    WEST,
};
// Array containing all four cardinal directions.
constexpr Direction ALL_DIRECTIONS[] = {
    Direction::NORTH, Direction::EAST, Direction::SOUTH, Direction::WEST,
};
// Returns the lowercase name string of a cardinal direction.
std::string direction_name(Direction direction);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

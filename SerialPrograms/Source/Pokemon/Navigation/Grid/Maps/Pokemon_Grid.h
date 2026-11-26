/*  Grid
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Tiles on an overworld grid, shared by the games that
 *  move the player a tile at a time.
 *
 */

#ifndef PokemonAutomation_Pokemon_Grid_H
#define PokemonAutomation_Pokemon_Grid_H

#include <compare>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include "Pokemon_Direction.h"

namespace PokemonAutomation{
namespace Pokemon{


// Grid coordinate with integer x and y components.
struct TilePos{
    int x = 0;
    int y = 0;

    TilePos next(Direction direction) const;
    bool operator==(const TilePos& other) const = default;
    //  x, then y: an order for sets and maps of tiles, not a walking order.
    auto operator<=>(const TilePos& other) const = default;
};
// Formats a TilePos as "(x, y)".
std::string to_string(const TilePos& pos);


//  One value per tile of a map, row by row from the top.
template <typename Cell>
struct Grid{
    int width = 0;
    int height = 0;
    std::vector<Cell> cells;

    bool contains(const TilePos& pos) const{
        return 0 <= pos.x && pos.x < width && 0 <= pos.y && pos.y < height;
    }
    //  `pos` must be inside the grid.
    const Cell& at(const TilePos& pos) const{
        return cells[(size_t)pos.y * width + pos.x];
    }
};
//  The cell at `pos` of `grid`, to change it; `pos` must be inside the grid.
//  A free function, so that Grid stays a plain value whose methods only read.
template <typename Cell>
Cell& cell_at(const TilePos& pos, Grid<Cell>& grid);

template <typename Cell>
Cell& cell_at(const TilePos& pos, Grid<Cell>& grid){
    return grid.cells[(size_t)pos.y * grid.width + pos.x];
}



} // namespace Pokemon
} // namespace PokemonAutomation


// Hash specialization for TilePos.
template <>
struct std::hash<PokemonAutomation::Pokemon::TilePos>{
    size_t operator()(const PokemonAutomation::Pokemon::TilePos& pos) const{
        return std::hash<long long>()(((long long)pos.x << 32) ^ (unsigned)pos.y);
    }
};


#endif

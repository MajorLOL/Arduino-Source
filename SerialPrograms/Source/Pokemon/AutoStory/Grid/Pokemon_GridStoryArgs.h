/*  Grid Story Args
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Typed JSON parsers for the grid story types shared by the tile games
 *  (FRLG, BDSP): a direction, a grid pose, straight legs, items, and a
 *  checkpoint's declared tile pose. Action parsers call these at plan load
 *  so malformed or unknown fields throw ParseException naming the source.
 *
 */

#ifndef PokemonAutomation_Pokemon_GridStoryArgs_H
#define PokemonAutomation_Pokemon_GridStoryArgs_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_Direction.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A straight leg of a fixed walk: {"direction": "north", "tiles": 3}.
struct StoryLeg{
    Direction direction = Direction::NORTH;
    int tiles = 0;

    bool operator==(const StoryLeg& other) const = default;
};

//  An item and how many to get: {"item": "Potion", "quantity": 2}.
struct StoryItem{
    std::string item = {};
    size_t quantity = 0;

    bool operator==(const StoryItem& other) const = default;
};

//  A checkpoint's declared tile pose: on one of `tiles` of `map` (any tile
//  when none), facing `facing` if set.
struct TileDeclared{
    std::string map = {};
    std::vector<TilePos> tiles = {};
    std::optional<Direction> facing = std::nullopt;

    bool operator==(const TileDeclared& other) const = default;
};

//  A wire direction name ("north", "east", "south", "west") to Direction.
Direction read_direction(std::string_view name, std::string_view source);

//  A grid pose read from JSON; no "tiles" reads as none (any tile).
//  Malformed and unknown fields throw ParseException naming `source`.
GridPose read_grid_pose(const JsonValue& value, std::string_view source);

//  Straight legs read from JSON, [{"direction": ..., "tiles": ...}].
std::vector<StoryLeg> read_story_legs(const JsonValue& value, std::string_view source);

//  Items read from JSON, [{"item": ..., "quantity": ...}].
std::vector<StoryItem> read_story_items(const JsonValue& value, std::string_view source);

//  A declared tile pose read from JSON (same shape as GridPose).
TileDeclared read_tile_declared(const JsonValue& value, std::string_view source);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

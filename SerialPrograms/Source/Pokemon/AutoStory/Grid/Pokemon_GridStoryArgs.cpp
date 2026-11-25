/*  Grid Story Args
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include <format>
#include <limits>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "Pokemon_GridStoryArgs.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  The wire names of the directions.
constexpr std::array<std::pair<Direction, std::string_view>, 4> DIRECTIONS{{
    {Direction::NORTH, "north"},
    {Direction::EAST, "east"},
    {Direction::SOUTH, "south"},
    {Direction::WEST, "west"},
}};

//  `value` as an object with no field outside `keys`.
const JsonObject& checked_object(
    const JsonValue& value, std::initializer_list<std::string_view> keys, std::string_view source
){
    const JsonObject& object = value.to_object_throw(std::string(source));
    try{
        reject_unknown_keys(object, keys);
    }catch (const ParseException& error){
        throw ParseException(std::format("{}: {}", source, error.message()));
    }
    return object;
}

int read_int(const JsonValue& value, std::string_view source){
    const int64_t number = value.to_integer_throw(std::string(source));
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()){
        throw ParseException(std::format("{}: number exceeds native int", source));
    }
    return (int)number;
}

std::vector<TilePos> read_tiles(const JsonValue& value, std::string_view source){
    std::vector<TilePos> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        const JsonArray& pair = item.to_array_throw(std::string(source));
        if (pair.size() != 2){
            throw ParseException(std::format("{}: a tile needs two coordinates", source));
        }
        result.push_back({read_int(pair[0], source), read_int(pair[1], source)});
    }
    return result;
}


} // namespace


Direction read_direction(std::string_view name, std::string_view source){
    for (const auto& [value, label] : DIRECTIONS){
        if (label == name){
            return value;
        }
    }
    throw ParseException(std::format("{}: unsupported direction {}", source, name));
}

GridPose read_grid_pose(const JsonValue& value, std::string_view source){
    const JsonObject& root = checked_object(value, {"map", "tiles", "facing"}, source);
    GridPose result;
    result.map = root.get_string_throw("map");
    if (const JsonValue* field = root.get_value("tiles")){
        result.tiles = read_tiles(*field, source);
    }
    if (const JsonValue* field = root.get_value("facing")){
        result.facing = read_direction(field->to_string_throw(), source);
    }
    return result;
}

std::vector<StoryLeg> read_story_legs(const JsonValue& value, std::string_view source){
    std::vector<StoryLeg> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        const JsonObject& row = checked_object(item, {"direction", "tiles"}, source);
        result.push_back({
            .direction = read_direction(row.get_string_throw("direction"), source),
            .tiles = read_int(row.get_value_throw("tiles"), source),
        });
    }
    return result;
}

std::vector<StoryItem> read_story_items(const JsonValue& value, std::string_view source){
    std::vector<StoryItem> result;
    for (const JsonValue& item : value.to_array_throw(std::string(source))){
        const JsonObject& row = checked_object(item, {"item", "quantity"}, source);
        const int64_t quantity = row.get_value_throw("quantity").to_integer_throw(std::string(source));
        if (quantity < 0){
            throw ParseException(std::format("{}: a quantity is not negative", source));
        }
        result.push_back({.item = row.get_string_throw("item"), .quantity = (size_t)quantity});
    }
    return result;
}

TileDeclared read_tile_declared(const JsonValue& value, std::string_view source){
    GridPose grid = read_grid_pose(value, source);
    return TileDeclared{
        .map = std::move(grid.map),
        .tiles = std::move(grid.tiles),
        .facing = grid.facing,
    };
}



} // namespace Pokemon
} // namespace PokemonAutomation

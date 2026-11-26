/*  PokemonLZA AutoStory - World walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Console-backed map-pin waypoint walking.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryWorldWalks_H
#define PokemonAutomation_PokemonLZA_AutoStoryWorldWalks_H

#include <optional>
#include <span>
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryConsoleBackend.h"
#include "PokemonLZA_AutoStoryZone1.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Opens the full map, reads the player pin, and closes the map; returns std::nullopt if unread.
std::optional<MapPoint> read_pin_on_map(StoryEnv& env, PlzaBackend& backend);

//  Walks sequentially through `waypoints` using full-map pin feedback.
//  Fails as "leg_off_target", "map_pin_not_read", or "heading_missed".
std::optional<Pokemon::StoryFailure> walk_map_legs(StoryEnv& env, PlzaBackend& backend, std::span<const MapWaypoint> waypoints);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

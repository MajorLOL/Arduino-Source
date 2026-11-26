/*  PokemonLZA AutoStory - Wild Zone 1
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure Wild Zone 1 map waypoints, exit routing, and zone-membership checks.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZone1_H
#define PokemonAutomation_PokemonLZA_AutoStoryZone1_H

#include <optional>
#include <string_view>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Target waypoint on the full map.
struct MapWaypoint{
    std::string_view name = {};
    MapPoint pin = {};
    double stop_radius = 0;
};

inline constexpr double CATCH_STOP_RADIUS = 12;

inline constexpr MapWaypoint ZONE1_ROCKS{.name = "Taunie's rocks", .pin = {.x = 476, .y = 224}};
inline constexpr MapWaypoint ZONE1_SPOT_100{.name = "the 100 spot", .pin = {.x = 442, .y = 227}, .stop_radius = CATCH_STOP_RADIUS};
//  Sidewalk by the blue kiosk (Weedle / Pichu lawn);
//  inside the Wild Zone 1 pentagon, clear of both its red reticle ring and MAP_CENTRE.
inline constexpr MapWaypoint ZONE1_SPOT_80{.name = "the 80 spot", .pin = {.x = 408, .y = 234}, .stop_radius = CATCH_STOP_RADIUS};

//  Returns map-pin waypoints from `start` to Wild Zone 1's far exit gate.
std::vector<MapPoint> zone_1_exit_legs(MapPoint start);

//  True if `pin` lies inside Wild Zone 1's bounds (excluding the out-of-zone map center).
bool pin_in_zone_1(MapPoint pin);

//  Confirms via `pin` reads that the player has exited Wild Zone 1.
//  Fails as "in_wild_zone" or "zone_unread".
std::optional<Pokemon::StoryFailure> check_out_of_zone_1(PlzaBackend& backend, MoveOnlyFunction<std::optional<MapPoint>()> pin);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

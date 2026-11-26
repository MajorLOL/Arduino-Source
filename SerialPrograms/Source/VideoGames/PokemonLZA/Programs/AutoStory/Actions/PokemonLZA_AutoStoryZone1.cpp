/*  PokemonLZA AutoStory - Wild Zone 1
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include <span>
#include <string>
#include "PokemonLZA_AutoStoryMapLegs.h"
#include "PokemonLZA_AutoStoryZone1.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

namespace{

//  Wild Zone 1's green outline on the full map, clockwise from its north
//  west corner, traced from map frames; every pin read in the zone so far
//  is inside it.
constexpr MapPoint ZONE1_OUTLINE[] = {
    {.x = 281, .y = 199}, {.x = 506, .y = 199}, {.x = 515, .y = 218}, {.x = 510, .y = 226},
    {.x = 500, .y = 230}, {.x = 450, .y = 248}, {.x = 400, .y = 260}, {.x = 370, .y = 265},
    {.x = 359, .y = 264}, {.x = 357, .y = 248}, {.x = 281, .y = 249},
};

//  Out of every wild zone the map opens on the player at its centre: on the
//  street outside the far gate the pin read (401, 223)-(401, 225). Keep the
//  radius tight so catching Weedle at (402, 230) inside Wild Zone 1 stays in-zone.
constexpr MapPoint MAP_CENTRE{.x = 401, .y = 224};
constexpr double MAP_CENTRE_RADIUS = 3;

constexpr size_t OUT_OF_ZONE_PIN_READS = 2;

//  Into the far gate along its line, west down the road; a run 20 degrees
//  off it ended on a pillar (2026-11-17).
constexpr double GATE_HEADING = 270;
constexpr double GATE_HEADING_TOLERANCE = 8;

//  The pin between the far gate's pillars, where "A Leave" shows; 1 north
//  of the (361, 253) read, as runs with no prompt ended at y 255 on the
//  left pillar.
constexpr MapPoint ZONE1_FAR_GATE{.x = 361, .y = 252};

//  Just east of the far gate on its center line so any point in Wild Zone 1
//  runs diagonally straight toward the opening, then straight west between the pillars.
constexpr MapPoint ZONE1_GATE_RUN_UP{.x = 375, .y = 252};

bool at_map_centre(MapPoint pin){
    return std::hypot(pin.x - MAP_CENTRE.x, pin.y - MAP_CENTRE.y) <= MAP_CENTRE_RADIUS;
}

//  Even-odd rule.
bool inside(std::span<const MapPoint> outline, MapPoint point){
    bool in = false;
    size_t previous = outline.size() - 1;
    for (size_t index = 0; index < outline.size(); previous = index++){
        const MapPoint& a = outline[index];
        const MapPoint& b = outline[previous];
        if ((a.y > point.y) == (b.y > point.y)){
            continue;
        }
        const double crossing_x = a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
        if (point.x < crossing_x){
            in = !in;
        }
    }
    return in;
}

} // namespace

bool pin_in_zone_1(MapPoint pin){
    if (at_map_centre(pin)){
        return false;
    }
    return inside(ZONE1_OUTLINE, pin);
}

std::vector<MapPoint> zone_1_exit_legs(MapPoint start){
    if (start.x > ZONE1_FAR_GATE.x && std::abs(bearing(start, ZONE1_FAR_GATE) - GATE_HEADING) <= GATE_HEADING_TOLERANCE){
        return {ZONE1_FAR_GATE};
    }
    return {ZONE1_GATE_RUN_UP, ZONE1_FAR_GATE};
}

std::optional<Pokemon::StoryFailure> check_out_of_zone_1(PlzaBackend& backend, MoveOnlyFunction<std::optional<MapPoint>()> pin){
    std::string reads;
    for (size_t read = 1; read <= OUT_OF_ZONE_PIN_READS; read++){
        const std::optional<MapPoint> at = pin ? pin() : std::nullopt;
        if (!at){
            const std::string detail = std::format(
                "no map pin on read {} of {}{}", read, OUT_OF_ZONE_PIN_READS, reads.empty() ? "" : std::format(" (before: {})", reads)
            );
            backend.log(std::format("[PLZA] out of Wild Zone 1 not known: {}", detail));
            return Pokemon::StoryFailure{.code = "zone_unread", .detail = std::format("out of Wild Zone 1 not known: {}", detail)};
        }
        reads += std::format("{}({:.0f}, {:.0f})", reads.empty() ? "" : ", ", at->x, at->y);
        if (pin_in_zone_1(*at)){
            backend.log(std::format("[PLZA] still in Wild Zone 1: the map pin reads {}", reads));
            return Pokemon::StoryFailure{.code = "in_wild_zone", .detail = std::format("still in Wild Zone 1: the map pin reads {}", reads)};
        }
        if (at_map_centre(*at)){
            break;
        }
    }
    backend.log(std::format("[PLZA] out of Wild Zone 1: the map pin reads {}", reads));
    return std::nullopt;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

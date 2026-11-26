/*  PokemonLZA AutoStory - World walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "VideoGames/PokemonLZA/Inference/Map/PokemonLZA_MapPinReader.h"
#include "PokemonLZA_AutoStoryWalks.h"
#include "PokemonLZA_AutoStoryMapLegs.h"
#include "PokemonLZA_AutoStoryWorldWalks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;

namespace{

//  From the shots, not timed.
constexpr Milliseconds MAP_OPEN_WAIT(1500);
constexpr Milliseconds MAP_CLOSE_WAIT(1000);
constexpr int PIN_LOOKS = 8;

std::string pin_text(MapPoint pin){
    return std::format("({:.0f}, {:.0f})", pin.x, pin.y);
}

std::string pin_text(const std::optional<MapPoint>& pin){
    return pin ? pin_text(*pin) : std::string("none");
}

std::string waypoint_text(const MapWaypoint& waypoint){
    return std::format("{} {}", waypoint.name, pin_text(waypoint.pin));
}

} // namespace


std::optional<MapPoint> read_pin_on_map(StoryEnv& env, PlzaBackend& backend){
    if (env.cached_map_pin){
        const MapPoint pin = *env.cached_map_pin;
        env.cached_map_pin.reset();
        backend.log(std::format("[PLZA] map pin (cached): {}", pin_text(pin)));
        return pin;
    }
    //  No minimap arrow read here: indoors there is none.
    backend.observe();
    backend.press(BUTTON_PLUS, /*hold=*/100ms, /*release=*/MAP_OPEN_WAIT);
    std::optional<MapPoint> pin;
    for (int looks = 0; looks < PIN_LOOKS && !pin; looks++){
        env.context.wait_for_all_requests();
        const VideoSnapshot frame = env.env.console.video().snapshot();
        if (frame){
            pin = read_map_pin(frame);
        }
        if (!pin){
            env.context.wait_for(250ms);
        }
    }
    backend.press(BUTTON_B, /*hold=*/100ms, /*release=*/MAP_CLOSE_WAIT);
    backend.log(pin ? std::format("[PLZA] map pin: {}", pin_text(*pin)) : std::string("[PLZA] map pin: none"));
    return pin;
}

namespace{

struct LegWalk{
    std::optional<Pokemon::StoryFailure> outcome;
    std::optional<MapPoint> pin;
};

LegWalk leg_over(PlzaBackend& backend, const MapWaypoint& waypoint, MapLegStep step, std::optional<MapPoint> pin){
    switch (step){
    case MapLegStep::ARRIVED:
        backend.log(std::format("[PLZA] map leg to {}: arrived at {}", waypoint_text(waypoint), pin_text(pin)));
        return {std::nullopt, pin};
    case MapLegStep::OFF_TARGET:
        return {Pokemon::StoryFailure{.code = "leg_off_target", .detail = std::format(
            "wanted {}, at {} ({})", pin_text(waypoint.pin), pin_text(pin), waypoint.name
        )}, pin};
    default:
        return {Pokemon::StoryFailure{.code = "map_pin_not_read", .detail = std::format("on the way to {}", waypoint.name)}, pin};
    }
}

LegWalk walk_leg(StoryEnv& env, PlzaBackend& backend, const MapWaypoint& waypoint, std::optional<MapPoint> pin){
    MapLeg leg(waypoint.pin, waypoint.stop_radius);
    while (true){
        backend.scope().throw_if_cancelled();
        const MapLegMove move = leg.next(pin);
        if (move.step == MapLegStep::READ_AGAIN){
            //  The map's cursor can snap onto a badge next to the player, its ring
            //  over the player's arrow (the zone's "1" at the 60 spot).
            backend.log("[PLZA] map pin not read: a short step, then the map again");
            backend.stick(/*degrees=*/0, /*hold=*/300ms, /*release=*/300ms, Pokemon::StoryPace::WALK);
            pin = read_pin_on_map(env, backend);
            continue;
        }
        if (move.step != MapLegStep::WALK){
            return leg_over(backend, waypoint, move.step, pin);
        }
        backend.log(std::format(
            "[PLZA] map leg to {}: {} from {}, heading {:.0f}, hold {} ms",
            waypoint_text(waypoint), move.correction == 0 ? std::string("walk") : std::format("correction {}", move.correction),
            pin_text(pin), move.heading, move.hold.count()
        ));
        const std::optional<Pokemon::StoryFailure> faced = face_heading(backend, minimap_arrow(env), move.heading);
        if (faced){
            return {faced, pin};
        }
        backend.stick(/*degrees=*/0, move.hold, /*release=*/300ms, Pokemon::StoryPace::WALK);
        pin = read_pin_on_map(env, backend);
    }
}

} // namespace


std::optional<Pokemon::StoryFailure> walk_map_legs(StoryEnv& env, PlzaBackend& backend, std::span<const MapWaypoint> waypoints){
    std::optional<MapPoint> pin = read_pin_on_map(env, backend);
    for (const MapWaypoint& waypoint : waypoints){
        const LegWalk walk = walk_leg(env, backend, waypoint, pin);
        if (walk.outcome){
            return walk.outcome;
        }
        pin = walk.pin;
    }
    return std::nullopt;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

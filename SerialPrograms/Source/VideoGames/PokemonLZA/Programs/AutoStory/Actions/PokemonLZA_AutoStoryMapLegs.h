/*  PokemonLZA AutoStory - Map legs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure bearing calculation and closed-loop waypoint leg state machine.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMapLegs_H
#define PokemonAutomation_PokemonLZA_AutoStoryMapLegs_H

#include <cstddef>
#include <optional>
#include "Common/Cpp/Time.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Returns the minimap bearing in degrees from `from` to `to` (0 = north/up, 90 = east/right).
double bearing(MapPoint from, MapPoint to);

//  Next action returned by MapLeg::next() after a map-pin reading.
enum class MapLegStep{
    ARRIVED,
    WALK,
    READ_AGAIN,
    OFF_TARGET,
    PIN_NOT_READ,
};

//  Command produced by MapLeg::next() for one step toward a waypoint.
struct MapLegMove{
    MapLegStep step = MapLegStep::ARRIVED;
    double heading = 0;
    Milliseconds hold = Milliseconds(0);
    size_t correction = 0;
};

//  Closed-loop controller stepping from full-map pin readings to `target` within `stop_radius`.
class MapLeg{
public:
    MapLeg(MapPoint target, double stop_radius)
        : m_target(target)
        , m_stop_radius(stop_radius)
    {}

    //  Computes the next move from the latest pin reading (std::nullopt if unread).
    MapLegMove next(std::optional<MapPoint> pin);

private:
    bool in_stop_area(MapPoint pin, double distance) const;

    const MapPoint m_target;
    const double m_stop_radius;
    size_t m_walks = 0;
    bool m_missed = false;
    std::optional<MapPoint> m_walked_from;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

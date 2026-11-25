/*  PokemonLZA AutoStory - Map legs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <numbers>
#include "PokemonLZA_AutoStoryMapLegs.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  Map pixels a second: the median of 35 measured legs was 6.9, the long
//  legs 7.0; at 6.7 every long leg overshot.
constexpr double WALK_PIXELS_PER_SECOND = 7.0;

//  ~5 marker units.
constexpr double TOLERANCE = 6;
constexpr size_t CORRECTIONS = 2;

} // namespace


double bearing(MapPoint from, MapPoint to){
    const double degrees = std::atan2(to.x - from.x, from.y - to.y) * 180 / std::numbers::pi;
    return degrees < 0 ? degrees + 360 : degrees;
}


bool MapLeg::in_stop_area(MapPoint pin, double distance) const{
    if (distance > m_stop_radius){
        return false;
    }
    if (!m_walked_from){
        return true;
    }
    //  The miss projects forward onto the walk's direction.
    const double way_x = m_target.x - m_walked_from->x;
    const double way_y = m_target.y - m_walked_from->y;
    return (pin.x - m_target.x) * way_x + (pin.y - m_target.y) * way_y >= 0;
}

MapLegMove MapLeg::next(std::optional<MapPoint> pin){
    if (!pin){
        if (m_missed){
            return {.step = MapLegStep::PIN_NOT_READ};
        }
        m_missed = true;
        return {.step = MapLegStep::READ_AGAIN};
    }
    m_missed = false;
    const double distance = std::hypot(m_target.x - pin->x, m_target.y - pin->y);
    if (distance <= TOLERANCE || in_stop_area(*pin, distance)){
        return {.step = MapLegStep::ARRIVED};
    }
    if (m_walks > CORRECTIONS){
        return {.step = MapLegStep::OFF_TARGET};
    }
    m_walked_from = *pin;
    return {
        .step = MapLegStep::WALK,
        .heading = bearing(*pin, m_target),
        .hold = Milliseconds((int64_t)(distance / WALK_PIXELS_PER_SECOND * 1000)),
        .correction = m_walks++,
    };
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

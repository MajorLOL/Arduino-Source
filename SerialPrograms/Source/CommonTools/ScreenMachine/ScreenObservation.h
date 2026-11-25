/*  Screen Observation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What one look at the game showed, as facts rather than pixels.
 *
 */

#ifndef PokemonAutomation_CommonTools_ScreenObservation_H
#define PokemonAutomation_CommonTools_ScreenObservation_H

#include <functional>
#include <map>
#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"

namespace PokemonAutomation{


//  One look at the game.
struct ScreenObservation{
    WallClock when = WallClock::min();
    ScreenEvents events = {};
    //  Named numbers the game's readers found, by names the game chooses;
    //  "heading" is degrees clockwise from north, [0, 360).
    std::map<std::string, int, std::less<>> readings = {};
    //  The frame looked at; none in a simulation.
    std::optional<VideoSnapshot> frame = std::nullopt;
};



} // namespace PokemonAutomation
#endif

/*  Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A game's detectors, read frame by frame as shared screen events.
 *
 */

#ifndef PokemonAutomation_CommonTools_ScreenEvents_ScreenEvents_H
#define PokemonAutomation_CommonTools_ScreenEvents_ScreenEvents_H

#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "ScreenEventTypes.h"

namespace PokemonAutomation{


//  A game's detectors, as events. It may keep state between frames, so one
//  observer watches one stream.
class ScreenEventObserver{
public:
    virtual ~ScreenEventObserver() = default;

    //  What `frame` shows; `now` is the caller's clock. A null frame shows
    //  nothing.
    virtual ScreenEvents observe(const VideoSnapshot& frame, WallClock now) = 0;

    //  The boxes the detectors read, for the video overlay.
    virtual void make_overlays(VideoOverlaySet& items) const = 0;
};


//  Whether the screen barely changed from `a` to `b` (SCREEN_STILL).
bool same_screen(const VideoSnapshot& a, const VideoSnapshot& b);



}  // namespace PokemonAutomation
#endif

/*  Pokemon FRLG Autostory Walk Tracker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_AutoStoryWalkTracker.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  1 pixel per frame at 59.73 frames per second, with some margin.
double max_speed(double frames_per_tile){
    const double frame_ms = 16.74;
    return AutoStoryScroll::TILE_PIXELS / frames_per_tile / frame_ms * 1.3;
}

int axis_x(DpadPosition direction){
    switch (direction){
    case DPAD_LEFT:     return -1;
    case DPAD_RIGHT:    return 1;
    default:            return 0;
    }
}
int axis_y(DpadPosition direction){
    switch (direction){
    case DPAD_UP:       return -1;
    case DPAD_DOWN:     return 1;
    default:            return 0;
    }
}

}


WalkTracker::WalkTracker(DpadPosition direction, double frames_per_tile)
    : VisualInferenceCallback("WalkTracker")
    , m_tracker(axis_x(direction), axis_y(direction), max_speed(frames_per_tile))
    , m_start(WallClock::min())
    , m_last(WallClock::min())
{}

void WalkTracker::stop_when_still(Milliseconds duration){
    m_stop_when_still_ms = (double)duration.count();
}

void WalkTracker::make_overlays(VideoOverlaySet&) const{}

bool WalkTracker::process_frame(const VideoSnapshot& frame){
    if (!frame){
        return false;
    }
    //  The same frame again.
    if (frame.timestamp == m_last){
        return false;
    }
    m_last = frame.timestamp;
    if (m_start == WallClock::min()){
        m_start = frame.timestamp;
    }

    ImageViewRGB32 game = extract_box_reference(*frame.frame, GameSettings::instance().GAME_BOX);
    AutoStoryScroll::sample_screen(
        m_screen,
        game.data(), game.bytes_per_row(),
        game.width(), game.height()
    );
    double time_ms = std::chrono::duration<double, std::milli>(frame.timestamp - m_start).count();
    //  Losing the picture does not stop anything: a battle is stopped by the
    //  battle detectors, and anything else (a fade, a text box) lets the
    //  run go on. Stopping the input then would leave a step half known.
    m_tracker.add(m_screen, time_ms);
    return !m_tracker.lost() &&
        m_stop_when_still_ms > 0 &&
        m_tracker.frames() >= 3 &&
        m_tracker.still_ms() >= m_stop_when_still_ms;
}



StableScreenWatcher::StableScreenWatcher(Milliseconds duration)
    : VisualInferenceCallback("StableScreenWatcher")
    , m_duration(duration)
    , m_last(WallClock::min())
    , m_stable_since(WallClock::min())
{}

void StableScreenWatcher::make_overlays(VideoOverlaySet&) const{}

bool StableScreenWatcher::process_frame(const VideoSnapshot& frame){
    if (!frame || frame.timestamp == m_last){
        return false;
    }
    m_last = frame.timestamp;
    ImageViewRGB32 game = extract_box_reference(*frame.frame, GameSettings::instance().GAME_BOX);
    AutoStoryScroll::sample_screen(
        m_screen,
        game.data(), game.bytes_per_row(),
        game.width(), game.height()
    );
    if (!m_has_previous){
        m_previous = m_screen;
        m_has_previous = true;
        return false;
    }
    //  The video noise is about 1. Animated tiles and people walking
    //  around add a little.
    double change = AutoStoryScroll::screen_mismatch(m_previous, m_screen, 0, 0);
    m_previous = m_screen;
    if (change > 3.0){
        m_stable_since = WallClock::min();
        return false;
    }
    if (m_stable_since == WallClock::min()){
        m_stable_since = frame.timestamp;
        return false;
    }
    return frame.timestamp - m_stable_since >= m_duration;
}



}
}
}

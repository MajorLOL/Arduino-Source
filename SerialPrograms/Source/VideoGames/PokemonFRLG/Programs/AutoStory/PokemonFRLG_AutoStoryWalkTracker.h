/*  Pokemon FRLG Autostory Walk Tracker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Watches the video while the player walks a straight run and measures how
 *  many tiles they really moved (see PokemonFRLG_AutoStoryScroll.h).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryWalkTracker_H
#define PokemonAutomation_PokemonFRLG_AutoStoryWalkTracker_H

#include <atomic>
#include "Common/Cpp/Time.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "CommonTools/VisualDetector.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "PokemonFRLG_AutoStoryScroll.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class WalkTracker : public VisualInferenceCallback{
public:
    //  "frames_per_tile": 16 walking (and jumping), 8 surfing.
    WalkTracker(DpadPosition direction, double frames_per_tile);

    //  Also stop the inference once the player has stood still this long
    //  (to let the video catch up with the end of a run).
    void stop_when_still(Milliseconds duration);
    //  Also stop the inference once the picture changes completely (a
    //  battle starting).
    void stop_when_lost(){ m_stop_when_lost = true; }
    //  When the picture changed completely, in ms after the first frame
    //  (negative: not lost).
    double lost_at_ms() const{ return m_lost_at_ms; }

    using VisualInferenceCallback::process_frame;
    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool process_frame(const VideoSnapshot& frame) override;

    const AutoStoryScroll::Tracker& tracker() const{ return m_tracker; }

    //  The map scrolls (the player walks). Safe to ask from other callbacks.
    bool moving() const{ return m_moving.load(std::memory_order_relaxed); }

private:
    AutoStoryScroll::Tracker m_tracker;
    std::atomic<bool> m_moving{false};
    AutoStoryScroll::Screen m_screen;
    WallClock m_start;
    WallClock m_last;
    double m_stop_when_still_ms = 0;
    bool m_stop_when_lost = false;
    double m_lost_at_ms = -1;
};


//  Stops when "detector" sees its thing for "duration", but only after it
//  did not see it at first: some places look like a text box all the time
//  (the white bottom of the S.S. Anne corridors), and a text box that
//  matters (a trainer talking) appears while walking.
//  With "motion": something that turns up while the map scrolls is scenery
//  coming into view (a text box does not appear while the player walks).
class TurnsOnWatcher : public VisualInferenceCallback{
public:
    TurnsOnWatcher(
        const std::string& name, StaticScreenDetector& detector, Milliseconds duration,
        const WalkTracker* motion = nullptr
    );

    using VisualInferenceCallback::process_frame;
    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool process_frame(const VideoSnapshot& frame) override;

    //  The detector saw its thing in the first frame looked at.
    bool on_at_start() const{ return m_on_at_start; }

private:
    StaticScreenDetector& m_detector;
    Milliseconds m_duration;
    const WalkTracker* m_motion;
    bool m_first = true;
    bool m_on_at_start = false;
    bool m_seen_off = false;
    WallClock m_last;
    WallClock m_on_since;
};


//  Stops once the game screen has not changed for "duration": menus are
//  closed, fades are over. A move measured from a screen that was still
//  changing would be measured wrong.
class StableScreenWatcher : public VisualInferenceCallback{
public:
    StableScreenWatcher(Milliseconds duration);

    using VisualInferenceCallback::process_frame;
    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool process_frame(const VideoSnapshot& frame) override;

private:
    Milliseconds m_duration;
    bool m_has_previous = false;
    AutoStoryScroll::Screen m_previous;
    AutoStoryScroll::Screen m_screen;
    WallClock m_last;
    WallClock m_stable_since;
};



}
}
}
#endif

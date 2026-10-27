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

#include "Common/Cpp/Time.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
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

    using VisualInferenceCallback::process_frame;
    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool process_frame(const VideoSnapshot& frame) override;

    const AutoStoryScroll::Tracker& tracker() const{ return m_tracker; }

private:
    AutoStoryScroll::Tracker m_tracker;
    AutoStoryScroll::Screen m_screen;
    WallClock m_start;
    WallClock m_last;
    double m_stop_when_still_ms = 0;
};



}
}
}
#endif

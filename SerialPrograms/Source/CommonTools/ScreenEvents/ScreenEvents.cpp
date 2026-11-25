/*  Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "ScreenEventTypes.h"
#include "ScreenFades.h"
#include "ScreenEvents.h"

namespace PokemonAutomation{


namespace{

//  A fade is uniform over the middle 80% of the screen, as the shared
//  Black/WhiteScreenWatcher test: a dim cave floor is textured, no fade.
constexpr double FADE_MAX_STDDEV_SUM = 10;
//  Dark up to an rgb sum of 225: a cave mouth's fade is dim, not black.
constexpr double FADE_MAX_DARK_SUM = 225;
//  White from an rgb sum of 500, as the shared WhiteScreenWatcher's.
constexpr double FADE_MIN_WHITE_SUM = 500;

} // namespace


const EnumStringMap<ScreenEvent>& SCREEN_EVENT_NAMES(){
    static const EnumStringMap<ScreenEvent> names{
        {ScreenEvent::FADE_BLACK,       "black fade"},
        {ScreenEvent::FADE_WHITE,       "white fade"},
        {ScreenEvent::LOCATION_BANNER,  "location banner"},
        {ScreenEvent::DIALOG_BOX,       "dialog box"},
        {ScreenEvent::CHOICE_BOX,       "choice box"},
        {ScreenEvent::INTERACT_PROMPT,  "interact prompt"},
        {ScreenEvent::BATTLE_STARTED,   "battle started"},
        {ScreenEvent::BATTLE_ENDED,     "battle ended"},
        {ScreenEvent::PLAYER_CAN_MOVE,  "player can move"},
        {ScreenEvent::SCREEN_STILL,     "screen still"},
        {ScreenEvent::SWITCH_HOME_MENU, "Switch HOME menu"},
    };
    return names;
}

const EnumStringMap<WarpFade>& WARP_FADE_NAMES(){
    static const EnumStringMap<WarpFade> names{
        {WarpFade::NONE,  "no fade"},
        {WarpFade::BLACK, "black fade"},
        {WarpFade::WHITE, "white fade"},
    };
    return names;
}


ScreenEvents::ScreenEvents(std::initializer_list<ScreenEvent> events){
    for (ScreenEvent event : events){
        set(event, true);
    }
}
void ScreenEvents::set(ScreenEvent event, bool on){
    if (on){
        m_bits |= bit(event);
    }else{
        m_bits &= ~bit(event);
    }
}

ScreenEvents fade_events(WarpFade fade){
    switch (fade){
    case WarpFade::NONE:
        return {};
    case WarpFade::BLACK:
        return {ScreenEvent::FADE_BLACK};
    case WarpFade::WHITE:
        return {ScreenEvent::FADE_WHITE};
    }
    return {};
}
WarpFade fade_in(const ScreenEvents& events){
    if (events.has(ScreenEvent::FADE_BLACK)){
        return WarpFade::BLACK;
    }
    if (events.has(ScreenEvent::FADE_WHITE)){
        return WarpFade::WHITE;
    }
    return WarpFade::NONE;
}


//  Black is is_black() (SolidColorTest, BlackScreenDetector's test) with
//  the fade's thresholds.
//  White is not is_white(): that also wants an even grey, and a fade to
//  white can be tinted.
WarpFade fade_of(const ImageStats& middle){
    if (is_black(middle, FADE_MAX_DARK_SUM, FADE_MAX_STDDEV_SUM)){
        return WarpFade::BLACK;
    }
    if (middle.stddev.sum() > FADE_MAX_STDDEV_SUM){
        return WarpFade::NONE;
    }
    return middle.average.sum() >= FADE_MIN_WHITE_SUM ? WarpFade::WHITE : WarpFade::NONE;
}
WarpFade fade_of_screen(const ImageViewRGB32& screen){
    return fade_of(image_stats(extract_box_reference(screen, ImageFloatBox(0.1, 0.1, 0.8, 0.8))));
}


bool same_screen(const VideoSnapshot& a, const VideoSnapshot& b){
    if (!a || !b || a->width() != b->width() || a->height() != b->height()){
        return false;
    }
    return ImageMatch::pixel_RMSD(a, b) < 2.0;
}


}  // namespace PokemonAutomation

/*  Screen Fades
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A fade to black or white, read from a screen.
 *
 */

#ifndef PokemonAutomation_CommonTools_ScreenEvents_ScreenFades_H
#define PokemonAutomation_CommonTools_ScreenEvents_ScreenFades_H

#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "ScreenEventTypes.h"

namespace PokemonAutomation{


//  The event of a fade: FADE_BLACK, FADE_WHITE, or none for WarpFade::NONE.
ScreenEvents fade_events(WarpFade fade);
//  The fade `events` show: BLACK before WHITE if they have both.
WarpFade fade_in(const ScreenEvents& events);


//  The fade `middle` (the stats of the screen's middle 80%) shows, if any.
WarpFade fade_of(const ImageStats& middle);
WarpFade fade_of_screen(const ImageViewRGB32& screen);



}  // namespace PokemonAutomation
#endif

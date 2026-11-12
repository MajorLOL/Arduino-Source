/*  Colored Text
 * 
 *  From: https://github.com/PokemonAutomation/
 * 
 */

#ifndef PokemonAutomation_ColoredText_H
#define PokemonAutomation_ColoredText_H

#include <string>
#include "Common/Cpp/Color.h"

namespace PokemonAutomation{

//  The PocketPilot themes (Night and Day) come first: Night is the default.
//  DEFAULT_MODE and DARK_MODE are the two classic looks.
enum class UiThemeMode{
    POCKET_NIGHT,
    POCKET_DAY,
    DEFAULT_MODE,
    DARK_MODE,
};

//  The theme in use. DEFAULT_MODE until a theme is applied: a new
//  application has no style sheet.
inline UiThemeMode CURRENT_THEME = UiThemeMode::DEFAULT_MODE;

//  The background color (0xRRGGBB) that text is shown on in the PocketPilot
//  themes.
inline uint32_t CURRENT_THEME_SURFACE = 0xffffff;

bool theme_is_dark();
bool theme_is_pocket();

//  A color changed just enough to read on the background of a PocketPilot
//  theme (a contrast of 4.5 to 1). The classic themes leave it as it is.
Color theme_readable(Color color);

Color theme_friendly_darkblue();
std::string html_color_text(const std::string& text, Color color);
std::string make_text_url(const std::string& url, const std::string& text);

}
#endif


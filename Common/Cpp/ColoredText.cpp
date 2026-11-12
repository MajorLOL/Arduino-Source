/*  Colored Text
 * 
 *  From: https://github.com/PokemonAutomation/
 * 
 */

#include <cmath>
#include "ColoredText.h"

namespace PokemonAutomation{


bool theme_is_dark(){
    return CURRENT_THEME == UiThemeMode::DARK_MODE || CURRENT_THEME == UiThemeMode::POCKET_NIGHT;
}
bool theme_is_pocket(){
    return CURRENT_THEME == UiThemeMode::POCKET_NIGHT || CURRENT_THEME == UiThemeMode::POCKET_DAY;
}

namespace{

//  The relative luminance of a color (WCAG 2), 0 to 1.
double relative_luminance(double red, double green, double blue){
    auto linear = [](double channel){
        channel /= 255.0;
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(red) + 0.7152 * linear(green) + 0.0722 * linear(blue);
}
double contrast_of(double luminance_a, double luminance_b){
    const double lighter = luminance_a > luminance_b ? luminance_a : luminance_b;
    const double darker = luminance_a > luminance_b ? luminance_b : luminance_a;
    return (lighter + 0.05) / (darker + 0.05);
}

}

Color theme_readable(Color color){
    if (!theme_is_pocket() || color == Color()){
        return color;
    }
    const double READABLE = 4.5;
    const uint32_t surface = CURRENT_THEME_SURFACE;
    const double surface_luminance = relative_luminance(
        (double)((surface >> 16) & 0xff), (double)((surface >> 8) & 0xff), (double)(surface & 0xff)
    );
    const double red = color.red();
    const double green = color.green();
    const double blue = color.blue();
    if (contrast_of(relative_luminance(red, green, blue), surface_luminance) >= READABLE){
        return color;
    }
    //  Toward white on a dark background, toward black on a light one, in
    //  steps, so that the color keeps as much of itself as it can.
    const double target = surface_luminance < 0.5 ? 255.0 : 0.0;
    const int STEPS = 24;
    for (int step = 1; step <= STEPS; step++){
        const double part = (double)step / STEPS;
        const double r = red + (target - red) * part;
        const double g = green + (target - green) * part;
        const double b = blue + (target - blue) * part;
        if (step == STEPS || contrast_of(relative_luminance(r, g, b), surface_luminance) >= READABLE){
            return Color((uint8_t)(r + 0.5), (uint8_t)(g + 0.5), (uint8_t)(b + 0.5));
        }
    }
    return color;
}

#if 1
Color theme_friendly_darkblue(){
    if (theme_is_dark()){
        return Color(0xff0080ff);
    }
    return COLOR_DARK_BLUE;
}
#endif
std::string html_color_text(const std::string& text, Color color){
    if (color == Color()){
        return text;
    }
    color = theme_readable(color);
    const char HEX[] = "0123456789abcdef";
    uint32_t rgb = (uint32_t)color;
    std::string str;
    str += HEX[(rgb >> 20) & 15];
    str += HEX[(rgb >> 16) & 15];
    str += HEX[(rgb >> 12) & 15];
    str += HEX[(rgb >>  8) & 15];
    str += HEX[(rgb >>  4) & 15];
    str += HEX[(rgb >>  0) & 15];
    return "<font color=#" + str + ">" + text + "</font>";
}
std::string make_text_url(const std::string& url, const std::string& text){
#if 0
    switch (CURRENT_THEME){
    case 0:
        return "<a href=\"" + url + "\">" + text + "</a>";
    case 1:
        return "<a href=\"" + url + "\" style=\"color: #0080ff\">" + text + "</a>";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Invalid theme #.");
#endif
    return "<a href=\"" + url + "\" style=\"color: #0080ff\">" + text + "</a>";
}






}

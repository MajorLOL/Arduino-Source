/*  Screen Event Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a frame shows, in words every game shares.
 *
 */

#ifndef PokemonAutomation_CommonTools_ScreenEvents_ScreenEventTypes_H
#define PokemonAutomation_CommonTools_ScreenEvents_ScreenEventTypes_H

#include <cstdint>
#include <format>
#include <initializer_list>
#include <string_view>
#include "Common/Cpp/EnumStringMap.h"

namespace PokemonAutomation{


//  One thing a frame shows. A game reports the ones its detectors can see.
enum class ScreenEvent : uint8_t{
    FADE_BLACK,         //  A uniform dark screen: a door, a cave mouth, a scene cut.
    FADE_WHITE,         //  A uniform white screen: a cave exit.
    LOCATION_BANNER,    //  A location banner. An observer given an expected
                        //  map reports only that map's.
    DIALOG_BOX,         //  A dialogue box.
    CHOICE_BOX,         //  A choice (Yes/No, a menu of answers).
    INTERACT_PROMPT,    //  A button prompt to interact ("A Talk").
    BATTLE_STARTED,     //  A battle shows (its menu, its HUD or its opening).
    BATTLE_ENDED,       //  A battle's closing screen.
    PLAYER_CAN_MOVE,    //  The overworld with the player in control.
    SCREEN_STILL,       //  The screen barely changed since the frame before.
    SWITCH_HOME_MENU,   //  The Switch's HOME menu: the game is not on the screen.
};
//  "dialog box", for the log.
const EnumStringMap<ScreenEvent>& SCREEN_EVENT_NAMES();


//  How a screen's fade looks: the kind of FADE_BLACK / FADE_WHITE.
enum class WarpFade{
    NONE,
    BLACK,      //  a door's black, or a cave mouth's dim fade
    WHITE,      //  a cave exit's white, with no battle after it
};
const EnumStringMap<WarpFade>& WARP_FADE_NAMES();


//  The events of one frame, or of all the frames a watcher saw.
class ScreenEvents{
public:
    ScreenEvents() = default;
    ScreenEvents(std::initializer_list<ScreenEvent> events);

    bool has(ScreenEvent event) const{
        return (m_bits & bit(event)) != 0;
    }
    bool any_of(const ScreenEvents& events) const{
        return (m_bits & events.m_bits) != 0;
    }
    //  Adds `event`, or removes it when `on` is false (as std::bitset::set).
    void set(ScreenEvent event, bool on);
    void merge(const ScreenEvents& events){
        m_bits |= events.m_bits;
    }

    bool operator==(const ScreenEvents& other) const = default;

private:
    static uint32_t bit(ScreenEvent event){
        return uint32_t(1) << (uint32_t)event;
    }

    uint32_t m_bits = 0;
};



}  // namespace PokemonAutomation


//  "dialog box, player can move"; "nothing" when empty.
template <>
struct std::formatter<PokemonAutomation::ScreenEvents> : std::formatter<std::string_view>{
    auto format(const PokemonAutomation::ScreenEvents& events, std::format_context& context) const{
        auto out = context.out();
        bool first = true;
        for (const auto& [event, name] : PokemonAutomation::SCREEN_EVENT_NAMES()){
            if (!events.has(event)){
                continue;
            }
            out = std::format_to(out, "{}{}", first ? "" : ", ", name);
            first = false;
        }
        return first ? std::format_to(out, "nothing") : out;
    }
};

#endif

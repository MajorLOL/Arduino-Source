/*  Story Backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story engine's input and observation: a console, a simulation or a
 *  scripted fake.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryBackend_H
#define PokemonAutomation_Pokemon_StoryBackend_H

#include <string_view>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How fast a held direction moves the player.
enum class StoryPace{
    WALK,
    RUN,
};

class StoryBackend{
public:
    virtual ~StoryBackend() = default;

    virtual void press(NintendoSwitch::Button button, Milliseconds hold, Milliseconds release) = 0;
    //  The move stick held at `degrees` clockwise from north (straight on,
    //  for a camera-relative game) for `duration`, then let go for `release`
    //  (a turn's time to finish) like press(): the games' walk and turn.
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, StoryPace pace) = 0;
    virtual void wait(Milliseconds duration) = 0;
    //  The game now, once every input sent so far has been done.
    virtual ScreenObservation observe() = 0;
    virtual void log(std::string_view line) = 0;
    //  A line for the file log only, not the stream's overlay.
    virtual void log_detail(std::string_view line){
        log(line);
    }
    virtual CancellableScope& scope() = 0;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

/*  Pokemon FRLG Input Timing Test
 *
 *  PocketPilot's own program (experimental: not tested on a console yet).
 *
 *  Measures how long the game takes to show its reaction to a button press,
 *  and how much that differs from press to press: what playing by the clock
 *  (a speedrun mode, hitting a frame for the random numbers) has to work
 *  with on this controller, console and capture card.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_InputTimingTest_H
#define PokemonAutomation_PokemonFRLG_InputTimingTest_H

#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StaticTextOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class InputTimingTest_Descriptor : public SingleSwitchProgramDescriptor{
public:
    InputTimingTest_Descriptor();
    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class InputTimingTest : public SingleSwitchProgramInstance{
public:
    using Descriptor = InputTimingTest_Descriptor;
    InputTimingTest();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;
    virtual void start_program_border_check(
        VideoStream& stream,
        FeedbackType feedback_type
    ) override{}

private:
    StaticTextOption SETUP_TEXT;
    SimpleIntegerOption<uint16_t> PRESSES;
    SimpleIntegerOption<uint16_t> HOLD_MS;

    EventNotificationsOption NOTIFICATIONS;
};


}
}
}
#endif

/*  Turbo Macro
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "NintendoSwitch_TurboMacro.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{
namespace NintendoSwitch{


TurboMacro_Descriptor::TurboMacro_Descriptor()
    : SingleSwitchProgramDescriptor(
        "NintendoSwitch:TurboMacro",
        "Nintendo Switch", "Turbo Macro",
        "Programs/NintendoSwitch/TurboMacro.html",
        "Create macros. Counts the loops it has done (the counter is new: experimental).",
        PanelQualityStanding::OFFICIAL,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::NONE,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

//  The loops done in this run, shown under the video like the counters of
//  other programs: how far a long run is.
struct TurboMacro_Descriptor::Stats : public StatsTracker{
    Stats()
        : loops(m_stats["Loops"])
    {
        m_display_order.emplace_back("Loops");
    }

    std::atomic<uint64_t>& loops;
};
std::unique_ptr<StatsTracker> TurboMacro_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}

TurboMacro::TurboMacro()
    : LOOP(
        "<b>Number of times to loop:</b><br>(Set to zero to loop forever.)",
        LockMode::UNLOCK_WHILE_RUNNING,
        100, 0
    )
    , TABLE(
        "Command Schedule:",
        {
            ControllerClass::NintendoSwitch_ProController,
            ControllerClass::NintendoSwitch_LeftJoycon,
            ControllerClass::NintendoSwitch_RightJoycon,
        }
    )
{
    PA_ADD_OPTION(LOOP);
    PA_ADD_OPTION(TABLE);
}


void TurboMacro::program(SingleSwitchProgramEnvironment& env, CancellableScope& scope){

    //  Connect the controller.
    //pbf_press_button(context, BUTTON_LCLICK, 40ms, 40ms);

    TurboMacro_Descriptor::Stats& stats = env.current_stats<TurboMacro_Descriptor::Stats>();
    for (uint32_t c = 0; c < LOOP || LOOP == 0; c++){
        TABLE.run(scope, env.console.controller());
        stats.loops++;
        env.update_stats();
    }
}


}
}

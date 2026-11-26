/*  Lock-On Sweep
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_MoveEffectivenessSymbol.h"
#include "PokemonLZA_LockOnSweep.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono;


void sweep_camera_for_lock_on(ProControllerContext& context, double camera_x, int rounds){
    for (int c = 0; c < rounds; c++){
        ssf_press_right_joystick(context, {camera_x, 0}, /*delay=*/0ms, /*hold=*/1000ms, /*cool=*/0ms);
        ssf_press_dpad(context, DpadPosition::DPAD_UP, /*delay=*/0ms, /*hold=*/160ms, /*cool=*/160ms);
        pbf_mash_button(context, BUTTON_ZL, /*duration=*/1000ms);
        pbf_press_button(context, BUTTON_ZL, /*hold=*/500ms, /*release=*/0ms);
    }
    pbf_press_button(context, BUTTON_L, /*hold=*/160ms, /*release=*/160ms);
}

bool lock_on_by_sweep(ConsoleHandle& console, ProControllerContext& context, double camera_x, int rounds){
    MoveEffectivenessSymbolWatcher battle_menu(COLOR_RED, &console.overlay(), /*hold_duration=*/100ms);
    context.wait_for_all_requests();
    int ret = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& context){
            sweep_camera_for_lock_on(context, camera_x, rounds);
        },
        {battle_menu}
    );
    context.wait_for_all_requests();
    return ret == 0;
}


}  // namespace PokemonLZA
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

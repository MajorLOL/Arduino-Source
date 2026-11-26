/*  Lock-On Sweep
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_LockOnSweep_H
#define PokemonAutomation_PokemonLZA_LockOnSweep_H

#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


// `rounds` rounds of: right stick at `camera_x` (negative turns left) for 1 s, D-pad up,
// ZL mashed 1 s and held 0.5 s; then L puts the camera back. Presses only: pair it with
// a MoveEffectivenessSymbolWatcher.
void sweep_camera_for_lock_on(ProControllerContext& context, double camera_x, int rounds);

// sweep_camera_for_lock_on() until the move effectiveness symbols show (a lock-on);
// false if the sweep ended without one.
bool lock_on_by_sweep(ConsoleHandle& console, ProControllerContext& context, double camera_x, int rounds);


}  // namespace PokemonLZA
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif

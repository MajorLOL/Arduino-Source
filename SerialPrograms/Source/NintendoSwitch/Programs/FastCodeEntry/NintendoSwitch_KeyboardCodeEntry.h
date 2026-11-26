/*  Keyboard Code Entry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_KeyboardCodeEntry_H
#define PokemonAutomation_NintendoSwitch_KeyboardCodeEntry_H

#include <string>
#include <vector>
#include "Controllers/StandardHid/StandardHid_Keyboard.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_KeyboardEntryMappings.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_CodeEntryTools.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace FastCodeEntry{



//  The D-pad moves from "source" to "destination" on the on-screen keyboard,
//  wrapping around the sides when that is shorter, then ENTER_CHAR.
std::vector<CodeEntryAction> keyboard_get_path(
    KeyboardEntryPosition source,
    KeyboardEntryPosition destination
);

//  Automatically select between keyboard or pro controller depending on the
//  controller type.
void keyboard_enter_code(
    ConsoleHandle& console, AbstractControllerContext& context,
    bool assume_console_type_is_ready,
    KeyboardLayout keyboard_layout,
    const std::string& code, bool include_plus
);

void keyboard_enter_code(
    ConsoleHandle& console, StandardHid::KeyboardContext& context,
    KeyboardLayout keyboard_layout,
    const std::string& code, bool include_plus
);
void keyboard_enter_code(
    ConsoleHandle& console, ProControllerContext& context,
    bool assume_console_type_is_ready,
    KeyboardLayout keyboard_layout,
    const std::string& code, bool include_plus
);



}
}
}
#endif

/*  System Keyboard Typing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Types text on the Switch's on-screen keyboard, reading the cursor from the video
 *  before each key, so it starts wherever the cursor is and corrects ignored moves.
 *  Supports the keyboards of NintendoSwitch_SystemKeyboardDetector.h.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_SystemKeyboardTyping_H
#define PokemonAutomation_NintendoSwitch_SystemKeyboardTyping_H

#include <string>
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/Programs/FastCodeEntry/NintendoSwitch_KeyboardEntryMappings.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace FastCodeEntry{


//  Types letters, digits and first-page symbols; uppercase via Shift (left stick
//  click), assuming the keyboard opens with Shift off.
//  Throws UserSetupError before the first press if a character has no key, and
//  OperationFailedException if the cursor can't be read or a press doesn't add to the
//  field (a wrong case is not caught). Holds are the Switch 1 code entry hold.
void type_on_system_keyboard(
    VideoStream& stream, ProControllerContext& context,
    const std::string& text,
    KeyboardLayout layout = KeyboardLayout::QWERTY
);


}  // namespace FastCodeEntry
}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif

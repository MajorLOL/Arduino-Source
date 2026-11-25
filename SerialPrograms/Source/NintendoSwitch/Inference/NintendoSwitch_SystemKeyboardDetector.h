/*  System Keyboard Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Switch's own on-screen keyboard (game name entry): read the key
 *  under its cursor and count white text pixels in its text field.
 *
 *  Supported: the Latin keyboard (QWERTY; AZERTY untested), whose layout follows
 *  the system language. Korean is not supported; Chinese and Switch 2 are untested.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_SystemKeyboardDetector_H
#define PokemonAutomation_NintendoSwitch_SystemKeyboardDetector_H

#include <cstddef>
#include <stdint.h>
#include <optional>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  Rows 0-3: character rows (0: digits); row 4: the bottom row. Columns 0-10:
//  characters; column 11: backspace (0, 11), Return (1, 11), OK (3, 11).
//  A row-4 key reports the nearest character column (Space: 8).
struct SystemKeyboardKey{
    uint8_t row;
    uint8_t col;

    bool operator==(const SystemKeyboardKey&) const = default;
};

//  The key under the cursor, or nothing if no cursor border is visible.
//  The keyboard remembers the cursor between uses, so read it, don't assume it.
std::optional<SystemKeyboardKey> read_system_keyboard_cursor(const ImageViewRGB32& screen);

//  White pixels in the text field (a "1" adds about 110 at 1080p): compare two
//  counts to tell whether a key press reached the field.
size_t system_keyboard_text_pixels(const ImageViewRGB32& screen);



//  Register the tests of this file.
void add_tests_SystemKeyboardDetector(UnitTestDatabase& database);



}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif

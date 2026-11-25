/*  PokemonLZA - New game intro screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Detectors and OCR readers for HOME tile selection and new-game setup screens.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_IntroScreens_H
#define PokemonAutomation_PokemonLZA_IntroScreens_H

#include <cstddef>
#include <optional>
#include <string>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Returns true if the highlighted Switch HOME tile's title OCR ends with "Z-A".
bool is_home_tile_plza(const ImageViewRGB32& frame);

//  Returns the highlighted language-list entry index (column-major), or std::nullopt.
std::optional<size_t> read_language_list_cursor(const ImageViewRGB32& frame);

//  Returns the highlighted look-picker entry (1..8), or std::nullopt if none is highlighted.
std::optional<size_t> read_look_picker_cursor(const ImageViewRGB32& frame);

//  Returns true when the 2 x 4 look-picker screen is displayed.
bool is_look_picker(const ImageViewRGB32& frame);

//  Returns true when the Switch system name-entry keyboard is displayed.
bool is_name_keyboard(const ImageViewRGB32& frame);

//  Reads the player name shown on Confirm Choices (spaces stripped), or empty string if unread.
std::string read_confirm_choices_name(const ImageViewRGB32& frame);

//  Reads the language label shown on Confirm Choices using `language` OCR.
std::string read_confirm_choices_language(const ImageViewRGB32& frame, Language language);

//  Row index of "Ready!" on Confirm Choices (0 = Change hair color, 1 = Change look, 2 = Ready!).
constexpr size_t CONFIRM_CHOICES_READY_ROW = 2;

//  Returns the highlighted row on Confirm Choices, or std::nullopt on any other frame.
std::optional<size_t> read_confirm_choices_cursor(const ImageViewRGB32& frame);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

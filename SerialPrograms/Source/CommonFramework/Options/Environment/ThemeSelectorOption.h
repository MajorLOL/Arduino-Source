/*  Theme Selector Option
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_ThemeSelectorOption_H
#define PokemonAutomation_ThemeSelectorOption_H

#include "Common/Cpp/ColoredText.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "PocketPilot/PocketPilot_Theme.h"

namespace PokemonAutomation{


//  The look of the program: PocketPilot's Night or Day, or one of the two
//  classic looks of Pokemon Automation.
class ThemeSelectorOption : public EnumDropdownOption<UiThemeMode>{
public:
    ThemeSelectorOption();

    virtual bool set_value(size_t value) override;
    virtual void load_json(const JsonValue& json) override;
};

//  The color of the PocketPilot themes: that of a starter.
class StarterColorOption : public EnumDropdownOption<PocketPilot::Starter>{
public:
    StarterColorOption();

    virtual bool set_value(size_t value) override;
    virtual void load_json(const JsonValue& json) override;
};

//  Applies what the two options above are set to. The options apply a
//  change themselves; this is for the start of the program, where settings
//  that were never saved have nothing to load. Only on the main thread,
//  once the QApplication exists.
void apply_current_theme();





}
#endif

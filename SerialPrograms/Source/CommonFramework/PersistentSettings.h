/*  Persistent Settings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */


#ifndef PokemonAutomation_PersistentSettings_H
#define PokemonAutomation_PersistentSettings_H

#include <stddef.h>
#include <functional>
#include "Common/Cpp/Json/JsonObject.h"

namespace PokemonAutomation{

// Global setting of the whole program.
// The setting is stored in the local folder, named after the program: ClaudeGG-Settings.json (SerialPrograms-Settings.json before build 98).
// The settings json has three fields:
// - "20-GlobalSettings": settings for CommonFramework/GlobalSettingsPanel.h: GlobalSetting,
//   global settings like Discord settings, hardware instruction set, thread priority and debugging.
// - "50-SwitchKeyboardMapping": keyboard mapping.
// - "99-Panels": settings for all the programs listed in the program panels.
//   Access via PersistentSettings::panels.
class PersistentSettings{
public:
    PersistentSettings();

    // Write settings to the json file.
    void write() const;
    // Load settings from the json file.
    void read();

public:
    JsonObject panels;

    //  PocketPilot: how the file was written so far (write()): replaced in
    //  one step, or the old way after that did not go through. For the
    //  test of the build.
    mutable size_t writes_in_one_step = 0;
    mutable size_t writes_the_old_way = 0;
};

// Return the singleton PersistentSettings.
PersistentSettings& PERSISTENT_SETTINGS();

//  PocketPilot: the options of the panel that is open, written to the
//  settings file now and not only when the window closes. From any thread:
//  the main window does it on the interface's thread, a moment later.
//  Nothing happens while there is no main window. (The AutoStory asks for
//  it at its start and at each save of the game. From 16 to 18 November
//  2026 the settings file stayed as a run of 16 November had left it: no
//  run after that got its options into it, however the program was closed,
//  and once a second window that was opened and closed beside the running
//  one wrote the old ones back.)
void request_panel_settings_save();
//  The main window: what does it. An empty function when the window goes.
void set_panel_settings_saver(std::function<void()> saver);



}


#endif

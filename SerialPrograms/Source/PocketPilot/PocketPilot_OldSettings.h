/*  Claude GG Old Settings
 *
 *  Part of Claude GG (PocketPilot before), which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  Until build 97 the program was SerialPrograms.exe and kept its settings in
 *  UserSettings/SerialPrograms-Settings.json: the settings file is named after
 *  the program. From build 98 the program is ClaudeGG.exe and its settings are
 *  ClaudeGG-Settings.json. A computer that has run an older build has its
 *  settings in the old file, so the first start of the new name takes them
 *  over: the old file is copied to the new name, when the new one is not there
 *  yet. Nothing is overwritten and nothing is deleted (a build with the old
 *  name still finds its file).
 *
 */

#ifndef PokemonAutomation_PocketPilot_OldSettings_H
#define PokemonAutomation_PocketPilot_OldSettings_H

#include <string>

namespace PokemonAutomation{
namespace PocketPilot{


//  The settings file of the builds from before the program was renamed.
extern const char OLD_SETTINGS_FILE_NAME[];

//  "folder" ends with "/" (as SETTINGS_PATH() does). True when the old file
//  was copied to "new_file_name": that is, when it is there and the new one
//  is not. False when there was nothing to take over, or when the name has
//  not changed, or when the copy failed (the program then starts with new
//  settings, as on a computer that never had any).
bool take_over_old_settings(const std::string& folder, const std::string& new_file_name);



}
}
#endif

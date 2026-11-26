/*  Claude GG Old Settings
 *
 *  Part of Claude GG (PocketPilot before), which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <exception>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "PocketPilot_OldSettings.h"

namespace PokemonAutomation{
namespace PocketPilot{


const char OLD_SETTINGS_FILE_NAME[] = "SerialPrograms-Settings.json";


bool take_over_old_settings(const std::string& folder, const std::string& new_file_name){
    if (new_file_name.empty() || new_file_name == OLD_SETTINGS_FILE_NAME){
        return false;
    }
    try{
        //  The paths are UTF-8, as every path of the program is.
        const Filesystem::Path old_path(folder + OLD_SETTINGS_FILE_NAME);
        const Filesystem::Path new_path(folder + new_file_name);
        if (Filesystem::exists(new_path) || !Filesystem::is_regular_file(old_path)){
            return false;
        }
        //  copy_file() does not overwrite: it stops with an error if the new
        //  file has appeared in the meantime.
        return Filesystem::copy_file(old_path, new_path);
    }catch (const std::exception&){
        return false;
    }
}



}
}

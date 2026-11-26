/*  Claude GG Installation Paths
 *
 *  Part of Claude GG (PocketPilot before), which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  Which installation a process is and where its data is: the part of
 *  PocketPilot_Instances.h that the GUI-free core needs too (the paths of the
 *  program, GlobalAutoPaths.cpp, are made from it). Nothing here needs Qt.
 *
 */

#ifndef PokemonAutomation_PocketPilot_InstancePaths_H
#define PokemonAutomation_PocketPilot_InstancePaths_H

#include <string>

namespace PokemonAutomation{
namespace PocketPilot{


//  The id of the first installation: its data is in the folder of the program.
extern const char* const MAIN_INSTANCE;             //  "1"

//  This process's installation ("1" unless the command line said otherwise).
const std::string& current_instance();
bool is_main_instance();

//  Sets this process's installation (what read_instance_options() does). False:
//  the folders were asked for already, they are made from the id and it can no
//  longer change; nothing is changed then.
bool set_current_instance(const std::string& id);
//  Whether the folders were asked for.
bool instance_paths_fixed();


//  ---------------------------------------------------------------------
//  Where the data is (all with a "/" at the end)
//  ---------------------------------------------------------------------

//  A path with '/' as its separator and one at the end (when it is not empty).
std::string path_with_slash(std::string path);

//  The folder that the program was started in (what the data of installation 1
//  is in): the working directory, as the program has always used it.
std::string shared_base_path();

//  "<scratch>Installations/", and the folder in it of one installation: where
//  its status and miniature are. "scratch" ends with "/".
std::string instances_root_path(const std::string& scratch);
std::string instance_meta_path(const std::string& scratch, const std::string& id);

//  Where the data of an installation is: the folder itself for number 1,
//  its folder in Installations for the others.
std::string instance_data_path(const std::string& scratch, const std::string& id);

//  What RUNTIME_BASE_PATH() is for this process; the id can no longer change
//  after this was asked.
std::string runtime_base_path(const std::string& scratch);


}
}
#endif

/*  Claude GG Installation Paths
 *
 *  Part of Claude GG (PocketPilot before), which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Cpp/Filesystem/Filesystem.h"
#include "PocketPilot_InstancePaths.h"

namespace PokemonAutomation{
namespace PocketPilot{


const char* const MAIN_INSTANCE = "1";


namespace{

std::string g_instance = MAIN_INSTANCE;
bool g_base_fixed = false;

}


const std::string& current_instance(){
    return g_instance;
}
bool is_main_instance(){
    return g_instance == MAIN_INSTANCE;
}

bool set_current_instance(const std::string& id){
    //  The folders are made from the id: it cannot change once they were asked for.
    if (g_base_fixed){
        return false;
    }
    g_instance = id;
    return true;
}
bool instance_paths_fixed(){
    return g_base_fixed;
}


std::string path_with_slash(std::string path){
    for (char& c : path){
        if (c == '\\'){
            c = '/';
        }
    }
    if (!path.empty() && path.back() != '/'){
        path += '/';
    }
    return path;
}

std::string shared_base_path(){
    return path_with_slash(Filesystem::application_scratch_path().string());
}

std::string instances_root_path(const std::string& scratch){
    return path_with_slash(scratch) + "Installations/";
}
std::string instance_meta_path(const std::string& scratch, const std::string& id){
    return instances_root_path(scratch) + id + "/";
}
std::string instance_data_path(const std::string& scratch, const std::string& id){
    if (id == MAIN_INSTANCE){
        return path_with_slash(scratch);
    }
    return instance_meta_path(scratch, id);
}

std::string runtime_base_path(const std::string& scratch){
    g_base_fixed = true;
    if (is_main_instance()){
        //  As it always was.
        return scratch;
    }
    return instance_data_path(scratch, g_instance);
}


}
}

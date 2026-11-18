/*  Persistent Settings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <mutex>
#include <system_error>
#include <utility>
#include <QCoreApplication>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonValue.h"
//#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/StaticGlobals.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/Options/Environment/PerformanceOptions.h"
#include "PersistentSettings.h"

// #include <iostream>
// using std::cout;
// using std::endl;

namespace PokemonAutomation{



PersistentSettings& PERSISTENT_SETTINGS(){
    static PersistentSettings settings;
    return settings;
}


PersistentSettings::PersistentSettings(){}



void PersistentSettings::write() const{
    JsonObject root;

    root["20-GlobalSettings"] = GlobalSettings::instance().to_json();
//    root["50-SwitchKeyboardMapping"] = NintendoSwitch::read_keyboard_mapping();

    root["99-Panels"] = panels.clone();

    //  PocketPilot: written beside the settings file and put in its place in
    //  one step. The settings are written at each save of a game now, not
    //  only when the window closes: a second window that starts at that
    //  moment never reads half a file, and a write that does not go through
    //  leaves the old file whole. If the file cannot be put in place (it is
    //  held open by something else), it is written the old way. The file
    //  beside it has one name for as long as this program runs: what a
    //  failed write leaves there is overwritten by the next.
    const std::string& path = PROGRAM_SETTING_JSON_PATH();
    const std::string beside = path + ".new-" + std::to_string((long long)QCoreApplication::applicationPid());
    try{
        root.dump(beside);
        std::error_code ec;
        Filesystem::rename(Filesystem::Path(beside), Filesystem::Path(path), ec);
        if (!ec){
            writes_in_one_step++;
            return;
        }
    }catch (...){}
    try{
        Filesystem::remove(Filesystem::Path(beside));
    }catch (...){}
    try{
        root.dump(path);
        writes_the_old_way++;
    }catch (FileException&){}
}


namespace{
std::mutex& settings_saver_lock(){
    static std::mutex lock;
    return lock;
}
std::function<void()>& settings_saver(){
    static std::function<void()> saver;
    return saver;
}
}

void request_panel_settings_save(){
    //  Under the lock: the main window takes its function away before it
    //  goes. (What is called only posts to the interface's thread.)
    std::lock_guard<std::mutex> lg(settings_saver_lock());
    const std::function<void()>& saver = settings_saver();
    if (saver){
        saver();
    }
}
void set_panel_settings_saver(std::function<void()> saver){
    std::lock_guard<std::mutex> lg(settings_saver_lock());
    settings_saver() = std::move(saver);
}


void PersistentSettings::read(){
    JsonValue json = load_json_file(PROGRAM_SETTING_JSON_PATH());
    JsonObject* obj = json.to_object();
    if (obj == nullptr){
        throw FileException(nullptr, PA_CURRENT_FUNCTION, "Invalid settings file.", PROGRAM_SETTING_JSON_PATH());
    }

    //  Need to load this subset of settings first because they will affect how
    //  "GlobalSettings" is constructed.
    const JsonValue* settings = obj->get_value("20-GlobalSettings");
    if (settings){
        STATIC_GLOBALS.load_json(*settings);
        GlobalSettings::instance().load_json(*settings);
    }

//    GlobalSettings::instance().PROCESS_PRIORITY0.update_priority_to_option();
    PerformanceOptions::instance().UI_THREAD_PRIORITY.set_on_this_thread(global_logger_tagged());

#if 0
    {
        const JsonArray* array = obj->get_array("50-SwitchKeyboardMapping");
        if (array){
            NintendoSwitch::set_keyboard_mapping(*array);
        }
    }
#endif
    {
        JsonObject* value = obj->get_object("99-Panels");
        if (value){
            panels = std::move(*value);
        }
    }
}



}


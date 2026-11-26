/*  PocketPilot Shiny Clip
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <cstdio>
#include <exception>
#include <filesystem>
#include <vector>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "PocketPilot_ShinyClip.h"

namespace PokemonAutomation{
namespace PocketPilot{


std::string clip_file_name(const std::string& program, const std::string& time){
    std::string name = "Shiny";
    std::string part;
    for (const char c : program){
        const bool fine = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (fine){
            part += c;
        }else if (c == ' '){
            part += '-';
        }
    }
    if (!part.empty()){
        name += "-" + part;
    }
    std::string stamp;
    for (const char c : time){
        const bool fine = (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (fine){
            stamp += c;
        }
    }
    if (!stamp.empty()){
        name += "-" + stamp;
    }
    return name + ".mp4";
}

std::string clip_size_text(uint64_t bytes){
    char text[32];
    if (bytes >= 1024ull * 1024){
        std::snprintf(text, sizeof(text), "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    }else{
        std::snprintf(text, sizeof(text), "%llu KB", (unsigned long long)((bytes + 1023) / 1024));
    }
    return text;
}

bool clip_can_be_sent(uint64_t bytes){
    return bytes > 0 && bytes <= CLIP_SEND_LIMIT;
}


std::string save_shiny_clip(VideoStream& stream, const std::string& program){
    const std::string folder = SCREENSHOTS_PATH() + "ShinyClips/";
    const std::string name = clip_file_name(program, now_to_filestring());
    const std::string path = folder + name;
    try{
        Filesystem::create_directories(Filesystem::Path(folder));
        if (!stream.save_stream_history(path)){
            stream.log(
                "No clip of the shiny: the stream history is off (\"History (seconds)\" is 0) or has nothing in it yet.",
                COLOR_ORANGE
            );
            return std::string();
        }
    }catch (OperationCancelledException&){
        throw;
    }catch (const Exception& e){
        stream.log("No clip of the shiny: " + e.message(), COLOR_ORANGE);
        return std::string();
    }catch (const std::exception& e){
        stream.log(std::string("No clip of the shiny: ") + e.what(), COLOR_ORANGE);
        return std::string();
    }
    std::error_code error;
    const uint64_t size = (uint64_t)std::filesystem::file_size(std::filesystem::path(path), error);
    if (error || size == 0){
        stream.log("No clip of the shiny: the file " + name + " was not written.", COLOR_ORANGE);
        return std::string();
    }
    stream.log(
        "A clip of the shiny is saved: " + name + " (" + clip_size_text(size) + ") in Screenshots/ShinyClips.",
        COLOR_BLUE
    );
    return path;
}

void send_shiny_clip(
    ProgramEnvironment& env, VideoStream& stream,
    EventNotificationOption& settings,
    const std::string& program
){
    const std::string path = save_shiny_clip(stream, program);
    if (path.empty()){
        return;
    }
    std::error_code error;
    const uint64_t size = (uint64_t)std::filesystem::file_size(std::filesystem::path(path), error);
    if (error || !clip_can_be_sent(size)){
        stream.log(
            "The clip (" + clip_size_text(size) + ") is over what a message takes (" +
            clip_size_text(CLIP_SEND_LIMIT) + "): it stays in the folder. A shorter \"History (seconds)\" "
            "or a lower picture rate makes it smaller.",
            COLOR_ORANGE
        );
        return;
    }
    const std::string name = std::filesystem::path(path).filename().string();
    send_program_notification_with_file(
        env, settings,
        COLOR_YELLOW,
        "Shiny clip",
        {{"Clip:", name + " (" + clip_size_text(size) + ")"}},
        "",
        path
    );
}


}
}

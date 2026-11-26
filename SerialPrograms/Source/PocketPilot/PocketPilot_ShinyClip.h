/*  PocketPilot Shiny Clip
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  A clip of the video of the last seconds when a shiny is found.
 *
 *  Pokemon Automation keeps the last seconds of the video in the stream
 *  history ("History (seconds)" in the settings, 30 by default) and can write
 *  them to a file, but no program of the games used it for a shiny: the
 *  message has a screenshot, and the "take video" options press the Capture
 *  button of the console. Here:
 *
 *    - the clip is saved in Screenshots/ShinyClips, with the program and the
 *      time in its name, so it is kept even when no message is sent;
 *    - when the notification of the program has a place to go to, the clip
 *      follows in a message of its own ("Shiny clip") if it is small enough
 *      for Discord. A clip that is too big stays in the folder, and the log
 *      says so (a shorter history or a lower picture rate makes it smaller);
 *    - nothing is sent that is not the video of the console itself, and the
 *      message has only the name of the file, not the folder it is in.
 *
 *  With the history switched off ("History (seconds)" at 0) there is no
 *  clip; the log says that.
 *
 */

#ifndef PokemonAutomation_PocketPilot_ShinyClip_H
#define PokemonAutomation_PocketPilot_ShinyClip_H

#include <stdint.h>
#include <string>

namespace PokemonAutomation{

class ProgramEnvironment;
class EventNotificationOption;
class VideoStream;

namespace PocketPilot{


//  The largest file that is sent. Discord's limit for a file in a webhook is
//  10 MiB for a server without boosts (it was 8 before, and 25 in a short
//  time): under both of the older ones.
const uint64_t CLIP_SEND_LIMIT = 8 * 1024 * 1024;

//  The name of a clip: "Shiny-<program>-<time>.mp4". Only letters, digits,
//  "-" and "_" of the program's name stay (a name with a space or a slash in
//  it makes a file that cannot be sent or opened by every program).
std::string clip_file_name(const std::string& program, const std::string& time);

//  A size as it is read in the log: "812 KB", "6.4 MB".
std::string clip_size_text(uint64_t bytes);

//  A clip of this size can be sent: it has something in it and is not over
//  the limit.
bool clip_can_be_sent(uint64_t bytes);

//  Saves the last seconds of the stream as a clip. Returns the path of the
//  file, or an empty string when there is none (and says why in the log): the
//  history is off or empty, or the file could not be written. It takes a few
//  seconds. Does not stop the program for a problem with a file.
std::string save_shiny_clip(VideoStream& stream, const std::string& program);

//  Saves the clip, and sends it in a message of its own through "settings"
//  (the notification of the program for a shiny) when it can be sent.
void send_shiny_clip(
    ProgramEnvironment& env, VideoStream& stream,
    EventNotificationOption& settings,
    const std::string& program
);


}
}
#endif

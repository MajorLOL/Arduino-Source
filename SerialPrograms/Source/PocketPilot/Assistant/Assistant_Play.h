/*  PocketPilot Assistant: Play
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  For a program of a game in which Claude decides what to do. The program
 *  offers what it can do as tools (look at the screen, press buttons, a
 *  whole battle the way another program fights it); Claude calls them; the
 *  program runs each call on the console and says what came of it. The
 *  more a tool does by itself, the fewer tokens a session costs.
 *
 *  The conversation is of the panel's kind (Assistant_Agent.h), with the
 *  user's key and the panel's connection, and is held in the thread of the
 *  interface. The program runs in its own thread: it waits here for the
 *  next call, runs it, and hands back what came of it.
 *
 *  No Qt in this header: programs of the games include it.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Assistant_Play_H
#define PokemonAutomation_PocketPilot_Assistant_Play_H

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

namespace PokemonAutomation{

class CancellableScope;
class ImageViewRGB32;

namespace PocketPilot{
namespace Assistant{


//  Something the program can do for Claude.
struct PlayTool{
    std::string name;
    //  For Claude: what it does, when to use it, what comes back.
    std::string description;
    //  The JSON schema of its input (an object).
    std::string input_schema;
};

//  Claude asks for a tool.
struct PlayCall{
    std::string id;
    std::string name;
    //  JSON: an object, as the tool's schema says.
    std::string input;
};

//  What came of a call.
struct PlayResult{
    std::string text;
    bool is_error = false;
    //  A picture that goes with the text: the bytes of a PNG or of a JPEG,
    //  as play_picture() packs them. Empty: none.
    std::string image;
};

//  What comes next in the conversation.
struct PlayEvent{
    enum class Kind{
        CALL,       //  Claude calls a tool: run it, then finish() it.
        ANSWERED,   //  Claude ended its turn without a call.
        CUT_OFF,    //  Its answer was as long as an answer may be.
        REFUSED,    //  Claude declined to go on.
        FAILED,     //  The API was not reached or refused the request.
    };
    Kind kind = Kind::FAILED;
    PlayCall call;
    //  What Claude wrote since the last event.
    std::string said;
    //  For REFUSED and FAILED: why, for the user.
    std::string message;
};

//  What a session used so far, all its conversations together.
struct PlayUsage{
    uint64_t requests = 0;
    uint64_t tokens = 0;
    //  At Anthropic's public prices: an estimate. Not known for a model
    //  whose price this build does not have.
    double cost_usd = 0;
    bool cost_known = true;
    //  How large the conversation that goes on is: the tokens its last
    //  request sent.
    uint64_t conversation_tokens = 0;
};


//  How a picture for Claude is packed. PNG keeps every pixel: small for a
//  game that draws few pixels in few colors. JPEG is a tenth of that for
//  the picture of a 3D game, and a conversation sends its pictures again
//  with every request.
enum class PlayPictureFormat{
    PNG,
    JPEG,
};

//  A picture of the game as it goes to Claude: scaled to "width" x "height"
//  (each pixel the average of what it stands for), every pixel then
//  enlarged to a square of "enlarge" pixels a side, and packed. For a game
//  that draws few pixels, its own size and a whole number keep the picture
//  sharp and the file small. As a PNG if it cannot be packed as a JPEG.
//  Empty if there is no picture.
std::string play_picture(
    const ImageViewRGB32& screen, int width, int height, int enlarge = 1,
    PlayPictureFormat format = PlayPictureFormat::PNG
);


class PlaySession{
public:
    struct Setup{
        //  As the API names it: "claude-haiku-5-5".
        std::string model;
        //  "low", "medium", "high". Empty: the model's own.
        std::string effort = "low";
        //  Claude's instructions and tools: the same for the whole session.
        std::string system;
        std::vector<PlayTool> tools;
        //  The most one answer may be, in tokens.
        int max_tokens = 4000;
    };

    //  Why Claude cannot play right now (no key yet), for the user. Empty
    //  if it can. Throws as "scope" does once the program is stopped.
    static std::string why_unavailable(CancellableScope& scope);

    explicit PlaySession(Setup setup);
    ~PlaySession();
    PlaySession(const PlaySession&) = delete;
    void operator=(const PlaySession&) = delete;

    //  A new conversation that starts with this message. One that goes on
    //  is dropped, with a call that waits for its result.
    void start(const std::string& message);
    //  After ANSWERED or CUT_OFF: the conversation goes on with this.
    void say(const std::string& message);
    //  After FAILED: the same request once more.
    void retry();

    //  Waits for what comes next. Throws as "scope" does once the program
    //  is stopped.
    PlayEvent wait(CancellableScope& scope);
    //  What came of the call that wait() handed out.
    void finish(const PlayCall& call, PlayResult result);

    PlayUsage usage() const;

private:
    struct Shared;
    std::shared_ptr<Shared> m_shared;
};



}
}
}
#endif

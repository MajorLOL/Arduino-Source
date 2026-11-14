/*  PocketPilot Claude Plays: the session
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  What every program in which Claude plays has in common: the user's goal
 *  goes to Claude with a list of tools, Claude calls them one at a time, the
 *  program runs each call on the console and says what came of it.
 *
 *  A game's program brings the tools (Game); the program it runs in brings
 *  the log, the numbers under the video and the way to stop (Host). The rest
 *  is here: Claude's notes and the journal of the last actions, the
 *  conversation that is started over with them when it has grown (a
 *  conversation is sent again with every request), the limits on actions
 *  and cost, and what to do when Claude stops calling tools.
 *
 *  No Qt in this header: programs of the games include it.
 *
 */

#ifndef PokemonAutomation_PocketPilot_ClaudePlays_Engine_H
#define PokemonAutomation_PocketPilot_ClaudePlays_Engine_H

#include <stdint.h>
#include <string>
#include <vector>
#include "Common/Cpp/Color.h"
#include "PocketPilot/Assistant/Assistant_Play.h"

namespace PokemonAutomation{

class CancellableScope;

namespace PocketPilot{
namespace ClaudePlays{

using Assistant::PlayCall;
using Assistant::PlayEvent;
using Assistant::PlayResult;
using Assistant::PlaySession;
using Assistant::PlayTool;
using Assistant::PlayUsage;


//  Without the white space around it.
std::string trimmed(const std::string& text);
//  On one line, and not longer than "limit" (then with "..." after it).
std::string one_line(const std::string& text, size_t limit);
//  "$0.07"
std::string money(double usd);
//  "1234 tokens in 5 requests, about $0.02"
std::string usage_line(const PlayUsage& usage);


//  The models a session can run on.
enum class Model{
    HAIKU,
    SONNET,
    OPUS,
    FABLE,
};
//  As the API names it: "claude-haiku-5-5".
const char* model_id(Model model);


//  The program a session runs in.
class Host{
public:
    virtual ~Host() = default;
    //  A line of the program's log.
    virtual void log(const std::string& text, Color color = Color()) = 0;
    //  A line on the video, for whoever watches.
    virtual void show(const std::string& text, Color color) = 0;
    //  The numbers under the video: called whenever one may have changed.
    virtual void count(uint64_t actions, const PlayUsage& usage) = 0;
    //  Cancelled when the program is stopped.
    virtual CancellableScope& scope() = 0;
};


//  What a game's program lets Claude do.
class Game{
public:
    virtual ~Game() = default;
    //  The tools, without "note" and "done" (session_tools()).
    virtual std::vector<PlayTool> tools() const = 0;
    //  Claude's instructions.
    virtual std::string instructions() const = 0;
    //  What the program finds on the screen by itself, in a sentence: what
    //  a conversation begins with.
    virtual std::string screen() = 0;
    //  Runs a call on the console, and says what came of it. (Not "note"
    //  and "done".) A call that cannot be run as asked is an error result,
    //  not an exception: Claude is told, and decides what to do.
    virtual PlayResult run(const PlayCall& call) = 0;
};

//  "note" and "done": the session's own tools, the last of every list.
std::vector<PlayTool> session_tools();

//  What Claude gets: the game's instructions and tools, and the session's.
PlaySession::Setup make_setup(const Game& game, Model model);


struct Limits{
    //  An action is one call that the game runs.
    uint64_t max_actions = 100;
    //  In US dollars, by the prices this build knows. 0: none.
    double max_cost = 0.50;
    //  A conversation whose last request sent this many tokens is started
    //  over with the notes and the last actions. (Every request sends the
    //  whole conversation again. From the cache that costs little, but past
    //  100,000 tokens the smallest model costs five times as much.)
    uint64_t restart_at_tokens = 40000;
    //  The same for the bytes of its pictures: they are sent again with
    //  every request too, and a request has a largest size.
    uint64_t restart_at_picture_bytes = 3000000;
};

//  How a session ended.
struct Ending{
    //  Claude said that the goal is reached.
    bool reached = false;
    //  The API could not be reached, or refused: "summary" says why. A
    //  program reports that as its error.
    bool failed = false;
    //  For the user, with what the session used.
    std::string summary;
    uint64_t actions = 0;
    PlayUsage usage;
};

//  Plays toward "goal" until Claude calls done, a limit is reached, Claude
//  no longer calls tools, or the API fails. Throws as the host's scope does
//  once the program is stopped.
Ending play(
    Host& host, Game& game, PlaySession& claude,
    const std::string& goal, const Limits& limits
);



}
}
}
#endif

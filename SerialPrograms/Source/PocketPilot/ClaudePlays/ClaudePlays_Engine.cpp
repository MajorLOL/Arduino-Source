/*  PocketPilot Claude Plays: the session
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <cctype>
#include <deque>
#include <utility>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include "Common/Cpp/CancellableScope.h"
#include "ClaudePlays_Engine.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace ClaudePlays{



std::string trimmed(const std::string& text){
    size_t first = 0;
    size_t last = text.size();
    while (first < last && std::isspace((unsigned char)text[first])){
        first++;
    }
    while (last > first && std::isspace((unsigned char)text[last - 1])){
        last--;
    }
    return text.substr(first, last - first);
}

std::string one_line(const std::string& text, size_t limit){
    std::string line;
    bool space = false;
    for (char ch : trimmed(text)){
        if (std::isspace((unsigned char)ch)){
            space = true;
            continue;
        }
        if (space && !line.empty()){
            line += ' ';
        }
        space = false;
        line += ch;
    }
    if (line.size() > limit){
        //  Not in the middle of a character of more than one byte.
        size_t cut = limit;
        while (cut > 0 && ((unsigned char)line[cut] & 0xc0) == 0x80){
            cut--;
        }
        line.resize(cut);
        line += "...";
    }
    return line;
}

std::string money(double usd){
    const long long cents = (long long)(usd * 100.0 + 0.5);
    const std::string rest = std::to_string(cents % 100);
    return "$" + std::to_string(cents / 100) + "." + (rest.size() < 2 ? "0" + rest : rest);
}

std::string usage_line(const PlayUsage& usage){
    std::string line = std::to_string(usage.tokens) + " tokens in " + std::to_string(usage.requests) +
        (usage.requests == 1 ? " request" : " requests");
    if (usage.cost_known && usage.requests > 0){
        line += usage.cost_usd < 0.005 ? ", less than $0.01" : ", about " + money(usage.cost_usd);
    }
    return line;
}

const char* model_id(Model model){
    switch (model){
    case Model::HAIKU:      return "claude-haiku-5-5";
    case Model::SONNET:     return "claude-sonnet-5-5";
    case Model::OPUS:       return "claude-opus-5-5";
    case Model::FABLE:      return "claude-fable-5-2";
    }
    return "claude-haiku-5-5";
}



std::vector<PlayTool> session_tools(){
    std::vector<PlayTool> tools;
    tools.push_back(PlayTool{
        "note",
        "Writes a line down for yourself. The program starts the conversation over now and then to save "
        "tokens, and only your notes and the last few actions come along: note where the player is, what is "
        "done and what comes next whenever that changes.",
        R"({"type":"object","properties":{"text":{"type":"string"}},"required":["text"]})"
    });
    tools.push_back(PlayTool{
        "done",
        "Ends the session: the goal is reached, or it cannot be reached. Say which, and what was done.",
        std::string(R"({"type":"object","properties":{"reached":{"type":"boolean"},)") +
        R"("summary":{"type":"string"}},"required":["reached","summary"]})"
    });
    return tools;
}

PlaySession::Setup make_setup(const Game& game, Model model){
    PlaySession::Setup setup;
    setup.model = model_id(model);
    setup.effort = "low";
    setup.system = game.instructions();
    setup.tools = game.tools();
    for (PlayTool& tool : session_tools()){
        setup.tools.push_back(std::move(tool));
    }
    setup.max_tokens = 4000;
    return setup;
}



namespace{

//  The last actions that go along into a conversation that starts over.
const size_t JOURNAL_LINES = 6;
//  How much of Claude's notes is kept (the newest).
const size_t NOTES_CHARACTERS = 2500;
//  Claude ending its turn without a tool call this often in a row ends the
//  session.
const int MAX_NUDGES = 3;

//  What a session keeps while it runs: Claude's notes (one a line) and the
//  last actions with what came of them. A conversation that starts over
//  begins with them.
struct Kept{
    std::string notes;
    std::deque<std::string> journal;
    uint64_t actions = 0;
};

//  A few words on a call, for the log and the journal.
std::string call_words(const PlayCall& call){
    return call.name + " " + one_line(call.input, 120);
}

QJsonObject input_of(const PlayCall& call){
    return QJsonDocument::fromJson(QByteArray(call.input.data(), (qsizetype)call.input.size())).object();
}

//  What a conversation begins with.
std::string first_message(const Kept& kept, const std::string& goal, const std::string& screen){
    std::string text = "Goal: " + goal + "\n\n";
    if (kept.actions > 0){
        text += "The program started this conversation over to save tokens, after " +
            std::to_string(kept.actions) + " actions.\n";
        text += kept.notes.empty()
            ? "You wrote no notes down.\n"
            : "Your notes:\n" + kept.notes;
        if (!kept.journal.empty()){
            text += "The last actions before that, with what came of them:\n";
            for (const std::string& line : kept.journal){
                text += "- " + line + "\n";
            }
        }
        text += "\n";
    }
    text += "Screen now: " + screen;
    return text;
}

}   //  namespace



Ending play(
    Host& host, Game& game, PlaySession& claude,
    const std::string& goal, const Limits& limits
){
    Kept kept;
    Ending ending;

    bool warned_no_price = false;
    auto counted = [&](){
        const PlayUsage usage = claude.usage();
        host.count(kept.actions, usage);
        if (!usage.cost_known && limits.max_cost > 0 && !warned_no_price){
            warned_no_price = true;
            host.log(
                "Claude Plays: this build does not know the price of the model, so the cost limit cannot "
                "stop the session. The limit on actions still does.",
                COLOR_ORANGE
            );
        }
        return usage;
    };

    //  The bytes of the pictures in the conversation that goes on.
    uint64_t picture_bytes = 0;
    claude.start(first_message(kept, goal, game.screen()));

    std::string summary;
    int nudges = 0;
    //  Everything Claude did, the notes and the turns without a tool call
    //  included: they cost requests like an action does.
    uint64_t turns = 0;
    while (summary.empty()){
        const PlayEvent event = claude.wait(host.scope());
        const PlayUsage usage = counted();
        if (!event.said.empty()){
            host.log("Claude: " + one_line(event.said, 600), COLOR_PURPLE);
            host.show("Claude: " + event.said, COLOR_WHITE);
        }
        turns++;
        if (limits.max_cost > 0 && usage.cost_known && usage.cost_usd >= limits.max_cost){
            summary = "Stopped at a cost of about " + money(usage.cost_usd) + ", the most this session may cost.";
            break;
        }
        if (turns > 2 * limits.max_actions + 20){
            summary = "Stopped: Claude took far more turns than the actions this session may take.";
            break;
        }

        if (event.kind == PlayEvent::Kind::FAILED){
            ending.failed = true;
            summary = "Claude could not be reached, after " + std::to_string(kept.actions) + " actions (" +
                usage_line(usage) + "): " + event.message;
            break;
        }
        if (event.kind == PlayEvent::Kind::REFUSED){
            summary = "Claude declined to go on. " + event.message;
            break;
        }
        if (event.kind == PlayEvent::Kind::ANSWERED || event.kind == PlayEvent::Kind::CUT_OFF){
            //  A turn without a tool call: it has to call done to end.
            if (++nudges >= MAX_NUDGES){
                summary = "Claude stopped calling tools without saying that it is done.";
                break;
            }
            claude.say(
                "The program: go on with the goal through the tools. If it is reached, or cannot be reached, "
                "call done."
            );
            continue;
        }

        //  A call.
        nudges = 0;
        const PlayCall& call = event.call;
        if (call.name == "done"){
            const QJsonObject input = input_of(call);
            ending.reached = input.value("reached").toBool(false);
            summary = std::string(ending.reached ? "Claude says the goal is reached. " : "Claude says the goal is not reached. ") +
                one_line(input.value("summary").toString().toStdString(), 600);
            break;
        }
        if (call.name == "note"){
            const std::string note = one_line(input_of(call).value("text").toString().toStdString(), 400);
            PlayResult result;
            if (note.empty()){
                result.is_error = true;
                result.text = "\"text\" was empty: nothing was written down.";
            }else{
                kept.notes += "- " + note + "\n";
                //  The newest notes are the ones to keep.
                while (kept.notes.size() > NOTES_CHARACTERS){
                    const size_t line_end = kept.notes.find('\n');
                    kept.notes.erase(0, line_end == std::string::npos ? kept.notes.size() : line_end + 1);
                }
                host.log("Claude's note: " + note, COLOR_PURPLE);
                result.text = "Written down.";
            }
            claude.finish(call, std::move(result));
            continue;
        }

        host.log("Claude Plays: " + call_words(call));
        host.show(call_words(call), COLOR_CYAN);
        const PlayResult result = game.run(call);
        kept.actions++;
        counted();
        host.log(
            std::string("Claude Plays:   ") + (result.is_error ? "(not done) " : "") + one_line(result.text, 500) +
            (result.image.empty() ? "" : " [with a picture]"),
            result.is_error ? COLOR_ORANGE : Color()
        );

        //  The conversation has grown: it starts over, and what came of this
        //  call goes along in the journal instead of back into the long
        //  conversation. A picture cannot go along that way, so with one the
        //  conversation goes on a little longer.
        picture_bytes += result.image.size();
        const bool grown =
            usage.conversation_tokens >= limits.restart_at_tokens ||
            picture_bytes >= limits.restart_at_picture_bytes;
        const bool far_grown =
            usage.conversation_tokens >= limits.restart_at_tokens / 4 * 5 ||
            picture_bytes >= limits.restart_at_picture_bytes / 4 * 5;
        const bool starts_over = grown && (result.image.empty() || far_grown);

        kept.journal.push_back(
            call_words(call) + " -> " + one_line(result.text, 240) +
            (starts_over && !result.image.empty() ? " (Its picture did not come along: look again.)" : "")
        );
        while (kept.journal.size() > JOURNAL_LINES){
            kept.journal.pop_front();
        }

        if (kept.actions >= limits.max_actions){
            summary = "Stopped after " + std::to_string(kept.actions) + " actions, the most this session may take.";
            break;
        }
        if (kept.actions % 10 == 0){
            host.log(
                "Claude Plays: " + std::to_string(kept.actions) + " actions so far, " + usage_line(usage) + ".",
                COLOR_BLUE
            );
        }

        if (starts_over){
            host.log(
                "Claude Plays: the conversation has grown to " + std::to_string(usage.conversation_tokens) +
                " tokens and " + std::to_string(picture_bytes / 1000) + " kB of pictures. Starting it over "
                "with Claude's notes and the last actions.",
                COLOR_BLUE
            );
            picture_bytes = 0;
            claude.start(first_message(kept, goal, game.screen()));
            continue;
        }
        claude.finish(call, result);
    }

    ending.usage = counted();
    ending.actions = kept.actions;
    if (!ending.failed){
        summary += " " + std::to_string(kept.actions) + (kept.actions == 1 ? " action, " : " actions, ") +
            usage_line(ending.usage) + ".";
    }
    ending.summary = summary;
    return ending;
}



}
}
}

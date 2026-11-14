/*  PocketPilot Assistant: Agent
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  A conversation with Claude: the messages so far, and the turn that goes
 *  on. A turn starts with what the user wrote. Claude answers, or asks for
 *  tools; the tools run here and their results go back, until Claude's
 *  answer is complete.
 *
 *  What the API requires of a conversation is kept here:
 *
 *  -   The instructions and the tools stay the same for a whole
 *      conversation, and no earlier message is ever changed. (The API's
 *      cache needs that to save anything, and the thinking blocks of
 *      Claude's answers are refused once something before them changed.)
 *  -   Every tool call gets its result, also when the user stops the turn
 *      halfway.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Assistant_Agent_H
#define PokemonAutomation_PocketPilot_Assistant_Agent_H

#include <functional>
#include <memory>
#include <vector>
#include <QJsonArray>
#include <QObject>
#include <QString>
#include "Assistant_Claude.h"
#include "Assistant_Tools.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


//  How a turn ended.
struct TurnEnd{
    enum class Kind{
        ANSWERED,       //  Claude's answer is complete.
        STOPPED,        //  The user stopped it.
        FAILED,         //  The API could not be reached, or refused.
        ROUND_LIMIT,    //  Many tool calls in a row: the user decides whether to go on.
        SPEND_LIMIT,    //  The conversation cost as much as the user's limit.
        CUT_OFF,        //  The answer was as long as an answer may be.
        REFUSED,        //  Claude declined.
    };
    Kind kind = Kind::ANSWERED;
    //  For the user. Empty for ANSWERED and STOPPED.
    QString message;
    //  go_on() makes sense.
    bool can_go_on() const{
        return kind == Kind::FAILED || kind == Kind::ROUND_LIMIT || kind == Kind::SPEND_LIMIT || kind == Kind::CUT_OFF;
    }
};


class Agent{
public:
    struct Settings{
        QString model;
        QString system;
        QString effort;
        int max_tokens = 24000;
        //  Requests in one turn before the user is asked whether to go on.
        int max_rounds = 40;
        //  Estimated US dollars for the conversation before the user is
        //  asked. 0: no limit.
        double spend_limit_usd = 0;
    };

    struct Events{
        //  A new answer of Claude begins (each request of a turn has one).
        std::function<void()> answer_begin;
        //  More of its text.
        std::function<void(const QString& text)> text;
        //  It starts over: forget the text of this answer so far.
        std::function<void()> text_restart;
        //  A tool call: starts, says what it is doing, ends.
        std::function<void(const QString& id, const QString& what)> tool_begin;
        std::function<void(const QString& id, const QString& progress)> tool_progress;
        std::function<void(const QString& id, bool ok, const QString& result)> tool_end;
        //  A tool asks the user. See ToolCallContext::approve.
        std::function<void(const ApprovalRequest& request, std::function<void(bool approved)> answer)> approve;
        //  What is being waited for. Empty: nothing special.
        std::function<void(const QString& what)> status;
        std::function<void(const Usage& total)> usage;
        std::function<void(const TurnEnd& end)> finished;
    };

    Agent(ClaudeClient& client, const ToolSet& tools);
    ~Agent();
    Agent(const Agent&) = delete;
    void operator=(const Agent&) = delete;

    Settings& settings(){ return m_settings; }
    void set_events(Events events){ m_events = std::move(events); }

    //  A turn goes on.
    bool busy() const{ return m_busy; }

    //  Starts a turn. "context" goes to Claude in front of the text but is
    //  not something the user wrote (what is open in the program right
    //  now). Not while busy.
    void ask(const QString& text, const QString& context = QString());
    //  Goes on with a turn that ended with TurnEnd::can_go_on().
    void go_on();
    //  Stops the turn that goes on. finished() comes with STOPPED.
    void stop();
    //  A new conversation. Stops a turn without telling.
    void clear();

    //  Another model from the next request on. The thinking of the answers
    //  so far is taken out of the conversation: another model cannot take
    //  it back.
    void set_model(const QString& model);

    bool empty() const{ return m_messages.isEmpty(); }
    const Usage& usage() const{ return m_usage; }
    //  How many tokens the last request sent: the size of the conversation.
    qint64 conversation_tokens() const{ return m_conversation_tokens; }
    //  As they go to the API. (For the tests.)
    const QJsonArray& messages() const{ return m_messages; }

private:
    struct PendingCall{
        QString id;
        QString name;
        QJsonObject input;
        bool broken = false;
    };

    void begin_turn();
    void request();
    void on_reply(const ClaudeReply& reply);
    void run_next_tool();
    void finish_tool(const PendingCall& call, ToolResult result);
    void close_tool_calls();
    void end_turn(TurnEnd::Kind kind, const QString& text = QString());
    void later(std::function<void()> step);
    QString round_limit_text() const;

private:
    ClaudeClient& m_client;
    const ToolSet& m_tools;
    Settings m_settings;
    Events m_events;

    QJsonArray m_messages;
    //  Of the conversation that goes on: fixed at its first message.
    QString m_system;
    QJsonArray m_tool_definitions;

    bool m_busy = false;
    int m_rounds = 0;
    //  The cost up to which the user said to go on, once past the point to
    //  ask at.
    double m_spend_allowed = 0;
    std::shared_ptr<HttpCall> m_call;

    //  The tool calls of the answer being worked on, and their results.
    std::vector<PendingCall> m_calls;
    size_t m_next_call = 0;
    QJsonArray m_results;
    //  The answer with these calls was cut off at its maximum length.
    bool m_cut_off = false;
    //  Told to the tools of the turn that goes on.
    std::shared_ptr<bool> m_stopped;
    //  False once this is gone: for results of tools that come after it.
    std::shared_ptr<bool> m_alive;
    //  Counts turns and conversations: what comes back from an older one
    //  is dropped.
    quint64 m_generation = 0;

    Usage m_usage;
    qint64 m_conversation_tokens = 0;

    //  The context of the steps taken later: they are dropped with this.
    QObject m_context;
};



}
}
}
#endif

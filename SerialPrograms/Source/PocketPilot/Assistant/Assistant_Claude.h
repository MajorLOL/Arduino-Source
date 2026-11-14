/*  PocketPilot Assistant: Claude API
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  One request to the Messages API of Claude, with the answer read as it
 *  streams in. Written against the API as documented in November 2026
 *  (https://platform.claude.com/docs/en/api/overview, version 2026-10-21):
 *
 *  -   The answer's blocks are kept exactly as the API sent them. The API
 *      checks the thinking blocks of an answer when they are sent back with
 *      the results of tools, and refuses changed ones.
 *  -   No sampling parameters and no forced tool choice: the current models
 *      refuse them.
 *  -   The request asks for automatic prompt caching: what a conversation
 *      repeats (the instructions, the tools, the earlier messages) is then
 *      billed at a fraction of the price.
 *
 *  What a request cost is counted from the answer's usage numbers.
 *
 *  The key goes in the header "x-api-key" (the API's first way, and still
 *  taken next to "Authorization: Bearer"). A key that was not made for one
 *  workspace has to name the workspace with each request, in the header
 *  "anthropic-workspace-id": the user enters its ID in the settings then.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Assistant_Claude_H
#define PokemonAutomation_PocketPilot_Assistant_Claude_H

#include <functional>
#include <memory>
#include <vector>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include "Assistant_Http.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


//  Tokens used, and what they cost at the public prices (an estimate: the
//  bill is Anthropic's).
struct Usage{
    qint64 input = 0;           //  Read at the full price.
    qint64 cache_write = 0;     //  Read and stored for the next request.
    qint64 cache_read = 0;      //  Taken from the cache.
    qint64 output = 0;          //  Written, thinking included.
    qint64 requests = 0;
    double cost_usd = 0;
    //  False once a request was counted whose model has no known price.
    bool cost_known = true;

    void add(const Usage& other);
    qint64 tokens() const{ return input + cache_write + cache_read + output; }
    //  What the last request sent: how large the conversation is by now.
    qint64 prompt() const{ return input + cache_write + cache_read; }
};

//  The price of one request in US dollars, at the public prices of
//  November 2026. False for a model whose price is not known here.
bool estimate_cost(
    const QString& model,
    qint64 input, qint64 cache_write, qint64 cache_read, qint64 output,
    double& usd
);

//  "12.4k tokens, about $0.03" (or without the price).
QString usage_text(const Usage& usage);


struct ModelChoice{
    QString id;         //  As the API wants it: "claude-sonnet-5-5".
    QString name;       //  "Claude Sonnet 5.5"
    QString line;       //  "opus", "sonnet", "haiku", "fable"... or empty.
};

//  The models offered before the API was asked for its list.
const std::vector<ModelChoice>& known_models();

//  One line on what a model is good for here. Empty if not known.
QString model_hint(const QString& model);


struct ClaudeRequest{
    QString model;
    //  The most the answer may be, thinking included.
    int max_tokens = 16000;
    QString system;
    QJsonArray tools;       //  As the API wants them.
    QJsonArray messages;    //  As the API wants them.
    //  "low", "medium", "high"... Empty: the model's own default.
    QString effort;
};

struct ClaudeReply{
    bool ok = false;

    //  With an answer:
    QString stop_reason;
    //  The blocks of the answer, as they have to go back to the API.
    QJsonArray content;
    //  Tool calls whose input did not arrive as valid JSON (their ids).
    //  Their "input" is empty in "content".
    QStringList broken_tool_calls;
    QString model;

    //  Without one:
    int status = 0;             //  The HTTP status, 0 without an answer.
    QString error_type;         //  The API's, or "network".
    QString error_message;      //  For the user.

    //  Of all tries together.
    Usage usage;
};


//  What to tell the user about an error of the API. "workspace_named": the
//  request named a workspace (see ClaudeClient). (Also for the tests.)
QString api_error_text(
    const QString& type, int status, const QString& message, const QString& error_code,
    bool workspace_named
);


//  The answer of the API as it streams in (server-sent events), put back
//  together into its blocks.
class MessageStream{
public:
    std::function<void(const QString& text)> on_text;
    std::function<void(const QString& name)> on_tool;

    void feed(const QByteArray& piece);
    //  No more pieces come.
    void finish();

    //  Anything of an answer came in.
    bool started() const{ return m_started; }
    //  The answer came in to its end.
    bool complete() const{ return m_complete; }
    //  The API reported an error in the stream.
    bool failed() const{ return m_failed; }
    const QString& error_type() const{ return m_error_type; }
    const QString& error_message() const{ return m_error_message; }

    const QString& stop_reason() const{ return m_stop_reason; }
    const QString& model() const{ return m_model; }
    QJsonArray content() const;
    QStringList broken_tool_calls() const{ return m_broken_tool_calls; }
    //  Tokens only: no price.
    Usage usage() const{ return m_usage; }

private:
    struct Block{
        bool present = false;
        bool closed = false;
        QJsonObject object;
        QString text;           //  Of a text or a thinking block.
        QString signature;
        QByteArray input_json;
        QJsonArray citations;
        bool has_citations = false;
    };

    void handle_line(const QByteArray& line);
    void handle_event(const QByteArray& name, const QByteArray& data);
    void read_usage(const QJsonObject& usage);
    void close_block(Block& block);
    Block& block_at(int index);

private:
    QByteArray m_buffer;
    QByteArray m_event_name;
    QByteArray m_event_data;
    bool m_event_has_data = false;

    std::vector<Block> m_blocks;
    bool m_started = false;
    bool m_complete = false;
    bool m_failed = false;
    QString m_error_type;
    QString m_error_message;
    QString m_stop_reason;
    QString m_model;
    QStringList m_broken_tool_calls;
    Usage m_usage;
};


struct ClaudeEvents{
    //  More text of the answer.
    std::function<void(const QString& text)> text;
    //  The answer starts over (a new try): forget its text so far.
    std::function<void()> restart;
    //  Claude starts to call a tool.
    std::function<void(const QString& name)> tool;
    //  What the client waits for ("The API is busy. Trying again in 6 s").
    //  Empty: nothing special any more.
    std::function<void(const QString& what)> status;
};


class ClaudeClient{
public:
    //  Asked for the key at each request, so that a key the user just
    //  entered is used at once.
    using KeySource = std::function<QString()>;

    //  "workspace": the ID of the workspace of the API to run in
    //  ("wrkspc_..."), asked for at each request like the key. For a key
    //  that was not made for one workspace. No source, or an empty ID: no
    //  workspace is named.
    ClaudeClient(HttpTransport& transport, KeySource key, KeySource workspace = KeySource());
    ~ClaudeClient();

    //  One request, with new tries where they can help (the API is busy,
    //  the connection broke). "done" is called once, from the event loop,
    //  unless the returned call is aborted first.
    std::shared_ptr<HttpCall> send(
        const ClaudeRequest& request,
        ClaudeEvents events,
        std::function<void(const ClaudeReply& reply)> done
    );

    //  The models the key can use, the newest of each line first. Costs no
    //  tokens: also the test of a key. An empty error: it worked.
    std::shared_ptr<HttpCall> list_models(
        std::function<void(const std::vector<ModelChoice>& models, const QString& error)> done
    );

    //  The request's body, as it is sent. (Also for the tests.)
    QByteArray request_body(const ClaudeRequest& request) const;
    //  The headers of a request, as they are sent now. (Also for the tests.)
    std::vector<HttpHeader> request_headers() const;

    //  Times between tries: 0 for the tests.
    void set_retry_scale(double scale){ m_retry_scale = scale; }

private:
    class Operation;
    friend class Operation;

    //  As their sources give them now.
    QString current_key() const;
    QString current_workspace() const;

    HttpTransport& m_transport;
    KeySource m_key;
    KeySource m_workspace;
    double m_retry_scale = 1.0;
    //  What this API turned out not to take (see send()): left out from
    //  then on.
    bool m_no_auto_cache = false;
    bool m_no_effort = false;
    bool m_old_version = false;
    //  Set when the client is gone: operations still on their way stop.
    std::shared_ptr<bool> m_alive;
};



}
}
}
#endif

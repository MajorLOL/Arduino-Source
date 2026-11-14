/*  PocketPilot Assistant: Claude API
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <algorithm>
#include <iterator>
#include <utility>
#include <QJsonDocument>
#include <QJsonValue>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include "Assistant_Claude.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


namespace{

const char* const API_MESSAGES = "https://api.anthropic.com/v1/messages";
const char* const API_MODELS = "https://api.anthropic.com/v1/models?limit=1000";
//  The version this was written against, and the one before it (which the
//  API still takes) for an API that does not know the newer one.
const char* const API_VERSION = "2026-10-21";
const char* const API_VERSION_BEFORE = "2023-06-01";

//  How often a request is sent at most, and how long to wait before the
//  second, third and fourth time.
const int MAX_TRIES = 4;
const int RETRY_WAIT_MS[] = {2000, 6000, 15000};
//  A wait the API asks for that is longer than this is not waited out: the
//  user is told instead.
const int LONGEST_WAIT_MS = 90000;


qint64 token_count(const QJsonValue& value){
    return value.isDouble() ? (qint64)value.toDouble() : 0;
}

QString tokens_text(qint64 tokens){
    if (tokens < 1000){
        return QString::number(tokens) + (tokens == 1 ? " token" : " tokens");
    }
    if (tokens < 1000000){
        return QString::number((double)tokens / 1000.0, 'f', tokens < 100000 ? 1 : 0) + "k tokens";
    }
    return QString::number((double)tokens / 1000000.0, 'f', 2) + "M tokens";
}

}



void Usage::add(const Usage& other){
    input += other.input;
    cache_write += other.cache_write;
    cache_read += other.cache_read;
    output += other.output;
    requests += other.requests;
    cost_usd += other.cost_usd;
    cost_known = cost_known && other.cost_known;
}


bool estimate_cost(
    const QString& model,
    qint64 input, qint64 cache_write, qint64 cache_read, qint64 output,
    double& usd
){
    //  US dollars for a million tokens: input, output, and what a read from
    //  the cache costs compared to input. Writing to the cache (kept for 5
    //  minutes) is 1.25 times input for all of them.
    //  (https://platform.claude.com/docs/en/about-claude/pricing, November 2026)
    struct Price{
        const char* model;
        double input;
        double output;
        double cache_read_factor;
    };
    static const Price PRICES[] = {
        {"claude-haiku-5-5",    0.10,   0.50,   0.10},
        {"claude-sonnet-5-5",   2.00,   10.00,  0.05},
        {"claude-opus-5-5",     4.00,   20.00,  0.05},
        {"claude-fable-5-2",    8.00,   40.00,  0.025},
    };
    for (const Price& price : PRICES){
        if (!model.startsWith(QLatin1String(price.model))){
            continue;
        }
        double factor = 1.0;
        //  Haiku 5.5 costs five times as much for a request that sends more
        //  than 100,000 tokens.
        if (model.startsWith(QLatin1String("claude-haiku-5-5")) && input + cache_write + cache_read > 100000){
            factor = 5.0;
        }
        usd = factor * (
            (double)input * price.input +
            (double)cache_write * price.input * 1.25 +
            (double)cache_read * price.input * price.cache_read_factor +
            (double)output * price.output
        ) / 1000000.0;
        return true;
    }
    usd = 0;
    return false;
}

QString usage_text(const Usage& usage){
    QString text = tokens_text(usage.tokens());
    if (usage.requests == 0 || !usage.cost_known){
        return text;
    }
    if (usage.cost_usd < 0.005){
        return text + ", less than $0.01";
    }
    return text + ", about $" + QString::number(usage.cost_usd, 'f', 2);
}


const std::vector<ModelChoice>& known_models(){
    static const std::vector<ModelChoice> MODELS{
        {"claude-sonnet-5-5",   "Claude Sonnet 5.5",    "sonnet"},
        {"claude-opus-5-5",     "Claude Opus 5.5",      "opus"},
        {"claude-haiku-5-5",    "Claude Haiku 5.5",     "haiku"},
        {"claude-fable-5-2",    "Claude Fable 5.2",     "fable"},
    };
    return MODELS;
}

QString model_hint(const QString& model){
    if (model.startsWith("claude-haiku")){
        return "The fastest and by far the cheapest: questions, and playing.";
    }
    if (model.startsWith("claude-sonnet")){
        return "Balanced: questions, and most changes to programs.";
    }
    if (model.startsWith("claude-opus")){
        return "The strongest for building and fixing programs. Twice the price of Sonnet.";
    }
    if (model.startsWith("claude-fable") || model.startsWith("claude-mythos")){
        return "For the hardest, longest work. Twice the price of Opus.";
    }
    return QString();
}



//  The stream.

MessageStream::Block& MessageStream::block_at(int index){
    if (index < 0){
        index = 0;
    }
    //  (An index far out of range cannot come from the API: its blocks are
    //  numbered from 0 without gaps.)
    if (index > 4096){
        index = 4096;
    }
    if ((size_t)index >= m_blocks.size()){
        m_blocks.resize((size_t)index + 1);
    }
    return m_blocks[(size_t)index];
}

void MessageStream::feed(const QByteArray& piece){
    m_buffer += piece;
    qsizetype position = 0;
    while (true){
        const qsizetype end = m_buffer.indexOf('\n', position);
        if (end < 0){
            break;
        }
        QByteArray line = m_buffer.mid(position, end - position);
        position = end + 1;
        if (line.endsWith('\r')){
            line.chop(1);
        }
        handle_line(line);
    }
    m_buffer.remove(0, position);
}

void MessageStream::finish(){
    if (!m_buffer.isEmpty()){
        QByteArray line = m_buffer;
        m_buffer.clear();
        if (line.endsWith('\r')){
            line.chop(1);
        }
        handle_line(line);
    }
    //  An event without the empty line after it.
    handle_line(QByteArray());
    //  Blocks of an answer that broke off: what came in of them.
    for (Block& block : m_blocks){
        if (block.present && !block.closed){
            close_block(block);
        }
    }
}

void MessageStream::handle_line(const QByteArray& line){
    if (line.isEmpty()){
        //  The end of an event.
        if (m_event_has_data || !m_event_name.isEmpty()){
            const QByteArray name = m_event_name;
            const QByteArray data = m_event_data;
            m_event_name.clear();
            m_event_data.clear();
            m_event_has_data = false;
            handle_event(name, data);
        }
        return;
    }
    if (line.startsWith(':')){
        //  A comment.
        return;
    }
    const qsizetype colon = line.indexOf(':');
    const QByteArray field = colon < 0 ? line : line.left(colon);
    QByteArray value = colon < 0 ? QByteArray() : line.mid(colon + 1);
    if (value.startsWith(' ')){
        value.remove(0, 1);
    }
    if (field == "event"){
        m_event_name = value;
    }else if (field == "data"){
        if (m_event_has_data){
            m_event_data += '\n';
        }
        m_event_data += value;
        m_event_has_data = true;
    }
}

void MessageStream::read_usage(const QJsonObject& usage){
    //  The numbers of a later event replace those of an earlier one: they
    //  are totals, not additions.
    if (usage.value("input_tokens").isDouble()){
        m_usage.input = token_count(usage.value("input_tokens"));
    }
    if (usage.value("cache_creation_input_tokens").isDouble()){
        m_usage.cache_write = token_count(usage.value("cache_creation_input_tokens"));
    }
    if (usage.value("cache_read_input_tokens").isDouble()){
        m_usage.cache_read = token_count(usage.value("cache_read_input_tokens"));
    }
    if (usage.value("output_tokens").isDouble()){
        m_usage.output = token_count(usage.value("output_tokens"));
    }
}

void MessageStream::close_block(Block& block){
    block.closed = true;
    const QString type = block.object.value("type").toString();
    if (type == "text"){
        block.object["text"] = block.text;
        if (block.has_citations){
            block.object["citations"] = block.citations;
        }
    }else if (type == "thinking"){
        block.object["thinking"] = block.text;
        block.object["signature"] = block.signature;
    }else if (type == "tool_use" || type == "server_tool_use"){
        QJsonObject input;
        bool valid = true;
        if (!block.input_json.trimmed().isEmpty()){
            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(block.input_json, &error);
            if (error.error == QJsonParseError::NoError && document.isObject()){
                input = document.object();
            }else{
                valid = false;
            }
        }else if (block.object.value("input").isObject()){
            //  Given whole with the start of the block.
            input = block.object.value("input").toObject();
        }
        block.object["input"] = input;
        if (!valid){
            m_broken_tool_calls.append(block.object.value("id").toString());
        }
    }
}

void MessageStream::handle_event(const QByteArray& name, const QByteArray& data){
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()){
        //  Not an event of this API (or a new kind without JSON): passed over.
        return;
    }
    const QJsonObject event = document.object();
    QString type = event.value("type").toString();
    if (type.isEmpty()){
        type = QString::fromUtf8(name);
    }

    if (type == "message_start"){
        m_started = true;
        const QJsonObject message = event.value("message").toObject();
        m_model = message.value("model").toString();
        read_usage(message.value("usage").toObject());
        return;
    }
    if (type == "content_block_start"){
        m_started = true;
        Block& block = block_at(event.value("index").toInt());
        block = Block();
        block.present = true;
        block.object = event.value("content_block").toObject();
        const QString block_type = block.object.value("type").toString();
        if (block_type == "text"){
            block.text = block.object.value("text").toString();
        }else if (block_type == "thinking"){
            block.text = block.object.value("thinking").toString();
            block.signature = block.object.value("signature").toString();
        }else if (block_type == "tool_use" && on_tool){
            on_tool(block.object.value("name").toString());
        }
        if (block_type == "text" && !block.text.isEmpty() && on_text){
            on_text(block.text);
        }
        return;
    }
    if (type == "content_block_delta"){
        Block& block = block_at(event.value("index").toInt());
        if (!block.present || block.closed){
            return;
        }
        const QJsonObject delta = event.value("delta").toObject();
        const QString delta_type = delta.value("type").toString();
        if (delta_type == "text_delta"){
            const QString text = delta.value("text").toString();
            block.text += text;
            if (on_text && !text.isEmpty()){
                on_text(text);
            }
        }else if (delta_type == "input_json_delta"){
            block.input_json += delta.value("partial_json").toString().toUtf8();
        }else if (delta_type == "thinking_delta"){
            block.text += delta.value("thinking").toString();
        }else if (delta_type == "signature_delta"){
            block.signature += delta.value("signature").toString();
        }else if (delta_type == "citations_delta"){
            block.citations.append(delta.value("citation"));
            block.has_citations = true;
        }
        //  Other kinds of delta: a newer API. The block stays as it is.
        return;
    }
    if (type == "content_block_stop"){
        Block& block = block_at(event.value("index").toInt());
        if (block.present && !block.closed){
            close_block(block);
        }
        return;
    }
    if (type == "message_delta"){
        const QJsonObject delta = event.value("delta").toObject();
        if (delta.value("stop_reason").isString()){
            m_stop_reason = delta.value("stop_reason").toString();
        }
        read_usage(event.value("usage").toObject());
        return;
    }
    if (type == "message_stop"){
        m_complete = true;
        return;
    }
    if (type == "error"){
        m_failed = true;
        const QJsonObject error = event.value("error").toObject();
        m_error_type = error.value("type").toString();
        m_error_message = error.value("message").toString();
        return;
    }
    //  "ping", and kinds of event a newer API adds: nothing to do.
}

QJsonArray MessageStream::content() const{
    QJsonArray content;
    for (const Block& block : m_blocks){
        if (block.present){
            content.append(block.object);
        }
    }
    return content;
}



//  The client.

QString api_error_text(
    const QString& type, int status, const QString& message, const QString& error_code,
    bool workspace_named
){
    if (type == "network"){
        return "No answer from the Claude API (" + message + "). Check the internet connection, then try again.";
    }
    //  The workspace the request runs in: a key that was not made for one
    //  has to name it. (The API's words, November 2026: "This API key is
    //  not scoped to a workspace, so this request must include the
    //  anthropic-workspace-id header with the ID of the workspace to use.",
    //  and "anthropic-workspace-id header must be a valid workspace ID.")
    if (message.contains("anthropic-workspace-id") || message.contains("scoped to a workspace")){
        if (!workspace_named){
            return
                "This API key was not made for one workspace, so the Claude API has to be told which workspace to "
                "use. Either make a new key in the Claude Console and choose a workspace for it, or enter the "
                "workspace's ID (wrkspc_..., on the Console's page Workspaces) in the settings of this panel. (" +
                message + ")";
        }
        return
            "The Claude API did not take the workspace ID of this panel's settings. Check it against the "
            "Console's page Workspaces (it starts with wrkspc_). (" + message + ")";
    }
    //  Sent along with each request: what to look at when the API does not
    //  find, or does not allow, what was asked for.
    const QString workspace_hint = workspace_named
        ? QString(
            " The workspace ID of this panel's settings is sent with each request: check it too, or clear it "
            "for a key that was made for one workspace."
        )
        : QString();
    if (type == "authentication_error" || status == 401){
        return "The Claude API did not accept the API key. Check the key in the settings of this panel. (" + message + ")";
    }
    if (type == "billing_error"){
        return "The Claude API reports a problem with the billing of this account: " + message;
    }
    if (type == "permission_error"){
        return "This API key may not use what was asked for: " + message + workspace_hint;
    }
    if (type == "rate_limit_error"){
        if (error_code == "enforced_spend_limit_reached"){
            return "The spending limit of this API account is reached: " + message;
        }
        return "The Claude API's limit for this account is reached for the moment. Wait a little, then try again. (" + message + ")";
    }
    if (type == "overloaded_error" || status == 529){
        return "The Claude API is overloaded right now. Try again in a moment.";
    }
    if (type == "not_found_error"){
        return
            "The Claude API does not know what was asked for (" + message + "). Pick another model in this panel." +
            workspace_hint;
    }
    if (type == "request_too_large" || status == 413){
        return "The conversation has become too large for the Claude API. Start a new one.";
    }
    if (type == "invalid_request_error"){
        return "The Claude API refused the request: " + message;
    }
    if (message.isEmpty()){
        return "The Claude API answered with an error (HTTP " + QString::number(status) + ").";
    }
    return "The Claude API reported an error: " + message;
}

namespace{

std::vector<HttpHeader> api_headers(const QString& key, const QString& workspace, bool old_version){
    std::vector<HttpHeader> headers{
        {"x-api-key", key.toUtf8()},
        {"anthropic-version", old_version ? API_VERSION_BEFORE : API_VERSION},
        {"content-type", "application/json"},
    };
    if (!workspace.isEmpty()){
        headers.push_back(HttpHeader{"anthropic-workspace-id", workspace.toUtf8()});
    }
    return headers;
}

//  The error of an answer that is not a stream: its JSON body.
void read_error_body(const QByteArray& body, QString& type, QString& message, QString& error_code){
    const QJsonDocument document = QJsonDocument::fromJson(body);
    const QJsonObject error = document.object().value("error").toObject();
    type = error.value("type").toString();
    message = error.value("message").toString();
    error_code = error.value("details").toObject().value("error_code").toString();
    if (type.isEmpty() && message.isEmpty()){
        //  Not from the API itself (something in between): what it says, cut short.
        message = QString::fromUtf8(body.left(300)).simplified();
    }
}

}


class ClaudeClient::Operation
    : public HttpCall
    , public std::enable_shared_from_this<ClaudeClient::Operation>
{
public:
    Operation(
        ClaudeClient& client,
        const ClaudeRequest& request,
        ClaudeEvents events,
        std::function<void(const ClaudeReply& reply)> done
    )
        : m_client(client)
        , m_alive(client.m_alive)
        , m_request(request)
        , m_events(std::move(events))
        , m_done(std::move(done))
    {}

    void start(){
        //  Stays until it is done or aborted, whoever keeps the handle.
        m_self = shared_from_this();
        //  Not from inside send().
        QTimer::singleShot(0, &m_context, [this]{
            attempt();
        });
    }

    virtual void abort() override{
        if (m_finished){
            return;
        }
        m_finished = true;
        if (m_call){
            m_call->abort();
            m_call.reset();
        }
        release();
    }

private:
    //  Lets go of itself, once nothing of it runs any more. (Not from one
    //  of its own timers' handlers: the timers go with it.)
    void release(){
        std::shared_ptr<Operation> self = std::move(m_self);
        m_self.reset();
        if (self){
            QTimer::singleShot(0, [self]{});
        }
    }

    void finish(ClaudeReply reply){
        if (m_finished){
            return;
        }
        m_finished = true;
        m_call.reset();
        if (m_events.status){
            m_events.status(QString());
        }
        reply.usage = m_usage;
        std::function<void(const ClaudeReply& reply)> done = std::move(m_done);
        release();
        if (done){
            done(reply);
        }
    }

    void fail(int status, const QString& type, const QString& message, const QString& error_code){
        ClaudeReply reply;
        reply.ok = false;
        reply.status = status;
        reply.error_type = type;
        reply.error_message = api_error_text(type, status, message, error_code, m_workspace_named);
        finish(std::move(reply));
    }

    void attempt(){
        if (m_finished || !*m_alive){
            return;
        }
        const QString key = m_client.current_key();
        if (key.isEmpty()){
            ClaudeReply reply;
            reply.error_type = "no_key";
            reply.error_message = "There is no API key yet. Add one in the settings of this panel.";
            finish(std::move(reply));
            return;
        }

        m_try++;
        m_stream = std::make_unique<MessageStream>();
        m_stream->on_text = [this](const QString& text){
            if (m_events.text){
                m_events.text(text);
            }
        };
        m_stream->on_tool = [this](const QString& name){
            if (m_events.tool){
                m_events.tool(name);
            }
        };

        HttpRequest request;
        request.method = "POST";
        request.url = QUrl(QString::fromLatin1(API_MESSAGES));
        const QString workspace = m_client.current_workspace();
        m_workspace_named = !workspace.isEmpty();
        request.headers = api_headers(key, workspace, m_client.m_old_version);
        request.body = m_client.request_body(m_request);
        //  The API sends something every few seconds while it works.
        request.idle_timeout_ms = 180000;

        std::shared_ptr<Operation> self = shared_from_this();
        m_call = m_client.m_transport.send(
            request,
            [self](const QByteArray& piece){
                if (self->m_finished || !*self->m_alive){
                    return;
                }
                self->m_stream->feed(piece);
            },
            [self](const HttpResponse& response){
                if (self->m_finished){
                    return;
                }
                if (!*self->m_alive){
                    self->m_finished = true;
                    self->release();
                    return;
                }
                self->on_response(response);
            }
        );
    }

    void on_response(const HttpResponse& response){
        m_call.reset();
        m_stream->finish();

        //  What this try used, also when it failed halfway.
        Usage used = m_stream->usage();
        if (m_stream->started()){
            used.requests = 1;
            const QString model = m_stream->model().isEmpty() ? m_request.model : m_stream->model();
            used.cost_known = estimate_cost(
                model, used.input, used.cache_write, used.cache_read, used.output, used.cost_usd
            );
            m_usage.add(used);
        }

        if (response.ok() && m_stream->complete() && !m_stream->failed()){
            ClaudeReply reply;
            reply.ok = true;
            reply.status = response.status;
            reply.stop_reason = m_stream->stop_reason();
            reply.content = m_stream->content();
            reply.broken_tool_calls = m_stream->broken_tool_calls();
            reply.model = m_stream->model();
            finish(std::move(reply));
            return;
        }

        //  What went wrong.
        int status = response.status;
        QString type;
        QString message;
        QString error_code;
        if (m_stream->failed()){
            type = m_stream->error_type();
            message = m_stream->error_message();
        }else if (status != 0 && (status < 200 || status >= 300)){
            read_error_body(response.body, type, message, error_code);
        }else if (!response.error.isEmpty()){
            type = "network";
            message = response.error;
        }else{
            type = "network";
            message = "the answer ended before it was complete";
        }

        //  Something this API does not take: once more without it, and
        //  without it from now on. (Not counted as a try. Not for what it
        //  says about the workspace: that is for the user to set right.)
        if (status == 400 && !message.contains("anthropic-workspace-id")){
            bool again = false;
            if (!m_client.m_no_auto_cache && message.contains("cache_control")){
                m_client.m_no_auto_cache = true;
                again = true;
            }else if (
                !m_client.m_no_effort && !m_request.effort.isEmpty() &&
                (message.contains("output_config") || message.contains("effort"))
            ){
                m_client.m_no_effort = true;
                again = true;
            }else if (!m_client.m_old_version && message.contains("anthropic-version")){
                m_client.m_old_version = true;
                again = true;
            }
            if (again){
                m_try--;
                attempt();
                return;
            }
        }

        bool retry = false;
        int wait_ms = RETRY_WAIT_MS[std::min(m_try, (int)std::size(RETRY_WAIT_MS)) - 1];
        QString why;
        if (
            type == "network" || type == "overloaded_error" || type == "api_error" || type == "timeout_error" ||
            status == 500 || status == 502 || status == 503 || status == 504 || status == 529
        ){
            retry = true;
            why = type == "network" ? "The connection to the Claude API broke off." : "The Claude API is busy.";
        }else if (type == "rate_limit_error" || status == 429){
            if (error_code != "enforced_spend_limit_reached"){
                retry = true;
                why = "The Claude API asks to slow down.";
                bool has_wait = false;
                const double seconds = response.header("retry-after").toDouble(&has_wait);
                if (has_wait && seconds >= 0){
                    wait_ms = (int)(std::min(seconds, 3600.0) * 1000.0) + 500;
                }
                if (wait_ms > LONGEST_WAIT_MS){
                    retry = false;
                }
            }
        }

        if (!retry || m_try >= MAX_TRIES){
            fail(status, type, message, error_code);
            return;
        }

        if (m_stream->started() && m_events.restart){
            m_events.restart();
        }
        wait_ms = (int)((double)wait_ms * m_client.m_retry_scale);
        if (m_events.status){
            m_events.status(
                why + " Trying again in " + QString::number((wait_ms + 999) / 1000) + " s (try " +
                QString::number(m_try + 1) + " of " + QString::number(MAX_TRIES) + ")."
            );
        }
        QTimer::singleShot(wait_ms, &m_context, [this]{
            if (m_events.status){
                m_events.status(QString());
            }
            attempt();
        });
    }

private:
    ClaudeClient& m_client;
    std::shared_ptr<bool> m_alive;
    ClaudeRequest m_request;
    ClaudeEvents m_events;
    std::function<void(const ClaudeReply& reply)> m_done;

    //  The context of the timers: they stop when this is gone.
    QObject m_context;
    std::shared_ptr<HttpCall> m_call;
    std::unique_ptr<MessageStream> m_stream;
    std::shared_ptr<Operation> m_self;
    Usage m_usage;
    int m_try = 0;
    bool m_finished = false;
    //  The last try named a workspace.
    bool m_workspace_named = false;
};



ClaudeClient::ClaudeClient(HttpTransport& transport, KeySource key, KeySource workspace)
    : m_transport(transport)
    , m_key(std::move(key))
    , m_workspace(std::move(workspace))
    , m_alive(std::make_shared<bool>(true))
{}

ClaudeClient::~ClaudeClient(){
    *m_alive = false;
}

QString ClaudeClient::current_key() const{
    return m_key ? m_key().trimmed() : QString();
}
QString ClaudeClient::current_workspace() const{
    return m_workspace ? m_workspace().trimmed() : QString();
}

std::vector<HttpHeader> ClaudeClient::request_headers() const{
    return api_headers(current_key(), current_workspace(), m_old_version);
}

QByteArray ClaudeClient::request_body(const ClaudeRequest& request) const{
    QJsonObject body;
    body["model"] = request.model;
    body["max_tokens"] = request.max_tokens;
    body["stream"] = true;
    if (!m_no_auto_cache){
        //  The API marks the end of what can be cached by itself, and moves
        //  the mark along as the conversation grows.
        body["cache_control"] = QJsonObject{{"type", "ephemeral"}};
    }
    if (!request.system.isEmpty()){
        body["system"] = request.system;
    }
    if (!request.tools.isEmpty()){
        body["tools"] = request.tools;
    }
    body["messages"] = request.messages;
    if (!request.effort.isEmpty() && !m_no_effort){
        body["output_config"] = QJsonObject{{"effort", request.effort}};
    }
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

std::shared_ptr<HttpCall> ClaudeClient::send(
    const ClaudeRequest& request,
    ClaudeEvents events,
    std::function<void(const ClaudeReply& reply)> done
){
    std::shared_ptr<Operation> operation = std::make_shared<Operation>(
        *this, request, std::move(events), std::move(done)
    );
    operation->start();
    return operation;
}

std::shared_ptr<HttpCall> ClaudeClient::list_models(
    std::function<void(const std::vector<ModelChoice>& models, const QString& error)> done
){
    HttpRequest request;
    request.url = QUrl(QString::fromLatin1(API_MODELS));
    request.headers = request_headers();
    request.idle_timeout_ms = 30000;
    const bool workspace_named = !current_workspace().isEmpty();
    std::shared_ptr<bool> alive = m_alive;
    return m_transport.send(
        request, nullptr,
        [alive, done, workspace_named](const HttpResponse& response){
            if (!*alive || !done){
                return;
            }
            if (!response.ok()){
                QString type;
                QString message;
                QString error_code;
                if (response.status == 0){
                    type = "network";
                    message = response.error;
                }else{
                    read_error_body(response.body, type, message, error_code);
                }
                done(
                    std::vector<ModelChoice>(),
                    api_error_text(type, response.status, message, error_code, workspace_named)
                );
                return;
            }
            struct Entry{
                ModelChoice model;
                QString created;
                int rank;
            };
            std::vector<Entry> entries;
            const QJsonArray data = QJsonDocument::fromJson(response.body).object().value("data").toArray();
            for (const QJsonValue& value : data){
                const QJsonObject object = value.toObject();
                const QString lifecycle = object.value("lifecycle").toString();
                if (!lifecycle.isEmpty() && lifecycle != "active"){
                    continue;
                }
                Entry entry;
                entry.model.id = object.value("id").toString();
                entry.model.name = object.value("display_name").toString();
                entry.model.line = object.value("line").toString();
                entry.created = object.value("created_at").toString();
                if (entry.model.id.isEmpty()){
                    continue;
                }
                if (entry.model.name.isEmpty()){
                    entry.model.name = entry.model.id;
                }
                //  The order of the list: by what they are good for here.
                static const char* const LINES[] = {"sonnet", "opus", "haiku", "fable", "mythos"};
                entry.rank = (int)std::size(LINES);
                for (int index = 0; index < (int)std::size(LINES); index++){
                    if (entry.model.line == LINES[index]){
                        entry.rank = index;
                        break;
                    }
                }
                entries.push_back(std::move(entry));
            }
            std::stable_sort(
                entries.begin(), entries.end(),
                [](const Entry& a, const Entry& b){
                    if (a.rank != b.rank){
                        return a.rank < b.rank;
                    }
                    return a.created > b.created;
                }
            );
            std::vector<ModelChoice> models;
            for (Entry& entry : entries){
                models.push_back(std::move(entry.model));
            }
            done(models, QString());
        }
    );
}



}
}
}

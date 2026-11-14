/*  PocketPilot Assistant: Agent
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <algorithm>
#include <exception>
#include <utility>
#include <QJsonValue>
#include <QTimer>
#include "Assistant_Agent.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


namespace{

//  A tool's result longer than this is cut: it would be paid for again
//  with every later request of the conversation.
const qsizetype MAX_RESULT_CHARACTERS = 60000;

QJsonObject text_block(const QString& text){
    return QJsonObject{{"type", "text"}, {"text", text}};
}

QJsonObject message(const char* role, const QJsonArray& content){
    return QJsonObject{{"role", QString::fromLatin1(role)}, {"content", content}};
}

QJsonObject failed_result(const QString& id, const QString& text){
    return QJsonObject{
        {"type", "tool_result"},
        {"tool_use_id", id},
        {"content", text},
        {"is_error", true},
    };
}

}



Agent::Agent(ClaudeClient& client, const ToolSet& tools)
    : m_client(client)
    , m_tools(tools)
    , m_stopped(std::make_shared<bool>(false))
    , m_alive(std::make_shared<bool>(true))
{}

Agent::~Agent(){
    *m_alive = false;
    clear();
}

void Agent::later(std::function<void()> step){
    const quint64 generation = m_generation;
    QTimer::singleShot(0, &m_context, [this, generation, step]{
        if (generation == m_generation){
            step();
        }
    });
}

void Agent::begin_turn(){
    m_busy = true;
    m_rounds = 0;
    m_generation++;
    m_stopped = std::make_shared<bool>(false);
}

void Agent::ask(const QString& text, const QString& context){
    if (m_busy || text.trimmed().isEmpty()){
        return;
    }
    if (m_messages.isEmpty()){
        //  From here on they stay as they are, until the conversation is
        //  cleared.
        m_system = m_settings.system;
        m_tool_definitions = m_tools.definitions();
    }
    QJsonArray content;
    if (!context.trimmed().isEmpty()){
        content.append(text_block(context));
    }
    content.append(text_block(text));
    m_messages.append(message("user", content));
    begin_turn();
    request();
}

void Agent::go_on(){
    if (m_busy || m_messages.isEmpty()){
        return;
    }
    //  After an answer that was cut off.
    if (m_messages.last().toObject().value("role").toString() == "assistant"){
        m_messages.append(message("user", QJsonArray{text_block("Go on from where you stopped.")}));
    }
    //  Past the point to ask at: on to the next one.
    const double limit = m_settings.spend_limit_usd;
    if (limit > 0 && m_usage.cost_usd >= std::max(m_spend_allowed, limit)){
        m_spend_allowed = m_usage.cost_usd + limit;
    }
    begin_turn();
    request();
}

void Agent::stop(){
    if (!m_busy){
        return;
    }
    *m_stopped = true;
    if (m_call){
        m_call->abort();
        m_call.reset();
    }
    close_tool_calls();
    end_turn(TurnEnd::Kind::STOPPED);
}

void Agent::clear(){
    *m_stopped = true;
    if (m_call){
        m_call->abort();
        m_call.reset();
    }
    m_generation++;
    m_busy = false;
    m_messages = QJsonArray();
    m_calls.clear();
    m_next_call = 0;
    m_results = QJsonArray();
    m_usage = Usage();
    m_conversation_tokens = 0;
    m_rounds = 0;
    m_spend_allowed = 0;
    m_cut_off = false;
}

void Agent::set_model(const QString& model){
    if (model == m_settings.model){
        return;
    }
    m_settings.model = model;
    if (m_busy || m_messages.isEmpty()){
        return;
    }
    //  Without the thinking of the answers so far. (All of it: the API
    //  takes a conversation without any, not one with some left out.)
    QJsonArray messages;
    for (const QJsonValue& value : m_messages){
        QJsonObject entry = value.toObject();
        if (entry.value("role").toString() == "assistant"){
            QJsonArray content;
            for (const QJsonValue& block : entry.value("content").toArray()){
                const QString type = block.toObject().value("type").toString();
                if (type != "thinking" && type != "redacted_thinking"){
                    content.append(block);
                }
            }
            if (content.isEmpty()){
                content.append(text_block("(No text.)"));
            }
            entry["content"] = content;
        }
        messages.append(entry);
    }
    m_messages = messages;
}

void Agent::request(){
    //  The point at which the user wants to be asked.
    const double limit = m_settings.spend_limit_usd;
    if (limit > 0 && m_usage.cost_known && m_usage.cost_usd >= std::max(m_spend_allowed, limit)){
        end_turn(
            TurnEnd::Kind::SPEND_LIMIT,
            "This conversation has cost about $" + QString::number(m_usage.cost_usd, 'f', 2) +
            " so far. You asked to be asked at every $" + QString::number(limit, 'f', 2) + ". Go on?"
        );
        return;
    }

    m_rounds++;
    ClaudeRequest request;
    request.model = m_settings.model;
    request.max_tokens = m_settings.max_tokens;
    request.system = m_system;
    request.tools = m_tool_definitions;
    request.messages = m_messages;
    request.effort = m_settings.effort;

    if (m_events.answer_begin){
        m_events.answer_begin();
    }
    const quint64 generation = m_generation;
    ClaudeEvents events;
    events.text = [this, generation](const QString& text){
        if (generation == m_generation && m_events.text){
            m_events.text(text);
        }
    };
    events.restart = [this, generation]{
        if (generation == m_generation && m_events.text_restart){
            m_events.text_restart();
        }
    };
    events.tool = [this, generation](const QString& name){
        if (generation == m_generation && m_events.status){
            const Tool* tool = m_tools.find(name);
            m_events.status("Claude is preparing: " + (tool != nullptr ? tool->name : name));
        }
    };
    events.status = [this, generation](const QString& what){
        if (generation == m_generation && m_events.status){
            m_events.status(what);
        }
    };
    m_call = m_client.send(
        request, std::move(events),
        [this, generation](const ClaudeReply& reply){
            if (generation == m_generation){
                on_reply(reply);
            }
        }
    );
}

void Agent::on_reply(const ClaudeReply& reply){
    m_call.reset();
    m_usage.add(reply.usage);
    if (reply.usage.requests > 0){
        //  What the next request sends at least: this one and its answer.
        m_conversation_tokens = (reply.usage.prompt() + reply.usage.output) / reply.usage.requests;
    }
    if (m_events.usage){
        m_events.usage(m_usage);
    }
    if (m_events.status){
        m_events.status(QString());
    }
    if (!reply.ok){
        end_turn(TurnEnd::Kind::FAILED, reply.error_message);
        return;
    }
    if (reply.stop_reason == "refusal"){
        //  Not kept: the conversation would only be refused again.
        end_turn(
            TurnEnd::Kind::REFUSED,
            "Claude declined to go on with this. Ask in another way, or start a new conversation."
        );
        return;
    }

    //  The answer as it goes back to the API with the next request. (The
    //  API refuses an empty text block.)
    QJsonArray content;
    m_calls.clear();
    for (const QJsonValue& value : reply.content){
        const QJsonObject block = value.toObject();
        const QString type = block.value("type").toString();
        if (type == "text" && block.value("text").toString().trimmed().isEmpty()){
            continue;
        }
        content.append(block);
        if (type == "tool_use"){
            PendingCall call;
            call.id = block.value("id").toString();
            call.name = block.value("name").toString();
            call.input = block.value("input").toObject();
            call.broken = reply.broken_tool_calls.contains(call.id);
            m_calls.push_back(std::move(call));
        }
    }
    if (content.isEmpty()){
        end_turn(TurnEnd::Kind::FAILED, "Claude's answer was empty. Ask again.");
        return;
    }
    m_messages.append(message("assistant", content));

    if (!m_calls.empty()){
        //  Also after an answer that was cut off in the middle of a call:
        //  each call needs its result.
        m_cut_off = reply.stop_reason == "max_tokens";
        m_next_call = 0;
        m_results = QJsonArray();
        later([this]{
            run_next_tool();
        });
        return;
    }
    if (reply.stop_reason == "max_tokens"){
        end_turn(TurnEnd::Kind::CUT_OFF, "The answer reached its maximum length and was cut off there.");
        return;
    }
    if (reply.stop_reason == "model_context_window_exceeded"){
        end_turn(
            TurnEnd::Kind::FAILED,
            "The conversation has become too long for this model. Start a new conversation."
        );
        return;
    }
    if (reply.stop_reason == "pause_turn"){
        //  The API paused a long answer: asked again, it goes on.
        if (m_rounds >= m_settings.max_rounds){
            end_turn(TurnEnd::Kind::ROUND_LIMIT, round_limit_text());
            return;
        }
        request();
        return;
    }
    end_turn(TurnEnd::Kind::ANSWERED);
}

QString Agent::round_limit_text() const{
    return "Claude has worked " + QString::number(m_rounds) +
        " steps in a row on this one message. Go on?";
}

void Agent::run_next_tool(){
    if (!m_busy){
        return;
    }
    if (m_next_call >= m_calls.size()){
        //  All results are there: back to Claude.
        m_messages.append(message("user", m_results));
        m_calls.clear();
        m_next_call = 0;
        m_results = QJsonArray();
        if (m_rounds >= m_settings.max_rounds){
            end_turn(TurnEnd::Kind::ROUND_LIMIT, round_limit_text());
            return;
        }
        request();
        return;
    }

    //  A copy: the list may be gone when the tool is done.
    const PendingCall call = m_calls[m_next_call];
    const Tool* tool = m_tools.find(call.name);
    if (m_events.tool_begin){
        QString what = call.name;
        if (tool != nullptr && tool->describe && !call.broken){
            what = tool->describe(call.input);
        }
        m_events.tool_begin(call.id, what);
    }
    if (call.broken){
        finish_tool(call, ToolResult::error(
            m_cut_off
                ? "The answer was cut off at its maximum length in the middle of this call, so it was not "
                  "run. Make the call again with less in it (a long file in several smaller calls)."
                : "The input of this call did not arrive as valid JSON, so it was not run. Make the call again."
        ));
        return;
    }
    if (tool == nullptr || !tool->run){
        finish_tool(call, ToolResult::error("There is no tool named \"" + call.name + "\"."));
        return;
    }

    const quint64 generation = m_generation;
    const std::shared_ptr<bool> alive = m_alive;
    ToolCallContext context;
    context.stopped = m_stopped;
    context.progress = [this, alive, generation, call](const QString& text){
        if (*alive && generation == m_generation && m_events.tool_progress){
            m_events.tool_progress(call.id, text);
        }
    };
    context.approve = [this, alive, generation](
        const ApprovalRequest& approval, std::function<void(bool approved)> answer
    ){
        //  Nobody to ask: no.
        if (!*alive || generation != m_generation || !m_events.approve){
            if (answer){
                answer(false);
            }
            return;
        }
        m_events.approve(approval, std::move(answer));
    };

    //  The result may come at once (from inside run()) or much later, and
    //  only the first one counts.
    const std::shared_ptr<bool> answered = std::make_shared<bool>(false);
    const auto done = [this, alive, generation, call, answered](ToolResult result){
        if (*answered || !*alive){
            return;
        }
        *answered = true;
        if (generation != m_generation){
            return;
        }
        later([this, call, result]{
            finish_tool(call, result);
        });
    };
    try{
        tool->run(call.input, context, done);
    }catch (const std::exception& e){
        done(ToolResult::error(QString("The tool failed: ") + e.what()));
    }catch (...){
        done(ToolResult::error("The tool failed."));
    }
}

void Agent::finish_tool(const PendingCall& call, ToolResult result){
    QString text = result.text;
    if (text.trimmed().isEmpty()){
        text = result.is_error ? "The tool failed without saying why." : "(Nothing.)";
    }
    if (text.size() > MAX_RESULT_CHARACTERS){
        const qsizetype length = text.size();
        text = text.left(MAX_RESULT_CHARACTERS) +
            "\n[Cut off here: the result was " + QString::number(length) +
            " characters long. Ask for a smaller part.]";
    }
    QJsonObject block{
        {"type", "tool_result"},
        {"tool_use_id", call.id},
    };
    if (result.image.isEmpty()){
        block["content"] = text;
    }else{
        //  A JPEG begins with FF D8 FF. Anything else here is a PNG.
        const bool jpeg = result.image.size() >= 3 &&
            (unsigned char)result.image[0] == 0xff &&
            (unsigned char)result.image[1] == 0xd8 &&
            (unsigned char)result.image[2] == 0xff;
        block["content"] = QJsonArray{
            text_block(text),
            QJsonObject{
                {"type", "image"},
                {"source", QJsonObject{
                    {"type", "base64"},
                    {"media_type", jpeg ? "image/jpeg" : "image/png"},
                    {"data", QString::fromLatin1(result.image.toBase64())},
                }},
            },
        };
    }
    if (result.is_error){
        block["is_error"] = true;
    }
    m_results.append(block);
    if (m_events.tool_end){
        m_events.tool_end(call.id, !result.is_error, text);
    }
    m_next_call++;
    run_next_tool();
}

void Agent::close_tool_calls(){
    if (m_calls.empty()){
        return;
    }
    //  The calls without a result get one: the API refuses a conversation
    //  with a call that was never answered.
    for (size_t index = m_next_call; index < m_calls.size(); index++){
        const QString text = index == m_next_call
            ? "The user stopped the conversation while this call was being worked on. Whether it did its "
              "work is not known: check before relying on it."
            : "The user stopped the conversation before this call was run.";
        m_results.append(failed_result(m_calls[index].id, text));
        if (m_events.tool_end){
            m_events.tool_end(m_calls[index].id, false, "Stopped.");
        }
    }
    m_messages.append(message("user", m_results));
    m_calls.clear();
    m_next_call = 0;
    m_results = QJsonArray();
}

void Agent::end_turn(TurnEnd::Kind kind, const QString& text){
    m_busy = false;
    //  What still comes back from this turn is dropped.
    m_generation++;
    m_call.reset();
    if (m_events.status){
        m_events.status(QString());
    }
    TurnEnd end;
    end.kind = kind;
    end.message = text;
    if (m_events.finished){
        m_events.finished(end);
    }
}



}
}
}

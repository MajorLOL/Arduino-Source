/*  PocketPilot Assistant: Play
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <chrono>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <utility>
#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "CommonFramework/ImageTypes/ImageRGB32_Qt.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "Assistant_Agent.h"
#include "Assistant_Service.h"
#include "Assistant_Play.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


namespace{

//  Runs "work" in the thread of the interface, some time soon. False if
//  there is no interface (any more).
bool post(std::function<void()> work){
    QCoreApplication* application = QCoreApplication::instance();
    if (application == nullptr){
        return false;
    }
    return QMetaObject::invokeMethod(application, std::move(work), Qt::QueuedConnection);
}

//  How long a waiting program sleeps between two looks.
const std::chrono::milliseconds LOOK_EVERY(20);

}



std::string play_picture(
    const ImageViewRGB32& screen, int width, int height, int enlarge,
    PlayPictureFormat format
){
    if (!screen || width <= 0 || height <= 0 || enlarge <= 0){
        return std::string();
    }
    const QImage frame = to_QImage_ref(screen);
    QImage picture = frame.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (enlarge > 1){
        picture = picture.scaled(width * enlarge, height * enlarge, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    //  Without transparency: there is none, and the file is smaller.
    picture = picture.convertToFormat(QImage::Format_RGB888);
    auto packed = [&](const char* kind, int quality){
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (!buffer.open(QIODevice::WriteOnly) || !picture.save(&buffer, kind, quality)){
            return std::string();
        }
        return std::string(bytes.constData(), (size_t)bytes.size());
    };
    if (format == PlayPictureFormat::JPEG){
        //  (Qt reads and writes JPEG through a plugin: without it, a PNG.)
        std::string jpeg = packed("JPG", 82);
        if (!jpeg.empty()){
            return jpeg;
        }
    }
    return packed("PNG", -1);
}



struct PlaySession::Shared : public std::enable_shared_from_this<PlaySession::Shared>{
    //  For both threads, under the lock. An event belongs to the
    //  conversation it was made in: the program only takes those of the
    //  one it asked for last.
    struct Queued{
        uint64_t conversation = 0;
        PlayEvent event;
    };
    mutable Mutex lock;
    std::deque<Queued> queue;
    uint64_t wanted_conversation = 0;
    PlayUsage usage;

    //  The rest only in the thread of the interface.
    Setup setup;
    //  The service whose connection the agent uses.
    Service* service = nullptr;
    ToolSet tools;
    std::unique_ptr<Agent> agent;
    uint64_t conversation = 0;
    //  What Claude wrote and no event took along yet.
    QString said;
    //  Of the conversation that goes on: what is in "usage" already.
    Usage counted;
    //  The calls that wait for what came of them.
    uint64_t calls = 0;
    std::map<std::string, std::function<void(ToolResult result)>> waiting;

    void push(PlayEvent event);
    void fail(const std::string& why);

    bool open();
    void close(const std::string& why);
    void start(const std::string& message, uint64_t number);
    void say(const std::string& message);
    void retry();
    void finish(const std::string& id, const PlayResult& result);
    void on_usage(const Usage& total);
    void on_finished(const TurnEnd& end);
};

void PlaySession::Shared::push(PlayEvent event){
    Queued queued;
    queued.conversation = conversation;
    queued.event = std::move(event);
    LockGuard<Mutex> lg(lock);
    queue.emplace_back(std::move(queued));
}

void PlaySession::Shared::fail(const std::string& why){
    PlayEvent event;
    event.kind = PlayEvent::Kind::FAILED;
    event.message = why;
    push(std::move(event));
}

//  An agent on the connection of the service there is right now.
bool PlaySession::Shared::open(){
    Service* current = Service::instance();
    if (agent && service == current){
        return true;
    }
    close(std::string());
    if (current == nullptr){
        return false;
    }
    service = current;

    const std::weak_ptr<Shared> weak = weak_from_this();
    if (tools.size() == 0){
        for (const PlayTool& definition : setup.tools){
            Tool tool;
            tool.name = QString::fromStdString(definition.name);
            tool.description = QString::fromStdString(definition.description);
            tool.input_schema = QJsonDocument::fromJson(QByteArray::fromStdString(definition.input_schema)).object();
            const std::string name = definition.name;
            //  Handed to the program, which runs it on the console. Its
            //  result comes with finish().
            tool.run = [weak, name](
                const QJsonObject& input, const ToolCallContext&,
                std::function<void(ToolResult result)> done
            ){
                const std::shared_ptr<Shared> self = weak.lock();
                if (!self){
                    done(ToolResult::error("The program has stopped."));
                    return;
                }
                PlayEvent event;
                event.kind = PlayEvent::Kind::CALL;
                event.call.id = std::to_string(++self->calls);
                event.call.name = name;
                event.call.input = QJsonDocument(input).toJson(QJsonDocument::Compact).toStdString();
                event.said = self->said.toStdString();
                self->said.clear();
                self->waiting[event.call.id] = std::move(done);
                self->push(std::move(event));
            };
            tools.add(std::move(tool));
        }
    }

    agent = std::make_unique<Agent>(current->claude(), tools);
    Agent::Events events;
    events.text = [weak](const QString& text){
        if (const std::shared_ptr<Shared> self = weak.lock()){
            self->said += text;
        }
    };
    events.text_restart = [weak]{
        if (const std::shared_ptr<Shared> self = weak.lock()){
            self->said.clear();
        }
    };
    events.usage = [weak](const Usage& total){
        if (const std::shared_ptr<Shared> self = weak.lock()){
            self->on_usage(total);
        }
    };
    events.finished = [weak](const TurnEnd& end){
        if (const std::shared_ptr<Shared> self = weak.lock()){
            self->on_finished(end);
        }
    };
    agent->set_events(std::move(events));

    current->at_end(this, [weak]{
        if (const std::shared_ptr<Shared> self = weak.lock()){
            self->close("Claude's connection was closed.");
        }
    });
    return true;
}

//  Without the agent from here on. With "why": the program is told.
void PlaySession::Shared::close(const std::string& why){
    if (service != nullptr){
        service->forget_end(this);
        service = nullptr;
    }
    //  The calls that wait get no answer any more: their conversation ends
    //  with the agent.
    waiting.clear();
    agent.reset();
    counted = Usage();
    said.clear();
    if (!why.empty()){
        fail(why);
    }
}

void PlaySession::Shared::start(const std::string& message, uint64_t number){
    conversation = number;
    if (!open()){
        fail("Claude is not available: the program's window is closing.");
        return;
    }
    waiting.clear();
    agent->clear();
    counted = Usage();
    said.clear();
    {
        LockGuard<Mutex> lg(lock);
        usage.conversation_tokens = 0;
    }
    Agent::Settings& settings = agent->settings();
    settings.model = QString::fromStdString(setup.model);
    settings.system = QString::fromStdString(setup.system);
    settings.effort = QString::fromStdString(setup.effort);
    settings.max_tokens = setup.max_tokens;
    //  The program has its own limits: on actions and on cost.
    settings.max_rounds = std::numeric_limits<int>::max();
    settings.spend_limit_usd = 0;
    agent->ask(QString::fromStdString(message));
    if (!agent->busy()){
        fail("The conversation could not be started: its first message was empty.");
    }
}

void PlaySession::Shared::say(const std::string& message){
    if (!agent || service != Service::instance()){
        close("Claude's connection was closed.");
        return;
    }
    if (agent->busy()){
        return;
    }
    agent->ask(QString::fromStdString(message));
}

void PlaySession::Shared::retry(){
    if (!agent || service != Service::instance()){
        close("Claude's connection was closed.");
        return;
    }
    if (agent->busy()){
        return;
    }
    agent->go_on();
    if (!agent->busy()){
        fail("There is nothing to try again.");
    }
}

void PlaySession::Shared::finish(const std::string& id, const PlayResult& result){
    const auto found = waiting.find(id);
    if (found == waiting.end()){
        //  Of a conversation that was dropped in the meantime.
        return;
    }
    const std::function<void(ToolResult result)> done = std::move(found->second);
    waiting.erase(found);
    ToolResult tool_result;
    tool_result.text = QString::fromStdString(result.text);
    tool_result.is_error = result.is_error;
    tool_result.image = QByteArray(result.image.data(), (qsizetype)result.image.size());
    done(std::move(tool_result));
}

void PlaySession::Shared::on_usage(const Usage& total){
    LockGuard<Mutex> lg(lock);
    usage.requests += (uint64_t)(total.requests - counted.requests);
    usage.tokens += (uint64_t)(total.tokens() - counted.tokens());
    usage.cost_usd += total.cost_usd - counted.cost_usd;
    usage.cost_known = usage.cost_known && total.cost_known;
    usage.conversation_tokens = agent ? (uint64_t)agent->conversation_tokens() : 0;
    counted = total;
}

void PlaySession::Shared::on_finished(const TurnEnd& end){
    PlayEvent event;
    switch (end.kind){
    case TurnEnd::Kind::STOPPED:
        //  Only this session stops a turn, and then it does not wait for it.
        return;
    case TurnEnd::Kind::ANSWERED:
        event.kind = PlayEvent::Kind::ANSWERED;
        break;
    case TurnEnd::Kind::CUT_OFF:
        event.kind = PlayEvent::Kind::CUT_OFF;
        break;
    case TurnEnd::Kind::REFUSED:
        event.kind = PlayEvent::Kind::REFUSED;
        break;
    case TurnEnd::Kind::FAILED:
    case TurnEnd::Kind::ROUND_LIMIT:
    case TurnEnd::Kind::SPEND_LIMIT:
        event.kind = PlayEvent::Kind::FAILED;
        break;
    }
    event.message = end.message.toStdString();
    event.said = said.toStdString();
    said.clear();
    push(std::move(event));
}



std::string PlaySession::why_unavailable(CancellableScope& scope){
    struct Answer{
        Mutex lock;
        bool there = false;
        std::string text;
    };
    const std::shared_ptr<Answer> answer = std::make_shared<Answer>();
    const bool posted = post([answer]{
        std::string text;
        const Service* service = Service::instance();
        if (service == nullptr){
            text = "Claude is not available in this window.";
        }else if (!service->has_api_key()){
            text = "Claude has no API key yet. Click \"Claude\" along the top of the window and add your key there first.";
        }
        LockGuard<Mutex> lg(answer->lock);
        answer->text = std::move(text);
        answer->there = true;
    });
    if (!posted){
        return "Claude is not available: the program has no interface.";
    }
    while (true){
        {
            LockGuard<Mutex> lg(answer->lock);
            if (answer->there){
                return answer->text;
            }
        }
        scope.wait_for(LOOK_EVERY);
    }
}

PlaySession::PlaySession(Setup setup)
    : m_shared(std::make_shared<Shared>())
{
    m_shared->setup = std::move(setup);
}

PlaySession::~PlaySession(){
    //  The agent goes where it lives: in the thread of the interface.
    const std::shared_ptr<Shared> shared = m_shared;
    post([shared]{
        shared->close(std::string());
    });
}

void PlaySession::start(const std::string& message){
    uint64_t number;
    {
        LockGuard<Mutex> lg(m_shared->lock);
        number = ++m_shared->wanted_conversation;
        m_shared->queue.clear();
    }
    const std::shared_ptr<Shared> shared = m_shared;
    const bool posted = post([shared, message, number]{
        shared->start(message, number);
    });
    if (!posted){
        Shared::Queued queued;
        queued.conversation = number;
        queued.event.kind = PlayEvent::Kind::FAILED;
        queued.event.message = "Claude is not available: the program has no interface.";
        LockGuard<Mutex> lg(m_shared->lock);
        m_shared->queue.emplace_back(std::move(queued));
    }
}

void PlaySession::say(const std::string& message){
    const std::shared_ptr<Shared> shared = m_shared;
    post([shared, message]{
        shared->say(message);
    });
}

void PlaySession::retry(){
    const std::shared_ptr<Shared> shared = m_shared;
    post([shared]{
        shared->retry();
    });
}

PlayEvent PlaySession::wait(CancellableScope& scope){
    while (true){
        {
            LockGuard<Mutex> lg(m_shared->lock);
            while (!m_shared->queue.empty()){
                Shared::Queued next = std::move(m_shared->queue.front());
                m_shared->queue.pop_front();
                //  Not what is left of an older conversation.
                if (next.conversation == m_shared->wanted_conversation){
                    return next.event;
                }
            }
        }
        scope.wait_for(LOOK_EVERY);
    }
}

void PlaySession::finish(const PlayCall& call, PlayResult result){
    const std::shared_ptr<Shared> shared = m_shared;
    const std::string id = call.id;
    post([shared, id, result]{
        shared->finish(id, result);
    });
}

PlayUsage PlaySession::usage() const{
    LockGuard<Mutex> lg(m_shared->lock);
    return m_shared->usage;
}



}
}
}

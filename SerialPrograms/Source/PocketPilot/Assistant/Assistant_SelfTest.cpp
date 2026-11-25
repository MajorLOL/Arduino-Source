/*  PocketPilot Assistant: Self Test
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <deque>
#include <map>
#include <utility>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QObject>
#include <QSslSocket>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include "Assistant_Claude.h"
#include "Assistant_Settings.h"
#include "Assistant_Workspace.h"
#include "Assistant_SelfTest.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


const char* const TestBackend::BASE_BRANCH = "pocketpilot";
const char* const TestBackend::SOURCE_FILE =
    "SerialPrograms/Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStory.cpp";


namespace{

const char* const TEST_REPOSITORY = "/repos/MajorLOL/Arduino-Source";

//  One answer of the Messages API as it streams in.
QByteArray make_stream(const QJsonArray& blocks, const QString& stop_reason){
    QByteArray stream;
    const auto event = [&](const char* name, const QJsonObject& data){
        stream += "event: ";
        stream += name;
        stream += "\ndata: " + QJsonDocument(data).toJson(QJsonDocument::Compact) + "\n\n";
    };
    event("message_start", QJsonObject{{"type", "message_start"}, {"message", QJsonObject{
        {"id", "msg_test"}, {"type", "message"}, {"role", "assistant"}, {"content", QJsonArray()},
        {"model", "claude-sonnet-5-5"},
        {"usage", QJsonObject{
            {"input_tokens", 1800}, {"cache_read_input_tokens", 3100},
            {"cache_creation_input_tokens", 400}, {"output_tokens", 1},
        }},
    }}});
    int index = 0;
    for (const QJsonValue& value : blocks){
        const QJsonObject block = value.toObject();
        if (block.value("type").toString() == "text"){
            event("content_block_start", QJsonObject{
                {"type", "content_block_start"}, {"index", index},
                {"content_block", QJsonObject{{"type", "text"}, {"text", ""}}},
            });
            const QString text = block.value("text").toString();
            for (qsizetype at = 0; at < text.size(); at += 24){
                event("content_block_delta", QJsonObject{
                    {"type", "content_block_delta"}, {"index", index},
                    {"delta", QJsonObject{{"type", "text_delta"}, {"text", text.mid(at, 24)}}},
                });
            }
        }else{
            event("content_block_start", QJsonObject{
                {"type", "content_block_start"}, {"index", index},
                {"content_block", QJsonObject{
                    {"type", "tool_use"}, {"id", block.value("id")}, {"name", block.value("name")},
                    {"input", QJsonObject()},
                }},
            });
            const QString json = QString::fromUtf8(
                QJsonDocument(block.value("input").toObject()).toJson(QJsonDocument::Compact)
            );
            for (qsizetype at = 0; at < json.size(); at += 40){
                event("content_block_delta", QJsonObject{
                    {"type", "content_block_delta"}, {"index", index},
                    {"delta", QJsonObject{{"type", "input_json_delta"}, {"partial_json", json.mid(at, 40)}}},
                });
            }
        }
        event("content_block_stop", QJsonObject{{"type", "content_block_stop"}, {"index", index}});
        index++;
    }
    event("message_delta", QJsonObject{
        {"type", "message_delta"},
        {"delta", QJsonObject{{"stop_reason", stop_reason}}},
        {"usage", QJsonObject{{"output_tokens", 240}}},
    });
    event("message_stop", QJsonObject{{"type", "message_stop"}});
    return stream;
}

HttpResponse json_response(int status, const QJsonValue& value){
    HttpResponse response;
    response.status = status;
    response.body = value.isArray()
        ? QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact)
        : QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
    return response;
}

HttpResponse not_found(){
    return json_response(404, QJsonObject{{"message", "Not Found"}});
}

}



struct TestBackend::Data{
    //  Claude: the answers to come.
    std::deque<QByteArray> answers;
    int claude_requests = 0;
    int next_call = 1;

    //  GitHub: files by their id, the files of each tree, the commits, and
    //  where each branch is.
    struct Commit{
        QString tree;
        QString parent;
    };
    std::map<QString, QByteArray> blobs;
    std::map<QString, std::map<QString, QString>> trees;
    std::map<QString, Commit> commits;
    std::map<QString, QString> refs;
    int counter = 0;

    QString new_id(const QByteArray& of){
        counter++;
        return QString::fromLatin1(
            QCryptographicHash::hash(of + QByteArray::number(counter), QCryptographicHash::Sha1).toHex()
        );
    }
    QString add_blob(const QByteArray& bytes){
        const QString id = QString::fromLatin1(
            QCryptographicHash::hash("blob " + bytes, QCryptographicHash::Sha1).toHex()
        );
        blobs[id] = bytes;
        return id;
    }
    const std::map<QString, QString>* files_of(const QString& commit) const{
        const auto entry = commits.find(commit);
        if (entry == commits.end()){
            return nullptr;
        }
        const auto tree = trees.find(entry->second.tree);
        return tree == trees.end() ? nullptr : &tree->second;
    }

    //  The answer to a request. For Claude's, "stream" gets the answer.
    HttpResponse answer(const HttpRequest& request, QByteArray& stream){
        const QString host = request.url.host();
        QString path = request.url.path(QUrl::FullyDecoded);
        const QString method = QString::fromLatin1(request.method);

        if (host == "api.anthropic.com"){
            claude_requests++;
            HttpResponse response;
            if (answers.empty()){
                return json_response(500, QJsonObject{
                    {"type", "error"},
                    {"error", QJsonObject{{"type", "api_error"}, {"message", "The test has no answer left."}}},
                });
            }
            response.status = 200;
            stream = answers.front();
            answers.pop_front();
            return response;
        }
        if (host == "raw.githubusercontent.com"){
            //  /owner/repository/commit/path
            const QString commit = path.section('/', 3, 3);
            const QString file = path.section('/', 4);
            const std::map<QString, QString>* files = files_of(commit);
            HttpResponse response;
            if (files == nullptr || files->find(file) == files->end()){
                response.status = 404;
                return response;
            }
            response.status = 200;
            response.body = blobs[files->at(file)];
            return response;
        }
        if (host != "api.github.com" || !path.startsWith(QLatin1String(TEST_REPOSITORY))){
            return not_found();
        }
        path = path.mid(QString::fromLatin1(TEST_REPOSITORY).size());
        const QJsonObject body = QJsonDocument::fromJson(request.body).object();

        if (method == "GET" && path.startsWith("/git/ref/heads/")){
            const QString branch = path.mid(QString("/git/ref/heads/").size());
            const auto ref = refs.find(branch);
            if (ref == refs.end()){
                return not_found();
            }
            return json_response(200, QJsonObject{{"object", QJsonObject{{"sha", ref->second}}}});
        }
        if (method == "GET" && path.startsWith("/git/trees/")){
            const std::map<QString, QString>* files = files_of(path.mid(QString("/git/trees/").size()));
            if (files == nullptr){
                return not_found();
            }
            QJsonArray entries;
            for (const auto& file : *files){
                entries.append(QJsonObject{
                    {"path", file.first}, {"mode", "100644"}, {"type", "blob"}, {"sha", file.second},
                    {"size", (double)blobs[file.second].size()},
                });
            }
            return json_response(200, QJsonObject{{"tree", entries}, {"truncated", false}});
        }
        if (method == "GET" && path.startsWith("/contents/")){
            const QString commit = QUrlQuery(request.url).queryItemValue("ref");
            const std::map<QString, QString>* files = files_of(commit);
            const QString file = path.mid(QString("/contents/").size());
            HttpResponse response;
            if (files == nullptr || files->find(file) == files->end()){
                return not_found();
            }
            response.status = 200;
            response.body = blobs[files->at(file)];
            return response;
        }
        if (method == "GET" && path.startsWith("/git/commits/")){
            const auto commit = commits.find(path.mid(QString("/git/commits/").size()));
            if (commit == commits.end()){
                return not_found();
            }
            return json_response(200, QJsonObject{{"tree", QJsonObject{{"sha", commit->second.tree}}}});
        }
        if (method == "POST" && path == "/git/blobs"){
            const QString id = add_blob(QByteArray::fromBase64(body.value("content").toString().toLatin1()));
            return json_response(201, QJsonObject{{"sha", id}});
        }
        if (method == "POST" && path == "/git/trees"){
            const auto base = trees.find(body.value("base_tree").toString());
            if (base == trees.end()){
                return json_response(422, QJsonObject{{"message", "base_tree is not a tree"}});
            }
            std::map<QString, QString> files = base->second;
            for (const QJsonValue& value : body.value("tree").toArray()){
                const QJsonObject entry = value.toObject();
                files[entry.value("path").toString()] = entry.value("sha").toString();
            }
            const QString id = new_id("tree");
            trees[id] = std::move(files);
            return json_response(201, QJsonObject{{"sha", id}});
        }
        if (method == "POST" && path == "/git/commits"){
            Commit commit;
            commit.tree = body.value("tree").toString();
            commit.parent = body.value("parents").toArray().first().toString();
            if (trees.find(commit.tree) == trees.end() || commits.find(commit.parent) == commits.end()){
                return json_response(422, QJsonObject{{"message", "Not a tree or not a commit"}});
            }
            const QString id = new_id("commit");
            commits[id] = commit;
            return json_response(201, QJsonObject{{"sha", id}});
        }
        if (method == "POST" && path == "/git/refs"){
            const QString branch = body.value("ref").toString().mid(QString("refs/heads/").size());
            if (branch.isEmpty() || refs.find(branch) != refs.end()){
                return json_response(422, QJsonObject{{"message", "Reference already exists"}});
            }
            refs[branch] = body.value("sha").toString();
            return json_response(201, QJsonObject{{"ref", body.value("ref")}});
        }
        if (method == "PATCH" && path.startsWith("/git/refs/heads/")){
            const QString branch = path.mid(QString("/git/refs/heads/").size());
            if (refs.find(branch) == refs.end()){
                return not_found();
            }
            refs[branch] = body.value("sha").toString();
            return json_response(200, QJsonObject{{"ref", "refs/heads/" + branch}});
        }
        if (method == "GET" && path.startsWith("/compare/")){
            return json_response(200, QJsonObject{{"ahead_by", 0}, {"behind_by", 0}, {"files", QJsonArray()}});
        }
        return not_found();
    }
};


namespace{

class TestCall : public HttpCall{
public:
    virtual void abort() override{
        aborted = true;
    }
    bool aborted = false;
};

class TestTransport : public HttpTransport{
public:
    explicit TestTransport(std::shared_ptr<TestBackend::Data> data)
        : m_data(std::move(data))
    {}

    virtual std::shared_ptr<HttpCall> send(
        const HttpRequest& request, DataHandler on_data, DoneHandler on_done
    ) override{
        std::shared_ptr<TestCall> call = std::make_shared<TestCall>();
        QByteArray stream;
        const HttpResponse response = m_data->answer(request, stream);
        //  As the network: the answer comes from the event loop.
        QTimer::singleShot(0, &m_context, [call, response, stream, on_data, on_done]{
            if (call->aborted){
                return;
            }
            if (on_data && !stream.isEmpty()){
                //  In pieces, as a stream comes in.
                for (qsizetype at = 0; at < stream.size() && !call->aborted; at += 300){
                    on_data(stream.mid(at, 300));
                }
            }
            if (!call->aborted && on_done){
                HttpResponse whole = response;
                if (!on_data){
                    whole.body += stream;
                }
                on_done(whole);
            }
        });
        return call;
    }

private:
    std::shared_ptr<TestBackend::Data> m_data;
    QObject m_context;
};

}


TestBackend::TestBackend()
    : m_data(std::make_shared<Data>())
{
    //  A repository with one source file and the list of files to build.
    QByteArray source = "/*  Pokemon FRLG Autostory\r\n *\r\n */\r\n\r\nnamespace PokemonAutomation{\r\n\r\n";
    for (int line = 1; line <= 36; line++){
        source += "//  Line " + QByteArray::number(line) + " of the test's source file.\r\n";
    }
    source +=
        "            if (league_again_losses > 6){\r\n"
        "                throw UserSetupError(env.logger(), \"The League was lost seven times in a row.\");\r\n"
        "            }\r\n"
        "\r\n}\r\n";
    std::map<QString, QString> files;
    files[QString::fromLatin1(SOURCE_FILE)] = m_data->add_blob(source);
    files["SerialPrograms/cmake/SourceFiles.cmake"] = m_data->add_blob(
        "    Source/VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStory.cpp\r\n"
    );
    const QString tree = m_data->new_id("tree");
    m_data->trees[tree] = files;
    const QString commit = m_data->new_id("commit");
    m_data->commits[commit] = Data::Commit{tree, QString()};
    m_data->refs[QString::fromLatin1(BASE_BRANCH)] = commit;
}

TestBackend::~TestBackend() = default;

std::unique_ptr<HttpTransport> TestBackend::make_transport(){
    return std::make_unique<TestTransport>(m_data);
}

void TestBackend::claude_calls(const QString& tool, const QJsonObject& input, const QString& text_before){
    QJsonArray blocks;
    if (!text_before.isEmpty()){
        blocks.append(QJsonObject{{"type", "text"}, {"text", text_before}});
    }
    blocks.append(QJsonObject{
        {"type", "tool_use"},
        {"id", "toolu_test_" + QString::number(m_data->next_call++)},
        {"name", tool},
        {"input", input},
    });
    m_data->answers.push_back(make_stream(blocks, "tool_use"));
}

void TestBackend::claude_says(const QString& text){
    m_data->answers.push_back(make_stream(QJsonArray{QJsonObject{{"type", "text"}, {"text", text}}}, "end_turn"));
}

bool TestBackend::has_branch(const QString& branch) const{
    return m_data->refs.find(branch) != m_data->refs.end();
}

QByteArray TestBackend::file_on(const QString& branch, const QString& path) const{
    const auto ref = m_data->refs.find(branch);
    if (ref == m_data->refs.end()){
        return QByteArray();
    }
    const std::map<QString, QString>* files = m_data->files_of(ref->second);
    if (files == nullptr || files->find(path) == files->end()){
        return QByteArray();
    }
    return m_data->blobs[files->at(path)];
}

int TestBackend::requests_to_claude() const{
    return m_data->claude_requests;
}



//  The checks.

namespace{

//  A stream as the documentation of the API shows it: some text, then a
//  tool call whose input comes in pieces.
const char* const DOCUMENTED_STREAM = R"sse(event: message_start
data: {"type":"message_start","message":{"id":"msg_014p7gG3wDgGV9EUtLvnow3U","type":"message","role":"assistant","model":"claude-opus-5","stop_sequence":null,"usage":{"input_tokens":472,"output_tokens":2},"content":[],"stop_reason":null}}

event: content_block_start
data: {"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}

event: ping
data: {"type": "ping"}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Okay"}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":", let's check"}}

event: content_block_stop
data: {"type":"content_block_stop","index":0}

event: content_block_start
data: {"type":"content_block_start","index":1,"content_block":{"type":"tool_use","id":"toolu_01T1x1fJ34qAmk2tNTrN7Up6","name":"get_weather","input":{}}}

event: content_block_delta
data: {"type":"content_block_delta","index":1,"delta":{"type":"input_json_delta","partial_json":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":1,"delta":{"type":"input_json_delta","partial_json":"{\"location\":"}}

event: content_block_delta
data: {"type":"content_block_delta","index":1,"delta":{"type":"input_json_delta","partial_json":" \"San"}}

event: content_block_delta
data: {"type":"content_block_delta","index":1,"delta":{"type":"input_json_delta","partial_json":" Francisco, CA\"}"}}

event: content_block_stop
data: {"type":"content_block_stop","index":1}

event: message_delta
data: {"type":"message_delta","delta":{"stop_reason":"tool_use","stop_sequence":null},"usage":{"output_tokens":89}}

event: message_stop
data: {"type":"message_stop"}

)sse";

//  An answer that starts with a thinking block, as the current models send
//  it: no text in it, and a signature that has to go back unchanged.
const char* const THINKING_STREAM = R"sse(event: message_start
data: {"type":"message_start","message":{"id":"msg_01","type":"message","role":"assistant","content":[],"model":"claude-sonnet-5-5","usage":{"input_tokens":10,"cache_read_input_tokens":900,"cache_creation_input_tokens":50,"output_tokens":1}}}

event: content_block_start
data: {"type":"content_block_start","index":0,"content_block":{"type":"thinking","thinking":"","signature":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"thinking_delta","thinking":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":0,"delta":{"type":"signature_delta","signature":"EqQBCgIYAhIM1gbcDa9GJwZA2b3hGgxBdjrkzLoky3dl1pkiMOYds"}}

event: content_block_stop
data: {"type":"content_block_stop","index":0}

event: content_block_start
data: {"type":"content_block_start","index":1,"content_block":{"type":"text","text":""}}

event: content_block_delta
data: {"type":"content_block_delta","index":1,"delta":{"type":"text_delta","text":"It is **21**."}}

event: content_block_stop
data: {"type":"content_block_stop","index":1}

event: message_delta
data: {"type":"message_delta","delta":{"stop_reason":"end_turn","stop_sequence":null},"usage":{"output_tokens":40}}

event: message_stop
data: {"type":"message_stop"}

)sse";

bool about_equal(double a, double b){
    const double difference = a - b;
    return difference < 1e-9 && difference > -1e-9;
}

}


CheckResult run_checks(const QString& folder, const std::function<void(const QString& line)>& report){
    CheckResult result;
    const auto check = [&](bool ok, const char* what){
        result.checks++;
        if (!ok){
            result.failed++;
            if (report){
                report(QString::fromLatin1(what));
            }
        }
    };

    //  Claude and GitHub are only reached over a secure connection: the
    //  build has to bring along what Qt needs for one.
    check(QSslSocket::supportsSsl(), "secure connections (TLS) can be made");

    //  The API's stream, whole and cut into pieces of several sizes.
    for (int size : {1, 7, 4096}){
        MessageStream stream;
        QString text;
        QString tool;
        stream.on_text = [&](const QString& more){
            text += more;
        };
        stream.on_tool = [&](const QString& name){
            tool = name;
        };
        //  With the line ends of a stream, whatever those of this file are.
        QByteArray data(DOCUMENTED_STREAM);
        data.replace("\r\n", "\n");
        for (qsizetype at = 0; at < data.size(); at += size){
            stream.feed(data.mid(at, size));
        }
        stream.finish();
        const QJsonArray content = stream.content();
        check(stream.complete() && !stream.failed(), "the documented stream is read to its end");
        check(stream.stop_reason() == "tool_use", "the stream's stop reason");
        check(text == "Okay, let's check", "the stream's text");
        check(tool == "get_weather", "the stream's tool call is announced");
        check(content.size() == 2, "the stream has two blocks");
        check(
            content[1].toObject().value("input").toObject().value("location").toString() == "San Francisco, CA",
            "the tool call's input is put together from its pieces"
        );
        check(content[1].toObject().value("id").toString() == "toolu_01T1x1fJ34qAmk2tNTrN7Up6", "the tool call's id");
        check(stream.usage().input == 472 && stream.usage().output == 89, "the stream's token counts");
        check(stream.broken_tool_calls().isEmpty(), "no tool call counts as broken");
    }
    {
        MessageStream stream;
        QByteArray data(THINKING_STREAM);
        //  As a server sends it.
        data.replace("\r\n", "\n");
        data.replace("\n", "\r\n");
        for (qsizetype at = 0; at < data.size(); at += 11){
            stream.feed(data.mid(at, 11));
        }
        stream.finish();
        const QJsonArray content = stream.content();
        check(stream.complete() && content.size() == 2, "a stream with a thinking block is read");
        const QJsonObject thinking = content[0].toObject();
        check(thinking.value("type").toString() == "thinking", "the thinking block stays a block");
        check(
            thinking.value("signature").toString() == "EqQBCgIYAhIM1gbcDa9GJwZA2b3hGgxBdjrkzLoky3dl1pkiMOYds",
            "the thinking block's signature is kept as it came"
        );
        check(thinking.value("thinking").isString() && thinking.value("thinking").toString().isEmpty(), "its empty text is kept");
        check(
            stream.usage().cache_read == 900 && stream.usage().cache_write == 50,
            "the tokens read from and written to the cache"
        );
    }
    {
        MessageStream stream;
        stream.feed("event: error\ndata: {\"type\": \"error\", \"error\": {\"type\": \"overloaded_error\", \"message\": \"Overloaded\"}}\n\n");
        stream.finish();
        check(stream.failed() && !stream.complete() && stream.error_type() == "overloaded_error", "an error in the stream");
    }

    //  What a request costs.
    {
        double usd = 0;
        check(estimate_cost("claude-sonnet-5-5", 1000000, 0, 0, 0, usd) && about_equal(usd, 2.0), "the price of Sonnet's input");
        check(
            estimate_cost("claude-sonnet-5-5", 0, 1000000, 1000000, 1000000, usd) && about_equal(usd, 2.5 + 0.10 + 10.0),
            "the prices of Sonnet's cache and output"
        );
        check(estimate_cost("claude-haiku-5-5", 50000, 0, 0, 1000, usd) && about_equal(usd, 0.0055), "the price of Haiku");
        check(
            estimate_cost("claude-haiku-5-5", 150000, 0, 0, 1000, usd) && about_equal(usd, 5 * 0.0155),
            "the price of Haiku for a long request"
        );
        check(estimate_cost("claude-opus-5-5", 0, 0, 1000000, 0, usd) && about_equal(usd, 0.20), "the price of Opus's cache");
        check(!estimate_cost("some-other-model", 1, 1, 1, 1, usd), "a model without a known price");
        Usage usage;
        usage.input = 12400;
        usage.requests = 1;
        usage.cost_usd = 0.031;
        check(usage_text(usage) == "12.4k tokens, about $0.03", "the text of the meter");
    }

    //  A request's body: what the API documents, and nothing it refuses.
    {
        class NoTransport : public HttpTransport{
        public:
            virtual std::shared_ptr<HttpCall> send(const HttpRequest&, DataHandler, DoneHandler) override{
                return nullptr;
            }
        };
        NoTransport transport;
        ClaudeClient client(transport, []{
            return QString();
        });
        ClaudeRequest request;
        request.model = "claude-sonnet-5-5";
        request.system = "Instructions.";
        request.effort = "low";
        request.messages = QJsonArray{QJsonObject{{"role", "user"}, {"content", "Hello"}}};
        const QJsonObject body = QJsonDocument::fromJson(client.request_body(request)).object();
        check(
            body.value("model").toString() == "claude-sonnet-5-5" && body.value("stream").toBool() &&
            body.value("max_tokens").toInt() > 0 && body.value("system").toString() == "Instructions.",
            "the body of a request"
        );
        check(body.value("cache_control").toObject().value("type").toString() == "ephemeral", "the request asks for caching");
        check(body.value("output_config").toObject().value("effort").toString() == "low", "the request's effort");
        check(
            !body.contains("temperature") && !body.contains("top_p") && !body.contains("top_k") &&
            !body.contains("tool_choice") && !body.contains("tools"),
            "the request has nothing the API refuses"
        );
    }

    //  The workspace of the API: named for a key that was not made for one,
    //  and words for the user when the API misses it.
    {
        class NoTransport : public HttpTransport{
        public:
            virtual std::shared_ptr<HttpCall> send(const HttpRequest&, DataHandler, DoneHandler) override{
                return nullptr;
            }
        };
        //  A header's value, "(none)" or "(twice)".
        const auto header_of = [](const std::vector<HttpHeader>& headers, const char* name){
            QByteArray value = "(none)";
            int found = 0;
            for (const HttpHeader& header : headers){
                if (header.name == name){
                    value = header.value;
                    found++;
                }
            }
            return found > 1 ? QByteArray("(twice)") : value;
        };
        NoTransport transport;
        QString workspace_id;
        ClaudeClient client(
            transport,
            []{
                return QString("  sk-ant-test-0123456789\n");
            },
            [&workspace_id]{
                return workspace_id;
            }
        );
        std::vector<HttpHeader> headers = client.request_headers();
        check(
            header_of(headers, "x-api-key") == "sk-ant-test-0123456789" &&
            header_of(headers, "anthropic-version").startsWith("20") &&
            header_of(headers, "content-type") == "application/json",
            "the headers of a request"
        );
        check(header_of(headers, "anthropic-workspace-id") == "(none)", "no workspace is named unless one is set");
        workspace_id = " wrkspc_01JwQvzr7rXLA5AGx3HKfFUJ ";
        headers = client.request_headers();
        check(
            header_of(headers, "anthropic-workspace-id") == "wrkspc_01JwQvzr7rXLA5AGx3HKfFUJ",
            "the workspace that is set is named"
        );

        const QString unscoped =
            "This API key is not scoped to a workspace, so this request must include the anthropic-workspace-id "
            "header with the ID of the workspace to use. Add the header, or use an API key that is scoped to a "
            "workspace.";
        const QString told = api_error_text("invalid_request_error", 400, unscoped, QString(), false);
        check(
            told.contains("was not made for one workspace") && told.contains("settings of this panel") &&
            told.contains(unscoped),
            "a key without a workspace: what to do about it"
        );
        check(
            api_error_text(
                "invalid_request_error", 400, "anthropic-workspace-id header must be a valid workspace ID.",
                QString(), true
            ).contains("did not take the workspace ID"),
            "a workspace ID the API does not take"
        );
        check(
            api_error_text("not_found_error", 404, "Not found", QString(), true).contains("workspace ID") &&
            !api_error_text("not_found_error", 404, "model: claude-none", QString(), false).contains("workspace"),
            "the workspace ID is pointed at only when one was named"
        );
        check(
            clean_workspace_id("  wrkspc_01Ab-Z9\r\nx-other: y ") == "wrkspc_01Ab-Z9x-othery" &&
            clean_workspace_id(QString()).isEmpty(),
            "a workspace ID is letters, digits, _ and - alone"
        );
    }

    //  Text files: their line ends and the mark in front come back as they were.
    {
        const QByteArray samples[] = {
            QByteArray("a\r\nb\r\n"), QByteArray("a\nb\n"), QByteArray("a\nb"),
            QByteArray("\xEF\xBB\xBF" "a\r\n\r\nb\r\n"), QByteArray(),
        };
        bool same = true;
        for (const QByteArray& sample : samples){
            const TextFile file = TextFile::from_bytes(sample);
            same = same && file.valid && file.to_bytes() == sample;
        }
        check(same, "text files come back byte for byte");
        check(TextFile::from_bytes("a\r\nb\r\n").crlf && !TextFile::from_bytes("a\nb\n").crlf, "the kind of line end");
        check(!TextFile::from_bytes(QByteArray("a\0b", 3)).valid && !TextFile::from_bytes("caf\xE9\n").valid, "what is not text");

        QByteArray before;
        for (int line = 1; line <= 20; line++){
            before += "line " + QByteArray::number(line) + "\r\n";
        }
        QByteArray after = before;
        after.replace("line 10\r\n", "line ten\r\n");
        const QString diff = diff_text(before, after);
        check(diff.contains("@@ -7,7 +7,7 @@") && diff.contains("-line 10\n+line ten\n"), "the lines of a change");
        check(diff_text(before, before) == "(No change.)\n", "no change");

        check(branch_slug("Fix the party menu!") == "fix-the-party-menu", "a branch's name from a topic");
        check(branch_slug("???") == "change", "a branch's name from nothing usable");
        check(why_not_writable(QString::fromLatin1(TestBackend::SOURCE_FILE)).isEmpty(), "a source file can be changed");
        check(!why_not_writable(".github/workflows/pocketpilot-windows.yml").isEmpty(), "a workflow cannot be changed");
        check(!why_not_writable("../outside.cpp").isEmpty() && !why_not_writable("a/../b.cpp").isEmpty(), "a path outside cannot be changed");
        check(!why_not_writable("3rdParty/library/file.h").isEmpty(), "third-party code cannot be changed");
        check(!why_not_writable("SerialPrograms/Source/picture.png").isEmpty(), "a picture cannot be changed");

        const QByteArray log =
            "2026-11-14T05:19:36.3Z cl : Command line warning D9025 : overriding '/W4' with '/W0'\n"
            "2026-11-14T05:19:36.5Z D:\\a\\A\\A\\A\\SerialPrograms\\Source\\PocketPilot\\PocketPilot_Home.cpp(123): error C2065: 'foo': undeclared identifier\n"
            "2026-11-14T05:19:37.1Z ##[error]Process completed with exit code 1.\n";
        const QStringList errors = build_errors(log, 40);
        check(
            errors.size() == 2 &&
            errors.value(0) == "SerialPrograms\\Source\\PocketPilot\\PocketPilot_Home.cpp(123): error C2065: 'foo': undeclared identifier",
            "the errors of a build's log"
        );
    }

    //  The keys: kept in their file, back after a new start, never in the
    //  file as they are.
    {
        const QString path = folder + "/Assistant-SelfTest-Keys.json";
        QFile::remove(path);
        const QString key = "sk-ant-test-0123456789-ABCDEFGHIJ";
        const QString token = "github_pat_test_9876543210";
        QString error;
        {
            SecretStore store(path);
            check(!store.has(Secret::CLAUDE_API_KEY), "no key before one is saved");
            check(store.set(Secret::CLAUDE_API_KEY, "  " + key + "\n", error), "a key is saved");
            check(store.set(Secret::GITHUB_TOKEN, token, error), "a token is saved");
        }
        {
            SecretStore store(path);
            check(store.get(Secret::CLAUDE_API_KEY) == key, "the key is back after a new start");
            check(store.get(Secret::GITHUB_TOKEN) == token, "the token is back after a new start");
            check(!store.unreadable(Secret::CLAUDE_API_KEY), "the saved key can be read");
            //  Closed again before the store writes: Windows does not
            //  replace a file that is open.
            QByteArray content;
            {
                QFile file(path);
                if (file.open(QIODevice::ReadOnly)){
                    content = file.readAll();
                }
            }
            check(!content.isEmpty() && !content.contains(key.toUtf8()) && !content.contains(token.toUtf8()), "the keys' file does not show the keys");
#ifdef _WIN32
            //  Encrypted, not only encoded.
            check(!content.contains(key.toUtf8().toBase64()), "the keys' file is encrypted");
#endif
            check(store.set(Secret::CLAUDE_API_KEY, QString(), error) && !store.has(Secret::CLAUDE_API_KEY), "a key is forgotten");
        }
        {
            SecretStore store(path);
            check(!store.has(Secret::CLAUDE_API_KEY) && store.has(Secret::GITHUB_TOKEN), "a forgotten key stays forgotten");
        }
        QFile::remove(path);

        //  Not remembered: the keys are in memory only. The file never has
        //  one, whatever is saved, and a new start has none.
        const auto file_text = [&]{
            QFile file(path);
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        };
        {
            SecretStore store(path);
            check(store.remembers(), "the keys are remembered unless it is switched off");
            check(store.set(Secret::CLAUDE_API_KEY, key, error), "a key is saved before it is switched off");
            check(!file_text().isEmpty() && file_text().contains("claude_api_key"), "a remembered key is in the file");
            check(store.set_remembers(false, error) && !store.remembers(), "remembering is switched off");
            check(store.get(Secret::CLAUDE_API_KEY) == key, "the key stays in use after it is switched off");
            check(
                !file_text().isEmpty() && !file_text().contains("claude_api_key") && !file_text().contains("github_token"),
                "switched off, the file has no key any more"
            );
            check(store.set(Secret::GITHUB_TOKEN, token, error) && store.get(Secret::GITHUB_TOKEN) == token, "a token is kept in memory");
            check(
                !file_text().contains("github_token") && !file_text().contains(token.toUtf8()) && !file_text().contains(key.toUtf8()),
                "a key that is not remembered is never written"
            );
            check(!store.protection_text().isEmpty() && store.protection_text().contains("memory"), "the note says the keys are in memory only");
        }
        {
            SecretStore store(path);
            check(!store.remembers(), "not remembering stays so after a new start");
            check(!store.has(Secret::CLAUDE_API_KEY) && !store.has(Secret::GITHUB_TOKEN), "a key that is not remembered is gone after a new start");
            check(!store.unreadable(Secret::CLAUDE_API_KEY), "no key is left that cannot be read");
            //  Back on: the key in use is written.
            check(store.set(Secret::CLAUDE_API_KEY, key, error), "a key is kept without remembering it");
            check(store.set_remembers(true, error) && store.remembers(), "remembering is switched on");
            check(file_text().contains("claude_api_key"), "switched on, the key in use is written");
        }
        {
            SecretStore store(path);
            check(store.remembers() && store.get(Secret::CLAUDE_API_KEY) == key, "the key is back after it was remembered again");
        }
        {
            //  A key in a file that says not to remember is not read.
            QFile::remove(path);
            {
                SecretStore store(path);
                store.set(Secret::CLAUDE_API_KEY, key, error);
            }
            QByteArray content = file_text();
            content.replace("\"remember\": true", "\"remember\": false");
            QFile file(path);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)){
                file.write(content);
            }
            file.close();
            SecretStore store(path);
            check(!store.remembers() && !store.has(Secret::CLAUDE_API_KEY), "a key in a file that says not to remember is not used");
        }
        QFile::remove(path);

        const QString settings_path = folder + "/Assistant-SelfTest-Settings.json";
        Settings settings;
        settings.chat_model = "claude-opus-5-5";
        settings.ask_at_usd = 2.5;
        settings.panel_width = 512;
        settings.api_workspace_id = "wrkspc_01SelfTest";
        check(settings.save(settings_path), "the settings are saved");
        Settings again;
        again.load(settings_path);
        check(
            again.chat_model == "claude-opus-5-5" && about_equal(again.ask_at_usd, 2.5) && again.panel_width == 512 &&
            again.chat_effort == Settings().chat_effort,
            "the settings are back after a new start"
        );
        check(
            again.api_workspace_id == "wrkspc_01SelfTest" && Settings().api_workspace_id.isEmpty(),
            "the workspace ID is back after a new start, and empty unless set"
        );
        QFile::remove(settings_path);
    }
    return result;
}



}
}
}

/*  PocketPilot Assistant: GitHub
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The REST API of GitHub, for the repository this program is built from:
 *  reading its files, and (with the user's token) making a branch and a
 *  commit on it. Reading a public repository needs no token.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Assistant_GitHub_H
#define PokemonAutomation_PocketPilot_Assistant_GitHub_H

#include <functional>
#include <memory>
#include <vector>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include "Assistant_Http.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


struct Repository{
    QString owner;
    QString name;
    //  The branch the program is built from: a change is made for it.
    QString base_branch;

    bool valid() const{
        return !owner.isEmpty() && !name.isEmpty() && !base_branch.isEmpty();
    }
    QString slug() const{ return owner + "/" + name; }
    //  "https://github.com/owner/name"
    QString web_url() const{ return "https://github.com/" + slug(); }
};


struct GitHubReply{
    int status = 0;
    QJsonDocument json;
    QByteArray body;
    //  Why it did not work, in words for the user (and for Claude). Empty:
    //  it worked.
    QString error;

    bool ok() const{ return error.isEmpty(); }
    QJsonObject object() const{ return json.object(); }
};


class GitHubClient{
public:
    using TokenSource = std::function<QString()>;
    using Done = std::function<void(const GitHubReply& reply)>;

    GitHubClient(HttpTransport& transport, TokenSource token);
    ~GitHubClient();

    bool has_token() const;

    //  A call of the API. "path" starts after the host: "/repos/...". The
    //  answer is JSON.
    void call(const QByteArray& method, const QString& path, const QJsonObject& body, Done done);
    void get(const QString& path, Done done){
        call("GET", path, QJsonObject(), std::move(done));
    }

    //  The bytes of a file at a commit, in "body".
    void file(const Repository& repository, const QString& commit, const QString& path, Done done);

    //  The log of a job of a workflow run, as text in "body". Needs a token.
    void job_log(const Repository& repository, qint64 job, Done done);

    //  "a/b c.txt" for use in a URL: "a/b%20c.txt".
    static QString encode_path(const QString& path);

private:
    void send(const HttpRequest& request, bool json, Done done);
    std::vector<HttpHeader> headers(const char* accept) const;

private:
    HttpTransport& m_transport;
    TokenSource m_token;
    std::shared_ptr<bool> m_alive;
    //  The context of answers given without a request.
    QObject m_context;
};



}
}
}
#endif

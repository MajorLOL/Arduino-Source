/*  PocketPilot Assistant: GitHub
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <utility>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include "Assistant_GitHub.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


namespace{

const char* const API_HOST = "https://api.github.com";
const char* const RAW_HOST = "https://raw.githubusercontent.com";
const char* const ACCEPT_JSON = "application/vnd.github+json";
const char* const ACCEPT_RAW = "application/vnd.github.raw+json";

//  Why a request did not work. Empty: it worked.
QString error_text(const HttpResponse& response, const QByteArray& method, bool has_token){
    if (response.status == 0){
        return "No answer from GitHub (" + response.error + "). Check the internet connection.";
    }
    if (response.status >= 200 && response.status < 300){
        if (!response.error.isEmpty()){
            return "The answer from GitHub broke off (" + response.error + ").";
        }
        return QString();
    }
    const QJsonObject object = QJsonDocument::fromJson(response.body).object();
    QString message = object.value("message").toString();
    for (const QJsonValue& value : object.value("errors").toArray()){
        const QString detail = value.isString() ? value.toString() : value.toObject().value("message").toString();
        if (!detail.isEmpty()){
            message += " (" + detail + ")";
        }
    }
    if (message.isEmpty()){
        message = "HTTP " + QString::number(response.status);
    }

    if (response.status == 401){
        return "GitHub did not accept the token. Check it in the settings of this panel. (" + message + ")";
    }
    if ((response.status == 403 || response.status == 429) && response.header("x-ratelimit-remaining") == "0"){
        QString until;
        bool ok = false;
        const qint64 reset = response.header("x-ratelimit-reset").toLongLong(&ok);
        if (ok && reset > 0){
            until = " until " + QDateTime::fromSecsSinceEpoch(reset).toString("HH:mm");
        }
        if (has_token){
            return "GitHub's limit of requests for this token is used up" + until + ".";
        }
        return "GitHub allows 60 requests an hour without a token, and they are used up" + until +
            ". With a GitHub token (in the settings of this panel) the limit is 5,000 an hour.";
    }
    if (response.status == 403){
        if (method != "GET"){
            return "GitHub refused this (" + message + "). The token needs the permission "
                "\"Contents: Read and write\" for this repository.";
        }
        return "GitHub refused this (" + message + ").";
    }
    if (response.status == 404){
        if (method != "GET" && has_token){
            return "GitHub does not find this, or the token has no access to this repository (" + message + ").";
        }
        return "GitHub does not find this (" + message + ").";
    }
    if (response.status == 409){
        return "GitHub reports a conflict: " + message;
    }
    return "GitHub answered with an error (HTTP " + QString::number(response.status) + "): " + message;
}

}



GitHubClient::GitHubClient(HttpTransport& transport, TokenSource token)
    : m_transport(transport)
    , m_token(std::move(token))
    , m_alive(std::make_shared<bool>(true))
{}

GitHubClient::~GitHubClient(){
    *m_alive = false;
}

bool GitHubClient::has_token() const{
    return m_token && !m_token().trimmed().isEmpty();
}

QString GitHubClient::encode_path(const QString& path){
    QStringList parts;
    for (const QString& part : path.split('/', Qt::SkipEmptyParts)){
        parts += QString::fromLatin1(QUrl::toPercentEncoding(part));
    }
    return parts.join('/');
}

std::vector<HttpHeader> GitHubClient::headers(const char* accept) const{
    std::vector<HttpHeader> headers{
        {"accept", accept},
        {"x-github-api-version", "2022-11-28"},
        //  GitHub refuses requests without one.
        {"user-agent", "PocketPilot"},
    };
    const QString token = m_token ? m_token().trimmed() : QString();
    if (!token.isEmpty()){
        headers.push_back(HttpHeader{"authorization", "Bearer " + token.toUtf8()});
    }
    return headers;
}

void GitHubClient::send(const HttpRequest& request, bool json, Done done){
    const std::shared_ptr<bool> alive = m_alive;
    const QByteArray method = request.method;
    const bool has_token_now = has_token();
    m_transport.send(
        request, nullptr,
        [alive, method, has_token_now, json, done](const HttpResponse& response){
            if (!*alive || !done){
                return;
            }
            GitHubReply reply;
            reply.status = response.status;
            reply.body = response.body;
            reply.error = error_text(response, method, has_token_now);
            if (json && reply.error.isEmpty()){
                reply.json = QJsonDocument::fromJson(response.body);
            }
            done(reply);
        }
    );
}

void GitHubClient::call(const QByteArray& method, const QString& path, const QJsonObject& body, Done done){
    HttpRequest request;
    request.method = method;
    request.url = QUrl(QString::fromLatin1(API_HOST) + path);
    request.headers = headers(ACCEPT_JSON);
    if (method != "GET" && method != "DELETE"){
        request.headers.push_back(HttpHeader{"content-type", "application/json"});
        request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    }
    request.idle_timeout_ms = 60000;
    send(request, true, std::move(done));
}

void GitHubClient::file(const Repository& repository, const QString& commit, const QString& path, Done done){
    //  Without a token from the host for raw files: it does not count
    //  against the 60 requests an hour the API allows without one.
    HttpRequest raw;
    raw.url = QUrl(
        QString::fromLatin1(RAW_HOST) + "/" + repository.slug() + "/" + commit + "/" + encode_path(path)
    );
    raw.headers = {{"user-agent", "PocketPilot"}};
    raw.follow_redirects = true;
    if (!has_token()){
        send(raw, false, std::move(done));
        return;
    }
    HttpRequest request;
    request.url = QUrl(
        QString::fromLatin1(API_HOST) + "/repos/" + repository.slug() + "/contents/" + encode_path(path) +
        "?ref=" + commit
    );
    request.headers = headers(ACCEPT_RAW);
    //  A token for another repository: a public one can still be read.
    const std::shared_ptr<bool> alive = m_alive;
    send(request, false, [this, alive, raw, done](const GitHubReply& reply){
        if (!*alive){
            return;
        }
        if (reply.status == 401 || reply.status == 403 || reply.status == 404){
            send(raw, false, done);
            return;
        }
        done(reply);
    });
}

void GitHubClient::job_log(const Repository& repository, qint64 job, Done done){
    if (!has_token()){
        QTimer::singleShot(0, &m_context, [done]{
            GitHubReply reply;
            reply.error = "Reading the log of a build needs a GitHub token (in the settings of this panel).";
            done(reply);
        });
        return;
    }
    HttpRequest request;
    request.url = QUrl(
        QString::fromLatin1(API_HOST) + "/repos/" + repository.slug() + "/actions/jobs/" +
        QString::number(job) + "/logs"
    );
    request.headers = headers(ACCEPT_JSON);
    //  The log is elsewhere: GitHub answers with where. The token does not
    //  go along to that place.
    request.follow_redirects = false;
    const std::shared_ptr<bool> alive = m_alive;
    m_transport.send(
        request, nullptr,
        [this, alive, done](const HttpResponse& response){
            if (!*alive){
                return;
            }
            const QByteArray location = response.header("location");
            if (response.status >= 300 && response.status < 400 && !location.isEmpty()){
                HttpRequest log;
                log.url = QUrl(QString::fromUtf8(location));
                log.headers = {{"user-agent", "PocketPilot"}};
                log.follow_redirects = true;
                log.idle_timeout_ms = 60000;
                send(log, false, done);
                return;
            }
            GitHubReply reply;
            reply.status = response.status;
            reply.body = response.body;
            reply.error = error_text(response, "GET", true);
            done(reply);
        }
    );
}



}
}
}

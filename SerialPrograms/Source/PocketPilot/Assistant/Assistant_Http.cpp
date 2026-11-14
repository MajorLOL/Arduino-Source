/*  PocketPilot Assistant: HTTP
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <utility>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include "Assistant_Http.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


QByteArray HttpResponse::header(const char* name) const{
    for (const HttpHeader& entry : headers){
        if (entry.name == name){
            return entry.value;
        }
    }
    return QByteArray();
}


namespace{

class NetworkCall : public HttpCall{
public:
    virtual void abort() override{
        if (m_aborted){
            return;
        }
        m_aborted = true;
        //  (abort() emits "finished": the handler below sees m_aborted.)
        if (!m_reply.isNull()){
            QNetworkReply* reply = m_reply.data();
            m_reply.clear();
            reply->abort();
            reply->deleteLater();
        }
    }

    bool m_aborted = false;
    QPointer<QNetworkReply> m_reply;
};


class NetworkTransport : public HttpTransport{
public:
    virtual std::shared_ptr<HttpCall> send(
        const HttpRequest& request,
        DataHandler on_data,
        DoneHandler on_done
    ) override{
        QNetworkRequest network_request(request.url);
        for (const HttpHeader& header : request.headers){
            network_request.setRawHeader(header.name, header.value);
        }
        network_request.setTransferTimeout(request.idle_timeout_ms);
        network_request.setAttribute(
            QNetworkRequest::RedirectPolicyAttribute,
            request.follow_redirects
                ? QNetworkRequest::NoLessSafeRedirectPolicy
                : QNetworkRequest::ManualRedirectPolicy
        );

        std::shared_ptr<NetworkCall> call = std::make_shared<NetworkCall>();
        QNetworkReply* reply = m_manager.sendCustomRequest(network_request, request.method, request.body);
        call->m_reply = reply;

        const auto successful = [](const QNetworkReply& answer){
            const int status = answer.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            return status >= 200 && status < 300;
        };

        //  The reply is the context of both connections: they end with it.
        QObject::connect(
            reply, &QNetworkReply::readyRead,
            reply, [call, reply, on_data, successful]{
                if (call->m_aborted || !on_data || !successful(*reply)){
                    return;
                }
                const QByteArray piece = reply->readAll();
                if (!piece.isEmpty()){
                    on_data(piece);
                }
            }
        );
        QObject::connect(
            reply, &QNetworkReply::finished,
            reply, [call, reply, on_data, on_done, successful]{
                if (call->m_aborted){
                    return;
                }
                call->m_reply.clear();

                HttpResponse response;
                response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                for (const QNetworkReply::RawHeaderPair& pair : reply->rawHeaderPairs()){
                    response.headers.push_back(HttpHeader{pair.first.toLower(), pair.second});
                }
                //  (Reading a reply whose connection failed only makes Qt
                //  warn that it is not open.)
                const QByteArray rest = reply->isOpen() ? reply->readAll() : QByteArray();
                if (on_data && successful(*reply)){
                    if (!rest.isEmpty()){
                        on_data(rest);
                    }
                }else{
                    response.body = rest;
                }
                //  An answer with an error status is an answer. No answer,
                //  or one that broke off, is an error of the request.
                if (reply->error() != QNetworkReply::NoError && response.status < 400){
                    response.error = reply->errorString();
                }
                reply->deleteLater();
                if (on_done){
                    on_done(response);
                }
            }
        );
        return call;
    }

private:
    QNetworkAccessManager m_manager;
};

}


std::unique_ptr<HttpTransport> make_network_transport(){
    return std::make_unique<NetworkTransport>();
}



}
}
}

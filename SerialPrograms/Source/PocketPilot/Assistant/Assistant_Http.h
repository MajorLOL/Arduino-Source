/*  PocketPilot Assistant: HTTP
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The one place where the assistant talks to the network: a request goes
 *  out, its answer comes back through the event loop. The Claude API and
 *  the GitHub API are both used through this, so that they can be tested
 *  against made-up answers (the interface test of the builds does that:
 *  it has no key and must not use the network).
 *
 *  Nothing here logs anything: a request carries the user's API key or
 *  token in its headers.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Assistant_Http_H
#define PokemonAutomation_PocketPilot_Assistant_Http_H

#include <functional>
#include <memory>
#include <vector>
#include <QByteArray>
#include <QString>
#include <QUrl>

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


struct HttpHeader{
    QByteArray name;
    QByteArray value;
};

struct HttpRequest{
    QByteArray method = "GET";
    QUrl url;
    std::vector<HttpHeader> headers;
    QByteArray body;
    //  The request fails when nothing was received for this long.
    int idle_timeout_ms = 60000;
    //  Off: a redirect comes back as its status (301, 302...) with its
    //  "location" header, so that the caller decides what to send there.
    //  (The headers of a request are sent along with a redirect that is
    //  followed, a token among them.)
    bool follow_redirects = false;
};

struct HttpResponse{
    //  The HTTP status. 0: there was no answer, "error" says why.
    int status = 0;
    //  Why there was no answer, or why it broke off before its end (the
    //  status and what was received so far are still there).
    QString error;
    //  The names in lower case.
    std::vector<HttpHeader> headers;
    //  The body, as far as it was not handed out piece by piece.
    QByteArray body;

    //  A header's value. Empty if it is not there. "name" in lower case.
    QByteArray header(const char* name) const;
    bool ok() const{ return status >= 200 && status < 300 && error.isEmpty(); }
};


//  A request on its way.
class HttpCall{
public:
    virtual ~HttpCall() = default;
    //  Stops the request. None of its handlers is called after this.
    virtual void abort() = 0;
};


class HttpTransport{
public:
    virtual ~HttpTransport() = default;

    //  The pieces of the body of a successful answer (status 2xx), as they
    //  come in.
    using DataHandler = std::function<void(const QByteArray& piece)>;
    using DoneHandler = std::function<void(const HttpResponse& response)>;

    //  Sends a request. Without "on_data" the whole body comes with the
    //  response. The handlers are called from the event loop of the thread
    //  that sent the request, never from inside send(); "on_done" once,
    //  unless the call is aborted first.
    virtual std::shared_ptr<HttpCall> send(
        const HttpRequest& request,
        DataHandler on_data,
        DoneHandler on_done
    ) = 0;
};


//  The network, with Qt.
std::unique_ptr<HttpTransport> make_network_transport();



}
}
}
#endif

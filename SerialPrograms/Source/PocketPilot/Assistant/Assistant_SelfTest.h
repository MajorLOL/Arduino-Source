/*  PocketPilot Assistant: Self Test
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  For the test of the interface that every build runs
 *  (PocketPilot_SelfTest.cpp): the build machine has no API key and must
 *  not use one, so the test answers for Claude and for GitHub itself.
 *
 *  -   A made-up Claude and GitHub behind the same connection the real ones
 *      are reached through. Everything in front of it is the real thing:
 *      the conversation, the tools, the commit to a branch, the panel.
 *  -   Checks of the parts that need no connection at all: reading the
 *      API's stream, the prices, line ends, the keys' file.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Assistant_SelfTest_H
#define PokemonAutomation_PocketPilot_Assistant_SelfTest_H

#include <functional>
#include <memory>
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include "Assistant_Http.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


//  A made-up Claude and GitHub.
class TestBackend{
public:
    TestBackend();
    ~TestBackend();
    TestBackend(const TestBackend&) = delete;
    void operator=(const TestBackend&) = delete;

    //  A connection that this backend answers. The backend has to stay
    //  for as long as the connection is used.
    std::unique_ptr<HttpTransport> make_transport();

    //  What Claude answers to the requests that come, in this order: a
    //  tool call (with some text in front of it, if given), or an answer.
    void claude_calls(const QString& tool, const QJsonObject& input, const QString& text_before = QString());
    void claude_says(const QString& text);

    //  The repository's base branch and a source file on it.
    static const char* const BASE_BRANCH;
    static const char* const SOURCE_FILE;
    //  The state of the made-up GitHub.
    bool has_branch(const QString& branch) const;
    QByteArray file_on(const QString& branch, const QString& path) const;
    int requests_to_claude() const;

    //  What it holds: shared with its connections.
    struct Data;

private:
    std::shared_ptr<Data> m_data;
};


//  The checks that need no connection. "report" gets a line for each
//  check that failed. Returns how many checks were made and how many
//  failed. "folder": a folder to write a file with made-up keys in.
struct CheckResult{
    int checks = 0;
    int failed = 0;
};
CheckResult run_checks(const QString& folder, const std::function<void(const QString& line)>& report);



}
}
}
#endif

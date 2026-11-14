/*  PocketPilot Assistant: Service
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  Claude in the program: the one place that holds the keys, the settings,
 *  the connection to the Claude API and to GitHub, the tools and the chat's
 *  conversation. The panel shows it; the program that lets Claude play uses
 *  its connection.
 *
 *  It knows nothing of the rest of the program: the main window hands it
 *  what the tools may look at (the programs, the one that is open, the
 *  log), and where the settings are kept.
 *
 *  Everything here lives in the thread of the interface.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Assistant_Service_H
#define PokemonAutomation_PocketPilot_Assistant_Service_H

#include <functional>
#include <map>
#include <memory>
#include <vector>
#include <QString>
#include <QStringList>
#include "Assistant_Agent.h"
#include "Assistant_Claude.h"
#include "Assistant_GitHub.h"
#include "Assistant_Http.h"
#include "Assistant_Settings.h"
#include "Assistant_Tools.h"
#include "Assistant_Workspace.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


struct ProgramInfo{
    QString game;
    QString section;
    QString name;
    QString description;
};

struct OpenProgramInfo{
    //  Nothing is open: the page with the games.
    bool open = false;
    QString game;
    QString name;
    QString description;
    bool running = false;
    //  Its options as it saves them (JSON). Empty for a page without
    //  options to show.
    QString settings;
};

//  What the main window hands over.
struct AppHooks{
    QString name;               //  The program's name.
    QString build;              //  Its build.
    QString upstream;           //  What it is built on, with the version.
    QString settings_folder;    //  With a slash at the end.
    //  Where the program's source is, unless the user sets another place.
    Repository repository;

    std::function<std::vector<ProgramInfo>()> programs;
    std::function<OpenProgramInfo()> open_program;
    //  The last lines of the log, oldest first.
    std::function<QStringList(int lines)> recent_log;
};


class Service{
public:
    //  "transport": the network, unless a test hands in another one.
    explicit Service(AppHooks hooks, std::unique_ptr<HttpTransport> transport = nullptr);
    ~Service();
    Service(const Service&) = delete;
    void operator=(const Service&) = delete;

    //  The one of the running program. Null before the main window made it
    //  and after it is gone.
    static Service* instance();

    const AppHooks& hooks() const{ return m_hooks; }
    SecretStore& secrets(){ return *m_secrets; }
    Settings& settings(){ return m_settings; }
    void save_settings();

    ClaudeClient& claude(){ return *m_claude; }
    GitHubClient& github(){ return *m_github; }
    Workspace& workspace(){ return *m_workspace; }
    Agent& agent(){ return *m_agent; }
    const ToolSet& tools() const{ return m_tools; }

    bool has_api_key() const{ return m_secrets->has(Secret::CLAUDE_API_KEY); }

    //  The repository the tools work on.
    Repository repository() const;

    //  The chat: a turn with what the user wrote, and a new conversation.
    void ask(const QString& text);
    void new_conversation();
    //  One line on where the user is in the program, put in front of each
    //  message for Claude.
    QString context_line() const;

    //  The models to choose from: the known ones, or the API's own list
    //  once refresh_models() got it. An empty error: it worked (also the
    //  test of the key).
    const std::vector<ModelChoice>& models() const{ return m_models; }
    void refresh_models(std::function<void(const QString& error)> done);

    //  For what uses this service's connection from outside (a program in
    //  which Claude plays, Assistant_Play.h): "close" is called when the
    //  service ends, before anything of it is gone.
    void at_end(const void* owner, std::function<void()> close);
    void forget_end(const void* owner);

    //  A commit was made: for the panel, which shows it with its buttons.
    std::function<void(const CommitInfo& info)> on_commit;
    //  The user wants to see the changes of the conversation.
    std::function<void()> on_show_changes;

private:
    AppHooks m_hooks;
    std::unique_ptr<HttpTransport> m_transport;
    std::unique_ptr<SecretStore> m_secrets;
    Settings m_settings;
    std::unique_ptr<ClaudeClient> m_claude;
    std::unique_ptr<GitHubClient> m_github;
    std::unique_ptr<Workspace> m_workspace;
    ToolSet m_tools;
    std::unique_ptr<Agent> m_agent;
    std::vector<ModelChoice> m_models;
    std::map<const void*, std::function<void()>> m_at_end;
    std::shared_ptr<bool> m_alive;
};


//  The instructions of the chat.
QString chat_instructions(const AppHooks& hooks, const Repository& repository);

//  The tools of the chat.
void add_chat_tools(ToolSet& tools, Service& service);



}
}
}
#endif

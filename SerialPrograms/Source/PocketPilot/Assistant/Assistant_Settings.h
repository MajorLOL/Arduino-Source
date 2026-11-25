/*  PocketPilot Assistant: Settings
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  What the assistant keeps between runs, in two files of the settings
 *  folder:
 *
 *  -   The keys (the Claude API key, the GitHub token), in a file of their
 *      own and never in the program's settings or its log. On Windows they
 *      are encrypted with the user's Windows account (the Data Protection
 *      API): the file is of no use on another computer or to another
 *      account. Elsewhere the file is only readable by the user.
 *  -   The choices: the models, the repository, when to ask about the cost.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Assistant_Settings_H
#define PokemonAutomation_PocketPilot_Assistant_Settings_H

#include <QString>

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


enum class Secret{
    CLAUDE_API_KEY,
    GITHUB_TOKEN,
};

class SecretStore{
public:
    //  Reads the file. No file: no keys.
    explicit SecretStore(const QString& file_path);

    bool has(Secret secret) const{ return !get(secret).isEmpty(); }
    const QString& get(Secret secret) const;
    //  Keeps a key and writes the file. An empty value forgets the key.
    //  False if the file could not be written ("error" says why): the key
    //  is then kept until the program closes.
    bool set(Secret secret, const QString& value, QString& error);

    //  A key is in the file that could not be read here: saved by another
    //  Windows account, or on another computer.
    bool unreadable(Secret secret) const;

    //  Whether the keys are kept in the file (the default) or only in memory,
    //  until the program closes. In memory only, nothing of a key is ever
    //  written, so no other program that runs as this user can read one from
    //  a file (the keys of a file are encrypted for the user, and any program
    //  of the same user can ask Windows to decrypt them).
    bool remembers() const{ return m_remember; }
    //  Changes it and writes the file: off, without the keys that are in it
    //  (the ones in memory stay); on, with the keys that are in memory.
    //  False if the file could not be written ("error" says why).
    bool set_remembers(bool remember, QString& error);

    //  How the keys are kept right now, for the user.
    QString protection_text() const;

private:
    bool write(QString& error) const;

private:
    QString m_path;
    QString m_values[2];
    bool m_unreadable[2] = {false, false};
    bool m_remember = true;
};


struct Settings{
    //  The model of the chat. (A program in which Claude plays has the
    //  model among its own options.)
    QString chat_model = "claude-sonnet-5-5";
    //  "low", "medium", "high". Empty: the model's own default.
    QString chat_effort;
    //  Ask whether to go on each time a conversation has cost this much
    //  more (estimated US dollars). 0: never ask.
    double ask_at_usd = 1.0;
    //  The workspace of the Claude API the requests run in ("wrkspc_..."),
    //  for a key that was not made for one workspace: the API wants it named
    //  with each request then. Empty: none is named, as a key of one
    //  workspace needs it.
    QString api_workspace_id;

    //  Where the program's source is, and the branch a change is made for.
    QString repository_owner;
    QString repository_name;
    QString base_branch;

    //  The panel: open, and how wide.
    bool panel_open = false;
    int panel_width = 430;

    //  From the file. What the file does not have stays as it is.
    void load(const QString& file_path);
    bool save(const QString& file_path) const;
};

//  A workspace ID as it can be sent: the letters, the digits, "_" and "-"
//  of what was entered. (It becomes a header of each request.)
QString clean_workspace_id(const QString& entered);



}
}
}
#endif

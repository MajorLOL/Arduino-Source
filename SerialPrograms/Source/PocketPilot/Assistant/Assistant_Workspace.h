/*  PocketPilot Assistant: Workspace
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The program's source as Claude works on it: the files of the repository
 *  on GitHub, the changes of the conversation that are not committed yet,
 *  and the branch they go to.
 *
 *  The rules of a change are kept here, in code, whatever Claude asks for:
 *
 *  -   A commit only ever goes to a branch named "claude/...". The branch
 *      the program is built from is never written to: a change gets there
 *      through a pull request the user opens and merges.
 *  -   The branch starts at the newest commit of the base branch, and the
 *      base branch is merged into it before a later commit when it has
 *      moved on, so that the pull request can be merged.
 *  -   A file that changed on GitHub since Claude read it is not
 *      overwritten: the commit stops and says so.
 *  -   The files of the build's own workflows, and what is not source text,
 *      cannot be changed.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Assistant_Workspace_H
#define PokemonAutomation_PocketPilot_Assistant_Workspace_H

#include <functional>
#include <memory>
#include <vector>
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include "Assistant_GitHub.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


//  A file changed in the conversation, not committed yet.
struct PendingChange{
    QString path;
    //  Not in the repository yet.
    bool created = false;
    //  The file's blob the change was made on. Empty for a new file.
    QString base_blob;
    QString mode = "100644";
    QByteArray before;
    QByteArray after;
};

//  What a commit made.
struct CommitInfo{
    QString branch;
    QString commit;
    QString commit_url;
    //  Where the user opens the pull request.
    QString pull_request_url;
    bool new_branch = false;
    QStringList files;
    QString message;
};


//  A text file's lines without their line ends, and how to write it back
//  as it was: the kind of line end, the mark in front.
struct TextFile{
    bool valid = true;      //  It is text (UTF-8).
    bool bom = false;
    bool crlf = false;
    bool final_newline = true;
    QStringList lines;

    static TextFile from_bytes(const QByteArray& bytes);
    QByteArray to_bytes() const;
};

//  The lines that differ between two texts, with a few lines around them,
//  as "-" and "+" lines under "@@" headers.
QString diff_text(const QByteArray& before, const QByteArray& after);

//  "Fix the party menu!" as part of a branch's name: "fix-the-party-menu".
QString branch_slug(const QString& topic);

//  Why a path cannot be changed. Empty: it can.
QString why_not_writable(const QString& path);

//  The lines of a build's log that say what went wrong, at most "limit".
QStringList build_errors(const QByteArray& log, int limit);


class Workspace{
public:
    //  What comes back: text for Claude, or why it did not work.
    using Done = std::function<void(const QString& text, const QString& error)>;

    Workspace(GitHubClient& github, std::function<Repository()> repository);
    ~Workspace();

    //  A new conversation: no branch, no changes.
    void reset();

    //  The branch of the conversation. Empty until its first commit.
    const QString& branch() const{ return m_branch; }
    const std::vector<PendingChange>& changes() const{ return m_changes; }
    //  Where the user opens the pull request for the branch. Empty without
    //  a branch.
    QString pull_request_url() const;

    //  Reading. All of them see the files as they are with the changes of
    //  the conversation.
    void find_files(const QString& words, const QString& prefix, Done done);
    void read_file(const QString& path, int first_line, int last_line, Done done);
    void outline_file(const QString& path, Done done);
    void search_text(
        const QString& pattern, const QString& prefix, bool regular_expression,
        int limit, std::function<void(const QString& progress)> progress, Done done
    );

    //  Changing. Nothing leaves this computer before commit().
    void edit_file(const QString& path, const QString& old_text, const QString& new_text, Done done);
    void write_file(const QString& path, const QString& content, bool overwrite, Done done);
    //  Takes a change back ("path" empty: all of them). Says what it did.
    QString discard(const QString& path);
    //  The changes, one line each. Empty without changes.
    QString changes_summary() const;
    //  The changes as "-" and "+" lines, for the user to look at.
    QString changes_diff() const;

    //  What has to be right before the user is asked about a commit: a
    //  token, changes, new files listed for the build. Empty error: ask.
    void check_commit(const QString& topic, const QString& message, Done done);
    //  The branch a commit with this topic goes to.
    QString commit_branch(const QString& topic) const;
    //  Commits the changes. "stopped" is checked before anything is written.
    void commit(
        const QString& topic, const QString& message, std::shared_ptr<bool> stopped,
        std::function<void(const CommitInfo* info, const QString& error)> done
    );

    //  The branch against the base branch, its pull request, its build.
    void branch_status(Done done);
    //  The "claude/..." branches of the repository. With "use": the
    //  conversation goes on on that one.
    void branches(const QString& use, Done done);

private:
    struct Entry{
        QString blob;
        qint64 size = 0;
        QString mode;
    };
    struct CommitJob;
    struct FetchBatch;

    Repository repository() const;
    QString working_ref() const;
    void resolve(const QString& branch, bool fresh, std::function<void(const QString& commit, const QString& error)> done);
    void load_tree(const QString& commit, std::function<void(const QString& error)> done);
    void with_tree(std::function<void(const QString& error)> done);
    void fetch(const QString& path, std::function<void(const QByteArray* bytes, const QString& error)> done);
    void fetch_many(
        const QStringList& paths, std::function<void(const QString& progress)> progress,
        std::function<void(const QString& error)> done
    );
    void fetch_pump(std::shared_ptr<FetchBatch> batch);
    const PendingChange* change_of(const QString& path) const;
    PendingChange* change_of(const QString& path);
    //  The bytes of a file as the conversation has it. Fetched before.
    bool current_bytes(const QString& path, QByteArray& bytes) const;
    void commit_step(std::shared_ptr<CommitJob> job);
    void later(std::function<void()> step);

private:
    GitHubClient& m_github;
    std::function<Repository()> m_repository;

    QString m_branch;
    std::vector<PendingChange> m_changes;

    //  The newest commit of a branch, as far as known, and when it was
    //  asked for (milliseconds since the epoch).
    struct Head{
        QString commit;
        qint64 asked = 0;
    };
    QHash<QString, Head> m_heads;
    //  The files of one commit.
    QString m_tree_commit;
    QString m_tree_of;      //  The repository it is of.
    QHash<QString, Entry> m_tree;
    QStringList m_paths;    //  Sorted.
    //  File contents by blob.
    QHash<QString, QByteArray> m_blobs;

    //  Counts conversations: what comes back for an older one is dropped.
    quint64 m_generation = 0;
    std::shared_ptr<bool> m_alive;
    QObject m_context;
};



}
}
}
#endif

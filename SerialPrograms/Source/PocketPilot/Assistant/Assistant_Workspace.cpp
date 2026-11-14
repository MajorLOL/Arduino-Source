/*  PocketPilot Assistant: Workspace
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <algorithm>
#include <utility>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QTimer>
#include <QUrl>
#include "Assistant_Workspace.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


namespace{

//  A branch's newest commit is asked for again after this long.
const qint64 HEAD_AGE_MS = 60000;
//  The largest file that can be read, and the largest that can be changed.
const qint64 MAX_READ_BYTES = 3000000;
const qint64 MAX_WRITE_BYTES = 1000000;
//  Lines of a file in one answer.
const int MAX_READ_LINES = 400;
const int MAX_LINE_CHARACTERS = 600;
//  A search: files at once, their size together, and the size of one.
const int MAX_SEARCH_FILES = 700;
const qint64 MAX_SEARCH_BYTES = 16000000;
const qint64 MAX_SEARCH_FILE_BYTES = 400000;
//  Files of one commit.
const int MAX_COMMIT_FILES = 40;
//  Files fetched at the same time.
const int FETCHES_AT_ONCE = 6;

const char* const SOURCE_LIST = "SerialPrograms/cmake/SourceFiles.cmake";

QString extension_of(const QString& path){
    const qsizetype slash = path.lastIndexOf('/');
    const qsizetype dot = path.lastIndexOf('.');
    if (dot <= slash + 1){
        return QString();
    }
    return path.mid(dot + 1).toLower();
}

bool is_cpp_source(const QString& path){
    const QString extension = extension_of(path);
    return extension == "cpp" || extension == "h" || extension == "tpp" || extension == "hpp" || extension == "c";
}

//  Files that are not text: not searched.
bool is_text_path(const QString& path){
    static const QSet<QString> BINARY{
        "png", "jpg", "jpeg", "gif", "ico", "icns", "bmp", "webp", "ttf", "otf", "woff", "woff2",
        "zip", "7z", "gz", "tar", "dll", "lib", "a", "so", "dylib", "exe", "bin", "obj", "o", "pdb",
        "onnx", "pb", "wav", "mp3", "mp4", "pdf", "hex", "uf2", "dat",
    };
    return !BINARY.contains(extension_of(path));
}

//  The line ends of a new file: as the files around it have them.
bool new_file_is_crlf(const QString& path){
    return path.startsWith("SerialPrograms/Source/") ||
        path.startsWith("SerialPrograms/cmake/") ||
        path.startsWith("Common/");
}

QString size_text(qint64 bytes){
    if (bytes < 1000){
        return QString::number(bytes) + " B";
    }
    if (bytes < 1000000){
        return QString::number((double)bytes / 1000.0, 'f', 1) + " kB";
    }
    return QString::number((double)bytes / 1000000.0, 'f', 1) + " MB";
}

int count_of(const QByteArray& text, const QByteArray& part, qsizetype* first = nullptr){
    int count = 0;
    qsizetype at = 0;
    while (!part.isEmpty()){
        at = text.indexOf(part, at);
        if (at < 0){
            break;
        }
        if (count == 0 && first != nullptr){
            *first = at;
        }
        count++;
        at += part.size();
    }
    return count;
}

QByteArray with_lf(const QString& text){
    QString copy = text;
    copy.replace("\r\n", "\n");
    return copy.toUtf8();
}

QByteArray lf_to_crlf(const QByteArray& text){
    QByteArray copy = text;
    copy.replace("\n", "\r\n");
    return copy;
}

QString normal_path(const QString& path){
    QString copy = path.trimmed();
    copy.replace('\\', '/');
    while (copy.startsWith("./")){
        copy.remove(0, 2);
    }
    while (copy.startsWith('/')){
        copy.remove(0, 1);
    }
    return copy;
}

//  A change that leaves a file as it was is no change.
void remove_unchanged(std::vector<PendingChange>& changes){
    changes.erase(
        std::remove_if(changes.begin(), changes.end(), [](const PendingChange& change){
            return !change.created && change.after == change.before;
        }),
        changes.end()
    );
}

//  A branch this program may write to.
bool is_claude_branch(const QString& branch){
    static const QRegularExpression PATTERN("^claude/[A-Za-z0-9][A-Za-z0-9._-]{0,80}$");
    return PATTERN.match(branch).hasMatch() && !branch.contains("..") && !branch.endsWith(".lock");
}

}



//  Text.

TextFile TextFile::from_bytes(const QByteArray& bytes){
    TextFile file;
    QByteArray data = bytes;
    if (data.startsWith("\xEF\xBB\xBF")){
        file.bom = true;
        data.remove(0, 3);
    }
    if (data.contains('\0')){
        file.valid = false;
        return file;
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString text = decoder(data);
    if (decoder.hasError()){
        file.valid = false;
        return file;
    }
    const qsizetype crlf = text.count("\r\n");
    const qsizetype lf = text.count('\n') - crlf;
    file.crlf = crlf > 0 && crlf >= lf;
    text.replace("\r\n", "\n");
    file.final_newline = text.isEmpty() || text.endsWith('\n');
    if (!text.isEmpty()){
        file.lines = text.split('\n');
        if (text.endsWith('\n')){
            file.lines.removeLast();
        }
    }
    return file;
}

QByteArray TextFile::to_bytes() const{
    const QString end = crlf ? "\r\n" : "\n";
    QString text = lines.join(end);
    if (final_newline && !lines.isEmpty()){
        text += end;
    }
    QByteArray bytes = text.toUtf8();
    if (bom){
        bytes.prepend("\xEF\xBB\xBF");
    }
    return bytes;
}


QString diff_text(const QByteArray& before, const QByteArray& after){
    const TextFile old_file = TextFile::from_bytes(before);
    const TextFile new_file = TextFile::from_bytes(after);
    if (!old_file.valid || !new_file.valid){
        return "(Not text.)\n";
    }
    const QStringList& a = old_file.lines;
    const QStringList& b = new_file.lines;
    const int a_size = (int)a.size();
    const int b_size = (int)b.size();

    //  What both have at their start and at their end.
    int prefix = 0;
    while (prefix < a_size && prefix < b_size && a[prefix] == b[prefix]){
        prefix++;
    }
    int suffix = 0;
    while (
        suffix < a_size - prefix && suffix < b_size - prefix &&
        a[a_size - 1 - suffix] == b[b_size - 1 - suffix]
    ){
        suffix++;
    }
    const int n = a_size - prefix - suffix;
    const int m = b_size - prefix - suffix;

    //  The lines in between: '=' in both, '-' only before, '+' only after.
    std::vector<char> middle;
    if ((qint64)n * (qint64)m <= 4000000){
        //  The longest common subsequence, from the end.
        std::vector<int> table((size_t)(n + 1) * (size_t)(m + 1), 0);
        const auto cell = [&](int i, int j) -> int&{
            return table[(size_t)i * (size_t)(m + 1) + (size_t)j];
        };
        for (int i = n - 1; i >= 0; i--){
            for (int j = m - 1; j >= 0; j--){
                if (a[prefix + i] == b[prefix + j]){
                    cell(i, j) = cell(i + 1, j + 1) + 1;
                }else{
                    cell(i, j) = std::max(cell(i + 1, j), cell(i, j + 1));
                }
            }
        }
        int i = 0;
        int j = 0;
        while (i < n && j < m){
            if (a[prefix + i] == b[prefix + j]){
                middle.push_back('=');
                i++;
                j++;
            }else if (cell(i + 1, j) >= cell(i, j + 1)){
                middle.push_back('-');
                i++;
            }else{
                middle.push_back('+');
                j++;
            }
        }
        for (; i < n; i++){
            middle.push_back('-');
        }
        for (; j < m; j++){
            middle.push_back('+');
        }
    }else{
        //  Too much to compare line by line: the old lines, then the new.
        middle.insert(middle.end(), (size_t)n, '-');
        middle.insert(middle.end(), (size_t)m, '+');
    }

    std::vector<char> ops;
    ops.reserve((size_t)prefix + middle.size() + (size_t)suffix);
    ops.insert(ops.end(), (size_t)prefix, '=');
    ops.insert(ops.end(), middle.begin(), middle.end());
    ops.insert(ops.end(), (size_t)suffix, '=');

    //  The changed lines with three lines around them.
    const int CONTEXT = 3;
    const int count = (int)ops.size();
    QString out;
    int at = 0;
    int a_line = 0;
    int b_line = 0;
    while (at < count){
        if (ops[(size_t)at] == '='){
            at++;
            a_line++;
            b_line++;
            continue;
        }
        //  The hunk: from three lines before this change to three lines
        //  after the last change that is close to it.
        int start = std::max(0, at - CONTEXT);
        int end = at;
        int same = 0;
        for (int scan = at; scan < count; scan++){
            if (ops[(size_t)scan] == '='){
                same++;
                if (same > 2 * CONTEXT){
                    break;
                }
            }else{
                same = 0;
                end = scan + 1;
            }
        }
        end = std::min(count, end + CONTEXT);
        const int back = at - start;
        int a_start = a_line - back;
        int b_start = b_line - back;
        int a_count = 0;
        int b_count = 0;
        QString body;
        int a_at = a_start;
        int b_at = b_start;
        for (int index = start; index < end; index++){
            const char op = ops[(size_t)index];
            if (op == '='){
                body += " " + a[a_at] + "\n";
                a_at++;
                b_at++;
                a_count++;
                b_count++;
            }else if (op == '-'){
                body += "-" + a[a_at] + "\n";
                a_at++;
                a_count++;
            }else{
                body += "+" + b[b_at] + "\n";
                b_at++;
                b_count++;
            }
        }
        out += "@@ -" + QString::number(a_count == 0 ? a_start : a_start + 1) + "," + QString::number(a_count) +
            " +" + QString::number(b_count == 0 ? b_start : b_start + 1) + "," + QString::number(b_count) + " @@\n";
        out += body;
        a_line = a_at;
        b_line = b_at;
        at = end;
    }
    if (out.isEmpty()){
        //  The same lines: only the line ends or the mark in front differ.
        out = before == after ? "(No change.)\n" : "(Only the line ends changed.)\n";
    }
    return out;
}


QString branch_slug(const QString& topic){
    QString slug;
    bool dash = false;
    for (const QChar character : topic.toLower()){
        const ushort code = character.unicode();
        if ((code >= 'a' && code <= 'z') || (code >= '0' && code <= '9')){
            if (dash && !slug.isEmpty()){
                slug += '-';
            }
            dash = false;
            slug += character;
        }else{
            dash = true;
        }
    }
    slug = slug.left(40);
    while (slug.endsWith('-')){
        slug.chop(1);
    }
    return slug.isEmpty() ? QString("change") : slug;
}


QString why_not_writable(const QString& path){
    if (path.isEmpty() || path.size() > 240 || path.startsWith('/') || path.contains('\\') ||
        path.contains("//") || path.endsWith('/'))
    {
        return "\"" + path + "\" is not the path of a file in the repository.";
    }
    for (const QString& part : path.split('/')){
        if (part == ".." || part == "." || part == ".git"){
            return "\"" + path + "\" is not the path of a file in the repository.";
        }
    }
    if (path.startsWith(".github/")){
        return "The workflows that build the program (.github/) cannot be changed from here.";
    }
    if (path.startsWith("3rdParty") || path.startsWith("Packages/")){
        return "\"" + path + "\" is not part of the program's own source: it cannot be changed from here.";
    }
    if (path.endsWith("PokemonFRLG_AutoStoryData.h")){
        return "PokemonFRLG_AutoStoryData.h is generated from the game's map data by the scripts in "
            "SerialPrograms/Scripts/PokemonFRLG/AutoStory/: it cannot be changed by hand, and the scripts "
            "cannot be run from here.";
    }
    static const QSet<QString> TEXT{
        "cpp", "h", "tpp", "hpp", "c", "cmake", "txt", "md", "py", "qss", "qrc", "json", "yml", "yaml",
        "ui", "svg", "html", "css", "js", "sh", "bat", "ps1", "ini", "cfg", "toml", "csv", "qml",
    };
    if (!TEXT.contains(extension_of(path))){
        return "Only source and text files can be changed from here, not \"" + path + "\".";
    }
    return QString();
}


QStringList build_errors(const QByteArray& log, int limit){
    static const QRegularExpression TIMESTAMP("^\\d{4}-\\d{2}-\\d{2}T[\\d:.]+Z\\s?");
    //  Colors of a terminal.
    static const QRegularExpression COLOR("\\x1B\\[[0-9;]*m");
    static const QRegularExpression ERROR_LINE(
        "error C\\d{4}|fatal error|error LNK\\d+|: error:|^FAILED:|warning C\\d{4}|##\\[error\\]|CMake Error|"
        "UI test: (could not|stopped|no game|no dropdowns)|[A-Za-z]+ test: .*(failed|wrong)|"
        "The interface test(?: failed| saved no pictures|.*did not finish|.*exited with code (?!0\\b))"
    );
    //  The build machine's folders in front of the repository's own.
    static const QRegularExpression MACHINE_PATH(
        "[A-Za-z]:[\\\\/](?:[^\\\\/\\s:]+[\\\\/])+?(?=(?:SerialPrograms|Common|3rdParty)[\\\\/])"
    );
    QStringList errors;
    QSet<QString> seen;
    for (const QByteArray& raw : log.split('\n')){
        QString line = QString::fromUtf8(raw).trimmed();
        line.remove(TIMESTAMP);
        //  The lines of a step's own script, which GitHub shows before it
        //  runs them (in cyan): not what happened.
        if (line.startsWith("\x1B[36;1m")){
            continue;
        }
        line.remove(COLOR);
        line = line.trimmed();
        if (line.isEmpty() || !ERROR_LINE.match(line).hasMatch()){
            continue;
        }
        line.remove(MACHINE_PATH);
        if (line.size() > 500){
            line = line.left(500) + "...";
        }
        if (seen.contains(line)){
            continue;
        }
        seen.insert(line);
        if (errors.size() >= limit){
            errors += "(More errors follow in the log.)";
            break;
        }
        errors += line;
    }
    return errors;
}



//  The workspace.

Workspace::Workspace(GitHubClient& github, std::function<Repository()> repository)
    : m_github(github)
    , m_repository(std::move(repository))
    , m_alive(std::make_shared<bool>(true))
{}

Workspace::~Workspace(){
    *m_alive = false;
}

void Workspace::reset(){
    m_generation++;
    m_branch.clear();
    m_changes.clear();
    //  The files read so far stay: a blob never changes.
}

Repository Workspace::repository() const{
    return m_repository ? m_repository() : Repository();
}

QString Workspace::working_ref() const{
    return m_branch.isEmpty() ? repository().base_branch : m_branch;
}

QString Workspace::pull_request_url() const{
    if (m_branch.isEmpty()){
        return QString();
    }
    const Repository repo = repository();
    return repo.web_url() + "/compare/" + repo.base_branch + "..." + m_branch + "?expand=1";
}

void Workspace::later(std::function<void()> step){
    const quint64 generation = m_generation;
    QTimer::singleShot(0, &m_context, [this, generation, step]{
        if (generation == m_generation){
            step();
        }
    });
}

const PendingChange* Workspace::change_of(const QString& path) const{
    for (const PendingChange& change : m_changes){
        if (change.path == path){
            return &change;
        }
    }
    return nullptr;
}
PendingChange* Workspace::change_of(const QString& path){
    for (PendingChange& change : m_changes){
        if (change.path == path){
            return &change;
        }
    }
    return nullptr;
}

bool Workspace::current_bytes(const QString& path, QByteArray& bytes) const{
    if (const PendingChange* change = change_of(path)){
        bytes = change->after;
        return true;
    }
    const auto entry = m_tree.constFind(path);
    if (entry == m_tree.constEnd()){
        return false;
    }
    const auto blob = m_blobs.constFind(entry->blob);
    if (blob == m_blobs.constEnd()){
        return false;
    }
    bytes = *blob;
    return true;
}


//  The newest commit of a branch.
void Workspace::resolve(
    const QString& branch, bool fresh,
    std::function<void(const QString& commit, const QString& error)> done
){
    const Repository repo = repository();
    const QString key = repo.slug() + "#" + branch;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto known = m_heads.constFind(key);
    if (!fresh && known != m_heads.constEnd() && now - known->asked < HEAD_AGE_MS){
        done(known->commit, QString());
        return;
    }
    const std::shared_ptr<bool> alive = m_alive;
    const quint64 generation = m_generation;
    m_github.get(
        "/repos/" + repo.slug() + "/git/ref/heads/" + branch,
        [this, alive, generation, repo, branch, key, done](const GitHubReply& reply){
            if (!*alive || generation != m_generation){
                return;
            }
            if (reply.status == 404){
                done(QString(), "The repository " + repo.slug() + " has no branch \"" + branch + "\".");
                return;
            }
            if (!reply.ok()){
                done(QString(), reply.error);
                return;
            }
            const QString commit = reply.object().value("object").toObject().value("sha").toString();
            if (commit.isEmpty()){
                done(QString(), "GitHub did not say where the branch \"" + branch + "\" is.");
                return;
            }
            Head head;
            head.commit = commit;
            head.asked = QDateTime::currentMSecsSinceEpoch();
            m_heads[key] = head;
            done(commit, QString());
        }
    );
}

//  The files of a commit.
void Workspace::load_tree(const QString& commit, std::function<void(const QString& error)> done){
    const Repository repo = repository();
    if (m_tree_commit == commit && m_tree_of == repo.slug()){
        done(QString());
        return;
    }
    const std::shared_ptr<bool> alive = m_alive;
    const quint64 generation = m_generation;
    m_github.get(
        "/repos/" + repo.slug() + "/git/trees/" + commit + "?recursive=1",
        [this, alive, generation, repo, commit, done](const GitHubReply& reply){
            if (!*alive || generation != m_generation){
                return;
            }
            if (!reply.ok()){
                done(reply.error);
                return;
            }
            QHash<QString, Entry> tree;
            QStringList paths;
            for (const QJsonValue& value : reply.object().value("tree").toArray()){
                const QJsonObject object = value.toObject();
                if (object.value("type").toString() != "blob"){
                    continue;
                }
                Entry entry;
                entry.blob = object.value("sha").toString();
                entry.size = (qint64)object.value("size").toDouble();
                entry.mode = object.value("mode").toString();
                const QString path = object.value("path").toString();
                tree.insert(path, entry);
                paths += path;
            }
            if (paths.isEmpty()){
                done("GitHub listed no files for the repository " + repo.slug() + ".");
                return;
            }
            paths.sort();
            m_tree = std::move(tree);
            m_paths = std::move(paths);
            m_tree_commit = commit;
            m_tree_of = repo.slug();
            done(QString());
        }
    );
}

void Workspace::with_tree(std::function<void(const QString& error)> done){
    const Repository repo = repository();
    if (!repo.valid()){
        done("No repository is set. Set the repository of this program in the settings of this panel.");
        return;
    }
    resolve(working_ref(), false, [this, done](const QString& commit, const QString& error){
        if (!error.isEmpty()){
            done(error);
            return;
        }
        load_tree(commit, done);
    });
}

//  One file's bytes, as the conversation has the file. After with_tree().
void Workspace::fetch(
    const QString& path,
    std::function<void(const QByteArray* bytes, const QString& error)> done
){
    QByteArray bytes;
    if (current_bytes(path, bytes)){
        done(&bytes, QString());
        return;
    }
    const auto entry = m_tree.constFind(path);
    if (entry == m_tree.constEnd()){
        done(nullptr, "There is no file \"" + path + "\".");
        return;
    }
    if (entry->size > MAX_READ_BYTES){
        done(nullptr, "\"" + path + "\" is " + size_text(entry->size) + ": too large to read here.");
        return;
    }
    const QString blob = entry->blob;
    const std::shared_ptr<bool> alive = m_alive;
    const quint64 generation = m_generation;
    m_github.file(
        repository(), m_tree_commit, path,
        [this, alive, generation, blob, done](const GitHubReply& reply){
            if (!*alive || generation != m_generation){
                return;
            }
            if (!reply.ok()){
                done(nullptr, reply.error);
                return;
            }
            m_blobs.insert(blob, reply.body);
            done(&reply.body, QString());
        }
    );
}

//  Fetches the files that are not here yet, a few at a time.
struct Workspace::FetchBatch{
    QStringList paths;
    qsizetype next = 0;
    int active = 0;
    int fetched = 0;
    QString error;
    bool finished = false;
    quint64 generation = 0;
    std::function<void(const QString& progress)> progress;
    std::function<void(const QString& error)> done;
};

void Workspace::fetch_many(
    const QStringList& paths, std::function<void(const QString& progress)> progress,
    std::function<void(const QString& error)> done
){
    std::shared_ptr<FetchBatch> batch = std::make_shared<FetchBatch>();
    for (const QString& path : paths){
        QByteArray bytes;
        if (!current_bytes(path, bytes)){
            batch->paths += path;
        }
    }
    batch->generation = m_generation;
    batch->progress = std::move(progress);
    batch->done = std::move(done);
    fetch_pump(batch);
}

//  Starts fetches of a batch until enough are under way, and ends the
//  batch when none is left. (The fetches under way keep the batch.)
void Workspace::fetch_pump(std::shared_ptr<FetchBatch> batch){
    if (batch->finished || batch->generation != m_generation){
        return;
    }
    const std::shared_ptr<bool> alive = m_alive;
    while (batch->error.isEmpty() && batch->active < FETCHES_AT_ONCE && batch->next < batch->paths.size()){
        const QString path = batch->paths[batch->next++];
        batch->active++;
        //  (Each answer comes from the event loop: a file that is not here
        //  is asked for over the network.)
        fetch(path, [this, alive, batch](const QByteArray*, const QString& error){
            if (!*alive){
                return;
            }
            batch->active--;
            batch->fetched++;
            if (!error.isEmpty() && batch->error.isEmpty()){
                batch->error = error;
            }
            if (batch->progress && batch->paths.size() > 3 && batch->fetched % 5 == 0){
                batch->progress(
                    "Fetched " + QString::number(batch->fetched) + " of " +
                    QString::number(batch->paths.size()) + " files"
                );
            }
            fetch_pump(batch);
        });
    }
    const bool no_more = batch->next >= batch->paths.size() || !batch->error.isEmpty();
    if (batch->active == 0 && no_more){
        batch->finished = true;
        batch->done(batch->error);
    }
}



//  Reading.

void Workspace::find_files(const QString& words, const QString& prefix, Done done){
    const std::shared_ptr<bool> alive = m_alive;
    with_tree([this, alive, words, prefix, done](const QString& error){
        if (!*alive){
            return;
        }
        if (!error.isEmpty()){
            done(QString(), error);
            return;
        }
        const QStringList wanted = words.toLower().split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
        const QString under = normal_path(prefix);
        //  The files of the repository, and the new ones of the conversation.
        QStringList all = m_paths;
        for (const PendingChange& change : m_changes){
            if (change.created){
                all += change.path;
            }
        }
        const int LIMIT = 60;
        QStringList found;
        int more = 0;
        for (const QString& path : all){
            if (!under.isEmpty() && !path.startsWith(under, Qt::CaseInsensitive)){
                continue;
            }
            const QString lower = path.toLower();
            bool match = true;
            for (const QString& word : wanted){
                if (!lower.contains(word)){
                    match = false;
                    break;
                }
            }
            if (!match){
                continue;
            }
            if (found.size() >= LIMIT){
                more++;
                continue;
            }
            const PendingChange* change = change_of(path);
            const qint64 size = change != nullptr ? change->after.size() : m_tree.value(path).size;
            found += path + " (" + size_text(size) + (change != nullptr ? ", changed here" : "") + ")";
        }
        if (found.isEmpty()){
            done(
                "No file" + (under.isEmpty() ? QString() : " under \"" + under + "\"") +
                " has " + (wanted.size() == 1 ? "this word" : "all of these words") + " in its path. "
                "Try fewer or other words: a program's files carry its name without spaces "
                "(\"Level Trainer\" is in PokemonFRLG_LevelTrainer.cpp).",
                QString()
            );
            return;
        }
        QString text = found.join("\n");
        if (more > 0){
            text += "\n(" + QString::number(more) + " more: add a word or a folder to narrow it down.)";
        }
        done(text, QString());
    });
}

void Workspace::read_file(const QString& path_given, int first_line, int last_line, Done done){
    const QString path = normal_path(path_given);
    const std::shared_ptr<bool> alive = m_alive;
    with_tree([this, alive, path, first_line, last_line, done](const QString& tree_error){
        if (!*alive){
            return;
        }
        if (!tree_error.isEmpty()){
            done(QString(), tree_error);
            return;
        }
        if (change_of(path) == nullptr && !m_tree.contains(path)){
            //  Files of that name elsewhere.
            const QString name = path.mid(path.lastIndexOf('/') + 1);
            QStringList close;
            for (const QString& other : m_paths){
                if (other.endsWith("/" + name, Qt::CaseInsensitive) || other.compare(name, Qt::CaseInsensitive) == 0){
                    close += other;
                    if (close.size() == 5){
                        break;
                    }
                }
            }
            done(
                QString(),
                "There is no file \"" + path + "\"." +
                (close.isEmpty()
                    ? QString(" Look for it with find_files.")
                    : " Files of that name: " + close.join(", ") + ".")
            );
            return;
        }
        fetch(path, [this, alive, path, first_line, last_line, done](const QByteArray* bytes, const QString& error){
            if (!*alive){
                return;
            }
            if (bytes == nullptr){
                done(QString(), error);
                return;
            }
            const TextFile file = TextFile::from_bytes(*bytes);
            if (!file.valid){
                done(QString(), "\"" + path + "\" is not a text file.");
                return;
            }
            const int total = (int)file.lines.size();
            int first = std::max(1, first_line);
            if (first > total && total > 0){
                done(QString(), "\"" + path + "\" has " + QString::number(total) + " lines.");
                return;
            }
            int last = last_line >= first ? last_line : total;
            last = std::min(last, total);
            bool cut = false;
            if (last - first + 1 > MAX_READ_LINES){
                last = first + MAX_READ_LINES - 1;
                cut = true;
            }
            QString text = path + ", ";
            if (total == 0){
                text += "empty";
            }else if (first == 1 && last == total){
                text += "all " + QString::number(total) + " lines";
            }else{
                text += "lines " + QString::number(first) + " to " + QString::number(last) + " of " + QString::number(total);
            }
            if (change_of(path) != nullptr){
                text += " (with the changes of this conversation, not committed yet)";
            }
            text += "\n";
            const int width = (int)QString::number(last).size();
            for (int number = first; number <= last; number++){
                QString line = file.lines[number - 1];
                if (line.size() > MAX_LINE_CHARACTERS){
                    line = line.left(MAX_LINE_CHARACTERS) + " [... line cut: " + QString::number(line.size()) + " characters]";
                }
                text += QString::number(number).rightJustified(width) + "| " + line + "\n";
            }
            if (cut){
                text += "(More lines follow: ask for them with first_line " + QString::number(last + 1) + ".)\n";
            }
            done(text, QString());
        });
    });
}

void Workspace::outline_file(const QString& path_given, Done done){
    const QString path = normal_path(path_given);
    const std::shared_ptr<bool> alive = m_alive;
    with_tree([this, alive, path, done](const QString& tree_error){
        if (!*alive){
            return;
        }
        if (!tree_error.isEmpty()){
            done(QString(), tree_error);
            return;
        }
        fetch(path, [alive, path, done](const QByteArray* bytes, const QString& error){
            if (!*alive){
                return;
            }
            if (bytes == nullptr){
                done(QString(), error);
                return;
            }
            const TextFile file = TextFile::from_bytes(*bytes);
            if (!file.valid){
                done(QString(), "\"" + path + "\" is not a text file.");
                return;
            }
            //  The lines that start something: a class, a function, a
            //  section of a document.
            static const QRegularExpression TYPE("^\\s*(class|struct|enum|namespace|union)\\b[^;]*$");
            static const QRegularExpression FUNCTION("^[A-Za-z_~][^;=]*\\([^;]*$");
            static const QRegularExpression SKIP("^(if|for|while|switch|return|else|do|case|using|typedef|static_assert)\\b");
            static const QRegularExpression PYTHON("^\\s*(def|class)\\s+\\w+");
            static const QRegularExpression HEADING("^#{1,4}\\s+\\S");
            const QString extension = extension_of(path);
            const bool python = extension == "py";
            const bool document = extension == "md";
            const int LIMIT = 220;
            QStringList found;
            const int total = (int)file.lines.size();
            const int width = (int)QString::number(total).size();
            int more = 0;
            for (int number = 1; number <= total; number++){
                const QString& line = file.lines[number - 1];
                bool take;
                if (python){
                    take = PYTHON.match(line).hasMatch();
                }else if (document){
                    take = HEADING.match(line).hasMatch();
                }else{
                    take = TYPE.match(line).hasMatch() ||
                        (FUNCTION.match(line).hasMatch() && !SKIP.match(line).hasMatch());
                }
                if (!take){
                    continue;
                }
                if (found.size() >= LIMIT){
                    more++;
                    continue;
                }
                found += QString::number(number).rightJustified(width) + "| " + line.left(160);
            }
            QString text = path + ", " + QString::number(total) + " lines. ";
            if (found.isEmpty()){
                text += "No outline could be made of it: read it with read_file.";
            }else{
                text += "Where its parts start:\n" + found.join("\n");
                if (more > 0){
                    text += "\n(" + QString::number(more) + " more.)";
                }
            }
            done(text, QString());
        });
    });
}

void Workspace::search_text(
    const QString& pattern, const QString& prefix_given, bool regular_expression,
    int limit, std::function<void(const QString& progress)> progress, Done done
){
    if (pattern.isEmpty()){
        done(QString(), "Nothing to search for.");
        return;
    }
    QRegularExpression expression;
    if (regular_expression){
        expression = QRegularExpression(pattern);
        if (!expression.isValid()){
            done(QString(), "Not a valid regular expression: " + expression.errorString() + ".");
            return;
        }
    }
    const QString prefix = normal_path(prefix_given);
    const int most = std::clamp(limit, 1, 100);
    const std::shared_ptr<bool> alive = m_alive;
    with_tree([this, alive, pattern, prefix, regular_expression, expression, most, progress, done](const QString& tree_error){
        if (!*alive){
            return;
        }
        if (!tree_error.isEmpty()){
            done(QString(), tree_error);
            return;
        }
        QStringList all = m_paths;
        for (const PendingChange& change : m_changes){
            if (change.created){
                all += change.path;
            }
        }
        QStringList files;
        QStringList large;
        qint64 bytes = 0;
        for (const QString& path : all){
            if (!prefix.isEmpty() && path != prefix && !path.startsWith(prefix, Qt::CaseInsensitive)){
                continue;
            }
            if (!is_text_path(path)){
                continue;
            }
            const PendingChange* change = change_of(path);
            const qint64 size = change != nullptr ? change->after.size() : m_tree.value(path).size;
            if (size > MAX_SEARCH_FILE_BYTES){
                large += path;
                continue;
            }
            files += path;
            bytes += size;
        }
        if (files.isEmpty()){
            done(
                QString(),
                "There are no text files under \"" + prefix + "\". Give a folder of the repository, such as "
                "SerialPrograms/Source/VideoGames/PokemonFRLG/ (find_files shows where things are)."
            );
            return;
        }
        if (files.size() > MAX_SEARCH_FILES || bytes > MAX_SEARCH_BYTES){
            done(
                QString(),
                QString::number(files.size()) + " files (" + size_text(bytes) + ") are under \"" + prefix +
                "\": too many to search at once. Give a narrower folder, such as one game's or one program's."
            );
            return;
        }
        fetch_many(files, progress, [this, alive, files, large, pattern, regular_expression, expression, most, done](const QString& error){
            if (!*alive){
                return;
            }
            if (!error.isEmpty()){
                done(QString(), error);
                return;
            }
            QStringList found;
            int more = 0;
            for (const QString& path : files){
                QByteArray bytes_of_file;
                if (!current_bytes(path, bytes_of_file)){
                    continue;
                }
                //  A file without the pattern anywhere: not split into lines.
                if (!regular_expression && !bytes_of_file.toLower().contains(pattern.toUtf8().toLower())){
                    continue;
                }
                const TextFile file = TextFile::from_bytes(bytes_of_file);
                if (!file.valid){
                    continue;
                }
                for (int number = 1; number <= (int)file.lines.size(); number++){
                    const QString& line = file.lines[number - 1];
                    const bool match = regular_expression
                        ? expression.match(line).hasMatch()
                        : line.contains(pattern, Qt::CaseInsensitive);
                    if (!match){
                        continue;
                    }
                    if (found.size() >= most){
                        more++;
                        continue;
                    }
                    found += path + ":" + QString::number(number) + ": " + line.trimmed().left(220);
                }
            }
            QString text;
            if (found.isEmpty()){
                text = "Not found in the " + QString::number(files.size()) + " files searched.";
            }else{
                text = found.join("\n");
                if (more > 0){
                    text += "\n(" + QString::number(more) + " more lines match: narrow the folder or the pattern.)";
                }
            }
            if (!large.isEmpty()){
                text += "\n(Not searched, too large: " + large.join(", ") + ".)";
            }
            done(text, QString());
        });
    });
}



//  Changing.

void Workspace::edit_file(const QString& path_given, const QString& old_text, const QString& new_text, Done done){
    const QString path = normal_path(path_given);
    const QString refused = why_not_writable(path);
    if (!refused.isEmpty()){
        done(QString(), refused);
        return;
    }
    if (old_text.isEmpty()){
        done(QString(), "old_text is empty. Give the exact text to replace, or use write_file for a new file.");
        return;
    }
    if (is_cpp_source(path) && new_text.contains('\t')){
        done(QString(), "new_text has tab characters. This repository's sources have none (its build checks): use 4 spaces.");
        return;
    }
    const std::shared_ptr<bool> alive = m_alive;
    with_tree([this, alive, path, old_text, new_text, done](const QString& tree_error){
        if (!*alive){
            return;
        }
        if (!tree_error.isEmpty()){
            done(QString(), tree_error);
            return;
        }
        fetch(path, [this, alive, path, old_text, new_text, done](const QByteArray* fetched, const QString& error){
            if (!*alive){
                return;
            }
            if (fetched == nullptr){
                done(QString(), error);
                return;
            }
            const QByteArray bytes = *fetched;
            const TextFile file = TextFile::from_bytes(bytes);
            if (!file.valid){
                done(QString(), "\"" + path + "\" is not a text file.");
                return;
            }
            //  The text as the file has its line ends. A file with both
            //  kinds is searched with both.
            const QByteArray old_lf = with_lf(old_text);
            const QByteArray new_lf = with_lf(new_text);
            QByteArray old_bytes;
            QByteArray new_bytes;
            qsizetype at = -1;
            int count;
            if (!old_lf.contains('\n')){
                old_bytes = old_lf;
                new_bytes = file.crlf ? lf_to_crlf(new_lf) : new_lf;
                count = count_of(bytes, old_bytes, &at);
            }else{
                qsizetype at_crlf = -1;
                qsizetype at_lf = -1;
                const QByteArray old_crlf = lf_to_crlf(old_lf);
                const int count_crlf = count_of(bytes, old_crlf, &at_crlf);
                const int count_lf = count_of(bytes, old_lf, &at_lf);
                count = count_crlf + count_lf;
                if (count_crlf > 0){
                    old_bytes = old_crlf;
                    new_bytes = lf_to_crlf(new_lf);
                    at = at_crlf;
                }else{
                    old_bytes = old_lf;
                    new_bytes = new_lf;
                    at = at_lf;
                }
            }
            if (count == 0){
                done(
                    QString(),
                    "old_text is not in \"" + path + "\". It has to be exactly as in the file: the same "
                    "spaces at the start of each line, no line numbers. Read the lines again with read_file "
                    "and copy them from there."
                );
                return;
            }
            if (count > 1){
                done(
                    QString(),
                    "old_text is in \"" + path + "\" " + QString::number(count) + " times. Add a line before "
                    "or after it so that it is there only once."
                );
                return;
            }
            if (old_bytes == new_bytes){
                done(QString(), "new_text is the same as old_text: nothing to change.");
                return;
            }
            QByteArray changed = bytes;
            changed.replace(at, old_bytes.size(), new_bytes);
            if (changed.size() > MAX_WRITE_BYTES){
                done(QString(), "\"" + path + "\" would be " + size_text(changed.size()) + ": too large to change here.");
                return;
            }

            PendingChange* change = change_of(path);
            if (change == nullptr){
                PendingChange fresh;
                fresh.path = path;
                const Entry entry = m_tree.value(path);
                fresh.base_blob = entry.blob;
                fresh.mode = entry.mode.isEmpty() ? QString("100644") : entry.mode;
                fresh.before = bytes;
                fresh.after = changed;
                m_changes.push_back(std::move(fresh));
            }else{
                change->after = changed;
            }
            remove_unchanged(m_changes);
            const int line = (int)bytes.left(at).count('\n') + 1;
            const int old_lines = (int)old_lf.count('\n') + 1;
            const int new_lines = (int)new_lf.count('\n') + 1;
            done(
                "Changed " + path + ": " + QString::number(old_lines) + (old_lines == 1 ? " line" : " lines") +
                " from line " + QString::number(line) + " replaced by " + QString::number(new_lines) +
                ". Kept on this computer: nothing is on GitHub until commit_changes.",
                QString()
            );
        });
    });
}

void Workspace::write_file(const QString& path_given, const QString& content, bool overwrite, Done done){
    const QString path = normal_path(path_given);
    const QString refused = why_not_writable(path);
    if (!refused.isEmpty()){
        done(QString(), refused);
        return;
    }
    if (content.trimmed().isEmpty()){
        done(QString(), "The content is empty.");
        return;
    }
    if (is_cpp_source(path) && content.contains('\t')){
        done(QString(), "The content has tab characters. This repository's sources have none (its build checks): use 4 spaces.");
        return;
    }
    const std::shared_ptr<bool> alive = m_alive;
    with_tree([this, alive, path, content, overwrite, done](const QString& tree_error){
        if (!*alive){
            return;
        }
        if (!tree_error.isEmpty()){
            done(QString(), tree_error);
            return;
        }
        const bool exists = change_of(path) != nullptr || m_tree.contains(path);
        if (exists && !overwrite){
            done(
                QString(),
                "\"" + path + "\" exists. Change a part of it with edit_file, or pass overwrite to replace "
                "all of it."
            );
            return;
        }
        //  The lines of the new content.
        QString text = content;
        text.replace("\r\n", "\n");
        while (text.endsWith('\n')){
            text.chop(1);
        }
        const auto store = [this, path, text, done](const TextFile* existing, const QByteArray& before){
            TextFile file;
            if (existing != nullptr){
                file.bom = existing->bom;
                file.crlf = existing->crlf;
            }else{
                file.crlf = new_file_is_crlf(path);
            }
            file.lines = text.split('\n');
            const QByteArray bytes = file.to_bytes();
            if (bytes.size() > MAX_WRITE_BYTES){
                done(QString(), "The content is " + size_text(bytes.size()) + ": too large for one file here.");
                return;
            }
            PendingChange* change = change_of(path);
            if (change != nullptr){
                change->after = bytes;
            }else{
                PendingChange fresh;
                fresh.path = path;
                fresh.created = existing == nullptr;
                if (existing != nullptr){
                    const Entry entry = m_tree.value(path);
                    fresh.base_blob = entry.blob;
                    fresh.mode = entry.mode.isEmpty() ? QString("100644") : entry.mode;
                    fresh.before = before;
                }
                fresh.after = bytes;
                m_changes.push_back(std::move(fresh));
            }
            remove_unchanged(m_changes);
            QString note;
            if (existing == nullptr && path.startsWith("SerialPrograms/Source/") && is_cpp_source(path)){
                note = " The build only compiles files that are listed in " + QString::fromLatin1(SOURCE_LIST) +
                    ": add this one there (as \"" + path.mid(QString("SerialPrograms/").size()) + "\").";
            }
            done(
                (existing == nullptr ? "New file " : "Replaced ") + path + ", " +
                QString::number(file.lines.size()) + " lines. Kept on this computer: nothing is on GitHub "
                "until commit_changes." + note,
                QString()
            );
        };
        if (!exists){
            store(nullptr, QByteArray());
            return;
        }
        fetch(path, [alive, path, store, done](const QByteArray* bytes, const QString& error){
            if (!*alive){
                return;
            }
            if (bytes == nullptr){
                done(QString(), error);
                return;
            }
            const TextFile existing = TextFile::from_bytes(*bytes);
            if (!existing.valid){
                done(QString(), "\"" + path + "\" is not a text file.");
                return;
            }
            store(&existing, *bytes);
        });
    });
}

QString Workspace::discard(const QString& path_given){
    const QString path = normal_path(path_given);
    if (m_changes.empty()){
        return "There are no changes to take back.";
    }
    if (path.isEmpty()){
        const size_t count = m_changes.size();
        m_changes.clear();
        return "Took back the changes of " + QString::number(count) + (count == 1 ? " file." : " files.");
    }
    const size_t before = m_changes.size();
    m_changes.erase(
        std::remove_if(m_changes.begin(), m_changes.end(), [&](const PendingChange& change){
            return change.path == path;
        }),
        m_changes.end()
    );
    if (m_changes.size() == before){
        return "\"" + path + "\" has no change in this conversation.";
    }
    return "Took back the change of " + path + ".";
}

QString Workspace::changes_summary() const{
    QStringList lines;
    for (const PendingChange& change : m_changes){
        const TextFile before = TextFile::from_bytes(change.before);
        const TextFile after = TextFile::from_bytes(change.after);
        if (change.created){
            lines += change.path + " (new, " + QString::number(after.lines.size()) + " lines)";
            continue;
        }
        //  Lines taken out and put in.
        int removed = 0;
        int added = 0;
        for (const QString& line : diff_text(change.before, change.after).split('\n')){
            if (line.startsWith('-')){
                removed++;
            }else if (line.startsWith('+')){
                added++;
            }
        }
        lines += change.path + " (+" + QString::number(added) + " -" + QString::number(removed) + " lines)";
    }
    return lines.join("\n");
}

QString Workspace::changes_diff() const{
    QString text;
    for (const PendingChange& change : m_changes){
        text += "=== " + change.path + (change.created ? " (new file)" : "") + "\n";
        text += diff_text(change.before, change.after);
        text += "\n";
    }
    return text;
}



//  Committing.

QString Workspace::commit_branch(const QString& topic) const{
    if (!m_branch.isEmpty()){
        return m_branch;
    }
    return "claude/" + branch_slug(topic);
}

void Workspace::check_commit(const QString& topic, const QString& message, Done done){
    Q_UNUSED(topic);
    const Repository repo = repository();
    if (!repo.valid()){
        done(QString(), "No repository is set. Set the repository of this program in the settings of this panel.");
        return;
    }
    if (m_changes.empty()){
        done(QString(), "There is nothing to commit: no file was changed in this conversation since the last commit.");
        return;
    }
    if (message.trimmed().isEmpty()){
        done(QString(), "The commit needs a message that says what the change does.");
        return;
    }
    if ((int)m_changes.size() > MAX_COMMIT_FILES){
        done(QString(), "The change has " + QString::number(m_changes.size()) + " files: too many for one commit here.");
        return;
    }
    if (!m_github.has_token()){
        done(
            QString(),
            "Committing needs a GitHub token, and none is set. Tell the user: the changes are kept on this "
            "computer; add a token in the settings of this panel (it needs \"Contents: Read and write\" for " +
            repo.slug() + "), then ask to commit again."
        );
        return;
    }
    for (const PendingChange& change : m_changes){
        const QString refused = why_not_writable(change.path);
        if (!refused.isEmpty()){
            done(QString(), refused);
            return;
        }
    }
    //  New source files have to be in the list of files the build compiles.
    QStringList fresh;
    for (const PendingChange& change : m_changes){
        if (change.created && change.path.startsWith("SerialPrograms/Source/") && is_cpp_source(change.path)){
            fresh += change.path.mid(QString("SerialPrograms/").size());
        }
    }
    if (fresh.isEmpty()){
        done(QString(), QString());
        return;
    }
    const std::shared_ptr<bool> alive = m_alive;
    with_tree([this, alive, fresh, done](const QString& tree_error){
        if (!*alive){
            return;
        }
        if (!tree_error.isEmpty()){
            done(QString(), tree_error);
            return;
        }
        fetch(QString::fromLatin1(SOURCE_LIST), [alive, fresh, done](const QByteArray* bytes, const QString& error){
            if (!*alive){
                return;
            }
            if (bytes == nullptr){
                done(QString(), error);
                return;
            }
            QStringList missing;
            for (const QString& path : fresh){
                if (!bytes->contains(path.toUtf8())){
                    missing += path;
                }
            }
            if (!missing.isEmpty()){
                done(
                    QString(),
                    "Not committed: the build only compiles files listed in " + QString::fromLatin1(SOURCE_LIST) +
                    ", and these new ones are not in it: " + missing.join(", ") + ". Add them there with "
                    "edit_file (one path a line, next to the files of the same folder, in alphabetical order), "
                    "then commit again."
                );
                return;
            }
            done(QString(), QString());
        });
    });
}

struct Workspace::CommitJob{
    Repository repo;
    QString topic;
    QString message;
    std::shared_ptr<bool> stopped;
    std::function<void(const CommitInfo* info, const QString& error)> done;

    std::vector<PendingChange> changes;
    QString branch;
    bool new_branch = false;
    int name_try = 1;
    QString base_head;
    QString parent;
    QString parent_tree;
    QStringList blobs;
    QString tree;
    QString commit;
    enum class Step{
        BASE_HEAD,
        BRANCH,
        BEHIND,
        MERGE,
        PARENT,
        FILES,
        BLOBS,
        TREE,
        COMMIT,
        REF,
    };
    Step step = Step::BASE_HEAD;
};

void Workspace::commit(
    const QString& topic, const QString& message, std::shared_ptr<bool> stopped,
    std::function<void(const CommitInfo* info, const QString& error)> done
){
    std::shared_ptr<CommitJob> job = std::make_shared<CommitJob>();
    job->repo = repository();
    job->topic = topic;
    job->message = message.trimmed();
    job->stopped = std::move(stopped);
    job->done = std::move(done);
    job->changes = m_changes;
    if (!job->repo.valid() || job->changes.empty() || !m_github.has_token()){
        job->done(nullptr, "There is nothing to commit, or no repository or token is set.");
        return;
    }
    commit_step(job);
}

void Workspace::commit_step(std::shared_ptr<CommitJob> job){
    using Step = CommitJob::Step;
    const std::shared_ptr<bool> alive = m_alive;
    const quint64 generation = m_generation;
    const QString api = "/repos/" + job->repo.slug();
    //  What comes back for a conversation that is gone is dropped, and a
    //  commit the user stopped writes nothing more.
    const auto current = [this, alive, generation]{
        return *alive && generation == m_generation;
    };
    const auto fail = [job](const QString& error){
        job->done(nullptr, error);
    };
    const bool writes = job->step == Step::MERGE || job->step == Step::BLOBS || job->step == Step::TREE ||
        job->step == Step::COMMIT || job->step == Step::REF;
    if (writes && job->stopped && *job->stopped){
        fail("Stopped before the commit was made. Nothing was changed on GitHub's branches.");
        return;
    }

    switch (job->step){
    case Step::BASE_HEAD:
        resolve(job->repo.base_branch, true, [this, job, fail](const QString& commit, const QString& error){
            if (!error.isEmpty()){
                fail(error);
                return;
            }
            job->base_head = commit;
            job->step = Step::BRANCH;
            commit_step(job);
        });
        return;

    case Step::BRANCH:{
        //  The conversation's branch, or a new one whose name is free.
        if (!m_branch.isEmpty()){
            job->branch = m_branch;
        }else{
            job->branch = "claude/" + branch_slug(job->topic);
            if (job->name_try > 1){
                job->branch += "-" + QString::number(job->name_try);
            }
        }
        if (!is_claude_branch(job->branch) || job->branch == job->repo.base_branch){
            fail("\"" + job->branch + "\" is not a branch this program writes to: only branches named claude/... are.");
            return;
        }
        m_github.get(api + "/git/ref/heads/" + job->branch, [this, job, current, fail](const GitHubReply& reply){
            if (!current()){
                return;
            }
            if (reply.status == 404){
                if (!m_branch.isEmpty()){
                    fail("The branch " + job->branch + " is no longer on GitHub. Start a new conversation for this change.");
                    return;
                }
                job->new_branch = true;
                job->parent = job->base_head;
                job->step = Step::PARENT;
                commit_step(job);
                return;
            }
            if (!reply.ok()){
                fail(reply.error);
                return;
            }
            if (m_branch.isEmpty()){
                //  The name is taken by an earlier change: another one.
                if (job->name_try >= 9){
                    fail("Branches named " + job->branch + " and the like exist already. Give the commit another topic.");
                    return;
                }
                job->name_try++;
                commit_step(job);
                return;
            }
            job->parent = reply.object().value("object").toObject().value("sha").toString();
            job->step = Step::BEHIND;
            commit_step(job);
        });
        return;
    }

    case Step::BEHIND:
        //  Has the base branch moved on since the branch was made?
        m_github.get(
            api + "/compare/" + job->branch + "..." + job->repo.base_branch,
            [this, job, current, fail](const GitHubReply& reply){
                if (!current()){
                    return;
                }
                if (!reply.ok()){
                    fail(reply.error);
                    return;
                }
                const int ahead = reply.object().value("ahead_by").toInt();
                job->step = ahead > 0 ? Step::MERGE : Step::PARENT;
                commit_step(job);
            }
        );
        return;

    case Step::MERGE:
        m_github.call(
            "POST", api + "/merges",
            QJsonObject{
                {"base", job->branch},
                {"head", job->repo.base_branch},
                {"commit_message", "Merge " + job->repo.base_branch + " into " + job->branch},
            },
            [this, job, current, fail](const GitHubReply& reply){
                if (!current()){
                    return;
                }
                if (reply.status == 409){
                    fail(
                        "The branch " + job->repo.base_branch + " has changed the same lines as " + job->branch +
                        " since it was made, and GitHub cannot put the two together by itself. Nothing was "
                        "committed. Tell the user: the pull request of this branch has to be resolved on GitHub, "
                        "or the change made again in a new conversation (a new branch from the newest " +
                        job->repo.base_branch + ")."
                    );
                    return;
                }
                if (!reply.ok()){
                    fail(reply.error);
                    return;
                }
                const QString merged = reply.object().value("sha").toString();
                if (!merged.isEmpty()){
                    job->parent = merged;
                }
                job->step = Step::PARENT;
                commit_step(job);
            }
        );
        return;

    case Step::PARENT:
        m_github.get(api + "/git/commits/" + job->parent, [this, job, current, fail](const GitHubReply& reply){
            if (!current()){
                return;
            }
            if (!reply.ok()){
                fail(reply.error);
                return;
            }
            job->parent_tree = reply.object().value("tree").toObject().value("sha").toString();
            if (job->parent_tree.isEmpty()){
                fail("GitHub did not say which files the commit " + job->parent.left(7) + " has.");
                return;
            }
            job->step = Step::FILES;
            commit_step(job);
        });
        return;

    case Step::FILES:
        //  The files as they are at the commit the new one goes on: none of
        //  them may have changed since it was read here.
        load_tree(job->parent, [this, job, current, fail](const QString& error){
            if (!current()){
                return;
            }
            if (!error.isEmpty()){
                fail(error);
                return;
            }
            QStringList moved;
            for (const PendingChange& change : job->changes){
                const auto entry = m_tree.constFind(change.path);
                if (change.created){
                    if (entry != m_tree.constEnd()){
                        moved += change.path;
                    }
                }else if (entry == m_tree.constEnd() || entry->blob != change.base_blob){
                    moved += change.path;
                }
            }
            if (!moved.isEmpty()){
                fail(
                    "Not committed: these files changed on GitHub since they were read here, and the change "
                    "made here would undo that: " + moved.join(", ") + ". For each of them: take the change "
                    "back with discard_changes, read the file again, and make the change again."
                );
                return;
            }
            job->step = Step::BLOBS;
            commit_step(job);
        });
        return;

    case Step::BLOBS:{
        if (job->blobs.size() >= (qsizetype)job->changes.size()){
            job->step = Step::TREE;
            commit_step(job);
            return;
        }
        const PendingChange& change = job->changes[(size_t)job->blobs.size()];
        m_github.call(
            "POST", api + "/git/blobs",
            QJsonObject{
                {"content", QString::fromLatin1(change.after.toBase64())},
                {"encoding", "base64"},
            },
            [this, job, current, fail](const GitHubReply& reply){
                if (!current()){
                    return;
                }
                const QString blob = reply.object().value("sha").toString();
                if (!reply.ok() || blob.isEmpty()){
                    fail(reply.ok() ? QString("GitHub did not store a file of the change.") : reply.error);
                    return;
                }
                job->blobs += blob;
                commit_step(job);
            }
        );
        return;
    }

    case Step::TREE:{
        QJsonArray entries;
        for (size_t index = 0; index < job->changes.size(); index++){
            entries.append(QJsonObject{
                {"path", job->changes[index].path},
                {"mode", job->changes[index].mode},
                {"type", "blob"},
                {"sha", job->blobs[(qsizetype)index]},
            });
        }
        m_github.call(
            "POST", api + "/git/trees",
            QJsonObject{{"base_tree", job->parent_tree}, {"tree", entries}},
            [this, job, current, fail](const GitHubReply& reply){
                if (!current()){
                    return;
                }
                job->tree = reply.object().value("sha").toString();
                if (!reply.ok() || job->tree.isEmpty()){
                    fail(reply.ok() ? QString("GitHub did not store the files of the change.") : reply.error);
                    return;
                }
                job->step = Step::COMMIT;
                commit_step(job);
            }
        );
        return;
    }

    case Step::COMMIT:
        m_github.call(
            "POST", api + "/git/commits",
            QJsonObject{
                {"message", job->message + "\n\nCo-Authored-By: Claude <noreply@anthropic.com>"},
                {"tree", job->tree},
                {"parents", QJsonArray{job->parent}},
            },
            [this, job, current, fail](const GitHubReply& reply){
                if (!current()){
                    return;
                }
                job->commit = reply.object().value("sha").toString();
                if (!reply.ok() || job->commit.isEmpty()){
                    fail(reply.ok() ? QString("GitHub did not make the commit.") : reply.error);
                    return;
                }
                job->step = Step::REF;
                commit_step(job);
            }
        );
        return;

    case Step::REF:{
        const auto finished = [this, job, current, fail](const GitHubReply& reply){
            if (!current()){
                return;
            }
            if (!reply.ok()){
                fail(reply.error);
                return;
            }
            //  The branch is the conversation's from here on, and the files
            //  here are as on that branch.
            m_branch = job->branch;
            Head head;
            head.commit = job->commit;
            head.asked = QDateTime::currentMSecsSinceEpoch();
            m_heads[job->repo.slug() + "#" + job->branch] = head;
            CommitInfo info;
            for (size_t index = 0; index < job->changes.size(); index++){
                const PendingChange& change = job->changes[index];
                Entry entry;
                entry.blob = job->blobs[(qsizetype)index];
                entry.size = change.after.size();
                entry.mode = change.mode;
                if (!m_tree.contains(change.path)){
                    m_paths += change.path;
                }
                m_tree.insert(change.path, entry);
                m_blobs.insert(entry.blob, change.after);
                info.files += change.path;
            }
            m_paths.sort();
            m_tree_commit = job->commit;
            //  What was committed is no longer a change to commit. (What was
            //  changed since the user was asked stays.)
            std::vector<PendingChange> left;
            for (const PendingChange& change : m_changes){
                bool committed = false;
                for (const PendingChange& done_change : job->changes){
                    if (done_change.path == change.path && done_change.after == change.after){
                        committed = true;
                        break;
                    }
                }
                if (!committed){
                    PendingChange keep = change;
                    for (size_t index = 0; index < job->changes.size(); index++){
                        if (job->changes[index].path == keep.path){
                            //  Changed again meanwhile: now a change of the committed file.
                            keep.created = false;
                            keep.base_blob = job->blobs[(qsizetype)index];
                            keep.before = job->changes[index].after;
                        }
                    }
                    left.push_back(std::move(keep));
                }
            }
            m_changes = std::move(left);

            info.branch = job->branch;
            info.commit = job->commit;
            info.commit_url = job->repo.web_url() + "/commit/" + job->commit;
            info.pull_request_url = pull_request_url();
            info.new_branch = job->new_branch;
            info.message = job->message;
            job->done(&info, QString());
        };
        if (job->new_branch){
            m_github.call(
                "POST", api + "/git/refs",
                QJsonObject{{"ref", "refs/heads/" + job->branch}, {"sha", job->commit}},
                finished
            );
        }else{
            m_github.call(
                "PATCH", api + "/git/refs/heads/" + job->branch,
                QJsonObject{{"sha", job->commit}, {"force", false}},
                finished
            );
        }
        return;
    }
    }
}



//  The branch.

void Workspace::branch_status(Done done){
    const Repository repo = repository();
    if (!repo.valid()){
        done(QString(), "No repository is set. Set the repository of this program in the settings of this panel.");
        return;
    }
    QString pending = changes_summary();
    if (!pending.isEmpty()){
        pending = "Changed in this conversation and not committed yet:\n" + pending + "\n";
    }
    if (m_branch.isEmpty()){
        done(
            pending + "This conversation has no branch yet: nothing of it is on GitHub. A branch claude/... "
            "is made from the newest " + repo.base_branch + " with the first commit_changes.",
            QString()
        );
        return;
    }

    struct Report{
        QString text;
        QString branch;
        Repository repo;
        Done done;
    };
    std::shared_ptr<Report> report = std::make_shared<Report>();
    report->text = pending;
    report->branch = m_branch;
    report->repo = repo;
    report->done = std::move(done);
    const QString api = "/repos/" + repo.slug();
    const std::shared_ptr<bool> alive = m_alive;
    const quint64 generation = m_generation;
    const auto current = [this, alive, generation]{
        return *alive && generation == m_generation;
    };
    const QString pull_request = pull_request_url();

    //  3. The build of the branch.
    const auto builds = [this, report, api, current]{
        m_github.get(
            api + "/actions/runs?branch=" + QString::fromLatin1(QUrl::toPercentEncoding(report->branch)) + "&per_page=5",
            [this, report, api, current](const GitHubReply& reply){
                if (!current()){
                    return;
                }
                if (!reply.ok()){
                    report->text += "The builds could not be asked for: " + reply.error + "\n";
                    report->done(report->text, QString());
                    return;
                }
                const QJsonArray runs = reply.object().value("workflow_runs").toArray();
                if (runs.isEmpty()){
                    report->text += "No build has run for this branch yet. The build runs when the pull request is "
                        "opened, and again for each later commit; it takes about 7 minutes.\n";
                    report->done(report->text, QString());
                    return;
                }
                const QJsonObject run = runs[0].toObject();
                const QString status = run.value("status").toString();
                const QString conclusion = run.value("conclusion").toString();
                const QString what = "Build \"" + run.value("name").toString() + "\" number " +
                    QString::number(run.value("run_number").toInt()) + " (commit " +
                    run.value("head_sha").toString().left(7) + ")";
                const QString url = run.value("html_url").toString();
                if (status != "completed"){
                    report->text += what + " is " + (status == "queued" ? "waiting to start" : "running") +
                        ": " + url + "\nAsk again in a few minutes.\n";
                    report->done(report->text, QString());
                    return;
                }
                if (conclusion == "success"){
                    report->text += what + " passed: the program compiles and its interface test ran. The built "
                        "program is under \"Artifacts\" on " + url + "\n";
                    report->done(report->text, QString());
                    return;
                }
                report->text += what + " ended as \"" + conclusion + "\": " + url + "\n";
                //  Which step, and what the log says.
                const qint64 run_id = (qint64)run.value("id").toDouble();
                m_github.get(
                    api + "/actions/runs/" + QString::number(run_id) + "/jobs?per_page=30",
                    [this, report, current](const GitHubReply& jobs_reply){
                        if (!current()){
                            return;
                        }
                        qint64 failed_job = 0;
                        for (const QJsonValue& value : jobs_reply.object().value("jobs").toArray()){
                            const QJsonObject job = value.toObject();
                            if (job.value("conclusion").toString() != "failure"){
                                continue;
                            }
                            failed_job = (qint64)job.value("id").toDouble();
                            QStringList steps;
                            for (const QJsonValue& step : job.value("steps").toArray()){
                                if (step.toObject().value("conclusion").toString() == "failure"){
                                    steps += "\"" + step.toObject().value("name").toString() + "\"";
                                }
                            }
                            if (!steps.isEmpty()){
                                report->text += "It failed in the step " + steps.join(", ") + ".\n";
                            }
                            break;
                        }
                        if (failed_job == 0){
                            report->done(report->text, QString());
                            return;
                        }
                        m_github.job_log(report->repo, failed_job, [report, current](const GitHubReply& log){
                            if (!current()){
                                return;
                            }
                            if (!log.ok()){
                                report->text += "Its log could not be read: " + log.error + "\n";
                            }else{
                                const QStringList errors = build_errors(log.body, 40);
                                if (errors.isEmpty()){
                                    report->text += "The log has no line that looks like an error. Its end:\n" +
                                        QString::fromUtf8(log.body.right(1500)) + "\n";
                                }else{
                                    report->text += "What the log says went wrong:\n" + errors.join("\n") + "\n";
                                }
                            }
                            report->done(report->text, QString());
                        });
                    }
                );
            }
        );
    };

    //  2. The pull request.
    const auto pulls = [this, report, api, current, builds, pull_request]{
        m_github.get(
            api + "/pulls?state=all&per_page=5&base=" + report->repo.base_branch + "&head=" + report->repo.owner +
                ":" + QString::fromLatin1(QUrl::toPercentEncoding(report->branch)),
            [report, current, builds, pull_request](const GitHubReply& reply){
                if (!current()){
                    return;
                }
                const QJsonArray list = reply.json.array();
                if (!reply.ok()){
                    report->text += "The pull requests could not be asked for: " + reply.error + "\n";
                }else if (list.isEmpty()){
                    report->text += "There is no pull request for it yet. Only the user can open it: with the "
                        "button \"Open pull request\" under the commit in this panel, or at " + pull_request + "\n";
                }else{
                    const QJsonObject pull = list[0].toObject();
                    QString state = pull.value("state").toString();
                    if (!pull.value("merged_at").toString().isEmpty()){
                        state = "merged";
                    }
                    report->text += "Pull request number " + QString::number(pull.value("number").toInt()) +
                        " is " + state + ": " + pull.value("html_url").toString() + "\n";
                }
                builds();
            }
        );
    };

    //  1. The branch against the base branch.
    m_github.get(
        api + "/compare/" + repo.base_branch + "..." + m_branch,
        [report, current, pulls](const GitHubReply& reply){
            if (!current()){
                return;
            }
            if (!reply.ok()){
                report->done(QString(), reply.error);
                return;
            }
            const QJsonObject object = reply.object();
            const int ahead = object.value("ahead_by").toInt();
            const int behind = object.value("behind_by").toInt();
            report->text += "The branch " + report->branch + " has " + QString::number(ahead) +
                (ahead == 1 ? " commit" : " commits") + " that " + report->repo.base_branch + " does not have";
            if (behind > 0){
                report->text += ", and " + report->repo.base_branch + " has " + QString::number(behind) +
                    " newer " + (behind == 1 ? "commit" : "commits") + " (they are merged into the branch with "
                    "its next commit)";
            }
            report->text += ".\n";
            QStringList files;
            for (const QJsonValue& value : object.value("files").toArray()){
                files += value.toObject().value("filename").toString();
            }
            if (!files.isEmpty()){
                const qsizetype shown = std::min<qsizetype>(files.size(), 30);
                report->text += "Files it changes: " + files.mid(0, shown).join(", ") +
                    (files.size() > shown ? " and " + QString::number(files.size() - shown) + " more" : QString()) + ".\n";
            }
            pulls();
        }
    );
}

void Workspace::branches(const QString& use_given, Done done){
    const Repository repo = repository();
    if (!repo.valid()){
        done(QString(), "No repository is set. Set the repository of this program in the settings of this panel.");
        return;
    }
    const QString use = use_given.trimmed();
    if (!use.isEmpty() && !m_changes.empty()){
        done(
            QString(),
            "This conversation has changes that are not committed. Commit them or take them back before going "
            "on on another branch."
        );
        return;
    }
    const std::shared_ptr<bool> alive = m_alive;
    const quint64 generation = m_generation;
    m_github.get(
        "/repos/" + repo.slug() + "/branches?per_page=100",
        [this, alive, generation, repo, use, done](const GitHubReply& reply){
            if (!*alive || generation != m_generation){
                return;
            }
            if (!reply.ok()){
                done(QString(), reply.error);
                return;
            }
            QStringList names;
            for (const QJsonValue& value : reply.json.array()){
                const QString name = value.toObject().value("name").toString();
                if (name.startsWith("claude/")){
                    names += name;
                }
            }
            if (use.isEmpty()){
                QString text = names.isEmpty()
                    ? "The repository has no claude/... branches yet."
                    : "The claude/... branches of " + repo.slug() + ":\n" + names.join("\n");
                text += m_branch.isEmpty()
                    ? QString("\nThis conversation has no branch yet.")
                    : "\nThis conversation works on " + m_branch + ".";
                done(text, QString());
                return;
            }
            if (!names.contains(use) || !is_claude_branch(use)){
                done(QString(), "There is no branch \"" + use + "\" this program can write to. " +
                    (names.isEmpty() ? QString("The repository has no claude/... branches.") : "The claude/... branches: " + names.join(", ") + "."));
                return;
            }
            m_branch = use;
            done(
                "This conversation now works on " + use + ": files are read from it, and commit_changes adds "
                "to it.",
                QString()
            );
        }
    );
}



}
}
}

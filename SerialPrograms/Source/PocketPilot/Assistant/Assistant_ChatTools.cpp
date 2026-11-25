/*  PocketPilot Assistant: Chat Tools
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The tools of the chat. The first three look at the program itself and
 *  use no network at all; the others read and change its source through
 *  the workspace.
 *
 */

#include <algorithm>
#include <utility>
#include <QRegularExpression>
#include "../PocketPilot_Scrub.h"
#include "Assistant_Service.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


namespace{

//  The answer of the workspace as the result of a tool.
Workspace::Done as_result(std::function<void(ToolResult result)> done){
    return [done](const QString& text, const QString& error){
        done(error.isEmpty() ? ToolResult::ok(text) : ToolResult::error(error));
    };
}

//  The last part of a path: what the user sees of a file.
QString short_path(const QString& path){
    const qsizetype slash = path.lastIndexOf('/');
    return slash < 0 ? path : path.mid(slash + 1);
}

QString one_line(const QString& text, int length){
    QString line = text.simplified();
    if (line.size() > length){
        line = line.left(length - 3) + "...";
    }
    return line;
}

//  A game's name without the series in front, as the program shows it.
QString short_game(const QString& game){
    if (game.startsWith("Pok", Qt::CaseInsensitive)){
        const qsizetype space = game.indexOf(' ');
        if (space > 0 && space <= 8){
            return game.mid(space + 1);
        }
    }
    return game;
}

//  Options that are not for Claude: where a notification goes, a token.
bool is_private_option(const QString& name){
    static const QRegularExpression PRIVATE(
        "token|webhook|password|secret|api.?key|credential", QRegularExpression::CaseInsensitiveOption
    );
    return PRIVATE.match(name).hasMatch();
}

}



void add_chat_tools(ToolSet& tools, Service& service){

    //  The programs.
    {
        Tool tool;
        tool.name = "list_programs";
        tool.description =
            "Lists the automation programs of this program, by game, with a line on what each does. "
            "Without parameters: the games and how many programs each has. With \"game\" (a part of its "
            "name, such as \"FireRed\"): the programs of that game. With \"words\": the programs of all "
            "games that have these words in their name or description.";
        tool.input_schema = tool_schema({
            {"game", "string", "A part of the game's name."},
            {"words", "string", "Words to look for in the names and descriptions of all programs."},
        });
        tool.describe = [](const QJsonObject& input){
            const QString game = tool_string(input, "game");
            const QString words = tool_string(input, "words");
            if (!words.isEmpty()){
                return "Looked for programs with \"" + words + "\"";
            }
            if (!game.isEmpty()){
                return "Listed the programs of " + game;
            }
            return QString("Listed the games");
        };
        tool.run = [&service](const QJsonObject& input, const ToolCallContext&, std::function<void(ToolResult)> done){
            const std::vector<ProgramInfo> programs =
                service.hooks().programs ? service.hooks().programs() : std::vector<ProgramInfo>();
            const QString game = tool_string(input, "game").trimmed();
            const QStringList words = tool_string(input, "words").toLower().split(
                QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts
            );
            if (game.isEmpty() && words.isEmpty()){
                QStringList games;
                QStringList lines;
                for (const ProgramInfo& program : programs){
                    if (!games.contains(program.game)){
                        games += program.game;
                    }
                }
                for (const QString& name : games){
                    int count = 0;
                    for (const ProgramInfo& program : programs){
                        if (program.game == name){
                            count++;
                        }
                    }
                    lines += short_game(name) + ": " + QString::number(count) + (count == 1 ? " program" : " programs");
                }
                done(ToolResult::ok(
                    lines.isEmpty() ? QString("The program lists no games.") : lines.join("\n")
                ));
                return;
            }
            const int LIMIT = 80;
            QStringList lines;
            QString last_game;
            QString last_section;
            int more = 0;
            for (const ProgramInfo& program : programs){
                if (!game.isEmpty() && !program.game.contains(game, Qt::CaseInsensitive)){
                    continue;
                }
                const QString where = (program.name + " " + program.description).toLower();
                bool match = true;
                for (const QString& word : words){
                    if (!where.contains(word)){
                        match = false;
                        break;
                    }
                }
                if (!match){
                    continue;
                }
                if (lines.size() >= LIMIT){
                    more++;
                    continue;
                }
                if (program.game != last_game){
                    last_game = program.game;
                    last_section.clear();
                    lines += short_game(program.game) + ":";
                }
                if (program.section != last_section && !program.section.isEmpty()){
                    last_section = program.section;
                    lines += "  [" + program.section.simplified() + "]";
                }
                //  The whole description for a few programs, its start for many.
                lines += "  " + program.name + ": " + program.description.simplified();
            }
            if (lines.isEmpty()){
                done(ToolResult::ok("No program matches. Call list_programs without parameters for the games."));
                return;
            }
            //  Many programs: their descriptions cut short.
            if (lines.size() > 30){
                for (QString& line : lines){
                    if (line.size() > 170){
                        line = line.left(167) + "...";
                    }
                }
            }
            QString text = lines.join("\n");
            if (more > 0){
                text += "\n(" + QString::number(more) + " more: narrow it down with \"game\" or \"words\".)";
            }
            done(ToolResult::ok(text));
        };
        tools.add(std::move(tool));
    }

    //  What is open.
    {
        Tool tool;
        tool.name = "get_open_program";
        tool.description =
            "What the user has open in the program right now: the automation program with its description, "
            "whether it is running, and its options as they are set (JSON, by the option's name in the "
            "source).";
        tool.input_schema = tool_schema({});
        tool.describe = [](const QJsonObject&){
            return QString("Looked at the open program");
        };
        tool.run = [&service](const QJsonObject&, const ToolCallContext&, std::function<void(ToolResult)> done){
            const OpenProgramInfo open =
                service.hooks().open_program ? service.hooks().open_program() : OpenProgramInfo();
            if (!open.open){
                done(ToolResult::ok("No program is open: the user is on the page with the games."));
                return;
            }
            QString text;
            if (open.game.isEmpty()){
                text = "The page \"" + open.name + "\" is open.";
            }else{
                text = "Open: \"" + open.name + "\" of \"" + open.game + "\", " +
                    (open.running ? "running" : "not running") + ".\n" + open.description.simplified();
            }
            if (!open.settings.isEmpty()){
                //  Without what is not Claude's to see, and not endless.
                QStringList kept;
                for (const QString& line : open.settings.split('\n')){
                    if (!is_private_option(line)){
                        kept += line;
                    }
                }
                QString settings = scrub_private_text(kept.join("\n"));
                if (settings.size() > 9000){
                    settings = settings.left(9000) + "\n(Cut off: the options are longer than this.)";
                }
                text += "\nIts options as set:\n" + settings;
            }
            done(ToolResult::ok(text));
        };
        tools.add(std::move(tool));
    }

    //  The log.
    {
        Tool tool;
        tool.name = "read_log";
        tool.description =
            "The last lines of the program's log: what the automation programs wrote while they ran (their "
            "steps, what they saw on the video, their errors; a line marked (red) is an error). \"lines\": "
            "how many, 80 if not given, at most 400. \"contains\": only the lines with this text. Read the "
            "log first when the user asks why a program stopped or what it did.";
        tool.input_schema = tool_schema({
            {"lines", "integer", "How many lines, counted from the end."},
            {"contains", "string", "Only lines that contain this text."},
        });
        tool.describe = [](const QJsonObject& input){
            const QString contains = tool_string(input, "contains");
            return contains.isEmpty()
                ? QString("Read the log")
                : "Read the log for \"" + one_line(contains, 40) + "\"";
        };
        tool.run = [&service](const QJsonObject& input, const ToolCallContext&, std::function<void(ToolResult)> done){
            const int wanted = std::clamp(tool_integer(input, "lines", 80), 1, 400);
            const QString contains = tool_string(input, "contains");
            if (!service.hooks().recent_log){
                done(ToolResult::error("The log cannot be read here."));
                return;
            }
            //  With a filter: looked for in much more of the log.
            const QStringList all = service.hooks().recent_log(contains.isEmpty() ? wanted : 10000);
            QStringList lines;
            for (const QString& raw : all){
                //  Cleaned before it is looked in or sent: Claude cannot ask
                //  the log for a token and be given it.
                const QString line = scrub_private_text(raw);
                if (contains.isEmpty() || line.contains(contains, Qt::CaseInsensitive)){
                    lines += line.size() > 600 ? line.left(600) + "..." : line;
                }
            }
            if (lines.size() > wanted){
                lines = lines.mid(lines.size() - wanted);
            }
            if (lines.isEmpty()){
                done(ToolResult::ok(
                    contains.isEmpty()
                        ? QString("The log is empty: nothing has run since the program started.")
                        : "No line of the log has \"" + contains + "\"."
                ));
                return;
            }
            done(ToolResult::ok(lines.join("\n")));
        };
        tools.add(std::move(tool));
    }

    //  The source: reading.
    {
        Tool tool;
        tool.name = "find_files";
        tool.description =
            "Finds files of the program's source by words of their path: all words have to be in it, case "
            "does not matter. Optionally only under a folder. Gives up to 60 paths with their sizes. The "
            "files of a program carry its name without spaces: \"Level Trainer\" is "
            "PokemonFRLG_LevelTrainer.cpp.";
        tool.input_schema = tool_schema({
            {"words", "string", "Words of the path, separated by spaces.", true},
            {"path_prefix", "string", "Only files under this folder."},
        });
        tool.describe = [](const QJsonObject& input){
            return "Looked for files with \"" + one_line(tool_string(input, "words"), 50) + "\"";
        };
        tool.run = [&service](const QJsonObject& input, const ToolCallContext&, std::function<void(ToolResult)> done){
            service.workspace().find_files(
                tool_string(input, "words"), tool_string(input, "path_prefix"), as_result(std::move(done))
            );
        };
        tools.add(std::move(tool));
    }
    {
        Tool tool;
        tool.name = "read_file";
        tool.description =
            "Reads lines of a source file. Each line comes with its number in front (\"12| text\"): the "
            "number and the bar are not part of the file. At most 400 lines at once: give first_line and "
            "last_line for a part of a larger file. The file is shown with the changes of this "
            "conversation.";
        tool.input_schema = tool_schema({
            {"path", "string", "The file's path in the repository.", true},
            {"first_line", "integer", "The first line to read. 1 if not given."},
            {"last_line", "integer", "The last line to read. The end of the file if not given."},
        });
        tool.describe = [](const QJsonObject& input){
            QString text = "Read " + short_path(tool_string(input, "path"));
            const int first = tool_integer(input, "first_line", 0);
            const int last = tool_integer(input, "last_line", 0);
            if (first > 0 && last >= first){
                text += ", lines " + QString::number(first) + " to " + QString::number(last);
            }else if (first > 1){
                text += ", from line " + QString::number(first);
            }
            return text;
        };
        tool.run = [&service](const QJsonObject& input, const ToolCallContext&, std::function<void(ToolResult)> done){
            service.workspace().read_file(
                tool_string(input, "path"), tool_integer(input, "first_line", 1),
                tool_integer(input, "last_line", 0), as_result(std::move(done))
            );
        };
        tools.add(std::move(tool));
    }
    {
        Tool tool;
        tool.name = "outline_file";
        tool.description =
            "Where the parts of a source file start: its classes and functions with their line numbers, "
            "without reading all of it. Use it on a large file to find the lines to read.";
        tool.input_schema = tool_schema({
            {"path", "string", "The file's path in the repository.", true},
        });
        tool.describe = [](const QJsonObject& input){
            return "Outlined " + short_path(tool_string(input, "path"));
        };
        tool.run = [&service](const QJsonObject& input, const ToolCallContext&, std::function<void(ToolResult)> done){
            service.workspace().outline_file(tool_string(input, "path"), as_result(std::move(done)));
        };
        tools.add(std::move(tool));
    }
    {
        Tool tool;
        tool.name = "search_text";
        tool.description =
            "Searches the source files under a folder for a text (case does not matter) or, with \"regex\", "
            "for a regular expression. Gives the lines found as path:line: text. \"path_prefix\" is "
            "required and should be narrow, such as one game's or one program's folder: at most 700 files "
            "are searched at once, and a file is fetched the first time it is searched.";
        tool.input_schema = tool_schema({
            {"pattern", "string", "The text or regular expression to look for.", true},
            {"path_prefix", "string", "The folder (or file) to search in.", true},
            {"regex", "boolean", "The pattern is a regular expression."},
            {"limit", "integer", "The most lines to give back. 40 if not given, at most 100."},
        });
        tool.describe = [](const QJsonObject& input){
            QString folder = tool_string(input, "path_prefix");
            while (folder.endsWith('/')){
                folder.chop(1);
            }
            return "Searched for \"" + one_line(tool_string(input, "pattern"), 40) + "\" in " + short_path(folder);
        };
        tool.run = [&service](const QJsonObject& input, const ToolCallContext& context, std::function<void(ToolResult)> done){
            service.workspace().search_text(
                tool_string(input, "pattern"), tool_string(input, "path_prefix"),
                tool_boolean(input, "regex", false), tool_integer(input, "limit", 40),
                context.progress, as_result(std::move(done))
            );
        };
        tools.add(std::move(tool));
    }

    //  The source: changing.
    {
        Tool tool;
        tool.name = "edit_file";
        tool.description =
            "Replaces a part of a source file: \"old_text\", exactly as it is in the file (the same spaces "
            "at the start of each line, without the line numbers of read_file), by \"new_text\". old_text "
            "has to be in the file exactly once: take enough lines to make it so. The change is kept on the "
            "user's computer until commit_changes. Line ends are taken care of.";
        tool.input_schema = tool_schema({
            {"path", "string", "The file's path in the repository.", true},
            {"old_text", "string", "The text to replace, exactly as in the file.", true},
            {"new_text", "string", "The text to put in its place.", true},
        });
        tool.describe = [](const QJsonObject& input){
            return "Changed " + short_path(tool_string(input, "path"));
        };
        tool.run = [&service](const QJsonObject& input, const ToolCallContext&, std::function<void(ToolResult)> done){
            service.workspace().edit_file(
                tool_string(input, "path"), tool_string(input, "old_text"), tool_string(input, "new_text"),
                as_result(std::move(done))
            );
        };
        tools.add(std::move(tool));
    }
    {
        Tool tool;
        tool.name = "write_file";
        tool.description =
            "Makes a new source file with the given content, or with \"overwrite\" replaces all of an "
            "existing one (for a part of a file, use edit_file). Kept on the user's computer until "
            "commit_changes. A new .cpp or .h under SerialPrograms/Source/ also has to be added to "
            "SerialPrograms/cmake/SourceFiles.cmake.";
        tool.input_schema = tool_schema({
            {"path", "string", "The file's path in the repository.", true},
            {"content", "string", "The whole content of the file.", true},
            {"overwrite", "boolean", "Replace the file if it exists."},
        });
        tool.describe = [](const QJsonObject& input){
            return "Wrote " + short_path(tool_string(input, "path"));
        };
        tool.run = [&service](const QJsonObject& input, const ToolCallContext&, std::function<void(ToolResult)> done){
            service.workspace().write_file(
                tool_string(input, "path"), tool_string(input, "content"), tool_boolean(input, "overwrite", false),
                as_result(std::move(done))
            );
        };
        tools.add(std::move(tool));
    }
    {
        Tool tool;
        tool.name = "show_changes";
        tool.description =
            "The files changed in this conversation that are not committed yet. With \"diff\": also the "
            "changed lines.";
        tool.input_schema = tool_schema({
            {"diff", "boolean", "Also give the changed lines."},
        });
        tool.describe = [](const QJsonObject&){
            return QString("Looked at the changes");
        };
        tool.run = [&service](const QJsonObject& input, const ToolCallContext&, std::function<void(ToolResult)> done){
            const QString summary = service.workspace().changes_summary();
            if (summary.isEmpty()){
                done(ToolResult::ok("Nothing is changed that is not committed."));
                return;
            }
            QString text = "Changed, not committed yet:\n" + summary;
            if (tool_boolean(input, "diff", false)){
                text += "\n\n" + service.workspace().changes_diff();
            }
            done(ToolResult::ok(text));
        };
        tools.add(std::move(tool));
    }
    {
        Tool tool;
        tool.name = "discard_changes";
        tool.description =
            "Takes back the uncommitted change of one file (\"path\"), or of all files (no path).";
        tool.input_schema = tool_schema({
            {"path", "string", "The file whose change to take back. All files if not given."},
        });
        tool.describe = [](const QJsonObject& input){
            const QString path = tool_string(input, "path");
            return path.isEmpty() ? QString("Took back all changes") : "Took back the change of " + short_path(path);
        };
        tool.run = [&service](const QJsonObject& input, const ToolCallContext&, std::function<void(ToolResult)> done){
            done(ToolResult::ok(service.workspace().discard(tool_string(input, "path"))));
        };
        tools.add(std::move(tool));
    }
    {
        Tool tool;
        tool.name = "commit_changes";
        tool.description =
            "Commits all uncommitted changes of this conversation to its branch on GitHub. The first commit "
            "of a conversation makes the branch, named claude/<topic>, from the newest commit of the base "
            "branch; later commits go on the same branch. The user is asked to approve each commit. "
            "\"topic\": two to five words that name the change, for the branch's name. \"message\": the "
            "commit message: a short first line, an empty line, then what changed and why.";
        tool.input_schema = tool_schema({
            {"topic", "string", "Two to five words that name the change.", true},
            {"message", "string", "The commit message.", true},
        });
        tool.describe = [&service](const QJsonObject& input){
            return "Commit to " + service.workspace().commit_branch(tool_string(input, "topic"));
        };
        tool.run = [&service](const QJsonObject& input, const ToolCallContext& context, std::function<void(ToolResult)> done){
            const QString topic = tool_string(input, "topic");
            const QString message = tool_string(input, "message").trimmed();
            Service* const target = &service;
            service.workspace().check_commit(topic, message, [target, topic, message, context, done](const QString&, const QString& error){
                if (!error.isEmpty()){
                    done(ToolResult::error(error));
                    return;
                }
                Workspace& workspace = target->workspace();
                const Repository repository = target->repository();
                const QString branch = workspace.commit_branch(topic);
                const size_t files = workspace.changes().size();
                ApprovalRequest approval;
                approval.title = "Commit to " + branch;
                approval.text =
                    message.section('\n', 0, 0) + "\n" +
                    QString::number(files) + (files == 1 ? " file" : " files") + ", on " +
                    (workspace.branch().isEmpty()
                        ? "a new branch made from " + repository.base_branch
                        : QString("the branch of this conversation")) +
                    " in " + repository.slug() + ". Nothing changes in " + repository.base_branch +
                    " until you merge the pull request.";
                approval.accept_label = "Commit";
                approval.decline_label = "Not now";
                approval.details_label = "View changes";
                approval.show_details = [target]{
                    if (target->on_show_changes){
                        target->on_show_changes();
                    }
                };
                if (!context.approve){
                    done(ToolResult::error("The user cannot be asked right now, so nothing was committed."));
                    return;
                }
                context.approve(approval, [target, topic, message, context, done](bool approved){
                    if (!approved){
                        done(ToolResult::ok(
                            "The user did not approve this commit. Nothing was committed; the changes are "
                            "still kept here. Ask what they want different."
                        ));
                        return;
                    }
                    if (context.progress){
                        context.progress("Committing");
                    }
                    target->workspace().commit(topic, message, context.stopped, [target, done](const CommitInfo* info, const QString& commit_error){
                        if (info == nullptr){
                            done(ToolResult::error(commit_error));
                            return;
                        }
                        if (target->on_commit){
                            target->on_commit(*info);
                        }
                        done(ToolResult::ok(
                            "Committed " + QString::number(info->files.size()) +
                            (info->files.size() == 1 ? " file" : " files") + " to " + info->branch +
                            (info->new_branch ? " (a new branch)" : "") + ": " + info->commit_url + "\n"
                            "The user sees the commit in this panel with the button \"Open pull request\" "
                            "(" + info->pull_request_url + "). The build runs once the pull request exists, "
                            "and again by itself after each later commit."
                        ));
                    });
                });
            });
        };
        tools.add(std::move(tool));
    }
    {
        Tool tool;
        tool.name = "branch_status";
        tool.description =
            "The state of this conversation's branch on GitHub: how it compares to the base branch, whether "
            "its pull request exists, and the result of its build, with the errors of the build's log if it "
            "failed. Use it when the user asks about the build or says that it finished.";
        tool.input_schema = tool_schema({});
        tool.describe = [](const QJsonObject&){
            return QString("Checked the branch and its build");
        };
        tool.run = [&service](const QJsonObject&, const ToolCallContext&, std::function<void(ToolResult)> done){
            service.workspace().branch_status(as_result(std::move(done)));
        };
        tools.add(std::move(tool));
    }
    {
        Tool tool;
        tool.name = "branches";
        tool.description =
            "The claude/... branches of the repository: the changes made from this program so far. With "
            "\"use\": this conversation goes on on that branch (to continue an earlier change, or to fix "
            "its build), instead of making a new one with its first commit.";
        tool.input_schema = tool_schema({
            {"use", "string", "The claude/... branch to go on with."},
        });
        tool.describe = [](const QJsonObject& input){
            const QString use = tool_string(input, "use");
            return use.isEmpty() ? QString("Listed the branches") : "Went on with " + use;
        };
        tool.run = [&service](const QJsonObject& input, const ToolCallContext&, std::function<void(ToolResult)> done){
            service.workspace().branches(tool_string(input, "use"), as_result(std::move(done)));
        };
        tools.add(std::move(tool));
    }
}



}
}
}

/*  PocketPilot Assistant: Tools
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  What Claude can do besides writing text: each tool is something this
 *  program does on its own computer (look up a program, read the log, read
 *  a source file, change one) and hands the result to Claude. A tool costs
 *  tokens only for what it gives back, so the tools give back little: the
 *  lines asked for, not the file.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Assistant_Tools_H
#define PokemonAutomation_PocketPilot_Assistant_Tools_H

#include <functional>
#include <initializer_list>
#include <memory>
#include <vector>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


struct ToolResult{
    QString text;
    bool is_error = false;
    //  A picture that goes with the text: the bytes of a PNG or of a JPEG
    //  (they say which themselves). Empty: none.
    QByteArray image;

    static ToolResult error(const QString& text){
        ToolResult result;
        result.text = text;
        result.is_error = true;
        return result;
    }
    static ToolResult ok(const QString& text){
        ToolResult result;
        result.text = text;
        return result;
    }
};


//  Something only the user may decide: shown as a card with two buttons.
struct ApprovalRequest{
    QString title;
    QString text;
    QString accept_label = "Allow";
    QString decline_label = "Not now";
    //  A third button that shows more (the changes of a commit). May be
    //  empty.
    QString details_label;
    std::function<void()> show_details;
};


//  What a tool call can use besides its input.
struct ToolCallContext{
    //  A line for the user on what the tool is doing ("Fetching 34 files").
    std::function<void(const QString& text)> progress;
    //  Asks the user. The answer comes later, or never if the user stops
    //  the conversation first.
    std::function<void(const ApprovalRequest& request, std::function<void(bool approved)> answer)> approve;
    //  True once the user stopped the turn this call belongs to: whatever
    //  the tool still wanted to do, it must not do it.
    std::shared_ptr<bool> stopped;
};


struct Tool{
    QString name;
    //  For Claude: what it does, when to use it, what comes back.
    QString description;
    QJsonObject input_schema;
    //  A few words for the user on one call: "Read Shell.cpp, lines 1 to 200".
    std::function<QString(const QJsonObject& input)> describe;
    //  Does the work and calls "done" once, now or later.
    std::function<void(
        const QJsonObject& input,
        const ToolCallContext& context,
        std::function<void(ToolResult result)> done
    )> run;
};


//  One parameter of a tool, for its schema.
struct ToolParameter{
    const char* name;
    const char* type;           //  "string", "integer", "boolean"
    const char* description;
    bool required = false;
};

//  The JSON schema of a tool's input.
QJsonObject tool_schema(std::initializer_list<ToolParameter> parameters);

//  A parameter of a call, with what to take when it is not there or of
//  another type.
QString tool_string(const QJsonObject& input, const char* name, const QString& fallback = QString());
int tool_integer(const QJsonObject& input, const char* name, int fallback);
bool tool_boolean(const QJsonObject& input, const char* name, bool fallback);


class ToolSet{
public:
    void add(Tool tool);
    const Tool* find(const QString& name) const;
    //  As the API wants them.
    QJsonArray definitions() const;
    size_t size() const{ return m_tools.size(); }

private:
    std::vector<Tool> m_tools;
};



}
}
}
#endif

/*  PocketPilot Assistant: Tools
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <utility>
#include <QJsonValue>
#include "Assistant_Tools.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


QJsonObject tool_schema(std::initializer_list<ToolParameter> parameters){
    QJsonObject properties;
    QJsonArray required;
    for (const ToolParameter& parameter : parameters){
        properties[QString::fromLatin1(parameter.name)] = QJsonObject{
            {"type", QString::fromLatin1(parameter.type)},
            {"description", QString::fromUtf8(parameter.description)},
        };
        if (parameter.required){
            required.append(QString::fromLatin1(parameter.name));
        }
    }
    QJsonObject schema{
        {"type", "object"},
        {"properties", properties},
    };
    if (!required.isEmpty()){
        schema["required"] = required;
    }
    return schema;
}

QString tool_string(const QJsonObject& input, const char* name, const QString& fallback){
    const QJsonValue value = input.value(QString::fromLatin1(name));
    if (value.isString()){
        return value.toString();
    }
    //  A number where text was asked for: as text.
    if (value.isDouble()){
        return QString::number(value.toDouble(), 'g', 15);
    }
    return fallback;
}

int tool_integer(const QJsonObject& input, const char* name, int fallback){
    const QJsonValue value = input.value(QString::fromLatin1(name));
    if (value.isDouble()){
        const double number = value.toDouble();
        if (number > -2000000000.0 && number < 2000000000.0){
            return (int)number;
        }
        return fallback;
    }
    //  A number written as text.
    if (value.isString()){
        bool ok = false;
        const int number = value.toString().trimmed().toInt(&ok);
        if (ok){
            return number;
        }
    }
    return fallback;
}

bool tool_boolean(const QJsonObject& input, const char* name, bool fallback){
    const QJsonValue value = input.value(QString::fromLatin1(name));
    if (value.isBool()){
        return value.toBool();
    }
    if (value.isString()){
        const QString text = value.toString().trimmed().toLower();
        if (text == "true" || text == "yes"){
            return true;
        }
        if (text == "false" || text == "no"){
            return false;
        }
    }
    return fallback;
}


void ToolSet::add(Tool tool){
    m_tools.push_back(std::move(tool));
}

const Tool* ToolSet::find(const QString& name) const{
    for (const Tool& tool : m_tools){
        if (tool.name == name){
            return &tool;
        }
    }
    return nullptr;
}

QJsonArray ToolSet::definitions() const{
    QJsonArray definitions;
    for (const Tool& tool : m_tools){
        definitions.append(QJsonObject{
            {"name", tool.name},
            {"description", tool.description},
            {"input_schema", tool.input_schema},
        });
    }
    return definitions;
}



}
}
}

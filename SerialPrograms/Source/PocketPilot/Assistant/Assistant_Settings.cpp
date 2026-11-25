/*  PocketPilot Assistant: Settings
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <algorithm>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include "Assistant_Settings.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "crypt32.lib")
#endif
#endif

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


namespace{

const char* const SECRET_NAMES[] = {"claude_api_key", "github_token"};

#ifdef _WIN32

const char* const PROTECTION = "windows-dpapi";

//  Encrypted for the Windows account that runs the program.
bool protect(const QByteArray& plain, QByteArray& stored){
    DATA_BLOB in;
    in.pbData = (BYTE*)plain.data();
    in.cbData = (DWORD)plain.size();
    DATA_BLOB out;
    out.pbData = nullptr;
    out.cbData = 0;
    if (!CryptProtectData(&in, L"PocketPilot assistant key", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)){
        return false;
    }
    stored = QByteArray((const char*)out.pbData, (qsizetype)out.cbData);
    LocalFree(out.pbData);
    return true;
}
bool unprotect(const QByteArray& stored, QByteArray& plain){
    DATA_BLOB in;
    in.pbData = (BYTE*)stored.data();
    in.cbData = (DWORD)stored.size();
    DATA_BLOB out;
    out.pbData = nullptr;
    out.cbData = 0;
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)){
        return false;
    }
    plain = QByteArray((const char*)out.pbData, (qsizetype)out.cbData);
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return true;
}

#else

const char* const PROTECTION = "file-permissions";

bool protect(const QByteArray& plain, QByteArray& stored){
    stored = plain;
    return true;
}
bool unprotect(const QByteArray& stored, QByteArray& plain){
    plain = stored;
    return true;
}

#endif

}


QString SecretStore::protection_text() const{
    if (!m_remember){
        return "Kept in memory only, until you close the program. Nothing of it is written to the disk.";
    }
#ifdef _WIN32
    return
        "Saved on this computer, encrypted with your Windows account. Never written to the settings file or the "
        "log. Other programs that run as you can decrypt it too: to rule that out, do not remember the key.";
#else
    return
        "Saved on this computer in a file only your account can read. Never written to the settings file or the "
        "log. Other programs that run as you can read it too: to rule that out, do not remember the key.";
#endif
}

SecretStore::SecretStore(const QString& file_path)
    : m_path(file_path)
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)){
        return;
    }
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    const bool same_protection = object.value("protection").toString() == QLatin1String(PROTECTION);
    m_remember = object.value("remember").toBool(true);
    for (int index = 0; index < 2 && m_remember; index++){
        const QString text = object.value(QLatin1String(SECRET_NAMES[index])).toString();
        if (text.isEmpty()){
            continue;
        }
        QByteArray plain;
        if (same_protection && unprotect(QByteArray::fromBase64(text.toLatin1()), plain) && !plain.isEmpty()){
            m_values[index] = QString::fromUtf8(plain);
        }else{
            m_unreadable[index] = true;
        }
    }
}

const QString& SecretStore::get(Secret secret) const{
    return m_values[(int)secret];
}

bool SecretStore::unreadable(Secret secret) const{
    return m_unreadable[(int)secret];
}

bool SecretStore::set(Secret secret, const QString& value, QString& error){
    m_values[(int)secret] = value.trimmed();
    m_unreadable[(int)secret] = false;
    return write(error);
}

bool SecretStore::set_remembers(bool remember, QString& error){
    m_remember = remember;
    if (!write(error)){
        if (!remember){
            error = "The keys that were saved could not be removed from the disk (" + error + ").";
        }
        return false;
    }
    return true;
}

bool SecretStore::write(QString& error) const{
    QJsonObject object;
    object["protection"] = QLatin1String(PROTECTION);
    object["remember"] = m_remember;
    for (int index = 0; index < 2; index++){
        if (!m_remember || m_values[index].isEmpty()){
            continue;
        }
        QByteArray stored;
        if (!protect(m_values[index].toUtf8(), stored)){
            error = "The key could not be encrypted, so it was not saved. It is kept until the program closes.";
            return false;
        }
        object[QLatin1String(SECRET_NAMES[index])] = QString::fromLatin1(stored.toBase64());
    }
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)){
        error = "The key could not be saved (" + file.errorString() + "). It is kept until the program closes.";
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    if (!file.commit()){
        error = "The key could not be saved (" + file.errorString() + "). It is kept until the program closes.";
        return false;
    }
    //  For the user alone. (On Windows the content is encrypted for the
    //  account as well.)
    QFile::setPermissions(m_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    error.clear();
    return true;
}



void Settings::load(const QString& file_path){
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly)){
        return;
    }
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    const auto text = [&](const char* name, QString& value){
        const QJsonValue entry = object.value(QLatin1String(name));
        if (entry.isString()){
            value = entry.toString().trimmed();
        }
    };
    text("chat_model", chat_model);
    text("chat_effort", chat_effort);
    text("repository_owner", repository_owner);
    text("repository_name", repository_name);
    text("base_branch", base_branch);
    text("api_workspace_id", api_workspace_id);
    api_workspace_id = clean_workspace_id(api_workspace_id);
    if (object.value("ask_at_usd").isDouble()){
        ask_at_usd = std::clamp(object.value("ask_at_usd").toDouble(), 0.0, 1000.0);
    }
    if (object.value("panel_open").isBool()){
        panel_open = object.value("panel_open").toBool();
    }
    if (object.value("panel_width").isDouble()){
        panel_width = std::clamp(object.value("panel_width").toInt(), 320, 1200);
    }
    if (chat_model.isEmpty()){
        chat_model = Settings().chat_model;
    }
    if (chat_effort != "low" && chat_effort != "medium" && chat_effort != "high"){
        chat_effort.clear();
    }
}

bool Settings::save(const QString& file_path) const{
    QJsonObject object{
        {"chat_model", chat_model},
        {"chat_effort", chat_effort},
        {"ask_at_usd", ask_at_usd},
        {"api_workspace_id", api_workspace_id},
        {"repository_owner", repository_owner},
        {"repository_name", repository_name},
        {"base_branch", base_branch},
        {"panel_open", panel_open},
        {"panel_width", panel_width},
    };
    QDir().mkpath(QFileInfo(file_path).absolutePath());
    QSaveFile file(file_path);
    if (!file.open(QIODevice::WriteOnly)){
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.commit();
}

QString clean_workspace_id(const QString& entered){
    QString id;
    for (const QChar character : entered){
        const char16_t code = character.unicode();
        if (
            (code >= u'0' && code <= u'9') || (code >= u'A' && code <= u'Z') || (code >= u'a' && code <= u'z') ||
            code == u'_' || code == u'-'
        ){
            id += character;
        }
    }
    return id.left(128);
}



}
}
}

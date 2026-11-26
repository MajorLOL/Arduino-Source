/*  PocketPilot Language
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <algorithm>
#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <vector>
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QMutex>
#include <QPointer>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QReadWriteLock>
#include <QRegularExpression>
#include <QSet>
#include <QTabBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include "Common/Cpp/ColoredText.h"
#include "Common/Cpp/Logging/GlobalLogger.h"
#include "Common/Cpp/Logging/LastLogTracker.h"
#include "Common/Cpp/Logging/MultiOutputLogger.h"
#include "Common/Cpp/Options/ConfigOption.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/PersistentSettings.h"
#include "PocketPilot_Instances.h"
#include "CommonFramework/Logging/Logger.h"
#include "PocketPilot_Language.h"
#include "PocketPilot_Setup.h"

namespace PokemonAutomation{
namespace PocketPilot{


const char* const NO_LANGUAGE_PROMPT_OPTION = "--no-language-prompt";
const char* const LANGUAGE_TEST_OPTION = "--language-test";


namespace{


const LanguageInfo LANGUAGES[] = {
    {"en", "English"},
    {"nl", "Nederlands"},
    {"de", "Deutsch"},
    {"fr", "Français"},
    {"es", "Español"},
};
constexpr size_t LANGUAGE_COUNT = sizeof(LANGUAGES) / sizeof(LANGUAGES[0]);

//  The code that stands for "Other": the file Languages/custom.json.
const char* const CUSTOM_CODE = "custom";

//  The most texts that are kept in the list of what was not found.
constexpr int MISSING_LIMIT = 4000;


QString code_of(UiLanguage language){
    switch (language){
    case UiLanguage::ENGLISH: return "en";
    case UiLanguage::DUTCH:   return "nl";
    case UiLanguage::GERMAN:  return "de";
    case UiLanguage::FRENCH:  return "fr";
    case UiLanguage::SPANISH: return "es";
    case UiLanguage::CUSTOM:  return CUSTOM_CODE;
    }
    return "en";
}
UiLanguage language_of(const QString& code){
    if (code == "nl") return UiLanguage::DUTCH;
    if (code == "de") return UiLanguage::GERMAN;
    if (code == "fr") return UiLanguage::FRENCH;
    if (code == "es") return UiLanguage::SPANISH;
    if (code == CUSTOM_CODE) return UiLanguage::CUSTOM;
    return UiLanguage::ENGLISH;
}

//  The console of the setup, as the codes of its screen ("switch", "switch2";
//  empty: none chosen).
QString console_code_of(SetupConsole console){
    switch (console){
    case SetupConsole::SWITCH:   return "switch";
    case SetupConsole::SWITCH2:  return "switch2";
    case SetupConsole::NONE:     break;
    }
    return QString();
}
SetupConsole console_of(const QString& code){
    if (code == "switch") return SetupConsole::SWITCH;
    if (code == "switch2") return SetupConsole::SWITCH2;
    return SetupConsole::NONE;
}


struct State{
    QReadWriteLock lock;
    QHash<QString, QString> words;
    //  What the translations are: a text that is one of them was translated
    //  already (a text the program made with tr_ui) and is not "missing".
    QSet<QString> translated;
    QString code = "en";

    QMutex missing_lock;
    QSet<QString> missing;
    bool saving_missing_installed = false;
};
State& state(){
    static State instance;
    return instance;
}

//  True while a language other than English is in use. Read by the filter
//  for every event, so not behind a lock.
std::atomic<bool> g_active(false);


QString user_folder_path(){
    //  The languages of the user are the same for every installation.
    return QDir(QString::fromStdString(PocketPilot::shared_base_path())).filePath("Languages");
}


bool has_letters(const QString& text){
    for (const QChar c : text){
        if (c.isLetter()) return true;
    }
    return false;
}
bool has_digits(const QString& text){
    for (const QChar c : text){
        if (c.isDigit()) return true;
    }
    return false;
}

void note_missing(const QString& english){
    //  Texts with a number in them are mostly a status that changes (a
    //  count, a time): not for a translator.
    if (english.size() > 600 || !has_letters(english) || has_digits(english)){
        return;
    }
    State& s = state();
    QMutexLocker lock(&s.missing_lock);
    if (s.missing.size() < MISSING_LIMIT){
        s.missing.insert(english);
    }
}


//  A dictionary file: {"English text": "translation", ...}. Keys that start
//  with "@" are for the people who read the file. An empty translation
//  means "not translated yet" and is left out.
bool read_dictionary(const QString& path, QHash<QString, QString>& words, QString* error){
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)){
        if (error != nullptr){
            *error = "cannot open " + path;
        }
        return false;
    }
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()){
        if (error != nullptr){
            *error = path + ": " + (document.isObject() ? parse_error.errorString() : QString("not a JSON object"));
        }
        return false;
    }
    const QJsonObject object = document.object();
    for (auto iter = object.begin(); iter != object.end(); ++iter){
        if (iter.key().startsWith('@') || !iter.value().isString()){
            continue;
        }
        const QString value = iter.value().toString();
        if (value.isEmpty()){
            continue;
        }
        words.insert(iter.key(), value);
    }
    return true;
}

QString built_in_path(const QString& code){
    return ":/pocketpilot/lang/" + code + ".json";
}

//  The words of a language: the file in the program, then the file of the
//  user over it.
QHash<QString, QString> load_language(const QString& code){
    QHash<QString, QString> words;
    if (code == "en" || code.isEmpty()){
        return words;
    }
    QString error;
    if (QFile::exists(built_in_path(code))){
        if (!read_dictionary(built_in_path(code), words, &error)){
            global_logger_tagged().log("Language: " + error.toStdString(), COLOR_RED);
        }
    }
    const QString user_path = QDir(user_folder_path()).filePath(code + ".json");
    if (QFileInfo::exists(user_path)){
        if (!read_dictionary(user_path, words, &error)){
            global_logger_tagged().log("Language: " + error.toStdString(), COLOR_RED);
        }
    }
    return words;
}


//  ---------------------------------------------------------------------
//  The filter: which text of which widget
//  ---------------------------------------------------------------------

//  The text that was on a widget before it was translated, and the text it
//  was translated to, are kept on the widget. The second tells whether the
//  widget still shows what the filter put there (then the first is still
//  the English text) or a new text that the program set since.
struct Kind{
    const char* source;
    const char* shown;
};
constexpr Kind KIND_TEXT        = {"ppSrcText", "ppShownText"};
constexpr Kind KIND_TOOL_TIP    = {"ppSrcTip", "ppShownTip"};
constexpr Kind KIND_TITLE       = {"ppSrcTitle", "ppShownTitle"};
constexpr Kind KIND_PLACEHOLDER = {"ppSrcHint", "ppShownHint"};

//  The colors that a text sets for itself (<font color="blue">), changed just
//  enough to read on the theme in use: the programs write them for a white
//  background. What is readable already stays as it is, and so does
//  everything in the classic themes.
QString readable_colors(const QString& html){
    if (!theme_is_pocket() || !html.contains("color", Qt::CaseInsensitive)){
        return html;
    }
    static const QRegularExpression font_color(
        "(<font\\b[^>]*?\\bcolor\\s*=\\s*)([\"'])([^\"']+)\\2",
        QRegularExpression::CaseInsensitiveOption
    );
    QString out;
    qsizetype last = 0;
    QRegularExpressionMatchIterator matches = font_color.globalMatch(html);
    while (matches.hasNext()){
        const QRegularExpressionMatch match = matches.next();
        const QColor color(match.captured(3).trimmed());
        if (!color.isValid()){
            continue;
        }
        const Color before(0xff000000u | ((uint32_t)color.rgb() & 0xffffffu));
        const Color after = theme_readable(before);
        if ((uint32_t)after == (uint32_t)before){
            continue;
        }
        out += html.mid(last, match.capturedStart(3) - last);
        out += QColor((uint32_t)after).name();
        last = match.capturedEnd(3);
    }
    if (last == 0){
        return html;
    }
    return out + html.mid(last);
}


//  The text to show, for the text that is on the widget now. With "restore"
//  (English) the text before the translation comes back.
QString next_text(
    QObject& holder,
    const QByteArray& source_name,
    const QByteArray& shown_name,
    const QString& current,
    bool restore
){
    const QVariant stored = holder.property(source_name.constData());
    QString source = current;
    if (stored.isValid() && current == holder.property(shown_name.constData()).toString()){
        source = stored.toString();
    }
    const QString shown = readable_colors(restore ? source : tr_ui(source));
    if (stored.isValid() || shown != source){
        if (!stored.isValid() || stored.toString() != source){
            holder.setProperty(source_name.constData(), source);
        }
        if (holder.property(shown_name.constData()).toString() != shown){
            holder.setProperty(shown_name.constData(), shown);
        }
    }
    return shown;
}

void retext(
    QObject& holder, const Kind& kind, const QString& current, bool restore,
    const std::function<void(const QString&)>& set
){
    if (current.isEmpty()){
        return;
    }
    const QString shown = next_text(holder, kind.source, kind.shown, current, restore);
    if (shown != current){
        set(shown);
    }
}
void retext_indexed(
    QObject& holder, const char* base, int index, const QString& current, bool restore,
    const std::function<void(const QString&)>& set
){
    if (current.isEmpty()){
        return;
    }
    const QByteArray source_name = QByteArray("ppSrc") + base + QByteArray::number(index);
    const QByteArray shown_name = QByteArray("ppShown") + base + QByteArray::number(index);
    const QString shown = next_text(holder, source_name, shown_name, current, restore);
    if (shown != current){
        set(shown);
    }
}


void translate_actions(const QList<QAction*>& actions, bool restore){
    for (QAction* action : actions){
        if (action == nullptr || action->isSeparator()){
            continue;
        }
        retext(*action, KIND_TEXT, action->text(), restore, [action](const QString& text){
            action->setText(text);
        });
        retext(*action, KIND_TOOL_TIP, action->toolTip(), restore, [action](const QString& text){
            action->setToolTip(text);
        });
        if (QMenu* menu = action->menu()){
            translate_actions(menu->actions(), restore);
        }
    }
}


//  One widget. The drop-down lists (QComboBox) and the items of lists and
//  trees are left alone on purpose: the programs read what they hold back.
void translate_widget(QWidget& widget, bool restore){
    if (widget.property("ppNoTranslate").toBool()){
        return;
    }

    if (QLabel* label = qobject_cast<QLabel*>(&widget)){
        retext(widget, KIND_TEXT, label->text(), restore, [label](const QString& text){
            label->setText(text);
        });
    }else if (QAbstractButton* button = qobject_cast<QAbstractButton*>(&widget)){
        retext(widget, KIND_TEXT, button->text(), restore, [button](const QString& text){
            button->setText(text);
        });
    }else if (QGroupBox* group = qobject_cast<QGroupBox*>(&widget)){
        retext(widget, KIND_TEXT, group->title(), restore, [group](const QString& text){
            group->setTitle(text);
        });
    }else if (QTabBar* tabs = qobject_cast<QTabBar*>(&widget)){
        for (int c = 0; c < tabs->count(); c++){
            retext_indexed(widget, "Tab", c, tabs->tabText(c), restore, [tabs, c](const QString& text){
                tabs->setTabText(c, text);
            });
        }
    }else if (QMenu* menu = qobject_cast<QMenu*>(&widget)){
        translate_actions(menu->actions(), restore);
    }else if (QMenuBar* bar = qobject_cast<QMenuBar*>(&widget)){
        translate_actions(bar->actions(), restore);
    }else if (QLineEdit* line = qobject_cast<QLineEdit*>(&widget)){
        retext(widget, KIND_PLACEHOLDER, line->placeholderText(), restore, [line](const QString& text){
            line->setPlaceholderText(text);
        });
    }else if (QTextEdit* edit = qobject_cast<QTextEdit*>(&widget)){
        retext(widget, KIND_PLACEHOLDER, edit->placeholderText(), restore, [edit](const QString& text){
            edit->setPlaceholderText(text);
        });
    }else if (QPlainTextEdit* plain = qobject_cast<QPlainTextEdit*>(&widget)){
        retext(widget, KIND_PLACEHOLDER, plain->placeholderText(), restore, [plain](const QString& text){
            plain->setPlaceholderText(text);
        });
    }else if (QTableWidget* table = qobject_cast<QTableWidget*>(&widget)){
        for (int c = 0; c < table->columnCount(); c++){
            QTableWidgetItem* item = table->horizontalHeaderItem(c);
            if (item == nullptr){
                continue;
            }
            retext_indexed(widget, "Column", c, item->text(), restore, [item](const QString& text){
                item->setText(text);
            });
        }
    }else if (QTreeWidget* tree = qobject_cast<QTreeWidget*>(&widget)){
        if (QTreeWidgetItem* header = tree->headerItem()){
            for (int c = 0; c < tree->columnCount(); c++){
                retext_indexed(widget, "Header", c, header->text(c), restore, [header, c](const QString& text){
                    header->setText(c, text);
                });
            }
        }
    }

    retext(widget, KIND_TOOL_TIP, widget.toolTip(), restore, [&widget](const QString& text){
        widget.setToolTip(text);
    });
    if (widget.isWindow()){
        retext(widget, KIND_TITLE, widget.windowTitle(), restore, [&widget](const QString& text){
            widget.setWindowTitle(text);
        });
    }
}


class LanguageFilter : public QObject{
public:
    explicit LanguageFilter(QObject* parent)
        : QObject(parent)
        , m_timer(this)
    {
        //  What the program writes onto a label after it is shown (a status,
        //  a message) is caught here.
        m_timer.setInterval(800);
        connect(&m_timer, &QTimer::timeout, this, [this]{
            sweep(false);
        });
    }

    void set_active(bool active){
        if (active){
            if (!m_timer.isActive()){
                m_timer.start();
            }
        }else{
            m_timer.stop();
        }
    }

    //  Every window, shown or not. "restore": back to English.
    void sweep_all(bool restore){
        const QWidgetList widgets = QApplication::allWidgets();
        for (QWidget* widget : widgets){
            translate_widget(*widget, restore);
        }
    }
    //  The windows that are on screen.
    void sweep(bool restore){
        if (!g_active.load(std::memory_order_acquire)){
            return;
        }
        const QWidgetList widgets = QApplication::allWidgets();
        for (QWidget* widget : widgets){
            if (widget->isVisible()){
                translate_widget(*widget, restore);
            }
        }
    }

protected:
    virtual bool eventFilter(QObject* object, QEvent* event) override{
        const QEvent::Type type = event->type();
        if ((type == QEvent::Polish || type == QEvent::Show) && object->isWidgetType()){
            //  In English the text stays, but its colors are still made
            //  readable (and a text that was translated comes back).
            translate_widget(*static_cast<QWidget*>(object), !g_active.load(std::memory_order_acquire));
        }
        return false;
    }

private:
    QTimer m_timer;
};

LanguageFilter* g_filter = nullptr;


void save_missing_texts(){
    State& s = state();
    QStringList texts;
    QString code;
    {
        QMutexLocker lock(&s.missing_lock);
        texts = QStringList(s.missing.begin(), s.missing.end());
    }
    {
        QReadLocker lock(&s.lock);
        code = s.code;
    }
    if (code == "en" || texts.isEmpty()){
        return;
    }
    //  Only what the language does not have, and nothing that it already
    //  holds in the file of the user: that file is the one to copy from.
    texts.sort();
    QJsonObject object;
    object.insert("@language", code);
    object.insert("@what", "Texts of the program that this language does not translate yet. Fill in the ones you want, "
        "keep %1 and the tags as they are, and put them in " + code + ".json");
    for (const QString& text : texts){
        object.insert(text, QString());
    }
    QDir().mkpath(user_folder_path());
    QFile file(QDir(user_folder_path()).filePath("missing-" + code + ".json"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)){
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    }
}


class SettingListener : public ConfigOption::Listener{
public:
    virtual void on_config_value_changed(void*) override{
        if (qApp == nullptr){
            return;
        }
        QMetaObject::invokeMethod(qApp, []{
            apply_language();
        }, Qt::QueuedConnection);
    }
};
SettingListener g_setting_listener;
bool g_listening = false;


QString setting_code(){
    return code_of(GlobalSettings::instance().LANGUAGE.get());
}


}   //  namespace



const LanguageInfo* language_list(size_t& count){
    count = LANGUAGE_COUNT;
    return LANGUAGES;
}


QString tr_ui(const QString& english){
    if (english.isEmpty() || !g_active.load(std::memory_order_acquire)){
        return english;
    }
    State& s = state();
    bool translated_already = false;
    {
        QReadLocker lock(&s.lock);
        if (!s.words.isEmpty()){
            const auto iter = s.words.constFind(english);
            if (iter != s.words.constEnd()){
                return iter.value();
            }
            //  A text with spaces or line breaks around it: the text in
            //  the middle is looked up and the spaces stay.
            int begin = 0;
            int end = (int)english.size();
            while (begin < end && english[begin].isSpace()){
                begin++;
            }
            while (end > begin && english[end - 1].isSpace()){
                end--;
            }
            if (begin > 0 || end < english.size()){
                const auto inner = s.words.constFind(english.mid(begin, end - begin));
                if (inner != s.words.constEnd()){
                    return english.left(begin) + inner.value() + english.mid(end);
                }
            }
            translated_already = s.translated.contains(english);
        }
    }
    if (!translated_already && has_letters(english)){
        note_missing(english);
    }
    return english;
}
QString tr_ui(const char* english){
    return tr_ui(QString::fromUtf8(english));
}

QString current_language_code(){
    State& s = state();
    QReadLocker lock(&s.lock);
    return s.code;
}


namespace{

struct ChangeListener{
    QPointer<QObject> owner;
    std::function<void()> callback;
};
std::vector<ChangeListener>& change_listeners(){
    static std::vector<ChangeListener> instance;
    return instance;
}

//  Puts a language into use: its dictionary, and every window that is open.
void use_language(const QString& code){
    QHash<QString, QString> words = load_language(code);
    const size_t count = (size_t)words.size();
    QSet<QString> translated;
    for (auto iter = words.constBegin(); iter != words.constEnd(); ++iter){
        translated.insert(iter.value());
    }
    State& s = state();
    {
        QWriteLocker lock(&s.lock);
        s.words = std::move(words);
        s.translated = std::move(translated);
        s.code = code;
    }
    const bool english = code == "en";
    const bool was_active = g_active.exchange(!english);
    global_logger_tagged().log("Language: " + code.toStdString() + ", " + std::to_string(count) + " texts.", COLOR_BLUE);
    if (g_filter != nullptr){
        //  Everything that is open: English again, or the new language.
        if (!english || was_active){
            g_filter->sweep_all(english);
        }
        g_filter->set_active(!english);
    }
    //  The windows that make texts themselves. A copy: a callback may open
    //  a window that adds a listener.
    const std::vector<ChangeListener> listeners = change_listeners();
    for (const ChangeListener& listener : listeners){
        if (!listener.owner.isNull() && listener.callback){
            listener.callback();
        }
    }
}

}   //  namespace


void apply_language(){
    use_language(setting_code());
}

void refresh_text_colors(){
    if (g_filter != nullptr){
        g_filter->sweep_all(!g_active.load(std::memory_order_acquire));
    }
}

void preview_language(const QString& code){
    use_language(code);
}

void on_language_changed(QObject* owner, std::function<void()> callback){
    std::vector<ChangeListener>& list = change_listeners();
    //  The ones whose window is gone.
    list.erase(std::remove_if(list.begin(), list.end(), [](const ChangeListener& item){
        return item.owner.isNull();
    }), list.end());
    list.push_back({QPointer<QObject>(owner), std::move(callback)});
}


void install_language_support(QApplication& application){
    if (g_filter == nullptr){
        g_filter = new LanguageFilter(&application);
        application.installEventFilter(g_filter);
        QObject::connect(&application, &QCoreApplication::aboutToQuit, &application, []{
            save_missing_texts();
        });
    }
    if (!g_listening){
        GlobalSettings::instance().LANGUAGE.add_listener(g_setting_listener);
        g_listening = true;
    }
    apply_language();
}


QString user_language_folder(){
    const QString path = user_folder_path();
    QDir().mkpath(path);
    return path;
}


//  ---------------------------------------------------------------------
//  The setup at the first start
//  ---------------------------------------------------------------------

namespace{


//  Keeps the language that was chosen: the setting, and for "Other" the file
//  to start from with the folder open.
void keep_language(const QString& code){
    GlobalSettings& settings = GlobalSettings::instance();
    settings.LANGUAGE.set(language_of(code));
    if (code == CUSTOM_CODE){
        //  A starting point for the file, and the folder open for the user.
        const QString folder = user_language_folder();
        const QString path = QDir(folder).filePath("custom.json");
        if (!QFileInfo::exists(path)){
            QFile file(path);
            if (file.open(QIODevice::WriteOnly)){
                QJsonObject object;
                object.insert("@what", "Your own language. Put the English text of the program as the key and your "
                    "translation as the value. Keep %1 and the tags as they are. Texts that are not here stay English.");
                object.insert("Settings", "");
                file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
            }
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    }
}

//  Keeps the console that was chosen.
void keep_console(const QString& code){
    GlobalSettings::instance().GAME_CONSOLE.set(console_of(code));
}

//  The lines that the program writes, kept for the log panel on the screen
//  with the programs (what "Copy for Claude" copies). It starts listening the
//  first time the setup is asked, with the lines that the program kept until
//  then, and keeps the last 1000.
class SetupLogTap : public Logger{
public:
    virtual void log(const std::string& msg, Color color = Color()) override{
        add(QDateTime::currentMSecsSinceEpoch(), msg, color);
    }

    void listen(){
        if (m_listening.exchange(true)){
            return;
        }
        //  What was written before this: the time is in the line itself.
        const QRegularExpression stamp("^\\d{4}-\\d{1,2}-\\d{1,2} (\\d{1,2}):(\\d{2}):(\\d{2})(?:\\.(\\d+))? - ");
        const QDateTime now = QDateTime::currentDateTime();
        for (const LogLine& line : global_last_log_history().get_recent(400)){
            qint64 msec = now.toMSecsSinceEpoch();
            const QString text = QString::fromStdString(line.text);
            const QRegularExpressionMatch match = stamp.match(text);
            if (match.hasMatch()){
                QDateTime when(now.date(), QTime(match.captured(1).toInt(), match.captured(2).toInt(), match.captured(3).toInt()));
                if (when > now){
                    when = when.addDays(-1);
                }
                msec = when.toMSecsSinceEpoch();
            }
            add(msec, line.text, line.color);
        }
        global_multi_logger().add_listener(*this);
    }

    void poll(quint64 after, std::vector<SetupLogLine>& out){
        std::lock_guard<std::mutex> guard(m_lock);
        for (const SetupLogLine& line : m_lines){
            if (line.serial > after){
                out.push_back(line);
            }
        }
    }

private:
    void add(qint64 msec, const std::string& msg, Color color){
        QString text = QString::fromStdString(msg);
        //  The time of the line is shown on its own.
        const qsizetype cut = text.indexOf(" - [");
        if (cut >= 0 && cut < 40){
            text = text.mid(cut + 3);
        }
        SetupLogLine line;
        line.msec = msec;
        line.text = text;
        if (color == COLOR_RED){
            line.level = 2;
        }else if (color == COLOR_ORANGE || color == COLOR_YELLOW){
            line.level = 1;
        }else if (text.contains("error", Qt::CaseInsensitive) || text.contains("exception", Qt::CaseInsensitive)){
            line.level = 2;
        }else if (text.contains("warning", Qt::CaseInsensitive)){
            line.level = 1;
        }
        std::lock_guard<std::mutex> guard(m_lock);
        line.serial = ++m_serial;
        m_lines.push_back(std::move(line));
        while (m_lines.size() > 1000){
            m_lines.pop_front();
        }
    }

    std::atomic<bool> m_listening{false};
    std::mutex m_lock;
    std::deque<SetupLogLine> m_lines;
    quint64 m_serial = 0;
};
SetupLogTap& setup_log_tap(){
    static SetupLogTap tap;
    return tap;
}

std::function<void(std::vector<HomeGame>&, std::vector<HomeProgram>&)> g_game_lister;
std::function<void(const QString&, const QString&)> g_game_opener;
std::function<std::unique_ptr<SetupProgramOptions>(QWidget*, int, const QString&)> g_options_maker;
//  A game (and the program in it) that was chosen while there was no window
//  to open it.
QString g_chosen_game;
QString g_chosen_program;

void run_and_keep(bool ask_language, bool ask_board, bool play_intro = false, bool ask_game = false){
    GlobalSettings& settings = GlobalSettings::instance();

    SetupRequest request;
    request.ask_language = ask_language;
    request.ask_board = ask_board;
    //  The console is a screen of the setup: asked where the board is asked.
    request.ask_console = ask_board;
    request.console_code = console_code_of(settings.GAME_CONSOLE.get());
    request.language_code = setting_code();
    request.play_intro = play_intro;
    //  The output log on the screen with the programs, and the build for the
    //  report that it copies.
    setup_log_tap().listen();
    request.log_poll = [](quint64 after, std::vector<SetupLogLine>& out){
        setup_log_tap().poll(after, out);
    };
    request.build = QString::fromStdString(PROGRAM_NAME + " " + PROGRAM_VERSION + ", build " + PROGRAM_BUILD);
    request.make_options = g_options_maker;
    //  The way back to the overview of the installations, in the corner.
    request.show_overview = []{
        show_instances_overview();
    };
    //  At the start the main window is made after this: the screen stays up
    //  until it is there. (Opened from the settings, it is there already.)
    request.hold_screen = (g_game_opener == nullptr);
    //  The choice of a game: a screen of its own after the installation of the
    //  board, and, when the setting asks for it, at the start.
    if ((ask_board || ask_game) && g_game_lister){
        g_game_lister(request.games, request.programs);
        global_logger_tagged().log(
            "Setup: " + std::to_string(request.games.size()) + " games to choose from, with " +
            std::to_string(request.programs.size()) + " programs."
        );
        //  The setting can leave the choice of a program out: the game then
        //  opens with its list, as before.
        if (!settings.ASK_PROGRAM){
            request.programs.clear();
        }
    }
    request.ask_game = ask_game && !request.games.empty();
    const SetupResult result = run_setup(request);
    if (play_intro){
        global_logger_tagged().log("Intro: done, " + result.intro_note.toStdString() + ".");
    }

    //  Asked is asked: a closed window does not bring the question back at
    //  every start (it is in the settings, under "Open the setup").
    if (ask_language){
        settings.ASK_LANGUAGE = false;
    }
    if (ask_board){
        settings.ASK_SETUP = false;
    }
    if (ask_language && result.language_chosen){
        keep_language(result.language_code);
    }
    if (result.console_chosen){
        keep_console(result.console_code);
        global_logger_tagged().log("Setup: the console is " + result.console_code.toStdString() + ".");
    }
    try{
        PERSISTENT_SETTINGS().write();
    }catch (...){
        global_logger_tagged().log("Setup: the answers could not be written to the settings file.", COLOR_RED);
    }
    //  What was only tried goes back to the language of the settings.
    apply_language();

    //  The game that was chosen: shown in the window when there is one, kept
    //  for the window that is made next when there is not.
    if (!result.game.isEmpty()){
        if (g_game_opener){
            g_game_opener(result.game, result.program);
        }else{
            g_chosen_game = result.game;
            g_chosen_program = result.program;
        }
    }
}


}   //  namespace


void ask_setup_if_needed(){
    GlobalSettings& settings = GlobalSettings::instance();
    const bool ask_language = settings.ASK_LANGUAGE;
    const bool ask_board = settings.ASK_SETUP;
    if (!ask_language && !ask_board){
        return;
    }
    run_and_keep(ask_language, ask_board);
}

void open_setup(){
    run_and_keep(true, true);
}

AppView* make_main_view(
    QWidget* parent,
    std::vector<HomeGame> games, std::vector<HomeProgram> programs,
    AppActions actions, bool test
){
    GlobalSettings& settings = GlobalSettings::instance();
    SetupRequest request;
    request.console_code = console_code_of(settings.GAME_CONSOLE.get());
    request.language_code = setting_code();
    request.games = std::move(games);
    request.programs = std::move(programs);
    setup_log_tap().listen();
    request.log_poll = [](quint64 after, std::vector<SetupLogLine>& out){
        setup_log_tap().poll(after, out);
    };
    request.build = QString::fromStdString(PROGRAM_NAME + " " + PROGRAM_VERSION + ", build " + PROGRAM_BUILD);
    request.make_options = g_options_maker;

    //  A console that is picked on the way is kept at once, as the setup does
    //  when it is over.
    std::function<void(const QString&)> after = std::move(actions.console_chosen);
    actions.console_chosen = [after](const QString& code){
        keep_console(code);
        global_logger_tagged().log("Setup: the console is " + code.toStdString() + ".");
        try{
            PERSISTENT_SETTINGS().write();
        }catch (...){
            global_logger_tagged().log("Setup: the console could not be written to the settings file.", COLOR_RED);
        }
        if (after){
            after(code);
        }
    };
    return make_app_view(parent, request, std::move(actions), test);
}

void show_start_screens(bool play_intro, bool ask_setup){
    GlobalSettings& settings = GlobalSettings::instance();
    const bool ask_language = ask_setup && settings.ASK_LANGUAGE;
    const bool ask_board = ask_setup && settings.ASK_SETUP;
    const bool ask_game = ask_setup && settings.ASK_GAME;
    if (!ask_language && !ask_board && !ask_game && !play_intro){
        return;
    }
    run_and_keep(ask_language, ask_board, play_intro, ask_game);
}

size_t count_games_for_setup(){
    std::vector<HomeGame> games;
    std::vector<HomeProgram> programs;
    if (g_game_lister){
        g_game_lister(games, programs);
    }
    return games.size();
}
size_t count_programs_for_setup(){
    std::vector<HomeGame> games;
    std::vector<HomeProgram> programs;
    if (g_game_lister){
        g_game_lister(games, programs);
    }
    return programs.size();
}

size_t count_options_for_setup(){
    std::vector<HomeGame> games;
    std::vector<HomeProgram> programs;
    if (!g_game_lister || !g_options_maker){
        return 0;
    }
    g_game_lister(games, programs);
    //  The first program of each game: made as the panel makes them, and
    //  deleted again (the parent outlives them).
    QWidget parent;
    std::set<int> seen;
    size_t made = 0;
    for (const HomeProgram& program : programs){
        if (!seen.insert(program.game).second){
            continue;
        }
        std::unique_ptr<SetupProgramOptions> options = g_options_maker(&parent, program.game, program.name);
        if (options != nullptr && options->widget() != nullptr){
            made++;
        }
    }
    return made;
}

void set_game_lister(std::function<void(std::vector<HomeGame>&, std::vector<HomeProgram>&)> lister){
    g_game_lister = std::move(lister);
}
void set_options_maker(std::function<std::unique_ptr<SetupProgramOptions>(QWidget* parent, int game, const QString& program)> maker){
    g_options_maker = std::move(maker);
}
void set_game_opener(std::function<void(const QString& game, const QString& program)> opener){
    g_game_opener = std::move(opener);
}
QString take_chosen_game(){
    QString game;
    game.swap(g_chosen_game);
    return game;
}
QString take_chosen_program(){
    QString program;
    program.swap(g_chosen_program);
    return program;
}


//  ---------------------------------------------------------------------
//  The test
//  ---------------------------------------------------------------------

QStringList check_translation(const QString& english, const QString& translated){
    QStringList problems;
    if (translated.trimmed().isEmpty()){
        problems << "empty translation";
        return problems;
    }

    //  The parts that must come through as they are.
    static const QRegularExpression placeholders("%\\d+|%n");
    static const QRegularExpression tags("<[^>]+>");
    auto collect = [](const QRegularExpression& pattern, const QString& text){
        QStringList found;
        QRegularExpressionMatchIterator iter = pattern.globalMatch(text);
        while (iter.hasNext()){
            found << iter.next().captured(0);
        }
        found.sort();
        return found;
    };
    if (collect(placeholders, english) != collect(placeholders, translated)){
        problems << "the %1, %2 ... differ";
    }
    if (collect(tags, english) != collect(tags, translated)){
        problems << "the tags (<b>, <br>, <a href=...>) differ";
    }
    if (english.count('\n') != translated.count('\n')){
        problems << "the line breaks differ";
    }
    const auto lead = [](const QString& text){
        int c = 0;
        while (c < text.size() && text[c].isSpace()){
            c++;
        }
        return text.left(c);
    };
    const auto tail = [](const QString& text){
        int c = (int)text.size();
        while (c > 0 && text[c - 1].isSpace()){
            c--;
        }
        return text.mid(c);
    };
    if (lead(english) != lead(translated) || tail(english) != tail(translated)){
        problems << "the spaces at the start or the end differ";
    }
    return problems;
}


namespace{


struct TestResult{
    int failures = 0;
    void check(bool ok, const QString& what){
        if (!ok){
            failures++;
            global_logger_command_line().log("Language test: FAILED: " + what.toStdString(), COLOR_RED);
        }
    }
};


//  Windows with some text on them, translated and brought back.
void test_filter(TestResult& result, const QString& code, const QHash<QString, QString>& words){
    //  A few entries that are plain text.
    QStringList keys;
    for (auto iter = words.begin(); iter != words.end(); ++iter){
        const QString& key = iter.key();
        if (key.size() >= 4 && key.size() < 60 && !key.contains('%') && !key.contains('<') && !key.contains('&')
            && key.trimmed() == key && iter.value() != key
        ){
            keys << key;
        }
    }
    keys.sort();
    if (keys.size() < 5){
        result.check(false, code + ": too few plain texts to try the filter on");
        return;
    }

    QWidget window;
    QVBoxLayout* layout = new QVBoxLayout(&window);
    QLabel* label = new QLabel(keys[0], &window);
    QPushButton* button = new QPushButton(keys[1], &window);
    button->setToolTip(keys[2]);
    QGroupBox* group = new QGroupBox(keys[3], &window);
    QLineEdit* edit = new QLineEdit(&window);
    edit->setPlaceholderText(keys[4]);
    QTabBar* tabs = new QTabBar(&window);
    tabs->addTab(keys[0]);
    QLabel* kept = new QLabel("Text that no language has", &window);
    layout->addWidget(label);
    layout->addWidget(button);
    layout->addWidget(group);
    layout->addWidget(edit);
    layout->addWidget(tabs);
    layout->addWidget(kept);
    window.setWindowTitle(keys[1]);

    //  The language as the settings would have set it.
    State& s = state();
    {
        QWriteLocker lock(&s.lock);
        s.words = words;
        s.code = code;
    }
    g_active.store(true);
    g_filter->set_active(true);

    window.show();
    QApplication::processEvents();
    result.check(label->text() == words[keys[0]], code + ": label");
    result.check(button->text() == words[keys[1]], code + ": button");
    result.check(button->toolTip() == words[keys[2]], code + ": tool tip");
    result.check(group->title() == words[keys[3]], code + ": group box");
    result.check(edit->placeholderText() == words[keys[4]], code + ": placeholder");
    result.check(tabs->tabText(0) == words[keys[0]], code + ": tab");
    result.check(window.windowTitle() == words[keys[1]], code + ": window title");
    result.check(kept->text() == "Text that no language has", code + ": a text without translation stays");

    //  A text the program sets later.
    label->setText(keys[2]);
    g_filter->sweep(false);
    result.check(label->text() == words[keys[2]], code + ": text set after the window was shown");

    //  Back to English.
    g_active.store(false);
    g_filter->sweep_all(true);
    g_filter->set_active(false);
    result.check(label->text() == keys[2], code + ": back to English (label)");
    result.check(button->text() == keys[1], code + ": back to English (button)");
    result.check(button->toolTip() == keys[2], code + ": back to English (tool tip)");
    result.check(tabs->tabText(0) == keys[0], code + ": back to English (tab)");
    result.check(window.windowTitle() == keys[1], code + ": back to English (window title)");
    {
        QWriteLocker lock(&s.lock);
        s.words.clear();
        s.code = "en";
    }
}


}   //  namespace


int run_language_test(QApplication& application){
    TestResult result;
    install_language_support(application);

    //  The dictionaries that come with the program.
    QSet<QString> reference_keys;
    QString reference_code;
    QHash<QString, QHash<QString, QString>> all;
    for (size_t c = 1; c < LANGUAGE_COUNT; c++){
        const QString code = LANGUAGES[c].code;
        QHash<QString, QString> words;
        QString error;
        const bool read = read_dictionary(built_in_path(code), words, &error);
        result.check(read, code + ": " + error);
        if (!read){
            continue;
        }
        result.check(words.size() >= 20, code + ": only " + QString::number(words.size()) + " texts");
        int problems = 0;
        for (auto iter = words.begin(); iter != words.end(); ++iter){
            const QStringList found = check_translation(iter.key(), iter.value());
            for (const QString& problem : found){
                if (problems++ < 12){
                    result.check(false, code + ": \"" + iter.key().left(70) + "\": " + problem);
                }
            }
        }
        result.check(problems == 0, code + ": " + QString::number(problems) + " translations that change what must stay");

        //  The same texts in every language.
        const QSet<QString> keys = QSet<QString>(words.keyBegin(), words.keyEnd());
        if (reference_code.isEmpty()){
            reference_code = code;
            reference_keys = keys;
        }else{
            const QSet<QString> missing = reference_keys - keys;
            const QSet<QString> extra = keys - reference_keys;
            int shown = 0;
            for (const QString& key : missing){
                if (shown++ < 8){
                    result.check(false, code + ": missing, but " + reference_code + " has it: \"" + key.left(70) + "\"");
                }
            }
            shown = 0;
            for (const QString& key : extra){
                if (shown++ < 8){
                    result.check(false, code + ": extra, " + reference_code + " does not have it: \"" + key.left(70) + "\"");
                }
            }
            result.check(missing.isEmpty() && extra.isEmpty(), code + ": the texts differ from " + reference_code);
        }
        global_logger_command_line().log(
            "Language test: " + code.toStdString() + ": " + std::to_string(words.size()) + " texts.",
            COLOR_BLUE
        );
        all.insert(code, words);
    }

    //  The filter, on windows.
    for (auto iter = all.begin(); iter != all.end(); ++iter){
        test_filter(result, iter.key(), iter.value());
    }

    //  tr_ui: no language, no change; spaces around a text stay.
    {
        State& s = state();
        const QString sample = all.isEmpty() ? QString() : all.begin().key();
        result.check(tr_ui("Text that no language has") == "Text that no language has", "tr_ui without language");
        if (!sample.isEmpty()){
            const QHash<QString, QString>& words = all[sample];
            QString key;
            for (auto iter = words.begin(); iter != words.end(); ++iter){
                if (iter.key().size() >= 4 && !iter.key().contains('%') && iter.key().trimmed() == iter.key()){
                    key = iter.key();
                    break;
                }
            }
            {
                QWriteLocker lock(&s.lock);
                s.words = words;
                s.code = sample;
            }
            g_active.store(true);
            result.check(tr_ui(" " + key + "\n") == " " + words[key] + "\n", "tr_ui keeps the spaces");
            g_active.store(false);
            {
                QWriteLocker lock(&s.lock);
                s.words.clear();
                s.code = "en";
            }
        }
    }

    global_logger_command_line().log(
        result.failures == 0 ? "Language test: passed." : "Language test: FAILED.",
        result.failures == 0 ? COLOR_BLUE : COLOR_RED
    );
    return result.failures == 0 ? 0 : 1;
}



}
}

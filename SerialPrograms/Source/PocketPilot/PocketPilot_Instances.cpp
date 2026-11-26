/*  Claude GG Installations
 *
 *  Part of Claude GG (PocketPilot before), which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QPainter>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Logging/GlobalLogger.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/PersistentSettings.h"
#include "CommonFramework/ImageTypes/ImageRGB32_Qt.h"
#include "CommonFramework/VideoPipeline/VideoSession.h"
#include "PocketPilot_Setup.h"
#include "PocketPilot_Instances.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace PokemonAutomation{
namespace PocketPilot{


const char* const INSTANCE_OPTION = "--installation";
const char* const INSTANCES_OPTION = "--installations";
const char* const INSTANCE_TEST_OPTION = "--installation-test";
const char* const INSTANCES_HUB_TEST_OPTION = "--installations-hub-test";


namespace{

bool g_overview = false;
bool g_test_run = false;

const char WINDOW_PROPERTY[] = "claudegg-instance-window";

std::string with_slash(std::string path){
    return path_with_slash(std::move(path));
}

QString to_qt(const std::string& text){
    return QString::fromStdString(text);
}

//  A whole small file, read in one go and closed (so that a writer can replace
//  it in the next moment: on Windows a file that is open cannot be replaced).
QByteArray read_all(const QString& path){
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)){
        return QByteArray();
    }
    return file.readAll();
}

QJsonObject read_object(const QString& path, bool* found = nullptr){
    const QByteArray bytes = read_all(path);
    if (found != nullptr){
        *found = false;
    }
    if (bytes.isEmpty()){
        return QJsonObject();
    }
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()){
        return QJsonObject();
    }
    if (found != nullptr){
        *found = true;
    }
    return document.object();
}

bool write_object(const QString& path, const QJsonObject& object){
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)){
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    return file.commit();
}

//  Numbers by value, then the rest by name.
bool id_before(const QString& a, const QString& b){
    bool a_number = false;
    bool b_number = false;
    const qulonglong x = a.toULongLong(&a_number);
    const qulonglong y = b.toULongLong(&b_number);
    if (a_number && b_number && x != y){
        return x < y;
    }
    if (a_number != b_number){
        return a_number;
    }
    return a < b;
}

}


//  ---------------------------------------------------------------------
//  Which installation this is
//  ---------------------------------------------------------------------

bool valid_instance_id(const QString& id){
    if (id.isEmpty() || id.size() > 32){
        return false;
    }
    for (const QChar c : id){
        const ushort u = c.unicode();
        const bool fine =
            (u >= '0' && u <= '9') ||
            (u >= 'a' && u <= 'z') ||
            (u >= 'A' && u <= 'Z') ||
            u == '-' || u == '_';
        if (!fine){
            return false;
        }
    }
    return true;
}

bool parse_instance_options(int argc, char* argv[], std::string& id, bool& overview, QString* error){
    id.clear();
    overview = false;
    for (int c = 1; c < argc; c++){
        if (std::string(argv[c]) == INSTANCES_OPTION || std::string(argv[c]) == INSTANCES_HUB_TEST_OPTION){
            overview = true;
        }else if (std::string(argv[c]) == INSTANCE_OPTION){
            const QString value = c + 1 < argc ? QString::fromUtf8(argv[c + 1]) : QString();
            if (!valid_instance_id(value)){
                if (error != nullptr){
                    *error = QString(INSTANCE_OPTION) +
                        " needs the number or name of an installation: letters, digits, - and _, at most 32.";
                }
                id.clear();
                overview = false;
                return false;
            }
            id = value.toStdString();
            c++;
        }
    }
    return true;
}

bool read_instance_options(int argc, char* argv[], QString* error){
    //  The folders are made from the id: it cannot change once they were asked for.
    if (instance_paths_fixed()){
        if (error != nullptr){
            *error = "The installation was asked for after its folders were.";
        }
        return false;
    }
    std::string id;
    bool overview = false;
    if (!parse_instance_options(argc, argv, id, overview, error)){
        return false;
    }
    if (!id.empty()){
        set_current_instance(id);
    }
    g_overview = overview;
    return true;
}

bool is_overview_process(){
    return g_overview;
}

void set_test_run(bool test_run){
    g_test_run = test_run;
}
bool is_test_run(){
    return g_test_run;
}


//  ---------------------------------------------------------------------
//  Where the data is: PocketPilot_InstancePaths.cpp
//  ---------------------------------------------------------------------

QString instance_setting_key_for(const QString& key, const std::string& id){
    if (id == MAIN_INSTANCE){
        return key;
    }
    return key + "-" + to_qt(id);
}
QString instance_setting_key(const QString& key){
    return instance_setting_key_for(key, current_instance());
}


//  ---------------------------------------------------------------------
//  The list of installations
//  ---------------------------------------------------------------------

InstanceRegistry::InstanceRegistry(const std::string& scratch)
    : m_scratch(to_qt(with_slash(scratch)))
{}

QString InstanceRegistry::root() const{
    return m_scratch + "Installations/";
}
QString InstanceRegistry::folder(const QString& id) const{
    return root() + id + "/";
}

InstanceInfo InstanceRegistry::read(const QString& id, qint64 now, bool with_picture) const{
    InstanceInfo info;
    info.id = id;
    info.main = id == MAIN_INSTANCE;
    const QString where = folder(id);

    const QJsonObject made = read_object(where + "installation.json");
    info.name = made.value("name").toString();
    const qint64 created = (qint64)made.value("created").toDouble();

    bool has_status = false;
    const QJsonObject status = read_object(where + "status.json", &has_status);
    if (has_status){
        const qint64 updated = (qint64)status.value("updated").toDouble();
        if (std::llabs(now - updated) <= INSTANCE_ALIVE_MSEC){
            info.running = true;
            info.pid = (qint64)status.value("pid").toDouble();
            info.state = status.value("state").toString();
            info.title = status.value("title").toString();
        }
    }
    info.starting = !info.running && !has_status && created > 0 && now - created >= 0 && now - created < INSTANCE_STARTING_MSEC;

    if (with_picture){
        const QByteArray bytes = read_all(where + "miniature.jpg");
        if (!bytes.isEmpty()){
            info.picture = QImage::fromData(bytes);
        }
    }
    return info;
}

std::vector<InstanceInfo> InstanceRegistry::list(qint64 now, bool with_pictures) const{
    QStringList ids;
    ids << MAIN_INSTANCE;
    const QStringList found = QDir(root()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : found){
        if (valid_instance_id(name) && name != MAIN_INSTANCE){
            ids << name;
        }
    }
    std::sort(ids.begin() + 1, ids.end(), id_before);

    std::vector<InstanceInfo> result;
    for (const QString& id : ids){
        result.emplace_back(read(id, now, with_pictures));
    }
    return result;
}

QString InstanceRegistry::create(qint64 now) const{
    QDir().mkpath(root());
    //  The next number after the ones that are there.
    qulonglong highest = 1;
    const QStringList found = QDir(root()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : found){
        bool number = false;
        const qulonglong value = name.toULongLong(&number);
        if (number && valid_instance_id(name)){
            highest = std::max(highest, value);
        }
    }
    for (int attempt = 0; attempt < 50; attempt++){
        const QString id = QString::number(highest + 1 + (qulonglong)attempt);
        //  mkdir() does not make a folder that is there already.
        if (!QDir(root()).mkdir(id)){
            continue;
        }
        QJsonObject made;
        made["created"] = (double)now;
        if (!write_object(folder(id) + "installation.json", made)){
            QDir(folder(id)).removeRecursively();
            return QString();
        }
        return id;
    }
    return QString();
}

bool InstanceRegistry::remove(const QString& id, qint64 now) const{
    if (!valid_instance_id(id) || id == MAIN_INSTANCE){
        return false;
    }
    if (read(id, now, false).running){
        return false;
    }
    const QString where = folder(id);
    if (!QDir(where).exists()){
        return false;
    }
    try{
        Filesystem::remove_all(Filesystem::Path(where.toStdString()));
    }catch (const std::exception&){
        return false;
    }
    return !QDir(where).exists();
}


//  ---------------------------------------------------------------------
//  What an installation tells about itself
//  ---------------------------------------------------------------------

bool write_instance_report_to(
    const QString& folder, qint64 pid, qint64 now,
    const QString& state, const QString& title, const QImage& picture
){
    if (!QDir().mkpath(folder)){
        return false;
    }
    QJsonObject status;
    status["pid"] = (double)(pid != 0 ? pid : QCoreApplication::applicationPid());
    status["updated"] = (double)(now != 0 ? now : QDateTime::currentMSecsSinceEpoch());
    status["state"] = state;
    status["title"] = title;
    bool fine = write_object(folder + "status.json", status);
    if (!picture.isNull()){
        QSaveFile file(folder + "miniature.jpg");
        if (file.open(QIODevice::WriteOnly)){
            picture.save(&file, "JPG", 80);
            fine = file.commit() && fine;
        }else{
            fine = false;
        }
    }
    return fine;
}

void write_instance_report(const QString& state, const QString& title, const QImage& picture, qint64 now){
    if (g_test_run){
        return;
    }
    write_instance_report_to(
        to_qt(instance_meta_path(shared_base_path(), current_instance())),
        0, now, state, title, picture
    );
}

void clear_instance_report(){
    if (g_test_run){
        return;
    }
    QFile::remove(to_qt(instance_meta_path(shared_base_path(), current_instance())) + "status.json");
}

QImage fit_miniature(const QImage& picture){
    if (picture.isNull() || picture.width() <= 0 || picture.height() <= 0){
        return QImage();
    }
    QImage result(MINIATURE_WIDTH, MINIATURE_HEIGHT, QImage::Format_RGB32);
    result.fill(Qt::black);
    const QImage scaled = picture.scaled(
        MINIATURE_WIDTH, MINIATURE_HEIGHT, Qt::KeepAspectRatio, Qt::SmoothTransformation
    );
    QPainter painter(&result);
    painter.drawImage(
        (MINIATURE_WIDTH - scaled.width()) / 2,
        (MINIATURE_HEIGHT - scaled.height()) / 2,
        scaled
    );
    return result;
}

namespace{

//  A window drawn at the size of a miniature, not at its own size (a window
//  on a large screen is large).
QImage render_miniature(QWidget* window){
    const QSize size = window->size();
    if (size.width() <= 0 || size.height() <= 0){
        return QImage();
    }
    QImage result(MINIATURE_WIDTH, MINIATURE_HEIGHT, QImage::Format_RGB32);
    result.fill(Qt::black);
    const double scale = std::min(
        (double)MINIATURE_WIDTH / size.width(),
        (double)MINIATURE_HEIGHT / size.height()
    );
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.translate(
        (MINIATURE_WIDTH - size.width() * scale) / 2.0,
        (MINIATURE_HEIGHT - size.height() * scale) / 2.0
    );
    painter.scale(scale, scale);
    window->render(&painter);
    painter.end();
    return result;
}

}

QImage instance_picture(QWidget* window){
    //  The picture of a capture card that runs: what the Switch shows.
    const VideoSnapshot snapshot = latest_video_snapshot(std::chrono::seconds(3));
    if (snapshot){
        return fit_miniature(to_QImage_ref(*snapshot.frame).copy());
    }
    //  Else the window, when it is there to be drawn.
    if (window == nullptr || !window->isVisible() || window->isMinimized()){
        return QImage();
    }
    return render_miniature(window);
}


//  ---------------------------------------------------------------------
//  One installation runs once
//  ---------------------------------------------------------------------

InstanceGuard::InstanceGuard(const QString& folder, const QString& file_name){
    QDir().mkpath(folder);
    m_lock = std::make_unique<QLockFile>(folder + file_name);
    //  Only a process that is gone makes a lock stale, not the time.
    m_lock->setStaleLockTime(0);
    if (!m_lock->tryLock(0)){
        m_lock.reset();
    }
}
InstanceGuard::~InstanceGuard() = default;
bool InstanceGuard::acquired() const{
    return m_lock != nullptr;
}


//  ---------------------------------------------------------------------
//  The windows talk
//  ---------------------------------------------------------------------

QString instance_link_name(const std::string& scratch, const QString& what){
    //  The same for every process that was started in the same folder.
    const QByteArray hash = QCryptographicHash::hash(
        to_qt(with_slash(scratch)).toLower().toUtf8(), QCryptographicHash::Sha1
    ).toHex().left(12);
    return "ClaudeGG-" + QString::fromLatin1(hash) + "-" + what;
}

InstanceLink::InstanceLink(const QString& name, std::function<void(const QString& command)> handler){
    m_server = std::make_unique<QLocalServer>();
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer* server = m_server.get();
    QObject::connect(server, &QLocalServer::newConnection, server, [server, handler]{
        while (QLocalSocket* socket = server->nextPendingConnection()){
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, handler]{
                //  One line is one command.
                while (socket->canReadLine()){
                    const QString command = QString::fromUtf8(socket->readLine()).trimmed();
                    if (handler){
                        handler(command);
                    }
                    socket->write("ok\n");
                    socket->flush();
                    socket->disconnectFromServer();
                }
            });
        }
    });
    if (!server->listen(name)){
        //  A name that nobody listens on can still be there from a process that
        //  did not end well (a file, on Linux and macOS). It goes, and again.
        QLocalSocket probe;
        probe.connectToServer(name);
        const bool taken = probe.state() != QLocalSocket::UnconnectedState || probe.waitForConnected(100);
        probe.abort();
        if (!taken){
            QLocalServer::removeServer(name);
            server->listen(name);
        }
    }
}
InstanceLink::~InstanceLink() = default;
bool InstanceLink::listening() const{
    return m_server != nullptr && m_server->isListening();
}

bool send_instance_command(const QString& name, const QString& command, int timeout_msec){
#ifdef _WIN32
    //  The window that is asked to come forward may do so: the user pressed a
    //  button here, a moment ago.
    AllowSetForegroundWindow(ASFW_ANY);
#endif
    QLocalSocket socket;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    bool answered = false;
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QLocalSocket::errorOccurred, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QLocalSocket::connected, &socket, [&]{
        socket.write((command + "\n").toUtf8());
        socket.flush();
    });
    QObject::connect(&socket, &QLocalSocket::readyRead, &loop, [&]{
        if (socket.readAll().startsWith("ok")){
            answered = true;
        }
        loop.quit();
    });
    timer.start(timeout_msec);
    socket.connectToServer(name);
    if (socket.state() == QLocalSocket::UnconnectedState){
        //  Nobody is there: it was told already.
        return false;
    }
    //  The events of the window itself go on (the answer of a process in this
    //  thread, in the test), the keys and the mouse wait.
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    socket.abort();
    return answered;
}

void mark_instance_window(QWidget* window){
    if (window != nullptr){
        window->setProperty(WINDOW_PROPERTY, true);
    }
}

void bring_instance_forward(){
    for (QWidget* window : QApplication::topLevelWidgets()){
        if (!window->property(WINDOW_PROPERTY).toBool()){
            continue;
        }
        if (!window->isVisible() && !window->isMinimized()){
            continue;
        }
        //  Back from the taskbar to what it was (full screen stays full screen).
        window->setWindowState((window->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        window->show();
        window->raise();
        window->activateWindow();
    }
}


//  ---------------------------------------------------------------------
//  Starting installations and the overview
//  ---------------------------------------------------------------------

QStringList instance_arguments(const QString& id){
    return QStringList{INSTANCE_OPTION, id};
}
QStringList overview_arguments(){
    return QStringList{INSTANCES_OPTION};
}

namespace{

//  A process of its own, in the folder that this one was started in (the data of
//  installation 1 is there, and so is everything that is looked up by it).
bool start_detached(const QStringList& arguments){
    return QProcess::startDetached(
        QCoreApplication::applicationFilePath(),
        arguments,
        to_qt(shared_base_path())
    );
}

}

bool start_instance_process(const QString& id){
    //  No process is started by a test.
    if (!valid_instance_id(id) || g_test_run){
        return false;
    }
    return start_detached(instance_arguments(id));
}

bool open_instance(const QString& id){
    if (!valid_instance_id(id) || g_test_run){
        return false;
    }
    if (send_instance_command(instance_link_name(shared_base_path(), "i" + id), "show")){
        return true;
    }
    return start_instance_process(id);
}

bool start_overview_process(){
    if (g_test_run){
        return false;
    }
    return start_detached(overview_arguments());
}

bool show_instances_overview(){
    if (g_test_run){
        return false;
    }
    if (send_instance_command(instance_link_name(shared_base_path(), "hub"), "show")){
        return true;
    }
    return start_overview_process();
}


//  ---------------------------------------------------------------------
//  The overview as a process
//  ---------------------------------------------------------------------

int run_instances_hub(QApplication& application, const QString& test_folder){
    (void)application;
    const std::string scratch = shared_base_path();
    const auto registry = std::make_shared<InstanceRegistry>(scratch);

    SetupRequest request;
    request.overview = true;
    request.instance_list = [registry]{
        return registry->list(QDateTime::currentMSecsSinceEpoch(), true);
    };
    request.instance_open = [](const QString& id){
        if (!open_instance(id)){
            global_logger_tagged().log("Installations: installation " + id.toStdString() + " could not be opened.", COLOR_RED);
        }
    };
    request.instance_create = [registry]() -> QString{
        const QString id = registry->create(QDateTime::currentMSecsSinceEpoch());
        if (id.isEmpty()){
            global_logger_tagged().log("Installations: a new installation could not be made.", COLOR_RED);
            return QString();
        }
        if (!start_instance_process(id)){
            global_logger_tagged().log("Installations: installation " + id.toStdString() + " could not be started.", COLOR_RED);
        }
        return id;
    };
    request.instance_remove = [registry](const QString& id){
        return registry->remove(id, QDateTime::currentMSecsSinceEpoch());
    };

    //  There is one overview: a second one asks the first to come forward. (The
    //  lock decides which one is first: on Windows two servers can listen on the
    //  same name of a pipe.)
    const QString link_name = instance_link_name(scratch, "hub");
    std::unique_ptr<InstanceGuard> guard;
    if (test_folder.isEmpty()){
        guard = std::make_unique<InstanceGuard>(to_qt(instances_root_path(scratch)), "hub.lock");
        if (!guard->acquired()){
            send_instance_command(link_name, "show");
            return 0;
        }
    }
    InstanceLink link(link_name, [](const QString& command){
        if (command == "show"){
            bring_overview_forward();
        }
    });
    return run_instances_overview(request, test_folder);
}


//  ---------------------------------------------------------------------
//  The tests
//  ---------------------------------------------------------------------

namespace{

QImage test_picture(int width, int height, const QColor& left, const QColor& right){
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(left);
    QPainter painter(&image);
    painter.fillRect(QRect(width / 2, 0, width - width / 2, height), right);
    return image;
}

bool close_to(const QColor& a, const QColor& b, int tolerance){
    return std::abs(a.red() - b.red()) <= tolerance &&
        std::abs(a.green() - b.green()) <= tolerance &&
        std::abs(a.blue() - b.blue()) <= tolerance;
}

}

int run_instances_checks(Logger& logger){
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("Installations test: FAILED: ") + what, COLOR_RED);
        }
    };

    //  Who may be an installation: a folder name that is safe.
    check(valid_instance_id("1") && valid_instance_id("12") && valid_instance_id("ci-test") && valid_instance_id("a_B-9"), "numbers and names are fine");
    check(!valid_instance_id("") && !valid_instance_id("../x") && !valid_instance_id("a b") && !valid_instance_id("x/y") && !valid_instance_id("x\\y"), "no empty name, no way out of the folder, no space");
    check(!valid_instance_id(QString(33, 'a')) && valid_instance_id(QString(32, 'a')), "at most 32 characters");
    check(!valid_instance_id(QString::fromUtf8("\xc3\xa9")) && !valid_instance_id("a.b") && !valid_instance_id(".."), "no letters outside ASCII, no dots");

    //  The command line.
    {
        auto parse = [](std::vector<std::string> words, std::string& id, bool& overview, QString* error){
            std::vector<char*> argv;
            for (std::string& word : words){
                argv.push_back(word.data());
            }
            return parse_instance_options((int)argv.size(), argv.data(), id, overview, error);
        };
        std::string id = "x";
        bool overview = true;
        QString error;
        check(parse({"p"}, id, overview, &error) && id.empty() && !overview, "no option: installation 1, no overview");
        check(parse({"p", "--installation", "2"}, id, overview, &error) && id == "2" && !overview, "--installation 2");
        check(parse({"p", "--no-intro", "--installation", "ci-test", "x"}, id, overview, &error) && id == "ci-test", "--installation among other options");
        check(parse({"p", "--installations"}, id, overview, &error) && id.empty() && overview, "--installations");
        error.clear();
        check(!parse({"p", "--installation"}, id, overview, &error) && !error.isEmpty() && id.empty(), "--installation with no id is refused");
        check(!parse({"p", "--installation", "../x"}, id, overview, nullptr), "--installation ../x is refused");
        check(!parse({"p", "--installation", "a b"}, id, overview, nullptr), "--installation with a space is refused");
    }

    //  Where things are, in a folder with a letter outside ASCII in its name.
    check(instances_root_path("/s") == "/s/Installations/", "the folder of the installations");
    check(instance_data_path("/s/", "1") == "/s/", "installation 1 keeps its data where it was");
    check(instance_data_path("/s/", "2") == "/s/Installations/2/", "installation 2 has a folder of its own");
    check(instance_meta_path("/s/", "1") == "/s/Installations/1/", "installation 1's status has a folder in Installations");
    check(instance_data_path("C:\\s", "ci-test") == "C:/s/Installations/ci-test/", "a Windows path is made as every path of the program is");
    check(instance_setting_key_for("capture_device", "1") == "capture_device", "installation 1 keeps its key in the user's settings");
    check(instance_setting_key_for("capture_device", "2") == "capture_device-2", "the others have a key of their own");
    check(instance_link_name("/s/", "hub") == instance_link_name("/s", "hub") && instance_link_name("/s/", "hub") != instance_link_name("/t/", "hub"), "the connection's name is that of the folder");
    check(instance_link_name("/s/", "i1") != instance_link_name("/s/", "i2"), "each installation has its own connection");

    QTemporaryDir temp;
    if (!temp.isValid()){
        logger.log("Installations test: FAILED: no temporary folder to test in", COLOR_RED);
        return wrong + 1;
    }
    QDir(temp.path()).mkdir(QString::fromUtf8("D\xc3\xa9nnis"));
    const std::string scratch = (QDir(temp.path()).filePath(QString::fromUtf8("D\xc3\xa9nnis")) + "/").toStdString();
    const InstanceRegistry registry(scratch);
    constexpr qint64 NOW = 1800000000000;

    //  The list.
    {
        std::vector<InstanceInfo> list = registry.list(NOW);
        check(list.size() == 1 && list[0].id == "1" && list[0].main && !list[0].running, "with nothing made, installation 1 is the only one, and it does not run");

        const QString second = registry.create(NOW);
        const QString third = registry.create(NOW);
        check(second == "2" && third == "3", "the new installations are 2 and 3");
        QDir(registry.root()).mkdir("9");
        QDir(registry.root()).mkdir("ci-test");
        QDir(registry.root()).mkdir("not a name");
        const QString after = registry.create(NOW);
        check(after == "10", "the next one comes after the highest number (9), not in a gap");

        list = registry.list(NOW);
        QStringList ids;
        for (const InstanceInfo& info : list){
            ids << info.id;
        }
        check(ids == QStringList({"1", "2", "3", "9", "10", "ci-test"}), "the list is 1 first, the numbers by value, then the names (a folder that is no name is left out)");
        check(list[1].starting && !list[1].running, "a new installation without a status is starting");
        check(!registry.read("2", NOW + INSTANCE_STARTING_MSEC + 1000, false).starting, "and is not starting for ever");
        check(!registry.read("9", NOW, false).starting, "a folder that nobody made through the program is not starting");
    }

    //  The status and the miniature.
    {
        const QImage picture = test_picture(640, 360, QColor(220, 30, 30), QColor(30, 30, 220));
        const QImage mini = fit_miniature(picture);
        check(mini.width() == MINIATURE_WIDTH && mini.height() == MINIATURE_HEIGHT, "a miniature is 480 x 270");
        check(close_to(mini.pixelColor(60, 135), QColor(220, 30, 30), 8) && close_to(mini.pixelColor(420, 135), QColor(30, 30, 220), 8), "a 16:9 picture fills it");
        const QImage narrow = fit_miniature(test_picture(400, 300, Qt::white, Qt::white));
        check(close_to(narrow.pixelColor(5, 135), Qt::black, 4) && close_to(narrow.pixelColor(240, 135), Qt::white, 8), "a 4:3 picture has black bars at its sides");
        check(fit_miniature(QImage()).isNull(), "no picture gives none");

        const QString folder = registry.folder("2");
        check(write_instance_report_to(folder, 4242, NOW, "games", "Games", mini), "the status and the miniature are written");
        InstanceInfo info = registry.read("2", NOW + 1000, true);
        check(info.running && info.pid == 4242 && info.state == "games" && info.title == "Games", "an installation that reported a second ago runs");
        check(!info.starting && !info.picture.isNull() && info.picture.width() == MINIATURE_WIDTH, "it has its miniature");
        check(close_to(info.picture.pixelColor(60, 135), QColor(220, 30, 30), 40), "the miniature is the picture (as a JPEG)");
        check(registry.read("2", NOW + INSTANCE_ALIVE_MSEC - 500, false).running, "a few seconds later it runs");
        info = registry.read("2", NOW + INSTANCE_ALIVE_MSEC + 1000, true);
        check(!info.running && info.state.isEmpty() && info.pid == 0, "after that it does not run");
        check(!info.picture.isNull(), "and its last miniature stays");
        check(write_instance_report_to(folder, 4242, NOW + 20000, "running", "AutoStory", QImage()), "a status without a picture is written");
        info = registry.read("2", NOW + 20500, true);
        check(info.running && info.state == "running" && info.title == "AutoStory" && !info.picture.isNull(), "the miniature is kept when a status has none");
        check(!registry.read("3", NOW + 20500, true).running && registry.read("3", NOW + 20500, true).picture.isNull(), "an installation that told nothing has nothing");

        //  A status that is not a status.
        QFile bad(folder + "status.json");
        if (bad.open(QIODevice::WriteOnly | QIODevice::Truncate)){
            bad.write("this is not json");
        }
        bad.close();
        check(!registry.read("2", NOW + 20500, false).running, "a status that is no JSON means it does not run");
    }

    //  Removing.
    {
        check(!registry.remove("1", NOW), "installation 1 is not removed");
        check(!registry.remove("../2", NOW) && !registry.remove("", NOW), "nor a name that is not one");
        write_instance_report_to(registry.folder("3"), 4243, NOW, "games", "Games", QImage());
        check(!registry.remove("3", NOW + 1000) && QDir(registry.folder("3")).exists(), "an installation that runs is not removed");
        check(registry.remove("3", NOW + 60000) && !QDir(registry.folder("3")).exists(), "one that does not run is, with its folder");
        check(!registry.remove("3", NOW + 60000), "and once");
        check(QDir(registry.folder("2")).exists(), "the others stay");
    }

    //  One installation runs once.
    {
        const QString folder = registry.folder("ci-test");
        auto first = std::make_unique<InstanceGuard>(folder);
        check(first->acquired(), "the first start takes the lock");
        InstanceGuard second(folder);
        check(!second.acquired(), "the second start of the same installation does not");
        InstanceGuard other(registry.folder("9"));
        check(other.acquired(), "another installation does");
        //  The overview has a lock of its own, in the folder of all of them.
        InstanceGuard hub(to_qt(instances_root_path(scratch)), "hub.lock");
        InstanceGuard hub_twice(to_qt(instances_root_path(scratch)), "hub.lock");
        check(hub.acquired() && !hub_twice.acquired(), "the overview is one, with a lock of its own");
        first.reset();
        InstanceGuard third(folder);
        check(third.acquired(), "when the first has ended the lock is free");
    }

    //  The connection.
    {
        const QString name = instance_link_name(scratch, "test");
        QStringList received;
        {
            InstanceLink link(name, [&](const QString& command){
                received << command;
            });
            check(link.listening(), "an installation listens on its connection");
            check(send_instance_command(name, "show") && received == QStringList({"show"}), "'show' comes through and is answered");
            check(send_instance_command(name, "show") && received.size() == 2, "and again");
        }
        QElapsedTimer timer;
        timer.start();
        check(!send_instance_command(name, "show", 1500), "nobody listens: no answer");
        check(timer.elapsed() < 1000, "and it does not wait for it");
        InstanceLink again(name, nullptr);
        check(again.listening(), "the name is free again when the first has ended");
    }

    logger.log(
        "Installations test: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}

int run_instance_test(QApplication& application){
    (void)application;
    Logger& logger = global_logger_tagged();
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("Installation test: FAILED: ") + what, COLOR_RED);
        }
    };

    const std::string base = shared_base_path();
    const std::string mine = base + "Installations/" + current_instance() + "/";
    check(!is_main_instance() && !current_instance().empty(), "this is an installation that is not number 1");
    check(RUNTIME_BASE_PATH() == mine, "its data is in its folder of Installations");
    check(SETTINGS_PATH() == mine + "UserSettings/", "its settings are there too");
    check(USER_FILE_PATH() == mine, "and its log");
    check(SCREENSHOTS_PATH() == mine + "Screenshots/", "and its pictures");

    //  It writes its settings where they are its own.
    try{
        Filesystem::create_directories(Filesystem::Path(SETTINGS_PATH()));
        PERSISTENT_SETTINGS().write();
    }catch (const std::exception& error){
        logger.log(std::string("Installation test: settings: ") + error.what(), COLOR_RED);
    }
    check(
        PROGRAM_SETTING_JSON_PATH().compare(0, mine.size(), mine) == 0 &&
        Filesystem::exists(Filesystem::Path(PROGRAM_SETTING_JSON_PATH())),
        "its settings file is written in its folder"
    );

    //  Its lock, its status and its miniature.
    const QString meta = to_qt(instance_meta_path(base, current_instance()));
    {
        InstanceGuard guard(meta);
        check(guard.acquired(), "it takes its lock");
        InstanceGuard twice(meta);
        check(!twice.acquired(), "a second start of it would not");
    }
    QImage picture(MINIATURE_WIDTH, MINIATURE_HEIGHT, QImage::Format_RGB32);
    {
        QPainter painter(&picture);
        painter.fillRect(picture.rect(), QColor(24, 24, 28));
        painter.fillRect(QRect(0, 0, MINIATURE_WIDTH, 90), QColor(217, 119, 87));
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPixelSize(40);
        painter.setFont(font);
        painter.drawText(QRect(0, 0, MINIATURE_WIDTH, 90), Qt::AlignCenter, "Installation " + to_qt(current_instance()));
        painter.setPen(QColor(240, 240, 240));
        font.setPixelSize(26);
        painter.setFont(font);
        painter.drawText(QRect(0, 110, MINIATURE_WIDTH, 60), Qt::AlignCenter, "its own capture card");
        painter.drawText(QRect(0, 160, MINIATURE_WIDTH, 60), Qt::AlignCenter, "its own microcontroller");
    }
    check(write_instance_report_to(meta, 0, 0, "games", "Games", picture), "it reports itself");
    const InstanceRegistry registry(base);
    const InstanceInfo info = registry.read(to_qt(current_instance()), QDateTime::currentMSecsSinceEpoch(), true);
    check(info.running && info.pid == QCoreApplication::applicationPid() && info.state == "games", "the overview sees it running, with its process");
    check(!info.picture.isNull() && info.picture.width() == MINIATURE_WIDTH, "and its miniature");
    check(registry.read("1", QDateTime::currentMSecsSinceEpoch(), false).pid != QCoreApplication::applicationPid(), "installation 1 is not this process");

    //  Its connection.
    {
        bool shown = false;
        InstanceLink link(instance_link_name(base, to_qt("i" + current_instance())), [&](const QString& command){
            if (command == "show"){
                shown = true;
            }
        });
        check(link.listening(), "it listens on its connection");
        check(send_instance_command(instance_link_name(base, to_qt("i" + current_instance())), "show") && shown, "the overview's 'show' reaches it");
    }

    //  It ends: it is not running any more, and what it showed stays.
    QFile::remove(meta + "status.json");
    check(!registry.read(to_qt(current_instance()), QDateTime::currentMSecsSinceEpoch(), false).running, "when it ends it does not run");

    logger.log(
        "Installation test: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong == 0 ? 0 : 1;
}


}
}

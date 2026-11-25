/*  PocketPilot Firmware
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <algorithm>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include "PocketPilot_Firmware.h"

namespace PokemonAutomation{
namespace PocketPilot{



//  ---------------------------------------------------------------------
//  The firmware files
//  ---------------------------------------------------------------------

const char* const FIRMWARE_LISTING_URL = "https://api.github.com/repos/PokemonAutomation/Packages/contents/Firmware";

QString firmware_kind(const QString& board){
    if (board == "pico"){
        return "Pico1W";
    }
    if (board == "pico2"){
        return "Pico2W";
    }
    if (board == "esp32"){
        return "ESP32";
    }
    if (board == "esp32s3"){
        return "ESP32-S3";
    }
    return QString();
}

QString firmware_extension(const QString& kind){
    return kind.startsWith("Pico") ? ".uf2" : ".bin";
}

bool parse_firmware_name(const QString& file_name, QString& kind, QString& version){
    static const QRegularExpression PATTERN("^PABotBase2-(Pico1W|Pico2W|ESP32-S3|ESP32|RP2040|RP2350)-(\\d+)\\.(uf2|bin)$");
    const QRegularExpressionMatch match = PATTERN.match(file_name);
    if (!match.hasMatch()){
        return false;
    }
    kind = match.captured(1);
    version = match.captured(2);
    return true;
}

QString firmware_folder(){
    return QDir(QCoreApplication::applicationDirPath()).filePath("Firmware");
}

namespace{

bool newer_first(const FirmwareFile& a, const FirmwareFile& b){
    if (a.version.size() != b.version.size()){
        return a.version.size() > b.version.size();
    }
    return a.version > b.version;
}

}

std::vector<FirmwareFile> local_firmware(const QString& folder, const QString& kind){
    std::vector<FirmwareFile> files;
    const QDir dir(folder);
    const QFileInfoList entries = dir.entryInfoList(QStringList() << "*.uf2" << "*.bin", QDir::Files);
    for (const QFileInfo& entry : entries){
        FirmwareFile file;
        if (!parse_firmware_name(entry.fileName(), file.kind, file.version) || file.kind != kind){
            continue;
        }
        file.file_name = entry.fileName();
        file.path = entry.absoluteFilePath();
        files.push_back(file);
    }
    std::sort(files.begin(), files.end(), newer_first);
    return files;
}

QString firmware_ref(const QString& folder){
    QFile file(QDir(folder).filePath("SOURCE.txt"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)){
        const QString text = QString::fromUtf8(file.read(2000));
        static const QRegularExpression COMMIT("\\b[0-9a-f]{40}\\b");
        const QRegularExpressionMatch match = COMMIT.match(text);
        if (match.hasMatch()){
            return match.captured(0);
        }
    }
    return "master";
}

QString firmware_url(const QString& file_name, const QString& ref){
    return "https://github.com/PokemonAutomation/Packages/raw/" + ref + "/Firmware/" + file_name;
}

std::vector<FirmwareFile> parse_firmware_listing(const QByteArray& json, const QString& kind){
    std::vector<FirmwareFile> files;
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isArray()){
        return files;
    }
    const QJsonArray array = document.array();
    for (const QJsonValue& value : array){
        const QJsonObject object = value.toObject();
        if (object.value("type").toString() != "file"){
            continue;
        }
        FirmwareFile file;
        if (!parse_firmware_name(object.value("name").toString(), file.kind, file.version) || file.kind != kind){
            continue;
        }
        file.file_name = object.value("name").toString();
        file.online = true;
        files.push_back(file);
    }
    std::sort(files.begin(), files.end(), newer_first);
    return files;
}

std::vector<FirmwareFile> merge_firmware(const std::vector<FirmwareFile>& local, const std::vector<FirmwareFile>& online){
    std::vector<FirmwareFile> merged = local;
    for (const FirmwareFile& file : online){
        bool known = false;
        for (FirmwareFile& have : merged){
            if (have.file_name == file.file_name){
                have.online = true;
                known = true;
                break;
            }
        }
        if (!known){
            merged.push_back(file);
        }
    }
    std::sort(merged.begin(), merged.end(), newer_first);
    return merged;
}

bool looks_like_firmware(const QString& path){
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)){
        return false;
    }
    const qint64 size = file.size();
    if (path.endsWith(".uf2", Qt::CaseInsensitive)){
        //  Blocks of 512 bytes, the first starting with "UF2\n".
        const QByteArray head = file.read(4);
        return size >= 512 && size % 512 == 0 && head == QByteArray("UF2\n");
    }
    return size >= 4096;
}

std::map<QString, QString> parse_firmware_checksums(const QByteArray& text){
    std::map<QString, QString> sums;
    static const QRegularExpression LINE("^([0-9a-fA-F]{64})\\s+\\*?(\\S.*?)\\s*$");
    for (const QByteArray& raw : text.split('\n')){
        const QRegularExpressionMatch match = LINE.match(QString::fromUtf8(raw).trimmed());
        if (match.hasMatch()){
            sums[match.captured(2)] = match.captured(1).toLower();
        }
    }
    return sums;
}

std::map<QString, QString> firmware_checksums(const QString& folder){
    QFile file(QDir(folder).filePath("SHA256SUMS.txt"));
    if (!file.open(QIODevice::ReadOnly)){
        return {};
    }
    return parse_firmware_checksums(file.read(1 << 20));
}

QString file_sha256(const QString& path){
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)){
        return QString();
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)){
        return QString();
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool firmware_matches_checksum(const QString& folder, const QString& path){
    const std::map<QString, QString> sums = firmware_checksums(folder);
    const auto found = sums.find(QFileInfo(path).fileName());
    if (found == sums.end()){
        return true;
    }
    return file_sha256(path) == found->second;
}



//  ---------------------------------------------------------------------
//  Downloading
//  ---------------------------------------------------------------------

Download::Download() = default;

Download::~Download(){
    cancel();
}

double Download::progress() const{
    if (m_done){
        return m_error.isEmpty() ? 1.0 : 0.0;
    }
    if (m_total <= 0){
        return -1;
    }
    return std::min(1.0, (double)m_received / (double)m_total);
}

void Download::cancel(){
    if (m_reply != nullptr){
        QNetworkReply* reply = m_reply;
        m_reply = nullptr;
        reply->disconnect();
        reply->abort();
        reply->deleteLater();
    }
    if (m_file != nullptr){
        const QString partial = m_file->fileName();
        m_file->close();
        m_file.reset();
        QFile::remove(partial);
    }
    m_manager.reset();
    m_running = false;
}

void Download::start(const QString& url, const QString& target){
    cancel();
    m_target = target;
    m_body.clear();
    m_received = 0;
    m_total = -1;
    m_done = false;
    m_error.clear();

    if (!target.isEmpty()){
        QDir().mkpath(QFileInfo(target).absolutePath());
        m_file = std::make_unique<QFile>(target + ".part");
        if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)){
            m_file.reset();
            finish("Cannot write " + target);
            return;
        }
    }

    m_manager = std::make_unique<QNetworkAccessManager>();
    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", "PocketPilot-Studio");
    m_reply = m_manager->get(request);
    m_running = true;

    QNetworkReply* reply = m_reply;
    QObject* context = m_manager.get();
    QObject::connect(reply, &QNetworkReply::readyRead, context, [this, reply]{
        const QByteArray data = reply->readAll();
        if (m_file != nullptr){
            m_file->write(data);
        }else{
            m_body += data;
        }
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, context, [this](qint64 received, qint64 total){
        m_received = received;
        m_total = total;
    });
    QObject::connect(reply, &QNetworkReply::finished, context, [this, reply]{
        const QByteArray rest = reply->readAll();
        if (m_file != nullptr){
            m_file->write(rest);
        }else{
            m_body += rest;
        }
        QString error;
        if (reply->error() != QNetworkReply::NoError){
            error = reply->errorString();
        }
        m_reply = nullptr;
        reply->deleteLater();
        finish(error);
    });
}

void Download::finish(const QString& error){
    QString problem = error;
    if (m_file != nullptr){
        const QString partial = m_file->fileName();
        m_file->close();
        m_file.reset();
        if (problem.isEmpty()){
            QFile::remove(m_target);
            if (!QFile::rename(partial, m_target)){
                problem = "Cannot write " + m_target;
            }
        }
        if (!problem.isEmpty()){
            QFile::remove(partial);
        }
    }
    m_error = problem;
    m_running = false;
    m_done = true;
}



//  ---------------------------------------------------------------------
//  The Pico
//  ---------------------------------------------------------------------

bool inspect_pico_drive(const QString& root, const QString& label, PicoDrive& drive){
    QString board_id;
    QFile info(QDir(root).filePath("INFO_UF2.TXT"));
    if (info.open(QIODevice::ReadOnly | QIODevice::Text)){
        const QString text = QString::fromUtf8(info.read(4000));
        static const QRegularExpression BOARD("^Board-ID:\\s*(\\S+)", QRegularExpression::MultilineOption);
        const QRegularExpressionMatch match = BOARD.match(text);
        if (match.hasMatch()){
            board_id = match.captured(1);
        }
    }
    const bool by_label = label.compare("RPI-RP2", Qt::CaseInsensitive) == 0 || label.compare("RP2350", Qt::CaseInsensitive) == 0;
    if (board_id.isEmpty() && !by_label){
        return false;
    }
    drive.root = root;
    drive.label = label;
    drive.board_id = board_id.isEmpty() ? label : board_id;
    return true;
}

std::vector<PicoDrive> find_pico_drives(){
    std::vector<PicoDrive> drives;
    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo& volume : volumes){
        if (!volume.isValid() || !volume.isReady()){
            continue;
        }
        const QString label = volume.name();
        const QString type = QString::fromLatin1(volume.fileSystemType()).toLower();
        const bool by_label = label.compare("RPI-RP2", Qt::CaseInsensitive) == 0 || label.compare("RP2350", Qt::CaseInsensitive) == 0;
        //  The drive of the boot loader is a small FAT drive; the other drives of
        //  the computer are not opened.
        if (!by_label && !type.contains("fat")){
            continue;
        }
        PicoDrive drive;
        if (inspect_pico_drive(volume.rootPath(), label, drive)){
            drives.push_back(drive);
        }
    }
    return drives;
}

bool pico_drive_fits(const PicoDrive& drive, const QString& board){
    if (board == "pico"){
        return drive.board_id.compare("RPI-RP2", Qt::CaseInsensitive) == 0;
    }
    if (board == "pico2"){
        return drive.board_id.compare("RP2350", Qt::CaseInsensitive) == 0;
    }
    return false;
}

CopyJob::CopyJob(const QString& source, const QString& drive_root)
    : m_source(source)
    , m_root(drive_root)
{
    m_thread = std::thread([this]{
        run();
    });
}

CopyJob::~CopyJob(){
    if (m_thread.joinable()){
        m_thread.join();
    }
}

void CopyJob::run(){
    auto fail = [this](const QString& error){
        {
            std::lock_guard<std::mutex> lock(m_lock);
            m_error = error;
        }
        m_done = true;
    };

    QFile in(m_source);
    if (!in.open(QIODevice::ReadOnly)){
        fail("Cannot read " + m_source);
        return;
    }
    const qint64 size = in.size();
    const QString target = QDir(m_root).filePath(QFileInfo(m_source).fileName());
    QFile out(target);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)){
        fail("Cannot write to " + m_root);
        return;
    }

    qint64 written = 0;
    QByteArray buffer;
    while (written < size){
        buffer = in.read(64 * 1024);
        if (buffer.isEmpty()){
            break;
        }
        if (out.write(buffer) != buffer.size()){
            fail("Cannot write to " + m_root);
            return;
        }
        written += buffer.size();
        m_progress = size <= 0 ? 1.0 : (double)written / (double)size;
    }
    if (written < size){
        fail("Cannot read " + m_source);
        return;
    }
    //  The Pico restarts as soon as it has the last block, so what comes now
    //  (the end of the write) may be answered with an error: the file is on
    //  its way, which the drive disappearing tells.
    (void)out.flush();
    out.close();
    m_progress = 1.0;
    m_done = true;
}



//  ---------------------------------------------------------------------
//  Serial ports
//  ---------------------------------------------------------------------

QString port_chip(int vendor, int product){
    switch (vendor){
    case 0x10C4:
        return "CP210x";
    case 0x1A86:
        return product == 0x55D4 ? "CH9102" : "CH340";
    case 0x0403:
        return "FTDI";
    case 0x303A:
        return "ESP32-S3";
    case 0x2E8A:
        return "Raspberry Pi";
    case 0x2341:
        return "Arduino";
    default:
        return QString();
    }
}

std::vector<PortInfo> find_ports(){
    std::vector<PortInfo> ports;
    const QList<QSerialPortInfo> list = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : list){
        PortInfo port;
#ifdef Q_OS_WIN
        port.name = info.portName();
#else
        port.name = info.systemLocation();
#endif
        port.description = info.description();
        port.vendor = info.hasVendorIdentifier() ? (int)info.vendorIdentifier() : -1;
        port.product = info.hasProductIdentifier() ? (int)info.productIdentifier() : -1;
        port.chip = port.vendor < 0 ? QString() : port_chip(port.vendor, port.product);
        ports.push_back(port);
    }
    //  The ones with a known chip first, then by name.
    std::stable_sort(ports.begin(), ports.end(), [](const PortInfo& a, const PortInfo& b){
        if (a.chip.isEmpty() != b.chip.isEmpty()){
            return !a.chip.isEmpty();
        }
        return a.name < b.name;
    });
    return ports;
}



//  ---------------------------------------------------------------------
//  esptool
//  ---------------------------------------------------------------------

FlashTool find_flash_tool(const QString& app_folder){
    FlashTool tool;
#ifdef Q_OS_WIN
    const QString exe = "esptool.exe";
#else
    const QString exe = "esptool";
#endif
    //  The one that comes with the program (Tools/esptool, or one folder deeper).
    const QDir tools(QDir(app_folder).filePath("Tools"));
    QStringList candidates;
    candidates << tools.filePath("esptool/" + exe) << tools.filePath(exe);
    const QStringList subfolders = QDir(tools.filePath("esptool")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& sub : subfolders){
        candidates << tools.filePath("esptool/" + sub + "/" + exe);
    }
    for (const QString& candidate : candidates){
        if (QFileInfo(candidate).isFile()){
            tool.found = true;
            tool.program = candidate;
            tool.where = "esptool in the Tools folder";
            return tool;
        }
    }

    //  One of this computer.
    for (const char* name : {"esptool", "esptool.py"}){
        const QString path = QStandardPaths::findExecutable(name);
        if (!path.isEmpty()){
            tool.found = true;
            tool.program = path;
            tool.where = "esptool on this computer";
            return tool;
        }
    }

    //  Python with esptool installed (pip install esptool).
    for (const char* name : {"python3", "python", "py"}){
        const QString python = QStandardPaths::findExecutable(name);
        if (python.isEmpty()){
            continue;
        }
        QProcess probe;
        probe.setProcessChannelMode(QProcess::MergedChannels);
        probe.start(python, QStringList() << "-m" << "esptool" << "version");
        if (!probe.waitForFinished(8000)){
            probe.kill();
            (void)probe.waitForFinished(1000);
            continue;
        }
        if (probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0){
            tool.found = true;
            tool.program = python;
            tool.prefix = QStringList() << "-m" << "esptool";
            tool.where = "esptool in Python";
            return tool;
        }
    }
    return tool;
}

int parse_flash_progress(const QString& line){
    if (line.contains("Hash of data verified")){
        return 100;
    }
    if (!line.contains("Writing at")){
        return -1;
    }
    static const QRegularExpression PERCENT("(\\d{1,3}(?:\\.\\d+)?)\\s*%");
    const QRegularExpressionMatch match = PERCENT.match(line);
    if (!match.hasMatch()){
        return -1;
    }
    return std::max(0, std::min(100, (int)match.captured(1).toDouble()));
}

QStringList flash_arguments(const FlashTool& tool, const QString& port, const QString& file, int baud, bool underscore){
    QStringList arguments = tool.prefix;
    arguments << "--port" << port << "--baud" << QString::number(baud);
    arguments << (underscore ? "write_flash" : "write-flash") << "0x0" << file;
    return arguments;
}

EspFlashJob::EspFlashJob(const FlashTool& tool, const QString& port, const QString& file, int baud)
    : m_tool(tool)
    , m_port(port)
    , m_file(file)
    , m_baud(baud)
{}

EspFlashJob::~EspFlashJob(){
    if (m_process != nullptr){
        m_process->disconnect();
        if (m_process->state() != QProcess::NotRunning){
            m_process->kill();
            m_process->waitForFinished(2000);
        }
    }
}

void EspFlashJob::start(){
    m_retried = false;
    m_done = false;
    m_ok = false;
    m_progress = 0;
    m_error_kind.clear();
    m_log.clear();
    m_status.clear();
    launch(false);
}

void EspFlashJob::launch(bool underscore){
    if (m_process != nullptr){
        m_process->disconnect();
        m_process.release()->deleteLater();
    }
    m_process = std::make_unique<QProcess>();
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert("PYTHONUNBUFFERED", "1");
    environment.insert("PYTHONIOENCODING", "utf-8");
    environment.insert("NO_COLOR", "1");
    m_process->setProcessEnvironment(environment);
    m_pending.clear();

    QProcess* process = m_process.get();
    QObject::connect(process, &QProcess::readyReadStandardOutput, process, [this]{
        read_output();
    });
    QObject::connect(process, &QProcess::finished, process, [this](int code, QProcess::ExitStatus status){
        finished(code, status == QProcess::CrashExit);
    });
    QObject::connect(process, &QProcess::errorOccurred, process, [this](QProcess::ProcessError error){
        if (error == QProcess::FailedToStart){
            m_status = m_process->errorString();
            m_error_kind = "tool";
            m_running = false;
            m_done = true;
            m_ok = false;
        }
    });
    m_running = true;
    m_status = "Connecting";
    process->start(m_tool.program, flash_arguments(m_tool, m_port, m_file, m_baud, underscore));
}

void EspFlashJob::cancel(){
    if (m_process != nullptr && m_process->state() != QProcess::NotRunning){
        m_process->disconnect();
        m_process->kill();
        m_process->waitForFinished(2000);
    }
    m_running = false;
    m_done = true;
    m_ok = false;
    m_error_kind = "cancelled";
}

void EspFlashJob::read_output(){
    m_pending += QString::fromUtf8(m_process->readAllStandardOutput());
    //  esptool redraws its progress on one line with \r.
    static const QRegularExpression BREAK("[\\r\\n]");
    const QStringList parts = m_pending.split(BREAK);
    m_pending = parts.last();
    for (int c = 0; c + 1 < parts.size(); c++){
        const QString line = parts[c].trimmed();
        if (line.isEmpty()){
            continue;
        }
        m_log << line;
        while (m_log.size() > 300){
            m_log.removeFirst();
        }
        const int percent = parse_flash_progress(line);
        if (percent >= 0){
            m_progress = std::max(m_progress, percent / 100.0);
        }
        m_status = line.left(120);
    }
}

void EspFlashJob::finished(int code, bool crashed){
    read_output();
    if (!m_pending.trimmed().isEmpty()){
        m_log << m_pending.trimmed();
        m_status = m_pending.trimmed().left(120);
        m_pending.clear();
    }
    const QString all = m_log.join("\n");

    //  An esptool of before the hyphens: the same command with an underscore.
    if (code != 0 && !m_retried && (all.contains("invalid choice") || all.contains("write-flash"))){
        m_retried = true;
        m_log.clear();
        launch(true);
        return;
    }

    m_running = false;
    m_done = true;
    m_ok = code == 0 && !crashed;
    if (m_ok){
        m_progress = 1.0;
        return;
    }
    if (all.contains("No module named esptool")){
        m_error_kind = "tool";
    }else if (
        all.contains("could not open port", Qt::CaseInsensitive) ||
        all.contains("Access is denied", Qt::CaseInsensitive) ||
        all.contains("PermissionError") ||
        all.contains("FileNotFoundError") ||
        all.contains("Could not configure port", Qt::CaseInsensitive)
    ){
        m_error_kind = "port";
    }else if (
        all.contains("Failed to connect", Qt::CaseInsensitive) ||
        all.contains("No serial data received", Qt::CaseInsensitive) ||
        all.contains("Wrong boot mode", Qt::CaseInsensitive) ||
        all.contains("Timed out waiting for packet", Qt::CaseInsensitive)
    ){
        m_error_kind = "connect";
    }else{
        m_error_kind = "other";
    }
}



//  ---------------------------------------------------------------------
//  Looking at the computer
//  ---------------------------------------------------------------------

namespace{

struct ToolCache{
    std::mutex lock;
    bool wanted = false;
    bool checked = false;
    FlashTool tool;
};

}

HardwareProbe::HardwareProbe(const QString& app_folder, Scan scan)
    : m_app_folder(app_folder)
    , m_scan(std::move(scan))
    , m_shared(std::make_shared<Shared>())
{
    if (!m_scan){
        std::shared_ptr<ToolCache> cache = std::make_shared<ToolCache>();
        const QString folder = app_folder;
        m_scan = [cache, folder]{
            HardwareState state;
            state.drives = find_pico_drives();
            state.ports = find_ports();
            //  The tool is looked for from the second look on: the drives and the
            //  ports are not kept waiting for Python.
            bool look = false;
            {
                std::lock_guard<std::mutex> lock(cache->lock);
                if (!cache->wanted){
                    cache->wanted = true;
                }else if (!cache->checked){
                    look = true;
                }
            }
            if (look){
                const FlashTool tool = find_flash_tool(folder);
                std::lock_guard<std::mutex> lock(cache->lock);
                cache->tool = tool;
                cache->checked = true;
            }
            {
                std::lock_guard<std::mutex> lock(cache->lock);
                state.tool = cache->tool;
                state.tool_checked = cache->checked;
            }
            return state;
        };
    }
}

HardwareProbe::~HardwareProbe() = default;

void HardwareProbe::step(double dt, bool synchronous){
    m_since += dt;
    if (m_since < 1.0){
        return;
    }
    if (synchronous){
        m_since = 0;
        HardwareState state = m_scan();
        std::lock_guard<std::mutex> lock(m_shared->lock);
        m_shared->state = std::move(state);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_shared->lock);
        if (m_shared->running){
            return;
        }
        m_shared->running = true;
    }
    m_since = 0;
    std::shared_ptr<Shared> shared = m_shared;
    Scan scan = m_scan;
    std::thread([shared, scan]{
        HardwareState state = scan();
        std::lock_guard<std::mutex> lock(shared->lock);
        shared->state = std::move(state);
        shared->running = false;
    }).detach();
}

HardwareState HardwareProbe::state() const{
    std::lock_guard<std::mutex> lock(m_shared->lock);
    return m_shared->state;
}

void HardwareProbe::refresh(){
    m_since = 100;
}

bool open_device_manager(){
#ifdef Q_OS_WIN
    return QProcess::startDetached("mmc.exe", QStringList() << "devmgmt.msc");
#else
    return false;
#endif
}



}
}

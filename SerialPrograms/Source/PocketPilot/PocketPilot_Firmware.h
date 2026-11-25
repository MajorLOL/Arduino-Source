/*  PocketPilot Firmware
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  What the setup needs to put the firmware on a board, without painting
 *  anything:
 *
 *    - the firmware files: which ones the Firmware folder next to the program
 *      has, which ones the Pokemon Automation "Packages" repository has, and
 *      the link where each can be downloaded;
 *    - the Raspberry Pi Pico: finding the drive it shows in flash mode
 *      (RPI-RP2 for the Pico W, RP2350 for the Pico 2 W) and copying the .uf2
 *      file to it;
 *    - the ESP32 and the ESP32-S3: finding the flash tool (esptool) and
 *      running it, the same command the Setup Guide gives;
 *    - the serial ports of the computer, and what chip a port belongs to.
 *
 *  Everything that takes time (copying, downloading, esptool, looking at the
 *  drives) happens off the screen: the setup only asks how far it is.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Firmware_H
#define PokemonAutomation_PocketPilot_Firmware_H

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <QByteArray>
#include <QString>
#include <QStringList>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

namespace PokemonAutomation{
namespace PocketPilot{



//  ---------------------------------------------------------------------
//  The firmware files
//  ---------------------------------------------------------------------

struct FirmwareFile{
    //  "Pico1W", "Pico2W", "ESP32" or "ESP32-S3".
    QString kind;
    //  The number in the name: 2026092800.
    QString version;
    //  PABotBase2-Pico1W-2026092800.uf2
    QString file_name;
    //  Where it is on this computer; empty when it is not.
    QString path;
    //  The Packages repository has it.
    bool online = false;
};

//  The kind of firmware a board needs: "pico" gives "Pico1W", "pico2" "Pico2W",
//  "esp32" "ESP32", "esp32s3" "ESP32-S3". Empty for any other board.
QString firmware_kind(const QString& board);
//  ".uf2" for the Picos, ".bin" for the ESP32s.
QString firmware_extension(const QString& kind);
//  Takes "PABotBase2-ESP32-S3-2026101400.bin" apart.
bool parse_firmware_name(const QString& file_name, QString& kind, QString& version);

//  The Firmware folder next to the program.
QString firmware_folder();
//  The files of this kind in that folder, the newest first.
std::vector<FirmwareFile> local_firmware(const QString& folder, const QString& kind);
//  The commit of Packages that the folder was taken from (Firmware/SOURCE.txt,
//  written by the build); "master" when that is not known.
QString firmware_ref(const QString& folder);
//  Where this file can be downloaded: its place in the Packages repository.
QString firmware_url(const QString& file_name, const QString& ref);
//  What the Packages repository lists (the answer of GitHub's contents
//  interface for its Firmware folder).
extern const char* const FIRMWARE_LISTING_URL;
std::vector<FirmwareFile> parse_firmware_listing(const QByteArray& json, const QString& kind);
//  The files of both lists, once each, the newest first.
std::vector<FirmwareFile> merge_firmware(const std::vector<FirmwareFile>& local, const std::vector<FirmwareFile>& online);
//  The file is what it says: a .uf2 starts with its marker, a .bin is not tiny.
bool looks_like_firmware(const QString& path);

//  The hashes of the firmware files that came with the program: the lines
//  "<SHA-256 in hex>  <file name>" of Firmware/SHA256SUMS.txt, which the build
//  writes. The map goes from the file name to the hash in lower case. Lines
//  that are not of this form are skipped.
std::map<QString, QString> parse_firmware_checksums(const QByteArray& text);
std::map<QString, QString> firmware_checksums(const QString& folder);
//  The SHA-256 of a file in lower case hex; empty when it cannot be read.
QString file_sha256(const QString& path);
//  False when the build made a hash of a file with this name and this file is
//  not what it hashed (the file was changed, or damaged on the way). True when
//  it is that file, and when the build knows no file of that name: a version
//  that is newer than the program has no hash to be checked against.
bool firmware_matches_checksum(const QString& folder, const QString& path);



//  ---------------------------------------------------------------------
//  Downloading (a file, or a page of text)
//  ---------------------------------------------------------------------

class Download{
public:
    Download();
    ~Download();

    //  Downloads url to target (through target + ".part"); target is replaced
    //  when it is complete. With an empty target the answer is kept in memory:
    //  see body().
    void start(const QString& url, const QString& target);
    void cancel();

    bool running() const{ return m_running; }
    bool done() const{ return m_done; }
    bool ok() const{ return m_done && m_error.isEmpty(); }
    //  0 to 1; -1 when the size is not known.
    double progress() const;
    const QString& error() const{ return m_error; }
    const QByteArray& body() const{ return m_body; }

private:
    void finish(const QString& error);

private:
    std::unique_ptr<QNetworkAccessManager> m_manager;
    QNetworkReply* m_reply = nullptr;
    QString m_target;
    std::unique_ptr<QFile> m_file;
    QByteArray m_body;
    qint64 m_received = 0;
    qint64 m_total = -1;
    bool m_running = false;
    bool m_done = false;
    QString m_error;
};



//  ---------------------------------------------------------------------
//  The Raspberry Pi Pico in flash mode
//  ---------------------------------------------------------------------

struct PicoDrive{
    //  E:/ on Windows.
    QString root;
    //  RPI-RP2 or RP2350.
    QString label;
    //  The "Board-ID" of INFO_UF2.TXT on the drive, or the label.
    QString board_id;
};

//  Looks at one drive: is it a Pico waiting for its firmware?
bool inspect_pico_drive(const QString& root, const QString& label, PicoDrive& drive);
//  All the Picos in flash mode.
std::vector<PicoDrive> find_pico_drives();
//  The drive is the one of this board ("pico": RPI-RP2, "pico2": RP2350).
bool pico_drive_fits(const PicoDrive& drive, const QString& board);

//  Copies the file onto the drive, in a thread of its own.
class CopyJob{
public:
    CopyJob(const QString& source, const QString& drive_root);
    ~CopyJob();
    double progress() const{ return m_progress; }
    bool done() const{ return m_done; }
    bool ok() const{ return m_done && m_error.isEmpty(); }
    QString error() const{
        std::lock_guard<std::mutex> lock(m_lock);
        return m_error;
    }
private:
    void run();
private:
    QString m_source;
    QString m_root;
    std::atomic<double> m_progress{0};
    std::atomic<bool> m_done{false};
    mutable std::mutex m_lock;
    QString m_error;
    std::thread m_thread;
};



//  ---------------------------------------------------------------------
//  Serial ports
//  ---------------------------------------------------------------------

struct PortInfo{
    //  COM5, /dev/ttyUSB0
    QString name;
    QString description;
    int vendor = -1;
    int product = -1;
    //  "CP210x", "CH340", ... when the USB numbers say so; else empty.
    QString chip;
};
QString port_chip(int vendor, int product);
std::vector<PortInfo> find_ports();



//  ---------------------------------------------------------------------
//  The ESP32 and the ESP32-S3: esptool
//  ---------------------------------------------------------------------

struct FlashTool{
    bool found = false;
    QString program;
    //  Before the arguments of esptool: "-m esptool" for Python.
    QStringList prefix;
    //  "esptool.exe in the Tools folder", "esptool on this computer", ...
    QString where;
};
//  Looks for esptool: in the Tools folder of the program, on the PATH, in Python.
FlashTool find_flash_tool(const QString& app_folder);

//  0 to 100 from a line of esptool ("Writing at 0x00010000... (37 %)"); -1 if the line has none.
int parse_flash_progress(const QString& line);
//  The arguments of esptool for this file: write-flash at 0x0, as the Setup Guide says.
QStringList flash_arguments(const FlashTool& tool, const QString& port, const QString& file, int baud, bool underscore);

//  Runs esptool. Needs the event loop of the program (the output arrives as events).
class EspFlashJob{
public:
    EspFlashJob(const FlashTool& tool, const QString& port, const QString& file, int baud = 460800);
    ~EspFlashJob();
    void start();
    void cancel();
    bool running() const{ return m_running; }
    bool done() const{ return m_done; }
    bool ok() const{ return m_done && m_ok; }
    //  0 to 1.
    double progress() const{ return m_progress; }
    //  The last thing esptool said.
    const QString& status() const{ return m_status; }
    //  What went wrong: "connect" (no answer from the chip), "port" (the port
    //  cannot be opened), "tool" (esptool does not start), "other"; empty if nothing.
    const QString& error_kind() const{ return m_error_kind; }
    const QStringList& log() const{ return m_log; }
private:
    void launch(bool underscore);
    void read_output();
    void finished(int code, bool crashed);
private:
    FlashTool m_tool;
    QString m_port;
    QString m_file;
    int m_baud;
    std::unique_ptr<QProcess> m_process;
    bool m_running = false;
    bool m_done = false;
    bool m_ok = false;
    bool m_retried = false;
    double m_progress = 0;
    QString m_status;
    QString m_error_kind;
    QStringList m_log;
    QString m_pending;
};



//  ---------------------------------------------------------------------
//  Looking at the computer, again and again
//  ---------------------------------------------------------------------

struct HardwareState{
    std::vector<PicoDrive> drives;
    std::vector<PortInfo> ports;
    FlashTool tool;
    bool tool_checked = false;
};

//  Asks for a new look once a second, off the screen (the drives of a computer
//  can take a moment to answer). The setup reads the last answer.
class HardwareProbe{
public:
    using Scan = std::function<HardwareState()>;
    //  Without a scan of your own: the real one, with the program's folder
    //  for the flash tool.
    explicit HardwareProbe(const QString& app_folder, Scan scan = Scan());
    ~HardwareProbe();

    //  Time passes. With synchronous the look is taken here and now (the tests).
    void step(double dt, bool synchronous);
    HardwareState state() const;
    //  A look is taken at the next step.
    void refresh();

private:
    struct Shared{
        std::mutex lock;
        HardwareState state;
        bool running = false;
        bool fresh = false;
    };
    QString m_app_folder;
    Scan m_scan;
    std::shared_ptr<Shared> m_shared;
    double m_since = 100;
};



//  Opens the Device Manager of Windows; false where there is none.
bool open_device_manager();



}
}
#endif

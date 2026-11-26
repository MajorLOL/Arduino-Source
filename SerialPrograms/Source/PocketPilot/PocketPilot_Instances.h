/*  Claude GG Installations
 *
 *  Part of Claude GG (PocketPilot before), which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  More than one installation of Claude GG can run at the same time, each with
 *  a Switch of its own: its own capture card and its own microcontroller. An
 *  installation is a process of its own with settings of its own; an overview
 *  of all of them, as miniatures, is a window of its own too. (In the code an
 *  installation is called an "instance": "installation" is also the word for
 *  the steps of a microcontroller's setup.)
 *
 *  Where the data is
 *  -----------------
 *  The first installation, number 1, is the one that has always been there: its
 *  settings, logs and pictures are where they were, in the folder of the program
 *  (the folder that it is started in). The others keep all of that in
 *
 *      Installations/<number>/
 *          UserSettings/, Screenshots/, ClaudeGG.log ...   (not for number 1)
 *          installation.json                                the name, when it was made
 *          status.json                                      "I am here": see below
 *          miniature.jpg                                    the miniature
 *          running.lock                                     held while it runs
 *
 *  next to the program; for number 1 only the last four of these are in that
 *  folder. The custom languages (Languages/) are shared by all installations.
 *  Which installation a process is, is told on the command line:
 *
 *      ClaudeGG.exe                          installation 1
 *      ClaudeGG.exe --installation 2         installation 2 (a new one starts with
 *                                            the setup, with no intro film)
 *      ClaudeGG.exe --installations          the overview of all of them
 *
 *  Starting an installation that already runs shows the overview instead.
 *
 *  What an installation tells the overview
 *  ---------------------------------------
 *  About once a second, while it has a window, it writes status.json (its
 *  process number, the time, what it is doing and what its window shows) and a
 *  new miniature.jpg: the picture of its capture card when it has one that runs,
 *  otherwise a picture of its window. The overview reads these files: an
 *  installation that wrote its status in the last few seconds is running.
 *
 *  How the windows talk
 *  --------------------
 *  Each installation, and the overview, listens on a local connection (a named
 *  pipe on Windows) for one-line commands. The only one is "show": bring your
 *  windows to the front. The overview sends it to open an installation that
 *  runs; an installation sends it to the overview (which it starts when it is
 *  not there) to go back to the miniatures.
 *
 *  The workflow that makes the builds also runs
 *
 *      ClaudeGG --installation ci-test --installation-test
 *      ClaudeGG --installations-hub-test <folder>
 *
 *  the first is an installation that starts with a folder of its own, reports
 *  itself and answers on its connection; the second shows the overview with
 *  what the first left behind, and saves a picture of it.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Instances_H
#define PokemonAutomation_PocketPilot_Instances_H

#include <stdint.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>
#include "PocketPilot_InstancePaths.h"

class QApplication;
class QLocalServer;
class QLockFile;
class QWidget;

namespace PokemonAutomation{

class Logger;

namespace PocketPilot{


//  Options on the command line.
extern const char* const INSTANCE_OPTION;           //  --installation <id>
extern const char* const INSTANCES_OPTION;          //  --installations
extern const char* const INSTANCE_TEST_OPTION;      //  --installation-test
extern const char* const INSTANCES_HUB_TEST_OPTION; //  --installations-hub-test <folder>


//  ---------------------------------------------------------------------
//  Which installation this is
//  ---------------------------------------------------------------------

//  An id is made of letters, digits, '-' and '_', at most 32: it is a folder name.
bool valid_instance_id(const QString& id);

//  What the command line says: the id after --installation (empty: it was not
//  given) and whether --installations is there. False: the id is missing or not
//  valid ("error" says what is wrong). Changes nothing of the program.
bool parse_instance_options(int argc, char* argv[], std::string& id, bool& overview, QString* error = nullptr);

//  Reads --installation <id> and --installations from the command line and keeps
//  them. Call it first thing in main(), before any path of the program is used.
//  False: see parse_instance_options(); nothing is kept then.
bool read_instance_options(int argc, char* argv[], QString* error = nullptr);

//  The process is the overview (--installations).
bool is_overview_process();

//  Whether this is a run of the tests: the installation then does not report
//  itself, and a second start of it is not turned into the overview.
void set_test_run(bool test_run);
bool is_test_run();


//  ---------------------------------------------------------------------
//  Where the data is: see PocketPilot_InstancePaths.h (shared_base_path(),
//  instance_meta_path(), instance_data_path(), runtime_base_path() ...)
//  ---------------------------------------------------------------------

//  A name for something that each installation keeps for itself in the
//  settings of the user (QSettings): the key for installation 1, a key of its
//  own for the others.
QString instance_setting_key(const QString& key);
QString instance_setting_key_for(const QString& key, const std::string& id);


//  ---------------------------------------------------------------------
//  The list of installations, and what an installation tells about itself
//  ---------------------------------------------------------------------

struct InstanceInfo{
    //  The folder name: "1", "2", ...
    QString id;
    //  The name that was given; empty: "Installation <id>" (in the language of the program).
    QString name;
    bool main = false;
    //  It wrote its status in the last few seconds.
    bool running = false;
    //  Made a moment ago, no status yet: the process is starting.
    bool starting = false;
    //  What it is doing: "setup", "games", "program", "running" (a program runs);
    //  empty when it does not run.
    QString state;
    //  What its window shows ("Pokémon FireRed: AutoStory"); may be empty.
    QString title;
    qint64 pid = 0;
    //  The miniature of the last time; null: there is none yet.
    QImage picture;
};

//  How long a status counts as "it runs".
constexpr qint64 INSTANCE_ALIVE_MSEC = 6000;
//  How long a new installation is "starting".
constexpr qint64 INSTANCE_STARTING_MSEC = 20000;
//  The size of a miniature.
constexpr int MINIATURE_WIDTH = 480;
constexpr int MINIATURE_HEIGHT = 270;

class InstanceRegistry{
public:
    //  "scratch": the folder of the program, with "/" at the end.
    explicit InstanceRegistry(const std::string& scratch);

    const QString& scratch() const{
        return m_scratch;
    }
    //  The folder with a folder for each installation.
    QString root() const;
    //  The folder of one: its status and its miniature.
    QString folder(const QString& id) const;

    //  Installation 1 first, then the others by number. "now": milliseconds
    //  since the epoch (what the status times are compared with).
    std::vector<InstanceInfo> list(qint64 now, bool with_pictures = true) const;

    //  Makes the next installation: a folder and installation.json. Returns its
    //  id; empty when that did not work.
    QString create(qint64 now) const;

    //  Deletes an installation's folder with everything in it (its settings, its
    //  logs and pictures). Not installation 1, and not one that runs.
    bool remove(const QString& id, qint64 now) const;

    //  The status of one, without its picture.
    InstanceInfo read(const QString& id, qint64 now, bool with_picture) const;

private:
    QString m_scratch;
};


//  What this process tells the overview: the status and, when "picture" is not
//  null, the miniature (it is kept as it was otherwise). Does nothing in a run
//  of the tests. "now": 0 is the time of the clock.
void write_instance_report(const QString& state, const QString& title, const QImage& picture, qint64 now = 0);
//  The same into any folder (with "/" at the end), for a process that is "pid" (0: this one).
bool write_instance_report_to(
    const QString& folder, qint64 pid, qint64 now,
    const QString& state, const QString& title, const QImage& picture
);
//  The installation ends: it is no longer "running" for the overview.
void clear_instance_report();

//  The miniature of what an installation shows now, MINIATURE_WIDTH x
//  MINIATURE_HEIGHT: the latest picture of a capture card that runs, otherwise
//  of "window" when it is on the screen (null when there is none).
QImage instance_picture(QWidget* window);
//  The same for a picture that is given (in a black frame of that size).
QImage fit_miniature(const QImage& picture);


//  ---------------------------------------------------------------------
//  One installation runs once
//  ---------------------------------------------------------------------

//  Held while an installation runs: the lock file in its folder (the overview
//  has its own file, hub.lock, in the folder Installations).
class InstanceGuard{
public:
    //  "folder": the installation's folder (with "/"; made when it is not there).
    explicit InstanceGuard(const QString& folder, const QString& file_name = "running.lock");
    ~InstanceGuard();
    //  False: another process holds it, the installation already runs.
    bool acquired() const;

private:
    std::unique_ptr<QLockFile> m_lock;
};


//  ---------------------------------------------------------------------
//  The windows talk
//  ---------------------------------------------------------------------

//  The name of the local connection of "what": "hub" or "i<id>".
QString instance_link_name(const std::string& scratch, const QString& what);

//  Listens on a local connection; every line that comes in is a command, and the
//  handler is called with it, on the thread of the event loop. The answer is "ok".
class InstanceLink{
public:
    InstanceLink(const QString& name, std::function<void(const QString& command)> handler);
    ~InstanceLink();
    //  False: the name is taken (another process listens on it).
    bool listening() const;

private:
    std::unique_ptr<QLocalServer> m_server;
};

//  Sends a command; true when somebody listened and answered it.
bool send_instance_command(const QString& name, const QString& command, int timeout_msec = 800);

//  The windows of this installation (the setup, the main window) to the front,
//  also when they were minimized.
void bring_instance_forward();
//  A window of the installation that the overview may bring forward and a
//  miniature may be made of: the setup's, the main window.
void mark_instance_window(QWidget* window);


//  ---------------------------------------------------------------------
//  Starting installations and the overview
//  ---------------------------------------------------------------------

//  Starts an installation as a process of its own. True when the process started.
bool start_instance_process(const QString& id);
//  Opens an installation from the overview: the window of the one that runs
//  comes forward, the one that does not run is started.
bool open_instance(const QString& id);

//  Starts the overview as a process of its own.
bool start_overview_process();
//  The overview to the front: it is started when it is not there. What an
//  installation's button "Installations" does (the caller puts its own windows away).
bool show_instances_overview();

//  The command line of the processes that are started.
QStringList instance_arguments(const QString& id);
QStringList overview_arguments();


//  ---------------------------------------------------------------------
//  The test
//  ---------------------------------------------------------------------

//  The checks that need no window: ids, folders, the list, the status and the
//  miniature, the guard, the connection. The log gets a line for each that
//  failed and one with the count; returns how many failed.
int run_instances_checks(Logger& logger);

//  --installation-test: this process is an installation with a folder of its
//  own; it checks that, reports itself, and answers on its connection. 0: well.
int run_instance_test(QApplication& application);

//  The video preview of the overview as a window; see PocketPilot_Setup.h
//  (run_instances_overview): what it needs of the program is made here.
int run_instances_hub(QApplication& application, const QString& test_folder);


}
}
#endif

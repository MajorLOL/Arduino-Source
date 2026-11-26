
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
//#include <QTextStream>
#include <QApplication>
#include "Common/Cpp/Logging/LastLogTracker.h"
#include "Common/Cpp/Logging/FileLogger.h"
#include "Common/Cpp/Logging/GlobalLogger.h"
#include "Common/Cpp/Logging/MultiOutputLogger.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Concurrency/Qt6.9ThreadBugWorkaround.h"
#include "Common/Cpp/Concurrency/AsyncTask.h"
#include "Common/Cpp/Concurrency/FireForgetDispatcher.h"
#include "Common/Cpp/Concurrency/Watchdog.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/ImageResolution.h"
#include "Common/Cpp/ScopeExit.h"
#include "Common/Qt/GlobalThreadPoolsQt.h"
#include "StaticRegistration.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "CommonFramework/Panels/OptionsPanel.h"
#include "CommonFramework/VideoPipeline/Backends/CameraImplementations.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "Tests/CommandLineTests.h"
#include "ControllerInput/ControllerInput.h"
#ifdef PA_SDL
#include "ControllerInput/Gamepad/GlobalGamepadTracker.h"
#endif
#include "Controllers/SerialPort/SerialPortPollerQt.h"
#include "Integrations/DiscordSettingsOption.h"
#include "Integrations/DiscordSocial/DiscordSocial.h"
#include "Integrations/DppIntegration/DppClient.h"
#include "Integrations/DiscordWebhook.h"
#include "VideoPipeline/Backends/MediaServicesQt6.h"
#include "ErrorReports/ProgramDumper.h"
#include "ErrorReports/ErrorReports.h"
#include "Environment/HardwareValidation.h"
//#include "Tools/StatsDatabase.h"
//#include "Windows/DpiScaler.h"
#include "Logging/Logger.h"
#include "Logging/OutputRedirector.h"
#include "Options/Environment/ThemeSelectorOption.h"
#include "Startup/SetupSettings.h"
#include "Windows/MainWindow.h"
#include "PocketPilot/PocketPilot_Instances.h"
#include "PocketPilot/PocketPilot_Intro.h"
#include "PocketPilot/PocketPilot_Language.h"
#include "PocketPilot/PocketPilot_OldSettings.h"
#include "PocketPilot/PocketPilot_Setup.h"
#include "PocketPilot/PocketPilot_SelfTest.h"
#include "GlobalSettingsPanel.h"
#include "GlobalServices.h"
#include "PersistentSettings.h"
#include "NintendoSwitch/NintendoSwitch_SettingsPanel.h"

using namespace PokemonAutomation;

Q_DECLARE_METATYPE(std::string)

void set_working_directory(){
    const std::string app_dir = Filesystem::application_binary_directory().string_slash_normalized();
    const std::string bundle_dir = "/Contents/MacOS";
    const std::string bundle_suffix = ".app" + bundle_dir;
    if (app_dir.size() > bundle_suffix.size() &&
        app_dir.compare(app_dir.size() - bundle_suffix.size(), bundle_suffix.size(), bundle_suffix) == 0){
        // a macOS bundle. Change working directory to the folder that hosts the .app folder.
        const std::string app_bundle_path = app_dir.substr(0, app_dir.size() - bundle_dir.size());
        const Filesystem::Path base_folder_path = Filesystem::Path(app_bundle_path).parent_path();
        std::error_code ec;
        Filesystem::set_current_path(base_folder_path, ec);
    }
}

namespace PokemonAutomation{



FileLogger& global_file_logger(){
    static FileLogger logger(
        GlobalThreadPools::unlimited_normal(),
        FileLoggerConfig{
            //  The overview of the installations has a log of its own: it runs at the
            //  same time as the installation whose folder it was started in.
            .file_path = USER_FILE_PATH() + Filesystem::application_binary_name().stem().string() +
                (PocketPilot::is_overview_process() ? "-Installations" : "") + ".log",
        }
    );
    return logger;
}

}



int run_program(int argc, char *argv[]){
//    qputenv("QT_RHI_BACKEND", "opengl");

#if defined(__APPLE__)
    Filesystem::set_startup_profile(argc, argv);
    QApplication application(argc, argv);
#else
    QApplication application(argc, argv);
#endif

    //  A run of the tests does not report itself to the overview, does not
    //  start other processes and does not take the lock of its installation.
    {
        bool test_run = false;
        for (int i = 1; i < argc; i++){
            if (strcmp(argv[i], PocketPilot::UI_SCREENSHOTS_OPTION) == 0 ||
                strcmp(argv[i], PocketPilot::SETUP_TEST_OPTION) == 0 ||
                strcmp(argv[i], PocketPilot::LANGUAGE_TEST_OPTION) == 0 ||
                strcmp(argv[i], PocketPilot::INTRO_TEST_OPTION) == 0 ||
                strcmp(argv[i], PocketPilot::TUTORIAL_TEST_OPTION) == 0 ||
                strcmp(argv[i], PocketPilot::INSTANCE_TEST_OPTION) == 0 ||
                strcmp(argv[i], PocketPilot::INSTANCES_HUB_TEST_OPTION) == 0 ||
                strcmp(argv[i], "--command-line-test-mode") == 0
            ){
                test_run = true;
            }
        }
        PocketPilot::set_test_run(test_run);
    }

    //  The folder of an installation that is not the first is made before the
    //  log is: the log is in it.
    Filesystem::create_directories(Filesystem::Path(RUNTIME_BASE_PATH()));

#ifdef PA_DPP
    // Configure OpenSSL before DPP or background tasks create a TLS context.
    std::string dpp_cert_message;
    bool dpp_cert_error = false;
    {
        const std::string cert_env = qgetenv("SSL_CERT_FILE").toStdString();
        const bool use_bundled_cert = cert_env.empty();
        const std::string cert_path = use_bundled_cert
            ? (Filesystem::application_binary_directory() / Filesystem::Path("dpp-cacert.pem")).string_slash_normalized()
            : cert_env;
        FileIO cert_file;
        if (!cert_file.open(Filesystem::Path(cert_path), FileMode::READ | FileMode::BINARY)){
            const std::string error = std::error_code(errno, std::generic_category()).message();
            dpp_cert_error = true;
            dpp_cert_message = "DPP CA certificate bundle cannot be read: " + cert_path +
                " (" + error + "). Discord bot connections may fail.";
        }else{
            if (use_bundled_cert){
                dpp_cert_error = !qputenv("SSL_CERT_FILE", cert_path.c_str());
#ifdef _WIN32
                // OpenSSL uses the release CRT even when Qt uses the debug CRT.
                // Set both environment copies so OpenSSL can find the bundle.
                const HMODULE crt = LoadLibraryExW(L"ucrtbase.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
                ScopeExit unload_crt([crt]{
                    if (crt){
                        FreeLibrary(crt);
                    }
                });
                using PutEnv = int (__cdecl *)(const char*, const char*);
                const PutEnv openssl_putenv = crt
                    ? reinterpret_cast<PutEnv>(GetProcAddress(crt, "_putenv_s"))
                    : nullptr;
                dpp_cert_error = dpp_cert_error ||
                    !openssl_putenv || openssl_putenv("SSL_CERT_FILE", cert_path.c_str()) != 0;
#endif
            }
            dpp_cert_message = dpp_cert_error
                ? "Failed to set SSL_CERT_FILE for DPP: " + cert_path
                : "DPP CA certificate bundle: " + cert_path;
        }
    }
#endif

    {
        const std::string qml_import_path =
            (Filesystem::application_binary_directory() / Filesystem::Path("qml")).string_slash_normalized();
        qputenv("QML_IMPORT_PATH", qml_import_path.c_str());
    }

    GlobalOutputRedirector redirect_stdout(std::cout, "stdout", Color());
    GlobalOutputRedirector redirect_stderr(std::cerr, "stderr", COLOR_RED);

    {
        MultiOutputLogger& logger = global_multi_logger();
        logger.add_listener(global_last_log_history());
        logger.add_listener(global_file_logger());
    }

    Logger& logger = global_logger_tagged();

    logger.log("================================================================================");
    logger.log("Starting Program...");
    logger.log("Current path: " + Filesystem::current_path().string_slash_normalized());
    logger.log("Executable path: " + Filesystem::application_binary_directory().string_slash_normalized());
    logger.log("Installation: " + PocketPilot::current_instance() + (PocketPilot::is_overview_process() ? " (the overview of the installations)" : ""));
    logger.log("Program setting folder: " + SETTINGS_PATH());
    logger.log("Program resources folder: " + RESOURCE_PATH());
#ifdef PA_DPP
    logger.log(dpp_cert_message, dpp_cert_error ? COLOR_RED : Color());
#endif

    qRegisterMetaType<size_t>("size_t");
    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<std::string>("std::string");
    qRegisterMetaType<Resolution>("Resolution");

#if defined(__linux) || defined(__APPLE__)
    // By default Qt uses native menubar but this only works on Windows.
    // We use menubar in our ButtonDiagram window to choose which controller's button mapping image to show.
    // So we fix it by don't using native menubar on non-Windows OS.
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
#endif

//#if QT_VERSION_MAJOR == 5 // AA_EnableHighDpiScaling is deprecated in Qt6
//    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
//#endif

    Filesystem::create_directories(Filesystem::Path(SETTINGS_PATH()));
    Filesystem::create_directories(Filesystem::Path(SCREENSHOTS_PATH()));



    ScopeExit cleanup([]{
#ifdef PA_SDL
        global_gamepad_tracker().stop();
#endif
        SerialPortPoller::instance().stop();
        GlobalMediaServices::instance().stop();
    });

    //  Several novice developers struggled to build and run the program due to missing Resources folder.
    //  Add this check to pop a message box when Resources folder is missing.
    if (!check_resource_folder(logger)){
        return 1;
    }

    //  An installation runs once: starting one that already runs shows the
    //  overview of the installations instead (where it can be opened, and a new
    //  one made). Nothing of it is read or written first. Not for the tests, and
    //  not for the overview itself, which has its own way of being once.
    std::unique_ptr<PocketPilot::InstanceGuard> instance_guard;
    std::unique_ptr<PocketPilot::InstanceLink> instance_link;
    if (!PocketPilot::is_test_run() && !PocketPilot::is_overview_process()){
        const std::string scratch = PocketPilot::shared_base_path();
        instance_guard = std::make_unique<PocketPilot::InstanceGuard>(
            QString::fromStdString(PocketPilot::instance_meta_path(scratch, PocketPilot::current_instance()))
        );
        if (!instance_guard->acquired()){
            logger.log(
                "Installation " + PocketPilot::current_instance() + " runs already: showing the overview of the installations.",
                COLOR_BLUE
            );
            PocketPilot::show_instances_overview();
            return 0;
        }
        //  The overview asks it to come forward on this connection.
        instance_link = std::make_unique<PocketPilot::InstanceLink>(
            PocketPilot::instance_link_name(scratch, QString::fromStdString("i" + PocketPilot::current_instance())),
            [](const QString& command){
                if (command == "show"){
                    PocketPilot::bring_instance_forward();
                }
            }
        );
    }

    //  Read program settings from json file: ClaudeGG-Settings.json (named
    //  after the program; SerialPrograms-Settings.json before build 98, whose
    //  settings are taken over at the first start under the new name).
    if (settings_taken_over_from_old_name()){
        logger.log(
            std::string("Settings: taken over from ") + PocketPilot::OLD_SETTINGS_FILE_NAME +
            " (the program was called SerialPrograms before). The old file is still there.",
            COLOR_BLUE
        );
    }
    try{
        PERSISTENT_SETTINGS().read();
    }catch (const FileException& error){
        logger.log(error.message(), COLOR_RED);
    }catch (const ParseException& error){
        logger.log(error.message(), COLOR_RED);
    }

    //  The theme, before any window is made. Settings that were saved have
    //  applied it when they were read; a new installation has nothing to
    //  read. Not in the main window's constructor: a change of style sends
    //  an event to every widget, also to a window that is still being made.
    apply_current_theme();

    //  The intro: the short film with the two Joy-Cons. Not in the tests
    //  (nobody is there to watch it) and not when the setting or --no-intro
    //  turns it off. It plays on the screen of the setup (see below), so that
    //  the film goes over into the first question without a window opening or
    //  closing in between. The test of the intro itself decodes the film
    //  without a window.
    //  (Not for an installation that is not the first: the film was seen. It
    //  starts at once with the setup.)
    bool play_intro = GlobalSettings::instance().PLAY_INTRO && PocketPilot::is_main_instance();
    {
        bool intro_test = false;
        bool tutorial_test = false;
        for (int i = 1; i < argc; i++){
            if (strcmp(argv[i], PocketPilot::NO_INTRO_OPTION) == 0 ||
                strcmp(argv[i], PocketPilot::UI_SCREENSHOTS_OPTION) == 0 ||
                strcmp(argv[i], "--command-line-test-mode") == 0
            ){
                play_intro = false;
            }
            if (strcmp(argv[i], PocketPilot::INTRO_TEST_OPTION) == 0){
                intro_test = true;
            }
            if (strcmp(argv[i], PocketPilot::TUTORIAL_TEST_OPTION) == 0){
                tutorial_test = true;
            }
        }
        if (intro_test){
            return PocketPilot::run_intro_test(application);
        }
        if (tutorial_test){
            //  What it found goes in the log, where the workflow reads it.
            QStringList report;
            const int code = PocketPilot::run_tutorial_test(application, report);
            Logger& test_logger = global_logger_tagged();
            for (const QString& line : report){
                test_logger.log(line.toStdString(), Color());
            }
            return code;
        }
    }

    //  The language: put into use now, before any window is made. The setup
    //  (the language, then the microcontroller) is asked once, on the screen
    //  of the intro. Not in the tests (nobody is there to answer).
    {
        bool ask_setup = true;
        bool language_test = false;
        bool setup_test = false;
        bool instance_test = false;
        bool hub_test = false;
        QString setup_folder;
        QString hub_folder;
        for (int i = 1; i < argc; i++){
            if (strcmp(argv[i], PocketPilot::INSTANCE_TEST_OPTION) == 0){
                instance_test = true;
            }
            if (strcmp(argv[i], PocketPilot::INSTANCES_HUB_TEST_OPTION) == 0){
                hub_test = true;
                if (i + 1 < argc){
                    hub_folder = QString::fromLocal8Bit(argv[i + 1]);
                }
            }
            if (strcmp(argv[i], PocketPilot::NO_LANGUAGE_PROMPT_OPTION) == 0 ||
                strcmp(argv[i], PocketPilot::UI_SCREENSHOTS_OPTION) == 0 ||
                strcmp(argv[i], PocketPilot::INTRO_TEST_OPTION) == 0 ||
                strcmp(argv[i], "--command-line-test-mode") == 0
            ){
                ask_setup = false;
            }
            if (strcmp(argv[i], PocketPilot::LANGUAGE_TEST_OPTION) == 0){
                language_test = true;
            }
            if (strcmp(argv[i], PocketPilot::SETUP_TEST_OPTION) == 0){
                setup_test = true;
                if (i + 1 < argc){
                    setup_folder = QString::fromLocal8Bit(argv[i + 1]);
                }
            }
        }
        if (language_test){
            return PocketPilot::run_language_test(application);
        }
        PocketPilot::install_language_support(application);
        if (setup_test){
            return PocketPilot::run_setup_test(application, setup_folder);
        }
        //  The tests of the installations, and the overview of them: a process
        //  of its own that shows the miniatures (see PocketPilot_Instances.h).
        if (instance_test){
            return PocketPilot::run_instance_test(application);
        }
        if (hub_test){
            return PocketPilot::run_instances_hub(application, hub_folder);
        }
        if (PocketPilot::is_overview_process()){
            return PocketPilot::run_instances_hub(application, QString());
        }
        //  The setup ends with the choice of a game: these are the games.
        PocketPilot::set_game_lister([](std::vector<PocketPilot::HomeGame>& games, std::vector<PocketPilot::HomeProgram>& programs){
            std::string error;
            describe_all_games(games, &error, &programs);
            if (!error.empty()){
                global_logger_tagged().log("Setup: the games could not be listed: " + error, COLOR_RED);
            }
        });
        //  The options of a program, as the widgets that its page has, for the
        //  panel next to the list of programs.
        PocketPilot::set_options_maker([](QWidget* parent, int game, const QString& program){
            return make_setup_options(parent, game, program);
        });
        //  The test of the interface also checks that the games can be listed
        //  now, before the main window is made, as the setup needs.
        for (int i = 1; i < argc; i++){
            if (strcmp(argv[i], PocketPilot::UI_SCREENSHOTS_OPTION) == 0){
                global_logger_tagged().log(
                    "Setup: the games before the window: " + std::to_string(PocketPilot::count_games_for_setup()) + "."
                );
                global_logger_tagged().log(
                    "Setup: the programs before the window: " + std::to_string(PocketPilot::count_programs_for_setup()) + "."
                );
                global_logger_tagged().log(
                    "Setup: the options before the window: " + std::to_string(PocketPilot::count_options_for_setup()) + "."
                );
            }
        }
        //  The intro and the setup: one screen. It stays up until the main
        //  window is shown (see below).
        PocketPilot::show_start_screens(play_intro, ask_setup);
    }

    //  Load the Switch framework settings now, not only when their panel is opened.
    OptionsPanelWrapper<NintendoSwitch::ConsoleSettingsPanel>().make_panel()->load_json_from_global();

    //  Preload a bunch of stuff now so they are ready later.
    SerialPortPoller::instance().ports();
    get_all_cameras();

    //  Force all the Qt thread pools to be constructed now on the main thread.
    GlobalThreadPools::qt_worker_threadpool();
    GlobalThreadPools::qt_event_threadpool();

    bool augment_test_color_distortions = false;
    std::string test_substring;
    bool test_substring_missing_value = false;
    //  The test of the interface: the folder for its pictures.
    std::string ui_screenshots_folder;
    bool ui_screenshots_missing_value = false;
    for (size_t i = 0; i < argc; i++){
        constexpr const char* force_run_tests = "--command-line-test-mode";
        constexpr const char* distort_test_images = "--augment-color-distortions";
        constexpr const char* filter_tests_substring = "--test-substring";

        if (strcmp(argv[i], PocketPilot::UI_SCREENSHOTS_OPTION) == 0){
            if (i + 1 < (size_t)argc){
                ui_screenshots_folder = argv[++i];
                continue;
            }else{
                ui_screenshots_missing_value = true;
            }
        }

        if (strcmp(argv[i], force_run_tests) == 0){
            GlobalSettings::instance().COMMAND_LINE_TEST_MODE = true;
        }
        if (strcmp(argv[i], distort_test_images) == 0){
            augment_test_color_distortions = true;
        }
        if (strcmp(argv[i], filter_tests_substring) == 0){
            if (i + 1 < argc){
                test_substring = argv[++i];
            }else{
                test_substring_missing_value = true;
            }
        }
    }
    if (test_substring_missing_value){
        global_logger_command_line().log("--test-substring requires a value", COLOR_RED);
        return 1;
    }
    if (ui_screenshots_missing_value){
        global_logger_command_line().log(std::string(PocketPilot::UI_SCREENSHOTS_OPTION) + " requires a folder", COLOR_RED);
        return 1;
    }

    if (GlobalSettings::instance().COMMAND_LINE_TEST_MODE){
        return run_command_line_tests(augment_test_color_distortions, test_substring);
    }

    //  Check whether the hardware is powerful enough to run this program.
    //  (Not for the test of the interface: nobody is there to answer a
    //  message box, and it runs no program.)
    if (ui_screenshots_folder.empty() && !check_hardware()){
        return 1;
    }

    //  No check for a new version: that check is for Pokemon Automation's
    //  own releases, not for this fork's builds.

    set_working_directory();

    //  Run this asynchronously to we don't block startup.
    AsyncTask task = send_all_unsent_reports(logger, true);



    Integration::DiscordIntegrationSettingsOption& discord_settings = GlobalSettings::instance().DISCORD->integration;
    if (discord_settings.run_on_start){
#ifdef PA_DPP
        Integration::DppClient::Client::instance().connect();
#endif
        discord_settings.on_config_value_changed(nullptr);
    }

#ifdef PA_SOCIAL_SDK
    if (GlobalSettings::instance().RICH_PRESENCE){
        Integration::DiscordSocialSDK::DiscordSocial::instance().run();
    }
#endif


#ifdef PA_SDL
    global_gamepad_tracker().start();
#endif
    //  The window is the screens of the setup (the games, the programs, the
    //  program itself). The test of the interface goes through this window
    //  with a clock of its own, then through one that its pictures drive.
    std::unique_ptr<MainWindow> window = std::make_unique<MainWindow>();
    MainWindow& w = *window;
    if (!ui_screenshots_folder.empty()){
        //  The test of the interface draws the window without putting it
        //  on the screen: the screen of a build machine is small, and a
        //  window on the screen cannot be larger than it.
        w.setAttribute(Qt::WA_DontShowOnScreen, true);
    }
    w.show();
    w.raise(); // bring the window to front on macOS
    set_permissions(w);
    //  The screen of the intro and the setup goes now, over the window.
    PocketPilot::release_setup_curtain();

    //  The test of the interface: pictures of every page, then exit. The
    //  settings are not written, since the test changes the theme.
    if (!ui_screenshots_folder.empty()){
        const int window_code = PocketPilot::run_ui_screenshots(application, w, ui_screenshots_folder);
        window.reset();
        const int app_code = PocketPilot::run_app_screenshots(application, ui_screenshots_folder + "/app");
        return window_code != 0 ? window_code : app_code;
    }

    int ret = application.exec();

    //  The overview sees that it does not run any more.
    PocketPilot::clear_instance_report();

    //  Write program settings back to the json file.
    std::cout << "Saving Settings..." << std::endl;
    PERSISTENT_SETTINGS().write();

    return ret;
}


int main(int argc, char *argv[]){
#if 0
    // Retrieve and store program name
    set_program_path(argv[0]);
#endif

    //  Which installation this is: before any folder of the program is asked for.
    {
        QString error;
        if (!PocketPilot::read_instance_options(argc, argv, &error)){
            std::cerr << error.toStdString() << std::endl;
            return 1;
        }
    }

#if defined(__linux__)
    // Qt multimedia, default to gstreamer to prevent flickering
    // Easier than the alternative which is compiling qt6multimedia with QT_DEFAULT_MEDIA_BACKEND
    // See: https://doc.qt.io/qt-6.5/qtmultimedia-index.html
    if (qEnvironmentVariableIsEmpty("QT_MEDIA_BACKEND")){
        qputenv("QT_MEDIA_BACKEND", "gstreamer");
    }
#endif

    //  So far, this is only needed on Mac where static initialization is fucked up.
    PokemonAutomation::register_all_statics();

    setup_crash_handler();

    int ret = run_program(argc, argv);


#ifdef PA_SOCIAL_SDK
    Integration::DiscordSocialSDK::DiscordSocial::instance().stop();
#endif

#ifdef PA_DPP
    Integration::DppClient::Client::instance().stop();
#endif

    //  We must clear the OCR cache or it will crash on Linux when the library
    //  unloads before the cache is destructed from static memory.
    OCR::clear_ocr_cache();

    //  Stop the controllers.
    global_input_stop();

    //  Stop misc. services.
    Integration::DiscordWebhook::DiscordWebhookSender::instance().stop();
    SystemSleepController::instance().stop();
    global_periodic_runner().stop();
    global_watchdog().stop();
    global_file_logger().stop();

//
//  Workaround Qt 6.9 thread-adoption bug on Windows.
//      https://github.com/PokemonAutomation/Arduino-Source/issues/570
//      https://bugreports.qt.io/browse/QTBUG-131892
//
//  Joining threads on Qt 6.9+ Windows will hang!
//
//  Command-line test mode gets the same exit treatment: the tests are
//  done by the time we get here, nothing after this needs the thread
//  pools, and skipping the joins avoids a hang with no diagnostics when
//  a thread is still inside a task that never finishes.
//
#ifdef PA_ENABLE_QT_ADOPTION_WORKAROUND
    std::cout << "Qt6.9 bug workaround: std::quick_exit() to avoid thread join hangs." << std::endl;
    std::quick_exit(ret);
#else
    if (GlobalSettings::instance().COMMAND_LINE_TEST_MODE){
        std::cout << "Command-line test mode: std::quick_exit() to avoid thread join hangs." << std::endl;
        std::quick_exit(ret);
    }
#endif

    //  Force stop the thread pools.
    //  This is where all the threads in the program are joined.
    PokemonAutomation::GlobalThreadPools::computation_realtime().stop();
    PokemonAutomation::GlobalThreadPools::computation_normal().stop();
    PokemonAutomation::GlobalThreadPools::unlimited_realtime().stop();
    PokemonAutomation::GlobalThreadPools::unlimited_normal().stop();
    PokemonAutomation::global_dispatcher.stop();

    GlobalThreadPools::qt_worker_threadpool().stop();
    GlobalThreadPools::qt_event_threadpool().stop();

    std::cout << "Exiting main()..." << std::endl;

    return ret;
}

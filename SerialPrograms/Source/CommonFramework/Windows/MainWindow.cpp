/*  Main Window UI
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <utility>
//#include <QStatusBar>
#include <QVBoxLayout>
#include "Common/Cpp/ScopeExit.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/MultiOutputLogger.h"
#include "Common/Cpp/Logging/GlobalLogger.h"
#include "Common/Cpp/Logging/LastLogTracker.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/CpuId/CpuId.h"
#include "Common/Cpp/ColoredText.h"
#include "Common/Qt/UiStateQtWidget.h"
#include "PanelLists.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/PersistentSettings.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/Logging/LoggerWindow.h"
#include "CommonFramework/Options/ResolutionOption.h"
#include "CommonFramework/Options/Environment/ThemeSelectorOption.h"
#include "CommonFramework/Options/QtWidget/WallpaperWidget.h"
#include "CommonFramework/Windows/DpiScaler.h"
#include "PocketPilot/PocketPilot_About.h"
#include "PocketPilot/PocketPilot_Home.h"
#include "PocketPilot/PocketPilot_Instances.h"
#include "PocketPilot/PocketPilot_Language.h"
#include "PocketPilot/PocketPilot_Logo.h"
#include "PocketPilot/PocketPilot_Setup.h"
#include "PocketPilot/Assistant/Assistant_Panel.h"
#include "PocketPilot/Assistant/Assistant_Service.h"
#include "WindowTracker.h"
#include "ButtonDiagram.h"
#include "MainWindow.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{


MainWindow::MainWindow(QWidget* parent, bool app_test)
    : QMainWindow(parent)
    , m_current_panel_widget(nullptr)
{
    global_panel_holder() = this;

    //  (The theme has been applied before this window was made: Main.cpp.)

    if (objectName().isEmpty()){
        setObjectName(QString::fromUtf8("MainWindow"));
    }
    this->setWindowIcon(PocketPilot::app_icon());
    //  The overview of the installations brings this window forward, and makes
    //  its miniature from it.
    PocketPilot::mark_instance_window(this);
//    QSize window_size = PERSISTENT_SETTINGS().window_size;
    resize(
        GlobalSettings::instance().WINDOW_SIZE->WIDTH,
        GlobalSettings::instance().WINDOW_SIZE->HEIGHT
    );
    // move main window to desired position on startup
    // auto const screen_geometry = QGuiApplication::primaryScreen()->availableGeometry();
    // uint32_t const screen_width = (uint32_t)screen_geometry.width();
    // uint32_t const screen_height = (uint32_t)screen_geometry.height();
    int32_t x_pos_main = GlobalSettings::instance().WINDOW_SIZE->X_POS;
    int32_t y_pos_main = GlobalSettings::instance().WINDOW_SIZE->Y_POS;
    int32_t move_x_main = move_x_within_screen_bounds(x_pos_main);
    int32_t move_y_main = move_y_within_screen_bounds(y_pos_main);
    move(move_x_main, move_y_main);

    centralwidget = new WallpaperWidget(this);
    centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
    setCentralWidget(centralwidget);
//    statusbar = new QStatusBar(this);
//    statusbar->setObjectName(QString::fromUtf8("statusbar"));
//    setStatusBar(statusbar);
    std::string title = PROGRAM_NAME + " (build " + PROGRAM_BUILD + ")";
#if defined(__APPLE__)
    if (!Filesystem::STARTUP_PROFILE().empty()){
        setWindowTitle(QString::fromStdString(title + " [Profile: " + Filesystem::STARTUP_PROFILE() + "]"));
    }else{
        setWindowTitle(QString::fromStdString(title));
    }
#else
    setWindowTitle(QString::fromStdString(title));
#endif

    //  The list of programs stays, out of sight: it makes the programs and
    //  loads their panels, as it always did. (The screens show them.)
    m_program_list = new ProgramSelect(*centralwidget, *this);
    m_program_list->hide();

    //  The windows behind the buttons along the top.
    m_button_diagram.reset(new ButtonDiagram());
    {
        m_output_window.reset(new LoggerWindow(nullptr));
        global_multi_logger().add_listener(*m_output_window);

        // get snapshot of the saved initial x/y position, since activating the window seems to change the window coordinates, 
        // which then causes initial x/y position to change, due to triggering moveEvent().
        int32_t x_pos_log = GlobalSettings::instance().LOG_WINDOW_SIZE->X_POS;
        int32_t y_pos_log = GlobalSettings::instance().LOG_WINDOW_SIZE->Y_POS;    

        if (GlobalSettings::instance().LOG_WINDOW_STARTUP){ // show the Output Window on startup
            m_output_window->show();
            m_output_window->raise(); // bring the window to front on macOS
            m_output_window->activateWindow(); // bring the window to front on Windows
        }

        // move the output window to desired position on startup
        int32_t move_x_log = move_x_within_screen_bounds(x_pos_log);
        int32_t move_y_log = move_y_within_screen_bounds(y_pos_log);
        m_output_window->move(move_x_log, move_y_log);
    }

    //  The screens first (they start on the games): no program is loaded at
    //  the start, the games are shown, as at a first start.
    make_app_view(app_test);

    //  Claude, in the panel at the right. Open if it was when the program
    //  closed.
    make_assistant(nullptr, SETTINGS_PATH());

    GlobalSettings::instance().WINDOW_SIZE->WIDTH.add_listener(*this);
    GlobalSettings::instance().WINDOW_SIZE->HEIGHT.add_listener(*this);
    GlobalSettings::instance().WINDOW_SIZE->X_POS.add_listener(*this);
    GlobalSettings::instance().WINDOW_SIZE->Y_POS.add_listener(*this);    
    GlobalSettings::instance().THEME->add_listener(*this);
    GlobalSettings::instance().WALLPAPER.add_listener(*this);
    SystemSleepController::instance().add_listener(*this);
    update_wallpaper();

    //  A program asks for its options to be written to the settings file
    //  (request_panel_settings_save(), from its own thread): done here, on
    //  the interface's thread, like every other write of the settings.
    set_panel_settings_saver([this]{
        QMetaObject::invokeMethod(this, [this]{
            if (m_current_panel != nullptr){
                try{
                    m_current_panel->save_settings();
                }catch (...){}
            }
        }, Qt::QueuedConnection);
    });

    //  The game that was chosen at the end of the setup: shown once the
    //  window is up (at the first start there was no window to show it in).
    //  And the setup that is opened from the settings later shows it at once.
    {
        const QString chosen = PocketPilot::take_chosen_game();
        const QString chosen_program = PocketPilot::take_chosen_program();
        if (!chosen.isEmpty()){
            QMetaObject::invokeMethod(this, [this, chosen, chosen_program]{
                open_game(chosen, chosen_program);
            }, Qt::QueuedConnection);
        }
        PocketPilot::set_game_opener([this](const QString& game, const QString& program){
            open_game(game, program);
        });
    }
//    cout << "Done constructing" << endl;
}
MainWindow::~MainWindow(){
    PocketPilot::set_game_opener(nullptr);
    set_panel_settings_saver(nullptr);
    //  The panel before what it shows.
    delete m_assistant_panel;
    m_assistant_panel = nullptr;
    m_assistant.reset();
    close_panel();
    SystemSleepController::instance().remove_listener(*this);
    GlobalSettings::instance().WINDOW_SIZE->WIDTH.remove_listener(*this);
    GlobalSettings::instance().WINDOW_SIZE->HEIGHT.remove_listener(*this);
    GlobalSettings::instance().WINDOW_SIZE->X_POS.remove_listener(*this);
    GlobalSettings::instance().WINDOW_SIZE->Y_POS.remove_listener(*this);
    GlobalSettings::instance().THEME->remove_listener(*this);
    GlobalSettings::instance().WALLPAPER.remove_listener(*this);
    if (m_output_window){
        global_multi_logger().remove_listener(*m_output_window);
    }
}

PocketPilot::AboutInfo MainWindow::about_info() const{
    PocketPilot::AboutInfo info;
    info.name = QString::fromStdString(PROGRAM_NAME);
    info.build = QString::fromStdString(PROGRAM_BUILD);
    info.upstream_name = QString::fromStdString(UPSTREAM_PROGRAM_NAME);
    info.upstream_version = QString::fromStdString(PROGRAM_VERSION);
    info.architecture = PA_ARCH_STRING;
    info.compiler = QString::fromStdString(COMPILER_VERSION);
    info.project_url = QString::fromStdString(PROJECT_HOME_URL);
    info.upstream_url = QString::fromStdString(PROJECT_SOURCE_URL);
    return info;
}

//  The page with the games instead of the program that is open. Not while
//  a program runs.
void MainWindow::show_games(){
    if (m_busy || m_panel_transition){
        return;
    }
    m_panel_transition = true;
    ScopeExit cleanup([&]{
        m_panel_transition = false;
    });
    close_panel();
    m_program_list->clear_current();
    m_app->show_games();
}
void MainWindow::show_settings(){
    if (m_busy){
        return;
    }
    if (report_new_panel_intent(GlobalSettings_Descriptor::instance())){
        m_program_list->clear_current();
        load_panel(nullptr, GlobalSettings_Descriptor::instance().make_panel());
    }
}
bool MainWindow::open_program(const std::string& game, const std::string& program){
    if (m_busy){
        return false;
    }
    return m_program_list->open_program(game, program);
}
bool MainWindow::open_game(const QString& game){
    if (m_busy || m_panel_transition){
        return false;
    }
    //  Not over a panel that is open (the settings, say): the page with the
    //  games first, then the game on it.
    if (m_current_panel_widget != nullptr){
        show_games();
    }
    //  Its programs, on the screen after the games.
    for (size_t c = 0; c < m_game_list.size(); c++){
        if (m_game_list[c].name == game){
            m_app->show_programs((int)c);
            return true;
        }
    }
    return false;
}
bool MainWindow::open_game(const QString& game, const QString& program){
    if (!open_game(game)){
        return false;
    }
    if (program.isEmpty()){
        return true;
    }
    //  The program that was chosen in the setup: its page, in the game's list.
    return open_program(game.toStdString(), program.toStdString());
}

//  Claude: the service with what the tools may look at in this program, and
//  the panel that shows it.
void MainWindow::make_assistant(
    std::unique_ptr<PocketPilot::Assistant::HttpTransport> transport,
    const std::string& settings_folder
){
    namespace Assistant = PocketPilot::Assistant;
    delete m_assistant_panel;
    m_assistant_panel = nullptr;
    m_assistant.reset();

    Assistant::AppHooks hooks;
    hooks.name = QString::fromStdString(PROGRAM_NAME);
    hooks.build = QString::fromStdString(PROGRAM_BUILD);
    hooks.upstream = QString::fromStdString(UPSTREAM_PROGRAM_NAME + " " + PROGRAM_VERSION);
    hooks.settings_folder = QString::fromStdString(settings_folder);
    //  Where this build's source is: "https://github.com/<owner>/<name>/tree/<branch>".
    {
        const QStringList parts = QString::fromStdString(PROJECT_HOME_URL).split('/', Qt::SkipEmptyParts);
        const qsizetype host = parts.indexOf("github.com");
        if (host >= 0 && parts.size() >= host + 5 && parts[host + 3] == "tree"){
            hooks.repository.owner = parts[host + 1];
            hooks.repository.name = parts[host + 2];
            hooks.repository.base_branch = parts.mid(host + 4).join('/');
        }
    }
    //  The programs, as the page with the games lists them.
    {
        std::vector<PocketPilot::HomeGame> games;
        std::vector<PocketPilot::HomeProgram> programs;
        m_program_list->describe(games, programs);
        std::vector<Assistant::ProgramInfo> list;
        for (const PocketPilot::HomeProgram& program : programs){
            if (program.game < 0 || program.game >= (int)games.size()){
                continue;
            }
            Assistant::ProgramInfo info;
            info.game = games[(size_t)program.game].name;
            info.section = program.section;
            info.name = program.name;
            info.description = program.description;
            list.emplace_back(std::move(info));
        }
        hooks.programs = [list]{
            return list;
        };
    }
    hooks.open_program = [this]{
        Assistant::OpenProgramInfo open;
        if (m_current_panel == nullptr){
            return open;
        }
        const PanelDescriptor& descriptor = m_current_panel->descriptor();
        open.open = true;
        open.name = QString::fromStdString(descriptor.display_name());
        //  The program's own settings have things that are nobody else's
        //  (where notifications go): only their name is told.
        if (&descriptor == &GlobalSettings_Descriptor::instance()){
            open.name = "Settings";
            return open;
        }
        open.game = QString::fromStdString(m_program_list->current_game());
        open.description = QString::fromStdString(descriptor.description());
        open.running = m_busy;
        try{
            open.settings = QString::fromStdString(m_current_panel->to_json().dump(2));
        }catch (...){}
        return open;
    };
    hooks.recent_log = [](int lines){
        QStringList text;
        for (const LogLine& line : global_last_log_history().get_recent((size_t)std::max(lines, 0))){
            text += (line.color == COLOR_RED ? "(red) " : "") + QString::fromStdString(line.text);
        }
        return text;
    };

    m_assistant = std::make_unique<Assistant::Service>(std::move(hooks), std::move(transport));
    //  Over the right of the window's screen.
    m_assistant_panel = new Assistant::Panel(*m_app->widget(), *m_assistant);
    m_assistant_panel->on_close = [this]{
        show_assistant(false);
    };
    m_app->set_claude(m_assistant_panel);
    m_app->show_claude(m_assistant->settings().panel_open);
}

void MainWindow::remake_assistant(
    std::unique_ptr<PocketPilot::Assistant::HttpTransport> transport,
    const std::string& settings_folder
){
    m_app->show_claude(false);
    make_assistant(std::move(transport), settings_folder);
}

void MainWindow::show_assistant(bool show){
    m_app->show_claude(show);
    if (m_assistant->settings().panel_open != show){
        m_assistant->settings().panel_open = show;
        m_assistant->save_settings();
    }
    if (show){
        m_assistant_panel->show_start_page();
        m_assistant_panel->focus_input();
    }
}

int32_t move_x_within_screen_bounds(int32_t x_pos){
    auto static const screen_geometry = QGuiApplication::primaryScreen()->availableGeometry();
    uint32_t static const screen_width = (uint32_t)screen_geometry.width();    
     // ensure move_x is greater than 0, but less than screen_width
    return scale_dpi_width(std::max(0, std::min(x_pos, (int32_t)(screen_width*0.97))));

}

int32_t move_y_within_screen_bounds(int32_t y_pos){
    auto static const screen_geometry = QGuiApplication::primaryScreen()->availableGeometry();
    uint32_t const screen_height = (uint32_t)screen_geometry.height();  
    // ensure move_y is greater than 0, but less than screen_height
    return scale_dpi_width(std::max(0, std::min(y_pos, (int32_t)(screen_height*0.97))));

}


void MainWindow::closeEvent(QCloseEvent* event){
    //  PocketPilot: the options of the panel that is open are written here,
    //  before anything is taken down. They were written only when the window
    //  was destroyed, after the assistant's panel: a close that did not get
    //  that far kept the settings of days before (the AutoStory's start and
    //  end, 17 and 18 November 2026).
    if (m_current_panel != nullptr){
        try{
            m_current_panel->save_settings();
        }catch (...){}
    }
    close_all_windows();
    QMainWindow::closeEvent(event);
}
void MainWindow::resizeEvent(QResizeEvent* event){
    m_pending_resize = true;
    GlobalSettings::instance().WINDOW_SIZE->WIDTH.set(width());
    GlobalSettings::instance().WINDOW_SIZE->HEIGHT.set(height());
    m_pending_resize = false;
}

void MainWindow::moveEvent(QMoveEvent* event){
    m_pending_move = true;
    GlobalSettings::instance().WINDOW_SIZE->X_POS.set(x());
    GlobalSettings::instance().WINDOW_SIZE->Y_POS.set(y());
    m_pending_move = false;
}

void MainWindow::close_panel() noexcept{
//    cout << "close_panel(): enter: " << m_current_panel_widget << endl;
    //  Must destroy the widget first since it references the instance.
    if (m_current_panel_widget != nullptr){
        //  Possible optimization. Hide it so Qt doesn't need to recalculate
        //  all the layouts.
        m_current_panel_widget->hide();

        //  The screen gives it back (it was only held there).
        m_app->unhost(m_current_panel_widget);
        QWidget* widget = m_current_panel_widget;
        m_current_panel_widget = nullptr;
        delete widget;
    }

//    cout << "close_panel(): mid: " << m_current_panel_widget << endl;

    //  Now it's safe to destroy the instance.
    if (m_current_panel == nullptr){
        return;
    }

    try{
        m_current_panel->save_settings();
    }catch (...){}

    m_current_panel.reset();
    m_current_panel_descriptor.reset();
}

bool MainWindow::report_new_panel_intent(const PanelDescriptor& descriptor){
    //  No active panel. Proceed with panel change.
    if (m_current_panel == nullptr){
        return true;
    }

    //  Panel is already active. Don't change.
    if (&m_current_panel->descriptor() == &descriptor){
        m_current_panel->save_settings();
        return false;
    }

    return true;
}
void MainWindow::load_panel(
    std::shared_ptr<const PanelDescriptor> descriptor,
    std::unique_ptr<PanelSession> panel
){
    if (m_panel_transition){
        global_logger_tagged().log(
            "Ignoring attempted panel change while existing one is still in progress.",
            COLOR_RED
        );
        return;
    }

    m_panel_transition = true;
    ScopeExit cleanup([&]{
        m_panel_transition = false;
    });
    close_panel();

    //  Make new widget.
    //  (No check for a new version here: that check is for Pokemon
    //  Automation's own releases, not for this fork's builds.)
    try{
        m_current_panel_widget = &dynamic_cast<UiComponentQtWidget*>(
            panel->make_ui_component(this).release()
        )->widget();
//        cout << "load_panel() = " << m_current_panel_widget << endl;
        m_current_panel_descriptor = std::move(descriptor);
        m_current_panel = std::move(panel);
        //  Held by the window's screen, under its header.
        const PanelDescriptor& current = m_current_panel->descriptor();
        int game = -1;
        QString game_name;
        QString title = "Settings";
        if (&current != &GlobalSettings_Descriptor::instance()){
            title = QString::fromStdString(current.display_name());
            const QString name = QString::fromStdString(m_program_list->current_game());
            for (size_t c = 0; c < m_game_list.size(); c++){
                if (m_game_list[c].name == name){
                    game = (int)c;
                }
            }
            game_name = PocketPilot::short_game_name(name);
        }
        m_app->host(m_current_panel_widget, game, game_name, title);
        update_resume();
        return;
    }catch (Exception& e){
        e.log(global_logger_tagged());
    }catch (std::exception& e){
        global_logger_tagged().log(std::string("MainWindow::load_panel() - Exception: ") + e.what(), COLOR_RED);
    }catch (...){
        global_logger_tagged().log("MainWindow::load_panel() - Unknown Exception", COLOR_RED);
    }
    if (m_current_panel_widget != nullptr){
        m_app->unhost(m_current_panel_widget);
        delete m_current_panel_widget;
        m_current_panel_widget = nullptr;
    }
}
void MainWindow::on_busy(){
    m_busy = true;
    std::string text = "Running";
    if (m_current_panel != nullptr){
        text += " " + m_current_panel->descriptor().display_name();
    }
    m_app->set_state(true, QString::fromStdString(text));
}
void MainWindow::on_idle(){
    m_busy = false;
    m_app->set_state(false, "Ready");
}


void MainWindow::on_config_value_changed(void* object){
    if (object == &GlobalSettings::instance().WINDOW_SIZE->WIDTH || object == &GlobalSettings::instance().WINDOW_SIZE->HEIGHT){
        QMetaObject::invokeMethod(this, [this]{
            if (!m_pending_resize){
                resize(
                    GlobalSettings::instance().WINDOW_SIZE->WIDTH,
                    GlobalSettings::instance().WINDOW_SIZE->HEIGHT
                );
            }
        });
    }else if (object == &GlobalSettings::instance().WINDOW_SIZE->X_POS || object == &GlobalSettings::instance().WINDOW_SIZE->Y_POS){
        QMetaObject::invokeMethod(this, [this]{
            if (!m_pending_move){
                move(
                    move_x_within_screen_bounds(GlobalSettings::instance().WINDOW_SIZE->X_POS),
                    move_y_within_screen_bounds(GlobalSettings::instance().WINDOW_SIZE->Y_POS)
                );
            }
        });        
    }else if (
        object == &*GlobalSettings::instance().THEME ||
        object == &GlobalSettings::instance().WALLPAPER
    ){
        update_wallpaper();
    }
}

void MainWindow::changeEvent(QEvent* event){
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange){
        update_wallpaper();
    }
}

void MainWindow::update_wallpaper(){
    //  A change of style or palette can arrive while the constructor has
    //  not made the central widget yet.
    if (centralwidget == nullptr){
        return;
    }
    GlobalSettings& settings = GlobalSettings::instance();
    const QColor surface_color = palette().color(QPalette::Active, QPalette::Window);
    centralwidget->set_appearance(
        settings.WALLPAPER.IMAGE_FIT,
        settings.WALLPAPER.IMAGE_OVERLAY,
        surface_color
    );
    centralwidget->set_image(
        settings.WALLPAPER.enabled(),
        QString::fromStdString(static_cast<std::string>(settings.WALLPAPER.IMAGE_PATH))
    );
    const QString error = centralwidget->take_error();

    if (!error.isEmpty()){
        global_logger_tagged().log(
            "Unable to load background image: " + error.toStdString(),
            COLOR_RED
        );
    }

    if (!centralwidget->active()){
        if (!centralwidget->styleSheet().isEmpty()){
            centralwidget->setStyleSheet(QString());
        }
        return;
    }

    const QString surface = QStringLiteral("rgba(%1, %2, %3, 215)")
        .arg(surface_color.red()).arg(surface_color.green()).arg(surface_color.blue());
    const QString stylesheet = QString::fromLatin1(
        // Shared wallpaper surfaces: apply the same translucent theme color.
        "QWidget#backgroundProgramHeader { background-color: %1; }"
        "QWidget#backgroundScrollContents { background-color: %1; }"

        // Shared layout containers and viewports: reveal the wallpaper surfaces.
        "QWidget#backgroundPanelRoot { background-color: transparent; }"
        "QScrollArea#backgroundScrollArea { background-color: transparent; border: none; }"
        "QWidget#backgroundScrollViewport { background-color: transparent; }"

        // QDarkStyle compatibility: its global QWidget background makes these containers opaque.
        "QLabel, QLabel:disabled { background-color: transparent; }"
        "QGroupBox, QGroupBox:disabled { background-color: transparent; }"
        "QWidget[backgroundContainer=\"true\"] { background-color: transparent; }"
    ).arg(surface);
    if (centralwidget->styleSheet() != stylesheet){
        centralwidget->setStyleSheet(stylesheet);
    }
}

void MainWindow::sleep_suppress_state_changed(SleepSuppress new_state){
    //  In words, without colors: the strip has the theme's color.
    QMetaObject::invokeMethod(this, [=, this]{
        switch (new_state){
        case PokemonAutomation::SleepSuppress::NONE:
            set_sleep_label("The computer may sleep");
            break;
        case PokemonAutomation::SleepSuppress::NO_SLEEP:
            set_sleep_label("The computer stays awake");
            break;
        case PokemonAutomation::SleepSuppress::SCREEN_ON:
            set_sleep_label("The screen stays on");
            break;
        }
    });
}
void MainWindow::update_resume(){
    if (m_app == nullptr){
        return;
    }
    std::string game, program;
    m_program_list->persistent_panel(game, program);
    m_app->set_resume(QString::fromStdString(game), QString::fromStdString(program));
}
void MainWindow::set_sleep_label(const QString& text){
    if (m_app != nullptr){
        m_app->set_sleep_text(text);
    }
}

//  The screens of the setup are the window. What the buttons along the top
//  do is what the buttons of the old top bar did.
void MainWindow::make_app_view(bool test){
    //  The games and their programs, as the list makes them.
    std::vector<PocketPilot::HomeProgram> programs;
    m_game_list.clear();
    m_program_list->describe(m_game_list, programs);

    PocketPilot::AppActions actions;
    actions.open_program = [this](int game, const QString& program){
        if (m_busy || m_panel_transition || game < 0 || game >= (int)m_game_list.size()){
            return false;
        }
        m_program_list->open_program(game, program.toStdString());
        return m_current_panel != nullptr;
    };
    actions.close_program = [this]{
        if (m_busy || m_panel_transition){
            return false;
        }
        m_panel_transition = true;
        ScopeExit cleanup([&]{
            m_panel_transition = false;
        });
        close_panel();
        m_program_list->clear_current();
        return true;
    };
    actions.settings = [this]{
        show_settings();
    };
    actions.about = [this]{
        PocketPilot::show_about(this, about_info());
    };
    actions.output = [this]{
        m_output_window->show();
        m_output_window->raise(); // bring the window to front on macOS
        m_output_window->activateWindow(); // bring the window to front on Windows
    };
    actions.buttons = [this]{
        m_button_diagram->show();
        m_button_diagram->raise(); // bring the window to front on macOS
        m_button_diagram->activateWindow(); // bring the window to front on Windows
    };
    actions.claude = [this](bool show){
        show_assistant(show);
    };
    //  The overview of the installations (the screen puts this window away).
    actions.installations = []{
        PocketPilot::show_instances_overview();
    };
    actions.keep_screen_on = [this, test](bool on){
        if (on){
            m_sleep_scope.reset(new SleepSuppressScope(SleepSuppress::SCREEN_ON));
        }else{
            m_sleep_scope.reset();
        }
        //  Kept for the next start. (The test must not change the settings.)
        if (!test){
            GlobalSettings::instance().KEEP_SCREEN_ON = on;
            try{
                PERSISTENT_SETTINGS().write();
            }catch (...){
                global_logger_tagged().log("The switch for the screen could not be written to the settings file.", COLOR_RED);
            }
        }
    };

    m_app = PocketPilot::make_main_view(centralwidget, m_game_list, std::move(programs), std::move(actions), test);
    QVBoxLayout* layout = new QVBoxLayout(centralwidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_app->widget());
    update_resume();
    //  As it was left the last time.
    if (!test && GlobalSettings::instance().KEEP_SCREEN_ON){
        m_sleep_scope.reset(new SleepSuppressScope(SleepSuppress::SCREEN_ON));
        m_app->set_keep_on(true);
    }
    MainWindow::sleep_suppress_state_changed(SystemSleepController::instance().current_state());
}




}


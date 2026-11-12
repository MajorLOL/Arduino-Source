/*  Main Window UI
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <utility>
//#include <QStatusBar>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include "Common/Cpp/ScopeExit.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/MultiOutputLogger.h"
#include "Common/Cpp/Logging/GlobalLogger.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/CpuId/CpuId.h"
#include "Common/Cpp/ColoredText.h"
#include "Common/Qt/UiStateQtWidget.h"
#include "PanelLists.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/Logging/LoggerWindow.h"
#include "CommonFramework/Options/ResolutionOption.h"
#include "CommonFramework/Options/Environment/ThemeSelectorOption.h"
#include "CommonFramework/Options/QtWidget/WallpaperWidget.h"
#include "CommonFramework/Windows/DpiScaler.h"
#include "PocketPilot/PocketPilot_About.h"
#include "PocketPilot/PocketPilot_Home.h"
#include "PocketPilot/PocketPilot_Logo.h"
#include "PocketPilot/PocketPilot_Shell.h"
#include "WindowTracker.h"
#include "ButtonDiagram.h"
#include "MainWindow.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_current_panel_widget(nullptr)
{
    global_panel_holder() = this;

    //  (The theme has been applied before this window was made: Main.cpp.)

    if (objectName().isEmpty()){
        setObjectName(QString::fromUtf8("MainWindow"));
    }
    this->setWindowIcon(PocketPilot::app_icon());
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

    //  The frame: the top bar, the side bar, the room for the page or the
    //  program that is open, and the status strip.
    PocketPilot::TopBar::Actions actions;
    actions.toggle_side_bar = [this]{
        m_frame->set_side_bar_visible(!m_frame->side_bar_visible());
    };
    actions.home = [this]{
        show_games();
    };
    actions.output = [this]{
        m_output_window->show();
        m_output_window->raise(); // bring the window to front on macOS
        m_output_window->activateWindow(); // bring the window to front on Windows
    };
    actions.keyboard = [this]{
        m_button_diagram->show();
        m_button_diagram->raise(); // bring the window to front on macOS
        m_button_diagram->activateWindow(); // bring the window to front on Windows
    };
    actions.settings = [this]{
        show_settings();
    };
    actions.about = [this]{
        PocketPilot::show_about(this, about_info());
    };
    m_frame = std::make_unique<PocketPilot::Frame>(
        *centralwidget,
        QString::fromStdString(PROGRAM_NAME),
        QString::fromStdString(
            PROGRAM_NAME + " build " + PROGRAM_BUILD + ", on " +
            UPSTREAM_PROGRAM_NAME + " " + PROGRAM_VERSION + " (" + PA_ARCH_STRING + ")"
        ),
        std::move(actions)
    );
    m_right_panel_layout = &m_frame->content_layout();

    //  The side bar: the games and the programs of one of them.
    m_program_list = new ProgramSelect(m_frame->side_bar(), *this);
    m_frame->side_layout().addWidget(m_program_list, 1);

    //  The status strip: holding off the system's sleep.
    {
        m_sleep_text = &m_frame->status_strip().sleep_label();
        m_sleep_box = &m_frame->status_strip().sleep_box();
        MainWindow::sleep_suppress_state_changed(SystemSleepController::instance().current_state());
#if QT_VERSION < 0x060700
        connect(
            m_sleep_box, &QCheckBox::stateChanged,
            this, [this](int){
                if (m_sleep_box->isChecked()){
                    m_sleep_scope.reset(new SleepSuppressScope(SleepSuppress::SCREEN_ON));
                }else{
                    m_sleep_scope.reset();
                }
            }
        );
#else
        connect(
            m_sleep_box, &QCheckBox::checkStateChanged,
            this, [this](Qt::CheckState state){
                if (state == Qt::CheckState::Checked){
                    m_sleep_scope.reset(new SleepSuppressScope(SleepSuppress::SCREEN_ON));
                }else{
                    m_sleep_scope.reset();
                }
            }
        );
#endif
    }

    //  The windows behind the top bar's buttons.
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

    //  The page with the games, shown while no program is open.
    {
        std::vector<PocketPilot::HomeGame> games;
        std::vector<PocketPilot::HomeProgram> programs;
        m_program_list->describe(games, programs);
        PocketPilot::HomePage::Actions home_actions;
        home_actions.open_game = [this](int game){
            m_program_list->open_game(game);
            m_frame->set_side_bar_visible(true);
        };
        home_actions.open_program = [this](int game, const QString& program){
            m_program_list->open_program(game, program.toStdString());
        };
        home_actions.resume = [this]{
            m_program_list->load_persistent_panel();
        };
        m_home = new PocketPilot::HomePage(
            m_frame->content(), std::move(games), std::move(programs), std::move(home_actions)
        );
        m_right_panel_layout->addWidget(m_home);
    }

    // Load the program panel specified in the persistent setting.
    m_program_list->load_persistent_panel();
    update_page();

    GlobalSettings::instance().WINDOW_SIZE->WIDTH.add_listener(*this);
    GlobalSettings::instance().WINDOW_SIZE->HEIGHT.add_listener(*this);
    GlobalSettings::instance().WINDOW_SIZE->X_POS.add_listener(*this);
    GlobalSettings::instance().WINDOW_SIZE->Y_POS.add_listener(*this);    
    GlobalSettings::instance().THEME->add_listener(*this);
    GlobalSettings::instance().WALLPAPER.add_listener(*this);
    SystemSleepController::instance().add_listener(*this);
    update_wallpaper();
//    cout << "Done constructing" << endl;
}
MainWindow::~MainWindow(){
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
    m_home->show_games();
    update_page();
    m_home->focus_search();
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

//  The page with the games while no program is open, and where the user
//  is in the top bar.
void MainWindow::update_page(){
    const bool open = m_current_panel_widget != nullptr;
    m_home->setVisible(!open);
    if (!open){
        std::string game, program;
        m_program_list->persistent_panel(game, program);
        m_home->set_resume(QString::fromStdString(game), QString::fromStdString(program));
        m_frame->top_bar().set_location(QString(), QString());
        return;
    }
    const PanelDescriptor& descriptor = m_current_panel->descriptor();
    if (&descriptor == &GlobalSettings_Descriptor::instance()){
        m_frame->top_bar().set_location(QString(), "Settings");
    }else{
        m_frame->top_bar().set_location(
            PocketPilot::short_game_name(QString::fromStdString(m_program_list->current_game())),
            QString::fromStdString(descriptor.display_name())
        );
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

        m_right_panel_layout->removeWidget(m_current_panel_widget);
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
        //  The page with the games makes room first.
        m_home->hide();
        m_right_panel_layout->addWidget(m_current_panel_widget);
        update_page();
        return;
    }catch (Exception& e){
        e.log(global_logger_tagged());
    }catch (std::exception& e){
        global_logger_tagged().log(std::string("MainWindow::load_panel() - Exception: ") + e.what(), COLOR_RED);
    }catch (...){
        global_logger_tagged().log("MainWindow::load_panel() - Unknown Exception", COLOR_RED);
    }
    if (m_current_panel_widget != nullptr){
        delete m_current_panel_widget;
        m_current_panel_widget = nullptr;
    }
    update_page();
}
void MainWindow::on_busy(){
    m_busy = true;
    if (m_program_list){
        m_program_list->lock();
        m_frame->top_bar().set_busy(true);
        std::string text = "Running";
        if (m_current_panel != nullptr){
            text += " " + m_current_panel->descriptor().display_name();
        }
        m_frame->status_strip().set_state(
            PocketPilot::StatusStrip::State::RUNNING, QString::fromStdString(text)
        );
    }
}
void MainWindow::on_idle(){
    m_busy = false;
    if (m_program_list){
        m_program_list->unlock();
        m_frame->top_bar().set_busy(false);
        m_frame->status_strip().set_state(PocketPilot::StatusStrip::State::READY, "Ready");
    }
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
        // (The side bar is not one of them: it is part of the frame.)
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
            m_sleep_text->setText("The computer may sleep");
//            m_sleep_box->setEnabled();
//            m_sleep_box->setCheckState(Qt::CheckState::Unchecked);
            break;
        case PokemonAutomation::SleepSuppress::NO_SLEEP:
            m_sleep_text->setText("The computer stays awake");
            break;
        case PokemonAutomation::SleepSuppress::SCREEN_ON:
            m_sleep_text->setText("The screen stays on");
            break;
        }
    });
}




}


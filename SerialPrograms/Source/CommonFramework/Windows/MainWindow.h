/*  Main Window UI
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_MainWindow_H
#define PokemonAutomation_MainWindow_H

#include <memory>
#include <string>
#include <vector>
#include <QMainWindow>
#include "Common/Cpp/Options/ConfigOption.h"
#include "PanelLists.h"
#include "CommonFramework/Environment/SystemSleep.h"
#include "CommonFramework/Panels/PanelTools.h"
#include "CommonFramework/Panels/PanelSession.h"

class QVBoxLayout;

namespace PokemonAutomation{

class ButtonDiagram;
class LoggerWindow;
class WallpaperWidget;
namespace PocketPilot{
    class AppView;
    struct AboutInfo;
    namespace Assistant{
        class HttpTransport;
        class Panel;
        class Service;
    }
}


class MainWindow
    : public QMainWindow
    , public PanelHolder
    , public ConfigOption::Listener
    , public SystemSleepController::Listener
{
public:
    //  The window's screen is the one of the setup: the games, the programs,
    //  the program itself (see PocketPilot_Setup.h).
    //  "app_test": the screens have no clock of their own and nothing opens
    //  outside the window (the test of the interface drives them).
    explicit MainWindow(QWidget* parent = nullptr, bool app_test = false);
    ~MainWindow();

    //  The screens.
    PocketPilot::AppView* app_view(){ return m_app; }

    //  What the buttons along the top do. Also for the test of the interface
    //  (PocketPilot_SelfTest.cpp).
    void show_games();
    void show_settings();
    //  Opens a program by the name of its game and its own name. False if
    //  a program runs or there is no such game.
    bool open_program(const std::string& game, const std::string& program);
    //  The game as a click on its cartridge shows it, and then one of its
    //  programs (empty: none).
    bool open_game(const QString& game, const QString& program);
    //  Shows a game by its name in the list of games, as a click on its
    //  cartridge does. False if a program runs or there is no such game.
    bool open_game(const QString& game);

    PocketPilot::AboutInfo about_info() const;
    LoggerWindow& output_window(){ return *m_output_window; }

    //  The panel with Claude at the right: shown or hidden.
    void show_assistant(bool show);
    PocketPilot::Assistant::Service& assistant(){ return *m_assistant; }
    PocketPilot::Assistant::Panel& assistant_panel(){ return *m_assistant_panel; }
    //  Makes the assistant anew with another connection than the network
    //  and its files in another folder: for the test of the interface,
    //  which answers for Claude and GitHub itself and has no keys.
    void remake_assistant(
        std::unique_ptr<PocketPilot::Assistant::HttpTransport> transport,
        const std::string& settings_folder
    );


private:
    virtual void changeEvent(QEvent* event) override;
    virtual void closeEvent(QCloseEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void moveEvent(QMoveEvent* event) override;

    void close_panel() noexcept;

    // implements PanelHolder::report_new_panel_intent()
    virtual bool report_new_panel_intent(const PanelDescriptor& descriptor) override;
    // implements PanelHolder::load_panel()
    virtual void load_panel(
        std::shared_ptr<const PanelDescriptor> descriptor,
        std::unique_ptr<PanelSession> panel
    ) override;

private:
    // implements PanelHolder::on_busy()
    // called when an automation program is running
    virtual void on_busy() override;
    // implements PanelHolder::on_idle()
    // called when no automation program is not running
    virtual void on_idle() override;
    virtual void on_config_value_changed(void* object) override;
    virtual void sleep_suppress_state_changed(SleepSuppress new_state) override;

    void update_wallpaper();
    //  The screens, with what the buttons along the top do.
    void make_app_view(bool test);
    //  The program of the last time, for "Continue with ..." on the games.
    void update_resume();
    void set_sleep_label(const QString& text);
    void make_assistant(
        std::unique_ptr<PocketPilot::Assistant::HttpTransport> transport,
        const std::string& settings_folder
    );

private:
    WallpaperWidget* centralwidget = nullptr;
//    QStatusBar* statusbar;

    //  The window's own screen (a child of the central widget), and the games
    //  it shows (the same order as the list of programs has).
    PocketPilot::AppView* m_app = nullptr;
    std::vector<PocketPilot::HomeGame> m_game_list;
    //  Claude: what it needs to work, and its panel at the right.
    std::unique_ptr<PocketPilot::Assistant::Service> m_assistant;
    PocketPilot::Assistant::Panel* m_assistant_panel = nullptr;

    //  The programs and the panel of each, kept out of sight: the screens show
    //  them.
    ProgramSelect* m_program_list = nullptr;

    //  Keep a reference to the panel descriptor since it is referenced by the
    //  panel instance and the original descriptor may destroyed at any time.
    std::shared_ptr<const PanelDescriptor> m_current_panel_descriptor;
    std::unique_ptr<PanelSession> m_current_panel;
    QWidget* m_current_panel_widget;

    std::unique_ptr<ButtonDiagram> m_button_diagram;
    std::unique_ptr<LoggerWindow> m_output_window;

    std::unique_ptr<SleepSuppressScope> m_sleep_scope;

    bool m_pending_resize = false;
    bool m_pending_move = false;
    bool m_panel_transition = false;
    //  A program runs.
    bool m_busy = false;
};

// returns given X value, but bounded by 0 and screen_width
int32_t move_x_within_screen_bounds(int32_t x_pos);
// returns given y value, but bounded by 0 and screen_height
int32_t move_y_within_screen_bounds(int32_t y_pos);


}
#endif

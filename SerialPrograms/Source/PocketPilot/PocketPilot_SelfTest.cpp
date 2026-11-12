/*  PocketPilot Self Test
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <memory>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QTimer>
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/Logging/LoggerWindow.h"
#include "CommonFramework/Options/Environment/ThemeSelectorOption.h"
#include "CommonFramework/Windows/MainWindow.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PocketPilot_About.h"
#include "PocketPilot_Home.h"
#include "PocketPilot_Theme.h"
#include "PocketPilot_SelfTest.h"

namespace PokemonAutomation{
namespace PocketPilot{


const char* const UI_SCREENSHOTS_OPTION = "--ui-screenshots";


namespace{

//  Lets the program go on (drawing, laying out) for a while.
void let_run(int milliseconds){
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

}


int run_ui_screenshots(QApplication& application, MainWindow& window, const std::string& folder){
    Logger& logger = global_logger_tagged();
    const QString out = QString::fromStdString(folder);
    QDir().mkpath(out);
    logger.log("UI test: pictures of the interface go to " + QDir(out).absolutePath().toStdString(), COLOR_BLUE);

    int number = 0;
    int failed = 0;
    auto save = [&](QWidget& widget, const char* name){
        let_run(700);
        number++;
        const QString path = out + "/" + QString::number(number).rightJustified(2, '0') + "-" + name + ".png";
        if (widget.grab().save(path)){
            logger.log("UI test: saved " + path.toStdString());
        }else{
            failed++;
            logger.log("UI test: could not save " + path.toStdString(), COLOR_RED);
        }
    };
    auto set_look = [&](UiThemeMode mode, Starter starter){
        GlobalSettings::instance().THEME->set(mode);
        GlobalSettings::instance().STARTER_COLOR->set(starter);
        let_run(300);
    };
    const std::string GAME = Pokemon::STRING_POKEMON + " FireRed and LeafGreen";
    const std::string PROGRAM = "AutoStory";
    auto open_program = [&]{
        if (!window.open_program(GAME, PROGRAM)){
            failed++;
            logger.log("UI test: could not open " + GAME + ", " + PROGRAM, COLOR_RED);
        }
    };

    window.resize(1360, 900);
    let_run(500);

    //  Night in the color of Fire, the look of a new installation: every
    //  page.
    set_look(UiThemeMode::POCKET_NIGHT, Starter::FIRE);
    window.show_games();
    save(window, "night-fire-games");
    window.home_page().show_game(QString::fromStdString(GAME));
    save(window, "night-fire-one-game");
    window.home_page().set_search_text("shiny");
    save(window, "night-fire-search");
    window.home_page().set_search_text("");
    open_program();
    save(window, "night-fire-program");
    window.show_settings();
    save(window, "night-fire-settings");
    //  The other windows, drawn without being put on the screen, like the
    //  main window in this test (see Main.cpp).
    {
        std::unique_ptr<QDialog> about = make_about(&window, window.about_info());
        about->setAttribute(Qt::WA_DontShowOnScreen, true);
        about->show();
        save(*about, "night-fire-about");
        about->close();
    }
    {
        LoggerWindow& output = window.output_window();
        if (!output.isVisible()){
            output.setAttribute(Qt::WA_DontShowOnScreen, true);
            output.show();
            save(output, "night-fire-output-log");
            output.hide();
            output.setAttribute(Qt::WA_DontShowOnScreen, false);
        }else{
            save(output, "night-fire-output-log");
        }
    }

    //  The other looks: the games and a program.
    struct Look{
        const char* games;
        const char* program;
        UiThemeMode mode;
        Starter starter;
    };
    const Look LOOKS[] = {
        {"day-water-games",     "day-water-program",        UiThemeMode::POCKET_DAY,    Starter::WATER},
        {"night-grass-games",   "night-grass-program",      UiThemeMode::POCKET_NIGHT,  Starter::GRASS},
        {"day-fire-games",      "day-fire-program",         UiThemeMode::POCKET_DAY,    Starter::FIRE},
        {"classic-light-games", "classic-light-program",    UiThemeMode::DEFAULT_MODE,  Starter::FIRE},
        {"classic-dark-games",  "classic-dark-program",     UiThemeMode::DARK_MODE,     Starter::FIRE},
    };
    for (const Look& look : LOOKS){
        set_look(look.mode, look.starter);
        window.show_games();
        save(window, look.games);
        open_program();
        save(window, look.program);
    }

    //  Back to the first look. (Nothing of this is saved: the program
    //  exits without writing its settings.)
    set_look(UiThemeMode::POCKET_NIGHT, Starter::FIRE);
    window.show_games();
    let_run(300);

    logger.log(
        "UI test: " + std::to_string(number - failed) + " of " + std::to_string(number) + " pictures saved.",
        failed == 0 ? COLOR_BLUE : COLOR_RED
    );
    application.processEvents();
    return failed == 0 ? 0 : 1;
}



}
}

/*  PocketPilot Self Test
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <algorithm>
#include <memory>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include "Common/Cpp/Exceptions.h"
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


namespace{

//  Takes the pictures. Returns how many of them could not be taken. What it
//  is about to do goes to the log first, so that the log of a test that
//  stopped halfway says where.
int take_pictures(QApplication& application, MainWindow& window, const QString& out, Logger& logger){
    int number = 0;
    int failed = 0;
    auto save_picture = [&](const QPixmap& picture, const char* name){
        number++;
        const QString path = out + "/" + QString::number(number).rightJustified(2, '0') + "-" + name + ".png";
        if (picture.save(path)){
            logger.log("UI test: saved " + path.toStdString());
        }else{
            failed++;
            logger.log("UI test: could not save " + path.toStdString(), COLOR_RED);
        }
    };
    auto save = [&](QWidget& widget, const char* name){
        let_run(700);
        save_picture(widget.grab(), name);
    };
    auto set_look = [&](UiThemeMode mode, Starter starter){
        logger.log(
            "UI test: theme " + std::to_string((int)mode) + " in starter color " + std::to_string((int)starter)
        );
        GlobalSettings::instance().THEME->set(mode);
        GlobalSettings::instance().STARTER_COLOR->set(starter);
        let_run(300);
    };
    const std::string GAME = Pokemon::STRING_POKEMON + " FireRed and LeafGreen";
    const std::string PROGRAM = "AutoStory";
    auto open_program = [&]{
        logger.log("UI test: opening " + GAME + ", " + PROGRAM);
        if (!window.open_program(GAME, PROGRAM)){
            failed++;
            logger.log("UI test: could not open " + GAME + ", " + PROGRAM, COLOR_RED);
        }
    };
    //  The list of a dropdown, opened and drawn without being put on the
    //  screen.
    auto grab_list = [&](QComboBox& box){
        logger.log("UI test: opening a list of " + std::to_string(box.count()) + " rows");
        QWidget* list = box.view()->window();
        list->setAttribute(Qt::WA_DontShowOnScreen, true);
        box.showPopup();
        let_run(400);
        QPixmap picture = list->grab();
        picture.setDevicePixelRatio(1);
        box.hidePopup();
        let_run(100);
        list->setAttribute(Qt::WA_DontShowOnScreen, false);
        return picture;
    };
    //  One picture of two opened lists: the games of the side bar, and the
    //  longest list among the options of the open program.
    auto save_lists = [&](const char* name){
        QComboBox* games = window.findChild<QComboBox*>("ppGamePicker");
        QComboBox* option = nullptr;
        for (QComboBox* box : window.findChildren<QComboBox*>()){
            if (box == games || !box->isVisible() || !box->isEnabled() || box->isEditable()){
                continue;
            }
            if (option == nullptr || box->count() > option->count()){
                option = box;
            }
        }
        if (games == nullptr || option == nullptr){
            failed++;
            logger.log(std::string("UI test: no dropdowns found for ") + name, COLOR_RED);
            return;
        }
        const QPixmap left = grab_list(*games);
        const QPixmap right = grab_list(*option);
        const int MARGIN = 16;
        QPixmap sheet(
            left.width() + right.width() + 3 * MARGIN,
            std::max(left.height(), right.height()) + 2 * MARGIN
        );
        sheet.fill(theme_colors().screen);
        {
            QPainter painter(&sheet);
            painter.drawPixmap(MARGIN, MARGIN, left);
            painter.drawPixmap(left.width() + 2 * MARGIN, MARGIN, right);
        }
        save_picture(sheet, name);
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
    logger.log("UI test: opening the settings");
    window.show_settings();
    save(window, "night-fire-settings");
    //  The other windows, drawn without being put on the screen, like the
    //  main window in this test (see Main.cpp).
    {
        logger.log("UI test: opening the About window");
        std::unique_ptr<QDialog> about = make_about(&window, window.about_info());
        about->setAttribute(Qt::WA_DontShowOnScreen, true);
        about->show();
        save(*about, "night-fire-about");
        about->close();
    }
    {
        logger.log("UI test: opening the output window");
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

    //  The opened lists of two dropdowns. Last, so that the pictures above
    //  are there whatever happens here.
    struct ListLook{
        const char* name;
        UiThemeMode mode;
        Starter starter;
    };
    const ListLook LIST_LOOKS[] = {
        {"classic-dark-lists",  UiThemeMode::DARK_MODE,     Starter::FIRE},
        {"classic-light-lists", UiThemeMode::DEFAULT_MODE,  Starter::FIRE},
        {"day-water-lists",     UiThemeMode::POCKET_DAY,    Starter::WATER},
        {"night-fire-lists",    UiThemeMode::POCKET_NIGHT,  Starter::FIRE},
    };
    for (const ListLook& look : LIST_LOOKS){
        set_look(look.mode, look.starter);
        open_program();
        let_run(500);
        save_lists(look.name);
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
    return failed;
}

}


int run_ui_screenshots(QApplication& application, MainWindow& window, const std::string& folder){
    Logger& logger = global_logger_tagged();
    const QString out = QString::fromStdString(folder);
    QDir().mkpath(out);
    logger.log("UI test: pictures of the interface go to " + QDir(out).absolutePath().toStdString(), COLOR_BLUE);
    //  An exception must not leave the program: that looks like a crash,
    //  while its message and the pictures taken so far say what went wrong.
    try{
        return take_pictures(application, window, out, logger) == 0 ? 0 : 1;
    }catch (const Exception& e){
        logger.log("UI test: stopped by an exception: " + e.to_str(), COLOR_RED);
    }catch (const std::exception& e){
        logger.log(std::string("UI test: stopped by an exception: ") + e.what(), COLOR_RED);
    }
    return 1;
}



}
}

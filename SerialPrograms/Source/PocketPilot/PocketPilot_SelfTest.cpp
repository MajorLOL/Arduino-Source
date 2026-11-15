/*  PocketPilot Self Test
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <algorithm>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTimer>
#include <QToolTip>
#include <QWheelEvent>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/AudioPipeline/AudioOption.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/Logging/LoggerWindow.h"
#include "CommonFramework/Options/Environment/ThemeSelectorOption.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "CommonFramework/Windows/MainWindow.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBump.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCollectionText.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryData.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryFerryTalk.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryNameMark.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPlaceList.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySmallRoom.h"
#include "VideoGames/PokemonFRLG/Programs/ClaudePlays/PokemonFRLG_ClaudePlays.h"
#include "Assistant/Assistant_Panel.h"
#include "Assistant/Assistant_Play.h"
#include "Assistant/Assistant_SelfTest.h"
#include "Assistant/Assistant_Service.h"
#include "ClaudePlays/ClaudePlays_AnyGame.h"
#include "ClaudePlays/ClaudePlays_Engine.h"
#include "ClaudePlays/ClaudePlays_Games.h"
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
    //  The window much taller than a screen, so that the options of the
    //  open program are in the picture too. (The window is not on the
    //  screen in this test: it can have any size.)
    const int PICTURE_WIDTH = 1360;
    const int PICTURE_HEIGHT = 900;
    const int PICTURE_TALL = 2600;
    auto save_tall = [&](const char* name){
        window.resize(PICTURE_WIDTH, PICTURE_TALL);
        save(window, name);
        window.resize(PICTURE_WIDTH, PICTURE_HEIGHT);
        let_run(300);
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
    auto open_named = [&](const std::string& program){
        logger.log("UI test: opening " + GAME + ", " + program);
        if (!window.open_program(GAME, program)){
            failed++;
            logger.log("UI test: could not open " + GAME + ", " + program, COLOR_RED);
        }
    };
    auto open_program = [&]{
        open_named(PROGRAM);
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

    window.resize(PICTURE_WIDTH, PICTURE_HEIGHT);
    let_run(500);

    //  Night in the color of Fire, the look of a new installation: every
    //  page.
    set_look(UiThemeMode::POCKET_NIGHT, Starter::FIRE);
    window.show_games();
    save(window, "night-fire-games");
    //  As a click on the game's cartridge: its programs on the page and
    //  its list in the side bar.
    if (!window.home_page().pick_game(QString::fromStdString(GAME))){
        failed++;
        logger.log("UI test: no game named " + GAME, COLOR_RED);
    }
    save(window, "night-fire-one-game");
    window.home_page().set_search_text("shiny");
    save(window, "night-fire-search");
    window.home_page().set_search_text("");
    open_program();
    save(window, "night-fire-program");
    save_tall("night-fire-program-tall");
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
        //  Also a tall picture of the program, with its options. Null: none.
        const char* program_tall;
        UiThemeMode mode;
        Starter starter;
    };
    const Look LOOKS[] = {
        {"day-water-games",     "day-water-program",        "day-water-program-tall",   UiThemeMode::POCKET_DAY,    Starter::WATER},
        {"night-grass-games",   "night-grass-program",      nullptr,                    UiThemeMode::POCKET_NIGHT,  Starter::GRASS},
        {"day-fire-games",      "day-fire-program",         nullptr,                    UiThemeMode::POCKET_DAY,    Starter::FIRE},
        {"classic-light-games", "classic-light-program",    nullptr,                    UiThemeMode::DEFAULT_MODE,  Starter::FIRE},
        {"classic-dark-games",  "classic-dark-program",     nullptr,                    UiThemeMode::DARK_MODE,     Starter::FIRE},
    };
    for (const Look& look : LOOKS){
        set_look(look.mode, look.starter);
        window.show_games();
        save(window, look.games);
        open_program();
        save(window, look.program);
        if (look.program_tall != nullptr){
            save_tall(look.program_tall);
        }
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

    //  An experimental program: its tag in the list, the note in its
    //  header, its options.
    set_look(UiThemeMode::POCKET_NIGHT, Starter::FIRE);
    open_named("Level Trainer");
    let_run(500);
    save(window, "night-fire-level-trainer");
    save_tall("night-fire-level-trainer-tall");
    open_named("Box Namer");
    let_run(500);
    save(window, "night-fire-box-namer");
    save_tall("night-fire-box-namer-tall");
    open_named("Claude Plays");
    let_run(500);
    save(window, "night-fire-claude-plays");
    save_tall("night-fire-claude-plays-tall");
    //  The Claude Plays of every other list, here of "Other Games".
    logger.log(std::string("UI test: opening ") + ClaudePlays::OTHER_GAMES_LIST + ", Claude Plays");
    if (!window.open_program(ClaudePlays::OTHER_GAMES_LIST, "Claude Plays")){
        failed++;
        logger.log(std::string("UI test: could not open ") + ClaudePlays::OTHER_GAMES_LIST + ", Claude Plays", COLOR_RED);
    }
    let_run(500);
    save(window, "night-fire-claude-plays-any-game");
    save_tall("night-fire-claude-plays-any-game-tall");

    //  Claude: its panel next to a program. The test answers for Claude
    //  and for GitHub itself (Assistant/Assistant_SelfTest.h): the build
    //  machine has no key and uses no network. Everything in front of
    //  those answers is the real thing, so the conversation here also
    //  shows that a change ends up on a branch of its own.
    {
        logger.log("UI test: the panel with Claude, with the test answering for Claude and GitHub");
        Assistant::TestBackend backend;
        const QString folder = out + "/assistant-test";
        QDir().mkpath(folder);
        window.remake_assistant(backend.make_transport(), (folder + "/").toStdString());
        open_program();
        window.show_assistant(true);
        save(window, "night-fire-claude-connect");

        Assistant::Service& service = window.assistant();
        Assistant::Panel& panel = window.assistant_panel();
        QString error;
        service.secrets().set(Assistant::Secret::CLAUDE_API_KEY, "a-key-made-up-for-the-test", error);
        service.secrets().set(Assistant::Secret::GITHUB_TOKEN, "a-token-made-up-for-the-test", error);
        panel.show_start_page();
        save(window, "night-fire-claude-empty");

        auto wait_until = [&](const std::function<bool()>& done, const char* what){
            for (int waited = 0; waited < 20000 && !done(); waited += 50){
                let_run(50);
            }
            if (!done()){
                failed++;
                logger.log(std::string("UI test: Claude's panel did not get to ") + what, COLOR_RED);
                return false;
            }
            return true;
        };
        auto find_button = [&](const QString& text){
            QPushButton* found = nullptr;
            for (QPushButton* button : panel.findChildren<QPushButton*>()){
                if (button->text() == text && button->isVisible()){
                    found = button;
                }
            }
            return found;
        };

        //  A question about a run: Claude looks in the log and the source.
        const QString source = QString::fromLatin1(Assistant::TestBackend::SOURCE_FILE);
        backend.claude_calls("read_log", QJsonObject{{"lines", 120}});
        backend.claude_calls("find_files", QJsonObject{{"words", "frlg autostory.cpp"}});
        backend.claude_calls("read_file", QJsonObject{{"path", source}, {"first_line", 40}, {"last_line", 46}});
        backend.claude_says(
            "The run stopped in `C13_02_CHAMPION`: the League was lost **seven times in a row**, and then "
            "the program stops by itself.\n\n"
            "## Where that is\n"
            "`PokemonFRLG_AutoStory.cpp`, line 43:\n\n"
            "```cpp\nif (league_again_losses > 6){\n    throw UserSetupError(env.logger(), \"The League was "
            "lost seven times in a row.\");\n}\n```\n\n"
            "What you can do:\n"
            "- Raise **Extra levels** in the options, so that the team trains further first.\n"
            "- Have the limit changed: say so and I make the change on a branch of its own."
        );
        panel.send("Why did the last run stop?");
        wait_until([&]{ return !service.agent().busy(); }, "the end of an answer");
        save(window, "night-fire-claude-answer");

        //  A change: made, and committed once the user says so.
        backend.claude_calls(
            "edit_file",
            QJsonObject{{"path", source}, {"old_text", "league_again_losses > 6"}, {"new_text", "league_again_losses > 9"}},
            "I will set the limit to ten losses in `PokemonFRLG_AutoStory.cpp`."
        );
        backend.claude_calls(
            "commit_changes",
            QJsonObject{
                {"topic", "League loss limit"},
                {"message", "FRLG AutoStory: stop after ten League losses in a row\n\nThe limit was seven."},
            }
        );
        backend.claude_says(
            "Committed to `claude/league-loss-limit`. Open the pull request with the button above: its build "
            "takes about 7 minutes. Tell me when it is done and I will look whether it passed."
        );
        panel.send("Make it ten losses.");
        if (wait_until([&]{ return find_button("Commit") != nullptr; }, "the question whether to commit")){
            save(window, "night-fire-claude-commit-question");
            find_button("Commit")->click();
        }
        wait_until([&]{ return !service.agent().busy(); }, "the end of the commit");
        save(window, "night-fire-claude-committed");

        const QString branch = "claude/league-loss-limit";
        const bool committed =
            backend.has_branch(branch) &&
            backend.file_on(branch, source).contains("league_again_losses > 9){\r\n") &&
            backend.file_on(Assistant::TestBackend::BASE_BRANCH, source).contains("league_again_losses > 6){\r\n") &&
            backend.requests_to_claude() == 7 &&
            service.workspace().branch() == branch &&
            service.workspace().changes().empty();
        if (committed){
            logger.log(
                "Assistant test: the conversation ended in a commit on a branch of its own, with the base "
                "branch as it was.",
                COLOR_BLUE
            );
        }else{
            failed++;
            logger.log("Assistant test: the conversation did not end in the commit it should have made.", COLOR_RED);
        }

        //  A program in which Claude plays (Assistant/Assistant_Play.h): it
        //  talks to Claude from a thread of its own, through this same
        //  connection. Here the "program" is a task that answers a look and
        //  a press with made-up results.
        {
            backend.claude_calls("look", QJsonObject(), "I look at the screen first.");
            backend.claude_calls("press", QJsonObject{{"buttons", QJsonArray{"A"}}});
            backend.claude_says("Nothing more to do here.");
            const int requests_before = backend.requests_to_claude();
            struct Played{
                std::string unavailable = "?";
                std::vector<Assistant::PlayEvent> events;
                Assistant::PlayUsage usage;
                size_t picture_bytes = 0;
            };
            Played played;
            CancellableHolder<CancellableScope> scope;
            AsyncTask task = GlobalThreadPools::unlimited_normal().dispatch_now_blocking([&played, &scope]{
                try{
                    played.unavailable = Assistant::PlaySession::why_unavailable(scope);
                    Assistant::PlaySession::Setup setup;
                    setup.model = "claude-haiku-5-5";
                    setup.system = NintendoSwitch::PokemonFRLG::claude_plays_instructions(false);
                    setup.tools = NintendoSwitch::PokemonFRLG::claude_plays_tools(false);
                    Assistant::PlaySession session(std::move(setup));
                    session.start("Goal: a test of the interface.");
                    for (int round = 0; round < 3; round++){
                        const Assistant::PlayEvent event = session.wait(scope);
                        played.events.push_back(event);
                        if (event.kind != Assistant::PlayEvent::Kind::CALL){
                            break;
                        }
                        Assistant::PlayResult result;
                        result.text = "Screen: made up for the test.";
                        if (event.call.name == "look"){
                            ImageRGB32 frame(1280, 720);
                            frame.fill(0xff204060);
                            result.image = Assistant::play_picture(frame, 240, 160, 2);
                            played.picture_bytes = result.image.size();
                        }
                        session.finish(event.call, std::move(result));
                    }
                    played.usage = session.usage();
                }catch (...){
                    //  Stopped by the scope: what was seen so far tells.
                }
            });
            for (int waited = 0; waited < 20000 && !task.is_finished(); waited += 20){
                let_run(20);
            }
            if (!task.is_finished()){
                scope.cancel(nullptr);
            }
            task.wait_and_ignore_exceptions();
            //  What the session posted on its way out.
            let_run(100);
            const bool played_well =
                played.unavailable.empty() &&
                played.events.size() == 3 &&
                played.events[0].kind == Assistant::PlayEvent::Kind::CALL &&
                played.events[0].call.name == "look" &&
                played.events[0].said == "I look at the screen first." &&
                played.events[1].kind == Assistant::PlayEvent::Kind::CALL &&
                played.events[1].call.name == "press" &&
                played.events[1].call.input == "{\"buttons\":[\"A\"]}" &&
                played.events[2].kind == Assistant::PlayEvent::Kind::ANSWERED &&
                played.picture_bytes > 100 &&
                played.usage.requests == 3 &&
                backend.requests_to_claude() == requests_before + 3;
            if (played_well){
                logger.log(
                    "Assistant test: a program's thread played three turns with Claude (a look with its "
                    "picture, a press, the end).",
                    COLOR_BLUE
                );
            }else{
                failed++;
                logger.log(
                    "Assistant test: a program's thread did not play as it should have: " +
                    std::to_string(played.events.size()) + " events, " + std::to_string(played.usage.requests) +
                    " requests, a picture of " + std::to_string(played.picture_bytes) + " bytes. " + played.unavailable,
                    COLOR_RED
                );
            }
        }

        //  The Claude Plays of every game (ClaudePlays/ClaudePlays_AnyGame.h):
        //  a whole session through that same connection, with a console that
        //  is made up here. Its picture changes with every press, so what
        //  the program says about the screen is part of the test.
        {
            backend.claude_calls("look", QJsonObject(), "I look at the screen first.");
            backend.claude_calls("press", QJsonObject{{"buttons", QJsonArray{"A", "DOWN"}}, {"read", "bottom"}});
            backend.claude_calls("hold", QJsonObject{{"left_stick", "up"}, {"buttons", QJsonArray{"B"}}, {"ms", 400}});
            backend.claude_calls("note", QJsonObject{{"text", "The menu is open."}});
            backend.claude_calls("done", QJsonObject{{"reached", true}, {"summary", "Looked, pressed and walked."}});
            const int requests_before = backend.requests_to_claude();

            struct MadeUpConsole : public ClaudePlays::PlayConsole{
                std::vector<std::string> did;
                int presses = 0;
                size_t read_width = 0;
                size_t read_height = 0;
                virtual const ClaudePlays::PadLayout& layout() const override{
                    return ClaudePlays::pro_controller_layout();
                }
                virtual void hold(const ClaudePlays::PadInput& input, std::chrono::milliseconds duration) override{
                    std::string line = "hold";
                    for (const std::string& button : input.buttons){
                        line += " " + button;
                    }
                    if (input.stick_y > 0.9){
                        line += " stick-up";
                    }
                    did.push_back(line + " " + std::to_string(duration.count()));
                    presses++;
                }
                virtual void mash(const std::string& button, std::chrono::milliseconds) override{
                    did.push_back("mash " + button);
                }
                virtual void wait(std::chrono::milliseconds) override{}
                virtual bool screen(const std::function<void(const ImageViewRGB32&)>& use) override{
                    //  A lighter band of 80 lines that moves down with every
                    //  press: a ninth of the picture where it was, and a ninth
                    //  where it is now.
                    std::vector<uint32_t> pixels((size_t)1280 * 720, 0xff204060);
                    const size_t top = (size_t)(presses % 6) * 80;
                    std::fill(pixels.begin() + top * 1280, pixels.begin() + (top + 80) * 1280, 0xffd0d8e8);
                    use(ImageViewRGB32(pixels.data(), 1280 * sizeof(uint32_t), 1280, 720));
                    return true;
                }
                virtual std::string read_text(const ImageViewRGB32& picture) override{
                    read_width = picture.width();
                    read_height = picture.height();
                    return "Start Game\n";
                }
            };
            struct TestHost : public ClaudePlays::Host{
                CancellableHolder<CancellableScope> cancel;
                std::vector<std::string> lines;
                uint64_t actions = 0;
                virtual void log(const std::string& text, Color) override{ lines.push_back(text); }
                virtual void show(const std::string&, Color) override{}
                virtual void count(uint64_t done, const Assistant::PlayUsage&) override{ actions = done; }
                virtual CancellableScope& scope() override{ return cancel; }
            };
            MadeUpConsole console;
            TestHost host;
            ClaudePlays::Ending ending;
            std::vector<std::string> results;
            bool jpeg = false;
            AsyncTask task = GlobalThreadPools::unlimited_normal().dispatch_now_blocking([&]{
                try{
                    ClaudePlays::GameFacts facts;
                    facts.name = "a game made up for the test";
                    //  The game, with what it hands back kept for the checks.
                    struct Watched : public ClaudePlays::AnyGame{
                        using ClaudePlays::AnyGame::AnyGame;
                        std::vector<std::string>* results = nullptr;
                        bool* jpeg = nullptr;
                        virtual Assistant::PlayResult run(const Assistant::PlayCall& call) override{
                            Assistant::PlayResult result = ClaudePlays::AnyGame::run(call);
                            results->push_back(result.text);
                            if (result.image.size() > 3 &&
                                (unsigned char)result.image[0] == 0xff && (unsigned char)result.image[1] == 0xd8
                            ){
                                *jpeg = true;
                            }
                            return result;
                        }
                    };
                    Watched game(console, facts, ClaudePlays::AnyGameOptions());
                    game.results = &results;
                    game.jpeg = &jpeg;
                    Assistant::PlaySession session(ClaudePlays::make_setup(game, ClaudePlays::Model::HAIKU));
                    ending = ClaudePlays::play(host, game, session, "A test of the interface.", ClaudePlays::Limits());
                }catch (...){
                    //  Stopped by the scope: what was done so far tells.
                }
            });
            for (int waited = 0; waited < 30000 && !task.is_finished(); waited += 20){
                let_run(20);
            }
            if (!task.is_finished()){
                host.cancel.cancel(nullptr);
            }
            task.wait_and_ignore_exceptions();
            let_run(100);
            const bool played_well =
                ending.reached && !ending.failed && ending.actions == 3 && host.actions == 3 &&
                backend.requests_to_claude() == requests_before + 5 &&
                console.did == std::vector<std::string>{"hold A 150", "hold DOWN 110", "hold B stick-up 400"} &&
                results.size() == 3 &&
                results[0] == "Screen: still now. [A picture of the screen.]" &&
                jpeg &&
                results[1] ==
                    "Pressed A, DOWN. Screen: 22% of the picture differs from before the action; still now.\n"
                    "Text in its bottom part, as the program reads it:\nStart Game" &&
                console.read_width == 1280 && console.read_height == 360 &&
                results[2].rfind("Held left stick up + B for 400 ms. Screen: 22% of the picture differs", 0) == 0;
            if (played_well){
                logger.log(
                    "Assistant test: Claude played a session of any game on a made-up console (a look as a "
                    "JPEG, presses with the text read after them, a stick held, a note, done).",
                    COLOR_BLUE
                );
            }else{
                failed++;
                logger.log(
                    "Assistant test: the session of any game did not go as it should have: " + ending.summary +
                    " | " + std::to_string(console.did.size()) + " inputs, " + std::to_string(results.size()) +
                    " results" + (jpeg ? "" : ", no JPEG"),
                    COLOR_RED
                );
                for (const std::string& line : results){
                    logger.log("Assistant test:   a result: " + line, COLOR_RED);
                }
                for (const std::string& line : console.did){
                    logger.log("Assistant test:   an input: " + line, COLOR_RED);
                }
            }
        }

        panel.show_page(Assistant::Panel::Page::SETTINGS);
        save(window, "night-fire-claude-settings");
        panel.show_start_page();
        set_look(UiThemeMode::POCKET_DAY, Starter::WATER);
        save(window, "day-water-claude");
        set_look(UiThemeMode::POCKET_NIGHT, Starter::FIRE);

        //  The made-up connection ends with this block: back to the
        //  network, with the files still in the test's folder and without
        //  the made-up keys, so that nothing can be sent with them.
        service.secrets().set(Assistant::Secret::CLAUDE_API_KEY, QString(), error);
        service.secrets().set(Assistant::Secret::GITHUB_TOKEN, QString(), error);
        window.show_assistant(false);
        window.remake_assistant(nullptr, (folder + "/").toStdString());
    }

    //  Back to the first look. (Nothing of this is saved: the program
    //  exits without writing its settings.)
    window.show_games();
    let_run(300);

    logger.log(
        "UI test: " + std::to_string(number - failed) + " of " + std::to_string(number) + " pictures saved.",
        failed == 0 ? COLOR_BLUE : COLOR_RED
    );
    application.processEvents();
    return failed;
}


//  The list of programs in the side bar scrolls, with the wheel and by its
//  bar, also while a program runs: the list is locked then, and a click on
//  another program opens nothing. (Up to build 19 the list was disabled
//  while a program ran, and a disabled list does not scroll.)
int check_program_list(MainWindow& window, Logger& logger){
    const std::string GAME = Pokemon::STRING_POKEMON + " FireRed and LeafGreen";
    const std::string PROGRAM = "AutoStory";
    int failed = 0;
    auto problem = [&](const std::string& what){
        failed++;
        logger.log("UI test: the list of programs: " + what, COLOR_RED);
    };
    auto open_name = [&]{
        return window.assistant().hooks().open_program().name.toStdString();
    };

    //  As the program does when a run starts and ends.
    PanelHolder& holder = window;
    struct Idle{
        PanelHolder& panel_holder;
        ~Idle(){
            panel_holder.on_idle();
        }
    };

    const UiThemeMode LOOKS[] = {UiThemeMode::POCKET_NIGHT, UiThemeMode::DEFAULT_MODE};
    for (UiThemeMode look : LOOKS){
        GlobalSettings::instance().THEME->set(look);
        let_run(300);
        if (!window.open_program(GAME, PROGRAM)){
            problem("could not open " + GAME + ", " + PROGRAM);
            continue;
        }
        let_run(300);
        QListWidget* list = window.findChild<QListWidget*>("ppProgramList");
        if (list == nullptr){
            problem("there is none in the window");
            continue;
        }
        QScrollBar* bar = list->verticalScrollBar();
        if (bar->maximum() <= 0){
            //  A window so low that the list is longer than its room.
            window.resize(1360, 480);
            let_run(300);
        }
        if (bar->maximum() <= 0){
            problem("it has nothing to scroll (" + std::to_string(list->count()) + " rows)");
            continue;
        }

        auto turn_wheel = [&]{
            const QPointF at(40, 60);
            QWheelEvent event(
                at, list->viewport()->mapToGlobal(at), QPoint(), QPoint(0, -120),
                Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false
            );
            QApplication::sendEvent(list->viewport(), &event);
        };
        auto send_mouse = [&](
            QWidget& target, QEvent::Type type, const QPoint& at, Qt::MouseButton button, Qt::MouseButtons held
        ){
            QMouseEvent event(type, QPointF(at), QPointF(target.mapToGlobal(at)), button, held, Qt::NoModifier);
            QApplication::sendEvent(&target, &event);
        };
        //  The handle of the bar, taken with the mouse and moved down.
        auto drag_bar = [&]{
            QStyleOptionSlider option;
            option.initFrom(bar);
            option.orientation = Qt::Vertical;
            option.minimum = bar->minimum();
            option.maximum = bar->maximum();
            option.sliderPosition = bar->sliderPosition();
            option.sliderValue = bar->value();
            option.singleStep = bar->singleStep();
            option.pageStep = bar->pageStep();
            const QRect handle = bar->style()->subControlRect(
                QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarSlider, bar
            );
            const QPoint from = handle.center();
            const QPoint to = from + QPoint(0, 40);
            send_mouse(*bar, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
            send_mouse(*bar, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
            send_mouse(*bar, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
        };

        const std::string look_name = look == UiThemeMode::POCKET_NIGHT ? "Night" : "classic light";
        //  The drag as this test makes it moved the bar while no program
        //  ran: only then does it say anything about a run.
        bool drag_works = false;
        for (bool running : {false, true}){
            const std::string when =
                std::string(running ? "while a program runs" : "while no program runs") + " (" + look_name + ")";
            if (running){
                holder.on_busy();
            }
            Idle idle{holder};
            let_run(100);

            bar->setValue(0);
            turn_wheel();
            if (bar->value() <= 0){
                problem("the wheel does not scroll it " + when);
            }
            if (!bar->isEnabled()){
                problem("its bar is off " + when);
            }
            bar->setValue(0);
            drag_bar();
            if (!running){
                drag_works = bar->value() > 0;
                if (!drag_works){
                    logger.log("UI test: the list of programs: the bar's handle could not be dragged by this test (" + look_name + ").");
                }
            }else if (drag_works && bar->value() <= 0){
                problem("its bar does not move " + when);
            }
            bar->setValue(0);
            let_run(50);

            if (!running){
                continue;
            }
            //  A click on another program: nothing is opened, and the row
            //  of the program that runs stays the marked one.
            QListWidgetItem* other = nullptr;
            for (int row = 0; row < list->count() && other == nullptr; row++){
                QListWidgetItem* item = list->item(row);
                const QRect box = list->visualItemRect(item);
                if (
                    !item->isHidden() && !item->isSelected() && (item->flags() & Qt::ItemIsSelectable) &&
                    list->viewport()->rect().contains(box)
                ){
                    other = item;
                }
            }
            if (other == nullptr){
                problem("no other program in sight to click on " + when);
                continue;
            }
            const QPoint at = list->visualItemRect(other).center();
            send_mouse(*list->viewport(), QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton);
            send_mouse(*list->viewport(), QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton);
            let_run(100);
            QToolTip::hideText();
            const QList<QListWidgetItem*> marked = list->selectedItems();
            if (open_name() != PROGRAM || marked.size() != 1 || marked.front()->text().toStdString() != PROGRAM){
                problem(
                    "a click on \"" + other->text().toStdString() + "\" " + when + " opened or marked it (open: \"" +
                    open_name() + "\")"
                );
            }
        }
        window.resize(1360, 900);
        let_run(200);
    }

    //  Back to the first look and size, and to the page with the games.
    GlobalSettings::instance().THEME->set(UiThemeMode::POCKET_NIGHT);
    window.resize(1360, 900);
    let_run(300);
    window.show_games();
    let_run(300);
    logger.log(
        failed == 0
            ? "UI test: the list of programs scrolls with the wheel and by its bar, also while a program runs, "
              "and a click opens no other program then."
            : "UI test: the list of programs does not work as it should.",
        failed == 0 ? COLOR_BLUE : COLOR_RED
    );
    return failed;
}


//  Settings written by an older build keep their audio input (FIXES.md, 19):
//  they have it as "InputFile" or "InputDevice", newer ones as
//  "InputSourceType". Only which kind of input comes out is checked: the
//  device itself is not on the build machine.
int check_older_audio_settings(Logger& logger){
    struct Case{
        const char* what;
        const char* source_type;    //  nullptr: settings of an older build
        const char* file;
        const char* device;
        AudioSourceType expected;
    };
    const Case CASES[] = {
        {"older settings with a device",            nullptr, "", "a device", AudioSourceType::Device},
        {"older settings with a file",              nullptr, "sound.wav", "a device", AudioSourceType::File},
        {"older settings without an input",         nullptr, "", "", AudioSourceType::None},
        {"newer settings (the old keys are not looked at)", "InputSource_None", "sound.wav", "a device", AudioSourceType::None},
    };
    int failed = 0;
    for (const Case& test : CASES){
        JsonObject json;
        if (test.source_type != nullptr){
            json["InputSourceType"] = test.source_type;
        }
        json["InputFile"] = test.file;
        json["InputDevice"] = test.device;
        AudioOption option;
        option.load_json(JsonValue(std::move(json)));
        if (option.input_source()->type != test.expected){
            failed++;
            logger.log(std::string("Settings test: the wrong kind of audio input for ") + test.what + ".", COLOR_RED);
        }
    }
    logger.log(
        "Settings test: " + std::to_string(std::size(CASES) - failed) + " of " + std::to_string(std::size(CASES)) +
        " settings came up with the audio input they name.",
        failed == 0 ? COLOR_BLUE : COLOR_RED
    );
    return failed;
}



//  The parts of the assistant that need no connection: reading the API's
//  stream, the prices, line ends, and the file with the keys (on Windows:
//  that it really is encrypted, and comes back).
int check_assistant(const QString& out, Logger& logger){
    const QString folder = out + "/assistant-test";
    QDir().mkpath(folder);
    const Assistant::CheckResult result = Assistant::run_checks(folder, [&](const QString& line){
        logger.log("Assistant test: failed: " + line.toStdString(), COLOR_RED);
    });
    logger.log(
        "Assistant test: " + std::to_string(result.checks - result.failed) + " of " +
        std::to_string(result.checks) + " checks passed.",
        result.failed == 0 ? COLOR_BLUE : COLOR_RED
    );

    //  What Claude gets in the program "Claude Plays": tools with a schema
    //  the API takes (an object with its properties), each name once, and
    //  the one that saves only when the user allows it.
    int wrong = 0;
    for (bool allow_save : {false, true}){
        const std::vector<Assistant::PlayTool> tools = NintendoSwitch::PokemonFRLG::claude_plays_tools(allow_save);
        std::set<std::string> names;
        bool saves = false;
        for (const Assistant::PlayTool& tool : tools){
            const JsonValue schema = parse_json(tool.input_schema);
            const JsonObject* object = schema.to_object();
            const bool fine =
                object != nullptr &&
                object->get_string_default("type") == "object" &&
                object->get_object("properties") != nullptr &&
                !tool.name.empty() && !tool.description.empty() &&
                names.insert(tool.name).second;
            if (!fine){
                wrong++;
                logger.log("Assistant test: failed: the tool \"" + tool.name + "\" of Claude Plays", COLOR_RED);
            }
            saves = saves || tool.name == "save_game";
        }
        const std::string instructions = NintendoSwitch::PokemonFRLG::claude_plays_instructions(allow_save);
        if (tools.size() < 8 || saves != allow_save ||
            (instructions.find("Do not save the game") != std::string::npos) == allow_save
        ){
            wrong++;
            logger.log("Assistant test: failed: saving in Claude Plays, allowed or not", COLOR_RED);
        }
    }
    //  The same for the Claude Plays of the other games: for each game its
    //  controller, with and without saving and HOME.
    int game_cases = 0;
    std::set<std::string> identifiers;
    for (ClaudePlays::GameId id : ClaudePlays::all_games()){
        const ClaudePlays::GameProfile& profile = ClaudePlays::game_profile(id);
        if (!identifiers.insert(profile.identifier).second){
            wrong++;
            logger.log("Assistant test: failed: two games with the identifier " + profile.identifier, COLOR_RED);
        }
        const ClaudePlays::PadLayout& layout = profile.pad == ClaudePlays::PadKind::RIGHT_JOYCON
            ? ClaudePlays::right_joycon_layout()
            : ClaudePlays::pro_controller_layout();
        for (bool allowed : {false, true}){
            game_cases++;
            ClaudePlays::AnyGameOptions options;
            options.allow_save = allowed;
            options.allow_home = allowed;
            std::vector<Assistant::PlayTool> tools = ClaudePlays::any_game_tools(layout, options, profile.save_routine);
            for (Assistant::PlayTool& tool : ClaudePlays::session_tools()){
                tools.push_back(std::move(tool));
            }
            std::set<std::string> names;
            bool saves = false;
            bool home = false;
            for (const Assistant::PlayTool& tool : tools){
                const JsonValue schema = parse_json(tool.input_schema);
                const JsonObject* object = schema.to_object();
                const bool fine =
                    object != nullptr &&
                    object->get_string_default("type") == "object" &&
                    object->get_object("properties") != nullptr &&
                    !tool.name.empty() && !tool.description.empty() &&
                    names.insert(tool.name).second;
                if (!fine){
                    wrong++;
                    logger.log(
                        "Assistant test: failed: the tool \"" + tool.name + "\" of Claude Plays for " + profile.identifier,
                        COLOR_RED
                    );
                }
                saves = saves || tool.name == "save_game";
                home = home || tool.input_schema.find("\"HOME\"") != std::string::npos;
            }
            ClaudePlays::GameFacts facts;
            facts.name = profile.name;
            facts.notes = profile.notes;
            const std::string instructions = ClaudePlays::any_game_instructions(layout, options, facts, profile.save_routine);
            if (tools.size() < 8 || saves != (allowed && profile.save_routine) || home != allowed ||
                (instructions.find("Do not save the game") != std::string::npos) == allowed ||
                (instructions.find("does not know which game") != std::string::npos) != profile.name.empty()
            ){
                wrong++;
                logger.log("Assistant test: failed: what Claude gets for " + profile.identifier, COLOR_RED);
            }
        }
    }
    logger.log(
        wrong == 0
            ? "Assistant test: the tools of Claude Plays are what the API takes (FireRed and LeafGreen, and " +
              std::to_string(game_cases) + " cases of the " + std::to_string(ClaudePlays::all_games().size()) +
              " other lists)."
            : std::string("Assistant test: the tools of Claude Plays are not right."),
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return result.failed + wrong;
}


//  The FRLG AutoStory tells the two NIDORANs apart by the mark at the end of
//  the name in the foe's box: that reader on names drawn for it, as this
//  build was compiled.
int check_name_mark(Logger& logger){
    int cases = 0;
    std::string failures;
    const int wrong = NintendoSwitch::PokemonFRLG::AutoStoryNameMark::check_reader(cases, failures);
    size_t at = 0;
    while (at < failures.size()){
        size_t end = failures.find('\n', at);
        if (end == std::string::npos){
            end = failures.size();
        }
        logger.log("AutoStory test: failed: " + failures.substr(at, end - at), COLOR_RED);
        at = end + 1;
    }
    logger.log(
        "AutoStory test: the mark at the end of a NIDORAN's name was read right in " +
        std::to_string(cases - wrong) + " of " + std::to_string(cases) + " drawn names.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  What the FRLG AutoStory catches where: a place's list for each game and
//  which place is worth a wait for a species, and the POKeDEX table through
//  its own file and back, as this build writes and reads it.
int check_collection_lists(const QString& out, Logger& logger){
    using namespace NintendoSwitch::PokemonFRLG;
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("AutoStory test: failed: ") + what, COLOR_RED);
        }
    };
    //  The center of the Safari Zone (PokemonFRLG_AutoStoryData.h).
    const char* CENTER =
        "chansey:1:1,exeggcute:20:20,nidoran-f:0:20,nidoran-m:20:0,nidorina:5:10,nidorino:10:5,"
        "parasect:5:5,pinsir:0:4,rhyhorn:20:20,scyther:4:0,venonat:15:15";
    std::map<std::string, int> fire_red = story_place_species(CENTER, 0);
    std::map<std::string, int> leaf_green = story_place_species(CENTER, 1);
    check(
        fire_red.size() == 9 && fire_red.count("nidoran-f") == 0 && fire_red.count("pinsir") == 0 &&
        fire_red["nidoran-m"] == 20 && fire_red["scyther"] == 4 && fire_red["nidorina"] == 5,
        "the center of the Safari Zone in FireRed"
    );
    check(
        leaf_green.size() == 9 && leaf_green.count("nidoran-m") == 0 && leaf_green.count("scyther") == 0 &&
        leaf_green["nidoran-f"] == 20 && leaf_green["pinsir"] == 4 && leaf_green["nidorina"] == 10,
        "the center of the Safari Zone in LeafGreen"
    );
    check(
        story_place_species("abra,zubat", 0).size() == 2 && story_place_species(nullptr, 0).empty() &&
        story_place_species(CENTER, -1).size() == 11,
        "a list without numbers, no list, either game"
    );
    check(
        story_place_worth(5, 5) && story_place_worth(3, 5) && !story_place_worth(2, 5) &&
        !story_place_worth(5, 30) && story_place_worth(10, 70) && !story_place_worth(1, 4) && story_place_worth(1, 0),
        "which place is worth a wait"
    );
    //  The table: written as the program writes its files (a mark in front,
    //  Windows line ends), read back.
    std::map<std::string, StoryCollectionCount> counts;
    counts["doduo"] = {2, 0};
    counts["nidoran-f"] = {1, 1};
    counts["pidgey"] = {0, 0};
    std::map<std::string, StoryCollectionCount> back;
    std::map<std::string, StoryCollectionCount> other;
    bool read = false;
    bool not_another = false;
    const std::string path = (out + "/autostory-collection-test.txt").toStdString();
    try{
        string_to_file(path, story_collection_to_text("Charmander", counts));
        std::string text;
        read = file_to_string(path, text) && story_collection_from_text(text, "Charmander", back);
        not_another = !story_collection_from_text(text, "Squirtle", other) && other.empty();
    }catch (...){
    }
    QFile::remove(QString::fromStdString(path));
    check(
        read && back.size() == 2 && back["doduo"].caught == 2 && back["nidoran-f"].caught == 1 &&
        back["nidoran-f"].shiny == 1,
        "the POKeDEX table through its file and back"
    );
    check(not_another, "the POKeDEX table of another starter is not taken");
    std::map<std::string, StoryCollectionCount> older;
    older["doduo"] = {1, 0};
    older["gloom"] = {3, 0};
    check(
        story_collection_merge(older, back) == 2 && older["doduo"].caught == 2 && older["gloom"].caught == 3 &&
        older["nidoran-f"].shiny == 1,
        "the larger count of two tables"
    );
    logger.log(
        "AutoStory test: the lists of what comes where in each game, and the POKeDEX table through its file: " +
        std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  The FRLG AutoStory's money for the POKeDEX collection: whether a Safari
//  Game was joined (the gate house has black at both sides of the
//  picture), which places wait for the money trip, and the trip in the
//  story's data as this build has it.
int check_money_trip(Logger& logger){
    using namespace NintendoSwitch;
    using namespace NintendoSwitch::PokemonFRLG;
    using AutoStorySmallRoom::Sides;
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("AutoStory test: failed: ") + what, COLOR_RED);
        }
    };

    //  Pictures of the game, 480 by 320: a room between "from" and "to" of
    //  the width (black beside it), boxes over it.
    const size_t DRAWN_WIDTH = 480;
    const size_t DRAWN_HEIGHT = 320;
    struct DrawnBox{
        double x0, y0, x1, y1;
        uint32_t color;
    };
    auto look = [&](double from, double to, const std::vector<DrawnBox>& boxes){
        std::vector<uint32_t> picture(DRAWN_WIDTH * DRAWN_HEIGHT, 0xff000000);
        auto fill = [&](const DrawnBox& box){
            for (size_t y = (size_t)(box.y0 * (double)DRAWN_HEIGHT); y < (size_t)(box.y1 * (double)DRAWN_HEIGHT) && y < DRAWN_HEIGHT; y++){
                for (size_t x = (size_t)(box.x0 * (double)DRAWN_WIDTH); x < (size_t)(box.x1 * (double)DRAWN_WIDTH) && x < DRAWN_WIDTH; x++){
                    //  A little texture, as a map has.
                    picture[y * DRAWN_WIDTH + x] = (x / 8 + y / 8) % 2 == 0 ? box.color : box.color - 0x00101010;
                }
            }
        };
        fill({from, 0, to, 1, 0xff60b860});
        for (const DrawnBox& box : boxes){
            fill(box);
        }
        return AutoStorySmallRoom::black_sides(picture.data(), DRAWN_WIDTH, DRAWN_HEIGHT, DRAWN_WIDTH * sizeof(uint32_t));
    };
    const DrawnBox money_box{0.03, 0.05, 0.63, 0.42, 0xfff8f8f8};
    const DrawnBox text_box{0.02, 0.72, 0.98, 0.98, 0xfff8f8f8};
    const DrawnBox start_menu{0.73, 0.00, 1.00, 0.75, 0xfff8f8f8};
    //  The gate house of the Safari Zone is 9 of the picture's 15 tiles wide.
    check(look(0.2, 0.8, {}) == Sides::BLACK, "the gate house, the player in its middle");
    check(look(4.0 / 15, 13.0 / 15, {}) == Sides::BLACK, "the gate house, the player a tile to the left");
    check(look(0.2, 0.8, {money_box, text_box}) == Sides::BLACK, "the gate house with the money box and a text box");
    check(look(0.2, 0.8, {start_menu}) == Sides::OPEN, "the gate house with the start menu over its right side");
    check(look(0, 1, {}) == Sides::OPEN, "outdoors");
    check(look(0, 0, {}) == Sides::DARK, "a black picture");
    check(look(1.0 / 15, 14.0 / 15, {}) == Sides::OPEN, "a room 13 tiles wide");

    //  Short of money or balls: which places wait.
    check(
        !story_places_wait(1, 1, false, false, true) && !story_places_wait(0, 0, true, true, true) &&
        story_places_wait(1, 0, true, false, true) && story_places_wait(0, 2, true, false, true) &&
        story_places_wait(0, 1, false, true, true),
        "with the money trip to come, every place waits for it"
    );
    check(
        story_places_wait(2, 0, true, false, false) && !story_places_wait(0, 2, true, false, false) &&
        !story_places_wait(1, 1, true, false, false) && story_places_wait(0, 3, false, true, false) &&
        !story_places_wait(1, 0, false, true, false) && story_places_wait(1, 1, true, true, false),
        "without it, only the places that take what is short"
    );

    //  The story's data: the walk of each REMATCH step (100 steps or more
    //  for the VS SEEKER, back to where it starts, facing the trainer), and
    //  the steps a Safari Game not joined skips (the game itself: one DEX
    //  step in the Safari Zone; then a walk).
    const StoryScript& script = AutoStoryData::story_CHARMANDER();
    int rematches = 0;
    int rematch_wins = 0;
    int shortest_walk = 0;
    bool walks_fine = true;
    int joins = 0;
    bool joins_fine = true;
    int purchases = 0;
    bool money_segment = false;
    for (size_t s = 0; s < script.segment_count; s++){
        const StorySegment& segment = script.segments[s];
        for (size_t k = 0; k < segment.step_count; k++){
            const StoryStep& step = script.step(segment, k);
            if (step.type == StepType::REMATCH){
                money_segment |= std::string(segment.id).find("_MONEY") != std::string::npos;
                const std::vector<int32_t> legs = script.leg_list(step.legs_a);
                int walked = 0;
                int dx = 0;
                int dy = 0;
                bool plain = true;
                for (int32_t leg_index : legs){
                    const StoryLeg& leg = script.leg(leg_index);
                    const StoryRun* runs = script.leg_runs(leg);
                    for (uint16_t r = 0; r < leg.run_count; r++){
                        plain &= runs[r].kind == RunKind::WALK;
                        walked += runs[r].count;
                        dx += runs[r].direction == DPAD_RIGHT ? runs[r].count : runs[r].direction == DPAD_LEFT ? -(int)runs[r].count : 0;
                        dy += runs[r].direction == DPAD_DOWN ? runs[r].count : runs[r].direction == DPAD_UP ? -(int)runs[r].count : 0;
                    }
                }
                const bool faces = !legs.empty() && script.leg(legs.back()).end == LegEnd::INTERACT &&
                    script.leg(legs.back()).face != DPAD_NONE;
                walks_fine &= plain && walked >= 100 && dx == 0 && dy == 0 && faces && step.p0 > 0 && step.p2 > 0;
                shortest_walk = rematches == 0 ? walked : std::min(shortest_walk, walked);
                rematches++;
                rematch_wins += step.p0;
            }
            if (step.type == StepType::BUY && step.p4 > 0){
                purchases++;
            }
            if (step.type == StepType::CUTSCENE && step.p3 > 0){
                joins++;
                int games = 0;
                for (size_t n = k + 1; n <= k + (size_t)step.p3 && n < segment.step_count; n++){
                    const StoryStep& skipped = script.step(segment, n);
                    games += skipped.type == StepType::DEX && skipped.p2 > 0 && (skipped.p2 & 1) != 0 ? 1 : 0;
                }
                const size_t after = k + (size_t)step.p3 + 1;
                joins_fine &= games == 1 && after < segment.step_count && script.step(segment, after).type == StepType::WALK;
            }
        }
    }
    check(
        money_segment && rematches == 2 && walks_fine && purchases == 4,
        "the money trip in the story's data: the walk for the VS SEEKER, the purchases"
    );
    check(joins >= 8 && joins_fine, "the steps skipped when a Safari Game is not joined");
    logger.log(
        "AutoStory test: money for the collection (a Safari Game not joined: the gate house on drawn pictures; "
        "which places wait; the money trip in the story's data: " + std::to_string(rematch_wins) +
        " battles, a walk of " + std::to_string(shortest_walk) + " steps for the VS SEEKER, " +
        std::to_string(joins) + " Safari Games): " +
        std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  The FRLG AutoStory at the pier of Vermilion City: the sailor's talk as
//  the game's script has it, pressed through without any of it read (his
//  text is blue, which the text reading does not see).
int check_ferry_talk(Logger& logger){
    using namespace NintendoSwitch::PokemonFRLG;
    using AutoStoryFerryTalk::Act;
    using AutoStoryFerryTalk::Look;
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("AutoStory test: failed: ") + what, COLOR_RED);
        }
    };

    //  A page of his talk, by whether it shows the advance arrow once it
    //  is printed. A on a printed page goes to the next one; A on a page
    //  still printing only hurries it.
    const bool WITH_ARROW = true;
    const bool NO_ARROW = false;
    //  "Oh! That's a MYSTICTICKET! Now that is rare." / "We'll be happy to
    //  take you to NAVEL ROCK anytime." (and the AURORATICKET's two pages).
    const std::vector<bool> TICKET{WITH_ARROW, NO_ARROW};
    //  "Would you like to board a SEAGALLOP ferry?" with the tickets' menu,
    //  the same with the Sevii Islands list, "Okay, everything's in
    //  order." / "SEAGALLOP HI-SPEED 7 will be departing immediately."
    const std::vector<bool> BOARDING{NO_ARROW, NO_ARROW, WITH_ARROW, NO_ARROW};
    auto pages_of = [&](int tickets){
        std::vector<bool> pages;
        for (int t = 0; t < tickets; t++){
            pages.insert(pages.end(), TICKET.begin(), TICKET.end());
        }
        pages.insert(pages.end(), BOARDING.begin(), BOARDING.end());
        return pages;
    };

    struct Run{
        Act end = Act::WAIT;        //  WAIT: still going after 400 looks
        int presses = 0;
        int looks = 0;
        size_t pages_done = 0;
        bool pressed_at_nothing = false;    //  an A without a text box
        bool long_enough = false;
        double seconds = 0;
    };
    //  "printing": how many looks a page takes to print. "missed": the
    //  game misses every so manieth A (0: none). "blind_at", "blind_for":
    //  looks from that one on at which the text box is not made out.
    auto run = [&](
        const std::vector<bool>& pages, int printing, int missed, bool first_row,
        int blind_at = -1, int blind_for = 0
    ){
        Run result;
        AutoStoryFerryTalk::Talk talk;
        size_t page = 0;
        int printed = 0;
        while (result.looks < 400){
            const bool blind = blind_at >= 0 && result.looks >= blind_at && result.looks < blind_at + blind_for;
            result.looks++;
            Look look = Look::NO_BOX;
            if (page < pages.size() && !blind){
                look = printed >= printing && pages[page] ? Look::ARROW : Look::BOX;
            }
            const Act act = AutoStoryFerryTalk::next(talk, look, first_row);
            if (act == Act::PRESS_A){
                result.presses++;
                result.seconds += 1.2;
                if (look == Look::NO_BOX){
                    result.pressed_at_nothing = true;
                }else if (printed < printing){
                    printed = printing;
                }else if (missed > 0 && result.presses % missed == 0){
                    //  Missed by the game: the page stays.
                }else{
                    page++;
                    printed = 0;
                }
                continue;
            }
            if (act == Act::WAIT){
                result.seconds += 0.5;
                printed++;
                continue;
            }
            result.end = act;
            break;
        }
        result.pages_done = page;
        result.long_enough = talk.long_enough();
        return result;
    };
    auto boarded = [](const Run& r, size_t pages){
        return r.end == Act::LEFT && r.pages_done == pages && !r.pressed_at_nothing && r.long_enough;
    };

    //  The first time with the RAINBOW PASS and both tickets: eight pages.
    const Run first = run(pages_of(2), 0, 0, true);
    check(boarded(first, 8) && first.presses == 8, "the first time (both tickets' texts): eight pages, eight presses");
    check(first.seconds < 30, "the first time: well within the 90 seconds of the step");
    //  Slow text: A's that only hurry a page.
    const Run slow = run(pages_of(2), 3, 0, true);
    check(boarded(slow, 8) && slow.presses > 8 && slow.seconds < 60, "the first time with slow text");
    //  Later times, and with one ticket only.
    const Run later = run(pages_of(0), 1, 0, true);
    check(boarded(later, 4), "a later time: the question, the two menus, the two pages of the departure");
    check(boarded(run(pages_of(1), 1, 0, true), 6), "with one ticket's text");
    //  The game misses a press now and then.
    check(boarded(run(pages_of(2), 1, 3, true), 8), "every third A missed by the game");
    check(boarded(run(pages_of(0), 0, 2, true), 4), "every second A missed by the game");
    //  The text box not made out for a moment, at any time of the talk
    //  (also after the fourth press, when the talk could be over): not
    //  taken for its end, and no A without a text box.
    bool blinks_fine = true;
    int blinks = 0;
    for (int tickets = 0; tickets <= 2; tickets += 2){
        for (int at = 0; at < 30; at++){
            for (int looks = 1; looks < AutoStoryFerryTalk::LOOKS_TO_LEAVE; looks++){
                blinks++;
                blinks_fine &= boarded(run(pages_of(tickets), 1, 0, true, at, looks), (size_t)(4 + 2 * tickets));
            }
        }
    }
    check(blinks_fine, "the text box not made out for up to three looks, at any time of the talk");
    //  No text at all: an error, without a press.
    const Run nothing = run({}, 0, 0, true);
    check(
        nothing.end == Act::NO_TEXT && nothing.presses == 0 && nothing.looks == AutoStoryFerryTalk::LOOKS_FOR_TEXT,
        "no text box at all"
    );
    //  A talk shorter than a departure ("The ship set sail."): over, but
    //  only after a longer wait, and marked as too short.
    const Run brief = run({NO_ARROW}, 0, 0, true);
    check(
        brief.end == Act::LEFT && !brief.long_enough && !brief.pressed_at_nothing &&
        brief.looks >= AutoStoryFerryTalk::LOOKS_TO_LEAVE_UNSURE,
        "a talk of one page"
    );
    //  Another row than the first: only the page with the arrow gets A,
    //  the menus are for the reading.
    const Run other_row = run(pages_of(2), 0, 0, false);
    check(
        other_row.end == Act::WAIT && other_row.presses == 1 && other_row.pages_done == 1,
        "another row than the first: no A on a page without the arrow"
    );

    logger.log(
        "AutoStory test: the sailor at the pier of Vermilion City (his pages and menus pressed through without "
        "reading them: the first time with both tickets' texts in " + std::to_string(first.presses) +
        " presses, slow text in " + std::to_string(slow.presses) + ", later times in " +
        std::to_string(later.presses) + "; presses missed by the game; the text box not made out for a "
        "moment, " + std::to_string(blinks) + " ways; no text at all): " +
        std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  The FRLG AutoStory over open water, where the video cannot count the
//  tiles: a run that ends where the way is shut (AutoStoryBump), played
//  against a line of water and land with a wall at its end, and the water
//  between Five Island and Resort Gorgeous in the story's data.
int check_bump(Logger& logger){
    using namespace NintendoSwitch;
    using namespace NintendoSwitch::PokemonFRLG;
    using AutoStoryBump::Act;
    using AutoStoryBump::Seen;
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("AutoStory test: failed: ") + what, COLOR_RED);
        }
    };

    struct Trip{
        Act end = Act::PART;
        int at = 0;         //  tiles from the start
        int inputs = 0;
    };
    //  "water" tiles of water, then "land" tiles of land (the first of them
    //  the shore), then what shuts the way. An input is timed for the
    //  water: on land it gets half as far, and the jump onto the shore
    //  takes as long as three tiles of water. "short_by": how many tiles
    //  the first input falls short (the game standing still on the way).
    //  "battle_at": a wild battle stops an input there (-1: none). "blind":
    //  tries at the end at which the video sees no movement without being
    //  sure of it.
    auto travel = [&](int water, int land, int planned, int short_by, int battle_at, int blind){
        Trip trip;
        const int end = water + land;
        AutoStoryBump::Run run;
        Act act = AutoStoryBump::first(run);
        bool battle_done = battle_at < 0;
        bool first_input = true;
        while (act == Act::WHOLE || act == Act::PART){
            trip.inputs++;
            //  In half tiles of water.
            int budget = 2 * (act == Act::WHOLE ? planned : AutoStoryBump::MORE_TILES);
            if (first_input){
                budget -= 2 * short_by;
                first_input = false;
            }
            const int from = trip.at;
            bool stopped = false;
            while (budget > 0 && trip.at < end){
                //  A step that has begun is finished.
                budget -= trip.at < water ? 2 : (trip.at == water && water > 0 ? 6 : 4);
                trip.at++;
                if (!battle_done && trip.at >= battle_at){
                    battle_done = true;
                    stopped = true;
                    break;
                }
            }
            Seen seen = Seen::MOVED;
            if (stopped){
                seen = Seen::STOPPED;
            }else if (trip.at == from){
                seen = blind > 0 ? Seen::STILL : Seen::STOOD;
                blind -= blind > 0 ? 1 : 0;
            }
            act = AutoStoryBump::next(run, seen);
        }
        trip.end = act;
        return trip;
    };
    auto arrived = [](const Trip& trip, int end){
        return trip.end == Act::DONE && trip.at == end;
    };

    //  Up the channel: 29 tiles of water to a rock. As timed, one short
    //  (what the run of 30 was each time), three short.
    const Trip channel = travel(29, 0, 29, 1, -1, 0);
    check(arrived(travel(29, 0, 29, 0, -1, 0), 29), "29 tiles of water to a rock, as timed");
    check(arrived(channel, 29), "the same with the first input one tile short");
    check(arrived(travel(29, 0, 29, 3, -1, 0), 29), "the same with it three tiles short");
    //  Along the row to the beach and over it: 16 of water, the shore, 6 of
    //  land. And down to Five Island: 30 of water, the shore, 9 of land.
    const Trip beach = travel(16, 7, 23, 1, -1, 0);
    check(arrived(beach, 23), "16 tiles of water, the shore and 6 tiles of land");
    check(arrived(travel(30, 10, 40, 1, -1, 0), 40), "30 tiles of water, the shore and 9 tiles of land");
    //  A wild battle anywhere on the way, and the first input short by up
    //  to two tiles: there every time, never before the end.
    bool all_arrive = true;
    int ways = 0;
    for (int short_by = 0; short_by <= 2; short_by++){
        for (int battle_at = 0; battle_at <= 40; battle_at++){
            ways += 3;
            all_arrive &= arrived(travel(29, 0, 29, short_by, battle_at <= 29 ? battle_at : -1, 0), 29);
            all_arrive &= arrived(travel(16, 7, 23, short_by, battle_at <= 23 ? battle_at : -1, 0), 23);
            all_arrive &= arrived(travel(30, 10, 40, short_by, battle_at, 0), 40);
        }
    }
    check(all_arrive, "a wild battle anywhere on the way, the first input up to two tiles short");
    //  The video not sure at the end: once, and never (the waves): there
    //  all the same, a few inputs later.
    const Trip unsure = travel(29, 0, 29, 1, -1, 1);
    const Trip never_sure = travel(29, 0, 29, 1, -1, 1000);
    check(arrived(unsure, 29) && unsure.inputs == channel.inputs + 1, "the video not sure at the end once");
    check(
        arrived(never_sure, 29) && never_sure.inputs == channel.inputs + AutoStoryBump::STILL_TO_END - AutoStoryBump::SURE_TO_END,
        "the video never sure at the end"
    );
    //  The way shut from the start (not on the water): an error, not an arrival.
    check(travel(0, 0, 29, 0, -1, 0).end == Act::SHUT, "the way shut from the start");

    //  The story's data: the water between Five Island and Resort Gorgeous
    //  is crossed by such runs only (four a round trip: up the channel,
    //  along the row to the beach, back along the row, down the channel),
    //  each the last run of its leg; no other run over water there is
    //  longer than a few tiles.
    const StoryScript& script = AutoStoryData::story_CHARMANDER();
    int money_bumps = 0;
    int cave_bumps = 0;
    bool bumps_last = true;
    int longest_other = 0;
    for (size_t s = 0; s < script.segment_count; s++){
        const StorySegment& segment = script.segments[s];
        const bool money = std::string(segment.id).find("_MONEY") != std::string::npos;
        for (size_t k = 0; k < segment.step_count; k++){
            const StoryStep& step = script.step(segment, k);
            if (step.type != StepType::WALK || step.leg < 0){
                continue;
            }
            const StoryLeg& leg = script.leg(step.leg);
            const StoryRun* runs = script.leg_runs(leg);
            const bool cave = std::string(leg.name).find("LostCave") != std::string::npos;
            for (uint16_t r = 0; r < leg.run_count; r++){
                if (runs[r].kind == RunKind::SURF_BUMP){
                    money_bumps += money ? 1 : 0;
                    cave_bumps += cave ? 1 : 0;
                    bumps_last &= r + 1 == leg.run_count && (money || cave);
                }else if (money && (runs[r].kind == RunKind::SURF || runs[r].kind == RunKind::SURF_CHECKED)){
                    longest_other = std::max(longest_other, (int)runs[r].count);
                }
            }
        }
    }
    check(
        money_bumps == 8 && bumps_last && longest_other <= 4,
        "the money trip in the story's data: the water to Resort Gorgeous and back"
    );
    check(cave_bumps == 8, "the Lost Cave in the story's data: the same water");

    logger.log(
        "AutoStory test: runs that end where the way is shut (over open water the video cannot count tiles: "
        "up the channel in " + std::to_string(channel.inputs) + " inputs with the first one a tile short, "
        "onto the beach in " + std::to_string(beach.inputs) + "; " + std::to_string(ways) +
        " ways with a wild battle on the way; the video not sure at the end, the way shut from the start; "
        "in the story's data " + std::to_string(money_bumps) + " such runs on the money trip, " +
        std::to_string(cave_bumps) + " to the Lost Cave): " +
        std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
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
        int failed = take_pictures(application, window, out, logger);
        failed += check_program_list(window, logger);
        failed += check_older_audio_settings(logger);
        failed += check_assistant(out, logger);
        failed += check_name_mark(logger);
        failed += check_collection_lists(out, logger);
        failed += check_money_trip(logger);
        failed += check_ferry_talk(logger);
        failed += check_bump(logger);
        return failed == 0 ? 0 : 1;
    }catch (const Exception& e){
        logger.log("UI test: stopped by an exception: " + e.to_str(), COLOR_RED);
    }catch (const std::exception& e){
        logger.log(std::string("UI test: stopped by an exception: ") + e.what(), COLOR_RED);
    }
    return 1;
}



}
}

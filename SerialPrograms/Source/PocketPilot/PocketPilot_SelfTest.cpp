/*  PocketPilot Self Test
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <algorithm>
#include <iterator>
#include <map>
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
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/AudioPipeline/AudioOption.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/Logging/LoggerWindow.h"
#include "CommonFramework/Options/Environment/ThemeSelectorOption.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "CommonFramework/Windows/MainWindow.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBoxes.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBump.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCollectionText.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryData.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryDexCollection.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryFerryTalk.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryFishing.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryNameMark.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPlaceList.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySafari.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryScroll.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySlowSteps.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySmallRoom.h"
#include "VideoGames/PokemonFRLG/Programs/ClaudePlays/PokemonFRLG_ClaudePlays.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_SafariOptimalAction.h"
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

    //  The last town is through: round the towns once more?
    check(
        //  16 November: the round brought four catches, the second CHANSEY
        //  was left.
        story_another_round(1, true, true, false, 2, false) &&
        //  ... and the round after it, if it brings nothing, is the last.
        !story_another_round(1, true, false, false, 3, false) &&
        //  Nothing left at a place of the route.
        !story_another_round(0, true, true, false, 2, false),
        "another round of the towns after a round that brought a catch, none after one that brought nothing"
    );
    check(
        //  Started again where the last run stopped: once round.
        story_another_round(1, false, false, false, 1, false) &&
        !story_another_round(1, false, false, true, 2, false) &&
        //  Short of money or balls with no money trip left, and the most
        //  rounds of a run.
        !story_another_round(3, true, true, false, 2, true) &&
        !story_another_round(3, true, true, false, STORY_MOST_ROUNDS, false),
        "a run that starts at the end goes round once; not when short of money or balls, not for ever"
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
    //  (Two REMATCH steps on the money trip; a third one since build 35,
    //  two battles for the evolution stones.)
    check(
        money_segment && rematches == 3 && rematch_wins == 11 && walks_fine && purchases == 4,
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
    int stones_bumps = 0;
    int cave_bumps = 0;
    //  ... and the laps for the POKeDEX off Five Island (build 32): up the
    //  same channel and down the column next to it.
    int lap_bumps = 0;
    bool bumps_last = true;
    int longest_other = 0;
    for (size_t s = 0; s < script.segment_count; s++){
        const StorySegment& segment = script.segments[s];
        const bool money = std::string(segment.id).find("_MONEY") != std::string::npos;
        //  The prize money for the evolution stones (build 35): the same
        //  way to Lady Gillian, once.
        const bool stones = std::string(segment.id).find("_STONES") != std::string::npos;
        for (size_t k = 0; k < segment.step_count; k++){
            const StoryStep& step = script.step(segment, k);
            if (step.type != StepType::WALK || step.leg < 0){
                continue;
            }
            const StoryLeg& leg = script.leg(step.leg);
            const StoryRun* runs = script.leg_runs(leg);
            const bool cave = std::string(leg.name).find("LostCave") != std::string::npos;
            const bool lap = std::string(leg.name).rfind("SurfFiveIsland", 0) == 0;
            for (uint16_t r = 0; r < leg.run_count; r++){
                if (runs[r].kind == RunKind::SURF_BUMP){
                    money_bumps += money ? 1 : 0;
                    stones_bumps += stones ? 1 : 0;
                    cave_bumps += cave ? 1 : 0;
                    lap_bumps += lap ? 1 : 0;
                    bumps_last &= r + 1 == leg.run_count && (money || stones || cave || lap);
                }else if (money && (runs[r].kind == RunKind::SURF || runs[r].kind == RunKind::SURF_CHECKED)){
                    longest_other = std::max(longest_other, (int)runs[r].count);
                }
            }
        }
    }
    check(
        money_bumps == 8 && stones_bumps == 4 && bumps_last && longest_other <= 4,
        "the money trip in the story's data: the water to Resort Gorgeous and back"
    );
    check(cave_bumps == 8 && lap_bumps == 4, "the Lost Cave and the laps off Five Island in the story's data: the same water");

    logger.log(
        "AutoStory test: runs that end where the way is shut (over open water the video cannot count tiles: "
        "up the channel in " + std::to_string(channel.inputs) + " inputs with the first one a tile short, "
        "onto the beach in " + std::to_string(beach.inputs) + "; " + std::to_string(ways) +
        " ways with a wild battle on the way; the video not sure at the end, the way shut from the start; "
        "in the story's data " + std::to_string(money_bumps) + " such runs on the money trip, " +
        std::to_string(cave_bumps) + " to the Lost Cave, " + std::to_string(lap_bumps) + " on the laps off Five Island): " +
        std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  The FRLG AutoStory over rock stairs (AutoStorySlowSteps): a step up from
//  them or down onto them takes 23 frames, not 16. The marks of the route's
//  data, the timing of an input over them against the game's own steps, and
//  the walk to the grass at the Canyon Entrance.
int check_slow_steps(Logger& logger){
    using namespace NintendoSwitch;
    using namespace NintendoSwitch::PokemonFRLG;
    namespace Slow = AutoStorySlowSteps;
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("AutoStory test: failed: ") + what, COLOR_RED);
        }
    };
    auto bit = [](int step){ return (uint64_t)1 << step; };

    //  The marks.
    const uint64_t marks = bit(1) | bit(28) | bit(63);
    check(
        Slow::is_slow(marks, 1) && Slow::is_slow(marks, 63) && !Slow::is_slow(marks, 0) &&
        !Slow::is_slow(marks, 64) && !Slow::is_slow(marks, -1),
        "a step is marked or not"
    );
    check(
        Slow::slow_steps(marks, 0, 40) == 2 && Slow::slow_steps(marks, 2, 26) == 0 &&
        Slow::slow_steps(marks, 28, 1) == 1 && Slow::slow_steps(marks, 0, 100) == 3,
        "how many steps of a part of a run are slow"
    );
    check(
        Slow::from_step(marks, 28) == (bit(0) | bit(35)) && Slow::from_step(marks, 64) == 0 &&
        Slow::from_step(marks, 0) == marks,
        "the marks of a run from one of its steps on"
    );
    uint64_t joined = 0;
    check(
        Slow::join(bit(1), 2, bit(0), 1, joined) && joined == (bit(1) | bit(2)),
        "two runs walked as one keep their marks"
    );
    check(Slow::join(marks, 70, 0, 30, joined) && joined == marks, "a run without marks joins any run");
    check(!Slow::join(0, 60, bit(5), 10, joined), "marks that do not fit behind the first run: not joined");

    //  The timing, in the game's terms: a step starts when the one before
    //  it is over and the input is still held. An input for "moves" steps
    //  from step "first" of a run on makes just those, with "slack" frames
    //  to spare either way; the player turned first (8 frames) or not.
    auto lands = [&](uint64_t slow, int first, int moves, double slack){
        const int on_stairs = Slow::slow_steps(slow, first, moves);
        for (int turned = 0; turned <= 1; turned++){
            const double turn = turned * 8.0;
            const double hold = Slow::hold_frames(turn, Slow::STEP_FRAMES, moves, on_stairs);
            if (Slow::steps_made(slow, first, turn, hold - slack) != moves ||
                Slow::steps_made(slow, first, turn, hold + slack) != moves
            ){
                return false;
            }
        }
        return true;
    };
    auto lands_from_anywhere = [&](uint64_t slow, int count){
        for (int first = 0; first < count; first++){
            if (!lands(slow, first, count - first, 7)){
                return false;
            }
        }
        return true;
    };
    bool one_stair = true;
    for (int at = 0; at < 19; at++){
        one_stair &= lands_from_anywhere(bit(at), 19);
    }
    check(one_stair, "a run of 19 with one step on stairs, wherever it is: 7 frames to spare either way");
    //  (Mt. Ember, down from the summit: six of 18.)
    const uint64_t ember = bit(4) | bit(5) | bit(10) | bit(11) | bit(14) | bit(15);
    check(lands_from_anywhere(ember, 18), "a run of 18 with six steps on stairs, and what is left of it after a stop");
    //  Which way the player faces not known: the input allows for half a
    //  turn (4 frames), the game takes none or 8.
    bool unknown_facing = true;
    for (int game_turn = 0; game_turn <= 8; game_turn += 8){
        const double hold = Slow::hold_frames(4, Slow::STEP_FRAMES, 18, 6);
        unknown_facing &= Slow::steps_made(ember, 0, game_turn, hold - 3) == 18;
        unknown_facing &= Slow::steps_made(ember, 0, game_turn, hold + 3) == 18;
    }
    check(unknown_facing, "the same with the facing not known");
    //  Timed without the stairs, as before: one step of 19 on stairs and
    //  three frames lost at a map border, a tile short (the walk to the
    //  Canyon Entrance); two steps, a tile short without a border (Nugget
    //  Bridge); four, two tiles (Victory Road).
    const double stall = 3;
    check(
        Slow::steps_made(bit(7), 0, 8 + stall, Slow::hold_frames(8, Slow::STEP_FRAMES, 19, 0)) == 18 &&
        Slow::steps_made(bit(7), 0, 8 + stall, Slow::hold_frames(8, Slow::STEP_FRAMES, 19, 1)) == 19,
        "one step on stairs and a map border: a tile short as it was timed, there with the stairs counted"
    );
    check(
        Slow::steps_made(bit(8) | bit(35), 0, 0, Slow::hold_frames(0, Slow::STEP_FRAMES, 37, 0)) == 36 &&
        Slow::steps_made(bit(2) | bit(4) | bit(6) | bit(8), 0, 8, Slow::hold_frames(8, Slow::STEP_FRAMES, 15, 0)) == 13,
        "two and four steps on stairs as they were timed: one and two tiles short"
    );

    //  The route's data, all three starters: marks on walking runs only,
    //  inside the run, and every marked run lands on its tile from any of
    //  its steps on.
    struct Counts{
        int runs = 0;
        int steps = 0;
    };
    auto walked = [](RunKind kind){
        return kind == RunKind::WALK || kind == RunKind::CHECKED || kind == RunKind::CHECKED_TRAINER ||
            kind == RunKind::APPROACH || kind == RunKind::BACK;
    };
    bool marks_fine = true;
    bool all_land = true;
    auto count_marks = [&](const StoryScript& script){
        Counts counts;
        for (size_t i = 0; i < script.leg_count; i++){
            const StoryLeg& leg = script.legs[i];
            const StoryRun* runs = script.leg_runs(leg);
            for (uint16_t r = 0; r < leg.run_count; r++){
                const StoryRun& run = runs[r];
                if (run.slow == 0){
                    continue;
                }
                counts.runs++;
                counts.steps += Slow::slow_steps(run.slow, 0, Slow::BITS);
                marks_fine &= walked(run.kind) && run.count <= Slow::BITS &&
                    Slow::slow_steps(run.slow, 0, run.count) == Slow::slow_steps(run.slow, 0, Slow::BITS) &&
                    (run.direction == DPAD_UP || run.direction == DPAD_DOWN);
                all_land &= lands_from_anywhere(run.slow, run.count);
            }
        }
        return counts;
    };
    const Counts bulbasaur = count_marks(AutoStoryData::story_BULBASAUR());
    const Counts squirtle = count_marks(AutoStoryData::story_SQUIRTLE());
    const Counts charmander = count_marks(AutoStoryData::story_CHARMANDER());
    check(
        bulbasaur.runs > 0 && squirtle.runs > 0 && charmander.runs > bulbasaur.runs && marks_fine,
        "the route's data: steps on stairs are marked on runs up and down that are walked, inside the run"
    );
    check(all_land, "the route's data: every run over stairs lands on its tile");

    //  The walk from the Pokemon Center of Seven Island to the grass at the
    //  Canyon Entrance: the run of 19 down, its eighth step onto the stairs
    //  at (8, 14); and the way back, 21 up, the twelfth step off them.
    const StoryScript& script = AutoStoryData::story_CHARMANDER();
    uint64_t to_grass = 0;
    uint64_t to_nurse = 0;
    for (size_t i = 0; i < script.leg_count; i++){
        const StoryLeg& leg = script.legs[i];
        const std::string name = leg.name;
        const StoryRun* runs = script.leg_runs(leg);
        for (uint16_t r = 0; r < leg.run_count; r++){
            if (name == "CollectCanyonEntrance_ToGrass_1" && runs[r].direction == DPAD_DOWN && runs[r].count == 19){
                to_grass = runs[r].slow;
            }
            if (name == "CollectCanyonEntrance_BackToNurse" && runs[r].direction == DPAD_UP && runs[r].count == 21){
                to_nurse = runs[r].slow;
            }
        }
    }
    check(to_grass == bit(7) && to_nurse == bit(11), "the stairs south of Seven Island in the walk to the Canyon Entrance and back");

    logger.log(
        "AutoStory test: rock stairs in the timing of a walk (a step up from them or down onto them takes 23 "
        "frames, not 16: marked in the route's data on " + std::to_string(bulbasaur.runs) + ", " +
        std::to_string(squirtle.runs) + " and " + std::to_string(charmander.runs) +
        " runs for the three starters, " + std::to_string(bulbasaur.steps) + ", " + std::to_string(squirtle.steps) +
        " and " + std::to_string(charmander.steps) + " steps; each of those runs lands on its tile with 7 "
        "frames to spare, from any of its steps on; as it was timed before, a tile short): " +
        std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  The FRLG AutoStory's video tracker (AutoStoryScroll) and the flash of a
//  poisoned Pokemon: every five steps the map is drawn as a mosaic for 10
//  frames, and the player walks on through it (FldEffPoison_Start()). The
//  tracker took that for the end of the map and counted no further: on
//  Bond Bridge a battle six tiles on was counted at the flash, and the
//  walk went on from the wrong tile (16 November 2026). Walks over a
//  made-up map of tiles, drawn as the game would draw them.
//
//  Whose count a walk is, is the other half (AutoStoryScroll::counts_run()):
//  a walk that nothing stopped goes by its timing when the map was lost on
//  the way, found again or not; a walk that a battle stops is counted by
//  the video up to the battle.
//
//  And a battle must stay a battle: after its two gray flashes the picture
//  is cut into lines that slide apart, every second one to the left, slowly
//  at first (B_TRANSITION_SLICE). On the rows the tracker compares that is
//  the map moving on in a run to the right, and builds 28 and 29 found the
//  map again in it (16 November 2026: 33 runs in the grass counted as timed
//  with a battle on the way, up to "308 px" for four tiles).
//
//  And a run that is over stands: on Kindle Road the frames after a run down
//  the grass fit badly and do not tell where the map is. Builds 28 and 29
//  left them out, did not see the player stand, and 0.3 seconds later put
//  the map where the walk would have got to ("64 px (4 tiles)" for three).
int check_poison_flash(Logger& logger){
    using namespace NintendoSwitch::PokemonFRLG;
    using AutoStoryScroll::Screen;
    using AutoStoryScroll::Tracker;
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("AutoStory test: failed: ") + what, COLOR_RED);
        }
    };

    struct Dice{
        uint32_t state;
        int below(int n){
            state = state * 1664525u + 1013904223u;
            return (int)((state >> 8) % (uint32_t)n);
        }
    };
    //  A map of 16 x 16 tiles as brightness values: patches of eight kinds
    //  of tile (paths, trees, water), or one kind only ("plain": ground
    //  that looks the same a tile further). "detail": how much the pixels
    //  of a tile differ. "band": ground that repeats every 8 pixels, as
    //  grass does, with a band of other tiles beside a walk across (1: rows
    //  15 to 17) or a walk up or down (2: columns 20 to 22); "band_seed":
    //  what those tiles are (water at one moment and at another).
    const int TILES_ACROSS = 60;
    const int TILES_DOWN = 40;
    const int MAP_WIDTH = TILES_ACROSS * 16;
    const int MAP_HEIGHT = TILES_DOWN * 16;
    auto make_map = [&](uint32_t seed, bool plain, int detail, int band, uint32_t band_seed){
        Dice dice{seed};
        std::vector<uint8_t> kinds(8 * 256);
        for (int k = 0; k < 8; k++){
            const int spread = (k % 2 == 0 ? 14 : 50) * detail;
            for (int i = 0; i < 256; i++){
                int v = 60 + 22 * k + dice.below(spread) - spread / 2;
                if (k >= 5 && (i % 16 < 2 || i / 16 < 2)){
                    v = 30 + 5 * k;
                }
                kinds[(size_t)(k * 256 + i)] = (uint8_t)std::min(255, std::max(0, v));
            }
        }
        std::vector<int> kind((size_t)(TILES_ACROSS * TILES_DOWN), 0);
        if (band > 0){
            for (int i = 0; i < 256; i++){
                kinds[(size_t)i] = kinds[(size_t)(((i / 16) % 8) * 16 + (i % 16) % 8)];
            }
            Dice tiles{band_seed};
            for (int ty = 0; ty < TILES_DOWN; ty++){
                for (int tx = 0; tx < TILES_ACROSS; tx++){
                    if ((band == 1 && ty >= 15 && ty <= 17) || (band == 2 && tx >= 20 && tx <= 22)){
                        kind[(size_t)(ty * TILES_ACROSS + tx)] = 1 + tiles.below(7);
                    }
                }
            }
        }
        if (!plain){
            for (int n = 0; n < TILES_ACROSS * TILES_DOWN / 6; n++){
                const int k = dice.below(8);
                const int left = dice.below(TILES_ACROSS);
                const int top = dice.below(TILES_DOWN);
                const int right = std::min(TILES_ACROSS, left + 1 + dice.below(5));
                const int bottom = std::min(TILES_DOWN, top + 1 + dice.below(4));
                for (int ty = top; ty < bottom; ty++){
                    for (int tx = left; tx < right; tx++){
                        kind[(size_t)(ty * TILES_ACROSS + tx)] = k;
                    }
                }
            }
        }
        std::vector<uint8_t> map((size_t)(MAP_WIDTH * MAP_HEIGHT));
        for (int py = 0; py < MAP_HEIGHT; py++){
            for (int px = 0; px < MAP_WIDTH; px++){
                const int k = kind[(size_t)((py / 16) * TILES_ACROSS + px / 16)];
                map[(size_t)(py * MAP_WIDTH + px)] = kinds[(size_t)(k * 256 + (py % 16) * 16 + px % 16)];
            }
        }
        return map;
    };
    //  The screen with its top left at (left, top) of the map. "mosaic":
    //  blocks of that many pixels and one more, each showing its top left
    //  pixel (REG_MOSAIC). "gray": blended that far, of 16, towards one gray
    //  (the two flashes that start a battle). "slide": the transition after
    //  them, every second line that many pixels to the left and the lines
    //  between them to the right, black where a line came from
    //  ("slide_left": 0 or 1, which lines go left).
    auto draw = [&](
        Screen& screen, const std::vector<uint8_t>& map, int left, int top,
        int mosaic, int gray, int slide, int slide_left, Dice& noise
    ){
        for (int sy = 0; sy < AutoStoryScroll::SCREEN_HEIGHT; sy++){
            const int moved = slide == 0 ? 0 : ((sy & 1) == slide_left ? slide : -slide);
            for (int sx = 0; sx < AutoStoryScroll::SCREEN_WIDTH; sx++){
                const int from = sx + moved;
                const bool black = from < 0 || from >= AutoStoryScroll::SCREEN_WIDTH;
                const int mx = (((left + from - (mosaic > 0 ? sx % (mosaic + 1) : 0)) % MAP_WIDTH) + MAP_WIDTH) % MAP_WIDTH;
                const int my = (((top + sy - (mosaic > 0 ? sy % (mosaic + 1) : 0)) % MAP_HEIGHT) + MAP_HEIGHT) % MAP_HEIGHT;
                int v = black ? 0 : map[(size_t)(my * MAP_WIDTH + mx)];
                if (gray > 0){
                    v = (v * (16 - gray) + 110 * gray) / 16;
                }
                v += noise.below(3) - 1;
                screen.pixels[(size_t)(sy * AutoStoryScroll::SCREEN_WIDTH + sx)] = (uint8_t)std::min(255, std::max(0, v));
            }
        }
    };

    struct Walk{
        int dir_x = 1;
        int dir_y = 0;
        int tiles = 9;
        int speed = 1;          //  pixels a frame: 1 walking, 2 surfing
        int poison_every = 0;   //  steps between two mosaics (0: none)
        int battle_after = -1;  //  steps; then the battle's flashes, its transition, black, another picture
        int slide_left = 0;     //  which lines of that transition go left (0 or 1)
        int watched = -1;       //  frames the video is looked at after the battle starts (-1: to the end)
        int detail = 3;
        bool plain = false;
        int band = 0;           //  see make_map(); it looks different once the player stands
        bool settle = false;    //  as a run does: the input over, then looked at until the player stands
        uint32_t seed = 1;
    };
    struct Outcome{
        int tiles = 0;          //  as the video counts them
        bool lost = false;
        bool followed = false;  //  the video followed the map to the end, well
        bool believed = false;  //  a walk that nothing stopped takes its count from the video
        size_t flashes = 0;
    };
    auto take = [&](const Walk& walk){
        const std::vector<uint8_t> map = make_map(walk.seed, walk.plain, walk.detail, walk.band, walk.seed * 7 + 1);
        const std::vector<uint8_t> later = make_map(walk.seed, walk.plain, walk.detail, walk.band, walk.seed * 7 + 1001);
        const std::vector<uint8_t> elsewhere = make_map(walk.seed + 77, false, walk.detail, 0, 0);
        Dice noise{walk.seed * 31 + 5};
        Dice jitter{walk.seed + 9};
        Tracker tracker(walk.dir_x, walk.dir_y, 16.0 / (16.0 / walk.speed) / 16.74 * 1.3);
        Screen screen;
        const int step_frames = 16 / walk.speed;
        const int walk_frames = walk.tiles * step_frames;
        const int stop_frame = walk.battle_after >= 0 ? walk.battle_after * step_frames : walk_frames;
        static const int MOSAIC_SIZES[10] = {2, 4, 6, 8, 10, 8, 6, 4, 2, 0};
        double next_look = 0;
        int looks = 0;
        for (int frame = 0; frame < walk_frames + 90; frame++){
            const double time_ms = frame * 16.74;
            if (time_ms + 1e-9 < next_look){
                continue;
            }
            if (walk.settle && frame >= walk_frames + 6){
                //  As a run does: the input is over 6 frames after the last
                //  step, and the video is looked at until the player is seen
                //  to stand, for 0.7 seconds at most.
                tracker.input_over();
                if ((!tracker.lost() && looks >= 3 && tracker.still_ms() >= 150) || frame > walk_frames + 6 + 42){
                    break;
                }
            }
            looks++;
            if (walk.battle_after >= 0 && walk.watched >= 0 && frame > stop_frame + walk.watched){
                //  (The input is over: the run looks for the battle itself now.)
                break;
            }
            //  (The tracker looks every 45 ms or so, not at every frame.)
            next_look = time_ms + 40 + jitter.below(11);
            const int pixels = std::min(frame, stop_frame) * walk.speed;
            int mosaic = 0;
            const int step = frame / step_frames;
            const int into_step = frame - step * step_frames;
            if (walk.poison_every > 0 && frame < stop_frame + 10 && step > 0 && step <= walk.tiles &&
                step % walk.poison_every == 0 && into_step < 10 && (walk.battle_after < 0 || frame < stop_frame)
            ){
                mosaic = MOSAIC_SIZES[into_step];
            }
            //  A battle: two fades to gray and back (32 frames), the lines
            //  sliding apart until nothing is left (Slice_Main(): the speed
            //  in 1/256 pixels a frame), black, and the battle's picture.
            int gray = 0;
            int slide = 0;
            bool black = false;
            bool battle_picture = false;
            if (walk.battle_after >= 0 && frame >= stop_frame){
                const int into = frame - stop_frame;
                if (into < 32){
                    const int phase = into % 16;
                    gray = phase < 8 ? (phase + 1) * 2 : (16 - phase) * 2 - 2;
                }else{
                    int rate = 1 << 8;
                    int gain = 1;
                    int gone = 0;
                    for (int n = 0; n <= into - 32 && gone < AutoStoryScroll::SCREEN_WIDTH; n++){
                        gone = std::min(gone + (rate >> 8), (int)AutoStoryScroll::SCREEN_WIDTH);
                        if (rate <= 0xfff){
                            rate += gain;
                        }
                        if (gain < 128){
                            gain <<= 1;
                        }
                    }
                    slide = gone < AutoStoryScroll::SCREEN_WIDTH ? gone : 0;
                    black = gone >= AutoStoryScroll::SCREEN_WIDTH && into < 32 + 37 + 40;
                    battle_picture = gone >= AutoStoryScroll::SCREEN_WIDTH && !black;
                }
            }
            if (battle_picture){
                draw(screen, elsewhere, 300 + (frame - stop_frame) * 3, 200, 0, 0, 0, 0, noise);
            }else if (black){
                for (uint8_t& pixel : screen.pixels){
                    pixel = (uint8_t)noise.below(3);
                }
            }else{
                draw(
                    screen, walk.band > 0 && frame >= walk_frames ? later : map,
                    320 + walk.dir_x * pixels, 240 + walk.dir_y * pixels,
                    mosaic, gray, slide, walk.slide_left, noise
                );
            }
            tracker.add(screen, time_ms);
        }
        Outcome outcome;
        outcome.lost = tracker.lost();
        //  (attempt_moves(): a tile from 10 px on when the screen changed.)
        outcome.tiles = outcome.lost ? (tracker.pixels() + 6) / 16 : tracker.tiles();
        outcome.followed = !tracker.lost() && tracker.reliable();
        outcome.believed = AutoStoryScroll::counts_run(tracker.lost(), tracker.reliable(), tracker.flashes());
        outcome.flashes = tracker.flashes();
        return outcome;
    };

    static const int WAYS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int walks = 0;
    int plain_walks_right = 0;
    int followed = 0;
    int surfing_followed = 0;
    size_t found_again = 0;
    bool none_wrong = true;         //  no wrong count where the video followed to the end
    bool timed_after_flash = true;  //  lost on the way, nothing stopped the walk: not the video's count
    bool last_step_right = true;
    int battles = 0;
    int battles_lost = 0;
    int battles_right = 0;
    int late_battles = 0;
    int late_battles_right = 0;
    int stands_right = 0;
    for (uint32_t seed = 1; seed <= 2; seed++){
        for (int way = 0; way < 4; way++){
            Walk walk;
            walk.seed = seed;
            walk.dir_x = WAYS[way][0];
            walk.dir_y = WAYS[way][1];
            walks++;
            //  Nothing on the way.
            Outcome outcome = take(walk);
            plain_walks_right += outcome.believed && outcome.tiles == 9 ? 1 : 0;
            //  The mosaic after steps 5 and 10 of 14, walking and surfing.
            walk.tiles = 14;
            walk.poison_every = 5;
            outcome = take(walk);
            followed += outcome.followed && outcome.tiles == 14 ? 1 : 0;
            found_again += outcome.flashes;
            none_wrong &= !outcome.followed || outcome.tiles == 14;
            timed_after_flash &= outcome.flashes == 0 || !outcome.believed;
            walk.speed = 2;
            outcome = take(walk);
            surfing_followed += outcome.followed && outcome.tiles == 14 ? 1 : 0;
            none_wrong &= !outcome.followed || outcome.tiles == 14;
            timed_after_flash &= outcome.flashes == 0 || !outcome.believed;
            walk.speed = 1;
            //  The mosaic after the last step: the player stands.
            walk.tiles = 5;
            outcome = take(walk);
            last_step_right &= (!outcome.followed && !outcome.lost) || outcome.tiles == 5;
            //  Battles: with either half of the lines sliding left, over
            //  maps of little and of much detail, looked at to the end and
            //  for 0.7 and 0.9 seconds (a run of four tiles with the battle
            //  at its first step: the input goes on about that long).
            walk.tiles = 14;
            for (int detail : {1, 3}){
                for (int slide_left = 0; slide_left < 2; slide_left++){
                    for (int watched : {-1, 44, 56}){
                        walk.detail = detail;
                        walk.slide_left = slide_left;
                        walk.watched = watched;
                        //  A battle after 3 steps of 14.
                        walk.poison_every = 0;
                        walk.battle_after = 3;
                        outcome = take(walk);
                        battles++;
                        battles_lost += outcome.lost ? 1 : 0;
                        battles_right += outcome.lost && outcome.tiles == 3 ? 1 : 0;
                        //  The mosaic after step 5, a battle after step 9 (Bond Bridge).
                        walk.poison_every = 5;
                        walk.battle_after = 9;
                        outcome = take(walk);
                        battles++;
                        late_battles++;
                        battles_lost += outcome.lost ? 1 : 0;
                        late_battles_right += outcome.lost && outcome.tiles == 9 ? 1 : 0;
                    }
                }
            }
            walk.detail = 3;
            walk.slide_left = 0;
            walk.watched = -1;
            walk.battle_after = -1;
            walk.poison_every = 0;
            //  Three tiles over ground that repeats, beside water that looks
            //  different once the player stands (Kindle Road).
            walk.tiles = 3;
            walk.plain = true;
            walk.band = way < 2 ? 1 : 2;
            walk.settle = true;
            outcome = take(walk);
            stands_right += !outcome.lost && outcome.tiles == 3 ? 1 : 0;
            walk.band = 0;
            walk.settle = false;
            //  Ground that looks the same a tile further, with the mosaic.
            walk.tiles = 14;
            walk.poison_every = 5;
            walk.plain = true;
            outcome = take(walk);
            none_wrong &= !outcome.followed || outcome.tiles == 14;
        }
    }
    check(plain_walks_right == walks, "a walk of 9 tiles with nothing on the way is counted by the video");
    check(followed >= walks - 1 && found_again >= (size_t)walks, "the mosaic on the way: the walk is followed to its end");
    check(none_wrong, "the mosaic on the way, walking, surfing and over plain ground: no wrong count where the video followed to the end");
    check(
        timed_after_flash &&
        AutoStoryScroll::counts_run(false, true, 0) && !AutoStoryScroll::counts_run(false, true, 1) &&
        !AutoStoryScroll::counts_run(true, true, 0) && !AutoStoryScroll::counts_run(false, false, 0),
        "a walk that nothing stopped is counted by the video only when the map was followed all the way"
    );
    check(last_step_right, "the mosaic after the last step, the player standing: the count is right");
    check(
        battles_lost == battles && battles_right >= battles - late_battles - 1,
        "a battle on the way: the screen is lost at its step and stays lost through the transition"
    );
    check(late_battles_right >= late_battles - 2, "the mosaic, then a battle four steps on: counted at the battle");
    check(
        stands_right == walks,
        "the input over and frames that do not tell where the map is: the player stands, the count stays"
    );
    {
        //  A fade is known by its contrast, the mosaic keeps it.
        const std::vector<uint8_t> map = make_map(3, false, 3, 0, 0);
        Dice noise{17};
        Screen plain_picture, mosaic_picture, gray_picture;
        draw(plain_picture, map, 320, 240, 0, 0, 0, 0, noise);
        draw(mosaic_picture, map, 320, 240, 10, 0, 0, 0, noise);
        draw(gray_picture, map, 320, 240, 0, 10, 0, 0, noise);
        const double before = AutoStoryScroll::screen_spread(plain_picture);
        check(
            before > 10 &&
            AutoStoryScroll::screen_spread(mosaic_picture) > 0.8 * before &&
            AutoStoryScroll::screen_spread(gray_picture) < 0.5 * before,
            "a fade to gray takes the picture's contrast, the mosaic leaves it"
        );
    }

    logger.log(
        "AutoStory test: the video tracker and the flash of a poisoned Pokemon (the map as a mosaic for 10 "
        "frames every five steps, the player walking on): of " + std::to_string(walks) + " walks with two "
        "flashes on the way " + std::to_string(followed) + " followed to the end (the map found again " +
        std::to_string(found_again) + " times), surfing " + std::to_string(surfing_followed) +
        "; of " + std::to_string(battles) + " battles on the way, with the lines of their transition sliding "
        "either way, " + std::to_string(battles_lost) + " lost for good and " +
        std::to_string(battles_right + late_battles_right) + " counted at their step; of " +
        std::to_string(walks) + " runs ending where the picture does not tell, " + std::to_string(stands_right) +
        " seen to stand at their tile; the flash after the last step, plain ground; a walk that nothing stopped "
        "goes by its timing when the map was lost on the way): " +
        std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}



//  Bait, rocks and balls in the Safari Zone: Pokemon Automation's table of
//  Safari actions, which the AutoStory follows for a species it wants,
//  against the game's own rules (PokemonFRLG_AutoStorySafari.h). The files
//  are read here as SafariOptimalAction reads them, from the resources this
//  build is packaged with.
int check_safari_table(Logger& logger){
    using namespace NintendoSwitch;
    using namespace NintendoSwitch::PokemonFRLG;
    namespace Safari = AutoStorySafari;
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const std::string& what){
        checks++;
        if (!fine){
            wrong++;
            logger.log("AutoStory test: failed: " + what, COLOR_RED);
        }
    };

    //  The game's numbers.
    auto about = [](double value, double wanted){ return std::abs(value - wanted) < 0.0006; };
    check(
        Safari::catch_factor(30) == 2 && Safari::catch_factor(45) == 3 && Safari::catch_factor(255) == 20 &&
        Safari::escape_factor(125) == 9 && Safari::escape_factor(25) == 2,
        "the catch factor and the escape factor of a species"
    );
    check(
        about(Safari::ball_chance(2), 0.0490) && about(Safari::ball_chance(3), 0.0809) &&
        about(Safari::ball_chance(20), 0.5028),
        "what a SAFARI BALL catches at full HP"
    );
    check(
        about(Safari::flee_chance(9, 0, 0), 0.45) && about(Safari::flee_chance(9, 3, 0), 0.10) &&
        about(Safari::flee_chance(9, 0, 3), 0.90) && about(Safari::flee_chance(2, 3, 0), 0.05),
        "how often a wild Pokemon flees: as it is, eating, angry"
    );
    check(
        Safari::table_name("nidoran-f") == "nidoran" && Safari::table_name("nidoran-m") == "nidoran" &&
        Safari::table_name("nidorina") == "nidorina" && Safari::table_name("chansey") == "chansey",
        "the two NIDORANs are one in the table"
    );
    {
        //  One turn, by hand: a ball at a CHANSEY with one ball left, and
        //  bait before it (it stays 55 times in 100, and the ball is then
        //  thrown at catch factor 3).
        const double ball = Safari::catch_odds(30, 125, {}, 1);
        const double bait_first = Safari::catch_odds(30, 125, {Safari::Action::BAIT}, 1);
        check(
            about(ball, Safari::ball_chance(2)) && about(bait_first, 0.55 * Safari::ball_chance(3)),
            "the odds of one ball, and of bait and one ball"
        );
    }

    //  The table.
    std::set<std::string> known;
    for (const Safari::Species& one : Safari::species()){
        known.insert(one.name);
    }
    check(known == SAFARI_ZONE_POKEMON_SUBSET, "the species of the table are the ones whose rates are known here");

    const std::string folder = RESOURCE_PATH() + "PokemonFRLG/SafariOptimalAction/International/";
    size_t files = 0;
    size_t cases = 0;
    size_t worse = 0;
    size_t odd_rows = 0;
    double chansey_table = 0;
    double chansey_balls = 0;
    double least_gain_45 = 1;
    bool chansey_bait_first = false;
    bool nidoran_balls_only = false;
    std::string problem;
    for (const Safari::Species& one : Safari::species()){
        const std::string path = folder + one.name + ".json";
        std::vector<std::vector<Safari::Action>> table;
        try{
            JsonValue json = load_json_file(path);
            for (const JsonValue& row_value : json.to_array_throw(path)){
                std::vector<Safari::Action>& row = table.emplace_back();
                for (const JsonValue& action_value : row_value.to_array_throw(path)){
                    const int64_t action = action_value.to_integer_throw(path);
                    if (action < 0 || action > 2){
                        odd_rows++;
                    }
                    row.emplace_back(
                        action == 1 ? Safari::Action::BAIT : (action == 2 ? Safari::Action::ROCK : Safari::Action::BALL)
                    );
                }
            }
        }catch (const Exception& e){
            problem = e.to_str();
            continue;
        }
        if ((int)table.size() != Safari::BALLS_A_GAME){
            problem = path + ": " + std::to_string(table.size()) + " rows";
            continue;
        }
        files++;
        for (int balls = 1; balls <= Safari::BALLS_A_GAME; balls++){
            const std::vector<Safari::Action>& row = table[(size_t)balls - 1];
            const double with_table = Safari::catch_odds(one.catch_rate, one.flee_rate, row, balls);
            const double balls_alone = Safari::catch_odds(one.catch_rate, one.flee_rate, {}, balls);
            cases++;
            if (with_table < balls_alone - 1e-9){
                worse++;
            }
            if (balls != Safari::BALLS_A_GAME){
                continue;
            }
            const std::string name = one.name;
            if (name == "chansey"){
                chansey_table = with_table;
                chansey_balls = balls_alone;
                chansey_bait_first = row.size() > 2 && row[0] == Safari::Action::BAIT && row[1] == Safari::Action::BAIT;
            }
            if (name == "nidoran"){
                nidoran_balls_only = std::count(row.begin(), row.end(), Safari::Action::BAIT) == 0;
            }
            if (one.catch_rate == 45 && one.flee_rate == 125){
                least_gain_45 = std::min(least_gain_45, with_table - balls_alone);
            }
        }
    }
    check(
        files == Safari::species().size() && odd_rows == 0,
        "Pokemon Automation's table of Safari actions: a file for each species, 30 rows each, balls, bait and "
        "rocks" + (problem.empty() ? std::string() : " (" + problem + ")")
    );
    check(
        files > 0 && worse == 0,
        "by the game's rules the table is never worse than balls alone (" + std::to_string(worse) + " of " +
        std::to_string(cases) + " cases)"
    );
    check(
        chansey_bait_first && chansey_table > 0.185 && chansey_balls < 0.105 && chansey_table > 1.8 * chansey_balls,
        "a CHANSEY with 30 balls: bait first, and caught nearly twice as often as with balls alone"
    );
    check(
        least_gain_45 > 0.02 && least_gain_45 < 0.5,
        "the species with catch rate 45 that flee as fast (KANGASKHAN, TAUROS, SCYTHER, PINSIR, DRAGONAIR): "
        "caught more often with the table"
    );
    check(nidoran_balls_only, "a NIDORAN: balls only, as before");

    logger.log(
        "AutoStory test: bait, rocks and balls in the Safari Zone (Pokemon Automation's table of Safari actions "
        "by the game's own rules: " + std::to_string(files) + " species, 1 to 30 SAFARI BALLs left, worse than "
        "balls alone in " + std::to_string(worse) + " of " + std::to_string(cases) + " cases; with 30 balls a "
        "CHANSEY is caught " + std::to_string((int)(chansey_table * 1000 + 0.5)) + " times in 1000, with balls "
        "alone " + std::to_string((int)(chansey_balls * 1000 + 0.5)) + " times): " +
        std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  The next steps of the POKeDEX collection (experimental): fishing, laps
//  over the water and from rock to rock, Pokemon that are given. What a
//  cast of a rod says (PokemonFRLG_AutoStoryFishing.h), and the story's
//  data as the program plays it: the places fished at, the laps and their
//  steps, the ways to the rods, the scenes of the gifts. And the segments
//  before them, step for step where they were: a save made in one of them
//  with an earlier build goes on where it was.
int check_next_steps(Logger& logger){
    using namespace NintendoSwitch;
    using namespace NintendoSwitch::PokemonFRLG;
    namespace Fishing = AutoStoryFishing;
    using Fishing::Cast;
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const std::string& what){
        checks++;
        if (!fine){
            wrong++;
            logger.log("AutoStory test: failed: " + what, COLOR_RED);
        }
    };

    //  The rods: their descriptions in the bag, as the program reads them.
    const std::string old_rod = "an old and beat-up fishing rod. use it by any body of water to fish for wild pok\xc3\xa9mon.";
    const std::string good_rod = "a new, good-quality fishing rod. use it by any body of water to fish for wild pok\xc3\xa9mon.";
    const std::string super_rod = "an awesome, high-tech fishing rod. use it by any body of water to fish for wild pok\xc3\xa9mon.";
    const std::string rods[3] = {old_rod, good_rod, super_rod};
    check(
        Fishing::is_rod(0) && Fishing::is_rod(2) && !Fishing::is_rod(-1) && !Fishing::is_rod(3) &&
        std::string(Fishing::rod_name(1)) == "GOOD ROD" && std::string(Fishing::rod_name(2)) == "SUPER ROD",
        "the rods by their numbers"
    );
    {
        bool own = true;
        bool others = true;
        for (int rod = 0; rod < Fishing::ROD_COUNT; rod++){
            for (int text = 0; text < Fishing::ROD_COUNT; text++){
                const bool match = Fishing::is_rod_description(rod, rods[text]);
                (rod == text ? own : others) &= rod == text ? match : !match;
            }
        }
        check(own, "each rod is found by its own description");
        check(others, "no rod is taken for another");
    }
    check(
        Fishing::is_rod_description(1, "a new, good quality fishing rod. use it by any body of water") &&
        Fishing::is_rod_description(2, "an awesome, high tech fishing rod.") &&
        Fishing::is_rod_description(0, "an old and beat up fishing rod."),
        "a rod's description with its hyphen read as a space"
    );
    check(
        !Fishing::is_rod_description(1, "a device that indicates trainers who want to battle. the battery charges while traveling.") &&
        !Fishing::is_rod_description(2, "a folding bicycle that is faster than the running shoes.") &&
        !Fishing::is_rod_description(1, "") && !Fishing::is_rod_description(7, good_rod),
        "other key items are no rod"
    );

    //  A cast.
    check(
        Fishing::read_cast("not even a nibble\xe2\x80\xa5") == Cast::NO_BITE &&
        Fishing::read_cast("not even a nibble..") == Cast::NO_BITE &&
        Fishing::read_cast("it got away..") == Cast::NO_BITE,
        "a cast without a bite"
    );
    check(
        Fishing::read_cast("a pok\xc3\xa9mon's on the hook!") == Cast::BITE &&
        Fishing::read_cast("a pokemon s on the hook") == Cast::BITE,
        "a bite"
    );
    //  The bite's text while it still prints (17 November: read as another
    //  text 30 times, with a B on it): the beginning of it is waited for.
    check(
        Fishing::bite_printing("a pokemon") && Fishing::bite_printing("a pokamon's on the h") &&
        Fishing::bite_printing("a pok\xc3\xa9mon's on t") && Fishing::bite_printing("a pokemon\"") &&
        Fishing::bite_printing("a pokemon's on the hoo") &&
        !Fishing::bite_printing("a pok") && !Fishing::bite_printing("a pokedex") &&
        !Fishing::bite_printing("a pokemon center") && !Fishing::bite_printing("") &&
        !Fishing::bite_printing("repel's effect wore off...") &&
        Fishing::read_cast("a pokemon's on") == Cast::NOTHING_YET &&
        Fishing::read_cast("a pokamon's on the ho") == Cast::NOTHING_YET &&
        Fishing::read_cast("a pokemon's on the hook!") == Cast::BITE,
        "the bite's text while it still prints is waited for"
    );
    check(
        Fishing::read_cast("") == Cast::NOTHING_YET && Fishing::read_cast(". . . .") == Cast::NOTHING_YET &&
        Fishing::read_cast("\xc2\xb7 \xc2\xb7 \xc2\xb7 e") == Cast::NOTHING_YET &&
        Fishing::read_cast(super_rod) == Cast::NOTHING_YET,
        "the dots of a cast, and the bag still closing"
    );
    check(
        Fishing::read_cast("oak: red! there's a time and place for everything! but not now.") == Cast::NOT_HERE &&
        Fishing::read_cast("repel's effect wore off...") == Cast::OTHER,
        "where no rod can be cast, and a text that is not of fishing"
    );
    check(
        !Fishing::no_bite_at_all(0) && !Fishing::no_bite_at_all(Fishing::CASTS_WITHOUT_A_BITE - 1) &&
        Fishing::no_bite_at_all(Fishing::CASTS_WITHOUT_A_BITE) && Fishing::CASTS_WITHOUT_A_BITE >= 18,
        "casts in a row without a bite"
    );

    //  A fishing place's list, and which species it is worth a wait for.
    {
        const char* vermilion_good_rod = "horsea:60:20,krabby:20:60,magikarp:20:20";
        const char* five_island = "gyarados:15:15,horsea:40:0,kingler:0:4,seadra:4:0,shellder:40:0,staryu:0:40";
        const std::map<std::string, int> fire_red = story_place_species(five_island, 0);
        const std::map<std::string, int> leaf_green = story_place_species(five_island, 1);
        check(
            story_place_species(vermilion_good_rod, 0).size() == 3 && fire_red.size() == 4 && leaf_green.size() == 3 &&
            fire_red.count("seadra") != 0 && leaf_green.count("seadra") == 0 && leaf_green.count("kingler") != 0,
            "a fishing place's list in each game"
        );
        //  SEADRA: 4 in 100 where it bites at all (waited for); a PSYDUCK
        //  that bites 5 in 100 and is 3 in 10 in the grass elsewhere: not.
        check(story_place_worth(4, 4) && !story_place_worth(5, 30) && story_place_worth(15, 15), "a rare one that is rare everywhere is waited for");
    }

    //  The story's data.
    const StoryScript& script = AutoStoryData::story_CHARMANDER();
    auto leg_faces = [&](const StoryStep& walk, DpadPosition direction){
        if (walk.type != StepType::WALK || walk.leg < 0 || (size_t)walk.leg >= script.leg_count){
            return false;
        }
        const StoryLeg& leg = script.leg(walk.leg);
        return leg.end == LegEnd::INTERACT && leg.face == direction;
    };
    int fishing_places = 0;
    bool fishing_fine = true;
    int lap_places = 0;
    int lap_steps = 0;
    bool laps_fine = true;
    int rod_ways = 0;
    bool rod_ways_fine = true;
    int gifts = 0;
    bool gifts_fine = true;
    int patient_talks = 0;
    bool patient_fine = true;
    int woken = 0;
    std::string gift_names;
    for (size_t s = 0; s < script.segment_count; s++){
        const StorySegment& segment = script.segments[s];
        for (size_t k = 0; k < segment.step_count; k++){
            const StoryStep& step = script.step(segment, k);
            if (step.type == StepType::DEX && step.p2 > 0 && (step.p2 & 4) != 0){
                //  Fishing: for the table, a rod the story fetches, the
                //  walk before it ending with the player facing the water.
                fishing_places++;
                fishing_fine &= step.p2 == 6 && (step.p4 == 1 || step.p4 == 2) && step.dir != DPAD_NONE &&
                    !story_place_species(step.text, 0).empty() && step.p0 > 0 && step.p1 > 0 &&
                    k > 0 && leg_faces(script.step(segment, k - 1), step.dir);
            }
            if (step.type == StepType::DEX && step.p2 > 0 && (step.p2 & 8) != 0){
                //  Laps: for the table, their steps in the same segment,
                //  walks and YES answers (SURF, ROCK SMASH) and waits only,
                //  and a step after them (the way back).
                lap_places++;
                const size_t count = (size_t)std::max(0, (int)step.p4);
                bool fine = step.p2 == 10 && count > 0 && k + count + 1 < segment.step_count &&
                    !story_place_species(step.text, 0).empty() && step.p0 > 0 && step.p1 > 0;
                bool walks = false;
                for (size_t n = 1; fine && n <= count; n++){
                    const StoryStep& lap = script.step(segment, k + n);
                    fine &= lap.type == StepType::WALK || lap.type == StepType::TALK_YES || lap.type == StepType::WAIT;
                    walks |= lap.type == StepType::WALK;
                }
                laps_fine &= fine && walks;
                lap_steps += (int)count;
            }
            if (step.type == StepType::SKIP_IF_ITEM){
                //  The way to a rod: skipped when nothing is fished for with
                //  it (the step before), and when the bag has it. Both skip
                //  to the segment's end; the one who gives it may have given
                //  it before.
                rod_ways++;
                bool fine = k == 1 && step.p2 >= 2 && step.p2 <= 3 && step.p1 == 1 &&
                    k + 1 + (size_t)std::max(0, (int)step.p0) == segment.step_count;
                if (fine){
                    const StoryStep& before = script.step(segment, 0);
                    fine = before.type == StepType::SKIP_IF_COLLECTED && before.p1 == step.p2 && before.p0 == step.p0 + 1;
                }
                bool asked = false;
                bool places = false;
                for (size_t n = k + 1; n < segment.step_count; n++){
                    const StoryStep& later = script.step(segment, n);
                    asked |= later.type == StepType::TALK_YES && later.p0 > 0 && (later.p0 & 1) != 0;
                }
                for (size_t n = 0; n < script.step_count; n++){
                    const StoryStep& place = script.steps[n];
                    places |= place.type == StepType::DEX && place.p2 > 0 && (place.p2 & 4) != 0 && place.p4 == step.p2 - 1;
                }
                rod_ways_fine &= fine && asked && places;
            }
            if (step.type == StepType::GOT){
                //  A gift: counted after the scene that hands it over, in a
                //  segment that is skipped as a whole once the table has it.
                gifts++;
                gift_names += (gift_names.empty() ? "" : ", ") + std::string(step.text);
                const StoryStep& first = script.step(segment, 0);
                gifts_fine &= k > 0 && script.step(segment, k - 1).type == StepType::CUTSCENE &&
                    script.step(segment, k - 1).p4 == 1 &&
                    first.type == StepType::SKIP_IF_OWNED && std::string(first.text) == step.text && first.p1 == 1 &&
                    1 + (size_t)std::max(0, (int)first.p0) == segment.step_count;
            }
            if ((step.type == StepType::TALK || step.type == StepType::TALK_YES) && step.p1 > 0){
                //  Someone who walks around: A where he comes by.
                patient_talks++;
                patient_fine &= step.p1 >= 60 && step.p2 != 1 && k > 0 &&
                    script.step(segment, k - 1).type == StepType::WALK &&
                    script.leg(script.step(segment, k - 1).leg).end == LegEnd::INTERACT;
            }
            if (step.type == StepType::BATTLE && step.p1 == 2 && step.p2 > 0 && (step.p2 & 4) != 0 && k >= 2 &&
                script.step(segment, k - 1).type == StepType::CUTSCENE &&
                script.step(segment, k - 2).type == StepType::TALK_YES &&
                script.step(segment, 0).type == StepType::SKIP_IF_OWNED
            ){
                //  One that is woken with a YES and may be left asleep.
                woken++;
            }
        }
    }
    check(fishing_places == 12 && fishing_fine, "the places fished at, in the story's data");
    check(lap_places == 4 && laps_fine, "the laps and their steps, in the story's data");
    check(rod_ways == 2 && rod_ways_fine, "the ways to the fishing rods");
    check(gifts == 3 && gifts_fine, "the Pokemon that are given, and their scenes");
    check(patient_talks == 4 && patient_fine, "the talks with someone who walks around");
    check(woken == 1, "the SNORLAX that may be left asleep");

    //  The towns before the next steps: as many steps as in build 31 (a
    //  save in one of them is at the same step).
    {
        struct Town{
            const char* id;
            size_t steps;
        };
        const Town towns[] = {
            {"C20_01_START", 1}, {"C20_01B_MONEY", 85}, {"C20_02_CERULEAN", 144}, {"C20_03_MT_MOON", 50},
            {"C20_04_VIRIDIAN", 84}, {"C20_05_VICTORY_ROAD", 26}, {"C20_06_ROUTE10", 66}, {"C20_07_LAVENDER", 136},
            {"C20_08_CELADON", 28}, {"C20_09_FUCHSIA", 296}, {"C20_10_CINNABAR", 74}, {"C20_11_VERMILION_SEVII", 628},
        };
        bool same = true;
        size_t found = 0;
        size_t last_town = 0;
        size_t next_steps = 0;
        for (size_t s = 0; s < script.segment_count; s++){
            const std::string id = script.segments[s].id;
            for (const Town& town : towns){
                if (id == town.id){
                    found++;
                    last_town = s;
                    same &= script.segments[s].step_count == town.steps;
                }
            }
        }
        for (size_t s = last_town + 1; s < script.segment_count; s++){
            next_steps += std::string(script.segments[s].id).rfind("C20_", 0) == 0 ? 1 : 0;
        }
        check(found == std::size(towns) && same, "the towns of the collection have the steps they had in build 31");
        check(next_steps == 14 && last_town + 1 + next_steps == script.segment_count, "the next steps come after the towns");
    }

    logger.log(
        "AutoStory test: the next steps of the collection (what a cast of a rod says; in the story's data " +
        std::to_string(fishing_places) + " visits of places fished at, " + std::to_string(lap_places) +
        " of laps with " + std::to_string(lap_steps) + " steps, " + std::to_string(rod_ways) + " ways to a rod, " +
        std::to_string(gifts) + " gifts (" + gift_names + "), " + std::to_string(woken) + " woken with the POKe FLUTE): " +
        std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}

//  A place of the collection whose visit fails twice in a row is left out
//  for the run, and so is the way to a fishing rod (build 33): the place of
//  a check, the count of the failures, and in the story's data which checks
//  are one trip from a nurse and back.
int check_leave_out(Logger& logger){
    using namespace NintendoSwitch;
    using namespace NintendoSwitch::PokemonFRLG;
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const std::string& what){
        checks++;
        if (!fine){
            wrong++;
            logger.log("AutoStory test: failed: " + what, COLOR_RED);
        }
    };

    check(
        story_trip_place("FishViridian?") == "FishViridian" &&
        story_trip_place("FishViridian_Visit2?") == "FishViridian" &&
        story_trip_place("CollectCeruleanCaveB1F_Visit3?") == "CollectCeruleanCaveB1F" &&
        story_trip_place("CollectSafariCenter2?") == "CollectSafariCenter2" &&
        story_trip_place("CollectRoute3") == "CollectRoute3",
        "the place of a trip: its check without the question mark and the number of the visit"
    );
    check(
        story_trip_place("_Visit2?") == "_Visit2" && story_trip_place("Route_Visit?") == "Route_Visit" &&
        story_trip_place("Route_Visitor?") == "Route_Visitor" && story_trip_place("?").empty() &&
        story_trip_place("").empty(),
        "the place of a trip: labels that only look like a visit's"
    );
    check(
        STORY_PLACE_FAILURES == 2 && !story_place_fails_out(0) && !story_place_fails_out(1) &&
        story_place_fails_out(2) && story_place_fails_out(3) &&
        !story_fishing_off(0) && !story_fishing_off(1) && story_fishing_off(2),
        "left out after two failures in a row; fishing after two of its places"
    );
    {
        StoryPlaceFailures places;
        const bool first = places.failed("FishViridian", true);
        const bool out_after_one = places.place_out("FishViridian");
        const bool second = places.failed("FishViridian", true);
        check(
            !first && !out_after_one && second && places.place_out("FishViridian") &&
            places.failures("FishViridian") == 2,
            "a place is left out at its second failure in a row, not at the first"
        );
        check(
            !places.place_out("FishVermilion") && !places.fishing_out() && places.fishing_left_out == 1,
            "one fishing place left out leaves the other places in"
        );
        places.failed("FishFuchsia", true);
        const bool fourth = places.failed("FishFuchsia", true);
        check(
            fourth && places.fishing_out() && !places.place_out("FishVermilion") &&
            !places.place_out("SurfFiveIsland"),
            "two fishing places left out: fishing is off, and the places themselves are not marked"
        );
    }
    {
        StoryPlaceFailures places;
        places.failed("CollectRoute3", false);
        places.went_well("CollectRoute3");
        const bool again = places.failed("CollectRoute3", false);
        check(
            !again && !places.place_out("CollectRoute3") && places.failures("CollectRoute3") == 1,
            "a visit that goes well in between: the failures are not in a row"
        );
        places.failed("CollectRoute3", false);
        places.failed("the way to the GOOD ROD", false);
        const bool way = places.failed("the way to the GOOD ROD", false);
        check(
            places.place_out("CollectRoute3") && way && places.place_out("the way to the GOOD ROD") &&
            !places.fishing_out() && places.fishing_left_out == 0,
            "places that are not fished at (and the way to a rod) do not turn fishing off"
        );
        const bool once_more = places.failed("CollectRoute3", false);
        check(!once_more && places.left_out.size() == 2, "a place is left out once");
        //  All that build 32 added can be left out; more than that stops
        //  the program.
        for (int k = 0; k < STORY_MOST_PLACES_LEFT_OUT - 2; k++){
            const std::string place = "Place" + std::to_string(k);
            places.failed(place, false);
            places.failed(place, false);
        }
        const bool at_most = !places.too_many() && places.left_out.size() == (size_t)STORY_MOST_PLACES_LEFT_OUT;
        places.failed("OneMore", false);
        places.failed("OneMore", false);
        check(
            STORY_MOST_PLACES_LEFT_OUT >= 2 + 2 + 2 + 4 && at_most && places.too_many(),
            "more places left out than all the new ones together: the program stops"
        );
    }

    //  The story's data: the checks that are one trip, and the others.
    const StoryScript& script = AutoStoryData::story_CHARMANDER();
    size_t trips = 0;
    size_t fishing_trips = 0;
    size_t lap_trips = 0;
    size_t at_a_nurse = 0;
    size_t trips_not_named = 0;
    size_t others = 0;
    size_t others_named = 0;
    size_t rod_ways = 0;
    size_t rod_ways_in_place = 0;
    std::set<std::string> places;
    for (size_t s = 0; s < script.segment_count; s++){
        const StorySegment& segment = script.segments[s];
        for (size_t i = 0; i < segment.step_count; i++){
            const StoryStep& step = script.step(segment, i);
            if (step.type == StepType::SKIP_IF_ITEM){
                if (story_rod_way(script, segment, i)){
                    rod_ways++;
                    //  The way is the whole segment: the check of the rod's
                    //  places, the look into the bag, the way, the save.
                    const bool whole = i == 1 && i + (size_t)step.p0 + 1 == segment.step_count &&
                        script.step(segment, 0).type == StepType::SKIP_IF_COLLECTED &&
                        script.step(segment, 0).p1 == step.p2;
                    rod_ways_in_place += whole ? 1 : 0;
                }
                continue;
            }
            if (step.type != StepType::SKIP_IF_COLLECTED || step.p1 > 0){
                continue;
            }
            const std::string label = step.label;
            const bool named =
                label.rfind("Collect", 0) == 0 || label.rfind("Fish", 0) == 0 ||
                label.rfind("Surf", 0) == 0 || label.rfind("Smash", 0) == 0;
            if (!story_plain_trip(script, segment, i)){
                others++;
                others_named += named ? 1 : 0;
                continue;
            }
            trips++;
            trips_not_named += named ? 0 : 1;
            places.insert(story_trip_place(label));
            for (size_t k = i + 1; k <= i + (size_t)step.p0; k++){
                const StoryStep& in = script.step(segment, k);
                if (in.type == StepType::DEX && in.p2 > 0 && (in.p2 & 4) != 0){
                    fishing_trips++;
                }
                if (in.type == StepType::DEX && in.p2 > 0 && (in.p2 & 8) != 0){
                    lap_trips++;
                }
            }
            //  The trip starts where it ends: at the nurse (the step before
            //  is the nurse, or the save of the trip before).
            const StepType before = i > 0 ? script.step(segment, i - 1).type : StepType::WALK;
            at_a_nurse += before == StepType::HEAL || before == StepType::SAVE ? 1 : 0;
        }
    }
    check(
        trips == 105 && trips_not_named == 0 && others_named == 0,
        "every visit of a place of the collection is one trip (105), and nothing else is"
    );
    check(at_a_nurse == trips, "every trip starts at the nurse it ends at");
    check(fishing_trips == 12 && lap_trips == 4, "the trips to fish (12) and to walk laps (4)");
    check(others == 26, "the checks of a segment (16) and of an island or a group (10) are not trips");
    check(places.size() == 56, "the places of the trips (48 with two visits or three, 8 Safari Games)");
    check(rod_ways == 2 && rod_ways_in_place == 2, "the two ways to a rod, each a segment of its own that ends with its save");

    //  The segments of the Pokemon that are there once: given up from any
    //  of their saves, the player gets to where the next segment's FLY can
    //  start.
    check(
        story_once_place("hitmonlee") == "the segment for HITMONLEE" &&
        story_once_place("mr-mime") == "the segment for MR-MIME",
        "the segment of a Pokemon that is there once, as a place"
    );
    check(
        story_outdoors("Saffron City") && story_outdoors("Route 16") && story_outdoors("Pallet Town") &&
        !story_outdoors("Saffron City Pokemon Center 1F") && !story_outdoors("Route 16 House") &&
        !story_outdoors("Route ") && !story_outdoors(" City") && !story_outdoors("Cinnabar Island") &&
        !story_outdoors(""),
        "the towns and routes of Kanto by their names"
    );
    size_t once = 0;
    std::string once_ids;
    size_t saves_at_a_nurse = 0;
    size_t saves_outdoors = 0;
    size_t saves_unknown = 0;
    size_t walks_out_wrong = 0;
    for (size_t s = 0; s < script.segment_count; s++){
        const StorySegment& segment = script.segments[s];
        if (!story_once_segment(script, segment)){
            continue;
        }
        once++;
        once_ids += (once_ids.empty() ? "" : ", ") + std::string(segment.id);
        for (size_t i = 2; i + 1 < segment.step_count; i++){
            if (script.step(segment, i - 1).type != StepType::SAVE){
                continue;
            }
            switch (story_once_leave(script, segment, i)){
            case OnceLeave::WALK_OUT_FIRST:{
                saves_at_a_nurse++;
                //  The walk leaves the Pokemon Center: the walk after it
                //  starts on another map, a town.
                const StoryLeg& out = script.leg(script.step(segment, i).leg);
                const StoryStep& after = script.step(segment, i + 1);
                //  (Or the step after it is FLY: only outdoors.)
                const bool fine =
                    after.type == StepType::FLY ||
                    (after.type == StepType::WALK && after.leg >= 0 && (size_t)after.leg < script.leg_count &&
                    script.leg(after.leg).start_map != out.start_map &&
                    script.leg(after.leg).start_map < script.map_name_count &&
                    story_outdoors(script.map_names[script.leg(after.leg).start_map]));
                walks_out_wrong += fine ? 0 : 1;
                break;
            }
            case OnceLeave::FROM_HERE:
                saves_outdoors++;
                break;
            case OnceLeave::PAST_STEP:
            case OnceLeave::NO:
                saves_unknown++;
                break;
            }
        }
    }
    check(
        once == 6 &&
        once_ids == "C20_19_DOJO, C20_20_SNORLAX, C20_21_KABUTO, C20_22_AERODACTYL, C20_24_PC_BOXES, C20_25_STONES",
        "the four segments of Pokemon that are there once, the one that reads the PC's boxes, the one of the stones"
    );
    check(
        saves_at_a_nurse == 3 && saves_outdoors == 2 && saves_unknown == 0 && walks_out_wrong == 0,
        "from every save in such a segment the player gets outdoors: at a nurse by the walk out of the Pokemon "
        "Center (3), or the save is outdoors (2)"
    );
    check(
        story_once_leave(script, script.segments[0], 2) == OnceLeave::NO &&
        !story_once_segment(script, script.segments[0]) && !story_plain_trip(script, script.segments[0], 0) &&
        !story_rod_way(script, script.segments[0], 0),
        "a segment of the story is none of these"
    );

    logger.log(
        "AutoStory test: a place that fails twice in a row is left out for the run (in the story's data " +
        std::to_string(trips) + " trips to " + std::to_string(places.size()) + " places, " +
        std::to_string(fishing_trips) + " of them to fish and " + std::to_string(lap_trips) + " to walk laps; " +
        std::to_string(others) + " checks that are not one trip; " + std::to_string(rod_ways) + " ways to a rod; " +
        std::to_string(once) + " segments for what is done once, with " +
        std::to_string(saves_at_a_nurse + saves_outdoors) + " saves to give them up from): " +
        std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}

//  The PC's boxes read (build 34): what the PC's text box says and what is
//  pressed on it, the order of the cells, the list as a file, what the
//  POKeDEX table says of evolving; and in the story's data the segment that
//  reads the boxes.
int check_boxes(Logger& logger){
    using namespace NintendoSwitch;
    using namespace NintendoSwitch::PokemonFRLG;
    namespace Boxes = AutoStoryBoxes;
    using Boxes::PcText;
    using Boxes::PcPress;
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const std::string& what){
        checks++;
        if (!fine){
            wrong++;
            logger.log("AutoStory test: failed: " + what, COLOR_RED);
        }
    };

    //  The PC's texts, as the program reads them (lower case).
    check(
        Boxes::read_pc_text("ash booted up the pc.") == PcText::BOOTED &&
        Boxes::read_pc_text("which pc should be accessed?") == PcText::WHICH_PC &&
        Boxes::read_pc_text("accessed bill's pc.") == PcText::ACCESSED &&
        Boxes::read_pc_text("accessed someone's pc.") == PcText::ACCESSED &&
        Boxes::read_pc_text("pok\xc3\xa9mon storage system opened.") == PcText::OPENED &&
        Boxes::read_pc_text("pokemon storage system opened") == PcText::OPENED,
        "the PC's texts on the way to its storage menu"
    );
    check(
        Boxes::read_pc_text("you can withdraw a pok\xc3\xa9mon if you have any in a box.") == PcText::WITHDRAW &&
        Boxes::read_pc_text("you can deposit your party pok\xc3\xa9mon in any box.") == PcText::DEPOSIT &&
        Boxes::read_pc_text("you can move pok\xc3\xa9mon that are stored in any box.") == PcText::MOVE_MONS &&
        Boxes::read_pc_text("you can move items held by any pok\xc3\xa9mon in a box or your party.") == PcText::MOVE_ITEMS &&
        Boxes::read_pc_text("see you later!") == PcText::SEE_YA,
        "the five entries of the storage menu, by what the text box says of them"
    );
    check(
        Boxes::read_pc_text("can't take any more pok\xc3\xa9mon.") == PcText::CANNOT &&
        Boxes::read_pc_text("can't deposit the last pok\xc3\xa9mon!") == PcText::CANNOT &&
        Boxes::read_pc_text("continue box operations?") == PcText::BOX_QUESTION &&
        Boxes::read_pc_text("exit from the box?") == PcText::BOX_QUESTION &&
        Boxes::read_pc_text("") == PcText::NONE && Boxes::read_pc_text(". .") == PcText::NONE &&
        Boxes::read_pc_text("ash boo") == PcText::OTHER &&
        Boxes::read_pc_text("repel's effect wore off...") == PcText::OTHER,
        "what the menu refuses, the box view's question, no text, another text"
    );
    check(
        Boxes::pc_press_in(PcText::NONE) == PcPress::WAIT && Boxes::pc_press_in(PcText::OTHER) == PcPress::FAIL &&
        Boxes::pc_press_in(PcText::BOX_QUESTION) == PcPress::FAIL &&
        Boxes::pc_press_in(PcText::WITHDRAW) == PcPress::DOWN && Boxes::pc_press_in(PcText::DEPOSIT) == PcPress::DOWN &&
        Boxes::pc_press_in(PcText::MOVE_ITEMS) == PcPress::UP && Boxes::pc_press_in(PcText::SEE_YA) == PcPress::UP &&
        Boxes::pc_enters_boxes(PcText::MOVE_MONS) && !Boxes::pc_enters_boxes(PcText::WITHDRAW) &&
        !Boxes::pc_enters_boxes(PcText::DEPOSIT) && !Boxes::pc_enters_boxes(PcText::MOVE_ITEMS),
        "A is pressed on no entry of the storage menu but MOVE POKeMON"
    );
    //  The way in, played against the PC as the game has it (pc.inc,
    //  Task_PCMainMenu): from every entry the menu's cursor may be on, with
    //  a party that is full (WITHDRAW refused) or not.
    {
        struct Pc{
            int state = 0;      //  0 "booted", 1 the list, 2 "accessed", 3 "opened", 4 the storage menu, 5 a box view, 6 off
            int cursor = 0;
            int entered = -1;
            bool refused = false;
            bool party_full = false;
            PcText text() const{
                static const PcText entries[5] = {
                    PcText::WITHDRAW, PcText::DEPOSIT, PcText::MOVE_MONS, PcText::MOVE_ITEMS, PcText::SEE_YA,
                };
                switch (state){
                case 0:     return PcText::BOOTED;
                case 1:     return PcText::WHICH_PC;
                case 2:     return PcText::ACCESSED;
                case 3:     return PcText::OPENED;
                case 4:     return refused ? PcText::CANNOT : entries[cursor];
                default:    return PcText::NONE;
                }
            }
            void press(PcPress button){
                const bool a = button == PcPress::A;
                const bool b = button == PcPress::B;
                switch (state){
                case 0:
                case 2:
                case 3:
                    state += a || b ? 1 : 0;
                    break;
                case 1:
                    state = a ? 2 : b ? 6 : 1;
                    break;
                case 4:
                    if (refused){
                        refused = !(a || b);
                    }else if (button == PcPress::UP){
                        cursor = (cursor + 4) % 5;
                    }else if (button == PcPress::DOWN){
                        cursor = (cursor + 1) % 5;
                    }else if (b || (a && cursor == 4)){
                        state = 1;
                    }else if (a && cursor == 0 && party_full){
                        refused = true;
                    }else if (a){
                        state = 5;
                        entered = cursor;
                    }
                    break;
                default:
                    break;
                }
            }
        };
        bool all_in = true;
        int most = 0;
        for (int cursor = 0; cursor < 5; cursor++){
            for (int full = 0; full < 2; full++){
                Pc pc;
                pc.party_full = full == 1;
                int presses = 0;
                bool menu_set = false;
                while (presses < 20 && pc.state < 5){
                    if (pc.state == 4 && !menu_set){
                        pc.cursor = cursor;
                        menu_set = true;
                    }
                    const PcPress button = Boxes::pc_press_in(pc.text());
                    if (button == PcPress::WAIT || button == PcPress::FAIL){
                        break;
                    }
                    pc.press(button);
                    presses++;
                }
                all_in &= pc.state == 5 && pc.entered == 2;
                most = std::max(most, presses);
            }
        }
        check(all_in && most <= 7, "from the PC's first text to the box view, always by MOVE POKeMON");
    }

    //  The cells of a box.
    {
        const std::vector<Boxes::Cell>& order = Boxes::scan_order();
        std::set<int> numbers;
        bool one_step = true;
        for (size_t k = 0; k < order.size(); k++){
            numbers.insert(Boxes::cell_number(order[k]));
            one_step &= Boxes::cell_in_box(order[k]) &&
                (k == 0 || Boxes::cell_moves(order[k - 1], order[k]).size() == 1);
        }
        check(
            order.size() == (size_t)Boxes::BOX_CELLS && numbers.size() == order.size() &&
            *numbers.begin() == 0 && *numbers.rbegin() == Boxes::BOX_CELLS - 1 && one_step &&
            order.front() == Boxes::Cell{0, 2} && order.back() == Boxes::Cell{0, 5},
            "the thirty cells of a box in one line: from where DOWN from the box's name puts the hand to the "
            "top row, each one step from the one before"
        );
        //  The moves from any cell to any other, by the game's rules: UP from
        //  the top row and DOWN from the bottom row leave the cells, LEFT
        //  and RIGHT go round at the edges.
        bool arrive = true;
        for (int from = 0; from < Boxes::BOX_CELLS; from++){
            for (int to = 0; to < Boxes::BOX_CELLS; to++){
                Boxes::Cell at{from / Boxes::BOX_COLUMNS, from % Boxes::BOX_COLUMNS};
                const Boxes::Cell target{to / Boxes::BOX_COLUMNS, to % Boxes::BOX_COLUMNS};
                const std::string moves = Boxes::cell_moves(at, target);
                bool left_cells = false;
                for (char move : moves){
                    if (move == 'U' || move == 'D'){
                        at.row += move == 'U' ? -1 : 1;
                        left_cells |= at.row < 0 || at.row >= Boxes::BOX_ROWS;
                    }else{
                        left_cells |= move == 'L' ? at.column == 0 : at.column == Boxes::BOX_COLUMNS - 1;
                        at.column = (at.column + (move == 'L' ? Boxes::BOX_COLUMNS - 1 : 1)) % Boxes::BOX_COLUMNS;
                    }
                }
                arrive &= !left_cells && at == target &&
                    moves.size() == (size_t)(std::abs(from / Boxes::BOX_COLUMNS - to / Boxes::BOX_COLUMNS) +
                        std::abs(from % Boxes::BOX_COLUMNS - to % Boxes::BOX_COLUMNS));
            }
        }
        check(
            arrive && Boxes::cell_moves(Boxes::Cell{4, 5}, Boxes::Cell{0, 0}) == "LLLLLUUUU" &&
            Boxes::cell_moves(Boxes::Cell{2, 2}, Boxes::Cell{2, 2}).empty() &&
            Boxes::cell_moves(Boxes::Cell{5, 0}, Boxes::Cell{0, 0}).empty(),
            "the hand from any cell to any other without leaving the cells"
        );
    }

    //  What was read, and its file.
    Boxes::BoxCensus census;
    census.boxes = Boxes::BOX_COUNT;
    census.closed = true;
    auto put = [&](int box, int cell, const char* slug, int level){
        Boxes::BoxPokemon one;
        one.box = box;
        one.cell = cell;
        one.slug = slug;
        one.level = level;
        census.pokemon.push_back(one);
    };
    put(0, 3, "nidorina", 25);
    put(0, 4, "metapod", 8);
    put(0, 5, "pidgeotto", 40);
    put(0, 9, "golbat", 30);
    put(0, 10, "", -1);
    put(1, 7, "nidorina", 31);
    put(2, 0, "eevee", 5);
    put(2, 1, "eevee", 25);
    {
        const std::vector<Boxes::BoxPokemon> eevees = census.of("eevee");
        check(
            census.count("eevee") == 2 && census.count("mew") == 0 && census.unread() == 1 &&
            census.in_box(0) == 5 && census.in_box(3) == 0 && census.boxes_used() == 3 &&
            eevees.size() == 2 && eevees[0].level == 25 && eevees[0].cell == 1 && eevees[1].level == 5,
            "what the boxes have of a species, the highest level first"
        );
        const std::string text = Boxes::census_to_text(census);
        Boxes::BoxCensus back;
        const bool read_back = Boxes::census_from_text(text + "# By level (2): a line of the program.\n", back);
        bool same = read_back && back.boxes == census.boxes && back.closed == census.closed &&
            back.pokemon.size() == census.pokemon.size();
        for (size_t k = 0; same && k < census.pokemon.size(); k++){
            same = back.pokemon[k].box == census.pokemon[k].box && back.pokemon[k].cell == census.pokemon[k].cell &&
                back.pokemon[k].slug == census.pokemon[k].slug && back.pokemon[k].level == census.pokemon[k].level;
        }
        check(same && text.find("\n0 10 ? -1\n") != std::string::npos, "the list of the boxes as a file, and read back");
        Boxes::BoxCensus none;
        check(
            !Boxes::census_from_text("", none) && !Boxes::census_from_text("hello\n", none) &&
            !Boxes::census_from_text("boxes 14 closed 1 pokemon 2\n0 0 pidgey 3\n", none) &&
            !Boxes::census_from_text("boxes 14 closed 1 pokemon 1\n0 30 pidgey 3\n", none) &&
            !Boxes::census_from_text("boxes 2 closed 1 pokemon 1\n2 0 pidgey 3\n", none) &&
            !Boxes::census_from_text("boxes 14 closed 1 pokemon 1\n0 0 pidgey 101\n", none) &&
            none.pokemon.empty() && none.boxes == 0 &&
            Boxes::census_from_text("boxes 14 closed 0 pokemon 0\n", none) && !none.closed && none.boxes == 14,
            "a file that is not such a list is not taken for one"
        );
    }

    //  Evolving, from the POKeDEX table: every way of evolving the table
    //  has is understood, in both games.
    size_t count = 0;
    const AutoStoryDex::CollectionEntry* entries = AutoStoryDex::collection(count);
    std::set<std::string> names;
    for (size_t k = 0; k < count; k++){
        names.insert(entries[k].name);
    }
    int hows = 0;
    int understood = 0;
    for (size_t k = 0; k < count; k++){
        for (int game = 0; game < 2; game++){
            const std::string how = entries[k].how[game];
            if (how.rfind("evolve from", 0) != 0){
                continue;
            }
            hows++;
            Boxes::EvolveHow way;
            understood += Boxes::read_evolve_how(how, way) && names.count(way.from) != 0 ? 1 : 0;
        }
    }
    check(hows >= 80 && understood == hows, "every \"evolve from\" of the POKeDEX table is understood");
    {
        Boxes::EvolveHow level;
        Boxes::EvolveHow stone;
        Boxes::EvolveHow sun;
        Boxes::EvolveHow friendship;
        Boxes::EvolveHow stats;
        Boxes::EvolveHow none;
        check(
            Boxes::read_evolve_how("evolve from Metapod: level 10", level) && level.from == "Metapod" &&
            level.by == Boxes::EvolveBy::LEVEL && level.level == 10 &&
            Boxes::read_evolve_how("evolve from Nidorina: Moon Stone", stone) && stone.from == "Nidorina" &&
            stone.by == Boxes::EvolveBy::STONE && stone.stone == "Moon Stone" &&
            Boxes::read_evolve_how("evolve from Gloom: Sun Stone (the only one: Ruin Valley, Six Island) (National Dex)", sun) &&
            sun.by == Boxes::EvolveBy::STONE && sun.stone == "Sun Stone" &&
            Boxes::read_evolve_how("evolve from Golbat: level up with high friendship (220+) (National Dex)", friendship) &&
            friendship.by == Boxes::EvolveBy::FRIENDSHIP &&
            Boxes::read_evolve_how("evolve from Tyrogue: level 20 with Attack = Defense (National Dex)", stats) &&
            stats.by == Boxes::EvolveBy::OTHER && stats.level == 20 &&
            !Boxes::read_evolve_how("Grass/cave: Route 1", none) && !Boxes::read_evolve_how("evolve from : level 10", none) &&
            !Boxes::read_evolve_how("evolve from Metapod: level 0", none) && !Boxes::read_evolve_how("evolve from Metapod", none),
            "a level, a stone, friendship, a level and its stats"
        );
    }
    //  FireRed with Charmander: what only comes by evolving, with nothing in
    //  the table yet and with the boxes above.
    std::vector<Boxes::TableRow> table;
    for (size_t k = 0; k < count; k++){
        if ((entries[k].available & (1 << 2)) == 0){
            continue;
        }
        Boxes::TableRow row;
        row.slug = entries[k].slug;
        row.name = entries[k].name;
        row.needed = entries[k].copies[0];
        row.how = entries[k].how[0];
        table.push_back(row);
    }
    int by_level = 0;
    int by_stone = 0;
    int by_friendship = 0;
    int by_other = 0;
    {
        const std::vector<Boxes::EvolveLine> plan = Boxes::evolve_plan(table, Boxes::BoxCensus());
        bool none_there = true;
        bool all_named = true;
        for (const Boxes::EvolveLine& line : plan){
            none_there &= line.box < 0 && line.in_boxes == 0;
            all_named &= !line.from_slug.empty();
            switch (line.how.by){
            case Boxes::EvolveBy::LEVEL:        by_level++; break;
            case Boxes::EvolveBy::STONE:        by_stone++; break;
            case Boxes::EvolveBy::FRIENDSHIP:   by_friendship++; break;
            case Boxes::EvolveBy::OTHER:        by_other++; break;
            }
        }
        check(
            table.size() == 175 && plan.size() == 41 && none_there && all_named &&
            by_level == 23 && by_stone == 14 && by_friendship == 3 && by_other == 1,
            "FireRed with Charmander: the 41 species of the table that only come by evolving, and how"
        );
        const std::vector<std::string> lines = Boxes::evolve_summary(plan);
        check(
            lines.size() == 1 && lines[0].find("is not in a box (41;") != std::string::npos &&
            lines[0].find("Butterfree (Metapod)") != std::string::npos,
            "with empty boxes every one of them is still to come"
        );
    }
    {
        const std::vector<Boxes::EvolveLine> plan = Boxes::evolve_plan(table, census);
        auto line_of = [&](const std::string& slug){
            for (const Boxes::EvolveLine& line : plan){
                if (line.slug == slug){
                    return line;
                }
            }
            return Boxes::EvolveLine();
        };
        const Boxes::EvolveLine nidoqueen = line_of("nidoqueen");
        const Boxes::EvolveLine butterfree = line_of("butterfree");
        const Boxes::EvolveLine pidgeot = line_of("pidgeot");
        const Boxes::EvolveLine vaporeon = line_of("vaporeon");
        const Boxes::EvolveLine jolteon = line_of("jolteon");
        const Boxes::EvolveLine flareon = line_of("flareon");
        const Boxes::EvolveLine crobat = line_of("crobat");
        check(
            nidoqueen.from_slug == "nidorina" && nidoqueen.in_boxes == 2 && nidoqueen.box == 1 &&
            nidoqueen.cell == 7 && nidoqueen.level == 31 && nidoqueen.how.stone == "Moon Stone" &&
            butterfree.box == 0 && butterfree.cell == 4 && butterfree.level == 8 && butterfree.how.level == 10 &&
            pidgeot.level == 40 && pidgeot.how.level == 36 && crobat.box == 0 && crobat.cell == 9 &&
            crobat.how.by == Boxes::EvolveBy::FRIENDSHIP,
            "for a species that comes by evolving: the one of the boxes to evolve, the highest level first"
        );
        check(
            vaporeon.box == 2 && vaporeon.cell == 1 && vaporeon.level == 25 &&
            jolteon.box == 2 && jolteon.cell == 0 && jolteon.level == 5 &&
            flareon.box < 0 && flareon.in_boxes == 2 && flareon.from_slug == "eevee",
            "one Pokemon of the boxes is for one species only (two EEVEEs: no FLAREON)"
        );
        const std::vector<std::string> lines = Boxes::evolve_summary(plan);
        std::string all;
        for (const std::string& line : lines){
            all += line + "\n";
        }
        check(
            all.find("With a Moon Stone (1), the one to use it on is in a box: Nidoqueen (NIDORINA Lv 31, box 2 cell 8).") != std::string::npos &&
            all.find("Butterfree (METAPOD Lv 8, box 1 cell 5: 2 more to level 10)") != std::string::npos &&
            all.find("Pidgeot (PIDGEOTTO Lv 40, box 1 cell 6: at its next level)") != std::string::npos &&
            all.find("Crobat (GOLBAT Lv 30, box 1 cell 10: friendship)") != std::string::npos &&
            all.find("Flareon (Eevee: the 2 in the boxes are for others)") != std::string::npos &&
            all.find("With a Water Stone (1)") != std::string::npos && all.find("With a Thunder Stone (1)") != std::string::npos,
            "what the boxes have for evolving, as the log says it"
        );
    }

    //  The story's data: one segment reads the boxes, the last one. The
    //  walk before the look ends in a Pokemon Center, the player facing up
    //  (the PC is in the wall above); the segment is skipped as a whole,
    //  starts with FLY and has its only save at its end.
    const StoryScript& script = AutoStoryData::story_CHARMANDER();
    int looks = 0;
    int skips = 0;
    bool looks_fine = true;
    std::string where;
    for (size_t s = 0; s < script.segment_count; s++){
        const StorySegment& segment = script.segments[s];
        for (size_t k = 0; k < segment.step_count; k++){
            const StoryStep& step = script.step(segment, k);
            skips += step.type == StepType::SKIP_IF_BOXES ? 1 : 0;
            if (step.type != StepType::BOXES){
                continue;
            }
            looks++;
            where = segment.id;
            bool fine = k > 0 && script.step(segment, k - 1).type == StepType::WALK &&
                story_once_segment(script, segment) &&
                script.step(segment, 0).type == StepType::SKIP_IF_BOXES;
            if (fine){
                const StoryLeg& leg = script.leg(script.step(segment, k - 1).leg);
                fine = leg.end == LegEnd::INTERACT && leg.face == DPAD_UP && leg.start_map < script.map_name_count &&
                    std::string(script.map_names[leg.start_map]).find("Pokemon Center 1F") != std::string::npos;
            }
            for (size_t n = 0; fine && n + 1 < segment.step_count; n++){
                fine = script.step(segment, n).type != StepType::SAVE;
            }
            looks_fine &= fine;
        }
    }
    check(
        looks == 1 && skips == 1 && looks_fine && where == "C20_24_PC_BOXES" &&
        story_boxes_place() == "the PC's boxes",
        "one segment of the story reads the PC's boxes, in front of the PC of a Pokemon Center"
    );

    //  Evolution stones (build 35). The six stones: found in the bag by the
    //  last line of their description, which no other stone has.
    {
        const std::vector<std::pair<std::string, std::string>> last_lines{
            {"Fire Stone", "it is colored orange."}, {"Thunder Stone", "it has a thunderbolt pattern."},
            {"Water Stone", "it is a clear light blue."}, {"Leaf Stone", "it has a leaf pattern."},
            {"Moon Stone", "it is as black as the night sky."}, {"Sun Stone", "it is as red as the sun."},
        };
        bool own = true;
        bool no_other = true;
        size_t words = 0;
        for (const Boxes::StoneKind& kind : Boxes::stone_kinds()){
            const std::string all = kind.words;
            for (size_t start = 0; start <= all.size();){
                const size_t bar = all.find('|', start);
                const std::string word = all.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
                words++;
                for (const std::pair<std::string, std::string>& line : last_lines){
                    const bool in_line = !word.empty() && line.second.find(word) != std::string::npos;
                    if (line.first == kind.name){
                        own &= in_line;
                    }else{
                        no_other &= !in_line;
                    }
                }
                if (bar == std::string::npos){
                    break;
                }
                start = bar + 1;
            }
        }
        const Boxes::StoneKind* fire = Boxes::stone_kind("Fire Stone");
        const Boxes::StoneKind* thunder = Boxes::stone_kind("Thunder Stone");
        const Boxes::StoneKind* water = Boxes::stone_kind("Water Stone");
        const Boxes::StoneKind* leaf = Boxes::stone_kind("Leaf Stone");
        const Boxes::StoneKind* moon = Boxes::stone_kind("Moon Stone");
        const Boxes::StoneKind* sun = Boxes::stone_kind("Sun Stone");
        check(
            Boxes::stone_kinds().size() == 6 && words == 11 && own && no_other &&
            fire != nullptr && thunder != nullptr && water != nullptr && leaf != nullptr && moon != nullptr &&
            sun != nullptr && Boxes::stone_kind("King's Rock") == nullptr &&
            fire->mart == 2 && thunder->mart == 3 && water->mart == 4 && leaf->mart == 5 &&
            moon->mart == -1 && sun->mart == -1 && std::string(moon->item) == "MOON STONE",
            "the six evolution stones: their words in the bag, their place in the Department Store's list"
        );
    }
    //  Which Pokemon of the boxes get a stone, and what is to buy.
    {
        const std::vector<Boxes::EvolveLine> plan = Boxes::evolve_plan(table, census);
        const std::vector<Boxes::StoneTarget> targets = Boxes::stone_targets(plan);
        const std::vector<std::pair<std::string, int>> needed = Boxes::stones_needed(targets);
        const std::vector<std::pair<std::string, int>> buy = Boxes::stones_to_buy(targets, {});
        const std::vector<std::pair<std::string, int>> buy_less = Boxes::stones_to_buy(targets, {"Water Stone", "Moon Stone"});
        const std::vector<Boxes::StoneTarget> no_moon = Boxes::stone_targets(plan, {"Moon Stone"});
        check(
            targets.size() == 3 && targets[0].slug == "nidoqueen" && targets[0].from_slug == "nidorina" &&
            targets[0].box == 1 && targets[0].cell == 7 && targets[0].stone == "Moon Stone" &&
            targets[1].slug == "vaporeon" && targets[1].from_slug == "eevee" && targets[1].cell == 1 &&
            targets[2].slug == "jolteon" && targets[2].cell == 0 && targets[2].stone == "Thunder Stone",
            "the Pokemon of the boxes that evolve with a stone, in the table's order (FLAREON has no EEVEE left)"
        );
        check(
            needed.size() == 3 && needed[0] == std::make_pair(std::string("Thunder Stone"), 1) &&
            needed[1] == std::make_pair(std::string("Water Stone"), 1) &&
            needed[2] == std::make_pair(std::string("Moon Stone"), 1) &&
            buy.size() == 2 && buy[0].first == "Thunder Stone" && buy[1].first == "Water Stone" &&
            buy_less.size() == 1 && buy_less[0] == std::make_pair(std::string("Thunder Stone"), 1) &&
            no_moon.size() == 2 && no_moon[0].slug == "vaporeon",
            "the stones they take; to buy: those that are sold, less the ones the bag has; none of a stone that is out"
        );
        check(
            Boxes::stone_targets_text(targets) ==
                "Nidoqueen (NIDORINA, box 2 cell 8, MOON STONE), Vaporeon (EEVEE, box 3 cell 2, WATER STONE), "
                "Jolteon (EEVEE, box 3 cell 1, THUNDER STONE)",
            "they as the log says them"
        );
        //  After the first one evolved: the table has NIDOQUEEN, the cell has
        //  her, and the other NIDORINA stays what it is.
        std::vector<Boxes::TableRow> table_after = table;
        for (Boxes::TableRow& row : table_after){
            row.have += row.slug == "nidoqueen" ? 1 : 0;
        }
        Boxes::BoxCensus census_after = census;
        census_after.put(1, 7, "nidoqueen", 31);
        const std::vector<Boxes::StoneTarget> then = Boxes::stone_targets(Boxes::evolve_plan(table_after, census_after));
        const Boxes::BoxPokemon* there = census_after.at(1, 7);
        Boxes::BoxCensus one_more = census;
        one_more.put(13, 29, "articuno", 50);
        const std::vector<Boxes::BoxPokemon> row_by_row = census.by_cell(0);
        bool in_order = !row_by_row.empty();
        for (size_t k = 1; k < row_by_row.size(); k++){
            in_order &= row_by_row[k - 1].cell < row_by_row[k].cell;
        }
        check(
            then.size() == 2 && then[0].slug == "vaporeon" && there != nullptr && there->slug == "nidoqueen" &&
            census_after.pokemon.size() == census.pokemon.size() && census_after.count("nidorina") == 1 &&
            census.at(1, 7) != nullptr && census.at(1, 7)->slug == "nidorina" && census.at(5, 5) == nullptr &&
            one_more.pokemon.size() == census.pokemon.size() + 1 && one_more.at(13, 29) != nullptr &&
            in_order && (int)row_by_row.size() == census.in_box(0),
            "the list after an evolution: another Pokemon in the cell, and the one evolved is not looked for again"
        );
    }
    //  The story's data: the last segment is the one of the stones.
    {
        const StorySegment& segment = script.segments[script.segment_count - 1];
        size_t stones_at = 0;
        size_t bag_at = 0;
        int stones = 0;
        int bags = 0;
        int buys = 0;
        int rematches = 0;
        for (size_t k = 0; k < segment.step_count; k++){
            const StoryStep& step = script.step(segment, k);
            if (step.type == StepType::STONES){
                stones++;
                stones_at = k;
            }
            if (step.type == StepType::SKIP_IF_STONES_IN_BAG){
                bags++;
                bag_at = k;
            }
        }
        bool way_fine = bags == 1 && bag_at + (size_t)std::max(0, (int)script.step(segment, bag_at).p0) + 1 < segment.step_count;
        if (way_fine){
            const size_t skipped = (size_t)script.step(segment, bag_at).p0;
            for (size_t k = bag_at + 1; k <= bag_at + skipped; k++){
                buys += script.step(segment, k).type == StepType::BUY_STONES ? 1 : 0;
                rematches += script.step(segment, k).type == StepType::REMATCH ? 1 : 0;
            }
            //  Skipped or not, a FLY comes next.
            way_fine = script.step(segment, bag_at + skipped + 1).type == StepType::FLY;
        }
        bool pc_fine = stones == 1 && stones_at > 0 && script.step(segment, stones_at - 1).type == StepType::WALK;
        if (pc_fine){
            const StoryLeg& leg = script.leg(script.step(segment, stones_at - 1).leg);
            pc_fine = leg.end == LegEnd::INTERACT && leg.face == DPAD_UP && leg.start_map < script.map_name_count &&
                std::string(script.map_names[leg.start_map]).find("Pokemon Center 1F") != std::string::npos;
        }
        check(
            std::string(segment.id) == "C20_25_STONES" && story_once_segment(script, segment) &&
            script.step(segment, 0).type == StepType::SKIP_IF_NO_STONES && way_fine && buys == 1 && rematches == 1 &&
            pc_fine && story_once_leave(script, segment, stones_at) == OnceLeave::PAST_STEP &&
            story_stones_place() == "the evolution stones",
            "the story's last segment is the one of the stones: the prize money and the purchase (skipped with "
            "the stones in the bag), then the PC of a Pokemon Center; given up from its own save past its step"
        );
    }

    logger.log(
        "AutoStory test: the PC's boxes are read, and what is in them gets its stone (the PC's texts, the cells, "
        "the list as a file, the six stones; of the " +
        std::to_string(table.size()) + " species of FireRed with Charmander " +
        std::to_string(by_level + by_stone + by_friendship + by_other) + " only come by evolving: " +
        std::to_string(by_level) + " by level, " + std::to_string(by_stone) + " by a stone, " +
        std::to_string(by_friendship) + " by friendship, " + std::to_string(by_other) + " by level and stats; " +
        std::to_string(hows) + " ways of evolving in the table; in the story's data the segments " + where +
        " and " + script.segments[script.segment_count - 1].id + "): " +
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
        failed += check_slow_steps(logger);
        failed += check_poison_flash(logger);
        failed += check_safari_table(logger);
        failed += check_next_steps(logger);
        failed += check_leave_out(logger);
        failed += check_boxes(logger);
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

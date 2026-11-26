/*  PocketPilot Self Test
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>
#include <string>
#include <vector>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTextEdit>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QWheelEvent>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/ColoredText.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Options/BatchOption.h"
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Qt/Options/ConfigWidget.h"
#include "CommonFramework/AudioPipeline/AudioOption.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/PersistentSettings.h"
#include "CommonFramework/Recording/StreamHistoryTracker_SaveFrames.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/Logging/LoggerWindow.h"
#include "CommonFramework/Options/Environment/ThemeSelectorOption.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "CommonFramework/Windows/MainWindow.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Pokemon/Resources/Pokemon_PokemonSlugs.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBoxes.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBump.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCoins.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCollectionText.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryData.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryDex.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryDexCollection.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryFerryTalk.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryFishing.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryNameMark.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPlaceList.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySafari.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryScroll.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySlowSteps.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySmallRoom.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTimes.h"
#include "VideoGames/PokemonFRLG/Programs/ClaudePlays/PokemonFRLG_ClaudePlays.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_SafariOptimalAction.h"
#include "Assistant/Assistant_Panel.h"
#include "Assistant/Assistant_Play.h"
#include "Assistant/Assistant_SelfTest.h"
#include "Assistant/Assistant_Service.h"
#include "ClaudePlays/ClaudePlays_AnyGame.h"
#include "ClaudePlays/ClaudePlays_Engine.h"
#include "ClaudePlays/ClaudePlays_Games.h"
#include "Common/Cpp/Concurrency/PeriodicRunner.h"
#include "ClaudePlays/ClaudePlays_Program.h"
#include "PocketPilot_About.h"
#include "PocketPilot_Home.h"
#include "PocketPilot_Instances.h"
#include "PocketPilot_Language.h"
#include "PocketPilot_OldSettings.h"
#include "PocketPilot_SettingsSearch.h"
#include "PocketPilot_ShinyClip.h"
#include "PocketPilot_Theme.h"
#include "PocketPilot_Setup.h"
#include "PocketPilot_SelfTest.h"
#include "PocketPilot_Update.h"

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

//  A connection that answers what it is told to, from the event loop as the
//  network does, and keeps the requests.
class ScriptedTransport : public Assistant::HttpTransport{
public:
    struct Call : public Assistant::HttpCall{
        virtual void abort() override{
            aborted = true;
        }
        bool aborted = false;
    };

    virtual std::shared_ptr<Assistant::HttpCall> send(
        const Assistant::HttpRequest& request,
        DataHandler, DoneHandler on_done
    ) override{
        requests.push_back(request);
        std::shared_ptr<Call> call = std::make_shared<Call>();
        calls.push_back(call);
        if (!silent){
            const Assistant::HttpResponse response = answer;
            QTimer::singleShot(0, &m_context, [call, response, on_done]{
                if (!call->aborted){
                    on_done(response);
                }
            });
        }
        return call;
    }

    //  What the next requests are answered with.
    Assistant::HttpResponse answer;
    //  Never answers.
    bool silent = false;
    std::vector<Assistant::HttpRequest> requests;
    std::vector<std::shared_ptr<Call>> calls;

private:
    QObject m_context;
};

//  The list of releases as GitHub answers it.
Assistant::HttpResponse releases_answer(int status, const QByteArray& body){
    Assistant::HttpResponse response;
    response.status = status;
    response.body = body;
    return response;
}

const char RELEASES_JSON[] =
    "[{\"tag_name\":\"pocketpilot-build-99\",\"draft\":true,\"prerelease\":true,\"published_at\":\"2026-11-26T08:00:00Z\"},"
    "{\"tag_name\":\"pocketpilot-build-88\",\"draft\":false,\"prerelease\":true,\"published_at\":\"2026-11-25T20:13:48Z\","
    "\"html_url\":\"https://evil.example/pocketpilot-build-88\"},"
    "{\"tag_name\":\"pocketpilot-build-9\",\"draft\":false,\"prerelease\":true,\"published_at\":\"2026-10-01T08:00:00Z\"},"
    "{\"tag_name\":\"v1.2.3\",\"draft\":false,\"prerelease\":false,\"published_at\":\"2026-12-01T08:00:00Z\"}]";

const char PROJECT_URL[] = "https://github.com/MajorLOL/Arduino-Source/tree/pocketpilot";

}


namespace{

//  Defined further down, with the other checks.
int check_settings_search(MainWindow& window, Logger& logger);

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
        //  A few small pictures, for the log of the build (see the workflow).
        const QString base = QString::fromLatin1(name);
        if (base == "night-fire-about-update" || base == "night-fire-output-log" || base == "night-fire-settings-search"){
            picture.scaledToWidth(960, Qt::SmoothTransformation).save(out + "/preview-" + base + ".jpg", "JPG", 72);
        }
    };
    auto save = [&](QWidget& widget, const char* name){
        let_run(700);
        save_picture(widget.grab(), name);
    };
    //  (The window is not on the screen in this test: it can have any size.)
    const int PICTURE_WIDTH = 1600;
    const int PICTURE_HEIGHT = 900;
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
    //  One picture of the opened list: the longest one among the options of
    //  the open program.
    auto save_lists = [&](const char* name){
        QComboBox* option = nullptr;
        for (QComboBox* box : window.findChildren<QComboBox*>()){
            if (!box->isVisible() || !box->isEnabled() || box->isEditable()){
                continue;
            }
            if (option == nullptr || box->count() > option->count()){
                option = box;
            }
        }
        if (option == nullptr){
            failed++;
            logger.log(std::string("UI test: no dropdown found for ") + name, COLOR_RED);
            return;
        }
        const int MARGIN = 16;
        const QPixmap list = grab_list(*option);
        QPixmap sheet(list.width() + 2 * MARGIN, list.height() + 2 * MARGIN);
        sheet.fill(theme_colors().screen);
        {
            QPainter painter(&sheet);
            painter.drawPixmap(MARGIN, MARGIN, list);
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
    //  The setup ends with the choice of a game: it lists the games without
    //  a window, and the game it returns is shown as a click on its
    //  cartridge does.
    {
        std::vector<HomeGame> listed;
        describe_all_games(listed);
        bool found = false;
        for (const HomeGame& listed_game : listed){
            if (listed_game.name == QString::fromStdString(GAME)){
                found = listed_game.programs > 0;
            }
        }
        if (!found){
            failed++;
            logger.log("UI test: the games of the setup do not have " + GAME, COLOR_RED);
        }
        if (!window.open_game(QString::fromStdString(GAME))){
            failed++;
            logger.log("UI test: the game that the setup chose could not be shown", COLOR_RED);
        }
        save(window, "night-fire-chosen-game");
        window.show_games();
    }
    open_program();
    save(window, "night-fire-program");
    logger.log("UI test: opening the settings");
    window.show_settings();
    save(window, "night-fire-settings");
    //  The search of the settings page: a picture with a word in it, and the
    //  checks.
    if (QLineEdit* search = window.findChild<QLineEdit*>("ppOptionsSearch")){
        search->setText("screen");
        let_run(100);
        save(window, "night-fire-settings-search");
        search->clear();
        let_run(50);
    }
    failed += check_settings_search(window, logger);
    //  The other windows, drawn without being put on the screen, like the
    //  main window in this test (see Main.cpp).
    {
        logger.log("UI test: opening the About window");
        //  With the button that checks for updates, answered by a made-up
        //  GitHub: the test does not use the network.
        //  One that is newer than this build, to show the link as well.
        ScriptedTransport transport;
        transport.answer = releases_answer(
            200, "[{\"tag_name\":\"pocketpilot-build-99999\",\"draft\":false,\"published_at\":\"2026-11-25T20:13:48Z\"}]"
        );
        std::unique_ptr<QDialog> about = make_about(&window, window.about_info(), &transport);
        about->setAttribute(Qt::WA_DontShowOnScreen, true);
        about->show();
        save(*about, "night-fire-about");
        if (QPushButton* update = about->findChild<QPushButton*>("ppAboutUpdateButton")){
            update->click();
            let_run(100);
            save(*about, "night-fire-about-update");
        }
        about->close();
        about.reset();
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

    //  An experimental program: its tag in the list, the note in its
    //  header, its options.
    set_look(UiThemeMode::POCKET_NIGHT, Starter::FIRE);
    open_named("Level Trainer");
    let_run(500);
    save(window, "night-fire-level-trainer");
    open_named("Box Namer");
    let_run(500);
    save(window, "night-fire-box-namer");
    open_named("Claude Plays");
    let_run(500);
    save(window, "night-fire-claude-plays");
    //  The Claude Plays of every other list, here of "Other Games".
    logger.log(std::string("UI test: opening ") + ClaudePlays::OTHER_GAMES_LIST + ", Claude Plays");
    if (!window.open_program(ClaudePlays::OTHER_GAMES_LIST, "Claude Plays")){
        failed++;
        logger.log(std::string("UI test: could not open ") + ClaudePlays::OTHER_GAMES_LIST + ", Claude Plays", COLOR_RED);
    }
    let_run(500);
    save(window, "night-fire-claude-plays-any-game");

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

        //  The keys can be kept in memory only: the box is on the page of the
        //  first key and in the settings, and both follow the switch.
        {
            std::vector<QCheckBox*> boxes;
            for (QCheckBox* box : panel.findChildren<QCheckBox*>()){
                if (box->text() == "Remember the key on this computer"){
                    boxes.push_back(box);
                }
            }
            const auto all_are = [&](bool checked){
                return std::all_of(boxes.begin(), boxes.end(), [&](QCheckBox* box){ return box->isChecked() == checked; });
            };
            bool ok = boxes.size() == 2 && all_are(true) && service.secrets().remembers();
            if (ok){
                boxes[0]->setChecked(false);
                let_run(50);
                ok = !service.secrets().remembers() && all_are(false) && service.secrets().has(Assistant::Secret::CLAUDE_API_KEY);
                boxes[1]->setChecked(true);
                let_run(50);
                ok = ok && service.secrets().remembers() && all_are(true);
            }
            if (!ok){
                failed++;
                logger.log("UI test: the box that keeps the keys in memory only is not on both pages or does not follow the switch", COLOR_RED);
            }
        }

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


//  The settings file while the program is open (build 37). It is put in its
//  place in one step. And a program asks, from its own thread, for the
//  options of the panel that is open to be written: the main window does
//  that on the interface's thread. (The AutoStory asks at its start and at
//  each save of the game.) On the build machine, with the real file.
int check_settings_file(MainWindow& window, Logger& logger){
    const std::string GAME = Pokemon::STRING_POKEMON + " FireRed and LeafGreen";
    const std::string PROGRAM = "AutoStory";
    const std::string KEY = "PocketPilot:SettingsTest";
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const std::string& what){
        checks++;
        if (!fine){
            wrong++;
            logger.log("Settings test: failed: " + what, COLOR_RED);
        }
    };
    //  What the file on the disk has under the test's key, and how many
    //  panels.
    size_t panels_in_file = 0;
    auto in_file = [&]() -> std::string{
        panels_in_file = 0;
        try{
            JsonValue json = load_json_file(PROGRAM_SETTING_JSON_PATH());
            const JsonObject* root = json.to_object();
            const JsonObject* panels = root == nullptr ? nullptr : root->get_object("99-Panels");
            if (panels == nullptr){
                return "(no panels)";
            }
            panels_in_file = panels->size();
            const std::string* value = panels->get_string(KEY);
            return value == nullptr ? std::string("(not there)") : *value;
        }catch (...){
            return "(not read)";
        }
    };
    auto left_beside = [&]{
        size_t files = 0;
        for (const QString& name : QDir(QString::fromStdString(SETTINGS_PATH())).entryList(QDir::Files)){
            if (name.contains(".new-")){
                files++;
            }
        }
        return files;
    };
    const size_t one_step_before = PERSISTENT_SETTINGS().writes_in_one_step;
    const size_t old_way_before = PERSISTENT_SETTINGS().writes_the_old_way;
    auto ask_from_another_thread = [&]{
        std::thread other([]{
            request_panel_settings_save();
        });
        other.join();
    };

    //  No program open.
    window.show_games();
    let_run(200);
    PERSISTENT_SETTINGS().panels[KEY] = "first";
    PERSISTENT_SETTINGS().write();
    check(in_file() == "first", "the settings file is written");
    PERSISTENT_SETTINGS().panels[KEY] = "second";
    PERSISTENT_SETTINGS().write();
    check(in_file() == "second", "the settings file is replaced by a newer one");
    check(left_beside() == 0, "nothing is left beside the settings file");

    PERSISTENT_SETTINGS().panels[KEY] = "third";
    ask_from_another_thread();
    let_run(300);
    check(in_file() == "second", "asked for with no program open: nothing is written");

    if (window.open_program(GAME, PROGRAM)){
        let_run(300);
        PERSISTENT_SETTINGS().panels[KEY] = "fourth";
        ask_from_another_thread();
        //  Not on the thread that asked: this one has not come to it yet.
        check(in_file() != "fourth", "asked for from another thread: not written on that thread");
        let_run(300);
        check(
            in_file() == "fourth" && panels_in_file >= 2,
            "asked for from another thread with a program open: its options are in the file a moment later"
        );
        check(left_beside() == 0, "nothing is left beside the settings file after that either");
    }else{
        check(false, "could not open " + GAME + ", " + PROGRAM);
    }
    window.show_games();
    let_run(300);

    //  The test's key out of the settings again.
    {
        JsonObject without;
        for (const auto& item : PERSISTENT_SETTINGS().panels){
            if (item.first != KEY){
                without[item.first] = item.second.clone();
            }
        }
        PERSISTENT_SETTINGS().panels = std::move(without);
        PERSISTENT_SETTINGS().write();
    }
    check(in_file() == "(not there)", "the file as it is without the test's own entry");

    //  Put in place in one step: every time, bar one that something else on
    //  the machine may have got in the way of.
    const size_t one_step = PERSISTENT_SETTINGS().writes_in_one_step - one_step_before;
    const size_t old_way = PERSISTENT_SETTINGS().writes_the_old_way - old_way_before;
    check(
        one_step >= 3 && old_way <= 1,
        "the settings file is put in its place in one step (" + std::to_string(one_step) + " times, " +
        std::to_string(old_way) + " times written the old way)"
    );

    logger.log(
        "Settings test: the settings file is replaced in one step (" + std::to_string(one_step) + " writes, " +
        std::to_string(old_way) + " the old way), and a program's request writes the open panel's options on "
        "the interface's thread: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) +
        " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
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
    //  The games that have nothing but Claude Plays: their list is its name,
    //  a card and that one program.
    int list_cases = 0;
    for (ClaudePlays::GameId id : ClaudePlays::all_games()){
        if (!ClaudePlays::has_only_claude_plays(id)){
            continue;
        }
        list_cases++;
        const ClaudePlays::GameProfile& profile = ClaudePlays::game_profile(id);
        const ClaudePlays::GameOnlyList list(id);
        const ImageViewRGB32& card = list.icon();
        const std::vector<PanelListDescriptor::ProgramSummary> programs = list.program_summaries();
        if (list.name() != profile.title || !card || card.width() != 256 || card.height() != 128 ||
            programs.size() != 1 || programs[0].name != "Claude Plays"
        ){
            wrong++;
            logger.log("Assistant test: failed: the list of " + profile.title + " (a card and Claude Plays)", COLOR_RED);
        }
    }
    logger.log(
        wrong == 0
            ? "Assistant test: the tools of Claude Plays are what the API takes (FireRed and LeafGreen, and " +
              std::to_string(game_cases) + " cases of the " + std::to_string(ClaudePlays::all_games().size()) +
              " other lists; " + std::to_string(list_cases) + " lists have nothing else)."
            : std::string("Assistant test: the tools of Claude Plays are not right."),
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return result.failed + wrong;
}



//  The texts of the programs that set a color for themselves (<font color="blue">,
//  written for a white background) in a shade that reads on the theme: on
//  Night and on Day, again after the theme changes while they are open, and
//  not at all in the classic themes. Returns how many checks failed.
int check_text_colors(Logger& logger){
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool ok, const char* what){
        checks++;
        if (!ok){
            wrong++;
            logger.log(std::string("Color test: FAILED: ") + what, COLOR_RED);
        }
    };

    const QString BLUE = "<font color=\"blue\" size=4><b>This is the host Switch.</b></font>";
    const QString ORANGE = "<font color='orange'>Warning: this takes a lot of memory.</font>";
    const QString RED = "<font color=\"red\">Careful.</font>";
    const QString PLAIN = "<b>No color of its own.</b>";
    const QString TWICE = "<font color=\"blue\">a</font> and <font color=\"orange\">b</font>";

    QLabel blue(BLUE);
    QLabel orange(ORANGE);
    QLabel red(RED);
    QLabel plain(PLAIN);
    QLabel twice(TWICE);
    QLabel* labels[] = {&blue, &orange, &red, &plain, &twice};

    //  The colors in a text, as colors.
    const auto colors_of = [](const QString& text){
        std::vector<Color> colors;
        static const QRegularExpression pattern("color\\s*=\\s*[\"']([^\"']+)[\"']");
        QRegularExpressionMatchIterator matches = pattern.globalMatch(text);
        while (matches.hasNext()){
            const QColor color(matches.next().captured(1));
            colors.emplace_back(0xff000000u | ((uint32_t)color.rgb() & 0xffffffu));
        }
        return colors;
    };
    //  The text without its colors: nothing else may change.
    const auto without_colors = [](QString text){
        static const QRegularExpression pattern("color\\s*=\\s*[\"'][^\"']*[\"']");
        return text.replace(pattern, "color=");
    };
    const auto readable = [&](const QString& text){
        const std::vector<Color> colors = colors_of(text);
        for (const Color& color : colors){
            if ((uint32_t)theme_readable(color) != (uint32_t)color){
                return false;
            }
        }
        return !colors.empty();
    };

    for (QLabel* label : labels){
        label->setAttribute(Qt::WA_DontShowOnScreen, true);
    }
    const auto show_all = [&]{
        for (QLabel* label : labels){
            label->hide();
            label->show();
        }
    };

    const UiThemeMode before = CURRENT_THEME;

    //  Night: blue does not read on it.
    apply_theme(UiThemeMode::POCKET_NIGHT, Starter::FIRE);
    show_all();
    check(readable(blue.text()) && readable(orange.text()) && readable(red.text()) && readable(twice.text()), "at night every color of a text reads on the background");
    check(blue.text() != BLUE, "at night blue is lightened");
    check(
        without_colors(blue.text()) == without_colors(BLUE) && without_colors(orange.text()) == without_colors(ORANGE) &&
        without_colors(twice.text()) == without_colors(TWICE),
        "only the colors change, not the rest of a text"
    );
    check(plain.text() == PLAIN, "a text without a color is left alone");
    check(colors_of(twice.text()).size() == 2, "both colors of a text are changed");

    //  Day: orange does not read on it.
    apply_theme(UiThemeMode::POCKET_DAY, Starter::WATER);
    check(readable(blue.text()) && readable(orange.text()) && readable(red.text()) && readable(twice.text()), "by day every color reads, after the theme changed while the labels were open");
    check(orange.text() != ORANGE, "by day orange is darkened");
    check(blue.text() == BLUE, "by day blue is as written again: it reads already");
    check(plain.text() == PLAIN, "a text without a color is still left alone");

    //  A text that the program sets later.
    blue.setText(ORANGE);
    show_all();
    check(readable(blue.text()) && without_colors(blue.text()) == without_colors(ORANGE), "a new text is changed as well");

    //  The classic themes: as written.
    apply_theme(UiThemeMode::DEFAULT_MODE, Starter::FIRE);
    blue.setText(BLUE);
    show_all();
    check(
        blue.text() == BLUE && orange.text() == ORANGE && red.text() == RED && twice.text() == TWICE && plain.text() == PLAIN,
        "in the classic theme the texts are as the programs wrote them"
    );

    apply_theme(before, Starter::FIRE);

    logger.log(
        "Color test: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  The output window's filter and search: lines in red and orange stay when
//  only the warnings and errors are asked for, lines that come in under the
//  filter follow it, and the search marks, counts and steps through its
//  matches. Returns how many checks failed.
int check_output_window(MainWindow& window, Logger& logger){
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool ok, const char* what){
        checks++;
        if (!ok){
            wrong++;
            logger.log(std::string("Output window test: FAILED: ") + what, COLOR_RED);
        }
    };

    LoggerWindow& output = window.output_window();
    QTextEdit* text = output.findChild<QTextEdit*>("centralwidget");
    QCheckBox* problems = output.findChild<QCheckBox*>("ppLogProblems");
    QLineEdit* search = output.findChild<QLineEdit*>("ppLogSearch");
    QPushButton* next = output.findChild<QPushButton*>("ppLogNext");
    QPushButton* previous = output.findChild<QPushButton*>("ppLogPrevious");
    QLabel* matches = output.findChild<QLabel*>("ppLogMatches");
    if (text == nullptr || problems == nullptr || search == nullptr || next == nullptr || previous == nullptr || matches == nullptr){
        logger.log("Output window test: FAILED: the window has no text, filter or search", COLOR_RED);
        return 1;
    }

    //  The text as shown (a space in the log is a non-breaking one).
    const auto shown = [&]{
        return text->toPlainText().replace(QChar(0xa0), ' ');
    };
    const auto wait_for = [&](const QString& what){
        for (int c = 0; c < 200 && !shown().contains(what); c++){
            let_run(50);
        }
        return shown().contains(what);
    };

    output.log("zq7x plain one");
    output.log("zq7x error", COLOR_RED);
    output.log("zq7x warning", COLOR_ORANGE);
    output.log("zq7x plain two", COLOR_BLUE);
    check(wait_for("zq7x plain two"), "the lines came into the window");
    {
        const QString all = shown();
        const qsizetype one = all.indexOf("zq7x plain one");
        const qsizetype error = all.indexOf("zq7x error");
        const qsizetype warning = all.indexOf("zq7x warning");
        const qsizetype two = all.indexOf("zq7x plain two");
        check(one >= 0 && one < error && error < warning && warning < two, "all four lines, in order");
    }

    //  The filter.
    problems->setChecked(true);
    check(
        shown().contains("zq7x error") && shown().contains("zq7x warning") &&
        !shown().contains("zq7x plain one") && !shown().contains("zq7x plain two"),
        "only the red and the orange line stay under the filter"
    );
    output.log("zq7x late plain");
    output.log("zq7x late error", COLOR_RED);
    check(wait_for("zq7x late error"), "a line in red comes in under the filter");
    check(!shown().contains("zq7x late plain"), "a plain line that comes in under the filter stays out");
    problems->setChecked(false);
    check(
        shown().contains("zq7x plain one") && shown().contains("zq7x late plain") && shown().contains("zq7x late error"),
        "without the filter every line is back, also the one that came in under it"
    );

    //  The search.
    search->setText("zq7x");
    check(text->extraSelections().size() == 6, "every match is marked (6)");
    check(matches->text() == PocketPilot::tr_ui("Matches: %1").arg(6), "the matches are counted");
    next->click();
    check(text->textCursor().selectedText() == "zq7x", "next selects a match");
    const int first = text->textCursor().selectionStart();
    next->click();
    const int second = text->textCursor().selectionStart();
    check(second > first, "next goes on to the next match");
    previous->click();
    check(text->textCursor().selectionStart() == first, "previous goes back");
    previous->click();
    check(text->textCursor().selectionStart() > second, "previous from the first goes round to the last");
    next->click();
    check(text->textCursor().selectionStart() == first, "next from the last goes round to the first");

    problems->setChecked(true);
    check(
        text->extraSelections().size() == 3 && matches->text() == PocketPilot::tr_ui("Matches: %1").arg(3),
        "the search follows the filter (3)"
    );
    problems->setChecked(false);
    search->setText("zq7x no such text");
    check(text->extraSelections().isEmpty() && matches->text() == PocketPilot::tr_ui("No matches"), "no matches");
    search->clear();
    check(text->extraSelections().isEmpty() && matches->text().isEmpty(), "an empty search marks nothing");

    logger.log(
        "Output window test: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}

//  Options that are made for the test of the search: a group with two
//  options, one with an accent in it, a loose one, a group that is closed
//  (what is in it is not made), and one that the program hides.
class SearchTestOptions : public BatchOption{
public:
    SearchTestOptions()
        : BatchOption(LockMode::UNLOCK_WHILE_RUNNING)
        , alpha("Alpha group", LockMode::UNLOCK_WHILE_RUNNING)
        , beta("Beta option", LockMode::UNLOCK_WHILE_RUNNING, false)
        , gamma("Gamma option with caf\xc3\xa9", LockMode::UNLOCK_WHILE_RUNNING, false)
        , delta("Delta option", LockMode::UNLOCK_WHILE_RUNNING, false)
        , closed("Closed group", LockMode::UNLOCK_WHILE_RUNNING, GroupOption::EnableMode::ALWAYS_ENABLED, false, false)
        , inside("Inside option", LockMode::UNLOCK_WHILE_RUNNING, false)
        , secret("Secret option", LockMode::UNLOCK_WHILE_RUNNING, false)
    {
        alpha.add_option(beta, "beta");
        alpha.add_option(gamma, "gamma");
        closed.add_option(inside, "inside");
        secret.set_visibility(ConfigOptionState::HIDDEN);
        add_option(alpha, "alpha");
        add_option(delta, "delta");
        add_option(closed, "closed");
        add_option(secret, "secret");
    }

    GroupOption alpha;
    BooleanCheckBoxOption beta;
    BooleanCheckBoxOption gamma;
    BooleanCheckBoxOption delta;
    GroupOption closed;
    BooleanCheckBoxOption inside;
    BooleanCheckBoxOption secret;
};

//  The search field of the settings: on the options of the settings page as
//  they are, and on a few that are made for it. The settings page has to be
//  open. Returns how many checks failed.
int check_settings_search(MainWindow& window, Logger& logger){
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool ok, const char* what){
        checks++;
        if (!ok){
            wrong++;
            logger.log(std::string("Settings search test: FAILED: ") + what, COLOR_RED);
        }
    };

    //  The settings page.
    QLineEdit* search = window.findChild<QLineEdit*>("ppOptionsSearch");
    QLabel* none = window.findChild<QLabel*>("ppOptionsSearchNone");
    if (search == nullptr || none == nullptr || search->parentWidget() == nullptr){
        logger.log("Settings search test: FAILED: the settings page has no search field", COLOR_RED);
        return 1;
    }
    QWidget* panel = search->parentWidget();
    const auto text_of = [](QWidget* widget) -> QString{
        if (QLabel* label = qobject_cast<QLabel*>(widget)){
            return label->text();
        }
        if (QAbstractButton* button = qobject_cast<QAbstractButton*>(widget)){
            return button->text();
        }
        return QString();
    };
    //  An option of the page, by a piece of its explanation.
    const auto option_with = [&](const QString& piece) -> QWidget*{
        for (QWidget* widget : panel->findChildren<QWidget*>()){
            if (text_of(widget).contains(piece)){
                return widget;
            }
        }
        return nullptr;
    };
    const auto shown = [&](const QString& piece){
        QWidget* widget = option_with(piece);
        return widget != nullptr && widget->isVisibleTo(panel);
    };
    const auto hidden_by_program = [&]{
        int count = 0;
        for (QWidget* widget : panel->findChildren<QWidget*>()){
            if (widget->isHidden() && !widget->property("ppSearchHidden").toBool()){
                count++;
            }
        }
        return count;
    };
    const auto hidden_by_search = [&]{
        int count = 0;
        for (QWidget* widget : panel->findChildren<QWidget*>()){
            if (widget->property("ppSearchHidden").toBool()){
                count++;
            }
        }
        return count;
    };

    check(search->text().isEmpty() && none->isHidden(), "the field starts empty, without the message");
    check(option_with("Holds off") != nullptr && option_with("The language of the menus") != nullptr, "the settings have the options the test looks for");
    check(shown("Holds off") && shown("The language of the menus"), "every option is shown to begin with");
    const int hidden_before = hidden_by_program();

    search->setText("holds off");
    check(shown("Holds off") && !shown("The language of the menus"), "a word of the explanation finds the option and hides the others");
    check(none->isHidden(), "no message when something is found");
    search->setText("  HOLDS   Off ");
    check(shown("Holds off") && !shown("The language of the menus"), "capitals and extra spaces do not matter");
    search->setText("francais");
    check(shown("The language of the menus") && !shown("Holds off"), "a word without the accent finds the language (Français in its list)");
    search->setText("qqqq zzzz");
    check(!shown("Holds off") && !shown("The language of the menus"), "nothing is shown for words that nothing has");
    check(!none->isHidden(), "the message comes when nothing is found");
    search->clear();
    check(shown("Holds off") && shown("The language of the menus") && none->isHidden(), "clearing shows everything again, without the message");
    check(hidden_by_search() == 0, "nothing stays hidden by the search once it is cleared");
    check(hidden_by_program() == hidden_before, "what the program hides is the same before and after");

    //  The options that are made for it.
    SearchTestOptions options;
    QWidget host;
    QVBoxLayout* layout = new QVBoxLayout(&host);
    ConfigWidget* widget = ConfigWidget::make_from_option(options, &host);
    layout->addWidget(&widget->widget());
    host.setAttribute(Qt::WA_DontShowOnScreen, true);
    host.show();
    let_run(50);

    const auto row_of = [&](ConfigOption& option) -> QWidget*{
        for (QWidget* inner : host.findChildren<QWidget*>()){
            ConfigWidget* config = dynamic_cast<ConfigWidget*>(inner);
            if (config != nullptr && &config->option() == &option){
                return inner;
            }
        }
        return nullptr;
    };
    const auto is_shown = [&](ConfigOption& option){
        QWidget* row = row_of(option);
        return row != nullptr && row->isVisibleTo(&host);
    };
    //  Which of them are shown: group, beta, gamma, delta, closed group, secret.
    const auto pattern = [&]{
        std::string text;
        for (ConfigOption* option : std::initializer_list<ConfigOption*>{
            &options.alpha, &options.beta, &options.gamma, &options.delta, &options.closed, &options.secret
        }){
            text += is_shown(*option) ? '1' : '0';
        }
        return text;
    };
    QWidget* list = &widget->widget();

    check(row_of(options.beta) != nullptr && row_of(options.inside) == nullptr, "the group that is open has its options and the closed one has not");
    check(pattern() == "111110", "to begin with: all shown but the one that the program hides");
    check(PocketPilot::filter_options(*list, "") == 4, "four options (the closed group counts as one)");

    check(PocketPilot::filter_options(*list, "beta") == 1 && pattern() == "110000", "an option is shown with the group it is in, and the others are not");
    check(PocketPilot::filter_options(*list, "alpha") == 2 && pattern() == "111000", "the title of a group shows everything in it");
    check(PocketPilot::filter_options(*list, "alpha beta") == 1 && pattern() == "110000", "a word for the group and a word for the option");
    check(PocketPilot::filter_options(*list, "beta gamma") == 0 && pattern() == "000000", "words that are in different options find nothing");
    check(PocketPilot::filter_options(*list, "CAFE") == 1 && pattern() == "101000", "an accent does not matter, a capital neither");
    check(PocketPilot::filter_options(*list, "option") == 3 && pattern() == "111100", "what the program hides is not found, and stays hidden");
    check(PocketPilot::filter_options(*list, "secret") == 0 && pattern() == "000000", "the hidden option is not shown for its own name");
    check(PocketPilot::filter_options(*list, "closed") == 1 && pattern() == "000010", "a group that is closed is found by its title");
    check(PocketPilot::filter_options(*list, "inside") == 0 && pattern() == "000000", "a group that is closed is not searched inside");
    check(PocketPilot::filter_options(*list, "delta") == 1 && pattern() == "000100", "a loose option");
    check(PocketPilot::filter_options(*list, "   ") == 4 && pattern() == "111110", "only spaces show everything again, the hidden one still hidden");
    {
        QWidget* secret = row_of(options.secret);
        check(secret != nullptr && secret->isHidden() && !secret->property("ppSearchHidden").toBool(), "the option of the program was never taken over by the search");
    }

    logger.log(
        "Settings search test: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  The report of where the time of a run went: how a time is written, the
//  lines for a run with goes that did not come through, a run of nothing, and
//  the timer that counts a go as lost when it ends in an exception.
int check_story_times(Logger& logger){
    using namespace NintendoSwitch::PokemonFRLG;
    namespace Times = AutoStoryTimes;
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("Story times test: FAILED: ") + what, COLOR_RED);
        }
    };
    const auto seconds = [](int64_t count){ return Milliseconds(count * 1000); };

    check(Times::time_text(Milliseconds(0)) == "0 s", "no time");
    check(Times::time_text(Milliseconds(499)) == "0 s" && Times::time_text(Milliseconds(500)) == "1 s", "rounded to the second");
    check(Times::time_text(Milliseconds(-5000)) == "0 s", "no time below nothing");
    check(Times::time_text(seconds(42)) == "42 s", "seconds");
    check(Times::time_text(seconds(25 * 60 + 10)) == "25 min 10 s", "minutes and seconds");
    check(Times::time_text(seconds(3599)) == "59 min 59 s", "just under an hour");
    check(Times::time_text(Milliseconds(3599500)) == "1 h 0 min", "just over: an hour");
    check(Times::time_text(seconds(2 * 3600 + 5 * 60 + 40)) == "2 h 5 min", "hours and minutes");

    Times::Times times;
    check(times.report(seconds(100)).empty(), "no lines for a run without a segment");
    times.add("C01_01", "Alpha", 1, seconds(600), true);
    times.add("C02_01", "Beta", 2, seconds(300), false);
    times.add("C02_01", "Beta", 2, seconds(100), true);
    times.add("C02_02", "Skipped", 2, Milliseconds(400), true);
    check(times.goes() == 4 && times.failed_goes() == 1, "the goes are counted");
    const std::vector<std::string> lines = times.report(seconds(1200));
    check(lines.size() == 4, "a line for the run, two segments and the chapters");
    if (lines.size() == 4){
        check(
            lines[0] == "Where the time went: 20 min 0 s in this run, 16 min 40 s (83 %) in 2 segments, "
            "of which 5 min 0 s (25 %) in goes that did not come through. The rest (3 min 20 s) is the "
            "start, the loads after a reset and what the program does between the segments.",
            "the line for the run"
        );
        check(lines[1] == "  C01_01 (Alpha): 10 min 0 s (50 %)", "the longest segment");
        check(
            lines[2] == "  C02_01 (Beta): 6 min 40 s (33 %), 2 goes, 1 did not come through",
            "the next one, with its goes"
        );
        check(lines[3] == "Longest chapters: 1: 10 min 0 s (50 %), 2: 6 min 40 s (33 %)", "the chapters");
    }
    check(times.report(seconds(1200), 1).size() == 3, "the number of segments listed can be set");
    //  A run time that is shorter than its segments (a clock that went
    //  back): the segments' time is taken for it.
    {
        const std::vector<std::string> short_run = times.report(seconds(10));
        check(
            !short_run.empty() &&
            short_run[0].find("16 min 40 s in this run, 16 min 40 s (100 %)") != std::string::npos,
            "never over 100 %"
        );
    }
    //  One chapter: no line for the chapters. Nothing lost: no word of it.
    {
        Times::Times one;
        one.add("C03_01", "Gamma", 3, seconds(90), true);
        const std::vector<std::string> only = one.report(seconds(100));
        check(
            only.size() == 2 && only[0].find("goes that did not come through") == std::string::npos &&
            only[0].find("in 1 segment.") != std::string::npos,
            "one chapter, nothing lost"
        );
    }
    //  The timer.
    {
        Times::Times timed;
        {
            Times::Timer timer(timed, "T01", "Done", 1);
            timer.done();
        }
        try{
            Times::Timer timer(timed, "T02", "Left by an exception", 1);
            throw std::runtime_error("stop");
        }catch (const std::runtime_error&){
        }
        check(timed.goes() == 2 && timed.failed_goes() == 1, "a go that ends in an exception is lost, one that was done is not");
    }

    logger.log(
        "Story times test: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  The clip of a shiny: its file name, how its size is written, and what is
//  small enough to send. (The saving and the message need a console.)
int check_shiny_clip(Logger& logger){
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("Shiny clip test: FAILED: ") + what, COLOR_RED);
        }
    };
    const std::string stamp = "20261126-153012123456";
    check(
        clip_file_name("ShinyHunt-Overworld", stamp) == "Shiny-ShinyHunt-Overworld-20261126-153012123456.mp4",
        "the name has the program and the time"
    );
    check(clip_file_name("Legendary Reset", stamp) == "Shiny-Legendary-Reset-" + stamp + ".mp4", "a space becomes a dash");
    check(clip_file_name("a/b\\c:d*e?\"f<g>h|i", stamp) == "Shiny-abcdefghi-" + stamp + ".mp4", "what a file name cannot have is left out");
    check(clip_file_name("../../x", stamp) == "Shiny-x-" + stamp + ".mp4", "no way out of the folder");
    check(clip_file_name("", stamp) == "Shiny-" + stamp + ".mp4", "no program");
    check(clip_file_name("X", "a:b") == "Shiny-X.mp4", "a time with nothing of a time in it");
    check(clip_file_name("X", "../../y") == "Shiny-X.mp4", "no way out of the folder through the time");

    check(clip_size_text(0) == "0 KB", "no size");
    check(clip_size_text(1) == "1 KB" && clip_size_text(1024) == "1 KB" && clip_size_text(1025) == "2 KB", "a size under a megabyte is rounded up");
    check(clip_size_text(812 * 1024) == "812 KB", "kilobytes");
    check(clip_size_text(1024 * 1024) == "1.0 MB" && clip_size_text(6710886) == "6.4 MB", "megabytes");

    check(!clip_can_be_sent(0), "an empty clip is not sent");
    check(clip_can_be_sent(1) && clip_can_be_sent(CLIP_SEND_LIMIT), "a small clip is sent, also at the limit");
    check(!clip_can_be_sent(CLIP_SEND_LIMIT + 1), "a clip over the limit is not");

    logger.log(
        "Shiny clip test: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  The frames of the stream history: a frame as a capture card gives it
//  (NV12), through compress_video_frame() and back. The four colors in it
//  and their places must be those of Qt's own conversion, whichever of the
//  two conversions the converter uses.
int check_history_frames(Logger& logger){
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("History frames test: FAILED: ") + what, COLOR_RED);
        }
    };

    //  Four quarters: red, green, blue and gray, in BT.601 as the video of a
    //  console has them.
    struct Yuv{
        uchar y;
        uchar u;
        uchar v;
    };
    const Yuv quarters[4] = {{82, 90, 240}, {145, 54, 34}, {41, 240, 110}, {128, 128, 128}};
    constexpr int WIDTH = 64;
    constexpr int HEIGHT = 48;
    QVideoFrame frame(QVideoFrameFormat(QSize(WIDTH, HEIGHT), QVideoFrameFormat::Format_NV12));
    if (!frame.isValid() || !frame.map(QVideoFrame::WriteOnly) || frame.planeCount() < 2){
        logger.log("History frames test: FAILED: no NV12 frame could be made to test with", COLOR_RED);
        return 1;
    }
    for (int row = 0; row < HEIGHT; row++){
        uchar* line = frame.bits(0) + (ptrdiff_t)row * frame.bytesPerLine(0);
        for (int x = 0; x < WIDTH; x++){
            line[x] = quarters[(row >= HEIGHT / 2 ? 2 : 0) + (x >= WIDTH / 2 ? 1 : 0)].y;
        }
    }
    for (int row = 0; row < HEIGHT / 2; row++){
        uchar* line = frame.bits(1) + (ptrdiff_t)row * frame.bytesPerLine(1);
        for (int x = 0; x < WIDTH / 2; x++){
            const Yuv& color = quarters[(row >= HEIGHT / 4 ? 2 : 0) + (x >= WIDTH / 4 ? 1 : 0)];
            line[2 * x] = color.u;
            line[2 * x + 1] = color.v;
        }
    }
    frame.unmap();

    //  What Qt makes of the frame is what the history showed before.
    const QImage qt = frame.toImage();
    check(!qt.isNull() && qt.width() == WIDTH && qt.height() == HEIGHT, "Qt converts the frame");
    if (qt.isNull()){
        return wrong + 1;
    }

    //  The middle of each quarter: the left and right, top and bottom of the
    //  picture, at any size the history is set to.
    const auto close = [](const QColor& a, const QColor& b){
        return std::abs(a.red() - b.red()) <= 24 && std::abs(a.green() - b.green()) <= 24 && std::abs(a.blue() - b.blue()) <= 24;
    };
    VideoFrameConverter converter(logger);
    bool all_close = true;
    bool all_there = true;
    int size_wrong = 0;
    for (int attempt = 0; attempt < 3; attempt++){
        const std::vector<uchar> jpeg = compress_video_frame(converter, frame);
        if (jpeg.empty()){
            all_there = false;
            continue;
        }
        const QImage back = decompress_video_frame(jpeg);
        if (back.isNull()){
            all_there = false;
            continue;
        }
        //  The same shape as the frame, whatever the size.
        if (back.width() * HEIGHT != back.height() * WIDTH){
            size_wrong++;
        }
        for (int quarter = 0; quarter < 4; quarter++){
            const double fx = (quarter % 2 == 0) ? 0.25 : 0.75;
            const double fy = (quarter / 2 == 0) ? 0.25 : 0.75;
            const QColor expected = qt.pixelColor((int)(fx * qt.width()), (int)(fy * qt.height()));
            const QColor got = back.pixelColor((int)(fx * back.width()), (int)(fy * back.height()));
            if (!close(expected, got)){
                all_close = false;
                logger.log(
                    "History frames test: quarter " + std::to_string(quarter) + ": Qt " + expected.name().toStdString() +
                    ", the history " + got.name().toStdString(),
                    COLOR_RED
                );
            }
        }
    }
    check(all_there, "a frame is a JPEG and back");
    check(size_wrong == 0, "the picture keeps the shape of the frame");
    check(all_close, "the colors of the four quarters are Qt's, in the same places (red, green, blue, gray)");

    //  A frame without anything in it is left out, not an error.
    check(compress_video_frame(converter, QVideoFrame()).empty(), "an empty frame gives nothing");

    logger.log(
        "History frames test: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  The periodic runner (Common/Cpp/Concurrency/PeriodicRunner.h): a runnable
//  that is removed before it has run once. Its place in the schedule was found
//  by the time of its entry, which was not the time the place was made for, so
//  the place stayed behind and the runner's thread ran what was not there any
//  more: a crash in the first moments of a program that starts and stops at
//  once (the program stops its serial port poller on every way out, also when
//  it was made a moment before). This was a fault of Pokemon Automation's code.
struct CountingRunnable : public PeriodicRunner::Runnable{
    std::atomic<int> count{0};
    virtual void run() noexcept override{
        count++;
    }
};
int check_periodic_runner(Logger& logger){
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("Periodic runner test: FAILED: ") + what, COLOR_RED);
        }
    };

    {
        PeriodicRunner runner;
        CountingRunnable removed;
        CountingRunnable kept;
        CountingRunnable twin;
        runner.add_runnable(removed, std::chrono::seconds(1));
        runner.remove_runnable(removed);
        runner.add_runnable(kept, std::chrono::milliseconds(50));
        //  Several in one moment: the removal takes its own place, not all of the
        //  places with the same time.
        runner.add_runnable(twin, std::chrono::milliseconds(50));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        check(kept.count >= 3, "a runnable that stays is run again and again, after another was removed");
        check(twin.count >= 3, "and another that was added in the same moment");
        runner.remove_runnable(twin);
        const int before = kept.count;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        check(kept.count > before, "one is removed and the other goes on");
        const int twin_count = twin.count;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        check(twin.count == twin_count, "the one that was removed is not run any more");
        runner.remove_runnable(kept);
        runner.stop();
    }

    logger.log(
        "Periodic runner test: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  The program's name, and the settings that a computer has from the time
//  the program was called SerialPrograms: the first start under the new name
//  takes them over. A folder with a letter outside ASCII in its name, as a
//  user's folder can have.
int check_old_settings(Logger& logger){
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const char* what){
        checks++;
        if (!fine){
            wrong++;
            logger.log(std::string("Old settings test: FAILED: ") + what, COLOR_RED);
        }
    };

    check(PROGRAM_NAME == "Claude GG", "the program is called Claude GG");
#ifdef _WIN32
    //  The file of the program is named after it, and so are its settings.
    check(
        Filesystem::application_binary_name().replace_extension().string() == "ClaudeGG",
        "the program's file is ClaudeGG.exe"
    );
    const std::string settings_path = PROGRAM_SETTING_JSON_PATH();
    const std::string wanted_end = "ClaudeGG-Settings.json";
    check(
        settings_path.size() >= wanted_end.size() &&
        settings_path.compare(settings_path.size() - wanted_end.size(), wanted_end.size(), wanted_end) == 0,
        "its settings file is ClaudeGG-Settings.json"
    );
#endif

    QTemporaryDir temp;
    if (!temp.isValid()){
        logger.log("Old settings test: FAILED: no temporary folder to test in", COLOR_RED);
        return wrong + 1;
    }
    const QString folder = temp.path() + "/Dénnis folder/";
    QDir().mkpath(folder);
    const std::string folder_utf8 = folder.toStdString();
    const std::string new_name = "ClaudeGG-Settings.json";
    const QString old_file = folder + PocketPilot::OLD_SETTINGS_FILE_NAME;
    const QString new_file = folder + QString::fromStdString(new_name);
    const auto write = [](const QString& path, const char* text){
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)){
            file.write(text);
        }
    };
    const auto read = [](const QString& path){
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("(not there)");
    };

    //  A computer that never had settings.
    check(!PocketPilot::take_over_old_settings(folder_utf8, new_name), "nothing to take over: nothing happens");
    check(!QFile::exists(new_file), "and no file is made");

    //  The old file only: copied, and left where it is.
    write(old_file, "{\"20-GlobalSettings\": {\"a\": 1}}");
    check(PocketPilot::take_over_old_settings(folder_utf8, new_name), "the old file is taken over");
    check(read(new_file) == QByteArray("{\"20-GlobalSettings\": {\"a\": 1}}"), "the new file has what the old one had");
    check(read(old_file) == QByteArray("{\"20-GlobalSettings\": {\"a\": 1}}"), "the old file is still there, unchanged");

    //  Both: the new one is the settings now, and is never overwritten.
    write(new_file, "{\"20-GlobalSettings\": {\"a\": 2}}");
    check(!PocketPilot::take_over_old_settings(folder_utf8, new_name), "with both files, nothing is taken over");
    check(read(new_file) == QByteArray("{\"20-GlobalSettings\": {\"a\": 2}}"), "the new settings are not overwritten");

    //  A program that still has the old name has nothing to take over.
    QFile::remove(new_file);
    check(!PocketPilot::take_over_old_settings(folder_utf8, PocketPilot::OLD_SETTINGS_FILE_NAME), "the old name takes nothing from itself");
    check(!PocketPilot::take_over_old_settings(folder_utf8, ""), "no name takes nothing");
    check(!QFile::exists(new_file), "and no file is made");

    logger.log(
        "Old settings test: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


//  "Check for updates" in the About window: reading the list of releases, the
//  verdict for a build, the request (and that nothing is asked before the
//  button is pressed), and the window with its button. Returns how many
//  checks failed.
int check_updates(Logger& logger){
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool ok, const char* what){
        checks++;
        if (!ok){
            wrong++;
            logger.log(std::string("Update test: FAILED: ") + what, COLOR_RED);
        }
    };

    //  Where the releases are.
    const ReleaseLocation location = release_location(PROJECT_URL);
    check(
        location.api_url == "https://api.github.com/repos/MajorLOL/Arduino-Source/releases?per_page=30" &&
        location.page_url == "https://github.com/MajorLOL/Arduino-Source/releases" &&
        location.tag_page_url == "https://github.com/MajorLOL/Arduino-Source/releases/tag/",
        "the addresses of the releases"
    );
    check(!release_location("https://example.com/MajorLOL/Arduino-Source/tree/x").valid(), "not another site's address");
    check(!release_location("https://github.com/../Arduino-Source/tree/x").valid(), "no \"..\" for an owner");
    check(!release_location("https://github.com/a b/c/tree/x").valid(), "no space in a name");
    check(!release_location("https://github.com/MajorLOL").valid(), "an address without a repository");
    check(!release_location(QString()).valid(), "no address");

    //  The numbers.
    check(build_of_tag("pocketpilot-build-88") == 88, "the number in a name of a build");
    check(build_of_tag("pocketpilot-build-") == 0, "a name without a number");
    check(build_of_tag("pocketpilot-build-8x") == 0, "a name with something after the number");
    check(build_of_tag("pocketpilot-build-0") == 0, "build 0 is none");
    check(build_of_tag("pocketpilot-build-1234567890") == 0, "a number that is too long");
    check(build_of_tag("v1.2.3") == 0, "a release that is not a build");
    check(build_of_this_program("88") == 88 && build_of_this_program(" 12 ") == 12, "the number of this program");
    check(build_of_this_program("local") == 0 && build_of_this_program(QString()) == 0, "a local build has no number");

    //  The newest build: by number (9 is before 88), no draft, no other release.
    {
        LatestBuild latest;
        QString error;
        check(newest_build(RELEASES_JSON, latest, error), "the list of releases was read");
        check(latest.build == 88 && latest.date == "2026-11-25", "the newest build is 88, from the 25th");
        check(newest_build("[]", latest, error) && latest.build == 0, "an empty list has no build");
        check(newest_build("[{\"tag_name\":\"v1\"},5,\"x\",null]", latest, error) && latest.build == 0, "odd entries are passed over");
        error.clear();
        check(!newest_build("not json", latest, error) && !error.isEmpty(), "text that is not JSON");
        error.clear();
        check(!newest_build("{\"message\":\"Not Found\"}", latest, error) && !error.isEmpty(), "an object is not a list");
    }

    //  The verdict.
    {
        const auto verdict = [&](int status, const QByteArray& body, int current){
            return update_result(releases_answer(status, body), location, current);
        };
        UpdateResult result = verdict(200, RELEASES_JSON, 80);
        check(result.state == UpdateState::NEWER_AVAILABLE && result.latest.build == 88, "build 80 is told that 88 is out");
        check(
            result.page_url == "https://github.com/MajorLOL/Arduino-Source/releases/tag/pocketpilot-build-88",
            "the page is made from the number, not taken from the answer"
        );
        const QString text = update_text(result, "#123456");
        check(
            text.contains("href=\"" + result.page_url + "\"") && !text.contains("evil.example") &&
            text.contains("88") && text.contains("80") && text.contains("2026-11-25"),
            "the text of a newer build: both numbers, the date and the link"
        );
        check(verdict(200, RELEASES_JSON, 88).state == UpdateState::UP_TO_DATE, "build 88 is up to date");
        check(!update_text(verdict(200, RELEASES_JSON, 88), "#123456").contains("href"), "no link when it is up to date");
        check(verdict(200, RELEASES_JSON, 90).state == UpdateState::NEWER_THAN_PUBLISHED, "build 90 is newer than what is published");
        result = verdict(200, RELEASES_JSON, 0);
        check(result.state == UpdateState::LOCAL_BUILD && result.latest.build == 88, "a local build is told the newest one, with no verdict");
        result = verdict(200, "[]", 80);
        check(result.state == UpdateState::NO_BUILDS && result.page_url == location.page_url, "no builds are published");
        for (int status : {403, 404, 429, 500, 301}){
            result = verdict(status, "{}", 80);
            check(result.state == UpdateState::FAILED && !result.error.isEmpty(), "a status that is not an answer");
        }
        check(verdict(200, "not json", 80).state == UpdateState::FAILED, "an answer that cannot be read");
        Assistant::HttpResponse broken;
        broken.error = "Host not found <b>";
        result = update_result(broken, location, 80);
        const QString failed_text = update_text(result, "#123456");
        check(
            result.state == UpdateState::FAILED && failed_text.contains("Host not found") &&
            !failed_text.contains("<b>") && !failed_text.contains("href"),
            "no connection: the reason, in plain text"
        );
    }

    //  The request, and what happens to it.
    {
        ScriptedTransport transport;
        transport.answer = releases_answer(200, RELEASES_JSON);
        UpdateCheck update(transport, PROJECT_URL, "80");
        check(transport.requests.empty() && !update.running(), "nothing is asked before start");
        int done_calls = 0;
        UpdateResult got;
        update.start([&](const UpdateResult& result){
            done_calls++;
            got = result;
        });
        check(update.running() && done_calls == 0 && transport.requests.size() == 1, "start sends one request and answers later");
        update.start([&](const UpdateResult&){
            done_calls += 100;
        });
        check(transport.requests.size() == 1, "a second start while one runs sends nothing");
        let_run(50);
        check(done_calls == 1 && !update.running() && got.state == UpdateState::NEWER_AVAILABLE, "the answer comes once");
        if (!transport.requests.empty()){
            const Assistant::HttpRequest& request = transport.requests[0];
            bool agent = false;
            bool accept = false;
            bool secret = false;
            for (const Assistant::HttpHeader& header : request.headers){
                const QByteArray name = header.name.toLower();
                agent = agent || name == "user-agent";
                accept = accept || (name == "accept" && header.value.contains("github"));
                secret = secret || name == "authorization" || name == "x-api-key" || name == "cookie";
            }
            check(
                request.method == "GET" && request.url == QUrl(location.api_url) && request.body.isEmpty() && agent && accept,
                "a plain GET of the list, with a user agent"
            );
            check(!secret, "the request carries no key, token or cookie");
        }
        //  Again after the answer.
        update.start([&](const UpdateResult&){
            done_calls++;
        });
        let_run(50);
        check(done_calls == 2 && transport.requests.size() == 2, "a check can be made again");
    }
    {
        //  Stopped by its end: nothing is called afterwards.
        ScriptedTransport transport;
        transport.answer = releases_answer(200, RELEASES_JSON);
        int done_calls = 0;
        {
            UpdateCheck update(transport, PROJECT_URL, "80");
            update.start([&](const UpdateResult&){
                done_calls++;
            });
        }
        let_run(50);
        check(done_calls == 0 && transport.calls.size() == 1 && transport.calls[0]->aborted, "a check that is destroyed stops, and says nothing");
    }
    {
        //  A fork without an address on github.com.
        ScriptedTransport transport;
        UpdateCheck update(transport, "https://example.com/x/y/tree/z", "80");
        UpdateResult got;
        int done_calls = 0;
        update.start([&](const UpdateResult& result){
            done_calls++;
            got = result;
        });
        let_run(50);
        check(
            done_calls == 1 && got.state == UpdateState::FAILED && transport.requests.empty(),
            "no address: an error, and nothing sent"
        );
    }

    //  The window.
    {
        AboutInfo info;
        info.name = "PocketPilot";
        info.build = "80";
        info.upstream_name = "Pokemon Automation";
        info.upstream_version = "1";
        info.architecture = "x86_64";
        info.compiler = "test";
        info.project_url = PROJECT_URL;
        info.upstream_url = "https://github.com/PokemonAutomation/";

        std::unique_ptr<QDialog> plain = make_about(nullptr, info);
        check(plain->findChild<QPushButton*>("ppAboutUpdateButton") == nullptr, "no button without a connection");
        plain.reset();

        ScriptedTransport transport;
        transport.answer = releases_answer(200, RELEASES_JSON);
        std::unique_ptr<QDialog> about = make_about(nullptr, info, &transport);
        about->setAttribute(Qt::WA_DontShowOnScreen, true);
        about->show();
        QPushButton* button = about->findChild<QPushButton*>("ppAboutUpdateButton");
        QLabel* label = about->findChild<QLabel*>("ppAboutUpdateResult");
        check(button != nullptr && label != nullptr, "the window has the button and the place for the answer");
        if (button != nullptr && label != nullptr){
            let_run(50);
            check(transport.requests.empty() && label->text().isEmpty(), "the window asks nothing by itself");
            button->click();
            check(!button->isEnabled() && !label->text().isEmpty() && transport.requests.size() == 1, "the button asks once and waits");
            button->click();
            check(transport.requests.size() == 1, "pressed again while it waits: nothing more");
            let_run(50);
            check(
                button->isEnabled() && label->text().contains("88") && label->text().contains("releases/tag/pocketpilot-build-88"),
                "the answer is in the window, with the link"
            );
            check(label->openExternalLinks(), "the link opens in the browser");

            //  A window closed while the check runs.
            transport.silent = true;
            button->click();
            check(transport.requests.size() == 2 && !button->isEnabled(), "a check that is waiting");
        }
        about->close();
        about.reset();
        check(transport.calls.size() == 2 && transport.calls[1]->aborted, "closing the window stops the check");
    }

    logger.log(
        "Update test: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
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
    //  (Two REMATCH steps on the money trip, 4 + 5 battles; a third one since
    //  build 35, two battles for the evolution stones; build 54: the same
    //  once more for the stones of the Eevee (STONES_2, 2 battles), and the
    //  same two as on the money trip on the way to each of the three
    //  legendary Pokemon, for their ULTRA BALLs: 2 + 1 + 1 + 3 * 2 = 10
    //  steps, 9 + 2 + 2 + 3 * 9 = 40 battles; build 56: three trips to Lady
    //  Gillian more for the coins of PORYGON: 6 steps, 27 battles; build 57:
    //  two visits more for the vitamins of HITMONTOP: 4 steps, 18 battles.)
    //  Build 62: the last trip of the PORYGON money is one round of four battles, not two
    //  of 4 and 5 (22 battles, 215600, for the 199000 of the packs): 5 steps, 22 battles
    //  there, so 19 steps and 80 battles in all.
    check(
        money_segment && rematches == 19 && rematch_wins == 80 && walks_fine && purchases == 4,
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
        //  The prize money for the Ultra Balls of the three legendaries of
        //  the Sevii Islands (build 54): the same way to Lady Gillian twice
        //  each, so they count as money trips too.
        const std::string segment_id = segment.id;
        const bool money =
            segment_id.find("_MONEY") != std::string::npos ||
            segment_id.find("_HO_OH") != std::string::npos ||
            segment_id.find("_LUGIA") != std::string::npos ||
            segment_id.find("_DEOXYS") != std::string::npos ||
            segment_id.find("_PORYGON") != std::string::npos ||
            segment_id.find("_VITAMINS") != std::string::npos;
        //  The prize money for the evolution stones (build 35): the same
        //  way to Lady Gillian, once (and once more in build 54).
        const bool stones = segment_id.find("_STONES") != std::string::npos;
        for (size_t k = 0; k < segment.step_count; k++){
            const StoryStep& step = script.step(segment, k);
            if (step.type != StepType::WALK || step.leg < 0){
                continue;
            }
            const StoryLeg& leg = script.leg(step.leg);
            const StoryRun* runs = script.leg_runs(leg);
            //  ... and the way to the Lax Incense in the Lost Cave for the
            //  Wynaut egg (build 54): the same water once more.
            const bool cave =
                std::string(leg.name).find("LostCave") != std::string::npos ||
                std::string(leg.name).find("LaxIncense") != std::string::npos;
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
    //  (4 such runs to a round of Lady Gillian: 17 rounds since build 62, 18 before: the last trip of
    //  the PORYGON money is one round.)
    check(
        money_bumps == 68 && stones_bumps == 8 && bumps_last && longest_other <= 4,
        "the money trips in the story's data: the water to Resort Gorgeous and back"
    );
    check(cave_bumps == 12 && lap_bumps == 4, "the Lost Cave and the laps off Five Island in the story's data: the same water");

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
                //  (7: in a Safari Game too, build 52.)
                fishing_fine &= (step.p2 == 6 || step.p2 == 7) && (step.p4 == 1 || step.p4 == 2) && step.dir != DPAD_NONE &&
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
            if (step.type == StepType::SKIP_IF_ITEM && step.p2 > 0){
                //  The way to a rod (p2 > 0; build 54: the way to the LAX
                //  INCENSE has p2 0 and is none): skipped when nothing is fished for with
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
    check(fishing_places == 15 && fishing_fine, "the places fished at, in the story's data");
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
        check(next_steps == 114 && last_town + 1 + next_steps == script.segment_count, "the next steps come after the towns");
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
//  HITMONTOP by vitamins and the number in the Game Corner's coins window (build 57,
//  experimental): the pure parts of both (PokemonFRLG_AutoStoryBoxes.h,
//  PokemonFRLG_AutoStoryCoins.h), and the steps the story's data has for them.
int check_vitamins_and_coins(Logger& logger){
    using namespace NintendoSwitch;
    using namespace NintendoSwitch::PokemonFRLG;
    namespace Boxes = AutoStoryBoxes;
    namespace Coins = AutoStoryCoins;
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const std::string& what){
        checks++;
        if (!fine){
            wrong++;
            logger.log("AutoStory test: failed: " + what, COLOR_RED);
        }
    };

    //  The 25 natures: what they do to the Attack and the Defense.
    {
        struct Nature{
            const char* name;
            int attack;
            int defense;
        };
        static const Nature natures[] = {
            {"Hardy", 100, 100}, {"Lonely", 110, 90}, {"Brave", 110, 100}, {"Adamant", 110, 100}, {"Naughty", 110, 100},
            {"Bold", 90, 110}, {"Docile", 100, 100}, {"Relaxed", 100, 110}, {"Impish", 100, 110}, {"Lax", 100, 110},
            {"Timid", 90, 100}, {"Hasty", 100, 90}, {"Serious", 100, 100}, {"Jolly", 100, 100}, {"Naive", 100, 100},
            {"Modest", 90, 100}, {"Mild", 100, 90}, {"Quiet", 100, 100}, {"Bashful", 100, 100}, {"Rash", 100, 100},
            {"Calm", 90, 100}, {"Gentle", 100, 90}, {"Sassy", 100, 100}, {"Careful", 100, 100}, {"Quirky", 100, 100},
        };
        size_t fine = 0;
        for (const Nature& nature : natures){
            int attack = 0;
            int defense = 0;
            fine += Boxes::nature_percent(nature.name, attack, defense) && attack == nature.attack && defense == nature.defense;
        }
        int attack = 0;
        int defense = 0;
        check(
            fine == std::size(natures) && std::size(natures) == 25 &&
            Boxes::nature_percent("ADAMANT", attack, defense) && attack == 110 && defense == 100 &&
            !Boxes::nature_percent("Dreamy", attack, defense) && !Boxes::nature_percent("", attack, defense),
            "the natures of the game and what they do to the Attack and the Defense (five raise and five lower "
            "each stat, the rest of the 25 leave the pair as it is)"
        );
    }

    //  The stat formula, and what a vitamin adds to it.
    check(
        Boxes::tyrogue_stat(20, 19, 100) == 22 && Boxes::tyrogue_stat(20, 20, 100) == 23 &&
        Boxes::tyrogue_stat(20, 19, 110) == 24 && Boxes::tyrogue_stat(20, 19, 90) == 19 &&
        Boxes::vitamin_x(10, 5, 3) == 18 && Boxes::vitamin_x(10, 24, 8) == 35 && Boxes::vitamin_x(10, 0, 0) == 10 &&
        Boxes::vitamin_x(31, 0, 20) == 56,
        "a TYROGUE's Attack and Defense by the game's formula; 10 EVs a vitamin, no more than 100"
    );

    //  What the aim says about readings that cannot be (no vitamins), and about one that is.
    {
        const Boxes::VitaminAim nothing = Boxes::vitamin_aim(
            Boxes::VitaminGoal::ATTACK_EQUALS_DEFENSE, 19, 20, 3, 4, 100, 100, 0, 0
        );
        const Boxes::VitaminAim same = Boxes::vitamin_aim(
            Boxes::VitaminGoal::ATTACK_EQUALS_DEFENSE, 19, 20, 22, 22, 100, 100, 0, 0
        );
        const Boxes::VitaminAim iron = Boxes::vitamin_aim(
            Boxes::VitaminGoal::DEFENSE_ABOVE_ATTACK, 19, 20, 22, 20, 100, 100, 0, 0
        );
        check(
            nothing.chance < 0 && nothing.protein == 0 && nothing.iron == 0 &&
            same.chance > 0 && same.chance <= 1 && same.attack_fits > 0 && same.defense_fits > 0 &&
            iron.protein == 0 && iron.iron > 0 && iron.chance > 0.5,
            "no vitamins for stats that no IV and EVs make; IRONs only for a HITMONCHAN when the Attack leads (22 against 20: 5 to 7 of them)"
        );
    }

    //  2000 TYROGUE (a fixed sequence of numbers, IVs 0 to 31, EVs of the fights 0 to 24, a nature, then the EVs
    //  of the last fights to level 20): left alone, with the vitamins of two readings for a HITMONTOP, and with
    //  IRONs for a HITMONCHAN.
    {
        uint64_t state = 0x2545F4914F6CDD1DULL;
        auto next = [&](int below){
            state = state * 6364136223846793005ULL + 1442695040888963407ULL;
            return (int)((state >> 33) % (uint64_t)below);
        };
        static const char* const natures[] = {
            "Hardy", "Lonely", "Brave", "Adamant", "Naughty", "Bold", "Docile", "Relaxed", "Impish", "Lax", "Timid",
            "Hasty", "Serious", "Jolly", "Naive", "Modest", "Mild", "Quiet", "Bashful", "Rash", "Calm", "Gentle",
            "Sassy", "Careful", "Quirky"
        };
        const int trials = 2000;
        int unaided_top = 0;
        int aimed_top = 0;
        int unaided_chan = 0;
        int aimed_chan = 0;
        long vitamins = 0;
        bool within = true;
        for (int t = 0; t < trials; t++){
            int ap = 100;
            int dp = 100;
            Boxes::nature_percent(natures[next(25)], ap, dp);
            const int iva = next(32);
            const int ivd = next(32);
            const int eba = next(Boxes::BATTLE_EV_MAX + 1);
            const int ebd = next(Boxes::BATTLE_EV_MAX + 1);
            const int noise_a = next(3);
            const int noise_d = next(2);
            const int a20 = Boxes::tyrogue_stat(Boxes::vitamin_x(iva, eba + noise_a, 0), 20, ap);
            const int d20 = Boxes::tyrogue_stat(Boxes::vitamin_x(ivd, ebd + noise_d, 0), 20, dp);
            unaided_top += a20 == d20;
            unaided_chan += d20 > a20;
            for (int goal = 0; goal < 2; goal++){
                const Boxes::VitaminGoal kind = goal == 0
                    ? Boxes::VitaminGoal::ATTACK_EQUALS_DEFENSE : Boxes::VitaminGoal::DEFENSE_ABOVE_ATTACK;
                int protein = 0;
                int iron = 0;
                for (int round = 0; round < 2; round++){
                    const int a19 = Boxes::tyrogue_stat(Boxes::vitamin_x(iva, eba, protein), 19, ap);
                    const int d19 = Boxes::tyrogue_stat(Boxes::vitamin_x(ivd, ebd, iron), 19, dp);
                    const Boxes::VitaminAim aim = Boxes::vitamin_aim(kind, 19, 20, a19, d19, ap, dp, protein, iron);
                    within &= aim.chance >= 0 && aim.chance <= 1 && aim.protein >= 0 && aim.iron >= 0 &&
                        protein + aim.protein <= Boxes::VITAMIN_MAX_EACH && iron + aim.iron <= Boxes::VITAMIN_MAX_EACH &&
                        (goal == 1 ? aim.protein == 0 : true);
                    if (aim.chance < 0 || (aim.protein == 0 && aim.iron == 0)){
                        break;
                    }
                    protein += aim.protein;
                    iron += aim.iron;
                }
                const int a = Boxes::tyrogue_stat(Boxes::vitamin_x(iva, eba + noise_a, protein), 20, ap);
                const int d = Boxes::tyrogue_stat(Boxes::vitamin_x(ivd, ebd + noise_d, iron), 20, dp);
                if (goal == 0){
                    aimed_top += a == d;
                    vitamins += protein + iron;
                }else{
                    aimed_chan += d > a;
                }
            }
        }
        const double top_unaided = unaided_top / (double)trials;
        const double top_aimed = aimed_top / (double)trials;
        const double chan_unaided = unaided_chan / (double)trials;
        const double chan_aimed = aimed_chan / (double)trials;
        check(
            within && top_unaided > 0.05 && top_unaided < 0.20 && top_aimed > 0.55 && top_aimed < 0.85 &&
            chan_unaided > 0.30 && chan_unaided < 0.60 && chan_aimed > 0.70 && vitamins / (double)trials < 6.0,
            "the vitamins' aim on " + std::to_string(trials) + " TYROGUE: a HITMONTOP in " +
            std::to_string((int)(top_unaided * 100 + 0.5)) + "% left alone, " + std::to_string((int)(top_aimed * 100 + 0.5)) +
            "% aimed (" + std::to_string(vitamins / (double)trials) + " vitamins each); a HITMONCHAN in " +
            std::to_string((int)(chan_unaided * 100 + 0.5)) + "% and " + std::to_string((int)(chan_aimed * 100 + 0.5)) + "%"
        );
    }

    //  The coins window: numbers drawn as the game does (the glyphs of the small font on white, dark grey with a
    //  light grey shadow), at the scales of a capture, a pixel off, with noise.
    {
        uint64_t state = 12345;
        auto noise = [&](int amount){
            state = state * 6364136223846793005ULL + 1442695040888963407ULL;
            return (int)((state >> 33) % (uint64_t)(2 * amount + 1)) - amount;
        };
        auto draw = [&](int value, int width, int height, int off_x, int off_y){
            std::vector<uint8_t> image((size_t)width * (size_t)height, 255);
            const double sx = (double)width / Coins::CROP_W;
            const double sy = (double)height / Coins::CROP_H;
            std::string number = std::to_string(value);
            while ((int)number.size() < Coins::SLOTS){
                number = " " + number;
            }
            for (int y = 0; y < height; y++){
                for (int x = 0; x < width; x++){
                    const int gx = (int)std::floor((x + 0.5) / sx + Coins::CROP_X - off_x) - Coins::DIGIT_X;
                    const int gy = (int)std::floor((y + 0.5) / sy + Coins::CROP_Y - off_y) - Coins::DIGIT_Y;
                    int v = 255;
                    if (gx >= 0 && gx < Coins::SLOTS * Coins::GLYPH_W && gy >= 0 && gy < Coins::GLYPH_H){
                        const char digit = number[(size_t)(gx / Coins::GLYPH_W)];
                        if (digit != ' '){
                            const char ch = Coins::glyph(digit - '0')[gy][gx % Coins::GLYPH_W];
                            v = ch == '#' ? 56 : ch == 'o' ? 216 : 255;
                        }
                    }
                    image[(size_t)y * (size_t)width + (size_t)x] = (uint8_t)std::max(0, std::min(255, v + noise(10)));
                }
            }
            return image;
        };
        static const int values[] = {0, 5, 49, 50, 99, 499, 549, 1049, 1234, 4999, 9549, 9949, 9999, 1000, 8888};
        static const int sizes[][2] = {{192, 66}, {160, 74}, {96, 33}, {256, 88}};
        int tried = 0;
        int read = 0;
        for (int value : values){
            for (const auto& size : sizes){
                for (int off_x = -1; off_x <= 1; off_x++){
                    for (int off_y = -1; off_y <= 0; off_y++){
                        const std::vector<uint8_t> image = draw(value, size[0], size[1], off_x, off_y);
                        const Coins::Reading reading = Coins::read_digits(image, size[0], size[1]);
                        tried++;
                        read += reading.ok && reading.value == value;
                    }
                }
            }
        }
        const std::vector<uint8_t> white((size_t)192 * 66, 255);
        std::vector<uint8_t> junk((size_t)192 * 66);
        for (uint8_t& v : junk){
            v = (uint8_t)(noise(127) + 127);
        }
        check(
            read == tried && !Coins::read_digits(white, 192, 66).ok && !Coins::read_digits(junk, 192, 66).ok &&
            !Coins::read_digits(white, 10, 10).ok,
            "the number in the coins window: " + std::to_string(read) + " of " + std::to_string(tried) +
            " drawings read right, a white window and noise read as nothing"
        );
        check(
            Coins::games_to_residue(50, 12) == 1 && Coins::games_to_residue(49, 12) == 0 &&
            Coins::games_to_residue(51, 12) == 2 && Coins::games_to_residue(99, 12) == 0 &&
            Coins::games_to_residue(148, 12) == 12 && Coins::games_to_residue(20, 12) == 0 &&
            Coins::games_to_residue(9549, 12) == 0 && Coins::games_to_residue(9999, 12) == 0,
            "the games of one COIN that bring the COINS to 49 modulo 50"
        );
        //  From 49 modulo 50 the clerk's packs end at 9999: 500 only below 9500, 50 only below 9950.
        int coins = 49;
        int big = 0;
        int small = 0;
        while (coins < 9500 && big < 19){
            coins += 500;
            big++;
        }
        while (coins < 9950 && small < 9){
            coins += 50;
            small++;
        }
        check(coins == 9999 && big == 19 && small == 9, "49 + 19 packs of 500 + 9 of 50 is 9999 with the clerk's limits");
    }

    //  The story's data.
    {
        const StoryScript& script = AutoStoryData::story_CHARMANDER();
        const StorySegment* vitamins = nullptr;
        const StorySegment* porygon = nullptr;
        size_t vitamins_at = 0;
        size_t level_20_at = 0;
        size_t wynaut_at = 0;
        for (size_t s = 0; s < script.segment_count; s++){
            const std::string id = script.segments[s].id;
            if (id == "C20_79B_VITAMINS"){
                vitamins = &script.segments[s];
                vitamins_at = s;
            }
            if (id == "C20_90_PORYGON"){
                porygon = &script.segments[s];
            }
            if (id == "C20_75_LEVEL_20"){
                level_20_at = s;
            }
            if (id == "C20_79_DAYCARE_14_WYNAUT"){
                wynaut_at = s;
            }
        }
        check(
            vitamins != nullptr && story_once_segment(script, *vitamins) &&
            script.step(*vitamins, 0).type == StepType::SKIP_IF_OWNED &&
            std::string(script.step(*vitamins, 0).text) == "hitmontop" &&
            wynaut_at + 1 == vitamins_at && vitamins_at + 2 == level_20_at,
            "the vitamins' segment: a once segment for HITMONTOP, between WYNAUT's egg and the fourth TYROGUE"
        );
        //  (Build 64) The fourth TYROGUE: a Day Care segment that is left out when the table has
        //  the HITMONTOP, whose trip is left out when a TYROGUE lies in the boxes already. The
        //  other TYROGUE (and every other egg of the Day Care) have no such check.
        {
            const StorySegment* fourth = nullptr;
            size_t day_care_outs = 0;
            size_t outs_with_boxes_check = 0;
            bool fourth_is_tyrogue_check = false;
            for (size_t s = 0; s < script.segment_count; s++){
                const StorySegment& segment = script.segments[s];
                const std::string id = segment.id;
                if (id == "C20_79C_DAYCARE_15_TYROGUE"){
                    fourth = &segment;
                }
                if (id.find("_DAYCARE_") == std::string::npos){
                    continue;
                }
                for (size_t k = 0; k < segment.step_count; k++){
                    const StoryStep& step = script.step(segment, k);
                    if (step.type != StepType::DAYCARE_OUT){
                        continue;
                    }
                    day_care_outs++;
                    if (step.p1 > 0){
                        outs_with_boxes_check++;
                        fourth_is_tyrogue_check = id == "C20_79C_DAYCARE_15_TYROGUE" &&
                            std::string(step.text) == "tyrogue" && step.p1 == 1;
                    }
                }
            }
            check(
                fourth != nullptr && story_once_segment(script, *fourth) &&
                script.step(*fourth, 0).type == StepType::SKIP_IF_OWNED &&
                std::string(script.step(*fourth, 0).text) == "hitmontop" && script.step(*fourth, 0).p1 == 1 &&
                day_care_outs == 15 && outs_with_boxes_check == 1 && fourth_is_tyrogue_check,
                "the fourth TYROGUE (build 64): a once segment for HITMONTOP; its Day Care trip is the only one "
                "of the fifteen that is left out when the boxes hold a TYROGUE"
            );
        }
        if (vitamins != nullptr){
            int protein = 0;
            int iron = 0;
            int rematches = 0;
            int wins = 0;
            int item_skips = 0;
            bool buys_after = true;
            bool seen_buy = false;
            for (size_t k = 0; k < vitamins->step_count; k++){
                const StoryStep& step = script.step(*vitamins, k);
                if (step.type == StepType::BUY){
                    seen_buy = true;
                    protein += step.p0 == 1 && step.p1 == 9 ? 1 : 0;
                    iron += step.p0 == 2 && step.p1 == 9 ? 1 : 0;
                }
                if (step.type == StepType::REMATCH){
                    rematches++;
                    wins += step.p0;
                    buys_after &= !seen_buy;
                }
                if (step.type == StepType::SKIP_IF_ITEM){
                    item_skips++;
                    buys_after &= std::string(step.text) == "base attack|base atta";
                }
            }
            check(
                protein == 1 && iron == 1 && rematches == 4 && wins == 18 && item_skips == 1 && buys_after,
                "the vitamins' segment: 18 battles with Lady Gillian (176400) before nine PROTEINs and nine IRONs "
                "from the fifth floor's clerk, all skipped with a PROTEIN in the bag"
            );
        }
        if (porygon != nullptr){
            std::string modes;
            int packs_big = 0;
            int packs_small = 0;
            for (size_t k = 0; k < porygon->step_count; k++){
                const StoryStep& step = script.step(*porygon, k);
                if (step.type != StepType::COINS){
                    continue;
                }
                modes += (char)('0' + step.p0);
                if (step.p0 == 0){
                    packs_big += (int)step.p1;
                    packs_small += (int)step.p2;
                }
            }
            check(
                modes == "0" "343434343434" "0" "2" "1212121212121212121212121212" &&
                packs_big == 19 && packs_small == 10,
                "the Game Corner's steps: a pack of 50, six rounds of a look at the coins' clerk and the machine's "
                "games of one COIN, the packs (19 of 500, 9 of 50), the prizes' clerk first, and the 14 rounds of "
                "build 56 as they were"
            );
        }
    }

    logger.log(
        "AutoStory test: the vitamins' aim and the coins window: " + std::to_string(checks - wrong) + " of " +
        std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}


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
        trips == 108 && trips_not_named == 0 && others_named == 0,
        "every visit of a place of the collection is one trip (108), and nothing else is"
    );
    check(at_a_nurse == trips, "every trip starts at the nurse it ends at");
    check(fishing_trips == 15 && lap_trips == 4, "the trips to fish (15, three of them in a Safari Game) and to walk laps (4)");
    check(others == 27, "the checks of a segment (17) and of an island or a group (10) are not trips");
    check(places.size() == 57, "the places of the trips (49 with two visits or three, 8 Safari Games)");
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
    bool exp_leave_no = true;
    //  (Build 56) Where a save at a nurse leads to the Sevii Islands, the
    //  segment is not given up: the program stops there with a message.
    size_t sevii_saves = 0;
    size_t sevii_saves_wrong = 0;
    size_t porygon_nurse_saves = 0;
    size_t porygon_sevii = 0;
    for (size_t s = 0; s < script.segment_count; s++){
        const StorySegment& segment = script.segments[s];
        if (!story_once_segment(script, segment)){
            continue;
        }
        once++;
        once_ids += (once_ids.empty() ? "" : ", ") + std::string(segment.id);
        //  The legendary Pokemon (build 54): a save in front of it is in a
        //  cave, and the one after Lady Gillian's battles in a Pokemon Center
        //  of Five Island: not a place such a segment is given up from. The
        //  same for UNOWN (build 55): its saves are in the Pokemon Center of
        //  Seven Island (a Sevii island: FLY does not go from there to Kanto).
        if (std::string(segment.id) == "C20_86_HO_OH" || std::string(segment.id) == "C20_87_LUGIA" ||
            std::string(segment.id) == "C20_88_DEOXYS" || std::string(segment.id) == "C20_89_UNOWN" ||
            std::string(segment.id) == "C20_79B_VITAMINS"){
            //  (Build 56) Its saves at a Pokemon Center of Five or Seven
            //  Island are the ones the segment is not left from: SEVII.
            for (size_t i = 2; i + 1 < segment.step_count; i++){
                if (script.step(segment, i - 1).type != StepType::SAVE ||
                    script.step(segment, i - 2).type != StepType::HEAL){
                    continue;
                }
                const OnceLeave leave = story_once_leave(script, segment, i);
                sevii_saves += leave == OnceLeave::SEVII ? 1 : 0;
                //  (The one that is not on the island: the step after the
                //  last nurse's walk out is a FLY or a walk of Kanto.)
                sevii_saves_wrong += leave == OnceLeave::SEVII || leave == OnceLeave::WALK_OUT_FIRST ? 0 : 1;
            }
            continue;
        }
        //  The Game Corner's slot machines for PORYGON (build 56): the saves
        //  in the Game Corner are not outdoors (no way out is known there);
        //  the one at the nurse of Celadon City walks out into the city.
        if (std::string(segment.id) == "C20_90_PORYGON"){
            for (size_t i = 2; i + 1 < segment.step_count; i++){
                if (script.step(segment, i - 1).type != StepType::SAVE ||
                    script.step(segment, i - 2).type != StepType::HEAL){
                    continue;
                }
                const OnceLeave leave = story_once_leave(script, segment, i);
                porygon_nurse_saves += leave == OnceLeave::WALK_OUT_FIRST ? 1 : 0;
                porygon_sevii += leave == OnceLeave::SEVII ? 1 : 0;
            }
            continue;
        }
        //  A round of the Elite Four for experience (build 46) has the Pokemon
        //  out of its box from its PC to its save: given up from none of them.
        if (script.step(segment, 0).type == StepType::SKIP_IF_NO_LEVELS && script.step(segment, 0).p1 == 2){
            for (size_t i = 2; i + 1 < segment.step_count; i++){
                exp_leave_no &= story_once_leave(script, segment, i) == OnceLeave::NO;
            }
            continue;
        }
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
            case OnceLeave::SEVII:
                saves_unknown++;
                break;
            }
        }
    }
    check(
        once == 105 && exp_leave_no &&
        once_ids == "C20_19_DOJO, C20_20_SNORLAX, C20_21_KABUTO, C20_22_AERODACTYL, C20_24_PC_BOXES, C20_25_STONES, "
            "C20_26_TRADE_FARFETCHD, C20_27_TRADE_JYNX, C20_28_TRADE_MR_MIME, C20_29_TRADE_LICKITUNG, "
            "C20_30_MOON_STONE_MANSION, C20_32_MOON_STONE_MT_MOON_1F, "
            "C20_33_MOON_STONE_MT_MOON_B2F, C20_34_SUN_STONE_RUIN_VALLEY, C20_EXP_01_ELITE_FOUR, "
            "C20_EXP_02_ELITE_FOUR, C20_EXP_03_ELITE_FOUR, C20_EXP_04_ELITE_FOUR, C20_EXP_05_ELITE_FOUR, "
            "C20_EXP_06_ELITE_FOUR, C20_EXP_07_ELITE_FOUR, C20_EXP_08_ELITE_FOUR, C20_EXP_09_ELITE_FOUR, "
            "C20_EXP_10_ELITE_FOUR, "
            "C20_35_LEVEL_01, C20_36_LEVEL_02, "
            "C20_37_LEVEL_03, C20_38_LEVEL_04, C20_39_LEVEL_05, C20_40_LEVEL_06, C20_41_LEVEL_07, "
            "C20_42_LEVEL_08, C20_43_LEVEL_09, C20_44_LEVEL_10, C20_45_LEVEL_11, C20_46_LEVEL_12, "
            "C20_47_LEVEL_13, C20_48_LEVEL_14, C20_49_LEVEL_15, C20_50_LEVEL_16, C20_51_LEVEL_17, "
            "C20_52_LEVEL_18, C20_53_LEVEL_19, "
            "C20_EXP_11_ELITE_FOUR, C20_EXP_12_ELITE_FOUR, C20_EXP_13_ELITE_FOUR, "
            "C20_EXP_14_ELITE_FOUR, C20_EXP_15_ELITE_FOUR, C20_EXP_16_ELITE_FOUR, "
            "C20_EXP_17_ELITE_FOUR, C20_EXP_18_ELITE_FOUR, C20_EXP_19_ELITE_FOUR, "
            "C20_EXP_20_ELITE_FOUR, C20_EXP_21_ELITE_FOUR, C20_EXP_22_ELITE_FOUR, "
            "C20_54_FRIEND_01, C20_55_FRIEND_02, C20_56_FRIEND_03, "
            "C20_57_FRIEND_04, C20_58_FRIEND_05, C20_59_FRIEND_06, C20_60_FRIEND_07, C20_61_FRIEND_08, "
            "C20_62_TOGEPI_EGG, C20_63_FRIEND_09, C20_64_FRIEND_10, C20_65_FRIEND_11, "
            "C20_66_DAYCARE_01_PICHU, C20_67_DAYCARE_02_CLEFFA, C20_68_DAYCARE_03_IGGLYBUFF, "
            "C20_69_DAYCARE_04_TOGEPI, C20_70_DAYCARE_05_SMOOCHUM, C20_71_DAYCARE_06_ELEKID, "
            "C20_72_DAYCARE_07_TYROGUE, C20_73_DAYCARE_08_TYROGUE, C20_74_DAYCARE_09_TYROGUE, "
            "C20_75_DAYCARE_10_KABUTO, C20_76_DAYCARE_11_EEVEE, C20_77_DAYCARE_12_EEVEE, "
            "C20_78_DAYCARE_13_EEVEE, C20_79_DAYCARE_14_WYNAUT, C20_79B_VITAMINS, "
            "C20_79C_DAYCARE_15_TYROGUE, "
            "C20_75_LEVEL_20, C20_76_LEVEL_21, C20_77_LEVEL_22, C20_78_LEVEL_23, C20_79_LEVEL_24, "
            "C20_80_LEVEL_25, C20_81_LEVEL_26, C20_82_LEVEL_27, C20_83_LEVEL_28, C20_84_LEVEL_29, "
            "C20_EXP_23_ELITE_FOUR, C20_EXP_24_ELITE_FOUR, C20_EXP_25_ELITE_FOUR, "
            "C20_EXP_26_ELITE_FOUR, C20_EXP_27_ELITE_FOUR, C20_EXP_28_ELITE_FOUR, "
            "C20_85_STONES_2, C20_86_HO_OH, C20_87_LUGIA, C20_88_DEOXYS, C20_89_UNOWN, C20_90_PORYGON",
        "the four segments of Pokemon that are there once, the one that reads the PC's boxes, the one of the "
        "stones, the four trades, the four that fetch a stone (the Moon Stone of the Rocket Hideout is taken by "
        "the story itself), the ten rounds of the Elite Four for experience (five more in build 52), the nineteen "
        "that evolve by level, the eight that evolve by friendship, the Togepi egg (build 47), three more that "
        "evolve by friendship and the nine eggs of the Day Care (build 48, experimental) and eight more that evolve "
        "by level after them (build 51, experimental, and three for the DRATINI of build 52); build 54 "
        "(experimental) adds five eggs (the second KABUTO, three EEVEE, WYNAUT), two more that evolve by level "
        "(KABUTO's line, MAGCARGO), the stones again, and HO-OH, LUGIA and DEOXYS; build 55 (experimental) "
        "adds UNOWN (the Tanoby Key's seven boulders, the Monean Chamber); build 56 (experimental) adds "
        "eighteen more rounds of the Elite Four for experience (twelve after the segments by level, six after the "
        "Day Care's) for what comes out of them with a lot of experience to go, and PORYGON (the Game Corner's "
        "slot machines for 9999 coins) at the end; build 57 (experimental) adds the segment of the vitamins for "
        "a HITMONTOP between the Day Care's eggs and the levels; build 64 (experimental) adds a fourth "
        "TYROGUE after it, for the HITMONTOP that is still missing when none is left in the boxes"
    );
    check(
        sevii_saves == 16 && sevii_saves_wrong == 0 && porygon_nurse_saves == 1 && porygon_sevii == 8,
        "a save at a Pokemon Center of the Sevii Islands (Five Island: the three legendary Pokemon of Lady "
        "Gillian's battles, 3 each, the three trips of PORYGON, 8 (3 + 3 + 2: the last trip is one round, build "
        "62), and the vitamins' two visits, 5; Seven Island: UNOWN, 2) is none a segment "
        "is given up from (build 56: the program stops there); the one of Celadon City (PORYGON) walks out "
        "into the city"
    );
    //  (Build 59) LUGIA's script has no text box before its battle (the screen shakes, it cries, then the
    //  battle): a talk waits for a box that never comes (a run of build 54: five failures in a row at the
    //  last step before the battle). The player faces it, A once, and the battle step waits for the battle.
    {
        size_t lugia_talks = 0;
        size_t lugia_battles = 0;
        bool lugia_press_before = false;
        for (size_t s = 0; s < script.segment_count; s++){
            const StorySegment& segment = script.segments[s];
            if (std::string(segment.id) != "C20_87_LUGIA"){
                continue;
            }
            for (size_t i = 1; i < segment.step_count; i++){
                const StoryStep& step = script.step(segment, i);
                if (std::string(step.label) != "Lugia"){
                    continue;
                }
                lugia_talks += step.type == StepType::TALK ? 1 : 0;
                if (step.type == StepType::BATTLE && step.p1 == 2){
                    lugia_battles++;
                    const StoryStep& before = script.step(segment, i - 1);
                    lugia_press_before = before.type == StepType::PRESS && std::string(before.text) == "A";
                }
            }
        }
        //  (Build 60) DEOXYS (level 30, some 74 HP) is not weakened: a hit of the lead (level 100) takes
        //  all its HP (26 tries in a row, the foe fainted before the first ball). p2 & 1: weaken it first
        //  (-1: no flags).
        size_t deoxys_battles = 0;
        size_t deoxys_weakened = 0;
        for (size_t s = 0; s < script.segment_count; s++){
            const StorySegment& segment = script.segments[s];
            if (std::string(segment.id) != "C20_88_DEOXYS"){
                continue;
            }
            for (size_t i = 0; i < segment.step_count; i++){
                const StoryStep& step = script.step(segment, i);
                if (step.type == StepType::BATTLE && step.p1 == 2 && std::string(step.label) == "Deoxys"){
                    deoxys_battles++;
                    deoxys_weakened += step.p2 > 0 && (step.p2 & 1) != 0 ? 1 : 0;
                }
            }
        }
        check(
            deoxys_battles == 1 && deoxys_weakened == 0,
            "DEOXYS is caught with balls at full HP (build 60): its battle step has no weakening"
        );
        //  (Build 62) The legendary Pokemon's segments buy three MAX POTIONs at the Indigo Plateau
        //  before the ULTRA BALLs; the Game Corner is saved after the first pack and before
        //  the 28 packs; the PORYGON money is 22 battles (4 + 5, 4 + 5, 4).
        size_t potion_buys_fine = 0;
        size_t porygon_rematch_wins = 0;
        size_t porygon_rematches = 0;
        bool save_after_first_pack = false;
        bool save_before_big_packs = false;
        bool ferry_is_186 = false;
        size_t game_corner_saves = 0;
        bool game_corner_saves_no = true;
        for (size_t s = 0; s < script.segment_count; s++){
            const StorySegment& segment = script.segments[s];
            const std::string id = segment.id;
            if (id == "C20_86_HO_OH" || id == "C20_87_LUGIA" || id == "C20_88_DEOXYS"){
                int potions = 0;
                bool before_balls = true;
                bool seen_balls = false;
                for (size_t i = 0; i < segment.step_count; i++){
                    const StoryStep& step = script.step(segment, i);
                    if (step.type != StepType::BUY){
                        continue;
                    }
                    const std::string label = step.label;
                    if (label == "BuyMaxPotionsForTheLegend"){
                        potions += step.p0 == 3 && step.p1 == 3 ? 1 : 0;
                        before_balls &= !seen_balls;
                    }else if (label == "BuyUltraBalls"){
                        seen_balls = true;
                    }
                }
                potion_buys_fine += potions == 1 && before_balls && seen_balls ? 1 : 0;
            }
            if (id == "C20_90_PORYGON"){
                bool first_pack_seen = false;
                for (size_t i = 0; i < segment.step_count; i++){
                    const StoryStep& step = script.step(segment, i);
                    if (step.type == StepType::REMATCH){
                        porygon_rematches++;
                        porygon_rematch_wins += (size_t)step.p0;
                    }
                    if (step.type == StepType::COINS && step.p0 == 0 && !first_pack_seen){
                        first_pack_seen = true;
                        save_after_first_pack = i + 1 < segment.step_count && script.step(segment, i + 1).type == StepType::SAVE;
                    }
                    if (std::string(step.label) == "BackToTheCoinsClerk" && step.type == StepType::WALK && i > 0 &&
                        !save_before_big_packs
                    ){
                        save_before_big_packs = script.step(segment, i - 1).type == StepType::SAVE;
                    }
                }
                //  The saves in the Game Corner (after a pack, a game): no way out is known there, so the
                //  segment is not given up from them (build 56: the program stops after two failures).
                for (size_t i = 2; i + 1 < segment.step_count; i++){
                    if (script.step(segment, i - 1).type == StepType::SAVE && script.step(segment, i - 2).type == StepType::COINS){
                        game_corner_saves++;
                        game_corner_saves_no &= story_once_leave(script, segment, i) == OnceLeave::NO;
                    }
                }
                ferry_is_186 = segment.step_count >= 186 &&
                    std::string(script.step(segment, 185).label) == "FerryToVermilion" &&
                    script.step(segment, 184).type == StepType::SAVE;
            }
        }
        check(
            potion_buys_fine == 3,
            "HO-OH, LUGIA and DEOXYS buy three MAX POTIONs at the Indigo Plateau before their ULTRA BALLs (build 62)"
        );
        check(
            porygon_rematches == 5 && porygon_rematch_wins == 22 && ferry_is_186,
            "the PORYGON money is five rounds of Lady Gillian, 22 battles (build 62): the ferry home after the "
            "last one is step 186, after a save"
        );
        check(
            save_after_first_pack && save_before_big_packs && game_corner_saves >= 2 && game_corner_saves_no,
            "the Game Corner is saved after the first pack of 50 and with the COINS at 49 modulo 50, before the "
            "28 packs (build 62)"
        );
        //  The step numbers of a save of build 61, carried over to the data of build 62.
        check(
            story_legacy_step("C20_90_PORYGON", 206) == 186 && story_legacy_step("C20_90_PORYGON", 186) == 186 &&
            story_legacy_step("C20_90_PORYGON", 190) == 186 && story_legacy_step("C20_90_PORYGON", 185) == 185 &&
            story_legacy_step("C20_90_PORYGON", 220) == 201 && story_legacy_step("C20_90_PORYGON", 251) == 233 &&
            story_legacy_step("C20_88_DEOXYS", 99) == 106 && story_legacy_step("C20_88_DEOXYS", 72) == 72 &&
            story_legacy_step("C20_01B_MONEY", 90) == 90,
            "a save of build 61 (three words in the save file) is carried over to the step numbers of build 62"
        );
        check(
            lugia_talks == 0 && lugia_battles == 1 && lugia_press_before,
            "LUGIA has no text before its battle (build 59): no talk waits for a text box, the step before the "
            "battle is one press of A"
        );
    }
    check(
        saves_at_a_nurse == 4 && saves_outdoors == 2 && saves_unknown == 0 && walks_out_wrong == 0,
        "from every save in such a segment the player gets outdoors: at a nurse by the walk out of the Pokemon "
        "Center (4: the dojo, the Snorlax, and the stones twice), or the save is outdoors (2)"
    );
    check(
        story_once_leave(script, script.segments[0], 2) == OnceLeave::NO &&
        !story_once_segment(script, script.segments[0]) && !story_plain_trip(script, script.segments[0], 0) &&
        !story_rod_way(script, script.segments[0], 0),
        "a segment of the story is none of these"
    );

    //  A start from a save that is past segments an earlier run left out
    //  (build 37): back to the first of them the table has nothing from.
    {
        auto index_of = [&](const char* id){
            for (size_t s = 0; s < script.segment_count; s++){
                if (std::string(script.segments[s].id) == id){
                    return s;
                }
            }
            return script.segment_count;
        };
        const size_t story_end = script.segment_count;
        const size_t dojo = index_of("C20_19_DOJO");
        const size_t kabuto = index_of("C20_21_KABUTO");
        const size_t port = index_of("C20_23_THREE_ISLE_PORT");
        const size_t pc_boxes = index_of("C20_24_PC_BOXES");
        const size_t farfetchd = index_of("C20_26_TRADE_FARFETCHD");
        const size_t jynx = index_of("C20_27_TRADE_JYNX");
        const size_t mime = index_of("C20_28_TRADE_MR_MIME");
        const size_t lickitung = index_of("C20_29_TRADE_LICKITUNG");
        const size_t mansion = index_of("C20_30_MOON_STONE_MANSION");
        const size_t sun_stone = index_of("C20_34_SUN_STONE_RUIN_VALLEY");
        std::set<std::string> missing = {"farfetch-d", "jynx", "mr-mime", "lickitung"};
        //  (The Day Care wants up to three Tyrogue and, since build 54, four
        //  Eevee: a Pokemon that is not missing is there four times.)
        auto have = [&](const std::string& slug){
            return missing.count(slug) != 0 ? 0 : 4;
        };
        std::vector<size_t> left_open;
        const size_t from_end = story_once_open_before(script, story_end, have, &left_open);
        check(
            lickitung + 1 == mansion && sun_stone + 1 == index_of("C20_EXP_01_ELITE_FOUR") &&
            index_of("C20_EXP_10_ELITE_FOUR") + 1 == index_of("C20_35_LEVEL_01") &&
            index_of("C20_53_LEVEL_19") + 1 == index_of("C20_EXP_11_ELITE_FOUR") &&
            index_of("C20_EXP_22_ELITE_FOUR") + 1 == index_of("C20_54_FRIEND_01") &&
            index_of("C20_61_FRIEND_08") + 1 == index_of("C20_62_TOGEPI_EGG") &&
            index_of("C20_62_TOGEPI_EGG") + 1 == index_of("C20_63_FRIEND_09") &&
            index_of("C20_65_FRIEND_11") + 1 == index_of("C20_66_DAYCARE_01_PICHU") &&
            index_of("C20_74_DAYCARE_09_TYROGUE") + 1 == index_of("C20_75_DAYCARE_10_KABUTO") &&
            index_of("C20_78_DAYCARE_13_EEVEE") + 1 == index_of("C20_79_DAYCARE_14_WYNAUT") &&
            index_of("C20_79_DAYCARE_14_WYNAUT") + 1 == index_of("C20_79B_VITAMINS") &&
            index_of("C20_79B_VITAMINS") + 1 == index_of("C20_79C_DAYCARE_15_TYROGUE") &&
            index_of("C20_79C_DAYCARE_15_TYROGUE") + 1 == index_of("C20_75_LEVEL_20") &&
            index_of("C20_84_LEVEL_29") + 1 == index_of("C20_EXP_23_ELITE_FOUR") &&
            index_of("C20_EXP_28_ELITE_FOUR") + 1 == index_of("C20_85_STONES_2") &&
            index_of("C20_88_DEOXYS") + 1 == index_of("C20_89_UNOWN") &&
            index_of("C20_89_UNOWN") + 1 == index_of("C20_90_PORYGON") &&
            index_of("C20_90_PORYGON") + 1 == story_end && from_end == farfetchd && left_open.size() == 4 &&
            left_open[0] == farfetchd && left_open[1] == jynx && left_open[2] == mime && left_open[3] == lickitung,
            "a save at the story's end with the four trades left out: the start goes back to the first of them"
        );
        //  The egg (build 47): room in the party, the walk until it hatches,
        //  what hatched into the box; no save in between (a reset undoes
        //  all of it, and the party and the boxes agree with the save).
        {
            const size_t egg_index = index_of("C20_62_TOGEPI_EGG");
            size_t room_at = 0;
            size_t hatch_at = 0;
            size_t back_at = 0;
            size_t saves_between = 0;
            bool egg_ok = egg_index < story_end && story_once_segment(script, script.segments[egg_index]);
            if (egg_ok){
                const StorySegment& egg = script.segments[egg_index];
                for (size_t i = 0; i < egg.step_count; i++){
                    const StoryStep& step = script.step(egg, i);
                    room_at = step.type == StepType::EGG_ROOM ? i : room_at;
                    hatch_at = step.type == StepType::HATCH ? i : hatch_at;
                    back_at = step.type == StepType::EGG_BACK ? i : back_at;
                    saves_between += step.type == StepType::SAVE && i + 1 < egg.step_count ? 1 : 0;
                }
                const StoryStep& hatch = script.step(egg, hatch_at);
                egg_ok = room_at > 1 && hatch_at > room_at && back_at > hatch_at && saves_between == 0 &&
                    hatch.p0 > 2805 && hatch.p1 >= 3 && std::string(hatch.text) == "togepi" && hatch.dir != DPAD_NONE;
            }
            check(egg_ok, "the Togepi egg: room in the party, the walk to the hatching, the hatched one into the box, no save between");
        }
        //  The Day Care (build 48, experimental): the parents out of the
        //  boxes, to the woman, up to 24 rounds of walking and asking the man
        //  for the egg, the walk until it hatches, the baby into a box, the
        //  parents back, and no save in between (a reset undoes all of it).
        {
            struct DayCare{
                const char* id;
                const char* slug;
                int copies;
            };
            static const DayCare day_cares[] = {
                {"C20_66_DAYCARE_01_PICHU", "pichu", 1},
                {"C20_67_DAYCARE_02_CLEFFA", "cleffa", 1},
                {"C20_68_DAYCARE_03_IGGLYBUFF", "igglybuff", 1},
                {"C20_69_DAYCARE_04_TOGEPI", "togepi", 2},
                {"C20_70_DAYCARE_05_SMOOCHUM", "smoochum", 1},
                {"C20_71_DAYCARE_06_ELEKID", "elekid", 1},
                {"C20_72_DAYCARE_07_TYROGUE", "tyrogue", 1},
                {"C20_73_DAYCARE_08_TYROGUE", "tyrogue", 2},
                {"C20_74_DAYCARE_09_TYROGUE", "tyrogue", 3},
                {"C20_75_DAYCARE_10_KABUTO", "kabuto", 2},
                {"C20_76_DAYCARE_11_EEVEE", "eevee", 2},
                {"C20_77_DAYCARE_12_EEVEE", "eevee", 3},
                {"C20_78_DAYCARE_13_EEVEE", "eevee", 4},
                {"C20_79_DAYCARE_14_WYNAUT", "wynaut", 1},
            };
            size_t day_cares_ok = 0;
            for (const DayCare& day_care : day_cares){
                const size_t at = index_of(day_care.id);
                if (at >= story_end || !story_once_segment(script, script.segments[at])){
                    continue;
                }
                const StorySegment& segment = script.segments[at];
                const StoryStep& first = script.step(segment, 0);
                size_t out_at = 0, give_at = 0, first_walk = 0, hatch_at = 0, store_at = 0, take_at = 0, end_at = 0;
                size_t outs = 0, gives = 0, walks = 0, men = 0, hatches = 0, stores = 0, takes = 0, ends = 0;
                size_t saves_between = 0;
                bool walks_fit = true;
                bool texts_fit = true;
                for (size_t i = 0; i < segment.step_count; i++){
                    const StoryStep& step = script.step(segment, i);
                    switch (step.type){
                    case StepType::DAYCARE_OUT:
                        outs++, out_at = i;
                        texts_fit &= std::string(step.text) == day_care.slug;
                        break;
                    case StepType::DAYCARE_GIVE:
                        gives++, give_at = i;
                        break;
                    case StepType::DAYCARE_WALK:
                        first_walk = walks == 0 ? i : first_walk;
                        walks++;
                        walks_fit &= step.p0 == 270 && step.p1 == 9;
                        break;
                    case StepType::DAYCARE_MAN:
                        men++;
                        break;
                    case StepType::HATCH:
                        hatches++, hatch_at = i;
                        texts_fit &= std::string(step.text) == day_care.slug;
                        break;
                    case StepType::DAYCARE_STORE:
                        stores++, store_at = i;
                        break;
                    case StepType::DAYCARE_TAKE:
                        takes++, take_at = i;
                        break;
                    case StepType::DAYCARE_END:
                        ends++, end_at = i;
                        break;
                    case StepType::SAVE:
                        saves_between += i + 1 < segment.step_count ? 1 : 0;
                        break;
                    default:
                        break;
                    }
                }
                const bool fine =
                    first.type == StepType::SKIP_IF_OWNED && std::string(first.text) == day_care.slug &&
                    first.p1 == day_care.copies &&
                    outs == 1 && gives == 1 && walks == 24 && men == 25 && hatches == 1 && stores == 1 &&
                    takes == 1 && ends == 1 && walks_fit && texts_fit && saves_between == 0 &&
                    out_at > 1 && give_at > out_at && first_walk > give_at && hatch_at > first_walk &&
                    store_at > hatch_at && take_at > store_at && end_at > take_at &&
                    script.step(segment, segment.step_count - 1).type == StepType::SAVE;
                day_cares_ok += fine ? 1 : 0;
            }
            check(
                day_cares_ok == std::size(day_cares),
                "the fourteen eggs of the Day Care (build 54: five more): the parents out, to the woman, 24 rounds at most, the hatching, the baby into a box, the parents back, no save between"
            );
        }
        left_open.clear();
        const size_t from_mime = story_once_open_before(script, mime, have, &left_open);
        check(
            from_mime == farfetchd && left_open.size() == 2 &&
            story_once_open_before(script, jynx, have) == farfetchd &&
            story_once_open_before(script, farfetchd, have) == farfetchd,
            "a save between two of them: only the ones before it are gone back to"
        );
        missing = {"jynx"};
        check(
            story_once_open_before(script, story_end, have) == jynx &&
            story_once_open_before(script, lickitung, have) == jynx &&
            story_once_open_before(script, mansion, have) == jynx &&
            story_once_open_before(script, jynx, have) == jynx,
            "one trade left open among others that were made"
        );
        missing.clear();
        check(
            story_once_open_before(script, story_end, have) == story_end &&
            story_once_open_before(script, mansion, have) == mansion &&
            story_once_open_before(script, pc_boxes, have) == pc_boxes &&
            story_once_open_before(script, 0, have) == 0 &&
            story_once_open_before(script, story_end + 5, have) == story_end,
            "nothing left open: the start stays where it is"
        );
        missing = {"hitmonlee", "snorlax"};
        check(
            dojo < kabuto && kabuto < port && port < pc_boxes &&
            story_once_open_before(script, story_end, have) == story_end &&
            story_once_open_before(script, port, have) == dojo &&
            story_once_open_before(script, kabuto, have) == dojo,
            "only the segments right before the start count, not ones behind a segment of another kind"
        );
    }

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
            table.size() == 175 && plan.size() == 42 && none_there && all_named &&
            by_level == 24 && by_stone == 14 && by_friendship == 3 && by_other == 1,
            "FireRed with Charmander: the 42 species of the table that only come by evolving, and how (build 52: "
            "DRAGONAIR is one of them, 1 bite in 100 of the SUPER ROD)"
        );
        const std::vector<std::string> lines = Boxes::evolve_summary(plan);
        check(
            lines.size() == 1 && lines[0].find("is not in a box (42;") != std::string::npos &&
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
        //  The segment that fetches one stone counts only the ones that take it (build 41).
        check(
            Boxes::stone_targets_only(targets, "Moon Stone").size() == 1 &&
            Boxes::stone_targets_only(targets, "Moon Stone")[0].slug == "nidoqueen" &&
            Boxes::stone_targets_only(targets, "Sun Stone").empty() &&
            Boxes::stone_targets_only(targets, "").size() == targets.size() &&
            Boxes::stone_targets_only(no_moon, "Moon Stone").empty(),
            "only the Pokemon that take the one stone of a segment that fetches it; none of a stone that is out"
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
    //  The story's data: the segment of the stones.
    {
        size_t stones_segment = 0;
        for (size_t s = 0; s < script.segment_count; s++){
            if (std::string(script.segments[s].id) == "C20_25_STONES"){
                stones_segment = s;
            }
        }
        const StorySegment& segment = script.segments[stones_segment];
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
            "the segment of the stones: the prize money and the purchase (skipped with the stones in the bag), "
            "then the PC of a Pokemon Center; given up from its own save past its step"
        );
    }

    //  The segments that fetch a stone that is not sold (build 41): the
    //  check carries the stone and the place ("Moon Stone in Mt. Moon 1F"),
    //  one pick-up that may find nothing, the PC of a Pokemon Center, and
    //  the segment's save.
    {
        check(
            story_stone_fetch_stone("Moon Stone in Mt. Moon 1F") == "Moon Stone" &&
            story_stone_fetch_stone("Sun Stone in Ruin Valley") == "Sun Stone" &&
            story_stone_fetch_stone("").empty() && story_stone_fetch_stone("Moon Stone").empty() &&
            story_stone_fetch_stone(" in the cave").empty() &&
            story_stone_fetch_place("Moon Stone in Mt. Moon 1F") == "the Moon Stone in Mt. Moon 1F" &&
            story_item_may_be_gone("Pick up item (FLAG_HIDE_MT_MOON_1F_POTION)") &&
            story_item_may_be_gone("Pick up stone: Moon Stone (Mt. Moon 1F)") &&
            !story_item_may_be_gone("RareCandyMtMoon") && !story_item_may_be_gone(""),
            "the text of the check of a segment that fetches a stone: the stone, its place; the pick-ups that may find nothing"
        );
        std::string fetch_ids;
        std::set<std::string> fetch_places;
        int moon_ones = 0;
        int sun_ones = 0;
        bool fetch_shape = true;
        for (size_t s = 0; s < script.segment_count; s++){
            const StorySegment& segment = script.segments[s];
            const StoryStep& first = script.step(segment, 0);
            if (segment.step_count < 3 || first.type != StepType::SKIP_IF_NO_STONES || first.text == nullptr ||
                first.text[0] == '\0'
            ){
                continue;
            }
            fetch_ids += (fetch_ids.empty() ? "" : ", ") + std::string(segment.id);
            const std::string stone = story_stone_fetch_stone(first.text);
            const Boxes::StoneKind* kind = Boxes::stone_kind(stone);
            fetch_places.insert(story_stone_fetch_place(first.text));
            moon_ones += stone == "Moon Stone" ? 1 : 0;
            sun_ones += stone == "Sun Stone" ? 1 : 0;
            int picks = 0;
            int pcs = 0;
            size_t pick_at = 0;
            size_t pc_at = 0;
            for (size_t k = 1; k < segment.step_count; k++){
                const StoryStep& step = script.step(segment, k);
                if (step.type == StepType::TALK && step.label != nullptr &&
                    std::string(step.label).rfind("Pick up stone", 0) == 0
                ){
                    picks++;
                    pick_at = k;
                }
                if (step.type == StepType::STONES){
                    pcs++;
                    pc_at = k;
                }
            }
            fetch_shape = fetch_shape && story_once_segment(script, segment) && kind != nullptr && kind->mart < 0 &&
                picks == 1 && pcs == 1 && pick_at < pc_at &&
                story_item_may_be_gone(script.step(segment, pick_at).label) &&
                story_once_leave(script, segment, pc_at) == OnceLeave::PAST_STEP;
        }
        check(
            fetch_ids == "C20_30_MOON_STONE_MANSION, C20_32_MOON_STONE_MT_MOON_1F, "
                "C20_33_MOON_STONE_MT_MOON_B2F, C20_34_SUN_STONE_RUIN_VALLEY" &&
            moon_ones == 3 && sun_ones == 1 && fetch_places.size() == 4 && fetch_shape,
            "the four segments that fetch a stone (3 Moon Stones, the Sun Stone): each its own place, one pick-up, "
            "one PC, given up from its own save past it"
        );
        const StoryStep& plain = script.step(script.segments[0], 0);
        size_t all_stones = script.segment_count;
        for (size_t s = 0; s < script.segment_count; s++){
            if (std::string(script.segments[s].id) == "C20_25_STONES"){
                all_stones = s;
            }
        }
        check(
            plain.type != StepType::SKIP_IF_NO_STONES && all_stones < script.segment_count &&
            script.step(script.segments[all_stones], 0).type == StepType::SKIP_IF_NO_STONES &&
            (script.step(script.segments[all_stones], 0).text == nullptr ||
                script.step(script.segments[all_stones], 0).text[0] == '\0'),
            "the segment of all the stones has no text in its check: it is not one that fetches"
        );
    }

    //  The game's trades (build 36): the one to give out of a box at a PC,
    //  the trader, what the trade gave into the box at the same PC, and no
    //  save in between.
    std::string trade_gets;
    {
        //  ("entries", "count": the POKeDEX table, from above.)
        int trades = 0;
        bool shape = true;
        bool table_says = true;
        bool mail_fine = true;
        bool before_fine = true;
        std::string mails;
        for (size_t s = 0; s < script.segment_count; s++){
            const StorySegment& segment = script.segments[s];
            if (std::string(segment.id).find("_TRADE_") == std::string::npos){
                continue;
            }
            trades++;
            size_t out_at = 0;
            size_t trade_at = 0;
            size_t in_at = 0;
            int outs = 0;
            int deals = 0;
            int ins = 0;
            for (size_t k = 0; k < segment.step_count; k++){
                const StoryStep& step = script.step(segment, k);
                if (step.type == StepType::BOX_OUT){
                    outs++;
                    out_at = k;
                }
                if (step.type == StepType::TRADE){
                    deals++;
                    trade_at = k;
                }
                if (step.type == StepType::BOX_IN){
                    ins++;
                    in_at = k;
                }
            }
            //  SKIP_IF_OWNED (what the trade gives), FLY, ..., BOX_OUT (not
            //  there: on past BOX_IN), ..., TRADE, ..., BOX_IN, the walk
            //  out, the save.
            bool fine = story_once_segment(script, segment) &&
                script.step(segment, 0).type == StepType::SKIP_IF_OWNED &&
                outs == 1 && deals == 1 && ins == 1 && out_at > 1 && out_at < trade_at && trade_at < in_at &&
                in_at + 3 == segment.step_count &&
                out_at + (size_t)std::max(0, (int)script.step(segment, out_at).p0) == in_at &&
                std::string(script.step(segment, 0).text) == script.step(segment, trade_at).text;
            if (fine){
                const StoryStep& to_out = script.step(segment, out_at - 1);
                const StoryStep& to_trade = script.step(segment, trade_at - 1);
                const StoryStep& to_in = script.step(segment, in_at - 1);
                fine = to_out.type == StepType::WALK && to_trade.type == StepType::WALK && to_in.type == StepType::WALK;
                if (fine){
                    const StoryLeg& a = script.leg(to_out.leg);
                    const StoryLeg& b = script.leg(to_trade.leg);
                    const StoryLeg& c = script.leg(to_in.leg);
                    fine = a.end == LegEnd::INTERACT && a.face == DPAD_UP && c.end == LegEnd::INTERACT &&
                        c.face == DPAD_UP && a.start_map == c.start_map && a.start_map < script.map_name_count &&
                        std::string(script.map_names[a.start_map]).find("Pokemon Center 1F") != std::string::npos &&
                        b.end == LegEnd::INTERACT;
                }
                for (size_t k = out_at + 1; k < in_at; k++){
                    const StepType type = script.step(segment, k).type;
                    fine &= type != StepType::SAVE && type != StepType::HEAL && type != StepType::FLY;
                }
            }
            shape &= fine;
            //  The table: what the trade gives is a species that comes
            //  from a trade for what is taken out of the box.
            const std::string get = script.step(segment, trade_at).text;
            const std::string give = script.step(segment, out_at).text;
            bool found = false;
            for (size_t k = 0; k < count; k++){
                if (get != entries[k].slug){
                    continue;
                }
                std::string how;
                for (const char* p = entries[k].how[0]; *p != 0; p++){
                    how += *p >= 'A' && *p <= 'Z' ? (char)(*p - 'A' + 'a') : *p;
                }
                found = entries[k].copies[0] == 1 && how.find("in-game trade (give " + give + ")") != std::string::npos;
            }
            table_says &= found;
            trade_gets += (trade_gets.empty() ? "" : ", ") + get;
            //  What the game's table of trades gives with a MAIL on it (the
            //  generator reads it from there): the JYNX, a FAB MAIL. The
            //  PC does not take it into a box, nor close the party with it
            //  in the hand, until the MAIL is off it.
            const bool holds_mail = script.step(segment, trade_at).p0 != 0;
            mail_fine &= holds_mail == (get == "jynx");
            //  What a BOX_OUT looks for in its cell, for a trade made before.
            before_fine &= story_trade_after(script, segment, out_at) == get &&
                story_trade_after(script, segment, in_at).empty() &&
                story_trade_after(script, segment, trade_at).empty();
            if (holds_mail){
                mails += (mails.empty() ? "" : ", ") + get;
            }
        }
        check(
            trades == 4 && shape && trade_gets == "farfetch-d, jynx, mr-mime, lickitung",
            "the four trades in the story's data: out of a box at a PC, the trader, into the box at the same PC, "
            "no save, nurse or FLY in between; without the one to give, on past all of it"
        );
        check(table_says, "each trade gives a species the FireRed table has from a trade for what the step takes out");
        check(
            mail_fine && mails == "jynx",
            "the trade that gives a Pokemon with a MAIL on it (the JYNX) says so, the others do not"
        );
        check(
            before_fine,
            "the step that takes the Pokemon to give out of a box finds the species its segment's trade gives "
            "(what its cell holds when the trade was made before), and none after the trade or at the box"
        );
    }

    //  Evolving by level (build 43): which of the boxes, in which order, and
    //  the file of a training under way.
    {
        const std::vector<Boxes::EvolveLine> plan = Boxes::evolve_plan(table, census);
        const std::vector<Boxes::LevelTarget> targets = Boxes::level_targets(plan);
        const std::vector<Boxes::LevelTarget> without = Boxes::level_targets(plan, {"pidgeot"});
        check(
            targets.size() == 2 && targets[0].slug == "pidgeot" && targets[0].from_slug == "pidgeotto" &&
            targets[0].level == 40 && targets[0].evolves_at == 36 && targets[0].goal == 41 &&
            targets[1].slug == "butterfree" && targets[1].from_slug == "metapod" && targets[1].box == 0 &&
            targets[1].cell == 4 && targets[1].level == 8 && targets[1].goal == 10 &&
            without.size() == 1 && without[0].slug == "butterfree",
            "the Pokemon of the boxes that evolve by a level: the fewest levels to go first, one at its next level"
        );
        check(
            Boxes::level_goal(8, 10) == 10 && Boxes::level_goal(10, 10) == 11 && Boxes::level_goal(40, 36) == 41 &&
            Boxes::level_goal(-1, 16) == 16 && Boxes::level_goal(100, 36) == 0 && Boxes::level_goal(99, 36) == 100 &&
            Boxes::level_goal(5, 1) == 0 && Boxes::level_goal(5, 101) == 0 &&
            Boxes::levels_to_go(targets[0]) == 1 && Boxes::levels_to_go(targets[1]) == 2,
            "the level a Pokemon is trained to, and the levels still to go"
        );
        check(
            Boxes::level_targets_text(targets) ==
                "Pidgeot (PIDGEOTTO Lv 40, box 1 cell 6, to level 41), Butterfree (METAPOD Lv 8, box 1 cell 5, to level 10)",
            "the targets as one line for the log"
        );
        Boxes::RaiseFile file;
        file.out = true;
        file.box = 2;
        file.cell = 17;
        file.extra = "articuno";
        file.from = "mr-mime";
        file.becomes = "pidgeot";
        file.goal = 36;
        Boxes::RaiseFile back;
        const std::string text = Boxes::raise_to_text(file);
        const bool same = Boxes::raise_from_text(text, back) && back.out && back.box == 2 && back.cell == 17 &&
            back.extra == "articuno" && back.from == "mr-mime" && back.becomes == "pidgeot" && back.goal == 36;
        Boxes::RaiseFile none;
        none.out = true;
        const bool off = Boxes::raise_from_text(Boxes::raise_to_text(Boxes::RaiseFile()), none) && !none.out;
        Boxes::RaiseFile bad;
        check(
            text == "raise 1 2 17 articuno mr-mime pidgeot 36\n" && same && off &&
            !Boxes::raise_from_text("", bad) && !Boxes::raise_from_text("raise", bad) &&
            !Boxes::raise_from_text("raise 1 2 17 articuno mr-mime pidgeot", bad) &&
            !Boxes::raise_from_text("raise 1 2 17 articuno mr-mime pidgeot 1", bad) &&
            !Boxes::raise_from_text("raise 1 x 17 articuno mr-mime pidgeot 36", bad) &&
            !Boxes::raise_from_text("raise 0 1", bad) && !Boxes::raise_from_text("other 1 2 17 a b c 36", bad) &&
            Boxes::raise_from_text("  raise 1 0 0 zapdos eevee jolteon 100\r\n", bad) && bad.out && bad.goal == 100,
            "the file of a training under way, and what is not taken from it"
        );
    }
    {
        int segments = 0;
        bool shape = true;
        bool legs_there = true;
        for (size_t s = 0; s < script.segment_count; s++){
            const StorySegment& segment = script.segments[s];
            if (segment.step_count < 3 || script.step(segment, 0).type != StepType::SKIP_IF_NO_LEVELS ||
                script.step(segment, 0).p1 != 0){
                continue;
            }
            segments++;
            size_t out_at = 0;
            size_t raise_at = 0;
            size_t in_at = 0;
            int outs = 0;
            int raises = 0;
            int ins = 0;
            int saves = 0;
            for (size_t k = 0; k < segment.step_count; k++){
                const StoryStep& step = script.step(segment, k);
                if (step.type == StepType::LEVEL_OUT){
                    out_at = k;
                    outs++;
                }else if (step.type == StepType::RAISE){
                    raise_at = k;
                    raises++;
                    legs_there &= step.legs_a != 0 && step.legs_b != 0 && step.legs_c != 0 && step.p3 > 0;
                }else if (step.type == StepType::LEVEL_IN){
                    in_at = k;
                    ins++;
                }else if (step.type == StepType::SAVE){
                    saves++;
                }
            }
            shape &= story_once_segment(script, segment) && outs == 1 && raises == 1 && ins == 1 && saves == 1 &&
                out_at < raise_at && raise_at < in_at &&
                (size_t)script.step(segment, out_at).p0 == in_at - out_at &&
                script.step(segment, 1).type == StepType::FLY &&
                script.step(segment, segment.step_count - 1).type == StepType::SAVE &&
                script.step(segment, in_at + 1).type == StepType::WALK;
            //  Given up from a save in it: not possible (the Pokemon is out of its box).
            for (size_t i = 2; i + 1 < segment.step_count; i++){
                shape &= story_once_leave(script, segment, i) == OnceLeave::NO;
            }
        }
        check(
            segments == 29 && shape && legs_there && story_level_place("pidgeot") == "raising PIDGEOT" &&
            story_levels_place() == "the evolving by level",
            "the 29 segments that evolve by level (19 before the Day Care, ten after it): FLY, the PC, the training, "
            "the PC, a save at the end only"
        );
    }

    //  DRATINI (build 52, experimental): DRAGONAIR is 1 bite in 100 of the SUPER ROD
    //  in the Safari Zone, DRATINI 15, so the table wants DRATINI twice and has
    //  DRAGONAIR and DRAGONITE come by evolving, as LARVITAR's line does. The
    //  fishing is a segment of its own among the fishing ones: three Safari
    //  Games, each the rod cast at the water of the center area, and the way
    //  out by the gate for when the game is not over by itself.
    {
        auto entry_of = [&](const char* slug){
            for (size_t k = 0; k < count; k++){
                if (std::string(entries[k].slug) == slug){
                    return entries[k];
                }
            }
            return entries[0];
        };
        const AutoStoryDex::CollectionEntry dratini = entry_of("dratini");
        const AutoStoryDex::CollectionEntry dragonair = entry_of("dragonair");
        const AutoStoryDex::CollectionEntry dragonite = entry_of("dragonite");
        Boxes::BoxCensus more = census;
        Boxes::BoxPokemon first;
        first.box = 6;
        first.cell = 1;
        first.slug = "dratini";
        first.level = 18;
        more.pokemon.push_back(first);
        const std::vector<Boxes::LevelTarget> to_dragonair = Boxes::level_targets(Boxes::evolve_plan(table, more));
        Boxes::BoxPokemon second;
        second.box = 6;
        second.cell = 2;
        second.slug = "dragonair";
        second.level = 31;
        more.pokemon.push_back(second);
        const std::vector<Boxes::LevelTarget> to_dragonite = Boxes::exp_targets(Boxes::evolve_plan(table, more));
        bool dragonair_line = false;
        for (const Boxes::LevelTarget& target : to_dragonair){
            dragonair_line |= target.slug == "dragonair" && target.from_slug == "dratini" && target.level == 18 &&
                target.evolves_at == 30 && target.goal == 30;
        }
        bool dragonite_line = false;
        for (const Boxes::LevelTarget& target : to_dragonite){
            dragonite_line |= target.slug == "dragonite" && target.from_slug == "dragonair" && target.level == 31 &&
                target.evolves_at == 55 && target.goal == 55;
        }
        check(
            dratini.copies[0] == 2 && dratini.copies[1] == 2 && dragonair.copies[0] == 0 && dragonair.copies[1] == 0 &&
            dragonite.copies[0] == 0 && std::string(dragonair.how[0]) == "evolve from Dratini: level 30" &&
            std::string(dratini.evolve[0]) == "Dragonair (level 30) -> Dragonite (level 55)" &&
            dragonair_line && dragonite_line,
            "DRATINI twice in the table, DRAGONAIR (level 30) and DRAGONITE (level 55) from the extra one: a "
            "DRATINI of the boxes is trained to level 30, the DRAGONAIR it becomes to 55"
        );

        auto index_of = [&](const char* id){
            for (size_t s = 0; s < script.segment_count; s++){
                if (std::string(script.segments[s].id) == id){
                    return s;
                }
            }
            return script.segment_count;
        };
        const size_t fuchsia = index_of("C20_16_FISH_FUCHSIA");
        const size_t safari = index_of("C20_16B_SAFARI_FISH");
        const size_t five = index_of("C20_17_FIVE_ISLAND_WATER");
        int games = 0;
        bool shape = safari < script.segment_count;
        if (shape){
            const StorySegment& segment = script.segments[safari];
            shape &= !story_once_segment(script, segment) && script.step(segment, 0).type == StepType::SKIP_IF_COLLECTED;
            for (size_t k = 0; k + 1 < segment.step_count; k++){
                const StoryStep& step = script.step(segment, k);
                if (step.type != StepType::DEX){
                    continue;
                }
                games++;
                //  Safari (1), the table (2), fishing (4): the SUPER ROD (2), an hour, no limit of battles
                //  in a row without a catch, DRATINI 15 in 100 of the casts in both games, and the way out:
                //  legs that end on a warp. The game's own end ("The Safari Game is over") follows.
                const std::vector<int32_t> out = script.leg_list(step.legs_b);
                shape &= step.p2 == 7 && step.p4 == 2 && step.p1 == 60 && step.p0 >= 1000 &&
                    std::string(step.text).find("dratini:15:15") != std::string::npos &&
                    std::string(step.text).find("dragonair:1:1") != std::string::npos &&
                    !out.empty() && (script.leg(out.back()).end == LegEnd::WARP_PUSH ||
                        script.leg(out.back()).end == LegEnd::WARP_STEP) &&
                    script.step(segment, k + 1).type == StepType::CUTSCENE;
            }
        }
        check(
            fuchsia + 1 == safari && safari + 1 == five && games == 3 && shape,
            "the Super Rod in the Safari Zone (build 52): three games between the fishing at Fuchsia City and "
            "Five Island, each with the rod cast, DRATINI 15 and DRAGONAIR 1 in 100, and the way out by the gate"
        );
    }

    //  TYROGUE (build 51, experimental): it evolves at level 20 into HITMONLEE,
    //  HITMONCHAN or HITMONTOP, which of them the game decides by its Attack
    //  and Defense. The table wants HITMONCHAN (HITMONLEE comes from the Dojo)
    //  and has HITMONTOP as one that only comes by evolving: a TYROGUE of the
    //  boxes is trained to level 20 for each, whatever comes of it.
    {
        auto table_with = [&](const std::set<std::string>& had){
            std::vector<Boxes::TableRow> rows = table;
            for (Boxes::TableRow& row : rows){
                row.have = had.count(row.slug) != 0 ? 1 : 0;
            }
            return rows;
        };
        auto census_of = [&](const std::vector<std::pair<std::string, int>>& list){
            Boxes::BoxCensus made;
            made.boxes = Boxes::BOX_COUNT;
            made.closed = true;
            int cell = 0;
            for (const auto& item : list){
                Boxes::BoxPokemon one;
                one.box = 13;
                one.cell = cell++;
                one.slug = item.first;
                one.level = item.second;
                made.pokemon.push_back(one);
            }
            return made;
        };
        auto find_line = [](const std::vector<Boxes::EvolveLine>& plan, const std::string& slug){
            for (const Boxes::EvolveLine& line : plan){
                if (line.slug == slug){
                    return line;
                }
            }
            return Boxes::EvolveLine();
        };
        //  After the Day Care: three TYROGUE of level 5, the Dojo's HITMONLEE had.
        const std::set<std::string> had{"hitmonlee", "tyrogue"};
        const std::vector<Boxes::EvolveLine> plan = Boxes::evolve_plan(
            table_with(had), census_of({{"tyrogue", 5}, {"tyrogue", 5}, {"tyrogue", 5}, {"hitmonlee", 25}})
        );
        const Boxes::EvolveLine chan = find_line(plan, "hitmonchan");
        const Boxes::EvolveLine top = find_line(plan, "hitmontop");
        const std::vector<Boxes::LevelTarget> targets = Boxes::level_targets(plan);
        check(
            find_line(plan, "hitmonlee").slug.empty() &&
            chan.from_slug == "tyrogue" && chan.how.by == Boxes::EvolveBy::OTHER && chan.how.level == 20 &&
            chan.box == 13 && chan.cell == 0 && chan.level == 5 &&
            top.from_slug == "tyrogue" && top.how.level == 20 && top.box == 13 && top.cell == 1,
            "TYROGUE: a line for the HITMONCHAN that is wanted and one for HITMONTOP, two TYROGUE of the boxes"
        );
        check(
            targets.size() == 2 && targets[0].slug == "hitmonchan" && targets[0].goal == 20 &&
            targets[0].evolves_at == 20 && Boxes::levels_to_go(targets[0]) == 15 &&
            targets[1].slug == "hitmontop" && targets[1].goal == 20 &&
            Boxes::level_targets(plan, {"hitmonchan"}).size() == 1 &&
            Boxes::exp_targets(plan).empty() && Boxes::stone_targets(plan).empty() &&
            Boxes::friend_targets(plan, std::map<std::string, Boxes::FriendRecord>()).empty(),
            "TYROGUE: trained to level 20 by level, not by the Elite Four's rounds, a stone or friendship"
        );
        //  The first of them became HITMONLEE: the next two are the ones left.
        const std::vector<Boxes::EvolveLine> after_lee = Boxes::evolve_plan(
            table_with(had), census_of({{"hitmonlee", 20}, {"tyrogue", 5}, {"tyrogue", 5}, {"hitmonlee", 25}})
        );
        //  HITMONCHAN came: HITMONTOP is the only one left to try for; and both: nothing.
        const std::vector<Boxes::LevelTarget> one_left = Boxes::level_targets(Boxes::evolve_plan(
            table_with({"hitmonlee", "hitmonchan", "tyrogue"}), census_of({{"hitmonchan", 20}, {"tyrogue", 5}, {"tyrogue", 5}})
        ));
        const std::vector<Boxes::EvolveLine> none_left = Boxes::evolve_plan(
            table_with({"hitmonlee", "hitmonchan", "hitmontop", "tyrogue"}), census_of({{"tyrogue", 5}})
        );
        //  No TYROGUE in the boxes (before the Day Care): no line for it.
        const std::vector<Boxes::EvolveLine> before = Boxes::evolve_plan(
            table_with({"hitmonlee"}), census_of({{"hitmonlee", 25}})
        );
        check(
            find_line(after_lee, "hitmonchan").cell == 1 && find_line(after_lee, "hitmontop").cell == 2 &&
            one_left.size() == 1 && one_left[0].slug == "hitmontop" && one_left[0].cell == 1 &&
            Boxes::level_targets(none_left).empty() && find_line(none_left, "hitmonchan").slug.empty() &&
            find_line(before, "hitmonchan").slug.empty(),
            "TYROGUE: the next ones after a HITMONLEE, only HITMONTOP once HITMONCHAN is there, none when both are, "
            "and no line without a TYROGUE in a box"
        );
        const std::set<std::string> tyrogue_line = Boxes::training_line("tyrogue", "hitmonchan");
        check(
            tyrogue_line == std::set<std::string>({"tyrogue", "hitmonlee", "hitmonchan", "hitmontop"}) &&
            Boxes::training_line("hitmonlee", "hitmonchan") == tyrogue_line &&
            Boxes::training_line("metapod", "butterfree") == std::set<std::string>({"metapod", "butterfree"}),
            "TYROGUE in training can be any of its three evolutions afterwards, the others the two"
        );
    }

    //  Evolving by friendship (build 45): the steps to walk, the file of what
    //  was walked, which of the boxes comes first, and the segments.
    {
        check(
            Boxes::friend_base("golbat") == 70 && Boxes::friend_base("chansey") == 140 &&
            Boxes::friend_base("togepi") == 120 && Boxes::friend_base("zubat") == 70 &&
            Boxes::friend_need("golbat") == 150 && Boxes::friend_need("chansey") == 80 &&
            Boxes::friend_need("togepi") == 100 && Boxes::FRIEND_EVOLVES == 220 && Boxes::FRIEND_BLOCK_STEPS == 128,
            "the friendship a species starts from, and how many points it needs for 220"
        );
        //  A block gives a point with a chance of one in two: for 150 points,
        //  324 blocks are 92 chances in 100 (exact: 0.9176), 378 are all but
        //  sure.
        check(
            Boxes::friend_blocks(0, 1.28) == 0 && Boxes::friend_blocks(-5, 1.28) == 0 &&
            Boxes::friend_blocks(150, 0.0) == 300 && Boxes::friend_blocks(150, 1.28) == 324 &&
            Boxes::friend_blocks(80, 1.28) == 178 && Boxes::friend_blocks(100, 1.28) == 219 &&
            Boxes::friend_blocks(150, 4.0) == 378 &&
            Boxes::friend_blocks(150, 2.0) > Boxes::friend_blocks(150, 1.0) &&
            Boxes::friend_blocks(151, 1.28) > Boxes::friend_blocks(150, 1.28),
            "the blocks of 128 steps to walk for the friendship a species needs"
        );
        bool wider = true;
        for (int tests = 0; tests + 1 < Boxes::FRIEND_TESTS; tests++){
            wider &= Boxes::friend_blocks_wanted("golbat", tests + 1) > Boxes::friend_blocks_wanted("golbat", tests);
        }
        check(
            wider && Boxes::friend_blocks_wanted("golbat", 0) == 324 && Boxes::friend_blocks_wanted("golbat", 3) == 378 &&
            Boxes::friend_blocks_wanted("golbat", 5) == 400 &&
            Boxes::friend_blocks_wanted("golbat", 99) == Boxes::friend_blocks_wanted("golbat", 5) &&
            Boxes::friend_blocks_wanted("golbat", -1) == Boxes::friend_blocks_wanted("golbat", 0) &&
            Boxes::friend_blocks_wanted("chansey", 0) == 178 && Boxes::friend_blocks_wanted("togepi", 0) == 219,
            "a wider margin of steps after each level trained without the evolution"
        );

        Boxes::FriendRecords records;
        records["golbat"].blocks = 120;
        records["golbat"].tests = 1;
        records["chansey"].blocks = 0;
        records["chansey"].tests = 0;
        const std::string text = Boxes::friend_to_text(records);
        Boxes::FriendRecords back;
        Boxes::FriendRecords bad;
        check(
            text == "friend golbat 120 1\n" && Boxes::friend_from_text(text, back) && back.size() == 1 &&
            back["golbat"].blocks == 120 && back["golbat"].tests == 1 &&
            Boxes::friend_from_text("", bad) && bad.empty() &&
            Boxes::friend_from_text("# a comment\r\n\r\nfriend togepi 5 0\r\nfriend golbat 7 2", bad) && bad.size() == 2 &&
            bad["togepi"].blocks == 5 && bad["golbat"].tests == 2 &&
            !Boxes::friend_from_text("friend golbat 1", bad) && !Boxes::friend_from_text("friend golbat x 1", bad) &&
            !Boxes::friend_from_text("friend golbat 1 -1", bad) && !Boxes::friend_from_text("walked golbat 1 1", bad) &&
            !Boxes::friend_from_text("friend golbat 1 1 1", bad) && bad.empty(),
            "the file of what was walked for the friendship, and what is not taken from it"
        );

        //  The boxes of the level test, with a CHANSEY more.
        Boxes::BoxCensus with_chansey = census;
        {
            Boxes::BoxPokemon one;
            one.box = 1;
            one.cell = 10;
            one.slug = "chansey";
            one.level = 20;
            with_chansey.pokemon.push_back(one);
        }
        const std::vector<Boxes::EvolveLine> plan = Boxes::evolve_plan(table, with_chansey);
        const std::vector<Boxes::LevelTarget> only_golbat = Boxes::friend_targets(Boxes::evolve_plan(table, census), Boxes::FriendRecords());
        const std::vector<Boxes::LevelTarget> fresh = Boxes::friend_targets(plan, Boxes::FriendRecords());
        Boxes::FriendRecords nearly;
        nearly["golbat"].blocks = 320;
        Boxes::FriendRecords all_tried;
        all_tried["golbat"].tests = Boxes::FRIEND_TESTS;
        const std::vector<Boxes::LevelTarget> golbat_near = Boxes::friend_targets(plan, nearly);
        const std::vector<Boxes::LevelTarget> golbat_out = Boxes::friend_targets(plan, all_tried);
        const std::vector<Boxes::LevelTarget> chansey_out = Boxes::friend_targets(plan, Boxes::FriendRecords(), {"blissey"});
        check(
            only_golbat.size() == 1 && only_golbat[0].slug == "crobat" && only_golbat[0].from_slug == "golbat" &&
            only_golbat[0].friendship && only_golbat[0].level == 30 && only_golbat[0].goal == 31 &&
            only_golbat[0].blocks == 324 && only_golbat[0].walked == 0 && only_golbat[0].tests == 0 &&
            fresh.size() == 2 && fresh[0].slug == "blissey" && fresh[0].goal == 21 && fresh[0].blocks == 178 &&
            fresh[1].slug == "crobat" &&
            golbat_near.size() == 2 && golbat_near[0].slug == "crobat" && golbat_near[0].walked == 320 &&
            golbat_near[1].slug == "blissey" &&
            golbat_out.size() == 1 && golbat_out[0].slug == "blissey" &&
            chansey_out.size() == 1 && chansey_out[0].slug == "crobat" &&
            Boxes::level_targets(plan).size() == 2,
            "the Pokemon of the boxes that evolve by friendship: the fewest blocks still to walk first, one at its "
            "next level, left out after six levels without it, and not mixed with those that evolve by a level"
        );
        check(
            Boxes::friend_targets_text(fresh) ==
                "Blissey (CHANSEY Lv 20, box 2 cell 11, 0 of 178 blocks of 128 steps walked), "
                "Crobat (GOLBAT Lv 30, box 1 cell 10, 0 of 324 blocks of 128 steps walked)" &&
            Boxes::friend_targets_text(golbat_near).find("320 of 324 blocks") != std::string::npos,
            "the targets as one line for the log"
        );

        Boxes::RaiseFile file;
        file.out = true;
        file.box = 0;
        file.cell = 9;
        file.extra = "articuno";
        file.from = "golbat";
        file.becomes = "crobat";
        file.goal = 31;
        file.friendship = true;
        Boxes::RaiseFile read;
        Boxes::RaiseFile plain;
        plain.out = true;
        plain.box = 0;
        plain.cell = 9;
        plain.extra = "articuno";
        plain.from = "golbat";
        plain.becomes = "crobat";
        plain.goal = 31;
        Boxes::RaiseFile other;
        check(
            Boxes::raise_to_text(file) == "raise 1 0 9 articuno golbat crobat 31 friend\n" &&
            Boxes::raise_from_text(Boxes::raise_to_text(file), read) && read.out && read.friendship && read.goal == 31 &&
            Boxes::raise_to_text(plain) == "raise 1 0 9 articuno golbat crobat 31\n" &&
            Boxes::raise_from_text(Boxes::raise_to_text(plain), read) && read.out && !read.friendship &&
            !Boxes::raise_from_text("raise 1 0 9 articuno golbat crobat 31 level", other) &&
            !Boxes::raise_from_text("raise 1 0 9 articuno golbat crobat 31 friend more", other),
            "the file of a training under way says when it is the one after a walk for friendship"
        );
        check(
            story_friends_place() == "the evolving by friendship" && story_friend_place("crobat") == "walking for CROBAT" &&
            story_friend_place("crobat") != story_level_place("crobat"),
            "the places of the friendship segments for the failures of a run"
        );

        int friends = 0;
        bool shape = true;
        for (size_t s = 0; s < script.segment_count; s++){
            const StorySegment& segment = script.segments[s];
            if (segment.step_count < 3 || script.step(segment, 0).type != StepType::SKIP_IF_NO_LEVELS ||
                script.step(segment, 0).p1 != 1){
                continue;
            }
            friends++;
            size_t out_at = 0;
            size_t pace_at = 0;
            size_t raise_at = 0;
            size_t in_at = 0;
            int outs = 0;
            int paces = 0;
            int raises = 0;
            int ins = 0;
            int saves = 0;
            for (size_t k = 0; k < segment.step_count; k++){
                const StoryStep& step = script.step(segment, k);
                if (step.type == StepType::LEVEL_OUT){
                    out_at = k;
                    outs++;
                    shape &= step.p1 == 1;
                }else if (step.type == StepType::PACE){
                    pace_at = k;
                    paces++;
                    //  A line of 3 tiles or more between two walls, and the direction along it.
                    shape &= step.p1 >= 3 && step.dir != DPAD_NONE;
                }else if (step.type == StepType::RAISE){
                    raise_at = k;
                    raises++;
                }else if (step.type == StepType::LEVEL_IN){
                    in_at = k;
                    ins++;
                }else if (step.type == StepType::SAVE){
                    saves++;
                }
            }
            shape &= story_once_segment(script, segment) && outs == 1 && paces == 1 && raises == 1 && ins == 1 &&
                saves == 1 && out_at < pace_at && pace_at < raise_at && raise_at < in_at &&
                (size_t)script.step(segment, out_at).p0 == in_at - out_at &&
                script.step(segment, 1).type == StepType::FLY &&
                script.step(segment, segment.step_count - 1).type == StepType::SAVE &&
                script.step(segment, pace_at + 1).type == StepType::WALK;
            //  Given up from a save in it: not possible (the Pokemon is out of its box).
            for (size_t i = 2; i + 1 < segment.step_count; i++){
                shape &= story_once_leave(script, segment, i) == OnceLeave::NO;
            }
        }
        check(
            friends == 11 && shape,
            "the eleven segments that evolve by friendship (eight before the egg, three after it): FLY, the PC, the walk, the training, the PC, a save at the end only"
        );
    }

    //  The Elite Four for experience (build 46): which of the boxes, the
    //  places, and the five rounds.
    {
        check(
            Boxes::EXP_ROUND_MIN == 20000 && Boxes::exp_to_go(43, 55) == 86868 && Boxes::exp_to_go(55, 55) == 0 &&
            Boxes::exp_to_go(56, 55) == 0 && Boxes::exp_to_go(0, 50) == 0 && Boxes::exp_to_go(-1, 50) == 0 &&
            Boxes::exp_to_go(50, 101) == 0 && Boxes::exp_to_go(99, 100) == 1000000 - 970299,
            "the experience points to go from a level to another, on the curve of the medium fast group"
        );
        Boxes::BoxCensus more = census;
        auto add = [&](int box, int cell, const char* slug, int level){
            Boxes::BoxPokemon one;
            one.box = box;
            one.cell = cell;
            one.slug = slug;
            one.level = level;
            more.pokemon.push_back(one);
        };
        //  Nothing of the census so far has 20000 points to go (PIDGEOTTO Lv 40
        //  to 41 is 4921, METAPOD Lv 8 to 10 is 488).
        const bool none_yet = Boxes::exp_targets(Boxes::evolve_plan(table, more)).empty();
        add(3, 0, "pupitar", 43);
        const std::vector<Boxes::LevelTarget> one = Boxes::exp_targets(Boxes::evolve_plan(table, more));
        const std::vector<Boxes::LevelTarget> left_out = Boxes::exp_targets(Boxes::evolve_plan(table, more), {"tyranitar"});
        more.pokemon.back().level = 52;
        const std::vector<Boxes::LevelTarget> edge = Boxes::exp_targets(Boxes::evolve_plan(table, more));
        more.pokemon.back().level = 53;
        const std::vector<Boxes::LevelTarget> below = Boxes::exp_targets(Boxes::evolve_plan(table, more));
        more.pokemon.back().level = -1;
        const std::vector<Boxes::LevelTarget> unknown = Boxes::exp_targets(Boxes::evolve_plan(table, more));
        check(
            none_yet && one.size() == 1 && one[0].slug == "tyranitar" && one[0].from_slug == "pupitar" &&
            one[0].level == 43 && one[0].goal == 55 && one[0].exp == 86868 && one[0].box == 3 && one[0].cell == 0 &&
            left_out.empty() && edge.size() == 1 && edge[0].exp == 25767 && below.empty() && unknown.empty(),
            "the Pokemon of the boxes for a round of the Elite Four: 20000 points to go or more, a known level, the "
            "most first"
        );
        //  Build 56: the segments by level leave the ones with 20000 points to go
        //  or more to the rounds of the Elite Four, unless they failed there.
        {
            auto light = [&](int level, const std::set<std::string>& exp_without = std::set<std::string>()){
                more.pokemon.back().level = level;
                return Boxes::level_targets_light(Boxes::evolve_plan(table, more), std::set<std::string>(), exp_without);
            };
            const std::vector<Boxes::LevelTarget> heavy = light(43);
            const std::vector<Boxes::LevelTarget> failed = light(43, {"tyranitar"});
            const std::vector<Boxes::LevelTarget> at_edge = light(52);
            const std::vector<Boxes::LevelTarget> just_below = light(53);
            const std::vector<Boxes::LevelTarget> not_known = light(-1);
            //  (The census has light targets of its own: the PIDGEOTTO and the METAPOD.)
            auto has = [](const std::vector<Boxes::LevelTarget>& targets){
                for (const Boxes::LevelTarget& target : targets){
                    if (target.slug == "tyranitar"){
                        return true;
                    }
                }
                return false;
            };
            check(
                !has(heavy) && has(failed) && !has(at_edge) && has(just_below) && has(not_known) &&
                !heavy.empty() && heavy.size() + 1 == failed.size() && heavy.size() + 1 == just_below.size(),
                "the segments by level leave a Pokemon with 20000 points to go or more to the Elite Four, unless it "
                "failed there twice in a row (then it is trained at Victory Road); a level that is not known is not heavy"
            );
        }
        more.pokemon.back().level = 43;
        check(
            Boxes::exp_targets_text(Boxes::exp_targets(Boxes::evolve_plan(table, more))) ==
                "Tyranitar (PUPITAR Lv 43, box 4 cell 1, to level 55, about 86868 points to go)" &&
            story_exp_rounds_place() == "the Elite Four for experience" &&
            story_exp_place("tyranitar") == "the Elite Four for TYRANITAR" &&
            story_exp_place("tyranitar") != story_level_place("tyranitar"),
            "the targets as one line for the log, and the places of the rounds for the failures of a run"
        );

        int rounds = 0;
        bool shape = true;
        for (size_t s = 0; s < script.segment_count; s++){
            const StorySegment& segment = script.segments[s];
            if (segment.step_count < 3 || script.step(segment, 0).type != StepType::SKIP_IF_NO_LEVELS ||
                script.step(segment, 0).p1 != 2){
                continue;
            }
            rounds++;
            size_t out_at = 0;
            size_t in_at = 0;
            size_t nurse_at = 0;
            size_t champion_at = 0;
            int outs = 0;
            int ins = 0;
            int raises = 0;
            int nurses = 0;
            int resets = 0;
            int flies = 0;
            for (size_t k = 0; k < segment.step_count; k++){
                const StoryStep& step = script.step(segment, k);
                if (step.type == StepType::LEVEL_OUT){
                    out_at = k;
                    outs++;
                    shape &= step.p1 == 2;
                }else if (step.type == StepType::LEVEL_IN){
                    in_at = k;
                    ins++;
                }else if (step.type == StepType::RAISE || step.type == StepType::PACE){
                    raises++;
                }else if (step.type == StepType::HEAL){
                    if (nurses == 0){
                        nurse_at = k;
                    }
                    nurses++;
                }else if (step.type == StepType::RESET){
                    resets++;
                    champion_at = k;
                }else if (step.type == StepType::FLY){
                    flies++;
                }
            }
            //  The League (the nurse first, for the loss handler), the Hall of
            //  Fame once, and FLY at the start and again after Oak's rating.
            shape &= story_once_segment(script, segment) && outs == 1 && ins == 1 && raises == 0 && resets == 1 &&
                flies == 2 && nurses >= 1 && out_at < nurse_at && nurse_at < champion_at && champion_at < in_at &&
                (size_t)script.step(segment, out_at).p0 == in_at - out_at &&
                script.step(segment, 1).type == StepType::FLY &&
                script.step(segment, segment.step_count - 1).type == StepType::SAVE &&
                script.step(segment, in_at + 1).type == StepType::WALK &&
                std::string(segment.id).find("_ELITE_FOUR") != std::string::npos &&
                std::string(segment.id).rfind("C20_EXP_", 0) == 0;
        }
        check(
            rounds == 28 && shape,
            "the 28 rounds of the Elite Four for experience (ten before the segments by level, twelve after them, "
            "six after the Day Care's): FLY, the PC, the nurse, the League, the Hall of Fame, FLY back, the PC, a "
            "save at the end"
        );
    }

    logger.log(
        "AutoStory test: the PC's boxes are read, and what is in them gets its stone or is traded (the PC's "
        "texts, the cells, the list as a file, the six stones, the trades for " + trade_gets + "; of the " +
        std::to_string(table.size()) + " species of FireRed with Charmander " +
        std::to_string(by_level + by_stone + by_friendship + by_other) + " only come by evolving: " +
        std::to_string(by_level) + " by level, " + std::to_string(by_stone) + " by a stone, " +
        std::to_string(by_friendship) + " by friendship, " + std::to_string(by_other) + " by level and stats; " +
        std::to_string(hows) + " ways of evolving in the table; in the story's data the segments " + where +
        ", C20_25_STONES, four of trades, four that fetch a stone, ten rounds of the Elite Four, 29 of levels and eleven of "
        "friendship): " +
        std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}

//  What the program calls a species is what Pokemon Automation calls it:
//  its readers of the summary and of the box give those names back, and the
//  program compares them with its own (18 November, build 38: "farfetch-d"
//  read, "farfetchd" in the table, and the trade of the FARFETCH'D failed
//  with the Pokemon in the party). Against the list of Pokemon Automation's
//  (Resources/Pokemon/Pokedex/Pokedex-National.json): the species the
//  program knows, the POKeDEX table with its numbers, the species the
//  story's steps name.
int check_species_names(Logger& logger){
    using namespace NintendoSwitch;
    using namespace NintendoSwitch::PokemonFRLG;
    const std::map<std::string, size_t>* dex = nullptr;
    try{
        dex = &Pokemon::SLUGS_TO_NATIONAL_DEX();
    }catch (const Exception& e){
        logger.log("AutoStory test: the species names were not compared: " + e.to_str(), COLOR_ORANGE);
        return 0;
    }catch (const std::exception& e){
        logger.log(std::string("AutoStory test: the species names were not compared: ") + e.what(), COLOR_ORANGE);
        return 0;
    }
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool fine, const std::string& what){
        checks++;
        if (!fine){
            wrong++;
            logger.log("AutoStory test: failed: " + what, COLOR_RED);
        }
    };
    auto unknown = [&](const std::string& slug){
        return dex->count(slug) == 0;
    };

    std::string not_known;
    size_t known_species = 0;
    for (const AutoStoryDex::SpeciesInfo& info : AutoStoryDex::species()){
        known_species++;
        if (unknown(info.slug)){
            not_known += (not_known.empty() ? "" : ", ") + std::string(info.slug);
        }
    }
    check(known_species > 300 && not_known.empty(), "species the program knows that Pokemon Automation names otherwise: " + not_known);

    size_t rows = 0;
    std::string table_wrong;
    {
        size_t count = 0;
        const AutoStoryDex::CollectionEntry* entries = AutoStoryDex::collection(count);
        for (size_t k = 0; k < count; k++){
            rows++;
            auto iter = dex->find(entries[k].slug);
            if (iter == dex->end() || iter->second != (size_t)entries[k].dex){
                table_wrong += (table_wrong.empty() ? "" : ", ") + std::string(entries[k].slug);
            }
        }
    }
    check(rows > 150 && table_wrong.empty(), "POKeDEX table rows whose name or number is not Pokemon Automation's: " + table_wrong);

    //  The steps that name one species: the table asked (SKIP_IF_OWNED),
    //  the one a trade gives, the one a box gives out.
    size_t named = 0;
    std::string steps_wrong;
    const StoryScript& script = AutoStoryData::story_CHARMANDER();
    for (size_t k = 0; k < script.step_count; k++){
        const StoryStep& step = script.steps[k];
        if (step.type != StepType::SKIP_IF_OWNED && step.type != StepType::TRADE && step.type != StepType::BOX_OUT){
            continue;
        }
        named++;
        if (unknown(step.text)){
            steps_wrong += (steps_wrong.empty() ? "" : ", ") + std::string(step.text);
        }
    }
    check(named > 5 && steps_wrong.empty(), "species the story's steps name that Pokemon Automation names otherwise: " + steps_wrong);

    logger.log(
        "AutoStory test: the species names are Pokemon Automation's (" + std::to_string(known_species) + " species, " +
        std::to_string(rows) + " rows of the POKeDEX table with their numbers, " + std::to_string(named) +
        " species the story's steps name): " + std::to_string(checks - wrong) + " of " + std::to_string(checks) +
        " checks passed.",
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
        failed += check_output_window(window, logger);
        failed += check_text_colors(logger);
        failed += check_settings_file(window, logger);
        failed += check_older_audio_settings(logger);
        failed += check_assistant(out, logger);
        failed += check_name_mark(logger);
        failed += check_updates(logger);
        failed += check_story_times(logger);
        failed += check_shiny_clip(logger);
        failed += check_history_frames(logger);
        failed += check_periodic_runner(logger);
        failed += check_old_settings(logger);
        failed += run_instances_checks(logger);
        failed += check_collection_lists(out, logger);
        failed += check_money_trip(logger);
        failed += check_ferry_talk(logger);
        failed += check_bump(logger);
        failed += check_slow_steps(logger);
        failed += check_poison_flash(logger);
        failed += check_safari_table(logger);
        failed += check_next_steps(logger);
        failed += check_vitamins_and_coins(logger);
        failed += check_leave_out(logger);
        failed += check_boxes(logger);
        failed += check_species_names(logger);
        return failed == 0 ? 0 : 1;
    }catch (const Exception& e){
        logger.log("UI test: stopped by an exception: " + e.to_str(), COLOR_RED);
    }catch (const std::exception& e){
        logger.log(std::string("UI test: stopped by an exception: ") + e.what(), COLOR_RED);
    }
    return 1;
}


namespace{

//  The window as the screens of the setup: from the games to a program and the
//  settings and back, with Claude's panel over the right. The real programs
//  and settings are opened, as a person opens them. Returns how many checks
//  failed.
int check_app_interface(const QString& out, Logger& logger){
    int checks = 0;
    int wrong = 0;
    auto check = [&](bool ok, const char* what){
        checks++;
        if (!ok){
            wrong++;
            logger.log(std::string("App interface test: FAILED: ") + what, COLOR_RED);
        }
    };
    //  Light on a black ground: something is drawn (not only the black).
    auto has_content = [](const QImage& image){
        int lit = 0;
        for (int y = 0; y < image.height(); y += 8){
            for (int x = 0; x < image.width(); x += 8){
                if (image.pixelColor(x, y).lightness() > 40){
                    lit++;
                }
            }
        }
        return lit > 60;
    };

    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen, true);
    window.resize(1600, 900);
    window.show();
    PocketPilot::AppView* view = window.app_view();
    check(view != nullptr, "the window has the screens of the setup");
    if (view == nullptr){
        return wrong;
    }
    int number = 0;
    auto save = [&](const char* name, bool preview){
        number++;
        let_run(100);
        const QImage picture = view->picture();
        const QString base = QString::number(number).rightJustified(2, '0') + "-" + name;
        if (picture.save(out + "/" + base + ".png")){
            logger.log("App interface test: saved " + base.toStdString() + ".png");
        }else{
            wrong++;
            logger.log("App interface test: could not save " + base.toStdString() + ".png", COLOR_RED);
        }
        if (preview){
            picture.scaledToWidth(960, Qt::SmoothTransformation).save(out + "/preview-" + base + ".jpg", "JPG", 72);
        }
        return picture;
    };
    auto menu_there = [&]{
        return view->has_button("app:settings") && view->has_button("app:claude") &&
            view->has_button("app:log") && view->has_button("app:buttons") && view->has_button("app:about");
    };

    //  ---- the games
    logger.log("App interface test: the games.");
    view->wait(2.5);
    check(view->page() == "games", "the window starts on the games");
    check(menu_there(), "the buttons of the top bar are along the top");
    check(!view->has_button("app:games"), "no way to the games on the games");
    check(!view->has_button("later"), "no 'Choose later': there is nothing to leave");
    check(has_content(save("games", true)), "the games are drawn");

    //  A game that has programs.
    logger.log("App interface test: a game and its programs.");
    for (int c = 0; c < 40 && view->page() != "programs"; c++){
        const QString tile = "game:" + QString::number(c);
        if (!view->has_button(tile)){
            break;
        }
        if (view->click(tile)){
            view->wait(0.2);
            view->click("start");
            view->wait(1.8);
        }
    }
    check(view->page() == "programs", "a game with programs leads to its programs");
    if (view->page() != "programs"){
        return wrong;
    }
    check(menu_there() && view->has_button("app:games"), "the buttons of the top bar are on the programs too");
    check(view->has_button("all") && !view->has_button("later"), "back to the games, and no 'Choose later'");
    check(has_content(save("programs", true)), "the programs are drawn");

    //  The program itself, in the screen.
    logger.log("App interface test: the program.");
    check(view->click("start"), "Start can be pressed");
    view->wait(1.8);
    check(view->page() == "host" && view->hosting(), "Start opens the program in the window, it does not leave");
    check(view->has_button("host-back") && view->has_button("host-sleep"), "the way back and the sleep switch are there");
    check(menu_there() && view->has_button("app:games"), "the buttons of the top bar are on the program too");
    check(has_content(save("program", true)), "the program is drawn");
    let_run(300);

    //  Claude.
    logger.log("App interface test: Claude.");
    check(!view->claude_shown(), "Claude is not shown at first");
    view->click("app:claude");
    view->wait(0.3);
    check(view->claude_shown(), "the button shows Claude");
    check(has_content(save("program-claude", true)), "Claude's panel is drawn");
    view->click("app:claude");
    view->wait(0.2);
    check(!view->claude_shown(), "and hides it again");

    //  Settings over the program.
    logger.log("App interface test: the settings.");
    view->click("app:settings");
    view->wait(1.8);
    check(view->page() == "host" && view->hosting(), "the settings open in the window");
    check(has_content(save("settings", true)), "the settings are drawn");

    //  Back.
    logger.log("App interface test: back.");
    check(view->click("host-back"), "the way back can be pressed");
    view->wait(1.8);
    check(view->page() == "games" && !view->hosting(), "back from the settings leads to the games");

    //  The games button closes a program that is open.
    check(view->click("app:settings"), "the settings can be opened from the games");
    view->wait(1.8);
    check(view->hosting(), "they are open");
    check(view->click("app:games"), "the games button can be pressed");
    view->wait(1.8);
    check(view->page() == "games" && !view->hosting(), "and it closes them");

    logger.log(
        "App interface test: " + std::to_string(checks - wrong) + " of " + std::to_string(checks) + " checks passed.",
        wrong == 0 ? COLOR_BLUE : COLOR_RED
    );
    return wrong;
}

}


int run_app_screenshots(QApplication& application, const std::string& folder){
    (void)application;
    Logger& logger = global_logger_tagged();
    const QString out = QString::fromStdString(folder);
    QDir().mkpath(out);
    logger.log("App interface test: pictures go to " + QDir(out).absolutePath().toStdString(), COLOR_BLUE);
    try{
        return check_app_interface(out, logger) == 0 ? 0 : 1;
    }catch (const Exception& e){
        logger.log("App interface test: stopped by an exception: " + e.to_str(), COLOR_RED);
    }catch (const std::exception& e){
        logger.log(std::string("App interface test: stopped by an exception: ") + e.what(), COLOR_RED);
    }
    return 1;
}



}
}

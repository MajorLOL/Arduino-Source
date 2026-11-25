/*  PocketPilot Claude Plays: the program
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <atomic>
#include <string>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/ImageTypes/ImageRGB32_Qt.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Joycon/NintendoSwitch_Joycon.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonBDSP/Programs/PokemonBDSP_GameNavigation.h"
#include "VideoGames/PokemonLA/Programs/PokemonLA_GameSave.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "VideoGames/PokemonSwSh/Programs/PokemonSwSh_MenuNavigation.h"
#include "ClaudePlays_AnyGame.h"
#include "ClaudePlays_GameCard.h"
#include "ClaudePlays_Program.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace ClaudePlays{

using namespace NintendoSwitch;



std::unique_ptr<PanelDescriptor> make_claude_plays(GameId game){
    return std::make_unique<GameProgram_Descriptor>(game);
}


namespace{

std::string description_of(const GameProfile& profile){
    return
        "Claude plays " + profile.title + " toward a goal you write down, on your own Claude API key: it "
        "looks at the screen, has the text on it read, and works the controller.";
}

}


GameProgram_Descriptor::GameProgram_Descriptor(GameId game)
    : SingleSwitchProgramDescriptor(
        game_profile(game).identifier,
        game_profile(game).category, "Claude Plays",
        "",
        description_of(game_profile(game)),
        PanelQualityStanding::EXPERIMENTAL,
        game_profile(game).pad == PadKind::RIGHT_JOYCON
            ? ProgramControllerClass::SpecializedController
            : ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        {
            //  Multi-line OCR (the text on the screen).
            "PaddleOCRDetection",
        }
    )
    , m_game(game)
{}

struct GameProgram_Descriptor::Stats : public StatsTracker{
    Stats()
        : actions(m_stats["Actions"])
        , requests(m_stats["Requests to Claude"])
        , tokens(m_stats["Tokens"])
    {
        m_display_order.emplace_back("Actions");
        m_display_order.emplace_back("Requests to Claude");
        m_display_order.emplace_back("Tokens");
    }

    std::atomic<uint64_t>& actions;
    std::atomic<uint64_t>& requests;
    std::atomic<uint64_t>& tokens;
};

std::unique_ptr<StatsTracker> GameProgram_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}
std::unique_ptr<GameConsole::ConsoleProgramInstance> GameProgram_Descriptor::make_instance(
    GameConsole::ConsoleSystemSession&
) const{
    return std::make_unique<GameProgram>(m_game);
}



namespace{


//  The buttons by the names Claude uses.
bool button_of(const std::string& name, Button& button){
    struct Entry{
        const char* name;
        Button button;
    };
    static const Entry ENTRIES[] = {
        {"A",       BUTTON_A},
        {"B",       BUTTON_B},
        {"X",       BUTTON_X},
        {"Y",       BUTTON_Y},
        {"L",       BUTTON_L},
        {"R",       BUTTON_R},
        {"ZL",      BUTTON_ZL},
        {"ZR",      BUTTON_ZR},
        {"PLUS",    BUTTON_PLUS},
        {"MINUS",   BUTTON_MINUS},
        {"LSTICK",  BUTTON_LCLICK},
        {"RSTICK",  BUTTON_RCLICK},
        //  Of a right Joy-Con.
        {"STICK",   BUTTON_RCLICK},
        {"HOME",    BUTTON_HOME},
    };
    for (const Entry& entry : ENTRIES){
        if (name == entry.name){
            button = entry.button;
            return true;
        }
    }
    return false;
}

//  The d-pad, from the directions that are held.
DpadPosition dpad_of(bool up, bool down, bool left, bool right){
    if (up && right){
        return DPAD_UP_RIGHT;
    }
    if (up && left){
        return DPAD_UP_LEFT;
    }
    if (down && right){
        return DPAD_DOWN_RIGHT;
    }
    if (down && left){
        return DPAD_DOWN_LEFT;
    }
    if (up){
        return DPAD_UP;
    }
    if (down){
        return DPAD_DOWN;
    }
    if (left){
        return DPAD_LEFT;
    }
    if (right){
        return DPAD_RIGHT;
    }
    return DPAD_NONE;
}

std::string read_picture(const ImageViewRGB32& picture){
    //  Every line of text in the picture, from the top.
    return OCR::ocr_read(Language::English, picture, OCR::PageSegMode::AUTO);
}


//  The console with a Pro Controller.
class ProConsole : public PlayConsole{
public:
    ProConsole(SingleSwitchProgramEnvironment& env, ProControllerContext& context, GameId game)
        : m_env(env)
        , m_context(context)
        , m_game(game)
    {}

    virtual const PadLayout& layout() const override{
        return pro_controller_layout();
    }
    virtual void hold(const PadInput& input, std::chrono::milliseconds duration) override{
        Button buttons = BUTTON_NONE;
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
        for (const std::string& name : input.buttons){
            Button button = BUTTON_NONE;
            if (button_of(name, button)){
                buttons |= button;
            }else if (name == "UP"){
                up = true;
            }else if (name == "DOWN"){
                down = true;
            }else if (name == "LEFT"){
                left = true;
            }else if (name == "RIGHT"){
                right = true;
            }
        }
        pbf_controller_state(
            m_context, buttons, dpad_of(up, down, left, right),
            JoystickPosition(input.stick_x, input.stick_y),
            JoystickPosition(input.stick2_x, input.stick2_y),
            duration
        );
    }
    virtual void mash(const std::string& name, std::chrono::milliseconds duration) override{
        Button button = BUTTON_NONE;
        if (button_of(name, button)){
            pbf_mash_button(m_context, button, duration);
        }
    }
    virtual void wait(std::chrono::milliseconds duration) override{
        pbf_wait(m_context, duration);
    }
    virtual bool screen(const std::function<void(const ImageViewRGB32&)>& use) override{
        m_context.wait_for_all_requests();
        VideoSnapshot snapshot = m_env.console.video().snapshot();
        if (!snapshot){
            return false;
        }
        const ImageViewRGB32& frame = *snapshot.frame;
        use(frame);
        return true;
    }
    virtual std::string read_text(const ImageViewRGB32& picture) override{
        return read_picture(picture);
    }

    virtual bool can_save() const override{
        return game_profile(m_game).save_routine;
    }
    virtual PlayResult save_game() override{
        PlayResult result;
        m_context.wait_for_all_requests();
        try{
            switch (m_game){
            case GameId::SWORD_SHIELD:
                PokemonSwSh::save_game(m_env.console, m_context);
                result.text = "The game is saved.";
                break;
            case GameId::BRILLIANT_DIAMOND_SHINING_PEARL:
                PokemonBDSP::save_game(m_env.console, m_context);
                result.text = "The routine opened the menu and saved the game.";
                break;
            case GameId::LEGENDS_ARCEUS:
                if (PokemonLA::save_game_from_overworld(m_env, m_env.console, m_context)){
                    result.text = "The game is saved.";
                }else{
                    result.is_error = true;
                    result.text =
                        "The game cannot be saved right now: the player is in the air, rolling or falling.";
                }
                break;
            case GameId::SCARLET_VIOLET:
                PokemonSV::save_game_from_menu_or_overworld(m_env.program_info(), m_env.console, m_context, true);
                result.text = "The game is saved, and the player is back in the overworld.";
                break;
            case GameId::LEGENDS_ZA:
                if (PokemonLZA::save_game_to_menu(m_env.console, m_context)){
                    result.text = "The game is saved. Its main menu is open: B closes it.";
                }else{
                    result.is_error = true;
                    result.text = "The game could not be saved here: a dialog came up instead of the question to save.";
                }
                break;
            default:
                result.is_error = true;
                result.text = "This game has no routine to save.";
                break;
            }
        }catch (OperationFailedException& e){
            //  The routine did not understand the screen: for Claude to sort
            //  out, with a look at it.
            m_env.log("Claude Plays: the routine that saves the game gave up: " + e.message(), COLOR_ORANGE);
            result.is_error = true;
            result.text =
                "The program's routine gave up: " + one_line(e.message(), 300) +
                " The game is as that left it: look before you go on.";
        }
        m_context.wait_for_all_requests();
        return result;
    }

private:
    SingleSwitchProgramEnvironment& m_env;
    ProControllerContext& m_context;
    const GameId m_game;
};


//  The console with a single right Joy-Con. A direction is its stick.
class JoyconConsole : public PlayConsole{
public:
    JoyconConsole(SingleSwitchProgramEnvironment& env, JoyconContext& context)
        : m_env(env)
        , m_context(context)
    {}

    virtual const PadLayout& layout() const override{
        return right_joycon_layout();
    }
    virtual void hold(const PadInput& input, std::chrono::milliseconds duration) override{
        Button buttons = BUTTON_NONE;
        for (const std::string& name : input.buttons){
            Button button = BUTTON_NONE;
            if (button_of(name, button)){
                buttons |= button;
            }
        }
        pbf_controller_state(m_context, buttons, JoystickPosition(input.stick_x, input.stick_y), duration);
    }
    virtual void mash(const std::string& name, std::chrono::milliseconds duration) override{
        Button button = BUTTON_NONE;
        if (button_of(name, button)){
            pbf_mash_button(m_context, button, duration);
        }
    }
    virtual void wait(std::chrono::milliseconds duration) override{
        pbf_wait(m_context, duration);
    }
    virtual bool screen(const std::function<void(const ImageViewRGB32&)>& use) override{
        m_context.wait_for_all_requests();
        VideoSnapshot snapshot = m_env.console.video().snapshot();
        if (!snapshot){
            return false;
        }
        const ImageViewRGB32& frame = *snapshot.frame;
        use(frame);
        return true;
    }
    virtual std::string read_text(const ImageViewRGB32& picture) override{
        return read_picture(picture);
    }

private:
    SingleSwitchProgramEnvironment& m_env;
    JoyconContext& m_context;
};


//  The program a session runs in.
class ProgramHost : public Host{
public:
    ProgramHost(
        SingleSwitchProgramEnvironment& env, CancellableScope& scope,
        GameProgram_Descriptor::Stats& stats, bool overlay
    )
        : m_env(env)
        , m_scope(scope)
        , m_stats(stats)
        , m_overlay(overlay)
    {}

    virtual void log(const std::string& text, Color color) override{
        m_env.log(text, color);
    }
    virtual void show(const std::string& text, Color color) override{
        if (m_overlay){
            m_env.console.overlay().add_log(one_line(text, 110), color);
        }
    }
    virtual void count(uint64_t actions, const PlayUsage& usage) override{
        m_stats.actions += actions - m_actions;
        m_stats.requests += usage.requests - m_requests;
        m_stats.tokens += usage.tokens - m_tokens;
        m_actions = actions;
        m_requests = usage.requests;
        m_tokens = usage.tokens;
        m_env.update_stats();
    }
    virtual CancellableScope& scope() override{
        return m_scope;
    }

private:
    SingleSwitchProgramEnvironment& m_env;
    CancellableScope& m_scope;
    GameProgram_Descriptor::Stats& m_stats;
    const bool m_overlay;
    //  What the numbers under the video have already.
    uint64_t m_actions = 0;
    uint64_t m_requests = 0;
    uint64_t m_tokens = 0;
};


std::string setup_text(const GameProfile& profile){
    const bool other = profile.id == GameId::OTHER_GAMES;
    return
        "<b>Experimental:</b> this program is new. It has not been tested on a console, and not with a real "
        "API key either: the build machine has none. Watch it. <b>Many games save by themselves:</b> what "
        "Claude does in the game can be final. Where that matters, make a save or a backup of your own "
        "first.<br>"
        "<b>What it does:</b> Claude, the AI model of Anthropic, plays " +
        (other ? std::string("whatever game is on the screen") : profile.title) +
        " toward the goal below. It gets small pictures of the screen and the text the program reads from "
        "it, and works the controller: buttons pressed, buttons and sticks held, a button mashed. After "
        "every action the program tells it how much of the picture changed, which costs next to nothing. "
        "It is slow next to a program made for one task (every decision is a request to Claude), and it can "
        "get lost: it is for things no program exists for.<br>"
        "<b>What it needs:</b> your Claude API key in the panel \"Claude\" (along the top of the window). Each request is billed "
        "to you by Anthropic; the pictures of the game and the text read from it are sent to Anthropic. What "
        "a session used is in the log and under the video.<br>"
        "<b>Setup:</b> " +
        (other
            ? std::string("a game on the screen that is played with a Pro Controller")
            : std::string("the game on the screen, with the controller its other programs use") +
              (profile.pad == PadKind::RIGHT_JOYCON ? " (a right Joy-Con)" : "")) +
        ". Text is read as English.";
}

}   //  namespace



GameProgram::GameProgram(GameId game)
    : m_game(game)
    , SETUP_TEXT(setup_text(game_profile(game)))
    , GOAL(
        "<b>Goal:</b><br>What Claude should do, in your own words (any language). Say where the game is "
        "at, if that helps, and when the goal is reached.",
        LockMode::LOCK_WHILE_RUNNING,
        "Look at the screen and say which game it shows, where it is at and what could be done from here. "
        "Do not press anything.",
        "For example: Open the menu, look at the first Pokemon of the party and tell me its moves."
    )
    , ABOUT(
        game == GameId::OTHER_GAMES
            ? "<b>About the game (optional):</b><br>Which game it is and what Claude should know about it: "
              "its controls, where the save is, what to stay away from. Without it Claude finds out from the "
              "screen."
            : "<b>About the game (optional):</b><br>What Claude should know on top of what the program "
              "tells it about this game: where the save is, what to stay away from.",
        LockMode::LOCK_WHILE_RUNNING,
        "",
        "For example: The player is in the first town, right after the tutorial."
    )
    , MODEL(
        "<b>Model:</b><br>The smallest model is the fastest and by far the cheapest, and enough for simple "
        "goals. A larger one finds its way better, most of all in a 3D game, and costs many times more.",
        {
            {Model::HAIKU,  "haiku",    "Claude Haiku 5.5 (fastest, cheapest)"},
            {Model::SONNET, "sonnet",   "Claude Sonnet 5.5 (about 20 times the price)"},
            {Model::OPUS,   "opus",     "Claude Opus 5.5 (about 40 times the price)"},
            {Model::FABLE,  "fable",    "Claude Fable 5.2 (about 80 times the price)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        Model::HAIKU
    )
    , MAX_ACTIONS(
        "<b>Stop after actions:</b><br>An action is one thing Claude has the program do: a look, a reading, "
        "some button presses, a wait.",
        LockMode::LOCK_WHILE_RUNNING,
        100, 1, 100000
    )
    , MAX_COST(
        "<b>Stop at a cost of (US dollars):</b><br>Estimated from Anthropic's public prices; the invoice is "
        "what counts. 0: no limit. A spending limit in Anthropic's console holds whatever this program does.",
        LockMode::LOCK_WHILE_RUNNING,
        0.50, 0, 1000
    )
    , PICTURE(
        "<b>Pictures for Claude:</b><br>Of the whole screen. Larger pictures show more and cost more tokens "
        "each. A part of the screen comes at twice the detail whatever is chosen here, and text is cheaper "
        "read than looked at.",
        {
            {Picture::SMALL,    "small",    "384 x 216 (about 112 tokens a picture)"},
            {Picture::NORMAL,   "normal",   "640 x 360 (about 299 tokens)"},
            {Picture::LARGE,    "large",    "960 x 540 (about 700 tokens)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        Picture::NORMAL
    )
    , ALLOW_SAVE(
        game_profile(game).save_routine
            ? "<b>Claude may save the game:</b><br>Off: Claude is told not to save. On: it saves with the "
              "routine of this game's other programs, when the goal asks for it. Either way Claude presses "
              "the buttons, and many games save by themselves: keep a save of your own."
            : "<b>Claude may save the game:</b><br>Off: Claude is told not to save. Either way Claude presses "
              "the buttons, and many games save by themselves: keep a save of your own.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , ALLOW_HOME(
        "<b>Claude may press HOME:</b><br>HOME leaves the game for the console's menu, where other games, "
        "the settings and the shop are. Off: the program does not press it, whatever Claude asks for.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , OVERLAY_NOTES(
        "<b>Show what Claude does on the video:</b><br>What Claude says and each action, in the log on the "
        "video (turn on \"Log\" above the video). The program's own log has all of it either way.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_STATIC(SETUP_TEXT);
    PA_ADD_OPTION(GOAL);
    PA_ADD_OPTION(ABOUT);
    PA_ADD_OPTION(MODEL);
    PA_ADD_OPTION(MAX_ACTIONS);
    PA_ADD_OPTION(MAX_COST);
    PA_ADD_OPTION(PICTURE);
    PA_ADD_OPTION(ALLOW_SAVE);
    PA_ADD_OPTION(ALLOW_HOME);
    PA_ADD_OPTION(OVERLAY_NOTES);
    PA_ADD_OPTION(NOTIFICATIONS);
}


void GameProgram::program(SingleSwitchProgramEnvironment& env, CancellableScope& scope){
    GameProgram_Descriptor::Stats& stats = env.current_stats<GameProgram_Descriptor::Stats>();
    const GameProfile& profile = game_profile(m_game);

    env.log(
        PROGRAM_NAME + " build " + PROGRAM_BUILD + " (Claude Plays: " +
        (m_game == GameId::OTHER_GAMES ? std::string(OTHER_GAMES_LIST) : profile.name) + ", experimental)",
        COLOR_BLUE
    );

    const std::string goal = trimmed(GOAL);
    if (goal.empty()){
        throw UserSetupError(env.console, "Write down a goal for Claude first.");
    }
    const std::string unavailable = PlaySession::why_unavailable(scope);
    if (!unavailable.empty()){
        throw UserSetupError(env.console, unavailable);
    }

    AnyGameOptions options;
    switch ((Picture)PICTURE){
    case Picture::SMALL:
        options.picture_width = 384;
        options.picture_height = 216;
        break;
    case Picture::NORMAL:
        options.picture_width = 640;
        options.picture_height = 360;
        break;
    case Picture::LARGE:
        options.picture_width = 960;
        options.picture_height = 540;
        break;
    }
    options.allow_save = ALLOW_SAVE;
    options.allow_home = ALLOW_HOME;

    GameFacts facts;
    facts.name = profile.name;
    facts.notes = profile.notes;
    facts.user_notes = trimmed(ABOUT);

    Limits limits;
    limits.max_actions = (uint64_t)(uint32_t)MAX_ACTIONS;
    limits.max_cost = MAX_COST;
    const Model model = MODEL;

    //  The session, on the game's controller.
    auto session = [&](PlayConsole& console, CancellableScope& controller){
        ProgramHost host(env, controller, stats, OVERLAY_NOTES);
        AnyGame game(console, facts, options);
        PlaySession claude(make_setup(game, model));
        env.log(
            std::string("Claude Plays: ") + model_id(model) + ", at most " + std::to_string(limits.max_actions) +
            " actions" + (limits.max_cost > 0 ? ", at most about " + money(limits.max_cost) : std::string(", no cost limit")) +
            ". Goal: " + one_line(goal, 300),
            COLOR_BLUE
        );
        host.show("Claude plays: " + goal, COLOR_GREEN);

        const Ending ending = play(host, game, claude, goal, limits);
        if (ending.failed){
            throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, ending.summary);
        }
        env.log("Claude Plays: " + ending.summary, ending.reached ? COLOR_BLUE : COLOR_ORANGE);
        host.show(ending.summary, ending.reached ? COLOR_GREEN : COLOR_ORANGE);
        send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH, ending.summary);
    };

    if (profile.pad == PadKind::RIGHT_JOYCON){
        JoyconContext context(scope, env.console.controller<RightJoycon>());
        if (!context->is_ready()){
            throw UserSetupError(context->logger(), "Controller is not ready.");
        }
        JoyconConsole console(env, context);
        session(console, context);
    }else{
        ProControllerContext context(scope, env.console.controller<ProController>());
        if (!context->is_ready()){
            throw UserSetupError(context->logger(), "Controller is not ready.");
        }
        ProConsole console(env, context, m_game);
        session(console, context);
    }
}



namespace{

//  The picture of the list "Other Games": a controller, in the size and
//  shape of the pictures of the other lists. On a ground of its own, in the
//  color of the list's cartridge (PocketPilot_Home.cpp): in the list of
//  games of the light themes a white controller alone is not seen.
ImageRGB32 other_games_picture(){
    QImage image(256, 128, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);

        painter.setBrush(QColor(0x5a, 0x55, 0xb8));
        painter.drawRoundedRect(QRectF(0, 0, 256, 128), 16, 16);

        //  The body: a bar with two grips.
        const QColor body(0xf2, 0xf3, 0xf7);
        painter.setBrush(body);
        painter.drawRoundedRect(QRectF(48, 34, 160, 58), 26, 26);
        painter.drawEllipse(QPointF(70, 78), 30, 28);
        painter.drawEllipse(QPointF(186, 78), 30, 28);

        //  The d-pad and the left stick.
        const QColor dark(0x2b, 0x30, 0x40);
        painter.setBrush(dark);
        painter.drawRoundedRect(QRectF(68, 50, 9, 27), 2, 2);
        painter.drawRoundedRect(QRectF(59, 59, 27, 9), 2, 2);
        painter.drawEllipse(QPointF(104, 80), 9, 9);
        //  The right stick.
        painter.drawEllipse(QPointF(152, 80), 9, 9);

        //  The four buttons.
        const QPointF centers[4] = {
            QPointF(183, 49), QPointF(197, 63), QPointF(169, 63), QPointF(183, 77),
        };
        for (const QPointF& center : centers){
            painter.drawEllipse(center, 6.5, 6.5);
        }
    }
    return QImage_to_ImageRGB32(std::move(image));
}

}


namespace{

//  The ground of a game's card.
QRgb card_ground(GameId game){
    switch (game){
    case GameId::OCTOPATH_TRAVELER_0:
        return qRgb(0x7a, 0x5c, 0x2e);
    case GameId::MARIO_KART_8_DELUXE:
        return qRgb(0xb3, 0x34, 0x2f);
    case GameId::SPLATOON_3:
        return qRgb(0x9a, 0x2f, 0x8c);
    case GameId::ANIMAL_CROSSING_NEW_HORIZONS:
        return qRgb(0x3a, 0x86, 0x58);
    default:
        return qRgb(0x44, 0x4a, 0x5c);
    }
}

}

GameOnlyList::GameOnlyList(GameId game)
    : PanelListDescriptor(game_profile(game).title)
    , m_game(game)
{
    m_icon = QImage_to_ImageRGB32(make_game_card(game_profile(game).title, card_ground(game)));
}

std::vector<PanelEntry> GameOnlyList::make_panels() const{
    std::vector<PanelEntry> ret;
    ret.emplace_back("---- Claude ----");
    ret.emplace_back(make_claude_plays(m_game));
    return ret;
}


OtherGamesList::OtherGamesList()
    : PanelListDescriptor(OTHER_GAMES_LIST)
{
    m_icon = other_games_picture();
}

std::vector<PanelEntry> OtherGamesList::make_panels() const{
    std::vector<PanelEntry> ret;
    ret.emplace_back("---- Claude ----");
    ret.emplace_back(make_claude_plays(GameId::OTHER_GAMES));
    return ret;
}



}
}
}

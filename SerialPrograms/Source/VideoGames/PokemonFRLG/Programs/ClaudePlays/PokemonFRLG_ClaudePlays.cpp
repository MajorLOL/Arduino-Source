/*  Pokemon FRLG Claude Plays
 *
 *  PocketPilot's own program (experimental: not tested on a console yet,
 *  and not against the real Claude API).
 *
 *  The user writes down a goal. Claude gets it with a list of tools, calls
 *  them one at a time, and this program runs each call on the console:
 *
 *  -   "look" sends a picture of the game. Nothing else does unless asked
 *      to: every tool returns what the program reads from the screen by
 *      itself (a text box and its text, a YES/NO question, a battle, the
 *      start menu), which costs a few tokens instead of a picture's.
 *
 *  -   "press" presses buttons. "walk", "battle", "advance_text",
 *      "close_menus", "heal_at_nurse" and "save_game" are routines of the
 *      AutoStory (PokemonFRLG_AutoStoryTools.h): a whole battle or scene
 *      for one call, without a token in between.
 *
 *  The conversation is held by PocketPilot's assistant
 *  (PocketPilot/Assistant/Assistant_Play.h) with the key of the panel
 *  "Claude". When it gets long it is started over with Claude's own notes
 *  and the last actions, so that a long session does not send its whole
 *  past with every request.
 *
 *  A routine of the AutoStory that does not understand the screen (it
 *  throws an OperationFailedException) does not stop the program: Claude
 *  is told, and decides what to do.
 *
 */

#include <algorithm>
#include <cctype>
#include <deque>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PocketPilot/Assistant/Assistant_Play.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_StartMenuDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryBattle.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryMemory.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTools.h"
#include "PokemonFRLG_ClaudePlays.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;

using PocketPilot::Assistant::PlayCall;
using PocketPilot::Assistant::PlayEvent;
using PocketPilot::Assistant::PlayResult;
using PocketPilot::Assistant::PlaySession;
using PocketPilot::Assistant::PlayTool;
using PocketPilot::Assistant::PlayUsage;


ClaudePlays_Descriptor::ClaudePlays_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:ClaudePlays",
        Pokemon::STRING_POKEMON + " FRLG", "Claude Plays",
        "",
        "Claude plays toward a goal you write down, on your own Claude API key: it looks at the screen and "
        "presses the buttons, and leaves battles, text and walking to the routines of the AutoStory.",
        PanelQualityStanding::EXPERIMENTAL,
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        {
            //  Multi-line OCR (the text of the game).
            "PaddleOCRDetection",
        }
    )
{}

struct ClaudePlays_Descriptor::Stats : public StatsTracker{
    Stats()
        : actions(m_stats["Actions"])
        , battles(m_stats["Battles"])
        , requests(m_stats["Requests to Claude"])
        , tokens(m_stats["Tokens"])
    {
        m_display_order.emplace_back("Actions");
        m_display_order.emplace_back("Battles", HIDDEN_IF_ZERO);
        m_display_order.emplace_back("Requests to Claude");
        m_display_order.emplace_back("Tokens");
    }

    std::atomic<uint64_t>& actions;
    std::atomic<uint64_t>& battles;
    std::atomic<uint64_t>& requests;
    std::atomic<uint64_t>& tokens;
};

std::unique_ptr<StatsTracker> ClaudePlays_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}


namespace{

//  The game draws 240 x 160 pixels, which the console enlarges.
const int GAME_WIDTH = 240;
const int GAME_HEIGHT = 160;

//  The text of the battle text box, as the AutoStory's battles read it.
const ImageFloatBox BATTLE_TEXT_BOX(0.045, 0.770, 0.890, 0.170);
//  Where the screen is looked at for "black", as the game's other
//  programs do.
const ImageFloatBox BLACK_SCREEN_BOX(0.231692, 0.0616538, 0.551385, 0.9045);

//  A conversation whose last request sent this many tokens is started
//  over, with the notes and the last actions. (Every request sends the
//  whole conversation again. From the cache that costs little, but past
//  100,000 tokens the smallest model costs five times as much.)
const uint64_t RESTART_AT_TOKENS = 40000;
//  The last actions that go along into a conversation that starts over.
const size_t JOURNAL_LINES = 6;
//  How much of Claude's notes is kept (the newest).
const size_t NOTES_CHARACTERS = 2500;
//  Claude ending its turn without a tool call this often in a row ends
//  the session.
const int MAX_NUDGES = 3;


const char* model_id(ClaudePlays::Model model){
    switch (model){
    case ClaudePlays::Model::HAIKU:     return "claude-haiku-5-5";
    case ClaudePlays::Model::SONNET:    return "claude-sonnet-5-5";
    case ClaudePlays::Model::OPUS:      return "claude-opus-5-5";
    case ClaudePlays::Model::FABLE:     return "claude-fable-5-2";
    }
    return "claude-haiku-5-5";
}

std::string trimmed(const std::string& text){
    size_t first = 0;
    size_t last = text.size();
    while (first < last && std::isspace((unsigned char)text[first])){
        first++;
    }
    while (last > first && std::isspace((unsigned char)text[last - 1])){
        last--;
    }
    return text.substr(first, last - first);
}

//  On one line, and not longer than "limit".
std::string one_line(const std::string& text, size_t limit){
    std::string line;
    bool space = false;
    for (char ch : trimmed(text)){
        if (std::isspace((unsigned char)ch)){
            space = true;
            continue;
        }
        if (space && !line.empty()){
            line += ' ';
        }
        space = false;
        line += ch;
    }
    if (line.size() > limit){
        line.resize(limit);
        line += "...";
    }
    return line;
}

std::string money(double usd){
    const long long cents = (long long)(usd * 100.0 + 0.5);
    const std::string rest = std::to_string(cents % 100);
    return "$" + std::to_string(cents / 100) + "." + (rest.size() < 2 ? "0" + rest : rest);
}

std::string usage_line(const PlayUsage& usage){
    std::string line = std::to_string(usage.tokens) + " tokens in " + std::to_string(usage.requests) +
        (usage.requests == 1 ? " request" : " requests");
    if (usage.cost_known && usage.requests > 0){
        line += usage.cost_usd < 0.005 ? ", less than $0.01" : ", about " + money(usage.cost_usd);
    }
    return line;
}


//  What the program finds on the screen by itself.
struct ScreenRead{
    std::string text;
    //  A text box, a menu or a battle is in the way of walking.
    bool busy = false;
    //  What a text box or a question says (lower case, as it is read).
    std::string said;

    //  The game asks whether to save ("Would you like to save the game?",
    //  "There is already a saved file. Is it okay to overwrite it?").
    bool asks_to_save() const{
        return said.find("save") != std::string::npos;
    }
};

ScreenRead read_screen(StoryRuntime& rt){
    rt.context.wait_for_all_requests();
    ScreenRead read;
    VideoSnapshot snapshot = rt.console.video().snapshot();
    if (!snapshot){
        read.text = "No video comes in from the capture card.";
        return read;
    }
    const ImageViewRGB32 frame = snapshot;
    BattleMenuDetector battle_menu(COLOR_RED);
    BattleDialogDetector battle_box(COLOR_RED);
    SelectionDialogDetector question(COLOR_RED);
    WhiteDialogDetector text_box(COLOR_RED);
    AdvanceWhiteDialogDetector more_text(COLOR_RED);
    StartMenuDetector start_menu(COLOR_RED);
    BlackScreenDetector black(COLOR_RED, GameSettings::instance().GAME_BOX.inner_to_outer(BLACK_SCREEN_BOX));
    if (battle_menu.detect(frame)){
        read.busy = true;
        read.text = "A battle: its menu (FIGHT, BAG, POKeMON, RUN) is open.";
    }else if (battle_box.detect(frame)){
        read.busy = true;
        read.text = "A battle: its text box says \"" +
            one_line(read_story_text(rt.console, BATTLE_TEXT_BOX, true), 200) + "\".";
    }else if (question.detect(frame)){
        read.busy = true;
        read.said = story_read_dialog(rt);
        read.text = "A YES/NO question: \"" + one_line(read.said, 200) + "\".";
    }else if (text_box.detect(frame)){
        read.busy = true;
        read.said = story_read_dialog(rt);
        read.text = "A text box: \"" + one_line(read.said, 200) + "\"" +
            (more_text.detect(frame) ? " (more text follows)." : ".");
    }else if (start_menu.detect(frame)){
        read.busy = true;
        read.text = "The start menu is open.";
    }else if (black.detect(frame)){
        read.text = "The screen is black (a fade, or a scene loading).";
    }else{
        read.text = "No text box, menu or battle found: the overworld, or a screen the program does not know.";
    }
    return read;
}

//  The game's own picture (without the borders around it), as it goes to
//  Claude. Empty without video.
std::string take_picture(StoryRuntime& rt, int enlarge){
    rt.context.wait_for_all_requests();
    VideoSnapshot snapshot = rt.console.video().snapshot();
    if (!snapshot){
        return std::string();
    }
    const ImageViewRGB32 frame = snapshot;
    const ImageFloatBox game_box = GameSettings::instance().GAME_BOX;
    const ImageViewRGB32 game = extract_box_reference(frame, game_box);
    return PocketPilot::Assistant::play_picture(game, GAME_WIDTH, GAME_HEIGHT, enlarge);
}


const char* const LOOK_PARAMETER =
    R"("look":{"type":"boolean","description":"true: a picture of the screen comes back with the result."})";

}   //  namespace


std::vector<PlayTool> claude_plays_tools(bool allow_save){
    std::vector<PlayTool> tools;
    tools.push_back(PlayTool{
        "look",
        "A picture of the game's screen as it is now, with what the program itself reads from it. A picture "
        "costs far more tokens than the text every tool returns: use it when that text is not enough.",
        R"({"type":"object","properties":{}})"
    });
    tools.push_back(PlayTool{
        "press",
        "Presses buttons one after the other: A, B, START, SELECT, L, R, UP, DOWN, LEFT, RIGHT. For menus, "
        "choices, talking (A) and anything no other tool does. In the overworld a direction turns the player "
        "that way, or takes one step if the player faces that way already: use walk for more.",
        std::string(R"({"type":"object","properties":{"buttons":{"type":"array","items":{"type":"string",)") +
        R"("enum":["A","B","START","SELECT","L","R","UP","DOWN","LEFT","RIGHT"]},)" +
        R"("description":"1 to 20 buttons, pressed in this order."},)" +
        R"("hold_ms":{"type":"integer","description":"How long each is held: 60 to 3000. Leave out for a normal press."},)" +
        LOOK_PARAMETER + R"(},"required":["buttons"]})"
    });
    tools.push_back(PlayTool{
        "walk",
        "Walks the player in the overworld, up to 20 steps one way, each step checked on the video. Stops "
        "early when something is in the way. A wild or trainer battle that starts on the way is fought to its "
        "end by the program, and the result says how it went.",
        std::string(R"({"type":"object","properties":{"direction":{"type":"string",)") +
        R"("enum":["up","down","left","right"]},"steps":{"type":"integer","description":"1 to 20."},)" +
        LOOK_PARAMETER + R"(},"required":["direction","steps"]})"
    });
    tools.push_back(PlayTool{
        "battle",
        "Fights the battle that is on the screen, or that starts right now, to its end without a token: each "
        "turn the move that does the most damage, new moves learned when they are better, the next Pokemon "
        "sent in when one faints. Returns how it ended.",
        std::string(R"({"type":"object","properties":{"kind":{"type":"string","enum":["wild","trainer"]},)") +
        R"("run":{"type":"boolean","description":"true: run from a wild battle instead of fighting it."},)" +
        LOOK_PARAMETER + R"(},"required":["kind"]})"
    });
    tools.push_back(PlayTool{
        "advance_text",
        "Gets through text and scenes until the player can move again, pressing B for each box. A YES/NO "
        "question on the way gets \"answer\" (a nickname is always declined). Returns what the first box "
        "said. If a battle starts instead, it says so: call battle then.",
        std::string(R"({"type":"object","properties":{"answer":{"type":"string","enum":["yes","no"],)") +
        R"("description":"For YES/NO questions. \"no\" if left out."},)" +
        R"("max_seconds":{"type":"integer","description":"Give up after this long: 5 to 300, 60 if left out."},)" +
        LOOK_PARAMETER + "}}"
    });
    tools.push_back(PlayTool{
        "close_menus",
        "Closes menus and text boxes with B until the player stands free in the overworld.",
        std::string(R"({"type":"object","properties":{)") + LOOK_PARAMETER + "}}"
    });
    tools.push_back(PlayTool{
        "heal_at_nurse",
        "In a Pokemon Center, with the player anywhere straight below the nurse (as right after coming in "
        "through the door): walks up to her counter, has the party healed and gets through her text.",
        std::string(R"({"type":"object","properties":{)") + LOOK_PARAMETER + "}}"
    });
    if (allow_save){
        tools.push_back(PlayTool{
            "save_game",
            "Saves the game from the overworld through the start menu. Only when the goal asks for it, or "
            "before something that could go wrong.",
            std::string(R"({"type":"object","properties":{"has_pokedex":{"type":"boolean",)") +
            R"("description":"false if the start menu has no POKeDEX entry yet. true if left out."},)" +
            LOOK_PARAMETER + "}}"
        });
    }
    tools.push_back(PlayTool{
        "wait",
        "Waits without pressing anything: an animation, a scene that plays by itself.",
        std::string(R"({"type":"object","properties":{"ms":{"type":"integer","description":"100 to 10000."},)") +
        LOOK_PARAMETER + R"(},"required":["ms"]})"
    });
    tools.push_back(PlayTool{
        "note",
        "Writes a line down for yourself. The program starts the conversation over now and then to save "
        "tokens, and only your notes and the last few actions come along: note where the player is, what is "
        "done and what comes next whenever that changes.",
        R"({"type":"object","properties":{"text":{"type":"string"}},"required":["text"]})"
    });
    tools.push_back(PlayTool{
        "done",
        "Ends the session: the goal is reached, or it cannot be reached. Say which, and what was done.",
        std::string(R"({"type":"object","properties":{"reached":{"type":"boolean"},)") +
        R"("summary":{"type":"string"}},"required":["reached","summary"]})"
    });
    return tools;
}

std::string claude_plays_instructions(bool allow_save){
    std::string text =
        "You are Claude, playing Pokemon FireRed or LeafGreen (in English) on a real Nintendo Switch through "
        "the program " + PROGRAM_NAME + ". The program sees the game through a capture card and presses the "
        "buttons you ask for. The user who started it gave you a goal and reads along in the program's log.\n"
        "\n"
        "How to play\n"
        "- You act only through the tools. Each returns, after \"Screen:\", what the program itself reads from "
        "the screen afterwards: a text box and what it says, a YES/NO question, a battle, the start menu. That "
        "costs almost nothing. A picture costs many times more: ask for one (look, or \"look\": true with a "
        "tool) when the text is not enough. In the overworld only a picture shows where the player stands and "
        "what is around, and the program's reading can be wrong: when a result surprises you, look.\n"
        "- A confirms and talks to what the player faces, B cancels and moves text on, START opens the menu. "
        "The overworld is a grid of tiles with the player in the middle of the picture.\n"
        "- Let the program do by itself what it can, without a token: battle fights a whole battle, "
        "advance_text gets through text and scenes, walk takes several steps and fights what comes on the way, "
        "close_menus gets back to the overworld. Use press for everything else, several buttons in one call "
        "when you are sure of them.\n"
        "- The program starts the conversation over now and then to save tokens. Only your notes and the last "
        "few actions come along, so keep the notes up to date.\n"
        "\n"
        "Rules\n"
        "- Work toward the goal in steps you can check. Before each tool call, say in one short sentence what "
        "you are about to do and why.\n";
    text += allow_save
        ? "- Save the game only with save_game, and only when the goal asks for it or before something that "
          "could go wrong.\n"
        : "- Do not save the game: the user has not allowed it in this session.\n";
    text +=
        "- Never start a new game, delete the save file or release a Pokemon. Do not sell or throw away an "
        "item, or teach a move over another, unless the goal says so.\n"
        "- When the goal is reached, or you see that it cannot be reached from here, call done.\n";
    return text;
}


namespace{


//  What a session keeps while it runs.
struct Session{
    Session(SingleSwitchProgramEnvironment& p_env, ProControllerContext& p_context, StoryRuntime& p_rt)
        : env(p_env)
        , context(p_context)
        , rt(p_rt)
    {}

    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
    StoryRuntime& rt;

    int enlarge = 2;
    bool allow_save = false;
    bool overlay = true;

    //  Claude's notes (one a line), and the last actions with what came of
    //  them: what a conversation that starts over begins with.
    std::string notes;
    std::deque<std::string> journal;
    uint64_t actions = 0;

    void show(const std::string& text, Color color = COLOR_WHITE) const{
        if (overlay){
            env.console.overlay().add_log(one_line(text, 110), color);
        }
    }
};

const char* const NO_SAVING =
    "The text on the screen asks about saving the game, and the user has not allowed saving in this session. "
    "Nothing was pressed. B says no (and moves a text on).";

//  A tool's own part of its result: what it did. An error if it could not
//  be done as asked.
struct Outcome{
    std::string text;
    bool is_error = false;

    static Outcome error(std::string why){
        Outcome outcome;
        outcome.text = std::move(why);
        outcome.is_error = true;
        return outcome;
    }
};

int64_t integer_between(const JsonObject& input, const char* name, int64_t fallback, int64_t low, int64_t high){
    return std::clamp(input.get_integer_default(name, fallback), low, high);
}

Outcome press_buttons(Session& session, const JsonObject& input){
    const JsonArray* buttons = input.get_array("buttons");
    if (buttons == nullptr || buttons->empty()){
        return Outcome::error("\"buttons\" has to be a list with at least one button.");
    }
    if (buttons->size() > 20){
        return Outcome::error("At most 20 buttons in one call.");
    }
    struct Press{
        bool dpad = false;
        Button button = BUTTON_A;
        DpadPosition direction = DPAD_NONE;
    };
    std::vector<Press> presses;
    std::string names;
    bool presses_a = false;
    for (size_t index = 0; index < buttons->size(); index++){
        const std::string* given = (*buttons)[index].to_string();
        std::string name;
        if (given != nullptr){
            for (char ch : trimmed(*given)){
                name += (char)std::toupper((unsigned char)ch);
            }
        }
        Press press;
        if (name == "A"){
            press.button = BUTTON_A;
            presses_a = true;
        }else if (name == "B"){
            press.button = BUTTON_B;
        }else if (name == "START"){
            press.button = BUTTON_PLUS;
        }else if (name == "SELECT"){
            press.button = BUTTON_MINUS;
        }else if (name == "L"){
            press.button = BUTTON_L;
        }else if (name == "R"){
            press.button = BUTTON_R;
        }else if (name == "UP"){
            press.dpad = true;
            press.direction = DPAD_UP;
        }else if (name == "DOWN"){
            press.dpad = true;
            press.direction = DPAD_DOWN;
        }else if (name == "LEFT"){
            press.dpad = true;
            press.direction = DPAD_LEFT;
        }else if (name == "RIGHT"){
            press.dpad = true;
            press.direction = DPAD_RIGHT;
        }else{
            return Outcome::error(
                "There is no button \"" + one_line(name, 20) +
                "\". The buttons are A, B, START, SELECT, L, R, UP, DOWN, LEFT, RIGHT. Nothing was pressed."
            );
        }
        presses.push_back(press);
        names += (names.empty() ? "" : ", ") + name;
    }
    //  Not a YES to saving, unless the user allowed it. (Claude is told not
    //  to save; this is for the press it makes all the same.)
    if (presses_a && !session.allow_save && read_screen(session.rt).asks_to_save()){
        return Outcome::error(NO_SAVING);
    }
    //  A direction held for less than 8 frames only turns a player who
    //  faces another way; held longer, a step follows the turn.
    const bool hold_given = input.get_value("hold_ms") != nullptr;
    const Milliseconds hold_asked(integer_between(input, "hold_ms", 160, 60, 3000));
    for (const Press& press : presses){
        if (press.dpad){
            pbf_press_dpad(session.context, press.direction, hold_given ? hold_asked : Milliseconds(100), 300ms);
            //  In a menu it moved the cursor, in the overworld it turned
            //  the player or took a step: which way the player faces is
            //  for the next walk to find out.
            session.rt.facing = DPAD_NONE;
        }else{
            pbf_press_button(session.context, press.button, hold_given ? hold_asked : Milliseconds(160), 340ms);
        }
    }
    pbf_wait(session.context, 300ms);
    session.context.wait_for_all_requests();
    Outcome outcome;
    outcome.text = "Pressed " + names + ".";
    return outcome;
}

const char* battle_text(StoryBattleResult result){
    switch (result){
    case StoryBattleResult::WON:            return "The battle is won.";
    case StoryBattleResult::RAN:            return "Ran from the battle.";
    case StoryBattleResult::CAUGHT:         return "The wild Pokemon was caught.";
    case StoryBattleResult::LEAD_FAINTED:   return "The first Pokemon of the party fainted in the battle. The battle is over.";
    case StoryBattleResult::NO_BATTLE:      return "No battle showed up.";
    case StoryBattleResult::LOST:
        return "The whole party fainted: the player whited out and is back at the last Pokemon Center (or at home).";
    case StoryBattleResult::OUT_OF_BALLS:   return "No ball is left in the bag. The battle is still on.";
    }
    return "";
}

//  What a battle left behind, for Claude.
std::string after_battle(StoryRuntime& rt){
    std::string text;
    if (rt.memory.player_hp >= 0){
        text += " The one that fought last has about " + std::to_string((int)(rt.memory.player_hp * 100.0 + 0.5)) +
            "% of its HP.";
    }
    if (rt.memory.level_up_seen){
        rt.memory.level_up_seen = false;
        text += " A Pokemon of the party grew a level.";
    }
    return text;
}

Outcome walk(Session& session, const JsonObject& input){
    const std::string name = input.get_string_default("direction");
    DpadPosition direction;
    if (name == "up"){
        direction = DPAD_UP;
    }else if (name == "down"){
        direction = DPAD_DOWN;
    }else if (name == "left"){
        direction = DPAD_LEFT;
    }else if (name == "right"){
        direction = DPAD_RIGHT;
    }else{
        return Outcome::error("\"direction\" has to be up, down, left or right.");
    }
    const int steps = (int)integer_between(input, "steps", 1, 1, 20);
    //  The walking routine takes a text box for a trainer who talks.
    const ScreenRead before = read_screen(session.rt);
    if (before.busy){
        return Outcome::error("The player cannot walk now. Deal with what is on the screen first.");
    }
    Outcome outcome;
    int made = 0;
    for (; made < steps; made++){
        bool moved = true;
        const StoryBattleResult result = story_checked_step(
            session.rt, direction, false, false, StoryBattleKind::WILD,
            "Claude Plays", false, &moved
        );
        if (!moved){
            outcome.text = "Walked " + std::to_string(made) + " of " + std::to_string(steps) + " steps " + name +
                ": the next step did not happen. Something is in the way, or the picture does not move here.";
            return outcome;
        }
        if (result != StoryBattleResult::NO_BATTLE){
            outcome.text = "Walked " + std::to_string(made + 1) + " of " + std::to_string(steps) + " steps " +
                name + ", then a battle came. " + battle_text(result) + after_battle(session.rt);
            return outcome;
        }
    }
    outcome.text = "Walked " + std::to_string(made) + (made == 1 ? " step " : " steps ") + name + ".";
    return outcome;
}

Outcome battle(Session& session, const JsonObject& input){
    const std::string kind_name = input.get_string_default("kind");
    if (kind_name != "wild" && kind_name != "trainer"){
        return Outcome::error("\"kind\" has to be wild or trainer.");
    }
    const bool run = input.get_boolean_default("run", false);
    if (run && kind_name == "trainer"){
        return Outcome::error("There is no running from a trainer battle. Nothing was done.");
    }
    const StoryBattleKind kind =
        kind_name == "trainer" ? StoryBattleKind::TRAINER :
        run ? StoryBattleKind::WILD_RUN : StoryBattleKind::WILD_FIGHT;
    const StoryBattleResult result = story_battle(session.rt, kind, "Claude Plays", false);
    Outcome outcome;
    outcome.text = battle_text(result);
    if (result != StoryBattleResult::NO_BATTLE){
        outcome.text += after_battle(session.rt);
    }
    return outcome;
}

Outcome advance_text(Session& session, const JsonObject& input){
    const std::string answer = input.get_string_default("answer", "no");
    if (answer != "yes" && answer != "no"){
        return Outcome::error("\"answer\" has to be yes or no.");
    }
    const Milliseconds limit(integer_between(input, "max_seconds", 60, 5, 300) * 1000);
    const ScreenRead before = read_screen(session.rt);
    if (answer == "yes" && !session.allow_save && before.asks_to_save()){
        return Outcome::error(NO_SAVING);
    }
    const bool battle_started = story_cutscene(session.rt, 0ms, limit, answer == "yes", "Claude Plays");
    Outcome outcome;
    outcome.text = "Before: " + before.text + " ";
    outcome.text += battle_started
        ? "Then a battle came up: call battle."
        : "The player can move again.";
    return outcome;
}

Outcome run_tool(Session& session, const PlayCall& call, const JsonObject& input){
    Outcome outcome;
    if (call.name == "look"){
        return outcome;
    }
    if (call.name == "press"){
        return press_buttons(session, input);
    }
    if (call.name == "walk"){
        return walk(session, input);
    }
    if (call.name == "battle"){
        return battle(session, input);
    }
    if (call.name == "advance_text"){
        return advance_text(session, input);
    }
    if (call.name == "close_menus"){
        story_back_to_overworld(session.rt, "Claude Plays");
        outcome.text = "The player stands free in the overworld.";
        return outcome;
    }
    if (call.name == "heal_at_nurse"){
        //  The routine walks up to the counter, and ends in front of it.
        story_heal(session.rt, "Claude Plays", true);
        outcome.text = "The nurse healed the party.";
        return outcome;
    }
    if (call.name == "save_game" && session.allow_save){
        story_save(session.rt, input.get_boolean_default("has_pokedex", true));
        story_saved(session.rt, 0);
        outcome.text = "The game is saved.";
        return outcome;
    }
    if (call.name == "wait"){
        pbf_wait(session.context, Milliseconds(integer_between(input, "ms", 1000, 100, 10000)));
        session.context.wait_for_all_requests();
        outcome.text = "Waited.";
        return outcome;
    }
    return Outcome::error("There is no tool \"" + one_line(call.name, 40) + "\" in this session.");
}

//  One call of Claude, run on the console, with what the screen shows
//  afterwards.
PlayResult run_call(Session& session, const PlayCall& call){
    const JsonValue json = parse_json(call.input);
    const JsonObject no_input;
    const JsonObject& input = json.to_object() != nullptr ? *json.to_object() : no_input;

    Outcome outcome;
    try{
        outcome = run_tool(session, call, input);
    }catch (OperationFailedException& e){
        //  The routine did not understand the screen: for Claude to sort
        //  out, with a look at it.
        session.env.log("Claude Plays: the routine of \"" + call.name + "\" gave up: " + e.message(), COLOR_ORANGE);
        outcome = Outcome::error(
            "The program's routine gave up: " + one_line(e.message(), 300) +
            " The game is as that left it: look before you go on."
        );
        //  Which way the player faces is not known any more.
        session.rt.facing = DPAD_NONE;
    }

    PlayResult result;
    result.is_error = outcome.is_error;
    result.text = outcome.text;
    const ScreenRead screen = read_screen(session.rt);
    result.text += (result.text.empty() ? "" : " ") + std::string("Screen: ") + screen.text;
    if (call.name == "look" || input.get_boolean_default("look", false)){
        result.image_png = take_picture(session.rt, session.enlarge);
        if (result.image_png.empty()){
            result.text += " (No picture could be taken.)";
        }
    }
    return result;
}

//  A few words on a call, for the log and the journal.
std::string call_words(const PlayCall& call){
    return call.name + " " + one_line(call.input, 120);
}

//  What a conversation begins with.
std::string first_message(const Session& session, const std::string& goal, const std::string& screen){
    std::string text = "Goal: " + goal + "\n\n";
    if (session.actions > 0){
        text += "The program started this conversation over to save tokens, after " +
            std::to_string(session.actions) + " actions.\n";
        text += session.notes.empty()
            ? "You wrote no notes down.\n"
            : "Your notes:\n" + session.notes;
        if (!session.journal.empty()){
            text += "The last actions before that, with what came of them:\n";
            for (const std::string& line : session.journal){
                text += "- " + line + "\n";
            }
        }
        text += "\n";
    }
    text += "Screen now: " + screen;
    return text;
}

}   //  namespace



ClaudePlays::ClaudePlays()
    : SETUP_TEXT(
        "<b>Experimental:</b> this program is new. It has not been tested on a console, and not with a real "
        "API key either: the build machine has none. Watch it, and <b>save the game by hand before you start "
        "it</b>.<br>"
        "<b>What it does:</b> Claude, the AI model of Anthropic, plays toward the goal below. It looks at the "
        "screen, presses the buttons, and leaves whole battles, text and walking to the routines of the "
        "AutoStory. It is slow next to a program made for one task (every decision is a request to Claude), "
        "and it can get lost: it is for things no program exists for.<br>"
        "<b>What it needs:</b> your Claude API key in the panel \"Claude\" (top bar). Each request is billed "
        "to you by Anthropic; the pictures of the game and the text read from it are sent to Anthropic. What "
        "a session used is in the log and under the video.<br>"
        "<b>Setup:</b> as for the AutoStory. In the game options: Text Speed: Fast. Battle Scene: Off. Battle "
        "Style: Set. Button Mode: not L=A. The game in English."
    )
    , GOAL(
        "<b>Goal:</b><br>What Claude should do, in your own words (any language). Say where the player is "
        "if that helps, and when it is done.",
        LockMode::LOCK_WHILE_RUNNING,
        "Look at the screen and say where the player is and what could be done from here. Do not press "
        "anything.",
        "For example: Walk to the Pokemon Center of this town and heal the party."
    )
    , MODEL(
        "<b>Model:</b><br>The smallest model is the fastest and by far the cheapest, and enough for simple "
        "goals. A larger one finds its way better and costs many times more.",
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
        "<b>Stop after actions:</b><br>An action is one thing Claude has the program do: a look, some button "
        "presses, a walk, a whole battle.",
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
        "<b>Pictures for Claude:</b><br>Larger pictures are easier to read and cost more tokens each. The game "
        "itself draws 240 x 160 pixels.",
        {
            {Picture::SMALL,    "small",    "240 x 160 (about 54 tokens a picture)"},
            {Picture::NORMAL,   "normal",   "480 x 320 (about 216 tokens)"},
            {Picture::LARGE,    "large",    "720 x 480 (about 468 tokens)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        Picture::NORMAL
    )
    , ALLOW_SAVE(
        "<b>Claude may save the game:</b><br>Off: Claude is told not to save, has no tool for it, and a press "
        "of A is not made while the game asks whether to save. Claude still presses the buttons, so the "
        "program cannot rule a save out altogether: keep a save of your own.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , OVERLAY_NOTES(
        "<b>Show what Claude does on the video:</b><br>What Claude says and each action, in the log on the "
        "video (turn on \"Log\" above the video). The program's own log has all of it either way.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_STATIC(SETUP_TEXT);
    PA_ADD_OPTION(GOAL);
    PA_ADD_OPTION(MODEL);
    PA_ADD_OPTION(MAX_ACTIONS);
    PA_ADD_OPTION(MAX_COST);
    PA_ADD_OPTION(PICTURE);
    PA_ADD_OPTION(ALLOW_SAVE);
    PA_ADD_OPTION(OVERLAY_NOTES);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}


void ClaudePlays::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    ClaudePlays_Descriptor::Stats& stats = env.current_stats<ClaudePlays_Descriptor::Stats>();

    env.log(PROGRAM_NAME + " build " + PROGRAM_BUILD + " (FRLG Claude Plays, experimental)", COLOR_BLUE);

    const std::string goal = trimmed(GOAL);
    if (goal.empty()){
        throw UserSetupError(env.console, "Write down a goal for Claude first.");
    }
    const std::string unavailable = PlaySession::why_unavailable(context);
    if (!unavailable.empty()){
        throw UserSetupError(env.console, unavailable);
    }

    const Model model = MODEL;
    const uint64_t max_actions = (uint64_t)(uint32_t)MAX_ACTIONS;
    const double max_cost = MAX_COST;

    StoryRuntime rt(env.console, context);
    rt.standalone = true;
    rt.segment_id = "Claude Plays";
    rt.has_dex = true;
    //  The steps are checked on the video: that is how a walk knows that
    //  something is in the way.
    rt.verify_walking = true;
    //  Claude saves with its own tool, if at all.
    rt.save_after_heal = false;
    rt.catch_shinies = false;
    //  No team whose party slots have to be kept free.
    rt.party_order_final = true;
    rt.overlay_notes = OVERLAY_NOTES;

    Session session(env, context, rt);
    session.allow_save = ALLOW_SAVE;
    session.overlay = OVERLAY_NOTES;
    switch ((Picture)PICTURE){
    case Picture::SMALL:    session.enlarge = 1; break;
    case Picture::NORMAL:   session.enlarge = 2; break;
    case Picture::LARGE:    session.enlarge = 3; break;
    }

    story_check_memory_at_start(rt);
    story_memory_reset();
    home_black_border_check(env.console, context);

    PlaySession::Setup setup;
    setup.model = model_id(model);
    setup.effort = "low";
    setup.system = claude_plays_instructions(session.allow_save);
    setup.tools = claude_plays_tools(session.allow_save);
    setup.max_tokens = 4000;
    PlaySession claude(std::move(setup));

    env.log(
        std::string("Claude Plays: ") + model_id(model) + ", at most " + std::to_string(max_actions) +
        " actions" + (max_cost > 0 ? ", at most about " + money(max_cost) : std::string(", no cost limit")) +
        ". Goal: " + one_line(goal, 300),
        COLOR_BLUE
    );
    session.show("Claude plays: " + goal, COLOR_GREEN);

    //  The numbers under the video.
    uint64_t requests_shown = 0;
    uint64_t tokens_shown = 0;
    uint64_t battles_shown = 0;
    bool warned_no_price = false;
    auto show_stats = [&](){
        const PlayUsage usage = claude.usage();
        stats.requests += usage.requests - requests_shown;
        stats.tokens += usage.tokens - tokens_shown;
        requests_shown = usage.requests;
        tokens_shown = usage.tokens;
        const uint64_t battles = rt.wild_battles + rt.trainer_battles;
        stats.battles += battles - battles_shown;
        battles_shown = battles;
        env.update_stats();
        if (!usage.cost_known && max_cost > 0 && !warned_no_price){
            warned_no_price = true;
            env.log(
                "Claude Plays: this build does not know the price of the model, so the cost limit cannot "
                "stop the session. The limit on actions still does.",
                COLOR_ORANGE
            );
        }
        return usage;
    };

    claude.start(first_message(session, goal, read_screen(rt).text));

    std::string summary;
    bool reached = false;
    int nudges = 0;
    //  Everything Claude did, the notes and the turns without a tool call
    //  included: they cost requests like an action does.
    uint64_t turns = 0;
    while (summary.empty()){
        const PlayEvent event = claude.wait(context);
        const PlayUsage usage = show_stats();
        if (!event.said.empty()){
            env.log("Claude: " + one_line(event.said, 600), COLOR_PURPLE);
            session.show("Claude: " + event.said);
        }
        turns++;
        if (max_cost > 0 && usage.cost_known && usage.cost_usd >= max_cost){
            summary = "Stopped at a cost of about " + money(usage.cost_usd) + ", the most this session may cost.";
            break;
        }
        if (turns > 2 * max_actions + 20){
            summary = "Stopped: Claude took far more turns than the actions this session may take.";
            break;
        }

        if (event.kind == PlayEvent::Kind::FAILED){
            throw OperationFailedException(
                ErrorReportMode::NO_ERROR_REPORT,
                "Claude could not be reached, after " + std::to_string(session.actions) + " actions (" +
                usage_line(usage) + "): " + event.message
            );
        }
        if (event.kind == PlayEvent::Kind::REFUSED){
            summary = "Claude declined to go on. " + event.message;
            break;
        }
        if (event.kind == PlayEvent::Kind::ANSWERED || event.kind == PlayEvent::Kind::CUT_OFF){
            //  A turn without a tool call: it has to call done to end.
            if (++nudges >= MAX_NUDGES){
                summary = "Claude stopped calling tools without saying that it is done.";
                break;
            }
            claude.say(
                "The program: go on with the goal through the tools. If it is reached, or cannot be reached, "
                "call done."
            );
            continue;
        }

        //  A call.
        nudges = 0;
        const PlayCall& call = event.call;
        if (call.name == "done"){
            const JsonValue json = parse_json(call.input);
            const JsonObject* input = json.to_object();
            reached = input != nullptr && input->get_boolean_default("reached", false);
            summary = std::string(reached ? "Claude says the goal is reached. " : "Claude says the goal is not reached. ") +
                (input != nullptr ? one_line(input->get_string_default("summary"), 600) : std::string());
            break;
        }
        if (call.name == "note"){
            const JsonValue json = parse_json(call.input);
            const JsonObject* input = json.to_object();
            const std::string note = input != nullptr ? one_line(input->get_string_default("text"), 400) : std::string();
            PlayResult result;
            if (note.empty()){
                result.is_error = true;
                result.text = "\"text\" was empty: nothing was written down.";
            }else{
                session.notes += "- " + note + "\n";
                //  The newest notes are the ones to keep.
                while (session.notes.size() > NOTES_CHARACTERS){
                    const size_t line_end = session.notes.find('\n');
                    session.notes.erase(0, line_end == std::string::npos ? session.notes.size() : line_end + 1);
                }
                env.log("Claude's note: " + note, COLOR_PURPLE);
                result.text = "Written down.";
            }
            claude.finish(call, std::move(result));
            continue;
        }

        env.log("Claude Plays: " + call_words(call));
        session.show(call_words(call), COLOR_CYAN);
        const PlayResult result = run_call(session, call);
        session.actions++;
        stats.actions++;
        show_stats();
        env.log(
            std::string("Claude Plays:   ") + (result.is_error ? "(not done) " : "") + one_line(result.text, 500) +
            (result.image_png.empty() ? "" : " [with a picture]"),
            result.is_error ? COLOR_ORANGE : Color()
        );
        session.journal.push_back(call_words(call) + " -> " + one_line(result.text, 240));
        while (session.journal.size() > JOURNAL_LINES){
            session.journal.pop_front();
        }

        if (session.actions >= max_actions){
            summary = "Stopped after " + std::to_string(session.actions) + " actions, the most this session may take.";
            break;
        }
        if (session.actions % 10 == 0){
            env.log("Claude Plays: " + std::to_string(session.actions) + " actions so far, " + usage_line(usage) + ".", COLOR_BLUE);
        }

        if (usage.conversation_tokens >= RESTART_AT_TOKENS){
            //  The call's result goes along in the journal instead of back
            //  into the long conversation.
            env.log(
                "Claude Plays: the conversation has grown to " + std::to_string(usage.conversation_tokens) +
                " tokens. Starting it over with Claude's notes and the last actions.",
                COLOR_BLUE
            );
            claude.start(first_message(session, goal, read_screen(rt).text));
            continue;
        }
        claude.finish(call, result);
    }

    const PlayUsage usage = show_stats();
    summary += " " + std::to_string(session.actions) + (session.actions == 1 ? " action, " : " actions, ") +
        usage_line(usage) + ".";
    env.log("Claude Plays: " + summary, reached ? COLOR_BLUE : COLOR_ORANGE);
    session.show(summary, reached ? COLOR_GREEN : COLOR_ORANGE);
    story_log_memory(rt, true);

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH, summary);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}


}
}
}

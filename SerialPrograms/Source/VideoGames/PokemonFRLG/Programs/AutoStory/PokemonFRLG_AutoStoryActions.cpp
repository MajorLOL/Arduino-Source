/*  Pokemon FRLG Autostory Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_BagDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_StatsReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_AutoStoryActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{


//  Message box of the party menu ("Teach which POKeMON?", "X learned Y!").
//  Window 6 of sSinglePartyMenuWindowTemplate: tiles (1,15) 28x4.
const ImageFloatBox PARTY_MESSAGE_BOX(0.040, 0.755, 0.920, 0.190);

//  Item description in the bag. Window 1 of sDefaultBagWindowsStd:
//  tiles (5,14) 25x6.
const ImageFloatBox BAG_DESCRIPTION_BOX(0.170, 0.705, 0.810, 0.280);


DpadPosition opposite(DpadPosition direction){
    switch (direction){
    case DPAD_UP:       return DPAD_DOWN;
    case DPAD_DOWN:     return DPAD_UP;
    case DPAD_LEFT:     return DPAD_RIGHT;
    case DPAD_RIGHT:    return DPAD_LEFT;
    default:            return DPAD_NONE;
    }
}

bool contains(const std::string& text, const char* word){
    return text.find(word) != std::string::npos;
}

std::string lower(const std::string& text){
    std::string ret;
    for (char c : text){
        ret += (char)std::tolower((unsigned char)c);
    }
    return ret;
}

void press(ProControllerContext& context, Button button, Milliseconds after = 840ms){
    pbf_press_button(context, button, 160ms, after);
}
void dpad(ProControllerContext& context, DpadPosition direction, Milliseconds after = 340ms){
    pbf_press_dpad(context, direction, 160ms, after);
}


//  Move index (AutoStoryDex::moves()) by its name, -1 if unknown.
int find_move(const std::string& name){
    std::string want = lower(name);
    const std::vector<AutoStoryDex::MoveInfo>& moves = AutoStoryDex::moves();
    for (size_t i = 0; i < moves.size(); i++){
        if (lower(moves[i].name) == want){
            return (int)i;
        }
    }
    return -1;
}

//  The move of the lead to replace with an HM: the weakest one that is not
//  an HM itself.
int forget_for_hm(const StoryRuntime& rt){
    int worst = -1;
    double worst_value = 1e9;
    for (int i = 0; i < 4; i++){
        int move = rt.memory.moves[i];
        if (move < 0){
            continue;
        }
        if (AutoStoryDex::moves()[(size_t)move].hm){
            continue;
        }
        double value = move_value(move, rt.battle);
        if (value < worst_value){
            worst_value = value;
            worst = i;
        }
    }
    return worst < 0 ? 0 : worst;
}


//  Text of the bag's item description, both text colors tried.
std::string read_bag_description(StoryRuntime& rt){
    std::string text = read_story_text(rt.console, BAG_DESCRIPTION_BOX, true);
    if (text.size() < 8){
        text += " " + read_story_text(rt.console, BAG_DESCRIPTION_BOX, false);
    }
    return text;
}

enum class BagPocket{
    ITEMS = 0,
    KEY_ITEMS = 1,
    POKE_BALLS = 2,
};

//  In the bag: go to a pocket, then down the list until the description of
//  the item under the cursor has one of the words (and none of "avoid").
bool select_bag_item(
    StoryRuntime& rt, BagPocket pocket,
    const std::vector<const char*>& words,
    const std::vector<const char*>& avoid,
    const std::string& what
){
    ProControllerContext& context = rt.context;
    //  The pockets do not wrap around: LEFT x3 is the first pocket.
    for (int i = 0; i < 3; i++){
        dpad(context, DPAD_LEFT, 500ms);
    }
    for (int i = 0; i < (int)pocket; i++){
        dpad(context, DPAD_RIGHT, 500ms);
    }
    //  The list does not wrap around either: to the top.
    for (int i = 0; i < 30; i++){
        pbf_press_dpad(context, DPAD_UP, 80ms, 80ms);
    }
    pbf_wait(context, 500ms);
    context.wait_for_all_requests();

    std::string last;
    int same = 0;
    for (int i = 0; i < 45; i++){
        std::string text = read_bag_description(rt);
        bool match = false;
        for (const char* word : words){
            match |= contains(text, word);
        }
        for (const char* word : avoid){
            match &= !contains(text, word);
        }
        if (match){
            rt.console.log("Found in the bag: " + what);
            return true;
        }
        if (text == last){
            same++;
            if (same >= 2){
                break;      //  The end of the list.
            }
        }else{
            same = 0;
        }
        last = text;
        dpad(context, DPAD_DOWN, 300ms);
        context.wait_for_all_requests();
    }
    rt.console.log("Not found in the bag: " + what, COLOR_RED);
    return false;
}

//  Read the party menu message, pressing A between pages, until one of the
//  words shows up. Returns the text that had it ("" if never).
std::string wait_party_message(
    StoryRuntime& rt, const std::vector<const char*>& words, int max_pages, bool press_between
){
    for (int i = 0; i < max_pages; i++){
        pbf_wait(rt.context, 1500ms);
        rt.context.wait_for_all_requests();
        std::string text = read_story_text(rt.console, PARTY_MESSAGE_BOX, false);
        rt.console.log("Message: " + text);
        for (const char* word : words){
            if (contains(text, word)){
                return text;
            }
        }
        if (press_between){
            press(rt.context, BUTTON_A, 200ms);
        }
    }
    return "";
}

//  In the bag, on an item from the ITEMS pocket: use it on a party slot.
void use_item_on(StoryRuntime& rt, int party_slot, const std::string& what){
    ProControllerContext& context = rt.context;
    rt.console.log("Using " + what + " on party slot " + std::to_string(party_slot) + ".");
    //  A: USE / GIVE / TOSS / CANCEL, cursor on USE. A: the party menu.
    press(context, BUTTON_A, 800ms);
    press(context, BUTTON_A, 2500ms);
    context.wait_for_all_requests();
    PartyMenuWatcher party(COLOR_RED);
    if (wait_until(rt.console, context, 3000ms, { party }) != 0){
        rt.console.log("The party menu did not open.", COLOR_RED);
        return;
    }
    for (int i = 1; i < party_slot; i++){
        dpad(context, DPAD_DOWN);
    }
    press(context, BUTTON_A, 3000ms);
    //  "X's HP was restored by N points." / "It won't have any effect."
    for (int i = 0; i < 4; i++){
        press(context, BUTTON_A, 1500ms);
        context.wait_for_all_requests();
        BagWatcher bag(COLOR_RED);
        PartyMenuWatcher party_again(COLOR_RED);
        int ret = wait_until(rt.console, context, 1500ms, { bag, party_again });
        if (ret == 0){
            return;
        }
        if (ret == 1){
            //  No effect: back to the party menu. B goes back to the bag.
            press(context, BUTTON_B, 1500ms);
            return;
        }
    }
}


//  Talk to the trash can in front and read everything it says.
std::string probe_trash_can(StoryRuntime& rt){
    story_talk_start(rt, "Trash can");
    std::string all;
    for (int page = 0; page < 8; page++){
        WhiteDialogWatcher dialog(COLOR_RED);
        rt.context.wait_for_all_requests();
        if (wait_until(rt.console, rt.context, 1500ms, { dialog }) != 0){
            break;
        }
        //  Let the page print, read it, next page.
        pbf_wait(rt.context, 1500ms);
        rt.context.wait_for_all_requests();
        all += story_read_dialog(rt) + " ";
        press(rt.context, BUTTON_B, 600ms);
    }
    story_back_to_overworld(rt, "trash can");
    rt.console.log("Trash can: " + all);
    return all;
}


}   //  namespace



int story_read_lead_level(StoryRuntime& rt){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    for (int attempt = 0; attempt < 2; attempt++){
        open_party_menu_from_overworld(console, context);
        //  LEFT from any slot on the right goes to slot 1.
        dpad(context, DPAD_LEFT, 500ms);
        //  A: SUMMARY is the first action. A: the summary (fades).
        press(context, BUTTON_A, 800ms);
        press(context, BUTTON_A, 500ms);
        context.wait_for_all_requests();
        SummaryWatcher summary(COLOR_RED);
        int ret = wait_until(console, context, 6000ms, { summary });
        int level = -1;
        if (ret == 0){
            pbf_wait(context, 800ms);
            context.wait_for_all_requests();
            VideoSnapshot screen = console.video().snapshot();
            StatsReader reader(COLOR_RED);
            PokemonFRLG_Stats stats;
            reader.read_page1(console.logger(), Language::English, screen, stats);
            if (stats.level.has_value()){
                level = (int)stats.level.value();
            }
        }
        story_back_to_overworld(rt, "summary");
        if (level > 0){
            console.log("Lead level: " + std::to_string(level));
            return level;
        }
        console.log("Could not read the lead's level.", COLOR_ORANGE);
    }
    return -1;
}



void story_teach_hm(
    StoryRuntime& rt,
    int hm_rank, int party_slot, int forget_slot,
    const std::string& move
){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log("Teaching " + move + " to party slot " + std::to_string(party_slot) + ".");

    open_bag_from_overworld(console, context);
    if (!select_bag_item(rt, BagPocket::KEY_ITEMS, {"tms and", "holds tm", "and hms", "case that"}, {}, "TM CASE")){
        story_back_to_overworld(rt, "bag");
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_teach_hm(): The TM CASE was not found in the bag.",
            console
        );
    }
    //  A: OPEN / REGISTER / CANCEL, cursor on OPEN. A: the TM Case.
    press(context, BUTTON_A, 800ms);
    press(context, BUTTON_A, 2500ms);
    //  HMs are listed first. To the top, then down to the HM.
    for (int i = 0; i < 30; i++){
        pbf_press_dpad(context, DPAD_UP, 80ms, 80ms);
    }
    pbf_wait(context, 300ms);
    for (int i = 1; i < hm_rank; i++){
        dpad(context, DPAD_DOWN);
    }
    //  A: USE / GIVE / EXIT, cursor on USE. A: the party menu (fades).
    press(context, BUTTON_A, 800ms);
    press(context, BUTTON_A, 2500ms);
    context.wait_for_all_requests();
    PartyMenuWatcher party(COLOR_RED);
    if (wait_until(console, context, 4000ms, { party }) != 0){
        story_back_to_overworld(rt, "TM Case");
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_teach_hm(): The party menu did not open from the TM Case.",
            console
        );
    }
    for (int i = 1; i < party_slot; i++){
        dpad(context, DPAD_DOWN);
    }
    press(context, BUTTON_A, 500ms);

    std::string text = wait_party_message(
        rt, {"learned", "four", "delete", "replace", "wants", "can't", "cannot", "already"}, 4, false
    );
    int new_move = find_move(move);
    if (contains(text, "four") || contains(text, "delete") || contains(text, "replace") || contains(text, "wants")){
        int slot = forget_slot;
        if (slot < 0){
            slot = forget_for_hm(rt);
        }
        console.log("Replacing move " + std::to_string(slot + 1) + " with " + move + ".");
        //  "... wants to learn the move X." / "However, ... already knows
        //  four moves." / "Should a move be deleted and replaced with X?"
        std::string question = wait_party_message(rt, {"should", "deleted", "replaced with"}, 5, true);
        if (question.empty()){
            console.log("The delete question was not read. Answering anyway.", COLOR_ORANGE);
        }
        pbf_wait(context, 800ms);
        press(context, BUTTON_A, 1500ms);   //  YES
        //  "Which move should be forgotten?" waits for a press, then the
        //  summary opens on the move list (cursor on move 1).
        press(context, BUTTON_A, 3500ms);
        for (int i = 0; i < slot; i++){
            dpad(context, DPAD_DOWN, 500ms);
        }
        press(context, BUTTON_A, 2500ms);
        //  "1, 2, and... Poof!" ... "X learned Y."
        wait_party_message(rt, {"learned"}, 8, true);
        press(context, BUTTON_A, 1500ms);
        if (party_slot == 1 && new_move >= 0){
            rt.memory.moves[slot] = new_move;
        }
    }else if (contains(text, "learned")){
        press(context, BUTTON_A, 1500ms);
        if (party_slot == 1 && new_move >= 0){
            for (int i = 0; i < 4; i++){
                if (rt.memory.moves[i] < 0){
                    rt.memory.moves[i] = new_move;
                    break;
                }
            }
        }
    }else{
        console.log("Unexpected answer while teaching " + move + ": " + text, COLOR_RED);
        press(context, BUTTON_A, 1500ms);
    }
    console.log("Taught " + move + ".");
    //  Back out of the TM Case and the bag.
    pbf_mash_button(context, BUTTON_B, 4000ms);
    context.wait_for_all_requests();
    story_back_to_overworld(rt, "TM Case");
}



void story_buy(StoryRuntime& rt, int index, int quantity, const std::string& what){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log("Buying " + std::to_string(quantity) + "x item " + std::to_string(index + 1) + ": " + what);

    //  "Hi there! May I help you?" BUY / SELL / SEE YA, cursor on BUY.
    story_talk_start(rt, what);
    pbf_wait(context, 1500ms);
    press(context, BUTTON_A, 2500ms);       //  BUY: the item list
    for (int i = 0; i < 12; i++){
        pbf_press_dpad(context, DPAD_UP, 80ms, 80ms);
    }
    for (int i = 0; i < index; i++){
        dpad(context, DPAD_DOWN);
    }
    press(context, BUTTON_A, 1500ms);       //  "How many would you like?"
    //  UP adds one (up to what the money allows).
    for (int i = 1; i < quantity; i++){
        dpad(context, DPAD_UP, 200ms);
    }
    press(context, BUTTON_A, 2000ms);       //  "That will be $X. OK?"
    press(context, BUTTON_A, 2000ms);       //  YES: "Here you are!"
    press(context, BUTTON_A, 1500ms);
    press(context, BUTTON_B, 2000ms);       //  close the list
    context.wait_for_all_requests();
    //  "Is there anything else?" -> B, "Please come again!"
    story_back_to_overworld(rt, "Poke Mart");
}



void story_use_item(StoryRuntime& rt, int party_slot, const std::string& words, const std::string& what){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log("Using an item on party slot " + std::to_string(party_slot) + ": " + what);
    open_bag_from_overworld(console, context);
    if (!select_bag_item(rt, BagPocket::ITEMS, {words.c_str()}, {}, what)){
        pbf_mash_button(context, BUTTON_B, 3000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, "bag");
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_use_item(): Not in the bag: " + what,
            console
        );
    }
    //  A: USE / GIVE / TOSS / CANCEL, cursor on USE. A: the party menu.
    press(context, BUTTON_A, 800ms);
    press(context, BUTTON_A, 2500ms);
    context.wait_for_all_requests();
    PartyMenuWatcher party(COLOR_RED);
    if (wait_until(console, context, 3000ms, { party }) != 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_use_item(): The party menu did not open: " + what,
            console
        );
    }
    for (int i = 1; i < party_slot; i++){
        dpad(context, DPAD_DOWN);
    }
    press(context, BUTTON_A, 3000ms);
    //  An evolution stone: "What? X is evolving!", the evolution (an item
    //  evolution cannot be stopped), "Congratulations! ...", then the bag.
    //  A goes through the text.
    WallClock deadline = current_time() + 40s;
    bool back = false;
    while (current_time() < deadline){
        BagWatcher bag(COLOR_RED);
        context.wait_for_all_requests();
        if (wait_until(console, context, 2000ms, { bag }) == 0){
            back = true;
            break;
        }
        press(context, BUTTON_A, 500ms);
    }
    console.log(back ? "Back in the bag: " + what : "Not back in the bag: " + what, back ? COLOR_BLUE : COLOR_ORANGE);
    pbf_mash_button(context, BUTTON_B, 3000ms);
    context.wait_for_all_requests();
    story_back_to_overworld(rt, "bag");
}



void story_heal_with_items(StoryRuntime& rt){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    bool fainted = rt.lead_fainted;
    console.log(fainted ? "Healing with items (the lead fainted)." : "Healing with items.");

    struct Use{
        int slot;
        bool revive;
    };
    std::vector<Use> uses;
    if (fainted){
        uses.push_back({1, true});
    }
    uses.push_back({1, false});
    uses.push_back({2, false});

    for (const Use& use : uses){
        open_bag_from_overworld(console, context);
        bool found = use.revive
            ? select_bag_item(rt, BagPocket::ITEMS, {"revives a", "revive"}, {"fully"}, "REVIVE")
            : select_bag_item(rt, BagPocket::ITEMS, {"fully restores", "fully restore"}, {"revive"}, "FULL RESTORE");
        if (found){
            use_item_on(rt, use.slot, use.revive ? "REVIVE" : "FULL RESTORE");
        }
        pbf_mash_button(context, BUTTON_B, 3000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, "bag");
    }
    rt.lead_fainted = false;
}



void story_elevator(StoryRuntime& rt, int from, int to, const std::string& what){
    ProControllerContext& context = rt.context;
    rt.console.log("Elevator: " + what + " (" + std::to_string(from) + " -> " + std::to_string(to) + ")");
    //  "Which floor do you want?" and the floor list, cursor on this floor.
    story_talk_start(rt, what);
    pbf_wait(context, 2000ms);
    for (int i = from; i < to; i++){
        dpad(context, DPAD_DOWN);
    }
    for (int i = to; i < from; i++){
        dpad(context, DPAD_UP);
    }
    press(context, BUTTON_A, 1000ms);
    context.wait_for_all_requests();
    //  The elevator shakes for a while.
    story_cutscene(rt, 3000ms, 30s, false, what);
}



void story_push_boulder(StoryRuntime& rt, DpadPosition direction){
    //  Facing the boulder with Strength on, a short press pushes it. The
    //  player stays. Released long before the push is over, so the player
    //  does not walk after it.
    pbf_press_dpad(rt.context, direction, 80ms, 1400ms);
    rt.context.wait_for_all_requests();
    rt.facing = direction;
}



void story_oscillate(StoryRuntime& rt, DpadPosition direction, Milliseconds max_time, const std::string& what){
    rt.console.log("Walking back and forth until a scene starts: " + what);
    DpadPosition back = opposite(direction);
    WhiteDialogWatcher dialog(COLOR_RED);
    BlackScreenWatcher black(COLOR_RED);
    WallClock deadline = current_time() + max_time;
    rt.context.wait_for_all_requests();
    int ret = run_until<ProControllerContext>(
        rt.console, rt.context,
        [&](ProControllerContext& context){
            while (current_time() < deadline){
                for (int i = 0; i < 10; i++){
                    DpadPosition d = (i % 2 == 0) ? direction : back;
                    //  Same timing as a single walking step.
                    pbf_press_dpad(context, d, 200ms, 300ms);
                }
                context.wait_for_all_requests();
            }
        },
        { dialog, black }
    );
    rt.facing = DPAD_NONE;
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_oscillate(): No scene started: " + what,
            rt.console
        );
    }
    rt.console.log("A scene started: " + what);
}



void story_trash_can_puzzle(StoryRuntime& rt, const StoryScript& script, const StoryStep& step){
    std::vector<int32_t> to_can = script.leg_list(step.legs_a);
    std::vector<int32_t> back = script.leg_list(step.legs_b);
    const int cans = (int)to_can.size();
    rt.console.log("Trash can puzzle: " + std::to_string(cans) + " cans.");
    if (cans != 15 || back.size() != to_can.size()){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_trash_can_puzzle(): Unexpected puzzle data.",
            rt.console
        );
    }

    enum class Answer{ NOPE, FIRST, SECOND, RESET };
    auto probe = [&](int can) -> Answer{
        rt.console.log("Trash can " + std::to_string(can + 1));
        walk_leg(rt, script, to_can[(size_t)can]);
        std::string text = probe_trash_can(rt);
        if (back[(size_t)can] >= 0){
            walk_leg(rt, script, back[(size_t)can]);
        }
        if (contains(text, "second") || contains(text, "motorized") || contains(text, "door opened")){
            return Answer::SECOND;
        }
        if (contains(text, "reset")){
            return Answer::RESET;
        }
        if (contains(text, "first") || contains(text, "switch") || contains(text, "turn it on")){
            return Answer::FIRST;
        }
        return Answer::NOPE;
    };

    //  The cans are a 5x3 grid, numbered left to right, top to bottom. The
    //  second switch is next to the first one (SetVermilionTrashCans).
    auto neighbors = [](int can){
        std::vector<int> ret;
        int x = can % 5;
        int y = can / 5;
        if (x > 0) ret.push_back(can - 1);
        if (x < 4) ret.push_back(can + 1);
        if (y > 0) ret.push_back(can - 5);
        if (y < 2) ret.push_back(can + 5);
        return ret;
    };

    std::set<int> candidates;
    for (int i = 0; i < cans; i++){
        candidates.insert(i);
    }
    for (int attempt = 0; attempt < 80; attempt++){
        if (candidates.empty()){
            for (int i = 0; i < cans; i++){
                candidates.insert(i);
            }
        }
        int can = *candidates.begin();
        Answer answer = probe(can);
        if (answer == Answer::NOPE){
            candidates.erase(can);
            continue;
        }
        if (answer == Answer::SECOND){
            rt.console.log("Both locks are open.", COLOR_BLUE);
            return;
        }
        if (answer == Answer::RESET){
            candidates.clear();
            continue;
        }
        //  First lock open. The second switch is next to it, but a wrong
        //  guess resets both switches (new random ones), so there is one
        //  guess per round.
        rt.console.log("First lock at can " + std::to_string(can + 1) + ".");
        std::vector<int> next = neighbors(can);
        Answer second = probe(next[(size_t)attempt % next.size()]);
        if (second == Answer::SECOND){
            rt.console.log("Both locks are open.", COLOR_BLUE);
            return;
        }
        candidates.clear();
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "story_trash_can_puzzle(): The locks did not open after 80 tries.",
        rt.console
    );
}



namespace{

const char* direction_word(DpadPosition direction){
    switch (direction){
    case DPAD_UP:       return "up";
    case DPAD_DOWN:     return "down";
    case DPAD_LEFT:     return "left";
    case DPAD_RIGHT:    return "right";
    default:            return "none";
    }
}

//  Training and catching walk back and forth between two grass tiles: the
//  first one, where the routes there and back meet, and the next one
//  ("forth"). Which of the two the player is on is counted step by step.
//  A step the count gets wrong (one the game missed, or a double step)
//  would leave the player a tile off, and the walk to the nurse would run
//  into a wall. With the video check, a picture taken on the first tile
//  tells where the player really stands.
class GrassSpot{
public:
    GrassSpot(StoryRuntime& rt, DpadPosition forth, StoryBattleKind kind, const std::string& where)
        : m_rt(rt)
        , m_forth(forth)
        , m_kind(kind)
        , m_where(where)
    {}

    //  The route has just brought the player onto the first tile. The first
    //  time, that is where the picture is taken; after that, it is checked.
    void arrived(){
        if (!m_has_picture){
            m_has_picture = story_take_picture(m_rt, m_picture);
            return;
        }
        bool away = false;
        to_first_tile(away);
    }

    //  Make "away" (on the second tile) match where the player stands. Off
    //  the two tiles, walk back to the first one.
    void check(bool& away){
        if (!m_has_picture){
            return;
        }
        const int forth_x = m_forth == DPAD_LEFT ? -1 : (m_forth == DPAD_RIGHT ? 1 : 0);
        const int forth_y = m_forth == DPAD_UP ? -1 : (m_forth == DPAD_DOWN ? 1 : 0);
        for (int attempt = 0; attempt < 4; attempt++){
            int dx = 0;
            int dy = 0;
            AutoStoryScroll::Screen now;
            if (!story_offset_from(m_rt, m_picture, m_where, dx, dy, now)){
                return;     //  The video does not tell: go by the count.
            }
            if (dx == 0 && dy == 0){
                if (away){
                    m_rt.console.log(
                        m_where + ": the player is on the first grass tile, not the second as counted. Going by the video.",
                        COLOR_ORANGE
                    );
                }
                away = false;
                //  The latest look of the place (people walking around, a
                //  sign popping up and going away).
                m_picture = std::move(now);
                return;
            }
            if (dx == forth_x && dy == forth_y){
                if (!away){
                    m_rt.console.log(
                        m_where + ": the player is on the second grass tile, not the first as counted. Going by the video.",
                        COLOR_ORANGE
                    );
                }
                away = true;
                return;
            }
            const DpadPosition toward =
                dy > 0 ? DPAD_UP :
                dy < 0 ? DPAD_DOWN :
                dx > 0 ? DPAD_LEFT : DPAD_RIGHT;
            m_rt.console.log(
                m_where + ": the player is " + std::to_string(dx) + ", " + std::to_string(dy) +
                " tiles from the first grass tile. A step " + direction_word(toward) + " back toward it.",
                COLOR_ORANGE
            );
            story_checked_step(m_rt, toward, false, false, m_kind, m_where);
            away = false;   //  Not known yet: looked at again.
        }
        m_rt.console.log(m_where + ": could not get back onto the grass tiles. Going on.", COLOR_RED);
    }

    //  Onto the first tile, where the routes there and back meet.
    void to_first_tile(bool& away){
        for (int attempt = 0; attempt < 3; attempt++){
            check(away);
            if (!away){
                return;
            }
            story_checked_step(m_rt, opposite(m_forth), false, false, m_kind, m_where);
            away = false;
            if (!m_has_picture){
                return;
            }
        }
    }

private:
    StoryRuntime& m_rt;
    DpadPosition m_forth;
    StoryBattleKind m_kind;
    std::string m_where;
    bool m_has_picture = false;
    AutoStoryScroll::Screen m_picture;
};

}



void story_grind(
    StoryRuntime& rt, const StoryScript& script, const StoryStep& step,
    int extra_levels
){
    ConsoleHandle& console = rt.console;
    const int target = step.p0 + extra_levels;
    const int heal_every = std::max(1, step.p1);
    const DpadPosition forth = step.dir;
    const DpadPosition back = opposite(forth);
    std::vector<int32_t> to_grass = script.leg_list(step.legs_a);
    std::vector<int32_t> to_nurse = script.leg_list(step.legs_b);
    std::vector<int32_t> to_grass_again = script.leg_list(step.legs_c);
    const std::string where = step.label;

    auto walk = [&](const std::vector<int32_t>& legs){
        for (int32_t leg : legs){
            walk_leg(rt, script, leg);
        }
    };

    int level = story_read_lead_level(rt);
    console.log(
        where + ": training the lead to level " + std::to_string(target) +
        " (now " + (level < 0 ? std::string("unknown") : std::to_string(level)) + ")."
    );
    walk(to_grass);
    GrassSpot spot(rt, forth, StoryBattleKind::WILD_FIGHT, where);
    spot.arrived();

    //  If the level cannot be read, stop after a fixed number of battles.
    const uint64_t battle_cap = level < 0 ? 40 : 400;
    uint64_t battles = 0;
    uint64_t steps_without_battle = 0;
    int since_heal = 0;
    bool away = false;      //  on the second grass tile
    bool done = level >= target;

    while (!done){
        DpadPosition d = away ? back : forth;
        StoryBattleResult result = story_checked_step(
            rt, d, false, false, StoryBattleKind::WILD_FIGHT, where, false
        );
        away = !away;
        if (result == StoryBattleResult::NO_BATTLE){
            steps_without_battle++;
            if (steps_without_battle > 1000){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "story_grind(): No wild battle in 1000 steps at " + where,
                    console
                );
            }
            continue;
        }
        steps_without_battle = 0;
        battles++;
        since_heal++;

        if (result == StoryBattleResult::LOST){
            //  Back at the Pokemon Center in front of the nurse, healed.
            console.log("The party fainted while training. Back to the grass.", COLOR_ORANGE);
            away = false;
            since_heal = 0;
            if (rt.save_after_heal){
                story_save(rt, rt.has_dex);
                rt.facing = DPAD_UP;
                story_saved(rt, rt.step_index);
            }
            walk(to_grass_again);
            spot.arrived();
            continue;
        }

        //  The battle is a moment the screen stands still anyway: check the
        //  count of the steps.
        spot.check(away);

        if (rt.memory.level_up_seen){
            rt.memory.level_up_seen = false;
            int now = story_read_lead_level(rt);
            if (now > 0){
                level = now;
            }
        }
        if (level >= target){
            break;
        }
        if (battles >= battle_cap){
            console.log("Stopping the training after " + std::to_string(battles) + " battles.", COLOR_ORANGE);
            break;
        }

        if (rt.lead_fainted || since_heal >= heal_every){
            spot.to_first_tile(away);
            walk(to_nurse);
            //  The training goes on: after a reset, it starts again from
            //  this save in front of the nurse.
            story_heal(rt, where, true);
            since_heal = 0;
            int now = story_read_lead_level(rt);
            if (now > 0){
                level = now;
            }
            if (level >= target){
                console.log(where + ": level " + std::to_string(level) + " reached.", COLOR_BLUE);
                if (rt.save_after_heal){
                    //  Saved right here: after a reset, go on with the next step.
                    story_saved(rt, rt.step_index + 1);
                }
                return;     //  Already healed at the nurse.
            }
            walk(to_grass_again);
            spot.arrived();
        }
    }

    console.log(where + ": level " + std::to_string(level) + " reached.", COLOR_BLUE);
    spot.to_first_tile(away);
    walk(to_nurse);
    story_heal(rt, where);
}



void story_catch(StoryRuntime& rt, const StoryStep& step){
    ConsoleHandle& console = rt.console;
    std::set<std::string> species = story_species_list(step.text);
    for (const std::string& slug : species){
        if (rt.caught.count(slug) != 0){
            console.log("Already caught on the way: " + slug, COLOR_BLUE);
            return;
        }
    }
    console.log(std::string("Catching one of: ") + step.text);

    StoryBattleSettings saved = rt.battle;
    rt.battle.catch_species = species;
    rt.battle.catch_balls = (uint16_t)std::max(1, step.p0);

    const DpadPosition forth = step.dir;
    const DpadPosition back = opposite(forth);
    GrassSpot spot(rt, forth, StoryBattleKind::WILD_RUN, step.label);
    spot.arrived();
    bool away = false;
    bool caught = false;
    WallClock deadline = current_time() + std::chrono::minutes(30);
    while (current_time() < deadline){
        StoryBattleResult result = story_checked_step(
            rt, away ? back : forth, false, false, StoryBattleKind::CATCH, step.label
        );
        away = !away;
        if (result == StoryBattleResult::CAUGHT){
            caught = true;
            break;
        }
        if (result != StoryBattleResult::NO_BATTLE){
            spot.check(away);
        }
    }
    rt.battle.catch_species.clear();
    spot.to_first_tile(away);
    rt.battle = saved;
    if (!caught){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::string("story_catch(): Nothing caught in 30 minutes: ") + step.text,
            console
        );
    }
}



}
}
}

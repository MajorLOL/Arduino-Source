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
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Map/PokemonFRLG_MapDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_BagDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_StatsReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_AutoStoryMemory.h"
#include "PokemonFRLG_AutoStoryCollection.h"
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

//  Party menu, the actions of a Pokemon: with the cursor on a field move its
//  description shows ("Fly to a known town."). sFieldMoveDescriptionWindowTemplate:
//  tiles (0,13) 18x3.
const ImageFloatBox FIELD_MOVE_DESCRIPTION_BOX(0.010, 0.660, 0.580, 0.130);


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


//  The summary's move list (choosing a move to forget): the name of move
//  "i" (0-3). POKESUM_WIN_RIGHT_PANE of the moves page: tiles (20,2) 10x18,
//  names at x 3, y 28 i + 5 (PokeSum_PrintMoveName).
ImageFloatBox summary_move_box(int i){
    return ImageFloatBox(0.675, (19.0 + 28.0 * i) / 160.0, 0.310, 0.100);
}

//  The move whose name (the longest one) is in "text". -1: none.
int move_in_text(const std::string& text){
    std::string squeezed;
    for (char c : text){
        if (c != ' '){
            squeezed += (char)std::tolower((unsigned char)c);
        }
    }
    int best = -1;
    size_t best_length = 0;
    const std::vector<AutoStoryDex::MoveInfo>& moves = AutoStoryDex::moves();
    for (size_t i = 0; i < moves.size(); i++){
        std::string name;
        for (const char* c = moves[i].name; *c; c++){
            if (*c != ' '){
                name += (char)std::tolower((unsigned char)*c);
            }
        }
        if (name.size() >= 3 && name.size() > best_length && squeezed.find(name) != std::string::npos){
            best = (int)i;
            best_length = name.size();
        }
    }
    return best;
}

//  On the summary's move list: the weakest of the 4 moves that is not an
//  HM, by the names on the screen ("prefer": that move if it is there).
//  -1: not all read. "lead": the moves are the lead's (remembered).
int weakest_move_on_summary(StoryRuntime& rt, int prefer, bool lead){
    int worst = -1;
    double worst_value = 1e9;
    int preferred = -1;
    bool all_read = true;
    for (int i = 0; i < 4; i++){
        const std::string text = read_story_text(rt.console, summary_move_box(i), false);
        const int move = move_in_text(text);
        rt.console.log("Move " + std::to_string(i + 1) + ": " + text +
                       (move < 0 ? std::string(" (not known)") : " = " + std::string(AutoStoryDex::moves()[(size_t)move].name)));
        if (move >= 0 && move == prefer){
            preferred = i;
        }
        if (move < 0){
            all_read = false;
            continue;
        }
        if (lead){
            rt.memory.moves[i] = move;
        }
        if (AutoStoryDex::moves()[(size_t)move].hm){
            continue;
        }
        const double value = move_value(move, rt.battle);
        if (value < worst_value){
            worst_value = value;
            worst = i;
        }
    }
    if (preferred >= 0){
        return preferred;
    }
    return all_read ? worst : -1;
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
    //  The party menu opened from the bag keeps the cursor where the last
    //  one left it (InitPartyMenu(), keepCursorPos). LEFT from any slot on
    //  the right goes to slot 1.
    dpad(context, DPAD_LEFT, 500ms);
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
    return story_read_slot(rt, 1, nullptr);
}


int story_read_slot(StoryRuntime& rt, int slot, std::string* species){
    StoryMemoryScope memory_scope("party menu");
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    for (int attempt = 0; attempt < 2; attempt++){
        open_party_menu_from_overworld(console, context);
        //  LEFT from any slot on the right goes to slot 1.
        dpad(context, DPAD_LEFT, 500ms);
        for (int i = 1; i < slot; i++){
            dpad(context, DPAD_DOWN);
        }
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
            if (species != nullptr){
                *species = stats.name;
            }
        }
        story_back_to_overworld(rt, "summary");
        if (level > 0){
            console.log(
                "Party slot " + std::to_string(slot) + ": " +
                (species != nullptr ? *species + " " : std::string()) + "level " + std::to_string(level)
            );
            return level;
        }
        console.log("Could not read party slot " + std::to_string(slot) + ".", COLOR_ORANGE);
    }
    return -1;
}


void story_check_lead(StoryRuntime& rt){
    if (rt.team.empty()){
        return;
    }
    ConsoleHandle& console = rt.console;
    const std::set<std::string>& line = rt.team[0].line;
    std::string front;
    for (int attempt = 0; attempt < 3 && front.empty(); attempt++){
        story_read_slot(rt, 1, &front);
    }
    //  The starter's types as it is now: the program starts with the first
    //  form's (CHARMANDER: FIRE), CHARIZARD is also FLYING (FLY with the
    //  same-type bonus, the weakest move against Moltres is CUT, not FLY).
    auto lead_types = [&](const std::string& slug){
        for (const AutoStoryDex::SpeciesInfo& info : AutoStoryDex::species()){
            if (slug != info.slug){
                continue;
            }
            if (rt.lead_type1 != info.type1 || rt.lead_type2 != info.type2){
                console.log("The lead's types: those of " + std::string(info.name) + ".");
            }
            rt.lead_type1 = info.type1;
            rt.lead_type2 = info.type2;
            rt.battle.lead_type1 = info.type1;
            rt.battle.lead_type2 = info.type2;
            return;
        }
    };
    if (!front.empty() && line.count(front) != 0){
        lead_types(front);
    }
    if (front.empty() || line.count(front) != 0){
        return;
    }
    console.log("Party slot 1 is " + front + ", not the starter. Looking for the starter.", COLOR_ORANGE);
    //  A member trains in front by swapping places with slot 1: the starter
    //  is in one of the team's slots.
    const int last = std::min(6, (int)rt.team.size() + 1);
    for (int slot = 2; slot <= last; slot++){
        std::string species;
        if (story_read_slot(rt, slot, &species) < 0){
            break;      //  No Pokemon there (or not readable).
        }
        if (line.count(species) != 0){
            story_swap_party(rt, 1, slot);
            console.log("The starter was in party slot " + std::to_string(slot) + ": back in slot 1.", COLOR_BLUE);
            lead_types(species);
            return;
        }
    }
    console.log("The starter was not found in the party. Going on as it is.", COLOR_RED);
}

void story_fix_party_order(StoryRuntime& rt, bool force){
    if (rt.team.empty()){
        return;
    }
    if (!force && (rt.party_extras <= 0 || rt.party_order_final)){
        return;
    }
    StoryMemoryScope memory_scope("party order");
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log("Checking that the team is in its party slots.");
    open_party_menu_from_overworld(console, context);
    pbf_wait(context, 800ms);
    context.wait_for_all_requests();
    const int count = (int)detect_last_occupied_party_slot(console) + 1;
    story_back_to_overworld(rt, "party menu");
    console.log("Pokemon in the party: " + std::to_string(count));

    std::vector<std::string> species((size_t)count + 1);
    bool all_read = true;
    for (int slot = 1; slot <= count; slot++){
        if (story_read_slot(rt, slot, &species[(size_t)slot]) < 0 || species[(size_t)slot].empty()){
            all_read = false;
        }
    }
    //  The team joins in its order: up to the first member not in the party.
    int in_place = 0;
    for (size_t i = 0; i < rt.team.size() && (int)i < count; i++){
        const std::set<std::string>& line = rt.team[i].line;
        const int want = (int)i + 1;
        if (species[(size_t)want].empty()){
            break;      //  Not read: not known.
        }
        if (line.count(species[(size_t)want]) != 0){
            in_place++;
            continue;
        }
        int found = -1;
        for (int j = want + 1; j <= count; j++){
            if (line.count(species[(size_t)j]) != 0){
                found = j;
                break;
            }
        }
        if (found < 0){
            break;
        }
        console.log("Team member " + species[(size_t)found] + " is in party slot " + std::to_string(found) + ", not " + std::to_string(want) + ".", COLOR_ORANGE);
        story_swap_party(rt, want, found);
        std::swap(species[(size_t)want], species[(size_t)found]);
        in_place++;
    }
    if (all_read && count - in_place != rt.party_extras){
        rt.party_extras = count - in_place;
        console.log("Pokemon in the party besides the team: " + std::to_string(rt.party_extras));
    }
    rt.party_order_final = all_read && in_place == (int)rt.team.size();
    if (rt.party_order_final){
        console.log("The whole team is in its party slots.", COLOR_BLUE);
    }
}

//  In the overworld: the party menu, the actions of "party_slot", the cursor
//  down the field moves until the one whose description has one of "words"
//  (Fly: "Fly to a known town."). True: the cursor is on it. False: not
//  found, back in the overworld.
static bool open_field_move_action(StoryRuntime& rt, int party_slot, const std::vector<std::string>& words){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    open_party_menu_from_overworld(console, context);
    dpad(context, DPAD_LEFT, 500ms);
    for (int i = 1; i < party_slot; i++){
        dpad(context, DPAD_DOWN);
    }
    //  A: the actions, cursor on SUMMARY; the field moves come next, in
    //  the order of the moves (SetPartyMonFieldSelectionActions).
    press(context, BUTTON_A, 1000ms);
    for (int i = 0; i < 4; i++){
        dpad(context, DPAD_DOWN, 600ms);
        context.wait_for_all_requests();
        std::string text = read_story_text(console, FIELD_MOVE_DESCRIPTION_BOX, false);
        if (text.size() < 6){
            text += " " + read_story_text(console, FIELD_MOVE_DESCRIPTION_BOX, true);
        }
        console.log("Field move " + std::to_string(i + 1) + ": " + text);
        for (const std::string& word : words){
            if (contains(text, word.c_str())){
                return true;
            }
        }
    }
    pbf_mash_button(context, BUTTON_B, 2500ms);
    context.wait_for_all_requests();
    story_back_to_overworld(rt, "party menu");
    return false;
}
static bool open_fly_action(StoryRuntime& rt, int party_slot){
    return open_field_move_action(rt, party_slot, {"fly", "known town"});
}

void story_field_move(StoryRuntime& rt, int party_slot, const std::string& move, DpadPosition facing, const std::string& what){
    StoryMemoryScope memory_scope("field move");
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log("Field move " + move + " from party slot " + std::to_string(party_slot) + ": " + what);
    story_fix_party_order(rt, false);
    story_face(rt, facing);
    //  The descriptions of the party menu (gText_CutATreeOrGrass...), in
    //  parts: the small text reads badly ("Cut a tree or grass." came out
    //  as "cut o trae or grgbs,"). No other description has these words.
    std::vector<std::string> words;
    if (move == "CUT"){
        words = {"cut", "tree", "grass"};
    }else if (move == "FLY"){
        words = {"fly", "known town"};
    }else if (move == "SURF"){
        words = {"travel", "on water"};
    }else if (move == "STRENGTH"){
        words = {"boulder", "heavy"};
    }else if (move == "FLASH"){
        words = {"light up", "darkness"};
    }else if (move == "ROCK SMASH"){
        words = {"shatter", "cracked"};
    }else if (move == "WATERFALL"){
        words = {"waterfall", "climb"};
    }else{
        std::string lower;
        for (char c : move){
            lower += (char)std::tolower((unsigned char)c);
        }
        words = {lower};
    }
    for (int attempt = 0; attempt < 3; attempt++){
        if (!open_field_move_action(rt, party_slot, words)){
            story_face(rt, facing);
            continue;
        }
        //  A: the menu closes, the move's field effect plays.
        pbf_press_button(context, BUTTON_A, 200ms, 4000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, "field move");
        rt.facing = facing;
        console.log("Used " + move + ": " + what, COLOR_BLUE);
        return;
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "story_field_move(): " + move + " not found in the party menu of slot " + std::to_string(party_slot) + ".",
        console
    );
}

bool story_knows_fly(StoryRuntime& rt, int party_slot){
    StoryMemoryScope memory_scope("fly");
    rt.console.log("Does party slot " + std::to_string(party_slot) + " know FLY?");
    for (int attempt = 0; attempt < 2; attempt++){
        if (open_fly_action(rt, party_slot)){
            pbf_mash_button(rt.context, BUTTON_B, 2500ms);
            rt.context.wait_for_all_requests();
            story_back_to_overworld(rt, "party menu");
            rt.console.log("It knows FLY.", COLOR_BLUE);
            return true;
        }
    }
    rt.console.log("It does not know FLY.", COLOR_BLUE);
    return false;
}

void story_fly(StoryRuntime& rt, int party_slot, int destination, const std::string& what){
    StoryMemoryScope memory_scope("fly");
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log("Flying: " + what);
    story_fix_party_order(rt, false);
    for (int attempt = 0; attempt < 3; attempt++){
        if (!open_fly_action(rt, party_slot)){
            continue;
        }
        KantoMapWatcher map(COLOR_RED);
        context.wait_for_all_requests();
        pbf_press_button(context, BUTTON_A, 200ms, 0ms);
        if (wait_until(console, context, 10000ms, { map }) != 0){
            console.log("The Fly map did not open.", COLOR_ORANGE);
            pbf_mash_button(context, BUTTON_B, 4000ms);
            context.wait_for_all_requests();
            story_back_to_overworld(rt, "Fly map");
            continue;
        }
        pbf_wait(context, 1000ms);
        context.wait_for_all_requests();
        //  The cursor to the town (from the top left corner), A until the
        //  screen goes black, then the flight and the landing.
        fly_from_kanto_map(console, context, (KantoFlyLocation)destination);
        rt.facing = DPAD_DOWN;      //  FlyInFieldEffect_End: DIR_SOUTH.
        rt.arrived_by_warp = true;
        console.log("Landed: " + what, COLOR_BLUE);
        return;
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "story_fly(): FLY not found in the actions of party slot " + std::to_string(party_slot) +
        ", or the Fly map did not open: " + what,
        console
    );
}

void story_swap_party(StoryRuntime& rt, int a, int b){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log("Swapping party slots " + std::to_string(a) + " and " + std::to_string(b) + ".");
    open_party_menu_from_overworld(console, context);
    dpad(context, DPAD_LEFT, 500ms);
    for (int i = 1; i < a; i++){
        dpad(context, DPAD_DOWN);
    }
    //  SUMMARY, (field moves), SWITCH, ITEM, CANCEL: up from the top wraps
    //  around to CANCEL, then ITEM, then SWITCH.
    press(context, BUTTON_A, 800ms);
    for (int i = 0; i < 3; i++){
        dpad(context, DPAD_UP, 300ms);
    }
    press(context, BUTTON_A, 1000ms);       //  "Move to where?"
    for (int i = a; i < b; i++){
        dpad(context, DPAD_DOWN);
    }
    press(context, BUTTON_A, 2500ms);       //  The two slide out and back.
    context.wait_for_all_requests();
    story_back_to_overworld(rt, "party menu");
}



void story_teach_hm(
    StoryRuntime& rt,
    int hm_rank, int party_slot, int forget_slot,
    const std::string& move_and_forget
){
    //  "MOVE|FORGET": the move to replace if the Pokemon knows it.
    std::string move = move_and_forget;
    std::string prefer_name;
    const size_t bar = move_and_forget.find('|');
    if (bar != std::string::npos){
        move = move_and_forget.substr(0, bar);
        prefer_name = move_and_forget.substr(bar + 1);
    }
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log("Teaching " + move + " to party slot " + std::to_string(party_slot) + ".");
    story_fix_party_order(rt, false);

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
    //  The party menu opened from the bag keeps the cursor where the last
    //  one left it (InitPartyMenu(), keepCursorPos). LEFT from any slot on
    //  the right goes to slot 1.
    dpad(context, DPAD_LEFT, 500ms);
    for (int i = 1; i < party_slot; i++){
        dpad(context, DPAD_DOWN);
    }
    press(context, BUTTON_A, 500ms);

    std::string text = wait_party_message(
        rt, {"learned", "four", "delete", "replace", "wants", "can't", "cannot", "already", "compatible"}, 4, false
    );
    if (contains(text, "compatible") && !contains(text, "four")){
        //  "X and Y are not compatible. Y can't be learned." (Not "... can't
        //  learn more than four moves.")
        pbf_mash_button(context, BUTTON_B, 4000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, "TM Case");
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_teach_hm(): Party slot " + std::to_string(party_slot) + " cannot learn " + move + ": " + text,
            console
        );
    }
    int new_move = find_move(move);
    if (contains(text, "four") || contains(text, "delete") || contains(text, "replace") || contains(text, "wants")){
        int slot = forget_slot;
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
        context.wait_for_all_requests();
        if (slot < 0){
            //  The moves as the screen shows them (for the lead, the ones
            //  last seen in a battle may be another team member's).
            slot = weakest_move_on_summary(rt, prefer_name.empty() ? -1 : find_move(prefer_name), party_slot == 1);
        }
        if (slot < 0 && party_slot == 1){
            slot = forget_for_hm(rt);
        }
        if (slot < 0){
            //  Not the lead, and its moves not read: Lapras got Surf in
            //  place of move 1 and Strength in place of move 4, so moves 2
            //  and 3 are its own; move 2 is the safer guess.
            console.log("The moves were not read: replacing move 2.", COLOR_ORANGE);
            slot = 1;
        }
        console.log("Replacing move " + std::to_string(slot + 1) + " with " + move + ".");
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
    //  Maybe healing items: look in the bag again when one is needed.
    rt.memory.no_heal_items = false;
    //  The game lets the quantity go only as high as the money allows
    //  (Task_BuyHowManyDialogueInit: money / price, 99 at most), and
    //  (AdjustQuantityAccordingToDPadInput) UP past that goes round to 1,
    //  DOWN from 1 round to the most, RIGHT adds ten and stops at the most.
    //  UP for a quantity the money does not allow therefore buys something
    //  else: 10 ULTRA BALLs with money for three were 1 (nine times UP: 2, 3,
    //  1, 2, 3, 1, 2, 3, 1). So: RIGHT for the tens in one purchase, then the
    //  rest one at a time. Each stops by itself where the money ends ("You
    //  don't have enough money." takes the same four presses), and never
    //  more is bought than asked for.
    const int tens = quantity > 0 ? (quantity - 1) / 10 : 0;
    const int singles = quantity > 0 ? (quantity - 1) % 10 : 0;
    console.log(
        (quantity <= 0
            ? std::string("Buying as many as the money allows of")
            : "Buying " + std::to_string(quantity) + " (or as many as the money allows) of") +
        " item " + std::to_string(index + 1) + ": " + what +
        (quantity > 0 && singles > 0
            ? " (" + std::to_string(1 + 10 * tens) + " in one purchase, " + std::to_string(singles) + " one at a time)"
            : std::string())
    );

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
    //  One purchase, from the item list back to the item list (the cursor
    //  stays on the item).
    auto purchase = [&](bool most, int right){
        press(context, BUTTON_A, 1500ms);       //  "How many would you like?"
        if (most){
            //  As many as the money allows: DOWN from 1 goes round to the
            //  most (up to 99).
            dpad(context, DPAD_DOWN, 200ms);
        }
        for (int i = 0; i < right; i++){
            dpad(context, DPAD_RIGHT, 200ms);
        }
        press(context, BUTTON_A, 2000ms);       //  "That will be $X. OK?"
        press(context, BUTTON_A, 2000ms);       //  YES: "Here you go! Thank you!"
        press(context, BUTTON_A, 1500ms);       //  the item list again
    };
    purchase(quantity <= 0, tens);
    for (int i = 0; i < singles; i++){
        purchase(false, 0);
    }
    press(context, BUTTON_B, 2000ms);       //  close the list
    context.wait_for_all_requests();
    //  "Is there anything else?" -> B, "Please come again!"
    story_back_to_overworld(rt, "Poke Mart");
}



void story_use_item(StoryRuntime& rt, int party_slot, const std::string& words, const std::string& what, bool give){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log(
        std::string(give ? "Giving an item to hold to party slot " : "Using an item on party slot ") +
        std::to_string(party_slot) + ": " + what
    );
    story_fix_party_order(rt, false);
    if (give && party_slot == 1){
        //  The starter (Charizard and the EXP. SHARE): in slot 1.
        story_check_lead(rt);
    }
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
    //  A: USE / GIVE / TOSS / CANCEL (sContextMenuItems_Field, the ITEMS
    //  pocket), cursor on USE. A: the party menu.
    press(context, BUTTON_A, 800ms);
    if (give){
        dpad(context, DPAD_DOWN, 500ms);
    }
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
    //  The party menu opened from the bag keeps the cursor where the last
    //  one left it (InitPartyMenu(), keepCursorPos). LEFT from any slot on
    //  the right goes to slot 1.
    dpad(context, DPAD_LEFT, 500ms);
    for (int i = 1; i < party_slot; i++){
        dpad(context, DPAD_DOWN);
    }
    press(context, BUTTON_A, 3000ms);
    //  An evolution stone: "What? X is evolving!", the evolution (an item
    //  evolution cannot be stopped), "Congratulations! ...", then the bag.
    //  A goes through the text. GIVE: "X was given the Y to hold.", or "X is
    //  already holding Z. Would you like to switch the two items?" (A: YES),
    //  then the bag.
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
    if (!rt.battle.team.empty()){
        //  The team: revive who fainted in the last battle, then a Full
        //  Restore for everyone who fought and is below 60% of its HP (as
        //  last seen; not seen: healed). The Full Restores are for the
        //  battles too, and the League is long.
        for (int index : rt.memory.team_used){
            const int slot = index + 1;
            const bool fainted = rt.memory.team_fainted.count(index) != 0;
            const double hp = index >= 0 && index < 6 ? rt.memory.team_hp[index] : -1;
            if (!fainted && hp >= 0.6){
                console.log(
                    "Party slot " + std::to_string(slot) + " is at " + std::to_string((int)(hp * 100 + 0.5)) +
                    "% of its HP: no Full Restore."
                );
                continue;
            }
            for (int round = 0; round < 2; round++){
                const bool revive = round == 0;
                if (revive && !fainted){
                    continue;
                }
                console.log((revive ? "Reviving party slot " : "Full Restore on party slot ") + std::to_string(slot) + ".");
                open_bag_from_overworld(console, context);
                bool found = revive
                    ? select_bag_item(rt, BagPocket::ITEMS, {"revives a", "revive"}, {"fully"}, "REVIVE")
                    : select_bag_item(rt, BagPocket::ITEMS, {"fully restores", "fully restore"}, {"revive"}, "FULL RESTORE");
                if (found){
                    use_item_on(rt, slot, revive ? "REVIVE" : "FULL RESTORE");
                    if (!revive && index >= 0 && index < 6){
                        rt.memory.team_hp[index] = 1.0;
                    }
                }
                pbf_mash_button(context, BUTTON_B, 3000ms);
                context.wait_for_all_requests();
                story_back_to_overworld(rt, "bag");
            }
        }
        rt.lead_fainted = false;
        return;
    }
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



void story_heal_after_legendary(StoryRuntime& rt){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log("Healing the first two of the team with items (worn out from the legendary Pokemon).", COLOR_BLUE);
    struct Use{
        int slot;
        bool revive;
    };
    const Use uses[] = {{1, true}, {2, true}, {1, false}, {2, false}};
    bool no_revive = false;
    bool no_restore = false;
    for (const Use& use : uses){
        if (use.revive ? no_revive : no_restore){
            continue;
        }
        open_bag_from_overworld(console, context);
        //  REVIVE or MAX REVIVE ("Revives a fainted POKeMON..."); FULL
        //  RESTORE or MAX POTION ("Fully restores the HP...").
        const bool found = use.revive
            ? select_bag_item(rt, BagPocket::ITEMS, {"revives a", "revive"}, {}, "REVIVE")
            : select_bag_item(rt, BagPocket::ITEMS, {"fully restores", "fully restore"}, {"revive"}, "FULL RESTORE");
        if (found){
            use_item_on(rt, use.slot, use.revive ? "REVIVE" : "FULL RESTORE");
        }else if (use.revive){
            no_revive = true;
        }else{
            no_restore = true;
        }
        pbf_mash_button(context, BUTTON_B, 3000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, "bag");
    }
    rt.lead_fainted = false;
}



bool story_escape_rope(StoryRuntime& rt){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log("ESCAPE ROPE: out to the entrance.", COLOR_BLUE);
    for (int attempt = 0; attempt < 2; attempt++){
        open_bag_from_overworld(console, context);
        //  "A long, durable rope. Use it to escape instantly from a cave or a dungeon."
        if (!select_bag_item(rt, BagPocket::ITEMS, {"escape instantly", "durable rope", "or a dungeon"}, {}, "ESCAPE ROPE")){
            pbf_mash_button(context, BUTTON_B, 3000ms);
            context.wait_for_all_requests();
            story_back_to_overworld(rt, "bag");
            console.log("No ESCAPE ROPE in the bag (not enough money at the Poke Mart?): walking out.", COLOR_ORANGE);
            return false;
        }
        //  USE / GIVE / TOSS / CANCEL, the cursor on USE. The bag fades out
        //  (black for a moment), "... used the ESCAPE ROPE." in the field,
        //  which waits for a button (DisplayItemMessageOnField), then the
        //  spin out, black, the spin in. Before build 103 the black looked
        //  for was the bag's (seen once in Silph Co., missed the other
        //  times) and nothing pressed that button: the B's after "no warp"
        //  did, the player went out unseen, and the second try outdoors got
        //  "This isn't the time to use that!". Now the watch for black only
        //  starts in the field, with A for the message.
        press(context, BUTTON_A, 800ms);
        press(context, BUTTON_A, 1500ms);
        context.wait_for_all_requests();
        const std::string said = story_read_dialog(rt);
        if (contains(said, "time to use") || contains(said, "isn't the time") || contains(said, "the time to")){
            pbf_mash_button(context, BUTTON_B, 3000ms);
            context.wait_for_all_requests();
            story_back_to_overworld(rt, "bag");
            if (attempt > 0){
                //  The rope of the try before did take the player out.
                console.log("\"This isn't the time to use that!\": out already, by the ESCAPE ROPE of the try before.", COLOR_ORANGE);
                rt.facing = DPAD_DOWN;
                rt.arrived_by_warp = true;
                return true;
            }
            console.log("\"This isn't the time to use that!\": no ESCAPE ROPE here: walking out.", COLOR_ORANGE);
            return false;
        }
        BlackScreenWatcher black(COLOR_RED);
        const int ret = run_until<ProControllerContext>(
            console, context,
            [](ProControllerContext& context){
                for (int i = 0; i < 8; i++){
                    //  The message, then the spin: A only until the screen
                    //  goes black (the watcher stops these presses).
                    pbf_press_button(context, BUTTON_A, 160ms, 1240ms);
                }
                pbf_wait(context, 3000ms);
            },
            { black }
        );
        if (ret == 0){
            BlackScreenOverWatcher over(COLOR_RED);
            wait_until(console, context, 10000ms, { over });
            pbf_wait(context, 3000ms);
            context.wait_for_all_requests();
            rt.facing = DPAD_DOWN;
            rt.arrived_by_warp = true;
            console.log("Out by the ESCAPE ROPE.", COLOR_BLUE);
            return true;
        }
        console.log("No warp after the ESCAPE ROPE.", COLOR_ORANGE);
        pbf_mash_button(context, BUTTON_B, 3000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, "bag");
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "story_escape_rope(): The ESCAPE ROPE did not take the player out.",
        console
    );
}

void story_use_repel(StoryRuntime& rt){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log("MAX REPEL: no weaker wild Pokemon for 250 steps.", COLOR_BLUE);
    open_bag_from_overworld(console, context);
    //  "Prevents weak wild POKeMON from appearing for 250 steps."
    if (!select_bag_item(rt, BagPocket::ITEMS, {"prevents weak", "weak wild", "from appearing"}, {}, "MAX REPEL")){
        console.log("No REPEL in the bag: walking with the wild Pokemon.", COLOR_ORANGE);
    }else{
        //  USE: "... used the MAX REPEL." (or "...the effects of a REPEL
        //  lingered from earlier.") in the bag; B closes it and the bag.
        press(context, BUTTON_A, 800ms);
        press(context, BUTTON_A, 2000ms);
        press(context, BUTTON_B, 800ms);
    }
    pbf_mash_button(context, BUTTON_B, 3000ms);
    context.wait_for_all_requests();
    story_back_to_overworld(rt, "bag");
}



int story_use_rare_candies(StoryRuntime& rt, int count){
    if (count <= 0 || rt.no_rare_candies){
        return 0;
    }
    StoryMemoryScope memory_scope("rare candies");
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    open_bag_from_overworld(console, context);
    //  "A candy that is packed with energy. It raises the level of a
    //  POKeMON by one."
    if (!select_bag_item(rt, BagPocket::ITEMS, {"raises the level", "level of a", "packed with energy"}, {}, "RARE CANDY")){
        rt.no_rare_candies = true;
        pbf_mash_button(context, BUTTON_B, 3000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, "bag");
        return 0;
    }
    console.log("Rare Candies: up to " + std::to_string(count) + " on party slot 1.", COLOR_BLUE);
    int used = 0;
    while (used < count){
        if (used > 0){
            //  The last one used up moves the cursor on to another item.
            std::string text = read_bag_description(rt);
            if (!contains(text, "raises the level") && !contains(text, "level of a") && !contains(text, "packed with energy")){
                console.log("No Rare Candy left.");
                break;
            }
        }
        //  A: USE / GIVE / TOSS / CANCEL, on USE. A: the party menu (the
        //  cursor where the last one left it: LEFT goes to slot 1).
        press(context, BUTTON_A, 800ms);
        press(context, BUTTON_A, 2500ms);
        context.wait_for_all_requests();
        PartyMenuWatcher party(COLOR_RED);
        if (wait_until(console, context, 3000ms, { party }) != 0){
            console.log("The party menu did not open.", COLOR_ORANGE);
            break;
        }
        dpad(context, DPAD_LEFT, 500ms);
        press(context, BUTTON_A, 1500ms);
        //  "X was elevated to Lv. N.", the stat window (twice), maybe a new
        //  move, then the bag again. A goes through it (and through an
        //  evolution, which B would stop). A move it wants to learn: NO
        //  (the battles choose the moves), and YES to "Stop trying to
        //  teach it?".
        bool back = false;
        for (int i = 0; i < 30; i++){
            BagWatcher bag(COLOR_RED);
            context.wait_for_all_requests();
            if (wait_until(console, context, 1200ms, { bag }) == 0){
                back = true;
                break;
            }
            std::string text = read_story_text(console, PARTY_MESSAGE_BOX, false);
            if (contains(text, "deleted") || contains(text, "replaced") || contains(text, "should a move")){
                console.log("Not learning the new move: " + text);
                press(context, BUTTON_B, 1500ms);
            }else{
                press(context, BUTTON_A, 800ms);
            }
        }
        if (!back){
            console.log("Not back in the bag after a Rare Candy.", COLOR_ORANGE);
            used++;
            break;
        }
        used++;
    }
    console.log("Used " + std::to_string(used) + " Rare " + (used == 1 ? "Candy." : "Candies."), COLOR_BLUE);
    pbf_mash_button(context, BUTTON_B, 3000ms);
    context.wait_for_all_requests();
    story_back_to_overworld(rt, "bag");
    return used;
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



namespace{
//  How much the screen changed where the boulder was and where a push
//  takes it: the tile in front of the player and the one beyond. The
//  player's tile is in the middle of the screen, (112..127, 72..87) at GBA
//  resolution. Mean brightness difference over both tiles and a margin.
double boulder_area_change(
    const AutoStoryScroll::Screen& before, const AutoStoryScroll::Screen& after,
    DpadPosition direction
){
    const int dx = direction == DPAD_LEFT ? -1 : (direction == DPAD_RIGHT ? 1 : 0);
    const int dy = direction == DPAD_UP ? -1 : (direction == DPAD_DOWN ? 1 : 0);
    const int tile = AutoStoryScroll::TILE_PIXELS;
    const int cx = 120;
    const int cy = 80;
    const int x0 = std::max(0, std::min(cx + dx * tile, cx + 2 * dx * tile) - 10);
    const int x1 = std::min(AutoStoryScroll::SCREEN_WIDTH, std::max(cx + dx * tile, cx + 2 * dx * tile) + 10);
    const int y0 = std::max(0, std::min(cy + dy * tile, cy + 2 * dy * tile) - 10);
    const int y1 = std::min(AutoStoryScroll::SCREEN_HEIGHT, std::max(cy + dy * tile, cy + 2 * dy * tile) + 10);
    double sum = 0;
    size_t count = 0;
    for (int y = y0; y < y1; y++){
        for (int x = x0; x < x1; x++){
            sum += std::abs((int)before.at(x, y) - (int)after.at(x, y));
            count++;
        }
    }
    return count == 0 ? 0 : sum / (double)count;
}
const char* push_word(DpadPosition direction){
    switch (direction){
    case DPAD_UP:       return "up";
    case DPAD_DOWN:     return "down";
    case DPAD_LEFT:     return "left";
    case DPAD_RIGHT:    return "right";
    default:            return "none";
    }
}
}

void story_push_boulder(StoryRuntime& rt, DpadPosition direction){
    //  Facing the boulder with Strength on, a short press pushes it. The
    //  player stays. Released long before the push is over, so the player
    //  does not walk after it.
    //  With the video check, each push is looked at:
    //   - The press is now and then missed, like the tap that turns the
    //     player: nothing changed in front of the player. Press again.
    //   - The boulder was pushed already, by the tap that turned the player
    //     or the end of the walk to it (Victory Road, three times in 144
    //     pushes): the press walks the player into the tile it left. The
    //     boulder is then where this push would have put it, and the player
    //     one tile too far: step back, and the route goes on from there.
    //     (Before, the next walk pushed it once more, and two pushes later
    //     it would not move: against a wall, or already on the switch.)
    AutoStoryScroll::Screen before;
    const bool check = story_take_picture(rt, before);
    const int dx = direction == DPAD_LEFT ? -1 : (direction == DPAD_RIGHT ? 1 : 0);
    const int dy = direction == DPAD_UP ? -1 : (direction == DPAD_DOWN ? 1 : 0);
    for (int attempt = 0; attempt < 3; attempt++){
        pbf_press_dpad(rt.context, direction, 80ms, 1400ms);
        rt.context.wait_for_all_requests();
        rt.facing = direction;
        AutoStoryScroll::Screen after;
        if (!check || !story_take_picture(rt, after)){
            return;
        }
        const AutoStoryScroll::TileOffset offset = AutoStoryScroll::tile_offset(before, after, 2);
        const double change = boulder_area_change(before, after, direction);
        rt.console.log(
            std::string("Push ") + push_word(direction) + ": " + offset.summary() +
            ", change in front " + std::to_string(change).substr(0, 4)
        );
        if (offset.clear && offset.x == dx && offset.y == dy){
            rt.console.log(
                "The player walked instead of pushing: the boulder had moved already. Stepping back.",
                COLOR_ORANGE
            );
            story_single_step(rt, opposite(direction));
            rt.facing = opposite(direction);
            return;
        }
        if (offset.clear && (offset.x != 0 || offset.y != 0)){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::string("story_push_boulder(): The player moved while pushing ") + push_word(direction) +
                ". The route does not know where the boulders are now.",
                rt.console
            );
        }
        if (change >= 6){
            return;
        }
        rt.console.log(
            "The boulder did not move (change " + std::to_string(change).substr(0, 4) + "). Pushing " +
            push_word(direction) + " again.",
            COLOR_ORANGE
        );
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::string("story_push_boulder(): The boulder does not move ") + push_word(direction) +
        ". The player may not be in front of it, or Strength is not on.",
        rt.console
    );
}



void story_oscillate(StoryRuntime& rt, DpadPosition direction, int length, Milliseconds max_time, const std::string& what){
    DpadPosition back = opposite(direction);
    WhiteDialogWatcher dialog(COLOR_RED);
    BlackScreenWatcher black(COLOR_RED);
    WallClock deadline = current_time() + max_time;
    //  A line of "length" tiles with a wall at both ends: run along it (B
    //  held, 8 frames a tile), each way a little longer than the line, so
    //  it ends bumping into the wall (no drift). The Safari Zone counts
    //  every tile: about 5 steps a second instead of 2.
    const bool line = length >= 3;
    const Milliseconds leg = Milliseconds((int64_t)((8 + 8 * (length - 1) + 12) * 1000 / 60));
    rt.console.log(
        "Walking back and forth until a scene starts: " + what +
        (line ? " (running along " + std::to_string(length) + " tiles between two walls)" : std::string())
    );
    rt.context.wait_for_all_requests();
    int ret = run_until<ProControllerContext>(
        rt.console, rt.context,
        [&](ProControllerContext& context){
            while (current_time() < deadline){
                for (int i = 0; i < 10; i++){
                    DpadPosition d = (i % 2 == 0) ? direction : back;
                    if (line){
                        pbf_controller_state(context, BUTTON_B, d, {0, 0}, {0, 0}, leg);
                        pbf_wait(context, 50ms);
                    }else{
                        //  Same timing as a single walking step.
                        pbf_press_dpad(context, d, 200ms, 300ms);
                    }
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
    //  A straight line of "length" grass tiles from the first one, toward
    //  "forth". The player walks it from end to end, each way as one held
    //  input: far fewer pauses to watch for a battle than one step at a
    //  time. A battle on the way stops the player somewhere on it: the
    //  picture taken on the first tile tells where (up to "length" tiles
    //  away), and the next run goes on from there.
    //  "run": the walks along it may be run (Running Shoes, a map that
    //  allows it).
    GrassSpot(StoryRuntime& rt, DpadPosition forth, int length, StoryBattleKind kind, const std::string& where, bool run = false)
        : m_rt(rt)
        , m_forth(forth)
        , m_length(std::max(2, std::min(length, 5)))
        , m_kind(kind)
        , m_where(where)
        , m_run(run && rt.running)
    {
        if (m_length > 2){
            rt.console.log(
                where + ": training along " + std::to_string(m_length) + " grass tiles" +
                (m_run ? ", running." : ".")
            );
        }
    }

    //  The route has just brought the player onto the first tile. The first
    //  time, that is where the picture is taken; after that, it is checked.
    void arrived(){
        m_pos = 0;
        m_forward = true;
        if (!m_has_picture){
            m_has_picture = story_take_picture(m_rt, m_picture);
            return;
        }
        to_first_tile();
    }

    //  Make the position on the line match where the player stands. Off the
    //  line, walk back onto it. Returns whether the video told where the
    //  player stands.
    bool check(){
        if (!m_has_picture){
            return false;
        }
        const int fx = forth_x();
        const int fy = forth_y();
        for (int attempt = 0; attempt < 4; attempt++){
            int dx = 0;
            int dy = 0;
            AutoStoryScroll::Screen now;
            if (!story_offset_from(m_rt, m_picture, m_where, dx, dy, now, std::max(3, m_length))){
                return false;   //  The video does not tell: go by the count.
            }
            const int along = dx * fx + dy * fy;
            const int side = dx * fy - dy * fx;
            if (side == 0 && along >= 0 && along < m_length){
                if (along != m_pos){
                    m_rt.console.log(
                        m_where + ": the player is on grass tile " + std::to_string(along + 1) +
                        ", not " + std::to_string(m_pos + 1) + " as counted. Going by the video.",
                        COLOR_ORANGE
                    );
                }
                m_pos = along;
                if (along == 0){
                    //  The latest look of the place (people walking around,
                    //  a sign popping up and going away).
                    m_picture = std::move(now);
                }
                return true;
            }
            //  Off the line: a step back toward the nearest tile on it,
            //  sideways first.
            DpadPosition toward;
            if (side != 0){
                //  "side" is how far the player is to the left of "forth".
                const int sx = fy;
                const int sy = -fx;
                const int bx = side > 0 ? -sx : sx;
                const int by = side > 0 ? -sy : sy;
                toward = bx > 0 ? DPAD_RIGHT : bx < 0 ? DPAD_LEFT : by > 0 ? DPAD_DOWN : DPAD_UP;
            }else{
                toward = along < 0 ? m_forth : opposite(m_forth);
            }
            m_rt.console.log(
                m_where + ": the player is " + std::to_string(dx) + ", " + std::to_string(dy) +
                " tiles from the first grass tile, off the line. A step " + direction_word(toward) + " back toward it.",
                COLOR_ORANGE
            );
            bool moved = true;
            story_checked_step(m_rt, toward, false, false, m_kind, m_where, true, &moved);
            if (!moved){
                //  In the way: the video took the player for somewhere
                //  else (Pokemon Tower 7F, a floor that looks the same
                //  everywhere: "-1, -2 tiles", and the step right ran into
                //  the wall six times).
                m_rt.console.log(
                    m_where + ": the step " + direction_word(toward) +
                    " does not happen, so the player is not where the video says. Going by the count.",
                    COLOR_ORANGE
                );
                return false;
            }
        }
        m_rt.console.log(m_where + ": could not get back onto the grass line. Going on.", COLOR_RED);
        return false;
    }

    //  Onto the first tile, where the routes there and back meet.
    void to_first_tile(){
        for (int attempt = 0; attempt < 4; attempt++){
            check();
            if (m_pos <= 0){
                m_pos = 0;
                m_forward = true;
                return;
            }
            const int moves = m_pos;
            if (moves == 1){
                bool moved = true;
                story_checked_step(m_rt, opposite(m_forth), false, false, m_kind, m_where, true, &moved);
                if (!moved){
                    //  In the way: the player is there already (see step()).
                    m_rt.console.log(
                        m_where + ": the step " + direction_word(opposite(m_forth)) +
                        " to the first grass tile does not happen. Taking the player to be on it already.",
                        COLOR_ORANGE
                    );
                }
                m_pos = 0;
            }else{
                int made = 0;
                bool sure = false;
                story_grass_run(m_rt, opposite(m_forth), moves, m_kind, m_where, true, made, sure, m_run);
                m_pos = std::max(0, m_pos - std::max(0, made));
            }
            m_forward = true;
            if (!m_has_picture){
                m_pos = 0;
                return;
            }
        }
    }

    //  The next run along the line: to the end it is heading for (and turn
    //  around there). A run that does not move at all (something is in the
    //  way) most likely means that the player already stands where it
    //  goes: a count that got it wrong, with no clear picture to tell. Then
    //  the count goes by that. Twice in a row: the player is somewhere else,
    //  and the program stops.
    StoryBattleResult step(StoryBattleKind kind, bool throw_on_loss = true){
        int target = m_forward ? m_length - 1 : 0;
        if (m_pos == target){
            m_forward = !m_forward;
            target = m_forward ? m_length - 1 : 0;
        }
        const int moves = std::abs(target - m_pos);
        const int sign = target > m_pos ? 1 : -1;
        const DpadPosition direction = sign > 0 ? m_forth : opposite(m_forth);
        const uint64_t false_before = m_rt.false_battles;

        bool moved = true;
        bool sure = true;
        StoryBattleResult result;
        if (moves == 1){
            result = story_checked_step(
                m_rt, direction, false, false, kind, m_where, throw_on_loss, &moved
            );
            if (moved){
                m_pos = target;
            }
        }else{
            int made = 0;
            result = story_grass_run(m_rt, direction, moves, kind, m_where, throw_on_loss, made, sure, m_run);
            moved = made > 0 || result != StoryBattleResult::NO_BATTLE;
            if (made > moves){
                //  Past the end of the line (the input lasted too long):
                //  look, and back onto it.
                sure = false;
            }
            m_pos = std::max(0, std::min(m_length - 1, m_pos + sign * std::max(0, made)));
        }
        if (result == StoryBattleResult::LOST){
            return result;      //  At the Pokemon Center.
        }

        if (m_rt.false_battles != false_before){
            //  The screen faded but no battle came: the player may have
            //  stepped onto a warp off the line (the Mansion's door, next
            //  to the training spot: out and in again for 20 minutes).
            //  Look where the player is.
            if (check()){
                m_false_in_row = 0;
            }else if (++m_false_in_row >= 2){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "The player is no longer on the grass of " + m_where +
                    ": the screen changed twice without a battle (a warp?).",
                    m_rt.console
                );
            }
        }else if (result != StoryBattleResult::NO_BATTLE){
            m_false_in_row = 0;
        }else if (!sure){
            //  The video did not count the run well: look.
            check();
        }
        if (moved){
            m_not_moved = 0;
            return result;
        }
        if (++m_not_moved >= 2){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "The player does not move either way on the grass: " + m_where +
                ". Something is in the way, or the player is not where the route expects.",
                m_rt.console
            );
        }
        if (moves > 1 && check()){
            m_rt.console.log(
                m_where + ": the run " + direction_word(direction) + " did not move. Going on from grass tile " +
                std::to_string(m_pos + 1) + ", as the video says.",
                COLOR_ORANGE
            );
            return result;
        }
        m_rt.console.log(
            m_where + ": the way " + direction_word(direction) + " does not happen. Taking the player to be at that end already.",
            COLOR_ORANGE
        );
        m_pos = target;
        return result;
    }

private:
    int forth_x() const{
        return m_forth == DPAD_LEFT ? -1 : (m_forth == DPAD_RIGHT ? 1 : 0);
    }
    int forth_y() const{
        return m_forth == DPAD_UP ? -1 : (m_forth == DPAD_DOWN ? 1 : 0);
    }

    StoryRuntime& m_rt;
    DpadPosition m_forth;
    int m_length;
    StoryBattleKind m_kind;
    std::string m_where;
    bool m_run;
    bool m_has_picture = false;
    AutoStoryScroll::Screen m_picture;
    int m_pos = 0;              //  0 = the first tile, m_length - 1 = the far end
    bool m_forward = true;      //  heading for the far end
    int m_not_moved = 0;
    int m_false_in_row = 0;
};

}



namespace{

//  The training itself: the Pokemon in party slot 1 fights. "member": it
//  is a team member put in front for this (see story_grind()).
void grind_front(
    StoryRuntime& rt, const StoryScript& script, const StoryStep& step,
    int extra_levels, bool member, int share_position = 0
){
    ConsoleHandle& console = rt.console;
    const int target = step.p0 + extra_levels;
    const int heal_every = std::max(1, step.p1);
    const DpadPosition forth = step.dir;
    const int length = std::max(2, (int)(step.p4 & 0xff));
    const bool candies = (step.p4 & 0x100) != 0;
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
    if (level >= target){
        //  The step starts in front of the nurse: nothing to do. (After a
        //  reset from the save made once a team member got there: it is
        //  put back in its place.)
        console.log(where + ": level " + std::to_string(level) + " reached already.", COLOR_BLUE);
        return;
    }
    if (candies && level > 0){
        //  The Rare Candies picked up on the way: a level each, where a
        //  level costs the most battles.
        if (story_use_rare_candies(rt, target - level) > 0){
            int now = story_read_lead_level(rt);
            if (now > 0){
                level = now;
            }
            if (level >= target){
                console.log(where + ": level " + std::to_string(level) + " reached with Rare Candies.", COLOR_BLUE);
                return;
            }
        }
    }
    walk(to_grass);
    //  The last walk to the grass is on the map of the grass.
    const bool run = !to_grass.empty() && to_grass.back() >= 0 && (size_t)to_grass.back() < script.leg_count &&
        script.legs[(size_t)to_grass.back()].run;
    GrassSpot spot(rt, forth, length, StoryBattleKind::WILD_FIGHT, where, run);
    spot.arrived();

    //  If the level cannot be read, stop after a fixed number of battles.
    const uint64_t battle_cap = level < 0 ? 40 : 400;
    uint64_t battles = 0;
    uint64_t steps_without_battle = 0;
    int since_heal = 0;
    bool done = level >= target;

    //  A member below the strongest wild Pokemon here only takes part: the
    //  lead comes in and fights, and the experience is shared. From that
    //  level on it fights alone.
    const int solo_level = member ? step.p3 : 0;
    auto update_share = [&](){
        const bool share = share_position > 0 && level >= 0 && level < solo_level;
        if (share != (rt.battle.share_position > 0)){
            if (share){
                console.log(
                    where + ": level " + std::to_string(level) + ", the wild Pokemon here go up to " +
                    std::to_string(solo_level) + ". The lead fights, the experience is shared.",
                    COLOR_BLUE
                );
            }else{
                console.log(where + ": level " + std::to_string(level) + ". Fighting alone now.", COLOR_BLUE);
                rt.memory.no_pp = 0;
                for (int& m : rt.memory.moves){
                    m = -1;
                }
            }
        }
        rt.battle.share_position = share ? share_position : 0;
        //  Alone, but not against a foe it needs many turns for.
        rt.battle.slow_share_position = !share && share_position > 0 ? share_position : 0;
        rt.battle.lead_level = level > 0 ? level : 0;
    };

    while (!done){
        update_share();
        StoryBattleResult result = spot.step(StoryBattleKind::WILD_FIGHT, false);
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
        spot.check();

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

        //  Heal once the one that fights is below half its HP (its HP bar
        //  is no longer green). Without a reading of the bar, every few
        //  battles; with one, still now and then (the PP).
        const double hp = rt.memory.player_hp;
        const bool hp_known = hp >= 0;
        console.log(
            where + ": HP " + (hp_known ? std::to_string((int)(hp * 100 + 0.5)) + "%" : std::string("not seen")) +
            ", " + std::to_string(since_heal) + " battles since healing."
        );
        if (rt.lead_fainted ||
            (hp_known && hp < 0.5) ||
            since_heal >= (hp_known ? heal_every * 4 : heal_every)
        ){
            spot.to_first_tile();
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
                rt.battle.share_position = 0;
                rt.battle.slow_share_position = 0;
                console.log(where + ": level " + std::to_string(level) + " reached.", COLOR_BLUE);
                if (rt.save_after_heal){
                    //  Saved right here: after a reset, go on with the next
                    //  step (a team member: this step again, to put the
                    //  party back in order).
                    story_saved(rt, member ? rt.step_index : rt.step_index + 1);
                }
                return;     //  Already healed at the nurse.
            }
            walk(to_grass_again);
            spot.arrived();
        }
    }

    rt.battle.share_position = 0;
    rt.battle.slow_share_position = 0;
    console.log(where + ": level " + std::to_string(level) + " reached.", COLOR_BLUE);
    spot.to_first_tile();
    walk(to_nurse);
    story_heal(rt, where, member);
}

const AutoStoryDex::SpeciesInfo* dex_species(const std::string& slug){
    for (const AutoStoryDex::SpeciesInfo& info : AutoStoryDex::species()){
        if (slug == info.slug){
            return &info;
        }
    }
    return nullptr;
}

}   //  namespace


void story_grind(
    StoryRuntime& rt, const StoryScript& script, const StoryStep& step,
    int extra_levels
){
    //  A team member (step.p2 = its party slot, step.text = its evolution
    //  line) trains in front: swapped with slot 1, and back afterwards.
    //  Saves during the training have it in front: after a reset the step
    //  starts again and finds it there.
    const int slot = step.p2 > 1 ? step.p2 : 0;
    if (slot == 0){
        //  The lead trains: make sure it is the starter.
        story_check_lead(rt);
        grind_front(rt, script, step, extra_levels, false);
        return;
    }
    ConsoleHandle& console = rt.console;
    const std::set<std::string> line = story_species_list(step.text);
    story_fix_party_order(rt, false);
    std::string front;
    for (int attempt = 0; attempt < 3 && front.empty(); attempt++){
        story_read_slot(rt, 1, &front);
    }
    if (line.count(front) == 0){
        story_swap_party(rt, 1, slot);
        story_read_slot(rt, 1, &front);
        if (!front.empty() && line.count(front) == 0){
            //  Not the member: maybe a shiny is in its slot (caught before
            //  the program was started this time). Back, sort the party,
            //  and again.
            console.log("Party slot " + std::to_string(slot) + " was " + front + ". Sorting the party.", COLOR_ORANGE);
            story_swap_party(rt, 1, slot);
            story_fix_party_order(rt, true);
            story_swap_party(rt, 1, slot);
            front.clear();
            story_read_slot(rt, 1, &front);
        }
        if (!front.empty() && line.count(front) == 0){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_grind(): Party slot " + std::to_string(slot) + " (" + step.text + ") did not come to the front: " + front,
                console
            );
        }
    }
    console.log("Training team member " + (front.empty() ? std::string(step.text) : front) + " (party slot " + std::to_string(slot) + ").", COLOR_BLUE);

    //  Its types count for the moves.
    const AutoStoryDex::SpeciesInfo* info = dex_species(front);
    if (info != nullptr){
        rt.battle.lead_type1 = info->type1;
        rt.battle.lead_type2 = info->type2;
    }
    //  Its moves are valued with its stats (its level: grind_front()).
    rt.battle.lead_species = info;
    rt.memory.no_pp = 0;
    for (int& m : rt.memory.moves){
        m = -1;
    }
    //  If it is too weak for the wild Pokemon here: the lead, now in its
    //  slot, comes in (shared experience).
    rt.battle.share_type1 = rt.lead_type1;
    rt.battle.share_type2 = rt.lead_type2;
    rt.battle.share_members = line;
    rt.memory.member_moves_known = false;

    grind_front(rt, script, step, extra_levels, true, slot - 1);
    rt.battle.share_position = 0;
    rt.battle.slow_share_position = 0;
    rt.battle.lead_species = nullptr;
    rt.battle.lead_level = 0;

    rt.battle.lead_type1 = rt.lead_type1;
    rt.battle.lead_type2 = rt.lead_type2;
    rt.memory.no_pp = 0;
    for (int& m : rt.memory.moves){
        m = -1;
    }
    //  Back in order. (A member evolves while training: compare the line.)
    story_swap_party(rt, 1, slot);
    story_read_slot(rt, 1, &front);
    if (!front.empty() && line.count(front) != 0){
        console.log("The party is still out of order. Swapping again.", COLOR_ORANGE);
        story_swap_party(rt, 1, slot);
    }
    if (rt.save_after_heal){
        story_save(rt, rt.has_dex);
        story_saved(rt, rt.step_index + 1);
    }
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
    GrassSpot spot(rt, forth, std::max(2, (int)(step.p4 & 0xff)), StoryBattleKind::WILD_RUN, step.label);
    spot.arrived();
    bool caught = false;
    WallClock deadline = current_time() + std::chrono::minutes(30);
    while (current_time() < deadline){
        StoryBattleResult result = spot.step(StoryBattleKind::CATCH);
        if (result == StoryBattleResult::CAUGHT){
            caught = true;
            break;
        }
        if (result != StoryBattleResult::NO_BATTLE){
            spot.check();
        }
    }
    rt.battle.catch_species.clear();
    spot.to_first_tile();
    rt.battle = saved;
    if (!caught){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::string("story_catch(): Nothing caught in 30 minutes: ") + step.text,
            console
        );
    }
}



namespace{
std::string species_names(const std::set<std::string>& species){
    std::string out;
    for (const std::string& slug : species){
        out += (out.empty() ? "" : ", ") + slug;
    }
    return out.empty() ? std::string("none") : out;
}
}

void story_roam(StoryRuntime& rt, const StoryScript& script, const StoryStep& step){
    ConsoleHandle& console = rt.console;
    const std::string label = step.label;
    const std::set<std::string> roamers = story_species_list(step.text);
    for (const std::string& slug : roamers){
        if (rt.caught.count(slug) != 0){
            console.log(label + ": " + slug + " was caught already.", COLOR_BLUE);
            return;
        }
    }
    const int per_cycle = std::max(1, (int)step.p0);
    const std::vector<int32_t> back_to_grass = script.leg_list(step.legs_a);
    const std::vector<int32_t> to_door = script.leg_list(step.legs_b);
    console.log(
        label + ": looking for the roaming " + species_names(roamers) + " (MASTER BALL). In and out of the "
        "building every " + std::to_string(per_cycle) + " battles: it moves to another route at each warp.",
        COLOR_BLUE
    );

    StoryBattleSettings saved = rt.battle;
    rt.battle.roamer_species = roamers;
    rt.battle.catch_species.clear();
    rt.battle.dex_species.clear();
    GrassSpot spot(rt, step.dir, std::max(2, (int)(step.p4 & 0xff)), StoryBattleKind::CATCH, label);
    spot.arrived();
    const WallClock start = current_time();
    const WallClock deadline = start + std::chrono::minutes(std::max(1, (int)step.p1));
    int battles = 0;
    int runs = 0;
    int cycles = 0;
    uint64_t total_battles = 0;
    bool caught = false;
    auto is_caught = [&](){
        for (const std::string& slug : roamers){
            if (rt.caught.count(slug) != 0){
                return true;
            }
        }
        return false;
    };
    try{
        while (current_time() < deadline){
            StoryBattleResult result = spot.step(StoryBattleKind::CATCH);
            runs++;
            if (is_caught()){
                caught = true;
                break;
            }
            if (result != StoryBattleResult::NO_BATTLE){
                battles++;
                total_battles++;
                spot.check();
            }
            if (battles >= per_cycle || runs >= 40){
                spot.to_first_tile();
                for (int32_t leg : to_door){
                    walk_leg(rt, script, leg);
                }
                for (int32_t leg : back_to_grass){
                    walk_leg(rt, script, leg);
                }
                spot.arrived();
                battles = 0;
                runs = 0;
                cycles++;
                if (cycles % 10 == 0){
                    const int minutes = (int)std::chrono::duration_cast<std::chrono::minutes>(current_time() - start).count();
                    console.log(
                        label + ": not met yet: " + std::to_string(total_battles) + " battles, " +
                        std::to_string(cycles) + " times in and out, " + std::to_string(minutes) + " minutes.",
                        COLOR_BLUE
                    );
                }
            }
        }
    }catch (...){
        rt.battle = saved;
        throw;
    }
    rt.battle = saved;
    if (!caught){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_roam(): The roaming Pokemon was not caught in " + std::to_string(step.p1) + " minutes: " + label,
            console
        );
    }
    console.log(label + ": caught! (" + std::to_string(total_battles) + " battles)", COLOR_BLUE);
    spot.to_first_tile();
}

void story_dex(StoryRuntime& rt, const StoryStep& step){
    ConsoleHandle& console = rt.console;
    const bool safari = step.p2 > 0 && (step.p2 & 1) != 0;
    //  For the POKeDEX table: what it still wants of the list, in its game.
    const bool collect = step.p2 > 0 && (step.p2 & 2) != 0 && rt.collection != nullptr;
    const std::string label = step.label;
    std::set<std::string> listed = story_species_list(step.text);
    std::set<std::string> wanted;
    std::set<std::string> extra;
    std::string counts;
    if (collect){
        wanted = rt.collection->wanted(listed);
        std::set<std::string> in_game;
        for (const std::string& slug : listed){
            if (rt.collection->needed(slug) > 0){
                in_game.insert(slug);
            }
        }
        listed = in_game;
        for (const std::string& slug : wanted){
            if (rt.collection->wants_more_than_one(slug)){
                extra.insert(slug);
            }
            counts += (counts.empty() ? "" : ", ") + slug + " " +
                std::to_string(rt.collection->caught(slug)) + "/" + std::to_string(rt.collection->needed(slug));
        }
    }else{
        for (const std::string& slug : listed){
            if (rt.caught.count(slug) == 0){
                wanted.insert(slug);
            }
        }
    }
    //  "Shiny only": only shiny ones of the wanted species, for a while at
    //  each place (one wild Pokemon in 8192 is shiny): no limit of battles
    //  in a row without a catch.
    const bool shiny_only = collect && rt.collect_shiny;
    const int idle_limit = shiny_only ? 1000000 : std::max(1, (int)step.p0);
    const int minutes_limit = shiny_only ? std::max(1, rt.collect_shiny_minutes) : std::max(1, (int)step.p1);
    if (shiny_only && !rt.shiny_watch){
        throw UserSetupError(console, "\"Shiny only\" needs the shiny sound: the program could not listen to the game.");
    }
    console.log(
        label + ": for the POKeDEX, catching " + (shiny_only ? "shiny " : "") + species_names(wanted) +
        (collect ? " (" + rt.collection->game_name() + ", the table: " + counts + ")" : std::string()) +
        (safari ? " (the Safari Game, until it is over)." :
            shiny_only ? " (all others are run from; at most " + std::to_string(minutes_limit) + " minutes)." :
            " (until " + std::to_string(idle_limit) + " battles in a row bring none, at most " +
            std::to_string(minutes_limit) + " minutes)."),
        COLOR_BLUE
    );
    if (wanted.empty() && !safari){
        console.log(label + ": all of them were caught already.", COLOR_BLUE);
        return;
    }
    //  Enough for what the POKeDEX is caught for (the National POKeDEX),
    //  as the game counts them: nothing to catch here.
    const int game_count = rt.dex_count_at_load >= 0
        ? rt.dex_count_at_load + (int)rt.dex_new_since_load.size()
        : -1;
    if (!safari && step.p3 > 0 && game_count >= step.p3){
        console.log(
            label + ": " + std::to_string(game_count) + " species caught as the game counts them, " +
            std::to_string(step.p3) + " are needed: nothing caught here.",
            COLOR_BLUE
        );
        return;
    }

    StoryBattleSettings saved = rt.battle;
    rt.battle.dex_species = wanted;
    rt.battle.dex_all = listed;
    rt.battle.dex_extra = extra;
    rt.battle.dex_shiny_only = shiny_only;
    rt.collecting = collect;
    rt.battle.catch_species.clear();
    rt.in_safari = safari;
    rt.safari_over = false;
    rt.safari_balls_used = 0;
    rt.no_balls = false;
    const uint64_t new_before = rt.dex_new;
    const uint64_t marked_before = rt.dex_owned_by_mark;

    GrassSpot spot(rt, step.dir, std::max(2, (int)(step.p4 & 0xff)), StoryBattleKind::CATCH, label);
    spot.arrived();
    int idle = 0;
    int battles = 0;
    size_t caught_here = 0;
    const WallClock start = current_time();
    const WallClock deadline = start + (safari ? std::chrono::minutes(30) : std::chrono::minutes(minutes_limit));
    try{
        while (current_time() < deadline && !rt.safari_over){
            if (!safari && (rt.battle.dex_species.empty() || idle >= idle_limit || rt.no_balls)){
                break;
            }
            if (!safari && step.p3 > 0 && rt.dex_count_at_load >= 0 &&
                rt.dex_count_at_load + (int)rt.dex_new_since_load.size() >= step.p3
            ){
                console.log(label + ": " + std::to_string(step.p3) + " species caught as the game counts them: enough.", COLOR_BLUE);
                break;
            }
            const size_t left = rt.battle.dex_species.size();
            const uint64_t catches_before = rt.dex_catches;
            StoryBattleResult result = spot.step(StoryBattleKind::CATCH);
            if (rt.safari_over){
                caught_here += (size_t)(rt.dex_catches - catches_before);
                break;
            }
            if (result == StoryBattleResult::NO_BATTLE){
                continue;
            }
            battles++;
            if (rt.dex_catches > catches_before){
                //  On the list (also an extra one to evolve), or not on it
                //  but new to the game (no ball mark).
                caught_here += (size_t)(rt.dex_catches - catches_before);
                idle = 0;
            }else if (rt.battle.dex_species.size() < left){
                idle = 0;           //  Found caught before (the ball mark).
            }else{
                idle++;
            }
            if (shiny_only && rt.warned_no_sound){
                throw UserSetupError(
                    console,
                    "\"Shiny only\" needs the game's sound, and none comes in from the capture card: set its "
                    "audio input and turn the game's sound on."
                );
            }
            if (collect && !safari && rt.shiny_unsaved){
                //  Not to be lost to a reset: saved at the nurse first. The
                //  next visit of the place goes on.
                console.log(label + ": a shiny Pokemon is caught: back to the nurse to save it.", COLOR_BLUE);
                break;
            }
            if (collect && !safari && !rt.battle.dex_species.empty() &&
                (rt.lead_fainted || (rt.memory.player_hp >= 0 && rt.memory.player_hp < 0.35))
            ){
                //  Catching at full HP lets the wild Pokemon hit back (Cerulean
                //  Cave: level 46 to 67). The visit ends at the nurse; the next
                //  visit of the place goes on with what is still wanted.
                console.log(
                    label + ": the lead is " + (rt.lead_fainted ? std::string("fainted") :
                        "at " + std::to_string((int)(rt.memory.player_hp * 100 + 0.5)) + "% HP") +
                    ": back to the nurse.",
                    COLOR_ORANGE
                );
                break;
            }
            spot.check();
        }
    }catch (...){
        rt.in_safari = false;
        rt.collecting = false;
        rt.battle = saved;
        throw;
    }
    const std::set<std::string> left = rt.battle.dex_species;
    rt.in_safari = false;
    rt.collecting = false;
    rt.battle = saved;
    const int minutes = (int)std::chrono::duration_cast<std::chrono::minutes>(current_time() - start).count();
    const uint64_t marked = rt.dex_owned_by_mark - marked_before;
    console.log(
        label + ": " + std::to_string(caught_here) + " caught (" +
        std::to_string(rt.dex_new - new_before) + " new to the POKeDEX)" +
        (marked > 0 ? ", " + std::to_string(marked) + " found caught before (the ball mark)" : std::string()) +
        " in " + std::to_string(battles) + " battles, " + std::to_string(minutes) +
        " minutes. Not caught: " + species_names(left) + "." +
        (rt.no_balls ? " No balls left." : ""),
        COLOR_BLUE
    );
    if (safari){
        if (!rt.safari_over){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_dex(): The Safari Game did not end in 30 minutes: " + label,
                console
            );
        }
        //  The PA's text and the walk out to the gate: the next steps.
        return;
    }
    spot.to_first_tile();
}



void story_seagallop(StoryRuntime& rt, int row, const std::string& what){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    //  The menus, in the game's screen: the tickets' (MULTICHOICE_SEVII_
    //  NAVEL_BIRTH, window at tile 13,5: SEVII ISLANDS, NAVEL ROCK, BIRTH
    //  ISLAND, EXIT), and the Sevii Islands list (DrawSeagallopDestination
    //  Menu, window at tile 17,0: ONE ... FOUR ISLAND, OTHER, EXIT).
    const ImageFloatBox TICKET_MENU(0.45, 0.27, 0.52, 0.45);
    const ImageFloatBox ISLAND_MENU(0.58, 0.02, 0.40, 0.58);
    auto has = [](const std::string& text, const char* part){
        return text.find(part) != std::string::npos;
    };
    console.log("The Seagallop from Vermilion City: row " + std::to_string(row) + " of the Sevii Islands list (" + what + ").");
    bool picked = false;
    int quiet = 0;
    const WallClock deadline = current_time() + 90s;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const std::string tickets = read_story_text(console, TICKET_MENU, false);
        if (!picked && has(tickets, "sevii")){
            console.log("The tickets' menu: SEVII ISLANDS.", COLOR_BLUE);
            pbf_press_button(context, BUTTON_A, 160ms, 1340ms);
            quiet = 0;
            continue;
        }
        const std::string islands = read_story_text(console, ISLAND_MENU, false);
        if (!picked && has(islands, "island") && (has(islands, "other") || has(islands, "exit"))){
            console.log("The Sevii Islands list: row " + std::to_string(row) + ".", COLOR_BLUE);
            for (int i = 0; i < row; i++){
                pbf_press_dpad(context, DPAD_DOWN, 160ms, 340ms);
            }
            pbf_press_button(context, BUTTON_A, 160ms, 1340ms);
            picked = true;
            quiet = 0;
            continue;
        }
        const std::string text = story_read_dialog(rt);
        if (!text.empty()){
            //  The sailor's pages (the tickets, "Would you like to board a
            //  SEAGALLOP ferry?", "...departing immediately."). A menu not
            //  read: A picks its first row (SEVII ISLANDS, ONE ISLAND),
            //  right for row 0 only.
            if (!picked && row != 0 && has(text, "board")){
                pbf_wait(context, 500ms);
                if (++quiet < 8){
                    continue;
                }
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "story_seagallop(): The Seagallop's destination list was not read: " + what,
                    console
                );
            }
            if (has(text, "depart") || has(text, "in order")){
                picked = true;
            }
            pbf_press_button(context, BUTTON_A, 160ms, 1040ms);
            quiet = 0;
            continue;
        }
        //  No text: the screen going black for the ferry (after the pick),
        //  or a page still printing.
        if (picked && ++quiet >= 2){
            console.log("The Seagallop leaves.", COLOR_BLUE);
            return;
        }
        if (!picked && ++quiet >= 20){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_seagallop(): The sailor's text did not come up: " + what,
                console
            );
        }
        pbf_wait(context, 500ms);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "story_seagallop(): Not on the Seagallop after 90 seconds: " + what,
        console
    );
}



}
}
}

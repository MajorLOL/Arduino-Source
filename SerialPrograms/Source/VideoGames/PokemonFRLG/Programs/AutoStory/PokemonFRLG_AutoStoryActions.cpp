/*  Pokemon FRLG Autostory Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Map/PokemonFRLG_MapDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_BagDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_StartMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_StatsReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_AutoStoryMemory.h"
#include "PokemonFRLG_AutoStoryCoins.h"
#include "PokemonFRLG_AutoStoryCollection.h"
#include "PokemonFRLG_AutoStoryFerryTalk.h"
#include "PokemonFRLG_AutoStoryFishing.h"
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

std::string upper_name(const std::string& text){
    std::string ret;
    for (char c : text){
        ret += (char)std::toupper((unsigned char)c);
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
//  "here_first": the item may be under the cursor already (each pocket
//  keeps its place from the last time the bag was open): look there before
//  going through the list from the top. (The VS SEEKER, the newest of some
//  twenty KEY ITEMS and used again and again: 40 seconds from the top each
//  time on 15 November 2026.)
//  "as_opened": the bag may even have opened on it (it opens on the pocket
//  and the place of the last time): look before moving at all. (A fishing
//  rod, used a hundred times in a row.)
bool select_bag_item(
    StoryRuntime& rt, BagPocket pocket,
    const std::vector<const char*>& words,
    const std::vector<const char*>& avoid,
    const std::string& what,
    bool here_first = false,
    bool as_opened = false
){
    ProControllerContext& context = rt.context;
    if (as_opened){
        pbf_wait(context, 300ms);
        context.wait_for_all_requests();
        const std::string here = read_bag_description(rt);
        bool match = false;
        for (const char* word : words){
            match |= contains(here, word);
        }
        for (const char* word : avoid){
            match &= !contains(here, word);
        }
        if (match){
            return true;
        }
    }
    //  The pockets do not wrap around: LEFT x3 is the first pocket.
    for (int i = 0; i < 3; i++){
        dpad(context, DPAD_LEFT, 500ms);
    }
    for (int i = 0; i < (int)pocket; i++){
        dpad(context, DPAD_RIGHT, 500ms);
    }
    if (here_first){
        pbf_wait(context, 500ms);
        context.wait_for_all_requests();
        const std::string here = read_bag_description(rt);
        bool match = false;
        for (const char* word : words){
            match |= contains(here, word);
        }
        for (const char* word : avoid){
            match &= !contains(here, word);
        }
        if (match){
            rt.console.log("Found in the bag, under the cursor: " + what);
            return true;
        }
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
//  True: used (the bag is back). False: it had no effect there (the item is
//  still in the bag), or what happened is not known.
bool use_item_on(StoryRuntime& rt, int party_slot, const std::string& what){
    ProControllerContext& context = rt.context;
    rt.console.log("Using " + what + " on party slot " + std::to_string(party_slot) + ".");
    //  A: USE / GIVE / TOSS / CANCEL, cursor on USE. A: the party menu.
    press(context, BUTTON_A, 800ms);
    press(context, BUTTON_A, 2500ms);
    context.wait_for_all_requests();
    PartyMenuWatcher party(COLOR_RED);
    if (wait_until(rt.console, context, 3000ms, { party }) != 0){
        rt.console.log("The party menu did not open.", COLOR_RED);
        return false;
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
            return true;
        }
        if (ret == 1){
            //  No effect: back to the party menu. B goes back to the bag.
            rt.console.log("No effect on party slot " + std::to_string(party_slot) + ": " + what + " stays in the bag.");
            press(context, BUTTON_B, 1500ms);
            return false;
        }
    }
    return false;
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
        std::string nature;
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
            nature = stats.nature;
        }
        story_back_to_overworld(rt, "summary");
        if (level > 0){
            console.log(
                "Party slot " + std::to_string(slot) + ": " +
                (species != nullptr ? *species + " " : std::string()) + "level " + std::to_string(level) +
                (nature.empty() ? std::string() : ", nature " + nature)
            );
            return level;
        }
        console.log("Could not read party slot " + std::to_string(slot) + ".", COLOR_ORANGE);
    }
    return -1;
}


bool story_read_stats(StoryRuntime& rt, int slot, StoryStats& out){
    StoryMemoryScope memory_scope("party menu");
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    for (int attempt = 0; attempt < 3; attempt++){
        out = StoryStats();
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
        bool agree = false;
        if (wait_until(console, context, 6000ms, { summary }) == 0){
            pbf_wait(context, 800ms);
            context.wait_for_all_requests();
            StatsReader reader(COLOR_RED);
            PokemonFRLG_Stats first;
            reader.read_page1(console.logger(), Language::English, console.video().snapshot(), first);
            //  Page 2 (the stats): RIGHT until its detector sees it.
            SummaryPage2Watcher page_two(COLOR_RED);
            const int ret = run_until<ProControllerContext>(
                console, context,
                [](ProControllerContext& context){
                    for (int i = 0; i < 5; i++){
                        pbf_press_dpad(context, DPAD_RIGHT, 200ms, 1800ms);
                    }
                },
                { page_two }
            );
            if (ret == 0){
                //  Twice, 500 ms apart: the same numbers, or not trusted.
                PokemonFRLG_Stats one;
                PokemonFRLG_Stats two;
                pbf_wait(context, 700ms);
                context.wait_for_all_requests();
                reader.read_page2(console.logger(), Language::English, console.video().snapshot(), one);
                pbf_wait(context, 500ms);
                context.wait_for_all_requests();
                reader.read_page2(console.logger(), Language::English, console.video().snapshot(), two);
                agree = one.attack.has_value() && one.defense.has_value() &&
                    two.attack.has_value() && two.defense.has_value() &&
                    *one.attack == *two.attack && *one.defense == *two.defense;
                if (agree){
                    out.attack = (int)*one.attack;
                    out.defense = (int)*one.defense;
                }
                if (first.level.has_value()){
                    out.level = (int)first.level.value();
                }
                out.species = first.name;
                out.nature = first.nature;
            }
        }
        story_back_to_overworld(rt, "summary");
        if (agree && out.level > 0 && !out.species.empty() && !out.nature.empty()){
            console.log(
                "Party slot " + std::to_string(slot) + ": " + out.species + " level " + std::to_string(out.level) +
                ", nature " + out.nature + ", Attack " + std::to_string(out.attack) +
                ", Defense " + std::to_string(out.defense)
            );
            return true;
        }
        console.log("Could not read the stats of party slot " + std::to_string(slot) + ".", COLOR_ORANGE);
    }
    return false;
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



void story_buy(StoryRuntime& rt, int index, int quantity, const std::string& what, int kind, bool up_to){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    //  Maybe healing items: look in the bag again when one is needed.
    rt.memory.no_heal_items = false;
    rt.no_revives = false;
    rt.no_full_restores = false;
    int* have = kind == 1 ? &rt.bag_revives : kind == 2 ? &rt.bag_restores : nullptr;
    if (have != nullptr && up_to && quantity > 0){
        const std::string name = kind == 1 ? "REVIVE" : "FULL RESTORE or MAX POTION";
        if (*have < 0){
            //  Not known how many the bag has (the program was started in
            //  between, or they are from before it counted): is there one?
            console.log("How many of " + name + " the bag has is not known: a look.");
            open_bag_from_overworld(console, context);
            const bool found = kind == 1
                ? select_bag_item(rt, BagPocket::ITEMS, {"revives a", "revive"}, {"fully"}, "REVIVE")
                : select_bag_item(rt, BagPocket::ITEMS, {"fully restores", "fully restore"}, {"revive"}, "FULL RESTORE");
            pbf_mash_button(context, BUTTON_B, 3000ms);
            context.wait_for_all_requests();
            story_back_to_overworld(rt, "bag");
            //  One or more: taken for half of what is wanted.
            *have = found ? (quantity + 1) / 2 : 0;
        }
        const int missing = quantity - *have;
        if (missing <= 0){
            console.log(
                "The bag has " + std::to_string(*have) + " of " + name + " (as far as the program knows), " +
                std::to_string(quantity) + " are wanted: none bought (" + what + ").",
                COLOR_BLUE
            );
            return;
        }
        console.log(
            "The bag has " + std::to_string(*have) + " of " + name + " (as far as the program knows), " +
            std::to_string(quantity) + " are wanted: " + std::to_string(missing) + " more.",
            COLOR_BLUE
        );
        quantity = missing;
    }
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
    if (have != nullptr && quantity > 0){
        //  As if the money was enough for all of them: a look in the bag
        //  that finds none sets it right.
        *have = std::max(0, *have) + quantity;
    }
}



void story_take_mail(StoryRuntime& rt, int party_slot, const std::string& label){
    StoryMemoryScope memory_scope("party menu");
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log(
        label + ": the Pokemon in party slot " + std::to_string(party_slot) + " holds a MAIL (the trade gives it "
        "with one): the MAIL goes to the PC's mailbox, since the PC's box view does not take a Pokemon that holds one."
    );
    //  The message of the party menu, as lower case text, after a moment for
    //  it to be printed.
    auto message = [&]() -> std::string{
        pbf_wait(context, 1300ms);
        context.wait_for_all_requests();
        const std::string text = lower(read_story_text(console, PARTY_MESSAGE_BOX, false));
        console.log(label + ": the party menu says: \"" + text + "\"");
        return text;
    };
    //  Not what the game shows at this point: nothing more is pressed that
    //  could do something else (the cursor is not where it was meant to
    //  be); out of the menus, and the error resets the game.
    auto give_up = [&](const std::string& why){
        pbf_mash_button(context, BUTTON_B, 3000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, "party menu");
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_take_mail(): " + why + ": " + label,
            console
        );
    };
    auto says = [&](const std::string& text, std::initializer_list<const char*> words){
        for (const char* word : words){
            if (contains(text, word)){
                return true;
            }
        }
        return false;
    };

    open_party_menu_from_overworld(console, context);
    //  LEFT from any slot on the right goes to slot 1.
    dpad(context, DPAD_LEFT, 500ms);
    for (int i = 1; i < party_slot; i++){
        dpad(context, DPAD_DOWN);
    }
    //  The actions: SUMMARY, (field moves), SWITCH, MAIL, CANCEL (with a
    //  MAIL held, ITEM is MAIL: SetPartyMonFieldSelectionActions). Up from
    //  the top wraps around to CANCEL, then MAIL, as story_swap_party() goes
    //  to SWITCH.
    press(context, BUTTON_A, 800ms);
    dpad(context, DPAD_UP, 300ms);
    dpad(context, DPAD_UP, 300ms);
    press(context, BUTTON_A, 900ms);
    //  "Do what with the MAIL?": READ, TAKE MAIL, CANCEL (the cursor on READ).
    if (!says(message(), {"mail", "mai1", "mall"})){
        give_up("the party menu does not ask what to do with a MAIL (the Pokemon holds none, or the cursor was elsewhere)");
    }
    dpad(context, DPAD_DOWN, 300ms);
    press(context, BUTTON_A, 900ms);
    //  "Send the removed MAIL to your PC?" YES / NO, the cursor on YES.
    if (!says(message(), {"send", "remov", "pc"})){
        give_up("the party menu does not ask whether the MAIL goes to the PC");
    }
    press(context, BUTTON_A, 900ms);
    //  "The MAIL was sent to your PC." or "Your PC's MAILBOX is full."
    const std::string result = message();
    if (says(result, {"full"})){
        give_up("the PC's MAILBOX is full (the MAIL cannot go there: take one out of it by hand)");
    }
    if (!says(result, {"sent", "pc"})){
        console.log(
            label + ": the message after YES was not read as \"The MAIL was sent to your PC.\": the box view tells if "
            "the MAIL is still there.",
            COLOR_ORANGE
        );
    }
    press(context, BUTTON_A, 900ms);        //  The text waits for a press.
    pbf_mash_button(context, BUTTON_B, 3000ms);
    context.wait_for_all_requests();
    story_back_to_overworld(rt, "party menu");
    console.log(label + ": the MAIL is off it.", COLOR_BLUE);
}

bool story_trade(StoryRuntime& rt, const std::string& label, const std::string& get, bool holds_mail){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    if (rt.out_now.empty()){
        console.log(label + ": no Pokemon was taken out of a box for this trade: the trader is not asked.", COLOR_ORANGE);
        return false;
    }
    const std::string give = rt.out_now;
    //  "Do you have a SPEAROW? Want to trade it for my FARFETCH'D?" YES.
    //  After the trade the trader only says how the old one is doing.
    if (!story_talk_answer(rt, true, label, StoryTalkAlt(), true)){
        console.log(label + ": the trader asks nothing: the trade was made before.", COLOR_ORANGE);
        return false;
    }
    //  ChoosePartyMon: the party comes up with its first one under the
    //  cursor; A on one picks it and closes the party.
    context.wait_for_all_requests();
    {
        PartyMenuWatcher party(COLOR_RED);
        if (wait_until(console, context, 10000ms, { party }) != 0){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_trade(): The party did not come up after YES: " + label,
                console
            );
        }
    }
    pbf_wait(context, 500ms);
    dpad(context, DPAD_LEFT, 500ms);
    for (int i = 1; i < 6; i++){
        dpad(context, DPAD_DOWN);
    }
    press(context, BUTTON_A, 3000ms);
    //  The scene of the trade. It takes no button until its end: after
    //  "Take good care of CH'DING!" it waits for A (trade_scene.c,
    //  STATE_END_LINK_TRADE: a trade in the game, not over a cable). Its
    //  text box is black, the trader's thanks after it come in the white
    //  one of the map: A every second and a half until a white text box
    //  is up and stays. (An A that closes the thanks before they are seen
    //  talks to the trader again: "How is my old ...?", white too.)
    //  Another species than asked for: "This is no SPEAROW." at once, and
    //  no scene; the look at the sixth one below tells.
    //  (The run of 18 November, build 36: the scene took half a minute and
    //  never looked like a text box; the 20th A ended it, "Thank you!" came
    //  a second and a half later. Build 36 then read the box with the
    //  reader for dark text, and what people say is coloured, a woman's
    //  words red and a man's blue: nothing read, so A again, which closed
    //  the thanks and asked her again, for three minutes.)
    const WallClock scene_until = current_time() + std::chrono::seconds(240);
    for (bool text_up = false; !text_up;){
        if (current_time() >= scene_until){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_trade(): No text of the trader's after the trade: " + label,
                console
            );
        }
        WhiteDialogWatcher text(COLOR_RED);
        context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            console, context,
            [](ProControllerContext& ctx){
                for (int i = 0; i < 20; i++){
                    pbf_press_button(ctx, BUTTON_A, 160ms, 1340ms);
                }
            },
            { text }
        );
        if (ret != 0){
            continue;
        }
        //  A text box, not a bright moment of the scene: its words (in the
        //  colours people speak in), or a box that is still there at a
        //  second look. Nothing is pressed in between.
        const std::string words = story_read_people(rt);
        int letters = 0;
        for (char ch : words){
            letters += std::isalpha((unsigned char)ch) ? 1 : 0;
        }
        if (letters >= 4){
            console.log(label + ": after the trade: \"" + words + "\"");
            text_up = true;
            continue;
        }
        pbf_wait(context, 300ms);
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        WhiteDialogDetector box(COLOR_RED);
        if (screen && box.detect(screen)){
            console.log(
                label + ": after the trade: a text box that stays (its words were not read: \"" + words + "\").",
                COLOR_ORANGE
            );
            text_up = true;
        }
    }
    //  The text closed (B), the player free again.
    story_cutscene(rt, 0ms, 60000ms, false, label);
    std::string now;
    story_read_slot(rt, 6, &now);
    if (now != get){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_trade(): Party slot 6 is " + (now.empty() ? std::string("not read") : now) + " after the trade, not " +
            get + ": " + label,
            console
        );
    }
    //  (Before the list and the table take it as done: the error of a
    //  MAIL that stays resets the game to the save before the trade.)
    if (holds_mail){
        story_take_mail(rt, 6, label);
    }
    rt.out_now = get;
    rt.caught.insert(get);
    if (rt.collection != nullptr){
        rt.collection->add_caught(get, false, false);
    }
    rt.dex_new_since_load.insert(get);
    console.log(
        "Got for the POKeDEX: " + get + " (traded for the " + give + " of the boxes)." +
        (rt.collection != nullptr
            ? " The POKeDEX table: " + std::to_string(rt.collection->caught(get)) + " of " +
                std::to_string(std::max(0, rt.collection->needed(get))) + "."
            : std::string()),
        COLOR_BLUE
    );
    story_note(rt, "Got " + get, COLOR_GREEN);
    return true;
}

bool story_use_item(
    StoryRuntime& rt, int party_slot, const std::string& words, const std::string& what, bool give, bool tolerant,
    bool keep_order
){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log(
        std::string(give ? "Giving an item to hold to party slot " : "Using an item on party slot ") +
        std::to_string(party_slot) + ": " + what
    );
    if (!keep_order){
        story_fix_party_order(rt, false);
    }
    if (give && party_slot == 1 && !keep_order){
        //  The starter (Charizard and the EXP. SHARE): in slot 1.
        story_check_lead(rt);
    }
    //  "a|b": one of the words (an evolution stone by the last line of its
    //  description, as the text reading may have it).
    std::vector<std::string> parts;
    for (size_t start = 0; start <= words.size();){
        const size_t bar = words.find('|', start);
        parts.emplace_back(words.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
        if (bar == std::string::npos){
            break;
        }
        start = bar + 1;
    }
    std::vector<const char*> any;
    for (const std::string& part : parts){
        if (!part.empty()){
            any.push_back(part.c_str());
        }
    }
    open_bag_from_overworld(console, context);
    if (!select_bag_item(rt, BagPocket::ITEMS, any, {}, what)){
        pbf_mash_button(context, BUTTON_B, 3000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, "bag");
        if (tolerant){
            console.log("Not in the bag: " + what, COLOR_ORANGE);
            return false;
        }
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
    return true;
}

bool story_take_item(StoryRuntime& rt, int party_slot, const std::string& label){
    StoryMemoryScope memory_scope("party menu");
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log(label + ": the item of party slot " + std::to_string(party_slot) + " goes back to the bag.");
    //  The message of the party menu, as lower case text, after a moment for
    //  it to be printed.
    auto message = [&]() -> std::string{
        pbf_wait(context, 1300ms);
        context.wait_for_all_requests();
        const std::string text = lower(read_story_text(console, PARTY_MESSAGE_BOX, false));
        console.log(label + ": the party menu says: \"" + text + "\"");
        return text;
    };
    auto says = [&](const std::string& text, std::initializer_list<const char*> words){
        for (const char* word : words){
            if (contains(text, word)){
                return true;
            }
        }
        return false;
    };

    open_party_menu_from_overworld(console, context);
    //  LEFT from any slot on the right goes to slot 1.
    dpad(context, DPAD_LEFT, 500ms);
    for (int i = 1; i < party_slot; i++){
        dpad(context, DPAD_DOWN);
    }
    //  The actions: SUMMARY, (field moves), SWITCH, ITEM, CANCEL. Up from the
    //  top wraps around to CANCEL, then ITEM (as story_take_mail() goes to
    //  MAIL, which takes ITEM's place when a MAIL is held).
    press(context, BUTTON_A, 800ms);
    dpad(context, DPAD_UP, 300ms);
    dpad(context, DPAD_UP, 300ms);
    press(context, BUTTON_A, 900ms);
    //  "Do what with an item?": GIVE, TAKE, CANCEL (the cursor on GIVE).
    const std::string asked = message();
    if (!says(asked, {"item", "do wh"})){
        pbf_mash_button(context, BUTTON_B, 3000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, "party menu");
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_take_item(): the party menu does not ask what to do with an item: " + label,
            console
        );
    }
    dpad(context, DPAD_DOWN, 300ms);
    press(context, BUTTON_A, 900ms);
    //  "Received the EXP. SHARE from CHARIZARD." (it waits for a press), or
    //  "CHARIZARD isn't holding anything."
    const std::string result = message();
    const bool none = says(result, {"holding", "anything"});
    const bool took = !none && says(result, {"received", "share", "from"});
    if (!none && !took){
        console.log(label + ": the message was not read as \"Received ...\" or \"isn't holding anything\".", COLOR_ORANGE);
    }
    press(context, BUTTON_A, 900ms);
    pbf_mash_button(context, BUTTON_B, 3000ms);
    context.wait_for_all_requests();
    story_back_to_overworld(rt, "party menu");
    console.log(label + (took ? ": the item is in the bag." : ": nothing was taken."), took ? COLOR_BLUE : COLOR_ORANGE);
    return took;
}



void story_heal_with_items(StoryRuntime& rt, int mode){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    if (!rt.battle.team.empty()){
        //  One item from the bag on a team member. 1: used. 2: it had no
        //  effect (and is still in the bag). 0: the bag has none.
        auto use = [&](int index, bool revive){
            const int slot = index + 1;
            bool& none = revive ? rt.no_revives : rt.no_full_restores;
            int& have = revive ? rt.bag_revives : rt.bag_restores;
            if (none){
                console.log(
                    std::string("Party slot ") + std::to_string(slot) + (revive ? " is down" : " is low on HP") +
                    ", but the bag has no " + (revive ? "REVIVE" : "FULL RESTORE or MAX POTION") + " (looked before).",
                    COLOR_ORANGE
                );
                return 0;
            }
            console.log(
                (revive ? "Reviving party slot " : "All HP back for party slot ") + std::to_string(slot) +
                (revive ? "." : " (FULL RESTORE or MAX POTION).")
            );
            open_bag_from_overworld(console, context);
            const bool found = revive
                ? select_bag_item(rt, BagPocket::ITEMS, {"revives a", "revive"}, {"fully"}, "REVIVE")
                : select_bag_item(rt, BagPocket::ITEMS, {"fully restores", "fully restore"}, {"revive"}, "FULL RESTORE");
            bool used = false;
            if (found){
                used = use_item_on(rt, slot, revive ? "REVIVE" : "FULL RESTORE");
                if (used && have > 0){
                    have--;
                }
            }else{
                none = true;
                have = 0;
            }
            pbf_mash_button(context, BUTTON_B, 3000ms);
            context.wait_for_all_requests();
            story_back_to_overworld(rt, "bag");
            return !found ? 0 : used ? 1 : 2;
        };
        const int members = (int)std::min<size_t>(rt.battle.team.size(), 6);
        //  Who is down gets a REVIVE (half its HP). Also those down since an
        //  earlier battle: without one then, with one now. (Only those of
        //  the last battle were looked at: ELECTRODE, down since Lorelei,
        //  was forgotten after Bruno.)
        for (int index = 0; index < members; index++){
            if (rt.memory.team_down.count(index) == 0 || !rt.battle.team[(size_t)index].fights){
                continue;
            }
            const int revived = use(index, true);
            if (revived == 1){
                rt.memory.team_down.erase(index);
                rt.memory.team_hp[index] = 0.5;
            }else if (revived == 2){
                //  It was not down after all.
                rt.memory.team_down.erase(index);
            }
        }
        //  Then the HP: the strongest first (the others in party order).
        int strongest = 0;
        for (int index = 1; index < members; index++){
            if (rt.battle.team[(size_t)index].strength > rt.battle.team[(size_t)strongest].strength){
                strongest = index;
            }
        }
        std::vector<int> by_strength{strongest};
        for (int index = 0; index < members; index++){
            if (index != strongest){
                by_strength.push_back(index);
            }
        }
        for (int index : by_strength){
            if (rt.memory.team_down.count(index) != 0 || !rt.battle.team[(size_t)index].fights){
                continue;
            }
            const double hp = rt.memory.team_hp[index];
            if (hp < 0){
                continue;       //  Not seen since it was healed.
            }
            //  The first League: below 60%. The League again (the items
            //  cost part of the prize money): the strongest below 50%, the
            //  others below 25%; before the Champion below 70% and 50%.
            const double below =
                mode == 2 ? (index == strongest ? 0.50 : 0.25) :
                mode == 3 ? (index == strongest ? 0.70 : 0.50) :
                0.60;
            const int percent = (int)(hp * 100 + 0.5);
            if (hp >= below){
                console.log(
                    "Party slot " + std::to_string(index + 1) + " is at " + std::to_string(percent) +
                    "% of its HP: nothing for it (only below " + std::to_string((int)(below * 100 + 0.5)) + "% here)."
                );
                continue;
            }
            if (use(index, false) != 0){
                rt.memory.team_hp[index] = 1.0;
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
        bool& none = use.revive ? rt.no_revives : rt.no_full_restores;
        int& have = use.revive ? rt.bag_revives : rt.bag_restores;
        if (none){
            console.log(std::string("The bag has no ") + (use.revive ? "REVIVE" : "FULL RESTORE") + " (looked before).");
            continue;
        }
        open_bag_from_overworld(console, context);
        bool found = use.revive
            ? select_bag_item(rt, BagPocket::ITEMS, {"revives a", "revive"}, {"fully"}, "REVIVE")
            : select_bag_item(rt, BagPocket::ITEMS, {"fully restores", "fully restore"}, {"revive"}, "FULL RESTORE");
        if (found){
            if (use_item_on(rt, use.slot, use.revive ? "REVIVE" : "FULL RESTORE") && have > 0){
                have--;
            }
        }else{
            none = true;
            have = 0;
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
    //  (This look for a REVIVE takes a MAX REVIVE too: one more than the
    //  look that set "no_revives".)
    bool no_revive = false;
    bool no_restore = rt.no_full_restores;
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
            const bool used = use_item_on(rt, use.slot, use.revive ? "REVIVE" : "FULL RESTORE");
            if (use.revive){
                //  Revived, or it was not down: up again either way.
                rt.memory.team_down.erase(use.slot - 1);
                if (used){
                    rt.memory.team_hp[use.slot - 1] = 0.5;
                    if (rt.bag_revives > 0){
                        rt.bag_revives--;
                    }
                }
            }else if (used){
                rt.memory.team_hp[use.slot - 1] = 1.0;
                if (rt.bag_restores > 0){
                    rt.bag_restores--;
                }
            }
        }else if (use.revive){
            no_revive = true;
            rt.no_revives = true;
            rt.bag_revives = 0;
        }else{
            no_restore = true;
            rt.no_full_restores = true;
            rt.bag_restores = 0;
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


void story_pace(
    StoryRuntime& rt, DpadPosition direction, int length, const std::string& what,
    const std::function<void()>& on_saved
){
    ConsoleHandle& console = rt.console;
    if (rt.out_box < 0 || !rt.out_friend || rt.out_give.empty()){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_pace(): No walk for friendship is under way (nothing was taken out of a box for it, and no "
            "file of one): " + what,
            console
        );
    }
    if (length < 3 || direction == DPAD_NONE){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_pace(): The step has no line to walk along: " + what,
            console
        );
    }
    std::string name;
    for (char ch : rt.out_give){
        name += ch >= 'a' && ch <= 'z' ? (char)(ch - 'a' + 'A') : ch;
    }
    AutoStoryBoxes::FriendRecord& record = rt.friend_records[rt.out_give];
    const int wanted = AutoStoryBoxes::friend_blocks_wanted(rt.out_give, record.tests);
    const int start = record.blocks;
    if (start >= wanted){
        console.log(
            what + ": " + name + " has walked " + std::to_string(start) + " of " + std::to_string(wanted) +
            " blocks of " + std::to_string(AutoStoryBoxes::FRIEND_BLOCK_STEPS) + " steps: nothing to walk.",
            COLOR_BLUE
        );
        return;
    }

    //  As story_oscillate(): along a line between two walls, B held, each way
    //  a little longer than the line, so it ends bumping into the wall (no
    //  drift). A bump is no step: a leg is length - 1 steps.
    const DpadPosition back = opposite(direction);
    const int64_t per_leg = length - 1;
    const Milliseconds leg = Milliseconds((int64_t)((8 + 8 * (length - 1) + 12) * 1000 / 60));
    //  Legs in a round (an even number: the round ends where it started), and
    //  the blocks between two saves.
    constexpr int64_t LEGS_PER_ROUND = 60;
    constexpr int SAVE_EVERY = 40;
    const int64_t total = (int64_t)(wanted - start) * AutoStoryBoxes::FRIEND_BLOCK_STEPS;
    const int64_t legs_total = (total + per_leg - 1) / per_leg;
    const int64_t minutes = legs_total * (leg.count() + 50) / 60000;
    console.log(
        what + ": walking " + name + " for its friendship: " + std::to_string(start) + " of " + std::to_string(wanted) +
        " blocks of " + std::to_string(AutoStoryBoxes::FRIEND_BLOCK_STEPS) + " steps walked (" +
        std::to_string(record.tests) + " levels trained without it evolving), " + std::to_string(total) +
        " steps to go: about " + std::to_string(minutes) + " minutes, running along a line of " +
        std::to_string(length) + " tiles (experimental).",
        COLOR_BLUE
    );
    story_note(rt, what + ": " + name + " " + std::to_string(start) + " of " + std::to_string(wanted) + " blocks");

    int64_t steps = 0;
    int saved_at = start;
    int rounds = 0;
    int hits = 0;
    AutoStoryScroll::Screen reference;
    const bool check = story_take_picture(rt, reference);
    while (record.blocks < wanted){
        const int64_t remaining = (int64_t)(wanted - record.blocks) * AutoStoryBoxes::FRIEND_BLOCK_STEPS;
        int64_t legs = std::min<int64_t>(LEGS_PER_ROUND, (remaining + per_leg - 1) / per_leg);
        legs += legs % 2;
        WhiteDialogWatcher dialog(COLOR_RED);
        BlackScreenWatcher black(COLOR_RED);
        rt.context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            rt.console, rt.context,
            [&](ProControllerContext& context){
                for (int64_t i = 0; i < legs; i++){
                    pbf_controller_state(context, BUTTON_B, i % 2 == 0 ? direction : back, {0, 0}, {0, 0}, leg);
                    pbf_wait(context, 50ms);
                }
                context.wait_for_all_requests();
            },
            { dialog, black }
        );
        rt.facing = DPAD_NONE;
        if (ret >= 0){
            //  Nothing on the plaza starts a text box or a screen change, so
            //  this may be the detector seeing things in the white of the
            //  stones: once or twice, back to the start of the line (B held
            //  against the wall behind it, which closes a text box) and on,
            //  without counting the round. Three in a row: an error.
            hits++;
            if (hits >= 3){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "story_pace(): A text box or a black screen came up in " + std::to_string(hits) +
                    " rounds in a row while " + name + " walked: " + what,
                    console
                );
            }
            console.log(
                what + ": a text box or a black screen came up while " + name + " walked (" + std::to_string(hits) +
                " in a row): back to the start of the line, the round is not counted.",
                COLOR_ORANGE
            );
            pbf_controller_state(rt.context, BUTTON_B, back, {0, 0}, {0, 0}, leg);
            pbf_wait(rt.context, 500ms);
            rt.context.wait_for_all_requests();
            continue;
        }
        hits = 0;
        steps += legs * per_leg;
        record.blocks = start + (int)(steps / AutoStoryBoxes::FRIEND_BLOCK_STEPS);
        rounds++;

        //  Back where the round started: the screen is the one of the first
        //  round's start. A clear offset of a tile or more means the player
        //  is elsewhere (somebody stood in the way, or the run fell short).
        if (check){
            AutoStoryScroll::Screen now;
            if (story_take_picture(rt, now)){
                const AutoStoryScroll::TileOffset offset = AutoStoryScroll::tile_offset(reference, now, 2);
                if (offset.clear && (offset.x != 0 || offset.y != 0)){
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::SEND_ERROR_REPORT,
                        "story_pace(): The player is " + offset.summary() + " from where the walk started after " +
                        std::to_string(steps) + " steps: " + what,
                        console
                    );
                }
            }
        }
        story_note(rt, what + ": " + name + " " + std::to_string(record.blocks) + " of " + std::to_string(wanted) + " blocks");
        if (rounds % 4 == 0 || record.blocks >= wanted){
            console.log(
                what + ": " + name + " has walked " + std::to_string(record.blocks) + " of " + std::to_string(wanted) +
                " blocks (" + std::to_string(steps) + " steps counted in this walk).",
                COLOR_BLUE
            );
        }
        const bool done = record.blocks >= wanted;
        if (rt.save_after_heal && (done || record.blocks - saved_at >= SAVE_EVERY)){
            //  The save and the file agree: after a reset, the walk goes on
            //  from here.
            story_save(rt, rt.has_dex);
            story_saved(rt, done ? rt.step_index + 1 : rt.step_index);
            saved_at = record.blocks;
            if (on_saved){
                on_saved();
            }
        }
    }
    console.log(what + ": " + name + " has walked " + std::to_string(record.blocks) + " blocks: done.", COLOR_BLUE);
}



int story_party_count(StoryRuntime& rt){
    StoryMemoryScope memory_scope("party menu");
    open_party_menu_from_overworld(rt.console, rt.context);
    pbf_wait(rt.context, 800ms);
    rt.context.wait_for_all_requests();
    const int count = (int)detect_last_occupied_party_slot(rt.console) + 1;
    story_back_to_overworld(rt, "party menu");
    return count;
}


std::string story_hatch(
    StoryRuntime& rt, DpadPosition direction, int length, int max_steps, const std::string& species,
    const std::string& what
){
    ConsoleHandle& console = rt.console;
    if (length < 3 || direction == DPAD_NONE){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_hatch(): The step has no line to walk along: " + what,
            console
        );
    }
    //  The egg: the party has one Pokemon more than when the room was made.
    const int count = story_party_count(rt);
    if (rt.egg_party > 0 && count != rt.egg_party + 1){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_hatch(): The party has " + std::to_string(count) + " Pokemon, but " +
            std::to_string(rt.egg_party + 1) + " were expected with the egg: it was not handed over (the man gives "
            "it to a lead Pokemon with the highest friendship, once, and the party needs room): " + what,
            console
        );
    }
    if (count < 2){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_hatch(): The party has no egg (" + std::to_string(count) + " Pokemon): " + what,
            console
        );
    }
    const DpadPosition back = opposite(direction);
    const int64_t per_leg = length - 1;
    const Milliseconds leg = Milliseconds((int64_t)((8 + 8 * (length - 1) + 12) * 1000 / 60));
    constexpr int64_t LEGS_PER_ROUND = 60;
    const int64_t legs_total = ((int64_t)max_steps + per_leg - 1) / per_leg;
    const int64_t minutes = legs_total * (leg.count() + 50) / 60000;
    console.log(
        what + ": the egg lies in party slot " + std::to_string(count) + ". Walking back and forth along " +
        std::to_string(length) + " tiles until it hatches: at most " + std::to_string(max_steps) + " steps, about " +
        std::to_string(minutes) + " minutes (experimental).",
        COLOR_BLUE
    );
    story_note(rt, what + ": walking for the egg");

    int64_t steps = 0;
    int rounds = 0;
    int false_alarms = 0;
    AutoStoryScroll::Screen reference;
    const bool check = story_take_picture(rt, reference);
    while (steps < max_steps){
        const int64_t remaining = (int64_t)max_steps - steps;
        int64_t legs = std::min<int64_t>(LEGS_PER_ROUND, (remaining + per_leg - 1) / per_leg);
        legs += legs % 2;
        WhiteDialogWatcher dialog(COLOR_RED);
        BlackScreenWatcher black(COLOR_RED);
        rt.context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            rt.console, rt.context,
            [&](ProControllerContext& context){
                for (int64_t i = 0; i < legs; i++){
                    pbf_controller_state(context, BUTTON_B, i % 2 == 0 ? direction : back, {0, 0}, {0, 0}, leg);
                    pbf_wait(context, 50ms);
                }
                context.wait_for_all_requests();
            },
            { dialog, black }
        );
        rt.facing = DPAD_NONE;
        if (ret >= 0){
            console.log(
                what + ": a text box or a black screen came up after about " + std::to_string(steps) +
                " steps: the egg hatches? B reads through it.",
                COLOR_BLUE
            );
            //  The scene: "Oh?", the animation, "... hatched from the EGG!"
            //  and the nickname question (B: NO).
            pbf_mash_button(rt.context, BUTTON_B, 14000ms);
            rt.context.wait_for_all_requests();
            story_back_to_overworld(rt, what + ": the scene of the hatching");
            std::string name;
            const int level = story_read_slot(rt, count, &name);
            if (level > 0 && !name.empty()){
                console.log(
                    what + ": " + name + " hatched (level " + std::to_string(level) + ")" +
                    (name == species ? "." : ", not " + species + " as expected."),
                    name == species ? COLOR_BLUE : COLOR_ORANGE
                );
                //  Back to where the line starts: a leg towards its wall.
                for (int k = 0; k < 2; k++){
                    pbf_controller_state(rt.context, BUTTON_B, back, {0, 0}, {0, 0}, leg);
                    pbf_wait(rt.context, 300ms);
                }
                rt.context.wait_for_all_requests();
                rt.facing = DPAD_NONE;
                return name;
            }
            false_alarms++;
            if (false_alarms >= 3){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "story_hatch(): A text box or a black screen came up " + std::to_string(false_alarms) +
                    " times, and party slot " + std::to_string(count) + " did not read as a hatched Pokemon: " + what,
                    console
                );
            }
            console.log(
                what + ": party slot " + std::to_string(count) + " does not read as a hatched Pokemon (" +
                std::to_string(false_alarms) + " in all): back to the start of the line, the walk goes on.",
                COLOR_ORANGE
            );
            for (int k = 0; k < 2; k++){
                pbf_controller_state(rt.context, BUTTON_B, back, {0, 0}, {0, 0}, leg);
                pbf_wait(rt.context, 300ms);
            }
            rt.context.wait_for_all_requests();
            //  (The steps up to here are not counted exactly: the round is
            //  taken as half done.)
            steps += legs * per_leg / 2;
            continue;
        }
        steps += legs * per_leg;
        rounds++;
        if (check){
            AutoStoryScroll::Screen now;
            if (story_take_picture(rt, now)){
                const AutoStoryScroll::TileOffset offset = AutoStoryScroll::tile_offset(reference, now, 2);
                if (offset.clear && (offset.x != 0 || offset.y != 0)){
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::SEND_ERROR_REPORT,
                        "story_hatch(): The player is " + offset.summary() + " from where the walk started after " +
                        std::to_string(steps) + " steps: " + what,
                        console
                    );
                }
            }
        }
        story_note(rt, what + ": " + std::to_string(steps) + " of at most " + std::to_string(max_steps) + " steps");
        if (rounds % 2 == 0){
            console.log(
                what + ": " + std::to_string(steps) + " steps walked, the egg has not hatched yet (at most " +
                std::to_string(max_steps) + ").",
                COLOR_BLUE
            );
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "story_hatch(): The egg did not hatch in " + std::to_string(steps) + " steps (a Togepi egg wants 2805): " + what,
        console
    );
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

    //  The steps taken along the line so far (those of step()).
    uint64_t steps() const{
        return m_steps;
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
                m_steps++;
            }
        }else{
            int made = 0;
            result = story_grass_run(m_rt, direction, moves, kind, m_where, throw_on_loss, made, sure, m_run);
            moved = made > 0 || result != StoryBattleResult::NO_BATTLE;
            m_steps += (uint64_t)std::max(0, made);
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
        if (m_rt.safari_over){
            //  The Safari Game ended on the way (the PA's text is up, or the
            //  last ball is thrown): the player no longer walks here, and a
            //  look at where they stand would be a look at that text.
            return result;
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
    uint64_t m_steps = 0;
};

}



namespace{

const AutoStoryDex::SpeciesInfo* dex_species(const std::string& slug){
    for (const AutoStoryDex::SpeciesInfo& info : AutoStoryDex::species()){
        if (slug == info.slug){
            return &info;
        }
    }
    return nullptr;
}

//  The level of the one that trains (party slot 1). In the story that is
//  the starter or a team member, and the caller knows its types. Not in a
//  run of the story (the Level Trainer) it is whatever the player put
//  there: its species gives the types for the same-type bonus and the stats
//  its moves are valued with, and it can change on the way (an evolution).
int read_training_level(StoryRuntime& rt){
    if (!rt.standalone){
        return story_read_lead_level(rt);
    }
    std::string front;
    const int level = story_read_slot(rt, 1, &front);
    const AutoStoryDex::SpeciesInfo* info = dex_species(front);
    if (info != nullptr && info != rt.battle.lead_species){
        rt.console.log(
            "Party slot 1 is " + std::string(info->name) + ": its types and stats count for the choice of moves.",
            COLOR_BLUE
        );
        rt.battle.lead_species = info;
        rt.battle.lead_type1 = info->type1;
        rt.battle.lead_type2 = info->type2;
        rt.lead_type1 = info->type1;
        rt.lead_type2 = info->type2;
    }
    return level;
}

//  The training itself: the Pokemon in party slot 1 fights. "member": it
//  is a team member put in front for this (see story_grind()).
void grind_front(
    StoryRuntime& rt, const StoryScript& script, const StoryStep& step,
    int extra_levels, bool member, int share_position = 0, uint64_t max_battles = 400
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

    int level = read_training_level(rt);
    console.log(
        where + ": training the lead to level " + std::to_string(target) +
        " (now " + (level < 0 ? std::string("unknown") : std::to_string(level)) + ")."
    );
    story_note(
        rt,
        where + ": level " + (level < 0 ? std::string("?") : std::to_string(level)) + " of " + std::to_string(target)
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
            int now = read_training_level(rt);
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
    const uint64_t battle_cap = level < 0 ? 40 : max_battles;
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
            int now = read_training_level(rt);
            if (now > 0){
                level = now;
                story_note(rt, where + ": level " + std::to_string(level) + " of " + std::to_string(target));
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
            int now = read_training_level(rt);
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



//  ---- Evolving by level (build 43) -------------------------------------------

namespace{

//  The EXP. SHARE's description in the bag ("...a share of EXP. points
//  without having to battle"): the words story C15_04 found it by.
const char* const EXP_SHARE_WORDS = "share of";

//  The EXP. SHARE from the lead to the party slot of the Pokemon in
//  training. A holder gets half of what a battle gives (the game splits the
//  rest between the Pokemon that were sent out); with the lead holding it,
//  the Pokemon in training would get a quarter. Not essential: when the
//  menus are not what they are expected to be, the training goes on without
//  (slower), and the item is looked for again at the end.
void exp_share_to(StoryRuntime& rt, int from_slot, int to_slot, const std::string& label){
    ConsoleHandle& console = rt.console;
    try{
        story_take_item(rt, from_slot, label + ": EXP. SHARE off party slot " + std::to_string(from_slot));
        if (!story_use_item(rt, to_slot, EXP_SHARE_WORDS, "EXP. SHARE to party slot " + std::to_string(to_slot), true, true)){
            console.log(label + ": the bag has no EXP. SHARE: the training goes on without.", COLOR_ORANGE);
        }
    }catch (OperationFailedException&){
        console.log(
            label + ": the EXP. SHARE could not be moved (see the error above): the training goes on as it is.",
            COLOR_ORANGE
        );
        try{
            story_back_to_overworld(rt, "party menu");
        }catch (OperationFailedException&){
        }
    }
}

}


void story_exp_share(StoryRuntime& rt, int from_slot, int to_slot, const std::string& label){
    exp_share_to(rt, from_slot, to_slot, label);
}


//  HITMONTOP (or HITMONCHAN) by vitamins (build 57, experimental, not run on
//  a console when written; PokemonFRLG_AutoStoryBoxes.h says how it works).
//  The TYROGUE in party slot 1 is at level 19, healed, in front of the nurse:
//  the Attack and the Defense are read (summary, page 2), the PROTEINs and
//  IRONs that make them what the goal says at level 20 are used, and that
//  is done once more (what the first ones did is what the reading did not
//  know). The vitamins come from the bag (the segment that buys them comes
//  before the training); with none in the bag the training goes on as it
//  is. True: something was used.
static bool aim_vitamins(StoryRuntime& rt, AutoStoryBoxes::VitaminGoal kind, const std::string& where){
    ConsoleHandle& console = rt.console;
    constexpr int LEVEL_READ = AutoStoryBoxes::TYROGUE_EVOLVES_AT - 1;
    const std::string key = std::to_string(rt.out_box) + ":" + std::to_string(rt.out_cell);
    if (rt.aim_key != key){
        rt.aim_key = key;
        rt.aim_protein = 0;
        rt.aim_iron = 0;
        rt.aim_done = false;
    }
    if (rt.aim_done){
        console.log(where + ": the vitamins were used on this one already.", COLOR_BLUE);
        return false;
    }
    bool used_any = false;
    for (int round = 1; round <= 2; round++){
        StoryStats stats;
        if (!story_read_stats(rt, 1, stats)){
            console.log(where + ": the stats could not be read: no vitamins.", COLOR_ORANGE);
            break;
        }
        if (stats.species != "tyrogue" || stats.level != LEVEL_READ){
            console.log(
                where + ": party slot 1 is " + stats.species + " of level " + std::to_string(stats.level) +
                ", not a TYROGUE of level " + std::to_string(LEVEL_READ) + ": no vitamins.",
                COLOR_ORANGE
            );
            break;
        }
        int attack_percent = 100;
        int defense_percent = 100;
        if (!AutoStoryBoxes::nature_percent(stats.nature, attack_percent, defense_percent)){
            console.log(where + ": \"" + stats.nature + "\" is not a nature it knows: no vitamins.", COLOR_ORANGE);
            break;
        }
        const AutoStoryBoxes::VitaminAim aim = AutoStoryBoxes::vitamin_aim(
            kind, LEVEL_READ, AutoStoryBoxes::TYROGUE_EVOLVES_AT,
            stats.attack, stats.defense, attack_percent, defense_percent, rt.aim_protein, rt.aim_iron
        );
        console.log(
            where + ": reading " + std::to_string(round) + ": " + std::to_string(aim.attack_fits) + " IV/EV pairs fit the Attack, " +
            std::to_string(aim.defense_fits) + " the Defense; " +
            (aim.chance < 0
                ? std::string("the stats fit none: no vitamins.")
                : std::to_string(aim.protein) + " PROTEIN and " + std::to_string(aim.iron) + " IRON more (" +
                    std::to_string((int)(aim.chance * 100 + 0.5)) + "% to " +
                    (kind == AutoStoryBoxes::VitaminGoal::ATTACK_EQUALS_DEFENSE ? "HITMONTOP" : "HITMONCHAN") + ")."),
            COLOR_BLUE
        );
        if (aim.chance < 0 || (aim.protein == 0 && aim.iron == 0)){
            break;
        }
        auto use = [&](int count, const char* words, const char* name){
            int used = 0;
            for (int i = 0; i < count; i++){
                if (!story_use_item(rt, 1, words, name, false, true, true)){
                    break;
                }
                used++;
            }
            return used;
        };
        const int protein = use(aim.protein, "base attack|base atta", "PROTEIN");
        const int iron = use(aim.iron, "base defense|base defen", "IRON");
        rt.aim_protein += protein;
        rt.aim_iron += iron;
        used_any |= protein + iron > 0;
        console.log(
            where + ": used " + std::to_string(protein) + " PROTEIN and " + std::to_string(iron) + " IRON (" +
            std::to_string(rt.aim_protein) + " and " + std::to_string(rt.aim_iron) + " on this one).",
            COLOR_BLUE
        );
        if (protein < aim.protein || iron < aim.iron){
            console.log(where + ": the bag ran out of vitamins.", COLOR_ORANGE);
            break;
        }
    }
    rt.aim_done = true;
    return used_any;
}


void story_raise(StoryRuntime& rt, const StoryScript& script, const StoryStep& step){
    ConsoleHandle& console = rt.console;
    const std::string where = step.label;
    if (rt.out_box < 0 || rt.out_give.empty() || rt.out_becomes.empty() || rt.out_goal < 2){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_raise(): No training is under way (nothing was taken out of a box, and no file of one): " + where,
            console
        );
    }
    const std::string from = rt.out_give;
    const std::string becomes = rt.out_becomes;
    const int goal = rt.out_goal;
    //  (TYROGUE: all three of its evolutions.)
    const std::set<std::string> line = AutoStoryBoxes::training_line(from, becomes);
    console.log(
        where + ": training " + from + " (" + becomes + " at level " + std::to_string(goal) + ", experimental).",
        COLOR_BLUE
    );

    //  Where it is: in the party's sixth place (from LEVEL_OUT, or from a save
    //  made before the training began), or in front (a save made at a nurse
    //  in the middle of it, which has it there).
    std::string front;
    for (int attempt = 0; attempt < 3 && front.empty(); attempt++){
        story_read_slot(rt, 1, &front);
    }
    if (line.count(front) == 0){
        std::string sixth;
        for (int attempt = 0; attempt < 3 && sixth.empty(); attempt++){
            story_read_slot(rt, 6, &sixth);
        }
        if (line.count(sixth) == 0){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_raise(): Party slot 6 is " + (sixth.empty() ? std::string("not read") : sixth) + " (slot 1: " +
                (front.empty() ? std::string("not read") : front) + "), not " + from + " or " + becomes + ": " + where,
                console
            );
        }
        exp_share_to(rt, 1, 6, where);
        story_swap_party(rt, 1, 6);
        front.clear();
        for (int attempt = 0; attempt < 3 && front.empty(); attempt++){
            story_read_slot(rt, 1, &front);
        }
        if (line.count(front) == 0){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_raise(): " + from + " did not come to the front: " + front + ": " + where,
                console
            );
        }
    }

    //  The same state as the training of a team member in front
    //  (story_grind()), with the lead (now in the sixth place) coming in.
    const AutoStoryDex::SpeciesInfo* info = dex_species(front);
    if (info != nullptr){
        rt.battle.lead_type1 = info->type1;
        rt.battle.lead_type2 = info->type2;
    }
    rt.battle.lead_species = info;
    rt.memory.no_pp = 0;
    for (int& m : rt.memory.moves){
        m = -1;
    }
    rt.battle.share_type1 = rt.lead_type1;
    rt.battle.share_type2 = rt.lead_type2;
    rt.battle.share_members = line;
    rt.memory.member_moves_known = false;
    //  No new move for it: nothing in it is valued, and it may be one of
    //  the Pokemon whose moves the program does not know.
    rt.battle.never_learn = true;

    StoryStep trained = step;
    trained.p0 = goal;
    trained.p2 = 6;
    trained.text = "";
    //  800 battles: a Tyranitar from a Pupitar of level 30 is some 400 of
    //  them (about 450 experience points a battle at 75%, 90 seconds each).
    //  A TYROGUE: to level 19 first, and there the vitamins (HITMONTOP while
    //  the table has none, then HITMONCHAN while it wants one).
    if (from == "tyrogue" && goal == AutoStoryBoxes::TYROGUE_EVOLVES_AT && rt.collection != nullptr){
        bool aim = false;
        AutoStoryBoxes::VitaminGoal kind = AutoStoryBoxes::VitaminGoal::ATTACK_EQUALS_DEFENSE;
        if (rt.collection->caught("hitmontop") == 0){
            aim = true;
        }else if (rt.collection->needed("hitmonchan") > rt.collection->caught("hitmonchan")){
            aim = true;
            kind = AutoStoryBoxes::VitaminGoal::DEFENSE_ABOVE_ATTACK;
        }
        if (aim){
            StoryStep to_19 = trained;
            to_19.p0 = goal - 1;
            grind_front(rt, script, to_19, 0, true, 5, 800);
            try{
                if (aim_vitamins(rt, kind, where) && rt.save_after_heal){
                    //  Saved with the vitamins in it: a reset does not lose them.
                    story_save(rt, rt.has_dex);
                    story_saved(rt, rt.step_index);
                }
            }catch (OperationFailedException&){
                console.log(where + ": the vitamins failed (see the error above): the training goes on as it is.", COLOR_ORANGE);
                try{
                    story_back_to_overworld(rt, "bag");
                }catch (OperationFailedException&){
                }
            }
        }
    }
    grind_front(rt, script, trained, 0, true, 5, 800);

    rt.battle.never_learn = false;
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
    rt.memory.member_moves_known = false;

    //  Back in order: the lead in front, the one in training in the sixth
    //  place, the EXP. SHARE on the lead. (After an evolution the party's
    //  front is of its line, too.)
    story_swap_party(rt, 1, 6);
    front.clear();
    for (int attempt = 0; attempt < 3 && front.empty(); attempt++){
        story_read_slot(rt, 1, &front);
    }
    if (!front.empty() && line.count(front) != 0){
        console.log("The party is still out of order. Swapping again.", COLOR_ORANGE);
        story_swap_party(rt, 1, 6);
    }
    exp_share_to(rt, 6, 1, where);
    if (rt.save_after_heal){
        story_save(rt, rt.has_dex);
        story_saved(rt, rt.step_index + 1);
    }
}


void story_train_at(StoryRuntime& rt, const StoryScript& script, const StoryStep& place, int level){
    StoryStep step = place;
    step.p0 = level;
    step.p2 = -1;                   //  Party slot 1 as it is: no team member.
    step.p3 = 0;
    step.p4 = place.p4 & 0xff;      //  The grass line only: no Rare Candies.
    step.text = "";
    grind_front(rt, script, step, 0, false);
}


StoryTrainEnd story_train_here(StoryRuntime& rt, const StoryTrainHere& options, int& level){
    ConsoleHandle& console = rt.console;
    const std::string where = "Level Trainer";
    const int target = options.level;

    level = read_training_level(rt);
    console.log(
        where + ": training party slot 1 to level " + std::to_string(target) +
        " (now " + (level < 0 ? std::string("unknown") : std::to_string(level)) + ")."
    );
    story_note(
        rt,
        where + ": level " + (level < 0 ? std::string("?") : std::to_string(level)) + " of " + std::to_string(target)
    );
    if (level >= target){
        console.log(where + ": level " + std::to_string(level) + " reached already.", COLOR_BLUE);
        return StoryTrainEnd::LEVEL_REACHED;
    }

    //  The player stands on the first tile of the line.
    GrassSpot spot(rt, options.forth, options.length, StoryBattleKind::WILD_FIGHT, where, options.run);
    spot.arrived();

    //  If the level cannot be read, stop after a fixed number of battles.
    const uint64_t battle_cap = options.max_battles > 0 ? options.max_battles : (level < 0 ? 40 : 100000);
    uint64_t battles = 0;
    uint64_t walks_without_battle = 0;
    StoryTrainEnd end = StoryTrainEnd::BATTLE_LIMIT;
    while (true){
        rt.battle.lead_level = level > 0 ? level : 0;
        const StoryBattleResult result = spot.step(StoryBattleKind::WILD_FIGHT, false);
        if (result == StoryBattleResult::NO_BATTLE){
            if (++walks_without_battle > 300){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "Level Trainer: no wild battle in 300 walks along the line. Is the player in tall grass "
                    "(or in a cave, or on water with wild Pokemon), and does the line go the way the options say?",
                    console
                );
            }
            continue;
        }
        walks_without_battle = 0;
        battles++;

        if (result == StoryBattleResult::LOST){
            //  At the last Pokemon Center, healed. No route from there.
            console.log(where + ": the party fainted.", COLOR_RED);
            return StoryTrainEnd::WHITED_OUT;
        }

        //  The battle is a moment the screen stands still anyway: check the
        //  count of the steps.
        spot.check();

        if (rt.memory.level_up_seen){
            rt.memory.level_up_seen = false;
            const int now = read_training_level(rt);
            if (now > 0){
                level = now;
                story_note(rt, where + ": level " + std::to_string(level) + " of " + std::to_string(target));
            }
        }
        if (level >= target){
            end = StoryTrainEnd::LEVEL_REACHED;
            break;
        }
        if (rt.lead_fainted || result == StoryBattleResult::LEAD_FAINTED){
            console.log(where + ": the Pokemon in party slot 1 fainted.", COLOR_ORANGE);
            end = StoryTrainEnd::LEAD_FAINTED;
            break;
        }

        const double hp = rt.memory.player_hp;
        const bool hp_known = hp >= 0;
        console.log(
            where + ": HP " + (hp_known ? std::to_string((int)(hp * 100 + 0.5)) + "%" : std::string("not seen")) +
            ", " + std::to_string(battles) + " battles."
        );
        if (hp_known && hp < options.stop_hp){
            console.log(where + ": low on HP, and no nurse here.", COLOR_ORANGE);
            end = StoryTrainEnd::LOW_HP;
            break;
        }
        //  Every move that was read is out of PP (found out one by one, as
        //  the move list comes back after a pick).
        {
            int known = 0;
            int empty = 0;
            for (int i = 0; i < 4; i++){
                if (rt.memory.moves[i] >= 0){
                    known++;
                    if (rt.memory.no_pp & (1 << i)){
                        empty++;
                    }
                }
            }
            if (known > 0 && empty == known){
                console.log(where + ": no PP left for any move.", COLOR_ORANGE);
                end = StoryTrainEnd::OUT_OF_PP;
                break;
            }
        }
        if (battles >= battle_cap){
            console.log(where + ": stopping after " + std::to_string(battles) + " battles.", COLOR_ORANGE);
            end = StoryTrainEnd::BATTLE_LIMIT;
            break;
        }
    }
    //  Back to where the program started.
    spot.to_first_tile();
    return end;
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

namespace{

//  Where a DEX step catches: a line of grass (cave floor) walked along, or
//  the water in front of the player with a rod.
class DexSpot{
public:
    virtual ~DexSpot() = default;
    //  The route has just brought the player there.
    virtual void arrived() = 0;
    //  The next try: a run along the grass, a cast of the rod.
    virtual StoryBattleResult step(StoryBattleKind kind) = 0;
    //  The tries so far: steps on the grass, casts.
    virtual uint64_t steps() const = 0;
    //  After a battle: the player where the count has them.
    virtual void check() = 0;
    //  Back to where the routes there and back meet.
    virtual void to_first_tile() = 0;
};

class GrassDexSpot : public DexSpot{
public:
    GrassDexSpot(StoryRuntime& rt, DpadPosition forth, int length, const std::string& where)
        : m_spot(rt, forth, length, StoryBattleKind::CATCH, where)
    {}
    virtual void arrived() override{ m_spot.arrived(); }
    virtual StoryBattleResult step(StoryBattleKind kind) override{ return m_spot.step(kind); }
    virtual uint64_t steps() const override{ return m_spot.steps(); }
    virtual void check() override{ m_spot.check(); }
    virtual void to_first_tile() override{ m_spot.to_first_tile(); }

private:
    GrassSpot m_spot;
};


//  What a cast of a rod came to.
enum class CastEnd{
    NO_BITE,        //  "Not even a nibble..."
    BATTLE,         //  a bite, and the battle it brought is over
    NO_ROD,         //  the rod is not in the bag
    NOT_HERE,       //  no rod can be cast where the player stands
    UNCLEAR,        //  neither a text nor a battle came of it
};

//  One cast where the player stands, on land facing the water: the rod
//  from the KEY ITEMS pocket (USE: the bag closes, FieldUseFunc_Rod), then
//  the text the cast ends with (PokemonFRLG_AutoStoryFishing.h). A bite: B
//  on its text, and the wild battle is run ("battle": how it ended). The
//  player is free again afterwards.
//  (From the bag each time, not registered to SELECT: the item's menu
//  reads REGISTER or DESELECT in the same place, and what is registered
//  now is not known. The bag opens on the rod from the second cast on.)
CastEnd cast_rod(
    StoryRuntime& rt, int rod, StoryBattleKind kind, const std::string& where,
    StoryBattleResult& battle, std::string& said
){
    using AutoStoryFishing::Cast;
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    battle = StoryBattleResult::NO_BATTLE;
    said.clear();
    const AutoStoryFishing::RodWords words = AutoStoryFishing::rod_words(rod);
    //  In a Safari Game the start menu is another one: RETIRE on top, so that the BAG lies
    //  a place lower (SetUpStartMenu_SafariZone() of the decompilation). With the ordinary
    //  order the cursor went to the POKeMON and the bag never opened (build 52, 22 November:
    //  "Failed to open bag" six times in each of two tries).
    open_bag_from_overworld(
        console, context, rt.in_safari ? StartMenuContext::SAFARI_ZONE : StartMenuContext::STANDARD
    );
    if (!select_bag_item(
        rt, BagPocket::KEY_ITEMS, words.words, words.avoid, AutoStoryFishing::rod_name(rod), true, true
    )){
        pbf_mash_button(context, BUTTON_B, 3000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, "bag");
        return CastEnd::NO_ROD;
    }
    rt.rods_in_bag.insert(rod);
    rt.rods_missing.erase(rod);
    //  A: USE / REGISTER / CANCEL, the cursor on USE. A: the bag closes and
    //  the rod is cast; where it cannot be, the bag stays, with PROF. OAK's
    //  words.
    press(context, BUTTON_A, 800ms);
    press(context, BUTTON_A, 1200ms);
    context.wait_for_all_requests();

    const WallClock cast_start = current_time();
    const WallClock deadline = cast_start + std::chrono::seconds(AutoStoryFishing::CAST_SECONDS + 4);
    int other_texts = 0;
    while (current_time() < deadline){
        WhiteDialogWatcher dialog(COLOR_RED);
        AdvanceBattleDialogWatcher battle_text(COLOR_RED);
        BattleMenuWatcher battle_menu(COLOR_RED);
        context.wait_for_all_requests();
        int ret = wait_until(console, context, 500ms, { dialog, battle_text, battle_menu });
        if (ret == 1 || ret == 2){
            //  A battle is on already. (Unless it is the white text box of
            //  the cast that the battle's detectors took for theirs: that
            //  is read below.)
            VideoSnapshot screen = console.video().snapshot();
            WhiteDialogDetector white(COLOR_RED);
            if (screen && white.detect(screen)){
                ret = 0;
            }else{
                battle = story_battle(rt, kind, where);
                return battle == StoryBattleResult::NO_BATTLE ? CastEnd::UNCLEAR : CastEnd::BATTLE;
            }
        }
        if (current_time() - cast_start > 2500ms){
            //  Still the bag: the rod was not cast.
            VideoSnapshot screen = console.video().snapshot();
            BagDetector bag(COLOR_RED);
            if (screen && bag.detect(screen)){
                said = read_bag_description(rt);
                pbf_mash_button(context, BUTTON_B, 3000ms);
                context.wait_for_all_requests();
                story_back_to_overworld(rt, "bag");
                return CastEnd::NOT_HERE;
            }
        }
        if (ret != 0){
            continue;
        }
        const std::string text = story_read_dialog(rt);
        switch (AutoStoryFishing::read_cast(text)){
        case Cast::NOTHING_YET:
            //  The dots.
            pbf_wait(context, 200ms);
            break;
        case Cast::NOT_HERE:
            said = text;
            pbf_mash_button(context, BUTTON_B, 3000ms);
            context.wait_for_all_requests();
            story_back_to_overworld(rt, "bag");
            return CastEnd::NOT_HERE;
        case Cast::NO_BITE:
            said = text;
            pbf_wait(context, 200ms);
            //  B closes the text. A press that comes while its last letters
            //  still print only speeds them up (the text is read as soon as
            //  "Not even" is there), and the text stays: B again until the
            //  text box is gone.
            for (int n = 0; n < 4; n++){
                pbf_press_button(context, BUTTON_B, 160ms, 640ms);
                context.wait_for_all_requests();
                VideoSnapshot screen = console.video().snapshot();
                WhiteDialogDetector white(COLOR_RED);
                if (!screen || !white.detect(screen)){
                    break;
                }
            }
            pbf_wait(context, 300ms);
            context.wait_for_all_requests();
            return CastEnd::NO_BITE;
        case Cast::BITE:
            said = text;
            pbf_wait(context, 200ms);
            //  B, and the wild battle starts (FishingWildEncounter()).
            for (int n = 0; n < 4; n++){
                BlackScreenWatcher battle_starting(COLOR_RED);
                AdvanceBattleDialogWatcher battle_started(COLOR_RED);
                BattleMenuWatcher menu(COLOR_RED);
                context.wait_for_all_requests();
                const int seen = run_until<ProControllerContext>(
                    console, context,
                    [](ProControllerContext& commands){
                        pbf_press_button(commands, BUTTON_B, 160ms, 2840ms);
                    },
                    { battle_starting, battle_started, menu }
                );
                if (seen >= 0){
                    battle = story_battle(rt, kind, where);
                    return battle == StoryBattleResult::NO_BATTLE ? CastEnd::UNCLEAR : CastEnd::BATTLE;
                }
            }
            console.log(where + ": a bite, but no battle after it.", COLOR_ORANGE);
            story_back_to_overworld(rt, "after a bite");
            return CastEnd::UNCLEAR;
        case Cast::OTHER:
            //  Some other text: B on it, a few times.
            if (other_texts < 3){
                console.log(where + ": a text after the cast that is not of fishing: \"" + text + "\". B.", COLOR_ORANGE);
            }
            other_texts++;
            said = text;
            pbf_press_button(context, BUTTON_B, 160ms, 640ms);
            break;
        }
    }
    story_back_to_overworld(rt, "after a cast");
    return CastEnd::UNCLEAR;
}

//  Fishing: the player stands on land and faces the water ("toward"). A
//  try is a cast.
class FishDexSpot : public DexSpot{
public:
    FishDexSpot(StoryRuntime& rt, int rod, DpadPosition toward, const std::string& where)
        : m_rt(rt)
        , m_rod(rod)
        , m_toward(toward)
        , m_where(where)
    {}

    virtual void arrived() override{
        story_face(m_rt, m_toward);
    }
    virtual StoryBattleResult step(StoryBattleKind kind) override{
        if (m_rt.facing != m_toward){
            story_face(m_rt, m_toward);
        }
        m_casts++;
        StoryBattleResult battle = StoryBattleResult::NO_BATTLE;
        std::string said;
        const CastEnd end = cast_rod(m_rt, m_rod, kind, m_where, battle, said);
        const std::string cast = m_where + ": cast " + std::to_string(m_casts);
        switch (end){
        case CastEnd::NO_ROD:
            m_no_rod = true;
            return StoryBattleResult::NO_BATTLE;
        case CastEnd::NOT_HERE:
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::string("The ") + AutoStoryFishing::rod_name(m_rod) + " cannot be cast here (\"" + said +
                "\"): the player does not stand facing the water the route fishes in: " + m_where,
                m_rt.console
            );
        case CastEnd::UNCLEAR:
            m_rt.console.log(cast + ": neither a text nor a battle came of it.", COLOR_ORANGE);
            if (++m_unclear >= 3){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "Three casts in a row brought neither a text nor a battle: " + m_where,
                    m_rt.console
                );
            }
            return StoryBattleResult::NO_BATTLE;
        case CastEnd::NO_BITE:
            m_unclear = 0;
            m_dry++;
            m_rt.console.log(cast + ": no bite (" + std::to_string(m_bites) + " so far).");
            if (AutoStoryFishing::no_bite_at_all(m_dry)){
                //  Half the casts bring one wherever anything bites.
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    std::to_string(m_dry) + " casts in a row without a bite, where every other cast brings one: "
                    "the player does not fish where the route has it: " + m_where,
                    m_rt.console
                );
            }
            return StoryBattleResult::NO_BATTLE;
        case CastEnd::BATTLE:
            m_unclear = 0;
            m_dry = 0;
            m_bites++;
            return battle;
        }
        return StoryBattleResult::NO_BATTLE;
    }
    virtual uint64_t steps() const override{ return m_casts; }
    virtual void check() override{}
    virtual void to_first_tile() override{}

    bool no_rod() const{ return m_no_rod; }
    uint64_t bites() const{ return m_bites; }

private:
    StoryRuntime& m_rt;
    int m_rod;
    DpadPosition m_toward;
    std::string m_where;
    uint64_t m_casts = 0;
    uint64_t m_bites = 0;
    int m_dry = 0;          //  casts in a row without a bite
    int m_unclear = 0;
    bool m_no_rod = false;
};

}



bool story_bag_has(StoryRuntime& rt, int pocket, int rod, const std::string& words, const std::string& what){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    //  One of the words and none of those to avoid: a rod's own, or
    //  "a|b|!c" (c: not with it).
    std::vector<std::string> parts;
    std::vector<const char*> any;
    std::vector<const char*> avoid;
    if (AutoStoryFishing::is_rod(rod)){
        const AutoStoryFishing::RodWords own = AutoStoryFishing::rod_words(rod);
        any = own.words;
        avoid = own.avoid;
    }else{
        size_t start = 0;
        while (start <= words.size()){
            const size_t bar = words.find('|', start);
            parts.emplace_back(words.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
            if (bar == std::string::npos){
                break;
            }
            start = bar + 1;
        }
        for (const std::string& part : parts){
            if (part.size() > 1 && part[0] == '!'){
                avoid.push_back(part.c_str() + 1);
            }else if (!part.empty() && part[0] != '!'){
                any.push_back(part.c_str());
            }
        }
    }
    if (any.empty()){
        console.log("Looking in the bag for " + what + ": no words to find it by.", COLOR_RED);
        return false;
    }
    console.log("Looking in the bag for: " + what);
    open_bag_from_overworld(console, context);
    const BagPocket where = pocket == 1 ? BagPocket::KEY_ITEMS : (pocket == 2 ? BagPocket::POKE_BALLS : BagPocket::ITEMS);
    const bool found = select_bag_item(rt, where, any, avoid, what, true);
    pbf_mash_button(context, BUTTON_B, 3000ms);
    context.wait_for_all_requests();
    story_back_to_overworld(rt, "bag");
    if (found && AutoStoryFishing::is_rod(rod)){
        rt.rods_in_bag.insert(rod);
        rt.rods_missing.erase(rod);
    }
    return found;
}



namespace{

//  A Safari Game that the visit of its water leaves before it is over (build
//  52, experimental): the walk out to the gate (the step's legs_b), where the
//  attendant asks whether the player is going to leave early (FuchsiaCity_
//  SafariZone_Entrance_EventScript_ExitEarly): YES, the balls are given back,
//  two steps down. The scene is read through as the one at the end of a game
//  is (the segment's next step does that one again, and finds the player free).
void story_safari_leave(StoryRuntime& rt, const StoryScript& script, const StoryStep& step){
    ConsoleHandle& console = rt.console;
    const std::string label = step.label;
    const std::vector<int32_t> out = script.leg_list(step.legs_b);
    if (out.empty()){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_safari_leave(): No way out of the Safari Game in the data: " + label,
            console
        );
    }
    console.log(label + ": the visit of the water is over: the Safari Game is left early, out by the gate.", COLOR_BLUE);
    for (int32_t leg : out){
        walk_leg(rt, script, leg);
        if (rt.safari_over){
            //  The last ball went on the way (a Pokemon of the grass that
            //  the table had not): the game ends by itself.
            break;
        }
    }
    story_cutscene(rt, 3000ms, 60s, true, label + " (leaving the Safari Game)");
}

}


void story_dex(StoryRuntime& rt, const StoryScript& script, const StoryStep& step){
    ConsoleHandle& console = rt.console;
    const bool safari = step.p2 > 0 && (step.p2 & 1) != 0;
    //  Fishing (p2 & 4): the rod p4, cast from where the player stands.
    const bool fishing = step.p2 > 0 && (step.p2 & 4) != 0;
    const int rod = fishing ? (int)step.p4 : -1;
    //  Both (build 52): the rod cast at the water of a Safari area, in a Safari
    //  Game. A cast costs no step, and a ball only goes at what is wanted, so
    //  the game does not end by itself: the visit ends as that of any other
    //  place (what is wanted is caught, the minutes are through), and the
    //  player leaves the game early (story_safari_leave()).
    const bool safari_fishing = safari && fishing;
    //  For the POKeDEX table: what it still wants of the list, in its game.
    const bool collect = step.p2 > 0 && (step.p2 & 2) != 0 && rt.collection != nullptr;
    const std::string label = step.label;
    //  A place's list has the species of both games, each with how often
    //  it comes there in FireRed and in LeafGreen. What does not come there
    //  in the game played is not looked for: the other game's species (the
    //  Safari Zone: SCYTHER in FireRed, PINSIR in LeafGreen), and those the
    //  two games have at different places (the Safari Zone's center:
    //  NIDORAN M in FireRed, NIDORAN F in LeafGreen; NIDORAN F is in
    //  FireRed's east and west areas). A list with one of them never got
    //  empty: the place was left only after its battles in a row without a
    //  catch, or after its minutes, and a second Safari Game was played.
    std::set<std::string> listed = story_species_list(step.text);
    std::set<std::string> wanted;
    std::set<std::string> extra;
    //  Collecting: what the place is worth a wait for, and what is only
    //  caught when one comes (rare here, common elsewhere).
    std::set<std::string> looked_for;
    std::set<std::string> in_passing;
    std::string counts;
    if (collect){
        const StoryPlaceWanted place = story_place_wanted(rt, step);
        listed.clear();
        for (const auto& item : place.here){
            listed.insert(item.first);
        }
        wanted = place.wanted;
        looked_for = place.looked_for;
        in_passing = place.in_passing;
        for (const std::string& slug : wanted){
            if (rt.collection->wants_more_than_one(slug)){
                extra.insert(slug);
            }
            counts += (counts.empty() ? "" : ", ") + slug + " " +
                std::to_string(rt.collection->caught(slug)) + "/" + std::to_string(rt.collection->needed(slug));
        }
    }else{
        if (rt.collection != nullptr){
            std::set<std::string> in_game;
            std::set<std::string> not_here;
            const std::map<std::string, int> here = story_place_species(step.text, rt.collection->game());
            for (const std::string& slug : listed){
                (here.count(slug) != 0 && rt.collection->needed(slug) > 0 ? in_game : not_here).insert(slug);
            }
            if (!in_game.empty() && !not_here.empty()){
                console.log(
                    label + ": not here in this game (" + rt.collection->game_name() + "), so not looked for: " +
                    species_names(not_here) + "."
                );
                listed = in_game;
            }
        }
        for (const std::string& slug : listed){
            if (rt.caught.count(slug) == 0){
                wanted.insert(slug);
            }
        }
        looked_for = wanted;
    }
    //  Something the place is looked at for is still on the list.
    auto any_looked_for = [&](){
        for (const std::string& slug : looked_for){
            if (rt.battle.dex_species.count(slug) != 0){
                return true;
            }
        }
        return false;
    };
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
        label + ": for the POKeDEX, " +
        (fishing ? std::string("fishing with the ") + AutoStoryFishing::rod_name(rod) + " for " : std::string("catching ")) +
        (shiny_only ? "shiny " : "") + species_names(looked_for) +
        (collect ? " (" + rt.collection->game_name() + ", the table: " + counts + ")" : std::string()) +
        (safari_fishing ? " (in the Safari Game: until the table has what is fished for, or " +
            std::to_string(minutes_limit) + " minutes are through, or the game is over; then out by the gate)." :
            safari ? " (the Safari Game, until it is over)." :
            shiny_only ? " (all others are run from; at most " + std::to_string(minutes_limit) + " minutes)." :
            " (until " + std::to_string(idle_limit) + " battles in a row bring none, at most " +
            std::to_string(minutes_limit) + " minutes).") +
        (in_passing.empty() ? std::string() :
            " Also caught if one comes, but not waited for (rare here, more common at another place): " +
            species_names(in_passing) + "."),
        COLOR_BLUE
    );
    if (looked_for.empty() && !safari){
        console.log(
            label + (wanted.empty()
                ? ": all of them were caught already."
                : ": nothing left that this place is looked at for."),
            COLOR_BLUE
        );
        return;
    }
    bool no_rod_here = false;
    if (fishing && (!AutoStoryFishing::is_rod(rod) || rt.rods_missing.count(rod) != 0)){
        console.log(
            label + ": the " + AutoStoryFishing::rod_name(rod) + " was not in the bag when it was last looked for: "
            "not fished here.",
            COLOR_ORANGE
        );
        if (!safari){
            return;
        }
        //  (The segment looks for the rod before the game is joined: this is
        //  for a rod that went missing in between.)
        no_rod_here = true;
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
    rt.dex_visits++;
    const uint64_t new_before = rt.dex_new;
    const uint64_t marked_before = rt.dex_owned_by_mark;

    std::unique_ptr<DexSpot> spot_here;
    FishDexSpot* fish = nullptr;
    if (fishing){
        fish = new FishDexSpot(rt, rod, step.dir, label);
        spot_here.reset(fish);
    }else{
        spot_here.reset(new GrassDexSpot(rt, step.dir, std::max(2, (int)(step.p4 & 0xff)), label));
    }
    DexSpot& spot = *spot_here;
    spot.arrived();
    int idle = 0;
    int battles = 0;
    size_t caught_here = 0;
    const WallClock start = current_time();
    //  A Safari Game ends by itself: after 600 steps, or with the last of
    //  its 30 balls. How long the steps take depends on the battles on the
    //  way, which take 20 seconds each also when they are only run from. In
    //  the east area on 14 November, 61 to 64 battles a game left some 500
    //  steps on the grass in 30 minutes (15 to 60 short of the end, with
    //  those of the way there), and a limit of 30 minutes ended four games
    //  in a row, and with the fourth the program. The limit is for a game
    //  that does not end at all.
    const std::chrono::minutes safari_limit(75);
    const WallClock deadline = start + (safari && !fishing ? safari_limit : std::chrono::minutes(minutes_limit));
    //  No wild Pokemon at all: the player is not on the grass. (Canyon
    //  Entrance, 16 November 2026: the walk there ended a row above the
    //  grass, and the row was walked back and forth for the 30 minutes of
    //  the visit, 1,413 steps without a battle; four times in a row, and
    //  the fourth error on the way back ended the program.) On the rarest
    //  ground of the game (the first room of the Lost Cave: 1 step in 100,
    //  and more with every step without one, StandardWildEncounter()) 400
    //  steps go by without a wild Pokemon once in some 3,000 visits; on
    //  water with its 2 in 100 once in ten million, on grass never. No
    //  REPEL is on at these steps (the generator's no_repel_here()).
    const std::chrono::minutes nothing_limit(6);
    const uint64_t nothing_steps = 400;
    try{
        while (current_time() < deadline && !rt.safari_over && !no_rod_here){
            if (safari_fishing
                ? (!any_looked_for() || idle >= idle_limit)
                : (!safari && (!any_looked_for() || idle >= idle_limit || rt.no_balls))
            ){
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
            if (fish != nullptr && fish->no_rod()){
                //  Nothing to fish with: the places of this rod are left
                //  out until the way to it has been gone again.
                rt.rods_in_bag.erase(rod);
                rt.rods_missing.insert(rod);
                console.log(
                    label + ": the " + AutoStoryFishing::rod_name(rod) + " is not in the bag (" +
                    AutoStoryFishing::rod_giver(rod) + " gives it). The places fished with it are left out for now.",
                    COLOR_RED
                );
                break;
            }
            if (result == StoryBattleResult::NO_BATTLE){
                //  (Casts without a bite have a limit of their own.)
                if (!safari && !fishing && battles == 0 && spot.steps() >= nothing_steps &&
                    current_time() - start > nothing_limit
                ){
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::SEND_ERROR_REPORT,
                        "story_dex(): No wild Pokemon in " + std::to_string((int)nothing_limit.count()) +
                        " minutes and " + std::to_string(spot.steps()) + " steps: the player is not on the grass "
                        "(or the water, or the cave floor) that the route walks here: " + label,
                        console
                    );
                }
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
            if (collect && !safari && any_looked_for() &&
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
    //  What the place was looked at for and did not give, and what was
    //  not waited for here and did not come.
    std::set<std::string> left;
    std::set<std::string> left_in_passing;
    for (const std::string& slug : rt.battle.dex_species){
        (in_passing.count(slug) != 0 ? left_in_passing : left).insert(slug);
    }
    //  (A game that is left early is still on while the way out is walked.)
    const bool leave_now = safari_fishing && !rt.safari_over;
    if (!leave_now){
        rt.in_safari = false;
    }
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
        (left_in_passing.empty() ? std::string() :
            " Not waited for here: " + species_names(left_in_passing) + ".") +
        (rt.no_balls ? " No balls left." : "") +
        (safari && !fishing ? " " + std::to_string(spot.steps()) + " steps on the grass (the game ends after 600, with those "
            "of the way there)." : std::string()) +
        (fish != nullptr ? " " + std::to_string(spot.steps()) + " casts, " + std::to_string(fish->bites()) + " bites."
            : std::string()),
        COLOR_BLUE
    );
    if (leave_now){
        story_safari_leave(rt, script, step);
        rt.in_safari = false;
        return;
    }
    if (safari){
        if (!rt.safari_over){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_dex(): The Safari Game did not end in " + std::to_string(safari_limit.count()) +
                " minutes: " + label,
                console
            );
        }
        //  The PA's text and the walk out to the gate: the next steps.
        return;
    }
    spot.to_first_tile();
}



namespace{

bool laps_any_looked_for(const StoryRuntime& rt){
    for (const std::string& slug : rt.laps.looked_for){
        if (rt.battle.dex_species.count(slug) != 0){
            return true;
        }
    }
    return false;
}

void laps_mark_lap(StoryRuntime& rt){
    rt.laps.wild_before_lap = rt.wild_battles;
    rt.laps.catches_before_lap = rt.dex_catches;
    rt.laps.left_before_lap = rt.battle.dex_species.size();
}

}

bool story_laps_begin(StoryRuntime& rt, const StoryStep& step){
    ConsoleHandle& console = rt.console;
    const std::string label = step.label;
    rt.laps = StoryLaps();
    if (rt.collection == nullptr){
        console.log(label + ": no POKeDEX table: the laps are left out.", COLOR_ORANGE);
        return false;
    }
    const StoryPlaceWanted place = story_place_wanted(rt, step);
    std::set<std::string> listed;
    for (const auto& item : place.here){
        listed.insert(item.first);
    }
    std::set<std::string> extra;
    std::string counts;
    for (const std::string& slug : place.wanted){
        if (rt.collection->wants_more_than_one(slug)){
            extra.insert(slug);
        }
        counts += (counts.empty() ? "" : ", ") + slug + " " +
            std::to_string(rt.collection->caught(slug)) + "/" + std::to_string(rt.collection->needed(slug));
    }
    if (place.looked_for.empty()){
        console.log(
            label + (place.wanted.empty()
                ? ": all of them were caught already."
                : ": nothing left that this place is looked at for."),
            COLOR_BLUE
        );
        return false;
    }
    //  "Shiny only", as in story_dex(): only shiny ones of the wanted
    //  species, for a while.
    const bool shiny_only = rt.collect_shiny;
    if (shiny_only && !rt.shiny_watch){
        throw UserSetupError(console, "\"Shiny only\" needs the shiny sound: the program could not listen to the game.");
    }
    const int idle_limit = shiny_only ? 1000000 : std::max(1, (int)step.p0);
    const int minutes_limit = shiny_only ? std::max(1, rt.collect_shiny_minutes) : std::max(1, (int)step.p1);
    console.log(
        label + ": for the POKeDEX, laps of the next " + std::to_string((int)step.p4) + " steps, catching " +
        (shiny_only ? "shiny " : "") + species_names(place.looked_for) + " on the way (" +
        rt.collection->game_name() + ", the table: " + counts + "; the others are run from; " +
        (shiny_only ? std::string() : "until " + std::to_string(idle_limit) + " battles in a row bring none, ") +
        "at most " + std::to_string(minutes_limit) + " minutes, and a lap under way is walked to its end)." +
        (place.in_passing.empty() ? std::string() :
            " Also caught if one comes, but not waited for: " + species_names(place.in_passing) + "."),
        COLOR_BLUE
    );

    StoryLaps& laps = rt.laps;
    laps.on = true;
    laps.label = label;
    laps.saved = rt.battle;
    laps.looked_for = place.looked_for;
    laps.in_passing = place.in_passing;
    laps.idle_limit = idle_limit;
    laps.start = current_time();
    laps.deadline = laps.start + std::chrono::minutes(minutes_limit);
    laps.new_before = rt.dex_new;
    laps.marked_before = rt.dex_owned_by_mark;

    rt.battle.dex_species = place.wanted;
    rt.battle.dex_all = listed;
    rt.battle.dex_extra = extra;
    rt.battle.dex_shiny_only = shiny_only;
    rt.battle.catch_species.clear();
    //  What is not wanted is run from, as in a visit of the grass.
    rt.battle.wild_run = true;
    rt.collecting = true;
    rt.in_safari = false;
    rt.safari_over = false;
    rt.no_balls = false;
    rt.dex_visits++;
    laps_mark_lap(rt);
    return true;
}

bool story_laps_again(StoryRuntime& rt){
    StoryLaps& laps = rt.laps;
    if (!laps.on){
        return false;
    }
    ConsoleHandle& console = rt.console;
    laps.laps++;
    const int battles = (int)(rt.wild_battles - laps.wild_before_lap);
    const uint64_t catches = rt.dex_catches - laps.catches_before_lap;
    laps.battles += battles;
    laps.caught += (size_t)catches;
    if (catches > 0 || rt.battle.dex_species.size() < laps.left_before_lap){
        laps.idle = 0;      //  A catch, or one found caught before (the ball mark).
    }else{
        laps.idle += battles;
    }
    console.log(
        laps.label + ": lap " + std::to_string(laps.laps) + " brought " + std::to_string(battles) +
        (battles == 1 ? " wild battle" : " wild battles") +
        (catches > 0 ? " and " + std::to_string(catches) + (catches == 1 ? " catch." : " catches.") : std::string(".")) +
        " Still looked for: " + species_names(rt.battle.dex_species) + "."
    );
    std::string why;
    if (!laps_any_looked_for(rt)){
        why = "nothing left that the place is looked at for";
    }else if (laps.idle >= laps.idle_limit){
        why = std::to_string(laps.idle) + " battles in a row without a catch";
    }else if (rt.no_balls){
        why = "no ball left for a wanted species";
    }else if (current_time() >= laps.deadline){
        why = "the minutes of this visit are over";
    }else if (rt.shiny_unsaved){
        why = "a shiny Pokemon is caught: to the nurse first, to save it";
    }else if (rt.lead_fainted || (rt.memory.player_hp >= 0 && rt.memory.player_hp < 0.35)){
        why = rt.lead_fainted ? std::string("the lead fainted")
            : "the lead is at " + std::to_string((int)(rt.memory.player_hp * 100 + 0.5)) + "% HP";
    }
    if (!why.empty()){
        console.log(laps.label + ": no more laps: " + why + ".", COLOR_BLUE);
        return false;
    }
    //  A lap is some sixty tiles of water, or six rocks: a dozen of them
    //  without a single wild Pokemon is not chance (once in 20,000 on the
    //  water with the fewest).
    if (laps.battles == 0 && laps.laps >= 12){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_laps_again(): " + std::to_string(laps.laps) + " laps without a single wild Pokemon: the player "
            "is not where the route walks them: " + laps.label,
            console
        );
    }
    laps_mark_lap(rt);
    return true;
}

void story_laps_end(StoryRuntime& rt, bool failed){
    StoryLaps& laps = rt.laps;
    if (!laps.on){
        return;
    }
    std::set<std::string> left;
    std::set<std::string> left_in_passing;
    for (const std::string& slug : rt.battle.dex_species){
        (laps.in_passing.count(slug) != 0 ? left_in_passing : left).insert(slug);
    }
    const bool no_balls = rt.no_balls;
    rt.collecting = false;
    rt.battle = laps.saved;
    laps.on = false;
    if (failed){
        return;
    }
    if (no_balls){
        //  The collection: no ball for a wanted species.
        rt.short_of_balls = true;
    }
    const int minutes = (int)std::chrono::duration_cast<std::chrono::minutes>(current_time() - laps.start).count();
    const uint64_t marked = rt.dex_owned_by_mark - laps.marked_before;
    rt.console.log(
        laps.label + ": " + std::to_string(laps.caught) + " caught (" +
        std::to_string(rt.dex_new - laps.new_before) + " new to the POKeDEX)" +
        (marked > 0 ? ", " + std::to_string(marked) + " found caught before (the ball mark)" : std::string()) +
        " in " + std::to_string(laps.battles) + " battles, " + std::to_string(laps.laps) + " laps, " +
        std::to_string(minutes) + " minutes. Not caught: " + species_names(left) + "." +
        (left_in_passing.empty() ? std::string() :
            " Not waited for here: " + species_names(left_in_passing) + ".") +
        (no_balls ? " No balls left." : ""),
        COLOR_BLUE
    );
}



void story_seagallop(StoryRuntime& rt, int row, const std::string& what){
    using AutoStoryFerryTalk::Act;
    using AutoStoryFerryTalk::Look;
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
    //  Build 54 (experimental): 100 and more is a row of the tickets' menu
    //  itself (101: NAVEL ROCK, 102: BIRTH ISLAND): that goes to the island
    //  at once, there is no Sevii Islands list after it.
    const bool ticket_mode = row >= 100;
    const int ticket_row = ticket_mode ? row - 100 : 0;
    const int list_row = ticket_mode ? 0 : row;
    const bool read_menus = ticket_mode || list_row != 0;
    console.log(
        ticket_mode
            ? "The Seagallop from Vermilion City: row " + std::to_string(ticket_row) + " of the tickets' menu (" + what + ")."
            : "The Seagallop from Vermilion City: row " + std::to_string(row) + " of the Sevii Islands list (" + what + ")."
    );
    //  The sailor's text is blue, and the text reading takes dark text
    //  only: on 15 November 2026 his first page ("Oh! That's a
    //  MYSTICTICKET! Now that is rare.") stood there until "The sailor's
    //  text did not come up", three times from the same save. His pages
    //  are told by the text box and its advance arrow instead
    //  (AutoStoryFerryTalk).
    //
    //  For the list's first row nothing has to be read: A is right on
    //  every page and on both menus (SEVII ISLANDS, ONE ISLAND). Another
    //  row needs the menus read (their text is dark), and the last page of
    //  a text about a ticket would wait: no route asks for one.
    AutoStoryFerryTalk::Talk talk;
    bool picked = false;
    const WallClock deadline = current_time() + 90s;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        if (read_menus && !picked){
            const std::string tickets = read_story_text(console, TICKET_MENU, false);
            if (has(tickets, "sevii") || has(tickets, "navel") || has(tickets, "birth")){
                if (ticket_mode){
                    console.log(
                        std::string("The tickets' menu: ") + (ticket_row == 1 ? "NAVEL ROCK" : "BIRTH ISLAND") + ".",
                        COLOR_BLUE
                    );
                    for (int i = 0; i < ticket_row; i++){
                        pbf_press_dpad(context, DPAD_DOWN, 160ms, 340ms);
                    }
                    pbf_press_button(context, BUTTON_A, 160ms, 1340ms);
                    talk.pressed++;
                    picked = true;
                    continue;
                }
                console.log("The tickets' menu: SEVII ISLANDS.", COLOR_BLUE);
                pbf_press_button(context, BUTTON_A, 160ms, 1340ms);
                talk.pressed++;
                continue;
            }
            //  (The list's box also takes in the right half of the tickets'
            //  menu: "ISLANDS", "ROCK", "ISLAND".)
            const std::string islands = read_story_text(console, ISLAND_MENU, false);
            if (!ticket_mode && has(islands, "island") && (has(islands, "other") || has(islands, "exit")) && !has(islands, "rock")){
                console.log("The Sevii Islands list: row " + std::to_string(row) + ".", COLOR_BLUE);
                for (int i = 0; i < row; i++){
                    pbf_press_dpad(context, DPAD_DOWN, 160ms, 340ms);
                }
                pbf_press_button(context, BUTTON_A, 160ms, 1340ms);
                talk.pressed++;
                picked = true;
                continue;
            }
        }
        VideoSnapshot screen = console.video().snapshot();
        AdvanceWhiteDialogDetector arrow(COLOR_RED);
        WhiteDialogDetector box(COLOR_RED);
        Look look = Look::NO_BOX;
        if (screen){
            if (arrow.detect(screen)){
                look = Look::ARROW;
            }else if (box.detect(screen)){
                look = Look::BOX;
            }
        }
        switch (AutoStoryFerryTalk::next(talk, look, !read_menus || picked)){
        case Act::PRESS_A:
            //  What he says, for the log only.
            console.log(
                "The sailor's text, A number " + std::to_string(talk.pressed) +
                (look == Look::ARROW
                    ? " (the advance arrow)"
                    : " (no arrow: the last page of a text, or a menu at its first row)") +
                ": \"" + story_read_people(rt) + "\"",
                COLOR_BLUE
            );
            pbf_press_button(context, BUTTON_A, 160ms, 1040ms);
            break;
        case Act::WAIT:
            pbf_wait(context, 500ms);
            break;
        case Act::LEFT:
            if (talk.long_enough()){
                console.log(
                    "The sailor's text box is gone after " + std::to_string(talk.pressed) +
                    " presses: the Seagallop leaves.",
                    COLOR_BLUE
                );
            }else{
                console.log(
                    "The sailor's text box is gone after " + std::to_string(talk.pressed) +
                    " presses, fewer than a departure takes: taking it that the Seagallop leaves all the same.",
                    COLOR_ORANGE
                );
            }
            return;
        case Act::NO_TEXT:
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_seagallop(): The sailor's text did not come up: " + what,
                console
            );
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "story_seagallop(): Not on the Seagallop after 90 seconds (" + std::to_string(talk.pressed) +
        " presses on the sailor's text): " + what,
        console
    );
}



namespace{

enum class SeekerUse{
    NOT_IN_BAG,
    NOT_CHARGED,    //  "The battery isn't charged enough."
    NO_ONE,         //  "There are no TRAINERS within range who can battle..."
    NOT_READY,      //  "The other TRAINERS don't appear to be ready for battle."
    USED,           //  no text: someone wants a battle (or was never beaten)
};

//  The VS SEEKER from the bag, used where the player stands
//  (FieldUseFunc_VsSeeker: the bag closes, Task_VsSeeker_0). The player is
//  free again afterwards.
SeekerUse use_vs_seeker(StoryRuntime& rt, const std::string& what){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    open_bag_from_overworld(console, context);
    //  "A device that indicates TRAINERS who want to battle. The battery
    //  charges while traveling."
    if (!select_bag_item(
        rt, BagPocket::KEY_ITEMS,
        {"indicates trainers", "want to battle", "battery charges", "while traveling"}, {}, "VS SEEKER", true
    )){
        pbf_mash_button(context, BUTTON_B, 3000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, "bag");
        return SeekerUse::NOT_IN_BAG;
    }
    //  A: USE / REGISTER / CANCEL, cursor on USE. A: the bag closes, and
    //  on the map the player holds the VS SEEKER up (or a text says why
    //  not). Trainers in reach answer with a mark over their heads; when
    //  none of them wants a battle, a text says so after that.
    press(context, BUTTON_A, 800ms);
    press(context, BUTTON_A, 3500ms);
    context.wait_for_all_requests();
    SeekerUse result = SeekerUse::USED;
    for (int look = 0; look < 2; look++){
        const std::string text = story_read_dialog(rt);
        //  (Not "battery" alone: the VS SEEKER's own description in the bag
        //  has that word.)
        if (contains(text, "charged enough") || contains(text, "isn't charged") ||
            contains(text, "steps required") || contains(text, "fully charge")
        ){
            result = SeekerUse::NOT_CHARGED;
        }else if (contains(text, "within range") || contains(text, "no trainers") || contains(text, "turned off")){
            result = SeekerUse::NO_ONE;
        }else if (contains(text, "ready for") || contains(text, "appear") || contains(text, "other trainers")){
            result = SeekerUse::NOT_READY;
        }
        if (result != SeekerUse::USED){
            console.log("The VS SEEKER (" + what + "): \"" + text + "\"");
            break;
        }
        if (look == 0){
            pbf_wait(context, 2500ms);
            context.wait_for_all_requests();
        }
    }
    if (result == SeekerUse::USED){
        //  (GetVsSeekerResponseInArea(): a text only when no trainer in
        //  reach wants a battle and none of them is one never beaten.)
        console.log(
            "The VS SEEKER (" + what + "): no text after it. A trainer wants a battle, or one in reach was "
            "never beaten."
        );
    }
    //  B closes its text (two pages), and does nothing without one.
    pbf_mash_button(context, BUTTON_B, 2500ms);
    context.wait_for_all_requests();
    story_back_to_overworld(rt, "VS SEEKER");
    return result;
}

}

int story_rematch(StoryRuntime& rt, const StoryScript& script, const StoryStep& step){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    const std::string label = step.label;
    const int wanted = std::max(1, (int)step.p0);
    const auto limit = std::chrono::minutes(std::max(5, (int)step.p1));
    const int prize = std::max(0, (int)step.p2);
    const std::vector<int32_t> walk = script.leg_list(step.legs_a);
    if (walk.empty()){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_rematch(): No walk for the VS SEEKER in the step's data: " + label,
            console
        );
    }
    console.log(
        label + ": prize money. Up to " + std::to_string(wanted) + " battles with the trainer here (" +
        std::to_string(prize) + " each): 100 steps and more, the VS SEEKER, A on the trainer, again and again. "
        "The trainer wants a battle 7 times in 10.",
        COLOR_BLUE
    );
    const WallClock start = current_time();
    int wins = 0;
    int uses = 0;
    int rounds = 0;
    int dry = 0;                //  rounds in a row without a battle
    bool have_seeker = true;
    bool lead_down = false;
    std::string stopped;
    while (wins < wanted){
        if (current_time() - start > limit){
            stopped = "the " + std::to_string((int)step.p1) + " minutes of this step are over";
            break;
        }
        if (dry >= 8){
            stopped = "eight rounds in a row without a battle";
            break;
        }
        rounds++;
        //  The battery: 100 steps with the VS SEEKER in the bag.
        for (int32_t leg : walk){
            rt.walking_leg = leg;
            walk_leg(rt, script, leg);
        }
        rt.walking_leg = -1;
        //  At the hundredth step after a use that found someone, the game
        //  stops for a moment and the trainers stand as before
        //  (EventScript_VsSeekerChargingDone).
        pbf_wait(context, 1000ms);
        context.wait_for_all_requests();

        SeekerUse use = SeekerUse::NOT_IN_BAG;
        if (have_seeker){
            use = use_vs_seeker(rt, label);
            if (use == SeekerUse::NOT_IN_BAG){
                have_seeker = false;
                console.log(
                    label + ": no VS SEEKER in the bag (the woman left of the nurse in the Vermilion City "
                    "Pokemon Center gives it). Without it a trainer battles only once.",
                    COLOR_RED
                );
            }else{
                uses++;
            }
        }

        //  A on the trainer: the words before a battle, or a line of text.
        story_talk_start(rt, label);
        //  What the trainer says, for the log only: the words before a
        //  rematch, or the line of one who does not want it. (16 November
        //  2026: one battle, then eight rounds in a row without, where the
        //  game gives 7 in 10 each time (vs_seeker.c) - once in 15,000 by
        //  chance - and the rounds looked the same as those with a battle
        //  in every line of the log.)
        pbf_wait(context, 600ms);
        context.wait_for_all_requests();
        console.log(label + ": the trainer says: \"" + story_read_people(rt) + "\"");
        bool battle = false;
        for (int part = 0; part < 4; part++){
            if (!story_cutscene(rt, 0ms, 60s, false, label)){
                break;
            }
            const StoryBattleResult result = story_battle(rt, StoryBattleKind::TRAINER, label);
            if (result == StoryBattleResult::NO_BATTLE){
                break;
            }
            battle = true;
            lead_down |= result == StoryBattleResult::LEAD_FAINTED;
        }
        if (battle){
            wins++;
            rt.trip_wins++;
            dry = 0;
            console.log(
                label + ": battle " + std::to_string(wins) + " of " + std::to_string(wanted) + " won (round " +
                std::to_string(rounds) + "): " + std::to_string(prize) + " more. Battles won on this trip: " +
                std::to_string(rt.trip_wins) + ".",
                COLOR_BLUE
            );
            story_note(rt, label + ": " + std::to_string(wins) + " of " + std::to_string(wanted) + " battles won");
        }else{
            dry++;
            console.log(
                label + ": no battle this time" +
                (use == SeekerUse::NOT_CHARGED ? " (the VS SEEKER was not charged yet)" :
                 use == SeekerUse::NO_ONE ? " (the VS SEEKER found no trainer in reach)" :
                 use == SeekerUse::NOT_READY ? " (the trainer was not ready)" : "") + "."
            );
            //  A trainer who was never beaten battles at the first talk,
            //  and one who was wants to 7 times in 10. Three rounds without
            //  a battle before anything was won on this trip: the player is
            //  not at her side. (The night of 15 to 16 November 2026: the
            //  way over the water ended a row too low, the player stood
            //  somewhere on the beach, and eight rounds went by, 16
            //  minutes, before the step gave up.)
            if (rt.trip_wins == 0 && dry >= 3){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "story_rematch(): No battle in the first " + std::to_string(dry) +
                    " rounds, and nothing won on this trip yet: the player is not at the trainer's side: " + label,
                    console
                );
            }
            //  At her side the VS SEEKER always has her in reach.
            if (use == SeekerUse::NO_ONE){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "story_rematch(): The VS SEEKER found no trainer in reach, and no battle came: "
                    "the player is not at the trainer's side: " + label,
                    console
                );
            }
        }
        if (lead_down){
            stopped = "the lead fainted";
            break;
        }
        if (!have_seeker){
            stopped = "no VS SEEKER";
            break;
        }
    }
    const auto took = std::chrono::duration_cast<std::chrono::minutes>(current_time() - start).count();
    console.log(
        label + ": " + std::to_string(wins) + " battles won in " + std::to_string(took) + " minutes (" +
        std::to_string(uses) + " uses of the VS SEEKER): " + std::to_string((long long)wins * prize) +
        " prize money." + (stopped.empty() ? std::string() : " Stopped: " + stopped + "."),
        wins >= wanted ? COLOR_BLUE : COLOR_ORANGE
    );
    return wins;
}


//  ---- The Day Care (build 48, experimental) ---------------------------------

namespace{

//  A press of A every 2.7 seconds (the text of the box is read in between)
//  until a YES/NO box is on the screen: true. (PA's own egg pickup of the Egg
//  RNG does the same: egg_pickup().)
bool daycare_prompt(StoryRuntime& rt, int presses, Milliseconds last = 2700ms){
    SelectionDialogWatcher prompt(COLOR_RED);
    rt.context.wait_for_all_requests();
    const int ret = run_until<ProControllerContext>(
        rt.console, rt.context,
        [&](ProControllerContext& context){
            for (int i = 0; i < presses; i++){
                pbf_wait(context, 2700ms);
                pbf_press_button(context, BUTTON_A, 160ms, 140ms);
            }
            pbf_wait(context, last);
        },
        { prompt }
    );
    return ret >= 0;
}

//  What the text of one of the Day Care's people came to.
enum class DaycareText{
    PROMPT,     //  A YES/NO box is on the screen.
    OVER,       //  The text box is gone, and did not come back: the talk is over.
    NONE,       //  Neither, in all the presses.
};

//  Like daycare_prompt(), for a text that may also end without a question
//  (the man: "Your POKeMON are doing fine." and the compatibility text when
//  no egg waits, but five pages and a question when one does). The press of A
//  stops with the box: never a B here, for B on the question of the egg is NO.
//  A box that is gone is looked at again for "confirm" (the woman walks away
//  and back, in the middle of her talk, with no box on the screen).
DaycareText daycare_text(StoryRuntime& rt, int presses, Milliseconds confirm = 2000ms){
    for (int again = 0; again < 6; again++){
        SelectionDialogWatcher prompt(COLOR_RED);
        WhiteDialogOverWatcher over(COLOR_RED);
        rt.context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            rt.console, rt.context,
            [&](ProControllerContext& context){
                for (int i = 0; i < presses; i++){
                    pbf_wait(context, 2700ms);
                    pbf_press_button(context, BUTTON_A, 160ms, 140ms);
                }
                pbf_wait(context, 2700ms);
            },
            { prompt, over }
        );
        if (ret == 0){
            return DaycareText::PROMPT;
        }
        if (ret < 0){
            return DaycareText::NONE;
        }
        WhiteDialogWatcher back(COLOR_RED);
        rt.context.wait_for_all_requests();
        const int shown = run_until<ProControllerContext>(
            rt.console, rt.context,
            [&](ProControllerContext& context){
                pbf_wait(context, confirm);
            },
            { back }
        );
        if (shown < 0){
            return DaycareText::OVER;
        }
        rt.console.log("The Day Care: a text box is on the screen again; the talk goes on.", COLOR_ORANGE);
    }
    return DaycareText::NONE;
}

//  The box is answered: B (NO, and what closes a text) until the text box
//  is gone.
void daycare_close(StoryRuntime& rt, const std::string& what, Milliseconds longest = 10000ms){
    WhiteDialogOverWatcher over(COLOR_RED);
    rt.context.wait_for_all_requests();
    const int ret = run_until<ProControllerContext>(
        rt.console, rt.context,
        [&](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, longest);
        },
        { over }
    );
    rt.context.wait_for_all_requests();
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "The Day Care: the text box did not go away after B for " + std::to_string(longest.count() / 1000) +
            " seconds: " + what,
            rt.console
        );
    }
    rt.facing = DPAD_NONE;
}

//  The party's slot in the Day Care's menu ("Which POKeMON should we
//  raise?"): the cursor starts on the first place; DOWN to "slot", A, STORE.
void daycare_pick(StoryRuntime& rt, const std::string& what, int slot, int party_size){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    PartyMenuWatcher party(COLOR_RED);
    context.wait_for_all_requests();
    const int ret = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& ctx){
            //  A closes "Which POKeMON should we raise?"; the menu follows
            //  after a fade. (A on the menu itself would open a place: the
            //  watcher stops this at the first look at it.)
            for (int i = 0; i < 4; i++){
                pbf_wait(ctx, 2200ms);
                pbf_press_button(ctx, BUTTON_A, 160ms, 140ms);
            }
            pbf_wait(ctx, 2200ms);
        },
        { party }
    );
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_daycare_give(): The party menu did not come up after the woman's question: " + what,
            console
        );
    }
    pbf_wait(context, 800ms);
    context.wait_for_all_requests();
    const int count = (int)detect_last_occupied_party_slot(console) + 1;
    if (count != party_size){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_daycare_give(): The party menu shows " + std::to_string(count) + " Pokemon, not " +
            std::to_string(party_size) + ": " + what,
            console
        );
    }
    for (int i = 1; i < slot; i++){
        dpad(context, DPAD_DOWN, 300ms);
    }
    press(context, BUTTON_A, 900ms);        //  The places: STORE, SUMMARY, CANCEL.
    press(context, BUTTON_A, 1500ms);       //  STORE.
    context.wait_for_all_requests();
}

}


void story_daycare_walk(
    StoryRuntime& rt, DpadPosition direction, int length, int steps, const std::string& what
){
    ConsoleHandle& console = rt.console;
    if (length < 3 || direction == DPAD_NONE){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_daycare_walk(): The step has no line to walk along: " + what,
            console
        );
    }
    //  As story_pace(): along the line between two walls, B held, each way a
    //  little longer than the line, so it ends bumping into the wall. A bump
    //  is no step: a leg is length - 1 steps. An even number of legs: the
    //  walk ends where it started.
    const DpadPosition back = opposite(direction);
    const int64_t per_leg = length - 1;
    const Milliseconds leg = Milliseconds((int64_t)((8 + 8 * (length - 1) + 12) * 1000 / 60));
    int64_t legs = ((int64_t)steps + per_leg - 1) / per_leg;
    legs += legs % 2;
    console.log(
        what + ": " + std::to_string(legs * per_leg) + " steps along " + std::to_string(length) +
        " tiles (the parents at the Day Care make an egg at every 256 of them; experimental).",
        COLOR_BLUE
    );
    AutoStoryScroll::Screen reference;
    const bool check = story_take_picture(rt, reference);
    int hits = 0;
    for (;;){
        WhiteDialogWatcher dialog(COLOR_RED);
        BlackScreenWatcher black(COLOR_RED);
        rt.context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            console, rt.context,
            [&](ProControllerContext& context){
                for (int64_t i = 0; i < legs; i++){
                    pbf_controller_state(context, BUTTON_B, i % 2 == 0 ? direction : back, {0, 0}, {0, 0}, leg);
                    pbf_wait(context, 50ms);
                }
                context.wait_for_all_requests();
            },
            { dialog, black }
        );
        rt.facing = DPAD_NONE;
        if (ret < 0){
            break;
        }
        //  Nothing on the line starts a text box or a black screen: once or
        //  twice back to the start of the line (the wall behind it closes
        //  nothing, B does) and again; three in a row: an error.
        hits++;
        if (hits >= 3){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_daycare_walk(): A text box or a black screen came up " + std::to_string(hits) +
                " times in a row on the line: " + what,
                console
            );
        }
        console.log(
            what + ": a text box or a black screen came up (" + std::to_string(hits) + " in a row): back to the "
            "start of the line, and again.",
            COLOR_ORANGE
        );
        pbf_mash_button(rt.context, BUTTON_B, 1500ms);
        pbf_controller_state(rt.context, BUTTON_B, back, {0, 0}, {0, 0}, leg);
        pbf_wait(rt.context, 500ms);
        rt.context.wait_for_all_requests();
    }
    if (check){
        AutoStoryScroll::Screen now;
        if (story_take_picture(rt, now)){
            const AutoStoryScroll::TileOffset offset = AutoStoryScroll::tile_offset(reference, now, 2);
            if (offset.clear && (offset.x != 0 || offset.y != 0)){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "story_daycare_walk(): The player is " + offset.summary() + " from where the walk started: " + what,
                    console
                );
            }
        }
    }
}

void story_daycare_give(StoryRuntime& rt, const std::string& label){
    ConsoleHandle& console = rt.console;
    if (rt.dc_parents[0].empty() || rt.dc_parents[1].empty()){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_daycare_give(): The parents were not brought into the party (the step before this trip did not "
            "run): " + label,
            console
        );
    }
    const int before = story_party_count(rt);
    if (before != 6){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_daycare_give(): The party has " + std::to_string(before) + " Pokemon, not six (the team and the "
            "two parents): " + label,
            console
        );
    }
    //  The parents are the party's fifth and sixth place; after the first
    //  one is left, the other one is the fifth.
    for (int k = 0; k < 2; k++){
        if (k == 0){
            //  "Would you like us to raise a POKeMON?" YES.
            story_talk_answer(rt, true, label + ": the woman of the Day Care");
        }else{
            //  "We can raise one more POKeMON. Would you like us to?" comes
            //  by itself after the first one is taken in.
            if (!daycare_prompt(rt, 5)){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "story_daycare_give(): The woman did not ask for a second Pokemon: " + label,
                    console
                );
            }
            pbf_wait(rt.context, 300ms);
            pbf_press_button(rt.context, BUTTON_A, 160ms, 840ms);
            rt.context.wait_for_all_requests();
        }
        console.log(
            label + ": " + upper_name(rt.dc_parents[k]) + " is chosen (party slot 5, " + std::to_string(6 - k) +
            " Pokemon in the party).",
            COLOR_BLUE
        );
        daycare_pick(rt, label, 5, 6 - k);
    }
    //  "We'll raise your X." and the end of the talk.
    daycare_close(rt, label + ": the end of the woman's talk");
    const int after = story_party_count(rt);
    if (after != before - 2){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_daycare_give(): The party has " + std::to_string(after) + " Pokemon after the two were given, "
            "not " + std::to_string(before - 2) + ": " + label,
            console
        );
    }
    //  The team is still the team (the places that were chosen were the fifth
    //  and the sixth): each place that is left holds a member of it.
    for (int slot = 1; slot <= after; slot++){
        std::string name;
        const int level = story_read_slot(rt, slot, &name);
        if (level < 0 || name.empty()){
            continue;
        }
        bool of_team = false;
        for (const StoryTeamMember& member : rt.team){
            of_team |= member.line.count(name) != 0;
        }
        //  (EEVEE, build 54: the team's place that the parent came from holds the
        //  one that was changed with it, until the parents are back.)
        if (!of_team && !(rt.dc_team_slot > 0 && slot == rt.dc_team_slot)){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_daycare_give(): Party slot " + std::to_string(slot) + " is " + upper_name(name) +
                ", not one of the team: the wrong Pokemon was given to the Day Care: " + label,
                console
            );
        }
    }
    rt.dc_party = after;
    rt.dc_given = true;
    rt.egg_party = after;       //  (StepType::HATCH looks for one more than this.)
    console.log(
        label + ": " + upper_name(rt.dc_parents[0]) + " and " + upper_name(rt.dc_parents[1]) + " are at the Day Care; "
        "the party has the team (" + std::to_string(after) + ").",
        COLOR_BLUE
    );
}

bool story_daycare_man(StoryRuntime& rt, const std::string& label, bool reject, const StoryTalkAlt& alt){
    ConsoleHandle& console = rt.console;
    if (!rt.dc_given){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_daycare_man(): The parents were not given to the Day Care: " + label,
            console
        );
    }
    //  A press of A in front of the tile he stands on when an egg waits; one
    //  step up, and again, when he is not there (he stands where he always
    //  does: no egg was made since the map was loaded).
    story_talk_start(rt, label, alt);
    //  With an egg: five pages ("Ah, it's you! We were raising your POKeMON,
    //  and ... Your POKeMON had an EGG! ... You do want it?") and YES/NO. A
    //  press of A for each page, and the same again for the pages that were
    //  still being written (build 48 pressed twice, and the B that closed
    //  the box then answered NO to the egg).
    //  Without: "Your POKeMON are doing fine." and the compatibility text,
    //  and the box is gone: no B is needed.
    const DaycareText text = daycare_text(rt, 12);
    if (text == DaycareText::NONE){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_daycare_man(): The man's text went on, with no question and no end, in 12 presses of A: " + label,
            console
        );
    }
    if (text == DaycareText::OVER){
        console.log(label + ": no egg is waiting.");
        rt.facing = DPAD_NONE;
        return false;
    }
    if (reject){
        //  "Do you want the egg?" NO, "I will keep it, do you want it?" NO.
        //  No A at all while the second question is waited for: A is YES.
        console.log(label + ": an egg is waiting: not wanted (NO, NO).", COLOR_BLUE);
        pbf_wait(rt.context, 300ms);
        pbf_press_button(rt.context, BUTTON_B, 160ms, 840ms);
        rt.context.wait_for_all_requests();
        if (daycare_prompt(rt, 0, 7000ms)){
            pbf_wait(rt.context, 300ms);
            pbf_press_button(rt.context, BUTTON_B, 160ms, 840ms);
            rt.context.wait_for_all_requests();
        }
        daycare_close(rt, label + ": the man's talk (an egg is waiting, not wanted)");
        return true;
    }
    console.log(label + ": an egg is waiting: YES.", COLOR_BLUE);
    pbf_wait(rt.context, 300ms);
    pbf_mash_button(rt.context, BUTTON_A, 1000ms);
    rt.context.wait_for_all_requests();
    daycare_close(rt, label + ": the man's talk (the egg)");
    const int count = story_party_count(rt);
    if (count != rt.dc_party + 1){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_daycare_man(): The party has " + std::to_string(count) + " Pokemon after the egg, not " +
            std::to_string(rt.dc_party + 1) + ": " + label,
            console
        );
    }
    rt.dc_egg = true;
    console.log(label + ": the egg is in the party (slot " + std::to_string(count) + ").", COLOR_BLUE);
    story_note(rt, label + ": the egg", COLOR_GREEN);
    return true;
}

void story_daycare_take(StoryRuntime& rt, const std::string& label){
    ConsoleHandle& console = rt.console;
    if (!rt.dc_given){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_daycare_take(): The parents were not given to the Day Care: " + label,
            console
        );
    }
    //  "Good to see you!", what they have grown, "Take your POKeMON back?"
    //  YES, the list of the two (the first), what it costs, YES; "Take the
    //  other one back too?" YES, what it costs, YES.
    story_talk_start(rt, label + ": the woman of the Day Care");
    int prompts = 0;
    for (int round = 0; round < 6; round++){
        //  The end of the talk is a box that is gone, and stays gone: she
        //  walks away and back before she hands over each one (about five
        //  seconds without a box), so seven are waited for. (A press of A
        //  after the last text would start the talk again, and answer
        //  "Would you like us to raise a POKeMON?" with YES.)
        if (daycare_text(rt, round == 0 ? 8 : 5, 7000ms) != DaycareText::PROMPT){
            break;
        }
        prompts++;
        console.log(label + ": question " + std::to_string(prompts) + ": YES.", COLOR_BLUE);
        pbf_wait(rt.context, 300ms);
        pbf_press_button(rt.context, BUTTON_A, 160ms, 1200ms);
        rt.context.wait_for_all_requests();
        if (prompts == 1){
            //  The list of the two (both have grown): A on the first. Once
            //  it is drawn; one more A that comes too late is the YES of
            //  the next question, which is wanted too.
            pbf_wait(rt.context, 1500ms);
            pbf_press_button(rt.context, BUTTON_A, 160ms, 1200ms);
            rt.context.wait_for_all_requests();
        }
    }
    const std::string last = story_read_dialog(rt);
    daycare_close(rt, label + ": the end of the woman's talk");
    const int count = story_party_count(rt);
    if (count != rt.dc_party + 2){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "story_daycare_take(): The party has " + std::to_string(count) + " Pokemon, not " +
            std::to_string(rt.dc_party + 2) + " (the team and the two parents) after " + std::to_string(prompts) +
            " questions. The last text read: \"" + last + "\". Not enough money (it costs 100 and 100 for each level "
            "they gained, each), or an egg is waiting for the man?: " + label,
            console
        );
    }
    std::string a;
    std::string b;
    story_read_slot(rt, count - 1, &a);
    story_read_slot(rt, count, &b);
    console.log(
        label + ": " + upper_name(a) + " and " + upper_name(b) + " are the party's fifth and sixth Pokemon again.",
        COLOR_BLUE
    );
    if (rt.dc_team_slot > 0){
        //  EEVEE (build 54, experimental): the team's member goes back to the
        //  place it came from, and the one that was there to where it was.
        const int team_slot = rt.dc_team_slot;
        const std::set<std::string>& line = rt.team[(size_t)team_slot - 1].line;
        const int from = line.count(a) != 0 ? count - 1 : line.count(b) != 0 ? count : 0;
        if (from == 0){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "story_daycare_take(): Neither " + upper_name(a) + " nor " + upper_name(b) + " is the team's member "
                "that went to the Day Care (party slot " + std::to_string(team_slot) + "): " + label,
                console
            );
        }
        console.log(
            label + ": " + upper_name(from == count - 1 ? a : b) + " (party slot " + std::to_string(from) +
            ") goes back to party slot " + std::to_string(team_slot) + " (experimental).",
            COLOR_BLUE
        );
        pbf_wait(rt.context, 800ms);
        rt.context.wait_for_all_requests();
        story_swap_party(rt, team_slot, from);
        story_read_slot(rt, count - 1, &a);
        story_read_slot(rt, count, &b);
        rt.dc_team_slot = 0;
    }
    rt.dc_parents[0] = a;
    rt.dc_parents[1] = b;
    rt.dc_given = false;
}



//  ---- The Celadon Game Corner (build 56, experimental) -----------------------
//
//  PORYGON is a prize for 9999 COINS. The packs of COINS are bought at the clerk
//  (the same keys for a pack, or for the clerk's "you can't afford that"), the
//  slot machine is played with the bet of 3 (R), the reels are stopped with A
//  (the machine's bias decides what comes up, whatever the timing is: a Python
//  port of slot_machine.c pays 7.6 COINS for 3 on average), and the prizes'
//  clerk is asked for the PORYGON after each series of games. The program does
//  not read the case: the nickname question (a YES/NO box that comes by itself)
//  says that the COINS were enough.

namespace{

//  One pack at the coins' clerk (across the counter, the player faces it). A
//  until the box is up ("Welcome to ROCKET GAME CORNER!" and, after a page
//  break, "Do you need some game COINS? Would you like to buy some?"), A for
//  the page, the list (50 COINS, 500 COINS, NO THANKS; the cursor on the
//  first) comes up when the page is printed, DOWN for the 500, A, A for the text
//  that says "Here are your COINS!" (or why not).
//  The number in the coins window of the clerk (PokemonFRLG_AutoStoryCoins.h):
//  read until two readings in a row are the same. -1: not read (the window
//  is not up, or what is in its place is no number).
int coins_read_window(StoryRuntime& rt, const std::string& what){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    int last = -1;
    for (int attempt = 0; attempt < 6; attempt++){
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
        ImageViewRGB32 piece = extract_box_reference(
            game,
            ImageFloatBox(
                AutoStoryCoins::CROP_X / 240.0, AutoStoryCoins::CROP_Y / 160.0,
                AutoStoryCoins::CROP_W / 240.0, AutoStoryCoins::CROP_H / 160.0
            )
        );
        const int width = (int)piece.width();
        const int height = (int)piece.height();
        std::vector<uint8_t> lum;
        lum.reserve((size_t)width * (size_t)height);
        for (int y = 0; y < height; y++){
            for (int x = 0; x < width; x++){
                const uint32_t pixel = piece.pixel((size_t)x, (size_t)y);
                const uint32_t r = (pixel >> 16) & 0xff;
                const uint32_t g = (pixel >> 8) & 0xff;
                const uint32_t b = pixel & 0xff;
                lum.push_back((uint8_t)((r + g + b) / 3));
            }
        }
        const AutoStoryCoins::Reading reading = AutoStoryCoins::read_digits(lum, width, height);
        if (reading.ok){
            console.log(
                what + ": the coins window says " + std::to_string(reading.value) + " (cost " +
                std::to_string(reading.cost) + ", margin " + std::to_string(reading.margin) + ", place " +
                std::to_string(reading.dx) + "," + std::to_string(reading.dy) + ")."
            );
            if (reading.value == last){
                return reading.value;
            }
            last = reading.value;
        }else{
            console.log(what + ": the coins window is not read: " + reading.why + ".", COLOR_ORANGE);
            last = -1;
        }
        pbf_wait(context, 400ms);
    }
    return -1;
}

//  What a reading after a purchase says about the reader: the same as
//  before and "size" more is right, anything else (not a refusal) is wrong.
void coins_check(StoryRuntime& rt, int before, int after, int size, const std::string& what){
    ConsoleHandle& console = rt.console;
    if (after < 0){
        rt.coins_seen = -1;
        return;
    }
    if (before >= 0){
        if (after - before == size){
            rt.coins_reader_sure = true;
        }else if (after == before){
            console.log(what + ": no COINS more (the clerk refused: the case is too full, or the money is short).", COLOR_ORANGE);
        }else{
            rt.coins_reader_bad = true;
            console.log(
                what + ": the window went from " + std::to_string(before) + " to " + std::to_string(after) +
                " with a pack of " + std::to_string(size) + ": the reading is not trusted from here.",
                COLOR_RED
            );
        }
    }
    rt.coins_seen = after;
}

//  Returns the reading of the window after the purchase (-1 when not read).
int coins_pack(StoryRuntime& rt, bool big, const std::string& what){
    ProControllerContext& context = rt.context;
    story_talk_start(rt, what);
    pbf_wait(context, 2500ms);
    press(context, BUTTON_A, 3000ms);
    if (big){
        dpad(context, DPAD_DOWN, 600ms);
    }
    //  The pack: the script adds the COINS, updates the window and says "Here are
    //  your COINS!" (or why not): the window shows what the case has now.
    press(context, BUTTON_A, 2500ms);
    const int after = coins_read_window(rt, what);
    press(context, BUTTON_A, 1500ms);
    context.wait_for_all_requests();
    return after;
}

void coins_buy(StoryRuntime& rt, int big, int small, const std::string& label){
    ConsoleHandle& console = rt.console;
    console.log(
        label + ": " + std::to_string(big) + " packs of 500 COINS and " + std::to_string(small) +
        " of 50 at the clerk (experimental): " + std::to_string(big * 500 + small * 50) + " COINS for " +
        std::to_string(big * 10000 + small * 1000) + " (a pack that the money or the case does not allow is "
        "refused with the same keys)."
    );
    for (int i = 0; i < big; i++){
        const std::string what = label + " (500 COINS, " + std::to_string(i + 1) + " of " + std::to_string(big) + ")";
        const int before = rt.coins_seen;
        coins_check(rt, before, coins_pack(rt, true, what), 500, what);
    }
    for (int i = 0; i < small; i++){
        const std::string what = label + " (50 COINS, " + std::to_string(i + 1) + " of " + std::to_string(small) + ")";
        const int before = rt.coins_seen;
        coins_check(rt, before, coins_pack(rt, false, what), 50, what);
    }
    story_back_to_overworld(rt, label);
    if (rt.coins_seen >= 0){
        console.log(
            label + ": the case holds " + std::to_string(rt.coins_seen) + " COINS (by the window" +
            std::string(rt.coins_reader_bad ? ", not trusted" : rt.coins_reader_sure ? ", confirmed" : "") + ").",
            COLOR_BLUE
        );
    }
}

//  Facing the coins' clerk: how many COINS the case holds, and from that how
//  many games of the slot machine with the bet of one COIN the next visit to it
//  plays (StoryRuntime::coins_games), to leave 49 modulo 50. The clerk is asked
//  as for a pack and says no (the window is up while the list is). Without a
//  reading, or with one that was found wrong, no games: the packs and the
//  slot machine's games of the route before this one do the rest.
void coins_look(StoryRuntime& rt, const std::string& label){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    rt.coins_games = 0;
    story_talk_start(rt, label);
    pbf_wait(context, 2500ms);
    press(context, BUTTON_A, 3000ms);
    const int seen = coins_read_window(rt, label);
    //  NO THANKS: B ("Please come play sometime").
    press(context, BUTTON_B, 2000ms);
    press(context, BUTTON_A, 1500ms);
    context.wait_for_all_requests();
    story_back_to_overworld(rt, label);
    if (seen >= 0 && rt.coins_seen == seen){
        //  The same number twice (after a pack and now): the reader is right.
        rt.coins_reader_sure = true;
    }
    rt.coins_seen = seen;
    if (seen < 0 || rt.coins_reader_bad){
        console.log(label + ": the COINS are not known: no games to bring them to 49 modulo 50.", COLOR_ORANGE);
        return;
    }
    rt.coins_games = AutoStoryCoins::games_to_residue(seen, 12);
    console.log(
        label + ": " + std::to_string(seen) + " COINS: " + std::to_string(rt.coins_games) +
        (seen % 50 == 49 ? " games more (49 modulo 50: the packs now end at 9999)." : " games of one COIN to play."),
        COLOR_BLUE
    );
}

//  Whether the player is out of the slot machine (build 61): the START menu comes
//  up by START in the overworld, and in the machine START does nothing that
//  matters. in_overworld() is not used here: it mashes B, and B in the machine
//  asks "Quit playing?" (the second B is the answer NO).
bool coins_machine_left(StoryRuntime& rt){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    StartMenuWatcher start_menu(COLOR_RED);
    context.wait_for_all_requests();
    const int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& ctx){
            for (int i = 0; i < 2; i++){
                pbf_press_button(ctx, BUTTON_PLUS, 200ms, 1500ms);
            }
        },
        { start_menu }
    );
    if (ret != 0){
        return false;
    }
    pbf_press_button(context, BUTTON_B, 160ms, 700ms);
    context.wait_for_all_requests();
    return true;
}

//  Up to "games" games of a slot machine, then out of it. "Do you want to
//  play the slot machine?" YES; the machine's picture comes up after a little
//  face over the player's head and a fade. A game: R (the bet: three COINS,
//  the reels start by themselves), A for each of the three reels, START now
//  and then (when the fanfare of a win is over, START adds the whole payout at
//  once; before that it does nothing). A key that comes too early or too late
//  does nothing harmful: R is only taken when a game can start, A only stops a
//  reel or starts one. Out: B (the question to quit, in the betting phase
//  only), A (YES: the cursor is on it).
//  "bet" 1 (build 57): DOWN puts one COIN on the middle line (the machine goes
//  back to the betting phase for more), A starts the reels; each game costs one
//  COIN and a win pays some. No games (nothing to play): the question is
//  answered NO.
//  Build 61 (the first run in a machine, 24 November: "Not back in the
//  overworld" twice): (1) the machine's own YES/NO box (slot_machine.c: tiles
//  19 to 24 of 30, 4 to the left of the overworld's) is not the box that
//  SelectionDialogWatcher looks for, so the question to quit was never seen,
//  and every B on it is the answer NO: the loop of B and START went round and
//  round. The way out is blind now: B, A (YES), a look for the START menu,
//  and again when it does not come. (2) A press only counts when the machine
//  is waiting for it (a reel being stopped, the little animation after the
//  start), so one A at fixed times lost some: A now every half second for the
//  whole game; with no bet or no reel waiting an A does nothing.
void coins_play(StoryRuntime& rt, int games, const std::string& label, int bet = 3){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    if (games <= 0){
        console.log(label + ": no games to play: NO to the machine.", COLOR_BLUE);
        story_talk_answer(rt, false, label);
        story_back_to_overworld(rt, label);
        return;
    }
    console.log(
        label + ": up to " + std::to_string(games) + " games of the slot machine with the bet of " +
        std::to_string(bet) + " (experimental)."
    );
    story_talk_answer(rt, true, label);
    pbf_wait(context, 6000ms);
    for (int game = 0; game < games; game++){
        if (bet == 1){
            dpad(context, DPAD_DOWN, 600ms);
        }else{
            press(context, BUTTON_R, 900ms);
        }
        for (int i = 0; i < 12; i++){
            press(context, BUTTON_A, 500ms);
        }
        for (int i = 0; i < 6; i++){
            press(context, BUTTON_PLUS, 450ms);
        }
        if ((game + 1) % 10 == 0){
            console.log(label + ": " + std::to_string(game + 1) + " games played.");
        }
    }
    //  Out of the machine: B asks "Quit playing?" in the betting phase only
    //  (the cursor is on YES), A answers. After the answer the machine fades
    //  out. Not out yet: the same again (a B on the question is NO: the next
    //  B asks again).
    bool out = false;
    for (int attempt = 0; attempt < 6 && !out; attempt++){
        pbf_press_button(context, BUTTON_PLUS, 160ms, 450ms);
        pbf_press_button(context, BUTTON_B, 160ms, 1200ms);
        pbf_press_button(context, BUTTON_A, 160ms, 3500ms);
        context.wait_for_all_requests();
        out = coins_machine_left(rt);
        if (!out){
            console.log(label + ": still in the machine after try " + std::to_string(attempt + 1) + " of B and A.", COLOR_ORANGE);
            //  (Build 62) What the screen shows: the reels, the payout, the
            //  question to quit, or the Game Corner.
            story_save_picture(rt, label + "-still-in-the-machine-try-" + std::to_string(attempt + 1));
        }
    }
    context.wait_for_all_requests();
    story_back_to_overworld(rt, label);
}

//  The prizes' clerk of the POKeMON (across the counter): the text, A, the list
//  (ABRA, CLEFAIRY, DRATINI, SCYTHER, PORYGON, NO THANKS: the cursor on the
//  first), DOWN four times, A, "So, you want the PORYGON?" YES. With 9999 COINS:
//  the fanfare, "obtained the PORYGON!" and the nickname question, a YES/NO box
//  that comes by itself (NO; the party is full: it goes to the PC). Without:
//  "Sorry, you'll need more COINS than that." and no box.
void coins_prize(StoryRuntime& rt, const std::string& species, const std::string& label){
    ConsoleHandle& console = rt.console;
    ProControllerContext& context = rt.context;
    console.log(label + ": the prizes' clerk is asked for " + species + " (9999 COINS; experimental).");
    story_talk_start(rt, label);
    pbf_wait(context, 2000ms);
    press(context, BUTTON_A, 3000ms);
    for (int i = 0; i < 4; i++){
        dpad(context, DPAD_DOWN, 450ms);
    }
    bool sure = false;
    {
        SelectionDialogWatcher question(COLOR_RED);
        context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            console, context,
            [](ProControllerContext& ctx){
                pbf_press_button(ctx, BUTTON_A, 160ms, 9000ms);
            },
            { question }
        );
        sure = ret == 0;
    }
    if (!sure){
        console.log(label + ": no \"So, you want the prize?\" question: the list was not as expected.", COLOR_ORANGE);
        pbf_mash_button(context, BUTTON_B, 3000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, label);
        return;
    }
    pbf_wait(context, 300ms);
    press(context, BUTTON_A, 1500ms);   //  YES
    bool given = false;
    {
        SelectionDialogWatcher nickname(COLOR_RED);
        context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            console, context,
            [](ProControllerContext& ctx){
                pbf_wait(ctx, 14000ms);
            },
            { nickname }
        );
        given = ret == 0;
    }
    if (!given){
        console.log(label + ": no nickname question: \"you'll need more COINS than that\". The case is not full yet.", COLOR_BLUE);
        pbf_mash_button(context, BUTTON_B, 3000ms);
        context.wait_for_all_requests();
        story_back_to_overworld(rt, label);
        return;
    }
    //  NO to the nickname; the text that says where it went, and the clerk's
    //  end of the talk.
    pbf_wait(context, 300ms);
    press(context, BUTTON_B, 1500ms);
    context.wait_for_all_requests();
    story_cutscene(rt, 0ms, 30000ms, false, label);
    story_back_to_overworld(rt, label);
    rt.caught.insert(species);
    if (rt.collection != nullptr){
        rt.collection->add_caught(species, false, false);
    }
    rt.dex_new_since_load.insert(species);
    console.log(
        "Got for the POKeDEX: " + species + " (the prize of the Game Corner for 9999 COINS)." +
        (rt.collection != nullptr
            ? " The POKeDEX table: " + std::to_string(rt.collection->caught(species)) + " of " +
                std::to_string(std::max(0, rt.collection->needed(species))) + "."
            : std::string()),
        COLOR_BLUE
    );
    story_note(rt, "Got " + species, COLOR_GREEN);
}

}


void story_coins(StoryRuntime& rt, const StoryStep& step, const std::string& label){
    switch (step.p0){
    case 0:
        coins_buy(rt, std::max(0, (int)step.p1), std::max(0, (int)step.p2), label);
        break;
    case 1:
        coins_play(rt, std::max(0, (int)step.p1), label);
        break;
    case 2:
        coins_prize(rt, step.text == nullptr ? std::string() : std::string(step.text), label);
        break;
    case 3:
        coins_look(rt, label);
        break;
    case 4:
        //  The games of one COIN the last look asked for.
        coins_play(rt, rt.coins_games, label, 1);
        rt.coins_games = 0;
        break;
    default:
        rt.console.log(label + ": a step of the Game Corner that this program does not know (" +
            std::to_string(step.p0) + ").", COLOR_RED);
        break;
    }
}


}
}
}

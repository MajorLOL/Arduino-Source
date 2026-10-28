/*  Pokemon FRLG Autostory Battles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
#include "CommonTools/OCR/OCR_StringMatchResult.h"
#include "CommonTools/Images/ImageFilter.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleSelectionArrowDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_OcrPreprocessing.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_WildEncounterReader.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_BattleMenuNavigation.h"
#include "PokemonFRLG_AutoStoryBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;
using AutoStoryDex::MoveClass;
using AutoStoryDex::MoveInfo;
using AutoStoryDex::SpeciesInfo;
using AutoStoryDex::Type;


namespace{


//  Where the move names are in the FIGHT menu (game screen coordinates).
//  The names start right of the selection arrows
//  (BattleSelectionArrowDetector::box_for_option(BattleMoveOption)).
const ImageFloatBox MOVE_NAME_BOXES[4] = {
    {0.066, 0.770, 0.255, 0.070},
    {0.366, 0.770, 0.255, 0.070},
    {0.066, 0.870, 0.255, 0.070},
    {0.366, 0.870, 0.255, 0.070},
};

//  The first entry of the battle menu: "FIGHT", or "BALL" in the Safari Zone.
const ImageFloatBox MENU_FIRST_OPTION_BOX(0.560, 0.770, 0.190, 0.070);

//  The text of the battle dialog box (white text on teal).
const ImageFloatBox BATTLE_TEXT_BOX(0.045, 0.770, 0.890, 0.170);


ImageViewRGB32 game_box(const ImageViewRGB32& frame, const ImageFloatBox& box){
    ImageViewRGB32 game_screen = extract_box_reference(frame, GameSettings::instance().GAME_BOX);
    return extract_box_reference(game_screen, box);
}


//  Dictionary of all move names (English). Tokens are "m<index>".
const OCR::SmallDictionaryMatcher& move_matcher(){
    static const OCR::SmallDictionaryMatcher matcher = [](){
        JsonObject english;
        const std::vector<MoveInfo>& moves = AutoStoryDex::moves();
        for (size_t i = 0; i < moves.size(); i++){
            JsonArray names;
            names.push_back(JsonValue(std::string(moves[i].name)));
            english["m" + std::to_string(i)] = JsonValue(std::move(names));
        }
        JsonObject root;
        root["eng"] = JsonValue(std::move(english));
        return OCR::SmallDictionaryMatcher(root);
    }();
    return matcher;
}

int token_to_move(const std::string& token){
    if (token.size() < 2 || token[0] != 'm'){
        return -1;
    }
    int index = std::atoi(token.c_str() + 1);
    if (index < 0 || (size_t)index >= AutoStoryDex::moves().size()){
        return -1;
    }
    return index;
}

int read_move_name(Logger& logger, const ImageViewRGB32& image, const std::vector<OCR::TextColorRange>& filters){
    ImageRGB32 ready = preprocess_for_ocr(image);
    OCR::StringMatchResult result = move_matcher().match_substring_from_image_multifiltered(
        &logger, Language::English, ready, filters, -1.0, 0.5, 0.01, 0.50
    );
    if (result.results.empty()){
        return -1;
    }
    return token_to_move(result.results.begin()->second.token);
}

//  Read the four move names of the FIGHT menu.
void read_fight_menu(ConsoleHandle& console, StoryBattleMemory& memory){
    VideoSnapshot screen = console.video().snapshot();
    std::string log = "Moves:";
    for (int i = 0; i < 4; i++){
        ImageViewRGB32 box = game_box(screen, MOVE_NAME_BOXES[i]);
        int move = read_move_name(console.logger(), box, DARK_TEXT_FILTERS());
        memory.moves[i] = move;
        log += " [";
        log += move < 0 ? std::string("-") : std::string(AutoStoryDex::moves()[(size_t)move].name);
        log += "]";
    }
    console.log(log);
}

const SpeciesInfo* find_species(const std::string& slug){
    for (const SpeciesInfo& info : AutoStoryDex::species()){
        if (slug == info.slug){
            return &info;
        }
    }
    return nullptr;
}

const std::set<std::string>& all_species_slugs(){
    static const std::set<std::string> slugs = [](){
        std::set<std::string> ret;
        for (const SpeciesInfo& info : AutoStoryDex::species()){
            ret.insert(info.slug);
        }
        return ret;
    }();
    return slugs;
}

//  Read the opponent's species from its HP box (top left).
const SpeciesInfo* read_opponent(ConsoleHandle& console, std::string& slug){
    VideoSnapshot screen = console.video().snapshot();
    WildEncounterReader reader(COLOR_RED);
    PokemonFRLG_WildEncounter encounter = reader.read_encounter(
        console.logger(), Language::English, screen, all_species_slugs()
    );
    slug = encounter.name;
    const SpeciesInfo* info = find_species(slug);
    if (info != nullptr){
        console.log(std::string("Opponent: ") + info->name);
    }else{
        console.log("Opponent: not read.", COLOR_ORANGE);
    }
    return info;
}

std::string read_text(ConsoleHandle& console, const ImageFloatBox& box, bool bright){
    VideoSnapshot screen = console.video().snapshot();
    ImageViewRGB32 image = game_box(screen, box);
    //  The OCR wants dark text on a light background: the text pixels
    //  become black, everything else white.
    ImageRGB32 bw = bright
        ? to_blackwhite_rgb32_range(image, true, combine_rgb(180, 180, 180), combine_rgb(255, 255, 255))
        : to_blackwhite_rgb32_range(image, true, combine_rgb(0, 0, 0), combine_rgb(128, 128, 128));
    std::string text = OCR::ocr_read(Language::English, bw, OCR::PageSegMode::SINGLE_BLOCK);
    std::string lower;
    for (char c : text){
        lower += c == '\n' ? ' ' : (char)std::tolower((unsigned char)c);
    }
    return lower;
}

bool menu_is_safari(ConsoleHandle& console){
    std::string text = read_text(console, MENU_FIRST_OPTION_BOX, false);
    return text.find("ball") != std::string::npos;
}


double effectiveness(Type move_type, const SpeciesInfo* opponent){
    if (opponent == nullptr){
        return 1.0;
    }
    double m = AutoStoryDex::type_multiplier_x10(move_type, opponent->type1) / 10.0;
    if (opponent->type2 != opponent->type1){
        m *= AutoStoryDex::type_multiplier_x10(move_type, opponent->type2) / 10.0;
    }
    return m;
}

double base_damage(const MoveInfo& move){
    switch (move.move_class){
    case MoveClass::DAMAGE:         return move.power;
    case MoveClass::STATUS:         return 0;
    case MoveClass::LEVEL_DAMAGE:   return 70;
    case MoveClass::FIXED_40:       return 45;
    case MoveClass::FIXED_20:       return 25;
    case MoveClass::AVOID:          return 0;
    case MoveClass::WEAK:           return move.power * 0.5;
    case MoveClass::HALF:           return move.power * 0.5;
    case MoveClass::TIMES_2:        return move.power * 2.0;
    case MoveClass::TIMES_3:        return move.power * 3.0;
    case MoveClass::RECOIL:         return move.power * 0.85;
    case MoveClass::VARIABLE:       return 50;
    }
    return 0;
}

bool type_ignores_matchups(MoveClass c){
    return c == MoveClass::LEVEL_DAMAGE || c == MoveClass::FIXED_40 || c == MoveClass::FIXED_20;
}

double move_score(int move, const StoryBattleSettings& settings, const SpeciesInfo* opponent){
    if (move < 0){
        return -1;
    }
    const MoveInfo& info = AutoStoryDex::moves()[(size_t)move];
    double score = base_damage(info);
    if (score <= 0){
        return 0;
    }
    if (!type_ignores_matchups(info.move_class)){
        if (info.type == settings.lead_type1 || info.type == settings.lead_type2){
            score *= 1.5;
        }
        score *= effectiveness(info.type, opponent);
    }else if (effectiveness(info.type, opponent) == 0){
        score = 0;  //  e.g. Seismic Toss against a ghost
    }
    if (info.accuracy > 0){
        score *= info.accuracy / 100.0;
    }
    return score;
}

//  Best slot in the FIGHT menu, skipping "excluded" (bit mask).
int best_move_slot(const StoryBattleMemory& memory, const StoryBattleSettings& settings,
                   const SpeciesInfo* opponent, uint8_t excluded){
    int best = -1;
    double best_score = -1;
    for (int i = 0; i < 4; i++){
        if (excluded & (1 << i)){
            continue;
        }
        double score = move_score(memory.moves[i], settings, opponent);
        if (memory.moves[i] < 0 && i == 0){
            score = 0.1;    //  Not read: the first move is better than nothing.
        }
        if (score > best_score){
            best_score = score;
            best = i;
        }
    }
    return best < 0 ? 0 : best;
}

BattleMoveOption slot_option(int slot){
    switch (slot){
    case 0: return BattleMoveOption::MOVE_1;
    case 1: return BattleMoveOption::MOVE_2;
    case 2: return BattleMoveOption::MOVE_3;
    default: return BattleMoveOption::MOVE_4;
    }
}


//  The move list is open: pick the best move and use it.
void use_best_move(
    ConsoleHandle& console, ProControllerContext& context,
    const StoryBattleSettings& settings, StoryBattleMemory& memory,
    const SpeciesInfo* opponent
){
    int before[4] = {memory.moves[0], memory.moves[1], memory.moves[2], memory.moves[3]};
    read_fight_menu(console, memory);
    for (int i = 0; i < 4; i++){
        if (memory.moves[i] != before[i]){
            //  A new move in this slot: its PP is full.
            memory.no_pp &= (uint8_t)~(1 << i);
        }
    }
    if ((memory.no_pp & 0x0f) == 0x0f){
        memory.no_pp = 0;   //  Then something was wrong. Try them all again.
    }

    uint8_t excluded = memory.no_pp;
    int cursor_slot = 0;
    for (int attempt = 0; attempt < 4; attempt++){
        int slot = best_move_slot(memory, settings, opponent, excluded);
        cursor_slot = slot;
        if (memory.moves[slot] >= 0){
            console.log(std::string("Using ") + AutoStoryDex::moves()[(size_t)memory.moves[slot]].name);
        }else{
            console.log("Using move " + std::to_string(slot + 1));
        }
        move_cursor_to_option(console, context, slot_option(slot));
        context.wait_for_all_requests();

        //  A move without PP shows its PP in red.
        BattleOutOfPpWatcher no_pp(COLOR_RED);
        if (wait_until(console, context, 400ms, { no_pp }) == 0){
            console.log("That move is out of PP.", COLOR_ORANGE);
            excluded |= (uint8_t)(1 << slot);
            if (excluded == 0x0f){
                break;      //  Struggle: any move.
            }
            continue;
        }
        break;
    }
    memory.last_slot = cursor_slot;
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    context.wait_for_all_requests();
}

//  At the battle menu: FIGHT, pick the best move.
void fight_one_turn(
    ConsoleHandle& console, ProControllerContext& context,
    const StoryBattleSettings& settings, StoryBattleMemory& memory
){
    std::string slug;
    const SpeciesInfo* opponent = read_opponent(console, slug);

    if (!move_cursor_to_option(console, context, BattleMenuOption::FIGHT)){
        console.log("FIGHT not selected. Pressing A anyway.", COLOR_ORANGE);
    }
    pbf_press_button(context, BUTTON_A, 160ms, 640ms);
    context.wait_for_all_requests();

    BattleSelectionArrowWatcher move_list(COLOR_RED, &console.overlay(), BATTLE_MOVE_ARROW_BOX);
    if (wait_until(console, context, 3000ms, { move_list }) < 0){
        //  (The old man's catching demo: the player has no say.)
        console.log("The move list did not open.", COLOR_ORANGE);
        return;
    }
    use_best_move(console, context, settings, memory, opponent);
}

void run_from_battle(ConsoleHandle& console, ProControllerContext& context, bool safari){
    if (safari){
        move_cursor_to_option(console, context, SafariBattleMenuOption::RUN);
    }else{
        move_cursor_to_option(console, context, BattleMenuOption::RUN);
    }
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    context.wait_for_all_requests();
}


//  Party menu during a battle (the lead fainted): send in the next one.
void send_next_pokemon(ConsoleHandle& console, ProControllerContext& context, int& next_slot){
    console.log("Sending in party slot " + std::to_string(next_slot + 1) + ".");
    //  The cursor is on the fainted lead (slot 1). Slots 2-6 are below it.
    for (int i = 0; i < next_slot; i++){
        pbf_press_dpad(context, DPAD_DOWN, 160ms, 340ms);
    }
    //  A opens the small menu, A on SHIFT sends it in.
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1840ms);
    context.wait_for_all_requests();
    next_slot++;
    if (next_slot > 5){
        next_slot = 1;
    }
}


//  A YES/NO box in battle. Read the question to know what it is.
enum class Question{
    DELETE_MOVE,
    STOP_LEARNING,
    USE_NEXT,
    SWITCH,
    NICKNAME,
    UNKNOWN,
};

Question read_question(ConsoleHandle& console, std::string& text){
    text = read_text(console, BATTLE_TEXT_BOX, true);
    console.log("Question: " + text);
    if (text.find("stop") != std::string::npos){
        return Question::STOP_LEARNING;
    }
    if (text.find("delete") != std::string::npos || text.find("room") != std::string::npos ||
        text.find("forget") != std::string::npos){
        return Question::DELETE_MOVE;
    }
    if (text.find("next") != std::string::npos){
        return Question::USE_NEXT;
    }
    if (text.find("change") != std::string::npos || text.find("switch") != std::string::npos){
        return Question::SWITCH;
    }
    if (text.find("nickname") != std::string::npos){
        return Question::NICKNAME;
    }
    return Question::UNKNOWN;
}


void answer(ProControllerContext& context, bool yes){
    pbf_wait(context, 300ms);
    if (yes){
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    }else{
        pbf_press_button(context, BUTTON_B, 160ms, 840ms);
    }
    context.wait_for_all_requests();
}


//  "Delete a move to make room for X?" The new move is in the question.
void handle_move_learning(
    ConsoleHandle& console, ProControllerContext& context,
    const StoryBattleSettings& settings, StoryBattleMemory& memory,
    const std::string& question
){
    //  Find the longest move name contained in the question.
    int new_move = -1;
    size_t best_len = 0;
    const std::vector<MoveInfo>& moves = AutoStoryDex::moves();
    for (size_t i = 0; i < moves.size(); i++){
        std::string name;
        for (const char* p = moves[i].name; *p; p++){
            name += (char)std::tolower((unsigned char)*p);
        }
        if (name.size() > best_len && question.find(name) != std::string::npos){
            best_len = name.size();
            new_move = (int)i;
        }
    }
    int forget = new_move < 0 ? -1 : choose_move_to_forget(memory.moves, new_move, settings);
    if (forget < 0){
        console.log("Not learning the new move.");
        answer(context, false);     //  NO -> "Stop learning X?"
        return;
    }
    console.log(
        std::string("Learning ") + AutoStoryDex::moves()[(size_t)new_move].name +
        " instead of " + AutoStoryDex::moves()[(size_t)memory.moves[forget]].name + "."
    );
    answer(context, true);
    //  YES fades to the summary screen with the move list (cursor on move 1).
    pbf_wait(context, 3000ms);
    for (int i = 0; i < forget; i++){
        pbf_press_dpad(context, DPAD_DOWN, 160ms, 440ms);
    }
    pbf_press_button(context, BUTTON_A, 160ms, 1840ms);
    context.wait_for_all_requests();
    memory.moves[forget] = new_move;
}


}   //  namespace



std::string read_story_text(
    ConsoleHandle& console, const ImageFloatBox& box, bool bright
){
    return read_text(console, box, bright);
}

double move_value(int move, const StoryBattleSettings& settings){
    if (move < 0){
        return 0;
    }
    const MoveInfo& info = AutoStoryDex::moves()[(size_t)move];
    double score = base_damage(info);
    if (!type_ignores_matchups(info.move_class) &&
        (info.type == settings.lead_type1 || info.type == settings.lead_type2)
    ){
        score *= 1.5;
    }
    if (info.accuracy > 0){
        score *= info.accuracy / 100.0;
    }
    return score;
}

int choose_move_to_forget(const int moves[4], int new_move, const StoryBattleSettings& settings){
    if (new_move < 0){
        return -1;
    }
    for (int i = 0; i < 4; i++){
        if (moves[i] < 0){
            return -1;      //  Moves not known: keep what we have.
        }
    }
    //  Keep one move of each of the lead's own types if possible: only
    //  compare values. The weakest move that is not an HM goes.
    int worst = -1;
    double worst_value = 1e9;
    for (int i = 0; i < 4; i++){
        const MoveInfo& info = AutoStoryDex::moves()[(size_t)moves[i]];
        if (info.hm){
            continue;
        }
        double value = move_value(moves[i], settings);
        if (value < worst_value){
            worst_value = value;
            worst = i;
        }
    }
    if (worst < 0){
        return -1;
    }
    double new_value = move_value(new_move, settings);
    //  Learn when clearly better (10%), so equal moves are not swapped back and forth.
    if (new_value > worst_value * 1.1 + 1){
        return worst;
    }
    return -1;
}



StoryBattleResult run_story_battle(
    ConsoleHandle& console, ProControllerContext& context,
    StoryBattleKind kind,
    const StoryBattleSettings& settings,
    StoryBattleMemory& memory
){
    memory.battles++;
    memory.last_slot = -1;
    memory.list_back = 0;
    StoryBattleResult result = StoryBattleResult::WON;
    bool seen_battle = false;
    bool decided = false;       //  wild: fight/run/catch decided
    bool run = false;
    bool safari = false;
    int run_attempts = 0;
    bool lost = false;
    int next_slot = 1;
    int unknown_in_row = 0;     //  turns of the loop with nothing recognized
    //  A battle for sure: its menu, a move list, a level-up window, a
    //  question or the party menu was seen. (The battle text detectors
    //  also take a plain white area at the bottom of the overworld, like
    //  on the S.S. Anne, for a text box.)
    bool battle_confirmed = false;
    int text_only_in_row = 0;   //  turns with only a text box (no arrow) seen
    WallClock deadline = current_time() + 20min;
    WallClock last_activity = current_time();

    while (current_time() < deadline){
        BattleMenuWatcher battle_menu(COLOR_RED);
        BattleLearnDialogWatcher question(COLOR_RED);
        PartyMenuWatcher party_menu(COLOR_RED);
        AdvanceBattleDialogWatcher advance(COLOR_RED);
        context.wait_for_all_requests();
        int ret = wait_until(
            console, context, 6000ms,
            { battle_menu, question, party_menu, advance }
        );
        if (ret >= 0){
            unknown_in_row = 0;
            text_only_in_row = 0;
        }
        if (ret == 0 || ret == 1 || ret == 2){
            battle_confirmed = true;
        }
        switch (ret){
        case 0:{
            seen_battle = true;
            last_activity = current_time();
            memory.list_back = 0;   //  The last pick went through.
            if (!decided){
                decided = true;
                safari = menu_is_safari(console);
                if (safari){
                    console.log("Safari Zone battle: running.");
                    run = true;
                }else if (kind == StoryBattleKind::WILD_RUN || (kind == StoryBattleKind::WILD && settings.wild_run)){
                    run = true;
                }else if (kind == StoryBattleKind::CATCH){
                    std::string slug;
                    read_opponent(console, slug);
                    if (settings.catch_species.count(slug) != 0){
                        console.log("Catching " + slug + ".");
                        int thrown = auto_catch(console, context, settings.catch_balls);
                        if (thrown > 0){
                            console.log("Caught " + slug + ".", COLOR_BLUE);
                            return StoryBattleResult::CAUGHT;
                        }
                        //  Not caught (out of balls?) or the battle went on.
                        if (in_overworld(console, context)){
                            return StoryBattleResult::RAN;
                        }
                    }
                    run = true;
                }
            }
            if (run && run_attempts < 6){
                run_attempts++;
                result = StoryBattleResult::RAN;
                run_from_battle(console, context, safari);
            }else{
                if (run){
                    console.log("Could not run away. Fighting.", COLOR_ORANGE);
                    result = StoryBattleResult::WON;
                }
                fight_one_turn(console, context, settings, memory);
            }
            break;
        }
        case 1:{
            last_activity = current_time();
            std::string text;
            Question q = read_question(console, text);
            switch (q){
            case Question::DELETE_MOVE:
                handle_move_learning(console, context, settings, memory, text);
                break;
            case Question::STOP_LEARNING:
                answer(context, true);
                break;
            case Question::USE_NEXT:
                memory.lead_faints++;
                result = StoryBattleResult::LEAD_FAINTED;
                answer(context, true);
                break;
            case Question::SWITCH:
            case Question::NICKNAME:
            case Question::UNKNOWN:
                answer(context, false);
                break;
            }
            break;
        }
        case 2:
            last_activity = current_time();
            if (seen_battle){
                memory.lead_faints++;
                result = StoryBattleResult::LEAD_FAINTED;
            }
            send_next_pokemon(console, context, next_slot);
            break;
        case 3:
            last_activity = current_time();
            seen_battle = true;
            if (!lost){
                //  "PLAYER is out of usable POKeMON! PLAYER whited out!"
                std::string text = read_text(console, BATTLE_TEXT_BOX, true);
                if (text.find("whited") != std::string::npos ||
                    text.find("blacked") != std::string::npos ||
                    text.find("usable") != std::string::npos
                ){
                    console.log("The party fainted: " + text, COLOR_RED);
                    lost = true;
                }
                if (text.find("grew to") != std::string::npos){
                    memory.level_up_seen = true;
                }
            }
            //  A advances the text. (B would cancel an evolution.)
            pbf_press_button(context, BUTTON_A, 160ms, 340ms);
            break;
        default:{
            if (current_time() - last_activity > 3min){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "run_story_battle(): Nothing recognized for 3 minutes.",
                    console
                );
            }
            //  The battle text box without the advance arrow: FRLG shows no
            //  arrow on some last lines (a trainer's words when defeated), and
            //  an evolution shows the box while it runs. A goes on in both
            //  cases. Never B here: B stops an evolution.
            VideoSnapshot screen = console.video().snapshot();

            //  The level-up window over the right of the screen (the stat
            //  gains, then the new stats). It hides the advance arrow. A
            //  goes on.
            BattleLevelUpDetector level_up(COLOR_RED, BattleLevelUpDialog::either, Language::English);
            if (level_up.detect(screen)){
                seen_battle = true;
                battle_confirmed = true;
                memory.level_up_seen = true;
                last_activity = current_time();
                unknown_in_row = 0;
                console.log("Level-up window.");
                pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                context.wait_for_all_requests();
                break;
            }

            //  The move list is open (a pick that did not go through): pick
            //  a move. Pressing A or B blindly here would go round in
            //  circles with the battle menu.
            {
                BattleSelectionArrowWatcher move_list(COLOR_RED, &console.overlay(), BATTLE_MOVE_ARROW_BOX);
                if (wait_until(console, context, 500ms, { move_list }) == 0){
                    seen_battle = true;
                    battle_confirmed = true;
                    last_activity = current_time();
                    unknown_in_row = 0;
                    console.log("The move list is open. Picking a move.", COLOR_ORANGE);
                    if (memory.last_slot >= 0 && ++memory.list_back >= 2){
                        //  The move picked last did not happen, twice: no PP
                        //  left. (Once is also a press the game missed.)
                        console.log("Move " + std::to_string(memory.last_slot + 1) + " seems to be out of PP.", COLOR_ORANGE);
                        memory.no_pp |= (uint8_t)(1 << memory.last_slot);
                        memory.last_slot = -1;
                        memory.list_back = 0;
                    }
                    std::string slug;
                    const SpeciesInfo* opponent = read_opponent(console, slug);
                    use_best_move(console, context, settings, memory, opponent);
                    break;
                }
            }

            BattleDialogDetector battle_text(COLOR_RED);
            if (battle_text.detect(screen)){
                //  Only a text box, again and again: maybe no battle at all
                //  (a white area at the bottom of the overworld), or the
                //  battle is over. Look.
                if (++text_only_in_row >= 3){
                    text_only_in_row = 0;
                    if (in_overworld(console, context)){
                        if (lost){
                            return StoryBattleResult::LOST;
                        }
                        if (!battle_confirmed){
                            console.log("No battle after all: the screen only looked like battle text.", COLOR_ORANGE);
                            return StoryBattleResult::NO_BATTLE;
                        }
                        console.log("Battle over.");
                        return result;
                    }
                }
                seen_battle = true;
                last_activity = current_time();
                pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                context.wait_for_all_requests();
                break;
            }
            //  Overworld text (the trainer's words before or after the battle).
            //  B advances it, and unlike A it never starts a new conversation
            //  with whoever the player faces once the text is over.
            WhiteDialogDetector white_dialog(COLOR_RED);
            if (white_dialog.detect(screen)){
                pbf_press_button(context, BUTTON_B, 160ms, 840ms);
                context.wait_for_all_requests();
                break;
            }
            if (in_overworld(console, context)){
                if (lost){
                    return StoryBattleResult::LOST;
                }
                if (!seen_battle){
                    return StoryBattleResult::NO_BATTLE;
                }
                console.log("Battle over.");
                return result;
            }
            //  Something else. Mostly A, now and then B (which backs out of
            //  menus and also advances text).
            unknown_in_row++;
            if (unknown_in_row % 3 == 0){
                console.log("Nothing recognized. Pressing B.", COLOR_ORANGE);
                pbf_press_button(context, BUTTON_B, 160ms, 840ms);
            }else{
                console.log("Nothing recognized. Pressing A.", COLOR_ORANGE);
                pbf_press_button(context, BUTTON_A, 160ms, 840ms);
            }
            context.wait_for_all_requests();
        }
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "run_story_battle(): The battle took longer than 20 minutes.",
        console
    );
}



}
}
}

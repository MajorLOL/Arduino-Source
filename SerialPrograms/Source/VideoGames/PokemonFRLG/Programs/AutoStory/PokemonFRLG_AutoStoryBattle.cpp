/*  Pokemon FRLG Autostory Battles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <map>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <string>
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "Common/Cpp/CancellableScope.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/Async/InferenceSession.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
#include "CommonTools/OCR/OCR_StringMatchResult.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_BagDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_StartMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Sounds/PokemonFRLG_ShinySoundDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_PokedexRegisteredDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleSelectionArrowDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_OcrPreprocessing.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_WildEncounterReader.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_BattleMenuNavigation.h"
#include "Pokemon/Inference/Pokemon_NameReader.h"
#include "PokemonFRLG_AutoStoryMemory.h"
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

//  An HP bar in a single battle: 48 GBA pixels from x = "left", rows "top"
//  to "bottom". Each column is filled (green, yellow or red) or empty (dark
//  grey-green), filled ones first. Returns the filled part, or -1 if no
//  such bar shows there (the box sliding in, the level-up window over it,
//  not a battle). The player's Pokemon: its box at the bottom right, from
//  x = 174, rows 91 to 93.
double read_hp_bar(const ImageViewRGB32& frame, double left, double top, double bottom){
    ImageViewRGB32 game = extract_box_reference(frame, GameSettings::instance().GAME_BOX);
    const size_t width = game.width();
    const size_t height = game.height();
    if (width < 240 || height < 160){
        return -1;
    }
    const double sx = (double)width / 240.0;
    const double sy = (double)height / 160.0;
    const size_t y0 = (size_t)(top * sy);
    const size_t y1 = std::max(y0 + 1, (size_t)(bottom * sy));
    int filled = 0;
    int empty = 0;
    int unknown = 0;
    int out_of_order = 0;
    for (int c = 0; c < 48; c++){
        const size_t x0 = (size_t)((left + c + 0.25) * sx);
        const size_t x1 = std::max(x0 + 1, (size_t)((left + c + 0.75) * sx));
        uint64_t sum[3] = {0, 0, 0};
        uint64_t count = 0;
        for (size_t y = y0; y < y1 && y < height; y++){
            for (size_t x = x0; x < x1 && x < width; x++){
                const uint32_t pixel = game.pixel(x, y);
                sum[0] += (pixel >> 16) & 0xff;
                sum[1] += (pixel >> 8) & 0xff;
                sum[2] += pixel & 0xff;
                count++;
            }
        }
        if (count == 0){
            return -1;
        }
        const int r = (int)(sum[0] / count);
        const int g = (int)(sum[1] / count);
        const int b = (int)(sum[2] / count);
        const int high = std::max({r, g, b});
        const int low = std::min({r, g, b});
        if (high - low > 50 && high > 150){
            filled++;
            if (empty > 0){
                out_of_order++;
            }
        }else if (high < 140 && high - low < 50 && g >= r && g + 10 >= b){
            empty++;
        }else{
            unknown++;
        }
    }
    if (unknown > 3 || out_of_order > 1 || filled + empty == 0){
        return -1;
    }
    return (double)filled / (double)(filled + empty);
}
double read_player_hp(const ImageViewRGB32& frame){
    return read_hp_bar(frame, 174, 91.3, 93.7);
}
//  The foe's, in its box at the top left: 48 pixels from x = 52, rows 34
//  to 36.
double read_foe_hp(const ImageViewRGB32& frame){
    return read_hp_bar(frame, 52, 33.8, 36.2);
}

//  The ball mark in the foe's box (wild battles): the game draws it when the
//  species is caught already (TryAddPokeballIconToHealthbox: tile 8 of the
//  HP bar sprite, an 8x8 subsprite 32 pixels left of the bar sprite's
//  center, so x = 20 to 27, rows 30 to 37; the bar is at x = 52 to 99 of
//  the same rows). The box is cream there, the ball has a dark outline and
//  a red top. Returns 1 (the mark), 0 (the box without it), -1 (no box
//  there). Only at the start of a battle: a status (PSN...) clears it.
int read_caught_mark(const ImageViewRGB32& frame){
    ImageViewRGB32 game = extract_box_reference(frame, GameSettings::instance().GAME_BOX);
    const size_t width = game.width();
    const size_t height = game.height();
    if (width < 240 || height < 160){
        return -1;
    }
    const double sx = (double)width / 240.0;
    const double sy = (double)height / 160.0;
    auto cream = [&](int x, int y){
        const size_t px = std::min(width - 1, (size_t)((x + 0.5) * sx));
        const size_t py = std::min(height - 1, (size_t)((y + 0.5) * sy));
        const uint32_t p = game.pixel(px, py);
        return ((p >> 16) & 0xff) > 200 && ((p >> 8) & 0xff) > 200 && (p & 0xff) > 150;
    };
    //  The box around it: cream left and right of the mark.
    int box = 0;
    int box_total = 0;
    for (int y = 31; y <= 37; y++){
        for (int x = 15; x <= 34; x++){
            if (x >= 20 && x <= 27){
                continue;
            }
            box_total++;
            box += cream(x, y) ? 1 : 0;
        }
    }
    if (box * 10 < box_total * 9){
        return -1;
    }
    //  The mark: 7 x 7 pixels, about 25 of them not cream.
    int marked = 0;
    for (int y = 31; y <= 37; y++){
        for (int x = 20; x <= 26; x++){
            marked += cream(x, y) ? 0 : 1;
        }
    }
    if (marked >= 15){
        return 1;
    }
    if (marked <= 2){
        return 0;
    }
    return -1;
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

//  Whether at least one move name of the FIGHT menu can be read: the move
//  list is really open. (Its arrow detector also fires on some overworld
//  scenes, Route 9: the D-pad presses that pick a move would walk then.)
bool move_names_readable(ConsoleHandle& console){
    VideoSnapshot screen = console.video().snapshot();
    for (int i = 0; i < 4; i++){
        ImageViewRGB32 box = game_box(screen, MOVE_NAME_BOXES[i]);
        if (read_move_name(console.logger(), box, DARK_TEXT_FILTERS()) >= 0){
            return true;
        }
    }
    return false;
}

//  Read the four move names of the FIGHT menu.
void read_fight_menu(ConsoleHandle& console, StoryBattleMemory& memory){
    StoryMemoryScope memory_scope("reading moves");
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

//  Made once: making a name reader for some of the species loads the name
//  lists of every language.
const Pokemon::PokemonNameReader& species_name_reader(){
    static const Pokemon::PokemonNameReader reader(all_species_slugs());
    return reader;
}

//  Read the opponent's species from its HP box (top left).
const SpeciesInfo* read_opponent(ConsoleHandle& console, std::string& slug, const std::string& known = std::string()){
    StoryMemoryScope memory_scope("reading species");
    VideoSnapshot screen = console.video().snapshot();
    WildEncounterReader reader(COLOR_RED);
    PokemonFRLG_WildEncounter encounter = reader.read_encounter_with_reader(
        console.logger(), Language::English, screen, species_name_reader()
    );
    slug = encounter.name;
    if (!known.empty() && slug != known){
        //  A legendary step: the scene says which Pokemon it is. The name
        //  can read as another one (Mewtwo: "MEHTHO", MEOWTH as likely).
        const SpeciesInfo* legendary = find_species(known);
        if (legendary != nullptr){
            console.log(
                std::string("Opponent: ") + legendary->name + " (the legendary Pokemon of this step; the name read as \"" +
                slug + "\")."
            );
            slug = known;
            return legendary;
        }
    }
    const SpeciesInfo* info = find_species(slug);
    if (info != nullptr){
        console.log(std::string("Opponent: ") + info->name);
    }else{
        console.log("Opponent: not read.", COLOR_ORANGE);
    }
    return info;
}

std::string read_text(ConsoleHandle& console, const ImageFloatBox& box, bool bright){
    StoryMemoryScope memory_scope("reading text");
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

double type_effect(Type move_type, Type type1, Type type2){
    double m = AutoStoryDex::type_multiplier_x10(move_type, type1) / 10.0;
    if (type2 != type1){
        m *= AutoStoryDex::type_multiplier_x10(move_type, type2) / 10.0;
    }
    return m;
}

//  The types the moves of the Elite Four's and the Champion's Pokemon hit
//  with (src/data/trainer_parties.h, all three Champion teams). Others: the
//  types of the species.
const std::vector<Type>* league_attacks(const std::string& slug){
    static const std::map<std::string, std::vector<Type>> table{
        {"aerodactyl", {Type::FLYING, Type::NORMAL, Type::ROCK}},
        {"alakazam", {Type::PSYCHIC}},
        {"arbok", {Type::DARK, Type::POISON, Type::STEEL}},
        {"arcanine", {Type::DARK, Type::FIRE, Type::NORMAL}},
        {"blastoise", {Type::DARK, Type::NORMAL, Type::WATER}},
        {"charizard", {Type::FIRE, Type::FLYING, Type::NORMAL}},
        {"cloyster", {Type::WATER}},
        {"dewgong", {Type::ICE, Type::WATER}},
        {"dragonair", {Type::DRAGON, Type::NORMAL}},
        {"dragonite", {Type::DRAGON, Type::FLYING, Type::NORMAL}},
        {"exeggutor", {Type::GRASS, Type::NORMAL}},
        {"gengar", {Type::GHOST, Type::POISON}},
        {"golbat", {Type::DARK, Type::FLYING, Type::POISON}},
        {"gyarados", {Type::DARK, Type::DRAGON, Type::NORMAL, Type::WATER}},
        {"hitmonchan", {Type::FIGHTING, Type::ROCK}},
        {"hitmonlee", {Type::FIGHTING, Type::NORMAL}},
        {"jynx", {Type::ICE, Type::NORMAL}},
        {"lapras", {Type::ICE, Type::NORMAL, Type::WATER}},
        {"machamp", {Type::FIGHTING, Type::ROCK}},
        {"onix", {Type::GROUND, Type::NORMAL, Type::ROCK, Type::STEEL}},
        {"pidgeot", {Type::FLYING}},
        {"rhydon", {Type::GROUND, Type::NORMAL, Type::ROCK}},
        {"slowbro", {Type::ICE, Type::WATER}},
        {"venusaur", {Type::GRASS}},
    };
    auto iter = table.find(slug);
    return iter == table.end() ? nullptr : &iter->second;
}

//  How well a team member does against "foe": its best hit on the foe
//  over the foe's best hit on it (by types, same-type bonus included).
double team_score(const StoryTeamMember& member, const SpeciesInfo* foe){
    if (foe == nullptr){
        return member.strength;
    }
    double offense = 0.25;
    for (Type t : member.attacks){
        double e = type_effect(t, foe->type1, foe->type2);
        if (t == member.type1 || t == member.type2){
            e *= 1.5;
        }
        offense = std::max(offense, e);
    }
    std::vector<Type> foe_attacks{foe->type1, foe->type2};
    const std::vector<Type>* known = league_attacks(foe->slug);
    if (known != nullptr){
        foe_attacks = *known;
    }
    double defense = 0.25;
    for (Type t : foe_attacks){
        double e = type_effect(t, member.type1, member.type2);
        if (t == foe->type1 || t == foe->type2){
            e *= 1.5;
        }
        defense = std::max(defense, e);
    }
    return member.strength * offense / defense;
}

//  The species named in a text ("LORELEI is about to use DEWGONG."):
//  the longest name found in it.
const SpeciesInfo* species_in_text(const std::string& text){
    const SpeciesInfo* best = nullptr;
    size_t best_length = 0;
    for (const SpeciesInfo& info : AutoStoryDex::species()){
        std::string name;
        for (const char* c = info.name; *c; c++){
            name += (char)std::tolower((unsigned char)*c);
        }
        if (name.size() >= 3 && name.size() > best_length && text.find(name) != std::string::npos){
            best = &info;
            best_length = name.size();
        }
    }
    return best;
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

//  Stats at a level (IVs 15, no EVs).
int stat_at(int base, int level){
    return (2 * base + 15) * level / 100 + 5;
}
int hp_at(int base, int level){
    return (2 * base + 15) * level / 100 + level + 10;
}
bool physical_type(Type t){
    switch (t){
    case Type::NORMAL: case Type::FIGHTING: case Type::FLYING: case Type::POISON: case Type::GROUND:
    case Type::ROCK: case Type::BUG: case Type::GHOST: case Type::STEEL:
        return true;
    default:
        return false;
    }
}

//  Damage of a move by the Gen 3 formula (without the random factor and
//  crits), times its accuracy: the attacker's species and level from
//  "settings", against a foe of the same level with the given base
//  defenses and type multiplier. Fixed-damage moves do what they do.
double damage_estimate(
    int move, const StoryBattleSettings& settings,
    int foe_defense, int foe_sp_defense, double effect
){
    const MoveInfo& info = AutoStoryDex::moves()[(size_t)move];
    const SpeciesInfo& me = *settings.lead_species;
    const int level = settings.lead_level;
    double damage = 0;
    switch (info.move_class){
    case MoveClass::STATUS:
    case MoveClass::AVOID:
        return 0;
    case MoveClass::FIXED_20:
        damage = effect == 0 ? 0 : 20;
        break;
    case MoveClass::FIXED_40:
        damage = effect == 0 ? 0 : 40;
        break;
    case MoveClass::LEVEL_DAMAGE:
        damage = effect == 0 ? 0 : level;
        break;
    default:{
        const bool physical = physical_type(info.type);
        const double attack = stat_at(physical ? me.attack : me.sp_attack, level);
        const double defense = stat_at(physical ? foe_defense : foe_sp_defense, level);
        damage = ((2.0 * level / 5 + 2) * base_damage(info) * attack / defense) / 50 + 2;
        if (info.type == settings.lead_type1 || info.type == settings.lead_type2){
            damage *= 1.5;
        }
        damage *= effect;
    }
    }
    if (info.accuracy > 0){
        damage *= info.accuracy / 100.0;
    }
    return damage;
}
bool can_estimate(const StoryBattleSettings& settings){
    return settings.lead_species != nullptr && settings.lead_species->attack > 0 && settings.lead_level > 0;
}

//  Turns the best of "moves" takes to bring the foe down, as estimated.
//  -1: not known.
int turns_to_win(const int moves[4], const StoryBattleSettings& settings, const SpeciesInfo* foe){
    if (!can_estimate(settings) || foe == nullptr || foe->hp == 0){
        return -1;
    }
    double best = 0;
    bool known = false;
    for (int i = 0; i < 4; i++){
        if (moves[i] < 0){
            continue;
        }
        known = true;
        const MoveInfo& info = AutoStoryDex::moves()[(size_t)moves[i]];
        best = std::max(best, damage_estimate(moves[i], settings, foe->defense, foe->sp_defense, effectiveness(info.type, foe)));
    }
    if (!known){
        return -1;
    }
    const int hp = hp_at(foe->hp, settings.lead_level);
    return best <= 0 ? 99 : (int)std::ceil(hp / best);
}

double move_score(int move, const StoryBattleSettings& settings, const SpeciesInfo* opponent){
    if (move < 0){
        return -1;
    }
    if (opponent != nullptr && opponent->defense > 0 && can_estimate(settings)){
        const MoveInfo& info = AutoStoryDex::moves()[(size_t)move];
        return damage_estimate(move, settings, opponent->defense, opponent->sp_defense, effectiveness(info.type, opponent));
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

//  Weakest move that does damage, skipping "excluded" (bit mask): to bring a
//  Pokemon's HP down without making it faint. -1: none that does damage.
int weakest_move_slot(const StoryBattleMemory& memory, const StoryBattleSettings& settings,
                      const SpeciesInfo* opponent, uint8_t excluded){
    int best = -1;
    double best_score = 0;
    for (int i = 0; i < 4; i++){
        if ((excluded & (1 << i)) || memory.moves[i] < 0){
            continue;
        }
        const double score = move_score(memory.moves[i], settings, opponent);
        if (score > 0 && (best < 0 || score < best_score)){
            best_score = score;
            best = i;
        }
    }
    return best;
}

BattleMoveOption slot_option(int slot){
    switch (slot){
    case 0: return BattleMoveOption::MOVE_1;
    case 1: return BattleMoveOption::MOVE_2;
    case 2: return BattleMoveOption::MOVE_3;
    default: return BattleMoveOption::MOVE_4;
    }
}


//  The move list is open: pick the best move and use it. "weakest": the
//  weakest move that does damage instead (to weaken a Pokemon to catch).
void use_best_move(
    ConsoleHandle& console, ProControllerContext& context,
    const StoryBattleSettings& settings, StoryBattleMemory& memory,
    const SpeciesInfo* opponent, bool weakest = false
){
    //  Did the move picked last turn take any of the foe's HP? (The move
    //  list leaves both HP bars in view.)
    double foe_hp = -1;
    {
        VideoSnapshot screen = console.video().snapshot();
        if (screen){
            foe_hp = read_foe_hp(screen);
        }
    }
    if (opponent != nullptr && opponent != memory.foe_for_hp){
        //  Another foe: start over.
        memory.reset_foe();
        memory.foe_for_hp = opponent;
    }else if (memory.last_slot >= 0 && memory.last_slot < 4 && memory.foe_hp >= 0 && foe_hp >= 0){
        const int slot = memory.last_slot;
        const int move = memory.moves[slot];
        if (move >= 0 && base_damage(AutoStoryDex::moves()[(size_t)move]) > 0){
            const std::string name = AutoStoryDex::moves()[(size_t)move].name;
            if (foe_hp < memory.foe_hp - 0.01){
                memory.damage_turns++;
                memory.no_damage[slot] = 0;
            }else if (foe_hp > memory.foe_hp + 0.05){
                //  Its HP went up: it healed itself after the hit (Mewtwo:
                //  RECOVER). Nothing to tell about the move from that.
                console.log("The foe's HP went up (it healed): " + name + " not judged by it.");
            }else if (memory.no_damage[slot] > 0 && memory.damage_turns > memory.damage_turns_at[slot]){
                //  Again nothing, and since the first time another move did
                //  damage: the player's Pokemon can attack, this move does
                //  nothing to this foe.
                memory.useless |= (uint8_t)(1 << slot);
                console.log(name + " again took none of the foe's HP, another move did: not using it against this foe.", COLOR_ORANGE);
            }else if (weakest && memory.no_damage[slot] < 2){
                //  Weakening a Pokemon to catch it: the next weakest move
                //  can take far more (Articuno at 67% after one CUT, the
                //  second CUT missed, FLY then made it faint: two of nine
                //  tries). A miss or a turn without moving is far more
                //  likely than a move that does nothing to it: the same
                //  move again, another one only after three turns in a row
                //  without damage.
                memory.no_damage[slot]++;
                memory.damage_turns_at[slot] = memory.damage_turns;
                console.log(
                    name + " took none of the foe's HP (a miss, or a turn without moving). "
                    "Weakening it: the same move again.",
                    COLOR_ORANGE
                );
            }else{
                //  Nothing, but maybe the player's Pokemon did not move
                //  (asleep, paralyzed, confused) or missed: try another move
                //  once, then this one again.
                memory.no_damage[slot]++;
                memory.damage_turns_at[slot] = memory.damage_turns;
                memory.probe |= (uint8_t)(1 << slot);
                console.log(name + " took none of the foe's HP. Trying another move once.", COLOR_ORANGE);
            }
        }
    }

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

    uint8_t excluded = (uint8_t)(memory.no_pp | memory.useless | memory.probe);
    if ((excluded & 0x0f) == 0x0f){
        excluded = (uint8_t)(memory.no_pp | memory.useless);
    }
    if ((excluded & 0x0f) == 0x0f){
        excluded = memory.no_pp;
    }
    memory.probe = 0;
    int cursor_slot = 0;
    for (int attempt = 0; attempt < 4; attempt++){
        int slot = -1;
        if (weakest){
            slot = weakest_move_slot(memory, settings, opponent, excluded);
        }
        if (slot < 0){
            slot = best_move_slot(memory, settings, opponent, excluded);
        }
        cursor_slot = slot;
        if (memory.moves[slot] >= 0){
            console.log(std::string("Using ") + AutoStoryDex::moves()[(size_t)memory.moves[slot]].name);
        }else{
            console.log("Using move " + std::to_string(slot + 1));
        }
        if (!move_cursor_to_option(console, context, slot_option(slot))){
            //  The arrow not seen: the cursor would stay on the move used
            //  last (20 minutes of FLAMETHROWER against a GROWLITHE with
            //  FLASH FIRE, the moves not read). The move list does not wrap
            //  (HandleInputChooseMove()): up and left reach move 1 from
            //  anywhere, then on to the move.
            console.log("Moving to move " + std::to_string(slot + 1) + " without seeing the cursor.", COLOR_ORANGE);
            pbf_press_dpad(context, DPAD_UP, 160ms, 240ms);
            pbf_press_dpad(context, DPAD_LEFT, 160ms, 240ms);
            if (slot % 2 == 1){
                pbf_press_dpad(context, DPAD_RIGHT, 160ms, 240ms);
            }
            if (slot / 2 == 1){
                pbf_press_dpad(context, DPAD_DOWN, 160ms, 240ms);
            }
        }
        context.wait_for_all_requests();

        //  A move without PP shows its PP in red.
        BattleOutOfPpWatcher no_pp(COLOR_RED);
        if (wait_until(console, context, 400ms, { no_pp }) == 0){
            console.log("That move is out of PP.", COLOR_ORANGE);
            //  Not again in this battle (until another Pokemon comes in):
            //  picking it just brings the list back, and the turn would
            //  count as the move doing nothing to the foe.
            memory.no_pp |= (uint8_t)(1 << slot);
            excluded |= (uint8_t)(1 << slot);
            if (excluded == 0x0f){
                break;      //  Struggle: any move.
            }
            continue;
        }
        break;
    }
    memory.last_slot = cursor_slot;
    memory.foe_hp = foe_hp;
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    context.wait_for_all_requests();
}

//  At the battle menu: FIGHT, pick the best move.
void fight_one_turn(
    ConsoleHandle& console, ProControllerContext& context,
    const StoryBattleSettings& settings, StoryBattleMemory& memory
){
    std::string slug;
    const SpeciesInfo* opponent = read_opponent(console, slug, settings.legendary_species);

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


//  Item description in the bag. Window 1 of sDefaultBagWindowsStd:
//  tiles (5,14) 25x6.
const ImageFloatBox BAG_DESCRIPTION_BOX(0.170, 0.705, 0.810, 0.280);

std::string read_bag_text(ConsoleHandle& console){
    std::string text = read_text(console, BAG_DESCRIPTION_BOX, true);
    if (text.size() < 8){
        text += " " + read_text(console, BAG_DESCRIPTION_BOX, false);
    }
    return text;
}

//  How good a ball is, by its description in the bag (items.json). 0: not
//  a ball to throw (the SAFARI BALL works only in the Safari Zone).
int ball_rank(const std::string& text){
    auto has = [&](const char* word){
        return text.find(word) != std::string::npos;
    };
    if (has("safari") || has("camouflage")){
        return 0;
    }
    if (has("ultimate") || has("without fail")){
        return 4;       //  MASTER BALL
    }
    if (has("high-grade") || has("high grade") || has("very high")){
        return 3;       //  ULTRA BALL
    }
    if (has("quality") || has("than a standard")){
        return 2;       //  GREAT BALL
    }
    if (has("capsule") || has("somewhat different") || has("comfortable") || has("commemorate")){
        return 1;       //  POKe BALL, and the others that catch about as well
    }
    return 0;
}
const char* ball_name(int rank){
    switch (rank){
    case 4: return "MASTER BALL";
    case 3: return "ULTRA BALL";
    case 2: return "GREAT BALL";
    default: return "POKe BALL";
    }
}

enum class BagUse{
    USED,
    NONE,       //  no such item in the bag: back at the battle menu
    FAILED,     //  a menu did not come or the item did nothing: back at the battle menu
};

//  At the battle menu: BAG, then the item of "pocket" (0 ITEMS, 1 KEY ITEMS,
//  2 POKe BALLS) that "rank" values most (by its description; 0 = not one
//  to use). Leaves the bag open with the cursor on it, or (NONE, FAILED)
//  closes the bag again. "expected": the rank found last time in this
//  battle; the bag opens on the pocket and the item used last, so when that
//  one is still under the cursor it is taken without going through the
//  whole pocket again (some 20 s for each ball at a legendary Pokemon).
BagUse pick_in_battle_bag(
    ConsoleHandle& console, ProControllerContext& context, int pocket,
    const std::function<int(const std::string&)>& rank, const std::string& what, int& best_rank,
    int expected = 0
){
    if (!move_cursor_to_option(console, context, BattleMenuOption::BAG)){
        console.log("BAG not selected.", COLOR_ORANGE);
        return BagUse::FAILED;
    }
    pbf_press_button(context, BUTTON_A, 160ms, 1340ms);
    context.wait_for_all_requests();
    BagWatcher bag(COLOR_RED);
    if (wait_until(console, context, 4000ms, { bag }) != 0){
        console.log("The bag did not open.", COLOR_ORANGE);
        pbf_mash_button(context, BUTTON_B, 1500ms);
        context.wait_for_all_requests();
        return BagUse::FAILED;
    }
    if (expected > 0){
        pbf_wait(context, 400ms);
        context.wait_for_all_requests();
        if (rank(read_bag_text(console)) == expected){
            best_rank = expected;
            return BagUse::USED;
        }
    }
    //  The pockets do not wrap around: LEFT x3 is ITEMS, then KEY ITEMS,
    //  POKe BALLS. The list does not wrap either: UP to the top.
    for (int i = 0; i < 3; i++){
        pbf_press_dpad(context, DPAD_LEFT, 160ms, 500ms);
    }
    for (int i = 0; i < pocket; i++){
        pbf_press_dpad(context, DPAD_RIGHT, 160ms, 500ms);
    }
    for (int i = 0; i < 25; i++){
        pbf_press_dpad(context, DPAD_UP, 80ms, 80ms);
    }
    pbf_wait(context, 500ms);
    context.wait_for_all_requests();

    int best = -1;
    best_rank = 0;
    std::string last;
    int same = 0;
    for (int row = 0; row < 24; row++){
        std::string text = read_bag_text(console);
        if (text == last){
            if (++same >= 2){
                break;      //  The end of the list.
            }
        }else{
            same = 0;
            const int r = rank(text);
            if (r > best_rank){
                best_rank = r;
                best = row;
            }
        }
        last = text;
        pbf_press_dpad(context, DPAD_DOWN, 160ms, 340ms);
        context.wait_for_all_requests();
    }
    if (best < 0){
        console.log("No " + what + " in the bag.", COLOR_ORANGE);
        pbf_mash_button(context, BUTTON_B, 2000ms);
        context.wait_for_all_requests();
        return BagUse::NONE;
    }
    for (int i = 0; i < 25; i++){
        pbf_press_dpad(context, DPAD_UP, 80ms, 80ms);
    }
    for (int i = 0; i < best; i++){
        pbf_press_dpad(context, DPAD_DOWN, 160ms, 340ms);
    }
    pbf_wait(context, 300ms);
    context.wait_for_all_requests();
    std::string text = read_bag_text(console);
    if (rank(text) != best_rank){
        console.log("Not on the " + what + " to use: " + text, COLOR_ORANGE);
        pbf_mash_button(context, BUTTON_B, 2000ms);
        context.wait_for_all_requests();
        return BagUse::FAILED;
    }
    return BagUse::USED;
}

//  At the battle menu: the best ball, thrown. Without "master", not the
//  MASTER BALL (kept for a legendary Pokemon). "cheapest": the cheapest
//  ball instead (POKe BALL, GREAT BALL, ULTRA BALL; for the POKeDEX, where
//  a POKe BALL catches the most for the money).
//  "last_rank": the ball thrown last in this battle (0: none yet).
BagUse throw_best_ball(ConsoleHandle& console, ProControllerContext& context, bool master, int& last_rank, bool cheapest = false){
    int rank = 0;
    auto value = [master, cheapest](const std::string& text){
        const int r = ball_rank(text);
        if (r == 4){
            return master && !cheapest ? 4 : 0;
        }
        return cheapest && r > 0 ? 4 - r : r;
    };
    BagUse ret = pick_in_battle_bag(console, context, 2, value, "POKe BALL", rank, last_rank);
    if (ret != BagUse::USED){
        last_rank = 0;
        return ret;
    }
    last_rank = rank;
    console.log(std::string("Throwing a ") + ball_name(cheapest && rank > 0 && rank < 4 ? 4 - rank : rank) + ".", COLOR_BLUE);
    //  A: USE / CANCEL (on USE). A: the bag closes and the ball flies.
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1840ms);
    context.wait_for_all_requests();
    return BagUse::USED;
}

//  How much a healing item of the ITEMS pocket heals, by its description
//  (items.json): HP points, more for the ones that heal it all. 0: not
//  one (Revive, Ether, status medicine).
int heal_rank(const std::string& text){
    auto has = [&](const char* word){
        return text.find(word) != std::string::npos;
    };
    if (has("faint") || has("revive") || has(" pp") || has("level")){
        return 0;
    }
    if (has("fully restores the hp and") || has("heals any status")){
        return 1000;    //  FULL RESTORE
    }
    if (has("fully restores the hp") || has("fully restores hp")){
        return 999;     //  MAX POTION
    }
    //  "It restores the HP of one POKeMON by 200 points." (and the drinks)
    const size_t by = text.find(" by ");
    if (by == std::string::npos || !has("point")){
        return 0;
    }
    int value = 0;
    for (size_t i = by + 4; i < text.size() && i < by + 9; i++){
        const char c = text[i];
        if (c >= '0' && c <= '9'){
            value = value * 10 + (c - '0');
        }else if (c == 'o' || c == 'O'){
            value = value * 10;     //  OCR: "2OO"
        }else if (value > 0){
            break;
        }
    }
    return value >= 10 && value <= 300 ? value : 0;
}
const char* heal_name(int rank){
    if (rank >= 1000) return "FULL RESTORE";
    if (rank >= 999) return "MAX POTION";
    if (rank >= 200) return "HYPER POTION";
    if (rank >= 50) return "SUPER POTION";
    return "POTION";
}

//  At the battle menu: the best healing item on the Pokemon that is out
//  (the first one in the party menu, which shows the battle order).
BagUse heal_in_battle(ConsoleHandle& console, ProControllerContext& context){
    int rank = 0;
    BagUse ret = pick_in_battle_bag(console, context, 0, heal_rank, "healing item", rank);
    if (ret != BagUse::USED){
        return ret;
    }
    console.log(std::string("Using a ") + heal_name(rank) + " on the Pokemon that is out.", COLOR_BLUE);
    //  A: USE / CANCEL (on USE). A: the party menu ("Use on which POKeMON?"),
    //  the cursor on the first one.
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1840ms);
    context.wait_for_all_requests();
    PartyMenuWatcher party(COLOR_RED);
    if (wait_until(console, context, 3000ms, { party }) != 0){
        console.log("The party menu did not open.", COLOR_ORANGE);
        pbf_mash_button(context, BUTTON_B, 2000ms);
        context.wait_for_all_requests();
        return BagUse::FAILED;
    }
    pbf_press_button(context, BUTTON_A, 160ms, 2840ms);
    context.wait_for_all_requests();
    //  "It won't have any effect." goes back to the bag.
    BagWatcher bag(COLOR_RED);
    if (wait_until(console, context, 1000ms, { bag }) == 0){
        console.log("The item had no effect.", COLOR_ORANGE);
        pbf_mash_button(context, BUTTON_B, 2000ms);
        context.wait_for_all_requests();
        return BagUse::FAILED;
    }
    //  "X's HP was restored by N points." Then the turn goes on.
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    context.wait_for_all_requests();
    return BagUse::USED;
}


}   //  namespace



struct StoryShinyWatch::Data{
    class Detector : public ShinySoundDetector{
    public:
        using ShinySoundDetector::ShinySoundDetector;
        uint64_t spectrums() const{
            return m_spectrums_processed;
        }
    };

    Data(ConsoleHandle& console, ProControllerContext& context, StoryShinyWatch& watch)
        : scope(static_cast<CancellableScope&>(context))
        , detector(console.logger(), [&watch](float){
            //  From the audio inference thread. Never stops anything.
            std::lock_guard<std::mutex> lg(watch.m_lock);
            watch.m_heard.push_back(current_time());
            if (watch.m_heard.size() > 8){
                watch.m_heard.erase(watch.m_heard.begin());
            }
            return false;
        })
        , session(scope, console, { detector })
    {}

    CancellableHolder<CancellableScope> scope;
    Detector detector;
    InferenceSession session;
};

StoryShinyWatch::StoryShinyWatch(ConsoleHandle& console, ProControllerContext& context)
    : m_console(console)
    , m_context(context)
    , m_started(current_time())
    , m_detector_started(m_started)
    , m_data(new Data(console, context, *this))
{}
StoryShinyWatch::~StoryShinyWatch() = default;

bool StoryShinyWatch::heard(WallClock from, WallClock to) const{
    std::lock_guard<std::mutex> lg(m_lock);
    for (WallClock time : m_heard){
        if (time > m_consumed && time >= from && time <= to){
            return true;
        }
    }
    return false;
}
void StoryShinyWatch::consume(){
    std::lock_guard<std::mutex> lg(m_lock);
    m_consumed = current_time();
}
bool StoryShinyWatch::has_sound() const{
    return m_had_sound || (m_data != nullptr && m_data->detector.spectrums() > 0);
}
void StoryShinyWatch::restart(){
    m_had_sound = has_sound();
    m_data.reset();
    m_detector_started = current_time();
    m_data.reset(new Data(m_console, m_context, *this));
}



std::string read_story_text(
    ConsoleHandle& console, const ImageFloatBox& box, bool bright
){
    return read_text(console, box, bright);
}

double move_value(int move, const StoryBattleSettings& settings){
    if (move < 0){
        return 0;
    }
    if (can_estimate(settings)){
        //  Against a middling foe (base defenses 70, no type matchup).
        return damage_estimate(move, settings, 70, 70, 1.0);
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
    const StoryBattleSettings& base_settings,
    StoryBattleMemory& memory
){
    StoryMemoryScope memory_scope("battle");
    memory.battles++;
    memory.last_slot = -1;
    memory.list_back = 0;
    memory.caught.clear();
    memory.shiny = false;
    memory.shiny_species.clear();
    memory.shiny_caught = false;
    memory.shiny_to_pc = false;
    memory.dex_registered = false;
    memory.safari_over = false;
    memory.no_balls = false;
    memory.legendary_balls = 0;
    memory.legendary_ball_worth = 0;
    memory.legendary_no_balls = false;
    memory.safari_balls = 0;
    memory.foe_mark = -1;
    memory.owned_by_mark.clear();
    memory.dex_by_mark.clear();

    //  The active Pokemon's types count for the moves (same-type bonus).
    StoryBattleSettings settings = base_settings;

    //  Team play (trainer battles): the battle order of the team (team
    //  indexes, the active first; the in-battle party menu shows this
    //  order), who is down, and the foe the active was chosen against.
    const bool team = !settings.team.empty() && kind == StoryBattleKind::TRAINER;
    std::vector<int> order;
    std::vector<bool> down(settings.team.size(), false);
    const SpeciesInfo* foe = nullptr;
    std::string evaluated;
    memory.team_used.clear();
    memory.team_fainted.clear();
    if (team){
        for (size_t i = 0; i < settings.team.size(); i++){
            order.push_back((int)i);
        }
        memory.team_used.insert(0);
    }
    //  The member at battle position "pos" came in.
    auto set_active = [&](int pos){
        std::swap(order[0], order[(size_t)pos]);
        const StoryTeamMember& member = settings.team[(size_t)order[0]];
        settings.lead_type1 = member.type1;
        settings.lead_type2 = member.type2;
        memory.team_used.insert(order[0]);
        memory.no_pp = 0;
        memory.last_slot = -1;
        memory.list_back = 0;
        for (int& m : memory.moves){
            m = -1;
        }
    };
    //  The battle position of the best member against "against" (-1: none).
    auto best_position = [&](const SpeciesInfo* against, bool with_active){
        int best = -1;
        double best_score = -1;
        for (size_t p = with_active ? 0 : 1; p < order.size(); p++){
            int index = order[p];
            if (down[(size_t)index] || !settings.team[(size_t)index].fights){
                continue;
            }
            double score = team_score(settings.team[(size_t)index], against);
            if (score > best_score){
                best_score = score;
                best = (int)p;
            }
        }
        return best;
    };
    auto score_at = [&](int pos, const SpeciesInfo* against){
        int index = order[(size_t)pos];
        return down[(size_t)index] ? 0.0 : team_score(settings.team[(size_t)index], against);
    };
    //  In the party menu (cursor on the active): send in the one at "pos".
    auto pick_in_party_menu = [&](int pos){
        for (int i = 0; i < pos; i++){
            pbf_press_dpad(context, DPAD_DOWN, 160ms, 340ms);
        }
        //  A opens the small menu, A on SHIFT sends it in.
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        pbf_press_button(context, BUTTON_A, 160ms, 1840ms);
        context.wait_for_all_requests();
    };

    StoryBattleResult result = StoryBattleResult::WON;
    bool seen_battle = false;
    bool shared = false;        //  the member came out and the other one went in
    memory.learner.clear();
    memory.player_hp = -1;
    memory.reset_foe();
    bool decided = false;       //  wild: fight/run/catch decided
    bool run = false;
    bool safari = false;
    int run_attempts = 0;
    bool lost = false;
    int next_slot = 1;
    int unknown_in_row = 0;     //  turns of the loop with nothing recognized
    int list_only_in_row = 0;   //  of those, how many showed the move list
    //  "Move lists" in a row with no move name read: then not taken for the
    //  move list (Route 7, three times in a night: 20 minutes each of
    //  "Moves: [-] [-] [-] [-]", A, + and B changing nothing).
    int unreadable_lists_in_row = 0;
    //  The last press was a blind one ("Nothing recognized"): a move list
    //  open now may be its doing (A on FIGHT), not a pick that did not go
    //  through.
    bool pressed_blind = false;
    //  A battle for sure: its menu, a move list, a level-up window, a
    //  question or the party menu was seen. (The battle text detectors
    //  also take a plain white area at the bottom of the overworld, like
    //  on the S.S. Anne, for a text box.)
    bool battle_confirmed = false;
    int text_only_in_row = 0;   //  turns with only a text box (no arrow) seen
    //  Turns with only a white text box seen, one after the other.
    int white_in_row = 0;
    bool white_last = false;
    //  The POKeDEX page seen again and again after it was closed.
    int dex_page_in_row = 0;
    WallClock deadline = current_time() + 20min;
    WallClock last_activity = current_time();

    //  A shiny: its sound played in the intro of a wild battle. Looked at on
    //  the first battle menu: heard from a little before the battle was
    //  noticed (in the dark only its text shows it) until shortly after the
    //  first thing seen in it ("Wild X appeared!"). The sparkles come before
    //  that text, those of the player's own Pokemon after it.
    const WallClock entered = current_time();
    WallClock intro_seen = WallClock::max();
    const bool wild = kind != StoryBattleKind::TRAINER;
    bool shiny_checked = false;
    bool mark_checked = false;
    bool shiny_catching = false;
    bool shiny_safari = false;
    bool shiny_wanted = false;
    bool shiny_caught = false;
    bool legendary = false;
    bool dex_catch = false;     //  a species wanted for the POKeDEX
    bool roamer = false;        //  the roaming legendary: the MASTER BALL
    int last_ball_rank = 0;
    //  Weakening a legendary Pokemon (settings.weaken_legendary): its HP
    //  before the last hit, the most one hit took, the hits so far.
    bool weakened = false;
    double weaken_hp_before = -1;
    double weaken_max_drop = -1;
    int weaken_turns = 0;
    double balls_from_hp = -1;      //  its HP when the balls began
    int balls_thrown = 0;
    int throw_failures = 0;
    int heals_used = 0;
    int heal_failures = 0;
    //  "Faster battles" (settings.fast_end): fades to black in this battle
    //  after which the start menu was tried for nothing, and all the fades
    //  looked at. A few of the first or many of the second, and the fade is
    //  no longer looked at in this battle (a place where the screen stays
    //  dark, or something that keeps fading).
    int fade_misses = 0;
    int fades_seen = 0;
    //  The battle is over: what it was.
    auto over = [&](){
        if (!shiny_caught){
            return result;
        }
        memory.shiny_caught = true;
        if (shiny_wanted){
            memory.caught = memory.shiny_species;
            return StoryBattleResult::CAUGHT;
        }
        return result == StoryBattleResult::LEAD_FAINTED ? result : StoryBattleResult::WON;
    };

    while (current_time() < deadline){
        BattleMenuWatcher battle_menu(COLOR_RED);
        BattleLearnDialogWatcher question(COLOR_RED);
        PartyMenuWatcher party_menu(COLOR_RED);
        AdvanceBattleDialogWatcher advance(COLOR_RED);
        //  After a ball: the POKeDEX page of a species caught the first time.
        PokedexRegisteredWatcher dex_page(COLOR_RED, &console.overlay());
        std::vector<PeriodicInferenceCallback> callbacks{ battle_menu, question, party_menu, advance };
        if (balls_thrown > 0){
            callbacks.emplace_back(dex_page);
        }
        //  Experimental ("Faster battles"): the fade to black at the end of
        //  the battle. Only in a battle that is one for sure and still goes
        //  on as a plain fight or run: not while catching (the POKeDEX page,
        //  the nickname question), not once the party fainted (the screen
        //  then stays black for the game's text).
        BlackScreenWatcher fade(COLOR_RED);
        int fade_index = -1;
        if (settings.fast_end && battle_confirmed && seen_battle && !lost &&
            balls_thrown == 0 && !shiny_catching && !shiny_caught &&
            fade_misses < 3 && fades_seen < 20
        ){
            fade_index = (int)callbacks.size();
            callbacks.emplace_back(fade);
        }
        context.wait_for_all_requests();
        int ret = wait_until(console, context, 6000ms, callbacks);
        if (fade_index >= 0 && ret == fade_index){
            //  Black. What comes after it, once the picture is back: the
            //  overworld (the battle is over), more of the battle (back from
            //  the party menu or the bag), an evolution, or the overworld
            //  with someone talking.
            const WallClock fade_seen = current_time();
            fades_seen++;
            {
                //  The same part of the game's picture as the watcher above
                //  (PokemonFRLG_DialogDetector.h), now for "no longer black".
                PokemonAutomation::BlackScreenWatcher picture_back(
                    COLOR_RED,
                    GameSettings::instance().GAME_BOX.inner_to_outer({0.231692, 0.0616538, 0.551385, 0.9045}),
                    100, 10,
                    PokemonAutomation::BlackScreenWatcher::FinderType::GONE, 100ms
                );
                //  Still black after 3 seconds: looked at all the same.
                wait_until(console, context, 3000ms, { picture_back });
            }
            bool battle_goes_on = false;
            {
                //  The fade-in takes about a third of a second, and the
                //  game takes no input before it is over. Anything of a
                //  battle in that time: it goes on.
                BattleMenuWatcher menu_again(COLOR_RED);
                BattleLearnDialogWatcher question_again(COLOR_RED);
                PartyMenuWatcher party_again(COLOR_RED);
                AdvanceBattleDialogWatcher advance_again(COLOR_RED);
                battle_goes_on = wait_until(
                    console, context, 600ms,
                    { menu_again, question_again, party_again, advance_again }
                ) >= 0;
            }
            if (!battle_goes_on){
                //  A text box of the battle (also an evolution: never B
                //  there), a level-up window or overworld text: not free to
                //  move. Left to the loop, which knows what to do with each.
                VideoSnapshot screen = console.video().snapshot();
                BattleLevelUpDetector level_up(COLOR_RED, BattleLevelUpDialog::either, Language::English);
                BattleDialogDetector battle_text(COLOR_RED);
                WhiteDialogDetector white_dialog(COLOR_RED);
                battle_goes_on = !screen ||
                    level_up.detect(screen) || battle_text.detect(screen) || white_dialog.detect(screen);
            }
            if (battle_goes_on){
                console.log("The screen faded to black and came back with the battle (or text): it goes on.");
            }else{
                //  Nothing of a battle on the screen. + opens the start menu
                //  only where the player is free to move. (In a battle, an
                //  evolution or a scene, + does nothing.) No B before the
                //  menu is seen: B stops an evolution.
                StartMenuWatcher start_menu(COLOR_RED);
                const int opened = run_until<ProControllerContext>(
                    console, context,
                    [](ProControllerContext& ctx){
                        pbf_press_button(ctx, BUTTON_PLUS, 200ms, 1300ms);
                        pbf_press_button(ctx, BUTTON_PLUS, 200ms, 1300ms);
                    },
                    { start_menu }
                );
                if (opened == 0){
                    pbf_mash_button(context, BUTTON_B, 500ms);
                    context.wait_for_all_requests();
                    const uint64_t took = (uint64_t)std::chrono::duration_cast<Milliseconds>(
                        current_time() - fade_seen
                    ).count();
                    memory.fast_ends++;
                    memory.fast_end_ms += took;
                    console.log(
                        "Battle over: the screen faded to black, and the player is free to move " +
                        std::to_string(took) + " ms later (\"Faster battles\", experimental)."
                    );
                    return over();
                }
                context.wait_for_all_requests();
                //  Two presses of + for nothing (3 seconds): after a few of
                //  those in one battle the fade is no longer looked at.
                fade_misses++;
                memory.fast_end_misses++;
                console.log(
                    "The screen faded to black, nothing of a battle shows, but the start menu does not open: "
                    "the player is not free to move. Going on as before.",
                    COLOR_ORANGE
                );
            }
            continue;
        }
        if (ret >= 0 && intro_seen == WallClock::max()){
            intro_seen = current_time();
        }
        if (ret >= 0){
            unknown_in_row = 0;
            list_only_in_row = 0;
            unreadable_lists_in_row = 0;
            text_only_in_row = 0;
            pressed_blind = false;
        }
        const bool white_before = white_last;
        white_last = false;
        if (ret != 4){
            dex_page_in_row = 0;
        }
        if (ret == 0 || ret == 1 || ret == 2){
            battle_confirmed = true;
        }
        if (ret == 0 || ret == 3){
            //  The battle menu or battle text: the player's HP bar shows.
            VideoSnapshot screen = console.video().snapshot();
            if (screen){
                double hp = read_player_hp(screen);
                if (hp >= 0){
                    memory.player_hp = hp;
                    if (team && !order.empty() && order[0] >= 0 && order[0] < 6){
                        memory.team_hp[order[0]] = hp;
                    }
                }
            }
        }
        switch (ret){
        case 0:{
            seen_battle = true;
            last_activity = current_time();
            memory.list_back = 0;   //  The last pick went through.
            if (wild && !mark_checked){
                //  The game's own word on the species: the ball mark of the
                //  foe's box (at the first menu: a status would clear it).
                mark_checked = true;
                VideoSnapshot screen = console.video().snapshot();
                if (screen){
                    memory.foe_mark = read_caught_mark(screen);
                    console.log(
                        memory.foe_mark == 1 ? "The foe's box has the ball mark: its species is caught already." :
                        memory.foe_mark == 0 ? "The foe's box has no ball mark: its species is not caught yet." :
                        "The ball mark of the foe's box was not read."
                    );
                }
            }
            if (!shiny_checked && wild && settings.catch_legendary){
                shiny_checked = true;
                std::string slug;
                read_opponent(console, slug, settings.legendary_species);
                memory.shiny_species = slug;
                legendary = true;
                shiny_wanted = true;
                shiny_catching = true;
                shiny_safari = false;
                console.log("Catching " + (slug.empty() ? std::string("the legendary Pokemon") : slug) + ".", COLOR_BLUE);
            }
            if (!shiny_checked && wild && !settings.roamer_species.empty()){
                std::string slug;
                read_opponent(console, slug, settings.legendary_species);
                if (!slug.empty() && settings.roamer_species.count(slug) != 0){
                    shiny_checked = true;
                    roamer = true;
                    legendary = true;
                    shiny_wanted = true;
                    shiny_catching = true;
                    shiny_safari = false;
                    memory.shiny_species = slug;
                    console.log("The roaming " + slug + "! The MASTER BALL, before it runs.", COLOR_BLUE);
                }
            }
            if (!shiny_checked){
                shiny_checked = true;
                if (wild && settings.shiny_watch != nullptr &&
                    settings.shiny_watch->heard(entered - 6s, intro_seen + 3s)
                ){
                    memory.shiny = true;
                    shiny_safari = menu_is_safari(console);
                    std::string slug;
                    read_opponent(console, slug, settings.legendary_species);
                    memory.shiny_species = slug;
                    shiny_wanted = !slug.empty() &&
                        (settings.catch_species.count(slug) != 0 || settings.dex_species.count(slug) != 0);
                    console.log("A shiny Pokemon: " + (slug.empty() ? std::string("(not read)") : slug) + "!", COLOR_BLUE);
                    if (settings.shiny_catch || shiny_wanted){
                        shiny_catching = true;
                    }else{
                        console.log(
                            "Not caught: \"Catch shiny Pokemon\" is off, or the party slot is still needed for "
                            "the team.",
                            COLOR_ORANGE
                        );
                    }
                }
                //  For the POKeDEX: a species not caught yet. (The two
                //  NIDORANs read alike: either, while one is wanted.) The
                //  game's ball mark decides over the program's list: with
                //  it, not caught again; without it, caught even if the
                //  list has it.
                if (!shiny_catching && wild && !settings.dex_shiny_only &&
                    (!settings.dex_species.empty() || !settings.dex_all.empty())
                ){
                    std::string slug;
                    read_opponent(console, slug, settings.legendary_species);
                    const bool nidoran = slug.rfind("nidoran", 0) == 0 &&
                        (settings.dex_species.count("nidoran-f") != 0 || settings.dex_species.count("nidoran-m") != 0);
                    bool wanted = !slug.empty() && (nidoran || settings.dex_species.count(slug) != 0);
                    if (wanted && memory.foe_mark == 1 && !nidoran && settings.dex_extra.count(slug) != 0){
                        console.log(
                            "The game shows " + slug + " as caught (the ball mark): another one, to evolve.",
                            COLOR_BLUE
                        );
                    }else if (wanted && memory.foe_mark == 1){
                        console.log("The game shows " + slug + " as caught (the ball mark): not caught again.", COLOR_BLUE);
                        wanted = false;
                        if (!nidoran){
                            memory.owned_by_mark = slug;
                        }
                    }else if (!wanted && memory.foe_mark == 0 && !slug.empty() && settings.dex_all.count(slug) != 0){
                        console.log(
                            "The game shows " + slug + " as not caught (no ball mark), though the program's list has it: "
                            "caught for the POKeDEX.",
                            COLOR_BLUE
                        );
                        wanted = true;
                        memory.dex_by_mark = slug;
                    }
                    if (wanted){
                        dex_catch = true;
                        shiny_catching = true;
                        shiny_wanted = true;
                        shiny_safari = menu_is_safari(console);
                        memory.shiny_species = slug;
                        console.log("Catching " + slug + " for the POKeDEX.", COLOR_BLUE);
                    }
                }
            }
            if (shiny_catching && legendary && settings.weaken_legendary && weakened && weaken_turns < 40){
                //  It healed itself while the balls came (Mewtwo: RECOVER,
                //  half its HP a time): at full HP a ball catches it half as
                //  often as in the red. Its HP down again first.
                double foe_hp = -1;
                {
                    VideoSnapshot screen = console.video().snapshot();
                    if (screen){
                        foe_hp = read_foe_hp(screen);
                    }
                }
                if (foe_hp >= 0.5 && foe_hp > balls_from_hp + 0.1){
                    console.log(
                        "Its HP is up again (" + std::to_string((int)(foe_hp * 100 + 0.5)) + "%): weakening it again.",
                        COLOR_BLUE
                    );
                    weakened = false;
                    //  Its HP since the last hit: its healing, not a hit.
                    weaken_hp_before = -1;
                }
            }
            if (shiny_catching){
                //  The Safari Zone: 20 of the 30 SAFARI BALLs at most (the
                //  game ends when they run out, and the way goes on there).
                //  For the POKeDEX: all of them there (the game ends when
                //  they run out), elsewhere up to dex_max_throws.
                const int max_throws = shiny_safari ? (dex_catch ? 30 : 20)
                    : (legendary ? 99 : (dex_catch ? std::max(1, settings.dex_max_throws) : 60));
                if (balls_thrown >= max_throws || throw_failures >= 4){
                    console.log(
                        dex_catch ? "No more balls thrown at it: running." : "No more balls thrown at the shiny: the battle goes on as usual.",
                        COLOR_ORANGE
                    );
                    shiny_catching = false;
                }else if (legendary && settings.weaken_legendary && !weakened && [&](){
                    //  Its HP down first: the weakest move, as long as one
                    //  more hit (the most one took so far, a quarter more for
                    //  the random factor) still leaves it 6%. A critical hit
                    //  can still make it faint: then the save before it.
                    double foe_hp = -1;
                    {
                        VideoSnapshot screen = console.video().snapshot();
                        if (screen){
                            foe_hp = read_foe_hp(screen);
                        }
                    }
                    if (foe_hp >= 0 && weaken_hp_before >= 0 && weaken_hp_before - foe_hp > 0.005){
                        weaken_max_drop = std::max(weaken_max_drop, weaken_hp_before - foe_hp);
                    }
                    const bool safe = weaken_max_drop < 0 || foe_hp - weaken_max_drop * 1.25 > 0.06;
                    if (foe_hp > 0.2 && safe && weaken_turns < 40){
                        std::string slug;
                        const SpeciesInfo* opponent = read_opponent(console, slug, settings.legendary_species);
                        if (move_cursor_to_option(console, context, BattleMenuOption::FIGHT)){
                            pbf_press_button(context, BUTTON_A, 160ms, 640ms);
                            context.wait_for_all_requests();
                            BattleSelectionArrowWatcher move_list(COLOR_RED, &console.overlay(), BATTLE_MOVE_ARROW_BOX);
                            if (wait_until(console, context, 3000ms, { move_list }) == 0){
                                console.log(
                                    "Its HP: " + std::to_string((int)(foe_hp * 100 + 0.5)) + "%. Weakening it.",
                                    COLOR_BLUE
                                );
                                use_best_move(console, context, settings, memory, opponent, true);
                                weaken_turns++;
                                weaken_hp_before = foe_hp;
                                return true;
                            }
                            pbf_press_button(context, BUTTON_B, 160ms, 840ms);
                            context.wait_for_all_requests();
                        }
                    }
                    weakened = true;
                    balls_from_hp = foe_hp;
                    console.log(
                        foe_hp < 0
                            ? std::string("Its HP not read: the balls now.")
                            : "Its HP: " + std::to_string((int)(foe_hp * 100 + 0.5)) + "%. The balls now.",
                        COLOR_BLUE
                    );
                    return false;
                }()){
                    break;
                }else if (shiny_safari){
                    if (move_cursor_to_option(console, context, SafariBattleMenuOption::BALL)){
                        balls_thrown++;
                        memory.safari_balls++;
                        console.log("SAFARI BALL " + std::to_string(balls_thrown) + (dex_catch ? "." : " at the shiny."), COLOR_BLUE);
                        pbf_press_button(context, BUTTON_A, 160ms, 1840ms);
                        context.wait_for_all_requests();
                    }else{
                        throw_failures++;
                    }
                    break;
                }else{
                    if (legendary && deadline < current_time() + 150min){
                        //  Up to 99 balls at half a minute each.
                        deadline = current_time() + 150min;
                    }
                    switch (throw_best_ball(console, context, legendary && (settings.legendary_master || roamer), last_ball_rank, dex_catch)){
                    case BagUse::USED:
                        balls_thrown++;
                        throw_failures = 0;
                        if (legendary){
                            memory.legendary_balls++;
                            memory.legendary_ball_worth +=
                                last_ball_rank >= 4 ? 100.0 : (last_ball_rank == 3 ? 2.0 : (last_ball_rank == 2 ? 1.5 : 1.0));
                        }
                        console.log("Ball " + std::to_string(balls_thrown) + (legendary || dex_catch ? "." : " at the shiny."), COLOR_BLUE);
                        break;
                    case BagUse::NONE:
                        if (legendary && settings.catch_legendary && !roamer){
                            //  A legendary Pokemon of the story and no ball
                            //  left: fighting on would only make it faint
                            //  (a minute a try). The battle is left as it
                            //  is: the step fails, and the save in front of
                            //  the Pokemon is loaded.
                            memory.legendary_no_balls = true;
                            console.log(
                                "No balls left (" + std::to_string(balls_thrown) + " thrown). The battle is left as it is: "
                                "the save in front of it comes next.",
                                COLOR_ORANGE
                            );
                            return StoryBattleResult::OUT_OF_BALLS;
                        }
                        console.log(
                            dex_catch ? "No balls left: running." : "No balls left for the shiny: the battle goes on as usual.",
                            COLOR_ORANGE
                        );
                        shiny_catching = false;
                        if (dex_catch){
                            memory.no_balls = true;
                        }
                        break;
                    case BagUse::FAILED:
                        throw_failures++;
                        break;
                    }
                    break;
                }
            }
            //  Low on HP in a trainer battle: a healing item instead of a move.
            if (kind == StoryBattleKind::TRAINER && settings.heal_in_battle && !memory.no_heal_items &&
                memory.player_hp >= 0 && memory.player_hp < 1.0 / 3 && heals_used < 10 && heal_failures < 2
            ){
                console.log(
                    "HP " + std::to_string((int)(memory.player_hp * 100 + 0.5)) + "%: healing with an item.",
                    COLOR_BLUE
                );
                switch (heal_in_battle(console, context)){
                case BagUse::USED:
                    heals_used++;
                    memory.player_hp = -1;      //  Seen again on the next turn.
                    break;
                case BagUse::NONE:
                    memory.no_heal_items = true;
                    break;
                case BagUse::FAILED:
                    heal_failures++;
                    break;
                }
                break;
            }
            if ((settings.share_position > 0 || settings.slow_share_position > 0) &&
                !shared && kind == StoryBattleKind::WILD_FIGHT
            ){
                shared = true;
                //  The member was sent out: it gets half the experience. Its
                //  moves first (it may learn one while the other fights),
                //  then the other one comes in. They stay the same until a
                //  level-up: no need to read them every battle.
                if (memory.member_moves_known){
                    //  The member is the active one.
                    for (int i = 0; i < 4; i++){
                        memory.moves[i] = memory.member_moves[i];
                    }
                }else if (move_cursor_to_option(console, context, BattleMenuOption::FIGHT)){
                    pbf_press_button(context, BUTTON_A, 160ms, 640ms);
                    context.wait_for_all_requests();
                    BattleSelectionArrowWatcher move_list(COLOR_RED, &console.overlay(), BATTLE_MOVE_ARROW_BOX);
                    if (wait_until(console, context, 3000ms, { move_list }) == 0){
                        read_fight_menu(console, memory);
                        for (int i = 0; i < 4; i++){
                            memory.member_moves[i] = memory.moves[i];
                            if (memory.moves[i] >= 0){
                                memory.member_moves_known = true;
                            }
                        }
                    }
                    pbf_press_button(context, BUTTON_B, 160ms, 840ms);
                    context.wait_for_all_requests();
                }
                int position = settings.share_position;
                bool switch_in = true;
                if (position <= 0){
                    //  The member fights alone, unless this foe takes it long.
                    std::string slug;
                    const SpeciesInfo* seen = read_opponent(console, slug, settings.legendary_species);
                    const int turns = turns_to_win(memory.moves, settings, seen);
                    if (turns >= 4){
                        console.log(
                            "The member needs about " + std::to_string(turns) + " turns against " +
                            (seen == nullptr ? std::string("this foe") : std::string(seen->name)) +
                            ": the other one comes in, the experience is shared.",
                            COLOR_BLUE
                        );
                        position = settings.slow_share_position;
                    }else{
                        switch_in = false;
                    }
                }
                if (switch_in){
                    if (move_cursor_to_option(console, context, BattleMenuOption::POKEMON)){
                        console.log("Shared experience: battle position " + std::to_string(position + 1) + " comes in.");
                        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                        PartyMenuWatcher party(COLOR_RED);
                        context.wait_for_all_requests();
                        if (wait_until(console, context, 3000ms, { party }) == 0){
                            pick_in_party_menu(position);
                            memory.player_hp = -1;  //  That was the member's.
                            settings.lead_type1 = settings.share_type1;
                            settings.lead_type2 = settings.share_type2;
                            settings.lead_species = nullptr;    //  Not known: valued by power and type.
                            settings.lead_level = 0;
                            memory.no_pp = 0;
                            memory.last_slot = -1;
                            for (int& m : memory.moves){
                                m = -1;
                            }
                        }else{
                            console.log("The party menu did not open.", COLOR_ORANGE);
                            pbf_mash_button(context, BUTTON_B, 1000ms);
                        }
                    }else{
                        console.log("POKeMON not selected: the member fights.", COLOR_ORANGE);
                    }
                    break;
                }
                //  The member fights: on to its move, in this same menu.
            }
            if (team){
                //  A new foe: the best answer comes in, if much better than
                //  the active (the switch costs the turn).
                std::string slug;
                const SpeciesInfo* seen = read_opponent(console, slug, settings.legendary_species);
                if (seen != nullptr){
                    foe = seen;
                }
                if (seen != nullptr && slug != evaluated){
                    evaluated = slug;
                    int pos = best_position(foe, false);
                    if (pos > 0 && score_at(pos, foe) >= score_at(0, foe) * 1.5 + 0.5 &&
                        move_cursor_to_option(console, context, BattleMenuOption::POKEMON)
                    ){
                        console.log("Switching to team member " + std::to_string(order[(size_t)pos] + 1) + " against " + seen->name + ".");
                        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                        PartyMenuWatcher party(COLOR_RED);
                        context.wait_for_all_requests();
                        if (wait_until(console, context, 3000ms, { party }) == 0){
                            pick_in_party_menu(pos);
                            set_active(pos);
                        }else{
                            console.log("The party menu did not open.", COLOR_ORANGE);
                            pbf_mash_button(context, BUTTON_B, 1000ms);
                        }
                        break;
                    }
                }
            }
            if (!decided){
                decided = true;
                safari = menu_is_safari(console);
                //  A wanted species: in a catch step, and also when met on
                //  the way in a wild battle.
                bool catching = !safari && (kind == StoryBattleKind::CATCH ||
                    (kind == StoryBattleKind::WILD && !settings.catch_species.empty()));
                bool wanted = false;
                if (catching){
                    std::string slug;
                    read_opponent(console, slug, settings.legendary_species);
                    wanted = settings.catch_species.count(slug) != 0;
                    if (wanted){
                        console.log("Catching " + slug + ".");
                        int thrown = auto_catch(console, context, settings.catch_balls);
                        if (thrown > 0){
                            console.log("Caught " + slug + ".", COLOR_BLUE);
                            memory.caught = slug;
                            return StoryBattleResult::CAUGHT;
                        }
                        //  Not caught (out of balls?) or the battle went on.
                        if (in_overworld(console, context)){
                            return StoryBattleResult::RAN;
                        }
                    }
                }
                if (safari){
                    console.log("Safari Zone battle: running.");
                    run = true;
                }else if (kind == StoryBattleKind::WILD_RUN || (kind == StoryBattleKind::WILD && settings.wild_run)){
                    run = true;
                }else if (kind == StoryBattleKind::CATCH || wanted){
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
            case Question::DELETE_MOVE:{
                bool member_learns = false;
                if (shared){
                    //  The member that stayed out learns, unless the text
                    //  named another Pokemon.
                    member_learns = memory.learner.empty();
                    for (const std::string& slug : settings.share_members){
                        if (memory.learner.find(slug) != std::string::npos){
                            member_learns = true;
                        }
                    }
                }
                if (member_learns){
                    //  Its moves and its types (the settings it came in with).
                    int active[4];
                    for (int i = 0; i < 4; i++){
                        active[i] = memory.moves[i];
                        memory.moves[i] = memory.member_moves[i];
                    }
                    handle_move_learning(console, context, base_settings, memory, text);
                    for (int i = 0; i < 4; i++){
                        memory.member_moves[i] = memory.moves[i];
                        memory.moves[i] = active[i];
                    }
                }else{
                    handle_move_learning(console, context, settings, memory, text);
                }
                memory.learner.clear();
                break;
            }
            case Question::STOP_LEARNING:
                answer(context, true);
                break;
            case Question::USE_NEXT:
                memory.lead_faints++;
                result = StoryBattleResult::LEAD_FAINTED;
                answer(context, true);
                break;
            case Question::SWITCH:
                if (team){
                    //  "FOE is about to use X. Will PLAYER change POKeMON?":
                    //  a free switch to the best answer.
                    const SpeciesInfo* next = species_in_text(text);
                    if (next != nullptr){
                        foe = next;
                        evaluated = next->slug;
                        int pos = best_position(next, false);
                        if (pos > 0 && score_at(pos, next) >= score_at(0, next) * 1.25 + 0.25){
                            console.log("Switching to team member " + std::to_string(order[(size_t)pos] + 1) + " against " + next->name + ".");
                            answer(context, true);
                            PartyMenuWatcher party(COLOR_RED);
                            if (wait_until(console, context, 3000ms, { party }) == 0){
                                pick_in_party_menu(pos);
                                set_active(pos);
                            }else{
                                console.log("The party menu did not open.", COLOR_ORANGE);
                            }
                            break;
                        }
                    }
                }
                answer(context, false);
                break;
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
            if (team){
                //  The active fainted: the best one left against the foe.
                down[(size_t)order[0]] = true;
                memory.team_fainted.insert(order[0]);
                int pos = best_position(foe, false);
                if (pos > 0){
                    console.log("Sending in team member " + std::to_string(order[(size_t)pos] + 1) + ".");
                    pick_in_party_menu(pos);
                    set_active(pos);
                    break;
                }
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
                    memory.level_ups++;
                    memory.member_moves_known = false;
                }
                if (text.find("trying to learn") != std::string::npos){
                    memory.learner = text;
                }
                if (balls_thrown > 0){
                    //  "Gotcha! X was caught!" ("It appeared to be caught!"
                    //  is one that broke free.)
                    if (text.find("gotcha") != std::string::npos || text.find("was caught") != std::string::npos){
                        if (!shiny_caught){
                            console.log(legendary || (dex_catch && !memory.shiny) ? "Caught it!" : "Caught the shiny Pokemon!", COLOR_BLUE);
                            //  Whatever the time: the POKeDEX page and the
                            //  nickname question still have to go by.
                            deadline = std::max(deadline, current_time() + 5min);
                        }
                        shiny_caught = true;
                        shiny_catching = false;
                    }
                    //  "X was transferred to BILL's PC." (the party is full)
                    if (text.find("transferred") != std::string::npos || text.find("pc was full") != std::string::npos){
                        memory.shiny_to_pc = true;
                    }
                    //  The ghost: "It dodged the thrown BALL! This POKeMON can't be caught!"
                    if (text.find("dodged") != std::string::npos){
                        shiny_catching = false;
                    }
                }
                //  "ANNOUNCER: You're out of SAFARI BALLS! Game over!": the
                //  battle ends, then the game takes the player out to the
                //  gate.
                if (text.find("game over") != std::string::npos ||
                    (text.find("out of") != std::string::npos && text.find("safari") != std::string::npos)
                ){
                    console.log("The Safari Game is over: no SAFARI BALLs left.", COLOR_BLUE);
                    memory.safari_over = true;
                }
            }
            //  A advances the text. (B would cancel an evolution.)
            pbf_press_button(context, BUTTON_A, 160ms, 340ms);
            break;
        case 4:
            //  The POKeDEX page of the caught species: B closes it.
            last_activity = current_time();
            if (memory.dex_registered && ++dex_page_in_row >= 3 && in_overworld(console, context)){
                //  Pokemon Tower 7F after a HAUNTER went to the PC: the
                //  floor looked like the POKeDEX page, and B went on for
                //  19 minutes until the 20-minute limit.
                if (lost){
                    return StoryBattleResult::LOST;
                }
                if (!seen_battle){
                    return StoryBattleResult::NO_BATTLE;
                }
                console.log("Battle over: the POKeDEX page was only the look of the place.");
                return over();
            }
            if (!memory.dex_registered){
                console.log("A new species in the POKeDEX.", COLOR_BLUE);
            }
            memory.dex_registered = true;
            pbf_press_button(context, BUTTON_B, 160ms, 840ms);
            context.wait_for_all_requests();
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
                memory.member_moves_known = false;
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
                bool list_open = wait_until(console, context, 500ms, { move_list }) == 0;
                if (list_open && !move_names_readable(console)){
                    console.log("Looked like the move list, but no move name is there.", COLOR_ORANGE);
                    list_open = false;
                }
                if (list_open){
                    //  move_names_readable() can match a name by chance in
                    //  what is no move list. Not one of the four read, again:
                    //  not the move list. Then nothing here counts as the
                    //  battle going on, and after 3 minutes the program
                    //  stops with a report (picture and video) instead of
                    //  picking moves blindly for 20 minutes.
                    StoryBattleMemory names;
                    read_fight_menu(console, names);
                    bool any = false;
                    for (int i = 0; i < 4; i++){
                        any |= names.moves[i] >= 0;
                    }
                    unreadable_lists_in_row = any ? 0 : unreadable_lists_in_row + 1;
                    if (unreadable_lists_in_row >= 2){
                        console.log(
                            "Looked like the move list " + std::to_string(unreadable_lists_in_row) +
                            " times in a row, but no move name was read: not the move list.",
                            COLOR_ORANGE
                        );
                        list_open = false;
                    }
                    if (unreadable_lists_in_row >= 12){
                        //  A couple of minutes of it, nothing else seen:
                        //  stuck. The report's picture and video show what
                        //  this screen is.
                        OperationFailedExceptionWithScreenshot::fire(
                            ErrorReportMode::SEND_ERROR_REPORT,
                            "run_story_battle(): A screen like the move list without move names, " +
                            std::to_string(unreadable_lists_in_row) + " times in a row.",
                            console
                        );
                    }
                }
                if (list_open){
                    //  The overworld can look like the move list (Cerulean
                    //  Cave 1F after running away: "BOUNCE" and "SNATCH"
                    //  read from the floor, and moves picked there for 20
                    //  minutes). Without the foe's box at the top left, or
                    //  twice in a row with nothing else between: look
                    //  whether the player is free to move.
                    list_only_in_row++;
                    //  The foe's box: where the ball mark goes, or its HP
                    //  bar. (The mark's place alone is no box once the foe
                    //  has a status: PAR, BRN... cover it. Route 7: every
                    //  paralyzed Meowth and Growlithe was "no box".)
                    const bool foe_box = screen && (read_caught_mark(screen) >= 0 || read_foe_hp(screen) >= 0);
                    const bool check_overworld = !foe_box || list_only_in_row >= 2;
                    if (check_overworld && in_overworld(console, context)){
                        if (lost){
                            return StoryBattleResult::LOST;
                        }
                        if (!seen_battle){
                            return StoryBattleResult::NO_BATTLE;
                        }
                        console.log("Battle over: the move list was only the look of the place.");
                        return over();
                    }
                    if (check_overworld){
                        //  in_overworld() pressed B: that closed the move
                        //  list, the battle menu is back. The next turn of
                        //  the loop sees it and picks the move from there.
                        //  (Picking here pressed on the battle menu, opened
                        //  the list again, and round it went: 20 minutes of
                        //  "Moves: [-] [-] [-] [-]", three times a night.)
                        console.log("Not the overworld: B closed the move list, back to the battle menu.", COLOR_ORANGE);
                        seen_battle = true;
                        battle_confirmed = true;
                        last_activity = current_time();
                        unknown_in_row = 0;
                        pressed_blind = false;
                        break;
                    }
                    seen_battle = true;
                    battle_confirmed = true;
                    last_activity = current_time();
                    unknown_in_row = 0;
                    if (shiny_catching){
                        //  Catching: back to the battle menu, where the
                        //  weakening or the next ball is picked. The best
                        //  move here made Moltres faint (FLAMETHROWER after
                        //  an A pressed on a screen not recognized).
                        console.log("The move list is open while catching: back to the battle menu.", COLOR_ORANGE);
                        pbf_press_button(context, BUTTON_B, 160ms, 840ms);
                        context.wait_for_all_requests();
                        pressed_blind = false;
                        break;
                    }
                    console.log("The move list is open. Picking a move.", COLOR_ORANGE);
                    if (pressed_blind){
                        memory.list_back = 0;
                    }else if (memory.last_slot >= 0 && ++memory.list_back >= 2){
                        //  The move picked last did not happen, twice: no PP
                        //  left. (Once is also a press the game missed.)
                        console.log("Move " + std::to_string(memory.last_slot + 1) + " seems to be out of PP.", COLOR_ORANGE);
                        memory.no_pp |= (uint8_t)(1 << memory.last_slot);
                        memory.last_slot = -1;
                        memory.list_back = 0;
                    }
                    std::string slug;
                    const SpeciesInfo* opponent = read_opponent(console, slug, settings.legendary_species);
                    use_best_move(console, context, settings, memory, opponent);
                    pressed_blind = false;
                    break;
                }
            }

            BattleDialogDetector battle_text(COLOR_RED);
            if (battle_text.detect(screen)){
                //  Only a text box: before the battle is certain, maybe no
                //  battle at all (a white area at the bottom of the
                //  overworld: A would talk to whoever stands there). Again
                //  and again: maybe the battle is over. Look.
                if (!battle_confirmed || ++text_only_in_row >= 3){
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
                        return over();
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
                if (settings.scene_after && battle_confirmed){
                    if (lost){
                        return StoryBattleResult::LOST;
                    }
                    console.log("Battle over: the scene after it has begun.");
                    return over();
                }
                //  Again and again: some places look like a text box all the
                //  time (Pokemon Tower 7F after a HAUNTER was caught: B for
                //  14 minutes). Look whether the player is free to move.
                white_last = true;
                white_in_row = white_before ? white_in_row + 1 : 1;
                if (white_in_row % 4 == 0 && in_overworld(console, context)){
                    if (lost){
                        return StoryBattleResult::LOST;
                    }
                    if (!seen_battle){
                        return StoryBattleResult::NO_BATTLE;
                    }
                    console.log("Battle over: only the look of a text box was left.");
                    return over();
                }
                //  The text goes on: that is something happening.
                last_activity = current_time();
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
                return over();
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
            pressed_blind = true;
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

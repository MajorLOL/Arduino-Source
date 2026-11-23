/*  Pokemon FRLG Autostory Boxes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Experimental (PocketPilot build 34: not run on a console when written).
 *  The PC's boxes, read: which Pokemon is in which cell, and its level. The
 *  first step toward evolving and trading what the story caught. Nothing in
 *  the boxes is changed by it.
 *
 *  The PC of a Pokemon Center (data/scripts/pc.inc, EventScript_PC), the
 *  player facing it:
 *
 *      A   "<PLAYER> booted up the PC."                    a button
 *          "Which PC should be accessed?", a list          A: its first entry, BILL's PC
 *          "Accessed BILL's PC."                           a button
 *          "POKeMON Storage System opened."                a button
 *          the storage menu (Task_PCMainMenu): WITHDRAW POKeMON, DEPOSIT
 *          POKeMON, MOVE POKeMON, MOVE ITEMS, SEE YA!; the text box says
 *          what the entry under the cursor is for
 *          DOWN, DOWN, A on MOVE POKeMON                   the box view
 *
 *  The box view (pokemon_storage_system): the hand starts on the first cell
 *  of the box the PC was on last. The D-pad moves it over the 5 rows of 6
 *  cells; UP from the top row puts it on the box's name, where LEFT and
 *  RIGHT change the box (HandleInput_BoxTitle: with every "Button mode" of
 *  the game's options; L and R change it only with "LR"), and DOWN brings
 *  it back to the third cell of the top row. The panel on the left shows the
 *  Pokemon under the hand: its name, its species, its level. B asks
 *  "Continue BOX operations?", and B again leaves.
 *
 *  To read the boxes, A is never pressed in the box view: nothing is picked
 *  up, moved or released. The box the PC is on is where the game starts to
 *  look for room for a caught Pokemon (SendMonToPC): after the last of the
 *  14 boxes the view is back on the box it started on (RIGHT goes round).
 *
 *  Since build 35 a Pokemon of a box can change places with the last one of
 *  the party (for an evolution stone): see "A Pokemon of a box into the
 *  party" at the end of this file.
 *
 *  This part has no dependencies on the rest of the program so it can be
 *  tested on its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBoxes_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBoxes_H

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStoryBoxes{


const int BOX_COUNT = 14;       //  TOTAL_BOXES_COUNT
const int BOX_ROWS = 5;
const int BOX_COLUMNS = 6;
const int BOX_CELLS = BOX_ROWS * BOX_COLUMNS;


//  ---- The PC's texts -------------------------------------------------------

//  What the text box says on the way into the boxes and out again (the text
//  as the program reads it, lower case).
enum class PcText{
    NONE,           //  No text (a fade, the box view, nothing came of the A).
    BOOTED,         //  "<PLAYER> booted up the PC."
    WHICH_PC,       //  "Which PC should be accessed?" (the list: BILL's PC first)
    ACCESSED,       //  "Accessed BILL's PC." ("Accessed Someone's PC.")
    OPENED,         //  "POKeMON Storage System opened."
    WITHDRAW,       //  The storage menu, the cursor on WITHDRAW POKeMON.
    DEPOSIT,        //  ... on DEPOSIT POKeMON.
    MOVE_MONS,      //  ... on MOVE POKeMON.
    MOVE_ITEMS,     //  ... on MOVE ITEMS.
    SEE_YA,         //  ... on SEE YA!
    CANNOT,         //  "Can't take any more POKeMON." / "Can't deposit the last POKeMON!"
    BOX_QUESTION,   //  In the box view: "Continue BOX operations?" / "Exit from the BOX?"
    OTHER,          //  Some other text: not the PC.
};

inline int letters_of(const std::string& text){
    int letters = 0;
    for (char ch : text){
        letters += ch >= 'a' && ch <= 'z' ? 1 : 0;
    }
    return letters;
}

inline PcText read_pc_text(const std::string& text){
    auto has = [&](const char* word){
        return text.find(word) != std::string::npos;
    };
    if (has("booted")){
        return PcText::BOOTED;
    }
    if (has("which pc") || has("be accessed") || has("should be")){
        return PcText::WHICH_PC;
    }
    if (has("accessed")){
        return PcText::ACCESSED;
    }
    if (has("storage system") || has("system opened")){
        return PcText::OPENED;
    }
    //  (Before the menu's own lines: "Can't deposit the last POKeMON!")
    if (has("can't take") || has("cant take") || has("any more") || has("the last")){
        return PcText::CANNOT;
    }
    if (has("withdraw")){
        return PcText::WITHDRAW;
    }
    if (has("deposit")){
        return PcText::DEPOSIT;
    }
    if (has("stored in") || has("that are stored")){
        return PcText::MOVE_MONS;
    }
    if (has("items held") || has("move items") || has("held by")){
        return PcText::MOVE_ITEMS;
    }
    if (has("see you")){
        return PcText::SEE_YA;
    }
    if (has("box operations") || has("continue box") || has("exit from")){
        return PcText::BOX_QUESTION;
    }
    return letters_of(text) < 4 ? PcText::NONE : PcText::OTHER;
}

//  What to press on it, on the way to the box view by MOVE POKeMON.
enum class PcPress{
    WAIT,       //  Nothing: look again.
    A,
    B,
    UP,
    DOWN,
    FAIL,       //  Not on the way in: give up (the caller closes what is open).
};

inline PcPress pc_press_in(PcText text){
    switch (text){
    case PcText::NONE:          return PcPress::WAIT;
    case PcText::BOOTED:
    case PcText::ACCESSED:
    case PcText::OPENED:        return PcPress::A;
    case PcText::WHICH_PC:      return PcPress::A;      //  BILL's PC, the first entry.
    case PcText::WITHDRAW:
    case PcText::DEPOSIT:       return PcPress::DOWN;
    case PcText::MOVE_MONS:     return PcPress::A;      //  The box view.
    case PcText::MOVE_ITEMS:
    case PcText::SEE_YA:        return PcPress::UP;
    case PcText::CANNOT:        return PcPress::B;
    case PcText::BOX_QUESTION:
    case PcText::OTHER:         return PcPress::FAIL;
    }
    return PcPress::FAIL;
}

//  A on this text is the one that opens the box view.
inline bool pc_enters_boxes(PcText text){
    return text == PcText::MOVE_MONS;
}


//  ---- The cells of a box ---------------------------------------------------

struct Cell{
    int row = 0;
    int column = 0;
};
inline bool operator==(const Cell& a, const Cell& b){
    return a.row == b.row && a.column == b.column;
}

//  The game's own count of the cells: row by row.
inline int cell_number(const Cell& cell){
    return cell.row * BOX_COLUMNS + cell.column;
}
inline bool cell_in_box(const Cell& cell){
    return cell.row >= 0 && cell.row < BOX_ROWS && cell.column >= 0 && cell.column < BOX_COLUMNS;
}

//  The order the cells of a box are read in: from where DOWN from the box's
//  name puts the hand (the third cell of the top row) through all thirty,
//  each one step from the one before, to the last cell of the top row, from
//  where UP is the box's name again.
inline const std::vector<Cell>& scan_order(){
    static const std::vector<Cell> order{
        {0, 2}, {0, 1}, {0, 0},
        {1, 0}, {2, 0}, {3, 0}, {4, 0},
        {4, 1}, {3, 1}, {2, 1}, {1, 1},
        {1, 2}, {2, 2}, {3, 2}, {4, 2},
        {4, 3}, {3, 3}, {2, 3}, {1, 3}, {0, 3},
        {0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4},
        {4, 5}, {3, 5}, {2, 5}, {1, 5}, {0, 5},
    };
    return order;
}

//  The D-pad presses from one cell to another ('U', 'D', 'L', 'R'): its
//  column first, then its row, never past an edge of the box (LEFT and
//  RIGHT go round there, UP and DOWN leave the cells).
inline std::string cell_moves(const Cell& from, const Cell& to){
    std::string moves;
    if (!cell_in_box(from) || !cell_in_box(to)){
        return moves;
    }
    moves.append((size_t)std::abs(to.column - from.column), to.column > from.column ? 'R' : 'L');
    moves.append((size_t)std::abs(to.row - from.row), to.row > from.row ? 'D' : 'U');
    return moves;
}


//  ---- What was read --------------------------------------------------------

struct BoxPokemon{
    int box = 0;        //  0: the box the PC was on, 1: the one after it (RIGHT)...
    int cell = 0;       //  0 to 29, row by row.
    std::string slug;   //  Its species ("" when the name was not read: an EGG, too).
    int level = -1;     //  -1 when not read.
};

struct BoxCensus{
    std::vector<BoxPokemon> pokemon;
    int boxes = 0;          //  Boxes gone through.
    //  After the last box the view was on the first one again: the game
    //  looks for room for a caught Pokemon from the same box as before, and
    //  no box was passed over or read twice.
    bool closed = false;

    int count(const std::string& slug) const{
        int found = 0;
        for (const BoxPokemon& one : pokemon){
            found += one.slug == slug ? 1 : 0;
        }
        return found;
    }
    //  Cells with a Pokemon whose name was not read.
    int unread() const{
        return count("");
    }
    int in_box(int box) const{
        int found = 0;
        for (const BoxPokemon& one : pokemon){
            found += one.box == box ? 1 : 0;
        }
        return found;
    }
    //  Boxes with a Pokemon in them.
    int boxes_used() const{
        int used = 0;
        for (int box = 0; box < boxes; box++){
            used += in_box(box) > 0 ? 1 : 0;
        }
        return used;
    }
    //  Those of a box, cell by cell (row by row from the top left).
    std::vector<BoxPokemon> by_cell(int box) const{
        std::vector<BoxPokemon> list;
        for (const BoxPokemon& one : pokemon){
            if (one.box == box){
                list.push_back(one);
            }
        }
        std::stable_sort(list.begin(), list.end(), [](const BoxPokemon& a, const BoxPokemon& b){
            return a.cell < b.cell;
        });
        return list;
    }
    //  What lies in a cell (nullptr: nothing).
    const BoxPokemon* at(int box, int cell) const{
        for (const BoxPokemon& one : pokemon){
            if (one.box == box && one.cell == cell){
                return &one;
            }
        }
        return nullptr;
    }
    //  Another Pokemon in a cell (or the first one in an empty cell).
    void put(int box, int cell, const std::string& slug, int level){
        for (BoxPokemon& one : pokemon){
            if (one.box == box && one.cell == cell){
                one.slug = slug;
                one.level = level;
                return;
            }
        }
        BoxPokemon one;
        one.box = box;
        one.cell = cell;
        one.slug = slug;
        one.level = level;
        pokemon.push_back(one);
    }
    //  Those of a species, the highest level first (then as they lie).
    std::vector<BoxPokemon> of(const std::string& slug) const{
        std::vector<BoxPokemon> list;
        for (const BoxPokemon& one : pokemon){
            if (one.slug == slug){
                list.push_back(one);
            }
        }
        std::stable_sort(list.begin(), list.end(), [](const BoxPokemon& a, const BoxPokemon& b){
            return a.level > b.level;
        });
        return list;
    }
};

//  As a text file: a line of what was read, then a line for each Pokemon,
//  "<box> <cell> <species> <level>" ("?" for a name not read).
inline std::string census_to_text(const BoxCensus& census){
    std::string text =
        "# The PC's boxes as the FRLG AutoStory read them. Each line: the box (0: the box the PC was on, "
        "1: the next one to the right...), the cell (0 to 29, row by row), the species, the level.\n";
    text += "boxes " + std::to_string(census.boxes) + " closed " + (census.closed ? "1" : "0") +
        " pokemon " + std::to_string(census.pokemon.size()) + "\n";
    for (const BoxPokemon& one : census.pokemon){
        text += std::to_string(one.box) + " " + std::to_string(one.cell) + " " +
            (one.slug.empty() ? std::string("?") : one.slug) + " " + std::to_string(one.level) + "\n";
    }
    return text;
}

//  False (and an empty census) for a text that is not such a file, or
//  whose lines do not add up to what its first line says.
inline bool census_from_text(const std::string& text, BoxCensus& census){
    census = BoxCensus();
    std::vector<std::vector<std::string>> lines;
    {
        std::vector<std::string> words;
        std::string word;
        bool comment = false;
        for (char ch : text + "\n"){
            if (ch == '\n'){
                if (!word.empty()){
                    words.push_back(word);
                }
                if (!words.empty() && !comment){
                    lines.push_back(words);
                }
                words.clear();
                word.clear();
                comment = false;
            }else if (comment){
            }else if (ch == '#' && words.empty() && word.empty()){
                comment = true;
            }else if (ch == ' ' || ch == '\t' || ch == '\r'){
                if (!word.empty()){
                    words.push_back(word);
                }
                word.clear();
            }else{
                word += ch;
            }
        }
    }
    auto number = [](const std::string& word, int& value){
        if (word.empty() || word.size() > 6){
            return false;
        }
        size_t at = word[0] == '-' ? 1 : 0;
        if (at >= word.size()){
            return false;
        }
        for (size_t k = at; k < word.size(); k++){
            if (word[k] < '0' || word[k] > '9'){
                return false;
            }
        }
        value = std::atoi(word.c_str());
        return true;
    };
    int boxes = 0;
    int closed = 0;
    int total = 0;
    if (lines.empty() || lines[0].size() != 6 || lines[0][0] != "boxes" || lines[0][2] != "closed" ||
        lines[0][4] != "pokemon" || !number(lines[0][1], boxes) || !number(lines[0][3], closed) ||
        !number(lines[0][5], total) || boxes < 0 || boxes > BOX_COUNT || total < 0 ||
        (size_t)total != lines.size() - 1
    ){
        return false;
    }
    BoxCensus read;
    read.boxes = boxes;
    read.closed = closed == 1;
    for (size_t k = 1; k < lines.size(); k++){
        const std::vector<std::string>& words = lines[k];
        BoxPokemon one;
        if (words.size() != 4 || !number(words[0], one.box) || !number(words[1], one.cell) ||
            !number(words[3], one.level) || one.box < 0 || one.box >= boxes ||
            one.cell < 0 || one.cell >= BOX_CELLS || one.level < -1 || one.level > 100
        ){
            return false;
        }
        one.slug = words[2] == "?" ? std::string() : words[2];
        read.pokemon.push_back(one);
    }
    census = read;
    return true;
}


//  ---- Evolving, from the POKeDEX table --------------------------------------

//  How a species comes by evolving, as the POKeDEX table says it
//  (PokemonFRLG_AutoStoryDexCollection.h, "how"):
//      "evolve from Metapod: level 10"
//      "evolve from Nidorina: Moon Stone"
//      "evolve from Gloom: Sun Stone (the only one: Ruin Valley, Six Island) (National Dex)"
//      "evolve from Golbat: level up with high friendship (220+) (National Dex)"
//      "evolve from Tyrogue: level 20 with Attack = Defense (National Dex)"
enum class EvolveBy{
    LEVEL,
    STONE,
    FRIENDSHIP,
    OTHER,      //  A level and something more (TYROGUE's stats).
};

struct EvolveHow{
    std::string from;       //  The name of the one that evolves, as the table has it.
    EvolveBy by = EvolveBy::OTHER;
    int level = 0;          //  LEVEL, OTHER: the level.
    std::string stone;      //  STONE: "Moon Stone".
};

inline bool read_evolve_how(const std::string& how, EvolveHow& out){
    out = EvolveHow();
    const std::string prefix = "evolve from ";
    if (how.compare(0, prefix.size(), prefix) != 0){
        return false;
    }
    const size_t colon = how.find(": ", prefix.size());
    if (colon == std::string::npos || colon == prefix.size()){
        return false;
    }
    out.from = how.substr(prefix.size(), colon - prefix.size());
    const std::string method = how.substr(colon + 2);
    const std::string friendship = "level up with high friendship";
    const std::string level = "level ";
    if (method.compare(0, friendship.size(), friendship) == 0){
        out.by = EvolveBy::FRIENDSHIP;
        return true;
    }
    if (method.compare(0, level.size(), level) == 0){
        size_t at = level.size();
        int value = 0;
        while (at < method.size() && method[at] >= '0' && method[at] <= '9' && value < 1000){
            value = value * 10 + (method[at] - '0');
            at++;
        }
        if (value < 1 || value > 100){
            return false;
        }
        out.level = value;
        const std::string with = " with ";
        out.by = method.compare(at, with.size(), with) == 0 ? EvolveBy::OTHER : EvolveBy::LEVEL;
        return true;
    }
    const std::string stone = " Stone";
    const size_t at = method.find(stone);
    if (at == std::string::npos || at == 0){
        return false;
    }
    out.by = EvolveBy::STONE;
    out.stone = method.substr(0, at + stone.size());
    return true;
}

//  A row of the POKeDEX table, as far as evolving goes.
struct TableRow{
    std::string slug;
    std::string name;
    int needed = 0;     //  How many to catch (0: it only comes by evolving).
    int have = 0;       //  How many the table has.
    std::string how;
};

//  One species that only comes by evolving and that the table does not have
//  yet: how, and which Pokemon of the boxes it would be.
struct EvolveLine{
    std::string slug;
    std::string name;
    EvolveHow how;
    std::string from_slug;  //  "" when the table has no row of that name.
    int in_boxes = 0;       //  How many of the one that evolves the boxes have.
    //  The one of them for this line (the species of one box Pokemon can go
    //  to one line only: three EEVEEs for VAPOREON, JOLTEON and FLAREON),
    //  the highest level first. box < 0: none (left).
    int box = -1;
    int cell = -1;
    int level = -1;
};

//  TYROGUE (build 51, experimental): it evolves at level 20 into HITMONLEE
//  (Attack above Defense), HITMONCHAN (Attack below Defense) or HITMONTOP (the
//  same), whatever the nature, the DVs and the experience of its training make
//  of them: not known beforehand, and no memory is read. The table has
//  HITMONLEE and HITMONCHAN as species to get (one copy each; the Dojo gives one
//  of them) and HITMONTOP as one that only comes by evolving.
constexpr int TYROGUE_EVOLVES_AT = 20;

inline bool tyrogue_line(const std::string& slug){
    return slug == "tyrogue" || slug == "hitmonlee" || slug == "hitmonchan" || slug == "hitmontop";
}

//  What a Pokemon in training can be called while it is trained: what it was,
//  what the plan said it becomes, and for TYROGUE all three of its evolutions
//  (which one comes is its Attack against its Defense at level 20).
inline std::set<std::string> training_line(const std::string& from, const std::string& becomes){
    std::set<std::string> line{from, becomes};
    if (tyrogue_line(from)){
        line.insert("tyrogue");
        line.insert("hitmonlee");
        line.insert("hitmonchan");
        line.insert("hitmontop");
    }
    return line;
}

//  HITMONTOP by vitamins (build 57, experimental, not run on a console when
//  written).
//  TYROGUE becomes HITMONTOP at level 20 when its Attack and its Defense are
//  the same then (EVO_LEVEL_ATK_EQ_DEF; the evolution looks at the stats of
//  the moment of the level, with the EVs it has by then). Left to itself
//  that is one try in nine. A PROTEIN adds 10 EVs to the Attack, an IRON 10
//  to the Defense (no more than 100 of them in all), and the stats are made
//  again at once, so the program reads both at level 19, works out which
//  vitamins make the two the same at level 20, uses them, reads again (what
//  the first ones did tells the EVs it did not know of) and goes on to level
//  20. The stats of a Pokemon of level L are
//      floor((floor((2 * 35 + IV + floor(EV / 4)) * L / 100) + 5) * nature / 100)
//  with the IV (0 to 31) and the EVs unknown: the reading of the Attack and
//  the Defense gives the pairs (IV, EV) that fit each, the EVs of the fights
//  before it taken as 0 to 24 (some fifteen battles at Victory Road).
//  A simulation (the same model, 600 TYROGUE, EVs gained in the last fights
//  to level 20 put in): one reading round 62%, two rounds 67%, with 3.5
//  vitamins each; left alone 11%. The same for a HITMONCHAN (the Defense
//  above the Attack): IRONs only.
constexpr int VITAMIN_EV = 10;
constexpr int VITAMIN_EV_CAP = 100;
constexpr int VITAMIN_MAX_EACH = 8;     //  A PROTEIN or IRON on one Pokemon, in all.
constexpr int BATTLE_EV_MAX = 24;       //  The EVs of a stat it may have from fights, at level 19.
constexpr int TYROGUE_BASE_STAT = 35;   //  Its base Attack and Defense.

//  The percentages the nature gives the Attack and the Defense (110, 100,
//  90). False: not a nature of the game (the reading was not one).
inline bool nature_percent(const std::string& nature, int& attack, int& defense){
    std::string name;
    for (char ch : nature){
        if (ch >= 'A' && ch <= 'Z'){
            name += (char)(ch - 'A' + 'a');
        }else if (ch >= 'a' && ch <= 'z'){
            name += ch;
        }
    }
    //  Attack up: Lonely, Adamant, Naughty, Brave. Attack down: Bold, Modest,
    //  Calm, Timid. Defense up: Bold, Impish, Lax, Relaxed. Defense down:
    //  Lonely, Mild, Gentle, Hasty. The other five (Hardy, Docile, Serious,
    //  Bashful, Quirky) change nothing; the rest of the 25 change other stats.
    static const char* const NEUTRAL[] = {
        "hardy", "docile", "serious", "bashful", "quirky",
        "rash", "quiet", "sassy", "careful", "jolly", "naive"
    };
    attack = 100;
    defense = 100;
    for (const char* known : NEUTRAL){
        if (name == known){
            return true;
        }
    }
    if (name == "lonely"){ attack = 110; defense = 90; return true; }
    if (name == "adamant" || name == "naughty" || name == "brave"){ attack = 110; return true; }
    if (name == "bold"){ attack = 90; defense = 110; return true; }
    if (name == "modest" || name == "calm" || name == "timid"){ attack = 90; return true; }
    if (name == "impish" || name == "lax" || name == "relaxed"){ defense = 110; return true; }
    if (name == "mild" || name == "gentle" || name == "hasty"){ defense = 90; return true; }
    return false;
}

//  The Attack or the Defense of a TYROGUE: "x" is IV + floor(EV / 4).
inline int tyrogue_stat(int x, int level, int percent){
    return ((2 * TYROGUE_BASE_STAT + x) * level / 100 + 5) * percent / 100;
}

//  x of a stat of IV "iv" and "ev" EVs from fights, "vitamins" vitamins on top.
inline int vitamin_x(int iv, int ev, int vitamins){
    return iv + std::min(ev + VITAMIN_EV * vitamins, VITAMIN_EV_CAP) / 4;
}

enum class VitaminGoal{
    ATTACK_EQUALS_DEFENSE,      //  HITMONTOP
    DEFENSE_ABOVE_ATTACK,       //  HITMONCHAN (IRONs only)
};

struct VitaminAim{
    int protein = 0;            //  More of them to use now.
    int iron = 0;
    double chance = -1;         //  That it ends as the goal says (-1: the reading fits no IV and EVs).
    size_t attack_fits = 0;     //  How many (IV, EV) pairs fit each reading.
    size_t defense_fits = 0;
};

//  What to use now, given the stats read at "level_read" with "protein_used"
//  and "iron_used" vitamins used already: the most likely to end as the goal
//  at "level_evolve", the fewest vitamins of those. (0, 0) when the reading
//  fits nothing the model knows (the program does nothing then).
inline VitaminAim vitamin_aim(
    VitaminGoal goal, int level_read, int level_evolve,
    int attack, int defense, int attack_percent, int defense_percent,
    int protein_used, int iron_used
){
    VitaminAim aim;
    struct Pair{
        int iv;
        int ev;
    };
    std::vector<Pair> attacks;
    std::vector<Pair> defenses;
    for (int iv = 0; iv <= 31; iv++){
        for (int ev = 0; ev <= BATTLE_EV_MAX; ev++){
            if (tyrogue_stat(vitamin_x(iv, ev, protein_used), level_read, attack_percent) == attack){
                attacks.push_back({iv, ev});
            }
            if (tyrogue_stat(vitamin_x(iv, ev, iron_used), level_read, defense_percent) == defense){
                defenses.push_back({iv, ev});
            }
        }
    }
    aim.attack_fits = attacks.size();
    aim.defense_fits = defenses.size();
    if (attacks.empty() || defenses.empty()){
        return aim;
    }
    const int more_protein = goal == VitaminGoal::ATTACK_EQUALS_DEFENSE
        ? std::max(0, VITAMIN_MAX_EACH - protein_used) : 0;
    const int more_iron = std::max(0, VITAMIN_MAX_EACH - iron_used);
    constexpr int STATS = 700;
    //  The stat each pair has at level_evolve with 0 to "more" vitamins more.
    auto histograms = [&](const std::vector<Pair>& pairs, int used, int more, int percent){
        std::vector<std::vector<size_t>> hist((size_t)more + 1, std::vector<size_t>(STATS, 0));
        for (int extra = 0; extra <= more; extra++){
            for (const Pair& pair : pairs){
                const int stat = tyrogue_stat(vitamin_x(pair.iv, pair.ev, used + extra), level_evolve, percent);
                if (stat >= 0 && stat < STATS){
                    hist[(size_t)extra][(size_t)stat]++;
                }
            }
        }
        return hist;
    };
    const std::vector<std::vector<size_t>> by_attack = histograms(attacks, protein_used, more_protein, attack_percent);
    const std::vector<std::vector<size_t>> by_defense = histograms(defenses, iron_used, more_iron, defense_percent);
    const double pairs = (double)attacks.size() * (double)defenses.size();
    double best = -1;
    int best_vitamins = 1 << 20;
    for (int p = 0; p <= more_protein; p++){
        for (int i = 0; i <= more_iron; i++){
            double hits = 0;
            for (int a = 0; a < STATS; a++){
                const size_t na = by_attack[(size_t)p][(size_t)a];
                if (na == 0){
                    continue;
                }
                if (goal == VitaminGoal::ATTACK_EQUALS_DEFENSE){
                    hits += (double)na * (double)by_defense[(size_t)i][(size_t)a];
                }else{
                    for (int d = a + 1; d < STATS; d++){
                        hits += (double)na * (double)by_defense[(size_t)i][(size_t)d];
                    }
                }
            }
            const double chance = hits / pairs;
            const int vitamins = p + i;
            if (chance > best + 1e-9 || (chance > best - 1e-9 && vitamins < best_vitamins)){
                best = chance;
                best_vitamins = vitamins;
                aim.protein = p;
                aim.iron = i;
                aim.chance = chance;
            }
        }
    }
    return aim;
}

inline std::vector<EvolveLine> evolve_plan(const std::vector<TableRow>& table, const BoxCensus& census){
    std::map<std::string, std::string> slug_of;
    for (const TableRow& row : table){
        slug_of[row.name] = row.slug;
    }
    std::map<std::string, size_t> taken;
    std::vector<EvolveLine> plan;
    for (const TableRow& row : table){
        EvolveLine line;
        bool tyrogue_wanted = false;
        if ((row.slug == "hitmonlee" || row.slug == "hitmonchan") && row.needed > row.have){
            //  A species to get that a TYROGUE of the boxes can be (after the
            //  Day Care): the one of the two that the Dojo did not give.
            if (slug_of.find("Tyrogue") == slug_of.end()){
                continue;
            }
            line.how.from = "Tyrogue";
            line.how.by = EvolveBy::OTHER;
            line.how.level = TYROGUE_EVOLVES_AT;
            tyrogue_wanted = true;
        }else if (row.needed != 0 || row.have > 0 || !read_evolve_how(row.how, line.how)){
            continue;
        }
        line.slug = row.slug;
        line.name = row.name;
        auto iter = slug_of.find(line.how.from);
        if (iter != slug_of.end()){
            line.from_slug = iter->second;
            const std::vector<BoxPokemon> ones = census.of(line.from_slug);
            line.in_boxes = (int)ones.size();
            size_t& next = taken[line.from_slug];
            if (next < ones.size()){
                line.box = ones[next].box;
                line.cell = ones[next].cell;
                line.level = ones[next].level;
                next++;
            }else if (tyrogue_wanted){
                //  No TYROGUE left in the boxes: not a species that only
                //  comes by evolving, so nothing to say of it.
                continue;
            }
        }
        plan.push_back(line);
    }
    return plan;
}

//  The plan as lines for the log.
inline std::vector<std::string> evolve_summary(const std::vector<EvolveLine>& plan){
    auto upper = [](const std::string& slug){
        std::string name;
        for (char ch : slug){
            name += ch >= 'a' && ch <= 'z' ? (char)(ch - 'a' + 'A') : ch;
        }
        return name;
    };
    auto where = [&](const EvolveLine& line){
        return upper(line.from_slug) + (line.level > 0 ? " Lv " + std::to_string(line.level) : std::string()) +
            ", box " + std::to_string(line.box + 1) + " cell " + std::to_string(line.cell + 1);
    };
    std::map<std::string, std::string> by_stone;
    std::map<std::string, int> stones;
    std::string by_level;
    int levels = 0;
    std::string by_else;
    int elses = 0;
    std::string missing;
    int missed = 0;
    for (const EvolveLine& line : plan){
        if (line.box < 0){
            missing += (missing.empty() ? "" : ", ") + line.name + " (" + line.how.from +
                (line.in_boxes > 0 ? ": the " + std::to_string(line.in_boxes) + " in the boxes are for others" : std::string()) +
                ")";
            missed++;
            continue;
        }
        switch (line.how.by){
        case EvolveBy::STONE:{
            std::string& list = by_stone[line.how.stone];
            list += (list.empty() ? "" : ", ") + line.name + " (" + where(line) + ")";
            stones[line.how.stone]++;
            break;
        }
        case EvolveBy::LEVEL:
            by_level += (by_level.empty() ? "" : ", ") + line.name + " (" + where(line) +
                (line.level < 0 ? ": to level " + std::to_string(line.how.level) :
                    line.level >= line.how.level ? std::string(": at its next level") :
                    ": " + std::to_string(line.how.level - line.level) + " more to level " +
                        std::to_string(line.how.level)) + ")";
            levels++;
            break;
        case EvolveBy::FRIENDSHIP:
            by_else += (by_else.empty() ? "" : ", ") + line.name + " (" + where(line) + ": friendship)";
            elses++;
            break;
        case EvolveBy::OTHER:
            by_else += (by_else.empty() ? "" : ", ") + line.name + " (" + where(line) + ": level " +
                std::to_string(line.how.level) + " and its stats)";
            elses++;
            break;
        }
    }
    std::vector<std::string> lines;
    for (const auto& item : by_stone){
        lines.push_back(
            "With a " + item.first + " (" + std::to_string(stones[item.first]) + "), the one to use it on is in a box: " +
            item.second + "."
        );
    }
    if (levels > 0){
        lines.push_back("By level (" + std::to_string(levels) + "), the one to raise is in a box: " + by_level + ".");
    }
    if (elses > 0){
        lines.push_back("By friendship or its stats (" + std::to_string(elses) + "), in a box: " + by_else + ".");
    }
    if (missed > 0){
        lines.push_back(
            "The one that evolves is not in a box (" + std::to_string(missed) + "; in the party, not caught yet, or "
            "itself still to evolve): " + missing + "."
        );
    }
    return lines;
}



//  ---- A Pokemon of a box into the party and back, a stone on it (build 35) --
//
//  MOVE POKeMON, with a party of six whose last one is not of the team:
//      the hand onto the cell                      the panel: its species
//      A (its menu, MOVE first), A                 the hand holds it
//      to the bottom row's left half, DOWN         the hand on PARTY POKeMON
//      A                                           the party slides in, the hand on
//                                                  its last one (SetCursorInParty:
//                                                  a full party while one is held)
//      A (the menu, SHIFT first), A                the two change places: the hand
//                                                  holds the party's one
//      B                                           the party slides away, the hand
//                                                  on the first cell; the panel: the
//                                                  one it holds (SetShiftedMonData)
//      the hand onto the cell, now empty
//      A (the menu, PLACE first), A                the party's one lies there
//  Only A and B besides the D-pad, and the D-pad never while a menu is up:
//  RELEASE is the fifth entry of those menus, and asks with NO under its
//  cursor. Then, outside the PC, the stone on party slot 6, a look at what
//  slot 6 is now, and the same change again: the evolved one into the cell,
//  the party's own one back in its place. The game is saved after that and
//  never in between: an error resets it to before the first press.

//  The evolution stones: as the POKeDEX table names them, as the game does,
//  words of their description in the bag (the last of its three lines; the
//  first two are the same for all six), and their place in the list of the
//  Celadon Department Store's fourth floor (-1: not sold anywhere).
struct StoneKind{
    const char* name;
    const char* item;
    const char* words;
    int mart;
};
inline const std::vector<StoneKind>& stone_kinds(){
    static const std::vector<StoneKind> kinds{
        {"Fire Stone", "FIRE STONE", "colored orange|orange", 2},
        {"Thunder Stone", "THUNDER STONE", "thunderbolt", 3},
        {"Water Stone", "WATER STONE", "light blue|clear light", 4},
        {"Leaf Stone", "LEAF STONE", "leaf pattern|a leaf", 5},
        {"Moon Stone", "MOON STONE", "night sky|black as", -1},
        {"Sun Stone", "SUN STONE", "as the sun|red as", -1},
    };
    return kinds;
}
inline const StoneKind* stone_kind(const std::string& name){
    for (const StoneKind& kind : stone_kinds()){
        if (name == kind.name){
            return &kind;
        }
    }
    return nullptr;
}

//  One evolution with a stone that can be made: the Pokemon is in a box.
struct StoneTarget{
    std::string slug;       //  What it becomes.
    std::string name;
    std::string from_slug;  //  What it is.
    std::string stone;      //  "Moon Stone".
    int box = -1;
    int cell = -1;
    int level = -1;
};

//  Those of a plan (evolve_plan()), in the order of the table, without the
//  stones in "without" (not in the bag and not sold, or bought and still
//  not there).
inline std::vector<StoneTarget> stone_targets(
    const std::vector<EvolveLine>& plan, const std::set<std::string>& without = std::set<std::string>()
){
    std::vector<StoneTarget> targets;
    for (const EvolveLine& line : plan){
        if (line.how.by != EvolveBy::STONE || line.box < 0 || line.cell < 0 || line.from_slug.empty()){
            continue;
        }
        if (stone_kind(line.how.stone) == nullptr || without.count(line.how.stone) != 0){
            continue;
        }
        StoneTarget target;
        target.slug = line.slug;
        target.name = line.name;
        target.from_slug = line.from_slug;
        target.stone = line.how.stone;
        target.box = line.box;
        target.cell = line.cell;
        target.level = line.level;
        targets.push_back(target);
    }
    return targets;
}

//  Of those, the ones that take this stone (all of them without one).
inline std::vector<StoneTarget> stone_targets_only(const std::vector<StoneTarget>& targets, const std::string& stone){
    if (stone.empty()){
        return targets;
    }
    std::vector<StoneTarget> only;
    for (const StoneTarget& target : targets){
        if (target.stone == stone){
            only.push_back(target);
        }
    }
    return only;
}

//  How many of each stone they take, in the order of stone_kinds().
inline std::vector<std::pair<std::string, int>> stones_needed(const std::vector<StoneTarget>& targets){
    std::vector<std::pair<std::string, int>> needed;
    for (const StoneKind& kind : stone_kinds()){
        int count = 0;
        for (const StoneTarget& target : targets){
            count += target.stone == kind.name ? 1 : 0;
        }
        if (count > 0){
            needed.emplace_back(kind.name, count);
        }
    }
    return needed;
}

//  They as one line for the log: "Arcanine (GROWLITHE, box 2 cell 10, FIRE STONE), ...".
inline std::string stone_targets_text(const std::vector<StoneTarget>& targets){
    std::string text;
    for (const StoneTarget& target : targets){
        const StoneKind* kind = stone_kind(target.stone);
        std::string from;
        for (char ch : target.from_slug){
            from += ch >= 'a' && ch <= 'z' ? (char)(ch - 'a' + 'A') : ch;
        }
        text += (text.empty() ? "" : ", ") + target.name + " (" + from + ", box " + std::to_string(target.box + 1) +
            " cell " + std::to_string(target.cell + 1) + ", " + (kind != nullptr ? kind->item : target.stone.c_str()) + ")";
    }
    return text;
}

//  What to buy: of each stone that is sold, what the targets take less the
//  ones the bag is known to have (a look in the bag tells that there is one,
//  not how many: one is counted).
inline std::vector<std::pair<std::string, int>> stones_to_buy(
    const std::vector<StoneTarget>& targets, const std::set<std::string>& in_bag
){
    std::vector<std::pair<std::string, int>> buy;
    for (const std::pair<std::string, int>& need : stones_needed(targets)){
        const StoneKind* kind = stone_kind(need.first);
        if (kind == nullptr || kind->mart < 0){
            continue;
        }
        const int count = need.second - (in_bag.count(need.first) != 0 ? 1 : 0);
        if (count > 0){
            buy.emplace_back(need.first, count);
        }
    }
    return buy;
}


//  ---- Evolving by level (build 43, experimental) ----------------------------
//
//  The species the table does not have and that come by a level of the one
//  before it ("evolve from Pidgeotto: level 36"), where that one lies in a
//  box: it comes into the party, trains at Victory Road until the level,
//  evolves there, and goes back into its cell (the stones do the same with
//  a stone). One of them in each segment, the one that has the fewest
//  levels to go first.
struct LevelTarget{
    std::string slug;       //  What it becomes.
    std::string name;
    std::string from_slug;  //  What it is.
    int box = -1;
    int cell = -1;
    int level = -1;         //  Its level as the list has it (-1: not known).
    int evolves_at = 0;     //  The level the table names.
    int goal = 0;           //  The level to train it to.
    //  Evolving by friendship (build 45): walked with first, then trained
    //  the one level (goal = its level + 1) that the evolution waits for.
    bool friendship = false;
    int blocks = 0;         //  The blocks of 128 steps to walk (in all) before the level.
    int walked = 0;         //  The blocks walked so far.
    int tests = 0;          //  The levels trained after walking that did not evolve it.
    //  The Elite Four for experience (build 46): the experience points to go,
    //  as exp_to_go() counts them.
    int exp = 0;
};

//  A Pokemon that is at the level of the evolution already (or above: it
//  was caught so, or a level was skipped) evolves at its next level.
//  0: not to train (level 100 cannot go up).
inline int level_goal(int level, int evolves_at){
    if (evolves_at < 2 || evolves_at > 100 || level >= 100){
        return 0;
    }
    return level >= evolves_at ? level + 1 : evolves_at;
}

//  The levels still to go. A level that is not known counts as the first.
inline int levels_to_go(const LevelTarget& target){
    return target.goal - (target.level > 0 ? target.level : 1);
}

//  Those of a plan (evolve_plan()) that come by a level and whose earlier
//  form lies in a box, the one with the fewest levels to go first, without
//  the species in "without" (left out after failures).
inline std::vector<LevelTarget> level_targets(
    const std::vector<EvolveLine>& plan, const std::set<std::string>& without = std::set<std::string>()
){
    std::vector<LevelTarget> targets;
    for (const EvolveLine& line : plan){
        //  (OTHER: a level and its stats, TYROGUE's; the level is trained to,
        //  what it becomes is the game's.)
        const bool by_level = line.how.by == EvolveBy::LEVEL ||
            (line.how.by == EvolveBy::OTHER && line.how.level > 0);
        if (!by_level || line.box < 0 || line.cell < 0 || line.from_slug.empty()){
            continue;
        }
        if (without.count(line.slug) != 0){
            continue;
        }
        LevelTarget target;
        target.slug = line.slug;
        target.name = line.name;
        target.from_slug = line.from_slug;
        target.box = line.box;
        target.cell = line.cell;
        target.level = line.level;
        target.evolves_at = line.how.level;
        target.goal = level_goal(line.level, line.how.level);
        if (target.goal <= 0){
            continue;
        }
        targets.push_back(target);
    }
    std::stable_sort(targets.begin(), targets.end(), [](const LevelTarget& a, const LevelTarget& b){
        return levels_to_go(a) < levels_to_go(b);
    });
    return targets;
}

//  They as one line for the log: "Pidgeot (PIDGEOTTO Lv 30, box 2 cell 10, to level 36), ...".
inline std::string level_targets_text(const std::vector<LevelTarget>& targets){
    std::string text;
    for (const LevelTarget& target : targets){
        std::string from;
        for (char ch : target.from_slug){
            from += ch >= 'a' && ch <= 'z' ? (char)(ch - 'a' + 'A') : ch;
        }
        text += (text.empty() ? "" : ", ") + target.name + " (" + from +
            (target.level > 0 ? " Lv " + std::to_string(target.level) : std::string()) + ", box " +
            std::to_string(target.box + 1) + " cell " + std::to_string(target.cell + 1) + ", to level " +
            std::to_string(target.goal) + ")";
    }
    return text;
}

//  ---- Evolving by friendship (build 45) --------------------------------------
//
//  The species the table wants that come by a level up with a friendship of
//  220 or more: CROBAT (GOLBAT), BLISSEY (CHANSEY), TOGETIC (TOGEPI). The game
//  does not show the friendship, and no memory is read, so the program
//  counts. The friendship goes up by one for every Pokemon of the party that
//  is not an egg, with a chance of one in two, every 128 steps (the game's
//  UpdateFriendshipStepCounter and AdjustFriendship, FRIENDSHIP_EVENT_WALKING).
//  What it starts from is what the species has when it is caught (GOLBAT
//  70, CHANSEY 140), or hatched (TOGEPI 120). More than that is possible (it
//  was in the party for a while): then the walking is more than needed, and
//  the level that is trained after it is the test.
constexpr int FRIEND_EVOLVES = 220;
constexpr int FRIEND_BLOCK_STEPS = 128;
//  The tests: how often a level was trained after walking and the Pokemon
//  did not evolve. After this many the Pokemon is left alone.
constexpr int FRIEND_TESTS = 6;

inline int friend_base(const std::string& slug){
    if (slug == "chansey"){
        return 140;
    }
    if (slug == "togepi"){
        return 120;
    }
    return 70;
}

inline int friend_need(const std::string& slug){
    return std::max(0, FRIEND_EVOLVES - friend_base(slug));
}

//  How many blocks of 128 steps make at least "need" points, with the chance
//  that "sigmas" standard deviations stand for (1.28: 90 in 100). Each block
//  gives a point with a chance of one in two: n blocks give n / 2 points, with
//  a deviation of sqrt(n) / 2, so the least n with n / 2 - sigmas * sqrt(n) / 2
//  >= need.
inline int friend_blocks(int need, double sigmas){
    if (need <= 0){
        return 0;
    }
    const double root = (sigmas + std::sqrt(sigmas * sigmas + 8.0 * (double)need)) / 2.0;
    //  (Less a hair: 300.00000000000006 blocks are 300.)
    return (int)std::ceil(root * root - 1e-6);
}

//  The blocks for the next level that is trained, after "tests" that did not
//  evolve it: a wider margin each time.
inline int friend_blocks_wanted(const std::string& slug, int tests){
    static const double SIGMAS[FRIEND_TESTS] = {1.28, 2.05, 3.0, 4.0, 4.5, 5.0};
    const int index = std::max(0, std::min(tests, FRIEND_TESTS - 1));
    return friend_blocks(friend_need(slug), SIGMAS[index]);
}

//  What was walked with a species of the boxes, and how often it was tried:
//  as a file of its own, written at each save made on the walk (the game's
//  save and the file agree: a game reset to its last save goes back to it).
//      friend <species> <blocks walked> <levels trained without it evolving>
struct FriendRecord{
    int blocks = 0;
    int tests = 0;
};
using FriendRecords = std::map<std::string, FriendRecord>;

inline std::string friend_to_text(const FriendRecords& records){
    std::string text;
    for (const auto& item : records){
        if (item.second.blocks <= 0 && item.second.tests <= 0){
            continue;
        }
        text += "friend " + item.first + " " + std::to_string(item.second.blocks) + " " +
            std::to_string(item.second.tests) + "\n";
    }
    return text;
}

inline bool friend_from_text(const std::string& text, FriendRecords& out){
    out.clear();
    FriendRecords records;
    size_t at = 0;
    while (at < text.size()){
        size_t end = text.find('\n', at);
        if (end == std::string::npos){
            end = text.size();
        }
        std::vector<std::string> words;
        std::string word;
        for (size_t k = at; k < end; k++){
            const char ch = text[k];
            if (ch == ' ' || ch == '\t' || ch == '\r'){
                if (!word.empty()){
                    words.push_back(word);
                    word.clear();
                }
                continue;
            }
            word += ch;
        }
        if (!word.empty()){
            words.push_back(word);
        }
        at = end + 1;
        if (words.empty() || words[0][0] == '#'){
            continue;
        }
        if (words.size() != 4 || words[0] != "friend"){
            return false;
        }
        auto number = [](const std::string& s, int& value){
            if (s.empty() || s.size() > 6){
                return false;
            }
            for (char ch : s){
                if (ch < '0' || ch > '9'){
                    return false;
                }
            }
            value = std::atoi(s.c_str());
            return true;
        };
        FriendRecord record;
        if (!number(words[2], record.blocks) || !number(words[3], record.tests)){
            return false;
        }
        records[words[1]] = record;
    }
    out = records;
    return true;
}

//  Those of a plan (evolve_plan()) that come by friendship and whose earlier
//  form lies in a box, the one with the fewest blocks still to walk first,
//  without the species in "without" (left out after failures). The one to
//  train is at its level plus one.
inline std::vector<LevelTarget> friend_targets(
    const std::vector<EvolveLine>& plan, const FriendRecords& records,
    const std::set<std::string>& without = std::set<std::string>()
){
    std::vector<LevelTarget> targets;
    for (const EvolveLine& line : plan){
        if (line.how.by != EvolveBy::FRIENDSHIP || line.box < 0 || line.cell < 0 || line.from_slug.empty()){
            continue;
        }
        if (without.count(line.slug) != 0){
            continue;
        }
        LevelTarget target;
        target.slug = line.slug;
        target.name = line.name;
        target.from_slug = line.from_slug;
        target.box = line.box;
        target.cell = line.cell;
        target.level = line.level;
        target.friendship = true;
        auto found = records.find(line.from_slug);
        if (found != records.end()){
            target.walked = found->second.blocks;
            target.tests = found->second.tests;
        }
        if (target.tests >= FRIEND_TESTS){
            continue;
        }
        target.goal = line.level >= 100 ? 0 : (line.level > 0 ? line.level + 1 : 2);
        if (target.goal <= 0){
            continue;
        }
        target.blocks = friend_blocks_wanted(line.from_slug, target.tests);
        targets.push_back(target);
    }
    std::stable_sort(targets.begin(), targets.end(), [](const LevelTarget& a, const LevelTarget& b){
        return std::max(0, a.blocks - a.walked) < std::max(0, b.blocks - b.walked);
    });
    return targets;
}

//  They as one line for the log.
inline std::string friend_targets_text(const std::vector<LevelTarget>& targets){
    std::string text;
    for (const LevelTarget& target : targets){
        std::string from;
        for (char ch : target.from_slug){
            from += ch >= 'a' && ch <= 'z' ? (char)(ch - 'a' + 'A') : ch;
        }
        text += (text.empty() ? "" : ", ") + target.name + " (" + from +
            (target.level > 0 ? " Lv " + std::to_string(target.level) : std::string()) + ", box " +
            std::to_string(target.box + 1) + " cell " + std::to_string(target.cell + 1) + ", " +
            std::to_string(target.walked) + " of " + std::to_string(target.blocks) + " blocks of " +
            std::to_string(FRIEND_BLOCK_STEPS) + " steps walked" +
            (target.tests > 0 ? ", " + std::to_string(target.tests) + " levels trained without it" : std::string()) +
            ")";
    }
    return text;
}


//  ---- The Elite Four for experience (build 46, experimental) -----------------
//
//  A Pokemon that needs a lot of experience (PUPITAR from level 43 to 55 is
//  108585 points) is trained at Victory Road at some 20000 points an hour.
//  The League again gives much more: its 26 Pokemon (level 63 to 75, of the
//  four Elite Four members and the Champion) give 48034 points before the game's factor,
//  and a Pokemon of the party's sixth place that holds the EXP. SHARE and is
//  never sent out gets half of each battle (and the factor of 1.5 for
//  trainers' battles): some 36000 points for a round of the five rooms, in 30
//  to 35 minutes, three times as fast per hour. A round is worth it from some
//  20000 points to go (the walk to the Indigo Plateau and back, Oak's rating
//  and the nurse take their time as well); for less, Victory Road is as
//  quick. The growth group of a species is not in the program's data: the
//  points are counted on the curve of the medium fast group (level to the
//  third), which is within a quarter of the others' (the slow group needs a
//  quarter more: a Pupitar from 43 to 55 is 108585, counted 86868).
const int EXP_ROUND_MIN = 20000;

//  The experience points from level "level" to level "goal" (0: not known or
//  not to go up).
inline int exp_to_go(int level, int goal){
    if (level < 1 || goal <= level || goal > 100){
        return 0;
    }
    return goal * goal * goal - level * level * level;
}

//  Those of the level targets that have EXP_ROUND_MIN points to go or more
//  (the level in the list must be known), the one with the most first.
inline std::vector<LevelTarget> exp_targets(
    const std::vector<EvolveLine>& plan, const std::set<std::string>& without = std::set<std::string>()
){
    std::vector<LevelTarget> targets;
    for (LevelTarget& target : level_targets(plan, without)){
        target.exp = exp_to_go(target.level, target.goal);
        if (target.level > 0 && target.exp >= EXP_ROUND_MIN){
            targets.push_back(target);
        }
    }
    std::stable_sort(targets.begin(), targets.end(), [](const LevelTarget& a, const LevelTarget& b){
        return a.exp > b.exp;
    });
    return targets;
}

//  Build 56: the targets of a segment that trains at Victory Road: those of
//  level_targets() without the ones that have EXP_ROUND_MIN points to go or
//  more, which are left to the rounds of the Elite Four (those come after these
//  segments too). "exp_without" are the species that failed in the rounds twice
//  in a row in this run: they are not left to them, so that they are still
//  trained, slowly, at Victory Road.
inline std::vector<LevelTarget> level_targets_light(
    const std::vector<EvolveLine>& plan, const std::set<std::string>& without = std::set<std::string>(),
    const std::set<std::string>& exp_without = std::set<std::string>()
){
    std::set<std::string> left = without;
    for (const LevelTarget& heavy : exp_targets(plan, exp_without)){
        left.insert(heavy.slug);
    }
    return level_targets(plan, left);
}

//  They as one line for the log.
inline std::string exp_targets_text(const std::vector<LevelTarget>& targets){
    std::string text;
    for (const LevelTarget& target : targets){
        std::string from;
        for (char ch : target.from_slug){
            from += ch >= 'a' && ch <= 'z' ? (char)(ch - 'a' + 'A') : ch;
        }
        text += (text.empty() ? "" : ", ") + target.name + " (" + from + " Lv " + std::to_string(target.level) +
            ", box " + std::to_string(target.box + 1) + " cell " + std::to_string(target.cell + 1) + ", to level " +
            std::to_string(target.goal) + ", about " + std::to_string(target.exp) + " points to go)";
    }
    return text;
}


//  What a training under way needs to go on after the game was reset to a
//  save made in the middle of it (the program is stopped, or the game
//  reset): the Pokemon of the box is in the party, the one it changed
//  places with waits in its cell. A file of one line:
//      raise 1 <box> <cell> <waiting species> <species out> <becomes> <level to train to> [friend]
//      raise 0
//  ("friend": the training is the one of evolving by friendship, after the walk.)
//  (Species as slugs, which have no spaces; the cell as the program counts it.)
struct RaiseFile{
    bool out = false;
    int box = -1;
    int cell = -1;
    std::string extra;      //  What waits in the cell.
    std::string from;       //  What came out of it.
    std::string becomes;    //  What it evolves into.
    int goal = 0;
    bool friendship = false;
};

inline std::string raise_to_text(const RaiseFile& file){
    if (!file.out){
        return "raise 0\n";
    }
    return "raise 1 " + std::to_string(file.box) + " " + std::to_string(file.cell) + " " + file.extra + " " +
        file.from + " " + file.becomes + " " + std::to_string(file.goal) + (file.friendship ? " friend" : "") + "\n";
}

inline bool raise_from_text(const std::string& text, RaiseFile& out){
    out = RaiseFile();
    std::vector<std::string> words;
    std::string word;
    for (char ch : text){
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'){
            if (!word.empty()){
                words.push_back(word);
                word.clear();
            }
            continue;
        }
        word += ch;
    }
    if (!word.empty()){
        words.push_back(word);
    }
    if (words.size() < 2 || words[0] != "raise"){
        return false;
    }
    if (words[1] == "0"){
        return words.size() == 2;
    }
    if (words[1] != "1" || (words.size() != 8 && words.size() != 9) || (words.size() == 9 && words[8] != "friend")){
        return false;
    }
    auto number = [](const std::string& s, int& value){
        if (s.empty() || s.size() > 4){
            return false;
        }
        for (char ch : s){
            if (ch < '0' || ch > '9'){
                return false;
            }
        }
        value = std::atoi(s.c_str());
        return true;
    };
    RaiseFile file;
    if (!number(words[2], file.box) || !number(words[3], file.cell) || !number(words[7], file.goal)){
        return false;
    }
    file.out = true;
    file.extra = words[4];
    file.from = words[5];
    file.becomes = words[6];
    file.friendship = words.size() == 9;
    if (file.box < 0 || file.cell < 0 || file.goal < 2 || file.goal > 100){
        return false;
    }
    out = file;
    return true;
}


}
}
}
}
#endif

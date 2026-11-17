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
 *  A is never pressed in the box view: nothing is picked up, moved or
 *  released. The box the PC is on is where the game starts to look for room
 *  for a caught Pokemon (SendMonToPC): after the last of the 14 boxes the
 *  view is back on the box it started on (RIGHT goes round).
 *
 *  This part has no dependencies on the rest of the program so it can be
 *  tested on its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBoxes_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBoxes_H

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
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

inline std::vector<EvolveLine> evolve_plan(const std::vector<TableRow>& table, const BoxCensus& census){
    std::map<std::string, std::string> slug_of;
    for (const TableRow& row : table){
        slug_of[row.name] = row.slug;
    }
    std::map<std::string, size_t> taken;
    std::vector<EvolveLine> plan;
    for (const TableRow& row : table){
        EvolveLine line;
        if (row.needed != 0 || row.have > 0 || !read_evolve_how(row.how, line.how)){
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


}
}
}
}
#endif

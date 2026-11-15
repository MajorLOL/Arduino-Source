/*  Pokemon FRLG Autostory Collection Text
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The counts of the POKeDEX collection table as a text: what the program
 *  keeps in a file of its own in the settings folder, written at each save
 *  of the game.
 *
 *  The settings themselves are written only when the program's window
 *  closes. A new build in a new folder, with a copy of the settings folder
 *  made while the old program was still open, started with the table of
 *  days before: on 15 November 13 species where the game had 72, and the
 *  collection looked again for what it had caught (Route 16: 35 battles
 *  for a RATICATE the game had since the Pokemon Mansion).
 *
 *      Charmander
 *      doduo 2 0
 *      rattata 1 0
 *
 *  The first line: whose counts they are (the starter of the playthrough).
 *  Then a line for each species with at least one: its slug, how many were
 *  caught, how many of them shiny.
 *
 *  This part has no dependencies on the rest of the program so it can be
 *  tested on its own.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryCollectionText_H
#define PokemonAutomation_PokemonFRLG_AutoStoryCollectionText_H

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  How many of a species the program caught, and how many of them shiny.
struct StoryCollectionCount{
    int caught = 0;
    int shiny = 0;
};


inline std::string story_collection_to_text(
    const std::string& owner, const std::map<std::string, StoryCollectionCount>& counts
){
    std::string text = owner + "\n";
    for (const auto& item : counts){
        if (item.first.empty() || (item.second.caught <= 0 && item.second.shiny <= 0)){
            continue;
        }
        text += item.first + " " + std::to_string(item.second.caught) + " " + std::to_string(item.second.shiny) + "\n";
    }
    return text;
}

//  False: no such text, or the counts of another owner. Lines that are not
//  "slug caught shiny" are passed over. (The text as the file has it: with
//  or without the byte order mark the program's files start with, with
//  Windows line ends or not.)
inline bool story_collection_from_text(
    const std::string& text, const std::string& owner,
    std::map<std::string, StoryCollectionCount>& counts
){
    counts.clear();
    std::vector<std::string> lines;
    {
        const size_t start = text.rfind("\xef\xbb\xbf", 0) == 0 ? 3 : 0;
        std::string line;
        for (char c : text.substr(start) + "\n"){
            if (c == '\r'){
                continue;
            }
            if (c != '\n'){
                line += c;
                continue;
            }
            while (!line.empty() && (line.back() == ' ' || line.back() == '\t')){
                line.pop_back();
            }
            if (!line.empty()){
                lines.push_back(line);
            }
            line.clear();
        }
    }
    if (lines.empty() || owner.empty() || lines[0] != owner){
        return false;
    }
    for (size_t i = 1; i < lines.size(); i++){
        const std::string& line = lines[i];
        const size_t first = line.find(' ');
        if (first == std::string::npos || first == 0){
            continue;
        }
        const std::string slug = line.substr(0, first);
        bool slug_fine = true;
        for (char c : slug){
            slug_fine = slug_fine && ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-');
        }
        if (!slug_fine){
            continue;
        }
        char* end = nullptr;
        const long caught = std::strtol(line.c_str() + first + 1, &end, 10);
        if (end == line.c_str() + first + 1){
            continue;
        }
        const long shiny = std::strtol(end, nullptr, 10);
        StoryCollectionCount& count = counts[slug];
        count.caught = (int)std::max(0L, std::min(99L, caught));
        count.shiny = (int)std::max(0L, std::min(99L, shiny));
    }
    return true;
}

//  The larger count of each species: "from" into "into". Returns for how
//  many species "from" had more.
inline size_t story_collection_merge(
    std::map<std::string, StoryCollectionCount>& into,
    const std::map<std::string, StoryCollectionCount>& from
){
    size_t more = 0;
    for (const auto& item : from){
        StoryCollectionCount& count = into[item.first];
        if (item.second.caught > count.caught || item.second.shiny > count.shiny){
            more++;
        }
        count.caught = std::max(count.caught, item.second.caught);
        count.shiny = std::max(count.shiny, item.second.shiny);
    }
    return more;
}


}
}
}
#endif

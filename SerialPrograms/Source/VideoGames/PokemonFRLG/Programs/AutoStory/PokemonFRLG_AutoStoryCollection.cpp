/*  Pokemon FRLG Autostory Collection
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <map>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "PokemonFRLG_AutoStoryDexCollection.h"
#include "PokemonFRLG_AutoStoryCollection.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using AutoStoryDex::CollectionEntry;


namespace{

const CollectionEntry* collection_entry(const std::string& slug){
    static const std::map<std::string, const CollectionEntry*> database = [](){
        std::map<std::string, const CollectionEntry*> ret;
        size_t count = 0;
        const CollectionEntry* entries = AutoStoryDex::collection(count);
        for (size_t i = 0; i < count; i++){
            ret[entries[i].slug] = &entries[i];
        }
        return ret;
    }();
    auto iter = database.find(slug);
    return iter == database.end() ? nullptr : iter->second;
}

bool available(const CollectionEntry& entry, int game, int starter){
    return ((entry.available >> (game * 3 + starter)) & 1) != 0;
}

std::string dex_number(uint16_t dex){
    std::string ret = std::to_string(dex);
    while (ret.size() < 3){
        ret = "0" + ret;
    }
    return "#" + ret;
}

}



StoryCollectionRow::StoryCollectionRow(EditableTableOption& parent_table)
    : EditableTableRow(parent_table)
    , number(LockMode::LOCK_WHILE_RUNNING, "")
    , pokemon(LockMode::LOCK_WHILE_RUNNING, "")
    , caught(LockMode::LOCK_WHILE_RUNNING, 0, 0, 99)
    , shiny(LockMode::LOCK_WHILE_RUNNING, 0, 0, 99)
    , needed(LockMode::LOCK_WHILE_RUNNING, "")
    , when(LockMode::LOCK_WHILE_RUNNING, "")
    , how(LockMode::LOCK_WHILE_RUNNING, "")
    , evolve(LockMode::LOCK_WHILE_RUNNING, "")
{
    PA_ADD_OPTION(number);
    PA_ADD_OPTION(pokemon);
    PA_ADD_OPTION(caught);
    PA_ADD_OPTION(shiny);
    PA_ADD_OPTION(needed);
    PA_ADD_OPTION(when);
    PA_ADD_OPTION(how);
    PA_ADD_OPTION(evolve);
}
std::unique_ptr<EditableTableRow> StoryCollectionRow::clone() const{
    std::unique_ptr<StoryCollectionRow> ret(new StoryCollectionRow(parent()));
    ret->m_slug = m_slug;
    ret->number.set_text(number.text());
    ret->pokemon.set_text(pokemon.text());
    ret->caught.set(caught.current_value());
    ret->shiny.set(shiny.current_value());
    ret->needed.set_text(needed.text());
    ret->when.set_text(when.text());
    ret->how.set_text(how.text());
    ret->evolve.set_text(evolve.text());
    return ret;
}
void StoryCollectionRow::load_json(const JsonValue& json){
    const JsonObject* obj = json.to_object();
    if (obj == nullptr){
        return;
    }
    const std::string* slug = obj->get_string("slug");
    if (slug == nullptr){
        return;
    }
    int64_t count = obj->get_integer_default("caught", 0);
    caught.set((uint8_t)std::max<int64_t>(0, std::min<int64_t>(99, count)));
    count = obj->get_integer_default("shiny", 0);
    shiny.set((uint8_t)std::max<int64_t>(0, std::min<int64_t>(99, count)));
    const StoryCollectionTable& table = static_cast<const StoryCollectionTable&>(parent());
    set_species(*slug, table.game());
}
JsonValue StoryCollectionRow::to_json() const{
    JsonObject obj;
    obj["slug"] = m_slug;
    obj["caught"] = (int64_t)caught.current_value();
    obj["shiny"] = (int64_t)shiny.current_value();
    return obj;
}
void StoryCollectionRow::set_species(const std::string& slug, int game){
    m_slug = slug;
    const CollectionEntry* entry = collection_entry(slug);
    if (entry == nullptr){
        pokemon.set_text(slug);
        return;
    }
    number.set_text(dex_number(entry->dex));
    pokemon.set_text(entry->name);
    const int copies = entry->copies[game];
    needed.set_text(copies > 0 ? std::to_string(copies) : std::string("evolve"));
    when.set_text(entry->post_game ? "after the Hall of Fame" : "story");
    how.set_text(entry->how[game]);
    evolve.set_text(entry->evolve[game]);
}



StoryCollectionTable::StoryCollectionTable(StaticTextOption& summary)
    : EditableTableOption_t<StoryCollectionRow>(
        "<b>Pokedex collection:</b><br>"
        "Every species this game can register with this starter without trading with another game "
        "(the Switch version gives the MYSTIC and AURORA TICKETs after the Hall of Fame), in POKeDEX order. "
        "<b>Caught</b>: how many the program caught (or knows of: the game's ball mark, the team); set it "
        "by hand where it is wrong. <b>Shiny</b>: how many of them were shiny (what counts with \"Shiny only\"). "
        "<b>Needed</b>: how many to get: 2 or 3 where the evolutions cannot be "
        "caught (keep one, evolve the others); \"evolve\": only by evolving. "
        "After Mewtwo (chapter 20) the program catches what is still needed in the grass, the caves and "
        "the Safari Zone of Kanto and the Sevii Islands, and skips the places where nothing is needed. "
        "Surfing, fishing, Rock Smash, gifts, trades, breeding and the evolving itself: not yet. "
        "A new game (\"Start a new game\" or the start at C0_01) sets all counts back to 0.",
        LockMode::LOCK_WHILE_RUNNING,
        false,  //  No load/save buttons: the rows follow the game and starter.
        0       //  Folded: double click to see it.
    )
    , m_summary(summary)
    , m_game(0)
    , m_starter(0)
    , m_shiny_only(false)
{
    rebuild({});
}
std::vector<std::string> StoryCollectionTable::make_header() const{
    return {"#", "Pokemon", "Caught", "Shiny", "Needed", "When", "Where / how", "Evolve the others into"};
}
void StoryCollectionTable::load_json(const JsonValue& json){
    std::map<std::string, StoryCollectionCount> saved;
    const JsonArray* array = json.to_array();
    if (array != nullptr){
        for (const JsonValue& item : *array){
            const JsonObject* obj = item.to_object();
            if (obj == nullptr){
                continue;
            }
            const std::string* slug = obj->get_string("slug");
            if (slug != nullptr){
                StoryCollectionCount& count = saved[*slug];
                count.caught = (int)obj->get_integer_default("caught", 0);
                count.shiny = (int)obj->get_integer_default("shiny", 0);
            }
        }
    }
    rebuild(saved);
}

void StoryCollectionTable::set_game(int game, int starter){
    game = game == 1 ? 1 : 0;
    starter = std::max(0, std::min(2, starter));
    if (m_game.load() == game && m_starter.load() == starter && current_rows() > 0){
        return;
    }
    std::map<std::string, StoryCollectionCount> kept = counts();
    m_game.store(game);
    m_starter.store(starter);
    rebuild(kept);
}
std::string StoryCollectionTable::game_name() const{
    static const char* STARTERS[] = {"Bulbasaur", "Squirtle", "Charmander"};
    return std::string(game() == 1 ? "LeafGreen" : "FireRed") + ", " + STARTERS[starter()] +
        (shiny_only() ? ", shiny only" : "");
}
void StoryCollectionTable::set_shiny_only(bool shiny_only){
    if (m_shiny_only.exchange(shiny_only) != shiny_only){
        update_summary();
    }
}

std::map<std::string, StoryCollectionCount> StoryCollectionTable::counts() const{
    std::map<std::string, StoryCollectionCount> ret;
    for (const std::shared_ptr<EditableTableRow>& item : current_refs()){
        const StoryCollectionRow* row = dynamic_cast<const StoryCollectionRow*>(item.get());
        if (row != nullptr && !row->slug().empty()){
            StoryCollectionCount& count = ret[row->slug()];
            count.caught = row->caught.current_value();
            count.shiny = row->shiny.current_value();
        }
    }
    return ret;
}
void StoryCollectionTable::rebuild(const std::map<std::string, StoryCollectionCount>& counts){
    const int g = game();
    const int s = starter();
    JsonArray array;
    //  "Restore defaults": the rows of this game and starter, all at 0.
    std::vector<std::unique_ptr<EditableTableRow>> defaults;
    size_t count = 0;
    const CollectionEntry* entries = AutoStoryDex::collection(count);
    for (size_t i = 0; i < count; i++){
        if (!available(entries[i], g, s)){
            continue;
        }
        std::unique_ptr<StoryCollectionRow> row(new StoryCollectionRow(*this));
        row->set_species(entries[i].slug, g);
        defaults.emplace_back(std::move(row));
        JsonObject obj;
        obj["slug"] = std::string(entries[i].slug);
        auto iter = counts.find(entries[i].slug);
        obj["caught"] = (int64_t)(iter == counts.end() ? 0 : iter->second.caught);
        obj["shiny"] = (int64_t)(iter == counts.end() ? 0 : iter->second.shiny);
        array.push_back(std::move(obj));
    }
    set_default(std::move(defaults));
    EditableTableOption::load_json(JsonValue(std::move(array)));
    update_summary();
}
std::shared_ptr<StoryCollectionRow> StoryCollectionTable::find(const std::string& slug) const{
    for (const std::shared_ptr<EditableTableRow>& item : current_refs()){
        std::shared_ptr<StoryCollectionRow> row = std::dynamic_pointer_cast<StoryCollectionRow>(item);
        if (row && row->slug() == slug){
            return row;
        }
    }
    return nullptr;
}

int StoryCollectionTable::needed(const std::string& slug) const{
    const CollectionEntry* entry = collection_entry(slug);
    if (entry == nullptr || !available(*entry, game(), starter())){
        return -1;
    }
    return entry->copies[game()];
}
int StoryCollectionTable::caught(const std::string& slug) const{
    std::shared_ptr<StoryCollectionRow> row = find(slug);
    return row ? row->caught.current_value() : 0;
}
int StoryCollectionTable::shinies(const std::string& slug) const{
    std::shared_ptr<StoryCollectionRow> row = find(slug);
    return row ? row->shiny.current_value() : 0;
}
int StoryCollectionTable::have(const std::string& slug) const{
    return shiny_only() ? shinies(slug) : caught(slug);
}
bool StoryCollectionTable::wants(const std::string& slug) const{
    const int n = needed(slug);
    return n > 0 && have(slug) < n;
}
std::set<std::string> StoryCollectionTable::wanted(const std::set<std::string>& species) const{
    std::set<std::string> ret;
    for (const std::string& slug : species){
        if (wants(slug)){
            ret.insert(slug);
        }
    }
    return ret;
}
bool StoryCollectionTable::wants_more_than_one(const std::string& slug) const{
    const int n = needed(slug);
    return n > 1 && have(slug) < n;
}

void StoryCollectionTable::add_caught(const std::string& slug, bool registered, bool shiny){
    {
        std::lock_guard<std::mutex> lg(m_lock);
        std::shared_ptr<StoryCollectionRow> row = find(slug);
        if (!row){
            return;
        }
        int count = row->caught.current_value();
        if (registered){
            count = std::max(count, 1);
        }
        row->caught.set((uint8_t)std::min(99, count + 1));
        if (shiny){
            row->shiny.set((uint8_t)std::min(99, row->shiny.current_value() + 1));
        }
    }
    update_summary();
}
void StoryCollectionTable::at_least(const std::string& slug, int count){
    {
        std::lock_guard<std::mutex> lg(m_lock);
        std::shared_ptr<StoryCollectionRow> row = find(slug);
        if (!row || row->caught.current_value() >= count){
            return;
        }
        row->caught.set((uint8_t)std::min(99, count));
    }
    update_summary();
}
void StoryCollectionTable::restore(const std::map<std::string, StoryCollectionCount>& saved){
    {
        std::lock_guard<std::mutex> lg(m_lock);
        for (const std::shared_ptr<EditableTableRow>& item : current_refs()){
            StoryCollectionRow* row = dynamic_cast<StoryCollectionRow*>(item.get());
            if (row == nullptr){
                continue;
            }
            auto iter = saved.find(row->slug());
            row->caught.set((uint8_t)(iter == saved.end() ? 0 : std::max(0, std::min(99, iter->second.caught))));
            row->shiny.set((uint8_t)(iter == saved.end() ? 0 : std::max(0, std::min(99, iter->second.shiny))));
        }
    }
    update_summary();
}
void StoryCollectionTable::clear_counts(){
    {
        std::lock_guard<std::mutex> lg(m_lock);
        for (const std::shared_ptr<EditableTableRow>& item : current_refs()){
            StoryCollectionRow* row = dynamic_cast<StoryCollectionRow*>(item.get());
            if (row != nullptr){
                row->caught.set(0);
                row->shiny.set(0);
            }
        }
    }
    update_summary();
}

void StoryCollectionTable::counts_summary(size_t& registered, size_t& total, size_t& to_catch) const{
    registered = 0;
    total = 0;
    to_catch = 0;
    for (const std::shared_ptr<EditableTableRow>& item : current_refs()){
        const StoryCollectionRow* row = dynamic_cast<const StoryCollectionRow*>(item.get());
        if (row == nullptr){
            continue;
        }
        total++;
        const int have = shiny_only() ? row->shiny.current_value() : row->caught.current_value();
        if (have > 0){
            registered++;
        }
        const int n = needed(row->slug());
        if (n > have){
            to_catch += (size_t)(n - have);
        }
    }
}
void StoryCollectionTable::update_summary(){
    size_t registered, total, to_catch;
    counts_summary(registered, total, to_catch);
    const std::string one = shiny_only() ? "at least one shiny" : "at least one";
    m_summary.set_text(
        "<b>Pokedex collection (" + game_name() + "):</b> " +
        std::to_string(total) + " species can be registered; the program counts " +
        std::to_string(registered) + " with " + one + ", " + std::to_string(total - registered) +
        " without; " + std::to_string(to_catch) + (shiny_only() ? " shiny" : "") +
        " catches still to make (extra ones to evolve included)."
    );
}



}
}
}

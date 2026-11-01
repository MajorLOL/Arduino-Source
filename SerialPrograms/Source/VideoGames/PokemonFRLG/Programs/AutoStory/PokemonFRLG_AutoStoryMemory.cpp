/*  Pokemon FRLG Autostory Memory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdio>
#include <map>
#include <mutex>
#include <utility>
#include <vector>
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/MemoryUtilization/MemoryUtilization.h"
#include "PokemonFRLG_AutoStoryMemory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct LedgerEntry{
    int64_t bytes = 0;
    uint64_t count = 0;
};

struct Ledger{
    Mutex lock;
    int64_t start = 0;
    std::map<std::string, LedgerEntry> entries;
    //  The scopes now open, the innermost last, with the memory when each
    //  last started counting.
    std::vector<std::pair<const char*, int64_t>> open;
};

Ledger& ledger(){
    static Ledger instance;
    return instance;
}

int64_t memory_now(){
    return (int64_t)process_memory_usage().process_virtual_memory;
}

std::string megabytes(int64_t bytes){
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%+.0f MB", (double)bytes / (1024.0 * 1024.0));
    return buffer;
}

}


StoryMemoryScope::StoryMemoryScope(const char* activity)
    : m_activity(activity)
{
    Ledger& ledger_ = ledger();
    const int64_t now = memory_now();
    std::lock_guard<Mutex> lg(ledger_.lock);
    if (!ledger_.open.empty()){
        //  The outer scope stops counting while this one is open.
        ledger_.entries[ledger_.open.back().first].bytes += now - ledger_.open.back().second;
    }
    ledger_.open.emplace_back(activity, now);
    ledger_.entries[activity].count++;
}

StoryMemoryScope::~StoryMemoryScope(){
    Ledger& ledger_ = ledger();
    const int64_t now = memory_now();
    std::lock_guard<Mutex> lg(ledger_.lock);
    //  Scopes close in the reverse order they opened (they live on the
    //  program thread's stack).
    for (size_t i = ledger_.open.size(); i > 0; i--){
        if (ledger_.open[i - 1].first != m_activity){
            continue;
        }
        if (i == ledger_.open.size()){
            ledger_.entries[m_activity].bytes += now - ledger_.open[i - 1].second;
        }
        ledger_.open.erase(ledger_.open.begin() + (std::ptrdiff_t)(i - 1));
        break;
    }
    if (!ledger_.open.empty()){
        ledger_.open.back().second = now;
    }
}


void story_memory_reset(){
    Ledger& ledger_ = ledger();
    const int64_t now = memory_now();
    std::lock_guard<Mutex> lg(ledger_.lock);
    ledger_.start = now;
    ledger_.entries.clear();
    ledger_.open.clear();
}

std::string story_memory_summary(){
    Ledger& ledger_ = ledger();
    const int64_t now = memory_now();
    std::vector<std::pair<std::string, LedgerEntry>> entries;
    int64_t start;
    {
        std::lock_guard<Mutex> lg(ledger_.lock);
        start = ledger_.start;
        entries.assign(ledger_.entries.begin(), ledger_.entries.end());
        //  What the open scopes counted so far.
        if (!ledger_.open.empty()){
            for (auto& item : entries){
                if (item.first == ledger_.open.back().first){
                    item.second.bytes += now - ledger_.open.back().second;
                }
            }
        }
    }
    std::sort(
        entries.begin(), entries.end(),
        [](const auto& a, const auto& b){ return a.second.bytes > b.second.bytes; }
    );
    int64_t counted = 0;
    std::string list;
    for (const auto& item : entries){
        counted += item.second.bytes;
        list += ", " + item.first + " " + megabytes(item.second.bytes) +
            " (" + std::to_string(item.second.count) + ")";
    }
    return "Memory growth since the start: " + megabytes(now - start) +
        ". By activity: other " + megabytes(now - start - counted) + list + ".";
}



}
}
}

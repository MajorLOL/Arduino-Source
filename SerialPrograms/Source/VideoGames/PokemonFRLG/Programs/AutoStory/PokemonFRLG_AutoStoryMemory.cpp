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
#include "Common/Cpp/Time.h"
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
    //  The time in the activity, without the scopes inside it.
    double seconds = 0;
};

struct OpenScope{
    const char* activity;
    int64_t bytes;
    WallClock time;
};

double seconds_between(WallClock from, WallClock to){
    return std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count() / 1000.0;
}

struct Ledger{
    Mutex lock;
    int64_t start = 0;
    WallClock start_time = current_time();
    std::map<std::string, LedgerEntry> entries;
    //  The scopes now open, the innermost last, with the memory and the time
    //  when each last started counting.
    std::vector<OpenScope> open;
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
    const WallClock time = current_time();
    std::lock_guard<Mutex> lg(ledger_.lock);
    if (!ledger_.open.empty()){
        //  The outer scope stops counting while this one is open.
        LedgerEntry& outer = ledger_.entries[ledger_.open.back().activity];
        outer.bytes += now - ledger_.open.back().bytes;
        outer.seconds += seconds_between(ledger_.open.back().time, time);
    }
    ledger_.open.push_back(OpenScope{activity, now, time});
    ledger_.entries[activity].count++;
}

StoryMemoryScope::~StoryMemoryScope(){
    Ledger& ledger_ = ledger();
    const int64_t now = memory_now();
    const WallClock time = current_time();
    std::lock_guard<Mutex> lg(ledger_.lock);
    //  Scopes close in the reverse order they opened (they live on the
    //  program thread's stack).
    for (size_t i = ledger_.open.size(); i > 0; i--){
        if (ledger_.open[i - 1].activity != m_activity){
            continue;
        }
        if (i == ledger_.open.size()){
            LedgerEntry& entry = ledger_.entries[m_activity];
            entry.bytes += now - ledger_.open[i - 1].bytes;
            entry.seconds += seconds_between(ledger_.open[i - 1].time, time);
        }
        ledger_.open.erase(ledger_.open.begin() + (std::ptrdiff_t)(i - 1));
        break;
    }
    if (!ledger_.open.empty()){
        ledger_.open.back().bytes = now;
        ledger_.open.back().time = time;
    }
}


void story_memory_reset(){
    Ledger& ledger_ = ledger();
    const int64_t now = memory_now();
    std::lock_guard<Mutex> lg(ledger_.lock);
    ledger_.start = now;
    ledger_.start_time = current_time();
    ledger_.entries.clear();
    ledger_.open.clear();
}

std::string story_memory_summary(){
    Ledger& ledger_ = ledger();
    const int64_t now = memory_now();
    std::vector<std::pair<std::string, LedgerEntry>> entries;
    int64_t start;
    WallClock start_time;
    {
        std::lock_guard<Mutex> lg(ledger_.lock);
        start = ledger_.start;
        start_time = ledger_.start_time;
        entries.assign(ledger_.entries.begin(), ledger_.entries.end());
        //  What the open scopes counted so far.
        if (!ledger_.open.empty()){
            for (auto& item : entries){
                if (item.first == ledger_.open.back().activity){
                    item.second.bytes += now - ledger_.open.back().bytes;
                    item.second.seconds += seconds_between(ledger_.open.back().time, current_time());
                }
            }
        }
    }
    std::sort(
        entries.begin(), entries.end(),
        [](const auto& a, const auto& b){ return a.second.bytes > b.second.bytes; }
    );
    //  The minutes in each activity: a leak that goes with the time (a
    //  thread that runs all the time) grows each activity by its minutes;
    //  one that goes with an activity grows that one faster.
    int64_t counted = 0;
    double counted_seconds = 0;
    std::string list;
    for (const auto& item : entries){
        counted += item.second.bytes;
        counted_seconds += item.second.seconds;
        list += ", " + item.first + " " + megabytes(item.second.bytes) +
            " (" + std::to_string(item.second.count) + ", " +
            std::to_string((int64_t)(item.second.seconds / 60 + 0.5)) + " min)";
    }
    //  The rate per hour compares runs (a change against the growth shows
    //  there); the activities only say what was going on (other threads
    //  allocate and free all the time, so a short scope swings both ways).
    const double hours = std::chrono::duration_cast<std::chrono::seconds>(current_time() - start_time).count() / 3600.0;
    char rate[64] = "";
    if (hours >= 0.1){
        snprintf(rate, sizeof(rate), " in %.1f h (%+.0f MB an hour)", hours, (double)(now - start) / (1024.0 * 1024.0) / hours);
    }
    const double other_minutes = std::max(0.0, hours * 60 - counted_seconds / 60);
    return "Memory growth since the start: " + megabytes(now - start) + rate +
        ". By activity: other " + megabytes(now - start - counted) +
        " (" + std::to_string((int64_t)(other_minutes + 0.5)) + " min)" + list + ".";
}



}
}
}

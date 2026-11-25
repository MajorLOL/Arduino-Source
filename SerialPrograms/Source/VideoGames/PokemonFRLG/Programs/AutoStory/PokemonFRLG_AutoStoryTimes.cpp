/*  Pokemon FRLG Autostory Times
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <map>
#include "PokemonFRLG_AutoStoryTimes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStoryTimes{

namespace{

//  A share in whole percent.
int percent(Milliseconds part, Milliseconds whole){
    if (whole.count() <= 0){
        return 0;
    }
    return (int)((part.count() * 100 + whole.count() / 2) / whole.count());
}

std::string with_share(Milliseconds time, Milliseconds whole){
    return time_text(time) + " (" + std::to_string(percent(time, whole)) + " %)";
}

}


std::string time_text(Milliseconds time){
    int64_t seconds = (int64_t)((time.count() + 500) / 1000);
    if (seconds < 0){
        seconds = 0;
    }
    const int64_t hours = seconds / 3600;
    const int64_t minutes = seconds / 60 % 60;
    if (hours > 0){
        return std::to_string(hours) + " h " + std::to_string(minutes) + " min";
    }
    if (minutes > 0){
        return std::to_string(minutes) + " min " + std::to_string(seconds % 60) + " s";
    }
    return std::to_string(seconds) + " s";
}


void Times::add(const std::string& id, const std::string& title, int32_t chapter, Milliseconds time, bool done){
    if (time.count() < 0){
        time = Milliseconds(0);
    }
    Entry* entry = nullptr;
    for (Entry& item : m_entries){
        if (item.id == id){
            entry = &item;
            break;
        }
    }
    if (entry == nullptr){
        m_entries.emplace_back();
        entry = &m_entries.back();
        entry->id = id;
        entry->title = title;
        entry->chapter = chapter;
    }
    entry->time += time;
    entry->goes++;
    m_goes++;
    if (!done){
        entry->lost += time;
        entry->failed++;
        m_failed_goes++;
    }
}

std::vector<std::string> Times::report(Milliseconds run_time, size_t longest) const{
    std::vector<std::string> lines;

    //  What was run: not the segments that were skipped in no time.
    std::vector<const Entry*> ran;
    Milliseconds in_segments{0};
    Milliseconds lost{0};
    for (const Entry& entry : m_entries){
        if (entry.time < Milliseconds(1000)){
            continue;
        }
        ran.push_back(&entry);
        in_segments += entry.time;
        lost += entry.lost;
    }
    if (ran.empty()){
        return lines;
    }
    if (run_time < in_segments){
        run_time = in_segments;
    }

    std::string head =
        "Where the time went: " + time_text(run_time) + " in this run, " + with_share(in_segments, run_time) +
        " in " + std::to_string(ran.size()) + (ran.size() == 1 ? " segment" : " segments");
    if (lost.count() > 0){
        head += ", of which " + with_share(lost, run_time) + " in goes that did not come through";
    }
    head += ". The rest (" + time_text(run_time - in_segments) +
        ") is the start, the loads after a reset and what the program does between the segments.";
    lines.push_back(head);

    //  The longest segments (the earlier one first when two took as long).
    std::stable_sort(ran.begin(), ran.end(), [](const Entry* a, const Entry* b){
        return a->time > b->time;
    });
    for (size_t c = 0; c < ran.size() && c < longest; c++){
        const Entry& entry = *ran[c];
        std::string line = "  " + entry.id + " (" + entry.title + "): " + with_share(entry.time, run_time);
        if (entry.goes > 1){
            line += ", " + std::to_string(entry.goes) + " goes";
        }
        if (entry.failed > 0){
            line += ", " + std::to_string(entry.failed) + " did not come through";
        }
        lines.push_back(line);
    }

    //  The longest chapters, when the run was in more than one.
    std::map<int32_t, Milliseconds> chapters;
    for (const Entry* entry : ran){
        chapters[entry->chapter] += entry->time;
    }
    if (chapters.size() > 1){
        std::vector<std::pair<int32_t, Milliseconds>> by_time(chapters.begin(), chapters.end());
        std::stable_sort(by_time.begin(), by_time.end(), [](const auto& a, const auto& b){
            return a.second > b.second;
        });
        std::string line = "Longest chapters: ";
        for (size_t c = 0; c < by_time.size() && c < 5; c++){
            line += (c == 0 ? "" : ", ") + std::to_string(by_time[c].first) + ": " + with_share(by_time[c].second, run_time);
        }
        lines.push_back(line);
    }
    return lines;
}


}
}
}
}

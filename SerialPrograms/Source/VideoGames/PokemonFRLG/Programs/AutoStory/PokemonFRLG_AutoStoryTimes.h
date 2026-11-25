/*  Pokemon FRLG Autostory Times
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the time of a run went.
 *
 *  The numbers of the run of 13 November 2026 (section 2 of the ROADMAP)
 *  came from reading the log by hand. Now the program keeps the time of
 *  every go at a segment, and says at the end of the run (also when it is
 *  stopped, or ends on an error):
 *
 *    - how long the run was, and how much of it was in segments;
 *    - how much of that was in goes that did not come through (a failure,
 *      a reset, a stop): the time that was lost;
 *    - the longest segments, with the goes they took;
 *    - the longest chapters.
 *
 *  What is not in a segment is the start, the loads after a reset and what
 *  the program does between the segments. A segment that was done in an
 *  earlier run is skipped in no time: it is not counted unless it took a
 *  second.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTimes_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTimes_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStoryTimes{


//  A time as it is read in a log: "42 s", "25 min 10 s", "2 h 5 min".
std::string time_text(Milliseconds time);


class Times{
public:
    //  One go at a segment: how long it took, and whether it came through.
    void add(const std::string& id, const std::string& title, int32_t chapter, Milliseconds time, bool done);

    //  All the goes, and the ones that did not come through.
    size_t goes() const{ return m_goes; }
    size_t failed_goes() const{ return m_failed_goes; }

    //  The lines for the log. "run_time" is the time of the whole run. No
    //  lines when no segment took a second.
    std::vector<std::string> report(Milliseconds run_time, size_t longest = 8) const;

private:
    struct Entry{
        std::string id;
        std::string title;
        int32_t chapter = 0;
        Milliseconds time{0};
        Milliseconds lost{0};   //  In goes that did not come through.
        size_t goes = 0;
        size_t failed = 0;
    };
    std::vector<Entry> m_entries;   //  In the order they were first run.
    size_t m_goes = 0;
    size_t m_failed_goes = 0;
};


//  The time of one go at a segment, added to "times" when it ends: as done
//  when done() was called before, as one that did not come through when the
//  scope was left by an exception (a failure, a stop).
class Timer{
    Timer(const Timer&) = delete;
    void operator=(const Timer&) = delete;

public:
    Timer(Times& times, const std::string& id, const std::string& title, int32_t chapter)
        : m_times(times)
        , m_id(id)
        , m_title(title)
        , m_chapter(chapter)
        , m_start(current_time())
    {}
    ~Timer(){
        m_times.add(
            m_id, m_title, m_chapter,
            std::chrono::duration_cast<Milliseconds>(current_time() - m_start),
            m_done
        );
    }
    void done(){
        m_done = true;
    }

private:
    Times& m_times;
    std::string m_id;
    std::string m_title;
    int32_t m_chapter;
    WallClock m_start;
    bool m_done = false;
};


}
}
}
}
#endif

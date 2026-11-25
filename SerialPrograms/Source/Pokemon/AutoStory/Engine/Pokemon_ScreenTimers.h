/*  Screen Timers
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Stateful temporal filters over boolean screen conditions: consecutive
 *  streaks, sustained holds, edges, and flickering signals.
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenTimers_H
#define PokemonAutomation_Pokemon_ScreenTimers_H

#include <algorithm>
#include <cstddef>
#include <deque>
#include <optional>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace Pokemon{


//  True once a condition has matched on `required` consecutive updates.
class ConfirmStreak{
public:
    explicit ConfirmStreak(size_t required = 2)
        : m_required(required)
    {}

    bool update(bool matched){
        if (matched){
            m_count++;
        }else{
            m_count = 0;
        }
        return matched && m_count >= m_required;
    }

    void reset(){
        m_count = 0;
    }

    size_t count() const{
        return m_count;
    }

private:
    size_t m_required;
    size_t m_count = 0;
};

//  True once a condition has stayed continuously matched for `duration`.
class HoldTimer{
public:
    explicit HoldTimer(Milliseconds duration)
        : m_duration(duration)
    {}

    bool update(bool matched, WallClock now){
        if (!matched){
            m_since.reset();
            return false;
        }
        if (!m_since.has_value()){
            m_since = now;
        }
        return held(now);
    }

    bool held(WallClock now) const{
        return m_since.has_value() && now - *m_since >= m_duration;
    }

    void reset(){
        m_since.reset();
    }

private:
    Milliseconds m_duration;
    std::optional<WallClock> m_since = std::nullopt;
};

//  Detects false-to-true (rising) and true-to-false (falling) transitions.
class EdgeTrigger{
public:
    bool rising(bool now){
        const bool edge = now && !m_state;
        m_state = now;
        return edge;
    }

    bool falling(bool now){
        const bool edge = !now && m_state;
        m_state = now;
        return edge;
    }

    bool state() const{
        return m_state;
    }

    void reset(){
        m_state = false;
    }

private:
    bool m_state = false;
};

//  A reading that flickers: on once seen on `hits` looks within `window`,
//  then stays on until unseen for `off_after`.
class FlickerFilter{
public:
    FlickerFilter(size_t hits, Milliseconds window, Milliseconds off_after)
        : m_hits(hits)
        , m_window(window)
        , m_off_after(off_after)
    {}

    //  Whether the signal is on with a look at `when` that saw it or not.
    bool on_with(bool seen, WallClock when) const{
        if (seen){
            const size_t recent = (size_t)std::count_if(m_seen.begin(), m_seen.end(), [&](WallClock at){
                return when - at <= m_window;
            });
            return m_on || recent + 1 >= m_hits;
        }
        return m_on && !m_seen.empty() && when - m_seen.back() < m_off_after;
    }

    void record(bool seen, WallClock when){
        m_on = on_with(seen, when);
        if (seen){
            m_seen.push_back(when);
        }
        while (m_seen.size() > 1 && when - m_seen.front() > m_window){
            m_seen.pop_front();
        }
    }

    void reset(){
        m_seen.clear();
        m_on = false;
    }

private:
    size_t m_hits;
    Milliseconds m_window;
    Milliseconds m_off_after;
    std::deque<WallClock> m_seen;
    bool m_on = false;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

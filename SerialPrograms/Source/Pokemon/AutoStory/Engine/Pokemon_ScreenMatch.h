/*  Screen Match
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Declarative predicate over one ScreenObservation: required, forbidden,
 *  and optional events and reading keys.
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenMatch_H
#define PokemonAutomation_Pokemon_ScreenMatch_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Fixed-capacity list of reading keys with no heap allocation.
template <size_t N = 6>
struct ReadingList{
    static_assert(N <= 255, "ReadingList capacity must fit in uint8_t.");

    std::array<std::string_view, N> items = {};
    uint8_t size = 0;

    constexpr ReadingList() = default;
    constexpr ReadingList(std::initializer_list<std::string_view> list){
        for (std::string_view item : list){
            if (size < N){
                items[size++] = item;
            }
        }
    }

    constexpr bool empty() const{
        return size == 0;
    }
    constexpr const std::string_view* begin() const{
        return items.data();
    }
    constexpr const std::string_view* end() const{
        return items.data() + size;
    }
};

//  Matches a ScreenObservation when every required event/reading is present,
//  no forbidden event/reading is present, and (when any_events or any_readings
//  is non-empty) at least one of those events or readings is present.
struct ScreenMatch{
    ScreenEvents all_events = {};
    ScreenEvents any_events = {};
    ScreenEvents no_events = {};
    ReadingList<6> all_readings = {};
    ReadingList<6> any_readings = {};
    ReadingList<6> no_readings = {};

    bool matches(const ScreenObservation& look) const{
        ScreenEvents merged = look.events;
        merged.merge(all_events);
        if (merged != look.events){
            return false;
        }
        for (std::string_view key : all_readings){
            if (!look.readings.contains(key)){
                return false;
            }
        }
        if (look.events.any_of(no_events)){
            return false;
        }
        for (std::string_view key : no_readings){
            if (look.readings.contains(key)){
                return false;
            }
        }
        const bool has_any_events = any_events != ScreenEvents{};
        const bool has_any_readings = !any_readings.empty();
        if (has_any_events || has_any_readings){
            if (has_any_events && look.events.any_of(any_events)){
                return true;
            }
            for (std::string_view key : any_readings){
                if (look.readings.contains(key)){
                    return true;
                }
            }
            return false;
        }
        return true;
    }

    bool operator()(const ScreenObservation& look) const{
        return matches(look);
    }
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

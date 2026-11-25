/*  RNG Target Search
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Ten Lines finds targets in two steps, and this does the same in one:
 *   1. Searcher (src/wasm/src/searcher.cpp, search_seeds_static()): PokeFinder's
 *      StaticSearcher3 lists every RNG state whose Method 1 Pokemon passes the filters.
 *   2. Initial Seed (src/wasm/src/initial_seed.cpp, frlg_seeds()): for a target state,
 *      the initial seeds from the seed table, ordered by the advances from the seed to
 *      the target. That is the LCRNG distance, since the game starts the RNG at the seed.
 *
 *  The code here is written from that description, not copied from either project.
 *
 */

#include <algorithm>
#include <map>
#include "PokemonFRLG_RngTargetSearch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;


namespace{

const uint32_t LCRNG_MULT = 0x41c64e6d;
const uint32_t LCRNG_ADD = 0x6073;
const uint32_t LCRNG_MULT_INVERSE = 0xeeb9eb65;
const uint32_t LCRNG_ADD_INVERSE = 0x0a3561a1;

//  The GBA frame rate, as Ten Lines uses it (SYSTEM_TIMING_DATA in src/tenLines/index.ts).
const double GBA_FRAMES_PER_SECOND = 16777216.0 / 280896.0;

}


uint32_t lcrng_next(uint32_t state){
    return state * LCRNG_MULT + LCRNG_ADD;
}
uint32_t lcrng_previous(uint32_t state){
    return state * LCRNG_MULT_INVERSE + LCRNG_ADD_INVERSE;
}

//  Bit by bit: once the low k bits of "from" and "to" agree, only a jump of 2^k steps
//  keeps them agreeing, and it flips bit k. So bit k of the distance is whether bit k
//  still differs.
uint32_t lcrng_distance(uint32_t from, uint32_t to){
    uint32_t mult = LCRNG_MULT;
    uint32_t add = LCRNG_ADD;
    uint32_t distance = 0;
    for (uint32_t bit = 1; from != to; bit <<= 1){
        if ((from ^ to) & bit){
            from = from * mult + add;
            distance |= bit;
        }
        add *= mult + 1;
        mult *= mult;
    }
    return distance;
}


AdvPokemonResult method1_pokemon(uint32_t state){
    uint32_t pid_low = lcrng_next(state);
    uint32_t pid_high = lcrng_next(pid_low);
    uint32_t iv_first = lcrng_next(pid_high);
    uint32_t iv_second = lcrng_next(iv_first);

    uint32_t pid = (pid_high & 0xffff0000) | (pid_low >> 16);
    iv_first >>= 16;
    iv_second >>= 16;

    AdvIVs ivs;
    ivs.hp      = iv_first & 0x1f;
    ivs.attack  = (iv_first >> 5) & 0x1f;
    ivs.defense = (iv_first >> 10) & 0x1f;
    ivs.speed   = iv_second & 0x1f;
    ivs.spatk   = (iv_second >> 5) & 0x1f;
    ivs.spdef   = (iv_second >> 10) & 0x1f;

    return {pid, uint8_t(pid & 0xff), AdvNature(pid % 25), AdvAbility(pid & 1), ivs};
}

AdvShinyType shiny_type(uint32_t pid, uint16_t tid, uint16_t sid){
    uint16_t value = uint16_t(pid >> 16) ^ uint16_t(pid) ^ tid ^ sid;
    if (value == 0){
        return AdvShinyType::Square;
    }
    return value < 8 ? AdvShinyType::Star : AdvShinyType::Normal;
}

HiddenPower hidden_power(const AdvIVs& ivs){
    static const char* TYPES[] = {
        "Fighting", "Flying", "Poison", "Ground", "Rock", "Bug", "Ghost", "Steel",
        "Fire", "Water", "Grass", "Electric", "Psychic", "Ice", "Dragon", "Dark",
    };
    //  Bit 0 of each IV picks the type, bit 1 the power, in the order HP, Atk, Def, Spe,
    //  SpA, SpD.
    const uint8_t order[] = {ivs.hp, ivs.attack, ivs.defense, ivs.speed, ivs.spatk, ivs.spdef};
    int type_bits = 0;
    int power_bits = 0;
    for (int c = 0; c < 6; c++){
        type_bits |= (order[c] & 1) << c;
        power_bits |= ((order[c] >> 1) & 1) << c;
    }
    return {TYPES[type_bits * 15 / 63], uint8_t(power_bits * 40 / 63 + 30)};
}


std::vector<InitialSeedEntry> initial_seed_entries(
    const SeedsDatabase& database,
    SoundSetting sound,
    const std::string& button_mode,
    bool allow_blackout
){
    //  Like SeedsDatabase::find_seed(), the first entry found wins a tie.
    std::map<uint16_t, InitialSeedEntry> earliest;
    auto add = [&](const InitialSeedEntry& entry){
        auto iter = earliest.find(entry.seed);
        if (iter == earliest.end() || entry.seed_time_ms < iter->second.seed_time_ms){
            earliest[entry.seed] = entry;
        }
    };

    const std::vector<uint64_t>& delays = database.delays();
    for (const SeedsDatabase::SeedColumn& column : database.columns()){
        if (column.sound != sound || column.button_mode != button_mode){
            continue;
        }
        for (size_t c = 0; c < column.seeds.size() && c < delays.size(); c++){
            uint16_t seed = column.seeds[c];
            add({seed, delays[c], column.seed_button, BlackoutButton::None});
            if (allow_blackout){
                add({uint16_t(seed - BLACKOUT_SEED_OFFSET), delays[c], column.seed_button, BlackoutButton::L});
            }
        }
    }

    std::vector<InitialSeedEntry> entries;
    entries.reserve(earliest.size());
    for (const auto& item : earliest){
        entries.emplace_back(item.second);
    }
    return entries;
}


namespace{

//  Every 16-bit x times the LCRNG multiplier, sorted. A state with known top 16 bits
//  "high" is (high << 16) + x, and its next state is lcrng_next(high << 16) + x * mult,
//  so the states whose next state is in a range are a range of this table.
class LowBitsTable{
public:
    LowBitsTable(){
        m_products.reserve(0x10000);
        for (uint32_t x = 0; x < 0x10000; x++){
            m_products.emplace_back(x * LCRNG_MULT, uint16_t(x));
        }
        std::sort(m_products.begin(), m_products.end());
    }

    //  Calls "found" with each state whose top 16 bits are "high" and whose next state
    //  is in [low, low + width).
    template <typename Callback>
    void find_states(uint16_t high, uint32_t low, uint32_t width, Callback&& found) const{
        uint32_t top = uint32_t(high) << 16;
        uint32_t start = low - lcrng_next(top);
        uint64_t end = uint64_t(start) + width;
        const uint64_t WRAP = uint64_t(1) << 32;
        visit(top, start, std::min(end, WRAP), found);
        if (end > WRAP){
            visit(top, 0, end - WRAP, found);
        }
    }

private:
    template <typename Callback>
    void visit(uint32_t top, uint64_t start, uint64_t end, Callback& found) const{
        auto iter = std::lower_bound(
            m_products.begin(), m_products.end(), start,
            [](const std::pair<uint32_t, uint16_t>& item, uint64_t value){ return item.first < value; }
        );
        for (; iter != m_products.end() && iter->first < end; ++iter){
            found(top | iter->second);
        }
    }

    std::vector<std::pair<uint32_t, uint16_t>> m_products;
};


//  Keeps the states whose Pokemon pass the filters, each with its nearest initial seed.
class TargetCollector{
public:
    TargetCollector(
        const std::vector<InitialSeedEntry>& seeds,
        uint16_t tid,
        const std::vector<SidCandidate>& candidates,
        const RngTargetFilters& filters
    )
        : m_tid(tid)
        , m_candidates(candidates)
        , m_filters(filters)
    {
        for (const InitialSeedEntry& entry : seeds){
            if (seed_time_allowed(entry.seed_time_ms)){
                m_seeds.push_back({lcrng_distance(0, entry.seed), entry});
            }
        }
        std::sort(m_seeds.begin(), m_seeds.end(), [](const SeedPosition& x, const SeedPosition& y){
            return x.distance < y.distance;
        });
        for (const SidCandidate& candidate : candidates){
            m_total_weight += candidate.weight;
        }
    }

    void consider(uint32_t state){
        AdvPokemonResult pokemon = method1_pokemon(state);
        if (!passes(pokemon)){
            return;
        }
        RngTarget target{state, {}, 0, pokemon, {}, 0};
        double weight = 0;
        for (size_t c = 0; c < m_candidates.size(); c++){
            AdvShinyType type = shiny_type(pokemon.pid, m_tid, m_candidates[c].sid);
            if (type != AdvShinyType::Normal){
                target.shiny_for.push_back({c, type});
                weight += m_candidates[c].weight;
            }
        }
        if (m_filters.shiny_required && target.shiny_for.empty()){
            return;
        }
        if (!nearest_seed(state, target)){
            return;
        }
        target.shiny_probability = m_total_weight > 0 ? weight / m_total_weight : 0;
        m_targets.emplace_back(std::move(target));
    }

    std::vector<RngTarget> results(){
        std::sort(m_targets.begin(), m_targets.end(), [](const RngTarget& x, const RngTarget& y){
            return x.advances != y.advances ? x.advances < y.advances : x.state < y.state;
        });
        return std::move(m_targets);
    }

private:
    struct SeedPosition{
        uint32_t distance;
        InitialSeedEntry entry;
    };

    bool seed_time_allowed(uint64_t time) const{
        if (time < m_filters.min_seed_time_ms){
            return false;
        }
        uint64_t gap = time > m_filters.avoided_seed_time_ms
            ? time - m_filters.avoided_seed_time_ms
            : m_filters.avoided_seed_time_ms - time;
        return m_filters.avoided_seed_time_radius_ms == 0 || gap > m_filters.avoided_seed_time_radius_ms;
    }

    bool passes(const AdvPokemonResult& pokemon) const{
        const std::vector<AdvNature>& natures = m_filters.natures;
        if (!natures.empty() && std::find(natures.begin(), natures.end(), pokemon.nature) == natures.end()){
            return false;
        }
        if (m_filters.ability != AdvAbility::Any && pokemon.ability != m_filters.ability){
            return false;
        }
        if (m_filters.gender != AdvGender::Any
            && gender_from_gender_value(pokemon.gender, m_filters.gender_threshold) != m_filters.gender
        ){
            return false;
        }
        const AdvIVs& ivs = pokemon.ivs;
        const AdvIVs& min = m_filters.min_ivs;
        return ivs.hp >= min.hp && ivs.attack >= min.attack && ivs.defense >= min.defense
            && ivs.spatk >= min.spatk && ivs.spdef >= min.spdef && ivs.speed >= min.speed;
    }

    //  The seed with the fewest advances to "state" that are still at least the minimum:
    //  the one with the largest distance from 0 that is at most "state"'s, less the
    //  minimum, wrapping around the cycle.
    bool nearest_seed(uint32_t state, RngTarget& target) const{
        if (m_seeds.empty()){
            return false;
        }
        uint32_t distance = lcrng_distance(0, state);
        uint32_t latest = distance - uint32_t(m_filters.min_advances);
        auto iter = std::upper_bound(
            m_seeds.begin(), m_seeds.end(), latest,
            [](uint32_t value, const SeedPosition& seed){ return value < seed.distance; }
        );
        const SeedPosition& seed = iter == m_seeds.begin() ? m_seeds.back() : *(iter - 1);
        uint64_t advances = uint32_t(distance - seed.distance);
        if (advances < m_filters.min_advances || advances > m_filters.max_advances){
            return false;
        }
        target.seed = seed.entry;
        target.advances = advances;
        return true;
    }

    const uint16_t m_tid;
    const std::vector<SidCandidate>& m_candidates;
    const RngTargetFilters& m_filters;
    double m_total_weight = 0;
    std::vector<SeedPosition> m_seeds;
    std::vector<RngTarget> m_targets;
};


//  Shiny Pokemon only: for each low PID half, the high halves that are shiny for a
//  candidate are 8 aligned values, so each takes one lookup.
void search_shiny_states(
    const LowBitsTable& table,
    uint16_t tid,
    const std::vector<SidCandidate>& candidates,
    TargetCollector& collector
){
    std::vector<uint16_t> shiny_values;
    for (const SidCandidate& candidate : candidates){
        shiny_values.push_back((tid ^ candidate.sid) & 0xfff8);
    }
    std::sort(shiny_values.begin(), shiny_values.end());
    shiny_values.erase(std::unique(shiny_values.begin(), shiny_values.end()), shiny_values.end());

    auto found = [&](uint32_t pid_low_state){
        collector.consider(lcrng_previous(pid_low_state));
    };
    for (uint32_t pid_low = 0; pid_low < 0x10000; pid_low++){
        for (uint16_t value : shiny_values){
            uint32_t pid_high = (pid_low & 0xfff8) ^ value;
            table.find_states(uint16_t(pid_low), pid_high << 16, 8 << 16, found);
        }
    }
}

//  Any Pokemon, like PokeFinder's StaticSearcher3: every IV combination at or above the
//  minimums gives the top 15 bits of the two IV calls.
void search_iv_states(const LowBitsTable& table, const AdvIVs& min_ivs, TargetCollector& collector){
    auto found = [&](uint32_t iv_first_state){
        collector.consider(lcrng_previous(lcrng_previous(lcrng_previous(iv_first_state))));
    };
    //  HP, Atk, Def, Spe, SpA, SpD: the order the IVs sit in the two calls.
    const uint8_t mins[6] = {min_ivs.hp, min_ivs.attack, min_ivs.defense, min_ivs.speed, min_ivs.spatk, min_ivs.spdef};
    uint8_t ivs[6];
    std::copy(mins, mins + 6, ivs);
    while (true){
        uint16_t first = uint16_t(ivs[0] | (ivs[1] << 5) | (ivs[2] << 10));
        uint16_t second = uint16_t(ivs[3] | (ivs[4] << 5) | (ivs[5] << 10));
        for (uint16_t first_top : {0x0000, 0x8000}){
            for (uint32_t second_top : {0x0000, 0x8000}){
                table.find_states(first | first_top, (second | second_top) << 16, 0x10000, found);
            }
        }
        int c = 0;
        while (c < 6 && ivs[c] == 31){
            ivs[c] = mins[c];
            c++;
        }
        if (c == 6){
            return;
        }
        ivs[c]++;
    }
}

}


std::vector<RngTarget> search_rng_targets(
    const std::vector<InitialSeedEntry>& seeds,
    uint16_t tid,
    const std::vector<SidCandidate>& candidates,
    const RngTargetFilters& filters
){
    TargetCollector collector(seeds, tid, candidates, filters);
    LowBitsTable table;
    if (filters.shiny_required){
        search_shiny_states(table, tid, candidates, collector);
    }else{
        search_iv_states(table, filters.min_ivs, collector);
    }
    return collector.results();
}


void rank_rng_targets(std::vector<RngTarget>& targets){
    auto time_ms = [](const RngTarget& target){
        return double(target.seed.seed_time_ms) + double(target.advances) * 1000.0 / GBA_FRAMES_PER_SECOND;
    };
    std::stable_sort(targets.begin(), targets.end(), [&](const RngTarget& x, const RngTarget& y){
        bool x_shiny = x.shiny_probability > 0;
        bool y_shiny = y.shiny_probability > 0;
        if (x_shiny != y_shiny){
            return x_shiny;
        }
        if (!x_shiny){
            return time_ms(x) < time_ms(y);
        }
        return time_ms(x) / x.shiny_probability < time_ms(y) / y.shiny_probability;
    });
}



}
}
}

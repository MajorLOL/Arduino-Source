/*  RNG Target Search Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The expected targets were found with Ten Lines (https://lincoln-lm.github.io/ten-lines)
 *  for LeafGreen (English) on Switch, TID 43771, SID 64694, Mono sound, Help button mode,
 *  Static 1: the lowest advances per target with total advances of at least 3,000 and
 *  seed times more than 3 seconds from 30,500 ms.
 *
 */

#include <algorithm>
#include <sstream>
#include "CommonFramework/Language.h"
#include "PokemonFRLG_RngTargetSearch.h"
#include "PokemonFRLG_RngTargetSearch_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;


namespace{

const uint16_t TID = 43771;
const uint16_t SID = 64694;

//  One Ten Lines result: Target, PID, Shiny, Ability, IVs (HP/Atk/Def/SpA/SpD/Spe),
//  Hidden Power, Initial Seed, Advances, Seed Time (ms), Buttons.
struct TenLinesRow{
    uint32_t state;
    uint32_t pid;
    std::string shiny;
    int ability;
    std::string ivs;
    std::string hidden_power;
    uint16_t seed;
    uint64_t advances;
    uint64_t seed_time_ms;
    std::string buttons;
};

struct TenLinesGroup{
    std::string name;
    std::vector<AdvNature> natures;
    AdvIVs min_ivs;
    std::string rows;
};

//  Ten Lines results, one per line, sorted by advances.
const char MODEST_ROWS[] =
    "9435B76A,68F03EBD,Square,1,31/14/7/29/1/27,Dragon 39,2EF6,29364,52641,A/None\n"
    "2E37261A,DB7C8D33,Star,1,28/16/6/30/11/29,Water 63,6E9A,50048,54054,A/Blackout R\n"
    "C408B1DF,192D4F60,Square,0,25/8/10/29/0/28,Rock 32,02A8,53397,55259,Start/Blackout R\n"
    "814DD9D3,84FBD2B6,Square,0,26/19/17/30/10/30,Flying 67,8EB6,58835,48990,Start/Blackout R\n"
    "0D815228,8C45DA08,Square,0,29/5/29/31/22/25,Steel 60,A356,59258,64959,Start/Blackout R\n"
    "016CB6DF,9C35CA7C,Star,0,29/11/29/30/20/27,Ground 46,A626,69439,48471,A/None\n"
    "5DFAB16B,9FC1C98E,Star,0,26/1/0/30/27/28,Fire 61,2B48,77713,41805,A/None\n"
    "0E710A6F,8E60D829,Star,1,26/1/9/31/10/25,Steel 61,47CD,89010,50903,A/None\n"
    "74224D48,A59DF3D7,Star,1,26/28/22/31/10/30,Ground 68,2CE5,95029,51237,Start/Blackout R\n"
    "18699F2D,4B0B1D46,Square,0,28/23/23/29/17/25,Dragon 33,A4C2,101793,47657,Start/Blackout R\n"
    "19D01211,FB76AD3F,Star,1,31/16/12/30/22/30,Fighting 66,7DC6,103697,68622,A/Blackout R\n"
    "BCDA4E7A,CF729938,Star,0,30/31/30/31/18/26,Rock 70,99D2,107880,36842,Start/None\n"
    "1A5A9954,E80BBE43,Star,1,29/24/22/29/4/29,Bug 32,C1C1,107989,35490,A/None\n"
    "5A6B83CC,ABF8FDB7,Star,1,27/30/3/31/17/30,Psychic 49,811D,119809,35822,A/Blackout R\n"
    "50F06E45,D01B8656,Square,0,26/8/29/31/4/26,Rock 45,4C09,125356,67439,A/None\n"
    "9F536DFD,C6FA90B7,Square,1,28/13/28/29/28/25,Ghost 30,1548,129923,66441,Start/None\n"
    "2F82C0CA,21ED77A6,Star,0,25/16/4/29/1/31,Ice 35,4FDC,150758,39071,Start/None\n"
    "4C7401EB,5AF00CBC,Star,0,30/12/0/30/21/26,Steel 45,45DC,155101,55440,Start/None\n"
    "BE31CFCC,FA05AC4E,Star,0,30/22/21/30/3/29,Grass 62,D17C,158288,38405,Start/Blackout R\n"
    "3F487560,7B922DDD,Star,1,30/21/5/31/21/27,Dragon 45,FF1C,171172,58240,A/None\n";

const char ADAMANT_ROWS[] =
    "537A2752,A823FE6A,Star,0,25/29/15/17/31/27,Dark 57,B897,4437,35972,A/Blackout R\n"
    "D9FDB041,B9B4EFFE,Star,0,27/31/19/11/5/31,Dark 49,D356,22897,60771,A/None\n"
    "E9F88FDD,EAD6BC98,Star,0,30/29/1/29/23/31,Dragon 56,CE81,26060,33587,Start/None\n"
    "C3B70E12,D96E8F26,Star,0,29/31/31/9/4/31,Steel 38,8AA1,30571,57386,Start/None\n"
    "37BB3B92,BD83EBCB,Star,1,27/30/19/8/26/25,Ground 54,F054,33142,46390,Start/None\n"
    "E8D67E1F,BEF4E8BB,Star,1,26/30/2/25/21/26,Electric 39,A9D1,39694,61024,A/Blackout R\n"
    "DE925A82,083B5E70,Star,0,28/30/22/26/3/27,Water 69,4665,42695,49155,Start/Blackout R\n"
    "D8713EE8,6E9838D2,Star,0,29/31/14/6/26/27,Poison 69,1233,48679,63991,Start/None\n"
    "B50AB28B,EC3FBA76,Star,0,30/31/22/25/27/27,Ice 59,704A,61591,65705,A/None\n"
    "0A550D9C,25487304,Star,0,30/29/14/5/27/31,Ice 58,B845,68937,53954,A/None\n"
    "119C3777,633C3574,Star,0,26/29/4/26/18/30,Fighting 66,A39D,85194,58722,Start/Blackout R\n"
    "315D5C57,AF63F92D,Star,1,30/29/23/13/21/29,Dragon 33,DE4C,95017,36725,A/None\n"
    "66A23785,8397D5DA,Square,0,25/30/7/24/27/31,Grass 59,9DAC,95463,59324,A/None\n"
    "A31E8DB4,462C1067,Star,1,31/31/17/9/22/28,Bug 52,0D4C,99560,36085,A/None\n"
    "6109BF46,A9DAFF92,Star,0,26/31/30/6/2/30,Fighting 70,3136,103312,63359,A/None\n"
    "3F5603C4,8C11DA5D,Star,1,25/29/6/6/24/29,Poison 42,D22E,111518,54657,Start/None\n";

const char NEUTRAL_ROWS[] =
    "130195A8,AD7BFB36,Square,0,31/27/31/31/27/26,Ice 70,4381,17225,61908,Start/None\n"
    "BE0D7E8F,6FA739E8,Star,0,31/21/25/29/26/25,Steel 50,C845,21690,41371,Start/None\n"
    "1798BCE3,066A5027,Square,1,28/23/25/30/31/28,Water 61,3DDC,26549,44721,A/None\n"
    "0602C959,EA14BC5F,Star,1,27/21/27/30/27/25,Electric 63,DA81,31480,46973,Start/None\n"
    "5B31FD89,A635F07F,Star,1,31/22/19/31/22/31,Ghost 70,B8BB,48990,35972,A/None\n"
    "0BA51887,D464822E,Star,0,25/16/17/31/28/28,Bug 40,3C95,59906,38258,A/Blackout R\n"
    "61407261,751D2350,Square,0,30/18/16/31/30/28,Ground 62,B83C,61923,51625,Start/Blackout R\n"
    "BBB6D222,9F67C92A,Square,0,30/28/25/30/24/31,Poison 45,1439,76611,62542,A/Blackout R\n"
    "2C224D48,1D9D4BD7,Star,1,26/28/28/31/24/30,Ground 45,0210,83512,56705,Start/None\n"
    "AC47BD6D,33D86591,Star,1,31/19/19/31/31/27,Dark 70,F483,101418,57476,A/Blackout R\n"
    "0937DE0D,AB36FD7F,Star,1,27/18/18/30/19/29,Water 64,E588,103955,53106,A/None\n"
    "30339DE2,E8A8BEE1,Star,1,27/17/25/29/19/29,Dark 50,47A0,126746,69310,A/Blackout R\n"
    "E78F978E,C86B9E25,Star,1,25/20/31/29/23/29,Dragon 52,5A24,136290,33839,A/None\n"
    "4A07D3EB,2CE57AA9,Star,1,25/26/24/30/27/25,Water 61,9754,145893,38338,Start/None\n";

std::vector<TenLinesGroup> groups(){
    AdvIVs modest;
    modest.hp = 25;
    modest.spatk = 29;
    modest.speed = 25;
    AdvIVs adamant;
    adamant.hp = 25;
    adamant.attack = 29;
    adamant.speed = 25;
    AdvIVs neutral = modest;
    neutral.attack = neutral.defense = neutral.spdef = 16;

    using enum AdvNature;
    return {
        {"Modest", {Modest}, modest, MODEST_ROWS},
        {"Adamant", {Adamant}, adamant, ADAMANT_ROWS},
        {"Neutral", {Hardy, Docile, Serious, Bashful, Quirky}, neutral, NEUTRAL_ROWS},
    };
}

std::vector<TenLinesRow> parse_rows(const std::string& text){
    std::vector<TenLinesRow> rows;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)){
        std::vector<std::string> cells;
        std::istringstream fields(line);
        std::string cell;
        while (std::getline(fields, cell, ',')){
            cells.push_back(cell);
        }
        rows.push_back({
            uint32_t(std::stoul(cells[0], nullptr, 16)),
            uint32_t(std::stoul(cells[1], nullptr, 16)),
            cells[2],
            std::stoi(cells[3]),
            cells[4],
            cells[5],
            uint16_t(std::stoul(cells[6], nullptr, 16)),
            std::stoull(cells[7]),
            std::stoull(cells[8]),
            cells[9],
        });
    }
    return rows;
}

std::string hex(uint32_t value){
    std::ostringstream out;
    out << std::hex << std::uppercase << value;
    return out.str();
}

std::string ivs_text(const AdvIVs& ivs){
    return std::to_string(ivs.hp) + "/" + std::to_string(ivs.attack) + "/" + std::to_string(ivs.defense)
        + "/" + std::to_string(ivs.spatk) + "/" + std::to_string(ivs.spdef) + "/" + std::to_string(ivs.speed);
}

std::string shiny_text(AdvShinyType type){
    switch (type){
    case AdvShinyType::Square:  return "Square";
    case AdvShinyType::Star:    return "Star";
    default:                    return "No";
    }
}

//  Ten Lines writes "Blackout R"; its Switch offsets for R and L are the same, and the
//  RNG programs hold L.
std::string buttons_text(const InitialSeedEntry& entry){
    std::string seed_button = entry.seed_button == SeedButton::Start ? "Start"
        : entry.seed_button == SeedButton::L ? "L" : "A";
    return seed_button + (entry.blackout_button == BlackoutButton::None ? "/None" : "/Blackout R");
}

std::string row_name(const TenLinesRow& row){
    return "Target " + hex(row.state) + ": ";
}

uint32_t advance(uint32_t state, uint64_t advances){
    for (uint64_t c = 0; c < advances; c++){
        state = lcrng_next(state);
    }
    return state;
}


std::string check_pokemon(const TenLinesRow& row, const TenLinesGroup& group){
    AdvPokemonResult pokemon = method1_pokemon(row.state);
    HiddenPower power = hidden_power(pokemon.ivs);
    std::string actual = hex(pokemon.pid) + " " + shiny_text(shiny_type(pokemon.pid, TID, SID))
        + " " + std::to_string(int(pokemon.ability)) + " " + ivs_text(pokemon.ivs)
        + " " + power.type + " " + std::to_string(power.power);
    std::string expected = hex(row.pid) + " " + row.shiny + " " + std::to_string(row.ability)
        + " " + row.ivs + " " + row.hidden_power;
    if (actual != expected){
        return row_name(row) + "expected " + expected + ", got " + actual;
    }
    const std::vector<AdvNature>& natures = group.natures;
    if (std::find(natures.begin(), natures.end(), pokemon.nature) == natures.end()){
        return row_name(row) + "nature " + nature_to_string(pokemon.nature) + " is not in the group";
    }
    return "";
}

std::string check_advances(const TenLinesRow& row){
    if (advance(row.seed, row.advances) != row.state){
        return row_name(row) + "seed " + hex(row.seed) + " + " + std::to_string(row.advances)
            + " advances does not reach it";
    }
    if (lcrng_distance(row.seed, row.state) != row.advances){
        return row_name(row) + "wrong distance from seed " + hex(row.seed);
    }
    //  The RNG programs count advances the same way.
    AdvRngSearcher program(row.seed, row.advances, AdvRngMethod::Method1);
    if (program.generate_pokemon().pid != row.pid){
        return row_name(row) + "AdvRngSearcher gives PID " + hex(program.generate_pokemon().pid);
    }
    return "";
}

class Test_RngTargetMethod1 : public UnitTest{
public:
    Test_RngTargetMethod1(TenLinesGroup group)
        : UnitTest("PokemonFRLG::RngTargetSearch - Method 1 - " + group.name)
        , m_group(std::move(group))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        for (const TenLinesRow& row : parse_rows(m_group.rows)){
            std::string error = check_pokemon(row, m_group);
            if (error.empty()){
                error = check_advances(row);
            }
            if (!error.empty()){
                return error;
            }
        }
        return true;
    }

private:
    const TenLinesGroup m_group;
};


RngTargetFilters ten_lines_filters(const TenLinesGroup& group){
    RngTargetFilters filters;
    filters.natures = group.natures;
    filters.min_ivs = group.min_ivs;
    filters.min_advances = 3000;
    filters.avoided_seed_time_ms = 30500;
    filters.avoided_seed_time_radius_ms = 3000;
    return filters;
}

std::vector<InitialSeedEntry> leafgreen_mono_help_seeds(){
    SeedsDatabase database(seeds_json_path(false, Language::English));
    return initial_seed_entries(database, SoundSetting::Mono, "Help", true);
}

std::string compare_seed_entry(const TenLinesRow& row, const InitialSeedEntry& entry){
    std::string expected = std::to_string(row.seed_time_ms) + " " + row.buttons;
    std::string actual = std::to_string(entry.seed_time_ms) + " " + buttons_text(entry);
    //  Ten Lines lists every way to reach a seed, and the RNG programs use the earliest
    //  (SeedsDatabase::find_seed()): 3136 by Start at 58991 ms, not by A at 63359 ms.
    if (buttons_text(entry) != row.buttons){
        return entry.seed_time_ms < row.seed_time_ms ? "" : "Expected " + expected + ", got " + actual;
    }
    //  Ten Lines rounds the seed time down from 1/16 frames; the seed table here is 1 ms
    //  lower on about 1 row in 17 (61023 instead of 61024 for row 1833).
    if (entry.seed_time_ms > row.seed_time_ms || entry.seed_time_ms + 1 < row.seed_time_ms){
        return "Expected " + expected + ", got " + actual;
    }
    return "";
}

std::string compare_target(const TenLinesRow& row, const RngTarget& target){
    std::string expected = hex(row.state) + " " + hex(row.seed) + " " + std::to_string(row.advances);
    std::string actual = hex(target.state) + " " + hex(target.seed.seed) + " " + std::to_string(target.advances);
    if (actual != expected){
        return "Expected " + expected + ", got " + actual;
    }
    std::string error = compare_seed_entry(row, target.seed);
    if (!error.empty()){
        return row_name(row) + error;
    }
    if (target.shiny_for.size() != 1 || shiny_text(target.shiny_for[0].type) != row.shiny || target.shiny_probability != 1){
        return row_name(row) + "expected shiny (" + row.shiny + ") for the one SID";
    }
    return "";
}

class Test_RngTargetSearch : public UnitTest{
public:
    Test_RngTargetSearch(TenLinesGroup group)
        : UnitTest("PokemonFRLG::RngTargetSearch - Search - " + group.name)
        , m_group(std::move(group))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<RngTarget> targets = search_rng_targets(
            leafgreen_mono_help_seeds(), TID, {{SID, 1}}, ten_lines_filters(m_group)
        );
        std::vector<TenLinesRow> rows = parse_rows(m_group.rows);
        if (targets.size() < rows.size()){
            return "Expected at least " + std::to_string(rows.size()) + " targets, got " + std::to_string(targets.size());
        }
        for (size_t c = 0; c < rows.size(); c++){
            std::string error = compare_target(rows[c], targets[c]);
            if (!error.empty()){
                return "Target #" + std::to_string(c + 1) + ": " + error;
            }
        }
        return true;
    }

private:
    const TenLinesGroup m_group;
};


//  Searching every IV combination finds the same shiny targets as searching shiny PIDs.
class Test_RngTargetIvSearch : public UnitTest{
public:
    Test_RngTargetIvSearch()
        : UnitTest("PokemonFRLG::RngTargetSearch - IV search")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const TenLinesGroup group = groups()[2];
        std::vector<InitialSeedEntry> seeds = leafgreen_mono_help_seeds();
        RngTargetFilters filters = ten_lines_filters(group);
        std::vector<RngTarget> shiny = search_rng_targets(seeds, TID, {{SID, 1}}, filters);

        filters.shiny_required = false;
        std::vector<RngTarget> any = search_rng_targets(seeds, TID, {{SID, 1}}, filters);
        std::vector<RngTarget> any_shiny;
        for (const RngTarget& target : any){
            if (!target.shiny_for.empty()){
                any_shiny.push_back(target);
            }
        }
        if (any.size() <= any_shiny.size()){
            return "Expected targets that are not shiny";
        }
        if (any_shiny.size() != shiny.size()){
            return "Expected " + std::to_string(shiny.size()) + " shiny targets, got " + std::to_string(any_shiny.size());
        }
        for (size_t c = 0; c < shiny.size(); c++){
            if (any_shiny[c].state != shiny[c].state || any_shiny[c].advances != shiny[c].advances){
                return "Shiny target #" + std::to_string(c + 1) + " differs: " + hex(any_shiny[c].state);
            }
        }
        return true;
    }
};


//  A second SID candidate: each target is shiny for one of the two, by weight.
class Test_RngTargetCandidates : public UnitTest{
public:
    Test_RngTargetCandidates()
        : UnitTest("PokemonFRLG::RngTargetSearch - SID candidates")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const TenLinesGroup group = groups()[0];
        const uint16_t other_sid = SID ^ 0x0100;
        std::vector<RngTarget> targets = search_rng_targets(
            leafgreen_mono_help_seeds(), TID, {{SID, 3}, {other_sid, 1}}, ten_lines_filters(group)
        );
        size_t found = 0;
        for (const RngTarget& target : targets){
            if (target.shiny_for.size() != 1){
                return "Target " + hex(target.state) + " is shiny for " + std::to_string(target.shiny_for.size()) + " SIDs";
            }
            double expected = target.shiny_for[0].candidate == 0 ? 0.75 : 0.25;
            if (target.shiny_probability != expected){
                return "Target " + hex(target.state) + " has shiny probability " + std::to_string(target.shiny_probability);
            }
            found += target.shiny_for[0].candidate == 0 && target.advances <= 171172;
        }
        if (found != 20){
            return "Expected the 20 Ten Lines targets for SID " + std::to_string(SID) + ", got " + std::to_string(found);
        }
        return true;
    }
};


class Test_RngTargetRanking : public UnitTest{
public:
    Test_RngTargetRanking()
        : UnitTest("PokemonFRLG::RngTargetSearch - Ranking")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        auto target = [](uint32_t state, uint64_t advances, double probability){
            RngTarget ret{state, {0, 40000, SeedButton::A, BlackoutButton::None}, advances, {}, {}, probability};
            return ret;
        };
        //  About 60 advances per second, after a 40 s seed time: A takes 207 s and B 249 s,
        //  but A is shiny only half the time. D takes 57 s, but is shiny one time in 10.
        std::vector<RngTarget> targets{
            target(0xA, 10000, 0.5),
            target(0xB, 12500, 1),
            target(0xC, 100, 0),
            target(0xD, 1000, 0.1),
        };
        rank_rng_targets(targets);
        std::string order;
        for (const RngTarget& item : targets){
            order += hex(item.state);
        }
        if (order != "BADC"){
            return "Expected order BADC, got " + order;
        }
        return true;
    }
};

}


void add_tests_RngTargetSearch(UnitTestDatabase& database){
    for (const TenLinesGroup& group : groups()){
        database.add<Test_RngTargetMethod1>(group);
        database.add<Test_RngTargetSearch>(group);
    }
    database.add<Test_RngTargetIvSearch>();
    database.add<Test_RngTargetCandidates>();
    database.add<Test_RngTargetRanking>();
}


}
}
}

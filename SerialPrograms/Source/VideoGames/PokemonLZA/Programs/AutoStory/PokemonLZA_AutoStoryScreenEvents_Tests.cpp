/*  PokemonLZA AutoStory - Screen Events Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonLZA_AutoStoryPlan.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStory_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

namespace{

struct PromptAndControl{
    bool interact_prompt = false;
    bool can_move = false;

    bool operator==(const PromptAndControl&) const = default;
};

std::string to_text(const PromptAndControl& events){
    return std::format("A prompt {}, party bar {}", events.interact_prompt ? "yes" : "no", events.can_move ? "yes" : "no");
}

PromptAndControl observed(const std::string& path){
    PlzaScreenObserver observer;
    const ScreenEvents events = observer.read(ImageRGB32(path));
    return PromptAndControl{
        .interact_prompt = events.has(ScreenEvent::INTERACT_PROMPT),
        .can_move = events.has(ScreenEvent::PLAYER_CAN_MOVE),
    };
}

PromptAndControl as_named(std::string_view name){
    return PromptAndControl{
        .interact_prompt = name.find("interact_prompt") != std::string_view::npos,
        .can_move = name.find("player_can_move") != std::string_view::npos,
    };
}


struct FrameCase{
    std::string name;
    struct Input{
        std::string_view path;  //  In CommandLineTests/PokemonLZA/.
    };
    Input input;
    struct Expectation{
        std::optional<bool> interact_prompt = std::nullopt;
        std::optional<bool> can_move = std::nullopt;
        std::optional<bool> battle = std::nullopt;
        std::optional<bool> dialog = std::nullopt;
    };
    Expectation expectation;
};

const FrameCase FRAME_CASES[] = {
    {
        //  No D-pad arrows; the empty slot frames show the bar is up.
        .name = "the elevator's A prompt over the party bar of one",
        .input = {.path = "AutoStoryScreenEvents/za2_m3v_s1.png"},
        .expectation = {.interact_prompt = true, .can_move = true},
    },
    {
        //  01.02: no Pokemon, no party bar.
        .name = "the A Talk prompt on the Italian game",
        .input = {.path = "AutoStoryScreenEvents/it_0102_parla_talk_prompt.png"},
        .expectation = {.interact_prompt = true, .can_move = false},
    },
    {
        .name = "the party bar of one Pokemon: five empty slot frames, no D-pad arrows",
        .input = {.path = "AutoStoryScreenEvents/de_0205_lobby_start_party1.png"},
        .expectation = {.interact_prompt = false, .can_move = true},
    },
    {
        .name = "the party bar of four over a sunlit road",
        .input = {.path = "AutoStoryScreenEvents/it_0308_zone1_road_no_battle.png"},
        .expectation = {.interact_prompt = false, .can_move = true},
    },
    {
        .name = "a battle with the player not locked on: the panel, no move hints",
        .input = {.path = "AutoStoryScreenEvents/it_0302b_battle_no_lock_on.png"},
        .expectation = {.battle = true},
    },
    {
        .name = "a wild battle at the zone's pillar, not locked on",
        .input = {.path = "AutoStoryScreenEvents/it_0308b_zone_pillar_wild_battle.png"},
        .expectation = {.battle = true},
    },
    {
        .name = "the Tepig battle's start over light paving stone in Spanish, not locked on",
        .input = {.path = "AutoStoryScreenEvents/es_0302b_tepig_battle_start.png"},
        .expectation = {.battle = true},
    },
    {
        .name = "the Tepig battle with the lead at 9 HP over light paving stone in Spanish",
        .input = {.path = "AutoStoryScreenEvents/es_0302b_tepig_battle_low_hp.png"},
        .expectation = {.battle = true},
    },
    {
        .name = "Taunie's 03.02b battle frame 1 in German, before move hints or stat markers",
        .input = {.path = "AutoStoryScreenEvents/de_0302b_battle_frame1_no_hint.png"},
        .expectation = {.battle = true},
    },
    {
        .name = "Taunie's 03.02b battle facing archway in German, no move hints or stat markers",
        .input = {.path = "AutoStoryScreenEvents/de_0302b_battle_archway_no_hint.png"},
        .expectation = {.battle = true},
    },
    {
        .name = "no battle at the zone's pillar with the lead's panel up",
        .input = {.path = "AutoStoryScreenEvents/it_0308b_zone_pillar_no_battle.png"},
        .expectation = {.battle = false},
    },
    {
        //  Ends 03.09's walk to the lab gate.
        .name = "Ryon's five-species dialog at the lab gate on the Italian game",
        .input = {.path = "AutoStoryScreenEvents/it_0309_lab_gate_ryon_dialog.png"},
        .expectation = {.interact_prompt = false, .dialog = true},
    },
};

UnitTestResult check_frame(const FrameCase& test){
    const std::string path = std::format("{}PokemonLZA/{}", UNIT_TEST_RESOURCE_PATH(), test.input.path);
    PlzaScreenObserver observer;
    const ScreenEvents events = observer.read(ImageRGB32(path));
    const std::pair<std::string_view, std::pair<ScreenEvent, std::optional<bool>>> checks[] = {
        {"A prompt", {ScreenEvent::INTERACT_PROMPT, test.expectation.interact_prompt}},
        {"party bar", {ScreenEvent::PLAYER_CAN_MOVE, test.expectation.can_move}},
        {"battle", {ScreenEvent::BATTLE_STARTED, test.expectation.battle}},
        {"dialog", {ScreenEvent::DIALOG_BOX, test.expectation.dialog}},
    };
    for (const auto& [label, check] : checks){
        if (!check.second){
            continue;
        }
        const bool got = events.has(check.first);
        if (got != *check.second){
            return std::format(
                "{}: expected {} {}, got {}", test.input.path, label,
                *check.second ? "yes" : "no", got ? "yes" : "no"
            );
        }
    }
    return true;
}

struct FolderCase{
    std::string name;
    struct Input{
        std::string_view folder;    //  In CommandLineTests/PokemonLZA/.
    };
    Input input;
    struct Expectation{
        size_t frames = 0;
    };
    Expectation expectation;
};

const FolderCase FOLDER_CASES[] = {
    {
        .name = "the catches to the zone's start",
        .input = {.folder = "AutoStoryDetectors/CatchesToZone1Start"},
        .expectation = {.frames = 8},
    },
};

UnitTestResult check_folder(const FolderCase& test){
    const Filesystem::Path folder(std::format("{}PokemonLZA/{}/", UNIT_TEST_RESOURCE_PATH(), test.input.folder));
    if (!Filesystem::is_directory(folder)){
        return std::format("{} is not a folder.", folder.string());
    }
    size_t frames = 0;
    std::string failures;
    for (const Filesystem::Path& path : Filesystem::list_directory(folder)){
        const std::string name = path.filename().string();
        if (!name.ends_with(".jpg") && !name.ends_with(".png")){
            continue;
        }
        frames++;
        const PromptAndControl got = observed(path.string());
        if (got != as_named(name)){
            failures += std::format(" {}: {};", name, to_text(got));
        }
    }
    if (frames != test.expectation.frames){
        return std::format("Expected {} frames in {}, found {}.", test.expectation.frames, folder.string(), frames);
    }
    if (!failures.empty()){
        return std::format("Wrong on:{}", failures);
    }
    return true;
}

using Pokemon::StoryCheckpoint;

std::optional<size_t> index_of(const std::vector<StoryCheckpoint>& rows, std::string_view id){
    for (size_t index = 0; index < rows.size(); index++){
        if (rows[index].id == id){
            return index;
        }
    }
    return std::nullopt;
}

//  A checkpoint id, or "stop" for start_position_unknown.
std::string run_start_id(
    const std::vector<StoryCheckpoint>& rows, const std::vector<CheckpointStart>& starts,
    std::string_view asked, const StartSignature& read
){
    const std::optional<size_t> asked_index = index_of(rows, asked);
    if (!asked_index){
        return std::format("no checkpoint {}", asked);
    }
    const std::optional<size_t> from = checkpoint_at(rows, starts, *asked_index, read);
    return from ? rows[*from].id : std::string("stop");
}


struct BuiltinCase{
    std::string name;
    struct Input{
        std::string asked;
        StartSignature read;
    };
    Input input;
    struct Expectation{
        std::string from;   //  A checkpoint id, or "stop".
    };
    Expectation expectation;
};

const BuiltinCase BUILTIN_CASES[] = {
    {
        //  Mable's talk autosave in her office (03.11) read as 03.06's start.
        .name = "asked 03.08, Mable's office with 5 in the party: stop",
        .input = {.asked = "03.08", .read = {.talk_prompt = true, .wild_zone = false, .party = 5}},
        .expectation = {.from = "stop"},
    },
    {
        //  The roof before Station Front.
        .name = "asked 03.04, the party and prompt of its start but not its minimap: stop",
        .input = {.asked = "03.04", .read = {.talk_prompt = false, .wild_zone = false, .party = 1}},
        .expectation = {.from = "stop"},
    },
    {
        //  A save on the street by the zone wall read as in a wild zone.
        .name = "asked 03.08, the street with five in the party: stop",
        .input = {.asked = "03.08", .read = {.talk_prompt = false, .wild_zone = true, .party = 5}},
        .expectation = {.from = "stop"},
    },
    {
        .name = "asked 03.09, the fifth catch's autosave in the zone: from 03.08b",
        .input = {.asked = "03.09", .read = {.talk_prompt = false, .wild_zone = true, .party = 5}},
        .expectation = {.from = "03.08b"},
    },
    {
        //  03.08b's menu save outside the far gate read the lead's panel as a
        //  wild zone; the map pin read (401, 225).
        .name = "asked 03.09, 03.08b's menu save on the street by its map pin: from 03.09",
        .input = {.asked = "03.09", .read = {
            .talk_prompt = false, .wild_zone = wild_zone_from_map(MapPoint{.x = 401, .y = 225}), .party = 5,
        }},
        .expectation = {.from = "03.09"},
    },
    {
        .name = "asked 03.06, Station Front's minimap: from 03.04",
        .input = {.asked = "03.06", .read = {.talk_prompt = false, .wild_zone = false, .party = 1, .minimap = "03.04-start"}},
        .expectation = {.from = "03.04"},
    },
};

UnitTestResult check_builtin(const BuiltinCase& test){
    const StoryData& data = builtin_story_data();
    const std::string from = run_start_id(data.checkpoints, data.starts, test.input.asked, test.input.read);
    if (from != test.expectation.from){
        return std::format("expected {}, got {}", test.expectation.from, from);
    }
    return true;
}


//  A made-up table, for the rules no built-in plan shows.
struct TableCase{
    std::string name;
    struct Input{
        std::vector<CheckpointStart> starts;
        std::string asked;
        StartSignature read;
    };
    Input input;
    struct Expectation{
        std::string from;   //  A checkpoint id, or "stop".
    };
    Expectation expectation;
};

const StartSignature AT_COUNTER{.talk_prompt = true, .wild_zone = false};
const StartSignature IN_ZONE{.wild_zone = true};

const TableCase TABLE_CASES[] = {
    {
        .name = "two earlier starts match the signed asked's screen: stop",
        .input = {
            .starts = {
                {.checkpoint = "A", .signature = {.wild_zone = false}},
                {.checkpoint = "B", .signature = AT_COUNTER},
                {.checkpoint = "C", .signature = IN_ZONE},
            },
            .asked = "C", .read = AT_COUNTER,
        },
        .expectation = {.from = "stop"},
    },
    {
        .name = "a later start matches, the asked one does not: stop",
        .input = {
            .starts = {{.checkpoint = "B", .signature = IN_ZONE}, {.checkpoint = "C", .signature = AT_COUNTER}},
            .asked = "B", .read = AT_COUNTER,
        },
        .expectation = {.from = "stop"},
    },
    {
        .name = "the asked start matches, an earlier one too: as asked",
        .input = {
            .starts = {{.checkpoint = "A", .signature = AT_COUNTER}, {.checkpoint = "B", .signature = {.talk_prompt = true}}},
            .asked = "B", .read = AT_COUNTER,
        },
        .expectation = {.from = "B"},
    },
    {
        .name = "a reading sharing no field with the asked start: stop",
        .input = {.starts = {{.checkpoint = "B", .signature = {.party = 1}}}, .asked = "B", .read = AT_COUNTER},
        .expectation = {.from = "stop"},
    },
};

std::vector<StoryCheckpoint> table_rows(){
    std::vector<StoryCheckpoint> rows;
    for (std::string_view id : {"A", "B", "C"}){
        StoryCheckpoint row;
        row.id = std::string(id);
        row.title = "Test";
        rows.push_back(std::move(row));
    }
    return rows;
}

UnitTestResult check_table(const TableCase& test){
    const std::string from = run_start_id(table_rows(), test.input.starts, test.input.asked, test.input.read);
    if (from != test.expectation.from){
        return std::format("expected {}, got {}", test.expectation.from, from);
    }
    return true;
}


//  Frames from PokemonLZA/AutoStoryStarts/<file>, with the map pin read there.
struct StartFrameCase{
    std::string name;
    struct Input{
        std::string file;
        std::string asked;
        std::optional<MapPoint> map_pin;
    };
    Input input;
    struct Expectation{
        std::string from;   //  A checkpoint id, or "stop".
    };
    Expectation expectation;
};

const StartFrameCase START_FRAME_CASES[] = {
    {
        //  The lead's panel at the bottom left.
        .name = "Taunie's rocks in Wild Zone 1: 03.08",
        .input = {.file = "Italian_zone1_rocks.png", .asked = "03.08", .map_pin = MapPoint{.x = 476, .y = 224}},
        .expectation = {.from = "03.08"},
    },
    {
        //  The lead's panel up.
        .name = "the street outside the far gate, facing the marker: 03.09",
        .input = {.file = "German_0309_start_facing_marker.png", .asked = "03.09", .map_pin = MapPoint{.x = 401, .y = 225}},
        .expectation = {.from = "03.09"},
    },
    //  Record Start Minimaps' frames of a Spanish new-game run (console B,
    //  2026-11-18): the frames their minimap references were made from.
    {
        .name = "03.03's courtyard: 03.03",
        .input = {.file = "Spanish_0303_start.png", .asked = "03.03", .map_pin = std::nullopt},
        .expectation = {.from = "03.03"},
    },
    {
        .name = "03.06's street outside the Center: 03.06",
        .input = {.file = "Spanish_0305_start.png", .asked = "03.06", .map_pin = std::nullopt},
        .expectation = {.from = "03.06"},
    },
    {
        //  No checkpoint starts there.
        .name = "the Center counter, asked 03.08: stop",
        .input = {.file = "Spanish_0306_start.png", .asked = "03.08", .map_pin = std::nullopt},
        .expectation = {.from = "stop"},
    },
    {
        .name = "03.07's street before the zone gate: 03.07",
        .input = {.file = "Spanish_0307_start.png", .asked = "03.07", .map_pin = std::nullopt},
        .expectation = {.from = "03.07"},
    },
};

UnitTestResult check_start_frame(const StartFrameCase& test){
    const std::string path = std::format("{}PokemonLZA/AutoStoryStarts/{}", UNIT_TEST_RESOURCE_PATH(), test.input.file);
    if (!Filesystem::exists(Filesystem::Path(path))){
        return UnitTestResult(UnitTestResult::SKIPPED, std::format("{} is not in the test images.", test.input.file));
    }
    StartSignature read = read_start_screen(ImageRGB32(path));
    if (read.wild_zone){
        return std::format("{}: read {}: the screen alone must not decide wild_zone", test.input.file, to_string(read));
    }
    read.wild_zone = wild_zone_from_map(test.input.map_pin);
    const StoryData& data = builtin_story_data();
    const std::string from = run_start_id(data.checkpoints, data.starts, test.input.asked, read);
    if (from != test.expectation.from){
        return std::format("{}: read {}: expected {}, got {}", test.input.file, to_string(read), test.expectation.from, from);
    }
    return true;
}


template <typename Case, UnitTestResult (*check)(const Case&)>
class CaseTest : public UnitTest{
public:
    CaseTest(std::string_view prefix, const Case& test_case)
        : UnitTest(std::format("{}{}", prefix, test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }
private:
    const Case& m_case;
};

} // namespace


void add_tests_AutoStoryScreenEvents(UnitTestDatabase& database){
    for (const FrameCase& test : FRAME_CASES){
        database.add<CaseTest<FrameCase, check_frame>>("PokemonLZA::AutoStoryScreenEvents - console frame: ", test);
    }
    for (const FolderCase& test : FOLDER_CASES){
        database.add<CaseTest<FolderCase, check_folder>>("PokemonLZA::AutoStoryScreenEvents - console frames: ", test);
    }
    for (const BuiltinCase& test : BUILTIN_CASES){
        database.add<CaseTest<BuiltinCase, check_builtin>>("PokemonLZA::AutoStoryStarts built-in - ", test);
    }
    for (const TableCase& test : TABLE_CASES){
        database.add<CaseTest<TableCase, check_table>>("PokemonLZA::AutoStoryStarts table - ", test);
    }
    for (const StartFrameCase& test : START_FRAME_CASES){
        database.add<CaseTest<StartFrameCase, check_start_frame>>("PokemonLZA::AutoStoryStarts frame - ", test);
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

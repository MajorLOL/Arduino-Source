/*  PokemonLZA - Detectors Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Synthetic frames and console frames (1920x1080 JPEG or PNG) from
 *  CommandLineTests/PokemonLZA/AutoStoryDetectors/: scene sequences,
 *  tutorial cards, caught-mark name plates, and Zone 1 map pins.
 *
 */

#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_BattleHudDetectors.h"
#include "VideoGames/PokemonLZA/Inference/Map/PokemonLZA_MapPinReader.h"
#include "PokemonLZA_AutosaveDetector.h"
#include "PokemonLZA_CaughtMarkReader.h"
#include "PokemonLZA_ControlsHintDetector.h"
#include "PokemonLZA_Inference_Tests.h"
#include "PokemonLZA_MissionCompleteDetector.h"
#include "PokemonLZA_PartyBarDetector.h"
#include "PokemonLZA_StoryDialogDetector.h"
#include "PokemonLZA_TutorialCardDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr size_t WIDTH = 1920;
constexpr size_t HEIGHT = 1080;
constexpr uint32_t WHITE = 0xffffffff;

void fill_box(size_t min_x, size_t min_y, size_t max_x, size_t max_y, uint32_t color, ImageRGB32& image){
    for (size_t y = min_y; y < max_y; y++){
        for (size_t x = min_x; x < max_x; x++){
            image.pixel(x, y) = color;
        }
    }
}

void fill_box(const ImagePixelBox& box, uint32_t color, ImageRGB32& image){
    fill_box(box.min_x, box.min_y, box.max_x, box.max_y, color, image);
}

ImageRGB32 solid_frame(uint32_t color){
    ImageRGB32 image(WIDTH, HEIGHT);
    image.fill(color);
    return image;
}

template <typename Case, UnitTestResult (*check)(const Case&, Logger&)>
class DetectorCaseTest : public UnitTest{
public:
    DetectorCaseTest(std::string_view prefix, const Case& test)
        : UnitTest(std::format("{}{}", prefix, test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return check(m_test, logger);
    }

private:
    const Case& m_test;
};


//  --- Story detectors (autosave, controls hint, advance arrow, mission complete, party bar) ---

struct DetectorReads{
    std::optional<bool> autosave = {};
    std::optional<bool> controls_hint = {};
    std::optional<bool> advance_arrow = {};
    std::optional<bool> mission_complete = {};
    std::optional<bool> party_bar = {};
    std::optional<bool> trainer_pokeballs = {};
};

struct DetectorField{
    std::string_view name;
    std::optional<bool> DetectorReads::* read;
};

const DetectorField DETECTOR_FIELDS[] = {
    {"autosave", &DetectorReads::autosave},
    {"controls hint", &DetectorReads::controls_hint},
    {"advance arrow", &DetectorReads::advance_arrow},
    {"mission complete", &DetectorReads::mission_complete},
    {"party bar", &DetectorReads::party_bar},
    {"trainer pokeballs", &DetectorReads::trainer_pokeballs},
};

DetectorReads every_detector(DetectorReads reads){
    for (const DetectorField& field : DETECTOR_FIELDS){
        reads.*field.read = (reads.*field.read).value_or(false);
    }
    return reads;
}

std::string firing(const DetectorReads& got){
    std::string text;
    for (const DetectorField& field : DETECTOR_FIELDS){
        if ((got.*field.read).value_or(false)){
            text += std::format(", {}", field.name);
        }
    }
    return text;
}

std::string mismatches(const DetectorReads& got, const DetectorReads& want){
    std::string text;
    for (const DetectorField& field : DETECTOR_FIELDS){
        const std::optional<bool>& expected = want.*field.read;
        if (expected && got.*field.read != expected){
            text += std::format(" {} (expected {})", field.name, *expected ? "yes" : "no");
        }
    }
    return text;
}

struct StoryDetectors{
    AutosaveDetector autosave;
    ControlsHintDetector hint;
    AdvanceArrowDetector arrow;
    MissionCompleteDetector complete;
    PartyBarDetector party_bar;
    TrainerPokeballsDetector trainer_pokeballs;

    DetectorReads read(const ImageViewRGB32& frame){
        return DetectorReads{
            .autosave = autosave.detect(frame),
            .controls_hint = hint.detect(frame), .advance_arrow = arrow.detect(frame),
            .mission_complete = complete.detect(frame),
            .party_bar = party_bar.detect(frame),
            .trainer_pokeballs = trainer_pokeballs.detect(frame),
        };
    }
};

ImageRGB32 autosave_badge_frame(uint32_t background, uint32_t glow){
    ImageRGB32 image = solid_frame(background);
    fill_box(/*min_x=*/1655, /*min_y=*/73, /*max_x=*/1667, /*max_y=*/104, glow, image);
    fill_box(/*min_x=*/1659, /*min_y=*/73, /*max_x=*/1663, /*max_y=*/104, WHITE, image);
    return image;
}

ImageRGB32 autosave_badge_by_mint_banner_frame(){
    ImageRGB32 image = autosave_badge_frame(/*background=*/0xff101418, /*glow=*/0xff28c8e6);
    fill_box(/*min_x=*/1600, /*min_y=*/60, /*max_x=*/1720, /*max_y=*/64, /*color=*/0xff60cb93, image);
    fill_box(/*min_x=*/1600, /*min_y=*/112, /*max_x=*/1720, /*max_y=*/116, /*color=*/0xff60cb93, image);
    return image;
}

ImageRGB32 trainer_pokeball_frame(size_t slot, uint32_t top_color, uint32_t bottom_color){
    ImageRGB32 image = solid_frame(/*color=*/0xff4a4846);
    const size_t cx = 858 + 41 * slot;
    fill_box(cx - 7, /*min_y=*/60, cx + 7, /*max_y=*/66, top_color, image);
    fill_box(cx - 7, /*min_y=*/72, cx + 7, /*max_y=*/80, bottom_color, image);
    return image;
}

struct SyntheticFrameCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
    };
    Input input;
    struct Expectation{
        DetectorReads reads = {};
    };
    Expectation expectation;
};

const SyntheticFrameCase SYNTHETIC_FRAME_CASES[] = {
    {
        .name = "an empty image",
        .input = {.frame = []{ return ImageRGB32(); }},
        .expectation = {.reads = every_detector({})},
    },
    {
        .name = "the autosave badge with a dim glow over a dark wall",
        .input = {.frame = []{ return autosave_badge_frame(0xff252722, 0xff1c4655); }},
        .expectation = {.reads = {.autosave = true}},
    },
    {
        .name = "the autosave badge's white stroke with no glow",
        .input = {.frame = []{ return autosave_badge_frame(0xff252722, 0xff252722); }},
        .expectation = {.reads = {.autosave = false}},
    },
    {
        .name = "the autosave badge with mint banner bands above and below",
        .input = {.frame = autosave_badge_by_mint_banner_frame},
        .expectation = {.reads = {.autosave = true}},
    },
    {
        .name = "a white stroke with a green sign's glow",
        .input = {.frame = []{ return autosave_badge_frame(0xff252722, 0xff3cf06e); }},
        .expectation = {.reads = {.autosave = false}},
    },
    {
        .name = "trainer pokeball strip with live ball in slot 0",
        .input = {.frame = []{ return trainer_pokeball_frame(0, 0xfff86268, 0xfff5f0ee); }},
        .expectation = {.reads = {.trainer_pokeballs = true}},
    },
    {
        .name = "trainer pokeball strip with live ball in slot 1",
        .input = {.frame = []{ return trainer_pokeball_frame(1, 0xfff86268, 0xfff5f0ee); }},
        .expectation = {.reads = {.trainer_pokeballs = true}},
    },
    {
        .name = "trainer pokeball strip with grey fainted ball only",
        .input = {.frame = []{ return trainer_pokeball_frame(0, 0xff706868, 0xff807878); }},
        .expectation = {.reads = {.trainer_pokeballs = false}},
    },
};

UnitTestResult check_synthetic_frame(const SyntheticFrameCase& test, Logger& logger){
    StoryDetectors detectors;
    const DetectorReads got = detectors.read(test.input.frame());
    logger.log(std::format("{} reads{}", test.name, firing(got)));
    const std::string wrong = mismatches(got, test.expectation.reads);
    if (!wrong.empty()){
        return std::format("Wrong on {}:{}", test.name, wrong);
    }
    return true;
}

std::optional<int> frame_number(std::string_view name, size_t offset, size_t digits){
    if (name.size() < offset + digits){
        return std::nullopt;
    }
    const std::string_view text = name.substr(offset, digits);
    int number = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
    if (error != std::errc() || end != text.data() + text.size()){
        return std::nullopt;
    }
    return number;
}

DetectorReads as_named(std::string_view name){
    const auto has = [name](std::string_view reading){
        return name.find(reading) != std::string_view::npos;
    };
    return DetectorReads{
        .autosave = has("+autosave"),
        .controls_hint = has("+controls_hint"), .advance_arrow = has("+advance_arrow"),
        .mission_complete = has("+mission_complete"),
    };
}

std::optional<DetectorReads> opening(std::string_view){
    return DetectorReads{};
}

std::optional<DetectorReads> arrival_to_ad(std::string_view name){
    const std::optional<int> index = frame_number(name, /*offset=*/0, /*digits=*/4);
    if (!index){
        return std::nullopt;
    }
    return DetectorReads{.controls_hint = *index <= 6, .advance_arrow = *index == 40};
}

std::optional<DetectorReads> arrival_to_ad_subtitles(std::string_view name){
    const std::optional<int> index = frame_number(name, /*offset=*/0, /*digits=*/4);
    if (!index){
        return std::nullopt;
    }
    const bool box_arrow = *index == 44 || *index == 49 || *index == 53 || *index == 78;
    return DetectorReads{
        .controls_hint = *index <= 6,
        .advance_arrow = box_arrow || (*index >= 93 && *index <= 115),
    };
}

std::optional<DetectorReads> arrival_to_mission_banner(std::string_view name){
    if (!frame_number(name, /*offset=*/0, /*digits=*/4)){
        return std::nullopt;
    }
    return DetectorReads{
        .controls_hint = name.find("+controls_hint") != std::string_view::npos,
        .advance_arrow = name.find("+advance_arrow") != std::string_view::npos,
    };
}

std::optional<DetectorReads> arrival_past_mission_banner(std::string_view name){
    DetectorReads want = as_named(name);
    if (name.starts_with("0197-")){
        want.autosave = true;
    }
    return want;
}

std::optional<DetectorReads> mission_complete_to_next_banner(std::string_view name){
    DetectorReads want = as_named(name);
    want.mission_complete = name.starts_with("0045-") || name.starts_with("0046-");
    return want;
}

struct FrameFolderCase{
    std::string name;
    struct Input{
        std::string_view folder;
    };
    Input input;
    struct Expectation{
        size_t frames = 0;
        std::function<std::optional<DetectorReads>(std::string_view name)> reads = {};
    };
    Expectation expectation;
};

const FrameFolderCase FRAME_FOLDER_CASES[] = {
    {.name = "the opening cutscene", .input = {.folder = "Opening"}, .expectation = {.frames = 56, .reads = opening}},
    {.name = "the arrival to the ad", .input = {.folder = "ArrivalToAd"}, .expectation = {.frames = 89, .reads = arrival_to_ad}},
    {.name = "the arrival to the ad's subtitles", .input = {.folder = "ArrivalToAdSubtitles"}, .expectation = {.frames = 115, .reads = arrival_to_ad_subtitles}},
    {.name = "the arrival to the mission banner", .input = {.folder = "ArrivalToMissionBanner"}, .expectation = {.frames = 181, .reads = arrival_to_mission_banner}},
    {.name = "the arrival past the mission banner", .input = {.folder = "ArrivalPastMissionBanner"}, .expectation = {.frames = 221, .reads = arrival_past_mission_banner}},
    {.name = "the mission complete card to the next mission's banner", .input = {.folder = "MissionCompleteToNextBanner"}, .expectation = {.frames = 139, .reads = mission_complete_to_next_banner}},
    {.name = "01.01's banner and controls hint in English and Italian", .input = {.folder = "Arrival0101Languages"}, .expectation = {.frames = 7, .reads = as_named}},
};

UnitTestResult check_frame_folder(const FrameFolderCase& test, Logger& logger){
    const Filesystem::Path folder(std::format("{}PokemonLZA/AutoStoryDetectors/{}/", UNIT_TEST_RESOURCE_PATH(), test.input.folder));
    if (!Filesystem::is_directory(folder)){
        return std::format("{} is not a folder.", folder.string());
    }
    StoryDetectors detectors;
    size_t frames = 0;
    std::string failures;
    for (const Filesystem::Path& path : Filesystem::list_directory(folder)){
        const std::string name = path.filename().string();
        if (!name.ends_with(".jpg") && !name.ends_with(".png")){
            continue;
        }
        const std::optional<DetectorReads> want = test.expectation.reads(name);
        if (!want){
            continue;
        }
        const DetectorReads got = detectors.read(ImageRGB32(path.string()));
        logger.log(std::format("{} reads{}", name, firing(got)));
        const std::string wrong = mismatches(got, every_detector(*want));
        if (!wrong.empty()){
            failures += std::format(" {}:{};", name, wrong);
        }
        frames++;
    }
    if (frames != test.expectation.frames){
        return std::format("Expected {} frames in {}, found {}.", test.expectation.frames, folder.string(), frames);
    }
    if (!failures.empty()){
        return std::format("Wrong on:{}", failures);
    }
    return true;
}


//  --- Tutorial card detector ---

struct TutorialCardFrameCase{
    std::string name;
    struct Input{
        std::string_view path;
    };
    Input input;
    struct Expectation{
        bool card = false;
    };
    Expectation expectation;
};

const TutorialCardFrameCase TUTORIAL_CARD_FRAME_CASES[] = {
    {
        .name = "the Map & Missions card over the map app (Italian)",
        .input = {.path = "AutoStoryScreenEvents/it_0302b_map_missions_card.png"},
        .expectation = {.card = true},
    },
    {
        .name = "the Save card over the main menu (Italian)",
        .input = {.path = "AutoStoryScreenEvents/it_menu_save_tutorial_card.png"},
        .expectation = {.card = true},
    },
    {
        .name = "no card on the main menu",
        .input = {.path = "MainMenuDetector/it_menu_after_save_tutorial_True.png"},
        .expectation = {.card = false},
    },
};

UnitTestResult check_tutorial_card_frame(const TutorialCardFrameCase& test, Logger&){
    const std::string path = std::format("{}PokemonLZA/{}", UNIT_TEST_RESOURCE_PATH(), test.input.path);
    const bool card = tutorial_card_shows(ImageRGB32(path));
    if (card != test.expectation.card){
        return std::format("{}: expected card {}, got {}", test.input.path, test.expectation.card ? "yes" : "no", card ? "yes" : "no");
    }
    return true;
}


//  --- Caught mark reader ---

const ImagePixelBox PLATE_BAR_BOX(950, 515, 1058, 533);
const ImagePixelBox PLAYER_BAR_BOX(990, 560, 1053, 572);
const ImagePixelBox GENDER_MARK_BOX(1020, 430, 1044, 454);
const ImagePixelBox GENDER_MARK_HOLE_BOX(1028, 438, 1036, 446);
const ImagePixelBox BALL_MARK_TOP_BOX(1056, 430, 1080, 442);
const ImagePixelBox BALL_MARK_BOTTOM_BOX(1056, 442, 1080, 454);

constexpr uint32_t PLATE_BAR_GREEN = 0xff2dc673;
constexpr uint32_t GENDER_ORANGE = 0xffff7644;
constexpr uint32_t BALL_RED = 0xffff6b6e;
constexpr uint32_t BRIGHT_GRASS = 0xff44a050;
constexpr uint32_t DARK_GROUND = 0xff303030;

struct PlateDrawing{
    bool plate_bar = false;
    bool player_bar = false;
    bool gender_mark = false;
    bool ball_mark = false;
};

ImageRGB32 plate_frame(uint32_t ground, PlateDrawing drawing){
    ImageRGB32 image = solid_frame(ground);
    if (drawing.plate_bar){
        fill_box(PLATE_BAR_BOX, PLATE_BAR_GREEN, image);
    }
    if (drawing.player_bar){
        fill_box(PLAYER_BAR_BOX, PLATE_BAR_GREEN, image);
    }
    if (drawing.gender_mark){
        fill_box(GENDER_MARK_BOX, GENDER_ORANGE, image);
        fill_box(GENDER_MARK_HOLE_BOX, WHITE, image);
    }
    if (drawing.ball_mark){
        fill_box(BALL_MARK_TOP_BOX, BALL_RED, image);
        fill_box(BALL_MARK_BOTTOM_BOX, WHITE, image);
    }
    return image;
}

struct PlateFrameCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
    };
    Input input;
    struct Expectation{
        std::optional<bool> caught;
    };
    Expectation expectation;
};

const PlateFrameCase PLATE_FRAME_CASES[] = {
    {
        .name = "the plate's bar and the player's HP bar on bright grass",
        .input = {.frame = []{ return plate_frame(BRIGHT_GRASS, {.plate_bar = true, .player_bar = true}); }},
        .expectation = {.caught = false},
    },
    {
        .name = "a plate with the gender mark only (Scatterbug, not caught)",
        .input = {.frame = []{ return plate_frame(DARK_GROUND, {.plate_bar = true, .gender_mark = true}); }},
        .expectation = {.caught = false},
    },
    {
        .name = "a plate with the gender mark and the Poke Ball (Bunnelby, caught)",
        .input = {.frame = []{ return plate_frame(DARK_GROUND, {.plate_bar = true, .gender_mark = true, .ball_mark = true}); }},
        .expectation = {.caught = true},
    },
    {
        .name = "the player's HP bar alone",
        .input = {.frame = []{ return plate_frame(DARK_GROUND, {.player_bar = true}); }},
        .expectation = {.caught = std::nullopt},
    },
    {
        .name = "bright grass alone",
        .input = {.frame = []{ return plate_frame(BRIGHT_GRASS, {}); }},
        .expectation = {.caught = std::nullopt},
    },
};

std::string describe_caught(const std::optional<bool>& caught){
    if (!caught){
        return "no plate";
    }
    return *caught ? "a plate with the caught mark" : "a plate with no caught mark";
}

UnitTestResult check_plate_frame(const PlateFrameCase& test, Logger&){
    const std::optional<bool> caught = lock_target_caught(test.input.frame());
    if (caught != test.expectation.caught){
        return std::format("Expected {}, read {}.", describe_caught(test.expectation.caught), describe_caught(caught));
    }
    return true;
}


//  --- Map pin reader ---

constexpr uint32_t ZONE_GREEN = 0xff98efad;
constexpr uint32_t ARROW_BLUE = 0xff00a0ff;
constexpr uint32_t CONE_BLUE = 0xff5ec9ea;

struct Square{
    double x;
    double y;
    size_t side;
    uint32_t colour;
};

ImageRGB32 map_with(std::initializer_list<Square> squares){
    ImageRGB32 image = solid_frame(ZONE_GREEN);
    for (const Square& square : squares){
        const size_t cx = (size_t)(square.x * 2.4);
        const size_t cy = (size_t)(square.y * 2.4);
        fill_box(cx - square.side / 2, cy - square.side / 2, cx + square.side / 2, cy + square.side / 2, square.colour, image);
    }
    return image;
}

ImageRGB32 zone1_map_frame(std::string_view name){
    return ImageRGB32(std::format("{}PokemonLZA/AutoStoryDetectors/Zone1Map/{}", UNIT_TEST_RESOURCE_PATH(), name));
}

std::string describe_pin(const std::optional<MapPoint>& pin){
    return pin ? std::format("a pin at ({:.1f}, {:.1f})", pin->x, pin->y) : std::string("no pin");
}

struct MapPinCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
    };
    Input input;
    struct Expectation{
        std::optional<MapPoint> pin;
    };
    Expectation expectation;
};

const MapPinCase MAP_PIN_CASES[] = {
    {
        .name = "the arrow with the camera's cone beside it",
        .input = {.frame = []{ return map_with({
            {.x = 350, .y = 215, .side = 40, .colour = CONE_BLUE},
            {.x = 370, .y = 230, .side = 24, .colour = ARROW_BLUE},
        }); }},
        .expectation = {.pin = MapPoint{.x = 370, .y = 230}},
    },
    {
        .name = "the cone only",
        .input = {.frame = []{ return map_with({{.x = 350, .y = 215, .side = 40, .colour = CONE_BLUE}}); }},
        .expectation = {.pin = std::nullopt},
    },
    {
        .name = "the arrow's blue outside the zone's window",
        .input = {.frame = []{ return map_with({{.x = 100, .y = 100, .side = 24, .colour = ARROW_BLUE}}); }},
        .expectation = {.pin = std::nullopt},
    },
    {
        .name = "a speck of the arrow's blue",
        .input = {.frame = []{ return map_with({{.x = 400, .y = 230, .side = 6, .colour = ARROW_BLUE}}); }},
        .expectation = {.pin = std::nullopt},
    },
    {
        .name = "console: stuck at the zone's north fence",
        .input = {.frame = []{ return zone1_map_frame("at_north_fence.png"); }},
        .expectation = {.pin = MapPoint{.x = 358.9, .y = 227.3}},
    },
    {
        .name = "console: the patrol's stop 10, the arrow partly under the zone's badge",
        .input = {.frame = []{ return zone1_map_frame("after_try6.png"); }},
        .expectation = {.pin = MapPoint{.x = 415.4, .y = 230.4}},
    },
};

UnitTestResult check_map_pin(const MapPinCase& test, Logger& logger){
    const std::optional<MapPoint> pin = read_map_pin(test.input.frame());
    logger.log(describe_pin(pin));
    const std::optional<MapPoint>& want = test.expectation.pin;
    const bool same = pin.has_value() == want.has_value()
        && (!pin || (std::abs(pin->x - want->x) <= 2 && std::abs(pin->y - want->y) <= 2));
    if (!same){
        return std::format("Expected {}, read {}.", describe_pin(want), describe_pin(pin));
    }
    return true;
}

} // namespace


void add_tests_Detectors(UnitTestDatabase& database){
    for (const SyntheticFrameCase& test : SYNTHETIC_FRAME_CASES){
        database.add<DetectorCaseTest<SyntheticFrameCase, check_synthetic_frame>>("PokemonLZA::AutoStoryDetectors - synthetic frame: ", test);
    }
    for (const FrameFolderCase& test : FRAME_FOLDER_CASES){
        database.add<DetectorCaseTest<FrameFolderCase, check_frame_folder>>("PokemonLZA::AutoStoryDetectors - console frames: ", test);
    }
    for (const TutorialCardFrameCase& test : TUTORIAL_CARD_FRAME_CASES){
        database.add<DetectorCaseTest<TutorialCardFrameCase, check_tutorial_card_frame>>("PokemonLZA::AutoStoryDetectors - tutorial card frame: ", test);
    }
    for (const PlateFrameCase& test : PLATE_FRAME_CASES){
        database.add<DetectorCaseTest<PlateFrameCase, check_plate_frame>>("PokemonLZA::AutoStory - Caught mark reader: ", test);
    }
    for (const MapPinCase& test : MAP_PIN_CASES){
        database.add<DetectorCaseTest<MapPinCase, check_map_pin>>("PokemonLZA::AutoStory - Map pin reader: ", test);
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  PokemonLZA - Minimap match Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_Inference_Tests.h"
#include "PokemonLZA_MinimapMatch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  As the reference files were made.
constexpr size_t SCREEN_WIDTH = 1280;
constexpr size_t SCREEN_HEIGHT = 720;
constexpr double BOX_X = 52;
constexpr double BOX_Y = 55;
constexpr double BOX_SIZE = 132;
constexpr size_t CELLS = 16;

constexpr std::string_view START_REFERENCE = "03.04-start";

ReferenceCells reference_cells(std::string_view name){
    const std::string wanted = std::format("plza-minimap-{}", name);
    for (const auto& [file, text] : autostory_reference_json()){
        if (file == wanted){
            const JsonValue json = parse_json(text);
            const JsonObject& grid = json.to_object_throw(file).get_object_throw("grid", file);
            return read_cell_grid(grid, CELLS, CELLS, file);
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No reference {}.", wanted));
}

//  `cells` painted into the minimap's square on black, scaled by `brightness`, moved
//  `shift_cols` left and `shift_rows` up (edge cells repeat).
ImageRGB32 painted_screen(const ReferenceCells& cells, double brightness, int shift_cols, int shift_rows){
    ImageRGB32 screen(SCREEN_WIDTH, SCREEN_HEIGHT);
    screen.fill((uint32_t)COLOR_BLACK);
    const auto channel = [&](double value){
        return (uint8_t)std::min(255.0, std::round(value * brightness));
    };
    const double cell_size = BOX_SIZE / CELLS;
    for (size_t y = (size_t)BOX_Y; y < (size_t)(BOX_Y + BOX_SIZE); y++){
        for (size_t x = (size_t)BOX_X; x < (size_t)(BOX_X + BOX_SIZE); x++){
            const int col = std::clamp((int)(((double)x + 0.5 - BOX_X) / cell_size) + shift_cols, 0, (int)CELLS - 1);
            const int row = std::clamp((int)(((double)y + 0.5 - BOX_Y) / cell_size) + shift_rows, 0, (int)CELLS - 1);
            const FloatPixel& pixel = cells[(size_t)row * CELLS + (size_t)col];
            screen.pixel(x, y) = combine_rgb(channel(pixel.r), channel(pixel.g), channel(pixel.b));
        }
    }
    return screen;
}


struct MatchCase{
    std::string name;
    struct Input{
        std::string reference = std::string(START_REFERENCE);
        double brightness = 1.0;
        int shift_cols = 0;
        int shift_rows = 0;
    };
    Input input;
    struct Expectation{
        std::optional<std::string> match;
    };
    Expectation expectation;
};

const std::vector<MatchCase> MATCH_CASES{
    {
        .name = "the start as stored: the start",
        .input = {},
        .expectation = {.match = "03.04-start"},
    },
    {
        .name = "the start 20% darker: the start",
        .input = {.brightness = 0.8},
        .expectation = {.match = "03.04-start"},
    },
    {
        .name = "the start 10% brighter, its whites clipped: the start",
        .input = {.brightness = 1.1},
        .expectation = {.match = "03.04-start"},
    },
    {
        .name = "the map moved one cell (8 pixels) east: no match",
        .input = {.shift_cols = 1},
        .expectation = {.match = std::nullopt},
    },
    //  The other checkpoint starts' minimaps (Spanish, console B,
    //  2026-11-18), each at least 49 from every other: each is its own
    //  place, not 03.04's Station Front.
    {
        .name = "03.03's courtyard 20% darker: 03.03's start",
        .input = {.reference = "03.03-start", .brightness = 0.8},
        .expectation = {.match = "03.03-start"},
    },
    {
        .name = "03.06's street outside the Center 20% darker: 03.06's start",
        .input = {.reference = "03.06-start", .brightness = 0.8},
        .expectation = {.match = "03.06-start"},
    },
    {
        .name = "03.07's street before the zone gate 20% darker: 03.07's start",
        .input = {.reference = "03.07-start", .brightness = 0.8},
        .expectation = {.match = "03.07-start"},
    },
};

UnitTestResult check_match(const MatchCase& test){
    const ImageRGB32 screen = painted_screen(
        reference_cells(test.input.reference), test.input.brightness, test.input.shift_cols, test.input.shift_rows
    );
    const std::optional<std::string> match = matching_minimap(screen);
    if (match != test.expectation.match){
        return std::format(
            "expected {}, got {}",
            test.expectation.match.value_or("no match"), match.value_or("no match")
        );
    }
    return true;
}


class MatchTest : public UnitTest{
public:
    explicit MatchTest(MatchCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryMinimapMatch - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_match(m_case);
    }
private:
    MatchCase m_case;
};

} // namespace


void add_tests_MinimapMatch(UnitTestDatabase& database){
    for (const MatchCase& test : MATCH_CASES){
        database.add<MatchTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  PokemonLZA - Dialog Detectors Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonLZA_DialogChoiceBox.h"
#include "PokemonLZA_Inference_Tests.h"
#include "PokemonLZA_SelectionArrowDetector.h"
#include "PokemonLZA_StoryDialogDetector.h"

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

ImageRGB32 solid_frame(uint32_t color){
    ImageRGB32 image(WIDTH, HEIGHT);
    image.fill(color);
    return image;
}

ImageRGB32 console_frame(std::string_view path){
    return ImageRGB32(std::format("{}PokemonLZA/{}", UNIT_TEST_RESOURCE_PATH(), path));
}

//  `path` with the arrow's area (x 1395-1470, y 935-1030) painted white: text not done.
ImageRGB32 console_frame_without_arrow(std::string_view path){
    ImageRGB32 image = console_frame(path);
    fill_box(/*min_x=*/1395, /*min_y=*/935, /*max_x=*/1470, /*max_y=*/1030, WHITE, image);
    return image;
}


struct DialogCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
        WhiteDialogPart white;
    };
    Input input;
    struct Expectation{
        bool dialog;
    };
    Expectation expectation;
};

const DialogCase DIALOG_CASES[] = {
    {
        .name = "a black frame, box and arrow",
        .input = {.frame = []{ return solid_frame(0xff000000); }, .white = WhiteDialogPart::BOX_AND_ARROW},
        .expectation = {.dialog = false},
    },
    {
        .name = "a white box on the console, box and arrow",
        .input = {
            .frame = []{ return console_frame("FlatWhiteDialogDetector/mac_restaurant_True.png"); },
            .white = WhiteDialogPart::BOX_AND_ARROW,
        },
        .expectation = {.dialog = true},
    },
    {
        .name = "a white box on the console in French, box and arrow",
        .input = {
            .frame = []{ return console_frame("FlatWhiteDialogDetector/french_fossil_1_True.png"); },
            .white = WhiteDialogPart::BOX_AND_ARROW,
        },
        .expectation = {.dialog = true},
    },
    {
        .name = "a white box on the console with its arrow painted out, box and arrow",
        .input = {
            .frame = []{ return console_frame_without_arrow("FlatWhiteDialogDetector/mac_restaurant_True.png"); },
            .white = WhiteDialogPart::BOX_AND_ARROW,
        },
        .expectation = {.dialog = false},
    },
    {
        .name = "a white box on the console with its arrow painted out, box only",
        .input = {
            .frame = []{ return console_frame_without_arrow("FlatWhiteDialogDetector/mac_restaurant_True.png"); },
            .white = WhiteDialogPart::BOX_ONLY,
        },
        .expectation = {.dialog = true},
    },
    {
        .name = "a blue box on the console",
        .input = {
            .frame = []{ return console_frame("BlueDialogDetector/mac_cannotFastTravel_True.png"); },
            .white = WhiteDialogPart::BOX_AND_ARROW,
        },
        .expectation = {.dialog = true},
    },
};

UnitTestResult check_dialog(const DialogCase& test){
    StoryDialogDetector detector({.white = test.input.white});
    const bool dialog = detector.detect(test.input.frame());
    if (dialog != test.expectation.dialog){
        return std::format("Expected {}, read {}.", test.expectation.dialog ? "a dialogue box" : "none", dialog ? "a dialogue box" : "none");
    }
    return true;
}

constexpr uint32_t PAVEMENT = 0xffc8c3be;
constexpr uint32_t BLUE_WALL = 0xff18375c;
constexpr uint32_t PANEL = 0xff243c6c;
constexpr uint32_t DARK_TEXT = 0xff1e1e28;

//  The panel grows left to fit its longest row.
struct MenuWidth{
    size_t panel_left = 1408;
    size_t text_end = 1540;
};

//  The menus whose rows read on the console in Japanese and Chinese.
constexpr MenuWidth NARROW_MENU{};
//  Italian 01.02's wide panel with rows as short as "Si": no text past x 1400.
constexpr MenuWidth WIDE_MENU_SHORT_ROWS{.panel_left = 1343, .text_end = 1400};

//  `rows` rows with the reader's geometry, the text in strokes one column in three:
//  dark on the highlighted row, white on the others.
ImageRGB32 choice_menu_frame(
    uint32_t background, size_t rows, std::initializer_list<size_t> highlighted,
    MenuWidth width = NARROW_MENU
){
    ImageRGB32 image(WIDTH, HEIGHT);
    image.fill(background);
    fill_box(width.panel_left, 808 - 80 * rows - 4, /*max_x=*/1728, /*max_y=*/808, PANEL, image);
    for (size_t row = 0; row < rows; row++){
        const size_t from_bottom = rows - 1 - row;
        const size_t top = 732 - 80 * from_bottom;
        bool lit = false;
        for (const size_t pick : highlighted){
            lit = lit || pick == row;
        }
        if (lit){
            fill_box(width.panel_left + 10, top, /*max_x=*/1715, top + 68, WHITE, image);
        }
        for (size_t x = width.panel_left + 35; x < width.text_end; x += 3){
            fill_box(x, top + 16, x + 1, top + 52, lit ? DARK_TEXT : WHITE, image);
        }
    }
    return image;
}

std::string describe(const std::optional<DialogChoiceRows>& rows){
    if (!rows){
        return "no choice menu";
    }
    return std::format("row {} of {}", rows->selected, rows->count);
}

bool same(const std::optional<DialogChoiceRows>& a, const std::optional<DialogChoiceRows>& b){
    if (!a || !b){
        return a.has_value() == b.has_value();
    }
    return a->count == b->count && a->selected == b->selected;
}


struct SyntheticCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
    };
    Input input;
    struct Expectation{
        std::optional<DialogChoiceRows> rows;   //  None: no choice menu read.
    };
    Expectation expectation;
};

const SyntheticCase SYNTHETIC_CASES[] = {
    {
        .name = "three rows on a blue wall, the middle highlighted",
        .input = {.frame = []{ return choice_menu_frame(BLUE_WALL, /*rows=*/3, /*highlighted=*/{1}); }},
        .expectation = {.rows = DialogChoiceRows{.count = 3, .selected = 1}},
    },
    {
        .name = "three short rows on a wide panel, on a blue wall, the bottom highlighted",
        .input = {.frame = []{ return choice_menu_frame(BLUE_WALL, /*rows=*/3, /*highlighted=*/{2}, WIDE_MENU_SHORT_ROWS); }},
        .expectation = {.rows = DialogChoiceRows{.count = 3, .selected = 2}},
    },
    {
        .name = "one highlighted row is no menu",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, /*rows=*/1, /*highlighted=*/{0}); }},
        .expectation = {.rows = std::nullopt},
    },
    {
        .name = "three rows, none highlighted",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, /*rows=*/3, /*highlighted=*/{}); }},
        .expectation = {.rows = std::nullopt},
    },
    {
        .name = "three rows, two highlighted",
        .input = {.frame = []{ return choice_menu_frame(PAVEMENT, /*rows=*/3, /*highlighted=*/{0, 2}); }},
        .expectation = {.rows = std::nullopt},
    },
};

UnitTestResult check_synthetic(const SyntheticCase& test){
    const std::optional<DialogChoiceRows> rows = read_dialog_choice_rows(test.input.frame());
    if (!same(rows, test.expectation.rows)){
        return std::format("Expected {}, read {}.", describe(test.expectation.rows), describe(rows));
    }
    return true;
}


//  Console frames, the menus as they open (top row highlighted).
struct FrameCase{
    std::string name;
    struct Input{
        std::string_view file;  //  In CommandLineTests/PokemonLZA/.
    };
    Input input;
    struct Expectation{
        std::optional<DialogChoiceRows> rows;   //  None: no choice menu read.
        //  Whether dialog_choice_box() finds the selection arrow; none: not checked.
        std::optional<bool> choice_arrow = std::nullopt;
    };
    Expectation expectation;
};

const FrameCase FRAME_CASES[] = {
    {
        .name = "the tourist's yes or yes (2 rows, Traditional Chinese)",
        .input = {.file = "AutoStoryDetectors/ArrivalToAd/0065-text_box+choice_box.jpg"},
        .expectation = {.rows = DialogChoiceRows{.count = 2, .selected = 0}, .choice_arrow = true},
    },
    {
        .name = "the bag's \"not really\" (2 rows, Traditional Chinese)",
        .input = {.file = "AutoStoryDetectors/ArrivalToMissionBanner/0173-text_box.jpg"},
        .expectation = {.rows = DialogChoiceRows{.count = 2, .selected = 0}},
    },
    {
        .name = "the Research Lab elevator's floors (3 rows, Japanese)",
        .input = {.file = "AutoStoryDialogChoices/elevator_3rows_row0.png"},
        .expectation = {.rows = DialogChoiceRows{.count = 3, .selected = 0}},
    },
    {
        .name = "Villy's \"I'll get it back\" or \"battle\" at 01.02 (2 rows, Italian: a wider panel)",
        .input = {.file = "AutoStoryDialogChoices/it_0102_villy_2rows_row0.png"},
        .expectation = {.rows = DialogChoiceRows{.count = 2, .selected = 0}, .choice_arrow = true},
    },
    {
        .name = "Villy's \"I'll get it back\" or \"battle\" at 01.02 (2 rows, French: the arrow left of x 1256)",
        .input = {.file = "AutoStoryDialogChoices/fr_0102_villy_2rows_row0.png"},
        .expectation = {.rows = DialogChoiceRows{.count = 2, .selected = 0}, .choice_arrow = true},
    },
    {
        .name = "light pavement, no menu",
        .input = {.file = "AutoStoryDetectors/ArrivalToAd/0038-nothing+autosave.jpg"},
        .expectation = {.rows = std::nullopt},
    },
};

UnitTestResult check_frame(const FrameCase& test){
    const Filesystem::Path path(std::format("{}PokemonLZA/{}", UNIT_TEST_RESOURCE_PATH(), test.input.file));
    if (!Filesystem::exists(path)){
        return std::format("{} is missing.", path.string());
    }
    const ImageRGB32 frame(path.string());
    const std::optional<DialogChoiceRows> rows = read_dialog_choice_rows(frame);
    if (!same(rows, test.expectation.rows)){
        return std::format("{}: expected {}, read {}.", test.input.file, describe(test.expectation.rows), describe(rows));
    }
    if (!test.expectation.choice_arrow){
        return true;
    }
    SelectionArrowDetector arrow(COLOR_YELLOW, /*overlay=*/nullptr, SelectionArrowType::RIGHT, dialog_choice_box());
    const bool choice_arrow = arrow.detect(frame);
    if (choice_arrow != *test.expectation.choice_arrow){
        return std::format("{}: expected the choice arrow {}.", test.input.file, *test.expectation.choice_arrow ? "on" : "off");
    }
    return true;
}


template <typename Case, UnitTestResult (*check)(const Case&)>
class DialogCaseTest : public UnitTest{
public:
    DialogCaseTest(std::string_view prefix, const Case& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - {}: {}", prefix, test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_test);
    }

private:
    const Case& m_test;
};

} // namespace


void add_tests_DialogDetectors(UnitTestDatabase& database){
    for (const DialogCase& test : DIALOG_CASES){
        database.add<DialogCaseTest<DialogCase, check_dialog>>("StoryDialogDetector", test);
    }
    for (const SyntheticCase& test : SYNTHETIC_CASES){
        database.add<DialogCaseTest<SyntheticCase, check_synthetic>>("Dialog choice rows, synthetic", test);
    }
    for (const FrameCase& test : FRAME_CASES){
        database.add<DialogCaseTest<FrameCase, check_frame>>("Dialog choice rows, console frame", test);
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  System Keyboard Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Tests/ImageUnitTest.h"
#include "NintendoSwitch_SystemKeyboardDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

namespace{

//  Key row centres, first column centre and column spacing, as screen fractions.
constexpr double KEY_ROW_Y[] = {0.500, 0.593, 0.681, 0.770, 0.858};
constexpr double KEY_COL_X0 = 0.077;
constexpr double KEY_COL_STEP = 0.075;

//  Right column: Return spans rows 1-2, OK rows 3-4; these are the gaps between.
constexpr double RIGHT_COLUMN_RETURN_TOP = 0.548;
constexpr double RIGHT_COLUMN_OK_TOP = 0.725;

//  From the top of the digit row to the bottom of the bottom row.
const ImageFloatBox KEYS_AREA(0.0, 0.43, 1.0, 0.49);

//  The bottom row's page-key labels. The dark theme draws the current page's label
//  cyan like the cursor (about 0, 230, 210), so the cursor read skips this band.
const ImageFloatBox PAGE_LABELS(0.0, 0.829, 0.49, 0.065);

bool in_box(double x, double y, const ImageFloatBox& box){
    return x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height;
}

//  The border pulses between about (50, 240, 220) and (0, 185, 210); dark theme about
//  (115, 245, 250). The OK button's blue (40, 90, 250) is not green enough.
bool is_cursor_pixel(uint32_t pixel){
    const int red = (pixel >> 16) & 0xff;
    const int green = (pixel >> 8) & 0xff;
    const int blue = pixel & 0xff;
    return red < 130 && green > 160 && blue > 170 && green - red > 60;
}

uint8_t nearest_row(double y){
    uint8_t row = 0;
    for (uint8_t r = 1; r < 5; r++){
        if (std::abs(KEY_ROW_Y[r] - y) < std::abs(KEY_ROW_Y[row] - y)){
            row = r;
        }
    }
    return row;
}

//  The key whose cursor border is centred at (x, y); nothing off the keyboard.
std::optional<SystemKeyboardKey> key_at(double x, double y){
    const long col = std::lround((x - KEY_COL_X0) / KEY_COL_STEP);
    if (col < 0 || col > 11){
        return std::nullopt;
    }
    if (col < 11){
        return SystemKeyboardKey{nearest_row(y), (uint8_t)col};
    }
    if (y < RIGHT_COLUMN_RETURN_TOP){
        return SystemKeyboardKey{0, 11};
    }
    if (y < RIGHT_COLUMN_OK_TOP){
        return SystemKeyboardKey{1, 11};
    }
    return SystemKeyboardKey{3, 11};
}

//  Points left of the first column or right of the right column belong to the edge keys.
SystemKeyboardKey key_cell_at(double x, double y){
    const double col = std::clamp((x - KEY_COL_X0) / KEY_COL_STEP, 0.0, 11.0);
    return *key_at(KEY_COL_X0 + col * KEY_COL_STEP, y);
}

struct KeyCellPixels{
    double area = 0;
    double count = 0;
    double sum_x = 0;
    double sum_y = 0;
};

//  The border covers about a fifth of its cell; more cyan is a solid cyan key
//  (the dark theme's OK with text: 60% of its cell).
constexpr double MAX_BORDER_SHARE = 0.35;

using KeyCells = std::array<std::array<KeyCellPixels, 12>, 5>;
KeyCells cyan_pixels_by_key(const ImageViewRGB32& keys){
    KeyCells cells{};
    for (size_t y = 0; y < keys.height(); y++){
        const double screen_y = KEYS_AREA.y + ((double)y + 0.5) / keys.height() * KEYS_AREA.height;
        for (size_t x = 0; x < keys.width(); x++){
            const double screen_x = KEYS_AREA.x + ((double)x + 0.5) / keys.width() * KEYS_AREA.width;
            if (in_box(screen_x, screen_y, PAGE_LABELS)){
                continue;
            }
            const SystemKeyboardKey key = key_cell_at(screen_x, screen_y);
            KeyCellPixels& cell = cells[key.row][key.col];
            cell.area++;
            if (is_cursor_pixel(keys.pixel(x, y))){
                cell.count++;
                cell.sum_x += (double)x;
                cell.sum_y += (double)y;
            }
        }
    }
    return cells;
}

}  // namespace


std::optional<SystemKeyboardKey> read_system_keyboard_cursor(const ImageViewRGB32& screen){
    //  Neighbouring keys overlap the border: take the centroid of cyan pixels outside
    //  solid cyan keys and map it to the nearest key.
    const ImageViewRGB32 keys = extract_box_reference(screen, KEYS_AREA);
    const KeyCells cells = cyan_pixels_by_key(keys);
    double count = 0, sum_x = 0, sum_y = 0;
    for (const auto& row : cells){
        for (const KeyCellPixels& cell : row){
            if (cell.count > MAX_BORDER_SHARE * cell.area){
                continue;
            }
            count += cell.count;
            sum_x += cell.sum_x;
            sum_y += cell.sum_y;
        }
    }
    //  The border is about 0.18% of the screen; a few stray pixels are not a cursor.
    if (count < 0.0005 * screen.width() * screen.height()){
        return std::nullopt;
    }
    return key_at(
        KEYS_AREA.x + sum_x / count / keys.width() * KEYS_AREA.width,
        KEYS_AREA.y + sum_y / count / keys.height() * KEYS_AREA.height
    );
}

size_t system_keyboard_text_pixels(const ImageViewRGB32& screen){
    //  The field, above its underline, which is white in some games.
    const ImageViewRGB32 field = extract_box_reference(screen, ImageFloatBox(0.03, 0.17, 0.87, 0.105));
    size_t count = 0;
    for (size_t y = 0; y < field.height(); y++){
        for (size_t x = 0; x < field.width(); x++){
            const uint32_t pixel = field.pixel(x, y);
            if (((pixel >> 16) & 0xff) > 190 && ((pixel >> 8) & 0xff) > 190 && (pixel & 0xff) > 190){
                count++;
            }
        }
    }
    return count;
}



namespace{

class Test_SystemKeyboardDetector : public ImageUnitTest{
public:
    Test_SystemKeyboardDetector(const std::string& image, SystemKeyboardKey cursor, bool has_text)
        : ImageUnitTest(std::format("NintendoSwitch::SystemKeyboardDetector - {}", image), {image})
        , m_cursor(cursor)
        , m_has_text(has_text)
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const ImageRGB32& image = images[0];
        const std::optional<SystemKeyboardKey> cursor = read_system_keyboard_cursor(image);
        if (cursor != m_cursor){
            return cursor
                ? std::format("Read the cursor at ({}, {}).", cursor->row, cursor->col)
                : std::format("Read no cursor.");
        }
        //  An empty field has no white text pixels; one typed character has many.
        const size_t text_pixels = system_keyboard_text_pixels(image);
        if ((text_pixels >= 20) != m_has_text){
            return std::format("system_keyboard_text_pixels() returned {}.", text_pixels);
        }
        return true;
    }

private:
    SystemKeyboardKey m_cursor;
    bool m_has_text;
};

std::string test_image_path(std::string_view image){
    return std::format("NintendoSwitch/SystemKeyboardDetector/{}", image);
}

void add_cursor_tests(UnitTestDatabase& database){
    //  Legends: Z-A, English and Spanish prompts; T, / and ! are dark.
    const std::pair<std::string_view, SystemKeyboardKey> cursors[] = {
        {"PLZA-Cursor-T.png",           {1, 4}},
        {"PLZA-Cursor-Slash.png",       {1, 10}},
        {"PLZA-Cursor-Exclamation.png", {3, 10}},
        {"PLZA-Cursor-0.png",           {0, 9}},
        {"PLZA-Cursor-Minus.png",       {0, 10}},
        {"PLZA-Cursor-Backspace.png",   {0, 11}},
        {"PLZA-Cursor-Return.png",      {1, 11}},
        {"PLZA-Cursor-OK.png",          {3, 11}},
        {"PLZA-Cursor-Globe.png",       {4, 0}},
        {"PLZA-Cursor-NTilde.png",      {4, 5}},
        {"PLZA-Cursor-Space.png",       {4, 8}},
    };
    for (const auto& [image, cursor] : cursors){
        database.add<Test_SystemKeyboardDetector>(test_image_path(image), cursor, false);
    }
}

void add_planned_tests(UnitTestDatabase& database){
    const SystemKeyboardKey key_1{0, 0};
    //  AZERTY needs a console whose system language uses it.
    struct Planned{
        std::string_view image;
        SystemKeyboardKey cursor;
        bool has_text;
    };
    const Planned planned[] = {
        {"AZERTY-Empty.png",    key_1,          false},
    };
    for (const Planned& test : planned){
        const std::string image = test_image_path(test.image);
        if (Filesystem::exists(std::format("{}{}", UNIT_TEST_RESOURCE_PATH(), image))){
            database.add<Test_SystemKeyboardDetector>(image, test.cursor, test.has_text);
        }
    }
}

}  // namespace


void add_tests_SystemKeyboardDetector(UnitTestDatabase& database){
    const SystemKeyboardKey key_1{0, 0};
    //  The OK button is blue with text in the field and grey without.
    database.add<Test_SystemKeyboardDetector>(test_image_path("BDSP-PlayerName-Empty.png"), key_1, false);
    database.add<Test_SystemKeyboardDetector>(test_image_path("BDSP-FriendName-Typed.png"), key_1, true);
    const std::string_view languages[] = {
        "ChineseSimplified", "ChineseTraditional", "English", "French", "German",
        "Italian", "Japanese", "Korean", "Spanish", "SpanishLATAM",
    };
    for (std::string_view language : languages){
        database.add<Test_SystemKeyboardDetector>(test_image_path(std::format("PLZA-{}-Empty.png", language)), key_1, false);
    }
    //  The console's dark theme (Basic Black), console B 2026-11-18: the cyan
    //  ABC label must not pull the cursor read off "1".
    database.add<Test_SystemKeyboardDetector>(test_image_path("PLZA-English-Empty-Dark.png"), key_1, false);
    //  "K" typed: OK turns solid cyan, which is not the cursor (on "8").
    database.add<Test_SystemKeyboardDetector>(test_image_path("PLZA-English-Typed-Dark.png"), SystemKeyboardKey{0, 7}, true);
    add_cursor_tests(database);
    add_planned_tests(database);
}



}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

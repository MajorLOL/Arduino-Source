/*  Party Bar Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include <format>
#include <string>
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "Tests/ImageUnitTest.h"
#include "PokemonLZA_PartyBarDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

namespace{

//  The slot's HP bar: 52 of 1920 wide from x = 86 + 76.4 * i, rows 1028-1031.
constexpr double SLOT_X = 0.045;
constexpr double SLOT_PITCH = 0.0398;
constexpr double BAR_WIDTH = 0.026;
constexpr double BAR_Y = 0.9519;
constexpr double BAR_HEIGHT = 0.0037;
//  The card between the icon and the bar: rows 1021-1025.
constexpr double CARD_Y = 0.9454;
constexpr double CARD_HEIGHT = 0.0046;

//  The trough is the card darkened (~0.46x scene + dark charcoal floor).
//  Keep 0.65 when no HP-colored pixels show (prevents empty slots over shadow
//  edges from false-positiving), and relax to 0.72 only when an HP segment
//  already shows (catches weakened yellow/red bars over dark asphalt, card ~73).
constexpr double TROUGH_BRIGHTNESS_RATIO = 0.65;
constexpr double WEAKENED_TROUGH_BRIGHTNESS_RATIO = 0.72;
constexpr double HP_COLOR_SHARE_MIN = 0.05;
//  Bars read 0.74+ (a weakened yellow/red bar over dark asphalt); slots without, 0.16 at most.
constexpr double BAR_SHARE_MIN = 0.5;

bool is_hp_color(Color color){
    const uint32_t r = color.red();
    const uint32_t g = color.green();
    const uint32_t b = color.blue();
    const bool green = g > 140 && g > r + 60 && g > b + 60;
    const bool yellow = r > 170 && g > 150 && b < 100;
    const bool red = r > 170 && g < 110 && b < 110;
    return green || yellow || red;
}

double brightness(Color color){
    return (color.red() + color.green() + color.blue()) / 3.0;
}

//  On 1920x1080: empty slot i's frame at x 78 + 76 * i, y 954, 66 px square, edges
//  2 px wide; the last slot's at x 458-523, y 954-1019.
constexpr double LAST_SLOT_X = 458;
constexpr double SLOT_FRAME_Y = 954;
constexpr double SLOT_FRAME_SIZE = 66;
constexpr double FRAME_LINE = 2;
//  3 px strips 2 px off the line on either side, read along each edge's middle,
//  8 px clear of the rounded corners.
constexpr double FRAME_STRIP = 3;
constexpr double FRAME_GAP = 2;
constexpr double FRAME_CORNER = 8;

//  The line is the scene under a light veil, the inside darkened: on the six frames
//  with the last slot empty the line is lighter than inside by 21-60; on 686 cutscene
//  frames (no bar) by 2 at most.
constexpr double FRAME_INSIDE_MARGIN = 10;
constexpr double FRAME_OUTSIDE_MARGIN = 0;

ImageFloatBox pixel_box(double x, double y, double width, double height){
    return {x / 1920, y / 1080, width / 1920, height / 1080};
}

double box_brightness(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return image_average(extract_box_reference(screen, box)).sum() / 3;
}

//  The 2 px line and the strips inside and outside it.
struct FrameEdge{
    ImageFloatBox line;
    ImageFloatBox inside;
    ImageFloatBox outside;
};

std::array<FrameEdge, 4> last_slot_edges(){
    const double left = LAST_SLOT_X;
    const double right = LAST_SLOT_X + SLOT_FRAME_SIZE - FRAME_LINE;
    const double top = SLOT_FRAME_Y;
    const double bottom = SLOT_FRAME_Y + SLOT_FRAME_SIZE - FRAME_LINE;
    const double span = SLOT_FRAME_SIZE - 2 * FRAME_CORNER;
    const double step = FRAME_LINE + FRAME_GAP;
    return {
        FrameEdge{
            .line = pixel_box(left + FRAME_CORNER, top, span, FRAME_LINE),
            .inside = pixel_box(left + FRAME_CORNER, top + step, span, FRAME_STRIP),
            .outside = pixel_box(left + FRAME_CORNER, top - FRAME_GAP - FRAME_STRIP, span, FRAME_STRIP),
        },
        FrameEdge{
            .line = pixel_box(left + FRAME_CORNER, bottom, span, FRAME_LINE),
            .inside = pixel_box(left + FRAME_CORNER, bottom - FRAME_GAP - FRAME_STRIP, span, FRAME_STRIP),
            .outside = pixel_box(left + FRAME_CORNER, bottom + step, span, FRAME_STRIP),
        },
        FrameEdge{
            .line = pixel_box(left, top + FRAME_CORNER, FRAME_LINE, span),
            .inside = pixel_box(left + step, top + FRAME_CORNER, FRAME_STRIP, span),
            .outside = pixel_box(left - FRAME_GAP - FRAME_STRIP, top + FRAME_CORNER, FRAME_STRIP, span),
        },
        FrameEdge{
            .line = pixel_box(right, top + FRAME_CORNER, FRAME_LINE, span),
            .inside = pixel_box(right - FRAME_GAP - FRAME_STRIP, top + FRAME_CORNER, FRAME_STRIP, span),
            .outside = pixel_box(right + step, top + FRAME_CORNER, FRAME_STRIP, span),
        },
    };
}

//  Every edge lighter than the strip inside it and no darker than the one outside.
bool last_slot_empty(const ImageViewRGB32& screen){
    static const std::array<FrameEdge, 4> edges = last_slot_edges();
    for (const FrameEdge& edge : edges){
        const double line = box_brightness(screen, edge.line);
        if (line - box_brightness(screen, edge.inside) < FRAME_INSIDE_MARGIN || line - box_brightness(screen, edge.outside) < FRAME_OUTSIDE_MARGIN){
            return false;
        }
    }
    return true;
}

} // namespace


PartyBarDetector::PartyBarDetector(Color color, VideoOverlay* overlay)
    : m_selection(color, overlay)
    , m_last_slot(pixel_box(LAST_SLOT_X, SLOT_FRAME_Y, SLOT_FRAME_SIZE, SLOT_FRAME_SIZE))
{
    for (size_t i = 0; i < 6; i++){
        const double x = SLOT_X + SLOT_PITCH * i;
        m_bar_boxes[i] = {x, BAR_Y, BAR_WIDTH, BAR_HEIGHT};
        m_card_boxes[i] = {x, CARD_Y, BAR_WIDTH, CARD_HEIGHT};
    }
}

void PartyBarDetector::make_overlays(VideoOverlaySet& items) const{
    m_selection.make_overlays(items);
    items.add(COLOR_GREEN, m_last_slot);
    for (size_t i = 0; i < 6; i++){
        items.add(COLOR_GREEN, m_bar_boxes[i]);
    }
}

bool PartyBarDetector::slot_has_hp_bar(const ImageViewRGB32& screen, size_t index) const{
    const ImageViewRGB32 card = extract_box_reference(screen, m_card_boxes[index]);
    const ImageViewRGB32 bar = extract_box_reference(screen, m_bar_boxes[index]);
    const size_t total = bar.width() * bar.height();
    if (total == 0 || card.width() * card.height() == 0){
        return false;
    }
    const double card_brightness = image_average(card).sum() / 3.0;
    const double strict_trough_max = card_brightness * TROUGH_BRIGHTNESS_RATIO;
    const double relaxed_trough_max = card_brightness * WEAKENED_TROUGH_BRIGHTNESS_RATIO;
    size_t hp_count = 0;
    size_t strict_count = 0;
    size_t relaxed_count = 0;
    for (size_t y = 0; y < bar.height(); y++){
        for (size_t x = 0; x < bar.width(); x++){
            const Color color(bar.pixel(x, y));
            if (is_hp_color(color)){
                hp_count++;
                strict_count++;
                relaxed_count++;
            }else if (brightness(color) < strict_trough_max){
                strict_count++;
                relaxed_count++;
            }else if (brightness(color) < relaxed_trough_max){
                relaxed_count++;
            }
        }
    }
    const bool has_hp = (double)hp_count / (double)total >= HP_COLOR_SHARE_MIN;
    const size_t count = has_hp ? relaxed_count : strict_count;
    return (double)count / (double)total >= BAR_SHARE_MIN;
}

bool PartyBarDetector::detect(const ImageViewRGB32& screen){
    m_filled_slots = 0;
    if (screen.width() == 0 || screen.height() == 0){
        return false;
    }
    //  The marker is read first: it also names the slot sent out.
    if (!m_selection.detect(screen) && !last_slot_empty(screen)){
        return false;
    }
    const uint8_t sent_out = m_selection.dpad_down_idx();
    //  Party slots fill left-to-right; a fainted lead has no green bar
    //  and a darkened card, so the rightmost lit slot gives the party count.
    for (size_t i = 0; i < 6; i++){
        if (i == sent_out || slot_has_hp_bar(screen, i)){
            m_filled_slots = static_cast<int>(i) + 1;
        }
    }
    return true;
}

namespace{

int party_bar_filled_slots(const ImageViewRGB32& screen){
    PartyBarDetector detector;
    detector.detect(screen);
    return detector.filled_slots();
}

struct PartyBarCase{
    std::string name;
    struct Input{
        std::string image;  //  In CommandLineTests/PokemonLZA/.
    };
    Input input;
    struct Expectation{
        int filled_slots;
    };
    Expectation expectation;
};

const PartyBarCase PARTY_BAR_CASES[] = {
    {
        .name = "five filled, slot 0 selected (D-pad up), the sixth empty",
        .input = {.image = "PartyBarDetector/party5_slot0_selected.png"},
        .expectation = {.filled_slots = 5},
    },
    {
        .name = "five filled, slot 0's partner sent out (Poke Ball, no bar), Mareep's bar red",
        .input = {.image = "PartyBarDetector/party5_slot0_sent_out.png"},
        .expectation = {.filled_slots = 5},
    },
    {
        .name = "five filled, slot 0's partner sent out, over bright ground",
        .input = {.image = "PartyBarDetector/party5_b.png"},
        .expectation = {.filled_slots = 5},
    },
    {
        //  No D-pad marker with a party of one: the empty last slot shows the bar.
        .name = "one filled in Hotel Z's lobby, five empty",
        .input = {.image = "PartyBarDetector/de_0205_lobby_start_party1.png"},
        .expectation = {.filled_slots = 1},
    },
    {
        .name = "one filled at the lobby elevator's A prompt",
        .input = {.image = "PartyBarDetector/de_0205_elevator_prompt_steady.png"},
        .expectation = {.filled_slots = 1},
    },
};

class Test_PartyBarDetector : public ImageUnitTest{
public:
    explicit Test_PartyBarDetector(const PartyBarCase& test_case)
        : ImageUnitTest(
            std::format("PokemonLZA::PartyBarDetector - {}", test_case.name),
            {std::format("PokemonLZA/{}", test_case.input.image)}
        )
        , m_case(test_case)
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const int filled = party_bar_filled_slots(images[0]);
        if (filled != m_case.expectation.filled_slots){
            return std::format("Expected {} filled slots, got {}.", m_case.expectation.filled_slots, filled);
        }
        return true;
    }

private:
    const PartyBarCase& m_case;
};

} // namespace


void add_tests_PartyBarDetector(UnitTestDatabase& database){
    for (const PartyBarCase& item : PARTY_BAR_CASES){
        database.add<Test_PartyBarDetector>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

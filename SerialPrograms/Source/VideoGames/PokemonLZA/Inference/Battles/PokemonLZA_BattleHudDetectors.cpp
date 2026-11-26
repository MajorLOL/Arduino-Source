/*  PokemonLZA - Battle HUD detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_PixelShares.h"
#include "PokemonLZA_BattleHudDetectors.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  BattlePanelDetector: level badge blue at x 240-320, y 799-818 on 1080p over half
//  its pixels, and HP bar at x 236-440, y 882-902 over 80% fill or grey trough. The
//  badge is translucent: over grass and paving stone it is as dark as (80, 170, 190),
//  with wider white "Nv." text in Spanish. In battle: badge 0.78-0.88, bar 0.88-0.95.
constexpr double BADGE_SHARE = 0.5;
constexpr double BAR_SHARE = 0.8;

//  An HP bar pixel: its fill (red, yellow or green: channels 90 or more
//  apart), or its grey trough (channels within 25, mean 60 to 140).
bool hp_bar_pixel(Color color){
    const int r = color.red();
    const int g = color.green();
    const int b = color.blue();
    const int spread = std::max({r, g, b}) - std::min({r, g, b});
    const int mean = (r + g + b) / 3;
    return spread >= 90 || (spread <= 25 && mean >= 60 && mean <= 140);
}

//  BattleMarkersDetector: five 16 x 12 slots at x 405-513 (27 apart), y 806 on 1080p.
//  In battle: one slot over 20% triangle blue (measured 0.27-0.44), and 3+ others
//  6-50% white (measured 0.06-0.38 over light paving stone too) with no blue.
constexpr double SLOT_CENTRES_X[] = {405, 432, 459, 486, 513};
constexpr double SLOT_WIDTH = 16;
constexpr double SLOT_HEIGHT = 12;
constexpr double SLOT_TOP = 800;

constexpr double TRIANGLE_SHARE = 0.2;
constexpr double NO_TRIANGLE_SHARE = 0.05;
constexpr double DOT_SHARE_MIN = 0.06;
constexpr double DOT_SHARE_MAX = 0.5;
constexpr size_t DOTS_MIN = 3;
constexpr int DOT_CHANNEL_SPREAD_MAX = 30;

//  The triangle's blue: red 70 or less, green 150 to 210, blue 225 or more.
double triangle_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.red() <= 70 && pixel.green() >= 150 && pixel.green() <= 210 && pixel.blue() >= 225;
    });
}

//  A dot's white: every channel over 200, within 30 of each other. Light paving stone
//  is as bright but yellow (channels 38-45 apart).
double dot_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        const int low = std::min({pixel.red(), pixel.green(), pixel.blue()});
        const int high = std::max({pixel.red(), pixel.green(), pixel.blue()});
        return low > 200 && high - low <= DOT_CHANNEL_SPREAD_MAX;
    });
}

//  TrainerPokeballsDetector: 6 Pokeball slots centered around x = 960 on 1080p
//  (centers 857.5..1062.5, spacing 41). A live ball has a coral-red top half
//  (y 60-66, measured 0.94-0.96) and a bright-white bottom half (y 72-80, measured 1.00).
constexpr size_t TRAINER_BALL_COUNT = 6;
constexpr double TRAINER_BALL_FIRST_X = 857.5;
constexpr double TRAINER_BALL_SPACING = 41.0;
constexpr double TRAINER_BALL_WIDTH = 14.0;
constexpr double TRAINER_BALL_TOP_Y = 60.0;
constexpr double TRAINER_BALL_TOP_H = 6.0;
constexpr double TRAINER_BALL_BOTTOM_Y = 72.0;
constexpr double TRAINER_BALL_BOTTOM_H = 8.0;
constexpr double TRAINER_BALL_SHARE = 0.4;
constexpr int TRAINER_BALL_WHITE_SPREAD_MAX = 40;

double trainer_ball_red_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.red() > 190 && pixel.red() > pixel.green() + 70 && pixel.red() > pixel.blue() + 60;
    });
}

double trainer_ball_white_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        const int low = std::min({pixel.red(), pixel.green(), pixel.blue()});
        const int high = std::max({pixel.red(), pixel.green(), pixel.blue()});
        return low > 200 && high - low <= TRAINER_BALL_WHITE_SPREAD_MAX;
    });
}

} // namespace


BattlePanelDetector::BattlePanelDetector(Color color)
    : m_color(color)
    , m_badge(240.0 / 1920, 799.0 / 1080, 80.0 / 1920, 19.0 / 1080)
    , m_bar(236.0 / 1920, 882.0 / 1080, 204.0 / 1920, 20.0 / 1080)
{}

void BattlePanelDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_badge);
    items.add(m_color, m_bar);
}

bool BattlePanelDetector::detect(const ImageViewRGB32& screen){
    return dim_cyan_share(screen, m_badge) >= BADGE_SHARE && share_of(screen, m_bar, hp_bar_pixel) >= BAR_SHARE;
}


BattleMarkersDetector::BattleMarkersDetector(Color color)
    : m_color(color)
{
    for (double centre : SLOT_CENTRES_X){
        m_slots.emplace_back(
            (centre - SLOT_WIDTH / 2) / 1920, SLOT_TOP / 1080, SLOT_WIDTH / 1920, SLOT_HEIGHT / 1080
        );
    }
}

void BattleMarkersDetector::make_overlays(VideoOverlaySet& items) const{
    for (const ImageFloatBox& slot : m_slots){
        items.add(m_color, slot);
    }
}

bool BattleMarkersDetector::detect(const ImageViewRGB32& screen){
    size_t triangles = 0;
    size_t dots = 0;
    for (const ImageFloatBox& slot : m_slots){
        const double blue = triangle_share(screen, slot);
        if (blue >= TRIANGLE_SHARE){
            triangles++;
            continue;
        }
        const double white = dot_share(screen, slot);
        if (blue < NO_TRIANGLE_SHARE && white >= DOT_SHARE_MIN && white <= DOT_SHARE_MAX){
            dots++;
        }
    }
    return triangles == 1 && dots >= DOTS_MIN;
}


TrainerPokeballsDetector::TrainerPokeballsDetector(Color color)
    : m_color(color)
{
    for (size_t i = 0; i < TRAINER_BALL_COUNT; i++){
        const double x = (TRAINER_BALL_FIRST_X + TRAINER_BALL_SPACING * i - TRAINER_BALL_WIDTH / 2) / 1920;
        const double w = TRAINER_BALL_WIDTH / 1920;
        m_slots.push_back(BallSlot{
            .top = ImageFloatBox(x, TRAINER_BALL_TOP_Y / 1080, w, TRAINER_BALL_TOP_H / 1080),
            .bottom = ImageFloatBox(x, TRAINER_BALL_BOTTOM_Y / 1080, w, TRAINER_BALL_BOTTOM_H / 1080),
        });
    }
}

void TrainerPokeballsDetector::make_overlays(VideoOverlaySet& items) const{
    for (const BallSlot& slot : m_slots){
        items.add(m_color, slot.top);
        items.add(m_color, slot.bottom);
    }
}

bool TrainerPokeballsDetector::detect(const ImageViewRGB32& screen){
    for (const BallSlot& slot : m_slots){
        if (trainer_ball_red_share(screen, slot.top) >= TRAINER_BALL_SHARE &&
            trainer_ball_white_share(screen, slot.bottom) >= TRAINER_BALL_SHARE
        ){
            return true;
        }
    }
    return false;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

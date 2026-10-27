/*  Pokemon FRLG Autostory Scroll Tracking
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <algorithm>
#include <array>
#include "PokemonFRLG_AutoStoryScroll.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace AutoStoryScroll{


namespace{

//  The player: a 16x32 sprite whose feet are on the tile at (112, 80).
//  Some margin for the grass and shadow effects around it.
constexpr int PLAYER_X0 = 104;
constexpr int PLAYER_X1 = 136;
constexpr int PLAYER_Y0 = 52;
constexpr int PLAYER_Y1 = 100;

//  Differences are capped so a sprite or a pop-up over part of the screen
//  cannot outweigh the rest of it.
constexpr int DIFF_CAP = 48;

//  Fewer compared pixels than this: not enough of the two pictures overlap.
constexpr long MIN_SAMPLES = 1500;

//  The most offsets looked at for one frame (frames missed for ~1.5 s).
constexpr int MAX_SPAN = 96;

inline int luminance(uint32_t pixel){
    int r = (int)((pixel >> 16) & 0xff);
    int g = (int)((pixel >> 8) & 0xff);
    int b = (int)(pixel & 0xff);
    return (r * 77 + g * 150 + b * 29) >> 8;
}

}



void sample_screen(
    Screen& out,
    const uint32_t* data, size_t bytes_per_row,
    size_t width, size_t height
){
    if (data == nullptr || width == 0 || height == 0){
        std::fill(out.pixels.begin(), out.pixels.end(), (uint8_t)0);
        return;
    }
    //  Each GBA pixel is a block of several video pixels. Average the 2x2
    //  video pixels around its center.
    int x0[SCREEN_WIDTH];
    int x1[SCREEN_WIDTH];
    int y0[SCREEN_HEIGHT];
    int y1[SCREEN_HEIGHT];
    const double sx = (double)width / SCREEN_WIDTH;
    const double sy = (double)height / SCREEN_HEIGHT;
    const int max_x = (int)width - 1;
    const int max_y = (int)height - 1;
    for (int x = 0; x < SCREEN_WIDTH; x++){
        int a = (int)std::floor((x + 0.5) * sx - 0.5);
        a = std::min(std::max(a, 0), max_x);
        x0[x] = a;
        x1[x] = std::min(a + 1, max_x);
    }
    for (int y = 0; y < SCREEN_HEIGHT; y++){
        int a = (int)std::floor((y + 0.5) * sy - 0.5);
        a = std::min(std::max(a, 0), max_y);
        y0[y] = a;
        y1[y] = std::min(a + 1, max_y);
    }
    const char* base = (const char*)data;
    for (int y = 0; y < SCREEN_HEIGHT; y++){
        const uint32_t* row0 = (const uint32_t*)(base + (size_t)y0[y] * bytes_per_row);
        const uint32_t* row1 = (const uint32_t*)(base + (size_t)y1[y] * bytes_per_row);
        uint8_t* dest = &out.pixels[(size_t)y * SCREEN_WIDTH];
        for (int x = 0; x < SCREEN_WIDTH; x++){
            int sum =
                luminance(row0[x0[x]]) + luminance(row0[x1[x]]) +
                luminance(row1[x0[x]]) + luminance(row1[x1[x]]);
            dest[x] = (uint8_t)((sum + 2) >> 2);
        }
    }
}


double screen_mismatch(const Screen& key, const Screen& now, int dx, int dy){
    const int xs = std::max(0, -dx);
    const int xe = std::min(SCREEN_WIDTH, SCREEN_WIDTH - dx);
    const int ys = std::max(0, -dy);
    const int ye = std::min(SCREEN_HEIGHT, SCREEN_HEIGHT - dy);
    long sum = 0;
    long count = 0;
    for (int y = ys; y < ye; y += 2){
        const int ky = y + dy;
        const bool player_row_now = y >= PLAYER_Y0 && y < PLAYER_Y1;
        const bool player_row_key = ky >= PLAYER_Y0 && ky < PLAYER_Y1;
        const uint8_t* n = &now.pixels[(size_t)y * SCREEN_WIDTH];
        const uint8_t* k = &key.pixels[(size_t)ky * SCREEN_WIDTH];
        for (int x = xs; x < xe; x += 2){
            if (player_row_now && x >= PLAYER_X0 && x < PLAYER_X1){
                continue;
            }
            const int kx = x + dx;
            if (player_row_key && kx >= PLAYER_X0 && kx < PLAYER_X1){
                continue;
            }
            int d = abs((int)n[x] - (int)k[kx]);
            sum += d < DIFF_CAP ? d : DIFF_CAP;
            count++;
        }
    }
    if (count < MIN_SAMPLES){
        return 1e9;
    }
    return (double)sum / (double)count;
}



std::string TileOffset::summary() const{
    char buffer[128];
    snprintf(
        buffer, sizeof(buffer), "(%d, %d) tiles, mismatch %.1f, margin %.1f%s",
        x, y, best, margin, clear ? "" : " - unclear"
    );
    return buffer;
}

TileOffset tile_offset(const Screen& key, const Screen& now, int max_tiles){
    auto in_player = [](int x, int y){
        return x >= PLAYER_X0 && x < PLAYER_X1 && y >= PLAYER_Y0 && y < PLAYER_Y1;
    };

    //  The best offset over the whole screen.
    TileOffset result;
    result.best = 1e18;
    for (int ty = -max_tiles; ty <= max_tiles; ty++){
        for (int tx = -max_tiles; tx <= max_tiles; tx++){
            double m = screen_mismatch(key, now, tx * TILE_PIXELS, ty * TILE_PIXELS);
            if (m < result.best){
                result.best = m;
                result.x = tx;
                result.y = ty;
            }
        }
    }

    //  Against every other offset: over a field of identical tiles (grass,
    //  floor) the picture looks the same a tile further, so the whole screen
    //  says little. Compare only the pixels where the two offsets would show
    //  different things (edges of trees, paths, water, buildings). There, the
    //  right offset matches up to the noise and the wrong one does not.
    const int bx = result.x * TILE_PIXELS;
    const int by = result.y * TILE_PIXELS;
    result.margin = 1e18;
    for (int ty = -max_tiles; ty <= max_tiles; ty++){
        for (int tx = -max_tiles; tx <= max_tiles; tx++){
            if (tx == result.x && ty == result.y){
                continue;
            }
            const int cx = tx * TILE_PIXELS;
            const int cy = ty * TILE_PIXELS;
            long sum_best = 0;
            long sum_other = 0;
            long count = 0;
            for (int y = 0; y < SCREEN_HEIGHT; y += 2){
                const int yb = y + by;
                const int yc = y + cy;
                if (yb < 0 || yb >= SCREEN_HEIGHT || yc < 0 || yc >= SCREEN_HEIGHT){
                    continue;
                }
                for (int x = 0; x < SCREEN_WIDTH; x += 2){
                    const int xb = x + bx;
                    const int xc = x + cx;
                    if (xb < 0 || xb >= SCREEN_WIDTH || xc < 0 || xc >= SCREEN_WIDTH){
                        continue;
                    }
                    if (in_player(x, y) || in_player(xb, yb) || in_player(xc, yc)){
                        continue;
                    }
                    const int kb = key.at(xb, yb);
                    const int kc = key.at(xc, yc);
                    if (abs(kb - kc) <= 16){
                        continue;
                    }
                    const int n = now.at(x, y);
                    sum_best += std::min(abs(n - kb), DIFF_CAP);
                    sum_other += std::min(abs(n - kc), DIFF_CAP);
                    count++;
                }
            }
            double margin = count < 50
                ? 0     //  The two offsets look the same: no telling them apart.
                : (double)(sum_other - sum_best) / (double)count;
            result.margin = std::min(result.margin, margin);
        }
    }

    //  The same place shows the same picture, give or take the video noise
    //  (about 1), animated water and flowers, and people walking around.
    result.clear = result.best <= 10.0 && result.margin >= 8.0;
    return result;
}



Tracker::Tracker(int dir_x, int dir_y, double max_pixels_per_ms)
    : m_dir_x(dir_x)
    , m_dir_y(dir_y)
    , m_max_speed(max_pixels_per_ms)
{}

void Tracker::offset_to_shift(int along, int side, int& dx, int& dy) const{
    //  "side" is 90 degrees from the direction of the run.
    dx = along * m_dir_x - side * m_dir_y;
    dy = along * m_dir_y + side * m_dir_x;
}

int Tracker::tiles() const{
    return (int)std::floor((m_along + TILE_PIXELS / 2.0) / TILE_PIXELS);
}

bool Tracker::reliable() const{
    if (m_frames < 2){
        return false;
    }
    if (m_unsure * 4 > m_frames){
        return false;
    }
    //  Walking straight, the screen never moves sideways.
    return std::abs(m_side) <= 1;
}

std::string Tracker::summary() const{
    std::string s;
    s += std::to_string(m_along) + " px (" + std::to_string(tiles()) + " tiles)";
    if (m_side != 0){
        s += ", " + std::to_string(m_side) + " px sideways";
    }
    s += ", " + std::to_string(m_frames) + " frames";
    if (m_unsure > 0){
        s += ", " + std::to_string(m_unsure) + " unsure";
    }
    if (m_lost){
        s += ", screen changed";
    }
    char buffer[64];
    snprintf(buffer, sizeof(buffer), ", noise %.1f", m_noise < 0 ? 0.0 : m_noise);
    s += buffer;
    return s;
}


Tracker::Result Tracker::add(const Screen& screen, double time_ms){
    if (m_lost){
        return Result::LOST;
    }
    m_frames++;
    if (!m_has_key){
        m_key = screen;
        m_has_key = true;
        m_last_time = time_ms;
        return Result::TRACKED;
    }

    double dt = time_ms - m_last_time;
    if (dt < 1){
        dt = 1;
    }

    //  Where the screen can be now, relative to the key frame. The player
    //  only goes forward, at most at full speed, and never sideways.
    const int prev = m_along - m_key_along;
    const int reach = (int)std::ceil(m_max_speed * dt) + 2;
    const int lo = prev - 1;
    int hi = std::min(prev + reach, lo + MAX_SPAN - 1);
    const int predicted = prev + (int)std::lround(m_speed * dt);

    //  Mismatch at an offset along the run, the best of the side offsets.
    auto evaluate = [&](int a, int& side){
        double row_best = 1e18;
        side = 0;
        for (int s_abs = -2; s_abs <= 2; s_abs++){
            int s = s_abs - m_key_side;
            int dx, dy;
            offset_to_shift(a, s, dx, dy);
            double m = screen_mismatch(m_key, screen, dx, dy);
            if (m < row_best){
                row_best = m;
                side = s;
            }
        }
        return row_best;
    };

    std::array<double, MAX_SPAN> per_along;
    std::array<int, MAX_SPAN> per_side;
    double best = 1e18;
    int best_a = prev;
    int best_s = m_side - m_key_side;
    auto scan = [&](int from, int to){
        for (int a = from; a <= to; a++){
            int side;
            double m = evaluate(a, side);
            per_along[(size_t)(a - lo)] = m;
            per_side[(size_t)(a - lo)] = side;
            if (m < best){
                best = m;
                best_a = a;
                best_s = side;
            }
        }
    };
    //  4 and 8 pixels away from the best: how clearly it stands out.
    auto probes = [&](){
        double result = 1e18;
        for (int off : {-8, -4, 4, 8}){
            int a = best_a + off;
            if (a >= lo && a <= hi){
                result = std::min(result, per_along[(size_t)(a - lo)]);
            }else{
                int side;
                result = std::min(result, evaluate(a, side));
            }
        }
        return result;
    };

    scan(lo, hi);
    double probe = probes();
    const bool poor = m_typical > 0 && best > m_typical * 2 + 2;
    if ((probe < best - 1.0 || poor) && hi < lo + MAX_SPAN - 1){
        //  It fits better further on, or nothing fits well: frames were
        //  missed or the speed is off. Look further.
        int old_hi = hi;
        hi = lo + MAX_SPAN - 1;
        scan(old_hi + 1, hi);
        probe = probes();
    }

    //  The best match elsewhere (at least 3 pixels off).
    double other = probe;
    for (int a = lo; a <= hi; a++){
        if (std::abs(a - best_a) >= 3){
            other = std::min(other, per_along[(size_t)(a - lo)]);
        }
    }

    //  A perfect match only differs by the noise of the video. (Capped: a
    //  menu or text box that was there at the start and went away must
    //  not pass for noise.)
    if (m_noise < 0 || best < m_noise){
        m_noise = std::min(std::max(best, 0.5), 4.0);
    }

    //  Nothing matches: the screen is not the same map any more.
    const double lost_threshold = std::max(m_noise * 3 + 6, m_noise + 14);
    if (best > lost_threshold){
        m_bad_in_row++;
        m_lost_frames++;
        if (m_bad_in_row >= 2){
            m_lost = true;
            return Result::LOST;
        }
        m_unsure++;
        return Result::UNSURE;
    }
    m_bad_in_row = 0;
    m_last_best = best;
    m_typical = m_typical < 0 ? best : 0.8 * m_typical + 0.2 * best;

    const double margin = std::max(1.5, m_noise);
    const bool unsure = other - best < margin;
    int chosen_a = best_a;
    int chosen_s = best_s;
    if (unsure){
        //  Several offsets fit (no detail, or a repeating pattern): take the
        //  one closest to where walking on at the same speed would be.
        m_unsure++;
        int closest = 1 << 30;
        for (int a = lo; a <= hi; a++){
            if (per_along[(size_t)(a - lo)] > best + margin){
                continue;
            }
            int distance = std::abs(a - predicted);
            if (distance < closest){
                closest = distance;
                chosen_a = a;
                chosen_s = per_side[(size_t)(a - lo)];
            }
        }
    }

    const int new_along = m_key_along + chosen_a;
    const int new_side = m_key_side + chosen_s;
    const double moved = new_along - m_along;
    m_speed = 0.6 * m_speed + 0.4 * std::max(0.0, moved / dt);
    if (new_along == m_along && new_side == m_side){
        m_still_ms += dt;
    }else{
        m_still_ms = 0;
    }
    m_along = new_along;
    m_side = new_side;
    m_last_time = time_ms;

    //  Far enough from the key frame: start comparing with this one.
    if (!unsure && (std::abs(chosen_a) >= 24 || std::abs(chosen_s) >= 6)){
        m_key = screen;
        m_key_along = m_along;
        m_key_side = m_side;
    }
    return unsure ? Result::UNSURE : Result::TRACKED;
}



}
}
}
}

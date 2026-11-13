/*  Pokemon FRLG Input Timing Test
 *
 *  PocketPilot's own program (experimental: not tested on a console yet).
 *
 *  One measurement: the player stands in the overworld, the program presses
 *  + and looks at every frame of the video for the start menu. The time
 *  from the press to the first frame with the menu is the whole chain: the
 *  serial link to the controller board, the console asking the controller
 *  (USB), the game reading its buttons once a frame, drawing the menu, the
 *  capture card and the video pipeline of this program. B closes the menu,
 *  and it starts over, each time a little later between two frames.
 *
 *  What the numbers are for: a program that plays by the clock instead of
 *  by looking has to know this delay and above all how much it varies. A
 *  spread of about one frame (17 ms) is what the game itself causes (it
 *  reads the buttons once a frame). Whether a press can be aimed at one
 *  frame shows in the table by where between two frames the press fell.
 *
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_StartMenuDetector.h"
#include "PokemonFRLG_InputTimingTest.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


InputTimingTest_Descriptor::InputTimingTest_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:InputTimingTest",
        Pokemon::STRING_POKEMON + " FRLG", "Input Timing Test",
        "",
        "Measure how long the game takes to show its reaction to a button press, and how much that "
        "varies: the start menu is opened and closed a few hundred times.",
        PanelQualityStanding::EXPERIMENTAL,
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

struct InputTimingTest_Descriptor::Stats : public StatsTracker{
    Stats()
        : presses(m_stats["Presses"])
        , missed(m_stats["Not seen"])
    {
        m_display_order.emplace_back("Presses");
        m_display_order.emplace_back("Not seen", HIDDEN_IF_ZERO);
    }

    std::atomic<uint64_t>& presses;
    std::atomic<uint64_t>& missed;
};

std::unique_ptr<StatsTracker> InputTimingTest_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}


namespace{

double to_ms(WallDuration duration){
    return (double)std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000.0;
}

//  Looks at every frame once: the last one before the press, and the first
//  one after it that shows the start menu.
class MenuFrameTimer : public VisualInferenceCallback{
public:
    MenuFrameTimer()
        : VisualInferenceCallback("MenuFrameTimer")
        , m_detector(COLOR_RED)
    {}

    //  The button goes down now.
    void arm(WallClock pressed){
        std::lock_guard<std::mutex> lg(m_lock);
        m_pressed = pressed;
        m_armed = true;
    }

    virtual void make_overlays(VideoOverlaySet& items) const override{
        m_detector.make_overlays(items);
    }
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override{
        std::lock_guard<std::mutex> lg(m_lock);
        if (timestamp == m_last){
            return false;       //  The same frame again.
        }
        if (m_last != WallClock::min()){
            m_gaps.push_back(to_ms(timestamp - m_last));
        }
        m_last = timestamp;
        if (!m_armed || timestamp <= m_pressed){
            m_before = timestamp;
            return false;
        }
        if (m_detector.detect(frame)){
            m_seen = timestamp;
            return true;
        }
        return false;
    }

    //  After the session: the press, the last frame before it, the first
    //  frame with the menu, and the times between the frames seen.
    WallClock pressed() const{ return m_pressed; }
    WallClock before() const{ return m_before; }
    WallClock seen() const{ return m_seen; }
    const std::vector<double>& gaps() const{ return m_gaps; }

private:
    std::mutex m_lock;
    StartMenuDetector m_detector;
    bool m_armed = false;
    WallClock m_pressed = WallClock::min();
    WallClock m_last = WallClock::min();
    WallClock m_before = WallClock::min();
    WallClock m_seen = WallClock::min();
    std::vector<double> m_gaps;
};

double median(std::vector<double> values){
    if (values.empty()){
        return 0;
    }
    std::sort(values.begin(), values.end());
    const size_t middle = values.size() / 2;
    return values.size() % 2 == 1 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
}

std::string fixed(double value, int decimals = 1){
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    return buffer;
}

}   //  namespace



InputTimingTest::InputTimingTest()
    : SETUP_TEXT(
        "<b>Experimental:</b> this program is new and has not been tested on a console yet.<br>"
        "<b>Where to start:</b> in the overworld, standing still, where + opens the start menu (not in a "
        "battle, a menu or a conversation). The program opens the start menu with + "
        "and closes it with B, nothing else. The result is in the log (\"Output\")."
    )
    , PRESSES(
        "<b>Presses:</b><br>How many times the start menu is opened. One takes about two seconds.",
        LockMode::LOCK_WHILE_RUNNING,
        200, 20, 2000
    )
    , HOLD_MS(
        "<b>Hold + for (ms):</b><br>The game reads its buttons once a frame (16.7 ms). A hold shorter "
        "than about two frames can fall between two readings.",
        LockMode::LOCK_WHILE_RUNNING,
        80, 10, 500
    )
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_STATIC(SETUP_TEXT);
    PA_ADD_OPTION(PRESSES);
    PA_ADD_OPTION(HOLD_MS);
    PA_ADD_OPTION(NOTIFICATIONS);
}


void InputTimingTest::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    InputTimingTest_Descriptor::Stats& stats = env.current_stats<InputTimingTest_Descriptor::Stats>();

    env.log(PROGRAM_NAME + " build " + PROGRAM_BUILD + " (FRLG Input Timing Test, experimental)", COLOR_BLUE);

    const uint16_t presses = PRESSES;
    const Milliseconds hold((int64_t)(uint16_t)HOLD_MS);

    //  A closed start menu to begin with.
    pbf_mash_button(context, BUTTON_B, 1000ms);
    context.wait_for_all_requests();

    std::vector<double> delays;     //  press -> the first frame with the menu, ms
    std::vector<double> phases;     //  the last frame before the press -> the press, ms
    std::vector<double> spans;      //  the last frame before the press -> the frame with the menu, ms
    std::vector<double> gaps;       //  between the frames, ms
    uint64_t missed = 0;

    for (uint16_t i = 0; i < presses; i++){
        context.wait_for_all_requests();
        MenuFrameTimer timer;
        //  0 to 33 ms more each time: the presses fall anywhere between two
        //  frames (37 and 34 share no factor: every value comes by).
        const Milliseconds extra((int64_t)((i * 37) % 34));
        const int ret = run_until<ProControllerContext>(
            env.console, context,
            [&](ProControllerContext& ctx){
                //  Some frames before the press.
                ctx.wait_for(400ms + extra);
                timer.arm(current_time());
                pbf_press_button(ctx, BUTTON_PLUS, hold, 1500ms);
            },
            { {timer, 4ms} }
        );
        gaps.insert(gaps.end(), timer.gaps().begin(), timer.gaps().end());
        stats.presses++;
        if (ret != 0 || timer.seen() == WallClock::min() || timer.before() == WallClock::min()){
            missed++;
            stats.missed++;
            env.log("Press " + std::to_string(i + 1) + ": the start menu was not seen.", COLOR_ORANGE);
            if (missed >= 10 && missed * 2 > (uint64_t)(i + 1)){
                throw UserSetupError(
                    env.logger(),
                    "The start menu did not show after most presses of +. Start in the overworld, "
                    "standing still, where + opens the start menu."
                );
            }
        }else{
            delays.push_back(to_ms(timer.seen() - timer.pressed()));
            phases.push_back(to_ms(timer.pressed() - timer.before()));
            spans.push_back(to_ms(timer.seen() - timer.before()));
        }
        env.update_stats();

        //  Close it, and let the game take the next press.
        pbf_press_button(context, BUTTON_B, 100ms, 500ms);
        context.wait_for_all_requests();
    }
    pbf_mash_button(context, BUTTON_B, 500ms);
    context.wait_for_all_requests();

    if (delays.size() < 10){
        throw UserSetupError(
            env.logger(),
            "Too few presses were seen to say anything (" + std::to_string(delays.size()) + ")."
        );
    }

    //  The video's frames: how far apart, how evenly.
    const double frame = median(gaps);
    size_t even = 0;
    double longest = 0;
    for (double gap : gaps){
        even += std::abs(gap - frame) <= 3.0 ? 1 : 0;
        longest = std::max(longest, gap);
    }

    //  The delay.
    double sum = 0;
    double lowest = delays[0];
    double highest = delays[0];
    for (double delay : delays){
        sum += delay;
        lowest = std::min(lowest, delay);
        highest = std::max(highest, delay);
    }
    const double mean = sum / (double)delays.size();
    double squares = 0;
    for (double delay : delays){
        squares += (delay - mean) * (delay - mean);
    }
    const double deviation = std::sqrt(squares / (double)delays.size());

    env.log("---- Input timing: " + std::to_string(delays.size()) + " presses seen, " +
        std::to_string(missed) + " not ----", COLOR_BLUE);
    env.log(
        "Video: a frame every " + fixed(frame, 2) + " ms (the median of " + std::to_string(gaps.size()) +
        "); " + fixed(100.0 * (double)even / (double)std::max<size_t>(gaps.size(), 1)) +
        "% within 3 ms of that, the longest wait for a frame " + fixed(longest) + " ms.",
        COLOR_BLUE
    );
    env.log(
        "From the press to the first frame with the start menu: " + fixed(lowest) + " to " + fixed(highest) +
        " ms, median " + fixed(median(delays)) + ", mean " + fixed(mean) + ", standard deviation " +
        fixed(deviation) + " ms" + (frame > 0 ? " (" + fixed(deviation / frame, 2) + " frames)." : "."),
        COLOR_BLUE
    );

    if (frame > 1.0){
        //  In whole frames of the video, from the last frame before the
        //  press: how often each number of frames.
        std::map<int, size_t> by_frames;
        for (double span : spans){
            by_frames[(int)std::lround(span / frame)]++;
        }
        std::string table;
        for (const auto& item : by_frames){
            table += (table.empty() ? "" : ", ") + std::to_string(item.first) + " frames: " +
                std::to_string(item.second);
        }
        env.log("The menu shows this many frames after the last frame before the press: " + table + ".", COLOR_BLUE);

        //  The same by where between two frames the press fell (quarters of
        //  a frame). If the press could be aimed, each quarter would have
        //  one number of frames.
        const int QUARTERS = 4;
        std::map<int, size_t> by_quarter[QUARTERS];
        for (size_t i = 0; i < spans.size(); i++){
            int quarter = (int)(phases[i] / frame * QUARTERS);
            quarter = std::min(std::max(quarter, 0), QUARTERS - 1);
            by_quarter[quarter][(int)std::lround(spans[i] / frame)]++;
        }
        size_t same = 0;
        for (int q = 0; q < QUARTERS; q++){
            size_t total = 0;
            size_t most = 0;
            int most_frames = 0;
            std::string row;
            for (const auto& item : by_quarter[q]){
                total += item.second;
                if (item.second > most){
                    most = item.second;
                    most_frames = item.first;
                }
                row += (row.empty() ? "" : ", ") + std::to_string(item.first) + " frames: " +
                    std::to_string(item.second);
            }
            same += most;
            env.log(
                "Pressed in quarter " + std::to_string(q + 1) + " of the time between two frames (" +
                std::to_string(total) + " presses): " + (row.empty() ? std::string("none") : row) +
                (total > 0 ? ". Most often " + std::to_string(most_frames) + " frames, " +
                    fixed(100.0 * (double)most / (double)total) + "%." : "."),
                COLOR_BLUE
            );
        }
        const double aimed = 100.0 * (double)same / (double)spans.size();
        env.log(
            "Knowing in which quarter the press falls gives the frame the menu shows in for " + fixed(aimed) +
            "% of the presses. (Near 100%: a press can be aimed at a frame on this setup. Near the share "
            "of the most common number of frames overall: it cannot, the delay varies by more than the "
            "program can see.)",
            COLOR_BLUE
        );
    }

    send_program_finished_notification(
        env, NOTIFICATION_PROGRAM_FINISH,
        "Input timing: " + fixed(lowest) + " to " + fixed(highest) + " ms, median " + fixed(median(delays)) +
        " ms, standard deviation " + fixed(deviation) + " ms over " + std::to_string(delays.size()) + " presses."
    );
}


}
}
}

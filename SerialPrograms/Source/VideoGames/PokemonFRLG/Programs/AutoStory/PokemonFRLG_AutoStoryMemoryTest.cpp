/*  Pokemon FRLG Autostory Memory Test
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The memory test (the program's option "Memory test"): no buttons, a few
 *  parts of some minutes each, the memory growth of each part in the log.
 *
 *  The memory grows only while a program runs (with PA open and no program
 *  running it stays the same for hours, although the video, the stream
 *  history and the sound go on). A first test (build 105) found it in the
 *  video: taking pictures of the screen 13 times a second grew it 27 KB a
 *  picture, a screen watcher 9-50 KB, the walk and battle watchers 15-90
 *  KB; the text reading and the shiny sound did not. This test takes the
 *  frames as they come from the capture card and turns them into pictures
 *  in different ways, to find the one that grows it:
 *    - PA's pictures (as the screen watchers get them; from build 107 the
 *      own conversion once it matched Qt's, VideoFrameConversion.h);
 *    - Qt's conversion (QVideoFrame::toImage());
 *    - the same on PA's thread pool (where the screen watchers' pictures
 *      are made);
 *    - Qt's conversion after mapping the frame (what the stream history
 *      does, which does not grow it);
 *    - the own conversion (no Qt).
 *  Before that it compares the colors of the own conversion with Qt's.
 *
 */

#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <QImage>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Concurrency/AsyncTask.h"
#include "Common/Cpp/MemoryUtilization/MemoryUtilization.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/Backends/VideoFrameQt.h"
#include "CommonFramework/VideoPipeline/Backends/VideoFrameConversion.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonFRLG_AutoStoryTools.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

std::string test_gigabytes(size_t bytes){
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    return buffer;
}


//  The newest frame of the video as it comes from the capture card, before
//  PA turns it into a picture.
class LatestFrame : public VideoFrameListener{
public:
    LatestFrame(VideoFeed& feed)
        : m_feed(feed)
    {
        feed.add_frame_listener(*this);
    }
    ~LatestFrame(){
        m_feed.remove_frame_listener(*this);
    }
    virtual void on_frame(std::shared_ptr<const VideoFrame> frame) override{
        std::shared_ptr<const VideoFrame> old;
        std::lock_guard<std::mutex> lg(m_lock);
        old = std::move(m_frame);
        m_frame = std::move(frame);
        m_count++;
    }
    std::shared_ptr<const VideoFrame> get(){
        std::lock_guard<std::mutex> lg(m_lock);
        return m_frame;
    }
    uint64_t count(){
        std::lock_guard<std::mutex> lg(m_lock);
        return m_count;
    }

private:
    VideoFeed& m_feed;
    std::mutex m_lock;
    std::shared_ptr<const VideoFrame> m_frame;
    uint64_t m_count = 0;
};


}



void story_memory_test(ConsoleHandle& console, ProControllerContext& context, uint16_t minutes){
    const Milliseconds part_time = std::chrono::minutes(minutes);
    console.log(
        "Memory test 2: " + std::to_string(minutes) + " minutes for each of 5 parts, no buttons pressed.",
        COLOR_BLUE
    );
    context.wait_for_all_requests();

    LatestFrame latest(console.video());
    for (int i = 0; i < 50 && latest.get() == nullptr; i++){
        context.wait_for(Milliseconds(100));
    }
    if (latest.get() == nullptr){
        console.log(
            "Memory test: no frames come from the video (this camera backend does not give them). "
            "Only the first part.",
            COLOR_RED
        );
    }

    //  What the frames are, and how close the own conversion comes to Qt's.
    bool own_known = false;
    size_t own_choice = 0;
    if (std::shared_ptr<const VideoFrame> sample = latest.get()){
        const QVideoFrame& frame = sample->frame;
        const QVideoFrameFormat format = frame.surfaceFormat();
        QImage qt_image = frame.toImage();
        console.log(
            "Memory test: video frames " + QVideoFrameFormat::pixelFormatToString(frame.pixelFormat()).toStdString() +
            " " + std::to_string(frame.width()) + " x " + std::to_string(frame.height()) +
            ", handle type " + std::to_string((int)frame.handleType()) +
            ", color space " + std::to_string((int)format.colorSpace()) +
            ", color range " + std::to_string((int)format.colorRange()) +
            ", planes " + std::to_string(frame.planeCount()) +
            "; Qt's picture format " + std::to_string((int)qt_image.format()) + "."
        );

        double means[YUV_CONVERSION_COUNT] = {};
        int largest[YUV_CONVERSION_COUNT] = {};
        int compared = 0;
        WallClock last = WallClock::min();
        for (int attempt = 0; attempt < 100 && compared < 5; attempt++){
            std::shared_ptr<const VideoFrame> current = latest.get();
            if (current == nullptr || current->timestamp == last){
                context.wait_for(Milliseconds(50));
                continue;
            }
            last = current->timestamp;
            QVideoFrame copy = current->frame;
            ImageRGB32 qt = qt_frame_to_image(copy);
            if (!image_has_colors(qt)){
                context.wait_for(Milliseconds(200));
                continue;
            }
            if (!copy.map(QVideoFrame::ReadOnly)){
                break;
            }
            bool ok = true;
            for (size_t i = 0; i < YUV_CONVERSION_COUNT && ok; i++){
                ImageRGB32 own = yuv_frame_to_image(copy, YUV_CONVERSIONS[i]);
                double mean = 0;
                int big = 0;
                ok = compare_images(own, qt, mean, big);
                means[i] += mean;
                largest[i] = std::max(largest[i], big);
            }
            copy.unmap();
            if (!ok){
                break;
            }
            compared++;
            context.wait_for(Milliseconds(200));
        }
        if (compared == 0){
            console.log("Memory test: no frame to compare the own conversion with (an unknown format, or no colors on the screen).", COLOR_ORANGE);
        }else{
            own_known = true;
            std::string line = "Memory test: own conversion against Qt's (" + std::to_string(compared) + " frames, mean / largest difference of a color 0-255):";
            for (size_t i = 0; i < YUV_CONVERSION_COUNT; i++){
                char item[96];
                snprintf(item, sizeof(item), " %s %.2f / %d;", YUV_CONVERSIONS[i].name, means[i] / compared, largest[i]);
                line += item;
                if (means[i] < means[own_choice]){
                    own_choice = i;
                }
            }
            console.log(line);
        }
    }

    std::string results;
    auto run_part = [&](const char* name, const std::function<uint64_t(WallClock end)>& part){
        MemoryUsage before = process_memory_usage();
        const uint64_t frames_before = latest.count();
        const WallClock start = current_time();
        console.log(std::string("Memory test: ") + name + "...", COLOR_BLUE);
        const uint64_t pictures = part(start + part_time);
        context.wait_for_all_requests();
        MemoryUsage after = process_memory_usage();
        const double seconds = (double)std::chrono::duration_cast<std::chrono::milliseconds>(current_time() - start).count() / 1000.0;
        const double hours = seconds / 3600.0;
        const double grown = ((double)after.process_virtual_memory - (double)before.process_virtual_memory) / (1024.0 * 1024.0);
        char line[200];
        snprintf(
            line, sizeof(line), "%s %+.0f MB in %.0f min (%+.0f MB an hour), %llu pictures (%+.1f KB each)",
            name, grown, seconds / 60, hours > 0 ? grown / hours : 0.0,
            (unsigned long long)pictures, pictures > 0 ? grown * 1024 / (double)pictures : 0.0
        );
        console.log(
            std::string("Memory test: ") + line + "; " +
            std::to_string(latest.count() - frames_before) + " frames came in; program " +
            test_gigabytes(after.process_physical_memory) + " (virtual " + test_gigabytes(after.process_virtual_memory) + ").",
            COLOR_BLUE
        );
        results += results.empty() ? "" : "; ";
        results += line;
    };

    //  Each part makes a picture about 15 times a second, each of a new frame.
    auto each_frame = [&](WallClock end, const std::function<bool(const std::shared_ptr<const VideoFrame>&)>& convert){
        uint64_t pictures = 0;
        WallClock last = WallClock::min();
        while (current_time() < end){
            std::shared_ptr<const VideoFrame> frame = latest.get();
            if (frame != nullptr && frame->timestamp != last){
                last = frame->timestamp;
                pictures += convert(frame);
            }
            context.wait_for(Milliseconds(66));
        }
        return pictures;
    };

    //  1. PA's pictures of the screen, as the screen watchers take them.
    run_part("PA pictures", [&](WallClock end){
        uint64_t pictures = 0;
        while (current_time() < end){
            VideoSnapshot screen = console.video().snapshot();
            pictures += screen.frame != nullptr;
            context.wait_for(Milliseconds(66));
        }
        return pictures;
    });
    if (latest.get() == nullptr){
        console.log("Memory test done: " + results + ".", COLOR_BLUE);
        return;
    }

    //  2. Qt's conversion, on this thread.
    run_part("Qt conversion", [&](WallClock end){
        return each_frame(end, [](const std::shared_ptr<const VideoFrame>& frame){
            QImage image = frame->frame.toImage();
            return !image.isNull();
        });
    });

    //  3. Qt's conversion on PA's thread pool, where PA makes the screen
    //  watchers' pictures.
    run_part("Qt conversion, thread pool", [&](WallClock end){
        return each_frame(end, [](const std::shared_ptr<const VideoFrame>& frame){
            bool ok = false;
            AsyncTask task = GlobalThreadPools::computation_realtime().dispatch_now_blocking(
                [&ok, frame](){
                    QImage image = frame->frame.toImage();
                    ok = !image.isNull();
                }
            );
            task.wait_and_rethrow_exceptions();
            return ok;
        });
    });

    //  4. Qt's conversion after mapping the frame, as the stream history
    //  does.
    run_part("Qt conversion after map", [&](WallClock end){
        return each_frame(end, [](const std::shared_ptr<const VideoFrame>& frame){
            QVideoFrame copy = frame->frame;
            if (!copy.map(QVideoFrame::ReadOnly)){
                return false;
            }
            QImage image = copy.toImage();
            copy.unmap();
            return !image.isNull();
        });
    });

    //  5. The own conversion (no Qt).
    if (own_known){
        run_part("own conversion", [&](WallClock end){
            return each_frame(end, [own_choice](const std::shared_ptr<const VideoFrame>& frame){
                ImageRGB32 image = yuv_frame_to_image_mapping(frame->frame, YUV_CONVERSIONS[own_choice]);
                return (bool)image;
            });
        });
    }

    console.log("Memory test done: " + results + ".", COLOR_BLUE);
}



}
}
}

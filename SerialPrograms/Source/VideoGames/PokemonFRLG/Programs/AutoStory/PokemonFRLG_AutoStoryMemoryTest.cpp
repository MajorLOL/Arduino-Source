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
 *  picture, a screen watcher 50 KB, the walk and battle watchers 90 KB; the
 *  text reading and the shiny sound did not. This test takes the frames as
 *  they come from the capture card and turns them into pictures in
 *  different ways, to find the one that grows it:
 *    - PA's pictures (as the screen watchers get them);
 *    - Qt's conversion (QVideoFrame::toImage(), what PA's pictures use);
 *    - the same on PA's thread pool (where the screen watchers' pictures
 *      are made);
 *    - Qt's conversion after mapping the frame (what the stream history
 *      does, which does not grow it);
 *    - a conversion of its own (no Qt).
 *  Before that it compares the colors of its own conversion with Qt's.
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
#include "CommonFramework/ImageTypes/ImageRGB32_Qt.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/Backends/VideoFrameQt.h"
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


//  YUV to RGB, the factors times 1024.
struct YuvMatrix{
    const char* name;
    int y;
    int rv;
    int gu;
    int gv;
    int bu;
    int y_offset;
};
const YuvMatrix BT601_LIMITED{"BT.601 limited", 1192, 1634, 401, 833, 2065, 16};
const YuvMatrix BT709_LIMITED{"BT.709 limited", 1192, 1836, 218, 546, 2163, 16};
const YuvMatrix BT601_FULL{"BT.601 full", 1024, 1436, 352, 731, 1815, 0};
const YuvMatrix BT709_FULL{"BT.709 full", 1024, 1613, 192, 479, 1900, 0};

inline int to_byte(int scaled){
    return std::clamp(scaled + 512, 0, 255 * 1024 + 1023) >> 10;
}
inline uint32_t yuv_pixel(const YuvMatrix& m, int y, int u, int v){
    const int yy = (y - m.y_offset) * m.y;
    u -= 128;
    v -= 128;
    const uint32_t r = (uint32_t)to_byte(yy + m.rv * v);
    const uint32_t g = (uint32_t)to_byte(yy - m.gu * u - m.gv * v);
    const uint32_t b = (uint32_t)to_byte(yy + m.bu * u);
    return 0xff000000u | (r << 16) | (g << 8) | b;
}

//  A mapped frame to a picture without Qt. Empty for a format it does not
//  know.
ImageRGB32 convert_mapped(const QVideoFrame& frame, const YuvMatrix& m){
    const int width = frame.width();
    const int height = frame.height();
    if (width <= 0 || height <= 0){
        return ImageRGB32();
    }
    const QVideoFrameFormat::PixelFormat format = frame.pixelFormat();
    const uchar* plane0 = frame.bits(0);
    const int stride0 = frame.bytesPerLine(0);
    if (plane0 == nullptr || stride0 <= 0){
        return ImageRGB32();
    }
    ImageRGB32 image((size_t)width, (size_t)height);
    auto out_row = [&](int row){
        return (uint32_t*)((char*)image.data() + (size_t)row * image.bytes_per_row());
    };

    switch (format){
    case QVideoFrameFormat::Format_NV12:
    case QVideoFrameFormat::Format_NV21:{
        const uchar* plane1 = frame.planeCount() >= 2 ? frame.bits(1) : nullptr;
        int stride1 = frame.planeCount() >= 2 ? frame.bytesPerLine(1) : stride0;
        if (plane1 == nullptr){
            plane1 = plane0 + (size_t)stride0 * (size_t)height;
            stride1 = stride0;
        }
        const int ui = format == QVideoFrameFormat::Format_NV12 ? 0 : 1;
        const int vi = 1 - ui;
        for (int row = 0; row < height; row++){
            const uchar* y = plane0 + (size_t)row * stride0;
            const uchar* uv = plane1 + (size_t)(row / 2) * stride1;
            uint32_t* out = out_row(row);
            for (int c = 0; c < width; c++){
                const uchar* pair = uv + (c / 2) * 2;
                out[c] = yuv_pixel(m, y[c], pair[ui], pair[vi]);
            }
        }
        return image;
    }
    case QVideoFrameFormat::Format_YUV420P:
    case QVideoFrameFormat::Format_YV12:{
        if (frame.planeCount() < 3 || frame.bits(1) == nullptr || frame.bits(2) == nullptr){
            return ImageRGB32();
        }
        const bool yv12 = format == QVideoFrameFormat::Format_YV12;
        const uchar* uplane = frame.bits(yv12 ? 2 : 1);
        const uchar* vplane = frame.bits(yv12 ? 1 : 2);
        const int ustride = frame.bytesPerLine(yv12 ? 2 : 1);
        const int vstride = frame.bytesPerLine(yv12 ? 1 : 2);
        for (int row = 0; row < height; row++){
            const uchar* y = plane0 + (size_t)row * stride0;
            const uchar* u = uplane + (size_t)(row / 2) * ustride;
            const uchar* v = vplane + (size_t)(row / 2) * vstride;
            uint32_t* out = out_row(row);
            for (int c = 0; c < width; c++){
                out[c] = yuv_pixel(m, y[c], u[c / 2], v[c / 2]);
            }
        }
        return image;
    }
    case QVideoFrameFormat::Format_YUYV:
    case QVideoFrameFormat::Format_UYVY:{
        //  YUYV: Y0 U Y1 V. UYVY: U Y0 V Y1.
        const bool yuyv = format == QVideoFrameFormat::Format_YUYV;
        for (int row = 0; row < height; row++){
            const uchar* in = plane0 + (size_t)row * stride0;
            uint32_t* out = out_row(row);
            for (int c = 0; c < width; c++){
                const uchar* quad = in + (c / 2) * 4;
                const int y = yuyv ? quad[(c & 1) * 2] : quad[1 + (c & 1) * 2];
                const int u = yuyv ? quad[1] : quad[0];
                const int v = yuyv ? quad[3] : quad[2];
                out[c] = yuv_pixel(m, y, u, v);
            }
        }
        return image;
    }
    default:
        return ImageRGB32();
    }
}

ImageRGB32 qt_picture(const QVideoFrame& frame){
    QImage image = frame.toImage();
    if (image.isNull()){
        return ImageRGB32();
    }
    if (image.format() != QImage::Format_ARGB32 && image.format() != QImage::Format_RGB32){
        image = image.convertToFormat(QImage::Format_ARGB32);
    }
    return QImage_to_ImageRGB32(std::move(image));
}

//  The mean and the largest difference of the color channels.
bool compare_pictures(const ImageRGB32& a, const ImageRGB32& b, double& mean, int& largest){
    if (!a || !b || a.width() != b.width() || a.height() != b.height()){
        return false;
    }
    uint64_t sum = 0;
    largest = 0;
    for (size_t row = 0; row < a.height(); row++){
        for (size_t c = 0; c < a.width(); c++){
            const uint32_t pa = a.pixel(c, row);
            const uint32_t pb = b.pixel(c, row);
            for (int shift = 0; shift < 24; shift += 8){
                const int d = std::abs((int)((pa >> shift) & 0xff) - (int)((pb >> shift) & 0xff));
                sum += (uint64_t)d;
                largest = std::max(largest, d);
            }
        }
    }
    mean = (double)sum / (3.0 * (double)a.width() * (double)a.height());
    return true;
}

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
    const YuvMatrix* own_matrix = &BT709_LIMITED;
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

        const YuvMatrix* matrices[] = {&BT601_LIMITED, &BT709_LIMITED, &BT601_FULL, &BT709_FULL};
        double means[4] = {0, 0, 0, 0};
        int largest[4] = {0, 0, 0, 0};
        int compared = 0;
        WallClock last = WallClock::min();
        for (int attempt = 0; attempt < 50 && compared < 5; attempt++){
            std::shared_ptr<const VideoFrame> current = latest.get();
            if (current == nullptr || current->timestamp == last){
                context.wait_for(Milliseconds(50));
                continue;
            }
            last = current->timestamp;
            QVideoFrame copy = current->frame;
            ImageRGB32 qt = qt_picture(copy);
            if (!copy.map(QVideoFrame::ReadOnly)){
                break;
            }
            bool ok = true;
            for (int i = 0; i < 4 && ok; i++){
                ImageRGB32 own = convert_mapped(copy, *matrices[i]);
                double mean = 0;
                int big = 0;
                ok = compare_pictures(own, qt, mean, big);
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
            console.log("Memory test: the own conversion does not know this frame format.", COLOR_ORANGE);
        }else{
            own_known = true;
            std::string line = "Memory test: own conversion against Qt's (" + std::to_string(compared) + " frames, mean / largest difference of a color 0-255):";
            double best = 1e9;
            for (int i = 0; i < 4; i++){
                char item[96];
                snprintf(item, sizeof(item), " %s %.2f / %d;", matrices[i]->name, means[i] / compared, largest[i]);
                line += item;
                if (means[i] / compared < best){
                    best = means[i] / compared;
                    own_matrix = matrices[i];
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
            return each_frame(end, [own_matrix](const std::shared_ptr<const VideoFrame>& frame){
                QVideoFrame copy = frame->frame;
                if (!copy.map(QVideoFrame::ReadOnly)){
                    return false;
                }
                ImageRGB32 image = convert_mapped(copy, *own_matrix);
                copy.unmap();
                return (bool)image;
            });
        });
    }

    console.log("Memory test done: " + results + ".", COLOR_BLUE);
}



}
}
}

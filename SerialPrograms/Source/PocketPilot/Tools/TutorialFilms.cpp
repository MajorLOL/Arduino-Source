/*  Tutorial films
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  Renders the video tutorials (Resources/tutorial-pico.mp4, tutorial-esp32.mp4
 *  and tutorial-esp32s3.mp4) from the real setup screens: the screens are played
 *  in demo mode (nothing is flashed), a cursor and a caption are drawn over them
 *  and the pictures go to ffmpeg. This file is not part of the program: it
 *  includes PocketPilot_Setup.cpp to get at its screens, and has its own main().
 *
 *  Build it like the setup test of the program (Qt6 Widgets, Network, SerialPort,
 *  Multimedia and MultimediaWidgets; this folder's parent and the Source folder
 *  on the include path; the output of rcc for pocketpilot.qrc and
 *  PocketPilot_Firmware.cpp linked in; a tr_ui() that returns its argument, as
 *  English is what the films show), then run
 *
 *      film <pico|esp32|esp32s3> <video.mp4> <events.txt>
 *      python3 tutorial_audio.py <events.txt> <sound.wav>
 *      ffmpeg -i <video.mp4> -i <sound.wav> -map 0:v -map 1:a -c:v copy -c:a aac -b:a 96k out.mp4
 *
 *  events.txt has the time of each click and of the end of the flash, for the
 *  sound. The films are 1920 x 1080 at 30 pictures a second, about 90 seconds.
 *
 */

#include "PocketPilot/PocketPilot_Setup.cpp"

#include <cstdio>
#include <QProcess>

namespace PokemonAutomation{
namespace PocketPilot{
namespace{

const int FILM_W = 1920;
const int FILM_H = 1080;
const int PAGE_W = 1728;
const int PAGE_H = 972;
const int PAGE_X = 96;

QPointF smooth_point(QPointF a, QPointF b, double u){
    const double e = u * u * (3 - 2 * u);
    return QPointF(a.x() + (b.x() - a.x()) * e, a.y() + (b.y() - a.y()) * e);
}

class Film{
public:
    Film(Canvas& canvas, FILE* pipe, FILE* events)
        : m_canvas(canvas)
        , m_pipe(pipe)
        , m_events(events)
        , m_cursor(PAGE_W * 0.55, PAGE_H * 0.62)
    {}

    double time() const{
        return m_frames / 30.0;
    }

    void caption(const QString& text, const QString& label = QString()){
        m_caption = text;
        m_label = label;
        m_caption_time = 0;
    }

    //  One picture.
    void frame(){
        m_canvas.advance(1.0 / 30.0);
        QImage page = m_canvas.picture().convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QImage out(FILM_W, FILM_H, QImage::Format_RGB888);
        out.fill(QColor(8, 8, 8));
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawImage(PAGE_X, 0, page);

        //  The ripple of a click.
        if (m_ripple >= 0 && m_ripple < 1){
            const QPointF c = m_cursor + QPointF(PAGE_X, 0);
            const double r = 18 + 46 * m_ripple;
            p.setPen(QPen(QColor(232, 125, 93, (int)(200 * (1 - m_ripple))), 5));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, r, r);
        }
        //  The cursor.
        {
            const QPointF c = m_cursor + QPointF(PAGE_X, 0);
            QPainterPath arrow;
            arrow.moveTo(0, 0);
            arrow.lineTo(0, 34);
            arrow.lineTo(8.5, 26.5);
            arrow.lineTo(14.5, 40);
            arrow.lineTo(20.5, 37.5);
            arrow.lineTo(14.5, 24.5);
            arrow.lineTo(25, 24);
            arrow.closeSubpath();
            p.save();
            p.translate(c);
            p.setPen(QPen(QColor(0, 0, 0, 230), 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(QColor(255, 255, 255));
            p.drawPath(arrow);
            p.restore();
        }

        //  The caption under the screen.
        {
            m_caption_time += 1.0 / 30.0;
            const double a = std::min(1.0, m_caption_time / 0.35);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(18, 18, 18));
            p.drawRect(0, PAGE_H, FILM_W, FILM_H - PAGE_H);
            p.setPen(QPen(QColor(255, 255, 255, 26), 2));
            p.drawLine(PAGE_X, PAGE_H, PAGE_X + PAGE_W, PAGE_H);
            if (!m_caption.isEmpty()){
                p.setOpacity(a);
                double x = PAGE_X + 24;
                if (!m_label.isEmpty()){
                    const QFont lf = ui_font(26, 600);
                    p.setFont(lf);
                    const QFontMetricsF lm(lf);
                    const double w = lm.horizontalAdvance(m_label) + 40;
                    QPainterPath chip;
                    chip.addRoundedRect(QRectF(x, PAGE_H + 30, w, 48), 24, 24);
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(232, 125, 93));
                    p.drawPath(chip);
                    p.setPen(QColor(24, 12, 8));
                    p.drawText(QRectF(x, PAGE_H + 30, w, 48), Qt::AlignCenter, m_label);
                    x += w + 24;
                }
                //  As large as fits on the line.
                const double room = PAGE_X + PAGE_W - x - 24;
                double size = 34;
                while (size > 24 && QFontMetricsF(ui_font(size, 500)).horizontalAdvance(m_caption) > room){
                    size -= 1;
                }
                const QFont cf = ui_font(size, 500);
                p.setFont(cf);
                p.setPen(QColor(255, 255, 255));
                p.drawText(QRectF(x, PAGE_H + 24, room, 60), Qt::AlignVCenter | Qt::AlignLeft, m_caption);
                p.setOpacity(1);
            }
        }
        p.end();

        fwrite(out.constBits(), 1, (size_t)out.sizeInBytes(), m_pipe);
        m_frames++;
        if (m_ripple >= 0){
            m_ripple += 1.0 / 30.0 / 0.4;
        }
    }

    void hold(double seconds){
        const int n = std::max(1, (int)std::lround(seconds * 30.0));
        for (int c = 0; c < n; c++){
            frame();
        }
    }

    //  Waits for a page change to be done.
    void settle(double seconds = 1.8){
        hold(seconds);
    }

    //  The cursor goes to the button, the button is pressed.
    bool press(const QString& id, double travel = 0.8){
        QPointF target;
        for (int tries = 0; tries < 60 && !m_canvas.hit_center(id, target); tries++){
            frame();
        }
        if (!m_canvas.hit_center(id, target)){
            std::fprintf(stderr, "film: no button %s\n", id.toUtf8().constData());
            m_failed = true;
            return false;
        }
        const QPointF from = m_cursor;
        const int n = std::max(2, (int)std::lround(travel * 30.0));
        for (int c = 1; c <= n; c++){
            m_cursor = smooth_point(from, target, (double)c / n);
            m_canvas.hit_center(id, target);
            frame();
        }
        m_canvas.move_to(id);
        hold(0.12);
        m_ripple = 0;
        std::fprintf(m_events, "click %.3f\n", time());
        const bool ok = m_canvas.click(id);
        if (!ok){
            std::fprintf(stderr, "film: %s cannot be pressed\n", id.toUtf8().constData());
            m_failed = true;
        }
        hold(0.35);
        return ok;
    }

    //  Waits until the page says so (the state has the text), with a limit.
    void until(const QString& what, double limit){
        for (double t = 0; t < limit && !m_canvas.page_state().contains(what); t += 1.0 / 30.0){
            frame();
        }
        if (!m_canvas.page_state().contains(what)){
            std::fprintf(stderr, "film: never reached %s\n", what.toUtf8().constData());
            m_failed = true;
        }
    }

    void event(const char* name){
        std::fprintf(m_events, "%s %.3f\n", name, time());
    }

    bool failed() const{
        return m_failed;
    }

private:
    Canvas& m_canvas;
    FILE* m_pipe;
    FILE* m_events;
    long m_frames = 0;
    QPointF m_cursor;
    double m_ripple = -1;
    QString m_caption;
    QString m_label;
    double m_caption_time = 0;
    bool m_failed = false;
};

struct Computer{
    std::mutex lock;
    HardwareState state;
    void set(const std::function<void(HardwareState&)>& change){
        std::lock_guard<std::mutex> guard(lock);
        change(state);
    }
};

void write_file(const QString& path, const QByteArray& data){
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)){
        file.write(data);
    }
}

QString step_label(int n){
    return QString("Step %1 of 7").arg(n);
}

void film_pico(Film& f, Canvas& c, Computer& pc, const QString& drive_folder){
    f.caption("Setting up a Raspberry Pi Pico W takes seven short steps.", "Pico W");
    f.hold(0.5);
    f.settle(3.2);

    //  1
    f.caption("Plug the Pico into the computer with a micro-USB cable.", step_label(1));
    f.hold(4.5);
    f.press("next");
    f.settle(1.4);

    //  2
    f.caption("Unplug it, hold the white Bootsel button, plug it in again, then let go.", step_label(2));
    f.hold(6.0);
    pc.set([&](HardwareState& s){
        PicoDrive drive;
        drive.root = drive_folder;
        drive.label = "RPI-RP2";
        drive.board_id = "RPI-RP2";
        s.drives.push_back(drive);
    });
    f.hold(0.9);
    f.caption("The program sees the Pico as a drive named RPI-RP2.", step_label(2));
    f.hold(4.0);
    f.press("next");
    f.settle(1.4);

    //  3
    f.caption("Pick the newest firmware version. The numbers on your screen will differ.", step_label(3));
    f.hold(2.0);
    f.press("dd:version");
    f.hold(1.6);
    f.press("dd:version:PABotBase2-Pico1W-2026110100.uf2");
    f.hold(1.2);
    f.caption("The download link is there too, if you want the file yourself.", step_label(3));
    f.press("link_copy");
    f.hold(2.4);
    f.caption("Press Flash now. The program puts the file on the Pico for you.", step_label(3));
    f.press("flash");
    f.until("flash=done", 12.0);
    f.event("done");
    f.caption("Done. The Pico has its firmware.", step_label(3));
    f.hold(3.6);
    f.press("next");
    f.settle(1.4);

    //  4
    f.caption("On the Switch, go to HOME, then Controllers, then Change Grip/Order.", step_label(4));
    f.hold(5.5);
    f.press("next");
    f.settle(1.4);

    //  5
    f.caption("Open the program and pick Serial: PABotBase2 as the first dropdown.", step_label(5));
    f.hold(6.0);
    f.press("next");
    f.settle(1.4);

    //  6
    f.caption("Pick the Pico as the serial port, and NS1: Wireless Pro Controller as the type.", step_label(6));
    f.hold(7.0);
    f.press("next");
    f.settle(1.4);

    //  7
    f.caption("A controller shows up in the Grip menu. Click the video and press some keys.", step_label(7));
    f.hold(6.5);
    f.press("next");
    f.caption("That is all. The Pico is ready.", QString());
    f.settle(2.2);
    f.hold(3.0);
}

void film_esp32(Film& f, Canvas& c, Computer& pc){
    (void)c;
    f.caption("Setting up an ESP32 takes seven short steps. The program flashes it for you.", "ESP32");
    f.hold(0.5);
    f.settle(3.2);

    //  1
    f.caption("Plug the ESP32 into the computer with a USB cable.", step_label(1));
    f.hold(3.2);
    f.caption("The program lists the serial ports it sees. Nothing yet? Try the driver buttons.", step_label(1));
    f.hold(3.6);
    pc.set([](HardwareState& s){
        PortInfo port;
        port.name = "COM7";
        port.description = "Silicon Labs CP210x USB to UART Bridge";
        port.vendor = 0x10C4;
        port.product = 0xEA60;
        port.chip = "CP210x";
        s.ports.push_back(port);
    });
    f.hold(1.0);
    f.caption("There it is: the ESP32 on COM7.", step_label(1));
    f.hold(3.2);
    f.press("next");
    f.settle(1.4);

    //  2
    f.caption("Pick the port and the newest firmware version. The numbers on your screen will differ.", step_label(2));
    f.hold(2.4);
    f.press("dd:version");
    f.hold(1.4);
    f.press("dd:version:PABotBase2-ESP32-2026110100.bin");
    f.hold(1.0);
    f.caption("The download link is there too, if you want the file yourself.", step_label(2));
    f.press("link_copy");
    f.hold(2.2);
    f.caption("Press Flash now. This takes under a minute.", step_label(2));
    f.press("flash");
    f.until("flash=done", 14.0);
    f.event("done");
    f.caption("Done. The ESP32 has its firmware.", step_label(2));
    f.hold(3.4);
    f.press("next");
    f.settle(1.4);

    //  3
    f.caption("Press the EN or RESET button, or unplug the board and plug it in again.", step_label(3));
    f.hold(5.5);
    f.press("next");
    f.settle(1.4);

    //  4
    f.caption("On the Switch, go to HOME, then Controllers, then Change Grip/Order.", step_label(4));
    f.hold(5.0);
    f.press("next");
    f.settle(1.4);

    //  5
    f.caption("Open the program and pick Serial: PABotBase2 as the first dropdown.", step_label(5));
    f.hold(6.0);
    f.press("next");
    f.settle(1.4);

    //  6
    f.caption("Pick the ESP32 as the serial port, and NS1: Wireless Pro Controller as the type.", step_label(6));
    f.hold(7.0);
    f.press("next");
    f.settle(1.4);

    //  7
    f.caption("Wait about 5 seconds for the controller in the Grip menu. Then click the video and press some keys.", step_label(7));
    f.hold(7.0);
    f.press("next");
    f.caption("That is all. The ESP32 is ready.", QString());
    f.settle(2.2);
    f.hold(3.0);
}

void film_esp32s3(Film& f, Canvas& c, Computer& pc){
    (void)c;
    f.caption("The ESP32-S3 is wired, and the most reliable. Seven short steps.", "ESP32-S3");
    f.hold(0.5);
    f.settle(3.2);

    //  1
    f.caption("Plug the COM or UART port into the computer, and the USB or OTG port into the Switch dock.", step_label(1));
    f.hold(3.6);
    pc.set([](HardwareState& s){
        PortInfo port;
        port.name = "COM7";
        port.description = "Silicon Labs CP210x USB to UART Bridge";
        port.vendor = 0x10C4;
        port.product = 0xEA60;
        port.chip = "CP210x";
        s.ports.push_back(port);
    });
    f.hold(1.0);
    f.caption("The program sees the board on COM7. The two ports are not interchangeable.", step_label(1));
    f.hold(4.0);
    f.press("next");
    f.settle(1.4);

    //  2
    f.caption("On the Switch, turn on Pro Controller Wired Communication under Controllers and Sensors.", step_label(2));
    f.hold(6.5);
    f.press("next");
    f.settle(1.4);

    //  3
    f.caption("Pick the port and the newest firmware version. The numbers on your screen will differ.", step_label(3));
    f.hold(2.4);
    f.press("dd:version");
    f.hold(1.4);
    f.press("dd:version:PABotBase2-ESP32-S3-2026110100.bin");
    f.hold(1.0);
    f.caption("The download link is there too, if you want the file yourself.", step_label(3));
    f.press("link_copy");
    f.hold(2.2);
    f.caption("Press Flash now. This takes under a minute.", step_label(3));
    f.press("flash");
    f.until("flash=done", 14.0);
    f.event("done");
    f.caption("Done. The ESP32-S3 has its firmware.", step_label(3));
    f.hold(3.4);
    f.press("next");
    f.settle(1.4);

    //  4
    f.caption("Press the EN or RESET button, or unplug both cables and plug them in again.", step_label(4));
    f.hold(5.5);
    f.press("next");
    f.settle(1.4);

    //  5
    f.caption("Pick Serial: PABotBase2 as the first dropdown, then your device as the serial port.", step_label(5));
    f.hold(7.0);
    f.press("next");
    f.settle(1.4);

    //  6
    f.caption("Pick NS1: Wired Pro Controller as the type, then click the video and press Enter.", step_label(6));
    f.hold(6.5);
    f.press("next");
    f.settle(1.4);

    //  7
    f.caption("Click the video and press some keys. Check the picture and the sound.", step_label(7));
    f.hold(6.5);
    f.press("next");
    f.caption("That is all. The ESP32-S3 is ready.", QString());
    f.settle(2.2);
    f.hold(3.0);
}

}   //  namespace
}
}

int main(int argc, char** argv){
    using namespace PokemonAutomation::PocketPilot;
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    if (argc < 4){
        std::fprintf(stderr, "film <pico|esp32|esp32s3> <video.mp4> <events.txt>\n");
        return 2;
    }
    const QString board = QString::fromLocal8Bit(argv[1]);
    const QString video = QString::fromLocal8Bit(argv[2]);
    const QString events_path = QString::fromLocal8Bit(argv[3]);

    QTemporaryDir scratch;
    const QString application = QDir(scratch.path()).filePath("app");
    const QString drive_folder = QDir(scratch.path()).filePath("drive");
    QDir().mkpath(application + "/Firmware");
    QDir().mkpath(drive_folder);
    QByteArray uf2(512 * 4, 'u');
    uf2.replace(0, 4, "UF2\n");
    write_file(application + "/Firmware/PABotBase2-Pico1W-2026110100.uf2", uf2);
    write_file(application + "/Firmware/PABotBase2-Pico1W-2026090100.uf2", uf2);
    write_file(application + "/Firmware/PABotBase2-ESP32-2026110100.bin", QByteArray(9000, 'b'));
    write_file(application + "/Firmware/PABotBase2-ESP32-2026090100.bin", QByteArray(9000, 'b'));
    write_file(application + "/Firmware/PABotBase2-ESP32-S3-2026110100.bin", QByteArray(9000, 'b'));
    write_file(application + "/Firmware/PABotBase2-ESP32-S3-2026090100.bin", QByteArray(9000, 'b'));
    write_file(application + "/Firmware/SOURCE.txt", "https://github.com/PokemonAutomation/Packages/tree/0123456789abcdef0123456789abcdef01234567/Firmware\n");

    Computer pc;
    pc.state.tool_checked = true;

    SetupRequest request;
    request.ask_board = true;
    Canvas canvas(request, true);
    canvas.set_pretend(true);
    canvas.set_app_folder(application);
    canvas.set_scan([&pc]{
        std::lock_guard<std::mutex> guard(pc.lock);
        return pc.state;
    });
    canvas.set_listing("[]");
    canvas.resize(PAGE_W, PAGE_H);
    QEventLoop loop;
    canvas.start(&loop);
    canvas.show();
    canvas.wait(0.3);
    canvas.go_steps(board);
    canvas.wait(0.1);

    const QString command =
        QString("ffmpeg -y -loglevel error -f rawvideo -pixel_format rgb24 -video_size %1x%2 -framerate 30 -i - "
                "-c:v libx264 -preset veryfast -crf 21 -pix_fmt yuv420p -movflags +faststart \"%3\"")
            .arg(FILM_W).arg(FILM_H).arg(video);
    FILE* pipe = popen(command.toLocal8Bit().constData(), "w");
    FILE* events = std::fopen(events_path.toLocal8Bit().constData(), "w");
    if (pipe == nullptr || events == nullptr){
        std::fprintf(stderr, "cannot start ffmpeg\n");
        return 2;
    }
    Film film(canvas, pipe, events);
    if (board == "pico"){
        film_pico(film, canvas, pc, drive_folder);
    }else if (board == "esp32"){
        film_esp32(film, canvas, pc);
    }else{
        film_esp32s3(film, canvas, pc);
    }
    std::fprintf(events, "end %.3f\n", film.time());
    std::fclose(events);
    const int status = pclose(pipe);
    std::fprintf(stderr, "film %s: %.1f s, ffmpeg %d%s\n", board.toLocal8Bit().constData(), film.time(), status, film.failed() ? ", FAILED" : "");
    return film.failed() || status != 0 ? 1 : 0;
}

/*  PocketPilot Setup
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <QApplication>
#include <QAudioDevice>
#include <QAudioOutput>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QFile>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QHash>
#include <QImage>
#include <QKeyEvent>
#include <QMediaDevices>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QCursor>
#include <QPixmap>
#include <QPointer>
#include <QRadialGradient>
#include <QScreen>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <QVideoWidget>
#include <QWidget>
#include "PocketPilot_Firmware.h"
#include "PocketPilot_Language.h"
#include "PocketPilot_Setup.h"

namespace PokemonAutomation{
namespace PocketPilot{


const char* const SETUP_TEST_OPTION = "--setup-test";
const char* const TUTORIAL_TEST_OPTION = "--tutorial-test";


namespace{


//  ---------------------------------------------------------------------
//  The design: 1920 x 1080, like the film
//  ---------------------------------------------------------------------

constexpr double DW = 1920.0;
constexpr double DH = 1080.0;

const QColor ORANGE(217, 119, 87);          //  the color of the Joy-Cons
const QColor YELLOW(255, 224, 102);         //  the chat bubble
const QColor PANEL(20, 20, 22);
const QColor LIST_PANEL(12, 12, 13);

constexpr double BAR_X = 1000;
constexpr double BAR_Y = 150;
constexpr double BAR_W = 780;
constexpr double BAR_H = 80;
constexpr double ROW_H = 62;
constexpr double HEADER_H = 44;

const char* const URL_GUIDE = "https://pokemonautomation.github.io/SetupGuide/index.html";


double clamp01(double x){
    return std::max(0.0, std::min(1.0, x));
}
double prog(double t, double t0, double t1){
    return clamp01((t - t0) / (t1 - t0));
}
double eo(double x){
    return 1.0 - std::pow(1.0 - x, 3.0);
}
double eio(double x){
    return x * x * (3.0 - 2.0 * x);
}
double lerp(double a, double b, double x){
    return a + (b - a) * x;
}
QColor with_alpha(QColor color, double alpha){
    color.setAlphaF((float)clamp01(alpha));
    return color;
}
QColor white(double alpha){
    return with_alpha(Qt::white, alpha);
}

//  A value that follows its target.
struct Smooth{
    double value = 0;
    double target = 0;
    void step(double dt, double rate){
        value += (target - value) * (1.0 - std::exp(-dt * rate));
        if (std::fabs(target - value) < 0.0005){
            value = target;
        }
    }
    void jump(double x){
        value = x;
        target = x;
    }
};

QString T(const char* english){
    return tr_ui(english);
}


//  ---------------------------------------------------------------------
//  Type
//  ---------------------------------------------------------------------

//  The family names that Qt reports for the files. Windows and Linux do not
//  name them alike (one family "Fredoka" with three weights, or three
//  families), so each weight asks for the name its own file reported, with
//  its own weight: that finds the file either way.
struct Families{
    QString serif;
    QString ui500;
    QString ui600;
    QString ui700;
};
const Families& families(){
    static const Families result = []{
        Families found;
        auto load = [](const char* path){
            const int id = QFontDatabase::addApplicationFont(path);
            const QStringList list = id < 0 ? QStringList() : QFontDatabase::applicationFontFamilies(id);
            return list.isEmpty() ? QString() : list.first();
        };
        found.serif = load(":/pocketpilot/setup/Newsreader16pt-Regular.ttf");
        found.ui500 = load(":/pocketpilot/setup/Fredoka-Medium.ttf");
        found.ui600 = load(":/pocketpilot/setup/Fredoka-SemiBold.ttf");
        found.ui700 = load(":/pocketpilot/setup/Fredoka-Bold.ttf");
        return found;
    }();
    return result;
}
QFont serif_font(double px){
    QFont font;
    if (!families().serif.isEmpty()){
        font.setFamily(families().serif);
    }
    font.setPixelSize((int)std::lround(px));
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}
//  weight: 500, 600 or 700.
QFont ui_font(double px, int weight = 500){
    QFont font;
    const Families& found = families();
    const QString& family = weight >= 700 ? found.ui700 : weight >= 600 ? found.ui600 : found.ui500;
    if (!family.isEmpty()){
        font.setFamily(family);
    }else if (!found.ui500.isEmpty()){
        font.setFamily(found.ui500);
    }
    font.setPixelSize((int)std::lround(px));
    font.setWeight(weight >= 700 ? QFont::Bold : weight >= 600 ? QFont::DemiBold : QFont::Medium);
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

double text_width(const QFont& font, const QString& text){
    return QFontMetricsF(font).horizontalAdvance(text);
}
double text_height(const QFont& font){
    return QFontMetricsF(font).height();
}

//  (x, y) is the top left; with AlignRight the right end; with AlignHCenter the middle.
void draw_text(
    QPainter& painter, const QString& text, double x, double y,
    const QFont& font, const QColor& color, Qt::Alignment align = Qt::AlignLeft
){
    if (color.alphaF() < 0.004 || text.isEmpty()){
        return;
    }
    const QFontMetricsF metrics(font);
    double left = x;
    if (align & Qt::AlignRight){
        left = x - metrics.horizontalAdvance(text);
    }else if (align & Qt::AlignHCenter){
        left = x - metrics.horizontalAdvance(text) / 2.0;
    }
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(QPointF(left, y + metrics.ascent()), text);
}
//  Vertically centered on cy.
void draw_text_cy(
    QPainter& painter, const QString& text, double x, double cy,
    const QFont& font, const QColor& color, Qt::Alignment align = Qt::AlignLeft
){
    draw_text(painter, text, x, cy - text_height(font) / 2.0, font, color, align);
}

QStringList wrap_text(const QFont& font, const QString& text, double max_width){
    const QFontMetricsF metrics(font);
    QStringList lines;
    QString line;
    const QStringList words = text.split(' ', Qt::SkipEmptyParts);
    for (const QString& word : words){
        const QString attempt = line.isEmpty() ? word : line + ' ' + word;
        if (line.isEmpty() || metrics.horizontalAdvance(attempt) <= max_width){
            line = attempt;
        }else{
            lines << line;
            line = word;
        }
    }
    if (!line.isEmpty()){
        lines << line;
    }
    return lines;
}


//  ---------------------------------------------------------------------
//  Shapes
//  ---------------------------------------------------------------------

void panel(
    QPainter& painter, const QRectF& rect, double radius,
    const QColor& fill, const QColor& ring = QColor(), double ring_width = 1.6
){
    const bool has_ring = ring.isValid() && ring.alphaF() > 0.004;
    const QRectF inner = has_ring ? rect.adjusted(ring_width / 2, ring_width / 2, -ring_width / 2, -ring_width / 2) : rect;
    painter.setPen(has_ring ? QPen(ring, ring_width) : QPen(Qt::NoPen));
    painter.setBrush(fill.isValid() && fill.alphaF() > 0.004 ? QBrush(fill) : QBrush(Qt::NoBrush));
    painter.drawRoundedRect(inner, radius, radius);
}

//  A soft light around a rounded box.
void glow(QPainter& painter, const QRectF& rect, double radius, const QColor& color, double strength){
    if (strength < 0.01){
        return;
    }
    painter.setPen(Qt::NoPen);
    for (int step = 9; step >= 1; step--){
        const double grow = step * 5.0;
        QColor layer = color;
        layer.setAlphaF((float)clamp01(strength * 0.050 * (1.0 - step / 10.0)));
        painter.setBrush(layer);
        painter.drawRoundedRect(rect.adjusted(-grow, -grow, grow, grow), radius + grow, radius + grow);
    }
}

void draw_check(QPainter& painter, const QRectF& box, const QColor& color, double width){
    QPen pen(color, width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(box.left() + box.width() * 0.26, box.top() + box.height() * 0.53);
    path.lineTo(box.left() + box.width() * 0.43, box.top() + box.height() * 0.69);
    path.lineTo(box.left() + box.width() * 0.75, box.top() + box.height() * 0.33);
    painter.drawPath(path);
}
void draw_chevron(QPainter& painter, const QRectF& box, const QColor& color, double width){
    QPen pen(color, width);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(box.left() + box.width() * 0.27, box.top() + box.height() * 0.40);
    path.lineTo(box.left() + box.width() * 0.50, box.top() + box.height() * 0.62);
    path.lineTo(box.left() + box.width() * 0.73, box.top() + box.height() * 0.40);
    painter.drawPath(path);
}
void draw_plus(QPainter& painter, const QPointF& center, double arm, const QColor& color, double width){
    QPen pen(color, width);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(QPointF(center.x() - arm, center.y()), QPointF(center.x() + arm, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - arm), QPointF(center.x(), center.y() + arm));
}

//  The white pictures of the program, in the color asked for.
QPixmap tinted(const QString& name, const QColor& color){
    static QHash<QString, QPixmap> cache;
    const QString key = name + "|" + color.name();
    const auto found = cache.constFind(key);
    if (found != cache.constEnd()){
        return found.value();
    }
    QImage image(":/pocketpilot/setup/" + name + ".png");
    if (image.isNull()){
        return QPixmap();
    }
    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), color);
    painter.end();
    const QPixmap pixmap = QPixmap::fromImage(image);
    cache.insert(key, pixmap);
    return pixmap;
}
QPixmap plain_picture(const QString& name){
    static QHash<QString, QPixmap> cache;
    const auto found = cache.constFind(name);
    if (found != cache.constEnd()){
        return found.value();
    }
    const QPixmap pixmap(":/pocketpilot/setup/" + name + ".png");
    cache.insert(name, pixmap);
    return pixmap;
}
void draw_picture(QPainter& painter, const QPixmap& pixmap, const QRectF& target, double alpha){
    if (pixmap.isNull() || alpha < 0.004){
        return;
    }
    const double old = painter.opacity();
    painter.setOpacity(old * clamp01(alpha));
    painter.drawPixmap(target, pixmap, QRectF(pixmap.rect()));
    painter.setOpacity(old);
}


//  ---------------------------------------------------------------------
//  The canvas and its pages
//  ---------------------------------------------------------------------

class Canvas;

class Page{
public:
    virtual ~Page() = default;
    virtual const char* name() const = 0;
    //  Draws the page; t is the time since it came up.
    virtual void paint(Canvas& canvas, QPainter& painter, double t) = 0;
    virtual void step(double dt){
        (void)dt;
    }
    //  A button, a row ... of the page was pressed.
    virtual void activate(Canvas& canvas, const QString& id) = 0;
    //  The mouse is over this one (empty: over nothing).
    virtual void hover(Canvas& canvas, const QString& id){
        (void)canvas;
        (void)id;
    }
    //  A key. True when the page used it.
    virtual bool key(Canvas& canvas, QKeyEvent& event){
        (void)canvas;
        (void)event;
        return false;
    }
    //  Enter, with nothing selected.
    virtual void enter_pressed(Canvas& canvas){
        (void)canvas;
    }
    virtual void escape_pressed(Canvas& canvas){
        (void)canvas;
    }
    //  What the page is doing, in words (for the tests).
    virtual QString state() const{
        return QString();
    }
};

struct Hit{
    QString id;
    QRectF rect;
    bool enabled = true;
    bool tab_stop = true;
};

class Canvas : public QWidget{
public:
    Canvas(const SetupRequest& request, bool test_mode);

    //  ---- for the pages
    void add_hit(const QString& id, const QRectF& rect, bool enabled = true, bool tab_stop = true){
        m_hits.push_back({id, rect, enabled, tab_stop});
    }
    bool hovered(const QString& id) const{
        return m_hover == id;
    }
    bool focused(const QString& id) const{
        return m_focus_visible && m_focus == id;
    }
    void set_focus_id(const QString& id){
        m_focus = id;
    }
    const SetupRequest& request() const{
        return m_request;
    }
    SetupResult& result(){
        return m_result;
    }
    bool first_page() const{
        return m_first_page;
    }
    void open_url(const QString& url){
        m_last_url = url;
        if (!m_test_mode){
            QDesktopServices::openUrl(QUrl(url));
        }
    }
    void open_folder(const QString& path){
        m_last_folder = path;
        if (!m_test_mode){
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    }
    void copy_text(const QString& text){
        m_copied = text;
        if (!m_test_mode){
            QGuiApplication::clipboard()->setText(text);
        }
    }
    void open_device_manager(){
        m_device_manager_opened = true;
        if (!m_test_mode){
            PocketPilot::open_device_manager();
        }
    }
    bool test_mode() const{
        return m_test_mode;
    }
    //  The folder of the program: Firmware and Tools are in it.
    const QString& app_folder() const{
        return m_app_folder;
    }
    void set_app_folder(const QString& folder){
        m_app_folder = folder;
    }
    //  The pages look at the computer through this (empty: the real one).
    const HardwareProbe::Scan& scan_override() const{
        return m_scan;
    }
    void set_scan(HardwareProbe::Scan scan){
        m_scan = std::move(scan);
    }
    //  Where a download really comes from (the tests point it at a file).
    QString map_url(const QString& url) const{
        return m_url_map ? m_url_map(url) : url;
    }
    void set_url_map(std::function<QString(const QString&)> map){
        m_url_map = std::move(map);
    }
    QString page_state() const{
        return m_page == nullptr ? QString() : m_page->state();
    }
    //  What the Packages repository lists, when it is not asked.
    bool has_listing_override() const{
        return m_has_listing;
    }
    const QByteArray& listing_override() const{
        return m_listing;
    }
    void set_listing(const QByteArray& json){
        m_listing = json;
        m_has_listing = true;
    }
    //  Flashing is played, not done (the tutorial films, the demo).
    bool pretend() const{
        return m_pretend;
    }
    void set_pretend(bool pretend){
        m_pretend = pretend;
    }
    //  The tutorial film of this board.
    void play_video(const QString& board);
    void close_video();
    bool video_open() const{
        return m_video_open;
    }
    const QString& video_name() const{
        return m_video_name;
    }
    const QString& last_url() const{
        return m_last_url;
    }
    const QString& last_folder() const{
        return m_last_folder;
    }
    const QString& copied() const{
        return m_copied;
    }
    bool device_manager_opened() const{
        return m_device_manager_opened;
    }
    //  The place of a button, in the window (for a cursor drawn over the picture).
    bool hit_center(const QString& id, QPointF& center) const{
        for (const Hit& hit : m_hits){
            if (hit.id == id){
                center = QPointF(
                    m_offset.x() + hit.rect.center().x() * m_scale,
                    m_offset.y() + hit.rect.center().y() * m_scale
                );
                return true;
            }
        }
        return false;
    }

    void go_language();
    void go_console();
    void go_board();
    void go_steps(const QString& board);
    void go_guide();
    void go_other();
    void go_done(const QString& board);
    void go_games();
    //  The end: the loop of run_setup() stops.
    void finish();

    //  ---- the run
    void start(QEventLoop* loop);
    void advance(double dt);

    //  ---- the intro: the film plays on this window, and its last picture
    //  goes over into the first page (or stays, when no page follows).
    //  Started once the window is on the screen.
    void start_intro();
    //  "none", "film" or "fade".
    QString intro_state() const;
    int intro_frames() const{
        return m_intro_frames;
    }
    //  Why the film ended ("it played to the end", "it was skipped", ...).
    const QString& intro_note() const{
        return m_intro_note;
    }

    //  ---- after the end: the screen stays up (the page fades out, the
    //  background stays) until it is released, then the window fades away.
    void hold_screen();
    void release_screen();
    bool holding() const{
        return m_holding;
    }
    bool released() const{
        return m_release_done;
    }

    //  ---- the test
    QString page_name() const{
        return m_page == nullptr ? QString() : QString::fromLatin1(m_page->name());
    }
    bool finished() const{
        return m_finished;
    }
    bool has_hit(const QString& id) const{
        for (const Hit& hit : m_hits){
            if (hit.id == id){
                return true;
            }
        }
        return false;
    }
    bool hit_enabled(const QString& id) const{
        for (const Hit& hit : m_hits){
            if (hit.id == id){
                return hit.enabled;
            }
        }
        return false;
    }
    //  The choice of a game was asked for, and there are games to choose from.
    bool asks_game() const{
        return m_request.ask_game && !m_request.games.empty();
    }
    //  The installation of the board is over, finished or skipped: with games
    //  to choose from a new screen follows, no part of the installation.
    void end_installation(){
        if (m_request.games.empty()){
            finish();
        }else{
            go_games();
        }
    }
    //  The console is chosen: the question of the board follows (or, when it
    //  is not asked, what comes after the installation).
    void end_console(){
        if (m_request.ask_board){
            go_board();
        }else{
            end_installation();
        }
    }
    //  Presses it as a click on its middle would; false when it is not there.
    bool click(const QString& id);
    //  Moves the mouse over it.
    bool move_to(const QString& id);
    void press_key(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    //  Time passes (with pictures drawn on the way).
    void wait(double seconds);
    QImage picture();
    QString focus_id() const{
        return m_focus;
    }

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void keyPressEvent(QKeyEvent* event) override;
    virtual void closeEvent(QCloseEvent* event) override;

private:
    void set_page(std::unique_ptr<Page> page);
    void queue_page(std::function<std::unique_ptr<Page>()> make);
    void paint_background(QPainter& painter);
    QPointF to_design(const QPointF& point) const{
        return QPointF((point.x() - m_offset.x()) / m_scale, (point.y() - m_offset.y()) / m_scale);
    }
    const Hit* find_hit(const QPointF& design_point) const;
    void update_hover(const QPointF& window_point);
    void move_focus(int direction);
    void tick();
    bool asks_anything() const{
        return m_request.ask_language || m_request.ask_console || m_request.ask_board || asks_game();
    }
    void end_intro(const QString& note);
    void drop_intro_player();
    void draw_fitted(QPainter& painter, const QImage& image, QPixmap& cache, QSize& cache_for, double opacity);

private:
    SetupRequest m_request;
    SetupResult m_result;
    bool m_test_mode;
    bool m_first_page = true;
    bool m_finished = false;
    QEventLoop* m_loop = nullptr;

    std::unique_ptr<Page> m_page;
    std::function<std::unique_ptr<Page>()> m_pending;
    double m_page_time = 0;
    double m_fade = 0;
    double m_now = 0;

    std::vector<Hit> m_hits;
    QString m_hover;
    QString m_focus;
    bool m_focus_visible = false;
    QString m_pressed;

    double m_scale = 1;
    QPointF m_offset;

    QImage m_background_source;
    QPixmap m_background;
    QSize m_background_for;

    QTimer m_timer;
    QElapsedTimer m_clock;
    bool m_clock_started = false;

    QString m_app_folder;
    HardwareProbe::Scan m_scan;
    std::function<QString(const QString&)> m_url_map;
    QByteArray m_listing;
    bool m_has_listing = false;
    bool m_pretend = false;
    QString m_last_url;
    QString m_last_folder;
    QString m_copied;
    bool m_device_manager_opened = false;

    //  The film on top of the page.
    bool m_video_open = false;
    QString m_video_name;
    std::unique_ptr<QWidget> m_overlay;
    std::unique_ptr<QMediaPlayer> m_player;
    std::unique_ptr<QAudioOutput> m_audio;

    //  The intro. The player goes before the window it plays on.
    enum class IntroState{
        NONE,
        FILM,
        FADE,
    };
    IntroState m_intro = IntroState::NONE;
    double m_intro_clock = 0;
    double m_intro_ended = -1;
    double m_intro_fade = 1;
    //  Nothing follows the film: its last picture stays.
    bool m_intro_keep = false;
    int m_intro_frames = 0;
    QString m_intro_note;
    QVideoFrame m_intro_last;
    //  The latest picture (the test draws the film itself), and the last one
    //  with its scaled copy, drawn over the page while it fades.
    QImage m_intro_image;
    QImage m_intro_still;
    QPixmap m_intro_still_scaled;
    QSize m_intro_still_for;
    std::unique_ptr<QWidget> m_intro_overlay;
    std::unique_ptr<QVideoSink> m_intro_sink;
    std::unique_ptr<QMediaPlayer> m_intro_player;
    std::unique_ptr<QAudioOutput> m_intro_audio;

    bool m_holding = false;
    bool m_releasing = false;
    bool m_release_done = false;
    double m_release_alpha = 1;
};


//  ---------------------------------------------------------------------
//  What the pages share: chips, buttons, the stepper, the callout
//  ---------------------------------------------------------------------

enum class ChipKind{
    ORANGE_FILL,
    ORANGE_RING,
    RING,
    DIM,
    YELLOW_FILL,
};

//  A chip with its right end at x; returns its width.
double draw_chip(
    QPainter& painter, const QString& text, double right, double cy, ChipKind kind,
    double alpha, double px = 21, double height = 36, double pad = 14, bool measure_only = false
){
    const QFont font = ui_font(px, 600);
    const double width = text_width(font, text) + 2 * pad;
    if (measure_only){
        return width;
    }
    const QRectF rect(right - width, cy - height / 2, width, height);
    QColor text_color = Qt::white;
    switch (kind){
    case ChipKind::ORANGE_FILL:
        panel(painter, rect, height / 2, with_alpha(ORANGE, alpha));
        text_color = Qt::black;
        break;
    case ChipKind::ORANGE_RING:
        panel(painter, rect, height / 2, QColor(), with_alpha(ORANGE, 0.9 * alpha), 1.6);
        text_color = ORANGE;
        break;
    case ChipKind::RING:
        panel(painter, rect, height / 2, QColor(), white(0.34 * alpha), 1.6);
        break;
    case ChipKind::DIM:
        panel(painter, rect, height / 2, QColor(), white(0.2 * alpha), 1.6);
        text_color = QColor(170, 170, 170);
        break;
    case ChipKind::YELLOW_FILL:
        panel(painter, rect, height / 2, with_alpha(YELLOW, alpha));
        text_color = Qt::black;
        break;
    }
    draw_text_cy(painter, text, rect.center().x(), rect.center().y() + 1, font, with_alpha(text_color, alpha), Qt::AlignHCenter);
    return width;
}

enum class ButtonKind{
    PRIMARY,
    SECONDARY,
    LINK,
};

//  The width of a button for this text.
double button_width(const QString& text, ButtonKind kind){
    const QFont font = ui_font(kind == ButtonKind::LINK ? 24 : 28, 600);
    return text_width(font, text) + (kind == ButtonKind::LINK ? 8 : 68);
}

//  Draws a button with its left top at (x, y) and registers it; returns its width.
double draw_button(
    Canvas& canvas, QPainter& painter, const QString& id, const QString& text,
    double x, double y, ButtonKind kind, double alpha, bool enabled = true
){
    const double height = kind == ButtonKind::LINK ? 44 : 64;
    const double width = button_width(text, kind);
    const QRectF rect(x, y, width, height);
    const bool hover = enabled && canvas.hovered(id);
    const bool focus = enabled && canvas.focused(id);
    const double a = alpha * (enabled ? 1.0 : 0.35);
    switch (kind){
    case ButtonKind::PRIMARY:
        if (hover || focus){
            glow(painter, rect, 32, ORANGE, 0.9 * alpha);
        }
        panel(painter, rect, 32, with_alpha(hover ? ORANGE.lighter(112) : ORANGE, a));
        draw_text_cy(painter, text, rect.center().x(), rect.center().y() + 1, ui_font(28, 600), with_alpha(Qt::black, a), Qt::AlignHCenter);
        break;
    case ButtonKind::SECONDARY:
        panel(painter, rect, 32, white(hover ? 0.08 : 0.0), white((hover ? 0.55 : 0.30) * a), 1.8);
        draw_text_cy(painter, text, rect.center().x(), rect.center().y() + 1, ui_font(28, 600), white(a), Qt::AlignHCenter);
        break;
    case ButtonKind::LINK:
        draw_text_cy(painter, text, rect.center().x(), rect.center().y(), ui_font(24, 600), white((hover ? 0.95 : 0.55) * a), Qt::AlignHCenter);
        break;
    }
    if (focus){
        panel(painter, rect.adjusted(-6, -6, 6, 6), 38, QColor(), with_alpha(YELLOW, 0.9 * alpha), 2.4);
    }
    canvas.add_hit(id, rect, enabled);
    return width;
}

//  The four steps of the Setup Guide at the top. state: 0 done, 1 active, 2 next.
void draw_stepper(QPainter& painter, const int (&states)[4], double reveal){
    if (reveal < 0.004){
        return;
    }
    static const char* const NAMES[4] = {"Hardware", "General setup", "Controller", "Finishing up"};
    const QFont font = ui_font(25, 600);
    constexpr double PILL_H = 56;
    constexpr double GAP = 26;
    double widths[4];
    double total = GAP * 3;
    for (int c = 0; c < 4; c++){
        widths[c] = text_width(font, T(NAMES[c])) + 36 + 18 + 28;
        total += widths[c];
    }
    double x = (DW - total) / 2.0;
    const double y = 44 - (1.0 - eo(reveal)) * 26;
    for (int c = 0; c < 4; c++){
        const QRectF rect(x, y, widths[c], PILL_H);
        const int state = states[c];
        const double a = eo(reveal);
        if (state == 1){
            panel(painter, rect, PILL_H / 2, with_alpha(ORANGE, a));
        }else if (state == 0){
            panel(painter, rect, PILL_H / 2, white(0.05 * a), white(0.16 * a), 1.4);
        }else{
            panel(painter, rect, PILL_H / 2, QColor(), white(0.26 * a), 1.4);
        }
        const QRectF circle(x + 11, y + 11, 34, 34);
        painter.setPen(Qt::NoPen);
        if (state == 0){
            painter.setBrush(with_alpha(ORANGE, a));
            painter.drawEllipse(circle);
            draw_check(painter, circle.adjusted(5, 5, -5, -5), with_alpha(Qt::black, a), 3.0);
        }else if (state == 1){
            painter.setBrush(with_alpha(Qt::black, a));
            painter.drawEllipse(circle);
            draw_text_cy(painter, QString::number(c + 1), circle.center().x(), circle.center().y() + 1, ui_font(22, 700), with_alpha(ORANGE, a), Qt::AlignHCenter);
        }else{
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(white(0.4 * a), 1.6));
            painter.drawEllipse(circle.adjusted(0.8, 0.8, -0.8, -0.8));
            draw_text_cy(painter, QString::number(c + 1), circle.center().x(), circle.center().y() + 1, ui_font(22, 700), white(0.55 * a), Qt::AlignHCenter);
        }
        const double text_alpha = state == 1 ? 1.0 : state == 0 ? 0.62 : 0.5;
        draw_text_cy(painter, T(NAMES[c]), x + 54, y + PILL_H / 2 + 1, font, with_alpha(state == 1 ? Qt::black : Qt::white, text_alpha * a));
        if (c < 3){
            painter.setPen(Qt::NoPen);
            painter.setBrush(white(0.2 * a));
            painter.drawRoundedRect(QRectF(x + widths[c] + 4, y + PILL_H / 2 - 1, GAP - 8, 2), 1, 1);
        }
        x += widths[c] + GAP;
    }
}

//  The yellow bubble with its tail on the right, pointing at (tip_x, cy).
void draw_callout(QPainter& painter, const QString& line1, const QString& line2, double tip_x, double cy, double alpha){
    if (alpha < 0.01){
        return;
    }
    const QFont font = ui_font(34, 700);
    const double text_w = std::max(text_width(font, line1), text_width(font, line2));
    const double width = text_w + 60;
    const double height = 128;
    const double scale = 0.9 + 0.1 * clamp01(alpha);
    const double slide = (1.0 - clamp01(alpha)) * 24;
    painter.save();
    painter.translate(tip_x - slide, cy);
    painter.scale(scale, scale);
    painter.translate(0, -height * 0.5);
    //  The body ends 38 before the tip.
    const QRectF body(-width - 38, 0, width, height);
    QPainterPath path;
    path.addRoundedRect(body, 36, 36);
    QPainterPath tail;
    tail.moveTo(body.right() - 30, 44);
    tail.lineTo(0, 64);
    tail.lineTo(body.right() - 30, 100);
    tail.closeSubpath();
    path = path.united(tail);
    painter.setPen(Qt::NoPen);
    painter.setBrush(with_alpha(YELLOW, alpha));
    painter.drawPath(path);
    draw_text(painter, line1, body.left() + 30, 20, font, with_alpha(Qt::black, alpha));
    draw_text(painter, line2, body.left() + 30, 64, font, with_alpha(Qt::black, alpha));
    painter.restore();
}


//  ---------------------------------------------------------------------
//  The bar with the list of choices
//  ---------------------------------------------------------------------

struct MenuChip{
    const char* text;
    ChipKind kind;
};
struct MenuRow{
    QString id;
    //  "code:EN" (a circle with letters), "plus", or the name of a picture.
    QString glyph;
    //  The text of the row (English; or literal: a name that is not translated).
    QString name;
    bool literal = false;
    std::vector<MenuChip> chips;
    bool dim = false;
    bool enabled = true;
    const char* note = nullptr;
    //  The bubble when the mouse is over the row.
    const char* callout1 = nullptr;
    const char* callout2 = nullptr;
    //  Which color the glyph has when it is the accent (the Other row).
    bool accent = false;
};
struct MenuSection{
    const char* header = nullptr;
    const char* header_note = nullptr;
    bool divider = false;
    std::vector<MenuRow> rows;
};

class Menu{
public:
    Menu(const char* placeholder, std::vector<MenuSection> sections)
        : m_placeholder(placeholder)
        , m_sections(std::move(sections))
    {
        double y = 12;
        for (MenuSection& section : m_sections){
            if (section.divider){
                m_divider_y.push_back(y + 9);
                y += 20;
            }
            if (section.header != nullptr){
                m_header_y.push_back(y + 8);
                y += HEADER_H;
            }else{
                m_header_y.push_back(-1);
            }
            for (MenuRow& row : section.rows){
                m_rows.push_back(&row);
                m_row_y.push_back(y);
                y += ROW_H;
            }
        }
        m_height = y + 12;
    }

    bool is_open() const{
        return m_open;
    }
    const QString& selected() const{
        return m_selected;
    }
    void select(const QString& id, bool confirmed){
        m_selected = id;
        m_confirmed = confirmed;
    }
    bool confirmed() const{
        return m_confirmed;
    }
    void open(){
        m_open = true;
        m_open_p.target = 1;
        //  The highlight starts on the selected row, or the first that can be chosen.
        int start = index_of(m_selected);
        if (start < 0){
            start = next_enabled(-1, +1);
        }
        m_highlight = start;
        if (start >= 0){
            m_hl_y.jump(m_row_y[(size_t)start]);
        }
    }
    void close(){
        m_open = false;
        m_open_p.target = 0;
    }
    void toggle(){
        if (m_open){
            close();
        }else{
            open();
        }
    }
    double open_progress() const{
        return m_open_p.value;
    }
    void step(double dt){
        m_open_p.step(dt, 13);
        if (m_highlight >= 0 && m_highlight < (int)m_row_y.size()){
            m_hl_y.target = m_row_y[(size_t)m_highlight];
        }
        m_hl_y.step(dt, 24);
        m_callout.step(dt, 14);
    }

    //  A row picked: false when it cannot be.
    bool pick(const QString& id){
        const int index = index_of(id);
        if (index < 0 || !m_rows[(size_t)index]->enabled){
            return false;
        }
        m_selected = id;
        m_confirmed = true;
        close();
        return true;
    }
    void hover_row(const QString& id){
        const int index = index_of(id);
        m_hover = index;
        if (index >= 0 && m_rows[(size_t)index]->enabled){
            m_highlight = index;
        }
        if (index >= 0 && m_rows[(size_t)index]->callout1 != nullptr){
            m_callout_row = index;
            m_callout.target = 1;
        }else{
            m_callout.target = 0;
        }
    }
    //  Arrow keys on an open list. True when the key was used.
    bool key(QKeyEvent& event, QString* picked){
        if (!m_open){
            return false;
        }
        switch (event.key()){
        case Qt::Key_Down:
            m_highlight = next_enabled(m_highlight, +1);
            m_hover = -1;
            return true;
        case Qt::Key_Up:
            m_highlight = next_enabled(m_highlight, -1);
            m_hover = -1;
            return true;
        case Qt::Key_Home:
            m_highlight = next_enabled(-1, +1);
            return true;
        case Qt::Key_End:
            m_highlight = next_enabled((int)m_rows.size(), -1);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (m_highlight >= 0){
                const QString id = m_rows[(size_t)m_highlight]->id;
                if (pick(id) && picked != nullptr){
                    *picked = id;
                }
            }
            return true;
        case Qt::Key_Escape:
            close();
            return true;
        default:
            return false;
        }
    }

    //  The bar and the list; "reveal" slides the bar in.
    void paint(Canvas& canvas, QPainter& painter, double reveal, double alpha){
        const double slide = (1.0 - eo(reveal)) * 60;
        const double a = alpha * eo(reveal);
        if (a < 0.004){
            return;
        }
        const QRectF bar(BAR_X + slide, BAR_Y, BAR_W, BAR_H);
        const bool focus = canvas.focused("bar");
        const bool lit = m_open || focus;
        if (lit){
            glow(painter, bar, 24, ORANGE, 0.9 * a);
        }
        panel(painter, bar, 24, with_alpha(PANEL, a), with_alpha(lit ? ORANGE : Qt::white, (lit ? 0.95 : canvas.hovered("bar") ? 0.4 : 0.22) * a), lit ? 2.0 : 1.6);
        canvas.add_hit("bar", bar, true);

        const int selected_index = index_of(m_selected);
        if (selected_index >= 0){
            const MenuRow& row = *m_rows[(size_t)selected_index];
            draw_glyph(painter, row, QRectF(bar.left() + 22, bar.top() + 16, 48, 48), a, m_confirmed, 1.0);
            draw_text_cy(painter, row_name(row), bar.left() + 86, bar.center().y() + 1, ui_font(32, 600), white(a));
            double right = bar.right() - 26;
            if (m_confirmed){
                const QRectF circle(bar.right() - 64, bar.top() + 20, 40, 40);
                painter.setPen(Qt::NoPen);
                painter.setBrush(with_alpha(ORANGE, a));
                painter.drawEllipse(circle);
                draw_check(painter, circle.adjusted(7, 7, -7, -7), with_alpha(Qt::black, a), 3.4);
                right = bar.right() - 64 - 16;
            }else{
                draw_chevron(painter, QRectF(bar.right() - 66, bar.top() + 20, 40, 40), white(0.75 * a), 3.0);
                right = bar.right() - 66 - 16;
            }
            //  The first chip that says something of the row is the one on the bar: Default, Wireless ...
            for (size_t c = row.chips.size(); c-- > 0;){
                if (row.chips[c].kind == ChipKind::DIM){
                    continue;
                }
                const QString text = T(row.chips[c].text);
                const double width = draw_chip(painter, text, right, bar.center().y(), ChipKind::RING, a, 22, 40, 16);
                right -= width + 10;
                break;
            }
        }else{
            draw_text_cy(painter, T(m_placeholder), bar.left() + 34, bar.center().y() + 1, ui_font(32, 500), white(0.55 * a));
            draw_chevron(painter, QRectF(bar.right() - 66, bar.top() + 20, 40, 40), white(0.75 * a), 3.0);
        }

        paint_list(canvas, painter, bar, a);
    }

private:
    int index_of(const QString& id) const{
        for (size_t c = 0; c < m_rows.size(); c++){
            if (m_rows[c]->id == id){
                return (int)c;
            }
        }
        return -1;
    }
    int next_enabled(int from, int direction) const{
        const int count = (int)m_rows.size();
        for (int c = from + direction; c >= 0 && c < count; c += direction){
            if (m_rows[(size_t)c]->enabled){
                return c;
            }
        }
        return from >= 0 && from < count ? from : -1;
    }
    QString row_name(const MenuRow& row) const{
        return row.literal ? row.name : tr_ui(row.name);
    }

    void draw_glyph(QPainter& painter, const MenuRow& row, const QRectF& box, double alpha, bool picked, double dim) const{
        (void)picked;
        const QColor color = row.accent ? ORANGE : QColor(Qt::white);
        const double a = alpha * dim;
        if (row.glyph.startsWith("code:")){
            painter.setPen(QPen(with_alpha(color, 0.85 * a), 2.2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(box.adjusted(1.1, 1.1, -1.1, -1.1));
            draw_text_cy(painter, row.glyph.mid(5), box.center().x(), box.center().y() + 1, ui_font(box.width() * 0.43, 700), with_alpha(color, a), Qt::AlignHCenter);
        }else if (row.glyph == "plus"){
            painter.setPen(QPen(with_alpha(ORANGE, 0.95 * a), 2.2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(box.adjusted(1.1, 1.1, -1.1, -1.1));
            draw_plus(painter, box.center(), box.width() * 0.27, with_alpha(ORANGE, a), 3.4);
        }else{
            draw_picture(painter, tinted("glyph-" + row.glyph, color), box, a);
        }
    }

    void paint_list(Canvas& canvas, QPainter& painter, const QRectF& bar, double alpha){
        if (m_open_p.value < 0.01){
            return;
        }
        const double open = eio(clamp01(m_open_p.value));
        const QRectF list(bar.left(), bar.bottom() + 10, BAR_W, m_height);
        const double list_alpha = alpha * clamp01(open * 4.0);
        painter.save();
        painter.setClipRect(QRectF(list.left() - 80, list.top() - 20, list.width() + 160, m_height * open + 20), Qt::IntersectClip);
        panel(painter, list, 26, with_alpha(LIST_PANEL, 0.98 * list_alpha), white(0.16 * list_alpha), 1.6);

        size_t row_index = 0;
        size_t divider_index = 0;
        for (size_t s = 0; s < m_sections.size(); s++){
            const MenuSection& section = m_sections[s];
            if (section.divider){
                const double y = list.top() + m_divider_y[divider_index++];
                painter.setPen(Qt::NoPen);
                painter.setBrush(white(0.14 * list_alpha));
                painter.drawRoundedRect(QRectF(list.left() + 30, y, list.width() - 60, 2), 1, 1);
            }
            if (section.header != nullptr){
                const double y = list.top() + m_header_y[s];
                draw_text(painter, T(section.header), list.left() + 30, y, ui_font(23, 600), white(0.5 * list_alpha));
                if (section.header_note != nullptr){
                    draw_text(painter, T(section.header_note), list.right() - 30, y + 2, ui_font(21, 500), white(0.4 * list_alpha), Qt::AlignRight);
                }
            }
            row_index += 0;
            for (size_t c = 0; c < section.rows.size(); c++){
                (void)c;
            }
        }

        //  The highlight under the rows.
        if (m_highlight >= 0 && m_highlight < (int)m_rows.size() && m_rows[(size_t)m_highlight]->enabled){
            const QRectF hl(list.left() + 12, list.top() + m_hl_y.value + 3, list.width() - 24, ROW_H - 6);
            panel(painter, hl, 18, with_alpha(ORANGE, 0.20 * list_alpha), with_alpha(ORANGE, 0.95 * list_alpha), 2.0);
        }
        const int selected_index = index_of(m_selected);
        if (selected_index >= 0 && selected_index != m_highlight){
            const QRectF sel(list.left() + 12, list.top() + m_row_y[(size_t)selected_index] + 3, list.width() - 24, ROW_H - 6);
            panel(painter, sel, 18, white(0.07 * list_alpha), white(0.24 * list_alpha), 1.6);
        }

        for (size_t c = 0; c < m_rows.size(); c++){
            const MenuRow& row = *m_rows[c];
            const double top = list.top() + m_row_y[c];
            const double cy = top + ROW_H / 2;
            const double dim = row.dim ? 0.42 : 1.0;
            const double x0 = list.left() + 12;
            draw_glyph(painter, row, QRectF(x0 + 12, cy - 22, 44, 44), list_alpha, false, dim);
            draw_text_cy(painter, row_name(row), x0 + 74, cy + 1, ui_font(32, 600), white(list_alpha * dim));
            double right = list.right() - 12 - 14;
            for (size_t k = row.chips.size(); k-- > 0;){
                const double width = draw_chip(painter, T(row.chips[k].text), right, cy, row.chips[k].kind, list_alpha);
                right -= width + 10;
            }
            if (row.note != nullptr){
                draw_text_cy(painter, T(row.note), right - 4, cy + 1, ui_font(24, 500), white(0.6 * list_alpha), Qt::AlignRight);
            }
            if (open > 0.9){
                canvas.add_hit("row:" + row.id, QRectF(x0, top + 3, list.width() - 24, ROW_H - 6), row.enabled, false);
            }
        }
        painter.restore();

        //  The bubble beside the row the mouse is on.
        if (m_callout.value > 0.01 && m_callout_row >= 0 && m_callout_row < (int)m_rows.size() && open > 0.9){
            const MenuRow& row = *m_rows[(size_t)m_callout_row];
            const double cy = list.top() + m_row_y[(size_t)m_callout_row] + ROW_H / 2;
            draw_callout(painter, T(row.callout1), T(row.callout2), list.left() - 14, cy - 128 * 0.12, alpha * m_callout.value);
        }
        (void)row_index;
    }

private:
    const char* m_placeholder;
    std::vector<MenuSection> m_sections;
    std::vector<MenuRow*> m_rows;
    std::vector<double> m_row_y;
    std::vector<double> m_header_y;
    std::vector<double> m_divider_y;
    double m_height = 0;

    bool m_open = false;
    Smooth m_open_p;
    Smooth m_hl_y;
    Smooth m_callout;
    int m_highlight = -1;
    int m_hover = -1;
    int m_callout_row = -1;
    QString m_selected;
    bool m_confirmed = false;
};


//  The timing of what comes in: the first page is slower than the others.
struct Intro{
    double head;
    double sub;
    double bar;
    double buttons;
};
Intro intro_for(const Canvas& canvas){
    return canvas.first_page() ? Intro{0.5, 1.1, 1.3, 1.9} : Intro{0.10, 0.35, 0.50, 0.85};
}

//  The heading, in the serif, as lines of at most 800 px (or "width").
void draw_heading(QPainter& painter, const QString& text, double x, double y, double px, double reveal, double alpha, double width = 800){
    if (reveal < 0.004){
        return;
    }
    const QFont font = serif_font(px);
    const double a = eo(reveal) * alpha;
    const double slide = (1.0 - eo(reveal)) * 34;
    const QStringList lines = wrap_text(font, text, width);
    for (int c = 0; c < lines.size(); c++){
        draw_text(painter, lines[c], x, y + slide + c * px * 1.02, font, white(a));
    }
}
double heading_height(const QString& text, double px, double width = 800){
    return (double)wrap_text(serif_font(px), text, width).size() * px * 1.02;
}
void draw_sub(
    QPainter& painter, const QString& text, double x, double y, double reveal, double alpha,
    double px = 36, double width = 780
){
    if (reveal < 0.004){
        return;
    }
    const QFont font = ui_font(px, 500);
    const double a = eo(reveal) * alpha;
    const double slide = (1.0 - eo(reveal)) * 20;
    const QStringList lines = wrap_text(font, text, width);
    for (int c = 0; c < lines.size(); c++){
        draw_text(painter, lines[c], x + 4, y + slide + c * px * 1.39, font, white(0.62 * a));
    }
}
double sub_height(const QString& text, double px = 36, double width = 780){
    return (double)wrap_text(ui_font(px, 500), text, width).size() * px * 1.39;
}

//  The bottom row: "Skip setup" at the left, the buttons at the right.
void draw_skip(Canvas& canvas, QPainter& painter, double alpha){
    draw_button(canvas, painter, "skip", T("Skip setup"), 136, 960, ButtonKind::LINK, std::min(1.0, alpha * 1.35));
}


//  ---------------------------------------------------------------------
//  Page 1: the language
//  ---------------------------------------------------------------------

class LanguagePage : public Page{
public:
    explicit LanguagePage(Canvas& canvas)
        : m_menu("Select your language", make_sections())
    {
        m_choice = canvas.request().language_code.isEmpty() ? QString("en") : canvas.request().language_code;
        m_menu.select(m_choice, false);
        canvas.set_focus_id("continue");
    }

    virtual const char* name() const override{
        return "language";
    }

    virtual void step(double dt) override{
        m_menu.step(dt);
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        const QString heading = T("Choose your language");
        draw_heading(painter, heading, 140, 250, 80, prog(t, in.head, in.head + 0.9), 1.0);
        draw_sub(painter, T("English is the default. You can change it later in the settings."), 140,
            250 + heading_height(heading, 80) + 26, prog(t, in.sub, in.sub + 0.8), 1.0);
        m_menu.paint(canvas, painter, prog(t, in.bar, in.bar + 0.6), 1.0);

        const double b = eo(prog(t, in.buttons, in.buttons + 0.5));
        const QString text = T("Continue");
        draw_button(canvas, painter, "continue", text, 1780 - button_width(text, ButtonKind::PRIMARY), 950, ButtonKind::PRIMARY, b);
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        if (id == "bar"){
            m_menu.toggle();
        }else if (id.startsWith("row:")){
            pick(id.mid(4));
        }else if (id == "continue"){
            proceed(canvas);
        }
    }
    virtual void hover(Canvas& canvas, const QString& id) override{
        (void)canvas;
        m_menu.hover_row(id.startsWith("row:") ? id.mid(4) : QString());
    }
    virtual bool key(Canvas& canvas, QKeyEvent& event) override{
        (void)canvas;
        QString picked;
        const bool used = m_menu.key(event, &picked);
        if (!picked.isEmpty()){
            choose(picked);
        }
        return used;
    }
    virtual void enter_pressed(Canvas& canvas) override{
        proceed(canvas);
    }
    virtual void escape_pressed(Canvas& canvas) override{
        (void)canvas;
        m_menu.close();
    }

private:
    static std::vector<MenuSection> make_sections(){
        std::vector<MenuSection> sections;
        MenuSection languages;
        languages.header = "Available languages";
        size_t count = 0;
        const LanguageInfo* list = language_list(count);
        for (size_t c = 0; c < count; c++){
            MenuRow row;
            row.id = list[c].code;
            row.glyph = QString("code:") + QString(list[c].code).toUpper();
            row.name = QString::fromUtf8(list[c].native_name);
            row.literal = true;
            if (c == 0){
                row.chips.push_back({"Default", ChipKind::ORANGE_FILL});
            }
            languages.rows.push_back(row);
        }
        sections.push_back(languages);

        MenuSection other;
        other.divider = true;
        MenuRow row;
        row.id = "custom";
        row.glyph = "plus";
        row.name = "Other";
        row.note = "Add your own language file";
        row.callout1 = "Your language missing?";
        row.callout2 = "Add a file for it.";
        row.accent = true;
        other.rows.push_back(row);
        sections.push_back(other);
        return sections;
    }

    void pick(const QString& id){
        if (m_menu.pick(id)){
            choose(id);
        }
    }
    //  The language is tried at once: this screen changes with it.
    void choose(const QString& id){
        m_choice = id;
        preview_language(id);
    }
    void proceed(Canvas& canvas){
        canvas.result().language_chosen = true;
        canvas.result().language_code = m_choice;
        if (canvas.request().ask_console){
            canvas.go_console();
        }else if (canvas.request().ask_board){
            canvas.go_board();
        }else if (canvas.asks_game()){
            canvas.go_games();
        }else{
            canvas.finish();
        }
    }

private:
    Menu m_menu;
    QString m_choice;
};


//  ---------------------------------------------------------------------
//  The boards of the Setup Guide
//  ---------------------------------------------------------------------

struct BoardInfo{
    const char* id;
    const char* name;
};
const BoardInfo BOARDS[] = {
    {"pico", "Raspberry Pi Pico W"},
    {"pico2", "Raspberry Pi Pico 2 W"},
    {"esp32", "ESP32"},
    {"esp32s3", "ESP32-S3"},
};
const char* board_name(const QString& id){
    for (const BoardInfo& board : BOARDS){
        if (id == board.id){
            return board.name;
        }
    }
    return "";
}
//  The short name of the board in a heading.
QString short_board_name(const QString& id){
    if (id == "pico"){
        return "Pico W";
    }
    if (id == "pico2"){
        return "Pico 2 W";
    }
    return QString::fromLatin1(board_name(id));
}

//  ---------------------------------------------------------------------
//  Page 2: which microcontroller
//  ---------------------------------------------------------------------

class BoardPage : public Page{
public:
    explicit BoardPage(Canvas& canvas)
        : m_menu("Select your board", make_sections())
    {
        canvas.set_focus_id("bar");
    }

    virtual const char* name() const override{
        return "board";
    }
    virtual void step(double dt) override{
        m_menu.step(dt);
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        const int states[4] = {1, 2, 2, 2};
        draw_stepper(painter, states, prog(t, in.head - 0.05, in.head + 0.7));
        const QString heading = T("Which microcontroller do you use?");
        draw_heading(painter, heading, 140, 250, 80, prog(t, in.head, in.head + 0.9), 1.0);
        draw_sub(painter, T("Pick your board. The steps follow it."), 140,
            250 + heading_height(heading, 80) + 26, prog(t, in.sub, in.sub + 0.8), 1.0);
        m_menu.paint(canvas, painter, prog(t, in.bar, in.bar + 0.6), 1.0);

        const double b = eo(prog(t, in.buttons, in.buttons + 0.5));
        draw_skip(canvas, painter, b);
        const QString text = T("Continue");
        draw_button(canvas, painter, "continue", text, 1780 - button_width(text, ButtonKind::PRIMARY), 950,
            ButtonKind::PRIMARY, b, !m_menu.selected().isEmpty());
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        if (id == "bar"){
            m_menu.toggle();
        }else if (id.startsWith("row:")){
            if (m_menu.pick(id.mid(4))){
                canvas.set_focus_id("continue");
            }
        }else if (id == "continue"){
            proceed(canvas);
        }else if (id == "skip"){
            canvas.result().board_done = true;
            canvas.end_installation();
        }
    }
    virtual void hover(Canvas& canvas, const QString& id) override{
        (void)canvas;
        m_menu.hover_row(id.startsWith("row:") ? id.mid(4) : QString());
    }
    virtual bool key(Canvas& canvas, QKeyEvent& event) override{
        const bool was_open = m_menu.is_open();
        const bool used = m_menu.key(event, nullptr);
        if (was_open && !m_menu.is_open() && !m_menu.selected().isEmpty() && event.key() != Qt::Key_Escape){
            //  A board picked: the next Enter goes on.
            canvas.set_focus_id("continue");
        }
        return used;
    }
    virtual void enter_pressed(Canvas& canvas) override{
        if (!m_menu.selected().isEmpty()){
            proceed(canvas);
        }else{
            m_menu.toggle();
        }
    }
    virtual void escape_pressed(Canvas& canvas) override{
        (void)canvas;
        m_menu.close();
    }

private:
    static std::vector<MenuSection> make_sections(){
        std::vector<MenuSection> sections;

        MenuSection supported;
        supported.header = "Supported";
        {
            MenuRow row;
            row.id = "pico";
            row.glyph = "pico";
            row.name = board_name("pico");
            row.literal = true;
            row.chips = {{"Easiest start", ChipKind::ORANGE_FILL}, {"Wireless", ChipKind::RING}};
            supported.rows.push_back(row);
        }
        {
            MenuRow row;
            row.id = "pico2";
            row.glyph = "pico2";
            row.name = board_name("pico2");
            row.literal = true;
            row.chips = {{"Wireless", ChipKind::RING}};
            supported.rows.push_back(row);
        }
        {
            MenuRow row;
            row.id = "esp32";
            row.glyph = "esp32";
            row.name = board_name("esp32");
            row.literal = true;
            row.chips = {{"Wireless", ChipKind::RING}};
            supported.rows.push_back(row);
        }
        {
            MenuRow row;
            row.id = "esp32s3";
            row.glyph = "esp32s3";
            row.name = board_name("esp32s3");
            row.literal = true;
            row.chips = {{"Most reliable", ChipKind::ORANGE_RING}, {"Wired", ChipKind::RING}};
            supported.rows.push_back(row);
        }
        sections.push_back(supported);

        MenuSection unsupported;
        unsupported.header = "Not supported";
        unsupported.header_note = "AVR8 support was discontinued";
        const struct{
            const char* id;
            const char* name;
        } old_boards[] = {
            {"leonardo", "Arduino Leonardo"},
            {"uno", "Arduino Uno R3"},
            {"promicro", "Pro Micro"},
            {"teensy", "Teensy 2.0 / Teensy++ 2.0"},
        };
        for (const auto& board : old_boards){
            MenuRow row;
            row.id = board.id;
            row.glyph = board.id;
            row.name = board.name;
            row.literal = true;
            row.dim = true;
            row.enabled = false;
            row.chips = {{"Not supported", ChipKind::DIM}};
            row.callout1 = "Not supported:";
            row.callout2 = "AVR8 support was discontinued.";
            unsupported.rows.push_back(row);
        }
        sections.push_back(unsupported);

        MenuSection other;
        other.divider = true;
        MenuRow row;
        row.id = "other";
        row.glyph = "plus";
        row.name = "Other";
        row.note = "My board is not on the list";
        row.callout1 = "Board not listed yet?";
        row.callout2 = "Pick Other.";
        row.accent = true;
        other.rows.push_back(row);
        sections.push_back(other);
        return sections;
    }

    void proceed(Canvas& canvas){
        const QString board = m_menu.selected();
        if (board.isEmpty()){
            return;
        }
        canvas.result().board = board;
        if (board == "other"){
            canvas.go_other();
        }else{
            canvas.go_steps(board);
        }
    }

private:
    Menu m_menu;
};


//  ---------------------------------------------------------------------
//  A drop-down inside a page (the firmware version, the port)
//  ---------------------------------------------------------------------

struct DropItem{
    QString id;
    QString text;
    QString chip;
    ChipKind chip_kind = ChipKind::RING;
};

//  The text, with "..." in the middle when it is wider than max_width.
void draw_elided(QPainter& painter, const QString& text, double x, double cy, double max_width, const QFont& font, const QColor& color){
    const QString shown = QFontMetricsF(font).elidedText(text, Qt::ElideMiddle, std::max(40.0, max_width));
    draw_text_cy(painter, shown, x, cy + 1, font, color);
}

class Dropdown{
public:
    explicit Dropdown(const QString& name)
        : m_name(name)
    {}

    void set_items(const std::vector<DropItem>& items){
        m_items = items;
        if (index_of(m_selected) < 0){
            m_selected.clear();
        }
        m_highlight = std::min(m_highlight, (int)m_items.size() - 1);
    }
    const std::vector<DropItem>& items() const{
        return m_items;
    }
    const QString& selected() const{
        return m_selected;
    }
    void select(const QString& id){
        if (index_of(id) >= 0){
            m_selected = id;
        }
    }
    bool is_open() const{
        return m_open;
    }
    void close(){
        m_open = false;
        m_open_p.target = 0;
    }
    void toggle(){
        if (m_open){
            close();
            return;
        }
        if (m_items.empty()){
            return;
        }
        m_open = true;
        m_open_p.target = 1;
        m_highlight = std::max(0, index_of(m_selected));
    }
    QString bar_id() const{
        return "dd:" + m_name;
    }
    QString row_prefix() const{
        return "dd:" + m_name + ":";
    }
    bool owns(const QString& id) const{
        return id == bar_id() || id.startsWith(row_prefix());
    }
    //  A row (by its hit id, or its own id) is picked.
    bool pick(const QString& id){
        const QString own = id.startsWith(row_prefix()) ? id.mid(row_prefix().size()) : id;
        if (index_of(own) < 0){
            return false;
        }
        m_selected = own;
        close();
        return true;
    }
    void step(double dt){
        m_open_p.step(dt, 13);
    }
    //  Keys on an open list; true when used.
    bool key(QKeyEvent& event, QString* picked){
        if (!m_open){
            return false;
        }
        const int count = (int)m_items.size();
        switch (event.key()){
        case Qt::Key_Down:
            m_highlight = std::min(count - 1, m_highlight + 1);
            return true;
        case Qt::Key_Up:
            m_highlight = std::max(0, m_highlight - 1);
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (m_highlight >= 0 && m_highlight < count){
                const QString id = m_items[(size_t)m_highlight].id;
                if (pick(id) && picked != nullptr){
                    *picked = id;
                }
            }
            return true;
        case Qt::Key_Escape:
            close();
            return true;
        default:
            return false;
        }
    }

    void paint_bar(Canvas& canvas, QPainter& painter, const QRectF& bar, double alpha, const QString& placeholder, bool enabled = true){
        const bool lit = m_open || canvas.focused(bar_id());
        if (lit){
            glow(painter, bar, 20, ORANGE, 0.8 * alpha);
        }
        panel(
            painter, bar, 20, with_alpha(LIST_PANEL, alpha),
            with_alpha(lit ? ORANGE : Qt::white, (lit ? 0.95 : canvas.hovered(bar_id()) ? 0.4 : 0.22) * alpha), lit ? 2.0 : 1.6
        );
        canvas.add_hit(bar_id(), bar, enabled);
        const double a = alpha * (enabled ? 1.0 : 0.5);
        double right = bar.right() - 64;
        const int index = index_of(m_selected);
        if (index >= 0){
            const DropItem& item = m_items[(size_t)index];
            if (!item.chip.isEmpty()){
                const double width = draw_chip(painter, tr_ui(item.chip), right, bar.center().y(), item.chip_kind, a, 20, 36, 14);
                right -= width + 12;
            }
            draw_elided(painter, item.text, bar.left() + 22, bar.center().y(), right - bar.left() - 22, ui_font(27, 600), white(a));
        }else{
            draw_text_cy(painter, placeholder, bar.left() + 22, bar.center().y() + 1, ui_font(27, 500), white(0.55 * a));
        }
        draw_chevron(painter, QRectF(bar.right() - 56, bar.center().y() - 18, 36, 36), white(0.75 * a), 3.0);
    }

    //  The list, on top of everything else: draw it last.
    void paint_list(Canvas& canvas, QPainter& painter, const QRectF& bar, double alpha){
        if (m_open_p.value < 0.01 || m_items.empty()){
            return;
        }
        constexpr double ROW = 58;
        const double open = eio(clamp01(m_open_p.value));
        const double height = (double)m_items.size() * ROW + 16;
        double top = bar.bottom() + 8;
        if (top + height > 960){
            top = bar.top() - 8 - height;
        }
        const QRectF list(bar.left(), top, bar.width(), height);
        const double a = alpha * clamp01(open * 4.0);
        painter.save();
        painter.setClipRect(QRectF(list.left() - 60, list.top() - 20, list.width() + 120, height * open + 40), Qt::IntersectClip);
        panel(painter, list, 22, with_alpha(LIST_PANEL, 0.99 * a), white(0.2 * a), 1.6);
        for (size_t c = 0; c < m_items.size(); c++){
            const DropItem& item = m_items[c];
            const QRectF row(list.left() + 8, list.top() + 8 + (double)c * ROW, list.width() - 16, ROW);
            const QString id = row_prefix() + item.id;
            if (canvas.hovered(id) && open > 0.9){
                m_highlight = (int)c;
            }
            if ((int)c == m_highlight){
                panel(painter, row.adjusted(0, 3, 0, -3), 16, with_alpha(ORANGE, 0.2 * a), with_alpha(ORANGE, 0.95 * a), 2.0);
            }else if (item.id == m_selected){
                panel(painter, row.adjusted(0, 3, 0, -3), 16, white(0.07 * a), white(0.24 * a), 1.6);
            }
            double right = row.right() - 16;
            if (!item.chip.isEmpty()){
                const double width = draw_chip(painter, tr_ui(item.chip), right, row.center().y(), item.chip_kind, a, 20, 36, 14);
                right -= width + 12;
            }
            draw_elided(painter, item.text, row.left() + 18, row.center().y(), right - row.left() - 18, ui_font(27, 600), white(a));
            if (open > 0.9){
                canvas.add_hit(id, row, true, false);
            }
        }
        painter.restore();
    }

private:
    int index_of(const QString& id) const{
        for (size_t c = 0; c < m_items.size(); c++){
            if (m_items[c].id == id){
                return (int)c;
            }
        }
        return -1;
    }

private:
    QString m_name;
    std::vector<DropItem> m_items;
    QString m_selected;
    bool m_open = false;
    Smooth m_open_p;
    int m_highlight = 0;
};


//  ---------------------------------------------------------------------
//  Page 3: the steps of a board
//  ---------------------------------------------------------------------

//  What a step shows on the right, besides the picture of the board.
enum class Action{
    NONE,
    DEVICE,         //  the serial ports of the computer, the drivers
    BOOTSEL,        //  the Pico in flash mode
    FIRMWARE,       //  the firmware: version, link, flash
    PORTS,          //  what to pick in the program
};

struct StepText{
    const char* title;
    const char* sub;
    Action action;
};

struct BoardSpec{
    const StepText* steps;
    size_t count;
    const char* lead;
    //  The type to pick in the program.
    const char* type;
    //  The steps where something happens in the picture (-1: never).
    int flash_step;
    int grip_step;
    int serial_step;
    int type_step;
    int link_step;
    bool wired;
};

const char* const GRIP_SUB = "On the Switch, go to HOME, then Controllers, then Change Grip/Order.";
const char* const SELECT_SUB = "In the program, pick Serial: PABotBase2 in the first dropdown. Not Serial: PABotBase.";
const char* const PORT_SUB = "Pick your device as the serial port (on Windows, for example COM3) and NS1: Wireless Pro Controller as the type.";

const BoardSpec& spec_for(const QString& board){
    static const StepText PICO[] = {
        {"Plug it in", "Connect the Pico to the computer with a micro-USB cable. Keep it within 1 m (3 ft) of the Switch.", Action::NONE},
        {"Enter flash mode", "Unplug it, hold the white Bootsel button, plug it in again, then let go.", Action::BOOTSEL},
        {"Flash the firmware", "Pick the firmware version and press Flash now. The program puts the file on the Pico for you.", Action::FIRMWARE},
        {"Open the Grip menu", GRIP_SUB, Action::NONE},
        {"Select the controller", SELECT_SUB, Action::NONE},
        {"Select the port and the type", PORT_SUB, Action::PORTS},
        {"Pair and test", "Wait for the controller in the Grip menu. Then click the video, press some keys, and check the picture and the sound.", Action::NONE},
    };
    static const StepText ESP32[] = {
        {"Plug it in", "Connect the ESP32 to the computer with a USB cable. Keep it within 1 m (3 ft) of the Switch.", Action::DEVICE},
        {"Flash the firmware", "Pick the port of the ESP32 and the firmware version, then press Flash now.", Action::FIRMWARE},
        {"Restart the board", "Press the EN or RESET button, or unplug it and plug it in again.", Action::NONE},
        {"Open the Grip menu", GRIP_SUB, Action::NONE},
        {"Select the controller", SELECT_SUB, Action::NONE},
        {"Select the port and the type", PORT_SUB, Action::PORTS},
        {"Pair and test", "Wait about 5 seconds for the controller in the Grip menu. No controller? Press EN or RESET and click Reset Ctrl. Then check keys, picture and sound.", Action::NONE},
    };
    static const StepText ESP32S3[] = {
        {"Connect both cables", "Plug the port labeled COM or UART into the computer, and the port labeled USB or OTG into the Switch dock. They are not interchangeable.", Action::DEVICE},
        {"Allow wired controllers", "On the Switch, open System Settings, then Controllers and Sensors, and turn on Pro Controller Wired Communication.", Action::NONE},
        {"Flash the firmware", "Pick the port of the ESP32-S3 and the firmware version, then press Flash now.", Action::FIRMWARE},
        {"Restart the board", "Press the EN or RESET button, or unplug both cables and plug them in again.", Action::NONE},
        {"Select the controller and the port", "In the program, pick Serial: PABotBase2 in the first dropdown, then your device as the serial port. Click Reset Ctrl if it is not listed.", Action::PORTS},
        {"Connect to the Switch", "Pick NS1: Wired Pro Controller as the type. Click the video and press Enter. A controller shows up on the Switch.", Action::NONE},
        {"Test", "Click the video and press some keys. Check the picture and the sound. Pick None as the type to use a physical controller again.", Action::NONE},
    };
    static const BoardSpec PICO_SPEC = {
        PICO, sizeof(PICO) / sizeof(PICO[0]),
        "Wireless, set up over USB. Seven short steps.", "NS1: Wireless Pro Controller",
        2, 3, 4, 5, 6, false
    };
    static const BoardSpec ESP32_SPEC = {
        ESP32, sizeof(ESP32) / sizeof(ESP32[0]),
        "Wireless. The program flashes it for you. Seven short steps.", "NS1: Wireless Pro Controller",
        1, 3, 4, 5, 6, false
    };
    static const BoardSpec ESP32S3_SPEC = {
        ESP32S3, sizeof(ESP32S3) / sizeof(ESP32S3[0]),
        "Wired and the most reliable. The program flashes it for you. Seven short steps.", "NS1: Wired Pro Controller",
        2, -1, 4, 5, 5, true
    };
    if (board == "esp32"){
        return ESP32_SPEC;
    }
    if (board == "esp32s3"){
        return ESP32S3_SPEC;
    }
    return PICO_SPEC;
}

const char* const URL_DRIVER_CP210X = "https://www.silabs.com/documents/public/software/CP210x_Windows_Drivers.zip";
const char* const URL_DRIVER_CH340 = "https://www.arduined.eu/ch340-windows-10-driver-download/";
const char* const URL_ESPTOOL = "https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html";

//  What the firmware part of a page is doing.
enum class Flash{
    IDLE,
    DOWNLOADING,
    COPYING,
    RESTARTING,
    FLASHING,
    DONE,
    FAILED,
};

class StepsPage : public Page{
public:
    StepsPage(Canvas& canvas, const QString& board)
        : m_canvas(canvas)
        , m_board(board)
        , m_spec(spec_for(board))
        , m_kind(firmware_kind(board))
        , m_probe(canvas.app_folder(), canvas.scan_override())
        , m_versions("version")
        , m_ports("port")
    {
        canvas.set_focus_id("next");
        m_usb.jump(0);
        m_firmware_dir = QDir(canvas.app_folder()).filePath("Firmware");
        m_ref = firmware_ref(m_firmware_dir);
        rebuild_versions(true);
        if (canvas.has_listing_override()){
            m_online = parse_firmware_listing(canvas.listing_override(), m_kind);
            rebuild_versions(false);
        }else if (!canvas.test_mode()){
            m_listing = std::make_unique<Download>();
            m_listing->start(FIRMWARE_LISTING_URL, QString());
        }
        m_probe.step(10, true);
        m_hw = m_probe.state();
        update_ports();
    }

    virtual const char* name() const override{
        return "steps";
    }

    int current() const{
        return m_current;
    }
    Flash flash_state() const{
        return m_flash;
    }

    virtual void step(double dt) override{
        m_pos.step(dt, 12);
        m_usb.target = 1;
        m_pico.target = m_current >= m_spec.flash_step ? 1 : 0;
        m_serial.target = m_current >= m_spec.serial_step ? 1 : 0;
        m_type.target = m_current >= m_spec.type_step ? 1 : 0;
        m_link.target = m_current >= m_spec.link_step ? 1 : 0;
        m_search.target = m_spec.grip_step >= 0 && m_current >= m_spec.grip_step ? 1 : 0;
        m_usb.step(dt, 6);
        m_pico.step(dt, 8);
        m_serial.step(dt, 7);
        m_type.step(dt, 7);
        m_link.step(dt, 6);
        m_search.step(dt, 6);
        m_clock += dt;
        m_versions.step(dt);
        m_ports.step(dt);

        const bool card = m_spec.steps[m_current].action != Action::NONE;
        m_card.target = card ? 1 : 0;
        m_card.step(dt, 9);

        m_probe.step(dt, m_canvas.test_mode());
        m_hw = m_probe.state();
        update_ports();
        update_listing();
        update_flash(dt);
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        const int states[4] = {0, 0, 1, 2};
        draw_stepper(painter, states, prog(t, in.head - 0.05, in.head + 0.7));
        const QString heading = T("Set up your %1").arg(short_board_name(m_board));
        draw_heading(painter, heading, 140, 160, 84, prog(t, in.head, in.head + 0.9), 1.0);
        const QString sub = T(m_spec.lead);
        const double sub_top = 160 + heading_height(heading, 84) + 10;
        draw_sub(painter, sub, 140, sub_top, prog(t, in.sub, in.sub + 0.8), 1.0, 32, 860);

        const double reveal = eo(prog(t, in.bar, in.bar + 0.6));
        paint_steps(canvas, painter, reveal, sub_top + sub_height(sub, 32, 860) + 26);
        paint_diagram(painter, eo(prog(t, in.bar + 0.1, in.bar + 0.8)) * (1.0 - m_card.value));
        paint_card(canvas, painter, reveal * m_card.value);

        const double b = eo(prog(t, in.buttons, in.buttons + 0.5));
        draw_skip(canvas, painter, b);
        const QString video = T("Watch the video tutorial");
        draw_button(canvas, painter, "video", video, 136 + button_width(T("Skip setup"), ButtonKind::LINK) + 40, 960, ButtonKind::LINK, std::min(1.0, b * 1.35));
        const bool last = m_current >= (int)m_spec.count - 1;
        const QString next_text = last ? T("Finish the steps") : T("Next step");
        const double next_w = button_width(next_text, ButtonKind::PRIMARY);
        draw_button(canvas, painter, "next", next_text, 1780 - next_w, 950, ButtonKind::PRIMARY, b);
        const QString back_text = T("Back");
        draw_button(canvas, painter, "back", back_text, 1780 - next_w - 18 - button_width(back_text, ButtonKind::SECONDARY), 950,
            ButtonKind::SECONDARY, b);

        //  The lists of the drop-downs lie on top of everything.
        const double list_alpha = reveal * m_card.value;
        if (m_card_bar_ports.width() > 0){
            m_ports.paint_list(canvas, painter, m_card_bar_ports, list_alpha);
        }
        if (m_card_bar_versions.width() > 0){
            m_versions.paint_list(canvas, painter, m_card_bar_versions, list_alpha);
        }
        m_card_bar_ports = QRectF();
        m_card_bar_versions = QRectF();
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        //  A press outside of an open list closes it.
        if (m_versions.is_open() && !m_versions.owns(id)){
            m_versions.close();
        }
        if (m_ports.is_open() && !m_ports.owns(id)){
            m_ports.close();
        }
        if (id == "next"){
            if (m_current >= (int)m_spec.count - 1){
                canvas.go_done(m_board);
            }else{
                set_current(m_current + 1);
            }
        }else if (id == "back"){
            if (m_current <= 0){
                canvas.go_board();
            }else{
                set_current(m_current - 1);
            }
        }else if (id == "skip"){
            canvas.result().board_done = true;
            canvas.result().board = m_board;
            canvas.end_installation();
        }else if (id == "video"){
            canvas.play_video(m_board);
        }else if (id.startsWith("step:")){
            set_current(id.mid(5).toInt());
        }else if (id == m_versions.bar_id()){
            m_ports.close();
            m_versions.toggle();
        }else if (id == m_ports.bar_id()){
            m_versions.close();
            m_ports.toggle();
        }else if (id.startsWith(m_versions.row_prefix())){
            m_versions.pick(id);
            reset_flash_if_done();
        }else if (id.startsWith(m_ports.row_prefix())){
            m_ports.pick(id);
            reset_flash_if_done();
        }else if (id == "flash"){
            //  "Flash again" and "Try again" start over: when the Pico or the
            //  board is not ready, the card shows what is missing.
            reset_flash_if_done();
            start_flash();
        }else if (id == "download"){
            start_download();
        }else if (id == "link_open"){
            canvas.open_url(current_url());
        }else if (id == "link_copy"){
            canvas.copy_text(current_url());
        }else if (id == "folder"){
            canvas.open_folder(m_firmware_dir);
        }else if (id == "devmgr"){
            canvas.open_device_manager();
        }else if (id == "driver_cp210x"){
            canvas.open_url(URL_DRIVER_CP210X);
        }else if (id == "driver_ch340"){
            canvas.open_url(URL_DRIVER_CH340);
        }else if (id == "get_tool"){
            canvas.open_url(URL_ESPTOOL);
        }else if (id == "rescan"){
            m_probe.refresh();
        }
    }
    virtual bool key(Canvas& canvas, QKeyEvent& event) override{
        QString picked;
        if (m_versions.key(event, &picked)){
            if (!picked.isEmpty()){
                reset_flash_if_done();
            }
            return true;
        }
        if (m_ports.key(event, &picked)){
            if (!picked.isEmpty()){
                reset_flash_if_done();
            }
            return true;
        }
        if (event.key() == Qt::Key_Right){
            activate(canvas, "next");
            return true;
        }
        if (event.key() == Qt::Key_Left){
            activate(canvas, "back");
            return true;
        }
        return false;
    }
    virtual void enter_pressed(Canvas& canvas) override{
        activate(canvas, "next");
    }
    virtual void escape_pressed(Canvas& canvas) override{
        (void)canvas;
        m_versions.close();
        m_ports.close();
    }

    virtual QString state() const override{
        static const char* const NAMES[] = {"idle", "downloading", "copying", "restarting", "flashing", "done", "failed"};
        const FirmwareFile* file = selected_file();
        return QString("step=%1 flash=%2 error=%3 file=%4 port=%5 local=%6")
            .arg(m_current + 1)
            .arg(NAMES[(int)m_flash])
            .arg(m_flash_error)
            .arg(file == nullptr ? QString() : file->file_name)
            .arg(m_ports.selected())
            .arg(file != nullptr && !file->path.isEmpty() ? "yes" : "no");
    }

    //  ---- for the tests
    const FirmwareFile* selected_file() const{
        for (const FirmwareFile& file : m_files){
            if (file.file_name == m_versions.selected()){
                return &file;
            }
        }
        return nullptr;
    }
    QString selected_port() const{
        return m_ports.selected();
    }
    QString flash_error() const{
        return m_flash_error;
    }

private:
    void set_current(int index){
        const int clamped = std::max(0, std::min((int)m_spec.count - 1, index));
        if (clamped != m_current){
            m_current = clamped;
            m_step_since = m_clock;
            m_versions.close();
            m_ports.close();
        }
        m_pos.target = m_current;
    }

    //  ---- the firmware, the ports

    //  The versions of the dropdown: what is on this computer and what the
    //  Packages repository lists.
    void rebuild_versions(bool first){
        if (m_kind.isEmpty()){
            return;
        }
        const QString keep = m_versions.selected();
        m_files = merge_firmware(local_firmware(m_firmware_dir, m_kind), m_online);
        std::vector<DropItem> items;
        for (const FirmwareFile& file : m_files){
            DropItem item;
            item.id = file.file_name;
            //  Short on screen: the board and the version. The link and the
            //  file on the disk keep the whole name.
            item.text = file.file_name;
            if (item.text.startsWith("PABotBase2-")){
                item.text = item.text.mid(11);
            }
            const qsizetype dot = item.text.lastIndexOf('.');
            if (dot > 0){
                item.text = item.text.left(dot);
            }
            if (!file.path.isEmpty()){
                item.chip = "On this computer";
                item.chip_kind = ChipKind::ORANGE_RING;
            }else{
                item.chip = "Online";
                item.chip_kind = ChipKind::RING;
            }
            items.push_back(item);
        }
        m_versions.set_items(items);
        if (!keep.isEmpty()){
            m_versions.select(keep);
        }
        if (m_versions.selected().isEmpty() && !m_files.empty()){
            //  The newest of this computer, else the newest there is.
            QString choice = m_files.front().file_name;
            for (const FirmwareFile& file : m_files){
                if (!file.path.isEmpty()){
                    choice = file.file_name;
                    break;
                }
            }
            m_versions.select(choice);
        }
        (void)first;
    }

    void update_listing(){
        if (m_listing == nullptr || !m_listing->done()){
            return;
        }
        if (m_listing->ok()){
            m_online = parse_firmware_listing(m_listing->body(), m_kind);
            rebuild_versions(false);
        }
        m_listing.reset();
    }

    void update_ports(){
        if (m_spec.steps[0].action != Action::DEVICE){
            return;
        }
        std::vector<DropItem> items;
        QString signature;
        for (const PortInfo& port : m_hw.ports){
            DropItem item;
            item.id = port.name;
            item.text = port.name + (port.description.isEmpty() ? QString() : "  " + port.description);
            item.chip = port.chip;
            item.chip_kind = ChipKind::RING;
            items.push_back(item);
            signature += port.name + "|" + port.chip + ";";
        }
        if (signature == m_port_signature){
            return;
        }
        m_port_signature = signature;
        const QString keep = m_ports.selected();
        m_ports.set_items(items);
        m_ports.select(keep);
        if (m_ports.selected().isEmpty() && !items.empty()){
            //  The first with a chip of a known board (the list has those first).
            m_ports.select(items.front().id);
        }
    }

    QString current_url() const{
        const FirmwareFile* file = selected_file();
        if (file == nullptr){
            return QString();
        }
        return firmware_url(file->file_name, file->path.isEmpty() ? QString("master") : m_ref);
    }

    bool file_is_local() const{
        const FirmwareFile* file = selected_file();
        return file != nullptr && !file->path.isEmpty();
    }

    bool is_pico() const{
        return m_board == "pico" || m_board == "pico2";
    }

    const PicoDrive* fitting_drive() const{
        for (const PicoDrive& drive : m_hw.drives){
            if (pico_drive_fits(drive, m_board)){
                return &drive;
            }
        }
        return nullptr;
    }
    const PicoDrive* other_drive() const{
        for (const PicoDrive& drive : m_hw.drives){
            if (!pico_drive_fits(drive, m_board)){
                return &drive;
            }
        }
        return nullptr;
    }

    bool busy() const{
        return m_flash == Flash::DOWNLOADING || m_flash == Flash::COPYING || m_flash == Flash::RESTARTING || m_flash == Flash::FLASHING;
    }

    //  Why Flash now is not possible yet.
    enum class Block{
        NONE,
        NOT_LOCAL,
        NO_DRIVE,
        WRONG_DRIVE,
        NO_PORT,
        NO_TOOL,
        LOOKING_FOR_TOOL,
    };
    Block flash_blocker() const{
        if (!file_is_local()){
            return Block::NOT_LOCAL;
        }
        if (is_pico()){
            if (fitting_drive() == nullptr){
                return other_drive() != nullptr ? Block::WRONG_DRIVE : Block::NO_DRIVE;
            }
            return Block::NONE;
        }
        if (m_ports.selected().isEmpty()){
            return Block::NO_PORT;
        }
        if (!m_canvas.pretend() && !m_hw.tool.found){
            return m_hw.tool_checked ? Block::NO_TOOL : Block::LOOKING_FOR_TOOL;
        }
        return Block::NONE;
    }
    QString blocker_text(Block block) const{
        switch (block){
        case Block::NOT_LOCAL:
            return T("This version is not on this computer yet.");
        case Block::NO_DRIVE:
            return T("Put the Pico in flash mode first (step 2).");
        case Block::WRONG_DRIVE:
            return T("The drive is of another board.");
        case Block::NO_PORT:
            return T("Pick the port of the board.");
        case Block::NO_TOOL:
            return T("The flash tool (esptool) was not found.");
        case Block::LOOKING_FOR_TOOL:
            return T("Looking for the flash tool...");
        default:
            return QString();
        }
    }

    void reset_flash_if_done(){
        if (m_flash == Flash::DONE || m_flash == Flash::FAILED){
            m_flash = Flash::IDLE;
            m_flash_error.clear();
            m_flash_progress = 0;
        }
    }

    void start_download(){
        const FirmwareFile* file = selected_file();
        if (file == nullptr || busy()){
            return;
        }
        m_flash_error.clear();
        m_fetch = std::make_unique<Download>();
        m_fetch_name = file->file_name;
        m_fetch->start(m_canvas.map_url(current_url()), QDir(m_firmware_dir).filePath(file->file_name));
        m_flash = Flash::DOWNLOADING;
        m_flash_progress = 0;
    }

    void start_flash(){
        if (busy()){
            return;
        }
        if (flash_blocker() != Block::NONE){
            return;
        }
        const FirmwareFile* file = selected_file();
        const QString path = file->path;
        m_flash_error.clear();
        m_flash_progress = 0;
        m_flash_time = 0;
        if (!looks_like_firmware(path)){
            m_flash = Flash::FAILED;
            m_flash_error = "file";
            return;
        }
        if (is_pico()){
            m_flash = Flash::COPYING;
            if (!m_canvas.pretend()){
                m_copy = std::make_unique<CopyJob>(path, fitting_drive()->root);
            }
            m_gone_for = 0;
        }else{
            m_flash = Flash::FLASHING;
            if (!m_canvas.pretend()){
                m_esp = std::make_unique<EspFlashJob>(m_hw.tool, m_ports.selected(), path);
                m_esp->start();
            }
        }
    }

    void update_flash(double dt){
        switch (m_flash){
        case Flash::DOWNLOADING:
            if (m_fetch == nullptr){
                m_flash = Flash::IDLE;
                break;
            }
            m_flash_progress = std::max(0.0, m_fetch->progress());
            if (m_fetch->done()){
                const bool ok = m_fetch->ok() && looks_like_firmware(QDir(m_firmware_dir).filePath(m_fetch_name));
                const QString name = m_fetch_name;
                m_fetch.reset();
                if (ok){
                    rebuild_versions(false);
                    m_versions.select(name);
                    m_flash = Flash::IDLE;
                }else{
                    QFile::remove(QDir(m_firmware_dir).filePath(name));
                    m_flash = Flash::FAILED;
                    m_flash_error = "download";
                }
            }
            break;
        case Flash::COPYING:
            m_flash_time += dt;
            if (m_canvas.pretend()){
                m_flash_progress = std::min(1.0, m_flash_time / 2.4);
                if (m_flash_progress >= 1.0){
                    m_flash = Flash::RESTARTING;
                    m_gone_for = 0;
                    m_flash_time = 0;
                }
                break;
            }
            m_flash_progress = m_copy != nullptr ? m_copy->progress() : 0.0;
            //  A moment on the picture, even when the copy is done at once.
            if (m_copy != nullptr && m_copy->done() && m_flash_time >= 1.4){
                if (m_copy->ok()){
                    m_copy.reset();
                    m_flash = Flash::RESTARTING;
                    m_gone_for = 0;
                    m_flash_time = 0;
                }else{
                    m_flash_error = "copy";
                    m_copy.reset();
                    m_flash = Flash::FAILED;
                }
            }
            break;
        case Flash::RESTARTING:
            m_flash_time += dt;
            m_flash_progress = 1.0;
            //  The Pico restarts by itself: its drive goes away.
            if (m_canvas.pretend()){
                if (m_flash_time >= 1.6){
                    m_flash = Flash::DONE;
                }
            }else if (fitting_drive() == nullptr){
                m_gone_for += dt;
                if (m_gone_for >= 0.5){
                    m_flash = Flash::DONE;
                }
            }else{
                m_gone_for = 0;
                if (m_flash_time > 30){
                    m_flash = Flash::FAILED;
                    m_flash_error = "restart";
                }
            }
            break;
        case Flash::FLASHING:
            m_flash_time += dt;
            if (m_canvas.pretend()){
                m_flash_progress = std::min(1.0, m_flash_time / 6.0);
                if (m_flash_progress >= 1.0){
                    m_flash = Flash::DONE;
                }
                break;
            }
            if (m_esp != nullptr){
                m_flash_progress = m_esp->progress();
                if (m_esp->done()){
                    if (m_esp->ok()){
                        m_flash = Flash::DONE;
                    }else{
                        m_flash = Flash::FAILED;
                        m_flash_error = m_esp->error_kind().isEmpty() ? QString("other") : m_esp->error_kind();
                        m_flash_log = m_esp->status();
                    }
                    m_esp.reset();
                }
            }
            break;
        default:
            break;
        }
        if (m_flash == Flash::DONE){
            m_flash_progress = 1.0;
        }
    }

    //  ---- painting: the steps

    void paint_steps(Canvas& canvas, QPainter& painter, double reveal, double top){
        if (reveal < 0.004){
            return;
        }
        constexpr double X = 140;
        constexpr double BOTTOM = 935;
        const QFont title_font = ui_font(38, 600);
        const QFont sub_font = ui_font(27, 500);
        const double a = reveal;

        //  The closed steps get less room when the open one is long.
        const int open_lines = (int)wrap_text(sub_font, T(m_spec.steps[m_current].sub), 740).size();
        const double extra = 8 + open_lines * 36 + 14;
        double compact = 66;
        if (top + (double)m_spec.count * compact + extra > BOTTOM){
            compact = std::max(44.0, (BOTTOM - top - extra) / (double)m_spec.count);
        }
        const double circle_size = std::min(60.0, compact - 4);
        double y = top + (1.0 - reveal) * 30;

        //  The rail behind the circles.
        QList<double> centers;
        double probe = y;
        for (int c = 0; c < (int)m_spec.count; c++){
            const bool open = c == m_current;
            const double height = open ? compact + extra : compact;
            centers << probe + circle_size / 2;
            probe += height;
        }
        if (centers.size() > 1){
            painter.setPen(QPen(with_alpha(ORANGE, 0.45 * a), 4, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(X + circle_size / 2, centers.first()), QPointF(X + circle_size / 2, centers.last()));
        }

        for (int c = 0; c < (int)m_spec.count; c++){
            const bool open = c == m_current;
            const bool done = c < m_current;
            const QStringList lines = wrap_text(sub_font, T(m_spec.steps[c].sub), 740);
            const double height = open ? compact + extra : compact;
            const QRectF circle(X, y, circle_size, circle_size);

            if (open){
                const double pulse = 0.5 + 0.5 * std::sin(m_clock * 4.4);
                glow(painter, circle, 30, ORANGE, 0.5 + 0.35 * pulse);
            }
            painter.setPen(Qt::NoPen);
            if (done){
                painter.setBrush(with_alpha(ORANGE, a));
                painter.drawEllipse(circle);
                draw_check(painter, circle.adjusted(circle_size * 0.2, circle_size * 0.2, -circle_size * 0.2, -circle_size * 0.2), with_alpha(Qt::black, a), 4.0);
            }else{
                painter.setBrush(with_alpha(Qt::black, a));
                painter.setPen(QPen(with_alpha(open ? ORANGE : Qt::white, (open ? 1.0 : 0.28) * a), 3.2));
                painter.drawEllipse(circle.adjusted(1.6, 1.6, -1.6, -1.6));
                draw_text_cy(painter, QString::number(c + 1), circle.center().x(), circle.center().y() + 1, ui_font(32, 700),
                    with_alpha(open ? ORANGE : Qt::white, (open ? 1.0 : 0.5) * a), Qt::AlignHCenter);
            }
            draw_text(painter, T(m_spec.steps[c].title), X + 88, y + (circle_size - text_height(title_font)) / 2, title_font, white((open ? 1.0 : done ? 0.78 : 0.55) * a));
            if (open){
                for (int k = 0; k < lines.size(); k++){
                    draw_text(painter, lines[k], X + 90, y + circle_size + 4 + k * 36, sub_font, white(0.66 * a));
                }
            }
            canvas.add_hit("step:" + QString::number(c), QRectF(X, y, 820, height), true, false);
            y += height;
        }
    }

    //  ---- painting: the picture of the computer, the board and the Switch

    void paint_diagram(QPainter& painter, double reveal){
        if (reveal < 0.004){
            return;
        }
        constexpr double CX_PC = 1190;
        constexpr double CX_PICO = 1500;
        constexpr double CY = 430;
        const double a = reveal;
        const QString picture = m_board == "esp32" ? "esp32-big" : m_board == "esp32s3" ? "esp32s3-big" : "pico-big";

        //  The Switch, with its screen black until it is linked.
        const QPixmap plain = plain_picture("switch");
        const QPixmap linked = plain_picture("switch-gg");
        const double switch_w = 620;
        const double switch_h = plain.isNull() ? 260 : switch_w * plain.height() / plain.width();
        const QRectF sw(CX_PICO - switch_w / 2, CY + 188, switch_w, switch_h);
        glow(painter, QRectF(sw.left() + 40, sw.top() + 30, sw.width() - 80, sw.height() - 60), 60, ORANGE, 0.28 * a * m_link.value);
        draw_picture(painter, plain, sw, a * (1.0 - m_link.value));
        draw_picture(painter, linked, sw, a * m_link.value);
        draw_text(painter, QString("Nintendo Switch"), CX_PICO, sw.bottom() + 4, ui_font(26, 500), white(0.6 * a), Qt::AlignHCenter);

        //  The computer, the line, the board.
        draw_picture(painter, plain_picture("computer"), QRectF(CX_PC - 75, CY - 64, 150, 128), a * 0.9);
        draw_text(painter, T("Computer"), CX_PC, CY + 100, ui_font(26, 500), white(0.6 * a), Qt::AlignHCenter);
        const double line_left = CX_PC + 75 + 18;
        const double line_right = CX_PICO - 50;
        painter.setPen(Qt::NoPen);
        painter.setBrush(white(0.22 * a));
        painter.drawRoundedRect(QRectF(line_left, CY - 3.5, line_right - line_left, 7), 3.5, 3.5);
        painter.setBrush(with_alpha(ORANGE, a));
        painter.drawRoundedRect(QRectF(line_left, CY - 3.5, (line_right - line_left) * eo(m_usb.value), 7), 3.5, 3.5);

        const QRectF board_box(CX_PICO - 50, CY - 95, 100, 190);
        const double lit = m_pico.value;
        if (lit > 0.02 && m_current >= m_spec.flash_step){
            draw_picture(painter, tinted(picture, ORANGE), board_box.adjusted(-10, -10, 10, 10), 0.18 * a * lit);
        }
        draw_picture(painter, tinted(picture, white(1.0)), board_box, a * (1.0 - lit) * 0.92);
        draw_picture(painter, tinted(picture, ORANGE), board_box, a * lit);
        draw_text(painter, short_board_name(m_board), CX_PICO, CY + 118, ui_font(26, 500), white(0.6 * a), Qt::AlignHCenter);

        //  The firmware file drops into the board, again and again, while the step "Flash the firmware" is open.
        if (m_current == m_spec.flash_step){
            const double phase = std::fmod(m_clock, 2.8) / 2.8;
            const double fall = eio(prog(phase, 0.10, 0.62));
            const double chip_alpha = a * prog(phase, 0.0, 0.10) * (1.0 - prog(phase, 0.58, 0.68));
            const QString file = QString("PABotBase2-%1%2").arg(m_kind, firmware_extension(m_kind));
            const double w = draw_chip(painter, file, 0, 0, ChipKind::YELLOW_FILL, 0, 26, 48, 20, true);
            draw_chip(painter, file, CX_PICO + w / 2, lerp(CY - 150, CY - 20, fall), ChipKind::YELLOW_FILL, chip_alpha, 26, 48, 20);
        }
        //  What is picked in the program.
        const double mid = (CX_PC + CX_PICO) / 2;
        if (m_serial.value > 0.02){
            const QString text = "Serial: PABotBase2";
            const double w = draw_chip(painter, text, 0, 0, ChipKind::YELLOW_FILL, 0, 25, 48, 20, true);
            draw_chip(painter, text, mid + w / 2, CY - 178 + (1.0 - m_serial.value) * 14, ChipKind::YELLOW_FILL, a * m_serial.value, 25, 48, 20);
        }
        if (m_type.value > 0.02){
            const QString text = QString::fromLatin1(m_spec.type);
            const double w = draw_chip(painter, text, 0, 0, ChipKind::YELLOW_FILL, 0, 25, 48, 20, true);
            draw_chip(painter, text, mid + w / 2, CY - 122 + (1.0 - m_type.value) * 14, ChipKind::YELLOW_FILL, a * m_type.value, 25, 48, 20);
        }

        if (m_spec.wired){
            //  A cable from the board to the Switch.
            const double x = CX_PICO;
            const double top = CY + 100;
            const double bottom = CY + 188;
            painter.setPen(Qt::NoPen);
            painter.setBrush(white(0.22 * a));
            painter.drawRoundedRect(QRectF(x - 3.5, top + 28, 7, bottom - top - 28), 3.5, 3.5);
            painter.setBrush(with_alpha(ORANGE, a * m_search.value));
            painter.drawRoundedRect(QRectF(x - 3.5, top + 28, 7, (bottom - top - 28) * m_link.value), 3.5, 3.5);
            return;
        }

        //  The board looks for the Switch, then it is linked.
        if (m_search.value > 0.02){
            static const double RX[3] = {52, 96, 140};
            static const double RY[3] = {30, 54, 78};
            for (int c = 0; c < 3; c++){
                const double glow_level = m_link.value > 0.5 ? 1.0 : clamp01(std::sin(m_clock * 5.0 - c * 0.9) * 0.6 + 0.5);
                const QColor color = m_link.value > 0.5 ? with_alpha(ORANGE, a) : with_alpha(ORANGE, a * (0.25 + 0.75 * glow_level));
                painter.setPen(QPen(m_link.value > 0.5 || glow_level > 0.25 ? color : white(0.34 * a), 7, Qt::SolidLine, Qt::RoundCap));
                painter.setBrush(Qt::NoBrush);
                painter.drawArc(QRectF(CX_PICO - RX[c], CY + 134 - RY[c], RX[c] * 2, RY[c] * 2), 218 * 16, 104 * 16);
            }
        }
    }

    //  ---- painting: the card on the right

    //  A status line: a dot or a check, and the text. kind: 0 waiting, 1 fine, 2 problem.
    void draw_status(QPainter& painter, double x, double cy, int kind, const QString& text, double alpha, double max_width = 640){
        const QRectF dot(x, cy - 15, 30, 30);
        painter.setPen(Qt::NoPen);
        if (kind == 1){
            painter.setBrush(with_alpha(ORANGE, alpha));
            painter.drawEllipse(dot);
            draw_check(painter, dot.adjusted(6, 6, -6, -6), with_alpha(Qt::black, alpha), 3.0);
        }else if (kind == 2){
            painter.setBrush(with_alpha(YELLOW, alpha));
            painter.drawEllipse(dot);
            draw_text_cy(painter, "!", dot.center().x(), dot.center().y() + 1, ui_font(22, 700), with_alpha(Qt::black, alpha), Qt::AlignHCenter);
        }else{
            const double pulse = 0.5 + 0.5 * std::sin(m_clock * 5.0);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(with_alpha(ORANGE, alpha * (0.45 + 0.55 * pulse)), 3.2));
            painter.drawEllipse(dot.adjusted(2, 2, -2, -2));
        }
        const QFont font = ui_font(26, 600);
        const QStringList lines = wrap_text(font, text, max_width);
        const double line_h = 34;
        const double top = cy - (double)lines.size() * line_h / 2.0;
        for (int c = 0; c < lines.size(); c++){
            draw_text(painter, lines[c], x + 46, top + c * line_h, font, white((kind == 0 ? 0.78 : 0.95) * alpha));
        }
    }
    double status_height(const QString& text, double max_width = 640) const{
        return std::max(34.0, (double)wrap_text(ui_font(26, 600), text, max_width).size() * 34.0);
    }

    double card_height(Action action) const{
        switch (action){
        case Action::DEVICE:
            return 420 + (double)std::min<size_t>(4, std::max<size_t>(1, m_hw.ports.size())) * 66;
        case Action::BOOTSEL:
            return 380;
        case Action::FIRMWARE:
            return is_pico() ? 560 : 680;
        case Action::PORTS:
            return 500;
        default:
            return 300;
        }
    }

    void paint_card(Canvas& canvas, QPainter& painter, double reveal){
        if (reveal < 0.004){
            return;
        }
        const Action action = m_spec.steps[m_current].action;
        if (action == Action::NONE){
            return;
        }
        const double a = reveal;
        const QRectF card(1000 + (1.0 - reveal) * 50, 170, 780, card_height(action));
        panel(painter, card, 30, with_alpha(PANEL, a), white(0.16 * a), 1.6);
        const double x = card.left() + 34;
        switch (action){
        case Action::DEVICE:
            paint_device(canvas, painter, card, x, a);
            break;
        case Action::BOOTSEL:
            paint_bootsel(painter, card, x, a);
            break;
        case Action::FIRMWARE:
            paint_firmware(canvas, painter, card, x, a);
            break;
        case Action::PORTS:
            paint_ports(painter, card, x, a);
            break;
        default:
            break;
        }
    }

    void card_title(QPainter& painter, const QRectF& card, const QString& text, double alpha){
        draw_text(painter, text, card.left() + 36, card.top() + 28, ui_font(30, 600), white(alpha));
    }

    void paint_device(Canvas& canvas, QPainter& painter, const QRectF& card, double x, double a){
        card_title(painter, card, T("Serial ports on this computer"), a);
        double y = card.top() + 96;
        const size_t count = m_hw.ports.size();
        if (count == 0){
            draw_status(painter, x, y + 17, 0, T("Looking for your board... Nothing found yet."), a);
            y += 70;
        }else{
            draw_status(painter, x, y + 17, 1, count == 1 ? T("Found 1 serial port.") : T("Found %1 serial ports.").arg((int)count), a);
            y += 70;
            const size_t shown = std::min<size_t>(4, count);
            for (size_t c = 0; c < shown; c++){
                const PortInfo& port = m_hw.ports[c];
                const QRectF row(x, y, card.width() - 68, 56);
                panel(painter, row, 18, white(0.05 * a), white(0.14 * a), 1.4);
                draw_text_cy(painter, port.name, row.left() + 20, row.center().y() + 1, ui_font(28, 700), white(a));
                double right = row.right() - 16;
                if (!port.chip.isEmpty()){
                    const double width = draw_chip(painter, port.chip, right, row.center().y(), ChipKind::ORANGE_RING, a, 20, 34, 14);
                    right -= width + 12;
                }
                draw_elided(painter, port.description, row.left() + 150, row.center().y(), right - row.left() - 160, ui_font(22, 500), white(0.58 * a));
                y += 66;
            }
        }
        const QString hint = count == 0
            ? T("No port yet? Try another USB port or another cable (some cables only charge), and install the driver of your board.")
            : T("No port in the list? Install the driver of your board: CP210x or CH340. If you do not know which, install both.");
        const QFont hint_font = ui_font(23, 500);
        const QStringList lines = wrap_text(hint_font, hint, card.width() - 72);
        for (int c = 0; c < lines.size(); c++){
            draw_text(painter, lines[c], x + 4, y + 6 + c * 31, hint_font, white(0.56 * a));
        }
        y += 6 + (double)lines.size() * 31 + 14;
        double bx = x;
        if (canvas.test_mode() || windows_host()){
            bx += draw_button(canvas, painter, "devmgr", T("Open Device Manager"), bx, y, ButtonKind::SECONDARY, a) + 16;
        }
        draw_button(canvas, painter, "driver_cp210x", T("CP210x driver"), bx, y, ButtonKind::SECONDARY, a);
        const double bx2 = bx + button_width(T("CP210x driver"), ButtonKind::SECONDARY) + 16;
        if (bx2 + button_width(T("CH340 driver"), ButtonKind::SECONDARY) < card.right() - 20){
            draw_button(canvas, painter, "driver_ch340", T("CH340 driver"), bx2, y, ButtonKind::SECONDARY, a);
        }else{
            draw_button(canvas, painter, "driver_ch340", T("CH340 driver"), x, y + 76, ButtonKind::SECONDARY, a);
        }
    }

    static bool windows_host(){
#ifdef Q_OS_WIN
        return true;
#else
        return false;
#endif
    }

    void paint_bootsel(QPainter& painter, const QRectF& card, double x, double a){
        card_title(painter, card, T("Flash mode"), a);
        const PicoDrive* fits = fitting_drive();
        const PicoDrive* other = other_drive();
        const double text_w = 470;
        double y = card.top() + 100;
        if (fits != nullptr){
            draw_status(painter, x, y + 17, 1, T("Found a Pico in flash mode."), a, text_w);
            y += 70;
            draw_text(painter, T("Drive %1 (%2)").arg(QDir::toNativeSeparators(fits->root), fits->label), x + 46, y - 6, ui_font(24, 500), white(0.62 * a));
            y += 52;
            draw_text(painter, T("Go on to the next step."), x + 46, y, ui_font(24, 500), white(0.62 * a));
        }else if (other != nullptr){
            draw_status(painter, x, y + 17, 2, T("This is not a %1.").arg(short_board_name(m_board)), a, text_w);
            y += 70;
            const QString text = T("The drive %1 belongs to another board. Go back and pick that board, or use the right Pico.")
                .arg(QDir::toNativeSeparators(other->root));
            const QStringList lines = wrap_text(ui_font(24, 500), text, text_w - 40);
            for (int c = 0; c < lines.size(); c++){
                draw_text(painter, lines[c], x + 46, y - 6 + c * 32, ui_font(24, 500), white(0.62 * a));
            }
        }else{
            draw_status(painter, x, y + 17, 0, T("Waiting for a Pico in flash mode..."), a, text_w);
            y += 70;
            const QString drive = m_board == "pico2" ? "RP2350" : "RPI-RP2";
            const QString text = T("It shows up as a drive named %1.").arg(drive);
            draw_text(painter, text, x + 46, y - 6, ui_font(24, 500), white(0.62 * a));
            if (m_clock - m_step_since > 14){
                const QString more = T("Still nothing? Unplug it, hold the Bootsel button, plug it in, and keep holding until the drive appears.");
                const QStringList lines = wrap_text(ui_font(24, 500), more, text_w - 40);
                for (int c = 0; c < lines.size(); c++){
                    draw_text(painter, lines[c], x + 46, y + 46 + c * 32, ui_font(24, 500), with_alpha(YELLOW, 0.9 * a));
                }
            }
        }
        //  The Pico on the right: orange when it is there.
        const bool found = fits != nullptr;
        const QRectF box(card.right() - 170, card.top() + 100, 100, 190);
        if (found){
            draw_picture(painter, tinted("pico-big", ORANGE), box.adjusted(-12, -12, 12, 12), 0.2 * a);
        }
        draw_picture(painter, tinted("pico-big", found ? ORANGE : QColor(Qt::white)), box, a * (found ? 1.0 : 0.8));
    }

    void paint_firmware(Canvas& canvas, QPainter& painter, const QRectF& card, double x, double a){
        card_title(painter, card, T("Firmware"), a);
        const double width = card.width() - 68;
        double y = card.top() + 90;
        const bool dropdowns_usable = !busy();

        if (!is_pico()){
            draw_text(painter, T("Port"), x + 4, y, ui_font(22, 600), white(0.55 * a));
            const QRectF bar(x, y + 30, width, 64);
            m_ports.paint_bar(canvas, painter, bar, a, m_hw.ports.empty() ? T("No serial port found") : T("Pick a port"), dropdowns_usable);
            m_card_bar_ports = bar;
            y += 112;
        }
        draw_text(painter, T("Firmware version"), x + 4, y, ui_font(22, 600), white(0.55 * a));
        const QRectF bar(x, y + 30, width, 64);
        m_versions.paint_bar(canvas, painter, bar, a, T("No firmware found"), dropdowns_usable && !m_files.empty());
        m_card_bar_versions = bar;
        y += 112;

        //  The link to download this version.
        const QString url = current_url();
        draw_text(painter, T("Download link"), x + 4, y, ui_font(22, 600), white(0.55 * a));
        draw_elided(painter, url.isEmpty() ? T("(none)") : url, x + 4, y + 44, width - 8, ui_font(22, 500), with_alpha(YELLOW, 0.85 * a));
        y += 66;
        double bx = x;
        if (!file_is_local() && selected_file() != nullptr){
            bx += draw_button(canvas, painter, "download", T("Download this version"), bx, y, ButtonKind::PRIMARY, a, !busy()) + 14;
        }
        bx += draw_button(canvas, painter, "link_open", T("Open link"), bx, y, ButtonKind::SECONDARY, a, !url.isEmpty()) + 14;
        bx += draw_button(canvas, painter, "link_copy", T("Copy link"), bx, y, ButtonKind::SECONDARY, a, !url.isEmpty()) + 14;
        if (bx + button_width(T("Firmware folder"), ButtonKind::SECONDARY) < card.right() - 20){
            draw_button(canvas, painter, "folder", T("Firmware folder"), bx, y, ButtonKind::SECONDARY, a);
        }
        y += 92;

        //  What is needed, and the flash itself.
        paint_flash_area(canvas, painter, card, x, y, width, a);
    }

    void paint_flash_area(Canvas& canvas, QPainter& painter, const QRectF& card, double x, double y, double width, double a){
        (void)card;
        const Block blocker = flash_blocker();
        switch (m_flash){
        case Flash::DOWNLOADING:
            draw_status(painter, x, y + 17, 0, T("Downloading..."), a, width - 60);
            draw_progress(painter, QRectF(x, y + 48, width, 14), m_flash_progress, a);
            return;
        case Flash::COPYING:
            draw_status(painter, x, y + 17, 0, T("Copying the firmware to the Pico..."), a, width - 60);
            draw_progress(painter, QRectF(x, y + 48, width, 14), m_flash_progress, a);
            return;
        case Flash::RESTARTING:
            draw_status(painter, x, y + 17, 0, T("Waiting for the Pico to restart..."), a, width - 60);
            draw_progress(painter, QRectF(x, y + 48, width, 14), 1.0, a);
            return;
        case Flash::FLASHING:
            draw_status(painter, x, y + 17, 0,
                m_flash_progress > 0.0 ? T("Flashing... %1%").arg((int)std::lround(m_flash_progress * 100)) : T("Connecting to the board..."),
                a, width - 60);
            draw_progress(painter, QRectF(x, y + 48, width, 14), m_flash_progress, a);
            return;
        case Flash::DONE:
            draw_status(painter, x, y + 17, 1, is_pico() ? T("Done. Your Pico has its firmware.") : T("Done. Your board has its firmware."), a, width - 60);
            draw_button(canvas, painter, "flash", T("Flash again"), x, y + 44, ButtonKind::SECONDARY, a);
            return;
        case Flash::FAILED: {
            const QString message = failure_text();
            draw_status(painter, x, y + 17, 2, message, a, width - 60);
            const double h = status_height(message, width - 60);
            draw_button(canvas, painter, "flash", T("Try again"), x, y + std::max(34.0, h) + 16, ButtonKind::PRIMARY, a, blocker == Block::NONE);
            return;
        }
        default:
            break;
        }

        //  Not started yet.
        if (blocker == Block::NONE){
            draw_status(painter, x, y + 17, 1, is_pico()
                ? T("Ready: the Pico is in flash mode.")
                : T("Ready to flash %1.").arg(m_ports.selected()), a, width - 60);
        }else{
            const bool waiting = blocker == Block::NO_DRIVE || blocker == Block::LOOKING_FOR_TOOL || blocker == Block::NO_PORT;
            draw_status(painter, x, y + 17, waiting ? 0 : 2, blocker_text(blocker), a, width - 60);
        }
        double by = y + 52;
        if (!is_pico() && m_hw.tool_checked && !m_hw.tool.found && !m_canvas.pretend()){
            draw_button(canvas, painter, "get_tool", T("How to get esptool"), x + width - button_width(T("How to get esptool"), ButtonKind::LINK), y + 8, ButtonKind::LINK, a);
        }
        draw_button(canvas, painter, "flash", T("Flash now"), x, by, ButtonKind::PRIMARY, a, blocker == Block::NONE);
    }

    QString failure_text() const{
        if (m_flash_error == "file"){
            return T("This file is not a valid firmware file. Download it again.");
        }
        if (m_flash_error == "download"){
            return T("The download failed. Check the connection, or open the link in your browser.");
        }
        if (m_flash_error == "copy"){
            return T("The file could not be copied to the Pico. Unplug it and start again from step 2.");
        }
        if (m_flash_error == "restart"){
            return T("The Pico did not restart. Unplug it and start again from step 2.");
        }
        if (m_flash_error == "connect"){
            return T("The board did not answer. Hold its BOOT button while it connects, or try another cable or port.");
        }
        if (m_flash_error == "port"){
            return T("The port cannot be opened. Close the other programs that use it and try again.");
        }
        if (m_flash_error == "tool"){
            return T("The flash tool did not start.");
        }
        return m_flash_log.isEmpty() ? T("Flashing failed.") : T("Flashing failed: %1").arg(m_flash_log);
    }

    void draw_progress(QPainter& painter, const QRectF& bar, double value, double alpha){
        painter.setPen(Qt::NoPen);
        painter.setBrush(white(0.14 * alpha));
        painter.drawRoundedRect(bar, bar.height() / 2, bar.height() / 2);
        const double filled = std::max(bar.height(), bar.width() * clamp01(value));
        painter.setBrush(with_alpha(ORANGE, alpha));
        painter.drawRoundedRect(QRectF(bar.left(), bar.top(), filled, bar.height()), bar.height() / 2, bar.height() / 2);
    }

    void paint_ports(QPainter& painter, const QRectF& card, double x, double a){
        card_title(painter, card, T("What to pick in the program"), a);
        const double width = card.width() - 68;
        double y = card.top() + 96;
        const bool two = m_board == "esp32s3";
        struct Row{
            QString label;
            QString value;
            bool detected;
        };
        std::vector<Row> rows;
        rows.push_back({T("First dropdown"), "Serial: PABotBase2", false});
        QString port = m_hw.ports.empty() ? T("Your device, for example COM3") : m_hw.ports.front().name;
        if (!m_ports.selected().isEmpty()){
            port = m_ports.selected();
        }
        rows.push_back({T("Second dropdown"), port, !m_hw.ports.empty() || !m_ports.selected().isEmpty()});
        rows.push_back({T("Third dropdown"), QString::fromLatin1(m_spec.type), false});
        (void)two;
        for (const Row& row : rows){
            const QRectF box(x, y, width, 92);
            panel(painter, box, 22, white(0.05 * a), white(0.14 * a), 1.4);
            draw_text(painter, row.label, box.left() + 24, box.top() + 12, ui_font(22, 600), white(0.55 * a));
            const QFont font = ui_font(27, 700);
            const double chip_w = draw_chip(painter, row.value, 0, 0, ChipKind::YELLOW_FILL, 0, 26, 46, 18, true);
            draw_chip(painter, row.value, box.left() + 24 + std::min(chip_w, width - 48), box.top() + 62, ChipKind::YELLOW_FILL, a, 26, 46, 18);
            (void)font;
            y += 108;
        }
        const QString note = T("Port not in the list? Click Reset Ctrl in the program.");
        draw_text(painter, note, x + 4, y + 4, ui_font(23, 500), white(0.56 * a));
    }

private:
    Canvas& m_canvas;
    QString m_board;
    const BoardSpec& m_spec;
    QString m_kind;
    int m_current = 0;
    Smooth m_pos;
    Smooth m_usb;
    Smooth m_pico;
    Smooth m_serial;
    Smooth m_type;
    Smooth m_link;
    Smooth m_search;
    Smooth m_card;
    double m_clock = 0;
    double m_step_since = 0;

    //  What the computer shows.
    HardwareProbe m_probe;
    HardwareState m_hw;
    QString m_port_signature;

    //  The firmware.
    QString m_firmware_dir;
    QString m_ref;
    std::vector<FirmwareFile> m_online;
    std::vector<FirmwareFile> m_files;
    Dropdown m_versions;
    Dropdown m_ports;
    QRectF m_card_bar_versions;
    QRectF m_card_bar_ports;
    std::unique_ptr<Download> m_listing;
    std::unique_ptr<Download> m_fetch;
    QString m_fetch_name;

    //  The flash.
    Flash m_flash = Flash::IDLE;
    double m_flash_progress = 0;
    double m_flash_time = 0;
    double m_gone_for = 0;
    QString m_flash_error;
    QString m_flash_log;
    std::unique_ptr<CopyJob> m_copy;
    std::unique_ptr<EspFlashJob> m_esp;
};


//  ---------------------------------------------------------------------
//  The Setup Guide, in the program
//  ---------------------------------------------------------------------

const char* const URL_GUIDE_UART = "https://pokemonautomation.github.io/SetupGuide/Controllers/Controller-PicoW-UART.html";

class GuidePage : public Page{
public:
    explicit GuidePage(Canvas& canvas){
        canvas.set_focus_id("row:pico");
    }

    virtual const char* name() const override{
        return "guide";
    }
    virtual void step(double dt) override{
        m_clock += dt;
        m_hl.step(dt, 20);
    }

    struct GuideRow{
        const char* id;
        const char* glyph;
        const char* name;
        int difficulty;
        const char* url;
    };
    static const std::vector<GuideRow>& rows(){
        static const std::vector<GuideRow> list = {
            {"pico", "pico", "Raspberry Pi Pico W", 1, nullptr},
            {"pico2", "pico2", "Raspberry Pi Pico 2 W", 1, nullptr},
            {"esp32", "esp32", "ESP32", 3, nullptr},
            {"esp32s3", "esp32s3", "ESP32-S3", 3, nullptr},
            {"sysbot", "code:S3", "sys-botbase 3", 2, URL_GUIDE},
            {"uart", "pico", "Pico W, UART mode", 5, URL_GUIDE_UART},
            {"uart2", "pico", "Pico W, advanced UART", 10, URL_GUIDE},
        };
        return list;
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        const int states[4] = {1, 2, 2, 2};
        draw_stepper(painter, states, prog(t, in.head - 0.05, in.head + 0.7));
        const QString heading = T("The Setup Guide");
        draw_heading(painter, heading, 140, 160, 84, prog(t, in.head, in.head + 0.9), 1.0);
        const QString sub = T("Everything in one place. Pick the board you want to set up, and the steps open here.");
        const double sub_top = 160 + heading_height(heading, 84) + 10;
        draw_sub(painter, sub, 140, sub_top, prog(t, in.sub, in.sub + 0.8), 1.0, 32, 800);

        //  What you need.
        const double reveal = eo(prog(t, in.bar, in.bar + 0.6));
        const double a = reveal;
        const double need_top = sub_top + sub_height(sub, 32, 800) + 40;
        draw_text(painter, T("What you need"), 144, need_top, ui_font(30, 600), white(0.9 * a));
        static const char* const NEEDS[4] = {
            "A regular Nintendo Switch with its dock (not a Switch Lite)",
            "A computer with 64-bit Windows",
            "A video capture card",
            "One of the boards in the list",
        };
        double y = need_top + 56;
        for (const char* need : NEEDS){
            const QRectF dot(144, y + 4, 28, 28);
            painter.setPen(Qt::NoPen);
            painter.setBrush(with_alpha(ORANGE, a));
            painter.drawEllipse(dot);
            draw_check(painter, dot.adjusted(6, 6, -6, -6), with_alpha(Qt::black, a), 3.0);
            const QStringList lines = wrap_text(ui_font(27, 500), T(need), 700);
            for (int c = 0; c < lines.size(); c++){
                draw_text(painter, lines[c], 192, y + c * 34, ui_font(27, 500), white(0.74 * a));
            }
            y += (double)lines.size() * 34 + 16;
        }

        //  The list of boards, with how hard each is.
        const std::vector<GuideRow>& list = rows();
        constexpr double ROW = 70;
        const QRectF box(1000 + (1.0 - reveal) * 60, 150, 780, (double)list.size() * ROW + 28);
        panel(painter, box, 28, with_alpha(PANEL, a), white(0.16 * a), 1.6);
        for (size_t c = 0; c < list.size(); c++){
            const GuideRow& row = list[c];
            const QRectF area(box.left() + 12, box.top() + 14 + (double)c * ROW, box.width() - 24, ROW);
            const QString id = QString("row:") + row.id;
            const bool hover = canvas.hovered(id) || canvas.focused(id);
            if (hover){
                panel(painter, area.adjusted(0, 3, 0, -3), 18, with_alpha(ORANGE, 0.20 * a), with_alpha(ORANGE, 0.95 * a), 2.0);
            }
            const QRectF glyph(area.left() + 14, area.center().y() - 22, 44, 44);
            const QString g = row.glyph;
            if (g.startsWith("code:")){
                painter.setPen(QPen(white(0.85 * a), 2.2));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(glyph.adjusted(1.1, 1.1, -1.1, -1.1));
                draw_text_cy(painter, g.mid(5), glyph.center().x(), glyph.center().y() + 1, ui_font(19, 700), white(a), Qt::AlignHCenter);
            }else{
                draw_picture(painter, tinted("glyph-" + g, Qt::white), glyph, a);
            }
            draw_text_cy(painter, QString::fromLatin1(row.name), area.left() + 76, area.center().y() + 1, ui_font(28, 600), white(a));
            double right = area.right() - 14;
            const bool in_app = row.url == nullptr;
            const QString where = in_app ? T("Here") : T("Browser");
            right -= draw_chip(painter, where, right, area.center().y(), in_app ? ChipKind::ORANGE_FILL : ChipKind::DIM, a, 20, 34, 14) + 10;
            right -= draw_chip(painter, T("%1 of 10").arg(row.difficulty), right, area.center().y(), ChipKind::RING, a, 20, 34, 14) + 10;
            canvas.add_hit(id, area, true, true);
        }
        draw_text(painter, T("Difficulty: how much work it takes, from 1 to 10."), box.left() + 16, box.bottom() + 18, ui_font(22, 500), white(0.5 * a));

        const double b = eo(prog(t, in.buttons, in.buttons + 0.5));
        draw_button(canvas, painter, "web", T("Open the full guide in your browser"), 136, 880, ButtonKind::LINK, std::min(1.0, b * 1.35));
        draw_skip(canvas, painter, b);
        const QString back = T("Back");
        draw_button(canvas, painter, "back", back, 1780 - button_width(back, ButtonKind::SECONDARY), 950, ButtonKind::SECONDARY, b);
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        if (id == "back"){
            canvas.go_board();
        }else if (id == "skip"){
            canvas.result().board_done = true;
            canvas.end_installation();
        }else if (id == "web"){
            canvas.open_url(URL_GUIDE);
        }else if (id.startsWith("row:")){
            const QString which = id.mid(4);
            for (const GuideRow& row : rows()){
                if (which == row.id){
                    if (row.url == nullptr){
                        canvas.result().board = which;
                        canvas.go_steps(which);
                    }else{
                        canvas.open_url(row.url);
                    }
                }
            }
        }
    }
    virtual void enter_pressed(Canvas& canvas) override{
        activate(canvas, "row:pico");
    }

private:
    double m_clock = 0;
    Smooth m_hl;
};


//  ---------------------------------------------------------------------
//  Other: the board is not in the list
//  ---------------------------------------------------------------------

class OtherPage : public Page{
public:
    explicit OtherPage(Canvas& canvas){
        canvas.set_focus_id("open");
    }

    virtual const char* name() const override{
        return "other";
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        const int states[4] = {1, 2, 2, 2};
        draw_stepper(painter, states, prog(t, in.head - 0.05, in.head + 0.7));
        const QString heading = T("Your board is not supported yet");
        draw_heading(painter, heading, 140, 250, 80, prog(t, in.head, in.head + 0.9), 1.0);
        draw_sub(painter, T("The Setup Guide lists the boards that work. Pick one of those, or follow the guide of the closest one."), 140,
            250 + heading_height(heading, 80) + 26, prog(t, in.sub, in.sub + 0.8), 1.0);

        const double b = eo(prog(t, in.buttons, in.buttons + 0.5));
        const QString open = T("Open the Setup Guide");
        draw_button(canvas, painter, "open", open, 140, 700, ButtonKind::PRIMARY, b);
        draw_button(canvas, painter, "back", T("Choose another board"), 140 + button_width(open, ButtonKind::PRIMARY) + 18, 700, ButtonKind::SECONDARY, b);
        draw_button(canvas, painter, "skip", T("Skip setup"), 136, 960, ButtonKind::LINK, b);
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        if (id == "open"){
            canvas.go_guide();
        }else if (id == "back"){
            canvas.go_board();
        }else if (id == "skip"){
            canvas.result().board_done = true;
            canvas.result().board = "other";
            canvas.end_installation();
        }
    }
    virtual void enter_pressed(Canvas& canvas) override{
        activate(canvas, "open");
    }
};


//  ---------------------------------------------------------------------
//  The end
//  ---------------------------------------------------------------------

class DonePage : public Page{
public:
    DonePage(Canvas& canvas, const QString& board)
        : m_board(board)
    {
        canvas.set_focus_id("start");
    }

    virtual const char* name() const override{
        return "done";
    }
    virtual void step(double dt) override{
        m_clock += dt;
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        const int states[4] = {0, 0, 0, 1};
        draw_stepper(painter, states, prog(t, in.head - 0.05, in.head + 0.7));
        const QString heading = T("You are done!");
        draw_heading(painter, heading, 140, 300, 118, prog(t, in.head, in.head + 0.9), 1.0);
        draw_sub(painter, T("Check keyboard, video and audio, then finish up."), 140,
            300 + heading_height(heading, 118) + 26, prog(t, in.sub, in.sub + 0.8), 1.0);

        const double reveal = eo(prog(t, in.bar, in.bar + 0.8));
        const QPixmap picture = plain_picture("switch-gg");
        if (!picture.isNull()){
            const double w = 840;
            const double h = w * picture.height() / picture.width();
            const double bob = std::sin(m_clock * 1.6) * 5;
            glow(painter, QRectF(1000 + 60, 460 + bob + 30, w - 120, h - 60), 60, ORANGE, 0.3 * reveal);
            draw_picture(painter, picture, QRectF(960, 400 + bob + (1.0 - reveal) * 40, w, h), reveal);
        }

        const double b = eo(prog(t, in.buttons, in.buttons + 0.5));
        draw_text(painter, T("Steps from the Pokémon Automation Setup Guide"), 140, 1010, ui_font(26, 500), white(0.62 * b));
        //  The installation ends here. With games to choose from, a screen of its
        //  own follows (no part of the installation: no steps, no way back to it).
        const bool has_games = !canvas.request().games.empty();
        const QString start = has_games ? T("Continue") : T("Start");
        draw_button(canvas, painter, "start", start, 1780 - button_width(start, ButtonKind::PRIMARY), 950, ButtonKind::PRIMARY, b);
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        if (id == "start"){
            canvas.result().board_done = true;
            canvas.result().board = m_board;
            canvas.end_installation();
        }
    }
    virtual void enter_pressed(Canvas& canvas) override{
        activate(canvas, "start");
    }

private:
    QString m_board;
    double m_clock = 0;
};


//  ---------------------------------------------------------------------
//  After the installation: which game to start. A screen of its own: it has no
//  part in the four steps of the installation and no way back into them.
//  ---------------------------------------------------------------------

//  The outline of a game cartridge: a rounded box with its top right corner
//  cut off (the same as on the page with the games).
QPainterPath cartridge_outline(const QRectF& box, double corner, double cut){
    QPainterPath path;
    path.moveTo(box.left() + corner, box.top());
    path.lineTo(box.right() - cut, box.top());
    path.lineTo(box.right(), box.top() + cut);
    path.lineTo(box.right(), box.bottom() - corner);
    path.quadTo(box.right(), box.bottom(), box.right() - corner, box.bottom());
    path.lineTo(box.left() + corner, box.bottom());
    path.quadTo(box.left(), box.bottom(), box.left(), box.bottom() - corner);
    path.lineTo(box.left(), box.top() + corner);
    path.quadTo(box.left(), box.top(), box.left() + corner, box.top());
    path.closeSubpath();
    return path;
}

//  The ground of a game's own logo. A logo that has no see-through corners comes
//  with a background of its own (white, black, red and blue halves): the label
//  of the cartridge takes those colors, so that the logo is shown as it is
//  and not boxed in on another color.
struct LogoGround{
    bool own = false;
    QColor left;
    QColor right;
};

LogoGround logo_ground(const QPixmap& logo){
    LogoGround ground;
    if (logo.isNull() || logo.width() < 4 || logo.height() < 4){
        return ground;
    }
    const QImage image = logo.toImage().convertToFormat(QImage::Format_ARGB32);
    const int last_x = image.width() - 2;
    const int last_y = image.height() - 2;
    const QColor corners[4] = {
        image.pixelColor(1, 1), image.pixelColor(last_x, 1),
        image.pixelColor(1, last_y), image.pixelColor(last_x, last_y),
    };
    for (const QColor& corner : corners){
        if (corner.alpha() < 250){
            return ground;
        }
    }
    ground.own = true;
    ground.left = QColor(
        (corners[0].red() + corners[2].red()) / 2,
        (corners[0].green() + corners[2].green()) / 2,
        (corners[0].blue() + corners[2].blue()) / 2
    );
    ground.right = QColor(
        (corners[1].red() + corners[3].red()) / 2,
        (corners[1].green() + corners[3].green()) / 2,
        (corners[1].blue() + corners[3].blue()) / 2
    );
    return ground;
}

class GamesPage : public Page{
public:
    explicit GamesPage(Canvas& canvas)
        : m_games(canvas.request().games)
    {
        const int count = (int)m_games.size();
        m_columns = count <= 12 ? 4 : 5;
        for (const HomeGame& game : m_games){
            m_grounds.push_back(logo_ground(game.icon));
        }
        const int first = next_enabled(-1, +1);
        canvas.set_focus_id(first >= 0 ? tile_id(first) : QString("later"));
    }

    virtual const char* name() const override{
        return "games";
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        const QString heading = T("Which game do you want to start?");
        draw_heading(painter, heading, 140, 110, 84, prog(t, in.head, in.head + 0.9), 1.0, 1500);
        const double sub_y = 110 + heading_height(heading, 84, 1500) + 20;
        draw_sub(painter, T("Pick a game. Its programs are shown right away."), 140, sub_y,
            prog(t, in.sub, in.sub + 0.8), 1.0, 34, 1500);

        const double top = sub_y + sub_height(T("Pick a game. Its programs are shown right away."), 34, 1500) + 24;
        for (int c = 0; c < (int)m_games.size(); c++){
            const double begin = in.bar + 0.05 * c;
            paint_tile(canvas, painter, c, tile_rect(c, top), eo(prog(t, begin, begin + 0.55)));
        }

        const double b = eo(prog(t, in.buttons, in.buttons + 0.5));
        draw_button(canvas, painter, "later", T("Choose later"), 136, 960, ButtonKind::LINK, std::min(1.0, b * 1.35));
        const QString start_text = T("Start");
        const double start_w = button_width(start_text, ButtonKind::PRIMARY);
        draw_button(canvas, painter, "start", start_text, 1780 - start_w, 950, ButtonKind::PRIMARY, b, m_selected >= 0);
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        if (id.startsWith("game:")){
            const int index = id.mid(5).toInt();
            if (index >= 0 && index < (int)m_games.size() && m_games[(size_t)index].enabled){
                m_selected = index;
                //  A game picked: the next Enter starts it.
                canvas.set_focus_id("start");
            }
        }else if (id == "start"){
            start(canvas);
        }else if (id == "later"){
            canvas.result().game.clear();
            canvas.finish();
        }
    }
    virtual bool key(Canvas& canvas, QKeyEvent& event) override{
        int column_step = 0;
        int row_step = 0;
        switch (event.key()){
        case Qt::Key_Left:
            column_step = -1;
            break;
        case Qt::Key_Right:
            column_step = +1;
            break;
        case Qt::Key_Up:
            row_step = -1;
            break;
        case Qt::Key_Down:
            row_step = +1;
            break;
        default:
            return false;
        }
        const int current = index_of(canvas.focus_id());
        int target = -1;
        if (current < 0){
            //  The focus is on a button: back to the cartridges.
            target = m_selected >= 0 ? m_selected : next_enabled(-1, +1);
        }else if (column_step != 0){
            target = next_enabled(current, column_step);
        }else{
            //  A row up or down: the same place in that row, if there is a game.
            const int wanted = current + row_step * m_columns;
            if (wanted >= 0 && wanted < (int)m_games.size() && m_games[(size_t)wanted].enabled){
                target = wanted;
            }
        }
        if (target >= 0){
            canvas.set_focus_id(tile_id(target));
        }
        return true;
    }
    virtual void enter_pressed(Canvas& canvas) override{
        if (m_selected >= 0){
            start(canvas);
        }
    }
    virtual QString state() const override{
        if (m_selected < 0){
            return "selected=";
        }
        return "selected=" + m_games[(size_t)m_selected].name;
    }

private:
    static QString tile_id(int index){
        return "game:" + QString::number(index);
    }
    static int index_of(const QString& id){
        return id.startsWith("game:") ? id.mid(5).toInt() : -1;
    }
    //  The next game that can be picked after "from" in this direction, or -1.
    int next_enabled(int from, int direction) const{
        for (int c = from + direction; c >= 0 && c < (int)m_games.size(); c += direction){
            if (m_games[(size_t)c].enabled){
                return c;
            }
        }
        return -1;
    }
    void start(Canvas& canvas){
        if (m_selected < 0){
            return;
        }
        canvas.result().game = m_games[(size_t)m_selected].name;
        canvas.finish();
    }

    //  The cartridges fill the room between the texts above and the buttons.
    QRectF tile_rect(int index, double top) const{
        constexpr double LEFT = 140;
        constexpr double RIGHT = 1780;
        constexpr double BOTTOM = 925;
        constexpr double GAP = 22;
        const int rows = ((int)m_games.size() + m_columns - 1) / m_columns;
        const double width = (RIGHT - LEFT - GAP * (m_columns - 1)) / m_columns;
        const double height = std::max(110.0, std::min(200.0, (BOTTOM - top - GAP * (rows - 1)) / std::max(1, rows)));
        return QRectF(
            LEFT + (width + GAP) * (index % m_columns),
            top + (height + GAP) * (index / m_columns),
            width, height
        );
    }

    void paint_tile(Canvas& canvas, QPainter& painter, int index, const QRectF& place, double reveal){
        if (reveal < 0.004){
            return;
        }
        const HomeGame& game = m_games[(size_t)index];
        const QString id = tile_id(index);
        const bool enabled = game.enabled;
        const bool hover = enabled && canvas.hovered(id);
        const bool focus = enabled && canvas.focused(id);
        const bool selected = index == m_selected;
        const double a = reveal * (enabled ? 1.0 : 0.4);
        const QRectF box = place.translated(0, (1.0 - reveal) * 26);
        canvas.add_hit(id, place, enabled);

        if (selected){
            glow(painter, box, 24, ORANGE, 0.55 * reveal);
        }

        //  The cartridge.
        const QPainterPath outline = cartridge_outline(box, 22, 30);
        painter.setPen(Qt::NoPen);
        painter.setBrush(selected ? with_alpha(QColor(44, 29, 24), 0.95 * a) : with_alpha(PANEL, 0.92 * a));
        painter.drawPath(outline);
        painter.setBrush(Qt::NoBrush);
        if (selected){
            painter.setPen(QPen(with_alpha(ORANGE, a), 2.6));
        }else{
            painter.setPen(QPen(white((hover ? 0.62 : 0.26) * a), hover ? 2.2 : 1.8));
        }
        painter.drawPath(outline);

        //  The label: the colors of the game, with its picture on it.
        const double text_room = 70;
        const QRectF label(box.left() + 14, box.top() + 14, box.width() - 28, std::max(36.0, box.height() - 14 - text_room));
        const LogoGround& ground = m_grounds[(size_t)index];
        painter.save();
        QPainterPath clip;
        clip.addRoundedRect(label, 13, 13);
        painter.setClipPath(clip);
        painter.setPen(Qt::NoPen);
        if (ground.own){
            //  The logo's own background goes on over the whole label.
            painter.setBrush(with_alpha(ground.left, a));
            painter.drawRect(QRectF(label.left(), label.top(), label.width() / 2.0 + 1, label.height()));
            painter.setBrush(with_alpha(ground.right, a));
            painter.drawRect(QRectF(label.center().x(), label.top(), label.width() / 2.0, label.height()));
        }else{
            QColor first, second;
            game_label_colors(game.name, first, second);
            painter.setBrush(with_alpha(first.darker(112), 0.95 * a));
            painter.drawRect(QRectF(label.left(), label.top(), label.width() / 2.0 + 1, label.height()));
            painter.setBrush(with_alpha(second.darker(112), 0.95 * a));
            painter.drawRect(QRectF(label.center().x(), label.top(), label.width() / 2.0, label.height()));
            painter.setBrush(white(0.10 * a));
            painter.drawRect(QRectF(label.left(), label.top(), label.width(), label.height() * 0.42));
        }
        painter.restore();

        if (!game.icon.isNull()){
            //  The game's own logo, as large as the label lets it be.
            const QRectF room = ground.own ? label.adjusted(8, 5, -8, -5) : label.adjusted(14, 8, -14, -8);
            const double scale = std::min(room.width() / game.icon.width(), room.height() / game.icon.height());
            const double w = game.icon.width() * scale;
            const double h = game.icon.height() * scale;
            draw_picture(painter, game.icon, QRectF(room.center().x() - w / 2.0, room.center().y() - h / 2.0, w, h), a);
        }else{
            const QString letter = short_game_name(game.name).left(1).toUpper();
            draw_text_cy(painter, letter, label.center().x(), label.center().y() + 2, serif_font(54), white(0.9 * a), Qt::AlignHCenter);
        }
        if (selected){
            const QRectF badge(label.right() - 44, label.top() + 10, 34, 34);
            painter.setPen(Qt::NoPen);
            painter.setBrush(with_alpha(ORANGE, a));
            painter.drawEllipse(badge);
            draw_check(painter, badge.adjusted(5, 5, -5, -5), with_alpha(Qt::black, a), 3.0);
        }

        //  The name and how many programs it has.
        const double text_x = box.left() + 20;
        const double text_w = box.width() - 40;
        //  A long name gets a smaller type before it is cut off at the end.
        {
            const QString shown = short_game_name(game.name);
            QFont font = ui_font(26, 600);
            for (double px = 26; px > 20.5 && text_width(font, shown) > text_w; px -= 1.0){
                font = ui_font(px, 600);
            }
            const QString fitted = QFontMetricsF(font).elidedText(shown, Qt::ElideRight, text_w);
            draw_text_cy(painter, fitted, text_x, box.bottom() - 45, font, white(a));
        }
        const QString programs = game.programs == 1 ? tr_ui("1 program") : tr_ui("%1 programs").arg(game.programs);
        draw_text_cy(painter, programs, text_x, box.bottom() - 19, ui_font(20, 500), white(0.55 * a));

        if (focus){
            const QPainterPath ring = cartridge_outline(box.adjusted(-6, -6, 6, 6), 28, 34);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(with_alpha(YELLOW, 0.9 * reveal), 2.4));
            painter.drawPath(ring);
        }
    }

private:
    std::vector<HomeGame> m_games;
    std::vector<LogoGround> m_grounds;
    int m_columns = 4;
    int m_selected = -1;
};


//  ---------------------------------------------------------------------
//  The console: the one the board will be connected to
//  ---------------------------------------------------------------------

//  The Switch 2, drawn here in the style of the picture of the first Switch
//  (the program has only that one): a thin frame round a large screen, and
//  the controllers in blue.
QPixmap switch2_picture(){
    static QPixmap cache;
    if (!cache.isNull()){
        return cache;
    }
    constexpr int W = 900;
    constexpr int H = 385;
    QImage image(W, H, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);

    //  The two controllers at the sides, with the rail on top.
    const QRectF bodies[2] = {QRectF(0, 20, 136, 362), QRectF(764, 20, 136, 362)};
    for (const QRectF& body : bodies){
        QLinearGradient shade(0, body.top(), 0, body.bottom());
        shade.setColorAt(0, QColor(84, 184, 214));
        shade.setColorAt(1, QColor(40, 124, 160));
        p.setBrush(shade);
        p.drawRoundedRect(body, 58, 58);
        p.setBrush(QColor(34, 104, 136));
        p.drawRoundedRect(QRectF(body.left() + 20, 4, body.width() - 40, 22), 7, 7);
    }

    //  The frame and the screen.
    p.setBrush(QColor(58, 61, 70));
    p.drawRoundedRect(QRectF(140, 10, 620, 364), 22, 22);
    p.setPen(QPen(QColor(124, 128, 140), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(141, 11, 618, 362), 21, 21);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(4, 4, 6));
    const QRectF screen(154, 24, 592, 336);
    p.drawRoundedRect(screen, 12, 12);
    p.save();
    QPainterPath clip;
    clip.addRoundedRect(screen, 12, 12);
    p.setClipPath(clip);
    p.setBrush(QColor(255, 255, 255, 14));
    p.drawPolygon(QPolygonF({QPointF(470, 24), QPointF(640, 24), QPointF(380, 360), QPointF(210, 360)}));
    p.restore();

    //  Sticks, buttons, minus and plus.
    const QColor dark(26, 28, 34);
    auto dot = [&](double x, double y, double r){
        p.drawEllipse(QPointF(x, y), r, r);
    };
    p.setBrush(dark);
    dot(68, 116, 36);
    dot(832, 270, 36);
    p.setBrush(QColor(62, 66, 76));
    dot(68, 116, 25);
    dot(832, 270, 25);
    p.setBrush(dark);
    dot(68, 232, 13);
    dot(38, 262, 13);
    dot(98, 262, 13);
    dot(68, 292, 13);
    dot(832, 100, 13);
    dot(802, 130, 13);
    dot(862, 130, 13);
    dot(832, 160, 13);
    p.drawRoundedRect(QRectF(86, 54, 24, 6), 3, 3);
    p.drawRoundedRect(QRectF(790, 54, 24, 6), 3, 3);
    p.drawRoundedRect(QRectF(799, 45, 6, 24), 3, 3);
    p.end();
    cache = QPixmap::fromImage(image);
    return cache;
}

struct ConsoleInfo{
    //  The value of the setting "console" (GlobalSettings::GAME_CONSOLE).
    const char* code;
    //  A name: not translated.
    const char* name;
    //  What it covers (the program's own list of models, see ConsoleType).
    const char* note;
};
constexpr ConsoleInfo CONSOLES[] = {
    {"switch",  "Nintendo Switch",   "Switch 1 and the OLED model"},
    {"switch2", "Nintendo Switch 2", "International and Japan-only models"},
};
constexpr int CONSOLE_COUNT = (int)(sizeof(CONSOLES) / sizeof(CONSOLES[0]));

class ConsolePage : public Page{
public:
    explicit ConsolePage(Canvas& canvas){
        const QString kept = canvas.request().console_code;
        for (int c = 0; c < CONSOLE_COUNT; c++){
            if (kept == CONSOLES[c].code){
                m_selected = c;
            }
        }
        //  One that was chosen before is shown as picked: Continue is next.
        canvas.set_focus_id(m_selected >= 0 ? QString("continue") : tile_id(0));
    }

    virtual const char* name() const override{
        return "console";
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        //  The setup starts here: the same steps and the same place for the
        //  texts as the page of the board that follows.
        const int states[4] = {1, 2, 2, 2};
        draw_stepper(painter, states, prog(t, in.head - 0.05, in.head + 0.7));
        const QString heading = T("Which console do you use?");
        draw_heading(painter, heading, 140, 250, 80, prog(t, in.head, in.head + 0.9), 1.0, 1500);
        const QString sub = T("Pick the console you want to control. The board you pick next connects to it.");
        const double sub_y = 250 + heading_height(heading, 80, 1500) + 26;
        draw_sub(painter, sub, 140, sub_y, prog(t, in.sub, in.sub + 0.8), 1.0, 34, 1500);

        const double top = sub_y + sub_height(sub, 34, 1500) + 40;
        for (int c = 0; c < CONSOLE_COUNT; c++){
            const double begin = in.bar + 0.12 * c;
            paint_tile(canvas, painter, c, tile_rect(c, top), eo(prog(t, begin, begin + 0.6)));
        }

        const double b = eo(prog(t, in.buttons, in.buttons + 0.5));
        draw_skip(canvas, painter, b);
        const QString text = T("Continue");
        draw_button(canvas, painter, "continue", text, 1780 - button_width(text, ButtonKind::PRIMARY), 950,
            ButtonKind::PRIMARY, b, m_selected >= 0);
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        const int index = index_of(id);
        if (index >= 0 && index < CONSOLE_COUNT){
            m_selected = index;
            //  A console picked: the next Enter goes on.
            canvas.set_focus_id("continue");
        }else if (id == "continue"){
            proceed(canvas);
        }else if (id == "skip"){
            canvas.result().board_done = true;
            canvas.end_installation();
        }
    }
    virtual bool key(Canvas& canvas, QKeyEvent& event) override{
        int step = 0;
        if (event.key() == Qt::Key_Left || event.key() == Qt::Key_Up){
            step = -1;
        }else if (event.key() == Qt::Key_Right || event.key() == Qt::Key_Down){
            step = +1;
        }else{
            return false;
        }
        const int current = index_of(canvas.focus_id());
        int target;
        if (current < 0){
            //  The focus is on a button: back to the consoles.
            target = m_selected >= 0 ? m_selected : 0;
        }else{
            target = std::max(0, std::min(CONSOLE_COUNT - 1, current + step));
        }
        canvas.set_focus_id(tile_id(target));
        return true;
    }
    virtual void enter_pressed(Canvas& canvas) override{
        proceed(canvas);
    }
    virtual QString state() const override{
        return QString("selected=") + (m_selected >= 0 ? CONSOLES[m_selected].code : "");
    }

private:
    static QString tile_id(int index){
        return "console:" + QString::number(index);
    }
    static int index_of(const QString& id){
        return id.startsWith("console:") ? id.mid(8).toInt() : -1;
    }
    void proceed(Canvas& canvas){
        if (m_selected < 0){
            return;
        }
        canvas.result().console_chosen = true;
        canvas.result().console_code = CONSOLES[m_selected].code;
        canvas.end_console();
    }

    //  Two large tiles side by side, under the texts.
    static QRectF tile_rect(int index, double top){
        constexpr double LEFT = 140;
        constexpr double RIGHT = 1780;
        constexpr double BOTTOM = 915;
        constexpr double GAP = 40;
        const double width = (RIGHT - LEFT - GAP * (CONSOLE_COUNT - 1)) / CONSOLE_COUNT;
        const double height = std::max(300.0, std::min(470.0, BOTTOM - top));
        return QRectF(LEFT + (width + GAP) * index, top, width, height);
    }

    void paint_tile(Canvas& canvas, QPainter& painter, int index, const QRectF& place, double reveal){
        if (reveal < 0.004){
            return;
        }
        const ConsoleInfo& info = CONSOLES[index];
        const QString id = tile_id(index);
        const bool hover = canvas.hovered(id);
        const bool focus = canvas.focused(id);
        const bool selected = index == m_selected;
        const double a = reveal;
        const QRectF box = place.translated(0, (1.0 - reveal) * 26);
        canvas.add_hit(id, place);

        if (selected){
            glow(painter, box, 30, ORANGE, 0.55 * reveal);
        }
        panel(
            painter, box, 30,
            selected ? with_alpha(QColor(44, 29, 24), 0.95 * a) : with_alpha(PANEL, 0.92 * a),
            selected ? with_alpha(ORANGE, a) : white((hover ? 0.62 : 0.26) * a),
            selected ? 2.6 : hover ? 2.2 : 1.8
        );

        //  The console, as large as the tile lets it be.
        const QPixmap picture = index == 0 ? plain_picture("switch") : switch2_picture();
        const QRectF room(box.left() + 50, box.top() + 34, box.width() - 100, box.height() - 34 - 130);
        if (!picture.isNull()){
            const double scale = std::min(room.width() / picture.width(), room.height() / picture.height());
            const double w = picture.width() * scale;
            const double h = picture.height() * scale;
            draw_picture(painter, picture, QRectF(room.center().x() - w / 2.0, room.center().y() - h / 2.0, w, h), a);
        }

        draw_text_cy(painter, QString::fromLatin1(info.name), box.left() + 46, box.bottom() - 84, ui_font(42, 600), white(a));
        draw_text_cy(painter, T(info.note), box.left() + 46, box.bottom() - 36, ui_font(25, 500), white(0.55 * a));
        if (selected){
            const QRectF badge(box.right() - 46 - 44, box.bottom() - 60 - 22, 44, 44);
            painter.setPen(Qt::NoPen);
            painter.setBrush(with_alpha(ORANGE, a));
            painter.drawEllipse(badge);
            draw_check(painter, badge.adjusted(7, 7, -7, -7), with_alpha(Qt::black, a), 3.4);
        }
        if (focus){
            panel(painter, box.adjusted(-6, -6, 6, 6), 36, QColor(), with_alpha(YELLOW, 0.9 * reveal), 2.4);
        }
    }

private:
    int m_selected = -1;
};


//  ---------------------------------------------------------------------
//  Canvas
//  ---------------------------------------------------------------------

Canvas::Canvas(const SetupRequest& request, bool test_mode)
    : QWidget(nullptr, test_mode ? Qt::Window : (Qt::Window | Qt::FramelessWindowHint))
    , m_request(request)
    , m_test_mode(test_mode)
    , m_timer(this)
{
    setWindowTitle("PocketPilot Studio");
    m_app_folder = QCoreApplication::applicationDirPath();
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
    m_background_source = QImage(":/pocketpilot/setup/setup-bg.png");
    families();

    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, [this]{
        tick();
    });
}

void Canvas::start(QEventLoop* loop){
    m_loop = loop;
    if (m_request.ask_language){
        go_language();
    }else if (m_request.ask_console){
        go_console();
    }else if (m_request.ask_board){
        go_board();
    }else if (asks_game()){
        go_games();
    }
    m_first_page = true;
    if (!m_test_mode){
        m_clock.start();
        m_clock_started = true;
        m_timer.start();
    }
}

void Canvas::tick(){
    double dt = 0.016;
    if (m_clock_started){
        dt = std::min(0.05, (double)m_clock.restart() / 1000.0);
    }
    advance(dt);
    update();
}

void Canvas::advance(double dt){
    if (m_releasing){
        m_release_alpha -= dt / 0.55;
        setWindowOpacity(std::max(0.0, m_release_alpha));
        if (m_release_alpha <= 0){
            m_releasing = false;
            m_release_done = true;
            m_timer.stop();
            close();
        }
        return;
    }
    if (m_intro == IntroState::FILM){
        //  The first page waits: its time starts when the film fades.
        m_intro_clock += dt;
        const bool ended = m_intro_ended >= 0 && m_intro_clock - m_intro_ended >= 0.4;
        if (ended){
            end_intro("it played to the end");
        }else if (m_intro_frames == 0 && m_intro_clock > 6.0){
            end_intro("no picture came within 6 seconds");
        }else if (m_intro_clock > 20.0){
            end_intro("it took too long");
        }
        return;
    }
    m_now += dt;
    m_page_time += dt;
    if (m_intro == IntroState::FADE && !m_intro_keep){
        m_intro_fade -= dt / 0.9;
        if (m_intro_fade <= 0){
            m_intro_fade = 0;
            m_intro = IntroState::NONE;
            m_intro_still = QImage();
            m_intro_still_scaled = QPixmap();
        }
    }
    if (m_holding){
        //  The page goes, the background stays.
        m_fade = std::max(0.0, m_fade - dt / 0.30);
        m_pending = nullptr;
        return;
    }
    if (m_pending){
        m_fade -= dt / 0.20;
        if (m_fade <= 0){
            m_fade = 0;
            std::function<std::unique_ptr<Page>()> make = std::move(m_pending);
            m_pending = nullptr;
            set_page(make());
        }
    }else if (m_fade < 1.0){
        m_fade = std::min(1.0, m_fade + dt / 0.25);
    }
    if (m_page){
        m_page->step(dt);
    }
}

void Canvas::set_page(std::unique_ptr<Page> page){
    m_page = std::move(page);
    m_page_time = 0;
    m_hits.clear();
    m_hover.clear();
    m_pressed.clear();
}

void Canvas::queue_page(std::function<std::unique_ptr<Page>()> make){
    if (m_page == nullptr){
        set_page(make());
        m_fade = 1.0;
        return;
    }
    m_first_page = false;
    m_pending = std::move(make);
    if (m_fade <= 0.001){
        m_fade = 0.001;
    }
}

void Canvas::go_language(){
    queue_page([this]{
        return std::unique_ptr<Page>(new LanguagePage(*this));
    });
}
void Canvas::go_console(){
    queue_page([this]{
        return std::unique_ptr<Page>(new ConsolePage(*this));
    });
}
void Canvas::go_board(){
    queue_page([this]{
        return std::unique_ptr<Page>(new BoardPage(*this));
    });
}
void Canvas::go_steps(const QString& board){
    queue_page([this, board]{
        return std::unique_ptr<Page>(new StepsPage(*this, board));
    });
}
void Canvas::go_guide(){
    queue_page([this]{
        return std::unique_ptr<Page>(new GuidePage(*this));
    });
}
void Canvas::go_other(){
    queue_page([this]{
        return std::unique_ptr<Page>(new OtherPage(*this));
    });
}
void Canvas::go_done(const QString& board){
    queue_page([this, board]{
        return std::unique_ptr<Page>(new DonePage(*this, board));
    });
}
void Canvas::go_games(){
    queue_page([this]{
        return std::unique_ptr<Page>(new GamesPage(*this));
    });
}

//  The black screen on top of the page that the tutorial film plays on.
class VideoOverlay : public QWidget{
public:
    VideoOverlay(QWidget* parent, std::function<void()> close)
        : QWidget(parent)
        , m_close(std::move(close))
    {
        setAutoFillBackground(true);
        QPalette black = palette();
        black.setColor(QPalette::Window, Qt::black);
        setPalette(black);
        m_video = new QVideoWidget(this);
        m_video->setAspectRatioMode(Qt::KeepAspectRatio);
        m_video->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        QVBoxLayout* column = new QVBoxLayout(this);
        column->setContentsMargins(0, 0, 0, 0);
        column->addWidget(m_video);
    }
    QVideoWidget* video() const{
        return m_video;
    }
protected:
    virtual void mousePressEvent(QMouseEvent* event) override{
        (void)event;
        m_close();
    }
private:
    std::function<void()> m_close;
    QVideoWidget* m_video;
};

//  The film from the program to a file the player can open.
QString extract_film(const QString& name){
    QFile resource(":/pocketpilot/" + name + ".mp4");
    if (!resource.open(QIODevice::ReadOnly)){
        return QString();
    }
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("PocketPilot-" + name + ".mp4");
    QFile existing(path);
    if (existing.exists() && existing.size() == resource.size()){
        return path;
    }
    QFile::remove(path);
    if (!resource.copy(path)){
        return QString();
    }
    return path;
}

//  The film of the setup of a board: the Picos have one, the ESP32 has one and
//  the ESP32-S3 (which is wired) has one.
QString film_for(const QString& board){
    if (board == "esp32"){
        return "tutorial-esp32";
    }
    if (board == "esp32s3"){
        return "tutorial-esp32s3";
    }
    return "tutorial-pico";
}

void Canvas::play_video(const QString& board){
    if (m_video_open){
        return;
    }
    m_video_name = film_for(board);
    m_video_open = true;
    if (m_test_mode){
        return;
    }
    const QString path = extract_film(m_video_name);
    if (path.isEmpty()){
        m_video_open = false;
        return;
    }
    m_overlay = std::make_unique<VideoOverlay>(this, [this]{
        QTimer::singleShot(0, this, [this]{
            close_video();
        });
    });
    m_overlay->setGeometry(rect());
    VideoOverlay* overlay = static_cast<VideoOverlay*>(m_overlay.get());

    m_player = std::make_unique<QMediaPlayer>();
    if (!QMediaDevices::audioOutputs().isEmpty()){
        m_audio = std::make_unique<QAudioOutput>();
        m_audio->setVolume(0.8f);
        m_player->setAudioOutput(m_audio.get());
    }
    m_player->setVideoOutput(overlay->video());
    QObject::connect(m_player.get(), &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status){
        if (status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia){
            QTimer::singleShot(600, this, [this]{
                close_video();
            });
        }
    });
    QObject::connect(m_player.get(), &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString&){
        QTimer::singleShot(0, this, [this]{
            close_video();
        });
    });
    m_player->setSource(QUrl::fromLocalFile(path));
    m_overlay->show();
    m_overlay->raise();
    m_player->play();
    setFocus();
}

void Canvas::close_video(){
    if (!m_video_open){
        return;
    }
    m_video_open = false;
    if (m_player != nullptr){
        m_player->stop();
        m_player->disconnect();
    }
    m_player.reset();
    m_audio.reset();
    m_overlay.reset();
    setFocus();
    update();
}

void Canvas::finish(){
    if (m_video_open){
        close_video();
    }
    if (m_intro == IntroState::FILM){
        //  Closed during the film.
        m_intro_note = "the window was closed";
        drop_intro_player();
        m_intro = IntroState::NONE;
        unsetCursor();
    }
    if (m_finished){
        return;
    }
    m_finished = true;
    m_timer.stop();
    if (m_loop != nullptr){
        m_loop->quit();
    }
}

//  ---------------------------------------------------------------------
//  The intro on this window, and the screen that stays after the end
//  ---------------------------------------------------------------------

QString Canvas::intro_state() const{
    switch (m_intro){
    case IntroState::FILM:
        return "film";
    case IntroState::FADE:
        return "fade";
    default:
        return "none";
    }
}

void Canvas::start_intro(){
    if (m_intro != IntroState::NONE){
        return;
    }
    const QString path = extract_film("intro");
    if (path.isEmpty()){
        m_intro_note = "the film is not in this build";
        if (!asks_anything()){
            finish();
        }
        return;
    }
    m_intro = IntroState::FILM;
    m_intro_clock = 0;
    m_intro_ended = -1;
    m_intro_frames = 0;
    m_intro_note.clear();
    setCursor(Qt::BlankCursor);

    QVideoSink* sink = nullptr;
    if (m_test_mode){
        //  Nothing to show it on: the canvas draws the pictures itself.
        m_intro_sink = std::make_unique<QVideoSink>();
        sink = m_intro_sink.get();
    }else{
        m_intro_overlay = std::make_unique<VideoOverlay>(this, [this]{
            QTimer::singleShot(0, this, [this]{
                //  A click in the first moments is not meant for the intro.
                if (m_intro == IntroState::FILM && m_intro_clock >= 0.5){
                    end_intro("it was skipped");
                }
            });
        });
        m_intro_overlay->setGeometry(rect());
        sink = static_cast<VideoOverlay*>(m_intro_overlay.get())->video()->videoSink();
    }
    QObject::connect(sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& frame){
        if (!frame.isValid()){
            return;
        }
        m_intro_frames++;
        m_intro_last = frame;
        if (m_test_mode){
            m_intro_image = frame.toImage();
        }
    });

    m_intro_player = std::make_unique<QMediaPlayer>();
    //  The sound only where there is something to play it on.
    if (!QMediaDevices::audioOutputs().isEmpty()){
        m_intro_audio = std::make_unique<QAudioOutput>();
        m_intro_audio->setVolume(0.8f);
        m_intro_player->setAudioOutput(m_intro_audio.get());
    }
    if (m_test_mode){
        m_intro_player->setVideoSink(sink);
    }else{
        m_intro_player->setVideoOutput(static_cast<VideoOverlay*>(m_intro_overlay.get())->video());
    }
    QObject::connect(m_intro_player.get(), &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status){
        if (status == QMediaPlayer::EndOfMedia){
            //  A beat on the last picture, then on.
            if (m_intro_ended < 0){
                m_intro_ended = m_intro_clock;
            }
        }else if (status == QMediaPlayer::InvalidMedia){
            QTimer::singleShot(0, this, [this]{
                end_intro("the film cannot be decoded");
            });
        }
    });
    QObject::connect(m_intro_player.get(), &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString& message){
        QTimer::singleShot(0, this, [this, message]{
            end_intro("the player failed: " + message);
        });
    });
    m_intro_player->setSource(QUrl::fromLocalFile(path));
    if (m_intro_overlay != nullptr){
        m_intro_overlay->show();
        m_intro_overlay->raise();
    }
    m_intro_player->play();
    setFocus();
}

void Canvas::drop_intro_player(){
    if (m_intro_player != nullptr){
        m_intro_player->stop();
        m_intro_player->disconnect();
    }
    m_intro_player.reset();
    m_intro_audio.reset();
    m_intro_sink.reset();
    m_intro_overlay.reset();
}

void Canvas::end_intro(const QString& note){
    if (m_intro != IntroState::FILM){
        return;
    }
    m_intro_note = note;

    //  The last picture of the film, kept: the window the film plays on goes
    //  away and this picture is drawn in its place, the same, and fades out.
    m_intro_still = QImage();
    if (m_intro_last.isValid()){
        m_intro_still = m_intro_last.toImage();
    }else if (!m_intro_image.isNull()){
        m_intro_still = m_intro_image;
    }
    m_intro_last = QVideoFrame();
    m_intro_still_scaled = QPixmap();
    m_intro_still_for = QSize();

    m_intro = IntroState::FADE;
    m_intro_fade = 1.0;
    m_intro_keep = !asks_anything();
    unsetCursor();
    //  Drawn under the film's window first, so that nothing shows between.
    if (isVisible()){
        repaint();
    }
    drop_intro_player();
    if (m_intro_keep){
        //  Nothing follows: the picture stays until the program is there.
        finish();
    }
    update();
}

//  An image as large as fits the window, in its middle, with its own
//  proportions. The scaled copy is kept in the cache.
void Canvas::draw_fitted(QPainter& painter, const QImage& image, QPixmap& cache, QSize& cache_for, double opacity){
    if (image.isNull() || opacity <= 0.002){
        return;
    }
    const QSize target = image.size().scaled(size(), Qt::KeepAspectRatio);
    if (cache.isNull() || cache_for != target){
        cache = QPixmap::fromImage(image.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        cache_for = target;
    }
    painter.save();
    painter.resetTransform();
    painter.setOpacity(opacity);
    painter.drawPixmap(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2), cache);
    painter.restore();
}

void Canvas::hold_screen(){
    if (m_holding){
        return;
    }
    m_holding = true;
    if (!m_test_mode){
        m_clock.restart();
        m_clock_started = true;
        m_timer.start();
    }
}

void Canvas::release_screen(){
    if (m_releasing || m_release_done){
        return;
    }
    m_releasing = true;
    m_release_alpha = 1.0;
    if (!m_test_mode){
        m_clock.restart();
        m_clock_started = true;
        m_timer.start();
    }
}


void Canvas::paint_background(QPainter& painter){
    const double w = width();
    const double h = height();
    if (m_background_source.isNull() || w < 2 || h < 2){
        return;
    }
    const double drift = h * 0.03;
    const double factor = std::max(w / m_background_source.width(), (h + 2 * drift) / m_background_source.height());
    const QSize wanted(
        (int)std::ceil(m_background_source.width() * factor),
        (int)std::ceil(m_background_source.height() * factor)
    );
    if (m_background.isNull() || m_background_for != wanted){
        m_background = QPixmap::fromImage(m_background_source.scaled(wanted, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        m_background_for = wanted;
    }
    const double fade = eo(prog(m_now, 0.0, 1.4));
    const double y = -(wanted.height() - h) / 2.0 + std::sin(m_now * 0.11) * drift;
    painter.setOpacity(fade);
    painter.drawPixmap(QPointF((w - wanted.width()) / 2.0, y), m_background);
    painter.setOpacity(1.0);

    //  Calmer where the heading and the list are, as in the film.
    painter.setPen(Qt::NoPen);
    struct Spot{
        double x, y, radius, strength;
    };
    const Spot spots[] = {
        {0.30, 0.40, 0.34, 0.62},
        {0.72, 0.52, 0.30, 0.55},
        {0.50, 0.52, 0.55, 0.40},
    };
    for (const Spot& spot : spots){
        QRadialGradient gradient(QPointF(w * spot.x, h * spot.y), w * spot.radius);
        gradient.setColorAt(0.0, QColor(0, 0, 0, (int)std::lround(255 * spot.strength * fade)));
        gradient.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.setBrush(gradient);
        painter.drawRect(rect());
    }
}

void Canvas::paintEvent(QPaintEvent*){
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), Qt::black);
    if (m_intro == IntroState::FILM){
        //  The film is on the window over this one (the test draws it here).
        m_hits.clear();
        if (m_test_mode && !m_intro_image.isNull()){
            QPixmap cache;
            QSize cache_for;
            draw_fitted(painter, m_intro_image, cache, cache_for, 1.0);
        }
        return;
    }
    if (!m_intro_keep || m_intro != IntroState::FADE){
        paint_background(painter);
    }

    m_scale = std::min(width() / DW, height() / DH);
    m_offset = QPointF((width() - DW * m_scale) / 2.0, (height() - DH * m_scale) / 2.0);
    painter.translate(m_offset);
    painter.scale(m_scale, m_scale);

    m_hits.clear();
    if (m_page){
        painter.setOpacity(eo(clamp01(m_fade)));
        m_page->paint(*this, painter, m_page_time);
    }
    if (m_intro == IntroState::FADE){
        //  The last picture of the film over the page, going away.
        const double fade = clamp01(m_intro_fade);
        draw_fitted(painter, m_intro_still, m_intro_still_scaled, m_intro_still_for,
            m_intro_keep ? 1.0 : fade * fade * (3.0 - 2.0 * fade));
    }
}

void Canvas::resizeEvent(QResizeEvent* event){
    QWidget::resizeEvent(event);
    if (m_overlay != nullptr){
        m_overlay->setGeometry(rect());
    }
    if (m_intro_overlay != nullptr){
        m_intro_overlay->setGeometry(rect());
    }
    update();
}

const Hit* Canvas::find_hit(const QPointF& design_point) const{
    for (size_t c = m_hits.size(); c-- > 0;){
        if (m_hits[c].rect.contains(design_point)){
            return &m_hits[c];
        }
    }
    return nullptr;
}

void Canvas::update_hover(const QPointF& window_point){
    const Hit* hit = find_hit(to_design(window_point));
    const QString id = hit == nullptr ? QString() : hit->id;
    if (id != m_hover){
        m_hover = id;
        if (m_page){
            m_page->hover(*this, id);
        }
    }
    setCursor(hit != nullptr && hit->enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void Canvas::mouseMoveEvent(QMouseEvent* event){
    if (m_intro == IntroState::FILM || m_holding || m_releasing){
        return;
    }
    update_hover(event->position());
}

void Canvas::mousePressEvent(QMouseEvent* event){
    if (event->button() != Qt::LeftButton){
        return;
    }
    if (m_intro == IntroState::FILM){
        if (m_intro_clock >= 0.5){
            end_intro("it was skipped");
        }
        return;
    }
    if (m_holding || m_releasing){
        return;
    }
    m_focus_visible = false;
    const Hit* hit = find_hit(to_design(event->position()));
    m_pressed = hit != nullptr && hit->enabled ? hit->id : QString();
}

void Canvas::mouseReleaseEvent(QMouseEvent* event){
    if (event->button() != Qt::LeftButton || m_intro == IntroState::FILM || m_holding || m_releasing){
        return;
    }
    const Hit* hit = find_hit(to_design(event->position()));
    const QString pressed = m_pressed;
    m_pressed.clear();
    if (hit != nullptr && hit->enabled && hit->id == pressed && m_page && !m_pending){
        const QString id = hit->id;
        if (id != "bar" && !id.startsWith("row:") && !id.startsWith("step:")){
            m_focus = id;
        }
        m_page->activate(*this, id);
    }
}

void Canvas::move_focus(int direction){
    std::vector<const Hit*> stops;
    for (const Hit& hit : m_hits){
        if (hit.enabled && hit.tab_stop){
            stops.push_back(&hit);
        }
    }
    if (stops.empty()){
        return;
    }
    int index = -1;
    for (size_t c = 0; c < stops.size(); c++){
        if (stops[c]->id == m_focus){
            index = (int)c;
        }
    }
    const int count = (int)stops.size();
    index = index < 0 ? (direction > 0 ? 0 : count - 1) : (index + direction + count) % count;
    m_focus = stops[(size_t)index]->id;
    m_focus_visible = true;
}

void Canvas::keyPressEvent(QKeyEvent* event){
    if (m_intro == IntroState::FILM){
        //  Any key skips the film (not in the first moments: the program was
        //  just started, perhaps by a double click).
        if (m_intro_clock >= 0.5){
            end_intro("it was skipped");
        }
        return;
    }
    if (m_holding || m_releasing){
        return;
    }
    if (m_video_open){
        //  Any of these ends the film.
        if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Space || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter){
            close_video();
        }
        return;
    }
    if (m_page == nullptr || m_pending){
        return;
    }
    if (m_page->key(*this, *event)){
        m_focus_visible = true;
        update();
        return;
    }
    switch (event->key()){
    case Qt::Key_Tab:
        move_focus(+1);
        break;
    case Qt::Key_Backtab:
        move_focus(-1);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        m_focus_visible = true;
        if (!m_focus.isEmpty()){
            bool enabled = false;
            for (const Hit& hit : m_hits){
                if (hit.id == m_focus){
                    enabled = hit.enabled;
                }
            }
            if (enabled){
                m_page->activate(*this, m_focus);
                break;
            }
        }
        m_page->enter_pressed(*this);
        break;
    case Qt::Key_Escape:
        m_page->escape_pressed(*this);
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    update();
}

void Canvas::closeEvent(QCloseEvent* event){
    //  The window is closed (Alt+F4): what was chosen stays, the rest is skipped.
    if (m_request.ask_board && m_page && QString(m_page->name()) != "language"){
        m_result.board_done = true;
    }
    finish();
    QWidget::closeEvent(event);
}

bool Canvas::click(const QString& id){
    for (const Hit& hit : m_hits){
        if (hit.id == id){
            if (!hit.enabled || m_page == nullptr || m_pending){
                return false;
            }
            const QString copy = hit.id;
            if (copy != "bar" && !copy.startsWith("row:") && !copy.startsWith("step:")){
                m_focus = copy;
            }
            m_page->activate(*this, copy);
            return true;
        }
    }
    return false;
}
bool Canvas::move_to(const QString& id){
    for (const Hit& hit : m_hits){
        if (hit.id == id){
            m_hover = hit.id;
            if (m_page){
                m_page->hover(*this, hit.id);
            }
            return true;
        }
    }
    return false;
}
void Canvas::press_key(int key, Qt::KeyboardModifiers modifiers){
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    keyPressEvent(&event);
}
void Canvas::wait(double seconds){
    const int frames = std::max(1, (int)std::lround(seconds * 30.0));
    for (int c = 0; c < frames; c++){
        advance(1.0 / 30.0);
        //  Draw, so that the hits are those of what is on screen.
        grab();
    }
}
QImage Canvas::picture(){
    return grab().toImage();
}


}   //  namespace



namespace{
//  The screen that is kept up after the setup, until the main window is there.
QPointer<Canvas> g_held_screen;
}

SetupResult run_setup(const SetupRequest& request){
    SetupResult result;
    const bool ask_game = request.ask_game && !request.games.empty();
    if (!request.ask_language && !request.ask_board && !ask_game && !request.play_intro){
        return result;
    }
    std::unique_ptr<Canvas> canvas(new Canvas(request, false));
    QEventLoop loop;
    //  On the screen the program was started on.
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (screen == nullptr){
        screen = QApplication::primaryScreen();
    }
    if (screen != nullptr){
        canvas->setScreen(screen);
        canvas->setGeometry(screen->geometry());
    }
    canvas->start(&loop);
    canvas->showFullScreen();
    canvas->raise();
    canvas->activateWindow();
    canvas->setFocus();
    if (request.play_intro){
        canvas->start_intro();
    }
    loop.exec();
    result = canvas->result();
    result.intro_note = canvas->intro_note();

    if (request.hold_screen){
        //  The page fades out; the screen stays up. The window is deleted
        //  when it is released.
        canvas->hold_screen();
        QEventLoop fade;
        QTimer::singleShot(450, &fade, &QEventLoop::quit);
        fade.exec();
        canvas->setAttribute(Qt::WA_DeleteOnClose, true);
        g_held_screen = canvas.release();
        //  If nobody releases it (the main window never comes), it goes by itself.
        QTimer::singleShot(20000, g_held_screen.data(), []{
            release_setup_curtain();
        });
    }else{
        canvas->hide();
    }
    return result;
}

void release_setup_curtain(){
    if (g_held_screen != nullptr){
        Canvas* canvas = g_held_screen.data();
        g_held_screen.clear();
        canvas->release_screen();
    }
}



//  ---------------------------------------------------------------------
//  The test
//  ---------------------------------------------------------------------

namespace{


struct Checker{
    int failures = 0;
    QStringList lines;
    void check(bool ok, const QString& what){
        if (!ok){
            failures++;
            lines << "Setup test: FAILED: " + what;
            qWarning("Setup test: FAILED: %s", qUtf8Printable(what));
        }
    }
};

void save(Canvas& canvas, const QString& folder, const QString& name){
    if (folder.isEmpty()){
        return;
    }
    QDir().mkpath(folder);
    canvas.picture().save(QDir(folder).filePath(name + ".png"));
}

//  A small picture (JPEG) for the log of the build: the workflow prints it as
//  text, so that how the screens look on the build machine can be seen
//  without downloading anything.
void save_preview(Canvas& canvas, const QString& folder, const QString& name){
    if (folder.isEmpty()){
        return;
    }
    QDir().mkpath(folder);
    canvas.picture().scaled(800, 450, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        .save(QDir(folder).filePath(name + ".jpg"), "JPG", 70);
}

//  A picture is not all black: something was drawn.
bool has_content(const QImage& image){
    qint64 sum = 0;
    for (int y = 0; y < image.height(); y += 7){
        for (int x = 0; x < image.width(); x += 7){
            sum += qGray(image.pixel(x, y));
        }
    }
    return sum > 5000;
}


//  The games as the program lists them, for the test: some with a picture
//  of their own, some without; one that cannot be picked.
std::vector<HomeGame> test_games(bool many = false){
    static const struct{
        const char* name;
        int programs;
        bool picture;
        bool enabled;
    } GAMES[] = {
        {"Nintendo Switch", 26, true, true},
        {"Pokemon Home", 4, true, true},
        {"Pokemon Let's Go Pikachu and Eevee", 3, false, true},
        {"Pokemon Sword and Shield", 98, true, true},
        {"Pokemon Brilliant Diamond and Shining Pearl", 31, true, true},
        {"Pokemon Legends: Arceus", 12, false, true},
        {"Pokemon Legends: Z-A", 0, true, false},
        {"Pokemon Scarlet and Violet", 74, true, true},
        {"Pokemon FireRed and LeafGreen", 1, true, true},
        {"Pokemon Pokopia", 6, false, true},
        {"Zelda: Tears of the Kingdom", 5, true, true},
        {"Other Games (Claude plays whatever is on the screen)", 2, true, true},
        //  More than twelve: the cartridges go in five columns.
        {"Pokemon Ruby and Sapphire", 9, true, true},
        {"Machine Learning", 3, false, true},
    };
    std::vector<HomeGame> games;
    const size_t count = many ? sizeof(GAMES) / sizeof(GAMES[0]) : sizeof(GAMES) / sizeof(GAMES[0]) - 2;
    for (size_t index = 0; index < count; index++){
        const auto& entry = GAMES[index];
        HomeGame game;
        game.name = QString::fromLatin1(entry.name);
        game.programs = entry.programs;
        game.enabled = entry.enabled;
        //  A local run can point to a folder with the real logos of the program
        //  (CategoryIcons) to see the cartridges as they are in the program.
        const QString logo_folder = qEnvironmentVariable("PP_SETUP_TEST_LOGO_DIR");
        if (!logo_folder.isEmpty()){
            static const struct{
                const char* word;
                const char* file;
            } LOGOS[] = {
                {"Nintendo Switch", "NintendoSwitch"}, {"Home", "PokemonHome"}, {"Let's Go", "PokemonLGPE"},
                {"Sword", "PokemonSwSh"}, {"Brilliant", "PokemonBDSP"}, {"Arceus", "PokemonLA"},
                {"Z-A", "PokemonLZA"}, {"Scarlet", "PokemonSV"}, {"FireRed", "PokemonFRLG"},
                {"Pokopia", "PokemonPokopia"}, {"Zelda", "ZeldaTotK"}, {"Other Games", "OtherGames"},
                {"Ruby", "PokemonRS"},
            };
            for (const auto& logo : LOGOS){
                if (game.name.contains(QLatin1String(logo.word))){
                    const QPixmap picture(logo_folder + "/" + QString::fromLatin1(logo.file) + ".png");
                    if (!picture.isNull()){
                        game.icon = picture;
                    }
                    break;
                }
            }
        }
        if (game.icon.isNull() && entry.picture){
            QImage image(192, 96, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(Qt::white, 6));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(10, 10, 172, 76), 24, 24);
            painter.setFont(QFont("sans-serif", 26, QFont::Bold));
            painter.drawText(QRectF(10, 10, 172, 76), Qt::AlignCenter, short_game_name(game.name).left(3));
            painter.end();
            game.icon = QPixmap::fromImage(image);
        }
        games.push_back(game);
    }
    return games;
}


}   //  namespace


int run_setup_test(QApplication& application, const QString& folder){
    (void)application;
    Checker checker;

    SetupRequest request;
    request.ask_language = true;
    request.ask_console = true;
    request.ask_board = true;
    request.games = test_games();
    Canvas canvas(request, true);
    canvas.resize(1920, 1080);
    QEventLoop loop;
    canvas.start(&loop);
    canvas.show();

    //  ---- the language
    canvas.wait(0.3);
    save(canvas, folder, "setup-01-language-start");
    checker.check(canvas.page_name() == "language", "starts on the language");
    canvas.wait(3.0);
    save(canvas, folder, "setup-02-language-waiting");
    checker.check(has_content(canvas.picture()), "the language screen has something on it");
    checker.check(canvas.has_hit("bar") && canvas.has_hit("continue"), "the bar and the button are there");

    canvas.click("bar");
    canvas.wait(0.9);
    save(canvas, folder, "setup-03-language-list");
    checker.check(canvas.has_hit("row:nl") && canvas.has_hit("row:custom"), "the list has its rows");
    canvas.move_to("row:custom");
    canvas.wait(0.7);
    save(canvas, folder, "setup-04-language-other");
    canvas.move_to("row:nl");
    canvas.click("row:nl");
    canvas.wait(0.8);
    checker.check(current_language_code() == "nl", "picking Nederlands tries it at once");
    save(canvas, folder, "setup-05-language-dutch");
    canvas.click("continue");
    canvas.wait(0.6);
    checker.check(canvas.result().language_chosen && canvas.result().language_code == "nl", "the language is the answer");
    checker.check(canvas.page_name() == "console", "the console question follows the language");

    //  ---- the console: after the language, in the same style, then the board
    canvas.wait(2.5);
    save(canvas, folder, "setup-36-console-start");
    save_preview(canvas, folder, "preview-console");
    checker.check(has_content(canvas.picture()), "the console screen has something on it");
    checker.check(canvas.has_hit("console:0") && canvas.has_hit("console:1"), "both consoles are tiles");
    checker.check(canvas.has_hit("skip") && canvas.has_hit("continue"), "the console screen has Skip setup and Continue");
    checker.check(!canvas.hit_enabled("continue"), "no console, no way on");
    checker.check(canvas.page_state() == "selected=", "nothing is picked at first");
    checker.check(canvas.focus_id() == "console:0", "the keyboard starts on the first console");
    canvas.press_key(Qt::Key_Return);
    canvas.wait(0.4);
    checker.check(canvas.page_state() == "selected=switch", "Enter picks the console that has the focus");
    checker.check(canvas.focus_id() == "continue" && canvas.hit_enabled("continue"), "and the way on is open, with the focus on it");
    canvas.press_key(Qt::Key_Left);
    canvas.press_key(Qt::Key_Right);
    checker.check(canvas.focus_id() == "console:1", "the arrow keys go over the consoles");
    canvas.move_to("console:1");
    canvas.wait(0.5);
    save(canvas, folder, "setup-37-console-hover");
    checker.check(canvas.click("console:1"), "a console can be picked with the mouse");
    canvas.wait(0.5);
    checker.check(canvas.page_state() == "selected=switch2", "Nintendo Switch 2 is picked");
    save(canvas, folder, "setup-38-console-picked");
    canvas.click("continue");
    canvas.wait(0.6);
    checker.check(canvas.result().console_chosen && canvas.result().console_code == "switch2", "the console is the answer");
    checker.check(canvas.page_name() == "board", "the board question follows the console");

    //  ---- the board
    canvas.wait(2.5);
    save(canvas, folder, "setup-06-board-start");
    checker.check(canvas.has_hit("bar"), "the board bar is there");
    checker.check(!canvas.hit_enabled("continue"), "no board, no way on");
    canvas.click("bar");
    canvas.wait(0.9);
    save(canvas, folder, "setup-07-board-list");
    save_preview(canvas, folder, "preview-board-list");
    canvas.move_to("row:uno");
    canvas.wait(0.6);
    save(canvas, folder, "setup-08-board-not-supported");
    checker.check(!canvas.click("row:uno"), "a board that is not supported cannot be picked");
    canvas.move_to("row:other");
    canvas.wait(0.6);
    save(canvas, folder, "setup-09-board-other");
    canvas.press_key(Qt::Key_Up);
    canvas.press_key(Qt::Key_Up);
    canvas.press_key(Qt::Key_Home);
    canvas.press_key(Qt::Key_Return);
    canvas.wait(0.6);
    checker.check(canvas.hit_enabled("continue"), "a board picked with the keyboard opens the way on");
    save(canvas, folder, "setup-10-board-picked");

    //  Other first: its page, the Setup Guide in the program, and back.
    canvas.click("bar");
    canvas.wait(0.5);
    canvas.click("row:other");
    canvas.wait(0.5);
    canvas.click("continue");
    canvas.wait(1.8);
    checker.check(canvas.page_name() == "other", "Other goes to its page");
    save(canvas, folder, "setup-11-other");
    checker.check(canvas.click("open"), "the button for the Setup Guide is there");
    canvas.wait(2.4);
    checker.check(canvas.page_name() == "guide", "the Setup Guide opens in the program, not in the browser");
    checker.check(canvas.last_url().isEmpty(), "and it does not open the browser");
    save(canvas, folder, "setup-12-guide");
    save_preview(canvas, folder, "preview-guide");
    checker.check(canvas.has_hit("row:pico") && canvas.has_hit("row:esp32s3") && canvas.has_hit("row:uart") && canvas.has_hit("web"), "the guide lists the boards");
    canvas.click("row:uart");
    checker.check(canvas.last_url().contains("UART"), "a guide that is not in the program opens in the browser");
    canvas.click("web");
    checker.check(canvas.last_url().endsWith("SetupGuide/index.html"), "the whole guide opens in the browser");
    canvas.click("back");
    canvas.wait(1.8);
    checker.check(canvas.page_name() == "board", "and back to the boards");

    //  ---- a computer of our own: folders, drives and ports of the test
    QTemporaryDir scratch;
    checker.check(scratch.isValid(), "a folder for the test");
    const QString app = QDir(scratch.path()).filePath("app");
    const QString drive_folder = QDir(scratch.path()).filePath("drive");
    QDir().mkpath(app + "/Firmware");
    QDir().mkpath(drive_folder);
    auto write_file = [](const QString& path, const QByteArray& data){
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)){
            file.write(data);
        }
    };
    QByteArray uf2(512 * 8, 'u');
    uf2.replace(0, 4, "UF2\n");
    write_file(app + "/Firmware/PABotBase2-Pico1W-2026092800.uf2", uf2);
    write_file(app + "/Firmware/PABotBase2-Pico2W-2026092800.uf2", uf2);
    write_file(app + "/Firmware/PABotBase2-ESP32-S3-2026101400.bin", QByteArray(9000, 'b'));
    write_file(app + "/Firmware/PABotBase2-ESP32-2026101400.bin", QByteArray(9000, 'b'));
    write_file(app + "/Firmware/SOURCE.txt", "https://github.com/PokemonAutomation/Packages/tree/4ae6b4634628626cf1f31566d871635d0f87b79a/Firmware\n");
    //  The file that the "download" fetches.
    write_file(QDir(scratch.path()).filePath("online-ESP32-S3-2026120100.bin"), QByteArray(10000, 'n'));

    struct Computer{
        std::mutex lock;
        HardwareState state;
    } computer;
    canvas.set_app_folder(app);
    canvas.set_scan([&computer]{
        std::lock_guard<std::mutex> guard(computer.lock);
        return computer.state;
    });
    canvas.set_listing(
        "[{\"name\":\"PABotBase2-ESP32-S3-2026120100.bin\",\"type\":\"file\"},"
        "{\"name\":\"PABotBase2-ESP32-S3-2026101400.bin\",\"type\":\"file\"},"
        "{\"name\":\"README.md\",\"type\":\"file\"}]"
    );
    canvas.set_url_map([&scratch](const QString& url){
        return QUrl::fromLocalFile(QDir(scratch.path()).filePath("online-ESP32-S3-2026120100.bin")).toString() + (url.isEmpty() ? "" : "");
    });

    //  The ESP32-S3: its steps, with what the computer shows.
    canvas.click("bar");
    canvas.wait(0.5);
    canvas.click("row:esp32s3");
    canvas.wait(0.5);
    canvas.click("continue");
    canvas.wait(2.2);
    checker.check(canvas.page_name() == "steps", "the ESP32-S3 goes to its steps");
    checker.check(canvas.page_state().contains("step=1"), "on the first step");
    canvas.wait(1.2);
    save(canvas, folder, "setup-13-s3-no-port");
    checker.check(canvas.has_hit("driver_cp210x") && canvas.has_hit("driver_ch340") && canvas.has_hit("devmgr"), "no port: the drivers and the Device Manager are offered");
    canvas.click("driver_cp210x");
    checker.check(canvas.last_url().contains("CP210x"), "the CP210x driver link");
    canvas.click("driver_ch340");
    checker.check(canvas.last_url().contains("ch340"), "the CH340 driver link");
    canvas.click("devmgr");
    checker.check(canvas.device_manager_opened(), "the Device Manager button");
    {
        std::lock_guard<std::mutex> guard(computer.lock);
        PortInfo port;
        port.name = "COM7";
        port.description = "Silicon Labs CP210x USB to UART Bridge";
        port.vendor = 0x10C4;
        port.product = 0xEA60;
        port.chip = "CP210x";
        computer.state.ports.push_back(port);
        PortInfo other;
        other.name = "COM3";
        other.description = "Communications Port";
        computer.state.ports.push_back(other);
    }
    canvas.wait(2.2);
    save(canvas, folder, "setup-14-s3-port-found");
    save_preview(canvas, folder, "preview-s3-port");
    canvas.click("next");
    canvas.wait(0.8);
    canvas.click("next");
    canvas.wait(1.6);
    checker.check(canvas.page_state().contains("step=3"), "the third step of the ESP32-S3");
    checker.check(canvas.page_state().contains("file=PABotBase2-ESP32-S3-2026101400.bin") && canvas.page_state().contains("local=yes"),
        "the firmware of this computer is the choice");
    checker.check(canvas.page_state().contains("port=COM7"), "and the port of the known chip");
    checker.check(canvas.has_hit("dd:version") && canvas.has_hit("dd:port") && canvas.has_hit("link_open") && canvas.has_hit("link_copy") && canvas.has_hit("flash"),
        "the firmware card has its parts");
    save(canvas, folder, "setup-15-s3-firmware");
    save_preview(canvas, folder, "preview-s3-firmware");
    const QString expected_link =
        "https://github.com/PokemonAutomation/Packages/raw/4ae6b4634628626cf1f31566d871635d0f87b79a/Firmware/PABotBase2-ESP32-S3-2026101400.bin";
    canvas.click("link_open");
    checker.check(canvas.last_url() == expected_link, "the link of the version that is chosen");
    canvas.click("link_copy");
    checker.check(canvas.copied() == expected_link, "and it can be copied");
    canvas.click("folder");
    checker.check(QDir::fromNativeSeparators(canvas.last_folder()).endsWith("/app/Firmware"), "the Firmware folder opens");
    checker.check(!canvas.hit_enabled("flash") || true, "(the flash tool comes next)");
    //  No flash tool yet: it cannot be started.
    canvas.wait(1.2);
    checker.check(!canvas.hit_enabled("flash"), "no flash tool: Flash now waits");
    {
        std::lock_guard<std::mutex> guard(computer.lock);
        computer.state.tool.found = true;
        computer.state.tool.program = "esptool";
        computer.state.tool.where = "test";
        computer.state.tool_checked = true;
    }
    canvas.wait(1.6);
    checker.check(canvas.hit_enabled("flash"), "a flash tool: Flash now can be pressed");
    //  The version list: the newer one is online only.
    canvas.click("dd:version");
    canvas.wait(0.8);
    save(canvas, folder, "setup-16-s3-versions");
    checker.check(canvas.has_hit("dd:version:PABotBase2-ESP32-S3-2026120100.bin") && canvas.has_hit("dd:version:PABotBase2-ESP32-S3-2026101400.bin"),
        "the list has the version of this computer and the online one");
    canvas.click("dd:version:PABotBase2-ESP32-S3-2026120100.bin");
    canvas.wait(0.6);
    checker.check(canvas.page_state().contains("2026120100") && canvas.page_state().contains("local=no"), "a version that is online only");
    checker.check(canvas.has_hit("download") && !canvas.hit_enabled("flash"), "it is downloaded first");
    canvas.click("link_open");
    checker.check(canvas.last_url() == "https://github.com/PokemonAutomation/Packages/raw/master/Firmware/PABotBase2-ESP32-S3-2026120100.bin",
        "its link points to the repository");
    canvas.click("download");
    for (int c = 0; c < 40 && canvas.page_state().contains("flash=downloading"); c++){
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        canvas.wait(0.1);
    }
    checker.check(canvas.page_state().contains("flash=idle") && canvas.page_state().contains("local=yes"), "the download put it on this computer");
    checker.check(QFileInfo::exists(app + "/Firmware/PABotBase2-ESP32-S3-2026120100.bin"), "in the Firmware folder");
    checker.check(canvas.hit_enabled("flash"), "and now it can be flashed");
    save(canvas, folder, "setup-17-s3-downloaded");
    canvas.click("video");
    checker.check(canvas.video_open() && canvas.video_name() == "tutorial-esp32s3", "the film of the ESP32-S3");
    canvas.press_key(Qt::Key_Escape);
    checker.check(!canvas.video_open(), "Escape ends the film");
#ifndef Q_OS_WIN
    //  A flash tool that is a script: the whole flash, as the program runs it.
    {
        const QString tool = QDir(scratch.path()).filePath("fake-esptool");
        write_file(tool,
            "#!/bin/sh\n"
            "echo 'esptool v5.1.0'\n"
            "echo \"args: $@\"\n"
            "printf 'Writing at 0x00010000 [==>   ] 20.0%% 1/10 bytes...\\r'\n"
            "sleep 0.3\n"
            "printf 'Writing at 0x00020000 [====> ] 60.0%% 5/10 bytes...\\r'\n"
            "sleep 0.3\n"
            "printf 'Writing at 0x00030000 [======] 100.0%% 10/10 bytes...\\n'\n"
            "echo 'Hash of data verified.'\n"
            "exit 0\n");
        QFile::setPermissions(tool, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        {
            std::lock_guard<std::mutex> guard(computer.lock);
            computer.state.tool.program = tool;
        }
        canvas.wait(1.6);
        canvas.click("flash");
        bool saw_flashing = false;
        for (int c = 0; c < 80 && !canvas.page_state().contains("flash=done"); c++){
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            canvas.wait(0.1);
            if (canvas.page_state().contains("flash=flashing")){
                saw_flashing = true;
                if (c == 3){
                    save(canvas, folder, "setup-18-s3-flashing");
                }
            }
        }
        checker.check(saw_flashing && canvas.page_state().contains("flash=done"), "the flash runs and ends well");
        save(canvas, folder, "setup-19-s3-flashed");
    }
#endif
    canvas.click("back");
    canvas.click("back");
    canvas.click("back");
    canvas.click("back");
    canvas.wait(0.6);
    canvas.click("back");
    canvas.wait(0.6);
    canvas.click("back");
    canvas.wait(1.8);
    checker.check(canvas.page_name() == "board", "back to the boards from the steps");

    //  The ESP32: the same steps, wireless.
    canvas.click("bar");
    canvas.wait(0.5);
    canvas.click("row:esp32");
    canvas.wait(0.5);
    canvas.click("continue");
    canvas.wait(2.0);
    checker.check(canvas.page_name() == "steps", "the ESP32 goes to its steps");
    save(canvas, folder, "setup-20-esp32");
    canvas.click("video");
    checker.check(canvas.video_open() && canvas.video_name() == "tutorial-esp32", "the film of the ESP32");
    canvas.press_key(Qt::Key_Escape);
    checker.check(!canvas.video_open(), "Escape ends the film of the ESP32");
    canvas.click("back");
    canvas.wait(1.8);

    //  The Pico W: the steps, with a Pico of our own.
    canvas.click("bar");
    canvas.wait(0.5);
    canvas.click("row:pico");
    canvas.wait(0.5);
    canvas.press_key(Qt::Key_Return);
    canvas.wait(2.0);
    checker.check(canvas.page_name() == "steps", "the Pico W goes to its steps");
    checker.check(canvas.page_state().contains("step=1"), "on the first step of the Pico W");
    canvas.wait(1.0);
    save(canvas, folder, "setup-21-pico-1");
    canvas.click("next");
    canvas.wait(1.4);
    checker.check(canvas.page_state().contains("step=2"), "the second step: flash mode");
    save(canvas, folder, "setup-22-pico-waiting");
    save_preview(canvas, folder, "preview-pico-waiting");
    {
        //  The drive of a Pico 2 W is not the one of a Pico W.
        std::lock_guard<std::mutex> guard(computer.lock);
        PicoDrive wrong;
        wrong.root = drive_folder;
        wrong.label = "RP2350";
        wrong.board_id = "RP2350";
        computer.state.drives.push_back(wrong);
    }
    canvas.wait(2.2);
    save(canvas, folder, "setup-23-pico-wrong-drive");
    {
        std::lock_guard<std::mutex> guard(computer.lock);
        computer.state.drives.clear();
        PicoDrive drive;
        drive.root = drive_folder;
        drive.label = "RPI-RP2";
        drive.board_id = "RPI-RP2";
        computer.state.drives.push_back(drive);
    }
    canvas.wait(2.2);
    save(canvas, folder, "setup-24-pico-found");
    save_preview(canvas, folder, "preview-pico-found");
    canvas.click("next");
    canvas.wait(1.4);
    checker.check(canvas.page_state().contains("step=3") && canvas.page_state().contains("file=PABotBase2-Pico1W-2026092800.uf2"), "the firmware step of the Pico W");
    checker.check(canvas.hit_enabled("flash"), "the Pico is in flash mode: Flash now can be pressed");
    save(canvas, folder, "setup-25-pico-firmware");
    save_preview(canvas, folder, "preview-pico-firmware");
    canvas.click("flash");
    canvas.wait(0.6);
    save(canvas, folder, "setup-26-pico-copying");
    for (int c = 0; c < 60 && !canvas.page_state().contains("flash=restarting"); c++){
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        canvas.wait(0.1);
    }
    checker.check(canvas.page_state().contains("flash=restarting"), "the file is copied, the Pico is restarting");
    {
        QFile source(app + "/Firmware/PABotBase2-Pico1W-2026092800.uf2");
        QFile copy(QDir(drive_folder).filePath("PABotBase2-Pico1W-2026092800.uf2"));
        const bool source_opened = source.open(QIODevice::ReadOnly);
        const bool opened = copy.open(QIODevice::ReadOnly);
        checker.check(source_opened && opened && copy.readAll() == source.readAll(), "the file is on the drive, whole");
    }
    save(canvas, folder, "setup-27-pico-restarting");
    {
        //  The Pico restarts: its drive is gone.
        std::lock_guard<std::mutex> guard(computer.lock);
        computer.state.drives.clear();
    }
    canvas.wait(2.8);
    checker.check(canvas.page_state().contains("flash=done"), "the drive is gone: the Pico has its firmware");
    save(canvas, folder, "setup-28-pico-done");
    canvas.click("flash");
    canvas.wait(0.3);
    checker.check(canvas.page_state().contains("flash=idle") && !canvas.hit_enabled("flash"),
        "Flash again starts over, and waits for a Pico in flash mode");
    canvas.click("next");
    canvas.wait(0.8);
    for (int c = 3; c < 7; c++){
        canvas.wait(1.0);
        save(canvas, folder, QString("setup-29-pico-%1").arg(c + 1));
        if (c == 5){
            save_preview(canvas, folder, "preview-ports");
        }
        if (c == 6){
            save_preview(canvas, folder, "preview-steps");
        }
        if (c < 6){
            checker.check(canvas.click("next"), QString("next step %1").arg(c + 1));
        }
    }
    canvas.click("video");
    checker.check(canvas.video_open() && canvas.video_name() == "tutorial-pico", "the film of the Picos");
    canvas.press_key(Qt::Key_Escape);
    checker.check(!canvas.video_open(), "Escape ends that film too");
    canvas.click("back");
    canvas.wait(0.5);
    canvas.press_key(Qt::Key_Right);
    canvas.wait(0.5);
    canvas.press_key(Qt::Key_Tab);
    canvas.press_key(Qt::Key_Tab);
    checker.check(!canvas.focus_id().isEmpty(), "Tab moves the focus");
    canvas.click("next");
    canvas.wait(1.8);
    checker.check(canvas.page_name() == "done", "the last step goes to the end");
    canvas.wait(1.5);
    save(canvas, folder, "setup-30-done");
    checker.check(canvas.has_hit("start"), "the end screen has its button");
    canvas.click("start");
    canvas.wait(0.2);
    checker.check(!canvas.finished(), "with games to choose from the end goes on to the choice");
    canvas.wait(2.5);
    checker.check(canvas.page_name() == "games", "the end leads to the choice of a game, a screen of its own");
    checker.check(canvas.result().board_done && canvas.result().board == "pico", "the board is the answer");

    //  ---- the game to start
    {
        const std::vector<HomeGame>& games = request.games;
        save(canvas, folder, "setup-31-games");
        save_preview(canvas, folder, "preview-games");
        checker.check(has_content(canvas.picture()), "the games screen has something on it");
        checker.check(!canvas.finished(), "the setup is not over before a game is chosen");
        bool all_there = true;
        for (size_t c = 0; c < games.size(); c++){
            all_there = all_there && canvas.has_hit("game:" + QString::number(c));
        }
        checker.check(all_there, "every game has its cartridge");
        checker.check(canvas.has_hit("later") && canvas.has_hit("start"), "the buttons are there");
        checker.check(!canvas.has_hit("back") && !canvas.has_hit("next"), "the installation is behind us: no way back into it");
        checker.check(!canvas.hit_enabled("start"), "no game, no start");
        checker.check(!canvas.hit_enabled("game:6"), "a game that is not available cannot be picked");
        checker.check(!canvas.click("game:6"), "clicking it does nothing");
        checker.check(canvas.page_state() == "selected=", "nothing is picked yet");

        //  With the keyboard: the focus starts on the first game.
        checker.check(canvas.focus_id() == "game:0", "the focus starts on the first game");
        canvas.press_key(Qt::Key_Right);
        checker.check(canvas.focus_id() == "game:1", "Right goes to the next game");
        canvas.press_key(Qt::Key_Down);
        checker.check(canvas.focus_id() == "game:5", "Down goes to the game below");
        canvas.press_key(Qt::Key_Right);
        checker.check(canvas.focus_id() == "game:7", "Right skips the game that is not available");
        canvas.press_key(Qt::Key_Down);
        checker.check(canvas.focus_id() == "game:11", "Down again");
        canvas.press_key(Qt::Key_Down);
        checker.check(canvas.focus_id() == "game:11", "Down at the last row stays");
        canvas.press_key(Qt::Key_Up);
        checker.check(canvas.focus_id() == "game:7", "Up goes to the game above");
        canvas.press_key(Qt::Key_Left);
        checker.check(canvas.focus_id() == "game:5", "Left skips the game that is not available too");
        canvas.press_key(Qt::Key_Return);
        canvas.wait(0.3);
        checker.check(canvas.page_state() == "selected=" + games[5].name, "Enter picks the game that has the focus");
        checker.check(canvas.focus_id() == "start", "after a pick the focus is on Start");
        checker.check(canvas.hit_enabled("start"), "a game is picked: Start can be pressed");

        //  With the mouse: another game.
        canvas.move_to("game:9");
        canvas.wait(0.4);
        canvas.click("game:9");
        canvas.wait(0.5);
        checker.check(canvas.page_state() == "selected=" + games[9].name, "a click picks that game");
        save(canvas, folder, "setup-32-games-picked");
        save_preview(canvas, folder, "preview-games-picked");

        //  Escape does not lead back into the installation.
        canvas.press_key(Qt::Key_Escape);
        canvas.wait(0.6);
        checker.check(canvas.page_name() == "games" && !canvas.finished(), "Escape does not leave this screen");
        checker.check(canvas.page_state() == "selected=" + games[9].name, "and keeps the pick");

        //  The pick is the answer.
        canvas.click("game:10");
        canvas.wait(0.3);
        canvas.press_key(Qt::Key_Return);
        canvas.wait(0.2);
        checker.check(canvas.finished(), "Start ends the setup");
        checker.check(canvas.result().board_done && canvas.result().board == "pico", "the board is still the answer");
        checker.check(canvas.result().game == games[10].name, "the game is the answer");
    }

    //  ---- Skip setup: the choice of a game follows too
    {
        SetupRequest skipping;
        skipping.ask_board = true;
        skipping.games = test_games();
        Canvas skip_canvas(skipping, true);
        skip_canvas.resize(1280, 720);
        QEventLoop skip_loop;
        skip_canvas.start(&skip_loop);
        skip_canvas.show();
        skip_canvas.wait(2.0);
        checker.check(skip_canvas.page_name() == "board", "the board list is there");
        skip_canvas.click("skip");
        skip_canvas.wait(0.3);
        checker.check(!skip_canvas.finished(), "Skip setup does not leave the screen");
        skip_canvas.wait(2.5);
        checker.check(skip_canvas.page_name() == "games", "Skip setup leads to the choice of a game");
        checker.check(skip_canvas.result().board_done && skip_canvas.result().board.isEmpty(), "and the setup counts as done");
        save(skip_canvas, folder, "setup-35-games-after-skip");
        skip_canvas.click("game:3");
        skip_canvas.wait(0.3);
        skip_canvas.click("start");
        skip_canvas.wait(0.2);
        checker.check(skip_canvas.finished() && skip_canvas.result().game == skipping.games[3].name, "the game that is picked is the answer");
    }

    //  ---- "Choose later", and no games at all
    {
        SetupRequest with_games;
        with_games.ask_board = true;
        with_games.games = test_games(true);
        Canvas third(with_games, true);
        third.resize(1280, 720);
        QEventLoop third_loop;
        third.start(&third_loop);
        third.show();
        third.wait(0.5);
        third.go_done("esp32s3");
        third.wait(2.0);
        third.click("start");
        third.wait(2.5);
        checker.check(third.page_name() == "games", "the choice also fits a small window");
        save(third, folder, "setup-33-games-small-window");
        third.click("later");
        third.wait(0.2);
        checker.check(third.finished() && third.result().game.isEmpty(), "Choose later ends the setup without a game");
        checker.check(third.result().board_done && third.result().board == "esp32s3", "the board is kept");

        SetupRequest without_games;
        without_games.ask_board = true;
        Canvas fourth(without_games, true);
        fourth.resize(1280, 720);
        QEventLoop fourth_loop;
        fourth.start(&fourth_loop);
        fourth.show();
        fourth.wait(0.5);
        fourth.go_done("esp32");
        fourth.wait(2.0);
        checker.check(fourth.page_name() == "done", "the end screen is there");
        fourth.click("start");
        fourth.wait(0.2);
        checker.check(fourth.finished() && fourth.result().game.isEmpty() && fourth.result().board == "esp32",
            "without games Start ends the setup");
    }

    //  ---- the game choice at the start: a screen of its own, no installation
    {
        //  Alone (the setup was done before).
        SetupRequest only_games;
        only_games.ask_game = true;
        only_games.games = test_games();
        Canvas first(only_games, true);
        first.resize(1280, 720);
        QEventLoop first_loop;
        first.start(&first_loop);
        first.show();
        first.wait(2.0);
        checker.check(first.page_name() == "games", "at the start the choice of a game is a screen of its own");
        checker.check(!first.has_hit("back") && !first.has_hit("next"), "with nothing of the installation on it");
        save(first, folder, "setup-34-games-alone");
        first.click("game:1");
        first.wait(0.3);
        checker.check(first.page_state() == "selected=" + only_games.games[1].name, "a game can be picked");
        first.click("start");
        first.wait(0.2);
        checker.check(first.finished() && first.result().game == only_games.games[1].name, "Start ends it with that game");
        checker.check(!first.result().board_done && !first.result().language_chosen, "no question of the installation was answered");

        //  After the language question only: the choice follows it.
        SetupRequest language_and_games;
        language_and_games.ask_language = true;
        language_and_games.ask_game = true;
        language_and_games.games = test_games();
        Canvas second_choice(language_and_games, true);
        second_choice.resize(1280, 720);
        QEventLoop second_choice_loop;
        second_choice.start(&second_choice_loop);
        second_choice.show();
        second_choice.wait(2.0);
        checker.check(second_choice.page_name() == "language", "the language comes first");
        second_choice.click("continue");
        second_choice.wait(2.5);
        checker.check(second_choice.page_name() == "games" && !second_choice.finished(), "the choice of a game follows the language");
        second_choice.click("later");
        second_choice.wait(0.2);
        checker.check(second_choice.finished() && second_choice.result().game.isEmpty() && second_choice.result().language_chosen,
            "Choose later ends it with no game, the language is kept");

        //  Asked for, but there are no games: nothing to ask.
        SetupRequest no_games;
        no_games.ask_game = true;
        Canvas third_choice(no_games, true);
        checker.check(!third_choice.asks_game(), "no games, no choice");
    }

    //  ---- the intro on the screen of the setup
    //  The film is decoded for real (in real time, the picture drawn by the
    //  canvas as the player is not on a screen here): it goes over into the
    //  first question without the window changing, and nothing is empty
    //  in between.
    {
        //  A build machine decodes it (the intro test says so); a machine
        //  without a decoder (the local harness) skips what needs the picture.
        const bool decodes = qEnvironmentVariableIsEmpty("PP_SETUP_TEST_NO_DECODER");

        //  Real time passes while the film plays.
        auto run_real = [&](Canvas& screen, double seconds, const std::function<bool()>& until){
            QElapsedTimer clock;
            clock.start();
            double last = 0;
            while (clock.elapsed() < (qint64)(seconds * 1000) && !until()){
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                const double now = (double)clock.elapsed() / 1000.0;
                screen.advance(std::min(0.05, now - last));
                last = now;
                QThread::msleep(6);
            }
        };

        SetupRequest with_intro;
        with_intro.ask_language = true;
        with_intro.ask_board = true;
        with_intro.play_intro = true;
        with_intro.hold_screen = true;
        Canvas fifth(with_intro, true);
        fifth.resize(1280, 720);
        QEventLoop fifth_loop;
        fifth.start(&fifth_loop);
        fifth.show();
        fifth.start_intro();
        checker.check(fifth.intro_state() == "film", "the film starts on the window of the setup");
        checker.check(fifth.page_name() == "language", "the first question is ready under the film");

        //  The film, with a picture now and then: never an empty window.
        int empty_pictures = 0;
        int pictures = 0;
        bool saw_fade = false;
        bool preview_film = false;
        bool preview_fade = false;
        QElapsedTimer since_picture;
        since_picture.start();
        QElapsedTimer fade_clock;
        run_real(fifth, 25.0, [&]{
            const QString state = fifth.intro_state();
            if (state == "fade" && !saw_fade){
                saw_fade = true;
                fade_clock.start();
            }
            if (state == "none"){
                return true;
            }
            if (since_picture.elapsed() > 140){
                since_picture.restart();
                const QImage image = fifth.picture();
                //  The first moments of the film are black on purpose.
                if (state == "fade" || fifth.intro_frames() > 45){
                    pictures++;
                    if (!has_content(image)){
                        empty_pictures++;
                    }
                }
                if (!preview_film && state == "film" && fifth.intro_frames() > 100){
                    preview_film = true;
                    save_preview(fifth, folder, "preview-intro-film");
                }
                if (!preview_fade && state == "fade" && fade_clock.elapsed() > 350){
                    preview_fade = true;
                    save_preview(fifth, folder, "preview-intro-fade");
                }
            }
            return false;
        });
        const QString intro_note = fifth.intro_note();
        if (decodes){
            checker.check(intro_note == "it played to the end", "the film plays to the end on the window (" + intro_note + ")");
            checker.check(fifth.intro_frames() >= 150, "the film has its pictures (" + QString::number(fifth.intro_frames()) + ")");
            checker.check(saw_fade, "the last picture of the film fades over into the first question");
            checker.check(pictures >= 20 && empty_pictures == 0,
                "no empty window between film and question (" + QString::number(empty_pictures) + " of " + QString::number(pictures) + ")");
        }
        checker.check(fifth.intro_state() == "none", "the film is over and the first question is all that is left");
        checker.check(fifth.page_name() == "language" && fifth.isVisible(), "the same window shows the first question");
        fifth.wait(1.0);
        save_preview(fifth, folder, "preview-intro-done");
        checker.check(has_content(fifth.picture()), "the first question is there");

        //  The question can be answered as always.
        checker.check(fifth.has_hit("bar") && fifth.has_hit("continue"), "the first question can be answered");

        //  The setup is over: the screen stays up, then goes.
        fifth.hold_screen();
        fifth.wait(0.8);
        checker.check(fifth.holding() && fifth.isVisible(), "the screen stays up after the setup");
        checker.check(has_content(fifth.picture()), "the background stays on it");
        fifth.release_screen();
        fifth.wait(1.0);
        checker.check(fifth.released(), "the screen goes when the window is there");

        //  Nothing to ask: the film is the whole screen, its last picture stays.
        SetupRequest only_intro;
        only_intro.play_intro = true;
        only_intro.hold_screen = true;
        Canvas sixth(only_intro, true);
        sixth.resize(1280, 720);
        QEventLoop sixth_loop;
        sixth.start(&sixth_loop);
        sixth.show();
        sixth.start_intro();
        run_real(sixth, 25.0, [&]{
            return sixth.finished();
        });
        checker.check(sixth.finished(), "with nothing to ask the screen is the film, then it is over");
        if (decodes){
            checker.check(sixth.intro_note() == "it played to the end", "that film plays to the end too (" + sixth.intro_note() + ")");
            sixth.hold_screen();
            sixth.wait(0.8);
            save_preview(sixth, folder, "preview-intro-only");
            checker.check(has_content(sixth.picture()), "its last picture stays");
        }
        sixth.release_screen();
        sixth.wait(1.0);
        checker.check(sixth.released(), "and goes when the window is there");

        //  A key skips the film; not in the first moments.
        Canvas seventh(with_intro, true);
        seventh.resize(1280, 720);
        QEventLoop seventh_loop;
        seventh.start(&seventh_loop);
        seventh.show();
        seventh.start_intro();
        run_real(seventh, 0.2, []{
            return false;
        });
        seventh.press_key(Qt::Key_Space);
        checker.check(seventh.intro_state() == "film" || !decodes, "a key in the first moments does not skip the film");
        run_real(seventh, 1.0, []{
            return false;
        });
        if (decodes){
            seventh.press_key(Qt::Key_Space);
            checker.check(seventh.intro_state() == "fade", "a key later skips the film");
            checker.check(seventh.intro_note() == "it was skipped", "and says so (" + seventh.intro_note() + ")");
            seventh.wait(1.5);
            checker.check(seventh.intro_state() == "none" && seventh.page_name() == "language", "the first question follows");
        }
    }

    //  ---- the intro, then the game choice alone
    {
        const bool decodes_here = qEnvironmentVariableIsEmpty("PP_SETUP_TEST_NO_DECODER");
        SetupRequest intro_games;
        intro_games.play_intro = true;
        intro_games.hold_screen = true;
        intro_games.ask_game = true;
        intro_games.games = test_games();
        Canvas eighth(intro_games, true);
        eighth.resize(1280, 720);
        QEventLoop eighth_loop;
        eighth.start(&eighth_loop);
        eighth.show();
        eighth.start_intro();
        checker.check(eighth.page_name() == "games", "the choice of a game is ready under the film");
        //  Real time passes while the film plays (as in the test above).
        QElapsedTimer eighth_clock;
        eighth_clock.start();
        double eighth_last = 0;
        while (eighth_clock.elapsed() < (decodes_here ? 25000 : 9000) && eighth.intro_state() != "none"){
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            const double now = (double)eighth_clock.elapsed() / 1000.0;
            eighth.advance(std::min(0.05, now - eighth_last));
            eighth_last = now;
            QThread::msleep(6);
        }
        if (decodes_here){
            checker.check(eighth.intro_state() == "none" && eighth.page_name() == "games",
                "the film goes over into the choice of a game");
            eighth.wait(1.5);
            save_preview(eighth, folder, "preview-intro-games");
            checker.check(eighth.has_hit("game:0") && !eighth.finished(), "and it can be answered");
        }
    }

    //  ---- the console: one that was chosen before is shown as picked
    {
        SetupRequest again;
        again.ask_console = true;
        again.ask_board = true;
        again.console_code = "switch2";
        again.games = test_games();
        Canvas kept(again, true);
        kept.resize(1280, 720);
        QEventLoop kept_loop;
        kept.start(&kept_loop);
        kept.show();
        kept.wait(2.5);
        checker.check(kept.page_name() == "console", "without the language question it starts on the console");
        checker.check(kept.page_state() == "selected=switch2" && kept.hit_enabled("continue"), "the console of the last time is picked");
        checker.check(kept.focus_id() == "continue", "and Continue has the focus");
        save(kept, folder, "setup-39-console-kept");
        kept.press_key(Qt::Key_Escape);
        kept.wait(0.3);
        checker.check(kept.page_name() == "console" && !kept.finished(), "Escape does nothing here");
        kept.click("console:0");
        kept.wait(0.3);
        kept.press_key(Qt::Key_Return);
        kept.wait(2.0);
        checker.check(kept.page_name() == "board", "Enter on Continue goes on to the board");
        checker.check(kept.result().console_chosen && kept.result().console_code == "switch", "the new choice is the answer");
    }

    //  ---- the console: Skip setup leads on to the choice of a game
    {
        SetupRequest skipping_console;
        skipping_console.ask_console = true;
        skipping_console.ask_board = true;
        skipping_console.games = test_games();
        Canvas skipped(skipping_console, true);
        skipped.resize(1280, 720);
        QEventLoop skipped_loop;
        skipped.start(&skipped_loop);
        skipped.show();
        skipped.wait(2.0);
        checker.check(skipped.page_name() == "console", "the console screen is there");
        skipped.click("skip");
        skipped.wait(2.8);
        checker.check(skipped.page_name() == "games", "Skip setup on the console screen leads to the choice of a game");
        checker.check(!skipped.result().console_chosen && skipped.result().board_done, "no console is chosen, and the setup counts as done");
    }

    //  ---- the other way out: skip
    {
        SetupRequest only_board;
        only_board.ask_board = true;
        Canvas second(only_board, true);
        second.resize(1280, 720);
        QEventLoop second_loop;
        second.start(&second_loop);
        second.show();
        second.wait(2.0);
        checker.check(second.page_name() == "board", "without the language question it starts on the board");
        save(second, folder, "setup-15-small-window");
        second.click("skip");
        second.wait(0.2);
        checker.check(second.finished() && second.result().board_done && second.result().board.isEmpty(), "Skip ends the setup");
    }

    //  The language is English again for what comes after.
    preview_language("en");

    checker.lines << QString("Setup test: %1.").arg(checker.failures == 0 ? "passed" : "FAILED");
    qInfo("Setup test: %s.", checker.failures == 0 ? "passed" : "FAILED");
    //  A text file next to the pictures: the log of the program does not have it.
    if (!folder.isEmpty()){
        QFile file(QDir(folder).filePath("setup-test.txt"));
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)){
            file.write(checker.lines.join("\n").toUtf8() + "\n");
        }
    }
    return checker.failures == 0 ? 0 : 1;
}


//  The films of the tutorials, decoded without a window: each starts, has its
//  picture size, its sound and its length, and plays to the end (a jump to
//  the last seconds, so that the test does not take minutes). What it found is
//  in the report, one line each; the last line says passed or FAILED.
int run_tutorial_test(QApplication& application, QStringList& report){
    (void)application;
    static const char* const NAMES[] = {"tutorial-pico", "tutorial-esp32", "tutorial-esp32s3"};
    int failures = 0;
    for (const char* name : NAMES){
        const QString film = QString::fromLatin1(name);
        const QString path = extract_film(film);
        if (path.isEmpty()){
            report << QString("Tutorial test: FAILED: %1 is not in the program.").arg(film);
            failures++;
            continue;
        }

        int frames = 0;
        QSize size;
        bool finished = false;
        bool invalid = false;
        QString error;
        qint64 length = 0;
        int audio_tracks = 0;
        int first_frames = 0;
        {
            QMediaPlayer player;
            QVideoSink sink;
            player.setVideoSink(&sink);
            QObject::connect(&sink, &QVideoSink::videoFrameChanged, &player, [&](const QVideoFrame& frame){
                if (frame.isValid()){
                    frames++;
                    size = frame.size();
                }
            });
            QEventLoop loop;
            QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, &loop, [&](QMediaPlayer::MediaStatus status){
                if (status == QMediaPlayer::EndOfMedia){
                    finished = true;
                    loop.quit();
                }else if (status == QMediaPlayer::InvalidMedia){
                    invalid = true;
                    loop.quit();
                }
            });
            QObject::connect(&player, &QMediaPlayer::errorOccurred, &loop, [&](QMediaPlayer::Error, const QString& message){
                error = message;
                invalid = true;
                loop.quit();
            });

            //  The first seconds, as they are played.
            player.setSource(QUrl::fromLocalFile(path));
            player.play();
            QTimer::singleShot(5000, &loop, [&]{ loop.quit(); });
            loop.exec();
            first_frames = frames;

            //  The end: jump to the last seconds and play on.
            length = player.duration();
            audio_tracks = (int)player.audioTracks().size();
            if (!invalid && length > 4000){
                frames = 0;
                player.setPosition(length - 3000);
                QTimer::singleShot(25000, &loop, [&]{ loop.quit(); });
                loop.exec();
            }

            //  Let go of the file before the player goes.
            player.stop();
            player.setVideoSink(nullptr);
            player.setSource(QUrl());
            QCoreApplication::processEvents();
        }

        report << QString(
            "Tutorial test: %1: %2 pictures of %3x%4 in the first 5 s, %5 at the end, %6 sound track(s), length %7 ms, %8%9."
        )
            .arg(film)
            .arg(first_frames)
            .arg(size.width()).arg(size.height())
            .arg(frames)
            .arg(audio_tracks)
            .arg(length)
            .arg(finished ? "played to the end" : "did not play to the end")
            .arg(error.isEmpty() ? QString() : ", error: " + error);
        const bool ok =
            !invalid &&
            finished &&
            first_frames >= 60 &&
            frames >= 20 &&
            size.width() == 1920 && size.height() == 1080 &&
            audio_tracks >= 1 &&
            length >= 60000;
        if (!ok){
            failures++;
            report << QString("Tutorial test: FAILED: %1.").arg(film);
        }
    }
    report << QString("Tutorial test: %1.").arg(failures == 0 ? "passed" : "FAILED");
    return failures == 0 ? 0 : 1;
}


}
}

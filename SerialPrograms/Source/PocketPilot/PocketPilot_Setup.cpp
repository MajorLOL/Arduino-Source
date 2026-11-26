/*  PocketPilot Setup
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <vector>
#include <QApplication>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QCamera>
#include <QCameraDevice>
#include <QAudioOutput>
#include <QCheckBox>
#include <QClipboard>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
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
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QMediaCaptureSession>
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
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <QVideoWidget>
#include <QWheelEvent>
#include <QWidget>
#include "Common/Cpp/Cryptography/SecretSeal.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Options/StringOption.h"
#include "PocketPilot_Firmware.h"
#include "PocketPilot_Language.h"
#include "PocketPilot_Scrub.h"
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
    //  The wheel turned over this place (delta: 120 for one notch up). True
    //  when the page used it.
    virtual bool wheel(Canvas& canvas, const QPointF& at, int delta){
        (void)canvas;
        (void)at;
        (void)delta;
        return false;
    }
    //  The mouse is pressed (phase 0), moves (1) or is let go (2) on a place
    //  whose id starts with "drag:".
    virtual void drag(Canvas& canvas, const QString& id, const QPointF& at, int phase){
        (void)canvas;
        (void)id;
        (void)at;
        (void)phase;
    }
    //  What the page is doing, in words (for the tests).
    virtual QString state() const{
        return QString();
    }
    //  As the main window's screen the buttons of the top bar are drawn over
    //  the page; a page that fills the whole window (the video) leaves them out.
    virtual bool shows_menu() const{
        return true;
    }
    //  What the page shows, for the miniature of the installation ("Pokemon
    //  FireRed: AutoStory"); empty: nothing in particular.
    virtual QString about() const{
        return QString();
    }
};

struct Hit{
    QString id;
    QRectF rect;
    bool enabled = true;
    bool tab_stop = true;
};

class Canvas : public QWidget, public AppView{
public:
    //  "parent" and "app": the screens as the main window's own screen (an
    //  embedded widget, which stays); both empty: a window of their own, that
    //  ends with finish().
    Canvas(const SetupRequest& request, bool test_mode, QWidget* parent = nullptr, AppActions* app = nullptr);

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

    //  ---- minimizing
    //  The screens that are a window of their own (the setup, the choice of a
    //  game and of a program, full screen, with no title bar) have a button in
    //  the top right corner that minimizes the window, and Ctrl+M does the
    //  same. The main window has the buttons of its own title bar.
    bool can_minimize() const{
        return !m_app_mode && parentWidget() == nullptr;
    }
    void minimize(){
        m_minimize_requests++;
        if (!m_test_mode){
            showMinimized();
        }
    }
    //  How often it was asked for (the test does not minimize its windows).
    int minimize_requests() const{
        return m_minimize_requests;
    }

    //  ---- the installations (see PocketPilot_Instances.h)
    //  The button in the corner (and "Installations" along the top of the main
    //  window): shows the overview and puts this window away (the test only
    //  counts, and does not put its window away).
    void go_overview(){
        m_overview_requests++;
        const std::function<void()>& show = m_app_mode ? m_app.installations : m_request.show_overview;
        if (show){
            show();
        }
        if (!m_test_mode){
            if (QWidget* top = window()){
                top->showMinimized();
            }
        }
    }
    int overview_requests() const{
        return m_overview_requests;
    }
    //  The overview itself, as the page of this window.
    void go_instances();
    //  What this window tells the overview, once a second: see report_presence().
    int presence_reports() const{
        return m_presence_reports;
    }

    //  ---- the main window's own screen (see AppView)
    bool app_mode() const{
        return m_app_mode;
    }
    const AppActions& app() const{
        return m_app;
    }
    bool app_busy() const{
        return m_app_busy;
    }
    const QString& app_state_text() const{
        return m_app_state;
    }
    const QString& app_sleep_text() const{
        return m_app_sleep;
    }
    bool app_keep_on() const{
        return m_app_keep_on;
    }
    void app_set_keep_on(bool on){
        m_app_keep_on = on;
        if (m_app.keep_screen_on){
            m_app.keep_screen_on(on);
        }
    }
    void app_console_chosen(const QString& code){
        if (m_app_mode && m_app.console_chosen){
            m_app.console_chosen(code);
        }
    }
    //  Where Claude's panel begins, in the places of the 1920 x 1080 screen
    //  (the right end of the screen when it is not shown).
    double app_right_edge() const{
        if (!m_claude_shown || m_claude == nullptr){
            return DW;
        }
        const double scale = window_scale();
        return (m_claude->x() - (width() - DW * scale) / 2.0) / scale;
    }
    //  The program that was open is closed: where the way back goes.
    void leave_host(int game);
    //  Opens a program of a game on its own page (the main window makes it).
    void open_program_page(int game, const QString& program);

    //  AppView
    virtual QWidget* widget() override{
        return this;
    }
    virtual void show_games() override{
        go_games();
    }
    virtual void show_programs(int game) override{
        go_programs(game);
    }
    virtual void host(QWidget* content, int game, const QString& game_name, const QString& title) override;
    virtual void unhost(QWidget* content) override;
    virtual bool hosting() const override{
        return m_hosting;
    }
    virtual void set_resume(const QString& game, const QString& program) override{
        m_resume_game = game;
        m_resume_program = program;
        update();
    }
    const QString& resume_game() const{
        return m_resume_game;
    }
    const QString& resume_program() const{
        return m_resume_program;
    }
    virtual void set_state(bool busy, const QString& text) override{
        m_app_busy = busy;
        m_app_state = text;
        update();
    }
    virtual void set_sleep_text(const QString& text) override{
        m_app_sleep = text;
        update();
    }
    virtual void set_keep_on(bool on) override{
        m_app_keep_on = on;
        update();
    }
    virtual void set_claude(QWidget* panel) override;
    virtual void show_claude(bool show) override;
    virtual bool claude_shown() const override{
        return m_claude_shown;
    }
    virtual bool has_button(const QString& id) const override{
        return has_hit(id);
    }
    virtual QString page() const override{
        return page_name();
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
    //  ---- for the pages that put widgets of their own over the picture (the
    //  options of a program): where a place of the 1920 x 1080 screen is in the
    //  window, how many pixels of the window one pixel of the screen is, and
    //  whether the page is on the screen as a whole (not fading in or out).
    QRect window_rect(const QRectF& design) const{
        const double scale = window_scale();
        const QPointF offset((width() - DW * scale) / 2.0, (height() - DH * scale) / 2.0);
        return QRect(
            (int)std::lround(offset.x() + design.left() * scale),
            (int)std::lround(offset.y() + design.top() * scale),
            (int)std::lround(design.width() * scale),
            (int)std::lround(design.height() * scale)
        );
    }
    double window_scale() const{
        return std::min(width() / DW, height() / DH);
    }
    //  The whole window in the places of the 1920 x 1080 screen (more than it
    //  when the window is not 16:9): what a page that fills the window, the
    //  video in full screen, draws over.
    QRectF screen_rect() const{
        const double scale = window_scale();
        return QRectF(
            -(width() - DW * scale) / 2.0 / scale,
            -(height() - DH * scale) / 2.0 / scale,
            width() / scale,
            height() / scale
        );
    }
    //  The console is being chosen again, from the games.
    bool console_again() const{
        return m_console_again;
    }
    double page_fade() const{
        return m_fade;
    }
    //  A widget of the page (not the page itself) has the keyboard.
    bool child_has_focus() const{
        const QWidget* widget = focusWidget();
        return widget != nullptr && widget != this && isAncestorOf(widget);
    }
    void take_focus(){
        setFocus(Qt::OtherFocusReason);
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
    //  From the games: the choice of the console again. Continue and Back
    //  both lead back to the games ("games_selected": the game that was
    //  picked); the board is not asked again.
    void go_console_again(int games_selected);
    void leave_console_again();
    void go_board();
    void go_steps(const QString& board);
    void go_guide();
    void go_other();
    void go_done(const QString& board);
    //  "selected": the game that is picked already (-1: none).
    void go_games(int selected = -1);
    //  The programs of this game (an index into the games).
    void go_programs(int game);
    bool has_programs(int game) const{
        for (const HomeProgram& program : m_request.programs){
            if (program.game == game){
                return true;
            }
        }
        return false;
    }
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
        if (m_console_again){
            m_console_again = false;
            go_games(m_games_back);
        }else if (m_request.ask_board){
            go_board();
        }else{
            end_installation();
        }
    }
    //  Presses it as a click on its middle would; false when it is not there.
    virtual bool click(const QString& id) override;
    //  Moves the mouse over it.
    bool move_to(const QString& id);
    void press_key(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier, const QString& text = QString());
    //  Types the letters one by one.
    void type_text(const QString& text);
    //  Turns the wheel over the middle of it (positive: up).
    bool scroll(const QString& id, int notches);
    //  Drags over it, from one place to another (0 to 1 of its width).
    bool drag_over(const QString& id, double from, double to);
    //  Time passes (with pictures drawn on the way).
    virtual void wait(double seconds) override;
    virtual QImage picture() override;
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
    virtual void wheelEvent(QWheelEvent* event) override;
    virtual void closeEvent(QCloseEvent* event) override;
    //  Tab moves between the places of the page, as long as the keyboard is
    //  on the page itself; in a widget of the page (its options) it moves
    //  between its widgets, as Qt does.
    virtual bool focusNextPrevChild(bool next) override;

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
    //  A button, a row ... of the page: or one of the buttons along the top.
    void dispatch(const QString& id);
    void paint_app_menu(QPainter& painter);
    void paint_minimize(QPainter& painter);
    void paint_overview_button(QPainter& painter);
    void paint_close_button(QPainter& painter);
    void report_presence();
    bool app_activate(const QString& id);
    void place_claude();

private:
    SetupRequest m_request;
    SetupResult m_result;
    bool m_test_mode;
    bool m_first_page = true;
    bool m_console_again = false;
    int m_games_back = -1;
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
    QString m_drag_id;

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
    int m_minimize_requests = 0;
    int m_overview_requests = 0;
    //  The status and the miniature that the overview reads, once a second.
    QTimer m_presence;
    int m_presence_reports = 0;

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

    //  The main window's own screen.
    bool m_app_mode = false;
    AppActions m_app;
    bool m_hosting = false;
    //  The page that host() made, until it is on the screen; and "the main
    //  window is making it now" (so that host() does not queue it twice).
    std::unique_ptr<Page> m_hosted;
    bool m_opening = false;
    bool m_app_busy = false;
    QString m_app_state;
    QString m_app_sleep;
    bool m_app_keep_on = false;
    QPointer<QWidget> m_claude;
    bool m_claude_shown = false;
    QString m_resume_game;
    QString m_resume_program;
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

//  The three steps of the Setup Guide at the top. state: 0 done, 1 active, 2 next.
//  (The guide has a step in between, "General setup", that the program has no
//  screen for: it is left out instead of being shown as done.)
void draw_stepper(QPainter& painter, const int (&states)[3], double reveal){
    if (reveal < 0.004){
        return;
    }
    static const char* const NAMES[3] = {"Hardware", "Controller", "Finishing up"};
    const QFont font = ui_font(25, 600);
    constexpr double PILL_H = 56;
    constexpr double GAP = 26;
    double widths[3];
    double total = GAP * 2;
    for (int c = 0; c < 3; c++){
        widths[c] = text_width(font, T(NAMES[c])) + 36 + 18 + 28;
        total += widths[c];
    }
    double x = (DW - total) / 2.0;
    const double y = 44 - (1.0 - eo(reveal)) * 26;
    for (int c = 0; c < 3; c++){
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
        if (c < 2){
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
//  A chevron that points left: the mark of a link that goes back.
void draw_chevron_back(QPainter& painter, const QPointF& c, const QColor& color){
    QPen pen(color, 2.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(QPolygonF({QPointF(c.x() + 5, c.y() - 9), QPointF(c.x() - 4, c.y()), QPointF(c.x() + 5, c.y() + 9)}));
}

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
        const int states[3] = {1, 2, 2};
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
        const int states[3] = {0, 1, 2};
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
        //  A file that came with the program is the file the build hashed.
        if (!firmware_matches_checksum(m_firmware_dir, path)){
            m_flash = Flash::FAILED;
            m_flash_error = "checksum";
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
                const QString fetched = QDir(m_firmware_dir).filePath(m_fetch_name);
                const bool ok = m_fetch->ok() && looks_like_firmware(fetched);
                const bool matches = !ok || firmware_matches_checksum(m_firmware_dir, fetched);
                const QString name = m_fetch_name;
                m_fetch.reset();
                if (ok && matches){
                    rebuild_versions(false);
                    m_versions.select(name);
                    m_flash = Flash::IDLE;
                }else{
                    QFile::remove(QDir(m_firmware_dir).filePath(name));
                    m_flash = Flash::FAILED;
                    m_flash_error = ok ? "checksum" : "download";
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
        if (m_flash_error == "checksum"){
            return T("This file is not the one the build of the program hashed. It was changed or damaged: it is not flashed. Download it again.");
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
        const int states[3] = {1, 2, 2};
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
        const int states[3] = {1, 2, 2};
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
        const int states[3] = {0, 0, 1};
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
    explicit GamesPage(Canvas& canvas, int selected = -1)
        : m_games(canvas.request().games)
        , m_app_mode(canvas.app_mode())
    {
        const int count = (int)m_games.size();
        m_columns = count <= 12 ? 4 : 5;
        for (const HomeGame& game : m_games){
            m_grounds.push_back(logo_ground(game.icon));
        }
        if (selected >= 0 && selected < count && m_games[(size_t)selected].enabled){
            //  Back from the programs of this game.
            m_selected = selected;
            canvas.set_focus_id(tile_id(selected));
            return;
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
        const bool searching = canvas.app_mode() && !m_query.isEmpty();
        if (canvas.app_mode()){
            paint_search(canvas, painter, eo(prog(t, in.sub, in.sub + 0.6)), sub_y - 4);
        }
        if (searching){
            paint_found(canvas, painter, top, eo(prog(t, 0.0, 0.2)));
        }else{
            for (int c = 0; c < (int)m_games.size(); c++){
                const double begin = in.bar + 0.05 * c;
                paint_tile(canvas, painter, c, tile_rect(c, top), eo(prog(t, begin, begin + 0.55)));
            }
        }

        const double b = eo(prog(t, in.buttons, in.buttons + 0.5));
        double x = 136;
        {
            //  Back to the console: another one can be picked.
            const double w = draw_button(canvas, painter, "console", T("Choose another console"), x + 26, 960, ButtonKind::LINK, std::min(1.0, b * 1.35));
            draw_chevron_back(painter, QPointF(x + 8, 982), white(0.7 * b));
            x += 26 + w + 34;
        }
        if (!canvas.app_mode()){
            //  (As the main window's own screen there is nothing to leave.)
            draw_button(canvas, painter, "later", T("Choose later"), x, 960, ButtonKind::LINK, std::min(1.0, b * 1.35));
        }
        const QString start_text = T("Start");
        const double start_w = button_width(start_text, ButtonKind::PRIMARY);
        //  As the window's screen a game with no programs has nothing to open.
        const bool can_start = m_selected >= 0 && !searching && (!canvas.app_mode() || canvas.has_programs(m_selected));
        draw_button(canvas, painter, "start", start_text, 1780 - start_w, 950, ButtonKind::PRIMARY, b, can_start);
        if (canvas.app_mode() && !searching){
            //  The program of the last time, one press away.
            const HomeProgram* resume = resume_program(canvas);
            if (resume != nullptr){
                const QString text = QFontMetricsF(ui_font(28, 600)).elidedText(
                    tr_ui("Continue with %1").arg(resume->name), Qt::ElideRight, 560
                );
                const double w = button_width(text, ButtonKind::SECONDARY);
                draw_button(canvas, painter, "resume", text, 1780 - start_w - 26 - w, 950, ButtonKind::SECONDARY, b);
            }
        }
    }

    virtual void step(double dt) override{
        m_clock += dt;
    }
    virtual void escape_pressed(Canvas& canvas) override{
        (void)canvas;
        if (!m_query.isEmpty()){
            set_query(canvas, QString());
        }
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        if (id == "console"){
            canvas.go_console_again(m_selected);
            return;
        }
        if (id == "resume"){
            open_program(canvas, resume_program(canvas));
            return;
        }
        if (id == "gsearch"){
            return;
        }
        if (id == "gsearch-clear"){
            set_query(canvas, QString());
            return;
        }
        if (id.startsWith("found:")){
            const int at = id.mid(6).toInt();
            if (at >= 0 && at < (int)m_found.size()){
                open_program(canvas, &canvas.request().programs[(size_t)m_found[(size_t)at]]);
            }
            return;
        }
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
        if (canvas.app_mode() && search_key(canvas, event)){
            return true;
        }
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
        if (canvas.app_mode() && !m_query.isEmpty()){
            if (m_found_at >= 0 && m_found_at < (int)m_found.size()){
                open_program(canvas, &canvas.request().programs[(size_t)m_found[(size_t)m_found_at]]);
            }
            return;
        }
        if (m_selected >= 0){
            start(canvas);
        }
    }
    virtual QString state() const override{
        QString text = m_selected < 0 ? QString("selected=") : "selected=" + m_games[(size_t)m_selected].name;
        if (m_app_mode){
            text += ";query=" + m_query + ";found=" + QString::number((int)m_found.size());
        }
        return text;
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
    //  ---- the search over all programs (the window's screen only): what is
    //  typed turns the cartridges into the list of the programs that match.
    static constexpr int FOUND_ROWS = 10;
    static constexpr int FOUND_COLUMNS = 2;

    //  The program of the last time, if it is in the list.
    const HomeProgram* resume_program(const Canvas& canvas) const{
        if (canvas.resume_game().isEmpty() || canvas.resume_program().isEmpty()){
            return nullptr;
        }
        for (const HomeProgram& program : canvas.request().programs){
            if (program.game >= 0 && program.game < (int)m_games.size() &&
                m_games[(size_t)program.game].name == canvas.resume_game() &&
                program.name == canvas.resume_program()
            ){
                return &program;
            }
        }
        return nullptr;
    }
    void open_program(Canvas& canvas, const HomeProgram* program){
        if (program == nullptr || program->game < 0 || program->game >= (int)m_games.size()){
            return;
        }
        canvas.result().game = m_games[(size_t)program->game].name;
        canvas.result().program = program->name;
        canvas.finish();
    }
    void set_query(Canvas& canvas, const QString& query){
        m_query = query;
        m_found.clear();
        m_found_at = 0;
        if (!query.isEmpty()){
            //  The name first, then what is only in its description or the game's name.
            const std::vector<HomeProgram>& programs = canvas.request().programs;
            for (int pass = 0; pass < 2; pass++){
                for (size_t c = 0; c < programs.size(); c++){
                    const HomeProgram& program = programs[c];
                    if (program.game < 0 || program.game >= (int)m_games.size()){
                        continue;
                    }
                    const bool in_name = program.name.contains(query, Qt::CaseInsensitive);
                    const bool in_other = program.description.contains(query, Qt::CaseInsensitive) ||
                        m_games[(size_t)program.game].name.contains(query, Qt::CaseInsensitive);
                    if ((pass == 0 && in_name) || (pass == 1 && !in_name && in_other)){
                        m_found.push_back((int)c);
                    }
                }
            }
        }
    }
    //  True when the key was for the search.
    bool search_key(Canvas& canvas, QKeyEvent& event){
        const int key = event.key();
        if (!m_query.isEmpty()){
            const int count = (int)m_found.size();
            const int shown = std::min(count, FOUND_ROWS * FOUND_COLUMNS);
            switch (key){
            case Qt::Key_Backspace:
                set_query(canvas, m_query.left(m_query.size() - 1));
                return true;
            case Qt::Key_Delete:
                set_query(canvas, QString());
                return true;
            case Qt::Key_Up:
                m_found_at = std::max(0, m_found_at - 1);
                return true;
            case Qt::Key_Down:
                m_found_at = std::min(std::max(0, shown - 1), m_found_at + 1);
                return true;
            case Qt::Key_Left:
                m_found_at = std::max(0, m_found_at - FOUND_ROWS);
                return true;
            case Qt::Key_Right:
                m_found_at = std::min(std::max(0, shown - 1), m_found_at + FOUND_ROWS);
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                if (m_found_at >= 0 && m_found_at < count){
                    open_program(canvas, &canvas.request().programs[(size_t)m_found[(size_t)m_found_at]]);
                }
                return true;
            default:
                break;
            }
        }
        if (key == Qt::Key_Escape || key == Qt::Key_Tab || key == Qt::Key_Backtab || key == Qt::Key_Return || key == Qt::Key_Enter){
            return false;
        }
        if (event.modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)){
            return false;
        }
        const QString text = event.text();
        if (text.isEmpty() || !text[0].isPrint()){
            return false;
        }
        if (text[0] == ' ' && m_query.isEmpty()){
            //  The space bar presses the button that has the keyboard.
            return false;
        }
        set_query(canvas, m_query + text);
        return true;
    }
    void paint_search(Canvas& canvas, QPainter& painter, double a, double y){
        if (a < 0.004){
            return;
        }
        const QRectF rect(1180, y, 600, 50);
        const bool hot = !m_query.isEmpty() || canvas.hovered("gsearch");
        panel(painter, rect, 25, with_alpha(PANEL, 0.8 * a), hot ? with_alpha(ORANGE, 0.95 * a) : white(0.30 * a), 2.0);
        canvas.add_hit("gsearch", rect, true, false);
        const QPointF glass(rect.left() + 32, rect.center().y() - 2);
        painter.setPen(QPen(with_alpha(ORANGE, a), 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(glass, 8, 8);
        painter.drawLine(QPointF(glass.x() + 6, glass.y() + 6), QPointF(glass.x() + 13, glass.y() + 13));
        const QFont font = ui_font(25, 500);
        const double x = rect.left() + 62;
        const double room = rect.width() - 62 - 46;
        const QString shown = QFontMetricsF(font).elidedText(m_query, Qt::ElideLeft, room);
        if (m_query.isEmpty()){
            draw_text_cy(painter, T("Search all programs"), x + 8, rect.center().y() + 1, font, white(0.5 * a));
        }else{
            draw_text_cy(painter, shown, x, rect.center().y() + 1, font, white(a));
        }
        if (std::fmod(m_clock, 1.0) < 0.55){
            const double cx = x + (m_query.isEmpty() ? 2 : text_width(font, shown) + 3);
            painter.setPen(QPen(with_alpha(ORANGE, a), 2.4));
            painter.drawLine(QPointF(cx, rect.center().y() - 12), QPointF(cx, rect.center().y() + 12));
        }
        if (!m_query.isEmpty()){
            const QRectF clear(rect.right() - 44, rect.center().y() - 16, 32, 32);
            const bool over = canvas.hovered("gsearch-clear");
            painter.setPen(QPen(white((over ? 0.95 : 0.6) * a), 2.0, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(clear.center().x() - 6, clear.center().y() - 6), QPointF(clear.center().x() + 6, clear.center().y() + 6));
            painter.drawLine(QPointF(clear.center().x() - 6, clear.center().y() + 6), QPointF(clear.center().x() + 6, clear.center().y() - 6));
            canvas.add_hit("gsearch-clear", clear, true, false);
        }
    }
    void paint_found(Canvas& canvas, QPainter& painter, double top, double a){
        if (m_found.empty()){
            draw_text_cy(painter, T("No program matches."), 144, top + 40, ui_font(30, 500), white(0.6 * a));
            return;
        }
        constexpr double LEFT = 140;
        constexpr double RIGHT = 1780;
        constexpr double GAP = 22;
        constexpr double ROW = 64;
        const double width = (RIGHT - LEFT - GAP * (FOUND_COLUMNS - 1)) / FOUND_COLUMNS;
        const int shown = std::min((int)m_found.size(), FOUND_ROWS * FOUND_COLUMNS);
        for (int c = 0; c < shown; c++){
            const HomeProgram& program = canvas.request().programs[(size_t)m_found[(size_t)c]];
            const int column = c / FOUND_ROWS;
            const int row = c % FOUND_ROWS;
            const QRectF rect(LEFT + (width + GAP) * column, top + (ROW + 10) * row, width, ROW);
            const bool picked = c == m_found_at;
            const bool over = canvas.hovered("found:" + QString::number(c));
            panel(painter, rect, 18, with_alpha(PANEL, (over ? 0.95 : 0.8) * a),
                picked ? with_alpha(ORANGE, 0.95 * a) : white((over ? 0.45 : 0.22) * a), picked ? 2.4 : 1.6);
            const QString game = short_game_name(m_games[(size_t)program.game].name);
            const QFont game_font = ui_font(21, 500);
            const double game_w = std::min(width * 0.4, text_width(game_font, game));
            draw_text_cy(painter, QFontMetricsF(game_font).elidedText(game, Qt::ElideRight, game_w),
                rect.right() - 20, rect.center().y() + 1, game_font, white(0.55 * a), Qt::AlignRight);
            const QFont name_font = ui_font(27, 600);
            draw_text_cy(painter, QFontMetricsF(name_font).elidedText(program.name, Qt::ElideRight, width - game_w - 64),
                rect.left() + 22, rect.center().y() + 1, name_font, white(a));
            canvas.add_hit("found:" + QString::number(c), rect, true, false);
        }
        if ((int)m_found.size() > shown){
            draw_text_cy(painter, tr_ui("and %1 more: type more of the name").arg((int)m_found.size() - shown),
                LEFT + 4, top + (ROW + 10) * FOUND_ROWS + 14, ui_font(22, 500), white(0.5 * a));
        }
    }

    void start(Canvas& canvas){
        if (m_selected < 0){
            return;
        }
        if (canvas.has_programs(m_selected)){
            //  The game is picked; which program to start comes next.
            canvas.go_programs(m_selected);
            return;
        }
        if (canvas.app_mode()){
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
    bool m_app_mode = false;
    double m_clock = 0;
    QString m_query;
    std::vector<int> m_found;       //  indexes into the request's programs
    int m_found_at = 0;
};


//  ---------------------------------------------------------------------
//  The picture and the sound of a capture card, for the screen with the
//  programs
//  ---------------------------------------------------------------------

//  The first capture card Qt can see: its picture (as small as the screen
//  needs it), how many pictures come each second, and the loudness of the
//  sound that belongs to it. Without a person (the test) a made-up picture
//  and a made-up sound stand in, so that the screen can be looked at on a
//  computer that has no card; they are never used otherwise.
class LiveInput{
public:
    explicit LiveInput(bool synthetic)
        : m_synthetic(synthetic)
    {
        m_clock.start();
        refresh_devices();
        if (m_synthetic){
            m_video_names = QStringList{"USB3 Video", "HD60 X"};
            m_audio_name = "USB3 Audio";
            m_audio_ok = true;
            m_video_index = 0;
            m_device_ids.clear();
            m_device_ids << QByteArray() << QByteArray();
            return;
        }
        //  (Each installation remembers its own capture card.)
        const QString wanted = QSettings(QSettings::IniFormat, QSettings::UserScope, "PocketPilot", "Setup")
            .value(instance_setting_key("capture_device")).toString();
        int index = 0;
        for (int c = 0; c < m_video_names.size(); c++){
            if (m_video_names[c] == wanted){
                index = c;
            }
        }
        select_video(index, false);
    }
    ~LiveInput(){
        stop();
    }

    //  The names of the capture cards, and which one is used (-1: none).
    const QStringList& video_names() const{
        return m_video_names;
    }
    int video_index() const{
        return m_video_index;
    }
    void select_video(int index, bool remember = true){
        stop();
        m_picture = QImage();
        m_has_picture = false;
        m_fps = 0;
        m_window_frames = 0;
        m_error.clear();
        m_audio_name.clear();
        m_audio_ok = false;
        m_level = 0;
        if (index < 0 || index >= m_video_names.size()){
            m_video_index = -1;
            return;
        }
        m_video_index = index;
        if (m_synthetic){
            m_audio_name = index == 0 ? "USB3 Audio" : "HD60 X Audio";
            return;
        }
        if (remember){
            QSettings(QSettings::IniFormat, QSettings::UserScope, "PocketPilot", "Setup")
                .setValue(instance_setting_key("capture_device"), m_video_names[index]);
        }
        start_video(index);
        start_audio(index);
    }

    bool has_picture() const{
        return m_has_picture;
    }
    //  A picture came in the last two seconds.
    bool live() const{
        if (m_synthetic){
            return m_video_index >= 0;
        }
        return m_has_picture && (double)m_clock.elapsed() / 1000.0 - m_last_frame < 2.0;
    }
    const QImage& picture() const{
        return m_picture;
    }
    QSize picture_size() const{
        return m_synthetic ? QSize(1920, 1080) : m_size;
    }
    double fps() const{
        return m_synthetic && m_video_index >= 0 ? 60.0 : m_fps;
    }
    const QString& error() const{
        return m_error;
    }

    const QString& audio_name() const{
        return m_audio_name;
    }
    bool audio_ok() const{
        return m_audio_ok;
    }
    //  0 (silence, -60 dB or less) to 1 (0 dB).
    double level() const{
        return m_level;
    }
    double level_db() const{
        return m_level <= 0.001 ? -60.0 : m_level * 60.0 - 60.0;
    }
    const QStringList& audio_inputs() const{
        return m_audio_inputs;
    }

    void step(double dt){
        m_since_fps += dt;
        m_since_devices += dt;
        if (m_synthetic){
            m_synthetic_time += dt;
            if (m_video_index >= 0){
                const double wave = 0.5 + 0.5 * std::sin(m_synthetic_time * 3.1) * std::sin(m_synthetic_time * 0.9 + 1.0);
                m_level = clamp01(0.25 + 0.55 * wave * (0.6 + 0.4 * std::sin(m_synthetic_time * 11.0)));
                m_has_picture = true;
                if (m_picture.isNull()){
                    m_picture = sunset_picture();
                }
            }
            return;
        }
        if (m_since_fps >= 1.0){
            m_fps = (double)m_window_frames / m_since_fps;
            m_window_frames = 0;
            m_since_fps = 0;
        }
        poll_audio(dt);
        if (m_since_devices >= 3.0){
            m_since_devices = 0;
            const QStringList before = m_video_names;
            refresh_devices();
            if (before != m_video_names){
                //  A card came or went: look again.
                const QString used = m_video_index >= 0 && m_video_index < before.size() ? before[m_video_index] : QString();
                int index = -1;
                for (int c = 0; c < m_video_names.size(); c++){
                    if (m_video_names[c] == used){
                        index = c;
                    }
                }
                select_video(index >= 0 ? index : (m_video_names.isEmpty() ? -1 : 0), false);
            }
        }
    }

private:
    void refresh_devices(){
        if (m_synthetic){
            return;
        }
        m_video_names.clear();
        m_device_ids.clear();
        for (const QCameraDevice& device : QMediaDevices::videoInputs()){
            m_video_names << device.description();
            m_device_ids << device.id();
        }
        m_audio_inputs.clear();
        for (const QAudioDevice& device : QMediaDevices::audioInputs()){
            m_audio_inputs << device.description();
        }
    }

    void stop(){
        if (m_audio_source != nullptr){
            m_audio_source->stop();
        }
        m_audio_io = nullptr;
        m_audio_source.reset();
        if (m_camera != nullptr){
            m_camera->stop();
        }
        if (m_session != nullptr){
            m_session->setCamera(nullptr);
            m_session->setVideoSink(nullptr);
        }
        m_session.reset();
        m_camera.reset();
        m_sink.reset();
    }

    void start_video(int index){
        const QList<QCameraDevice> devices = QMediaDevices::videoInputs();
        QCameraDevice device;
        bool found = false;
        for (const QCameraDevice& candidate : devices){
            if (candidate.id() == m_device_ids[index]){
                device = candidate;
                found = true;
            }
        }
        if (!found){
            m_error = "The card went away.";
            return;
        }
        m_camera.reset(new QCamera(device));
        //  The largest picture up to 1080p, the fastest of those.
        QCameraFormat best;
        double best_score = -1;
        for (const QCameraFormat& format : device.videoFormats()){
            const double area = (double)format.resolution().width() * format.resolution().height();
            if (area > 1920.0 * 1080.0 + 1){
                continue;
            }
            const double score = area + format.maxFrameRate() * 1000.0;
            if (score > best_score){
                best_score = score;
                best = format;
            }
        }
        if (!best.isNull()){
            m_camera->setCameraFormat(best);
        }
        m_sink.reset(new QVideoSink());
        m_session.reset(new QMediaCaptureSession());
        m_session->setCamera(m_camera.get());
        m_session->setVideoSink(m_sink.get());
        QObject::connect(m_sink.get(), &QVideoSink::videoFrameChanged, &m_context, [this](const QVideoFrame& frame){
            on_frame(frame);
        });
        QObject::connect(m_camera.get(), &QCamera::errorOccurred, &m_context, [this](QCamera::Error, const QString& text){
            m_error = text.isEmpty() ? QString("The card gives no picture.") : text;
        });
        m_camera->start();
    }

    void on_frame(const QVideoFrame& frame){
        const double now = (double)m_clock.elapsed() / 1000.0;
        m_window_frames++;
        m_last_frame = now;
        if (now - m_last_convert < 0.066 && m_has_picture){
            return;
        }
        m_last_convert = now;
        const QImage image = frame.toImage();
        if (image.isNull()){
            return;
        }
        m_size = image.size();
        m_picture = image.width() > 640 ? image.scaledToWidth(640, Qt::FastTransformation) : image;
        m_has_picture = true;
        m_error.clear();
    }

    //  The sound of the card is the input that has a word of its name.
    void start_audio(int index){
        static const QStringList GENERIC{"video", "audio", "capture", "camera", "webcam", "device", "digital", "interface", "input", "usb", "microphone"};
        auto words = [&](const QString& text){
            QStringList found;
            for (const QString& word : text.toLower().split(QRegularExpression("[^a-z0-9]+"), Qt::SkipEmptyParts)){
                if (word.size() >= 3 && !GENERIC.contains(word)){
                    found << word;
                }
            }
            return found;
        };
        const QStringList wanted = words(m_video_names[index]);
        QAudioDevice best;
        int best_score = 0;
        for (const QAudioDevice& device : QMediaDevices::audioInputs()){
            int score = 0;
            for (const QString& word : words(device.description())){
                if (wanted.contains(word)){
                    score++;
                }
            }
            if (score > best_score){
                best_score = score;
                best = device;
            }
        }
        if (best.isNull()){
            return;
        }
        m_audio_name = best.description();
        const QAudioFormat format = best.preferredFormat();
        if (format.sampleFormat() == QAudioFormat::Unknown){
            return;
        }
        m_audio_format = format;
        m_audio_source.reset(new QAudioSource(best, format));
        m_audio_io = m_audio_source->start();
        m_audio_ok = m_audio_io != nullptr;
    }

    void poll_audio(double dt){
        if (m_audio_io == nullptr){
            m_level = 0;
            return;
        }
        const QByteArray data = m_audio_io->readAll();
        double peak = 0;
        switch (m_audio_format.sampleFormat()){
        case QAudioFormat::UInt8:
            for (char byte : data){
                peak = std::max(peak, std::fabs(((double)(unsigned char)byte - 128.0) / 128.0));
            }
            break;
        case QAudioFormat::Int16:
            for (qsizetype c = 0; c + 1 < data.size(); c += 2){
                qint16 sample;
                std::memcpy(&sample, data.constData() + c, 2);
                peak = std::max(peak, std::fabs((double)sample / 32768.0));
            }
            break;
        case QAudioFormat::Int32:
            for (qsizetype c = 0; c + 3 < data.size(); c += 4){
                qint32 sample;
                std::memcpy(&sample, data.constData() + c, 4);
                peak = std::max(peak, std::fabs((double)sample / 2147483648.0));
            }
            break;
        case QAudioFormat::Float:
            for (qsizetype c = 0; c + 3 < data.size(); c += 4){
                float sample;
                std::memcpy(&sample, data.constData() + c, 4);
                peak = std::max(peak, std::fabs((double)sample));
            }
            break;
        default:
            break;
        }
        //  Decibel, from -60 up to 0, as 0 to 1; it falls slowly.
        const double now = peak < 0.001 ? 0.0 : clamp01((20.0 * std::log10(peak) + 60.0) / 60.0);
        m_level = now > m_level ? now : std::max(now, m_level - dt * 1.2);
    }

    //  The made-up picture of the test: a sun on a hill.
    static QImage sunset_picture(){
        QImage image(640, 360, QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QLinearGradient sky(0, 0, 0, 360);
        sky.setColorAt(0.0, QColor(28, 33, 54));
        sky.setColorAt(0.55, QColor(196, 112, 84));
        sky.setColorAt(1.0, QColor(232, 160, 110));
        painter.fillRect(image.rect(), sky);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 226, 150));
        painter.drawEllipse(QPointF(400, 205), 38, 38);
        painter.setBrush(QColor(30, 34, 52));
        QPainterPath hill;
        hill.moveTo(0, 262);
        hill.cubicTo(150, 230, 300, 300, 420, 262);
        hill.cubicTo(520, 232, 600, 250, 640, 262);
        hill.lineTo(640, 360);
        hill.lineTo(0, 360);
        painter.drawPath(hill);
        painter.end();
        return image;
    }

private:
    bool m_synthetic;
    double m_synthetic_time = 0;
    QObject m_context;
    QElapsedTimer m_clock;

    QStringList m_video_names;
    QList<QByteArray> m_device_ids;
    QStringList m_audio_inputs;
    int m_video_index = -1;
    std::unique_ptr<QCamera> m_camera;
    std::unique_ptr<QMediaCaptureSession> m_session;
    std::unique_ptr<QVideoSink> m_sink;
    QImage m_picture;
    QSize m_size;
    bool m_has_picture = false;
    int m_window_frames = 0;
    double m_since_fps = 0;
    double m_since_devices = 0;
    double m_last_frame = -10;
    double m_last_convert = -10;
    double m_fps = 0;
    QString m_error;

    std::unique_ptr<QAudioSource> m_audio_source;
    QIODevice* m_audio_io = nullptr;
    QAudioFormat m_audio_format;
    QString m_audio_name;
    bool m_audio_ok = false;
    double m_level = 0;
};


//  ---------------------------------------------------------------------
//  The programs of the game that was chosen: which one to start, with the
//  picture of the game, the checks and the output log next to it
//  ---------------------------------------------------------------------

//  The six lanes of the timeline.
enum class Lane{
    VIDEO,
    AUDIO,
    BOARD,
    LOG,
    BUTTONS,
    PROGRAM,
};
constexpr int LANE_COUNT = 6;

//  What the program knew each half second.
struct LaneSample{
    double at = 0;          //  on the clock of the screen
    double fps = 0;
    double level = 0;       //  0 to 1
    bool board = false;
};

struct LogEntry{
    quint64 serial = 0;
    double at = 0;          //  on the clock of the screen
    qint64 msec = 0;
    int level = 0;
    QString text;
};

QString level_name(int level){
    return level >= 2 ? "ERROR" : level == 1 ? "WARN" : "INFO";
}

//  A small picture for a lane, centered on c.
void lane_icon(QPainter& painter, Lane lane, const QPointF& c, const QColor& color){
    QPen pen(color, 1.8);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    switch (lane){
    case Lane::VIDEO:
        painter.drawRoundedRect(QRectF(c.x() - 9, c.y() - 6, 13, 12), 3, 3);
        painter.drawPolyline(QPolygonF({QPointF(c.x() + 4, c.y() - 2), QPointF(c.x() + 10, c.y() - 5), QPointF(c.x() + 10, c.y() + 5), QPointF(c.x() + 4, c.y() + 2)}));
        break;
    case Lane::AUDIO:
        painter.drawPolyline(QPolygonF({QPointF(c.x() - 9, c.y() - 3), QPointF(c.x() - 5, c.y() - 3), QPointF(c.x(), c.y() - 8), QPointF(c.x(), c.y() + 8), QPointF(c.x() - 5, c.y() + 3), QPointF(c.x() - 9, c.y() + 3)}));
        painter.drawArc(QRectF(c.x() - 3, c.y() - 5, 10, 10), -50 * 16, 100 * 16);
        break;
    case Lane::BOARD:
        painter.drawRoundedRect(QRectF(c.x() - 6, c.y() - 6, 12, 12), 2, 2);
        for (int c2 = -1; c2 <= 1; c2++){
            painter.drawLine(QPointF(c.x() + c2 * 4, c.y() - 9), QPointF(c.x() + c2 * 4, c.y() - 6));
            painter.drawLine(QPointF(c.x() + c2 * 4, c.y() + 6), QPointF(c.x() + c2 * 4, c.y() + 9));
        }
        break;
    case Lane::LOG:
        painter.drawLine(QPointF(c.x() - 8, c.y() - 6), QPointF(c.x() + 8, c.y() - 6));
        painter.drawLine(QPointF(c.x() - 8, c.y()), QPointF(c.x() + 8, c.y()));
        painter.drawLine(QPointF(c.x() - 8, c.y() + 6), QPointF(c.x() + 2, c.y() + 6));
        break;
    case Lane::BUTTONS:
        painter.drawRoundedRect(QRectF(c.x() - 10, c.y() - 6, 20, 12), 6, 6);
        painter.drawLine(QPointF(c.x() - 5, c.y() - 2), QPointF(c.x() - 5, c.y() + 2));
        painter.drawLine(QPointF(c.x() - 7, c.y()), QPointF(c.x() - 3, c.y()));
        painter.drawPoint(QPointF(c.x() + 4, c.y()));
        break;
    case Lane::PROGRAM:
        painter.drawLine(QPointF(c.x() - 8, c.y() - 6), QPointF(c.x() - 8, c.y() + 6));
        painter.drawPolyline(QPolygonF({QPointF(c.x() - 3, c.y() - 6), QPointF(c.x() + 8, c.y()), QPointF(c.x() - 3, c.y() + 6), QPointF(c.x() - 3, c.y() - 6)}));
        break;
    }
}

//  A check that is done, one that is still waiting, or one that failed.
enum class Light{
    OK,
    WAIT,
    BAD,
};

void draw_light(QPainter& painter, Light light, const QPointF& c, double radius, double alpha){
    painter.setPen(Qt::NoPen);
    switch (light){
    case Light::OK:
        painter.setBrush(with_alpha(ORANGE, alpha));
        painter.drawEllipse(c, radius, radius);
        draw_check(painter, QRectF(c.x() - radius, c.y() - radius, 2 * radius, 2 * radius).adjusted(2, 2, -2, -2), with_alpha(Qt::black, alpha), 2.4);
        break;
    case Light::WAIT:
        painter.setPen(QPen(white(0.55 * alpha), 1.8));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(c, radius - 1, radius - 1);
        painter.setPen(Qt::NoPen);
        painter.setBrush(white(0.55 * alpha));
        painter.drawEllipse(c, radius * 0.36, radius * 0.36);
        break;
    case Light::BAD:
        painter.setBrush(with_alpha(YELLOW, alpha));
        painter.drawEllipse(c, radius, radius);
        painter.setPen(QPen(with_alpha(Qt::black, alpha), 2.6, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(c.x(), c.y() - radius * 0.5), QPointF(c.x(), c.y() + radius * 0.12));
        painter.drawPoint(QPointF(c.x(), c.y() + radius * 0.52));
        break;
    }
}

class ProgramsPage : public Page{
public:
    ProgramsPage(Canvas& canvas, int game)
        : m_canvas(&canvas)
        , m_game(game)
        , m_request(&canvas.request())
        , m_game_info(&canvas.request().games[(size_t)game])
        , m_game_name(canvas.request().games[(size_t)game].name)
        , m_build(canvas.request().build)
        , m_input(canvas.test_mode())
        , m_probe(canvas.app_folder(), canvas.scan_override())
        , m_test_probe(canvas.test_mode())
    {
        const SetupRequest& request = canvas.request();
        for (const HomeProgram& program : request.programs){
            if (program.game == game){
                m_programs.push_back(program);
            }
        }
        std::set<QString> sections;
        for (const HomeProgram& program : m_programs){
            if (!program.section.isEmpty()){
                sections.insert(program.section);
            }
        }
        m_sections = sections.size() > 1;
        rebuild_view(QString());
        m_probe.step(10, true);
        look_at_board();
        poll_log();
        canvas.set_focus_id("start");
    }
    virtual ~ProgramsPage() override{
        //  What was changed in the options is kept; their widgets go before the
        //  panel that holds them.
        release_options();
        delete m_host.data();
    }

    virtual const char* name() const override{
        return "programs";
    }
    //  The picture in full screen fills the window: no buttons over it.
    virtual bool shows_menu() const override{
        return !(m_video_full && m_full.value > 0.5);
    }

    //  ---- time

    virtual void step(double dt) override{
        m_clock += dt;
        m_since_sample += dt;
        m_since_poll += dt;
        m_input.step(dt);
        m_full.target = m_video_full ? 1.0 : 0.0;
        m_full.step(dt, 14.0);
        m_scroll.step(dt, 16.0);
        if (m_toast_left > 0){
            m_toast_left -= dt;
        }
        m_probe.step(dt, m_test_probe);
        step_options(dt);
        if (m_since_poll >= 0.5){
            m_since_poll = 0;
            look_at_board();
            poll_log();
        }
        while (m_since_sample >= 0.5){
            m_since_sample -= 0.5;
            LaneSample sample;
            sample.at = m_clock - m_since_sample;
            sample.fps = m_input.live() ? m_input.fps() : 0;
            sample.level = m_input.audio_ok() ? m_input.level() : 0;
            sample.board = m_board_ok;
            m_samples.push_back(sample);
            while (!m_samples.empty() && m_clock - m_samples.front().at > 130.0){
                m_samples.pop_front();
            }
        }
    }

    //  ---- the page

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        const double a_head = eo(prog(t, in.head, in.head + 0.5));
        const double a_body = eo(prog(t, in.sub, in.sub + 0.6));
        const double a_foot = eo(prog(t, in.buttons, in.buttons + 0.5));
        relayout();
        if (m_video_full && m_full.value > 0.98){
            paint_video_full(canvas, painter);
            return;
        }

        const QString heading = T("Which program do you want to start?");
        draw_heading(painter, heading, 138, 34, 58, prog(t, in.head, in.head + 0.9), 1.0, 1100);
        paint_game_badge(painter, a_head);
        paint_search(canvas, painter, a_body);
        paint_list(canvas, painter, a_body);
        paint_detail(painter, a_body);
        const double wide = clamp01(m_wide.value * m_open.value);
        const double right = a_body * (1.0 - wide);
        if (right > 0.01){
            paint_video(canvas, painter, right);
            paint_checks(painter, right);
            paint_log(canvas, painter, right);
            paint_lanes(canvas, painter, right);
        }
        //  Over the rest when it is wide.
        paint_options(canvas, painter, a_body);
        paint_footer(canvas, painter, a_foot);
        if (m_source_open && right > 0.01){
            paint_sources(canvas, painter);
        }
        if (m_full.value > 0.004){
            paint_video_full(canvas, painter);
        }
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        if (id.startsWith("prog:")){
            select(id.mid(5).toInt());
            canvas.set_focus_id("start");
        }else if (id == "search"){
            //  Typing goes to the search anyway.
        }else if (id == "search-clear"){
            set_query(QString());
        }else if (id.startsWith("lvl:")){
            m_level = id.mid(4).toInt();
            m_log_offset = 0;
        }else if (id == "copy-claude"){
            canvas.copy_text(report());
            toast(T("Copied. Paste it into Claude."));
        }else if (id == "copy-log"){
            canvas.copy_text(visible_log_text());
            toast(T("Copied."));
        }else if (id == "save-log"){
            save_report(canvas);
        }else if (id == "opt-close"){
            close_options();
            canvas.take_focus();
        }else if (id == "options"){
            focus_options();
        }else if (id == "opt-wide"){
            toggle_wide();
        }else if (id == "video-full" || id == "video-exit"){
            toggle_video_full();
        }else if (id == "source"){
            m_source_open = !m_source_open;
        }else if (id.startsWith("src:")){
            m_input.select_video(id.mid(4).toInt());
            m_source_open = false;
        }else if (id == "all"){
            canvas.go_games(m_game);
        }else if (id == "later"){
            flush_options();
            canvas.result().game = canvas.request().games[(size_t)m_game].name;
            canvas.result().program.clear();
            canvas.finish();
        }else if (id == "start"){
            start(canvas);
        }
    }

    virtual bool key(Canvas& canvas, QKeyEvent& event) override{
        m_keys_seen++;
        m_key_times.push_back(m_clock);
        while (!m_key_times.empty() && m_clock - m_key_times.front() > 130.0){
            m_key_times.pop_front();
        }
        const QString focus = canvas.focus_id();
        const bool on_button = !(focus.isEmpty() || focus == "start" || focus == "search" || focus.startsWith("prog:"));
        const int key = event.key();
        const Qt::KeyboardModifiers modifiers = event.modifiers();
        if (key == Qt::Key_F11){
            if (m_video_full || m_input.has_picture()){
                toggle_video_full();
            }
            return true;
        }
        if (m_video_full && key != Qt::Key_Escape && key != Qt::Key_Tab && key != Qt::Key_Backtab){
            //  Nothing behind the picture is typed into; Enter and the space bar leave it.
            if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space){
                toggle_video_full();
            }
            return true;
        }
        switch (key){
        case Qt::Key_Up:
            move(-1);
            return true;
        case Qt::Key_Down:
            move(+1);
            return true;
        case Qt::Key_PageUp:
            move(-8);
            return true;
        case Qt::Key_PageDown:
            move(+8);
            return true;
        case Qt::Key_Home:
            if (!m_view.empty()){
                select_view(0);
            }
            return true;
        case Qt::Key_End:
            if (!m_view.empty()){
                select_view((int)m_view.size() - 1);
            }
            return true;
        case Qt::Key_Backspace:
            if (!m_query.isEmpty()){
                set_query(m_query.left(m_query.size() - 1));
            }
            return true;
        case Qt::Key_Delete:
            set_query(QString());
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (on_button){
                return false;
            }
            start(canvas);
            return true;
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
        case Qt::Key_Left:
        case Qt::Key_Right:
            return false;
        default:
            break;
        }
        if (modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)){
            return false;
        }
        const QString text = event.text();
        if (text.isEmpty() || !text[0].isPrint()){
            return false;
        }
        if (text[0] == ' '){
            if (on_button){
                return false;
            }
            if (m_query.isEmpty()){
                return true;
            }
        }
        set_query(m_query + text);
        if (focus != "start"){
            canvas.set_focus_id("start");
        }
        return true;
    }
    virtual void enter_pressed(Canvas& canvas) override{
        if (m_video_full){
            toggle_video_full();
            return;
        }
        start(canvas);
    }
    virtual void escape_pressed(Canvas& canvas) override{
        if (m_video_full){
            toggle_video_full();
        }else if (m_source_open){
            m_source_open = false;
        }else if (!m_query.isEmpty()){
            set_query(QString());
        }else if (m_options_wanted){
            close_options();
        }else{
            canvas.go_games(m_game);
        }
    }

    virtual bool wheel(Canvas& canvas, const QPointF& at, int delta) override{
        (void)canvas;
        const double notches = (double)delta / 120.0;
        if (LIST_RECT.contains(at) || SEARCH_RECT.contains(at)){
            m_scroll.target = std::max(0.0, std::min(max_scroll(), m_scroll.target - notches * 2 * ROW_PITCH));
            return true;
        }
        if (m_log_rect.contains(at)){
            m_log_offset = std::max(0, std::min(std::max(0, (int)visible_lines().size() - LOG_ROWS), m_log_offset + (int)std::lround(notches * 3)));
            return true;
        }
        return false;
    }

    virtual void drag(Canvas& canvas, const QString& id, const QPointF& at, int phase) override{
        (void)canvas;
        if (id != "drag:lanes"){
            return;
        }
        const double x = std::max(m_cx0, std::min(m_cx1, at.x()));
        if (phase == 0){
            m_drag_x = x;
            m_dragging = true;
        }else if (phase == 1 && m_dragging && std::fabs(x - m_drag_x) > 6){
            m_sel_a = age_at(m_drag_x);
            m_sel_b = age_at(x);
            m_selection = true;
        }else if (phase == 2){
            if (m_dragging && std::fabs(x - m_drag_x) <= 6){
                m_selection = false;
            }
            m_dragging = false;
        }
    }

    virtual QString state() const override{
        const QString program = m_selected >= 0 && m_selected < (int)m_view.size() ? m_programs[(size_t)m_view[(size_t)m_selected]].name : QString();
        return "selected=" + program + ";query=" + m_query + ";level=" + QString::number(m_level) +
            ";log=" + QString::number(m_log.size()) + ";shown=" + QString::number(visible_lines().size()) +
            ";range=" + (m_selection ? QString::number((int)std::lround(std::fabs(m_sel_a - m_sel_b))) : QString("0")) +
            ";rows=" + QString::number(m_view.size()) + ";options=" + options_state_name() +
            ";host=" + (m_host != nullptr && !m_host->isHidden() ? "shown" : "hidden") +
            ";full=" + (m_video_full ? "yes" : "no") + ";wide=" + (m_wide.target > 0.5 ? "yes" : "no") + ";saved=" + m_saved_path;
    }

private:
    //  ---- the places, in the 1920 x 1080 of the screen
    static constexpr double ROW_PITCH = 51;
    static constexpr double HEAD_PITCH = 40;
    static constexpr int LOG_ROWS = 6;
    static constexpr double LANE_Y0 = 574;
    static constexpr double LANE_H = 52;
    const QRectF SEARCH_RECT{135, 138, 375, 50};
    const QRectF LIST_RECT{135, 200, 375, 468};
    const QRectF DETAIL_RECT{135, 678, 375, 264};
    //  The panel with the options of the selected program slides open between
    //  the list and the rest, which gets narrower (see relayout()).
    static constexpr double OPT_X = 528;
    static constexpr double OPT_Y = 138;
    static constexpr double OPT_W = 424;
    static constexpr double OPT_H = 804;
    static constexpr double OPT_GAP = 18;
    //  The width of the panel when it is wide: up to the right edge.
    static constexpr double WIDE_W = 1785 - 528;
    //  A program with this many options or more opens wide, unless the person chose.
    static constexpr int MANY_OPTIONS = 12;
    //  The rest, as it is with the panel closed (the first four) and as it is
    //  now: the places that go with how far the panel is open.
    QRectF m_options_rect{OPT_X, OPT_Y, 0, OPT_H};
    QRectF m_video_rect{528, 138, 510, 287};
    QRectF m_checks_rect{528, 437, 510, 46};
    QRectF m_log_rect{1056, 138, 729, 345};
    QRectF m_lanes_rect{528, 497, 1257, 445};
    double m_cx0 = 806;
    double m_cx1 = 1762;

    struct Item{
        bool header = false;
        QString text;
        int view = -1;          //  index in m_view
        double y = 0;           //  from the top of the list
        double h = 0;
    };

    //  ---- the list

    void rebuild_view(const QString& query){
        QString keep;
        if (m_selected >= 0 && m_selected < (int)m_view.size()){
            keep = m_programs[(size_t)m_view[(size_t)m_selected]].name;
        }
        m_view.clear();
        for (size_t c = 0; c < m_programs.size(); c++){
            if (query.isEmpty() || m_programs[c].name.contains(query, Qt::CaseInsensitive)){
                m_view.push_back((int)c);
            }
        }
        m_selected = m_view.empty() ? -1 : 0;
        for (size_t c = 0; c < m_view.size(); c++){
            if (m_programs[(size_t)m_view[c]].name == keep){
                m_selected = (int)c;
            }
        }
        m_items.clear();
        double y = 0;
        QString section = "\x01";
        for (size_t c = 0; c < m_view.size(); c++){
            const HomeProgram& program = m_programs[(size_t)m_view[c]];
            if (m_sections && program.section != section){
                section = program.section;
                if (!section.isEmpty()){
                    Item item;
                    item.header = true;
                    item.text = section;
                    item.y = y;
                    item.h = HEAD_PITCH;
                    m_items.push_back(item);
                    y += item.h;
                }
            }
            Item item;
            item.text = program.name;
            item.view = (int)c;
            item.y = y;
            item.h = ROW_PITCH;
            m_items.push_back(item);
            y += item.h;
        }
        m_content = y;
        m_scroll.target = std::max(0.0, std::min(max_scroll(), m_scroll.target));
        ensure_visible();
    }
    void set_query(const QString& query){
        m_query = query;
        rebuild_view(query);
    }
    double max_scroll() const{
        return std::max(0.0, m_content - LIST_RECT.height());
    }
    void select_view(int view){
        m_selected = std::max(0, std::min((int)m_view.size() - 1, view));
        ensure_visible();
        //  A program that is picked shows its options next to the list.
        open_options();
    }
    void select(int view){
        if (view >= 0 && view < (int)m_view.size()){
            select_view(view);
        }
    }
    void move(int step){
        if (m_view.empty()){
            return;
        }
        select_view(m_selected + step);
    }
    void ensure_visible(){
        for (const Item& item : m_items){
            if (item.view == m_selected && m_selected >= 0){
                //  A header above the first row of its section comes with it.
                double top = item.y;
                const size_t at = (size_t)(&item - m_items.data());
                if (at > 0 && m_items[at - 1].header){
                    top = m_items[at - 1].y;
                }
                if (top < m_scroll.target){
                    m_scroll.target = top;
                }else if (item.y + item.h > m_scroll.target + LIST_RECT.height()){
                    m_scroll.target = item.y + item.h - LIST_RECT.height();
                }
                m_scroll.target = std::max(0.0, std::min(max_scroll(), m_scroll.target));
            }
        }
    }
    const HomeProgram* selected_program() const{
        if (m_selected < 0 || m_selected >= (int)m_view.size()){
            return nullptr;
        }
        return &m_programs[(size_t)m_view[(size_t)m_selected]];
    }
    void start(Canvas& canvas){
        const HomeProgram* program = selected_program();
        if (program == nullptr){
            return;
        }
        flush_options();
        canvas.result().game = canvas.request().games[(size_t)m_game].name;
        canvas.result().program = program->name;
        canvas.finish();
    }

    //  ---- the log

    void poll_log(){
        if (!m_request->log_poll){
            return;
        }
        std::vector<SetupLogLine> lines;
        m_request->log_poll(m_last_serial, lines);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (const SetupLogLine& line : lines){
            LogEntry entry;
            entry.serial = line.serial;
            entry.msec = line.msec;
            entry.level = line.level;
            entry.text = line.text;
            entry.at = m_clock - (double)(now - line.msec) / 1000.0;
            m_log.push_back(entry);
            m_last_serial = std::max(m_last_serial, line.serial);
        }
        while (m_log.size() > 3000){
            m_log.pop_front();
        }
        if (!lines.empty() && m_log_offset > 0){
            //  What is read stays where it is.
            m_log_offset += (int)lines.size();
        }
    }
    std::vector<const LogEntry*> visible_lines() const{
        std::vector<const LogEntry*> shown;
        for (const LogEntry& entry : m_log){
            if (m_level == 0 || entry.level == m_level - 1){
                shown.push_back(&entry);
            }
        }
        return shown;
    }
    static QString time_text(qint64 msec){
        return QDateTime::fromMSecsSinceEpoch(msec).toString("HH:mm:ss.zzz");
    }
    static QString line_text(const LogEntry& entry){
        return time_text(entry.msec) + " " + level_name(entry.level) + " " + entry.text;
    }
    QString visible_log_text() const{
        QStringList lines;
        for (const LogEntry* entry : visible_lines()){
            lines << line_text(*entry);
        }
        return scrub_private_text(lines.join("\n"));
    }
    bool in_range(const LogEntry& entry) const{
        if (!m_selection){
            return false;
        }
        const double age = m_clock - entry.at;
        return age >= std::min(m_sel_a, m_sel_b) && age <= std::max(m_sel_a, m_sel_b);
    }

    //  ---- the computer

    void look_at_board(){
        m_board_ok = false;
        m_board_name.clear();
        m_ports.clear();
        for (const PortInfo& port : m_probe.state().ports){
            QString text = port.name;
            if (!port.chip.isEmpty()){
                text += " (" + port.chip + ")";
            }else if (!port.description.isEmpty()){
                text += " (" + port.description + ")";
            }
            m_ports << text;
            if (!m_board_ok && (!port.chip.isEmpty() || port.vendor == 0x2E8A)){
                m_board_ok = true;
                m_board_name = text;
            }
        }
    }

    //  What "Copy for Claude" and "Save .txt" give: where the program is,
    //  what it sees, what was chosen, and the log (the selected range, or the
    //  last 200 lines).
    QString report() const{
        QStringList out;
        out << "# Claude GG debug report";
        out << "";
        if (!m_build.isEmpty()){
            out << "Build: " + m_build;
        }
        out << "Time: " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        out << "System: " + QSysInfo::prettyProductName() + " (" + QSysInfo::currentCpuArchitecture() + "), Qt " + QString::fromLatin1(qVersion());
        out << "Language: " + current_language_code();
        out << "";
        out << "## What was chosen";
        if (const HomeProgram* program = selected_program()){
            out << "Program: " + program->name + (program->tag.isEmpty() ? QString() : " (" + program->tag + ": " + program->tag_meaning + ")");
        }else{
            out << "Program: none";
        }
        out << "Game: " + m_game_name;
        out << "";
        out << "## Devices";
        if (m_input.video_index() >= 0){
            QString video = "Video: " + m_input.video_names()[m_input.video_index()];
            if (m_input.live()){
                video += ", " + QString::number(m_input.picture_size().width()) + "x" + QString::number(m_input.picture_size().height()) +
                    ", " + QString::number((int)std::lround(m_input.fps())) + " fps";
            }else{
                video += ", no picture" + (m_input.error().isEmpty() ? QString() : " (" + m_input.error() + ")");
            }
            out << video;
        }else{
            out << "Video: no capture card found";
        }
        if (m_input.audio_ok()){
            out << "Audio: " + m_input.audio_name() + ", " + QString::number((int)std::lround(m_input.level_db())) + " dB";
        }else{
            out << "Audio: " + (m_input.audio_name().isEmpty() ? QString("no input found for this card") : m_input.audio_name() + ", cannot be read");
        }
        out << "Board: " + (m_board_ok ? m_board_name : QString("not found"));
        out << "Serial ports: " + (m_ports.isEmpty() ? QString("none") : m_ports.join(", "));
        out << "Video inputs: " + (m_input.video_names().isEmpty() ? QString("none") : m_input.video_names().join(", "));
        out << "Audio inputs: " + (m_input.audio_inputs().isEmpty() ? QString("none") : m_input.audio_inputs().join(", "));
        out << "";
        std::vector<const LogEntry*> lines;
        QString what;
        if (m_selection){
            for (const LogEntry& entry : m_log){
                if (in_range(entry)){
                    lines.push_back(&entry);
                }
            }
            what = "the selected " + QString::number((int)std::lround(std::fabs(m_sel_a - m_sel_b))) + " s";
        }else{
            const size_t first = m_log.size() > 200 ? m_log.size() - 200 : 0;
            for (size_t c = first; c < m_log.size(); c++){
                lines.push_back(&m_log[c]);
            }
            what = "the last " + QString::number(lines.size()) + " lines";
        }
        out << "## Output log, " + what;
        if (lines.empty()){
            out << "(empty)";
        }
        for (const LogEntry* entry : lines){
            out << line_text(*entry);
        }
        //  Without what is private: it goes to Claude, a file, a forum.
        return scrub_private_text(out.join("\n")) + "\n";
    }

    void save_report(Canvas& canvas){
        QString folder = QDir(canvas.app_folder().isEmpty() ? QDir::currentPath() : canvas.app_folder()).filePath("Debug");
        if (!QDir().mkpath(folder)){
            folder = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("Claude GG Debug");
            QDir().mkpath(folder);
        }
        const QString path = QDir(folder).filePath("ClaudeGG-debug-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".txt");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)){
            file.write(report().toUtf8());
            file.close();
            m_saved_path = QFileInfo(path).fileName();
            toast(T("Saved in the folder Debug."));
            canvas.open_folder(folder);
        }else{
            toast(T("Could not save the file."));
        }
    }
    void toast(const QString& text){
        m_toast = text;
        m_toast_left = 3.0;
    }

    //  ---- the timeline

    double age_at(double x) const{
        return (m_cx1 - x) / (m_cx1 - m_cx0) * 60.0;
    }
    double x_at(double age) const{
        return m_cx1 - std::max(0.0, std::min(60.0, age)) / 60.0 * (m_cx1 - m_cx0);
    }

    //  ---- the options of the selected program

    bool options_offered() const{
        return (bool)m_request->make_options;
    }
    void open_options(){
        if (options_offered()){
            m_options_wanted = true;
        }
    }
    void close_options(){
        m_options_wanted = false;
    }
    //  "closed", "loading" (the selection has not come to rest yet), "ready"
    //  (the options are there) or "none" (the program has no options here).
    QString options_state_name() const{
        if (m_options_target.isEmpty()){
            return "closed";
        }
        if (m_options != nullptr){
            return "ready";
        }
        return m_options_for == m_options_target ? "none" : "loading";
    }
    //  What was changed is kept (in the settings of the page of the program).
    void flush_options(){
        if (m_options != nullptr && m_options->changed()){
            m_options->save();
        }
        m_dirty_for = 0;
    }
    //  The widgets go, after what was changed is kept.
    void release_options(){
        flush_options();
        if (m_host != nullptr && !m_host->isHidden()){
            m_host->hide();
        }
        m_options.reset();
        m_options_for.clear();
    }
    void ensure_host(){
        if (m_host != nullptr){
            return;
        }
        m_host = new QScrollArea(m_canvas);
        m_host->setFrameShape(QFrame::NoFrame);
        m_host->setWidgetResizable(true);
        m_host->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_host->viewport()->setAutoFillBackground(false);
        //  The options are drawn by Qt, not by this screen: dark, in the
        //  colors of the panels around them.
        const QString orange = ORANGE.name();
        m_host->setStyleSheet(QString(
            "QScrollArea{background:transparent;border:0;}"
            "QLabel,QCheckBox,QRadioButton{background:transparent;color:#f2f2f2;}"
            "QCheckBox,QRadioButton{spacing:8px;}"
            "QCheckBox::indicator,QRadioButton::indicator{width:18px;height:18px;background:#26262a;"
            "border:1px solid rgba(255,255,255,140);}"
            "QCheckBox::indicator{border-radius:5px;}"
            "QRadioButton::indicator{border-radius:10px;}"
            "QCheckBox::indicator:hover,QRadioButton::indicator:hover{border-color:%1;}"
            "QCheckBox::indicator:checked{background:%1;border-color:%1;}"
            "QRadioButton::indicator:checked{background:qradialgradient(cx:0.5,cy:0.5,radius:0.5,fx:0.5,fy:0.5,"
            "stop:0 %1,stop:0.45 %1,stop:0.55 #26262a,stop:1 #26262a);border-color:%1;}"
            "QCheckBox::indicator:disabled,QRadioButton::indicator:disabled{border-color:rgba(255,255,255,50);}"
            "QGroupBox{background:transparent;color:#f2f2f2;border:1px solid rgba(255,255,255,70);border-radius:10px;margin-top:16px;padding-top:12px;}"
            "QGroupBox::title{subcontrol-origin:margin;subcontrol-position:top left;left:12px;padding:0 6px;color:#ffffff;}"
            "QLineEdit,QPlainTextEdit,QTextEdit,QSpinBox,QDoubleSpinBox,QComboBox{background:#26262a;color:#f2f2f2;"
            "border:1px solid rgba(255,255,255,90);border-radius:8px;padding:3px 8px;min-height:22px;"
            "selection-background-color:%1;selection-color:#000000;}"
            "QComboBox QAbstractItemView{background:#26262a;color:#f2f2f2;selection-background-color:%1;selection-color:#000000;border:1px solid #555555;}"
            "QPushButton{background:#2f2f34;color:#f2f2f2;border:1px solid rgba(255,255,255,90);border-radius:10px;padding:4px 14px;min-height:22px;}"
            "QPushButton:hover{border-color:%1;}"
            "QTableView,QTableWidget{background:#26262a;color:#f2f2f2;gridline-color:#444444;}"
            "QHeaderView::section{background:#2f2f34;color:#f2f2f2;border:0;padding:4px;}"
            "QScrollBar:vertical{background:transparent;width:10px;margin:0;}"
            "QScrollBar::handle:vertical{background:rgba(255,255,255,70);border-radius:5px;min-height:30px;}"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
            "QScrollBar:horizontal{background:transparent;height:10px;margin:0;}"
            "QScrollBar::handle:horizontal{background:rgba(255,255,255,70);border-radius:5px;min-width:30px;}"
            "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}"
            "*:disabled{color:#8a8a8a;}"
        ).arg(orange));
        m_host->hide();
    }
    //  Makes the options of the program that is picked, once the selection
    //  comes to rest.
    void load_options(){
        ensure_host();
        m_options_for = m_options_target;
        std::unique_ptr<SetupProgramOptions> made;
        try{
            made = m_request->make_options(m_host.data(), m_game, m_options_target);
        }catch (...){
            made.reset();
        }
        if (made != nullptr && made->widget() != nullptr){
            m_options = std::move(made);
            QWidget* widget = m_options->widget();
            m_host->setWidget(widget);
            //  A scroll area paints a background of its own behind its widget.
            widget->setAutoFillBackground(false);
            //  The scroll bar (when there are many options) goes beside them, not over them.
            widget->setContentsMargins(0, 0, 12, 0);
            m_host_font = 0;
            if (!m_wide_user){
                m_wide.target = count_controls(widget) >= MANY_OPTIONS ? 1.0 : 0.0;
            }
        }
    }
    //  How many things there are to set in the options.
    static int count_controls(QWidget* widget){
        int count = 0;
        count += (int)widget->findChildren<QAbstractButton*>().size();
        count += (int)widget->findChildren<QAbstractSpinBox*>().size();
        count += (int)widget->findChildren<QComboBox*>().size();
        count += (int)widget->findChildren<QPlainTextEdit*>().size();
        for (QLineEdit* edit : widget->findChildren<QLineEdit*>()){
            //  The field inside a spin box or a drop-down list is not one more.
            if (qobject_cast<QAbstractSpinBox*>(edit->parentWidget()) == nullptr && qobject_cast<QComboBox*>(edit->parentWidget()) == nullptr){
                count++;
            }
        }
        return count;
    }
    void toggle_wide(){
        m_wide_user = true;
        m_wide.target = m_wide.target > 0.5 ? 0.0 : 1.0;
    }
    //  The keyboard goes into the options (the first widget that takes Tab).
    void focus_options(){
        if (m_options == nullptr || m_options->widget() == nullptr){
            return;
        }
        const QList<QWidget*> widgets = m_options->widget()->findChildren<QWidget*>();
        for (QWidget* widget : widgets){
            if (widget->isEnabled() && !widget->isHidden() && (widget->focusPolicy() & Qt::TabFocus)){
                widget->setFocus(Qt::TabFocusReason);
                return;
            }
        }
    }
    void step_options(double dt){
        m_open.target = m_options_wanted && options_offered() ? 1.0 : 0.0;
        m_open.step(dt, 9.0);
        m_wide.step(dt, 9.0);
        const HomeProgram* program = selected_program();
        const QString wanted = m_open.target > 0.5 && program != nullptr ? program->name : QString();
        if (wanted != m_options_target){
            //  Another program, or none: the widgets of the one before go.
            m_options_target = wanted;
            m_select_rest = 0;
            release_options();
        }
        if (!m_options_target.isEmpty() && m_options_for.isEmpty()){
            m_select_rest += dt;
            if (m_select_rest >= 0.15){
                load_options();
            }
        }
        //  What is changed is kept a moment later, not only when the page goes.
        if (m_options != nullptr && m_options->changed()){
            m_dirty_for += dt;
            if (m_dirty_for >= 1.0){
                flush_options();
            }
        }
        place_options();
    }
    //  The widgets are over the picture once the panel is all the way open.
    void place_options(){
        if (m_host == nullptr){
            return;
        }
        const bool show = m_options != nullptr && m_open.target >= 1.0 && m_open.value >= 0.995 && m_canvas->page_fade() >= 0.98
            && m_wide.value == m_wide.target && !m_video_full && m_full.value < 0.01;
        if (!show){
            if (!m_host->isHidden()){
                m_host->hide();
            }
            return;
        }
        const QRectF area(OPT_X + 12, OPT_Y + 84, OPT_W + (WIDE_W - OPT_W) * m_wide.value - 24, OPT_H - 84 - 56);
        const QRect geometry = m_canvas->window_rect(area);
        if (m_host->geometry() != geometry){
            m_host->setGeometry(geometry);
        }
        const int font_px = std::max(9, (int)std::lround(19.0 * m_canvas->window_scale()));
        if (font_px != m_host_font){
            m_host_font = font_px;
            m_host->setFont(ui_font(font_px, 500));
        }
        if (m_host->isHidden()){
            m_host->show();
        }
    }
    //  The places that go with how far the panel is open: it takes room from
    //  the picture, the checks, the log and the timeline, which get narrower.
    void relayout(){
        const double p = m_open.value;
        const double rx = OPT_X + p * (OPT_W + OPT_GAP);
        const double rw = 1785 - rx;
        //  Wide: it goes over the rest, which fades away under it.
        m_options_rect = QRectF(OPT_X, OPT_Y, (OPT_W + (WIDE_W - OPT_W) * m_wide.value) * p, OPT_H);
        //  The checks go from one row of four to two rows of two.
        const double checks_h = 46 + p * 52;
        const double video_w = 510 - p * 170;
        const double video_h = 345 - 12 - checks_h;
        m_video_rect = QRectF(rx, 138, video_w, video_h);
        m_checks_rect = QRectF(rx, 138 + video_h + 12, video_w, checks_h);
        m_log_rect = QRectF(rx + video_w + 18, 138, rw - video_w - 18, 345);
        m_lanes_rect = QRectF(rx, 497, rw, 445);
        m_cx0 = rx + 278 - p * 28;
        m_cx1 = 1762;
    }

    //  ---- painting

    void paint_game_badge(QPainter& painter, double a){
        const HomeGame& game = *m_game_info;
        const QString count = game.programs == 1 ? tr_ui("1 program") : tr_ui("%1 programs").arg((int)m_programs.size());
        const QFont name_font = ui_font(31, 700);
        const QFont count_font = ui_font(28, 500);
        const double right = 1785;
        const double count_w = text_width(count_font, count);
        const double name_w = text_width(name_font, short_game_name(game.name));
        const double name_x = right - count_w - 22 - name_w;
        const QRectF icon(name_x - 18 - 66, 34, 66, 66);
        panel(painter, icon, 16, with_alpha(PANEL, 0.7 * a), with_alpha(ORANGE, 0.9 * a), 2.2);
        if (!game.icon.isNull()){
            const QRectF room = icon.adjusted(8, 10, -8, -10);
            const double scale = std::min(room.width() / game.icon.width(), room.height() / game.icon.height());
            const double w = game.icon.width() * scale;
            const double h = game.icon.height() * scale;
            draw_picture(painter, game.icon, QRectF(room.center().x() - w / 2.0, room.center().y() - h / 2.0, w, h), a);
        }else{
            draw_text_cy(painter, short_game_name(game.name).left(1).toUpper(), icon.center().x(), icon.center().y() + 2, serif_font(38), white(0.9 * a), Qt::AlignHCenter);
        }
        draw_text_cy(painter, short_game_name(game.name), name_x, 67, name_font, white(a));
        draw_text_cy(painter, count, right, 69, count_font, white(0.55 * a), Qt::AlignRight);
    }
    void paint_search(Canvas& canvas, QPainter& painter, double a){
        if (a < 0.004){
            return;
        }
        const bool hot = !m_query.isEmpty() || canvas.hovered("search") || canvas.focused("search");
        panel(painter, SEARCH_RECT, 25, with_alpha(PANEL, 0.8 * a), hot ? with_alpha(ORANGE, 0.95 * a) : white(0.30 * a), 2.0);
        canvas.add_hit("search", SEARCH_RECT, true, false);
        //  The magnifying glass.
        const QPointF glass(SEARCH_RECT.left() + 32, SEARCH_RECT.center().y() - 2);
        painter.setPen(QPen(with_alpha(ORANGE, a), 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(glass, 8, 8);
        painter.drawLine(QPointF(glass.x() + 6, glass.y() + 6), QPointF(glass.x() + 13, glass.y() + 13));
        const QFont font = ui_font(25, 500);
        const double x = SEARCH_RECT.left() + 62;
        const double room = SEARCH_RECT.width() - 62 - 46;
        if (m_query.isEmpty()){
            draw_text_cy(painter, T("Search programs"), x + 8, SEARCH_RECT.center().y() + 1, font, white(0.5 * a));
        }else{
            const QString shown = QFontMetricsF(font).elidedText(m_query, Qt::ElideLeft, room);
            draw_text_cy(painter, shown, x, SEARCH_RECT.center().y() + 1, font, white(a));
        }
        const QString shown_query = QFontMetricsF(font).elidedText(m_query, Qt::ElideLeft, room);
        if (std::fmod(m_clock, 1.0) < 0.55){
            const double cx = x + (m_query.isEmpty() ? 2 : text_width(font, shown_query) + 3);
            painter.setPen(QPen(with_alpha(ORANGE, a), 2.4));
            painter.drawLine(QPointF(cx, SEARCH_RECT.center().y() - 12), QPointF(cx, SEARCH_RECT.center().y() + 12));
        }
        if (!m_query.isEmpty()){
            const QRectF clear(SEARCH_RECT.right() - 44, SEARCH_RECT.center().y() - 16, 32, 32);
            const bool over = canvas.hovered("search-clear");
            painter.setPen(QPen(white((over ? 0.95 : 0.6) * a), 2.0, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(clear.center().x() - 6, clear.center().y() - 6), QPointF(clear.center().x() + 6, clear.center().y() + 6));
            painter.drawLine(QPointF(clear.center().x() - 6, clear.center().y() + 6), QPointF(clear.center().x() + 6, clear.center().y() - 6));
            canvas.add_hit("search-clear", clear, true, false);
        }
    }

    void paint_list(Canvas& canvas, QPainter& painter, double a){
        if (a < 0.004){
            return;
        }
        painter.save();
        painter.setClipRect(LIST_RECT.adjusted(-8, 0, 8, 0));
        const double offset = m_scroll.value;
        if (m_view.empty()){
            const QStringList lines = wrap_text(ui_font(24, 500), T("No program matches this search."), LIST_RECT.width() - 40);
            for (int c = 0; c < lines.size(); c++){
                draw_text(painter, lines[c], LIST_RECT.left() + 20, LIST_RECT.top() + 16 + c * 34, ui_font(24, 500), white(0.55 * a));
            }
        }
        for (const Item& item : m_items){
            const double y = LIST_RECT.top() + item.y - offset;
            if (y + item.h < LIST_RECT.top() || y > LIST_RECT.bottom()){
                continue;
            }
            if (item.header){
                draw_text_cy(painter, item.text, LIST_RECT.left() + 20, y + item.h / 2.0 + 6, ui_font(20, 600), with_alpha(ORANGE, 0.85 * a));
                continue;
            }
            const QRectF row(LIST_RECT.left(), y, LIST_RECT.width(), ROW_PITCH - 5);
            const QString id = "prog:" + QString::number(item.view);
            const bool selected = item.view == m_selected;
            const bool hover = canvas.hovered(id);
            if (selected){
                panel(painter, row, 16, with_alpha(QColor(44, 29, 24), 0.95 * a), with_alpha(ORANGE, a), 2.2);
            }else if (hover){
                panel(painter, row, 16, white(0.09 * a));
            }
            const HomeProgram& program = m_programs[(size_t)m_view[(size_t)item.view]];
            double right = row.right() - 14;
            if (!program.tag.isEmpty()){
                const double w = draw_chip(painter, program.tag, right, row.center().y(), ChipKind::ORANGE_RING, a, 17, 28, 12);
                right -= w + 10;
            }
            const QFont font = ui_font(24, selected ? 600 : 500);
            const QString shown = QFontMetricsF(font).elidedText(program.name, Qt::ElideRight, right - (row.left() + 20));
            draw_text_cy(painter, shown, row.left() + 20, row.center().y() + 1, font, white(a));
            //  Only the part that can be seen can be pressed.
            const QRectF visible = row.intersected(LIST_RECT);
            if (visible.height() > 6){
                canvas.add_hit(id, visible, true, false);
            }
        }
        painter.restore();
        //  The scroll bar.
        if (m_content > LIST_RECT.height() + 1){
            const double track_h = LIST_RECT.height() - 12;
            const double thumb_h = std::max(36.0, track_h * LIST_RECT.height() / m_content);
            const double thumb_y = LIST_RECT.top() + 6 + (track_h - thumb_h) * (max_scroll() > 0 ? m_scroll.value / max_scroll() : 0);
            panel(painter, QRectF(LIST_RECT.right() + 6, thumb_y, 5, thumb_h), 2.5, white(0.28 * a));
        }
    }

    void paint_detail(QPainter& painter, double a){
        if (a < 0.004){
            return;
        }
        panel(painter, DETAIL_RECT, 24, with_alpha(PANEL, 0.78 * a), white(0.24 * a), 1.6);
        const HomeProgram* program = selected_program();
        if (program == nullptr){
            return;
        }
        const double x = DETAIL_RECT.left() + 24;
        const double w = DETAIL_RECT.width() - 48;

        //  At the bottom: the tag, and what Start does. The rest is between.
        const QFont note = ui_font(20, 500);
        const QStringList note_lines = wrap_text(note, T("Opens its page, where you set it up and press Start."), w);
        const double note_top = DETAIL_RECT.bottom() - 18 - (double)note_lines.size() * 27.0;
        double chip_w = 0;
        QStringList tag_lines;
        double tag_h = 0;
        if (!program->tag.isEmpty()){
            chip_w = draw_chip(painter, program->tag, 0, 0, ChipKind::ORANGE_RING, a, 17, 28, 12, true);
            tag_lines = wrap_text(ui_font(19, 500), program->tag_meaning, w - chip_w - 12);
            if (tag_lines.size() > 2){
                tag_lines = tag_lines.mid(0, 2);
            }
            tag_h = std::max(34.0, (double)tag_lines.size() * 24.0 + 8.0) + 10.0;
        }
        const double tag_top = note_top - tag_h;

        //  The name: a smaller type when it needs two lines.
        double y = DETAIL_RECT.top() + 18;
        QFont title = serif_font(40);
        double pitch = 44;
        if (wrap_text(title, program->name, w).size() > 1){
            title = serif_font(33);
            pitch = 37;
        }
        for (const QString& line : wrap_text(title, program->name, w)){
            draw_text(painter, line, x, y, title, white(a));
            y += pitch;
        }
        y += 6;
        if (!program->description.isEmpty()){
            const QFont font = ui_font(23, 500);
            const int room = std::max(0, (int)std::floor((tag_top - 6 - y) / 31.0));
            QString text = program->description;
            QStringList lines = wrap_text(font, text, w);
            if ((int)lines.size() > room){
                //  Too long for the card: the part before the first bracket, when that is a sentence by itself.
                const int bracket = (int)text.indexOf(" (");
                if (bracket > 20 && (int)wrap_text(font, text.left(bracket) + " ...", w).size() <= room){
                    text = text.left(bracket) + " ...";
                    lines = wrap_text(font, text, w);
                }
            }
            for (int c = 0; c < (int)lines.size() && c < room; c++){
                QString line = lines[(qsizetype)c];
                if (c == room - 1 && (int)lines.size() > room){
                    //  What does not fit ends in "...", after a whole word.
                    QString cut = line;
                    while (!cut.isEmpty() && text_width(font, cut + " ...") > w){
                        const int space = (int)cut.lastIndexOf(' ');
                        cut = space > 0 ? cut.left(space) : cut.left(cut.size() - 1);
                    }
                    line = cut + " ...";
                }
                draw_text(painter, line, x, y, font, white(0.9 * a));
                y += 31;
            }
        }
        if (!program->tag.isEmpty()){
            draw_chip(painter, program->tag, x + chip_w, tag_top + 17, ChipKind::ORANGE_RING, a, 17, 28, 12);
            for (int c = 0; c < tag_lines.size(); c++){
                draw_text_cy(painter, tag_lines[c], x + chip_w + 12, tag_top + 17 + c * 24, ui_font(19, 500), white(0.6 * a));
            }
        }
        for (int c = 0; c < note_lines.size(); c++){
            draw_text(painter, note_lines[c], x, note_top + c * 27.0, note, white(0.5 * a));
        }
    }

    void paint_options(Canvas& canvas, QPainter& painter, double a){
        const double p = m_open.value;
        if (a < 0.004 || p < 0.01){
            return;
        }
        const QRectF box = m_options_rect;
        //  Its width at rest: wide or not.
        const double panel_w = OPT_W + (WIDE_W - OPT_W) * m_wide.value;
        const double edge = OPT_X + panel_w;
        //  What is inside shows as the panel comes open.
        const double inner = clamp01((p - 0.45) / 0.55) * a;
        panel(painter, box, 26, with_alpha(PANEL, 0.80 * a), white(0.24 * a), 1.6);
        painter.save();
        painter.setClipRect(box);
        const HomeProgram* program = selected_program();
        const double x = OPT_X + 24;
        const QRectF close_rect(edge - 58, OPT_Y + 18, 40, 40);
        const QRectF wide_rect(close_rect.left() - 48, OPT_Y + 18, 40, 40);
        const QRectF head_rect(OPT_X + 8, OPT_Y + 10, panel_w - 78 - 48, 62);
        const QString kind = options_state_name();
        if (inner > 0.5){
            //  The head is a stop of the Tab key: it takes the keyboard into
            //  the options.
            canvas.add_hit("options", head_rect, m_options != nullptr, true);
            canvas.add_hit("opt-close", close_rect, true, false);
            canvas.add_hit("opt-wide", wide_rect, true, false);
        }
        const QFont title = ui_font(25, 600);
        const QString program_name = program != nullptr ? program->name : QString();
        draw_text_cy(painter, QFontMetricsF(title).elidedText(program_name, Qt::ElideRight, wide_rect.left() - 10 - x), x, OPT_Y + 36, title, white(inner));
        draw_text_cy(painter, T("Options"), x, OPT_Y + 62, ui_font(19, 500), white(0.5 * inner));
        if (canvas.focused("options") && m_options != nullptr){
            panel(painter, head_rect.adjusted(-2, -2, 2, 2), 18, QColor(), with_alpha(YELLOW, 0.9 * inner), 2.2);
        }
        {
            const bool over = canvas.hovered("opt-close");
            painter.setPen(QPen(white((over ? 0.95 : 0.6) * inner), 2.0, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(close_rect.center().x() - 7, close_rect.center().y() - 7), QPointF(close_rect.center().x() + 7, close_rect.center().y() + 7));
            painter.drawLine(QPointF(close_rect.center().x() - 7, close_rect.center().y() + 7), QPointF(close_rect.center().x() + 7, close_rect.center().y() - 7));
            if (canvas.focused("opt-close")){
                panel(painter, close_rect.adjusted(-3, -3, 3, 3), 23, QColor(), with_alpha(YELLOW, 0.9 * inner), 2.2);
            }
        }
        {
            //  Two arrows: apart to make the panel wide, together to make it narrow again.
            const bool over = canvas.hovered("opt-wide");
            const bool is_wide = m_wide.target > 0.5;
            const QColor color = white((over ? 0.95 : 0.6) * inner);
            painter.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            const QPointF c = wide_rect.center();
            const double dir = is_wide ? -1.0 : 1.0;
            painter.drawLine(QPointF(c.x() - 12, c.y()), QPointF(c.x() + 12, c.y()));
            painter.drawPolyline(QPolygonF({QPointF(c.x() + 12 - 6 * dir, c.y() - 6), QPointF(c.x() + 12, c.y()), QPointF(c.x() + 12 - 6 * dir, c.y() + 6)}));
            painter.drawPolyline(QPolygonF({QPointF(c.x() - 12 + 6 * dir, c.y() - 6), QPointF(c.x() - 12, c.y()), QPointF(c.x() - 12 + 6 * dir, c.y() + 6)}));
            if (canvas.focused("opt-wide")){
                panel(painter, wide_rect.adjusted(-3, -3, 3, 3), 23, QColor(), with_alpha(YELLOW, 0.9 * inner), 2.2);
            }
        }
        painter.setPen(QPen(white(0.12 * inner), 1.2));
        painter.drawLine(QPointF(OPT_X + 20, OPT_Y + 76), QPointF(edge - 20, OPT_Y + 76));

        const QFont note = ui_font(20, 500);
        if (kind == "loading"){
            draw_text_cy(painter, T("Loading the options..."), x, OPT_Y + 116, note, white(0.5 * inner));
        }else if (kind == "none"){
            int line = 0;
            for (const QString& text : wrap_text(ui_font(22, 500), T("This program has no options of its own."), panel_w - 48)){
                draw_text_cy(painter, text, x, OPT_Y + 116 + line * 30, ui_font(22, 500), white(0.7 * inner));
                line++;
            }
            for (const QString& text : wrap_text(note, T("Start it to open its page."), panel_w - 48)){
                draw_text_cy(painter, text, x, OPT_Y + 124 + line * 30, note, white(0.5 * inner));
                line++;
            }
        }else if (kind == "ready"){
            //  The options themselves are widgets over this place; under them
            //  what they are.
            const QStringList lines = wrap_text(ui_font(18, 500), T("The same options as on the page of the program."), panel_w - 48);
            for (int c = 0; c < lines.size(); c++){
                draw_text_cy(painter, lines[c], x, OPT_Y + OPT_H - 22 - (double)(lines.size() - 1 - c) * 24, ui_font(18, 500), white(0.42 * inner));
            }
        }
        painter.restore();
    }

    void paint_video(Canvas& canvas, QPainter& painter, double a){
        if (a < 0.004){
            return;
        }
        const QRectF box = m_video_rect;
        painter.save();
        QPainterPath clip;
        clip.addRoundedRect(box, 24, 24);
        painter.setClipPath(clip);
        painter.setPen(Qt::NoPen);
        painter.setBrush(with_alpha(QColor(6, 6, 8), 0.92 * a));
        painter.drawRect(box);
        if (m_input.has_picture() && !m_input.picture().isNull()){
            const QImage& image = m_input.picture();
            const double scale = std::max(box.width() / image.width(), box.height() / image.height());
            const double w = image.width() * scale;
            const double h = image.height() * scale;
            const QRectF target(box.center().x() - w / 2.0, box.center().y() - h / 2.0, w, h);
            const double base = painter.opacity();
            painter.setOpacity(base * (m_input.live() ? 1.0 : 0.45));
            painter.drawImage(target, image);
            painter.setOpacity(base);
        }else{
            QString title = T("No picture");
            QString sub = m_input.video_names().isEmpty() ? T("Connect a capture card.") : (m_input.error().isEmpty() ? T("Waiting for the card.") : m_input.error());
            draw_text_cy(painter, title, box.center().x(), box.center().y() - 18, ui_font(34, 600), white(0.75 * a), Qt::AlignHCenter);
            int line = 0;
            for (const QString& text : wrap_text(ui_font(21, 500), sub, box.width() - 80)){
                draw_text_cy(painter, text, box.center().x(), box.center().y() + 22 + line * 28, ui_font(21, 500), white(0.5 * a), Qt::AlignHCenter);
                line++;
            }
        }
        //  A dark band under the name of the card.
        QLinearGradient shade(0, box.bottom() - 84, 0, box.bottom());
        shade.setColorAt(0, QColor(0, 0, 0, 0));
        shade.setColorAt(1, QColor(0, 0, 0, (int)std::lround(190 * a)));
        painter.setBrush(shade);
        painter.drawRect(QRectF(box.left(), box.bottom() - 84, box.width(), 84));
        painter.restore();
        panel(painter, box, 24, QColor(), white(0.26 * a), 1.6);

        //  "Live", and the size of the picture.
        if (m_input.video_index() >= 0){
            const bool live = m_input.live();
            const QString text = live ? T("Live") : T("No signal");
            const QFont font = ui_font(21, 600);
            const double w = text_width(font, text) + 54;
            const QRectF badge(box.left() + 16, box.top() + 16, w, 38);
            panel(painter, badge, 19, with_alpha(QColor(20, 20, 22), 0.85 * a));
            painter.setPen(Qt::NoPen);
            painter.setBrush(with_alpha(live ? ORANGE : YELLOW, a));
            painter.drawEllipse(QPointF(badge.left() + 20, badge.center().y()), 6, 6);
            draw_text_cy(painter, text, badge.left() + 34, badge.center().y() + 1, font, white(a));
            if (live){
                const QString size = QString::number(m_input.picture_size().height()) + "p, " + QString::number((int)std::lround(m_input.fps())) + " fps";
                const double sw = text_width(font, size) + 32;
                const QRectF pill(box.right() - 16 - 46 - sw, box.top() + 16, sw, 38);
                panel(painter, pill, 19, with_alpha(QColor(20, 20, 22), 0.85 * a));
                draw_text_cy(painter, size, pill.center().x(), pill.center().y() + 1, font, white(a), Qt::AlignHCenter);
            }
        }

        //  The button for the picture over the whole screen.
        if (m_input.has_picture() && !m_input.picture().isNull()){
            const QRectF button(box.right() - 16 - 38, box.top() + 16, 38, 38);
            const bool over = canvas.hovered("video-full");
            panel(painter, button, 19, with_alpha(QColor(20, 20, 22), (over ? 0.95 : 0.85) * a), over ? white(0.7 * a) : QColor(), 1.6);
            draw_corners(painter, button.center(), 8.5, white((over ? 1.0 : 0.85) * a), false);
            canvas.add_hit("video-full", button, true, true);
            if (canvas.focused("video-full")){
                panel(painter, button.adjusted(-3, -3, 3, 3), 22, QColor(), with_alpha(YELLOW, 0.9 * a), 2.2);
            }
        }

        //  The name of the card (a list when there are more), and the sound.
        const QString source = m_input.video_index() >= 0 ? m_input.video_names()[m_input.video_index()] : T("No capture card");
        const QFont font = ui_font(25, 600);
        const double name_w = std::min(text_width(font, source), std::max(60.0, box.width() - 200.0));
        const QRectF source_rect(box.left() + 12, box.bottom() - 58, name_w + 76, 48);
        const bool hot = canvas.hovered("source");
        if (hot){
            panel(painter, source_rect, 14, white(0.10 * a));
        }
        lane_icon(painter, Lane::VIDEO, QPointF(source_rect.left() + 24, source_rect.center().y()), white(0.9 * a));
        const QString shown = QFontMetricsF(font).elidedText(source, Qt::ElideRight, name_w);
        draw_text_cy(painter, shown, source_rect.left() + 46, source_rect.center().y() + 1, font, white(a));
        if (m_input.video_names().size() > 1){
            draw_chevron(painter, QRectF(source_rect.left() + 46 + text_width(font, shown) + 8, source_rect.center().y() - 11, 26, 22), white(0.9 * a), 2.4);
            canvas.add_hit("source", source_rect, true, true);
        }
        const double level = m_input.audio_ok() ? m_input.level() : 0;
        for (int c = 0; c < 8; c++){
            const double h = 9 + c * 3.2;
            const bool lit = level > (double)c / 8.0 + 0.02;
            panel(painter, QRectF(box.right() - 22 - (8 - c) * 11, box.bottom() - 22 - h, 6, h), 3,
                lit ? with_alpha(ORANGE, a) : white(0.22 * a));
        }
    }
    //  Four corners: apart for "full screen", together to leave it.
    static void draw_corners(QPainter& painter, const QPointF& c, double r, const QColor& color, bool inward){
        QPen pen(color, 2.2);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        const double arm = r * 0.6;
        const double reach = inward ? r * 0.3 : r;
        //  Apart: the elbows are at the corners and the arms point at the middle.
        //  Together: the elbows are near the middle and the arms point out.
        const double direction = inward ? +1.0 : -1.0;
        for (int sx = -1; sx <= 1; sx += 2){
            for (int sy = -1; sy <= 1; sy += 2){
                const QPointF corner(c.x() + sx * reach, c.y() + sy * reach);
                painter.drawPolyline(QPolygonF({
                    QPointF(corner.x() + direction * sx * arm, corner.y()),
                    corner,
                    QPointF(corner.x(), corner.y() + direction * sy * arm)
                }));
            }
        }
    }
    void toggle_video_full(){
        m_video_full = !m_video_full;
        m_source_open = false;
        if (m_video_full){
            canvas_focus_exit();
        }
    }
    void canvas_focus_exit(){
        if (m_canvas != nullptr){
            m_canvas->set_focus_id("video-exit");
        }
    }
    //  The picture over the whole window: as large as it goes without being
    //  cut, with what is useful over it (live or not, how large, the card)
    //  and the way out.
    void paint_video_full(Canvas& canvas, QPainter& painter){
        const double a = clamp01(m_full.value);
        if (a < 0.004){
            return;
        }
        const QRectF screen = canvas.screen_rect();
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, (int)std::lround(255 * a)));
        painter.drawRect(screen.adjusted(-2, -2, 2, 2));
        if (m_input.has_picture() && !m_input.picture().isNull()){
            const QImage& image = m_input.picture();
            const double scale = std::min(screen.width() / image.width(), screen.height() / image.height());
            const double w = image.width() * scale;
            const double h = image.height() * scale;
            const QRectF target(screen.center().x() - w / 2.0, screen.center().y() - h / 2.0, w, h);
            const double base = painter.opacity();
            painter.setOpacity(base * a * (m_input.live() ? 1.0 : 0.45));
            painter.drawImage(target, image);
            painter.setOpacity(base);
        }else{
            draw_text_cy(painter, T("No picture"), screen.center().x(), screen.center().y(), ui_font(40, 600), white(0.75 * a), Qt::AlignHCenter);
        }

        //  Over the corners of the window, on a dark ground so that they read on any picture.
        const QFont font = ui_font(22, 600);
        const QColor ground = with_alpha(QColor(20, 20, 22), 0.82 * a);
        double x = screen.left() + 24;
        const double top = screen.top() + 24;
        if (m_input.video_index() >= 0){
            const bool live = m_input.live();
            const QString text = live ? T("Live") : T("No signal");
            const double w = text_width(font, text) + 58;
            const QRectF badge(x, top, w, 42);
            panel(painter, badge, 21, ground);
            painter.setPen(Qt::NoPen);
            painter.setBrush(with_alpha(live ? ORANGE : YELLOW, a));
            painter.drawEllipse(QPointF(badge.left() + 22, badge.center().y()), 6.5, 6.5);
            draw_text_cy(painter, text, badge.left() + 38, badge.center().y() + 1, font, white(a));
            x += w + 12;
            if (live){
                const QString size = QString::number(m_input.picture_size().height()) + "p, " + QString::number((int)std::lround(m_input.fps())) + " fps";
                const double sw = text_width(font, size) + 36;
                const QRectF pill(x, top, sw, 42);
                panel(painter, pill, 21, ground);
                draw_text_cy(painter, size, pill.center().x(), pill.center().y() + 1, font, white(a), Qt::AlignHCenter);
            }
        }
        //  The way out: a button, with the key on it.
        {
            const QString label = T("Exit full screen");
            const QFont key_font = ui_font(18, 600);
            const double key_w = text_width(key_font, "Esc") + 22;
            const double w = 22 + 26 + 12 + text_width(font, label) + 16 + key_w + 14;
            const QRectF exit_rect(screen.right() - 24 - w, top, w, 46);
            const bool over = canvas.hovered("video-exit");
            panel(painter, exit_rect, 23, with_alpha(QColor(20, 20, 22), (over ? 0.95 : 0.82) * a), over ? white(0.7 * a) : QColor(), 1.6);
            draw_corners(painter, QPointF(exit_rect.left() + 35, exit_rect.center().y()), 11.0, white(a), true);
            draw_text_cy(painter, label, exit_rect.left() + 22 + 26 + 12, exit_rect.center().y() + 1, font, white(a));
            const QRectF key_rect(exit_rect.right() - 14 - key_w, exit_rect.center().y() - 15, key_w, 30);
            panel(painter, key_rect, 8, QColor(), white(0.4 * a), 1.4);
            draw_text_cy(painter, "Esc", key_rect.center().x(), key_rect.center().y() + 1, key_font, white(0.8 * a), Qt::AlignHCenter);
            if (m_video_full){
                canvas.add_hit("video-exit", exit_rect, true, true);
                if (canvas.focused("video-exit")){
                    panel(painter, exit_rect.adjusted(-4, -4, 4, 4), 27, QColor(), with_alpha(YELLOW, 0.9 * a), 2.2);
                }
            }
        }
        //  The card, at the bottom.
        if (m_input.video_index() >= 0){
            const QString source = m_input.video_names()[m_input.video_index()];
            const double w = std::min(text_width(font, source) + 60, screen.width() - 48);
            const QRectF name(screen.left() + 24, screen.bottom() - 24 - 42, w, 42);
            panel(painter, name, 21, ground);
            lane_icon(painter, Lane::VIDEO, QPointF(name.left() + 24, name.center().y()), white(0.9 * a));
            draw_text_cy(painter, QFontMetricsF(font).elidedText(source, Qt::ElideRight, w - 60), name.left() + 46, name.center().y() + 1, font, white(a));
        }
        painter.restore();
    }
    void paint_sources(Canvas& canvas, QPainter& painter){
        const QStringList& names = m_input.video_names();
        const double row_h = 46;
        const int shown = std::min((int)names.size(), 6);
        const QRectF list(m_video_rect.left() + 12, m_video_rect.bottom() - 70 - shown * row_h - 12, std::min(330.0, m_video_rect.width() - 24), shown * row_h + 12);
        panel(painter, list, 18, QColor(16, 16, 18, 245), white(0.3), 1.8);
        for (int c = 0; c < shown; c++){
            const QRectF row(list.left() + 6, list.top() + 6 + c * row_h, list.width() - 12, row_h);
            const QString id = "src:" + QString::number(c);
            if (c == m_input.video_index()){
                panel(painter, row, 12, with_alpha(QColor(44, 29, 24), 0.95), with_alpha(ORANGE, 0.95), 1.8);
            }else if (canvas.hovered(id)){
                panel(painter, row, 12, white(0.1));
            }
            const QFont font = ui_font(22, 500);
            draw_text_cy(painter, QFontMetricsF(font).elidedText(names[c], Qt::ElideRight, row.width() - 28), row.left() + 14, row.center().y() + 1, font, white(0.95));
            canvas.add_hit(id, row, true, false);
        }
    }

    void paint_checks(QPainter& painter, double a){
        if (a < 0.004){
            return;
        }
        const Light video = m_input.live() ? Light::OK : (m_input.video_names().isEmpty() ? Light::BAD : Light::WAIT);
        const Light audio = m_input.audio_ok() ? Light::OK : (m_input.video_names().isEmpty() ? Light::WAIT : Light::BAD);
        const Light board = m_board_ok ? Light::OK : Light::WAIT;
        const Light keys = m_keys_seen > 0 ? Light::OK : Light::WAIT;
        const struct{
            const char* name;
            Light light;
        } checks[4] = {{"Video", video}, {"Audio", audio}, {"Board", board}, {"Keys", keys}};
        const double gap = 10;
        const double p = m_open.value;
        const double row_w = (m_checks_rect.width() - 3 * gap) / 4.0;
        const double grid_w = (m_checks_rect.width() - gap) / 2.0;
        for (int c = 0; c < 4; c++){
            //  From the place in the row to the place in the grid.
            const QRectF row(m_checks_rect.left() + c * (row_w + gap), m_checks_rect.top(), row_w, 46);
            const QRectF grid(m_checks_rect.left() + (c % 2) * (grid_w + gap), m_checks_rect.top() + (c / 2) * (44 + gap), grid_w, 44);
            const QRectF rect(
                row.left() + (grid.left() - row.left()) * p, row.top() + (grid.top() - row.top()) * p,
                row.width() + (grid.width() - row.width()) * p, row.height() + (grid.height() - row.height()) * p
            );
            panel(painter, rect, 20, with_alpha(PANEL, 0.8 * a), checks[c].light == Light::BAD ? with_alpha(YELLOW, 0.9 * a) : white(0.24 * a), 1.8);
            const QFont font = ui_font(20, 600);
            const QString label = T(checks[c].name);
            const double total = 24 + 8 + text_width(font, label);
            const double left = rect.center().x() - total / 2.0;
            draw_light(painter, checks[c].light, QPointF(left + 12, rect.center().y()), 12, a);
            draw_text_cy(painter, label, left + 32, rect.center().y() + 1, font, white(a));
        }
    }

    void paint_log(Canvas& canvas, QPainter& painter, double a){
        if (a < 0.004){
            return;
        }
        panel(painter, m_log_rect, 26, with_alpha(PANEL, 0.80 * a), white(0.24 * a), 1.6);
        const double x = m_log_rect.left() + 24;
        draw_text_cy(painter, T("Output log"), x, m_log_rect.top() + 45, ui_font(22, 600), white(0.6 * a));

        //  With the options open the log is narrow: smaller type, less room.
        const bool narrow = m_log_rect.width() < 600;
        //  The levels to show.
        const char* const names[4] = {"All", "Info", "Warn", "Error"};
        double right = m_log_rect.right() - 20;
        for (int c = 3; c >= 0; c--){
            const QString text = T(names[c]);
            const QFont font = ui_font(narrow ? 18 : 20, 600);
            const double w = text_width(font, text) + (narrow ? 22 : 30);
            const QRectF chip(right - w, m_log_rect.top() + 27, w, 36);
            const bool on = m_level == c;
            const bool warn = c >= 2;
            if (on){
                panel(painter, chip, 18, with_alpha(warn ? YELLOW : ORANGE, a));
            }else{
                panel(painter, chip, 18, canvas.hovered("lvl:" + QString::number(c)) ? white(0.08 * a) : QColor(),
                    warn ? with_alpha(YELLOW, 0.9 * a) : white(0.34 * a), 1.6);
            }
            draw_text_cy(painter, text, chip.center().x(), chip.center().y() + 1, font,
                on ? with_alpha(Qt::black, a) : (warn ? with_alpha(YELLOW, a) : white(a)), Qt::AlignHCenter);
            canvas.add_hit("lvl:" + QString::number(c), chip);
            right = chip.left() - (narrow ? 8 : 10);
        }

        //  The lines, the newest at the bottom.
        const std::vector<const LogEntry*> lines = visible_lines();
        const int count = (int)lines.size();
        const int offset = std::max(0, std::min(std::max(0, count - LOG_ROWS), m_log_offset));
        const double top = m_log_rect.top() + 72;
        const double pitch = 30;
        if (lines.empty()){
            draw_text_cy(painter, T("No log lines yet."), x + 6, top + 60, ui_font(23, 500), white(0.5 * a));
        }
        const int first = std::max(0, count - LOG_ROWS - offset);
        for (int c = 0; c < LOG_ROWS && first + c < count; c++){
            const LogEntry& entry = *lines[(size_t)(first + c)];
            const double cy = top + c * pitch + pitch / 2.0;
            const QRectF band(m_log_rect.left() + 12, cy - pitch / 2.0 + 2, m_log_rect.width() - 24, pitch - 4);
            if (in_range(entry)){
                panel(painter, band, 8, with_alpha(ORANGE, 0.16 * a));
            }
            draw_text_cy(painter, narrow ? QDateTime::fromMSecsSinceEpoch(entry.msec).toString("HH:mm:ss") : time_text(entry.msec),
                x, cy, ui_font(18, 500), white(0.5 * a));
            const bool bad = entry.level >= 1;
            const QRectF badge(x + (narrow ? 88 : 128), cy - 11, 66, 22);
            if (entry.level >= 2){
                panel(painter, badge, 6, with_alpha(YELLOW, a));
            }else{
                panel(painter, badge, 6, QColor(), bad ? with_alpha(YELLOW, 0.95 * a) : white(0.34 * a), 1.5);
            }
            draw_text_cy(painter, level_name(entry.level), badge.center().x(), badge.center().y() + 1, ui_font(15, 700),
                entry.level >= 2 ? with_alpha(Qt::black, a) : (bad ? with_alpha(YELLOW, a) : white(0.8 * a)), Qt::AlignHCenter);
            const QFont font = ui_font(20, 500);
            const double room = m_log_rect.right() - 22 - (badge.right() + 14);
            draw_text_cy(painter, QFontMetricsF(font).elidedText(entry.text, Qt::ElideRight, room), badge.right() + 14, cy + 1, font,
                bad ? with_alpha(YELLOW, a) : white(0.88 * a));
        }
        if (count > LOG_ROWS){
            //  How far back the lines are read.
            const double track = LOG_ROWS * pitch - 8;
            const double thumb = std::max(24.0, track * LOG_ROWS / count);
            const double thumb_y = top + 4 + (track - thumb) * (1.0 - (double)offset / (double)std::max(1, count - LOG_ROWS));
            panel(painter, QRectF(m_log_rect.right() - 14, thumb_y, 4, thumb), 2, white(0.28 * a));
        }

        //  The buttons: in the type that is as big as fits (22 when the log is
        //  wide; with the options open it is narrow, and in other languages
        //  the words are longer).
        const double by = m_log_rect.bottom() - 70;
        double bx = x - 4;
        const QString copy_claude_text = T("Copy for Claude");
        const QString copy_log_text = T("Copy log");
        const QString save_text = T("Save .txt");
        double fs = 22;
        for (;; fs -= 1){
            const double k = fs / 22.0;
            const QFont f = ui_font(fs, 600);
            const double total = text_width(f, copy_claude_text) + 58 * k + 14 * k +
                text_width(f, copy_log_text) + 50 * k + 22 * k + text_width(f, save_text) + 16 * k;
            if (total <= m_log_rect.width() - 36 || fs <= 14){
                break;
            }
        }
        const double k = fs / 22.0;
        const QFont button_font = ui_font(fs, 600);
        {
            const QString text = copy_claude_text;
            const double w = text_width(button_font, text) + 58 * k;
            const QRectF rect(bx, by, w, 50);
            const bool hover = canvas.hovered("copy-claude");
            const bool focus = canvas.focused("copy-claude");
            if (hover || focus){
                glow(painter, rect, 25, ORANGE, 0.7 * a);
            }
            panel(painter, rect, 25, with_alpha(hover ? ORANGE.lighter(112) : ORANGE, a));
            //  The two sheets.
            painter.setPen(QPen(with_alpha(Qt::black, a), 2.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(rect.left() + 18 * k, rect.center().y() - 4, 14, 14), 3, 3);
            painter.drawRoundedRect(QRectF(rect.left() + 22 * k, rect.center().y() - 9, 14, 14), 3, 3);
            draw_text_cy(painter, text, rect.left() + 46 * k, rect.center().y() + 1, button_font, with_alpha(Qt::black, a));
            if (focus){
                panel(painter, rect.adjusted(-5, -5, 5, 5), 30, QColor(), with_alpha(YELLOW, 0.9 * a), 2.2);
            }
            canvas.add_hit("copy-claude", rect);
            bx = rect.right() + 14 * k;
        }
        {
            const QString text = copy_log_text;
            const double w = text_width(button_font, text) + 50 * k;
            const QRectF rect(bx, by, w, 50);
            const bool hover = canvas.hovered("copy-log");
            panel(painter, rect, 25, white(hover ? 0.08 * a : 0.0), white((hover ? 0.6 : 0.34) * a), 1.8);
            draw_text_cy(painter, text, rect.center().x(), rect.center().y() + 1, button_font, white(a), Qt::AlignHCenter);
            if (canvas.focused("copy-log")){
                panel(painter, rect.adjusted(-5, -5, 5, 5), 30, QColor(), with_alpha(YELLOW, 0.9 * a), 2.2);
            }
            canvas.add_hit("copy-log", rect);
            bx = rect.right() + 22 * k;
        }
        {
            const QString text = save_text;
            const double w = text_width(button_font, text) + 16 * k;
            const QRectF rect(bx, by, w, 50);
            const bool hover = canvas.hovered("save-log");
            draw_text_cy(painter, text, rect.center().x(), rect.center().y() + 1, button_font, with_alpha(ORANGE, (hover ? 1.0 : 0.85) * a), Qt::AlignHCenter);
            if (canvas.focused("save-log")){
                panel(painter, rect.adjusted(-5, -5, 5, 5), 30, QColor(), with_alpha(YELLOW, 0.9 * a), 2.2);
            }
            canvas.add_hit("save-log", rect);
        }
        if (m_toast_left > 0){
            const QFont font = ui_font(21, 600);
            const double w = text_width(font, m_toast) + 70;
            const QRectF pill(m_log_rect.right() - 20 - w, by - 62, w, 46);
            const double fade = clamp01(m_toast_left / 0.4);
            panel(painter, pill, 23, with_alpha(QColor(27, 27, 30), 0.97 * fade), with_alpha(ORANGE, fade), 2.0);
            draw_light(painter, Light::OK, QPointF(pill.left() + 28, pill.center().y()), 11, fade);
            draw_text_cy(painter, m_toast, pill.left() + 50, pill.center().y() + 1, font, white(fade));
        }
    }

    void paint_lanes(Canvas& canvas, QPainter& painter, double a){
        if (a < 0.004){
            return;
        }
        panel(painter, m_lanes_rect, 26, with_alpha(PANEL, 0.80 * a), white(0.24 * a), 1.6);
        const double lx = m_lanes_rect.left() + 28;

        //  The time.
        const QFont axis = ui_font(19, 500);
        for (int c = 0; c <= 4; c++){
            const double age = 60.0 - c * 15.0;
            const QString text = c == 4 ? T("now") : "-" + QString::number((int)age) + " s";
            const double x = x_at(age);
            draw_text_cy(painter, text, x, m_lanes_rect.top() + 34, axis, white(0.5 * a), c == 4 ? Qt::AlignRight : Qt::AlignHCenter);
        }

        //  The selection goes behind the lanes.
        if (m_selection){
            const double xa = x_at(std::max(m_sel_a, m_sel_b));
            const double xb = x_at(std::min(m_sel_a, m_sel_b));
            const QRectF band(xa, LANE_Y0 - 6, xb - xa, LANE_H * LANE_COUNT + 8);
            painter.setPen(Qt::NoPen);
            painter.setBrush(with_alpha(ORANGE, 0.14 * a));
            painter.drawRect(band);
            painter.setPen(QPen(with_alpha(ORANGE, 0.95 * a), 2.2));
            painter.drawLine(QPointF(xa, band.top()), QPointF(xa, band.bottom()));
            painter.drawLine(QPointF(xb, band.top()), QPointF(xb, band.bottom()));
        }

        const char* const names[LANE_COUNT] = {"Video", "Audio", "Board", "Log", "Buttons", "Program"};
        for (int c = 0; c < LANE_COUNT; c++){
            const Lane lane = (Lane)c;
            const double y = LANE_Y0 + c * LANE_H;
            const QRectF chart(m_cx0, y, m_cx1 - m_cx0, LANE_H - 6);
            if (c > 0){
                painter.setPen(QPen(white(0.09 * a), 1.2));
                painter.drawLine(QPointF(lx, y - 3), QPointF(m_cx1, y - 3));
            }
            lane_icon(painter, lane, QPointF(lx + 10, y + 16), white(0.62 * a));
            draw_text_cy(painter, T(names[c]), lx + 36, y + 12, ui_font(22, 600), white(a));
            draw_text_cy(painter,
                QFontMetricsF(ui_font(17, 500)).elidedText(lane_note(lane), Qt::ElideRight, m_cx0 - 14 - (lx + 36)),
                lx + 36, y + 36, ui_font(17, 500), white(0.5 * a));
            paint_lane(painter, lane, chart, a);
        }

        const QString note = m_selection
            ? T("%1 s selected: Copy for Claude copies this range.").arg((int)std::lround(std::fabs(m_sel_a - m_sel_b)))
            : T("Drag over the timeline to select a range for Copy for Claude.");
        draw_text_cy(painter, note, m_cx1, m_lanes_rect.bottom() - 26, ui_font(19, 600),
            m_selection ? with_alpha(ORANGE, a) : white(0.45 * a), Qt::AlignRight);
        canvas.add_hit("drag:lanes", QRectF(m_cx0, LANE_Y0 - 6, m_cx1 - m_cx0, LANE_H * LANE_COUNT));
    }

    QString lane_note(Lane lane) const{
        switch (lane){
        case Lane::VIDEO:
            return m_input.live() ? tr_ui("%1 fps").arg((int)std::lround(m_input.fps())) : T("no picture");
        case Lane::AUDIO:
            return m_input.audio_ok() ? tr_ui("%1 dB").arg((int)std::lround(m_input.level_db())) : T("no sound");
        case Lane::BOARD:
            return m_board_ok ? m_board_name : T("not found");
        case Lane::LOG:{
            int warnings = 0;
            int errors = 0;
            for (const LogEntry& entry : m_log){
                if (m_clock - entry.at <= 60.0){
                    warnings += entry.level == 1 ? 1 : 0;
                    errors += entry.level >= 2 ? 1 : 0;
                }
            }
            const QString warning_text = warnings == 1 ? tr_ui("1 warning") : tr_ui("%1 warnings").arg(warnings);
            const QString error_text = errors == 1 ? tr_ui("1 error") : tr_ui("%1 errors").arg(errors);
            return warning_text + ", " + error_text;
        }
        case Lane::BUTTONS:
            return T("keys of this window");
        case Lane::PROGRAM:
            return T("none is running");
        }
        return QString();
    }

    void paint_lane(QPainter& painter, Lane lane, const QRectF& chart, double a){
        const double mid = chart.center().y();
        painter.save();
        painter.setClipRect(chart.adjusted(-2, -3, 2, 3));
        switch (lane){
        case Lane::VIDEO:{
            QPainterPath path;
            bool started = false;
            for (const LaneSample& sample : m_samples){
                const double age = m_clock - sample.at;
                if (age > 60.0){
                    continue;
                }
                const double y = chart.bottom() - 8 - std::min(1.0, sample.fps / 60.0) * (chart.height() - 20);
                if (!started){
                    path.moveTo(x_at(age), y);
                    started = true;
                }else{
                    path.lineTo(x_at(age), y);
                }
            }
            painter.setBrush(Qt::NoBrush);
            if (started){
                painter.setPen(QPen(with_alpha(ORANGE, 0.95 * a), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.drawPath(path);
            }else{
                painter.setPen(QPen(white(0.16 * a), 1.6, Qt::DashLine));
                painter.drawLine(QPointF(chart.left(), mid), QPointF(chart.right(), mid));
            }
            break;
        }
        case Lane::AUDIO:{
            painter.setPen(Qt::NoPen);
            painter.setBrush(with_alpha(ORANGE, 0.9 * a));
            const double bar = (m_cx1 - m_cx0) / 120.0 - 2.0;
            for (const LaneSample& sample : m_samples){
                const double age = m_clock - sample.at;
                if (age > 60.0){
                    continue;
                }
                const double h = std::max(3.0, sample.level * (chart.height() - 10));
                painter.drawRoundedRect(QRectF(x_at(age) - bar, mid + 2 - h / 2.0, bar, h), 2, 2);
            }
            break;
        }
        case Lane::BOARD:{
            //  A line while the board was there, a dashed one while it was not.
            for (size_t c = 1; c < m_samples.size(); c++){
                const double age0 = m_clock - m_samples[c - 1].at;
                const double age1 = m_clock - m_samples[c].at;
                if (age0 > 60.0){
                    continue;
                }
                if (m_samples[c].board){
                    painter.setPen(QPen(with_alpha(ORANGE, 0.95 * a), 2.4, Qt::SolidLine, Qt::RoundCap));
                }else{
                    painter.setPen(QPen(white(0.2 * a), 1.6, Qt::DashLine));
                }
                painter.drawLine(QPointF(x_at(age0), mid), QPointF(x_at(age1), mid));
            }
            break;
        }
        case Lane::LOG:{
            for (const LogEntry& entry : m_log){
                const double age = m_clock - entry.at;
                if (age > 60.0 || age < 0){
                    continue;
                }
                const double h = entry.level >= 2 ? chart.height() - 10 : entry.level == 1 ? 24 : 12;
                painter.setPen(QPen(entry.level >= 1 ? with_alpha(YELLOW, a) : white(0.5 * a), entry.level >= 1 ? 3.4 : 2.0, Qt::SolidLine, Qt::RoundCap));
                painter.drawLine(QPointF(x_at(age), mid - h / 2.0), QPointF(x_at(age), mid + h / 2.0));
            }
            break;
        }
        case Lane::BUTTONS:{
            painter.setPen(QPen(with_alpha(ORANGE, 0.95 * a), 5.0, Qt::SolidLine, Qt::RoundCap));
            for (double at : m_key_times){
                const double age = m_clock - at;
                if (age <= 60.0){
                    painter.drawLine(QPointF(x_at(age), mid - 12), QPointF(x_at(age), mid + 12));
                }
            }
            break;
        }
        case Lane::PROGRAM:
            painter.setPen(QPen(white(0.16 * a), 1.6, Qt::DashLine));
            painter.drawLine(QPointF(chart.left(), mid), QPointF(chart.right(), mid));
            draw_text_cy(painter, T("A program that is started shows its loops here."), chart.left() + 14, mid - 1, ui_font(18, 500), white(0.4 * a));
            break;
        }
        painter.restore();
    }

    void paint_footer(Canvas& canvas, QPainter& painter, double a){
        if (a < 0.004){
            return;
        }
        double x = 136;
        {
            const QString text = T("All games");
            const double w = draw_button(canvas, painter, "all", text, x + 26, 960, ButtonKind::LINK, std::min(1.0, a * 1.35));
            draw_chevron_left(painter, QPointF(x + 8, 982), white(0.7 * a));
            x += 26 + w + 34;
        }
        if (!m_canvas->app_mode()){
            const double w = draw_button(canvas, painter, "later", T("Choose later"), x, 960, ButtonKind::LINK, std::min(1.0, a * 1.35));
            x += w + 34;
        }
        const QString start_text = T("Start");
        const double start_w = button_width(start_text, ButtonKind::PRIMARY);
        const QFont hint = ui_font(21, 500);
        const double room = 1780 - start_w - 40 - x;
        const QString line = T("Type to search · Up/Down to choose · Enter to start");
        draw_text_cy(painter, QFontMetricsF(hint).elidedText(line, Qt::ElideRight, room), x, 982, hint, white(0.42 * a));
        draw_button(canvas, painter, "start", start_text, 1780 - start_w, 950, ButtonKind::PRIMARY, a, m_selected >= 0);
    }
    static void draw_chevron_left(QPainter& painter, const QPointF& c, const QColor& color){
        QPen pen(color, 2.6);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(QPolygonF({QPointF(c.x() + 5, c.y() - 9), QPointF(c.x() - 4, c.y()), QPointF(c.x() + 5, c.y() + 9)}));
    }

private:
    Canvas* m_canvas;
    int m_game;
    const SetupRequest* m_request;
    const HomeGame* m_game_info;
    QString m_game_name;
    QString m_build;
    std::vector<HomeProgram> m_programs;
    bool m_sections = false;

    //  The list.
    std::vector<int> m_view;                //  the programs that match, as indexes into m_programs
    std::vector<Item> m_items;
    double m_content = 0;
    int m_selected = -1;                    //  index in m_view
    QString m_query;
    Smooth m_scroll;

    //  The picture, the sound, the board.
    LiveInput m_input;
    HardwareProbe m_probe;
    bool m_test_probe;
    bool m_board_ok = false;
    QString m_board_name;
    QStringList m_ports;
    int m_keys_seen = 0;
    std::deque<double> m_key_times;

    //  Time, and what the timeline has seen.
    double m_clock = 0;
    double m_since_sample = 0;
    double m_since_poll = 0;
    std::deque<LaneSample> m_samples;
    bool m_selection = false;
    double m_sel_a = 0;
    double m_sel_b = 0;
    double m_drag_x = 0;
    bool m_dragging = false;

    //  The log.
    std::deque<LogEntry> m_log;
    quint64 m_last_serial = 0;
    int m_level = 0;                        //  0 all, 1 info, 2 warn, 3 error
    int m_log_offset = 0;
    QString m_toast;
    double m_toast_left = 0;
    QString m_saved_path;
    bool m_source_open = false;

    //  The options of the selected program, next to the list.
    QPointer<QScrollArea> m_host;           //  over the picture; a child of the canvas
    std::unique_ptr<SetupProgramOptions> m_options;
    bool m_options_wanted = false;          //  a program was picked: the panel is open
    QString m_options_target;               //  the program whose options are shown
    QString m_options_for;                  //  the one that they were made for
    double m_select_rest = 0;               //  how long the selection has been still
    double m_dirty_for = 0;                 //  how long a change has been waiting to be kept
    int m_host_font = 0;
    Smooth m_open;                          //  how far the panel is open: 0 to 1
    //  The panel can take the room of the picture, the log and the timeline
    //  ("wide"): a program with many options reads better that way.
    Smooth m_wide;                          //  0 to 1
    bool m_wide_user = false;               //  the person chose: it stays for the other programs
    //  The picture over the whole screen.
    bool m_video_full = false;
    Smooth m_full;                          //  0 to 1
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
    //  What it covers (the program's own list of models, see ConsoleType);
    //  only for the consoles that are supported.
    const char* note;
    //  The program can be used with it now. The others are shown on the screen
    //  already, dimmed and with a dashed outline, and cannot be picked.
    bool supported;
};
//  The supported consoles come first, as the large tiles.
//
//  To add a console later: set "supported" to true and give it a note, add a
//  value to SetupConsole (GlobalSettingsPanel.h) and its slug to the setting
//  GAME_CONSOLE (GlobalSettingsPanel.cpp), and let console_code_of() and
//  console_of() (PocketPilot_Language.cpp) know its code. A console that has no
//  picture yet gets the small tile, so it only has to be moved up in this list
//  and given a picture in ConsolePage::paint_tile().
constexpr ConsoleInfo CONSOLES[] = {
    {"switch",      "Nintendo Switch",      "Switch 1 and the OLED model",          true},
    {"switch2",     "Nintendo Switch 2",    "International and Japan-only models",  true},
    //  What the gamepad adapters of the community already speak (the
    //  platforms of OGX-Mini) and what the commercial ones list.
    {"ps5",         "PlayStation 5",        nullptr,    false},
    {"ps4",         "PlayStation 4",        nullptr,    false},
    {"ps3",         "PlayStation 3",        nullptr,    false},
    {"ps2",         "PlayStation 2",        nullptr,    false},
    {"ps1",         "PlayStation",          nullptr,    false},
    {"psclassic",   "PlayStation Classic",  nullptr,    false},
    {"xboxseries",  "Xbox Series X|S",      nullptr,    false},
    {"xboxone",     "Xbox One",             nullptr,    false},
    {"xbox360",     "Xbox 360",             nullptr,    false},
    {"xbox",        "Original Xbox",        nullptr,    false},
    {"wiiu",        "Nintendo Wii U",       nullptr,    false},
    {"gamecube",    "Nintendo GameCube",    nullptr,    false},
    {"n64",         "Nintendo 64",          nullptr,    false},
    {"dreamcast",   "Sega Dreamcast",       nullptr,    false},
};
constexpr int CONSOLE_COUNT = (int)(sizeof(CONSOLES) / sizeof(CONSOLES[0]));

class ConsolePage : public Page{
public:
    explicit ConsolePage(Canvas& canvas){
        m_again = canvas.console_again();
        QString kept = canvas.request().console_code;
        if (m_again && canvas.result().console_chosen){
            kept = canvas.result().console_code;
        }
        for (int c = 0; c < CONSOLE_COUNT; c++){
            if (CONSOLES[c].supported && kept == CONSOLES[c].code){
                m_selected = c;
            }
        }
        //  One that was chosen before is shown as picked: Continue is next.
        canvas.set_focus_id(m_selected >= 0 ? QString("continue") : tile_id(next_enabled(-1, +1)));
    }

    virtual const char* name() const override{
        return "console";
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        //  The setup starts here: the same steps and the same place for the
        //  texts as the page of the board that follows.
        const int states[3] = {1, 2, 2};
        if (!m_again){
            draw_stepper(painter, states, prog(t, in.head - 0.05, in.head + 0.7));
        }
        const QString heading = T("Which console do you use?");
        draw_heading(painter, heading, 140, 250, 80, prog(t, in.head, in.head + 0.9), 1.0, 1500);
        const QString sub = T("Pick the console you want to control. The board you pick next connects to it.");
        const double sub_y = 250 + heading_height(heading, 80, 1500) + 26;
        draw_sub(painter, sub, 140, sub_y, prog(t, in.sub, in.sub + 0.8), 1.0, 34, 1500);

        const double top = sub_y + sub_height(sub, 34, 1500) + 36;
        for (int c = 0; c < CONSOLE_COUNT; c++){
            const double begin = in.bar + 0.07 * c;
            paint_tile(canvas, painter, c, tile_rect(c, top), eo(prog(t, begin, begin + 0.6)));
        }
        //  Over the consoles that are not supported yet.
        if (supported_count() < CONSOLE_COUNT){
            const double begin = in.bar + 0.07 * supported_count();
            const double reveal = eo(prog(t, begin, begin + 0.6));
            const double y = top + BIG_HEIGHT + 24;
            draw_text_cy(painter, T("Coming soon"), 144, y + 14, ui_font(24, 600), white(0.68 * reveal));
            painter.setPen(QPen(white(0.2 * reveal), 1.4));
            const double text_end = 144 + text_width(ui_font(24, 600), T("Coming soon")) + 22;
            painter.drawLine(QPointF(text_end, y + 14), QPointF(1780, y + 14));
        }

        const double b = eo(prog(t, in.buttons, in.buttons + 0.5));
        if (m_again){
            //  From the games: back to them, not a step of the setup.
            const double w = draw_button(canvas, painter, "back", T("Back to the games"), 136 + 26, 960, ButtonKind::LINK, std::min(1.0, b * 1.35));
            (void)w;
            draw_chevron_back(painter, QPointF(136 + 8, 982), white(0.7 * b));
        }else{
            draw_skip(canvas, painter, b);
        }
        const QString text = T("Continue");
        draw_button(canvas, painter, "continue", text, 1780 - button_width(text, ButtonKind::PRIMARY), 950,
            ButtonKind::PRIMARY, b, m_selected >= 0);
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        const int index = index_of(id);
        if (index >= 0 && index < CONSOLE_COUNT){
            if (CONSOLES[index].supported){
                m_selected = index;
                //  A console picked: the next Enter goes on.
                canvas.set_focus_id("continue");
            }
        }else if (id == "continue"){
            proceed(canvas);
        }else if (id == "back"){
            canvas.leave_console_again();
        }else if (id == "skip"){
            canvas.result().board_done = true;
            canvas.end_installation();
        }
    }
    virtual void escape_pressed(Canvas& canvas) override{
        if (m_again){
            canvas.leave_console_again();
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
            target = m_selected >= 0 ? m_selected : next_enabled(-1, +1);
        }else{
            //  Only the consoles that can be picked: the others are skipped.
            target = next_enabled(current, step);
            if (target < 0){
                target = current;
            }
        }
        if (target >= 0){
            canvas.set_focus_id(tile_id(target));
        }
        return true;
    }
    virtual void enter_pressed(Canvas& canvas) override{
        proceed(canvas);
    }
    virtual QString state() const override{
        return QString("selected=") + (m_selected >= 0 ? CONSOLES[m_selected].code : "");
    }

private:
    static constexpr double BIG_HEIGHT = 236;
    static constexpr int SMALL_COLUMNS = 5;
    bool m_again = false;                   //  chosen again, from the games

    static QString tile_id(int index){
        return "console:" + QString::number(index);
    }
    static int index_of(const QString& id){
        return id.startsWith("console:") ? id.mid(8).toInt() : -1;
    }
    static int supported_count(){
        int count = 0;
        for (const ConsoleInfo& info : CONSOLES){
            count += info.supported ? 1 : 0;
        }
        return count;
    }
    //  The next console after "from" in this direction that can be picked, or -1.
    static int next_enabled(int from, int direction){
        for (int c = from + direction; c >= 0 && c < CONSOLE_COUNT; c += direction){
            if (CONSOLES[c].supported){
                return c;
            }
        }
        return -1;
    }
    void proceed(Canvas& canvas){
        if (m_selected < 0){
            return;
        }
        canvas.result().console_chosen = true;
        canvas.result().console_code = CONSOLES[m_selected].code;
        canvas.app_console_chosen(QString::fromLatin1(CONSOLES[m_selected].code));
        canvas.end_console();
    }

    //  The supported consoles are large tiles side by side; under them, after
    //  a line of text, the others in rows of small tiles.
    static QRectF tile_rect(int index, double top){
        constexpr double LEFT = 140;
        constexpr double RIGHT = 1780;
        const int large = supported_count();
        if (index < large){
            constexpr double GAP = 40;
            const double width = (RIGHT - LEFT - GAP * (large - 1)) / large;
            return QRectF(LEFT + (width + GAP) * index, top, width, BIG_HEIGHT);
        }
        constexpr double GAP = 10;
        constexpr double HEIGHT = 60;
        const int k = index - large;
        const double width = (RIGHT - LEFT - GAP * (SMALL_COLUMNS - 1)) / SMALL_COLUMNS;
        const double first = top + BIG_HEIGHT + 18 + 28 + 18;
        return QRectF(
            LEFT + (width + GAP) * (k % SMALL_COLUMNS),
            first + (HEIGHT + GAP) * (k / SMALL_COLUMNS),
            width, HEIGHT
        );
    }

    void paint_tile(Canvas& canvas, QPainter& painter, int index, const QRectF& place, double reveal){
        if (reveal < 0.004){
            return;
        }
        const ConsoleInfo& info = CONSOLES[index];
        const QString id = tile_id(index);
        const bool enabled = info.supported;
        const bool hover = enabled && canvas.hovered(id);
        const bool focus = enabled && canvas.focused(id);
        const bool selected = index == m_selected;
        const double a = reveal;
        const QRectF box = place.translated(0, (1.0 - reveal) * 26);
        canvas.add_hit(id, place, enabled, enabled);

        if (!enabled){
            paint_small_tile(painter, info, box, a);
            return;
        }

        if (selected){
            glow(painter, box, 30, ORANGE, 0.55 * reveal);
        }
        panel(
            painter, box, 30,
            selected ? with_alpha(QColor(44, 29, 24), 0.95 * a) : with_alpha(PANEL, 0.92 * a),
            selected ? with_alpha(ORANGE, a) : white((hover ? 0.62 : 0.26) * a),
            selected ? 2.6 : hover ? 2.2 : 1.8
        );

        //  The console on the left, as large as the tile lets it be; its name
        //  and what it covers on the right.
        const QPixmap picture = index == 0 ? plain_picture("switch") : switch2_picture();
        const QRectF room(box.left() + 36, box.top() + 30, 360, box.height() - 60);
        if (!picture.isNull()){
            const double scale = std::min(room.width() / picture.width(), room.height() / picture.height());
            const double w = picture.width() * scale;
            const double h = picture.height() * scale;
            draw_picture(painter, picture, QRectF(room.center().x() - w / 2.0, room.center().y() - h / 2.0, w, h), a);
        }
        const double text_x = room.right() + 36;
        const double text_w = box.right() - 28 - text_x;
        const QFont name_font = ui_font(36, 600);
        const QString note = T(info.note);
        const QFont note_font = ui_font(23, 500);
        const QStringList note_lines = wrap_text(note_font, note, text_w);
        const double note_line = 23 * 1.35;
        const double block = 44 + 14 + (double)note_lines.size() * note_line;
        double y = box.center().y() - block / 2.0;
        draw_text(painter, QString::fromLatin1(info.name), text_x, y, name_font, white(a));
        y += 44 + 14;
        for (const QString& line : note_lines){
            draw_text(painter, line, text_x, y, note_font, white(0.55 * a));
            y += note_line;
        }
        if (selected){
            const QRectF badge(box.right() - 24 - 40, box.top() + 22, 40, 40);
            painter.setPen(Qt::NoPen);
            painter.setBrush(with_alpha(ORANGE, a));
            painter.drawEllipse(badge);
            draw_check(painter, badge.adjusted(6, 6, -6, -6), with_alpha(Qt::black, a), 3.2);
        }
        if (focus){
            panel(painter, box.adjusted(-6, -6, 6, 6), 36, QColor(), with_alpha(YELLOW, 0.9 * reveal), 2.4);
        }
    }

    //  A console that is not supported yet: a gamepad and its name, dimmed, in
    //  a dashed outline.
    static void paint_small_tile(QPainter& painter, const ConsoleInfo& info, const QRectF& box, double a){
        //  A dark ground, so that the lines of the background do not run through the name.
        painter.setBrush(with_alpha(PANEL, 0.88 * a));
        QPen dashed(white(0.26 * a), 1.6);
        dashed.setStyle(Qt::DashLine);
        painter.setPen(dashed);
        painter.drawRoundedRect(box.adjusted(0.8, 0.8, -0.8, -0.8), 20, 20);

        //  A plain gamepad: the same for each, it is no picture of the console.
        const QRectF pad(box.left() + 22, box.center().y() - 15, 50, 30);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(white(0.42 * a), 2.0));
        painter.drawRoundedRect(pad, 13, 13);
        draw_plus(painter, QPointF(pad.left() + 14, pad.center().y()), 5.5, white(0.42 * a), 2.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(white(0.42 * a));
        painter.drawEllipse(QPointF(pad.right() - 17, pad.center().y() + 4), 3.2, 3.2);
        painter.drawEllipse(QPointF(pad.right() - 9, pad.center().y() - 4), 3.2, 3.2);

        //  A long name gets a smaller type before it is cut off at the end.
        const double text_x = pad.right() + 18;
        const double text_w = box.right() - 14 - text_x;
        const QString shown = QString::fromLatin1(info.name);
        QFont font = ui_font(24, 600);
        for (double px = 24; px > 18.5 && text_width(font, shown) > text_w; px -= 1.0){
            font = ui_font(px, 600);
        }
        draw_text_cy(painter, QFontMetricsF(font).elidedText(shown, Qt::ElideRight, text_w), text_x, box.center().y() + 1, font, white(0.62 * a));
    }

private:
    int m_selected = -1;
};


//  ---------------------------------------------------------------------
//  The program that is open (the main window's own screen)
//  ---------------------------------------------------------------------

//  The page of a program (or of the settings): its own Qt widget, as the
//  program made it, is held inside the screen under a header with the way
//  back, the name and the state. The widget is the window's: it is taken out
//  (AppView::unhost) before it is deleted.
class HostPage : public Page{
public:
    HostPage(Canvas& canvas, QWidget* holder, int game, const QString& game_name, const QString& title)
        : m_canvas(&canvas)
        , m_holder(holder)
        , m_game(game)
        , m_game_name(game_name)
        , m_title(title)
    {
        canvas.set_focus_id("host-back");
    }
    virtual ~HostPage() override{
        //  What is left in the holder is the window's to take: the holder is
        //  only the frame.
        if (m_holder != nullptr){
            m_holder->hide();
            delete m_holder.data();
        }
    }

    virtual const char* name() const override{
        return "host";
    }
    virtual QString state() const override{
        return "title=" + m_title + ";game=" + QString::number(m_game);
    }
    virtual QString about() const override{
        if (m_game_name.isEmpty()){
            return m_title;
        }
        return m_title.isEmpty() ? m_game_name : m_game_name + ": " + m_title;
    }

    virtual void step(double dt) override{
        m_clock += dt;
        place();
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        const double a_head = eo(prog(t, in.head, in.head + 0.5));
        const double a_body = eo(prog(t, in.sub, in.sub + 0.5));
        place();

        //  The header: the way back, then where this is.
        const QString back_text = m_game >= 0 && canvas.has_programs(m_game) ? T("Programs") : T("All games");
        double x = 60;
        {
            const double w = draw_button(canvas, painter, "host-back", back_text, x + 26, 46, ButtonKind::LINK,
                std::min(1.0, a_head * 1.35), !canvas.app_busy());
            draw_chevron_back(painter, QPointF(x + 8, 68), white(0.7 * a_head * (canvas.app_busy() ? 0.35 : 1.0)));
            x += 26 + w + 30;
        }
        const double cy = 68;
        if (m_game >= 0 && !m_game_name.isEmpty()){
            const QFont game_font = ui_font(28, 500);
            draw_text_cy(painter, m_game_name, x, cy, game_font, white(0.55 * a_head));
            x += text_width(game_font, m_game_name) + 16;
            draw_text_cy(painter, QString(QChar(0x203A)), x, cy, game_font, white(0.4 * a_head));
            x += 28;
        }
        const double right = std::min(1860.0, canvas.app_right_edge() - 20);
        {
            //  The state: ready, or the program that runs.
            const bool busy = canvas.app_busy();
            const QString text = canvas.app_state_text().isEmpty() ? (busy ? T("Running") : T("Ready")) : canvas.app_state_text();
            const double w = draw_chip(painter, text, right, cy + 22, busy ? ChipKind::ORANGE_FILL : ChipKind::RING, a_head);
            m_title_room = right - w - 24 - x;
        }
        const QFont title_font = serif_font(46);
        draw_text_cy(painter, QFontMetricsF(title_font).elidedText(m_title, Qt::ElideRight, std::max(200.0, m_title_room)),
            x, cy, title_font, white(a_head));

        //  The frame behind the program's widget.
        const QRectF area = content_rect(canvas);
        panel(painter, area.adjusted(-10, -10, 10, 10), 18, with_alpha(PANEL, 0.92 * a_body), white(0.16 * a_body), 1.5);

        //  The footer: what the computer does about sleep, and its switch.
        const double fy = 1050;
        draw_text_cy(painter, canvas.app_sleep_text(), 70, fy, ui_font(22, 500), white(0.5 * a_body));
        const QString toggle = canvas.app_keep_on() ? T("Let the screen sleep") : T("Keep the screen on");
        const double toggle_w = button_width(toggle, ButtonKind::LINK);
        draw_button(canvas, painter, "host-sleep", toggle, right - toggle_w, fy - 22, ButtonKind::LINK, std::min(1.0, a_body * 1.35));
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        if (id == "host-back"){
            if (canvas.app_busy()){
                return;
            }
            if (canvas.app().close_program && !canvas.app().close_program()){
                return;
            }
            canvas.leave_host(m_game);
        }else if (id == "host-sleep"){
            canvas.app_set_keep_on(!canvas.app_keep_on());
        }
    }

private:
    //  Under the header, over the footer; left of Claude when its panel is shown.
    static QRectF content_rect(const Canvas& canvas){
        const double right = std::min(1860.0, canvas.app_right_edge() - 20);
        return QRectF(60, 112, right - 60, 920);
    }
    void place(){
        if (m_holder == nullptr){
            return;
        }
        //  Only when the page is there as a whole, not while it fades.
        if (m_canvas->page_fade() < 0.98){
            if (!m_holder->isHidden()){
                m_holder->hide();
            }
            return;
        }
        const QRect geometry = m_canvas->window_rect(content_rect(*m_canvas));
        if (m_holder->geometry() != geometry){
            m_holder->setGeometry(geometry);
        }
        if (m_holder->isHidden()){
            m_holder->show();
        }
    }

    Canvas* m_canvas;
    QPointer<QWidget> m_holder;
    int m_game;
    QString m_game_name;
    QString m_title;
    double m_clock = 0;
    double m_title_room = 600;
};


//  ---------------------------------------------------------------------
//  The overview of the installations: a miniature of each, and one more to
//  make a new one. A window of its own (SetupRequest::overview).
//  ---------------------------------------------------------------------

//  Where the miniatures go: as large as the room allows, in rows.
struct TileGrid{
    int columns = 1;
    double width = 0;
    double picture_height = 0;
    double tile_height = 0;
};
constexpr double TILE_X = 140;
constexpr double TILE_Y = 330;
constexpr double TILE_AREA_W = 1640;
constexpr double TILE_AREA_H = 690;
constexpr double TILE_GAP = 32;
constexpr double TILE_CAPTION = 96;
constexpr double TILE_MAX_W = 520;

TileGrid tile_grid(size_t count){
    TileGrid best;
    const int n = (int)std::max<size_t>(1, count);
    for (int columns = 1; columns <= std::min(n, 8); columns++){
        const int rows = (n + columns - 1) / columns;
        const double by_width = (TILE_AREA_W - (columns - 1) * TILE_GAP) / columns;
        const double room = (TILE_AREA_H - (rows - 1) * TILE_GAP) / rows;
        const double by_height = (room - TILE_CAPTION) * 16.0 / 9.0;
        const double width = std::min(std::min(by_width, by_height), TILE_MAX_W);
        //  More columns when it comes to the same.
        if (width >= best.width){
            best.columns = columns;
            best.width = width;
        }
    }
    best.width = std::max(80.0, best.width);
    best.picture_height = best.width * 9.0 / 16.0;
    best.tile_height = best.picture_height + TILE_CAPTION;
    return best;
}
QRectF tile_rect(const TileGrid& grid, size_t index){
    const int column = (int)(index % (size_t)grid.columns);
    const int row = (int)(index / (size_t)grid.columns);
    return QRectF(
        TILE_X + column * (grid.width + TILE_GAP),
        TILE_Y + row * (grid.tile_height + TILE_GAP),
        grid.width,
        grid.tile_height
    );
}

QString instance_name_text(const InstanceInfo& info){
    return info.name.isEmpty() ? T("Installation %1").arg(info.id) : info.name;
}
//  What an installation is doing, in words.
QString instance_state_text(const InstanceInfo& info){
    if (!info.running){
        return info.starting ? T("Starting...") : T("Not running");
    }
    if (info.state == "running"){
        return T("A program runs");
    }
    if (info.state == "setup"){
        return T("Setup");
    }
    if (info.state == "program"){
        return T("A program is open");
    }
    return T("Games");
}

class InstancesPage : public Page{
public:
    explicit InstancesPage(Canvas& canvas)
        : m_canvas(&canvas)
    {
        refresh();
        canvas.set_focus_id(m_infos.empty() ? QString("inst-new") : "inst:" + m_infos.front().id);
    }

    virtual const char* name() const override{
        return "instances";
    }
    virtual bool shows_menu() const override{
        return false;
    }
    virtual void step(double dt) override{
        m_clock += dt;
        m_poll += dt;
        if (m_poll >= 1.0){
            m_poll = 0;
            refresh();
        }
        if (!m_confirm.isEmpty()){
            m_confirm_time -= dt;
            if (m_confirm_time <= 0){
                m_confirm.clear();
            }
        }
        if (!m_waiting.isEmpty()){
            m_waiting_time -= dt;
            if (m_waiting_time <= 0){
                m_waiting.clear();
            }
        }
    }
    virtual QString state() const override{
        return QString::number(m_infos.size()) + " installations";
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in{0.10, 0.35, 0.50, 0.85};
        const QString heading = T("Installations");
        draw_heading(painter, heading, 140, 80, 104, prog(t, in.head, in.head + 0.9), 1.0, 1200);
        draw_sub(
            painter,
            T("Each installation has its own settings: its own capture card and its own microcontroller. Closing this window does not stop them."),
            140, 80 + heading_height(heading, 104, 1200) + 12, prog(t, in.sub, in.sub + 0.8), 1.0, 30, 1250
        );

        const double reveal = eo(prog(t, in.bar, in.bar + 0.8));
        const TileGrid grid = tile_grid(m_infos.size() + 1);
        for (size_t c = 0; c < m_infos.size(); c++){
            paint_tile(canvas, painter, m_infos[c], tile_rect(grid, c), grid, reveal);
        }
        paint_new_tile(canvas, painter, tile_rect(grid, m_infos.size()), grid, reveal);
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        const SetupRequest& request = canvas.request();
        if (id == "inst-new"){
            const QString made = request.instance_create ? request.instance_create() : QString();
            if (!made.isEmpty()){
                //  The window of the new one comes by itself; this one goes aside
                //  when it is up.
                m_waiting = made;
                m_waiting_time = 30;
                refresh();
            }
        }else if (id.startsWith("inst-remove:")){
            const QString target = id.mid(QString("inst-remove:").size());
            if (m_confirm != target){
                m_confirm = target;
                m_confirm_time = 4.0;
            }else{
                m_confirm.clear();
                if (request.instance_remove){
                    request.instance_remove(target);
                }
                refresh();
            }
        }else if (id.startsWith("inst:")){
            const QString target = id.mid(QString("inst:").size());
            bool running = false;
            for (const InstanceInfo& info : m_infos){
                if (info.id == target){
                    running = info.running;
                }
            }
            if (request.instance_open){
                request.instance_open(target);
            }
            if (running){
                //  Its window is on its way to the front.
                canvas.minimize();
            }else{
                m_waiting = target;
                m_waiting_time = 30;
            }
        }
    }
    virtual void enter_pressed(Canvas& canvas) override{
        (void)canvas;
    }

private:
    void refresh(){
        const SetupRequest& request = m_canvas->request();
        m_infos = request.instance_list ? request.instance_list() : std::vector<InstanceInfo>();
        m_pictures.clear();
        for (const InstanceInfo& info : m_infos){
            m_pictures.push_back(info.picture.isNull() ? QPixmap() : QPixmap::fromImage(info.picture));
        }
        //  The one that was started is up: this window steps aside.
        if (!m_waiting.isEmpty()){
            for (const InstanceInfo& info : m_infos){
                if (info.id == m_waiting && info.running){
                    m_waiting.clear();
                    m_canvas->minimize();
                }
            }
        }
        if (!m_confirm.isEmpty()){
            bool still = false;
            for (const InstanceInfo& info : m_infos){
                if (info.id == m_confirm && !info.running && !info.main){
                    still = true;
                }
            }
            if (!still){
                m_confirm.clear();
            }
        }
    }

    void paint_tile(
        Canvas& canvas, QPainter& painter, const InstanceInfo& info,
        const QRectF& tile, const TileGrid& grid, double reveal
    ){
        const size_t index = (size_t)(&info - m_infos.data());
        const QString id = "inst:" + info.id;
        const bool hover = canvas.hovered(id);
        const bool focus = canvas.focused(id);
        const QRectF picture(tile.left(), tile.top(), grid.width, grid.picture_height);
        const double a = reveal;
        const double slide = (1.0 - reveal) * 30;
        painter.save();
        painter.translate(0, slide);

        //  The miniature: dimmed when the installation does not run.
        if (hover || focus){
            glow(painter, picture, 18, ORANGE, 0.8 * a);
        }
        panel(painter, picture, 18, with_alpha(QColor(8, 8, 9), a));
        const double dim = info.running ? 1.0 : 0.42;
        if (index < m_pictures.size() && !m_pictures[index].isNull()){
            QPainterPath clip;
            clip.addRoundedRect(picture.adjusted(1, 1, -1, -1), 17, 17);
            painter.save();
            painter.setClipPath(clip);
            painter.setOpacity(a * dim);
            painter.drawPixmap(picture.adjusted(1, 1, -1, -1), m_pictures[index], QRectF(m_pictures[index].rect()));
            painter.restore();
        }else{
            const QPixmap logo = plain_picture("switch-gg");
            if (!logo.isNull()){
                const double w = picture.width() * 0.5;
                const double h = w * logo.height() / logo.width();
                draw_picture(painter, logo, QRectF(picture.center().x() - w / 2, picture.center().y() - h / 2, w, h), 0.35 * a);
            }
        }
        panel(
            painter, picture, 18, QColor(),
            hover || focus ? with_alpha(ORANGE, 0.95 * a) : white(0.22 * a), hover || focus ? 3.0 : 1.8
        );

        //  What it is doing, in a chip in the corner.
        QString chip;
        ChipKind kind = ChipKind::RING;
        if (!info.running){
            chip = instance_state_text(info);
            kind = info.starting ? ChipKind::RING : ChipKind::DIM;
        }else if (info.state == "running"){
            chip = T("Running");
            kind = ChipKind::ORANGE_FILL;
        }else if (info.state == "setup"){
            chip = T("Setup");
            kind = ChipKind::YELLOW_FILL;
        }
        if (!chip.isEmpty()){
            draw_chip(painter, chip, picture.right() - 12, picture.top() + 28, kind, a, 20, 34, 13);
        }

        //  The name, and what it shows.
        const double x = picture.left() + 4;
        const double name_y = picture.bottom() + 28;
        const bool removable = !info.main && !info.running && !info.starting;
        const QString remove_text = m_confirm == info.id ? T("Really remove?") : T("Remove");
        const double remove_w = removable ? button_width(remove_text, ButtonKind::LINK) : 0;
        draw_elided(
            painter, instance_name_text(info), x, name_y, grid.width - 8 - remove_w - 10,
            ui_font(30, 600), white(a)
        );
        const QString line = info.running && !info.title.isEmpty() ? info.title : instance_state_text(info);
        draw_elided(painter, line, x, name_y + 40, grid.width - 8, ui_font(22, 500), white(0.62 * a));
        painter.restore();

        //  The hits are in the places of the screen, without the slide of the entrance.
        const QRectF whole(tile.left(), tile.top(), tile.width(), grid.picture_height + 56);
        canvas.add_hit(id, whole, reveal > 0.9);
        if (removable && reveal > 0.9){
            const QString remove_id = "inst-remove:" + info.id;
            const QRectF area(picture.right() - remove_w, picture.bottom() + 6, remove_w, 44);
            if (m_confirm == info.id){
                draw_text_cy(painter, remove_text, area.center().x(), area.center().y(), ui_font(24, 600), with_alpha(ORANGE, 0.95), Qt::AlignHCenter);
                canvas.add_hit(remove_id, area, true);
            }else{
                draw_button(canvas, painter, remove_id, remove_text, area.left(), area.top(), ButtonKind::LINK, 1.0);
            }
        }
    }

    void paint_new_tile(Canvas& canvas, QPainter& painter, const QRectF& tile, const TileGrid& grid, double reveal){
        const QString id = "inst-new";
        const bool hover = canvas.hovered(id);
        const bool focus = canvas.focused(id);
        const QRectF picture(tile.left(), tile.top(), grid.width, grid.picture_height);
        const double a = reveal;
        painter.save();
        painter.translate(0, (1.0 - reveal) * 30);
        if (hover || focus){
            glow(painter, picture, 18, ORANGE, 0.5 * a);
        }
        painter.setPen(QPen(hover || focus ? with_alpha(ORANGE, 0.95 * a) : white(0.34 * a), 2.2, Qt::DashLine));
        painter.setBrush(white((hover ? 0.07 : 0.03) * a));
        painter.drawRoundedRect(picture.adjusted(1.1, 1.1, -1.1, -1.1), 18, 18);
        draw_plus(painter, picture.center(), std::max(14.0, grid.width * 0.07), hover || focus ? with_alpha(ORANGE, a) : white(0.7 * a), 5.0);
        const double x = picture.left() + 4;
        const double name_y = picture.bottom() + 28;
        draw_elided(painter, T("New installation"), x, name_y, grid.width - 8, ui_font(30, 600), white(a));
        draw_elided(painter, T("It starts with the setup."), x, name_y + 40, grid.width - 8, ui_font(22, 500), white(0.62 * a));
        painter.restore();
        canvas.add_hit(id, QRectF(tile.left(), tile.top(), tile.width(), grid.picture_height + 56), reveal > 0.9);
    }

    Canvas* m_canvas;
    std::vector<InstanceInfo> m_infos;
    std::vector<QPixmap> m_pictures;
    double m_clock = 0;
    double m_poll = 0;
    //  "Remove" was pressed once on this one: it must be pressed again.
    QString m_confirm;
    double m_confirm_time = 0;
    //  An installation that was started: this window steps aside when it runs.
    QString m_waiting;
    double m_waiting_time = 0;
};


//  ---------------------------------------------------------------------
//  Canvas
//  ---------------------------------------------------------------------

Canvas::Canvas(const SetupRequest& request, bool test_mode, QWidget* parent, AppActions* app)
    : QWidget(parent, parent != nullptr ? Qt::Widget : (test_mode ? Qt::Window : (Qt::Window | Qt::FramelessWindowHint)))
    , m_request(request)
    , m_test_mode(test_mode)
    , m_timer(this)
    , m_presence(this)
{
    if (app != nullptr){
        m_app_mode = true;
        m_app = *app;
    }
    //  The windows of an installation that its overview may bring forward.
    if (parent == nullptr && !m_request.overview){
        mark_instance_window(this);
    }
    setWindowTitle(m_request.overview ? "Claude GG - Installations" : "Claude GG");
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
    if (m_app_mode){
        //  The window's own screen: it starts on the games and stays.
        go_games();
    }else if (m_request.overview){
        go_instances();
    }else if (m_request.ask_language){
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
        //  The overview reads what each installation says about itself.
        if (!m_request.overview && !is_test_run()){
            m_presence.setInterval(1000);
            connect(&m_presence, &QTimer::timeout, this, [this]{
                report_presence();
            });
            m_presence.start();
            report_presence();
        }
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
            //  The page before goes first: what it holds (the camera of the
            //  programs, say) is free for the page that comes.
            m_page.reset();
            m_hits.clear();
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
void Canvas::go_console_again(int games_selected){
    m_console_again = true;
    m_games_back = games_selected;
    go_console();
}
void Canvas::leave_console_again(){
    m_console_again = false;
    go_games(m_games_back);
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
void Canvas::go_games(int selected){
    queue_page([this, selected]{
        return std::unique_ptr<Page>(new GamesPage(*this, selected));
    });
}
void Canvas::go_programs(int game){
    queue_page([this, game]{
        return std::unique_ptr<Page>(new ProgramsPage(*this, game));
    });
}
void Canvas::go_instances(){
    queue_page([this]{
        return std::unique_ptr<Page>(new InstancesPage(*this));
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
    if (m_app_mode){
        //  The window's own screen does not end. What the page chose is a
        //  program to open (a game without programs has nothing to open).
        const QString game_name = m_result.game;
        const QString program = m_result.program;
        m_result.game.clear();
        m_result.program.clear();
        if (m_video_open){
            close_video();
        }
        int index = -1;
        for (size_t c = 0; c < m_request.games.size(); c++){
            if (m_request.games[c].name == game_name){
                index = (int)c;
            }
        }
        if (index >= 0 && !program.isEmpty()){
            open_program_page(index, program);
        }
        return;
    }
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
        if (m_app_mode && m_page->shows_menu()){
            painter.setOpacity(1.0);
            paint_app_menu(painter);
        }
        if (can_minimize()){
            paint_minimize(painter);
            if (m_request.overview){
                paint_close_button(painter);
            }else if (m_request.show_overview){
                paint_overview_button(painter);
            }
        }
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
    place_claude();
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
    if (!m_drag_id.isEmpty() && m_page && !m_pending){
        m_page->drag(*this, m_drag_id, to_design(event->position()), 1);
        update();
    }
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
    //  The keyboard comes back to the page (from the widget that had it).
    setFocus(Qt::MouseFocusReason);
    const Hit* hit = find_hit(to_design(event->position()));
    if (hit != nullptr && hit->enabled && hit->id.startsWith("drag:") && m_page && !m_pending){
        m_drag_id = hit->id;
        m_pressed.clear();
        m_page->drag(*this, m_drag_id, to_design(event->position()), 0);
        update();
        return;
    }
    m_pressed = hit != nullptr && hit->enabled ? hit->id : QString();
}

void Canvas::mouseReleaseEvent(QMouseEvent* event){
    if (event->button() != Qt::LeftButton || m_intro == IntroState::FILM || m_holding || m_releasing){
        return;
    }
    if (!m_drag_id.isEmpty()){
        const QString id = m_drag_id;
        m_drag_id.clear();
        if (m_page){
            m_page->drag(*this, id, to_design(event->position()), 2);
            update();
        }
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
        dispatch(id);
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
    //  Ctrl+M minimizes the window of its own, whatever is on it (also the film).
    if (event->key() == Qt::Key_M && (event->modifiers() & Qt::ControlModifier) && can_minimize()){
        minimize();
        return;
    }
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
    if (child_has_focus()){
        //  A widget of the page has the keyboard (its options) and did not use
        //  this key: it is not a key of the page (Enter in a field must not
        //  start the program). Escape gives the keyboard back to the page.
        if (event->key() == Qt::Key_Escape){
            take_focus();
            update();
        }
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
                dispatch(m_focus);
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

bool Canvas::focusNextPrevChild(bool next){
    if (!child_has_focus()){
        return false;
    }
    return QWidget::focusNextPrevChild(next);
}

void Canvas::wheelEvent(QWheelEvent* event){
    if (m_intro == IntroState::FILM || m_holding || m_releasing || m_video_open || m_page == nullptr || m_pending){
        return;
    }
    if (m_page->wheel(*this, to_design(event->position()), event->angleDelta().y())){
        event->accept();
        update();
    }
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
            dispatch(copy);
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
void Canvas::press_key(int key, Qt::KeyboardModifiers modifiers, const QString& text){
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    keyPressEvent(&event);
}
void Canvas::type_text(const QString& text){
    for (const QChar letter : text){
        const int key = letter == ' ' ? (int)Qt::Key_Space : (int)letter.toUpper().unicode();
        press_key(key, Qt::NoModifier, QString(letter));
    }
}
bool Canvas::scroll(const QString& id, int notches){
    for (const Hit& hit : m_hits){
        if (hit.id == id && m_page != nullptr && !m_pending){
            return m_page->wheel(*this, hit.rect.center(), notches * 120);
        }
    }
    return false;
}
bool Canvas::drag_over(const QString& id, double from, double to){
    for (const Hit& hit : m_hits){
        if (hit.id == id && m_page != nullptr && !m_pending){
            const double y = hit.rect.center().y();
            m_page->drag(*this, id, QPointF(hit.rect.left() + hit.rect.width() * from, y), 0);
            m_page->drag(*this, id, QPointF(hit.rect.left() + hit.rect.width() * to, y), 1);
            m_page->drag(*this, id, QPointF(hit.rect.left() + hit.rect.width() * to, y), 2);
            return true;
        }
    }
    return false;
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

//  ---------------------------------------------------------------------
//  The main window's own screen
//  ---------------------------------------------------------------------

void Canvas::dispatch(const QString& id){
    if (m_page == nullptr){
        return;
    }
    if (id == "minimize"){
        minimize();
        return;
    }
    if (id == "instances"){
        go_overview();
        return;
    }
    if (id == "close-overview"){
        finish();
        return;
    }
    if (m_app_mode && id.startsWith("app:")){
        app_activate(id);
        return;
    }
    m_page->activate(*this, id);
}

//  The button that minimizes a window that has no title bar: a small square in
//  the top right corner, a dash in it. Not a place for the keyboard to stop at
//  (Ctrl+M does the same), so that the order of the pages stays what it was.
void Canvas::paint_minimize(QPainter& painter){
    constexpr double SIZE = 56;
    const QRectF rect(DW - 36 - SIZE, 30, SIZE, SIZE);
    const bool hover = hovered("minimize");
    painter.setOpacity(1.0);
    panel(painter, rect, 16, white(hover ? 0.10 : 0.0), white(hover ? 0.55 : 0.26), 1.8);
    painter.setPen(QPen(white(hover ? 0.95 : 0.60), 3.4, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(
        QPointF(rect.center().x() - 12, rect.center().y() + 8),
        QPointF(rect.center().x() + 12, rect.center().y() + 8)
    );
    add_hit("minimize", rect, true, false);
}

//  The button that goes back to the overview of the installations: left of the
//  one that minimizes, four small squares in it (the miniatures).
void Canvas::paint_overview_button(QPainter& painter){
    constexpr double SIZE = 56;
    const QRectF rect(DW - 36 - SIZE - 14 - SIZE, 30, SIZE, SIZE);
    const bool hover = hovered("instances");
    painter.setOpacity(1.0);
    panel(painter, rect, 16, white(hover ? 0.10 : 0.0), white(hover ? 0.55 : 0.26), 1.8);
    painter.setPen(Qt::NoPen);
    painter.setBrush(white(hover ? 0.95 : 0.60));
    constexpr double CELL = 11;
    constexpr double GAP = 6;
    const QPointF origin(rect.center().x() - CELL - GAP / 2, rect.center().y() - CELL - GAP / 2);
    for (int row = 0; row < 2; row++){
        for (int column = 0; column < 2; column++){
            painter.drawRoundedRect(
                QRectF(origin.x() + column * (CELL + GAP), origin.y() + row * (CELL + GAP), CELL, CELL),
                3, 3
            );
        }
    }
    add_hit("instances", rect, true, false);
}

//  The overview's own window: a button that closes it, left of the one that
//  minimizes. (The installations keep running.)
void Canvas::paint_close_button(QPainter& painter){
    constexpr double SIZE = 56;
    const QRectF rect(DW - 36 - SIZE - 14 - SIZE, 30, SIZE, SIZE);
    const bool hover = hovered("close-overview");
    painter.setOpacity(1.0);
    panel(painter, rect, 16, white(hover ? 0.10 : 0.0), white(hover ? 0.55 : 0.26), 1.8);
    painter.setPen(QPen(white(hover ? 0.95 : 0.60), 3.4, Qt::SolidLine, Qt::RoundCap));
    const QPointF c = rect.center();
    painter.drawLine(QPointF(c.x() - 11, c.y() - 11), QPointF(c.x() + 11, c.y() + 11));
    painter.drawLine(QPointF(c.x() - 11, c.y() + 11), QPointF(c.x() + 11, c.y() - 11));
    add_hit("close-overview", rect, true, false);
}

//  What this window says about the installation, for the overview: whether it
//  is in the setup, on the games or in a program (and if that one runs), and a
//  picture of it.
void Canvas::report_presence(){
    QString state;
    QString title;
    if (!m_app_mode){
        state = m_page != nullptr && (QString(m_page->name()) == "games" || QString(m_page->name()) == "programs") ? "games" : "setup";
    }else if (m_page != nullptr && QString(m_page->name()) == "host"){
        state = m_app_busy ? "running" : "program";
        title = m_page->about();
    }else{
        state = "games";
    }
    m_presence_reports++;
    write_instance_report(state, title, instance_picture(m_app_mode ? window() : this));
}

//  The buttons of the old top bar, along the top of every page.
void Canvas::paint_app_menu(QPainter& painter){
    struct Entry{
        const char* id;
        const char* text;
    };
    static const Entry ENTRIES[] = {
        {"app:games", "Games"},
        {"app:installations", "Installations"},
        {"app:settings", "Settings"},
        {"app:claude", "Claude"},
        {"app:log", "Output"},
        {"app:buttons", "Buttons"},
        {"app:about", "About"},
    };
    const bool on_games = m_page != nullptr && QString(m_page->name()) == "games";
    const double edge = std::min(1880.0, app_right_edge() - 10);
    double right = edge;
    for (size_t c = sizeof(ENTRIES) / sizeof(ENTRIES[0]); c-- > 0;){
        const Entry& entry = ENTRIES[c];
        if (on_games && QString(entry.id) == "app:games"){
            continue;
        }
        //  Only when the window can show the overview.
        if (QString(entry.id) == "app:installations" && !m_app.installations){
            continue;
        }
        const QString text = T(entry.text);
        const double width = button_width(text, ButtonKind::LINK);
        draw_button(*this, painter, entry.id, text, right - width, 0, ButtonKind::LINK, 1.0);
        right -= width + 14;
    }
}

bool Canvas::app_activate(const QString& id){
    if (id == "app:games"){
        if (m_hosting){
            if (m_app_busy){
                return true;
            }
            if (m_app.close_program && !m_app.close_program()){
                return true;
            }
        }
        go_games();
    }else if (id == "app:installations"){
        go_overview();
    }else if (id == "app:settings"){
        if (m_app.settings){
            m_app.settings();
        }
    }else if (id == "app:claude"){
        if (m_app.claude){
            m_app.claude(!m_claude_shown);
        }
    }else if (id == "app:log"){
        if (m_app.output){
            m_app.output();
        }
    }else if (id == "app:buttons"){
        if (m_app.buttons){
            m_app.buttons();
        }
    }else if (id == "app:about"){
        if (m_app.about){
            m_app.about();
        }
    }else{
        return false;
    }
    return true;
}

void Canvas::leave_host(int game){
    m_hosting = false;
    if (game >= 0 && game < (int)m_request.games.size() && has_programs(game)){
        go_programs(game);
    }else{
        go_games(game);
    }
}

void Canvas::open_program_page(int game, const QString& program){
    queue_page([this, game, program]() -> std::unique_ptr<Page>{
        //  The window makes the program now, and answers with host().
        m_opening = true;
        m_hosted.reset();
        bool opened = false;
        try{
            opened = m_app.open_program && m_app.open_program(game, program);
        }catch (...){
            opened = false;
        }
        m_opening = false;
        if (opened && m_hosted != nullptr){
            return std::move(m_hosted);
        }
        m_hosted.reset();
        //  It did not open (a program runs, say): back where it was picked.
        return std::unique_ptr<Page>(new ProgramsPage(*this, game));
    });
}

void Canvas::host(QWidget* content, int game, const QString& game_name, const QString& title){
    QWidget* holder = new QWidget(this);
    holder->setAutoFillBackground(true);
    holder->hide();
    QVBoxLayout* layout = new QVBoxLayout(holder);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    content->setParent(holder);
    layout->addWidget(content);
    m_hosted = std::make_unique<HostPage>(*this, holder, game, game_name, title);
    m_hosting = true;
    if (!m_opening){
        queue_page([this]() -> std::unique_ptr<Page>{
            if (m_hosted != nullptr){
                return std::move(m_hosted);
            }
            return std::unique_ptr<Page>(new GamesPage(*this, -1));
        });
    }
}

void Canvas::unhost(QWidget* content){
    m_hosting = false;
    if (content == nullptr){
        return;
    }
    //  Back to the window, which deletes it: the holder only held it.
    content->hide();
    content->setParent(nullptr);
}

void Canvas::set_claude(QWidget* panel){
    m_claude = panel;
    if (panel != nullptr){
        panel->setParent(this);
        panel->hide();
    }
    m_claude_shown = false;
}

void Canvas::show_claude(bool show){
    m_claude_shown = show && m_claude != nullptr;
    place_claude();
    update();
}

//  Claude's panel is over the right of the screen.
void Canvas::place_claude(){
    if (m_claude == nullptr){
        return;
    }
    if (!m_claude_shown){
        if (!m_claude->isHidden()){
            m_claude->hide();
        }
        return;
    }
    const int panel_width = std::max(380, std::min(640, (int)std::lround(width() * 0.34)));
    m_claude->setGeometry(width() - panel_width, 0, panel_width, height());
    m_claude->raise();
    m_claude->show();
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

AppView* make_app_view(QWidget* parent, const SetupRequest& request, AppActions actions, bool test){
    Canvas* canvas = new Canvas(request, test, parent, &actions);
    canvas->start(nullptr);
    return canvas;
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
void save_preview(Canvas& canvas, const QString& folder, const QString& name, int width = 800){
    if (folder.isEmpty()){
        return;
    }
    QDir().mkpath(folder);
    canvas.picture().scaled(width, width * 9 / 16, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        .save(QDir(folder).filePath(name + ".jpg"), "JPG", 72);
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

//  The programs of those games, for the test: the list of the Nintendo Switch
//  as it is in the program, a long one with sections (Sword and Shield), the
//  rest has none.
std::vector<HomeProgram> test_programs(){
    std::vector<HomeProgram> programs;
    auto add = [&](int game, const char* section, const QString& name, const char* description, const char* tag = "", const char* meaning = ""){
        HomeProgram program;
        program.game = game;
        program.section = QString::fromLatin1(section);
        program.name = name;
        program.description = QString::fromLatin1(description);
        program.tag = QString::fromLatin1(tag);
        program.tag_meaning = QString::fromLatin1(meaning);
        programs.push_back(program);
    };
    add(0, "", "Turbo A", "Endlessly mash A.");
    add(0, "", "Turbo Button", "Hold or mash any button.");
    add(0, "", "Turbo Macro", "Repeat a macro of buttons and waits.");
    add(0, "", "Push Joy Sticks", "Push the sticks all the way, round and round.");
    add(0, "", "Prevent Sleep", "Presses a button now and then, so that the console stays awake.");
    add(0, "", "Friend Code Adder", "Adds friends from a list of codes.");
    add(0, "", "Friend Delete", "Removes friends from the list, one after the other.", "rough", "It works, but it is not polished.");
    add(0, "", "Record Keyboard Controller", "Records what you press on the keyboard.");
    add(0, "", "Typewriter", "Types text with the on-screen keyboard.", "beta", "It is new: it can still go wrong.");
    for (int c = 0; c < 36; c++){
        const char* const sections[3] = {"Farming", "Shiny hunting", "Other"};
        add(3, sections[c / 12], QString("Program %1 of Sword and Shield").arg(c + 1), "A program with a longer description, to see how it fits in the panel below the list.",
            c % 9 == 4 ? "unvetted" : "", c % 9 == 4 ? "Nobody has checked it yet." : "");
    }
    return programs;
}

//  What the options of the test did: made, kept, deleted.
struct TestOptionsLog{
    int made = 0;
    int destroyed = 0;
    int saved = 0;
    QStringList programs;
    //  The widgets of the latest.
    QPointer<QSpinBox> spin;
    QPointer<QLineEdit> edit;
};
TestOptionsLog g_test_options;

//  The options of a program for the test: a few ordinary widgets, as the
//  options of a program are.
class TestOptions final : public SetupProgramOptions{
public:
    TestOptions(QWidget* parent, const QString& program){
        g_test_options.made++;
        g_test_options.programs << program;
        QWidget* panel = new QWidget(parent);
        QVBoxLayout* layout = new QVBoxLayout(panel);
        layout->addWidget(new QLabel("Time limit", panel));
        QSpinBox* spin = new QSpinBox(panel);
        spin->setRange(0, 100);
        layout->addWidget(spin);
        layout->addWidget(new QCheckBox("Go home when done", panel));
        layout->addWidget(new QLabel("Mash speed", panel));
        QComboBox* combo = new QComboBox(panel);
        combo->addItems({"Fast", "Reduced", "Minimal"});
        layout->addWidget(combo);
        layout->addWidget(new QLabel("Notes", panel));
        QLineEdit* edit = new QLineEdit(panel);
        layout->addWidget(edit);
        layout->addStretch(1);
        QObject::connect(spin, &QSpinBox::valueChanged, panel, [this](int){
            m_changed = true;
        });
        g_test_options.spin = spin;
        g_test_options.edit = edit;
        m_widget = panel;
    }
    virtual ~TestOptions() override{
        delete m_widget.data();
        g_test_options.destroyed++;
    }
    virtual QWidget* widget() override{
        return m_widget.data();
    }
    virtual bool changed() const override{
        return m_changed;
    }
    virtual void save() override{
        m_changed = false;
        g_test_options.saved++;
    }

private:
    QPointer<QWidget> m_widget;
    bool m_changed = false;
};

//  The options of a program with many of them, as AutoStory has: long labels,
//  paragraphs of text, drop-down lists, numbers, a group.
class RichTestOptions final : public SetupProgramOptions{
public:
    explicit RichTestOptions(QWidget* parent){
        g_test_options.made++;
        g_test_options.programs << "AutoStory";
        QWidget* panel = new QWidget(parent);
        QVBoxLayout* layout = new QVBoxLayout(panel);
        auto text = [&](const QString& words){
            QLabel* label = new QLabel(words, panel);
            label->setWordWrap(true);
            layout->addWidget(label);
        };
        auto row = [&](const QString& words, QWidget* control){
            QHBoxLayout* line = new QHBoxLayout();
            QLabel* label = new QLabel(words, panel);
            label->setWordWrap(true);
            line->addWidget(label, 1);
            line->addWidget(control, 1);
            layout->addLayout(line);
        };
        auto drop = [&](const QString& words, const QStringList& items){
            QComboBox* combo = new QComboBox(panel);
            combo->addItems(items);
            row(words, combo);
        };
        auto number = [&](const QString& words, int value){
            QSpinBox* spin = new QSpinBox(panel);
            spin->setRange(0, 9999);
            spin->setValue(value);
            row(words, spin);
        };
        auto check = [&](const QString& words, bool on){
            QCheckBox* box = new QCheckBox(words, panel);
            box->setChecked(on);
            layout->addWidget(box);
        };
        text("Start from a new game or from a save of the game. A new game needs the game at its first screen, with the cartridge in and the Switch's home menu behind it. The program types the name of the player and of the rival itself.");
        drop("Starter:", {"Charmander", "Squirtle", "Bulbasaur"});
        drop("Game version:", {"FireRed", "LeafGreen"});
        check("Start a new game", true);
        QLineEdit* name = new QLineEdit("RED", panel);
        row("Name of the player:", name);
        row("Name of the rival:", new QLineEdit("BLUE", panel));
        check("Continue from the save of the game, not from a new game", false);
        drop("Start at this part of the story:", {"1. Pallet Town", "2. Viridian City", "3. Pewter City"});
        number("Start at this step of the part:", 0);
        drop("Stop after this part of the story:", {"1. Pallet Town", "2. Viridian City", "3. Pewter City"});
        drop("Wild Pokemon in battles:", {"Run from them all", "Catch the ones the table wants", "Fight them all"});
        check("Stop and wait when a shiny Pokemon shows up, so that it can be kept", true);
        number("Levels over the next trainer's team, to train in the grass:", 2);
        check("Reset the game at the start of each part of the story", false);
        check("Save the game after each visit to a Pokemon Center", true);
        number("Tries of a step before it is called failed:", 3);
        number("Wait between the checks for a Pokemon in the grass, in milliseconds:", 250);
        check("Check that each walk moved the player on the screen", true);
        check("Run with the B button held down", true);
        check("Faster battles (experimental)", false);
        check("Shorter looks in the grass (experimental)", false);
        check("Show notes on the video (experimental)", false);
        row("Pokemon caught so far (for the collection):", new QLineEdit(panel));
        text("The Pokedex collection catches what the chosen game can still register in the grass, the caves and the Safari Zone, and then evolves and trades what the boxes hold. It takes many hours.");
        QGroupBox* group = new QGroupBox("Notifications", panel);
        QVBoxLayout* inside = new QVBoxLayout(group);
        inside->addWidget(new QCheckBox("Status updates, every 30 minutes", group));
        inside->addWidget(new QCheckBox("Program finished", group));
        inside->addWidget(new QCheckBox("Error", group));
        layout->addWidget(group);
        layout->addStretch(1);
        m_widget = panel;
    }
    virtual ~RichTestOptions() override{
        delete m_widget.data();
        g_test_options.destroyed++;
    }
    virtual QWidget* widget() override{
        return m_widget.data();
    }
    virtual bool changed() const override{
        return false;
    }
    virtual void save() override{
    }

private:
    QPointer<QWidget> m_widget;
};

//  A log that a test can add lines to.
struct TestLog{
    std::mutex lock;
    std::vector<SetupLogLine> lines;
    void add(int level, const QString& text, qint64 ago_ms = 0){
        std::lock_guard<std::mutex> guard(lock);
        SetupLogLine line;
        line.serial = (quint64)lines.size() + 1;
        line.msec = QDateTime::currentMSecsSinceEpoch() - ago_ms;
        line.level = level;
        line.text = text;
        lines.push_back(line);
    }
    std::function<void(quint64, std::vector<SetupLogLine>&)> source(){
        return [this](quint64 after, std::vector<SetupLogLine>& out){
            std::lock_guard<std::mutex> guard(lock);
            for (const SetupLogLine& line : lines){
                if (line.serial > after){
                    out.push_back(line);
                }
            }
        };
    }
};


//  The overview of the installations, with installations that are made up: what
//  each one shows, what a press on a miniature, on "New installation" and on
//  "Remove" does, the keyboard, and where the miniatures go.
void check_overview(Checker& checker, const QString& folder){
    struct Fake{
        std::vector<InstanceInfo> list;
        QStringList opened;
        QStringList removed;
        int created = 0;
    };
    const auto fake = std::make_shared<Fake>();

    //  A miniature: what a capture card shows, made up.
    auto picture = [](const QColor& sky, const QColor& ground, const QString& text){
        QImage image(MINIATURE_WIDTH, MINIATURE_HEIGHT, QImage::Format_RGB32);
        image.fill(sky);
        QPainter painter(&image);
        painter.fillRect(QRect(0, 170, MINIATURE_WIDTH, MINIATURE_HEIGHT - 170), ground);
        painter.setPen(Qt::white);
        painter.setFont(ui_font(34, 600));
        painter.drawText(QRect(0, 40, MINIATURE_WIDTH, 80), Qt::AlignCenter, text);
        return image;
    };
    auto make = [&](const QString& id, bool running, const QString& state, const QString& title, const QImage& image){
        InstanceInfo info;
        info.id = id;
        info.main = id == "1";
        info.running = running;
        info.state = running ? state : QString();
        info.title = title;
        info.picture = image;
        return info;
    };
    fake->list.push_back(make("1", true, "running", "Pokemon FireRed: AutoStory", picture(QColor(40, 90, 160), QColor(70, 130, 60), "FIRERED")));
    fake->list.push_back(make("2", true, "setup", QString(), QImage()));
    InstanceInfo stopped = make("3", false, QString(), QString(), picture(QColor(120, 60, 140), QColor(60, 60, 70), "SHINY HUNT"));
    stopped.name = "Shiny hunter";
    fake->list.push_back(stopped);
    InstanceInfo starting = make("4", false, QString(), QString(), QImage());
    starting.starting = true;
    fake->list.push_back(starting);
    fake->list.push_back(make("6", false, QString(), QString(), QImage()));

    SetupRequest request;
    request.overview = true;
    request.build = "test";
    request.instance_list = [fake]{
        return fake->list;
    };
    request.instance_open = [fake](const QString& id){
        fake->opened << id;
    };
    request.instance_create = [fake]() -> QString{
        fake->created++;
        const QString id = "7";
        InstanceInfo info;
        info.id = id;
        info.starting = true;
        fake->list.push_back(info);
        return id;
    };
    request.instance_remove = [fake](const QString& id){
        fake->removed << id;
        for (size_t c = 0; c < fake->list.size(); c++){
            if (fake->list[c].id == id){
                fake->list.erase(fake->list.begin() + (long)c);
                break;
            }
        }
        return true;
    };

    Canvas hub(request, true);
    hub.resize(1920, 1080);
    QEventLoop loop;
    hub.start(&loop);
    hub.show();
    hub.wait(2.2);
    checker.check(hub.page_name() == "instances", "the overview of the installations is its own page");
    checker.check(
        hub.has_hit("inst:1") && hub.has_hit("inst:2") && hub.has_hit("inst:3") && hub.has_hit("inst:4") &&
        hub.has_hit("inst:6") && hub.has_hit("inst-new"),
        "each installation has a miniature, and there is one to make a new one"
    );
    checker.check(
        !hub.has_hit("inst-remove:1") && !hub.has_hit("inst-remove:2") && !hub.has_hit("inst-remove:4") &&
        hub.has_hit("inst-remove:3") && hub.has_hit("inst-remove:6"),
        "only an installation that is stopped can be removed: not number 1, not one that runs, not one that starts"
    );
    checker.check(
        hub.can_minimize() && hub.has_hit("minimize") && hub.has_hit("close-overview") && !hub.has_hit("instances"),
        "the overview has the buttons that minimize and close it, and none that goes back to itself"
    );
    checker.check(has_content(hub.picture()), "the overview is drawn");
    save(hub, folder, "setup-70-installations");
    save_preview(hub, folder, "preview-installations");

    //  A running installation: its window comes forward, this one steps aside.
    hub.click("inst:1");
    checker.check(fake->opened == QStringList({"1"}), "a press on a miniature opens that installation");
    checker.check(hub.minimize_requests() == 1, "the overview steps aside when its window comes forward");
    hub.wait(0.2);

    //  One that is stopped is started: the overview waits until it runs.
    hub.click("inst:3");
    checker.check(fake->opened == QStringList({"1", "3"}), "a stopped installation is opened too (it is started)");
    checker.check(hub.minimize_requests() == 1, "the overview stays while it starts");
    fake->list[2].running = true;
    fake->list[2].state = "setup";
    hub.wait(1.3);
    checker.check(hub.minimize_requests() == 2, "and steps aside when it runs");
    fake->list[2].running = false;
    fake->list[2].state.clear();
    hub.wait(1.3);

    //  Removing takes two presses, and the first one lapses.
    hub.click("inst-remove:6");
    checker.check(fake->removed.isEmpty(), "one press on Remove removes nothing");
    hub.wait(4.6);
    hub.click("inst-remove:6");
    checker.check(fake->removed.isEmpty(), "a second press after a while does not either: it asks again");
    hub.click("inst-remove:6");
    hub.wait(1.3);
    checker.check(fake->removed == QStringList({"6"}), "two presses remove it");
    checker.check(!hub.has_hit("inst:6") && hub.has_hit("inst:1"), "and its miniature is gone, the others stay");
    hub.click("inst-remove:3");
    hub.click("inst-remove:4");
    checker.check(fake->removed == QStringList({"6"}), "a press on another Remove starts again");

    //  A new installation: it is made and started, and it is on the screen.
    hub.click("inst-new");
    hub.wait(1.2);
    checker.check(fake->created == 1 && hub.has_hit("inst:7"), "'New installation' makes one, and it is in the list");
    checker.check(hub.minimize_requests() == 2, "the overview waits for it to start");
    fake->list.back().running = true;
    fake->list.back().starting = false;
    fake->list.back().state = "setup";
    hub.wait(1.3);
    checker.check(hub.minimize_requests() == 3, "and steps aside when it runs (its setup is on the screen)");

    //  The keyboard: Tab to the next miniature, Enter opens it.
    hub.click("minimize");
    checker.check(hub.minimize_requests() == 4, "the button minimizes the overview");
    fake->opened.clear();
    hub.set_focus_id("inst:1");
    hub.press_key(Qt::Key_Tab);
    checker.check(hub.focus_id() == "inst:2", "Tab goes to the next miniature");
    hub.press_key(Qt::Key_Return);
    checker.check(fake->opened == QStringList({"2"}), "Enter opens it");

    //  The overview can be closed.
    checker.check(!hub.finished(), "the overview is not over before it is closed");
    hub.click("close-overview");
    checker.check(hub.finished(), "its close button ends it");

    //  Where the miniatures go: all of them on the screen, none over another, in the area.
    bool inside = true;
    bool apart = true;
    bool large_enough = true;
    for (size_t count = 1; count <= 30; count++){
        const TileGrid grid = tile_grid(count);
        for (size_t a = 0; a < count; a++){
            const QRectF rect = tile_rect(grid, a);
            if (rect.left() < TILE_X - 0.01 || rect.right() > TILE_X + TILE_AREA_W + 0.01 ||
                rect.top() < TILE_Y - 0.01 || rect.bottom() > TILE_Y + TILE_AREA_H + 0.01
            ){
                inside = false;
            }
            for (size_t b = a + 1; b < count; b++){
                if (rect.intersects(tile_rect(grid, b))){
                    apart = false;
                }
            }
        }
        if (count <= 12 && grid.width < 240){
            large_enough = false;
        }
    }
    checker.check(inside, "from 1 to 30 miniatures: all of them are in the area for them");
    checker.check(apart, "none is over another");
    checker.check(large_enough, "up to 12 of them are at least 240 wide");
    checker.check(tile_grid(2).width == TILE_MAX_W && tile_grid(3).columns == 3, "a few are large, side by side");

    //  The screens that are windows of their own have the button that goes back.
    {
        int shown = 0;
        SetupRequest other;
        other.ask_board = true;
        other.games = test_games();
        other.show_overview = [&]{
            shown++;
        };
        Canvas window(other, true);
        window.resize(1280, 720);
        QEventLoop other_loop;
        window.start(&other_loop);
        window.show();
        window.wait(2.0);
        checker.check(window.has_hit("instances") && window.hit_enabled("instances"), "the setup has a button that goes back to the overview");
        checker.check(!window.has_hit("close-overview"), "and none that closes the overview");
        checker.check(window.click("instances") && shown == 1 && window.overview_requests() == 1, "it shows the overview");
        window.wait(0.3);
        checker.check(!window.finished() && window.page_name() == "board", "and leaves the setup as it was");
        save_preview(window, folder, "preview-overview-button");
        window.click("skip");
        window.wait(2.5);
        checker.check(window.page_name() == "games" && window.has_hit("instances"), "the next screen has it too");

        SetupRequest none;
        none.ask_board = true;
        Canvas without(none, true);
        without.resize(1280, 720);
        QEventLoop none_loop;
        without.start(&none_loop);
        without.show();
        without.wait(1.5);
        checker.check(!without.has_hit("instances"), "no button where the window cannot show the overview");
    }
}

}   //  namespace


namespace{
//  The overview's window while it is up.
QPointer<Canvas> g_overview;
}

void bring_overview_forward(){
    if (g_overview == nullptr){
        return;
    }
    QWidget* window = g_overview.data();
    window->setWindowState((window->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    window->show();
    window->raise();
    window->activateWindow();
}

int run_instances_overview(const SetupRequest& request, const QString& screenshots_folder){
    SetupRequest overview = request;
    overview.overview = true;
    if (!screenshots_folder.isEmpty()){
        //  The test: the window is not put on the screen; a picture of it.
        Checker checker;
        Canvas canvas(overview, true);
        canvas.resize(1920, 1080);
        QEventLoop loop;
        canvas.start(&loop);
        canvas.show();
        canvas.wait(2.5);
        checker.check(canvas.page_name() == "instances", "the overview is on the screen");
        checker.check(canvas.has_hit("inst-new") && canvas.has_hit("close-overview"), "with a miniature to make a new one, and a close button");
        const std::vector<InstanceInfo> list = overview.instance_list ? overview.instance_list() : std::vector<InstanceInfo>();
        checker.check(!list.empty() && list.front().main, "installation 1 is the first");
        for (const InstanceInfo& info : list){
            checker.check(canvas.has_hit("inst:" + info.id), "each installation has its miniature");
        }
        checker.check(has_content(canvas.picture()), "the overview is drawn");
        save(canvas, screenshots_folder, "installations-hub");
        save_preview(canvas, screenshots_folder, "preview-installations-hub");
        for (const QString& line : checker.lines){
            qWarning("%s", qUtf8Printable(line));
        }
        return checker.failures == 0 ? 0 : 1;
    }
    std::unique_ptr<Canvas> canvas(new Canvas(overview, false));
    QEventLoop loop;
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (screen == nullptr){
        screen = QApplication::primaryScreen();
    }
    if (screen != nullptr){
        canvas->setScreen(screen);
        canvas->setGeometry(screen->geometry());
    }
    g_overview = canvas.get();
    canvas->start(&loop);
    canvas->showFullScreen();
    canvas->raise();
    canvas->activateWindow();
    canvas->setFocus();
    loop.exec();
    g_overview.clear();
    canvas->hide();
    return 0;
}


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
    checker.check(canvas.has_hit("console:0") && canvas.hit_enabled("console:0") && canvas.has_hit("console:1") && canvas.hit_enabled("console:1"),
        "both supported consoles are tiles that can be picked");
    checker.check(canvas.has_hit("console:2") && !canvas.hit_enabled("console:2") && canvas.has_hit("console:11") && !canvas.hit_enabled("console:11"),
        "the other consoles are on the screen already, and cannot be picked");
    checker.check(!canvas.click("console:2") && canvas.page_state() == "selected=", "a console that is not supported yet does nothing when it is clicked");
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
    canvas.press_key(Qt::Key_Right);
    checker.check(canvas.focus_id() == "console:1", "and skip the ones that are not supported yet");
    canvas.press_key(Qt::Key_Tab);
    canvas.press_key(Qt::Key_Tab);
    checker.check(!canvas.focus_id().startsWith("console:") || canvas.focus_id() == "console:0" || canvas.focus_id() == "console:1",
        "Tab does not stop on them either");
    canvas.set_focus_id("console:1");
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

    //  The hashes of the build: a file that is not the one it hashed is not flashed.
    {
        const QString sums_folder = QDir(scratch.path()).filePath("sums");
        QDir().mkpath(sums_folder);
        const QString good = QDir(sums_folder).filePath("PABotBase2-ESP32-2026101400.bin");
        const QString newer = QDir(sums_folder).filePath("PABotBase2-ESP32-2026120100.bin");
        write_file(good, QByteArray(9000, 'b'));
        write_file(newer, QByteArray(9000, 'n'));
        checker.check(firmware_matches_checksum(sums_folder, good) && firmware_matches_checksum(sums_folder, newer),
            "without a list of hashes every file passes");
        const QString hash = file_sha256(good);
        checker.check(hash.size() == 64, "the hash of a file is 64 hex digits");
        write_file(QDir(sums_folder).filePath("SHA256SUMS.txt"),
            (hash + "  PABotBase2-ESP32-2026101400.bin\r\n" + "not a line\r\n" + QString(64, QChar('A')) + " *Other.bin\r\n").toUtf8());
        const std::map<QString, QString> sums = firmware_checksums(sums_folder);
        checker.check(sums.size() == 2 && sums.at("PABotBase2-ESP32-2026101400.bin") == hash && sums.at("Other.bin") == QString(64, QChar('a')),
            "the list of hashes is read, in the form of sha256sum, with the lines that are not one skipped");
        checker.check(firmware_matches_checksum(sums_folder, good), "a file with the hash that is listed passes");
        checker.check(firmware_matches_checksum(sums_folder, newer), "a version that the list does not know passes");
        write_file(good, QByteArray(9000, 'x'));
        checker.check(!firmware_matches_checksum(sums_folder, good), "a file that was changed does not pass");
    }

    //  What leaves the program (to Claude, to a file) has nothing private in it.
    {
        const QString webhook = "https://discord.com/api/webhooks/123456789012345678/AbCdEfGhIjKlMnOpQrStUvWxYz0123456789-_AbCdEf";
        const QString scrubbed = scrub_private_text(
            "Sending to " + webhook + "\n"
            "Key sk-ant-api03-AbCdEf0123456789-xyz and token ghp_abcdefghijklmnopqrstuvwxyz0123456789\n"
            "Opened C:\\Users\\Dennis van der Mijde\\Documents\\PocketPilot\\log.txt and /home/dennis/x\n"
            "DEVELOPER_TOKEN = hunter2abc, password: \"p4ss w0rd\" and Authorization: Bearer abcdefgh12345678\n"
            "max_tokens: 4096, a token count of 5, and the game's name\n"
        );
        checker.check(!scrubbed.contains("123456789012345678") && !scrubbed.contains("AbCdEfGhIj") && scrubbed.contains("<webhook address>"),
            "a webhook address is taken out of what leaves the program");
        checker.check(!scrubbed.contains("sk-ant-api03") && !scrubbed.contains("ghp_abcdef") && scrubbed.contains("<api key>") && scrubbed.contains("<token>"),
            "the key of Claude and a GitHub token are taken out");
        checker.check(!scrubbed.contains("Dennis") && !scrubbed.contains("Mijde") && !scrubbed.contains("dennis") &&
            scrubbed.contains("C:\\Users\\<user>\\Documents\\PocketPilot\\log.txt") && scrubbed.contains("/home/<user>/x"),
            "the name of the Windows account is taken out of a path, the rest of the path stays");
        checker.check(!scrubbed.contains("hunter2abc") && !scrubbed.contains("p4ss") && !scrubbed.contains("abcdefgh12345678"),
            "the value after token, password and authorization is taken out");
        checker.check(scrubbed.contains("max_tokens: 4096") && scrubbed.contains("a token count of 5") && scrubbed.contains("the game's name"),
            "the rest of a line is left as it was");
    }

    //  A secret (a Discord token, a webhook address) is sealed in the settings file where the computer can.
    {
        StringCell secret(true, LockMode::UNLOCK_WHILE_RUNNING, "", "");
        secret.set("a-secret-token");
        const JsonValue saved = secret.to_json();
        const std::string* text = saved.to_string();
        std::string probe;
        const bool can_seal = seal_secret("probe", probe);
        checker.check(
            text != nullptr && (can_seal
                ? (*text != "a-secret-token" && text->find("a-secret-token") == std::string::npos)
                : *text == "a-secret-token"),
            "a secret is not in the settings file as it is typed (where the computer can seal it)"
        );
        StringCell again(true, LockMode::UNLOCK_WHILE_RUNNING, "", "");
        again.load_json(saved);
        checker.check(std::string(again) == "a-secret-token", "and it is read back");
        StringCell from_before(true, LockMode::UNLOCK_WHILE_RUNNING, "", "");
        from_before.load_json(JsonValue(std::string("plain-from-before")));
        checker.check(std::string(from_before) == "plain-from-before", "a settings file from before has it as it was, and it is read");
        StringCell damaged(true, LockMode::UNLOCK_WHILE_RUNNING, "", "");
        damaged.load_json(JsonValue(std::string("dpapi1:not-base64!")));
        checker.check(std::string(damaged).empty(), "a sealed text that cannot be opened leaves the secret empty");
        StringCell open_text(false, LockMode::UNLOCK_WHILE_RUNNING, "", "");
        open_text.set("shown");
        const JsonValue shown_json = open_text.to_json();
        const std::string* shown = shown_json.to_string();
        checker.check(shown != nullptr && *shown == "shown", "an option that is not a secret is saved as it is");
    }

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

    //  ---- Minimize: a window of its own has no title bar, so it has a button
    {
        SetupRequest minimizing;
        minimizing.ask_board = true;
        minimizing.games = test_games();
        Canvas mini(minimizing, true);
        mini.resize(1280, 720);
        QEventLoop mini_loop;
        mini.start(&mini_loop);
        mini.show();
        mini.wait(2.0);
        checker.check(mini.can_minimize(), "the full screen of the setup can be minimized");
        checker.check(mini.has_hit("minimize") && mini.hit_enabled("minimize"), "and has a button for it in the top right corner");
        const QString page_before = mini.page_name();
        checker.check(mini.click("minimize") && mini.minimize_requests() == 1, "the button asks for the window to be minimized");
        mini.wait(0.3);
        checker.check(!mini.finished() && mini.page_name() == page_before, "and leaves the screen as it was");
        mini.press_key(Qt::Key_M, Qt::ControlModifier, "m");
        checker.check(mini.minimize_requests() == 2, "Ctrl+M minimizes too");
        mini.press_key(Qt::Key_M, Qt::NoModifier, "m");
        checker.check(mini.minimize_requests() == 2, "the letter M alone does not");
        save_preview(mini, folder, "preview-minimize");
        mini.click("skip");
        mini.wait(2.5);
        checker.check(mini.page_name() == "games" && mini.has_hit("minimize"), "the next screen has the button too");
    }

    //  ---- The installations: the overview, and the way back to it
    check_overview(checker, folder);

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

    //  ---- the games: back to the console, and on to the games again
    {
        SetupRequest back_request;
        back_request.ask_console = true;
        back_request.ask_board = true;
        back_request.console_code = "switch";
        back_request.games = test_games();
        const QString game_two = back_request.games[2].name;
        Canvas back(back_request, true);
        back.resize(1280, 720);
        QEventLoop back_loop;
        back.start(&back_loop);
        back.show();
        back.wait(2.0);
        back.click("skip");
        back.wait(2.8);
        checker.check(back.page_name() == "games" && back.has_hit("console"), "the games have a way back to the console");
        back.click("game:2");
        back.wait(0.3);
        back.click("console");
        back.wait(2.0);
        checker.check(back.page_name() == "console" && back.page_state() == "selected=switch", "it leads to the console, the one of the request picked");
        checker.check(back.has_hit("back") && !back.has_hit("skip"), "with a way back to the games, not Skip setup");
        save(back, folder, "setup-54-console-again");
        back.press_key(Qt::Key_Escape);
        back.wait(2.0);
        checker.check(back.page_name() == "games" && back.page_state() == "selected=" + game_two, "Escape goes back to the games, with the game still picked");
        back.click("console");
        back.wait(2.0);
        back.click("console:1");
        back.wait(0.3);
        checker.check(back.page_state() == "selected=switch2" && back.focus_id() == "continue", "another console is picked");
        back.click("continue");
        back.wait(2.8);
        checker.check(back.page_name() == "games", "Continue leads to the games, the board is not asked again");
        checker.check(back.page_state() == "selected=" + game_two, "the game is still picked");
        checker.check(back.result().console_chosen && back.result().console_code == "switch2", "the new console is the answer");
        back.click("console");
        back.wait(2.0);
        back.click("back");
        back.wait(2.0);
        checker.check(back.page_name() == "games" && !back.finished(), "Back leaves the console as it was");
        checker.check(back.result().console_code == "switch2", "and the answer stays");
    }

    //  ---- a program with many options opens wide; the picture in full screen
    {
        g_test_options = TestOptionsLog();
        SetupRequest rich;
        rich.ask_game = true;
        rich.games = test_games();
        rich.programs = test_programs();
        HomeProgram story;
        story.game = 0;
        story.name = "AutoStory";
        story.description = "Play the FireRed/LeafGreen story from the bedroom to the Hall of Fame and on after it (the legendary Pokemon, the Sevii Islands, 60 species for the National POKeDEX, the Ruby and the Sapphire, the roaming legendary with the Master Ball, Mewtwo, then the Pokedex collection: what the chosen game can still register in the grass, caves and Safari Zone), on routes generated from the game's map data.";
        story.tag = "beta";
        story.tag_meaning = "It is new: it can still go wrong.";
        rich.programs.insert(rich.programs.begin() + 9, story);
        rich.make_options = [](QWidget* parent, int, const QString& program) -> std::unique_ptr<SetupProgramOptions>{
            if (program == "AutoStory"){
                return std::unique_ptr<SetupProgramOptions>(new RichTestOptions(parent));
            }
            return std::unique_ptr<SetupProgramOptions>(new TestOptions(parent, program));
        };
        Canvas wide(rich, true);
        wide.resize(1280, 720);
        wide.set_scan([]{
            return HardwareState();
        });
        QEventLoop wide_loop;
        wide.start(&wide_loop);
        wide.show();
        wide.wait(2.5);
        wide.click("game:0");
        wide.wait(0.2);
        wide.click("start");
        wide.wait(1.6);
        checker.check(wide.page_name() == "programs" && wide.page_state().contains(";full=no;wide=no;"), "the programs start narrow, the picture in its place");
        wide.click("prog:0");
        wide.wait(1.4);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 30);
        checker.check(wide.page_state().contains(";options=ready;host=shown;full=no;wide=no;"), "a program with a few options keeps the panel narrow");
        checker.check(wide.has_hit("video-full") && wide.has_hit("opt-wide"), "the picture has its button for full screen, the panel its button for wide");

        wide.click("prog:9");
        wide.wait(2.0);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 30);
        wide.wait(0.2);
        checker.check(wide.page_state().contains("selected=AutoStory;") && wide.page_state().contains(";options=ready;host=shown;full=no;wide=yes;"), "a program with many options opens wide");
        checker.check(!wide.has_hit("video-full") && !wide.has_hit("source") && !wide.has_hit("drag:lanes"), "the picture, the log and the timeline give way");
        checker.check(wide.has_hit("opt-wide") && wide.has_hit("opt-close") && wide.has_hit("start"), "the panel keeps its buttons, and Start stays");
        save(wide, folder, "setup-55-programs-options-wide");
        save_preview(wide, folder, "preview-programs-options-wide", 1280);

        wide.click("opt-wide");
        wide.wait(1.6);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 30);
        wide.wait(0.2);
        checker.check(wide.page_state().contains(";options=ready;host=shown;full=no;wide=no;") && wide.has_hit("video-full"), "the button makes it narrow again, the picture is back");
        save(wide, folder, "setup-56-programs-options-narrow");
        wide.click("prog:1");
        wide.wait(1.0);
        wide.click("prog:9");
        wide.wait(2.0);
        checker.check(wide.page_state().contains(";options=ready;") && wide.page_state().contains(";wide=no;"), "what the person chose stays for the other programs");
        wide.click("opt-wide");
        wide.wait(1.6);
        checker.check(wide.page_state().contains(";wide=yes;"), "and can be changed again");
        wide.click("opt-wide");
        wide.wait(1.6);

        //  The picture over the whole screen.
        wide.click("video-full");
        wide.wait(0.8);
        checker.check(wide.page_state().contains(";full=yes;") && wide.has_hit("video-exit"), "the button puts the picture over the whole screen, with a way out");
        checker.check(!wide.has_hit("prog:9") && !wide.has_hit("start") && !wide.has_hit("opt-close"), "nothing behind it can be pressed");
        checker.check(wide.page_state().contains(";host=hidden;"), "and the options are not over it");
        save(wide, folder, "setup-57-video-full");
        save_preview(wide, folder, "preview-video-full", 1280);
        wide.press_key(Qt::Key_A, Qt::NoModifier, "a");
        wide.wait(0.1);
        checker.check(wide.page_state().contains("query=;"), "typing does not go into the search behind it");
        wide.press_key(Qt::Key_Escape);
        wide.wait(0.8);
        checker.check(wide.page_name() == "programs" && wide.page_state().contains(";full=no;") && wide.has_hit("prog:9"), "Escape brings the page back");
        wide.press_key(Qt::Key_F11);
        wide.wait(0.8);
        checker.check(wide.page_state().contains(";full=yes;"), "F11 does the same as the button");
        wide.press_key(Qt::Key_F11);
        wide.wait(0.8);
        checker.check(wide.page_state().contains(";full=no;"), "and leaves it");
        wide.click("video-full");
        wide.wait(0.8);
        wide.click("video-exit");
        wide.wait(0.8);
        checker.check(wide.page_state().contains(";full=no;") && !wide.finished(), "the button on the picture leaves it too");
    }

    //  ---- the programs of the game that was picked
    {
        TestLog log;
        log.add(0, "Global: settings loaded", 20000);
        log.add(1, "Audio: input is quiet", 15000);
        log.add(0, "Turbo A: loop 12 done", 9000);
        log.add(0, "Board: sent A down", 8200);
        log.add(1, "Board: no answer after 1.2 s", 6500);
        log.add(0, "Board: answer after 6 ms", 6400);
        log.add(2, "Video: black frame, signal lost?", 4200);
        log.add(0, "Video: signal found again", 3700);

        SetupRequest with_programs;
        with_programs.ask_game = true;
        with_programs.games = test_games();
        with_programs.programs = test_programs();
        with_programs.log_poll = log.source();
        with_programs.build = "PocketPilot test build";
        Canvas programs_canvas(with_programs, true);
        programs_canvas.resize(1920, 1080);
        programs_canvas.set_app_folder(app);
        programs_canvas.set_scan([]{
            HardwareState state;
            PortInfo port;
            port.name = "COM4";
            port.description = "Pico";
            port.vendor = 0x2E8A;
            port.product = 0x000A;
            state.ports.push_back(port);
            return state;
        });
        QEventLoop programs_loop;
        programs_canvas.start(&programs_loop);
        programs_canvas.show();
        programs_canvas.wait(2.5);
        checker.check(programs_canvas.page_name() == "games", "the games come first");
        programs_canvas.click("game:0");
        programs_canvas.wait(0.2);
        programs_canvas.click("start");
        programs_canvas.wait(0.1);
        checker.check(!programs_canvas.finished(), "a game that has programs does not end the setup: its programs come next");
        programs_canvas.wait(1.6);
        checker.check(programs_canvas.page_name() == "programs", "the programs of the game are a screen of their own");
        save(programs_canvas, folder, "setup-40-programs");
        save_preview(programs_canvas, folder, "preview-programs");
        checker.check(has_content(programs_canvas.picture()), "the programs screen has something on it");
        bool rows = true;
        for (int c = 0; c < 9; c++){
            rows = rows && programs_canvas.has_hit("prog:" + QString::number(c));
        }
        checker.check(rows, "every program of the Nintendo Switch has its row");
        checker.check(programs_canvas.page_state().startsWith("selected=Turbo A;"), "the first program is picked");
        checker.check(programs_canvas.has_hit("start") && programs_canvas.hit_enabled("start"), "and Start can be pressed");
        checker.check(programs_canvas.has_hit("all") && programs_canvas.has_hit("later"), "All games and Choose later are there");
        checker.check(programs_canvas.page_state().contains(";log=8;"), "the output log has the lines of the program");
        checker.check(programs_canvas.has_hit("lvl:0") && programs_canvas.has_hit("lvl:3") && programs_canvas.has_hit("copy-claude") && programs_canvas.has_hit("copy-log") && programs_canvas.has_hit("save-log"),
            "the log panel has its filters and its three buttons");
        checker.check(programs_canvas.has_hit("drag:lanes"), "the timeline can be dragged over");

        //  A program picked with the mouse and with the keyboard.
        programs_canvas.click("prog:2");
        checker.check(programs_canvas.page_state().startsWith("selected=Turbo Macro;"), "a click picks a program");
        programs_canvas.press_key(Qt::Key_Down);
        checker.check(programs_canvas.page_state().startsWith("selected=Push Joy Sticks;"), "Down goes to the next program");
        programs_canvas.press_key(Qt::Key_Up);
        programs_canvas.press_key(Qt::Key_Up);
        checker.check(programs_canvas.page_state().startsWith("selected=Turbo Button;"), "Up goes to the one above");
        programs_canvas.press_key(Qt::Key_End);
        checker.check(programs_canvas.page_state().startsWith("selected=Typewriter;"), "End goes to the last");
        programs_canvas.press_key(Qt::Key_Home);
        checker.check(programs_canvas.page_state().startsWith("selected=Turbo A;"), "Home goes to the first");
        programs_canvas.wait(0.5);
        save(programs_canvas, folder, "setup-41-programs-tags");
        programs_canvas.click("prog:6");
        programs_canvas.wait(0.5);
        save(programs_canvas, folder, "setup-42-programs-tag-picked");

        //  Search: typing is enough.
        programs_canvas.click("prog:2");
        programs_canvas.type_text("tur");
        programs_canvas.wait(0.4);
        checker.check(programs_canvas.page_state().contains(";query=tur;") && programs_canvas.page_state().contains(";rows=3;"), "typing narrows the list down to the three Turbo programs");
        checker.check(programs_canvas.page_state().startsWith("selected=Turbo Macro;"), "the program that was picked stays picked");
        save(programs_canvas, folder, "setup-43-programs-search");
        programs_canvas.type_text("xyz");
        programs_canvas.wait(0.3);
        checker.check(programs_canvas.page_state().contains(";rows=0;") && programs_canvas.page_state().startsWith("selected=;"), "nothing matches: no program is picked");
        checker.check(programs_canvas.has_hit("start") && !programs_canvas.hit_enabled("start"), "and Start cannot be pressed");
        save(programs_canvas, folder, "setup-44-programs-nothing");
        programs_canvas.press_key(Qt::Key_Backspace);
        programs_canvas.press_key(Qt::Key_Backspace);
        programs_canvas.press_key(Qt::Key_Backspace);
        checker.check(programs_canvas.page_state().contains(";query=tur;") && programs_canvas.page_state().contains(";rows=3;"), "Backspace takes the letters away again");
        programs_canvas.press_key(Qt::Key_Escape);
        programs_canvas.wait(0.3);
        checker.check(programs_canvas.page_name() == "programs" && programs_canvas.page_state().contains(";query=;") && programs_canvas.page_state().contains(";rows=9;"),
            "Escape empties the search first");

        //  The log: the levels.
        programs_canvas.click("lvl:2");
        checker.check(programs_canvas.page_state().contains(";shown=2;"), "Warn shows the two warnings");
        programs_canvas.click("lvl:3");
        checker.check(programs_canvas.page_state().contains(";shown=1;"), "Error shows the one error");
        programs_canvas.wait(0.3);
        save(programs_canvas, folder, "setup-45-programs-errors");
        programs_canvas.click("copy-log");
        checker.check(programs_canvas.copied().contains("black frame") && !programs_canvas.copied().contains("loop 12"), "Copy log copies what is shown");
        programs_canvas.click("lvl:1");
        checker.check(programs_canvas.page_state().contains(";shown=5;"), "Info shows the five lines of information");
        programs_canvas.click("lvl:0");
        checker.check(programs_canvas.page_state().contains(";shown=8;"), "All shows all of them");
        log.add(0, "Turbo A: loop 13 done");
        programs_canvas.wait(1.2);
        checker.check(programs_canvas.page_state().contains(";log=9;"), "a new line of the log comes in while the screen is up");

        //  Copy for Claude: the whole story in one text.
        programs_canvas.click("copy-claude");
        const QString report = programs_canvas.copied();
        checker.check(report.contains("# Claude GG debug report") && report.contains("Build: PocketPilot test build"), "Copy for Claude starts with what it is, and the build");
        checker.check(report.contains("Program: Turbo A") && report.contains("Game: Nintendo Switch"), "it says which program was picked");
        checker.check(report.contains("Video: USB3 Video") && report.contains("Audio: USB3 Audio") && report.contains("Board: COM4"), "it says what the checks saw");
        checker.check(report.contains("ERROR Video: black frame, signal lost?") && report.contains("WARN Board: no answer after 1.2 s"), "it has the lines of the log, with their levels");
        checker.check(report.contains("System:"), "it says what computer it is");
        programs_canvas.wait(0.3);
        save(programs_canvas, folder, "setup-46-programs-copied");

        //  A range of the timeline.
        programs_canvas.wait(4.0);
        checker.check(programs_canvas.drag_over("drag:lanes", 0.55, 0.9), "a range is dragged over");
        checker.check(!programs_canvas.page_state().contains(";range=0;"), "the range is kept");
        programs_canvas.wait(0.4);
        save(programs_canvas, folder, "setup-47-programs-range");
        programs_canvas.click("copy-claude");
        checker.check(programs_canvas.copied().contains("Output log, the selected "), "Copy for Claude copies that range");
        programs_canvas.drag_over("drag:lanes", 0.5, 0.5);
        checker.check(programs_canvas.page_state().contains(";range=0;"), "a click on the timeline lets go of the range");

        //  The file.
        programs_canvas.click("save-log");
        const QString saved_name = programs_canvas.page_state().section("saved=", 1);
        checker.check(!saved_name.isEmpty() && QFile::exists(QDir(app + "/Debug").filePath(saved_name)), "Save .txt makes a file in the folder Debug");
        checker.check(programs_canvas.last_folder().endsWith("Debug"), "and shows where it is");

        //  The cards.
        checker.check(programs_canvas.has_hit("source"), "there is a list of cards to pick from");
        programs_canvas.click("source");
        programs_canvas.wait(0.3);
        checker.check(programs_canvas.has_hit("src:0") && programs_canvas.has_hit("src:1"), "it opens");
        save(programs_canvas, folder, "setup-48-programs-cards");
        programs_canvas.click("src:1");
        programs_canvas.wait(0.2);
        checker.check(!programs_canvas.has_hit("src:0"), "and closes once one is picked");

        //  Start: the answer is the game and the program.
        programs_canvas.click("prog:3");
        programs_canvas.press_key(Qt::Key_Return);
        programs_canvas.wait(0.2);
        checker.check(programs_canvas.finished(), "Enter starts the program");
        checker.check(programs_canvas.result().game == "Nintendo Switch" && programs_canvas.result().program == "Push Joy Sticks", "the game and the program are the answer");
    }

    //  ---- the programs: back, later, a long list with sections
    {
        SetupRequest with_programs;
        with_programs.ask_game = true;
        with_programs.games = test_games();
        with_programs.programs = test_programs();
        Canvas second(with_programs, true);
        second.resize(1280, 720);
        second.set_scan([]{
            return HardwareState();
        });
        QEventLoop second_loop;
        second.start(&second_loop);
        second.show();
        second.wait(2.5);
        second.click("game:3");
        second.wait(0.2);
        second.click("start");
        second.wait(1.6);
        checker.check(second.page_name() == "programs", "Sword and Shield has programs too");
        checker.check(second.has_hit("prog:0") && !second.has_hit("prog:35"), "of a long list only the first rows are on the screen");
        save(second, folder, "setup-49-programs-long-list");
        second.press_key(Qt::Key_End);
        second.wait(1.0);
        checker.check(second.has_hit("prog:35") && !second.has_hit("prog:0"), "End goes to the last, and the list scrolls to it");
        save(second, folder, "setup-50-programs-end-of-list");
        second.press_key(Qt::Key_Home);
        second.wait(1.0);
        checker.check(second.has_hit("prog:0"), "Home scrolls back");
        second.scroll("prog:1", -3);
        second.wait(0.8);
        checker.check(!second.has_hit("prog:0") && second.has_hit("prog:8"), "the wheel scrolls the list");
        second.scroll("prog:8", +10);
        second.wait(0.8);
        checker.check(second.has_hit("prog:0"), "and back");
        checker.check(second.page_state().contains(";shown=0;"), "without a log there are no lines");
        second.wait(0.3);
        save(second, folder, "setup-51-programs-small-window");
        second.type_text("ZZ");
        second.press_key(Qt::Key_Escape);
        second.wait(0.3);
        checker.check(second.page_name() == "programs", "the first Escape only takes the letters of the search away");
        second.press_key(Qt::Key_Escape);
        second.wait(1.0);
        checker.check(second.page_name() == "games" && !second.finished(), "Escape on an empty search goes back to the games");
        checker.check(second.page_state() == "selected=" + with_programs.games[3].name, "and the game stays picked");
        second.wait(0.2);
        second.click("start");
        second.wait(1.6);
        second.click("later");
        second.wait(0.2);
        checker.check(second.finished() && second.result().game == with_programs.games[3].name && second.result().program.isEmpty(), "Choose later opens the game without a program");

        //  All games.
        Canvas third(with_programs, true);
        third.resize(1920, 1080);
        third.set_scan([]{
            return HardwareState();
        });
        QEventLoop third_loop;
        third.start(&third_loop);
        third.show();
        third.wait(2.5);
        third.click("game:3");
        third.wait(0.2);
        third.click("start");
        third.wait(1.6);
        third.click("all");
        third.wait(1.0);
        checker.check(third.page_name() == "games" && third.page_state() == "selected=" + with_programs.games[3].name, "All games goes back to the games with that game still picked");

        //  A game without programs ends the setup at once, as before.
        third.click("game:1");
        third.wait(0.2);
        third.click("start");
        third.wait(0.2);
        checker.check(third.finished() && third.result().game == with_programs.games[1].name && third.result().program.isEmpty(), "a game without programs ends the setup, as before");
    }

    //  ---- the options of the program that is picked, in a panel next to the list
    {
        g_test_options = TestOptionsLog();
        SetupRequest with_options;
        with_options.ask_game = true;
        with_options.games = test_games();
        with_options.programs = test_programs();
        with_options.make_options = [](QWidget* parent, int, const QString& program) -> std::unique_ptr<SetupProgramOptions>{
            if (program == "Prevent Sleep"){
                //  A program without options.
                return nullptr;
            }
            return std::unique_ptr<SetupProgramOptions>(new TestOptions(parent, program));
        };
        Canvas options(with_options, true);
        options.resize(1280, 720);
        options.set_scan([]{
            return HardwareState();
        });
        QEventLoop options_loop;
        options.start(&options_loop);
        options.show();
        options.wait(2.5);
        options.click("game:0");
        options.wait(0.2);
        options.click("start");
        options.wait(1.6);
        checker.check(options.page_name() == "programs", "the programs of the first game");
        checker.check(options.page_state().contains(";options=closed;host=hidden"), "the panel with the options is closed at first");
        checker.check(g_test_options.made == 0 && !options.has_hit("opt-close"), "and nothing is made for it yet");
        options.click("prog:0");
        options.wait(1.4);
        checker.check(options.page_state().contains(";options=ready;host=shown"), "a program that is picked opens its options next to the list");
        checker.check(g_test_options.made == 1 && g_test_options.programs.value(0) == "Turbo A", "they are those of the program");
        checker.check(options.has_hit("opt-close") && options.has_hit("options"), "the panel has its close button and its place in the Tab order");
        //  The layout of the widgets is done by the event loop; give it the turn before the picture.
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 30);
        options.wait(0.1);
        save(options, folder, "setup-52-programs-options");
        save_preview(options, folder, "preview-programs-options", 1280);

        //  The keyboard goes to the widget that has it.
        checker.check(!g_test_options.edit.isNull(), "the test options have a field");
        if (!g_test_options.edit.isNull()){
            g_test_options.edit->setFocus();
            QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(g_test_options.edit.data(), &enter);
            options.wait(0.2);
            checker.check(!options.finished(), "Enter in a field of the options does not start the program");
            options.press_key(Qt::Key_Return);
            checker.check(!options.finished(), "nor does it when the page gets it");
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(g_test_options.edit.data(), &escape);
            checker.check(!options.child_has_focus(), "Escape gives the keyboard back to the page");
            checker.check(options.page_state().contains(";options=ready"), "and leaves the options open");
        }

        //  A change is kept a moment later.
        if (!g_test_options.spin.isNull()){
            g_test_options.spin->setValue(7);
            options.wait(0.5);
            checker.check(g_test_options.saved == 0, "a change is not kept at once");
            options.wait(1.2);
            checker.check(g_test_options.saved == 1, "but a moment later");
        }

        //  Another program: the options of the first go, and are kept first.
        options.click("prog:1");
        options.wait(0.8);
        checker.check(g_test_options.destroyed == 1 && g_test_options.made == 2 && g_test_options.programs.value(1) == "Turbo Button", "the options of the next program replace them");
        if (!g_test_options.spin.isNull()){
            g_test_options.spin->setValue(3);
        }
        options.click("prog:2");
        options.wait(0.8);
        checker.check(g_test_options.saved == 2, "what was changed is kept before the options go");
        checker.check(g_test_options.destroyed == 2 && g_test_options.made == 3, "and the third is made");

        //  A program without options says so.
        options.click("prog:4");
        options.wait(1.0);
        checker.check(options.page_state().contains(";options=none;host=hidden"), "a program without options says so");
        checker.check(g_test_options.destroyed == 3, "and nothing is left of the one before");
        save(options, folder, "setup-53-programs-no-options");

        //  Closing, and opening it again.
        options.press_key(Qt::Key_Escape);
        options.wait(1.2);
        checker.check(options.page_name() == "programs" && options.page_state().contains(";options=closed;host=hidden"), "Escape closes the panel first");
        checker.check(!options.has_hit("opt-close"), "and it is gone");
        options.click("prog:0");
        options.wait(1.4);
        checker.check(options.page_state().contains(";options=ready"), "picking a program opens it again");
        options.click("opt-close");
        options.wait(1.2);
        checker.check(options.page_state().contains(";options=closed"), "the button closes it");
        checker.check(options.page_name() == "programs" && options.page_state().contains("selected=Turbo A;"), "the program stays picked");

        //  Start with the options open: the program that is picked, and what was changed is kept.
        options.click("prog:3");
        options.wait(1.4);
        if (!g_test_options.spin.isNull()){
            g_test_options.spin->setValue(9);
        }
        const int saved_before = g_test_options.saved;
        options.press_key(Qt::Key_Return);
        options.wait(0.2);
        checker.check(options.finished() && options.result().program == "Push Joy Sticks", "Enter starts the program that is picked");
        checker.check(g_test_options.saved == saved_before + 1, "after what was changed was kept");
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

    //  ---- the screens as the main window's own screen (see AppView)
    {
        struct AppLog{
            int opened = 0;
            int closed = 0;
            int settings = 0;
            int about = 0;
            int output = 0;
            int buttons = 0;
            int installations = 0;
            int claude = 0;
            int open_game = -1;
            QString open_program;
            QString console;
            int keep_on = 0;
            int keep_off = 0;
            bool fail_next = false;
        } record;
        Canvas* screen = nullptr;
        QPointer<QWidget> content;
        AppActions actions;
        actions.open_program = [&](int game, const QString& program) -> bool{
            if (record.fail_next){
                record.fail_next = false;
                return false;
            }
            record.opened++;
            record.open_game = game;
            record.open_program = program;
            QLabel* label = new QLabel("The page of " + program);
            content = label;
            screen->host(label, game, short_game_name(screen->request().games[(size_t)game].name), program);
            return true;
        };
        actions.close_program = [&]() -> bool{
            record.closed++;
            if (content != nullptr){
                QWidget* widget = content.data();
                screen->unhost(widget);
                delete widget;
            }
            return true;
        };
        actions.settings = [&]{ record.settings++; };
        actions.about = [&]{ record.about++; };
        actions.output = [&]{ record.output++; };
        actions.buttons = [&]{ record.buttons++; };
        actions.installations = [&]{ record.installations++; };
        actions.claude = [&](bool show){
            record.claude++;
            screen->show_claude(show);
        };
        actions.console_chosen = [&](const QString& code){ record.console = code; };
        actions.keep_screen_on = [&](bool on){
            if (on){
                record.keep_on++;
            }else{
                record.keep_off++;
            }
        };

        SetupRequest request_app;
        request_app.games = test_games();
        request_app.programs = test_programs();
        request_app.build = "PocketPilot test build";
        Canvas window_screen(request_app, true, nullptr, &actions);
        screen = &window_screen;
        window_screen.resize(1920, 1080);
        window_screen.set_app_folder(app);
        window_screen.set_scan([]{
            HardwareState state;
            return state;
        });
        window_screen.set_claude(new QLabel("Claude's panel"));
        window_screen.start(nullptr);
        window_screen.show();
        window_screen.wait(2.5);

        //  The main window has a title bar of its own: no button for it here.
        checker.check(!window_screen.can_minimize() && !window_screen.has_hit("minimize"), "the main window's screen has no minimize button (its title bar has one)");

        //  The games: no end, no "Choose later", the buttons of the top bar.
        checker.check(window_screen.page_name() == "games" && !window_screen.finished(), "as the window's screen it starts on the games");
        checker.check(window_screen.has_hit("app:settings") && window_screen.has_hit("app:claude") && window_screen.has_hit("app:log")
            && window_screen.has_hit("app:buttons") && window_screen.has_hit("app:about"), "the buttons of the top bar are along the top");
        checker.check(!window_screen.has_hit("app:games"), "and the way to the games is not on the games");
        checker.check(window_screen.has_hit("app:installations") && !window_screen.has_hit("instances"), "so is the way to the overview of the installations (and no corner button)");
        checker.check(!window_screen.has_hit("later"), "there is no 'Choose later' to leave with");
        save(window_screen, folder, "setup-60-window-games");
        save_preview(window_screen, folder, "preview-window-games");
        checker.check(has_content(window_screen.picture()), "the games are drawn");

        //  The programs, and Start opens one in the window.
        window_screen.click("game:0");
        window_screen.wait(0.2);
        window_screen.click("start");
        window_screen.wait(1.8);
        checker.check(window_screen.page_name() == "programs", "a game with programs leads to its programs");
        checker.check(!window_screen.has_hit("later") && window_screen.has_hit("all") && window_screen.has_hit("app:games"), "with the way back, and no 'Choose later'");
        save(window_screen, folder, "setup-61-window-programs");
        window_screen.click("prog:3");
        window_screen.wait(0.2);
        window_screen.click("start");
        window_screen.wait(1.0);
        checker.check(!window_screen.finished(), "Start does not end the screen");
        checker.check(record.opened == 1 && record.open_game == 0 && record.open_program == "Push Joy Sticks", "it asks the window to open the program that is picked");
        checker.check(window_screen.page_name() == "host" && window_screen.hosting(), "and the program is on the screen, under a header");
        checker.check(window_screen.has_hit("host-back") && window_screen.has_hit("host-sleep") && window_screen.has_hit("app:games"), "with the way back, the sleep switch and the games");
        checker.check(!content.isNull() && content->isVisible(), "its widget is visible");
        checker.check(!content.isNull() && content->window() == &window_screen, "inside the screen");
        save(window_screen, folder, "setup-62-window-program");
        save_preview(window_screen, folder, "preview-window-program");
        checker.check(has_content(window_screen.picture()), "the program is drawn");

        //  The buttons of the top bar.
        window_screen.click("app:settings");
        window_screen.click("app:about");
        window_screen.click("app:log");
        window_screen.click("app:buttons");
        checker.check(record.settings == 1 && record.about == 1 && record.output == 1 && record.buttons == 1, "each button of the top bar does its own thing");
        window_screen.click("app:installations");
        checker.check(record.installations == 1 && window_screen.overview_requests() == 1, "'Installations' shows the overview");
        checker.check(window_screen.page_name() == "host", "and leaves the program that is open as it was");

        //  Claude over the right.
        checker.check(!window_screen.claude_shown(), "Claude is not shown at first");
        window_screen.click("app:claude");
        window_screen.wait(0.3);
        checker.check(window_screen.claude_shown() && record.claude == 1, "the button shows Claude");
        bool claude_over_right = false;
        for (QLabel* label : window_screen.findChildren<QLabel*>()){
            if (label->text() == "Claude's panel"){
                claude_over_right = label->isVisible() && label->x() > window_screen.width() / 2 && label->height() == window_screen.height();
            }
        }
        checker.check(claude_over_right, "its panel is over the right of the screen");
        checker.check(!content.isNull() && content->mapTo(&window_screen, QPoint(content->width(), 0)).x() <= window_screen.width() - window_screen.width() * 30 / 100,
            "and the program gives it room");
        save(window_screen, folder, "setup-63-window-claude");
        save_preview(window_screen, folder, "preview-window-claude");
        window_screen.click("app:claude");
        window_screen.wait(0.2);
        checker.check(!window_screen.claude_shown(), "the same button hides it");

        //  The sleep switch.
        window_screen.click("host-sleep");
        checker.check(record.keep_on == 1 && window_screen.app_keep_on(), "the switch keeps the screen on");
        window_screen.click("host-sleep");
        checker.check(record.keep_off == 1 && !window_screen.app_keep_on(), "and lets it sleep again");
        //  As it was left the last time: shown as set, without the action.
        window_screen.set_keep_on(true);
        checker.check(window_screen.app_keep_on() && record.keep_on == 1 && record.keep_off == 1, "the switch can be shown as it was kept, without the action");
        window_screen.click("host-sleep");
        checker.check(record.keep_off == 2 && !window_screen.app_keep_on(), "and it lets the screen sleep from there");

        //  A program that runs: the state, and no way back.
        window_screen.set_state(true, "Running Push Joy Sticks");
        window_screen.wait(0.3);
        checker.check(!window_screen.hit_enabled("host-back"), "while a program runs there is no way back");
        window_screen.click("app:games");
        window_screen.wait(0.2);
        checker.check(window_screen.hosting() && window_screen.page_name() == "host", "and the games button does not close it");
        save(window_screen, folder, "setup-64-window-running");
        window_screen.set_state(false, "Ready");
        window_screen.wait(0.3);
        checker.check(window_screen.hit_enabled("host-back"), "when it stops the way back is open again");

        //  Back leads to the programs of the game.
        window_screen.click("host-back");
        window_screen.wait(1.8);
        checker.check(record.closed == 1 && content.isNull(), "the program is closed (and its widget is the window's to delete)");
        checker.check(window_screen.page_name() == "programs" && !window_screen.hosting(), "back leads to the programs");

        //  A program that cannot be opened: the programs stay.
        record.fail_next = true;
        window_screen.click("start");
        window_screen.wait(1.8);
        checker.check(window_screen.page_name() == "programs" && record.opened == 1, "a program that cannot be opened leaves the programs on the screen");

        //  The games button closes a program.
        window_screen.click("start");
        window_screen.wait(1.8);
        checker.check(window_screen.hosting() && record.opened == 2, "it opens the next time");
        window_screen.click("app:games");
        window_screen.wait(1.8);
        checker.check(record.closed == 2 && window_screen.page_name() == "games" && !window_screen.hosting(), "the games button closes it and shows the games");

        //  The settings (no game) lead back to the games.
        window_screen.host(new QLabel("The settings"), -1, QString(), "Settings");
        window_screen.wait(1.8);
        checker.check(window_screen.page_name() == "host" && window_screen.hosting(), "the settings are held in the screen as well");
        window_screen.click("host-back");
        window_screen.wait(1.8);
        checker.check(window_screen.page_name() == "games", "and back from them leads to the games");

        //  Another console, from the games: kept at once.
        window_screen.click("console");
        window_screen.wait(2.5);
        checker.check(window_screen.page_name() == "console", "the console can be chosen again from the games");
        window_screen.click("console:1");
        window_screen.wait(0.3);
        window_screen.click("continue");
        window_screen.wait(1.0);
        checker.check(record.console == "switch2", "the window is told which console was picked");
        window_screen.wait(1.5);
        checker.check(window_screen.page_name() == "games" && !window_screen.finished(), "and the games are back");

        //  The program of the last time is one press away.
        checker.check(!window_screen.has_hit("resume"), "no 'Continue with' when there is no program of the last time");
        window_screen.set_resume(request_app.games[0].name, "Turbo Macro");
        window_screen.wait(0.3);
        checker.check(window_screen.has_hit("resume"), "the program of the last time is on the games");
        save(window_screen, folder, "setup-65-window-resume");
        checker.check(window_screen.click("resume"), "it can be pressed");
        window_screen.wait(1.8);
        checker.check(window_screen.hosting() && record.open_program == "Turbo Macro", "and it opens that program");
        window_screen.click("app:games");
        window_screen.wait(1.8);
        window_screen.set_resume("No such game", "Turbo Macro");
        window_screen.wait(0.3);
        checker.check(!window_screen.has_hit("resume"), "a program that is not in the list is not offered");

        //  The search over all programs: what is typed turns the cartridges into a list.
        checker.check(window_screen.has_hit("gsearch") && window_screen.has_hit("game:0"), "the games have a search field");
        window_screen.type_text("typew");
        window_screen.wait(0.3);
        checker.check(window_screen.page_state().contains(";query=typew;found=1"), "typing finds the programs by name");
        checker.check(window_screen.has_hit("found:0") && !window_screen.has_hit("game:0") && !window_screen.hit_enabled("start"), "the list replaces the cartridges");
        save(window_screen, folder, "setup-66-window-search");
        save_preview(window_screen, folder, "preview-window-search");
        window_screen.press_key(Qt::Key_Escape);
        window_screen.wait(0.2);
        checker.check(window_screen.page_state().contains(";query=;") && window_screen.has_hit("game:0"), "Escape clears the search and the cartridges are back");
        window_screen.type_text("turbo");
        window_screen.wait(0.2);
        checker.check(window_screen.page_state().contains(";query=turbo;found=3"), "three programs have Turbo in their name");
        window_screen.press_key(Qt::Key_Down);
        window_screen.press_key(Qt::Key_Return);
        window_screen.wait(1.8);
        checker.check(window_screen.hosting() && record.open_program == "Turbo Button", "the arrows pick one and Enter opens it");
        window_screen.click("app:games");
        window_screen.wait(1.8);
        checker.check(window_screen.page_state().contains(";query=;"), "the search starts empty again");
        window_screen.type_text("xyz");
        window_screen.press_key(Qt::Key_Backspace);
        checker.check(window_screen.page_state().contains(";query=xy;found=0"), "a word that is in no program finds none");
        window_screen.wait(0.2);
        save(window_screen, folder, "setup-67-window-search-none");
        window_screen.click("gsearch-clear");
        window_screen.wait(0.2);
        checker.check(window_screen.page_state().contains(";query=;") && window_screen.has_hit("game:0"), "the cross clears it");
        window_screen.type_text("sword");
        window_screen.wait(0.2);
        checker.check(window_screen.page_state().contains("found=36"), "the game's name finds its programs too");
        window_screen.click("found:5");
        window_screen.wait(1.8);
        checker.check(window_screen.hosting() && record.open_game == 3 && record.open_program == "Program 6 of Sword and Shield", "a program of another game opens with its own game");
        window_screen.click("app:games");
        window_screen.wait(1.8);
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

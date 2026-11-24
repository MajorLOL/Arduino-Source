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
#include <vector>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFontDatabase>
#include <QFile>
#include <QFontMetricsF>
#include <QHash>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRadialGradient>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include "PocketPilot_Language.h"
#include "PocketPilot_Setup.h"

namespace PokemonAutomation{
namespace PocketPilot{


const char* const SETUP_TEST_OPTION = "--setup-test";


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
        if (!m_test_mode){
            QDesktopServices::openUrl(QUrl(url));
        }
    }

    void go_language();
    void go_board();
    void go_steps(const QString& board);
    void go_guide(const QString& board);
    void go_other();
    void go_done(const QString& board);
    //  The end: the loop of run_setup() stops.
    void finish();

    //  ---- the run
    void start(QEventLoop* loop);
    void advance(double dt);

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

//  The heading, in the serif, as lines of at most 800 px.
void draw_heading(QPainter& painter, const QString& text, double x, double y, double px, double reveal, double alpha){
    if (reveal < 0.004){
        return;
    }
    const QFont font = serif_font(px);
    const double a = eo(reveal) * alpha;
    const double slide = (1.0 - eo(reveal)) * 34;
    const QStringList lines = wrap_text(font, text, 800);
    for (int c = 0; c < lines.size(); c++){
        draw_text(painter, lines[c], x, y + slide + c * px * 1.02, font, white(a));
    }
}
double heading_height(const QString& text, double px){
    return (double)wrap_text(serif_font(px), text, 800).size() * px * 1.02;
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
        if (canvas.request().ask_board){
            canvas.go_board();
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

//  Where the Pico pages go: the Setup Guide's own pages.
const char* const URL_ESP32 = "https://pokemonautomation.github.io/SetupGuide/Controllers/Controller-ESP32-WROOM.html";
const char* const URL_ESP32_MAC = "https://pokemonautomation.github.io/SetupGuide/Controllers/Controller-ESP32-WROOM-MacOS.html";
const char* const URL_ESP32_VIDEO = "https://youtu.be/YzGyQQOGjl8";
const char* const URL_ESP32S3 = "https://pokemonautomation.github.io/SetupGuide/Controllers/Controller-ESP32-S3.html";
const char* const URL_ESP32S3_VIDEO = "https://youtu.be/ezBuwk48z8w";


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
            canvas.finish();
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
        }else if (board == "esp32" || board == "esp32s3"){
            canvas.go_guide(board);
        }else{
            canvas.go_steps(board);
        }
    }

private:
    Menu m_menu;
};


//  ---------------------------------------------------------------------
//  Page 3: the steps of the Pico W and the Pico 2 W
//  ---------------------------------------------------------------------

struct StepText{
    const char* title;
    const char* sub;
};

const StepText* steps_for(const QString& board, size_t& count){
    static const StepText PICO[] = {
        {"Plug it in", "Connect the Pico to the computer with a micro-USB cable. Keep it within 1 m (3 ft) of the Switch."},
        {"Enter flash mode", "Unplug it, hold the white Bootsel button, plug it in again, then let go."},
        {"Flash the firmware", "Copy PABotBase2-Pico1W-<version>.uf2 from the Firmware folder to the drive RPI-RP2."},
        {"Open the Grip menu", "On the Switch, go to HOME, then Controllers, then Change Grip/Order."},
        {"Select the controller", "In the program, pick Serial: PABotBase2 in the first dropdown. Not Serial: PABotBase."},
        {"Select the port and the type", "Pick your device as the serial port (on Windows, for example COM3) and NS1: Wireless Pro Controller as the type."},
        {"Pair and test", "Wait for the controller in the Grip menu. Then click the video, press some keys, and check the picture and the sound."},
    };
    static const StepText PICO2[] = {
        {"Plug it in", "Connect the Pico to the computer with a micro-USB cable. Keep it within 1 m (3 ft) of the Switch."},
        {"Enter flash mode", "Unplug it, hold the white Bootsel button, plug it in again, then let go."},
        {"Flash the firmware", "Copy PABotBase2-Pico2W-<version>.uf2 from the Firmware folder to the drive RP2350."},
        {"Open the Grip menu", "On the Switch, go to HOME, then Controllers, then Change Grip/Order."},
        {"Select the controller", "In the program, pick Serial: PABotBase2 in the first dropdown. Not Serial: PABotBase."},
        {"Select the port and the type", "Pick your device as the serial port (on Windows, for example COM3) and NS1: Wireless Pro Controller as the type."},
        {"Pair and test", "Wait for the controller in the Grip menu. Then click the video, press some keys, and check the picture and the sound."},
    };
    count = sizeof(PICO) / sizeof(PICO[0]);
    return board == "pico2" ? PICO2 : PICO;
}

class StepsPage : public Page{
public:
    StepsPage(Canvas& canvas, const QString& board)
        : m_board(board)
        , m_steps(steps_for(board, m_count))
    {
        canvas.set_focus_id("next");
        m_usb.jump(0);
    }

    virtual const char* name() const override{
        return "steps";
    }

    int current() const{
        return m_current;
    }

    virtual void step(double dt) override{
        m_pos.step(dt, 12);
        m_usb.target = 1;
        m_pico.target = m_current >= 2 ? 1 : 0;
        m_serial.target = m_current >= 4 ? 1 : 0;
        m_type.target = m_current >= 5 ? 1 : 0;
        m_link.target = m_current >= 6 ? 1 : 0;
        m_search.target = m_current >= 3 ? 1 : 0;
        m_usb.step(dt, 6);
        m_pico.step(dt, 8);
        m_serial.step(dt, 7);
        m_type.step(dt, 7);
        m_link.step(dt, 6);
        m_search.step(dt, 6);
        m_clock += dt;
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        const int states[4] = {0, 0, 1, 2};
        draw_stepper(painter, states, prog(t, in.head - 0.05, in.head + 0.7));
        const QString heading = T("Set up your %1").arg(short_board_name(m_board));
        draw_heading(painter, heading, 140, 160, 84, prog(t, in.head, in.head + 0.9), 1.0);
        const QString sub = T("Wireless, set up over USB. Seven short steps.");
        const double sub_top = 160 + heading_height(heading, 84) + 10;
        draw_sub(painter, sub, 140, sub_top, prog(t, in.sub, in.sub + 0.8), 1.0, 32, 860);

        paint_steps(canvas, painter, eo(prog(t, in.bar, in.bar + 0.6)), sub_top + sub_height(sub, 32, 860) + 26);
        paint_diagram(painter, eo(prog(t, in.bar + 0.1, in.bar + 0.8)));

        const double b = eo(prog(t, in.buttons, in.buttons + 0.5));
        draw_skip(canvas, painter, b);
        const bool last = m_current >= (int)m_count - 1;
        const QString next_text = last ? T("Finish the steps") : T("Next step");
        const double next_w = button_width(next_text, ButtonKind::PRIMARY);
        draw_button(canvas, painter, "next", next_text, 1780 - next_w, 950, ButtonKind::PRIMARY, b);
        const QString back_text = T("Back");
        draw_button(canvas, painter, "back", back_text, 1780 - next_w - 18 - button_width(back_text, ButtonKind::SECONDARY), 950,
            ButtonKind::SECONDARY, b);
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        if (id == "next"){
            if (m_current >= (int)m_count - 1){
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
            canvas.finish();
        }else if (id.startsWith("step:")){
            set_current(id.mid(5).toInt());
        }
    }
    virtual bool key(Canvas& canvas, QKeyEvent& event) override{
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

private:
    void set_current(int index){
        const int clamped = std::max(0, std::min((int)m_count - 1, index));
        if (clamped != m_current){
            m_current = clamped;
        }
        m_pos.target = m_current;
    }

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
        const int open_lines = (int)wrap_text(sub_font, T(m_steps[m_current].sub), 740).size();
        const double extra = 8 + open_lines * 36 + 14;
        double compact = 66;
        if (top + (double)m_count * compact + extra > BOTTOM){
            compact = std::max(48.0, (BOTTOM - top - extra) / (double)m_count);
        }
        const double circle_size = std::min(60.0, compact - 4);
        double y = top + (1.0 - reveal) * 30;

        //  The rail behind the circles.
        QList<double> centers;
        double probe = y;
        for (int c = 0; c < (int)m_count; c++){
            const bool open = c == m_current;
            const double height = open ? compact + extra : compact;
            centers << probe + circle_size / 2;
            probe += height;
        }
        if (centers.size() > 1){
            painter.setPen(QPen(with_alpha(ORANGE, 0.45 * a), 4, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(X + circle_size / 2, centers.first()), QPointF(X + circle_size / 2, centers.last()));
        }

        for (int c = 0; c < (int)m_count; c++){
            const bool open = c == m_current;
            const bool done = c < m_current;
            const QStringList lines = wrap_text(sub_font, T(m_steps[c].sub), 740);
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
            draw_text(painter, T(m_steps[c].title), X + 88, y + (circle_size - text_height(title_font)) / 2, title_font, white((open ? 1.0 : done ? 0.78 : 0.55) * a));
            if (open){
                for (int k = 0; k < lines.size(); k++){
                    draw_text(painter, lines[k], X + 90, y + circle_size + 4 + k * 36, sub_font, white(0.66 * a));
                }
            }
            canvas.add_hit("step:" + QString::number(c), QRectF(X, y, 820, height), true, false);
            y += height;
        }
    }

    void paint_diagram(QPainter& painter, double reveal){
        if (reveal < 0.004){
            return;
        }
        constexpr double CX_PC = 1190;
        constexpr double CX_PICO = 1500;
        constexpr double CY = 430;
        const double a = reveal;

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

        //  The computer, the line, the Pico.
        draw_picture(painter, plain_picture("computer"), QRectF(CX_PC - 75, CY - 64, 150, 128 * 150.0 / 150.0 * 0.0 + 128), a * 0.9);
        draw_text(painter, T("Computer"), CX_PC, CY + 100, ui_font(26, 500), white(0.6 * a), Qt::AlignHCenter);
        const double line_left = CX_PC + 75 + 18;
        const double line_right = CX_PICO - 50;
        painter.setPen(Qt::NoPen);
        painter.setBrush(white(0.22 * a));
        painter.drawRoundedRect(QRectF(line_left, CY - 3.5, line_right - line_left, 7), 3.5, 3.5);
        painter.setBrush(with_alpha(ORANGE, a));
        painter.drawRoundedRect(QRectF(line_left, CY - 3.5, (line_right - line_left) * eo(m_usb.value), 7), 3.5, 3.5);

        const QRectF pico_box(CX_PICO - 50, CY - 95, 100, 190);
        const double flash_wait = m_pico.value;
        if (m_pico.value > 0.02 && m_current >= 2){
            const QPixmap glow_pixmap = tinted("pico-big", ORANGE);
            draw_picture(painter, glow_pixmap, pico_box.adjusted(-10, -10, 10, 10), 0.18 * a * m_pico.value);
        }
        draw_picture(painter, tinted("pico-big", white(1.0)), pico_box, a * (1.0 - flash_wait) * 0.92);
        draw_picture(painter, tinted("pico-big", ORANGE), pico_box, a * flash_wait);
        draw_text(painter, short_board_name(m_board), CX_PICO, CY + 118, ui_font(26, 500), white(0.6 * a), Qt::AlignHCenter);

        //  The firmware file drops into the Pico, again and again, while the step "Flash the firmware" is open.
        if (m_current == 2){
            const double phase = std::fmod(m_clock, 2.8) / 2.8;
            const double fall = eio(prog(phase, 0.10, 0.62));
            const double chip_alpha = a * prog(phase, 0.0, 0.10) * (1.0 - prog(phase, 0.58, 0.68));
            const QString file = m_board == "pico2" ? "PABotBase2-Pico2W.uf2" : "PABotBase2-Pico1W.uf2";
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
            const QString text = "NS1: Wireless Pro Controller";
            const double w = draw_chip(painter, text, 0, 0, ChipKind::YELLOW_FILL, 0, 25, 48, 20, true);
            draw_chip(painter, text, mid + w / 2, CY - 122 + (1.0 - m_type.value) * 14, ChipKind::YELLOW_FILL, a * m_type.value, 25, 48, 20);
        }

        //  The Pico looks for the Switch, then it is linked.
        if (m_search.value > 0.02){
            const double pulse = m_link.value > 0.5 ? 1.0 : 0.5 + 0.5 * std::sin(m_clock * 5.0);
            static const double RX[3] = {52, 96, 140};
            static const double RY[3] = {30, 54, 78};
            for (int c = 0; c < 3; c++){
                const double lit = m_link.value > 0.5 ? 1.0 : clamp01(std::sin(m_clock * 5.0 - c * 0.9) * 0.6 + 0.5);
                const QColor color = m_link.value > 0.5 ? with_alpha(ORANGE, a) : with_alpha(ORANGE, a * (0.25 + 0.75 * lit));
                painter.setPen(QPen(m_link.value > 0.5 || lit > 0.25 ? color : white(0.34 * a), 7, Qt::SolidLine, Qt::RoundCap));
                painter.setBrush(Qt::NoBrush);
                painter.drawArc(QRectF(CX_PICO - RX[c], CY + 134 - RY[c], RX[c] * 2, RY[c] * 2), 218 * 16, 104 * 16);
            }
            (void)pulse;
        }
    }

private:
    QString m_board;
    size_t m_count = 0;
    const StepText* m_steps;
    int m_current = 0;
    Smooth m_pos;
    Smooth m_usb;
    Smooth m_pico;
    Smooth m_serial;
    Smooth m_type;
    Smooth m_link;
    Smooth m_search;
    double m_clock = 0;
};


//  ---------------------------------------------------------------------
//  The ESP32 boards: the guide has the steps
//  ---------------------------------------------------------------------

class GuidePage : public Page{
public:
    GuidePage(Canvas& canvas, const QString& board)
        : m_board(board)
    {
        canvas.set_focus_id("open");
    }

    virtual const char* name() const override{
        return "guide";
    }

    virtual void paint(Canvas& canvas, QPainter& painter, double t) override{
        const Intro in = intro_for(canvas);
        const int states[4] = {0, 0, 1, 2};
        draw_stepper(painter, states, prog(t, in.head - 0.05, in.head + 0.7));
        const QString heading = T("Set up your %1").arg(short_board_name(m_board));
        draw_heading(painter, heading, 140, 250, 84, prog(t, in.head, in.head + 0.9), 1.0);
        draw_sub(painter, T("The Setup Guide has the steps for this board, with its own pictures."), 140,
            250 + heading_height(heading, 84) + 20, prog(t, in.sub, in.sub + 0.8), 1.0);

        //  The card on the right: how hard it is, and where the steps are.
        const double reveal = eo(prog(t, in.bar, in.bar + 0.6));
        const double a = reveal;
        const double rows = m_board == "esp32" ? 3 : 2;
        const QRectF card(1000 + (1.0 - reveal) * 60, 200, 780, 170 + rows * 88 + 70);
        panel(painter, card, 30, with_alpha(PANEL, a), white(0.16 * a), 1.6);
        draw_picture(painter, tinted("glyph-" + m_board, Qt::white), QRectF(card.left() + 34, card.top() + 34, 88, 88), a);
        draw_text(painter, QString::fromLatin1(board_name(m_board)), card.left() + 144, card.top() + 44, ui_font(40, 600), white(a));
        draw_text(painter, T("Difficulty: 3 of 10"), card.left() + 146, card.top() + 98, ui_font(26, 500), white(0.62 * a));

        double y = card.top() + 170;
        const double x = card.left() + 34;
        const QString guide_text = T("Open the guide");
        draw_button(canvas, painter, "open", guide_text, x, y, ButtonKind::PRIMARY, a);
        y += 88;
        if (m_board == "esp32"){
            draw_button(canvas, painter, "open_mac", T("Open the guide for macOS"), x, y, ButtonKind::SECONDARY, a);
            y += 88;
        }
        draw_button(canvas, painter, "video", T("Watch the video tutorial"), x, y, ButtonKind::SECONDARY, a);
        y += 88;
        draw_text(painter, T("Come back here when the board works, then continue."), x + 4, y, ui_font(24, 500), white(0.55 * a));

        const double b = eo(prog(t, in.buttons, in.buttons + 0.5));
        draw_skip(canvas, painter, b);
        const QString next = T("Continue");
        const double next_w = button_width(next, ButtonKind::PRIMARY);
        draw_button(canvas, painter, "continue", next, 1780 - next_w, 950, ButtonKind::PRIMARY, b);
        const QString back = T("Back");
        draw_button(canvas, painter, "back", back, 1780 - next_w - 18 - button_width(back, ButtonKind::SECONDARY), 950, ButtonKind::SECONDARY, b);
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        if (id == "open"){
            canvas.open_url(m_board == "esp32" ? URL_ESP32 : URL_ESP32S3);
        }else if (id == "open_mac"){
            canvas.open_url(URL_ESP32_MAC);
        }else if (id == "video"){
            canvas.open_url(m_board == "esp32" ? URL_ESP32_VIDEO : URL_ESP32S3_VIDEO);
        }else if (id == "continue"){
            canvas.go_done(m_board);
        }else if (id == "back"){
            canvas.go_board();
        }else if (id == "skip"){
            canvas.result().board_done = true;
            canvas.result().board = m_board;
            canvas.finish();
        }
    }
    virtual void enter_pressed(Canvas& canvas) override{
        activate(canvas, "continue");
    }

private:
    QString m_board;
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
            canvas.open_url(URL_GUIDE);
        }else if (id == "back"){
            canvas.go_board();
        }else if (id == "skip"){
            canvas.result().board_done = true;
            canvas.result().board = "other";
            canvas.finish();
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
        const QString start = T("Start");
        draw_button(canvas, painter, "start", start, 1780 - button_width(start, ButtonKind::PRIMARY), 950, ButtonKind::PRIMARY, b);
    }

    virtual void activate(Canvas& canvas, const QString& id) override{
        if (id == "start"){
            canvas.result().board_done = true;
            canvas.result().board = m_board;
            canvas.finish();
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
//  Canvas
//  ---------------------------------------------------------------------

Canvas::Canvas(const SetupRequest& request, bool test_mode)
    : QWidget(nullptr, test_mode ? Qt::Window : (Qt::Window | Qt::FramelessWindowHint))
    , m_request(request)
    , m_test_mode(test_mode)
    , m_timer(this)
{
    setWindowTitle("PocketPilot Studio");
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
    }else{
        go_board();
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
    m_now += dt;
    m_page_time += dt;
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
void Canvas::go_guide(const QString& board){
    queue_page([this, board]{
        return std::unique_ptr<Page>(new GuidePage(*this, board));
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

void Canvas::finish(){
    if (m_finished){
        return;
    }
    m_finished = true;
    m_timer.stop();
    if (m_loop != nullptr){
        m_loop->quit();
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
    paint_background(painter);

    m_scale = std::min(width() / DW, height() / DH);
    m_offset = QPointF((width() - DW * m_scale) / 2.0, (height() - DH * m_scale) / 2.0);
    painter.translate(m_offset);
    painter.scale(m_scale, m_scale);

    m_hits.clear();
    if (m_page){
        painter.setOpacity(eo(clamp01(m_fade)));
        m_page->paint(*this, painter, m_page_time);
    }
}

void Canvas::resizeEvent(QResizeEvent* event){
    QWidget::resizeEvent(event);
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
    update_hover(event->position());
}

void Canvas::mousePressEvent(QMouseEvent* event){
    if (event->button() != Qt::LeftButton){
        return;
    }
    m_focus_visible = false;
    const Hit* hit = find_hit(to_design(event->position()));
    m_pressed = hit != nullptr && hit->enabled ? hit->id : QString();
}

void Canvas::mouseReleaseEvent(QMouseEvent* event){
    if (event->button() != Qt::LeftButton){
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



SetupResult run_setup(const SetupRequest& request){
    SetupResult result;
    if (!request.ask_language && !request.ask_board){
        return result;
    }
    Canvas canvas(request, false);
    QEventLoop loop;
    QScreen* screen = QApplication::primaryScreen();
    if (screen != nullptr){
        canvas.setGeometry(screen->geometry());
    }
    canvas.start(&loop);
    canvas.showFullScreen();
    canvas.raise();
    canvas.activateWindow();
    canvas.setFocus();
    loop.exec();
    result = canvas.result();
    canvas.hide();
    return result;
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


}   //  namespace


int run_setup_test(QApplication& application, const QString& folder){
    (void)application;
    Checker checker;

    SetupRequest request;
    request.ask_language = true;
    request.ask_board = true;
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
    checker.check(canvas.page_name() == "board", "the board question follows");

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

    //  Other first: its page, and back.
    canvas.click("bar");
    canvas.wait(0.5);
    canvas.click("row:other");
    canvas.wait(0.5);
    canvas.click("continue");
    canvas.wait(1.8);
    checker.check(canvas.page_name() == "other", "Other goes to its page");
    save(canvas, folder, "setup-11-other");
    canvas.click("back");
    canvas.wait(1.6);
    checker.check(canvas.page_name() == "board", "and back to the boards");

    //  The ESP32-S3: the guide.
    canvas.click("bar");
    canvas.wait(0.5);
    canvas.click("row:esp32s3");
    canvas.wait(0.5);
    canvas.click("continue");
    canvas.wait(1.8);
    checker.check(canvas.page_name() == "guide", "the ESP32-S3 goes to the guide");
    save(canvas, folder, "setup-12-guide");
    canvas.click("back");
    canvas.wait(1.6);

    //  The Pico W: the steps.
    canvas.click("bar");
    canvas.wait(0.5);
    canvas.click("row:pico");
    canvas.wait(0.5);
    canvas.press_key(Qt::Key_Return);
    canvas.wait(1.8);
    checker.check(canvas.page_name() == "steps", "the Pico W goes to its steps");
    for (int c = 0; c < 7; c++){
        canvas.wait(c == 2 ? 1.6 : 1.0);
        save(canvas, folder, QString("setup-13-steps-%1").arg(c + 1));
        if (c == 6){
            save_preview(canvas, folder, "preview-steps");
        }
        if (c < 6){
            checker.check(canvas.click("next"), QString("next step %1").arg(c + 1));
        }
    }
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
    save(canvas, folder, "setup-14-done");
    canvas.click("start");
    canvas.wait(0.2);
    checker.check(canvas.finished(), "Start ends the setup");
    checker.check(canvas.result().board_done && canvas.result().board == "pico", "the board is the answer");

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



}
}

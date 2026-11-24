/*  PocketPilot Shell
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <algorithm>
#include <utility>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include "PocketPilot_Language.h"
#include "PocketPilot_Logo.h"
#include "PocketPilot_Theme.h"
#include "PocketPilot_Shell.h"

namespace PokemonAutomation{
namespace PocketPilot{

namespace{


//  The button that shows and hides the side bar: three lines.
class MenuButton : public QPushButton{
public:
    MenuButton(QWidget& parent)
        : QPushButton(&parent)
    {
        setObjectName("ppBarButton");
        setFixedSize(38, 36);
        //  The keyboard reaches it with Tab; a click does not leave a
        //  focus frame on it.
        setFocusPolicy(Qt::TabFocus);
        setCursor(Qt::PointingHandCursor);
        setToolTip("Show or hide the list of programs");
        setAccessibleName("Show or hide the list of programs");
    }

protected:
    virtual void paintEvent(QPaintEvent* event) override{
        QPushButton::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(theme_colors().shell_text);
        pen.setWidthF(1.8);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        const qreal left = width() / 2.0 - 8;
        const qreal right = width() / 2.0 + 8;
        for (int line = -1; line <= 1; line++){
            const qreal y = height() / 2.0 + line * 5.5;
            painter.drawLine(QPointF(left, y), QPointF(right, y));
        }
    }
};


//  The button that opens the chat: a speech bubble and a name. It stays
//  down while the panel is open.
class AssistantButton : public QPushButton{
public:
    AssistantButton(QWidget& parent, const QString& text)
        : QPushButton(text, &parent)
    {
        setObjectName("ppAssistantButton");
        setProperty("open", false);
        setFocusPolicy(Qt::TabFocus);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    virtual void paintEvent(QPaintEvent* event) override{
        QPushButton::paintEvent(event);
        const ThemeColors& theme = theme_colors();
        const bool open = property("open").toBool();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(open ? theme.shell : theme.shell_text);
        pen.setWidthF(1.6);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        //  A rounded box with a small tail at its lower left.
        const qreal left = 13;
        const qreal top = height() / 2.0 - 6.5;
        QPainterPath bubble;
        bubble.addRoundedRect(QRectF(left, top, 14, 10.5), 3, 3);
        painter.drawPath(bubble);
        QPainterPath tail;
        tail.moveTo(left + 3, top + 10.5);
        tail.lineTo(left + 3, top + 14);
        tail.lineTo(left + 7, top + 10.5);
        painter.drawPath(tail);
    }
};


}



TopBar::TopBar(QWidget& parent, const QString& name, Actions actions)
    : QWidget(&parent)
    , m_actions(std::move(actions))
{
    setObjectName("ppTopBar");
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(52);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 10, 0);
    layout->setSpacing(4);

    MenuButton* menu = new MenuButton(*this);
    layout->addWidget(menu);
    connect(
        menu, &QPushButton::clicked,
        this, [this](bool){
            if (m_actions.toggle_side_bar){
                m_actions.toggle_side_bar();
            }
        }
    );

    layout->addSpacing(6);
    layout->addWidget(new LogoWidget(*this, 40));
    layout->addSpacing(8);

    //  The name; its last word, if it has more than one, a shade quieter.
    const qsizetype space = name.lastIndexOf(' ');
    QLabel* wordmark = new QLabel(space > 0 ? name.left(space) : name, this);
    wordmark->setObjectName("ppWordmark");
    wordmark->setFont(display_font(28));
    layout->addWidget(wordmark);
    if (space > 0){
        layout->addSpacing(5);
        QLabel* second = new QLabel(name.mid(space + 1), this);
        second->setObjectName("ppWordmarkSecond");
        second->setFont(display_font(28));
        layout->addWidget(second);
    }
    layout->addSpacing(22);

    //  It takes the room that is left, and never asks for more.
    m_location = new QLabel(this);
    m_location->setObjectName("ppBreadcrumb");
    m_location->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_location->setTextFormat(Qt::PlainText);
    layout->addWidget(m_location, 1);

    m_home = add_button("Games", "All games, and a search over the programs of all of them", m_actions.home);
    add_button("Output log", "What the programs write while they run", m_actions.output);
    add_button("Keyboard layout", "The keys that press the controller's buttons", m_actions.keyboard);
    m_settings = add_button("Settings", "The settings of the program itself", m_actions.settings);
    add_button("About", "The version, the license and where it comes from", m_actions.about);

    layout->addSpacing(6);
    m_assistant = new AssistantButton(*this, "Claude");
    m_assistant->setToolTip("Ask Claude about a program, or have it build or fix one");
    layout->addWidget(m_assistant);
    connect(
        m_assistant, &QPushButton::clicked,
        this, [this](bool){
            if (m_actions.assistant){
                m_actions.assistant();
            }
        }
    );
}

QPushButton* TopBar::add_button(const QString& text, const QString& tip, const std::function<void()>& action){
    QPushButton* button = new QPushButton(text, this);
    button->setObjectName("ppBarButton");
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::TabFocus);
    layout()->addWidget(button);
    connect(
        button, &QPushButton::clicked,
        this, [action](bool){
            if (action){
                action();
            }
        }
    );
    return button;
}

void TopBar::set_location(const QString& game, const QString& program){
    QString text = game;
    if (!game.isEmpty() && !program.isEmpty()){
        //  A single right-pointing angle between the two.
        text += QString::fromUtf8("  \xe2\x80\xba  ");
    }
    text += program;
    m_location->setText(text);
    m_location->setToolTip(text);
}

void TopBar::set_busy(bool busy){
    m_home->setEnabled(!busy);
    m_settings->setEnabled(!busy);
    const QString why = busy ? tr_ui(" (not while a program runs)") : QString();
    m_home->setToolTip(tr_ui("All games, and a search over the programs of all of them") + why);
    m_settings->setToolTip(tr_ui("The settings of the program itself") + why);
}

void TopBar::set_assistant_open(bool open){
    if (m_assistant->property("open").toBool() == open){
        return;
    }
    m_assistant->setProperty("open", open);
    //  The style sheet looks at the property.
    m_assistant->style()->unpolish(m_assistant);
    m_assistant->style()->polish(m_assistant);
    m_assistant->update();
}



//  A small round lamp: green when ready, amber while a program runs.
class StatusStrip::Lamp : public QWidget{
public:
    Lamp(QWidget& parent)
        : QWidget(&parent)
    {
        setObjectName("ppLamp");
        setFixedSize(10, 10);
    }
    void set_running(bool running){
        m_running = running;
        update();
    }

protected:
    virtual void paintEvent(QPaintEvent*) override{
        const ThemeColors& theme = theme_colors();
        //  The strip is dark in the PocketPilot themes and can be light in
        //  a classic one.
        const bool on_dark = theme.shell_deep.lightness() < 128;
        QColor color;
        if (m_running){
            color = on_dark ? QColor(0xff, 0xcf, 0x5c) : QColor(0xb2, 0x6a, 0x00);
        }else{
            color = on_dark ? QColor(0x84, 0xe0, 0x9e) : QColor(0x1f, 0x82, 0x45);
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QRectF(1, 1, width() - 2, height() - 2));
    }

private:
    bool m_running = false;
};


StatusStrip::StatusStrip(QWidget& parent, const QString& version)
    : QWidget(&parent)
{
    setObjectName("ppStatusStrip");
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(30);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 14, 0);
    layout->setSpacing(8);

    m_lamp = new Lamp(*this);
    layout->addWidget(m_lamp);
    m_state = new QLabel("Ready", this);
    m_state->setObjectName("ppStateText");
    m_state->setTextFormat(Qt::PlainText);
    layout->addWidget(m_state);

    layout->addSpacing(20);
    m_sleep_label = new QLabel(this);
    m_sleep_label->setTextFormat(Qt::PlainText);
    layout->addWidget(m_sleep_label);
    m_sleep_box = new QCheckBox("Keep the screen on", this);
    m_sleep_box->setToolTip("Keeps the screen from turning off, also when no program runs");
    layout->addWidget(m_sleep_box);

    layout->addStretch(1);

    QLabel* version_label = new QLabel(version, this);
    version_label->setTextFormat(Qt::PlainText);
    layout->addWidget(version_label);
}

void StatusStrip::set_state(State state, const QString& text){
    m_lamp->set_running(state == State::RUNNING);
    m_state->setText(text);
}



//  The room for the panel at the right: as wide as the user wants it, and
//  narrower when the window has less room.
class Frame::PanelHost : public QWidget{
public:
    explicit PanelHost(QWidget& parent)
        : QWidget(&parent)
    {
        setMinimumWidth(PANEL_MIN_WIDTH);
        setMaximumWidth(PANEL_MAX_WIDTH);
        //  Never wider than asked for, narrower if it has to be.
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    }
    void set_wanted_width(int width){
        width = std::max((int)PANEL_MIN_WIDTH, std::min(width, (int)PANEL_MAX_WIDTH));
        if (width != m_wanted){
            m_wanted = width;
            updateGeometry();
        }
    }
    int wanted_width() const{ return m_wanted; }
    virtual QSize sizeHint() const override{
        return QSize(m_wanted, QWidget::sizeHint().height());
    }

private:
    int m_wanted = 430;
};


//  The left edge of the panel at the right: dragged, it makes the panel
//  wider or narrower.
class Frame::Grip : public QWidget{
public:
    Grip(QWidget& parent, Frame& frame)
        : QWidget(&parent)
        , m_frame(frame)
    {
        setObjectName("ppAssistantGrip");
        setFixedWidth(5);
        setCursor(Qt::SplitHCursor);
        setToolTip("Drag to make the panel wider or narrower");
    }

protected:
    virtual void mousePressEvent(QMouseEvent* event) override{
        if (event->button() == Qt::LeftButton){
            m_dragging = true;
            m_start_x = event->globalPosition().toPoint().x();
            //  From the width it has, which a narrow window may have made
            //  less than the width that was asked for.
            m_start_width = m_frame.m_panel->width();
        }
    }
    virtual void mouseMoveEvent(QMouseEvent* event) override{
        if (m_dragging){
            m_frame.set_panel_width(m_start_width + m_start_x - event->globalPosition().toPoint().x());
        }
    }
    virtual void mouseReleaseEvent(QMouseEvent* event) override{
        if (event->button() == Qt::LeftButton && m_dragging){
            m_dragging = false;
            if (m_frame.on_panel_width){
                m_frame.on_panel_width(m_frame.panel_width());
            }
        }
    }

private:
    Frame& m_frame;
    bool m_dragging = false;
    int m_start_x = 0;
    int m_start_width = 0;
};



Frame::Frame(QWidget& central, const QString& name, const QString& version, TopBar::Actions actions){
    QVBoxLayout* rows = new QVBoxLayout(&central);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(0);

    m_top_bar = new TopBar(central, name, std::move(actions));
    rows->addWidget(m_top_bar);

    QHBoxLayout* middle = new QHBoxLayout();
    middle->setContentsMargins(0, 0, 0, 0);
    middle->setSpacing(0);
    rows->addLayout(middle, 1);

    m_side_bar = new QWidget(&central);
    m_side_bar->setObjectName("ppSidebar");
    m_side_bar->setAttribute(Qt::WA_StyledBackground, true);
    m_side_bar->setFixedWidth(SIDE_BAR_WIDTH);
    middle->addWidget(m_side_bar);
    m_side_layout = new QVBoxLayout(m_side_bar);
    m_side_layout->setContentsMargins(10, 6, 10, 10);
    m_side_layout->setSpacing(8);

    //  No background of its own: the window's shows through (the screen's
    //  color, or the wallpaper).
    m_content = new QWidget(&central);
    middle->addWidget(m_content, 1);
    m_content_layout = new QVBoxLayout(m_content);
    m_content_layout->setContentsMargins(12, 10, 12, 10);
    m_content_layout->setSpacing(0);
    //  The side bar and the panel can be hidden; this cannot.
    m_content->setMinimumWidth(CONTENT_MIN_WIDTH);

    m_grip = new Grip(central, *this);
    m_grip->setVisible(false);
    middle->addWidget(m_grip);
    m_panel = new PanelHost(central);
    m_panel->setVisible(false);
    middle->addWidget(m_panel);
    m_panel_layout = new QVBoxLayout(m_panel);
    m_panel_layout->setContentsMargins(0, 0, 0, 0);
    m_panel_layout->setSpacing(0);

    m_status_strip = new StatusStrip(central, version);
    rows->addWidget(m_status_strip);
}

void Frame::set_side_bar_visible(bool visible){
    m_side_bar->setVisible(visible);
    //  The user's own choice from here on.
    m_side_bar_made_room = false;
}
bool Frame::side_bar_visible() const{
    return !m_side_bar->isHidden();
}

void Frame::set_panel_visible(bool visible){
    if (visible == panel_visible()){
        return;
    }
    m_grip->setVisible(visible);
    m_panel->setVisible(visible);
    m_top_bar->set_assistant_open(visible);
    if (visible){
        //  With the panel next to it, the page or program in the middle
        //  should still have room: the list of programs makes way. (The
        //  window's own width: it has one before its parts are laid out.)
        const int width = m_panel->window()->width();
        const int left = width - SIDE_BAR_WIDTH - m_grip->width() - m_panel->wanted_width();
        if (!m_side_bar->isHidden() && width > 0 && left < CONTENT_GOOD_WIDTH){
            m_side_bar->setVisible(false);
            m_side_bar_made_room = true;
        }
    }else if (m_side_bar_made_room){
        m_side_bar->setVisible(true);
        m_side_bar_made_room = false;
    }
}
bool Frame::panel_visible() const{
    return !m_panel->isHidden();
}
void Frame::set_panel_width(int width){
    m_panel->set_wanted_width(width);
}
int Frame::panel_width() const{
    return m_panel->wanted_width();
}



}
}

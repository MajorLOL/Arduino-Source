/*  PocketPilot Logo
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <QPainter>
#include <QPainterPath>
#include "PocketPilot_Theme.h"
#include "PocketPilot_Logo.h"

namespace PokemonAutomation{
namespace PocketPilot{



void paint_logo(QPainter& painter, const QRectF& rect, const LogoColors& colors){
    const qreal side = qMin(rect.width(), rect.height());
    const qreal left = rect.left() + (rect.width() - side) / 2;
    const qreal top = rect.top() + (rect.height() - side) / 2;
    auto x = [=](qreal part){ return left + side * part; };
    auto y = [=](qreal part){ return top + side * part; };

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    //  The handheld: a wide body with round ends.
    QPainterPath body;
    body.addRoundedRect(QRectF(x(0.03), y(0.22), side * 0.94, side * 0.56), side * 0.17, side * 0.17);
    painter.fillPath(body, colors.body);

    //  Its screen in the middle.
    QPainterPath screen;
    screen.addRoundedRect(QRectF(x(0.30), y(0.30), side * 0.40, side * 0.40), side * 0.05, side * 0.05);
    painter.fillPath(screen, colors.screen);

    //  The "play" arrow on the screen: the game runs by itself.
    QPainterPath arrow;
    arrow.moveTo(x(0.435), y(0.385));
    arrow.lineTo(x(0.435), y(0.615));
    arrow.lineTo(x(0.615), y(0.500));
    arrow.closeSubpath();
    painter.fillPath(arrow, colors.mark);

    //  The directional pad left of the screen and two buttons right of it.
    //  Too small to draw in a small icon.
    if (side >= 22){
        const qreal arm = side * 0.085;
        const qreal thick = side * 0.058;
        const QPointF pad(x(0.165), y(0.50));
        painter.fillRect(QRectF(pad.x() - arm, pad.y() - thick / 2, 2 * arm, thick), colors.screen);
        painter.fillRect(QRectF(pad.x() - thick / 2, pad.y() - arm, thick, 2 * arm), colors.screen);
        painter.setBrush(colors.screen);
        const qreal radius = side * 0.042;
        painter.drawEllipse(QPointF(x(0.800), y(0.545)), radius, radius);
        painter.drawEllipse(QPointF(x(0.885), y(0.455)), radius, radius);
    }

    painter.restore();
}



QPixmap app_icon_pixmap(int size){
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    //  The tile: ember, the color of the Fire starter.
    const QColor tile(0xe2, 0x57, 0x30);
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, size, size), size * 0.22, size * 0.22);
    painter.fillPath(path, tile);

    LogoColors colors;
    colors.body = QColor(0xff, 0xf6, 0xec);
    colors.screen = QColor(0x2b, 0x1b, 0x17);
    colors.mark = QColor(0xff, 0x9d, 0x70);
    const qreal inset = size * 0.07;
    paint_logo(painter, QRectF(inset, inset, size - 2 * inset, size - 2 * inset), colors);
    return pixmap;
}

QIcon app_icon(){
    QIcon icon;
    for (int size : {16, 20, 24, 32, 40, 48, 64, 128, 256}){
        icon.addPixmap(app_icon_pixmap(size));
    }
    return icon;
}



LogoWidget::LogoWidget(QWidget& parent, int size)
    : QWidget(&parent)
    , m_size(size)
{
    setFixedSize(size, size);
}
QSize LogoWidget::sizeHint() const{
    return QSize(m_size, m_size);
}
void LogoWidget::paintEvent(QPaintEvent*){
    const ThemeColors& theme = theme_colors();
    LogoColors colors;
    colors.body = theme.shell_text;
    colors.screen = theme.shell_deep;
    colors.mark = theme.dark ? theme.accent : theme.shell_text;
    QPainter painter(this);
    paint_logo(painter, QRectF(rect()), colors);
}



}
}

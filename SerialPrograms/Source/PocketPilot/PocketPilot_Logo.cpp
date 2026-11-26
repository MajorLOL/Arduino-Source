/*  Claude GG Logo
 *
 *  Part of Claude GG (PocketPilot before), which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <QPainter>
#include <QPainterPath>
#include "PocketPilot_Logo.h"

namespace PokemonAutomation{
namespace PocketPilot{


namespace{

//  The picture is the same for every size: loaded once.
const QPixmap& mark_picture(){
    static const QPixmap picture(":/pocketpilot/setup/switch-gg.png");
    return picture;
}

}


void paint_logo(QPainter& painter, const QRectF& rect){
    const QPixmap& picture = mark_picture();
    if (picture.isNull() || rect.width() <= 0 || rect.height() <= 0){
        return;
    }
    const qreal scale = qMin(rect.width() / picture.width(), rect.height() / picture.height());
    const qreal width = picture.width() * scale;
    const qreal height = picture.height() * scale;
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawPixmap(
        QRectF(rect.left() + (rect.width() - width) / 2, rect.top() + (rect.height() - height) / 2, width, height),
        picture,
        QRectF(picture.rect())
    );
    painter.restore();
}



QPixmap app_icon_pixmap(int size){
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    //  The tile: black like the program's screens, with a thin light rim so
    //  that its shape shows on a dark task bar.
    const qreal rim = qMax<qreal>(1.0, size * 0.012);
    const QRectF tile(rim / 2, rim / 2, size - rim, size - rim);
    QPainterPath path;
    path.addRoundedRect(tile, size * ICON_TILE_RADIUS, size * ICON_TILE_RADIUS);
    painter.fillPath(path, QColor(0x0b, 0x0b, 0x0c));
    painter.setPen(QPen(QColor(255, 255, 255, 56), rim));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    painter.setPen(Qt::NoPen);

    const qreal width = size * ICON_MARK_WIDTH;
    //  The mark is wide: it takes the middle of the tile.
    paint_logo(painter, QRectF((size - width) / 2, size * 0.2, width, size * 0.6));
    return pixmap;
}

QIcon app_icon(){
    QIcon icon;
    for (int size : {16, 20, 24, 32, 40, 48, 64, 128, 256}){
        icon.addPixmap(app_icon_pixmap(size));
    }
    return icon;
}




}
}

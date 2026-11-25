/*  PocketPilot Claude Plays: the card of a game
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <algorithm>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainter>
#include <QStringList>
#include "ClaudePlays_GameCard.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace ClaudePlays{


namespace{

constexpr int WIDTH = 256;
constexpr int HEIGHT = 128;
//  Room for the text inside the card.
constexpr int MARGIN = 14;
constexpr int MAX_LINES = 3;

//  The name in lines no wider than the card (as far as the words allow).
QStringList break_into_lines(const QString& name, const QFontMetrics& metrics, int width){
    QStringList lines;
    QString line;
    for (const QString& word : name.split(' ', Qt::SkipEmptyParts)){
        const QString attempt = line.isEmpty() ? word : line + ' ' + word;
        if (line.isEmpty() || metrics.horizontalAdvance(attempt) <= width){
            line = attempt;
        }else{
            lines.append(line);
            line = word;
        }
    }
    if (!line.isEmpty()){
        lines.append(line);
    }
    return lines;
}

}


QImage make_game_card(const std::string& name, QRgb ground){
    QImage image(WIDTH, HEIGHT, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor::fromRgb(ground));
    painter.drawRoundedRect(QRectF(0, 0, WIDTH, HEIGHT), 16, 16);

    //  Fonts need the GUI application; the tests that run without one get
    //  the ground.
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) == nullptr){
        return image;
    }

    const QString text = QString::fromStdString(name);
    const int room = WIDTH - 2 * MARGIN;

    //  The largest size at which the name fits in two lines (not smaller than
    //  a legible size), and failing that in three.
    QFont font;
    font.setBold(true);
    QStringList lines;
    bool done = false;
    for (int max_lines = 2; max_lines <= MAX_LINES && !done; max_lines++){
        const int smallest = max_lines == MAX_LINES ? 12 : 24;
        for (int size = 34; size >= smallest; size -= 2){
            font.setPixelSize(size);
            const QFontMetrics metrics(font);
            lines = break_into_lines(text, metrics, room);
            done = (int)lines.size() <= max_lines &&
                std::all_of(lines.begin(), lines.end(), [&](const QString& line){
                    return metrics.horizontalAdvance(line) <= room;
                });
            if (done){
                break;
            }
        }
    }
    painter.setFont(font);
    painter.setPen(QColor(0xff, 0xff, 0xff));
    const QFontMetrics metrics(font);
    const int line_height = metrics.lineSpacing();
    const int total = line_height * (int)lines.size();
    const double top = (HEIGHT - total) / 2.0;
    for (int index = 0; index < lines.size(); index++){
        painter.drawText(
            QRectF(MARGIN, top + index * line_height, room, line_height),
            Qt::AlignCenter,
            lines[index]
        );
    }
    return image;
}



}
}
}

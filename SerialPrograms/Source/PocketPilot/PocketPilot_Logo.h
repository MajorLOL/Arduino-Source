/*  PocketPilot Logo
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The program's mark: a handheld console with a "play" arrow on its
 *  screen (the game plays by itself). Drawn here, so that it is sharp at
 *  every size and takes the colors of the theme.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Logo_H
#define PokemonAutomation_PocketPilot_Logo_H

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QRectF>
#include <QWidget>

class QPainter;

namespace PokemonAutomation{
namespace PocketPilot{


struct LogoColors{
    QColor body;    //  The handheld.
    QColor screen;  //  Its screen, and its pad and buttons.
    QColor mark;    //  The arrow on the screen.
};

//  The mark in a square (the largest one that fits in "rect", centered).
void paint_logo(QPainter& painter, const QRectF& rect, const LogoColors& colors);

//  The mark on its tile, as the program's icon: "size" pixels wide and high.
QPixmap app_icon_pixmap(int size);
//  The same in the sizes a window and the task bar ask for.
QIcon app_icon();


//  The mark in the colors of the shell, for the top bar.
class LogoWidget : public QWidget{
public:
    LogoWidget(QWidget& parent, int size);

    virtual QSize sizeHint() const override;

protected:
    virtual void paintEvent(QPaintEvent*) override;

private:
    int m_size;
};



}
}
#endif

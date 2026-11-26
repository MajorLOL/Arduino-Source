/*  Claude GG Logo
 *
 *  Part of Claude GG (PocketPilot before), which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The program's mark: the handheld Switch of the intro, with its orange
 *  Joy-Cons and the chat bubble "GG" on its screen (the same picture as on the
 *  setup's last step: Resources/setup/switch-gg.png). The program's name,
 *  Claude GG, is set beside or under it where there is room for it (the About
 *  window, the README), in the typeface of the headings.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Logo_H
#define PokemonAutomation_PocketPilot_Logo_H

#include <QIcon>
#include <QPixmap>
#include <QRectF>

class QPainter;

namespace PokemonAutomation{
namespace PocketPilot{


//  The Switch with the bubble, as large as fits in "rect" and centered in it.
void paint_logo(QPainter& painter, const QRectF& rect);

//  The mark on its tile, as the program's icon: "size" pixels wide and high.
//  A tile that is as dark as the program's screens, so that the orange and the
//  white of the Switch are what is seen, on a dark or a light task bar.
QPixmap app_icon_pixmap(int size);
//  The same in the sizes a window and the task bar ask for.
QIcon app_icon();

//  The proportions of the icon, as fractions of its size: the same numbers
//  draw IconResource/claudegg.ico, which the program's file has.
constexpr double ICON_TILE_RADIUS = 0.22;
constexpr double ICON_MARK_WIDTH = 0.94;



}
}
#endif

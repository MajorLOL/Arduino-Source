/*  PocketPilot Home
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <QColor>
#include <QString>
#include "PocketPilot_Home.h"

namespace PokemonAutomation{
namespace PocketPilot{



QString short_game_name(const QString& name){
    //  The series' name is the first word, with an accented letter in it.
    if (name.startsWith("Pok", Qt::CaseInsensitive)){
        const qsizetype space = name.indexOf(' ');
        if (space > 0 && space <= 8 && space + 1 < name.size()){
            const QString rest = name.mid(space + 1);
            //  Not for the one that would be left as "Home".
            if (rest.compare("Home", Qt::CaseInsensitive) != 0){
                return rest;
            }
        }
    }
    return name;
}

void game_label_colors(const QString& name, QColor& first, QColor& second){
    struct Entry{
        const char* word;
        QRgb first;
        QRgb second;
    };
    //  The pairs of versions first: each half of the label gets a version.
    static const Entry ENTRIES[] = {
        {"FireRed",         0xffdf5230, 0xff3f9d4e},
        {"Scarlet",         0xffd8402b, 0xff7a4cc4},
        {"Sword",           0xff2f8fd6, 0xffd23a5a},
        {"Brilliant",       0xff4f8fe6, 0xffe38bb0},
        {"Let's Go",        0xffe3b437, 0xff9a693b},
        {"Ruby",            0xffc23048, 0xff2f62c0},
        {"Arceus",          0xff5c6ea8, 0xff5c6ea8},
        {"Z-A",             0xff27a587, 0xff27a587},
        {"Pokopia",         0xffe2875c, 0xffe2875c},
        {"Home",            0xff2c9c93, 0xff2c9c93},
        {"Zelda",           0xff4c8c68, 0xff4c8c68},
        //  The list for whatever else is on the screen (ClaudePlays).
        {"Other Games",     0xff5a55b8, 0xff5a55b8},
    };
    for (const Entry& entry : ENTRIES){
        if (name.contains(QLatin1String(entry.word), Qt::CaseInsensitive)){
            first = QColor(entry.first);
            second = QColor(entry.second);
            return;
        }
    }
    first = QColor(0x67, 0x6d, 0x84);
    second = first;
}



}
}

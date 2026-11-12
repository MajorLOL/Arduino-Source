/*  PocketPilot About
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The "About" window: what the program is, which version runs, where it
 *  comes from and under which licenses.
 *
 */

#ifndef PokemonAutomation_PocketPilot_About_H
#define PokemonAutomation_PocketPilot_About_H

#include <memory>
#include <QString>

class QDialog;
class QWidget;

namespace PokemonAutomation{
namespace PocketPilot{


struct AboutInfo{
    QString name;               //  The program's name.
    QString build;              //  The number of the build, or "local".
    QString upstream_name;      //  The project this one is based on.
    QString upstream_version;   //  Its version in this build.
    QString architecture;       //  What the build was compiled for.
    QString compiler;
    QString project_url;        //  Where this program's source is.
    QString upstream_url;       //  Where the upstream project's source is.
};

//  The window, not shown yet.
std::unique_ptr<QDialog> make_about(QWidget* parent, const AboutInfo& info);

//  Shows the window and returns when it is closed.
void show_about(QWidget* parent, const AboutInfo& info);



}
}
#endif

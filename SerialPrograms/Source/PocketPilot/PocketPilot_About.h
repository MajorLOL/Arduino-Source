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
namespace Assistant{
    class HttpTransport;
}


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

//  The window, not shown yet. With a connection ("transport", which has to
//  stay for as long as the window), the window has a button "Check for
//  updates" (PocketPilot_Update.h); without one it has no such button.
std::unique_ptr<QDialog> make_about(QWidget* parent, const AboutInfo& info, Assistant::HttpTransport* transport = nullptr);

//  Shows the window, with the button to check for updates over the network,
//  and returns when it is closed.
void show_about(QWidget* parent, const AboutInfo& info);



}
}
#endif

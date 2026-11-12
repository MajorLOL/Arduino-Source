/*  PocketPilot Shell
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The frame of the main window: the top bar (the program's name, where
 *  you are, and the buttons that are always there), the side bar (for the
 *  list of programs), the room for the page or the program that is open,
 *  and the status strip at the bottom (what the program is doing, keeping
 *  the screen on, the version).
 *
 *  It knows nothing of the rest of the program: the main window hands it
 *  what to show and what to do on a click.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Shell_H
#define PokemonAutomation_PocketPilot_Shell_H

#include <functional>
#include <QString>
#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace PokemonAutomation{
namespace PocketPilot{


class TopBar : public QWidget{
public:
    struct Actions{
        std::function<void()> toggle_side_bar;
        std::function<void()> home;
        std::function<void()> output;
        std::function<void()> keyboard;
        std::function<void()> settings;
        std::function<void()> about;
    };

    TopBar(QWidget& parent, const QString& name, Actions actions);

    //  Where the user is: the game and the program, or only one of them, or
    //  nothing (the home page).
    void set_location(const QString& game, const QString& program);

    //  While a program runs it cannot be left: Games and Settings are off.
    void set_busy(bool busy);

private:
    QPushButton* add_button(const QString& text, const QString& tip, const std::function<void()>& action);

private:
    Actions m_actions;
    QLabel* m_location;
    QPushButton* m_home;
    QPushButton* m_settings;
};


class StatusStrip : public QWidget{
public:
    enum class State{
        READY,
        RUNNING,
    };

    StatusStrip(QWidget& parent, const QString& version);

    void set_state(State state, const QString& text);

    //  The system's sleep: what holds it off now, and the box that keeps
    //  the screen on. The main window fills the label and listens to the box.
    QLabel& sleep_label(){ return *m_sleep_label; }
    QCheckBox& sleep_box(){ return *m_sleep_box; }

private:
    class Lamp;

    Lamp* m_lamp;
    QLabel* m_state;
    QLabel* m_sleep_label;
    QCheckBox* m_sleep_box;
};


//  The whole frame, built into the main window's central widget:
//
//      top bar
//      side bar | content
//      status strip
//
class Frame{
public:
    static constexpr int SIDE_BAR_WIDTH = 292;

    Frame(QWidget& central, const QString& name, const QString& version, TopBar::Actions actions);

    TopBar& top_bar(){ return *m_top_bar; }
    StatusStrip& status_strip(){ return *m_status_strip; }

    //  The side bar and its layout, for the list of programs.
    QWidget& side_bar(){ return *m_side_bar; }
    QVBoxLayout& side_layout(){ return *m_side_layout; }

    //  The layout of the room right of the side bar, for the page or the
    //  program that is open.
    QWidget& content(){ return *m_content; }
    QVBoxLayout& content_layout(){ return *m_content_layout; }

    void set_side_bar_visible(bool visible);
    bool side_bar_visible() const;

private:
    TopBar* m_top_bar;
    QWidget* m_side_bar;
    QVBoxLayout* m_side_layout;
    QWidget* m_content;
    QVBoxLayout* m_content_layout;
    StatusStrip* m_status_strip;
};



}
}
#endif

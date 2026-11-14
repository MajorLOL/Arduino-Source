/*  Panel List Widget
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PanelListWidget_H
#define PokemonAutomation_PanelListWidget_H

#include <QListWidget>
#include "CommonFramework/Panels/PanelList.h"
#include "PocketPilot/PocketPilot_ProgramList.h"

namespace PokemonAutomation{



//  PocketPilot: on a list that can be locked while a program runs and
//  still scrolls then (PocketPilot::ProgramList), in place of one that is
//  disabled.
class PanelListWidget : public PocketPilot::ProgramList{
public:
    static const std::string JSON_PROGRAM_PANEL;

public:
    PanelListWidget(
        QWidget& parent, PanelHolder& holder,
        std::vector<PanelEntry> list
    );

    size_t items() const{ return m_panel_map.size(); }

    void set_panel(const std::string& panel_name);

    //  No program is open anymore: no row is marked.
    void clear_current();

    //  Shows only the programs with these words in their name or
    //  description. Returns how many are shown.
    int filter(const QString& words);

private:
    void handle_panel_clicked(const std::string& text);
    //  True while the list is locked: the click on this row opens nothing.
    bool refuse_while_locked(const QListWidgetItem& item);

private:
    PanelHolder& m_panel_holder;
    std::map<std::string, std::shared_ptr<const PanelDescriptor>> m_panel_map;
};



}
#endif

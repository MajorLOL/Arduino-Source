/*  Panel List Widget
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <QMessageBox>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/PersistentSettings.h"
//#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/Panels/PanelSession.h"
#include "PocketPilot/PocketPilot_ProgramList.h"
#include "PanelListWidget.h"

namespace PokemonAutomation{




PanelListWidget* PanelListDescriptor::make_QWidget(QWidget& parent, PanelHolder& holder) const{
    return new PanelListWidget(
        parent, holder,
        this->make_panels()
    );
}



const std::string PanelListWidget::JSON_PROGRAM_PANEL = "ProgramPanel";

PanelListWidget::PanelListWidget(
    QWidget& parent, PanelHolder& holder,
    std::vector<PanelEntry> list
)
    : PocketPilot::ProgramList(&parent)     //  With the looks of the side bar.
    , m_panel_holder(holder)
{
    for (PanelEntry& item : list){
        const std::string& display_name = item.display_name;
        PanelDescriptor* descriptor = item.descriptor.get();

        addItem(QString::fromStdString(display_name));
        QListWidgetItem* list_item = this->item(this->count() - 1);

        //  Label/divider: the title of a section. Not to be selected.
        if (descriptor == nullptr){
            list_item->setData(PocketPilot::PROGRAM_LIST_SECTION, true);
            list_item->setFlags(Qt::ItemIsEnabled);
            continue;
        }

        //  Program
        if (!m_panel_map.emplace(display_name, std::move(item.descriptor)).second){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Duplicate program name: " + display_name);
        }

        //  What kind of program it is shows as a small tag behind its name
        //  (it used to be the color of the name).
        std::string tag_meaning;
        const std::string tag = panel_standing_tag(descriptor->standing(), &tag_meaning);
        list_item->setData(PocketPilot::PROGRAM_LIST_TAG, QString::fromStdString(tag));

        std::string tip = descriptor->description();
        if (!tag_meaning.empty()){
            tip += tip.empty() ? "" : "\n\n";
            tip += tag_meaning;
        }
        list_item->setToolTip(QString::fromStdString(tip));
    }
    connect(
        this, &QListWidget::itemClicked,
        this, [this](QListWidgetItem* item){
            if (item->data(PocketPilot::PROGRAM_LIST_SECTION).toBool()){
                return;
            }
            if (refuse_while_locked(*item)){
                return;
            }
            handle_panel_clicked(item->text().toStdString());
        }
    );
    //  The Enter key on a row, for those who use the keyboard.
    connect(
        this, &QListWidget::itemActivated,
        this, [this](QListWidgetItem* item){
            if (item->data(PocketPilot::PROGRAM_LIST_SECTION).toBool()){
                return;
            }
            if (refuse_while_locked(*item)){
                return;
            }
            handle_panel_clicked(item->text().toStdString());
        }
    );
}

bool PanelListWidget::refuse_while_locked(const QListWidgetItem& item){
    if (!locked()){
        return false;
    }
    //  A program runs: no other one is opened. (The row of the one that
    //  runs needs no words.)
    if (!item.isSelected()){
        show_locked_hint();
    }
    return true;
}


void PanelListWidget::handle_panel_clicked(const std::string& text){
    auto iter = m_panel_map.find(text);
    if (iter == m_panel_map.end()){
        PERSISTENT_SETTINGS().panels[JSON_PROGRAM_PANEL] = "";
        return;
    }
    std::shared_ptr<const PanelDescriptor>& descriptor = iter->second;
//    cout << descriptor->display_name() << endl;
    if (!m_panel_holder.report_new_panel_intent(*descriptor)){
        return;
    }
    try{
        std::unique_ptr<PanelSession> panel = descriptor->make_panel();
//        try{
            panel->load_json(PERSISTENT_SETTINGS().panels[descriptor->identifier()]);
//        }catch (ParseException&){}
        m_panel_holder.load_panel(descriptor, std::move(panel));

        PERSISTENT_SETTINGS().panels[JSON_PROGRAM_PANEL] = iter->first;
    }catch (const Exception& error){
        QMessageBox box;
        box.critical(
            nullptr,
            "Error",
            "Failed to load program.\n\n" + QString::fromStdString(error.to_str())
        );
    }
}

void PanelListWidget::set_panel(const std::string& panel_name){
    const auto panels = findItems(QString::fromStdString(panel_name), Qt::MatchExactly);
    if (panels.size() > 0){
        setCurrentItem(panels[0]);
    }

    handle_panel_clicked(panel_name);
}

void PanelListWidget::clear_current(){
    setCurrentItem(nullptr);
    clearSelection();
}

int PanelListWidget::filter(const QString& words){
    return PocketPilot::filter_program_list(*this, words);
}




}

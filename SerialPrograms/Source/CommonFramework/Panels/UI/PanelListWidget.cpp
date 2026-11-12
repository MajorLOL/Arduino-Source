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
    : QListWidget(&parent)
    , m_panel_holder(holder)
{
    //  The looks of the side bar: how a row is drawn, and its height.
    PocketPilot::style_program_list(*this);

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
        std::string tag;
        std::string tag_meaning;
        switch (descriptor->standing()){
        case PanelQualityStanding::OFFICIAL:
            break;
        case PanelQualityStanding::OFFICIAL_BETA:
            tag = "beta";
            tag_meaning = "Beta: it may not be fully stable.";
            break;
        case PanelQualityStanding::OFFICIAL_BELOW_STANDARDS:
            tag = "rough";
            tag_meaning = "Rough: it works, but may be difficult to use.";
            break;
        case PanelQualityStanding::THIRD_PARTY:
            tag = "3rd party";
            tag_meaning = "3rd party: made by others and not fully vetted.";
            break;
        case PanelQualityStanding::THIRD_PARTY_UNVETTED:
            tag = "unvetted";
            tag_meaning = "Unvetted: made by others and not checked.";
            break;
        case PanelQualityStanding::UNMAINTAINED:
            tag = "unmaintained";
            tag_meaning = "Unmaintained: nobody keeps it working anymore.";
            break;
        case PanelQualityStanding::DEPRECATED:
            tag = "deprecated";
            tag_meaning = "Deprecated: it will likely be removed.";
            break;
        case PanelQualityStanding::DEVELOPER:
            tag = "dev";
            tag_meaning = "Dev: a tool for developers.";
            break;
        }
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
            handle_panel_clicked(item->text().toStdString());
        }
    );
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

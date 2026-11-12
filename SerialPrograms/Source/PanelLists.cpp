/*  Left-Side Panel
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <QLabel>
#include <QLineEdit>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include "Common/Qt/NoWheelComboBox.h"
#include "CommonFramework/StaticGlobals.h"
#include "CommonFramework/PersistentSettings.h"
#include "CommonFramework/ImageTypes/ImageRGB32_Qt.h"
#include "CommonFramework/Windows/DpiScaler.h"
#include "CommonFramework/Panels/UI/PanelListWidget.h"
#include "ML/ML_Panels.h"
#include "NintendoSwitch/NintendoSwitch_Panels.h"
#include "VideoGames/PokemonSwSh/PokemonSwSh_Panels.h"
#include "VideoGames/PokemonHome/PokemonHome_Panels.h"
#include "PokemonBDSP/PokemonBDSP_Panels.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Panels.h"
#include "VideoGames/PokemonLA/PokemonLA_Panels.h"
#include "VideoGames/PokemonLGPE/PokemonLGPE_Panels.h"
#include "VideoGames/PokemonRSE/PokemonRSE_Panels.h"
#include "VideoGames/PokemonSV/PokemonSV_Panels.h"
#include "VideoGames/PokemonLZA/PokemonLZA_Panels.h"
#include "VideoGames/PokemonPokopia/PokemonPokopia_Panels.h"
#include "VideoGames/ZeldaTotK/ZeldaTotK_Panels.h"
#include "PanelLists.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{



ProgramSelect::ProgramSelect(QWidget& parent, PanelHolder& holder)
    : QWidget(&parent)
    , m_holder(holder)
{
    setObjectName(QStringLiteral("ppProgramSelect"));
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignTop);
    m_dropdown = new NoWheelCompactComboBox(this);
    m_dropdown->setObjectName(QStringLiteral("ppGamePicker"));
    m_dropdown->setMaxVisibleItems(20);
    m_dropdown->setAccessibleName("Game");
    m_dropdown->setToolTip("The game whose programs are listed below");
    this->setStyleSheet("QComboBox QAbstractItemView::item { min-height: 24px; }");
    {
        //  A list view of its own, so that the rule above applies to its rows.
        //  No word wrap: with it the list measures its rows before it has its
        //  width, and opens almost twice as tall as its rows the first time.
        QListView* list_view = new QListView(this);
        m_dropdown->setView(list_view);
    }
    {
//        QSize size = m_dropdown->iconSize();
//        cout << "size = " << size.width() << " x " << size.height() << endl;
//        size.setWidth(size.width() * 2);
        m_dropdown->setIconSize(QSize(48, 24));
    }
    layout->addWidget(m_dropdown);

    //  The search over the list below.
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("ppSearch"));
    m_search->setPlaceholderText("Search this game's programs");
    m_search->setAccessibleName("Search this game's programs");
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);
    m_nothing_found = new QLabel("No program of this game has these words. The Games page searches all games.", this);
    m_nothing_found->setObjectName(QStringLiteral("ppSidebarLabel"));
    m_nothing_found->setWordWrap(true);
    m_nothing_found->setVisible(false);
    layout->addWidget(m_nothing_found);


    add(std::make_unique<NintendoSwitch::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonHome::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonLGPE::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonSwSh::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonBDSP::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonLA::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonSV::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonLZA::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonFRLG::PanelListFactory>());
    add(std::make_unique<NintendoSwitch::PokemonPokopia::PanelListFactory>());
    if (STATIC_GLOBALS.DEVELOPER_MODE){
        add(std::make_unique<NintendoSwitch::PokemonRSE::PanelListFactory>());
    }

    add(std::make_unique<NintendoSwitch::ZeldaTotK::PanelListFactory>());

    if (STATIC_GLOBALS.DEVELOPER_MODE){
        add(std::make_unique<ML::PanelListFactory>());
    }


    //  Load the 1st list by default.
    m_dropdown->setCurrentIndex(0);
    m_active_index = 0;
    m_active_list = m_lists[0]->make_QWidget(*this, m_holder);
    layout->addWidget(m_active_list);

    connect(
        m_dropdown, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated),
        this, [this](int index){
            change_list(index);
        }
    );
    connect(
        m_search, &QLineEdit::textChanged,
        this, [this](const QString&){
            apply_filter();
        }
    );
}

void ProgramSelect::lock(){
    m_locked = true;
    m_active_list->setEnabled(false);
}
void ProgramSelect::unlock(){
    m_locked = false;
    m_active_list->setEnabled(true);
}

void ProgramSelect::add(std::unique_ptr<PanelListDescriptor> list){
    int index = m_dropdown->count();
    const ImageViewRGB32& icon = list->icon();
    //  Without the series' name in front: the box is narrow.
    const QString short_name = PocketPilot::short_game_name(QString::fromStdString(list->name()));
    if (icon){
        m_dropdown->addItem(
            QIcon(QPixmap::fromImage(to_QImage_ref(icon))),
            short_name
        );
    }else{
        m_dropdown->addItem(short_name);
    }
    m_dropdown->setItemData(index, QString::fromStdString(list->name()), Qt::ToolTipRole);

    m_lists.emplace_back(std::move(list));
    const PanelListDescriptor& back = *m_lists.back();
    if (!m_tab_map.emplace(back.name(), index).second){
        m_lists.pop_back();
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Duplicate Category Name: " + back.name());
    }
    if (!back.enabled()){
        auto* model = qobject_cast<QStandardItemModel*>(m_dropdown->model());
        if (model != nullptr){
            QStandardItem* line_handle = model->item(index);
            if (line_handle != nullptr){
                line_handle->setEnabled(false);
            }
        }
    }
}



void ProgramSelect::load_persistent_panel(){
    const std::string* str = PERSISTENT_SETTINGS().panels.get_string("ProgramCategory");
    if (str == nullptr){
        return;
    }
    auto iter = m_tab_map.find(*str);
    if (iter == m_tab_map.end()){
        return;
    }
    m_dropdown->setCurrentIndex(iter->second);
    change_list(iter->second);
    str = PERSISTENT_SETTINGS().panels.get_string(PanelListWidget::JSON_PROGRAM_PANEL);
    if (str != nullptr){
        m_active_list->set_panel(*str);
    }
}

void ProgramSelect::change_list(int index){
    if (index < 0 || index >= (int)m_lists.size()){
        return;
    }
    if (m_active_index == index && m_active_list != nullptr){
        return;
    }
    m_dropdown->setCurrentIndex(index);
    m_active_index = index;
    PERSISTENT_SETTINGS().panels["ProgramCategory"] = m_lists[index]->name();
    delete m_active_list;
    m_active_list = m_lists[index]->make_QWidget(*this, m_holder);
    m_active_list->setEnabled(!m_locked);
    layout()->addWidget(m_active_list);
    //  The words searched for were for the other game.
    if (m_search->text().isEmpty()){
        apply_filter();
    }else{
        m_search->clear();
    }
}

void ProgramSelect::apply_filter(){
    if (m_active_list == nullptr){
        return;
    }
    const int shown = m_active_list->filter(m_search->text());
    m_nothing_found->setVisible(shown == 0 && !m_search->text().trimmed().isEmpty());
}

void ProgramSelect::persistent_panel(std::string& game, std::string& program) const{
    game.clear();
    program.clear();
    const std::string* str = PERSISTENT_SETTINGS().panels.get_string("ProgramCategory");
    if (str == nullptr || m_tab_map.find(*str) == m_tab_map.end()){
        return;
    }
    game = *str;
    str = PERSISTENT_SETTINGS().panels.get_string(PanelListWidget::JSON_PROGRAM_PANEL);
    if (str != nullptr){
        program = *str;
    }
}

void ProgramSelect::describe(
    std::vector<PocketPilot::HomeGame>& games,
    std::vector<PocketPilot::HomeProgram>& programs
) const{
    games.clear();
    programs.clear();
    for (size_t index = 0; index < m_lists.size(); index++){
        const PanelListDescriptor& list = *m_lists[index];
        PocketPilot::HomeGame game;
        game.name = QString::fromStdString(list.name());
        const ImageViewRGB32& icon = list.icon();
        if (icon){
            //  A copy: the pixmap must not point into the list's picture.
            game.icon = QPixmap::fromImage(to_QImage_ref(icon).copy());
        }
        game.enabled = list.enabled();
        for (PanelListDescriptor::ProgramSummary& summary : list.program_summaries()){
            PocketPilot::HomeProgram program;
            program.game = (int)index;
            program.section = QString::fromStdString(summary.section);
            program.name = QString::fromStdString(summary.name);
            program.description = QString::fromStdString(summary.description);
            programs.emplace_back(std::move(program));
            game.programs++;
        }
        games.emplace_back(std::move(game));
    }
}

const std::string& ProgramSelect::current_game() const{
    return m_lists[m_active_index]->name();
}

void ProgramSelect::open_game(int game){
    change_list(game);
}
void ProgramSelect::open_program(int game, const std::string& program){
    if (game < 0 || game >= (int)m_lists.size()){
        return;
    }
    change_list(game);
    m_active_list->set_panel(program);
}
bool ProgramSelect::open_program(const std::string& game, const std::string& program){
    auto iter = m_tab_map.find(game);
    if (iter == m_tab_map.end()){
        return false;
    }
    open_program(iter->second, program);
    return true;
}

void ProgramSelect::clear_current(){
    if (m_active_list != nullptr){
        m_active_list->clear_current();
    }
}

QSize ProgramSelect::sizeHint() const{
    QSize size = QWidget::sizeHint();
//    cout << size.width() << " x " << size.height() << endl;
//    cout << this->size().width() << " x " << this->size().height() << endl;
    size.setWidth(scale_dpi_width(size.width() + 10));
    return size;
}







}

/*  Options Panel Widget
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Panels/UI/PanelElements.h"
#include "PocketPilot/PocketPilot_SettingsSearch.h"
#include "OptionsPanelWidget.h"

namespace PokemonAutomation{

template class RegisterUiStateQtWidget<OptionsPanelWidget>;




OptionsPanelWidget::OptionsPanelWidget(
    QWidget& parent,
    OptionsPanelSession& session
)
    : QWidget(&parent)
{
    populate_panel_widget(
        *this,
        session.descriptor(),
        nullptr,
        session.options(),
        {make_actions_bar(*this, session)}
    );
    //  PocketPilot: a search field above the options.
    PocketPilot::add_options_search(*this);
}




}

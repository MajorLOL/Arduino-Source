/*  PocketPilot Settings Search
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  A search field above the list of options of the settings page (and of
 *  the settings of a game): it hides the options that do not have the
 *  typed words in their text, and the groups that have none of them.
 *
 *  -   The text that is searched is the text that is shown, so in the
 *      language that is chosen, and with the explanation of an option as
 *      well as its name. Upper and lower case and accents do not matter
 *      ("francais" finds "Français").
 *  -   Every word that is typed has to be in the option (or in the title of
 *      the group it is in).
 *  -   A group that is closed (double click opens it) is searched by its
 *      title only, since what is in it is not made until it is opened.
 *  -   An option that the program itself hides (an advanced one) stays
 *      hidden, and the search shows again only what it hid.
 *  -   Ctrl+F goes to the field.
 *
 */

#ifndef PokemonAutomation_PocketPilot_SettingsSearch_H
#define PokemonAutomation_PocketPilot_SettingsSearch_H

#include <stddef.h>
#include <QString>

class QLineEdit;
class QWidget;

namespace PokemonAutomation{
namespace PocketPilot{


//  The text of a search, to compare with: in lower case and without accents.
QString fold_search_text(const QString& text);

//  Hides the options in "list" (the widget that holds the rows of options)
//  that do not match the words of "text", and shows the ones that do. An
//  empty text shows everything again. Returns how many options are left.
size_t filter_options(QWidget& list, const QString& text);

//  Puts the search field above the list of options of an options panel (the
//  one that populate_panel_widget() made). Returns it, or nullptr when the
//  panel has no list.
QLineEdit* add_options_search(QWidget& panel);


}
}
#endif

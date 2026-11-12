/*  PocketPilot Program List
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  How the list of a game's programs looks in the side bar, and the search
 *  box above it. A row is a program or the title of a section. The program
 *  that is open has the arrow of a game menu in front of it; a program that
 *  is not a regular one (beta, third party, ...) has a small tag behind its
 *  name.
 *
 */

#ifndef PokemonAutomation_PocketPilot_ProgramList_H
#define PokemonAutomation_PocketPilot_ProgramList_H

#include <QString>
#include <QStyledItemDelegate>

class QListWidget;

namespace PokemonAutomation{
namespace PocketPilot{


//  What the list's rows carry besides their text.
enum ProgramListRole{
    //  bool: the row is the title of a section, not a program.
    PROGRAM_LIST_SECTION = Qt::UserRole + 40,
    //  QString: the tag behind the name. Empty for a regular program.
    PROGRAM_LIST_TAG,
};

//  The title of a section without its dashes: "---- Story ----" becomes
//  "Story".
QString section_title(const QString& text);

//  Gives a list the looks of the side bar: this delegate, hover and no frame.
void style_program_list(QListWidget& list);

//  Shows the programs with the words in their name or description, and the
//  sections that still have a program. No words: everything. Returns how
//  many programs are shown.
int filter_program_list(QListWidget& list, const QString& words);


class ProgramListDelegate : public QStyledItemDelegate{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    virtual void paint(
        QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index
    ) const override;
    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};



}
}
#endif

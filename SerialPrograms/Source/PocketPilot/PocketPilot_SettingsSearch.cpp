/*  PocketPilot Settings Search
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <vector>
#include <QAbstractButton>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QScrollArea>
#include <QShortcut>
#include <QStringList>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QVBoxLayout>
#include "Common/Qt/Options/BatchWidget.h"
#include "Common/Qt/Options/ConfigWidget.h"
#include "Common/Qt/Options/GroupWidget.h"
#include "PocketPilot_SettingsSearch.h"

namespace PokemonAutomation{
namespace PocketPilot{

namespace{

//  Set on a row that the search hid, so it is known from one that the
//  program hid itself.
const char HIDDEN_BY_SEARCH[] = "ppSearchHidden";


//  The text a widget shows by itself.
void add_own_text(const QWidget& widget, QString& text){
    if (const QLabel* label = qobject_cast<const QLabel*>(&widget)){
        QString shown = label->text();
        if (label->textFormat() != Qt::PlainText && Qt::mightBeRichText(shown)){
            shown = QTextDocumentFragment::fromHtml(shown).toPlainText();
        }
        text += shown + '\n';
    }else if (const QAbstractButton* button = qobject_cast<const QAbstractButton*>(&widget)){
        QString shown = button->text();
        shown.remove('&');
        text += shown + '\n';
    }else if (const QGroupBox* group = qobject_cast<const QGroupBox*>(&widget)){
        text += group->title() + '\n';
    }else if (const QComboBox* box = qobject_cast<const QComboBox*>(&widget)){
        for (int c = 0; c < box->count(); c++){
            text += box->itemText(c) + '\n';
        }
    }
}

//  The text of a widget and of what is in it that is shown (what the
//  program hid is not searched).
void add_text(const QWidget& widget, QString& text){
    if (widget.isHidden() && !widget.property(HIDDEN_BY_SEARCH).toBool()){
        return;
    }
    add_own_text(widget, text);
    for (const QObject* child : widget.children()){
        if (const QWidget* inner = qobject_cast<const QWidget*>(child)){
            add_text(*inner, text);
        }
    }
}

//  The rows of options in a layout: the widgets of options, looking through
//  the boxes that only hold them.
void collect_rows(QLayout* layout, std::vector<QWidget*>& rows){
    if (layout == nullptr){
        return;
    }
    for (int c = 0; c < layout->count(); c++){
        QLayoutItem* item = layout->itemAt(c);
        if (item == nullptr){
            continue;
        }
        if (QWidget* widget = item->widget()){
            if (dynamic_cast<ConfigWidget*>(widget) != nullptr){
                rows.push_back(widget);
            }else{
                collect_rows(widget->layout(), rows);
            }
        }else{
            collect_rows(item->layout(), rows);
        }
    }
}

//  The text that decides if a row is a match: of an option, all it shows; of
//  a group, its title (what is in it is looked at row by row).
QString row_text(QWidget& row){
    QString text;
    if (dynamic_cast<BatchWidget*>(&row) != nullptr){
        return text;
    }
    if (dynamic_cast<GroupWidget*>(&row) != nullptr){
        if (const QGroupBox* box = row.findChild<QGroupBox*>(QString(), Qt::FindDirectChildrenOnly)){
            text = box->title();
        }
        return text;
    }
    add_text(row, text);
    return text;
}

bool has_all(const QString& folded_text, const QStringList& words){
    for (const QString& word : words){
        if (!folded_text.contains(word)){
            return false;
        }
    }
    return true;
}

void set_hidden(QWidget& row, bool hide){
    if (hide){
        //  Not one that the program hides: that one stays as it is.
        if (!row.isHidden()){
            row.setProperty(HIDDEN_BY_SEARCH, true);
            row.setVisible(false);
        }
    }else if (row.property(HIDDEN_BY_SEARCH).toBool()){
        row.setProperty(HIDDEN_BY_SEARCH, false);
        row.setVisible(true);
    }
}

//  Shows or hides a row and what is in it. "above" is the text of the titles
//  of the groups the row is in (folded), which count as part of its text: in
//  "alpha beta" the first word can be the group and the second the option.
//  Returns how many options are shown in it.
size_t filter_row(QWidget& row, const QStringList& words, const QString& above){
    if (row.isHidden() && !row.property(HIDDEN_BY_SEARCH).toBool()){
        return 0;
    }
    std::vector<QWidget*> inner;
    collect_rows(row.layout(), inner);

    const QString text = above + '\n' + fold_search_text(row_text(row));
    size_t shown = 0;
    if (inner.empty()){
        shown = has_all(text, words) ? 1 : 0;
    }else{
        for (QWidget* item : inner){
            shown += filter_row(*item, words, text);
        }
    }
    set_hidden(row, shown == 0);
    return shown;
}

}


QString fold_search_text(const QString& text){
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString folded;
    folded.reserve(decomposed.size());
    for (const QChar c : decomposed){
        if (c.category() == QChar::Mark_NonSpacing){
            continue;
        }
        folded += c == QChar(0xa0) ? QChar(' ') : c.toCaseFolded();
    }
    return folded;
}

size_t filter_options(QWidget& list, const QString& text){
    const QStringList words = fold_search_text(text).split(' ', Qt::SkipEmptyParts);
    std::vector<QWidget*> rows;
    collect_rows(list.layout(), rows);
    size_t shown = 0;
    for (QWidget* row : rows){
        shown += filter_row(*row, words, QString());
    }
    return shown;
}

QLineEdit* add_options_search(QWidget& panel){
    QScrollArea* scroll = panel.findChild<QScrollArea*>("backgroundScrollArea");
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(panel.layout());
    if (scroll == nullptr || layout == nullptr || scroll->widget() == nullptr){
        return nullptr;
    }
    const int at = layout->indexOf(scroll);
    if (at < 0){
        return nullptr;
    }

    //  The texts are English here; the language filter of the program
    //  translates them, also when the language is changed.
    QLineEdit* search = new QLineEdit(&panel);
    search->setObjectName("ppOptionsSearch");
    search->setPlaceholderText("Search the settings");
    search->setClearButtonEnabled(true);
    layout->insertWidget(at, search);

    QLabel* none = new QLabel("No setting has these words.", &panel);
    none->setObjectName("ppOptionsSearchNone");
    none->setContentsMargins(6, 4, 6, 4);
    none->setVisible(false);
    layout->insertWidget(at + 1, none);

    QWidget* list = scroll->widget();
    QObject::connect(search, &QLineEdit::textChanged, none, [list, none](const QString& text){
        const size_t shown = filter_options(*list, text);
        none->setVisible(shown == 0 && !text.trimmed().isEmpty());
    });
    new QShortcut(QKeySequence::Find, &panel, [search]{
        search->setFocus();
        search->selectAll();
    });
    return search;
}


}
}

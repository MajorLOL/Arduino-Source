/*  PocketPilot Program List
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include "PocketPilot_Theme.h"
#include "PocketPilot_ProgramList.h"

namespace PokemonAutomation{
namespace PocketPilot{

namespace{

const int ROW_PADDING = 10;         //  Added to the height of the text.
const int SECTION_EXTRA = 8;        //  Room above the title of a section.
const int SIDE = 6;                 //  From the list's edge to a row's highlight.
const int TEXT_LEFT = 28;           //  Where a program's name starts.

}



QString section_title(const QString& text){
    qsizetype begin = 0;
    qsizetype end = text.size();
    while (begin < end && (text[begin] == '-' || text[begin].isSpace())){
        begin++;
    }
    while (end > begin && (text[end - 1] == '-' || text[end - 1].isSpace())){
        end--;
    }
    return text.mid(begin, end - begin);
}

void style_program_list(QListWidget& list){
    list.setObjectName("ppProgramList");
    list.setItemDelegate(new ProgramListDelegate(&list));
    list.setFrameShape(QFrame::NoFrame);
    list.setMouseTracking(true);
    list.viewport()->setAttribute(Qt::WA_Hover);
    list.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list.setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list.setTextElideMode(Qt::ElideRight);
}

int filter_program_list(QListWidget& list, const QString& words){
    const QString wanted = words.trimmed();
    const int rows = list.count();
    int shown = 0;
    //  Backward: a section is shown when a program below it is.
    bool program_below = false;
    for (int row = rows - 1; row >= 0; row--){
        QListWidgetItem* item = list.item(row);
        if (item->data(PROGRAM_LIST_SECTION).toBool()){
            item->setHidden(!wanted.isEmpty() && !program_below);
            program_below = false;
            continue;
        }
        const bool match =
            wanted.isEmpty() ||
            item->text().contains(wanted, Qt::CaseInsensitive) ||
            item->toolTip().contains(wanted, Qt::CaseInsensitive);
        item->setHidden(!match);
        if (match){
            program_below = true;
            shown++;
        }
    }
    return shown;
}



QSize ProgramListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const{
    int height = option.fontMetrics.height() + ROW_PADDING;
    if (index.data(PROGRAM_LIST_SECTION).toBool()){
        height += SECTION_EXTRA;
    }
    return QSize(0, height);
}

void ProgramListDelegate::paint(
    QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index
) const{
    const ThemeColors& theme = theme_colors();
    const QRect row = option.rect;
    const bool enabled = option.state.testFlag(QStyle::State_Enabled);
    const QString text = index.data(Qt::DisplayRole).toString();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    //  The title of a section: small, with a line to the right of it.
    if (index.data(PROGRAM_LIST_SECTION).toBool()){
        QFont font = option.font;
        font.setBold(true);
        painter->setFont(font);
        const QString title = section_title(text);
        const QRect text_box(row.left() + SIDE + 6, row.top() + SECTION_EXTRA, row.width() - 2 * SIDE - 12, row.height() - SECTION_EXTRA);
        painter->setPen(theme.shell_muted);
        painter->drawText(text_box, Qt::AlignLeft | Qt::AlignVCenter, title);
        const int text_end = text_box.left() + QFontMetrics(font).horizontalAdvance(title) + 10;
        if (text_end < text_box.right() - 8){
            painter->setPen(mix_colors(theme.shell, theme.shell_text, 0.22));
            const int y = text_box.center().y() + 1;
            painter->drawLine(text_end, y, text_box.right(), y);
        }
        painter->restore();
        return;
    }

    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool hover = enabled && option.state.testFlag(QStyle::State_MouseOver);

    //  The highlight: the open program, and the one under the mouse.
    const QColor highlight = mix_colors(theme.shell, theme.shell_text, selected ? 0.22 : 0.11);
    if (selected || hover){
        QPainterPath path;
        path.addRoundedRect(QRectF(row).adjusted(SIDE, 1, -SIDE, -1), 6, 6);
        painter->fillPath(path, highlight);
    }

    //  The arrow of a game menu in front of the open program.
    if (selected){
        const qreal middle = row.center().y() + 0.5;
        const qreal left = row.left() + SIDE + 9;
        QPainterPath arrow;
        arrow.moveTo(left, middle - 5);
        arrow.lineTo(left + 7, middle);
        arrow.lineTo(left, middle + 5);
        arrow.closeSubpath();
        //  In the accent color where it stands out on the row. It does not
        //  by day (the accent is the shell's own color) and in the classic
        //  dark look (a dark blue): there the arrow has the text's color.
        QColor arrow_color = theme.dark ? theme.accent : theme.shell_text;
        if (contrast_ratio(arrow_color, highlight) < 2.0){
            arrow_color = theme.shell_text;
        }
        painter->fillPath(arrow, arrow_color);
    }

    //  The tag behind the name, at the right edge.
    int text_right = row.right() - SIDE - 8;
    const QString tag = index.data(PROGRAM_LIST_TAG).toString();
    if (!tag.isEmpty()){
        QFont tag_font = option.font;
        if (tag_font.pointSizeF() > 0){
            tag_font.setPointSizeF(tag_font.pointSizeF() * 0.86);
        }
        const QFontMetrics metrics(tag_font);
        const int tag_width = metrics.horizontalAdvance(tag) + 12;
        const int tag_height = metrics.height() + 2;
        const QRect tag_box(text_right - tag_width, row.center().y() - tag_height / 2 + 1, tag_width, tag_height);
        QPainterPath path;
        path.addRoundedRect(QRectF(tag_box), 4, 4);
        painter->fillPath(path, mix_colors(theme.shell, theme.shell_text, selected ? 0.36 : 0.18));
        painter->setFont(tag_font);
        painter->setPen(enabled ? theme.shell_text : theme.shell_muted);
        painter->drawText(tag_box, Qt::AlignCenter, tag);
        text_right = tag_box.left() - 8;
    }

    //  The name.
    QFont font = option.font;
    font.setBold(selected);
    painter->setFont(font);
    painter->setPen(enabled ? theme.shell_text : mix_colors(theme.shell_text, theme.shell, 0.45));
    const QRect text_box(row.left() + TEXT_LEFT, row.top(), text_right - row.left() - TEXT_LEFT, row.height());
    painter->drawText(
        text_box, Qt::AlignLeft | Qt::AlignVCenter,
        QFontMetrics(font).elidedText(text, Qt::ElideRight, text_box.width())
    );

    painter->restore();
}



}
}

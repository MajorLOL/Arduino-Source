/*  PocketPilot Home
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <algorithm>
#include <utility>
#include <QAbstractButton>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include "PocketPilot_ProgramList.h"
#include "PocketPilot_Theme.h"
#include "PocketPilot_Home.h"

namespace PokemonAutomation{
namespace PocketPilot{



QString short_game_name(const QString& name){
    //  The series' name is the first word, with an accented letter in it.
    if (name.startsWith("Pok", Qt::CaseInsensitive)){
        const qsizetype space = name.indexOf(' ');
        if (space > 0 && space <= 8 && space + 1 < name.size()){
            const QString rest = name.mid(space + 1);
            //  Not for the one that would be left as "Home".
            if (rest.compare("Home", Qt::CaseInsensitive) != 0){
                return rest;
            }
        }
    }
    return name;
}

void game_label_colors(const QString& name, QColor& first, QColor& second){
    struct Entry{
        const char* word;
        QRgb first;
        QRgb second;
    };
    //  The pairs of versions first: each half of the label gets a version.
    static const Entry ENTRIES[] = {
        {"FireRed",         0xffdf5230, 0xff3f9d4e},
        {"Scarlet",         0xffd8402b, 0xff7a4cc4},
        {"Sword",           0xff2f8fd6, 0xffd23a5a},
        {"Brilliant",       0xff4f8fe6, 0xffe38bb0},
        {"Let's Go",        0xffe3b437, 0xff9a693b},
        {"Ruby",            0xffc23048, 0xff2f62c0},
        {"Arceus",          0xff5c6ea8, 0xff5c6ea8},
        {"Z-A",             0xff27a587, 0xff27a587},
        {"Pokopia",         0xffe2875c, 0xffe2875c},
        {"Home",            0xff2c9c93, 0xff2c9c93},
        {"Zelda",           0xff4c8c68, 0xff4c8c68},
    };
    for (const Entry& entry : ENTRIES){
        if (name.contains(QLatin1String(entry.word), Qt::CaseInsensitive)){
            first = QColor(entry.first);
            second = QColor(entry.second);
            return;
        }
    }
    first = QColor(0x67, 0x6d, 0x84);
    second = first;
}



namespace{

//  How wide the text, the search box and the programs found get at most,
//  and the list of one game's programs.
const int TEXT_WIDTH = 620;
const int LIST_WIDTH = 980;


//  A rounded box with its top right corner cut off: the outline of a game
//  cartridge.
QPainterPath cartridge_path(const QRectF& box, qreal corner, qreal cut){
    QPainterPath path;
    path.moveTo(box.left() + corner, box.top());
    path.lineTo(box.right() - cut, box.top());
    path.lineTo(box.right(), box.top() + cut);
    path.lineTo(box.right(), box.bottom() - corner);
    path.quadTo(box.right(), box.bottom(), box.right() - corner, box.bottom());
    path.lineTo(box.left() + corner, box.bottom());
    path.quadTo(box.left(), box.bottom(), box.left(), box.bottom() - corner);
    path.lineTo(box.left(), box.top() + corner);
    path.quadTo(box.left(), box.top(), box.left() + corner, box.top());
    path.closeSubpath();
    return path;
}


}



//  One game, as a cartridge: its picture on a label in the game's colors,
//  its name and how many programs it has.
class HomePage::GameTile : public QAbstractButton{
public:
    //  The grid makes it as wide as the row allows, between these two.
    static constexpr int MIN_WIDTH = 196;
    static constexpr int MAX_WIDTH = 252;
    static constexpr int HEIGHT = 168;

    GameTile(QWidget& parent, const HomeGame& game)
        : QAbstractButton(&parent)
        , m_name(short_game_name(game.name))
        , m_icon(game.icon)
        , m_programs(game.programs)
    {
        game_label_colors(game.name, m_first, m_second);
        setFixedHeight(HEIGHT);
        setAttribute(Qt::WA_Hover);
        setFocusPolicy(Qt::StrongFocus);
        setEnabled(game.enabled);
        if (game.enabled){
            setCursor(Qt::PointingHandCursor);
        }
        setAccessibleName(game.name);
        setToolTip(game.name);
    }

protected:
    virtual bool event(QEvent* event) override{
        switch (event->type()){
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::FocusIn:
        case QEvent::FocusOut:
            update();
            break;
        default:;
        }
        return QAbstractButton::event(event);
    }

    virtual void paintEvent(QPaintEvent*) override{
        const ThemeColors& theme = theme_colors();
        const bool hot = isEnabled() && (underMouse() || hasFocus());

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        //  The cartridge.
        const QRectF box = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        const QPainterPath body = cartridge_path(box, 11, 20);
        painter.fillPath(body, hot ? theme.surface_alt : theme.surface);
        QPen outline(hot ? theme.accent : theme.line);
        outline.setWidthF(hot ? 2.0 : 1.0);
        painter.setPen(outline);
        painter.drawPath(body);

        //  The label, in the game's colors: one half for each version.
        const QRectF label(box.left() + 11, box.top() + 11, box.width() - 22, 78);
        const QPainterPath label_path = cartridge_path(label, 6, 12);
        painter.save();
        painter.setClipPath(label_path);
        painter.setPen(Qt::NoPen);
        painter.fillRect(label, m_first);
        if (m_second != m_first){
            QPolygonF half;
            half << QPointF(label.left() + label.width() * 0.60, label.top())
                 << label.topRight()
                 << label.bottomRight()
                 << QPointF(label.left() + label.width() * 0.40, label.bottom());
            painter.setBrush(m_second);
            painter.drawPolygon(half);
        }
        if (!isEnabled()){
            QColor veil = theme.surface;
            veil.setAlphaF(0.55f);
            painter.fillRect(label, veil);
        }
        if (!m_icon.isNull()){
            const QSizeF room(label.width() - 26, label.height() - 16);
            const QSizeF size = QSizeF(m_icon.size()).scaled(room, Qt::KeepAspectRatio);
            const QRectF target(
                label.center().x() - size.width() / 2,
                label.center().y() - size.height() / 2,
                size.width(), size.height()
            );
            QPainterPath picture;
            picture.addRoundedRect(target, 4, 4);
            painter.setClipPath(picture, Qt::IntersectClip);
            painter.setOpacity(isEnabled() ? 1.0 : 0.5);
            painter.drawPixmap(target, m_icon, QRectF(m_icon.rect()));
        }
        painter.restore();

        //  The name, on up to two lines.
        painter.setFont(display_font(20));
        painter.setPen(isEnabled() ? theme.text : theme.disabled);
        const QRectF name_box(box.left() + 13, label.bottom() + 6, box.width() - 26, 42);
        painter.drawText(name_box, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_name);

        //  How many programs.
        QString count;
        if (!isEnabled()){
            count = "Not available";
        }else if (m_programs == 1){
            count = "1 program";
        }else{
            count = QString::number(m_programs) + " programs";
        }
        painter.setFont(font());
        painter.setPen(isEnabled() ? theme.muted : theme.disabled);
        const QRectF count_box(box.left() + 13, box.bottom() - 25, box.width() - 26, 18);
        painter.drawText(count_box, Qt::AlignLeft | Qt::AlignVCenter, count);
    }

private:
    QString m_name;
    QPixmap m_icon;
    int m_programs;
    QColor m_first;
    QColor m_second;
};


//  The tiles in rows, as many in a row as fit. It is as high as its rows.
class HomePage::TileGrid : public QWidget{
public:
    static constexpr int GAP = 14;

    TileGrid(QWidget& parent, QScrollArea& scroll)
        : QWidget(&parent)
        , m_scroll(scroll)
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        setFixedHeight(GameTile::HEIGHT);
    }
    void add(GameTile* tile){
        m_tiles.emplace_back(tile);
    }
    void arrange(){
        if (m_tiles.empty()){
            return;
        }
        //  Count the columns as if the scroll bar were there, so that the
        //  bar coming and going does not move the tiles back and forth.
        int room = width();
        const QScrollBar* bar = m_scroll.verticalScrollBar();
        if (bar != nullptr && !bar->isVisible()){
            room -= bar->sizeHint().width();
        }
        const int columns = std::max(1, (room + GAP) / (GameTile::MIN_WIDTH + GAP));
        const int tile_width = std::max(
            (int)GameTile::MIN_WIDTH,
            std::min((int)GameTile::MAX_WIDTH, (room - (columns - 1) * GAP) / columns)
        );
        const int count = (int)m_tiles.size();
        for (int index = 0; index < count; index++){
            m_tiles[(size_t)index]->setGeometry(
                (index % columns) * (tile_width + GAP),
                (index / columns) * (GameTile::HEIGHT + GAP),
                tile_width, GameTile::HEIGHT
            );
        }
        const int rows = (count + columns - 1) / columns;
        const int needed = rows * GameTile::HEIGHT + (rows - 1) * GAP;
        if (needed != height()){
            setFixedHeight(needed);
        }
    }

protected:
    virtual void resizeEvent(QResizeEvent*) override{
        arrange();
    }

private:
    QScrollArea& m_scroll;
    std::vector<GameTile*> m_tiles;
};



HomePage::HomePage(
    QWidget& parent,
    std::vector<HomeGame> games,
    std::vector<HomeProgram> programs,
    Actions actions
)
    : QWidget(&parent)
    , m_games(std::move(games))
    , m_programs(std::move(programs))
    , m_actions(std::move(actions))
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    //  See through to the window: the screen's color, or the wallpaper.
    scroll->viewport()->setAutoFillBackground(false);
    outer->addWidget(scroll);

    QWidget* page = new QWidget(scroll);
    scroll->setWidget(page);
    //  After setWidget(), which turns it on.
    page->setAutoFillBackground(false);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 12, 20, 20);
    layout->setSpacing(0);

    //  The way back from one game's programs to all games.
    m_back = new QPushButton(QString::fromUtf8("\xe2\x80\xb9  All games"), page);
    m_back->setObjectName("ppBack");
    m_back->setCursor(Qt::PointingHandCursor);
    m_back->setVisible(false);
    layout->addWidget(m_back, 0, Qt::AlignLeft);

    m_title = new QLabel(page);
    m_title->setObjectName("ppHomeTitle");
    m_title->setFont(display_font(38));
    m_title->setTextFormat(Qt::PlainText);
    layout->addWidget(m_title);

    m_lead = new QLabel(page);
    m_lead->setObjectName("ppHomeLead");
    m_lead->setWordWrap(true);
    m_lead->setTextFormat(Qt::PlainText);
    m_lead->setMaximumWidth(TEXT_WIDTH);
    layout->addWidget(m_lead);
    layout->addSpacing(16);

    m_search = new QLineEdit(page);
    m_search->setObjectName("ppHomeSearch");
    m_search->setPlaceholderText("Search the programs of all games, for example: shiny");
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(TEXT_WIDTH);
    m_search->setAccessibleName("Search the programs of all games");
    layout->addWidget(m_search);
    layout->addSpacing(12);

    //  The programs found, or the programs of one game.
    m_results = new QTreeWidget(page);
    m_results->setObjectName("ppSearchResults");
    m_results->setColumnCount(2);
    m_results->setHeaderHidden(true);
    m_results->setRootIsDecorated(false);
    m_results->setItemsExpandable(false);
    m_results->setUniformRowHeights(true);
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    m_results->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_results->setTextElideMode(Qt::ElideRight);
    m_results->header()->setStretchLastSection(false);
    m_results->setVisible(false);
    layout->addWidget(m_results);

    m_no_results = new QLabel(page);
    m_no_results->setObjectName("ppMuted");
    m_no_results->setWordWrap(true);
    m_no_results->setTextFormat(Qt::PlainText);
    m_no_results->setMaximumWidth(TEXT_WIDTH);
    m_no_results->setVisible(false);
    layout->addWidget(m_no_results);

    //  The way back to the program that was open last.
    m_resume_row = new QWidget(page);
    QHBoxLayout* resume_layout = new QHBoxLayout(m_resume_row);
    resume_layout->setContentsMargins(0, 0, 0, 16);
    resume_layout->setSpacing(12);
    m_resume_button = new QPushButton(m_resume_row);
    m_resume_button->setObjectName("ppPrimary");
    m_resume_button->setCursor(Qt::PointingHandCursor);
    resume_layout->addWidget(m_resume_button);
    m_resume_label = new QLabel(m_resume_row);
    m_resume_label->setObjectName("ppMuted");
    m_resume_label->setTextFormat(Qt::PlainText);
    resume_layout->addWidget(m_resume_label);
    resume_layout->addStretch(1);
    m_resume_row->setVisible(false);
    layout->addWidget(m_resume_row);

    m_grid = new TileGrid(*page, *scroll);
    for (size_t index = 0; index < m_games.size(); index++){
        GameTile* tile = new GameTile(*m_grid, m_games[index]);
        m_grid->add(tile);
        connect(
            tile, &QAbstractButton::clicked,
            this, [this, index](bool){
                show_game((int)index);
                if (m_actions.open_game){
                    m_actions.open_game((int)index);
                }
            }
        );
    }
    layout->addWidget(m_grid);

    //  Takes the room that is left, except when the programs of a game
    //  fill it.
    m_filler = new QWidget(page);
    m_filler->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    layout->addWidget(m_filler, 1);

    connect(
        m_back, &QPushButton::clicked,
        this, [this](bool){
            show_games();
            focus_search();
        }
    );
    connect(
        m_search, &QLineEdit::textChanged,
        this, [this](const QString&){
            refresh();
        }
    );
    //  Enter in the search box opens the first program found.
    connect(
        m_search, &QLineEdit::returnPressed,
        this, [this]{
            if (!m_results->isVisible() || m_search->text().trimmed().isEmpty()){
                return;
            }
            for (int row = 0; row < m_results->topLevelItemCount(); row++){
                QTreeWidgetItem* item = m_results->topLevelItem(row);
                if (item->data(0, Qt::UserRole).toInt() >= 0){
                    open_result(item);
                    return;
                }
            }
        }
    );
    //  A click, or Enter on a row.
    connect(
        m_results, &QTreeWidget::itemClicked,
        this, [this](QTreeWidgetItem* item, int){
            open_result(item);
        }
    );
    connect(
        m_results, &QTreeWidget::itemActivated,
        this, [this](QTreeWidgetItem* item, int){
            open_result(item);
        }
    );
    connect(
        m_resume_button, &QPushButton::clicked,
        this, [this](bool){
            if (m_actions.resume){
                m_actions.resume();
            }
        }
    );

    m_ready = true;
    refresh();
}

void HomePage::changeEvent(QEvent* event){
    QWidget::changeEvent(event);
    //  The rows of the list carry colors of the theme.
    if (m_ready && event->type() == QEvent::PaletteChange && m_results->isVisible()){
        refresh();
    }
}

void HomePage::set_resume(const QString& game, const QString& program){
    m_resume_program = program;
    m_resume_button->setText("Continue with " + program);
    m_resume_label->setText(short_game_name(game));
    refresh();
}

void HomePage::show_games(){
    m_game = -1;
    if (m_search->text().isEmpty()){
        refresh();
    }else{
        //  Clearing the box refreshes.
        m_search->clear();
    }
}
void HomePage::show_game(int game){
    if (game < 0 || game >= (int)m_games.size()){
        return;
    }
    m_game = game;
    if (m_search->text().isEmpty()){
        refresh();
    }else{
        m_search->clear();
    }
}

bool HomePage::show_game(const QString& name){
    for (size_t index = 0; index < m_games.size(); index++){
        if (m_games[index].name == name){
            show_game((int)index);
            return true;
        }
    }
    return false;
}

void HomePage::set_search_text(const QString& text){
    m_search->setText(text);
}
void HomePage::focus_search(){
    m_search->setFocus(Qt::OtherFocusReason);
}

void HomePage::open_result(QTreeWidgetItem* item){
    if (item == nullptr || !m_actions.open_program){
        return;
    }
    const int index = item->data(0, Qt::UserRole).toInt();
    if (index < 0 || index >= (int)m_programs.size()){
        return;
    }
    //  A copy: opening a program may close this page.
    const HomeProgram program = m_programs[(size_t)index];
    m_actions.open_program(program.game, program.name);
}

void HomePage::add_program_row(int index, const QString& second_column){
    const HomeProgram& program = m_programs[(size_t)index];
    QTreeWidgetItem* item = new QTreeWidgetItem(m_results);
    item->setText(0, program.name);
    item->setText(1, second_column);
    item->setForeground(1, theme_colors().muted);
    item->setToolTip(0, program.description);
    item->setToolTip(1, program.description);
    item->setData(0, Qt::UserRole, index);
}

//  Shows what the page is in: all games, the programs of one game, or the
//  programs found by the search box (which goes before the other two).
void HomePage::refresh(){
    const QString wanted = m_search->text().trimmed();
    const bool searching = !wanted.isEmpty();
    const bool one_game = !searching && m_game >= 0;

    m_back->setVisible(one_game);
    m_grid->setVisible(!searching && !one_game);
    m_resume_row->setVisible(!searching && !one_game && !m_resume_program.isEmpty());
    m_no_results->setVisible(false);
    m_results->clear();

    if (one_game){
        const HomeGame& game = m_games[(size_t)m_game];
        m_title->setText(short_game_name(game.name));
        m_lead->setText(
            (game.programs == 1 ? QString("1 program") : QString::number(game.programs) + " programs") +
            ". Pick one here or in the list on the left."
        );
    }else{
        m_title->setText("Pick a game");
        m_lead->setText(
            "Each game has its own programs. A program watches the game through your capture card "
            "and presses the buttons for you."
        );
    }

    if (!searching && !one_game){
        m_results->setVisible(false);
        m_filler->setVisible(true);
        return;
    }

    int shown = 0;
    if (one_game){
        //  The game's programs under the titles of their sections, each
        //  with what it does.
        QString section;
        bool first = true;
        for (size_t index = 0; index < m_programs.size(); index++){
            const HomeProgram& program = m_programs[index];
            if (program.game != m_game){
                continue;
            }
            if (first || program.section != section){
                first = false;
                section = program.section;
                const QString title = section_title(section);
                if (!title.isEmpty()){
                    QTreeWidgetItem* header = new QTreeWidgetItem(m_results);
                    header->setText(0, title);
                    header->setFirstColumnSpanned(true);
                    //  Not a row to pick.
                    header->setFlags(Qt::NoItemFlags);
                    header->setForeground(0, theme_colors().muted);
                    QFont font = header->font(0);
                    font.setBold(true);
                    header->setFont(0, font);
                    header->setData(0, Qt::UserRole, -1);
                }
            }
            add_program_row((int)index, program.description);
            shown++;
        }
        m_results->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_results->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    }else{
        //  A name that starts with the words comes first, then a name with
        //  the words in it, then a description with them.
        const int MAX_RESULTS = 60;
        std::vector<std::pair<int, int>> found;
        for (size_t index = 0; index < m_programs.size(); index++){
            const HomeProgram& program = m_programs[index];
            if (program.game < 0 || program.game >= (int)m_games.size()){
                continue;
            }
            int rank;
            if (program.name.startsWith(wanted, Qt::CaseInsensitive)){
                rank = 0;
            }else if (program.name.contains(wanted, Qt::CaseInsensitive)){
                rank = 1;
            }else if (program.description.contains(wanted, Qt::CaseInsensitive)){
                rank = 2;
            }else{
                continue;
            }
            found.emplace_back(rank, (int)index);
        }
        std::stable_sort(
            found.begin(), found.end(),
            [](const std::pair<int, int>& a, const std::pair<int, int>& b){
                return a.first < b.first;
            }
        );
        for (const std::pair<int, int>& entry : found){
            if (shown == MAX_RESULTS){
                break;
            }
            const HomeProgram& program = m_programs[(size_t)entry.second];
            add_program_row(entry.second, short_game_name(m_games[(size_t)program.game].name));
            m_results->topLevelItem(m_results->topLevelItemCount() - 1)->setTextAlignment(
                1, Qt::AlignRight | Qt::AlignVCenter
            );
            shown++;
        }
        m_results->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_results->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    }

    if (shown == 0){
        m_results->setVisible(false);
        m_filler->setVisible(true);
        m_no_results->setText(
            searching
                ? "No program has \"" + wanted + "\" in its name or description. Try a shorter word."
                : QString("This game has no programs here.")
        );
        m_no_results->setVisible(true);
        return;
    }

    if (one_game){
        //  All the room that is left; the list scrolls by itself.
        m_results->setMaximumWidth(LIST_WIDTH);
        m_results->setMinimumHeight(160);
        m_results->setMaximumHeight(QWIDGETSIZE_MAX);
        m_results->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        m_filler->setVisible(false);
    }else{
        //  As high as its rows, up to a dozen; more of them scroll.
        m_results->setMaximumWidth(TEXT_WIDTH);
        const int row = m_results->sizeHintForRow(0) > 0 ? m_results->sizeHintForRow(0) : 30;
        m_results->setFixedHeight(std::min(shown, 12) * row + 12);
        m_filler->setVisible(true);
    }
    m_results->setVisible(true);
}



}
}

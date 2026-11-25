/*  File and Window Logger
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include "Common/Cpp/ColoredText.h"
#include "Common/Cpp/Logging/LastLogTracker.h"
#include "Common/Cpp/Logging/GlobalLogger.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/GlobalSettingsPanel.h"
#include "CommonFramework/Windows/DpiScaler.h"
#include "CommonFramework/Windows/WindowTracker.h"
#include "CommonFramework/Windows/MainWindow.h"
#include "CommonFramework/Options/ResolutionOption.h"
#include "PocketPilot/PocketPilot_Language.h"
#include "LoggerWindow.h"

namespace PokemonAutomation{


void LoggerWindow::log(const std::string& msg, Color color){
    Line new_lines;
    new_lines.html = to_window_str(msg, color);
    //  What the program writes in red or in orange is an error or a warning.
    new_lines.problem = (uint32_t)color == (uint32_t)COLOR_RED || (uint32_t)color == (uint32_t)COLOR_ORANGE;

    bool pending_update = false;
    {
        WriteSpinLock lg(m_lock, nullptr);
        pending_update = m_pending_update;
        m_pending_update = true;

        Line* qstr = m_pending.try_push_back();
        if (qstr == nullptr){
            m_pending.pop_front();
            qstr = &m_pending.push_back();
        }
        *qstr = std::move(new_lines);

        WallClock now = current_time();
        WallClock threshold = now - WINDOW_SIZE;
        while (!m_timestamps.empty() && m_timestamps.front() < threshold){
            m_timestamps.pop_front();
        }

        m_timestamps.try_push_back(now);

//        printf("Pending = %d, Later = %d\n", pending_update, update_later);
    }

    if (pending_update){
        return;
    }

    QMetaObject::invokeMethod(this, [this]{
        update();
    }, Qt::QueuedConnection);
}

QString LoggerWindow::to_window_str(const std::string& msg, Color color){
    // Convert message to HTML for display in QTextEdit.
    // Replace spaces with &nbsp; and newlines with <br>.
    std::string str;
    if (color){
        //  In a color that can be read on the theme's background.
        str += "<font color=\"" + QColor((uint32_t)theme_readable(color)).name().toStdString() + "\">";
    }else{
        str += "<font>";
    }
    for (char ch : msg){
        if (ch == ' '){
            str += "&nbsp;";
            continue;
        }
        if (ch == '\n'){
            str += "<br>";
            continue;
        }
        str += ch;
    }
    str += "</font>";

    return QString::fromStdString(str);
}


LoggerWindow::LoggerWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_timestamps(MAX_LOGS_PER_WINDOW)
    , m_pending(MAX_LINES)
{
    if (objectName().isEmpty()){
        setObjectName(QString::fromUtf8("TextWindow"));
    }
    uint32_t width = GlobalSettings::instance().LOG_WINDOW_SIZE->WIDTH;
    uint32_t height = GlobalSettings::instance().LOG_WINDOW_SIZE->HEIGHT;
    resize(scale_dpi_width(width), scale_dpi_height(height));

    //  A bar with the filter and the search over the text.
    QWidget* panel = new QWidget(this);
    QVBoxLayout* column = new QVBoxLayout(panel);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    QHBoxLayout* bar = new QHBoxLayout();
    bar->setContentsMargins(6, 4, 6, 4);
    bar->setSpacing(6);
    column->addLayout(bar);
    m_search = new QLineEdit(panel);
    m_search->setObjectName("ppLogSearch");
    m_search->setPlaceholderText("Find in the output");
    m_search->setClearButtonEnabled(true);
    bar->addWidget(m_search, 1);
    m_previous = new QPushButton("Previous", panel);
    m_previous->setObjectName("ppLogPrevious");
    bar->addWidget(m_previous);
    m_next = new QPushButton("Next", panel);
    m_next->setObjectName("ppLogNext");
    bar->addWidget(m_next);
    m_matches = new QLabel(panel);
    m_matches->setObjectName("ppLogMatches");
    bar->addWidget(m_matches);
    m_problems_only = new QCheckBox("Only warnings and errors", panel);
    m_problems_only->setObjectName("ppLogProblems");
    m_problems_only->setToolTip("Shows only the lines in red and in orange: the errors and the warnings.");
    bar->addWidget(m_problems_only);
    m_text = new QTextEdit(panel);
    m_text->setObjectName(QString::fromUtf8("centralwidget"));
    column->addWidget(m_text, 1);
    setCentralWidget(panel);
    m_menubar = new QMenuBar(this);
    m_menubar->setObjectName(QString::fromUtf8("menubar"));
    setMenuBar(m_menubar);
//    m_statusbar = new QStatusBar(this);
//    m_statusbar->setObjectName(QString::fromUtf8("statusbar"));
//    setStatusBar(m_statusbar);
    setWindowTitle("Program Output");
    this->setWindowIcon(QIcon(QString::fromStdString(RESOURCE_PATH() + "icon.png")));

    m_text->setReadOnly(true);
    m_text->setAcceptRichText(true);
    m_text->document()->setMaximumBlockCount(MAX_LINES);

    connect(
        this, &LoggerWindow::signal_log,
        m_text, [this](){
            update();
        }
    );

    connect(m_problems_only, &QCheckBox::toggled, this, [this]{
        refill();
    });
    connect(m_search, &QLineEdit::textChanged, this, [this]{
        update_search();
    });
    connect(m_search, &QLineEdit::returnPressed, this, [this]{
        find_step(false);
    });
    connect(m_next, &QPushButton::clicked, this, [this]{
        find_step(false);
    });
    connect(m_previous, &QPushButton::clicked, this, [this]{
        find_step(true);
    });
    new QShortcut(QKeySequence::Find, this, [this]{
        m_search->setFocus();
        m_search->selectAll();
    });

    GlobalSettings::instance().LOG_WINDOW_SIZE->WIDTH.add_listener(*this);
    GlobalSettings::instance().LOG_WINDOW_SIZE->HEIGHT.add_listener(*this);
    GlobalSettings::instance().LOG_WINDOW_SIZE->X_POS.add_listener(*this);
    GlobalSettings::instance().LOG_WINDOW_SIZE->Y_POS.add_listener(*this);

    for (const LogLine& item : global_last_log_history().get_recent(MAX_LINES)){
        log(item.text, item.color);
    }

    log("================================================================================");
    log("<b>Window Startup...</b>");
    add_window(*this);
}

LoggerWindow::~LoggerWindow(){
    remove_window(*this);
    GlobalSettings::instance().LOG_WINDOW_SIZE->WIDTH.remove_listener(*this);
    GlobalSettings::instance().LOG_WINDOW_SIZE->HEIGHT.remove_listener(*this);
    GlobalSettings::instance().LOG_WINDOW_SIZE->X_POS.remove_listener(*this);
    GlobalSettings::instance().LOG_WINDOW_SIZE->Y_POS.remove_listener(*this);
}

bool LoggerWindow::shown(const Line& line) const{
    return !m_problems_only->isChecked() || line.problem;
}

//  The text again, from the lines that are kept: after the filter changed.
void LoggerWindow::refill(){
    m_text->setUpdatesEnabled(false);
    m_text->clear();
    for (const Line& line : m_lines){
        if (shown(line)){
            m_text->append(line.html);
        }
    }
    m_text->setUpdatesEnabled(true);
    QScrollBar* bar = m_text->verticalScrollBar();
    bar->setValue(bar->maximum());
    update_search();
}

//  Marks every place of the text in the search box, and counts them.
void LoggerWindow::update_search(){
    const QString text = m_search->text();
    QList<QTextEdit::ExtraSelection> selections;
    int count = 0;
    if (!text.isEmpty()){
        QTextDocument* document = m_text->document();
        QTextCursor cursor = document->find(text);
        while (!cursor.isNull() && count < 5000){
            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            selection.format.setBackground(QColor(255, 214, 0));
            selection.format.setForeground(Qt::black);
            selections.append(selection);
            count++;
            cursor = document->find(text, cursor);
        }
    }
    m_text->setExtraSelections(selections);
    if (text.isEmpty()){
        m_matches->clear();
    }else if (count == 0){
        m_matches->setText(PocketPilot::tr_ui("No matches"));
    }else{
        m_matches->setText(PocketPilot::tr_ui("Matches: %1").arg(count));
    }
}

//  To the next (or the previous) place of the text in the search box, and round
//  from the end to the start.
void LoggerWindow::find_step(bool backward){
    const QString text = m_search->text();
    if (text.isEmpty()){
        return;
    }
    QTextDocument::FindFlags flags;
    if (backward){
        flags |= QTextDocument::FindBackward;
    }
    if (!m_text->find(text, flags)){
        QTextCursor cursor = m_text->textCursor();
        cursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        m_text->setTextCursor(cursor);
        m_text->find(text, flags);
    }
}

void LoggerWindow::update(){
    bool appended = false;
    bool drained = false;
    for (size_t c = 0; c < MAX_LOGS_PER_WINDOW; c++){
        Line line;
        bool full;
        {
            WriteSpinLock lg(m_lock, nullptr);
            if (m_pending.empty()){
                m_pending_update = false;
                drained = true;
                break;
            }
            full = m_pending.full();
            line = std::move(m_pending.front());
            m_pending.pop_front();
        }
        if (full){
            m_text->clear();
            m_lines.clear();
        }
        const bool show = shown(line);
        if (show){
            m_text->append(line.html);
            appended = true;
        }
        m_lines.push_back(std::move(line));
        while (m_lines.size() > MAX_LINES){
            m_lines.pop_front();
        }
    }
    if (appended && !m_search->text().isEmpty()){
        update_search();
    }
    if (drained){
        return;
    }
    QTimer::singleShot(WINDOW_SIZE.count(), this, [this](){
        emit signal_log();
    });
}


void LoggerWindow::resizeEvent(QResizeEvent* event){
    m_pending_resize = true;
    GlobalSettings::instance().LOG_WINDOW_SIZE->WIDTH.set(width());
    GlobalSettings::instance().LOG_WINDOW_SIZE->HEIGHT.set(height());
    m_pending_resize = false;
}

void LoggerWindow::moveEvent(QMoveEvent* event){
    m_pending_move = true;
    GlobalSettings::instance().LOG_WINDOW_SIZE->X_POS.set(x());
    GlobalSettings::instance().LOG_WINDOW_SIZE->Y_POS.set(y());
    m_pending_move = false;
}

void LoggerWindow::on_config_value_changed(void* object){
    if (object == &GlobalSettings::instance().LOG_WINDOW_SIZE->WIDTH || object == &GlobalSettings::instance().LOG_WINDOW_SIZE->HEIGHT){
        QMetaObject::invokeMethod(this, [this]{
            if (!m_pending_resize){
                resize(
                    GlobalSettings::instance().LOG_WINDOW_SIZE->WIDTH,
                    GlobalSettings::instance().LOG_WINDOW_SIZE->HEIGHT
                );
            }
        });
    }else if (object == &GlobalSettings::instance().LOG_WINDOW_SIZE->X_POS || object == &GlobalSettings::instance().LOG_WINDOW_SIZE->Y_POS){
        QMetaObject::invokeMethod(this, [this]{
            if (!m_pending_move){
                move(
                    move_x_within_screen_bounds(GlobalSettings::instance().LOG_WINDOW_SIZE->X_POS),
                    move_y_within_screen_bounds(GlobalSettings::instance().LOG_WINDOW_SIZE->Y_POS)
                );
            }
        });
    }
}


}

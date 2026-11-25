/*  PocketPilot Assistant: Panel
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include <algorithm>
#include <cmath>
#include <utility>
#include <QAbstractTextDocumentLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include "PocketPilot/PocketPilot_Theme.h"
#include "Assistant_Panel.h"

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


namespace{

const char* const KEY_HELP_URL = "https://platform.claude.com/docs/en/get-api-key";
const char* const WORKSPACES_URL = "https://platform.claude.com/settings/workspaces";
const char* const TOKEN_HELP_URL = "https://github.com/settings/personal-access-tokens/new";

//  The marks in front of a tool's line: at work, done, failed.
const QString MARK_BUSY = QString::fromUtf8("\xe2\x80\xa2");        //  a bullet
const QString MARK_DONE = QString::fromUtf8("\xe2\x9c\x93");        //  a check mark
const QString MARK_FAILED = QString::fromUtf8("\xc3\x97");          //  a multiplication sign


//  The Markdown a text view shows, kept as a property of the view.
const char* const MARKDOWN_PROPERTY = "ppMarkdown";

//  A text view as high as its text: it never scrolls itself, the
//  conversation around it does.
class TextView : public QTextBrowser{
public:
    explicit TextView(QWidget* parent)
        : QTextBrowser(parent)
    {
        setObjectName("ppAssistantText");
        setFrameShape(QFrame::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        setOpenExternalLinks(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        viewport()->setAutoFillBackground(false);
        document()->setDocumentMargin(0);
        //  A list's items a little in from the text, not far.
        document()->setIndentWidth(16);
        //  Text to read: a size larger than the labels around it.
        QFont larger = font();
        if (larger.pointSizeF() > 0){
            larger.setPointSizeF(larger.pointSizeF() + 1.0);
            setFont(larger);
        }
        setFixedHeight(fontMetrics().lineSpacing());
    }

    //  As high as the text is at the width the view has.
    void fit(){
        const int width = viewport()->width();
        if (width <= 0){
            return;
        }
        document()->setTextWidth(width);
        const int height = std::max(
            (int)std::ceil(document()->size().height()) + 1, fontMetrics().lineSpacing()
        );
        if (height != m_height){
            m_height = height;
            setFixedHeight(height);
        }
    }

    //  How wide the text would be on lines of its own, without wrapping.
    int ideal_width(){
        document()->setTextWidth(-1);
        const int width = (int)std::ceil(document()->idealWidth()) + 2;
        fit();
        return width;
    }

protected:
    virtual void resizeEvent(QResizeEvent* event) override{
        QTextBrowser::resizeEvent(event);
        fit();
    }
    //  The wheel scrolls the conversation, not this view.
    virtual void wheelEvent(QWheelEvent* event) override{
        event->ignore();
    }

private:
    int m_height = -1;
};


//  What the user wrote: a bubble at the right, as wide as its text and no
//  wider than the row allows.
class BubbleRow : public QWidget{
public:
    BubbleRow(QWidget* parent, const QString& text)
        : QWidget(parent)
    {
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(LEFT_ROOM, 0, 0, 0);
        layout->setSpacing(0);
        layout->addStretch(1);
        m_bubble = new QFrame(this);
        m_bubble->setObjectName("ppUserBubble");
        QVBoxLayout* inner = new QVBoxLayout(m_bubble);
        inner->setContentsMargins(PADDING, 8, PADDING, 8);
        m_view = new TextView(m_bubble);
        m_view->setPlainText(text);
        inner->addWidget(m_view);
        layout->addWidget(m_bubble);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    virtual void resizeEvent(QResizeEvent* event) override{
        QWidget::resizeEvent(event);
        const int room = width() - LEFT_ROOM;
        if (room <= 2 * PADDING){
            return;
        }
        const int wanted = m_view->ideal_width() + 2 * PADDING;
        const int bubble = std::min(wanted, room);
        if (bubble != m_bubble->width()){
            m_bubble->setFixedWidth(bubble);
        }
    }

private:
    static constexpr int LEFT_ROOM = 44;
    static constexpr int PADDING = 12;
    QFrame* m_bubble;
    TextView* m_view;
};


//  The lines of a change in colors: taken out, put in, where.
class DiffColors : public QSyntaxHighlighter{
public:
    explicit DiffColors(QTextDocument* document)
        : QSyntaxHighlighter(document)
    {}

protected:
    virtual void highlightBlock(const QString& text) override{
        const ThemeColors& theme = theme_colors();
        QTextCharFormat format;
        if (text.startsWith("=== ")){
            format.setForeground(theme.text);
            format.setFontWeight(QFont::Bold);
        }else if (text.startsWith("@@")){
            format.setForeground(theme.muted);
        }else if (text.startsWith('+')){
            format.setForeground(theme.ok);
        }else if (text.startsWith('-')){
            format.setForeground(theme.danger);
        }else{
            return;
        }
        setFormat(0, (int)text.size(), format);
    }
};


QLabel* make_label(QWidget* parent, const char* object_name, const QString& text){
    QLabel* label = new QLabel(text, parent);
    label->setObjectName(object_name);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    return label;
}

QPushButton* make_button(QWidget* parent, const char* object_name, const QString& text){
    QPushButton* button = new QPushButton(text, parent);
    button->setObjectName(object_name);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

//  A link under a text, opened in the browser.
QLabel* make_link(QWidget* parent, const QString& text, const char* url){
    QLabel* label = new QLabel(parent);
    label->setObjectName("ppAssistantLink");
    label->setTextFormat(Qt::RichText);
    label->setText("<a href=\"" + QString::fromLatin1(url) + "\">" + text.toHtmlEscaped() + "</a>");
    label->setOpenExternalLinks(true);
    label->setToolTip(QString::fromLatin1(url));
    return label;
}

QString first_line(const QString& text, int length){
    QString line = text.section('\n', 0, 0).simplified();
    if (line.size() > length){
        line = line.left(length - 3) + "...";
    }
    return line;
}

}



void show_markdown(QTextBrowser& view, const QString& markdown){
    const ThemeColors& theme = theme_colors();
    //  Kept with the view: the backgrounds below are the theme's of this
    //  moment, and are made again when the theme changes.
    view.setProperty(MARKDOWN_PROPERTY, markdown);
    QTextDocument* document = view.document();
    document->setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);

    const QFont base = view.font();
    const qreal base_size = base.pointSizeF() > 0 ? base.pointSizeF() : 9.0;
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()){
        QTextBlockFormat format = block.blockFormat();
        const int heading = format.headingLevel();
        const bool code =
            format.hasProperty(QTextFormat::BlockCodeFence) || format.hasProperty(QTextFormat::BlockCodeLanguage);
        cursor.setPosition(block.position());
        if (heading > 0){
            //  A heading of an answer is a line in bold, not a title.
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            QTextCharFormat characters;
            characters.setProperty(QTextFormat::FontSizeAdjustment, 0);
            characters.setFontPointSize(base_size * (heading <= 2 ? 1.12 : 1.0));
            characters.setFontWeight(QFont::DemiBold);
            cursor.mergeCharFormat(characters);
            format.setTopMargin(heading <= 2 ? 12 : 8);
            format.setBottomMargin(4);
            cursor.setBlockFormat(format);
        }else if (code){
            //  On its own background, and wrapped where a line is longer
            //  than the panel is wide: nothing of it is out of sight.
            format.setBackground(theme.dark ? theme.field : theme.surface_alt);
            format.setLeftMargin(8);
            format.setRightMargin(8);
            format.setNonBreakableLines(false);
            cursor.setBlockFormat(format);
        }else{
            //  Room between paragraphs, less between the items of a list.
            if (block.textList() == nullptr){
                format.setBottomMargin(7);
            }else{
                format.setTopMargin(2);
                format.setBottomMargin(2);
            }
            cursor.setBlockFormat(format);
            //  Code inside a line.
            for (QTextBlock::iterator part = block.begin(); !part.atEnd(); ++part){
                const QTextFragment fragment = part.fragment();
                if (!fragment.isValid() || !fragment.charFormat().fontFixedPitch()){
                    continue;
                }
                QTextCursor range(document);
                range.setPosition(fragment.position());
                range.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                QTextCharFormat characters;
                characters.setBackground(theme.surface_alt);
                range.mergeCharFormat(characters);
            }
        }
    }
    cursor.endEditBlock();
}


void show_changes_window(QWidget* parent, const QString& diff){
    QDialog* window = new QDialog(parent);
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    window->setWindowTitle("Changes of this conversation");
    window->resize(900, 640);
    QVBoxLayout* layout = new QVBoxLayout(window);
    QLabel* lead = new QLabel(
        "The lines taken out (-) and put in (+), with a few lines around them. Nothing of this is on GitHub "
        "until you approve the commit.", window
    );
    lead->setObjectName("ppMuted");
    lead->setWordWrap(true);
    layout->addWidget(lead);
    QPlainTextEdit* text = new QPlainTextEdit(window);
    text->setObjectName("ppAssistantDiff");
    text->setReadOnly(true);
    text->setLineWrapMode(QPlainTextEdit::NoWrap);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    new DiffColors(text->document());
    text->setPlainText(diff.isEmpty() ? QString("No changes.") : diff);
    layout->addWidget(text, 1);
    QHBoxLayout* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    QPushButton* close = make_button(window, "ppSecondary", "Close");
    QObject::connect(close, &QPushButton::clicked, window, &QDialog::close);
    buttons->addWidget(close);
    layout->addLayout(buttons);
    window->show();
    window->raise();
    window->activateWindow();
}



//  A box in the conversation with a title, a text and buttons: a question
//  to the user, a commit, something that went wrong.
class Panel::Card : public QFrame{
public:
    Card(QWidget* parent, const QString& kind, const QString& title, const QString& text)
        : QFrame(parent)
    {
        setObjectName("ppCard");
        setProperty("kind", kind);
        //  As high as its text needs at the width it gets.
        QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        policy.setHeightForWidth(true);
        setSizePolicy(policy);
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(6);
        if (!title.isEmpty()){
            QLabel* title_label = make_label(this, "ppCardTitle", title);
            QFont bold = title_label->font();
            bold.setBold(true);
            title_label->setFont(bold);
            layout->addWidget(title_label);
        }
        if (!text.isEmpty()){
            QLabel* text_label = make_label(this, "ppCardText", text);
            text_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            layout->addWidget(text_label);
        }
        m_buttons = new QWidget(this);
        m_button_row = new QHBoxLayout(m_buttons);
        m_button_row->setContentsMargins(0, 2, 0, 0);
        m_button_row->setSpacing(8);
        m_button_row->addStretch(1);
        m_buttons->setVisible(false);
        layout->addWidget(m_buttons);
        m_outcome = make_label(this, "ppCardText", QString());
        m_outcome->setVisible(false);
        layout->addWidget(m_outcome);
    }

    QPushButton* add_button(const QString& label, bool primary, std::function<void()> action){
        QPushButton* button = make_button(m_buttons, primary ? "ppPrimary" : "ppSecondary", label);
        m_button_row->insertWidget(m_button_row->count() - 1, button);
        m_buttons->setVisible(true);
        QObject::connect(button, &QPushButton::clicked, this, [action](bool){
            if (action){
                action();
            }
        });
        return button;
    }

    //  The question is answered: no buttons any more, and what came of it.
    void resolve(const QString& outcome){
        m_resolved = true;
        m_buttons->setVisible(false);
        m_outcome->setText(outcome);
        m_outcome->setVisible(!outcome.isEmpty());
    }
    bool resolved() const{ return m_resolved; }

    //  Called when the card is closed without the user's answer.
    std::function<void()> on_dismiss;

private:
    QWidget* m_buttons;
    QHBoxLayout* m_button_row;
    QLabel* m_outcome;
    bool m_resolved = false;
};



Panel::Panel(QWidget& parent, Service& service)
    : QWidget(&parent)
    , m_service(service)
    , m_alive(std::make_shared<bool>(true))
{
    setObjectName("ppAssistant");
    setAttribute(Qt::WA_StyledBackground, true);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(make_header());

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(make_chat_page());
    m_pages->addWidget(make_connect_page());
    m_pages->addWidget(make_settings_page());
    layout->addWidget(m_pages, 1);

    m_render_timer = new QTimer(this);
    m_render_timer->setSingleShot(true);
    m_render_timer->setInterval(60);
    connect(m_render_timer, &QTimer::timeout, this, [this]{
        render_answer();
    });

    connect_agent();
    refresh_models();
    refresh_meter();
    refresh_send_button();
    refresh_empty_state();
    show_start_page();
}

Panel::~Panel(){
    *m_alive = false;
    //  The service outlives the panel: it must not call into it any more.
    m_service.agent().set_events(Agent::Events());
    m_service.on_commit = nullptr;
    m_service.on_show_changes = nullptr;
}

QWidget* Panel::make_header(){
    QWidget* header = new QWidget(this);
    header->setObjectName("ppAssistantHeader");
    QHBoxLayout* layout = new QHBoxLayout(header);
    layout->setContentsMargins(14, 8, 8, 6);
    layout->setSpacing(2);

    QLabel* title = new QLabel("Claude", header);
    title->setObjectName("ppAssistantTitle");
    title->setFont(display_font(26));
    layout->addWidget(title);
    layout->addStretch(1);

    m_new_chat = make_button(header, "ppGhost", "New chat");
    m_new_chat->setToolTip("Start a new conversation. A new one costs fewer tokens than a long one.");
    layout->addWidget(m_new_chat);
    connect(m_new_chat, &QPushButton::clicked, this, [this](bool){
        start_new_chat();
    });

    m_settings_button = make_button(header, "ppGhost", "Settings");
    m_settings_button->setToolTip("The API key, the model, and the repository Claude works on");
    layout->addWidget(m_settings_button);
    connect(m_settings_button, &QPushButton::clicked, this, [this](bool){
        if (page() == Page::SETTINGS){
            show_start_page();
        }else{
            show_page(Page::SETTINGS);
        }
    });

    QPushButton* close = make_button(header, "ppGhost", QString::fromUtf8("\xc3\x97"));
    close->setToolTip("Close this panel. The conversation stays.");
    close->setAccessibleName("Close the panel");
    close->setFixedWidth(30);
    layout->addWidget(close);
    connect(close, &QPushButton::clicked, this, [this](bool){
        if (on_close){
            on_close();
        }
    });
    return header;
}

QWidget* Panel::make_chat_page(){
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_scroll = new QScrollArea(page);
    m_scroll->setObjectName("ppAssistantScroll");
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->viewport()->setAutoFillBackground(false);
    m_rows_widget = new QWidget(m_scroll);
    m_rows_widget->setObjectName("ppAssistantRows");
    m_scroll->setWidget(m_rows_widget);
    m_rows_widget->setAutoFillBackground(false);
    m_rows = new QVBoxLayout(m_rows_widget);
    m_rows->setContentsMargins(14, 6, 14, 10);
    m_rows->setSpacing(8);
    m_rows->addStretch(1);
    layout->addWidget(m_scroll, 1);
    //  The conversation stays at its end while it grows (and when the
    //  window changes size), unless the user scrolled up to read.
    {
        QScrollBar* bar = m_scroll->verticalScrollBar();
        connect(bar, &QScrollBar::rangeChanged, this, [this, bar](int, int maximum){
            if (m_at_end){
                bar->setValue(maximum);
            }
        });
        connect(bar, &QScrollBar::valueChanged, this, [this, bar](int value){
            m_at_end = value >= bar->maximum() - 24;
        });
    }

    //  Before the first message: what to ask.
    {
        m_empty = new QWidget(m_rows_widget);
        QVBoxLayout* empty = new QVBoxLayout(m_empty);
        empty->setContentsMargins(0, 18, 0, 0);
        empty->setSpacing(8);
        QLabel* heading = new QLabel("Ask about a program", m_empty);
        heading->setObjectName("ppAssistantTitle");
        heading->setFont(display_font(30));
        heading->setWordWrap(true);
        empty->addWidget(heading);
        empty->addWidget(make_label(
            m_empty, "ppMuted",
            "What it does, why a run went wrong, or something to build or fix. Claude looks things up in "
            "the program, its log and its source."
        ));
        empty->addSpacing(6);
        struct Suggestion{
            const char* text;
            bool send;
        };
        const Suggestion SUGGESTIONS[] = {
            {"What does the open program do, and what do its options mean?", true},
            {"Why did the last run stop? Look at the log.", true},
            {"I want to change a program: ", false},
        };
        for (const Suggestion& suggestion : SUGGESTIONS){
            const QString text = QString::fromLatin1(suggestion.text);
            QPushButton* button = make_button(
                m_empty, "ppSuggestion", suggestion.send ? text : text.trimmed().chopped(1) + QString::fromUtf8("\xe2\x80\xa6")
            );
            const bool send_now = suggestion.send;
            connect(button, &QPushButton::clicked, this, [this, text, send_now](bool){
                if (send_now){
                    send(text);
                }else{
                    set_input(text);
                    focus_input();
                }
            });
            empty->addWidget(button);
        }
        empty->addSpacing(6);
        QLabel* where_it_goes = make_label(
            m_empty, "ppAssistantSmall",
            "Runs on your own API key. What you write, and what Claude looks up with its tools, goes to "
            "Anthropic."
        );
        empty->addWidget(where_it_goes);
        m_rows->insertWidget(0, m_empty);
    }

    //  What is being waited for.
    m_status = make_label(page, "ppAssistantStatus", QString());
    m_status->setContentsMargins(16, 0, 14, 4);
    m_status->setVisible(false);
    layout->addWidget(m_status);

    //  The box to write in.
    QWidget* composer = new QWidget(page);
    QVBoxLayout* composer_layout = new QVBoxLayout(composer);
    composer_layout->setContentsMargins(12, 4, 12, 10);
    composer_layout->setSpacing(6);
    m_input = new QPlainTextEdit(composer);
    m_input->setObjectName("ppAssistantInput");
    m_input->setPlaceholderText("Ask a question, or say what to build or fix");
    m_input->setAccessibleName("Message to Claude");
    m_input->setTabChangesFocus(true);
    m_input->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_input->installEventFilter(this);
    composer_layout->addWidget(m_input);
    const auto fit_input = [this]{
        //  One line high, up to six: more than that scrolls.
        const int lines = std::clamp((int)m_input->document()->size().height(), 1, 6);
        m_input->setFixedHeight(lines * m_input->fontMetrics().lineSpacing() + 26);
    };
    connect(m_input, &QPlainTextEdit::textChanged, this, [this, fit_input]{
        fit_input();
        refresh_send_button();
    });
    fit_input();

    QHBoxLayout* bottom = new QHBoxLayout();
    bottom->setContentsMargins(0, 0, 0, 0);
    bottom->setSpacing(8);
    m_model = new QComboBox(composer);
    m_model->setObjectName("ppAssistantModel");
    m_model->setAccessibleName("Model");
    m_model->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    bottom->addWidget(m_model);
    connect(m_model, &QComboBox::activated, this, [this](int index){
        const QString model = m_model->itemData(index).toString();
        if (model.isEmpty() || model == m_service.settings().chat_model){
            return;
        }
        m_service.settings().chat_model = model;
        m_service.save_settings();
        refresh_models();
    });
    m_meter = new QLabel(composer);
    m_meter->setObjectName("ppAssistantSmall");
    m_meter->setTextFormat(Qt::PlainText);
    m_meter->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bottom->addWidget(m_meter, 1);
    m_send = make_button(composer, "ppPrimary", "Send");
    m_send->setMinimumWidth(72);
    bottom->addWidget(m_send);
    connect(m_send, &QPushButton::clicked, this, [this](bool){
        if (m_service.agent().busy()){
            m_service.agent().stop();
        }else{
            send_from_input();
        }
    });
    composer_layout->addLayout(bottom);
    layout->addWidget(composer);
    return page;
}

QWidget* Panel::make_connect_page(){
    QWidget* page = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 22, 16, 16);
    layout->setSpacing(10);

    QLabel* heading = new QLabel("Connect Claude", page);
    heading->setObjectName("ppAssistantTitle");
    heading->setFont(display_font(30));
    layout->addWidget(heading);
    layout->addWidget(make_label(
        page, "ppCardText",
        "Ask about the programs here, have one built or fixed, or let Claude play. It runs on your own "
        "Claude API key, billed to you by Anthropic for what it uses."
    ));
    layout->addWidget(make_label(
        page, "ppCardText",
        "What you write, and what Claude looks up for its answers (lines of the log, source files, pictures "
        "of the game while it plays), goes to Anthropic. Nothing is sent before you add a key."
    ));
    layout->addSpacing(6);

    QLabel* field_label = make_label(page, "ppCardTitle", "API key");
    layout->addWidget(field_label);
    m_key_field = new QLineEdit(page);
    m_key_field->setObjectName("ppAssistantField");
    m_key_field->setEchoMode(QLineEdit::Password);
    m_key_field->setPlaceholderText("sk-ant-...");
    m_key_field->setAccessibleName("Claude API key");
    layout->addWidget(m_key_field);
    m_key_error = make_label(page, "ppAssistantProblem", QString());
    m_key_error->setVisible(false);
    layout->addWidget(m_key_error);
    layout->addWidget(make_remember_box(page));
    QHBoxLayout* buttons = new QHBoxLayout();
    QPushButton* save = make_button(page, "ppPrimary", "Save the key");
    buttons->addWidget(save);
    buttons->addStretch(1);
    layout->addLayout(buttons);
    connect(save, &QPushButton::clicked, this, [this](bool){
        save_key();
    });
    connect(m_key_field, &QLineEdit::returnPressed, this, [this]{
        save_key();
    });
    layout->addWidget(make_link(page, "How to get an API key", KEY_HELP_URL));
    layout->addWidget(make_label(
        page, "ppAssistantSmall",
        "In the Claude Console, make the key for one workspace: it then works here as it is. (A key that was "
        "not made for one workspace needs the workspace's ID, under Settings in this panel.)"
    ));
    layout->addWidget(make_protection_note(page));
    layout->addStretch(1);
    return page;
}

QWidget* Panel::make_settings_page(){
    QScrollArea* scroll = new QScrollArea(this);
    scroll->setObjectName("ppAssistantFormScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    QWidget* page = new QWidget(scroll);
    page->setObjectName("ppAssistantForm");
    scroll->setWidget(page);
    page->setAutoFillBackground(false);
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 6, 16, 16);
    layout->setSpacing(8);

    QPushButton* back = make_button(page, "ppBack", QString::fromUtf8("\xe2\x80\xb9  Back to the chat"));
    layout->addWidget(back, 0, Qt::AlignLeft);
    connect(back, &QPushButton::clicked, this, [this](bool){
        show_start_page();
    });

    const auto section = [&](const QString& title){
        layout->addSpacing(8);
        QLabel* label = make_label(page, "ppCardTitle", title);
        QFont bold = label->font();
        bold.setBold(true);
        label->setFont(bold);
        layout->addWidget(label);
    };
    const auto field = [&](const QString& placeholder, const QString& name){
        QLineEdit* edit = new QLineEdit(page);
        edit->setObjectName("ppAssistantField");
        edit->setPlaceholderText(placeholder);
        edit->setAccessibleName(name);
        return edit;
    };

    //  The key.
    section("Claude API key");
    m_key_state = make_label(page, "ppCardText", QString());
    layout->addWidget(m_key_state);
    m_key_replace = field("A new key: sk-ant-...", "A new Claude API key");
    m_key_replace->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_key_replace);
    {
        QHBoxLayout* buttons = new QHBoxLayout();
        buttons->setSpacing(8);
        QPushButton* save = make_button(page, "ppSecondary", "Save");
        QPushButton* test = make_button(page, "ppSecondary", "Test the key");
        QPushButton* forget = make_button(page, "ppSecondary", "Forget the key");
        test->setToolTip("Asks the Claude API which models the key can use. Costs nothing.");
        buttons->addWidget(save);
        buttons->addWidget(test);
        buttons->addWidget(forget);
        buttons->addStretch(1);
        layout->addLayout(buttons);
        m_key_result = make_label(page, "ppCardText", QString());
        m_key_result->setVisible(false);
        layout->addWidget(m_key_result);
        const auto save_now = [this]{
            const QString key = m_key_replace->text().trimmed();
            if (key.isEmpty()){
                return;
            }
            QString error;
            m_service.secrets().set(Secret::CLAUDE_API_KEY, key, error);
            m_key_replace->clear();
            m_key_result->setText(
                !error.isEmpty() ? error :
                m_service.secrets().remembers() ? QString("Saved.") : QString("Kept until you close the program.")
            );
            m_key_result->setVisible(true);
            refresh_settings_page();
            refresh_send_button();
        };
        connect(save, &QPushButton::clicked, this, [save_now](bool){
            save_now();
        });
        connect(m_key_replace, &QLineEdit::returnPressed, this, save_now);
        connect(test, &QPushButton::clicked, this, [this](bool){
            if (!m_service.has_api_key()){
                m_key_result->setText("There is no key to test.");
                m_key_result->setVisible(true);
                return;
            }
            m_key_result->setText("Asking the Claude API...");
            m_key_result->setVisible(true);
            const std::shared_ptr<bool> alive = m_alive;
            m_service.refresh_models([this, alive](const QString& error){
                if (!*alive){
                    return;
                }
                if (error.isEmpty()){
                    m_key_result->setText(
                        "The key works: it can use " + QString::number(m_service.models().size()) + " models."
                    );
                    refresh_models();
                }else{
                    m_key_result->setText(error);
                }
            });
        });
        connect(forget, &QPushButton::clicked, this, [this](bool){
            QString error;
            m_service.secrets().set(Secret::CLAUDE_API_KEY, QString(), error);
            m_key_result->setText("The key is forgotten.");
            m_key_result->setVisible(true);
            refresh_settings_page();
            refresh_send_button();
        });
    }
    layout->addWidget(make_link(page, "How to get an API key", KEY_HELP_URL));
    layout->addWidget(make_remember_box(page));
    layout->addWidget(make_protection_note(page));

    //  The workspace, for a key that was not made for one.
    layout->addSpacing(4);
    layout->addWidget(make_label(
        page, "ppCardText",
        "Workspace ID. Only for a key that was not made for one workspace: the Claude API then has to be told "
        "which workspace to use."
    ));
    m_api_workspace = field("wrkspc_...  (empty for a key of one workspace)", "Workspace ID of the Claude API");
    layout->addWidget(m_api_workspace);
    m_api_workspace_note = make_label(page, "ppAssistantProblem", QString());
    m_api_workspace_note->setVisible(false);
    layout->addWidget(m_api_workspace_note);
    layout->addWidget(make_link(page, "The workspaces and their IDs, in the Claude Console", WORKSPACES_URL));
    connect(m_api_workspace, &QLineEdit::editingFinished, this, [this]{
        const QString id = clean_workspace_id(m_api_workspace->text());
        if (id != m_api_workspace->text()){
            m_api_workspace->setText(id);
        }
        if (id != m_service.settings().api_workspace_id){
            m_service.settings().api_workspace_id = id;
            m_service.save_settings();
        }
        refresh_workspace_note();
    });

    //  The model and the cost.
    section("Model and cost");
    m_model_hint = make_label(page, "ppCardText", QString());
    layout->addWidget(m_model_hint);
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->setSpacing(8);
        row->addWidget(make_label(page, "ppCardText", "Effort"));
        m_effort = new QComboBox(page);
        m_effort->setObjectName("ppAssistantChoice");
        m_effort->setAccessibleName("Effort");
        m_effort->addItem("The model's own", QString());
        m_effort->addItem("Low: quickest and cheapest", "low");
        m_effort->addItem("Medium", "medium");
        m_effort->addItem("High: most thorough", "high");
        m_effort->setToolTip("How much the model thinks and looks up before it answers. More costs more tokens.");
        row->addWidget(m_effort, 1);
        layout->addLayout(row);
        connect(m_effort, &QComboBox::activated, this, [this](int index){
            m_service.settings().chat_effort = m_effort->itemData(index).toString();
            m_service.save_settings();
        });
    }
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->setSpacing(8);
        QLabel* label = make_label(page, "ppCardText", "Ask before a conversation costs more than");
        row->addWidget(label, 1);
        m_ask_at = new QDoubleSpinBox(page);
        m_ask_at->setObjectName("ppAssistantChoice");
        m_ask_at->setAccessibleName("Ask before a conversation costs more than");
        m_ask_at->setPrefix("$ ");
        m_ask_at->setDecimals(2);
        m_ask_at->setRange(0.0, 100.0);
        m_ask_at->setSingleStep(0.5);
        m_ask_at->setSpecialValueText("Never ask");
        m_ask_at->setToolTip("Estimated from Anthropic's public prices. 0: never ask.");
        row->addWidget(m_ask_at);
        layout->addLayout(row);
        connect(m_ask_at, &QDoubleSpinBox::valueChanged, this, [this](double value){
            m_service.settings().ask_at_usd = value;
            m_service.save_settings();
        });
    }

    //  The source.
    section("The program's source on GitHub");
    layout->addWidget(make_label(
        page, "ppCardText",
        "Claude reads the source here, and puts its changes on a branch of its own (claude/...) for you to "
        "merge with a pull request."
    ));
    {
        QHBoxLayout* row = new QHBoxLayout();
        row->setSpacing(6);
        m_owner = field("owner", "Repository owner");
        m_repository = field("repository", "Repository name");
        row->addWidget(m_owner, 1);
        row->addWidget(new QLabel("/", page));
        row->addWidget(m_repository, 1);
        layout->addLayout(row);
        QHBoxLayout* branch_row = new QHBoxLayout();
        branch_row->setSpacing(8);
        branch_row->addWidget(make_label(page, "ppCardText", "Changes are made for the branch"));
        m_branch = field("branch", "Base branch");
        branch_row->addWidget(m_branch, 1);
        layout->addLayout(branch_row);
        const auto save_repository = [this]{
            Settings& settings = m_service.settings();
            const Repository standard = m_service.hooks().repository;
            const QString owner = m_owner->text().trimmed();
            const QString name = m_repository->text().trimmed();
            const QString branch = m_branch->text().trimmed();
            //  Only what differs from where this build comes from is kept.
            settings.repository_owner = owner == standard.owner ? QString() : owner;
            settings.repository_name = name == standard.name ? QString() : name;
            settings.base_branch = branch == standard.base_branch ? QString() : branch;
            m_service.save_settings();
        };
        connect(m_owner, &QLineEdit::editingFinished, this, save_repository);
        connect(m_repository, &QLineEdit::editingFinished, this, save_repository);
        connect(m_branch, &QLineEdit::editingFinished, this, save_repository);
    }
    m_token_state = make_label(page, "ppCardText", QString());
    layout->addWidget(m_token_state);
    m_token_field = field("A GitHub token: github_pat_...", "GitHub token");
    m_token_field->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_token_field);
    {
        QHBoxLayout* buttons = new QHBoxLayout();
        buttons->setSpacing(8);
        QPushButton* save = make_button(page, "ppSecondary", "Save");
        QPushButton* forget = make_button(page, "ppSecondary", "Forget the token");
        buttons->addWidget(save);
        buttons->addWidget(forget);
        buttons->addStretch(1);
        layout->addLayout(buttons);
        const auto save_now = [this]{
            const QString token = m_token_field->text().trimmed();
            if (token.isEmpty()){
                return;
            }
            QString error;
            m_service.secrets().set(Secret::GITHUB_TOKEN, token, error);
            m_token_field->clear();
            refresh_settings_page();
            if (!error.isEmpty()){
                m_token_state->setText(error);
            }
        };
        connect(save, &QPushButton::clicked, this, [save_now](bool){
            save_now();
        });
        connect(m_token_field, &QLineEdit::returnPressed, this, save_now);
        connect(forget, &QPushButton::clicked, this, [this](bool){
            QString error;
            m_service.secrets().set(Secret::GITHUB_TOKEN, QString(), error);
            refresh_settings_page();
        });
    }
    layout->addWidget(make_label(
        page, "ppAssistantSmall",
        "Reading needs no token. For commits: a fine-grained token for this one repository, with the "
        "permissions \"Contents: Read and write\" and \"Actions: Read-only\" (to read a build's log)."
    ));
    layout->addWidget(make_link(page, "Make a token on GitHub", TOKEN_HELP_URL));
    layout->addStretch(1);
    return scroll;
}

void Panel::connect_agent(){
    Agent::Events events;
    events.answer_begin = [this]{
        m_answer = nullptr;
        m_answer_text.clear();
        m_status->setText("Claude is thinking...");
        m_status->setVisible(true);
    };
    events.text = [this](const QString& text){
        m_answer_text += text;
        if (!m_render_timer->isActive()){
            m_render_timer->start();
        }
    };
    events.text_restart = [this]{
        m_answer_text.clear();
        render_answer();
    };
    events.tool_begin = [this](const QString& id, const QString& what){
        //  The text so far is complete: shown before the tool's line.
        render_answer();
        m_answer = nullptr;
        m_answer_text.clear();
        QLabel* line = make_label(m_rows_widget, "ppToolLine", MARK_BUSY + "  " + what);
        m_tool_lines[id] = line;
        m_tool_texts[id] = what;
        add_row(line);
        m_status->setVisible(false);
    };
    events.tool_progress = [this](const QString& id, const QString& progress){
        const auto line = m_tool_lines.find(id);
        if (line != m_tool_lines.end()){
            line->second->setText(MARK_BUSY + "  " + m_tool_texts[id] + " (" + progress + ")");
        }
    };
    events.tool_end = [this](const QString& id, bool ok, const QString& result){
        const auto line = m_tool_lines.find(id);
        if (line == m_tool_lines.end()){
            return;
        }
        line->second->setText((ok ? MARK_DONE : MARK_FAILED) + "  " + m_tool_texts[id]);
        line->second->setProperty("failed", !ok);
        //  What Claude got back, for who wants to know.
        line->second->setToolTip(result.size() > 700 ? result.left(700) + "..." : result);
        m_tool_lines.erase(line);
        m_tool_texts.erase(id);
    };
    events.approve = [this](const ApprovalRequest& request, std::function<void(bool approved)> answer){
        Card* card = add_card("ask", request.title, request.text);
        const std::shared_ptr<bool> answered = std::make_shared<bool>(false);
        const auto decide = [card, answered, answer](bool approved, const QString& outcome){
            if (*answered){
                return;
            }
            *answered = true;
            card->resolve(outcome);
            if (answer){
                answer(approved);
            }
        };
        const QString accepted = request.accept_label;
        card->add_button(request.accept_label, true, [decide, accepted]{
            decide(true, "You chose: " + accepted + ".");
        });
        if (!request.details_label.isEmpty()){
            card->add_button(request.details_label, false, request.show_details);
        }
        card->add_button(request.decline_label, false, [decide]{
            decide(false, "You chose not to.");
        });
        card->on_dismiss = [decide]{
            decide(false, "Not answered.");
        };
        m_open_cards.push_back(card);
        m_status->setVisible(false);
    };
    events.status = [this](const QString& what){
        m_status->setText(what);
        m_status->setVisible(!what.isEmpty());
    };
    events.usage = [this](const Usage&){
        refresh_meter();
    };
    events.finished = [this](const TurnEnd& end){
        render_answer();
        m_answer = nullptr;
        m_answer_text.clear();
        m_status->setVisible(false);
        //  Calls that never ended, questions never answered.
        for (auto& line : m_tool_lines){
            line.second->setText(MARK_FAILED + "  " + m_tool_texts[line.first]);
        }
        m_tool_lines.clear();
        m_tool_texts.clear();
        close_open_cards();

        if (end.kind == TurnEnd::Kind::STOPPED){
            add_note("Stopped.");
        }else if (!end.message.isEmpty()){
            const bool problem = end.kind == TurnEnd::Kind::FAILED || end.kind == TurnEnd::Kind::REFUSED;
            Card* card = add_card(problem ? "problem" : "ask", QString(), end.message);
            if (end.can_go_on()){
                card->add_button(end.kind == TurnEnd::Kind::FAILED ? "Try again" : "Go on", true, [this, card]{
                    card->resolve(QString());
                    if (!m_service.agent().busy()){
                        m_service.agent().go_on();
                        refresh_send_button();
                    }
                });
                m_open_cards.push_back(card);
            }
        }
        refresh_send_button();
        refresh_meter();
    };
    m_service.agent().set_events(std::move(events));

    m_service.on_commit = [this](const CommitInfo& info){
        Card* card = add_card(
            "done", "Committed to " + info.branch,
            first_line(info.message, 120) + "\n" + QString::number(info.files.size()) +
            (info.files.size() == 1 ? " file. " : " files. ") +
            "Open the pull request to have it built and to merge it."
        );
        const QString pull_request = info.pull_request_url;
        const QString commit = info.commit_url;
        card->add_button("Open pull request", true, [pull_request]{
            QDesktopServices::openUrl(QUrl(pull_request));
        });
        card->add_button("View the commit", false, [commit]{
            QDesktopServices::openUrl(QUrl(commit));
        });
    };
    m_service.on_show_changes = [this]{
        show_changes_window(window(), m_service.workspace().changes_diff());
    };
}

void Panel::refresh_models(){
    const QString current = m_service.settings().chat_model;
    m_model->blockSignals(true);
    m_model->clear();
    bool found = false;
    for (const ModelChoice& model : m_service.models()){
        //  Without "Claude" in front: the panel says it.
        QString name = model.name;
        if (name.startsWith("Claude ")){
            name = name.mid(7);
        }
        m_model->addItem(name, model.id);
        m_model->setItemData(m_model->count() - 1, model_hint(model.id), Qt::ToolTipRole);
        if (model.id == current){
            m_model->setCurrentIndex(m_model->count() - 1);
            found = true;
        }
    }
    if (!found && !current.isEmpty()){
        m_model->addItem(current, current);
        m_model->setCurrentIndex(m_model->count() - 1);
    }
    m_model->blockSignals(false);
    const QString hint = model_hint(current);
    m_model->setToolTip(
        (hint.isEmpty() ? QString("The model that answers.") : hint) +
        " Another model takes over from the next message on."
    );
    if (m_model_hint != nullptr){
        m_model_hint->setText(
            "The model is chosen under the box you write in. " +
            (hint.isEmpty() ? QString() : m_model->currentText() + ": " + hint)
        );
    }
}

QCheckBox* Panel::make_remember_box(QWidget* parent){
    QCheckBox* box = new QCheckBox("Remember the key on this computer", parent);
    box->setObjectName("ppAssistantCheck");
    box->setAccessibleName("Remember the keys on this computer");
    box->setToolTip(
        "On: the keys are saved, encrypted for your Windows account, and are there at the next start.\n"
        "Off: the keys stay in memory until you close the program and are never written to the disk, so no "
        "other program that runs as you can read them from a file. You enter them again at each start."
    );
    box->setChecked(m_service.secrets().remembers());
    connect(box, &QCheckBox::toggled, this, [this](bool remember){
        QString error;
        m_service.secrets().set_remembers(remember, error);
        refresh_protection();
        refresh_settings_page();
        if (!error.isEmpty()){
            add_note(error);
        }
    });
    m_remember_boxes.push_back(box);
    return box;
}

QLabel* Panel::make_protection_note(QWidget* parent){
    QLabel* note = make_label(parent, "ppAssistantSmall", m_service.secrets().protection_text());
    m_protection_notes.push_back(note);
    return note;
}

void Panel::refresh_protection(){
    const SecretStore& secrets = m_service.secrets();
    for (QCheckBox* box : m_remember_boxes){
        box->blockSignals(true);
        box->setChecked(secrets.remembers());
        box->blockSignals(false);
    }
    for (QLabel* note : m_protection_notes){
        note->setText(secrets.protection_text());
    }
}

void Panel::refresh_settings_page(){
    SecretStore& secrets = m_service.secrets();
    if (secrets.has(Secret::CLAUDE_API_KEY)){
        const QString key = secrets.get(Secret::CLAUDE_API_KEY);
        m_key_state->setText(
            secrets.remembers()
                ? "A key is saved (it ends in ..." + key.right(4) + "). Enter a new one to replace it."
                : "A key is in use (it ends in ..." + key.right(4) + "). It is not saved: it is gone when you close "
                  "the program. Enter a new one to replace it."
        );
    }else if (secrets.unreadable(Secret::CLAUDE_API_KEY)){
        m_key_state->setText(
            "A key was saved by another Windows account or on another computer and cannot be read here. "
            "Enter it again."
        );
    }else{
        m_key_state->setText(secrets.remembers() ? "No key is saved." : "No key is in use.");
    }
    if (secrets.has(Secret::GITHUB_TOKEN)){
        m_token_state->setText("A token is saved (it ends in ..." + secrets.get(Secret::GITHUB_TOKEN).right(4) + ").");
    }else if (secrets.unreadable(Secret::GITHUB_TOKEN)){
        m_token_state->setText("A token was saved by another Windows account and cannot be read here. Enter it again.");
    }else{
        m_token_state->setText("No token is saved: Claude can read the source, not commit.");
    }
    m_api_workspace->setText(m_service.settings().api_workspace_id);
    refresh_workspace_note();
    const Repository repository = m_service.repository();
    m_owner->setText(repository.owner);
    m_repository->setText(repository.name);
    m_branch->setText(repository.base_branch);
    const int effort = m_effort->findData(m_service.settings().chat_effort);
    m_effort->setCurrentIndex(effort < 0 ? 0 : effort);
    m_ask_at->blockSignals(true);
    m_ask_at->setValue(m_service.settings().ask_at_usd);
    m_ask_at->blockSignals(false);
    refresh_models();
}

void Panel::refresh_meter(){
    const Agent& agent = m_service.agent();
    if (agent.usage().requests == 0){
        m_meter->setText(QString());
        m_meter->setToolTip(QString());
        return;
    }
    m_meter->setText(usage_text(agent.usage()));
    m_meter->setToolTip(
        "What this conversation used so far, and what that costs at Anthropic's public prices (an "
        "estimate: the bill is Anthropic's). Each message sends the conversation along again, now about " +
        QString::number(agent.conversation_tokens()) + " tokens: a new chat starts small."
    );
}

void Panel::refresh_send_button(){
    const bool busy = m_service.agent().busy();
    m_send->setText(busy ? "Stop" : "Send");
    m_send->setEnabled(busy || !m_input->toPlainText().trimmed().isEmpty());
    m_model->setEnabled(!busy);
    m_new_chat->setEnabled(!m_service.agent().empty() || busy);
}

void Panel::refresh_empty_state(){
    m_empty->setVisible(m_service.agent().empty() && !m_service.agent().busy() && m_rows->count() <= 2);
}

Panel::Page Panel::page() const{
    switch (m_pages->currentIndex()){
    case 1:
        return Page::CONNECT;
    case 2:
        return Page::SETTINGS;
    default:
        return Page::CHAT;
    }
}

void Panel::show_page(Page target){
    switch (target){
    case Page::CHAT:
        m_pages->setCurrentIndex(0);
        break;
    case Page::CONNECT:
        m_key_error->setVisible(false);
        m_pages->setCurrentIndex(1);
        break;
    case Page::SETTINGS:
        m_key_result->setVisible(false);
        refresh_settings_page();
        m_pages->setCurrentIndex(2);
        break;
    }
}

void Panel::show_start_page(){
    show_page(m_service.has_api_key() ? Page::CHAT : Page::CONNECT);
}

void Panel::focus_input(){
    if (page() == Page::CONNECT){
        m_key_field->setFocus(Qt::OtherFocusReason);
    }else if (page() == Page::CHAT){
        m_input->setFocus(Qt::OtherFocusReason);
    }
}

void Panel::set_input(const QString& text){
    m_input->setPlainText(text);
    QTextCursor cursor = m_input->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_input->setTextCursor(cursor);
}

void Panel::refresh_workspace_note(){
    const QString id = m_service.settings().api_workspace_id;
    //  What the Console shows as a workspace's ID starts like this; a name
    //  entered instead would only come back as an error of the API.
    const bool odd = !id.isEmpty() && !id.startsWith("wrkspc_");
    if (odd){
        m_api_workspace_note->setText(
            "A workspace ID starts with wrkspc_ (the Console's page Workspaces shows it), not with the "
            "workspace's name."
        );
    }
    m_api_workspace_note->setVisible(odd);
}

void Panel::save_key(){
    const QString key = m_key_field->text().trimmed();
    if (key.isEmpty()){
        m_key_error->setText("Paste your API key in the box first.");
        m_key_error->setVisible(true);
        return;
    }
    QString error;
    m_service.secrets().set(Secret::CLAUDE_API_KEY, key, error);
    m_key_field->clear();
    if (!error.isEmpty()){
        //  Kept for this run all the same.
        add_note(error);
    }
    show_page(Page::CHAT);
    refresh_send_button();
    focus_input();
}

void Panel::send(const QString& text){
    if (m_service.agent().busy() || text.trimmed().isEmpty()){
        return;
    }
    if (!m_service.has_api_key()){
        show_page(Page::CONNECT);
        return;
    }
    show_page(Page::CHAT);
    close_open_cards();
    add_user_message(text.trimmed());
    m_service.ask(text.trimmed());
    refresh_send_button();
    refresh_empty_state();
}

void Panel::send_from_input(){
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty() || m_service.agent().busy()){
        return;
    }
    m_input->clear();
    send(text);
}

void Panel::start_new_chat(){
    const size_t changes = m_service.workspace().changes().size();
    if (changes > 0){
        const QMessageBox::StandardButton choice = QMessageBox::question(
            window(), "New chat",
            "This conversation changed " + QString::number(changes) + (changes == 1 ? " file" : " files") +
            " without committing. A new chat forgets those changes.\n\nStart a new chat?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No
        );
        if (choice != QMessageBox::Yes){
            return;
        }
    }
    m_service.new_conversation();
    clear_conversation();
    refresh_meter();
    refresh_send_button();
    refresh_empty_state();
    show_start_page();
    focus_input();
}

void Panel::add_row(QWidget* widget, Qt::Alignment alignment){
    //  In front of the stretch that keeps the rows at the top.
    m_rows->insertWidget(m_rows->count() - 1, widget, 0, alignment);
    m_empty->setVisible(false);
}

void Panel::add_user_message(const QString& text){
    add_row(new BubbleRow(m_rows_widget, text));
    scroll_to_end();
}

void Panel::add_note(const QString& text){
    add_row(make_label(m_rows_widget, "ppToolLine", text));
}

Panel::Card* Panel::add_card(const QString& kind, const QString& title, const QString& text){
    Card* card = new Card(m_rows_widget, kind, title, text);
    add_row(card);
    return card;
}

void Panel::close_open_cards(){
    //  (A card's answer can end a turn, which comes back here: the list is
    //  taken first.)
    std::vector<Card*> cards = std::move(m_open_cards);
    m_open_cards.clear();
    for (Card* card : cards){
        if (card->resolved()){
            continue;
        }
        if (card->on_dismiss){
            card->on_dismiss();
        }else{
            card->resolve(QString());
        }
    }
}

void Panel::clear_conversation(){
    m_open_cards.clear();
    m_tool_lines.clear();
    m_tool_texts.clear();
    m_answer = nullptr;
    m_answer_text.clear();
    m_status->setVisible(false);
    //  All rows but the first (what to ask) and the last (the stretch).
    while (m_rows->count() > 2){
        QLayoutItem* item = m_rows->takeAt(1);
        if (item->widget() != nullptr){
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void Panel::scroll_to_end(){
    //  Now, and again whenever the rows change size (see the scroll bar's
    //  handlers).
    m_at_end = true;
    QScrollBar* bar = m_scroll->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void Panel::render_answer(){
    m_render_timer->stop();
    if (m_answer_text.trimmed().isEmpty()){
        if (m_answer != nullptr){
            m_answer->clear();
        }
        return;
    }
    if (m_answer == nullptr){
        TextView* view = new TextView(m_rows_widget);
        m_answer = view;
        add_row(view);
        m_status->setVisible(false);
    }
    show_markdown(*m_answer, m_answer_text);
    static_cast<TextView*>(m_answer)->fit();
}

void Panel::changeEvent(QEvent* event){
    QWidget::changeEvent(event);
    //  The colors of the answers follow the theme: the text by itself, the
    //  backgrounds of their code by showing them again. Once the theme is
    //  all there, and once for the several events one change of it sends.
    const bool theme_changed = event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange;
    if (!theme_changed || m_rows_widget == nullptr || m_recolor_pending){
        return;
    }
    m_recolor_pending = true;
    const std::shared_ptr<bool> alive = m_alive;
    QTimer::singleShot(0, this, [this, alive]{
        if (!*alive){
            return;
        }
        m_recolor_pending = false;
        for (QSyntaxHighlighter* colors : window()->findChildren<QSyntaxHighlighter*>()){
            colors->rehighlight();
        }
        for (QTextBrowser* view : m_rows_widget->findChildren<QTextBrowser*>()){
            const QVariant markdown = view->property(MARKDOWN_PROPERTY);
            if (!markdown.isValid()){
                continue;
            }
            show_markdown(*view, markdown.toString());
            if (TextView* text = dynamic_cast<TextView*>(view)){
                text->fit();
            }
        }
    });
}

bool Panel::eventFilter(QObject* object, QEvent* event){
    if (object == m_input && event->type() == QEvent::KeyPress){
        const QKeyEvent* key = static_cast<QKeyEvent*>(event);
        const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
        //  Enter sends; with Shift it is a new line.
        if (enter && !(key->modifiers() & Qt::ShiftModifier)){
            if (!m_service.agent().busy()){
                send_from_input();
            }
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}



}
}
}

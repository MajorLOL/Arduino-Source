/*  PocketPilot Assistant: Panel
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 *  The panel at the right of the main window: the conversation with Claude,
 *  the box to write in, and behind its "Settings" the key, the model and
 *  the repository. Without a key it shows how to connect.
 *
 *  It shows what the service does and hands it what the user asks: the
 *  conversation itself, the tools and the keys are the service's.
 *
 */

#ifndef PokemonAutomation_PocketPilot_Assistant_Panel_H
#define PokemonAutomation_PocketPilot_Assistant_Panel_H

#include <functional>
#include <map>
#include <memory>
#include <vector>
#include <QString>
#include <QWidget>
#include "Assistant_Service.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTextBrowser;
class QTimer;
class QVBoxLayout;

namespace PokemonAutomation{
namespace PocketPilot{
namespace Assistant{


//  Puts Markdown into a text view the way the panel shows Claude's answers:
//  headings no larger than the text needs, code on its own background.
void show_markdown(QTextBrowser& view, const QString& markdown);

//  A window with the changes of the conversation as "-" and "+" lines.
void show_changes_window(QWidget* parent, const QString& diff);


class Panel : public QWidget{
public:
    static constexpr int MIN_WIDTH = 340;
    static constexpr int MAX_WIDTH = 1000;

    Panel(QWidget& parent, Service& service);
    ~Panel();

    enum class Page{
        CHAT,
        CONNECT,
        SETTINGS,
    };
    //  The page that fits: the chat with a key, how to connect without.
    void show_start_page();
    void show_page(Page page);
    Page page() const;

    void focus_input();
    void set_input(const QString& text);
    //  As if the user wrote it and pressed Enter.
    void send(const QString& text);

    //  Called when the user closes the panel with its own button.
    std::function<void()> on_close;

protected:
    virtual void changeEvent(QEvent* event) override;
    virtual bool eventFilter(QObject* object, QEvent* event) override;

private:
    class Card;

    QWidget* make_header();
    QWidget* make_chat_page();
    QWidget* make_connect_page();
    QWidget* make_settings_page();

    void connect_agent();
    void refresh_models();
    void refresh_settings_page();
    //  The box "Remember the key on this computer", and the note under it.
    QCheckBox* make_remember_box(QWidget* parent);
    QLabel* make_protection_note(QWidget* parent);
    void refresh_protection();
    void refresh_meter();
    void refresh_send_button();
    void refresh_empty_state();

    //  The conversation as it is shown.
    void add_row(QWidget* widget, Qt::Alignment alignment = Qt::Alignment());
    void add_user_message(const QString& text);
    void add_note(const QString& text);
    Card* add_card(const QString& kind, const QString& title, const QString& text);
    void close_open_cards();
    void clear_conversation();
    void scroll_to_end();
    void render_answer();

    void send_from_input();
    void start_new_chat();
    void save_key();
    void refresh_workspace_note();

private:
    Service& m_service;
    std::shared_ptr<bool> m_alive;

    QStackedWidget* m_pages = nullptr;
    QComboBox* m_model = nullptr;
    QPushButton* m_new_chat = nullptr;
    QPushButton* m_settings_button = nullptr;

    //  The chat.
    QScrollArea* m_scroll = nullptr;
    //  The conversation is scrolled to its end, and stays there.
    bool m_at_end = true;
    //  The answers are about to be shown again in the colors of a new theme.
    bool m_recolor_pending = false;
    QWidget* m_rows_widget = nullptr;
    QVBoxLayout* m_rows = nullptr;
    QWidget* m_empty = nullptr;
    QLabel* m_status = nullptr;
    QPlainTextEdit* m_input = nullptr;
    QPushButton* m_send = nullptr;
    QLabel* m_meter = nullptr;
    QTimer* m_render_timer = nullptr;

    //  The answer that is coming in: its view (made with its first text)
    //  and its text so far.
    QTextBrowser* m_answer = nullptr;
    QString m_answer_text;
    //  The lines of the tool calls under way, by the call's id.
    std::map<QString, QLabel*> m_tool_lines;
    std::map<QString, QString> m_tool_texts;
    //  Cards that wait for the user.
    std::vector<Card*> m_open_cards;

    //  How to connect.
    QLineEdit* m_key_field = nullptr;
    QLabel* m_key_error = nullptr;

    //  The settings.
    QLabel* m_key_state = nullptr;
    QLineEdit* m_key_replace = nullptr;
    QLabel* m_key_result = nullptr;
    QLineEdit* m_api_workspace = nullptr;
    QLabel* m_api_workspace_note = nullptr;
    QComboBox* m_effort = nullptr;
    QDoubleSpinBox* m_ask_at = nullptr;
    QLabel* m_token_state = nullptr;
    QLineEdit* m_token_field = nullptr;
    QLineEdit* m_owner = nullptr;
    QLineEdit* m_repository = nullptr;
    QLineEdit* m_branch = nullptr;
    QLabel* m_model_hint = nullptr;

    //  The choice to keep the keys in the file or in memory only, and the
    //  note on how they are kept: on the page of the first key and on the
    //  settings.
    std::vector<QCheckBox*> m_remember_boxes;
    std::vector<QLabel*> m_protection_notes;
};



}
}
}
#endif

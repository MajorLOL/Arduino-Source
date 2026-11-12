/*  PocketPilot Theme
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <cmath>
#include <utility>
#include <vector>
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QTextStream>
#include "CommonFramework/Windows/DpiScaler.h"
#include "PocketPilot_Theme.h"

namespace PokemonAutomation{
namespace PocketPilot{



QColor mix_colors(const QColor& from, const QColor& to, double part){
    auto channel = [part](int a, int b){
        return (int)std::lround((double)a + (double)(b - a) * part);
    };
    return QColor(
        channel(from.red(), to.red()),
        channel(from.green(), to.green()),
        channel(from.blue(), to.blue())
    );
}

double contrast_ratio(const QColor& a, const QColor& b){
    auto luminance = [](const QColor& color){
        auto linear = [](int value){
            const double channel = (double)value / 255.0;
            return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear(color.red()) + 0.7152 * linear(color.green()) + 0.0722 * linear(color.blue());
    };
    const double first = luminance(a);
    const double second = luminance(b);
    const double lighter = first > second ? first : second;
    const double darker = first > second ? second : first;
    return (lighter + 0.05) / (darker + 0.05);
}



ThemeColors make_theme_colors(bool dark, Starter starter){
    ThemeColors colors;
    colors.dark = dark;

    //  The starter: the shell at night and by day, and the color of what is
    //  pressed and selected at night (by day that is the shell's color).
    QColor night_shell;
    QColor night_accent;
    QColor day_shell;
    switch (starter){
    case Starter::FIRE:
        night_shell  = QColor(0x6c, 0x2b, 0x1e);
        night_accent = QColor(0xff, 0x8f, 0x61);
        day_shell    = QColor(0xb0, 0x3f, 0x27);
        break;
    case Starter::WATER:
        night_shell  = QColor(0x1d, 0x3f, 0x6c);
        night_accent = QColor(0x6a, 0xb8, 0xff);
        day_shell    = QColor(0x21, 0x5a, 0xa0);
        break;
    case Starter::GRASS:
        night_shell  = QColor(0x21, 0x52, 0x3a);
        night_accent = QColor(0x78, 0xd8, 0x8e);
        day_shell    = QColor(0x27, 0x72, 0x45);
        break;
    }

    const QColor white(0xff, 0xff, 0xff);
    const QColor black(0x00, 0x00, 0x00);

    if (dark){
        colors.shell        = night_shell;
        colors.shell_deep   = mix_colors(night_shell, black, 0.32);
        colors.shell_text   = mix_colors(white, night_shell, 0.05);
        colors.shell_muted  = mix_colors(colors.shell_text, night_shell, 0.30);

        //  The grays of the screen, with a little of the starter in them.
        const double TINT = 0.07;
        colors.screen       = mix_colors(QColor(0x1e, 0x20, 0x27), night_shell, TINT);
        colors.surface      = mix_colors(QColor(0x28, 0x2b, 0x34), night_shell, TINT);
        colors.surface_alt  = mix_colors(QColor(0x33, 0x37, 0x43), night_shell, TINT);
        colors.field        = mix_colors(QColor(0x16, 0x18, 0x1d), night_shell, TINT);
        colors.line         = mix_colors(QColor(0x47, 0x4c, 0x5d), night_shell, TINT);
        colors.text         = QColor(0xec, 0xee, 0xf3);
        colors.muted        = QColor(0xa9, 0xae, 0xbe);
        colors.disabled     = QColor(0x6f, 0x74, 0x84);

        colors.accent       = night_accent;
        colors.accent_hover = mix_colors(night_accent, white, 0.18);
        colors.accent_text  = QColor(0x1b, 0x14, 0x12);

        colors.ok           = QColor(0x74, 0xd3, 0x8f);
        colors.warning      = QColor(0xf2, 0xc1, 0x4e);
        colors.danger       = QColor(0xff, 0x70, 0x70);
    }else{
        colors.shell        = day_shell;
        colors.shell_deep   = mix_colors(day_shell, black, 0.28);
        colors.shell_text   = white;
        colors.shell_muted  = mix_colors(white, day_shell, 0.22);

        const double TINT = 0.04;
        colors.screen       = mix_colors(QColor(0xf1, 0xf2, 0xf6), day_shell, TINT);
        colors.surface      = white;
        colors.surface_alt  = mix_colors(QColor(0xe7, 0xe9, 0xf0), day_shell, TINT);
        colors.field        = white;
        colors.line         = mix_colors(QColor(0xc3, 0xc7, 0xd5), day_shell, TINT);
        colors.text         = QColor(0x1e, 0x22, 0x30);
        colors.muted        = QColor(0x58, 0x5e, 0x70);
        colors.disabled     = QColor(0x9a, 0x9f, 0xae);

        colors.accent       = day_shell;
        colors.accent_hover = mix_colors(day_shell, black, 0.14);
        colors.accent_text  = white;

        colors.ok           = QColor(0x1f, 0x82, 0x45);
        colors.warning      = QColor(0x96, 0x64, 0x00);
        colors.danger       = QColor(0xc2, 0x2f, 0x2f);
    }
    return colors;
}



namespace{


//  The colors for the two classic looks, so that the program's own widgets
//  fit in: a gray shell, the palette's highlight as the accent.
ThemeColors make_classic_colors(bool qdarkstyle, const QPalette& palette){
    ThemeColors colors;
    const QColor white(0xff, 0xff, 0xff);
    const QColor black(0x00, 0x00, 0x00);
    if (qdarkstyle){
        //  The colors of QDarkStyle's sheet.
        colors.dark         = true;
        colors.screen       = QColor(0x19, 0x23, 0x2d);
        colors.surface      = QColor(0x19, 0x23, 0x2d);
        colors.surface_alt  = QColor(0x25, 0x34, 0x44);
        colors.field        = QColor(0x19, 0x23, 0x2d);
        colors.line         = QColor(0x45, 0x5a, 0x6f);
        colors.text         = QColor(0xe0, 0xe1, 0xe3);
        colors.muted        = QColor(0x9d, 0xa9, 0xb5);
        colors.disabled     = QColor(0x78, 0x82, 0x8c);
        colors.accent       = QColor(0x34, 0x6b, 0x9e);
        colors.accent_hover = QColor(0x3f, 0x80, 0xbc);
        colors.accent_text  = white;
    }else{
        const QColor window = palette.color(QPalette::Active, QPalette::Window);
        colors.dark         = window.lightness() < 128;
        colors.screen       = window;
        colors.surface      = palette.color(QPalette::Active, QPalette::Base);
        colors.text         = palette.color(QPalette::Active, QPalette::WindowText);
        colors.surface_alt  = mix_colors(window, colors.text, 0.08);
        colors.field        = colors.surface;
        colors.line         = mix_colors(window, colors.text, 0.25);
        colors.muted        = mix_colors(colors.text, window, 0.35);
        colors.disabled     = mix_colors(colors.text, window, 0.60);
        colors.accent       = palette.color(QPalette::Active, QPalette::Highlight);
        colors.accent_hover = mix_colors(colors.accent, colors.dark ? white : black, 0.15);
        colors.accent_text  = palette.color(QPalette::Active, QPalette::HighlightedText);
    }
    colors.shell        = mix_colors(colors.screen, colors.text, colors.dark ? 0.10 : 0.09);
    colors.shell_deep   = mix_colors(colors.screen, colors.text, colors.dark ? 0.04 : 0.16);
    colors.shell_text   = colors.text;
    colors.shell_muted  = mix_colors(colors.text, colors.shell, 0.30);
    colors.ok           = colors.dark ? QColor(0x74, 0xd3, 0x8f) : QColor(0x1f, 0x82, 0x45);
    colors.warning      = colors.dark ? QColor(0xf2, 0xc1, 0x4e) : QColor(0x96, 0x64, 0x00);
    colors.danger       = colors.dark ? QColor(0xff, 0x70, 0x70) : QColor(0xc2, 0x2f, 0x2f);
    return colors;
}


QString read_resource(const QString& path){
    QFile file(path);
    if (!file.open(QFile::ReadOnly | QFile::Text)){
        return QString();
    }
    QTextStream stream(&file);
    return stream.readAll();
}


//  A style sheet of the resources with its "@name@"s replaced by colors.
QString fill_style_sheet(const QString& path, const ThemeColors& colors){
    QString sheet = read_resource(path);
    if (sheet.isEmpty()){
        return sheet;
    }

    const QColor white(0xff, 0xff, 0xff);
    const QColor black(0x00, 0x00, 0x00);
    //  Toward the text color: lighter at night, darker by day.
    const QColor& shell_toward = colors.shell_text;
    const QColor button = colors.dark ? colors.surface_alt : colors.surface;

    const std::vector<std::pair<QString, QString>> names{
        {"shell",           colors.shell.name()},
        {"shell_deep",      colors.shell_deep.name()},
        {"shell_text",      colors.shell_text.name()},
        {"shell_muted",     colors.shell_muted.name()},
        {"shell_hover",     mix_colors(colors.shell, shell_toward, 0.13).name()},
        {"shell_pressed",   mix_colors(colors.shell, shell_toward, 0.22).name()},
        {"shell_field",     mix_colors(colors.shell, colors.dark ? black : white, colors.dark ? 0.26 : 0.14).name()},
        {"shell_line",      mix_colors(colors.shell, shell_toward, 0.26).name()},
        {"shell_disabled",  mix_colors(colors.shell_text, colors.shell, 0.58).name()},

        {"screen",          colors.screen.name()},
        {"surface",         colors.surface.name()},
        {"surface_alt",     colors.surface_alt.name()},
        {"field",           colors.field.name()},
        {"line",            colors.line.name()},
        {"line_soft",       mix_colors(colors.line, colors.screen, 0.55).name()},
        {"text",            colors.text.name()},
        {"muted",           colors.muted.name()},
        {"disabled",        colors.disabled.name()},

        {"button",          button.name()},
        {"button_hover",    mix_colors(button, colors.text, 0.09).name()},
        {"button_pressed",  mix_colors(button, colors.text, 0.18).name()},

        {"accent",          colors.accent.name()},
        {"accent_hover",    colors.accent_hover.name()},
        {"accent_pressed",  mix_colors(colors.accent, black, 0.16).name()},
        {"accent_text",     colors.accent_text.name()},

        {"scroll",          mix_colors(colors.screen, colors.text, 0.24).name()},
        {"scroll_hover",    mix_colors(colors.screen, colors.text, 0.42).name()},

        //  The arrows of the combo boxes: light ones on dark, dark ones on light.
        {"arrow_down",          colors.dark ? ":/pocketpilot/arrow-light.png" : ":/pocketpilot/arrow-dark.png"},
        {"arrow_down_disabled", ":/pocketpilot/arrow-gray.png"},
        {"arrow_down_shell",    colors.shell_text.lightness() >= 128 ? ":/pocketpilot/arrow-light.png" : ":/pocketpilot/arrow-dark.png"},
        //  The marks of the check boxes, on the accent and on the shell's text color.
        {"check",               colors.accent_text.lightness() >= 128 ? ":/pocketpilot/check-light.png" : ":/pocketpilot/check-dark.png"},
        {"check_shell",         colors.shell_text.lightness() >= 128 ? ":/pocketpilot/check-dark.png" : ":/pocketpilot/check-light.png"},
    };
    for (const std::pair<QString, QString>& name : names){
        sheet.replace("@" + name.first + "@", name.second);
    }
    return sheet;
}


struct ThemeState{
    //  The style and the palette the system gave the application, to go
    //  back to for the classic looks.
    bool captured = false;
    QString system_style;
    QPalette system_palette;
    //  A PocketPilot theme changed them.
    bool changed = false;

    ThemeColors colors = make_theme_colors(true, Starter::FIRE);

    bool font_loaded = false;
    QString font_family;
};
ThemeState& theme_state(){
    static ThemeState state;
    return state;
}


//  The window frames (title bars) dark or light like the theme. Qt 6.8 and
//  later; before that they follow the system's setting.
enum class FrameScheme{
    SYSTEM,
    LIGHT,
    DARK,
};
void set_frame_scheme(FrameScheme scheme){
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    Qt::ColorScheme color_scheme = Qt::ColorScheme::Unknown;
    switch (scheme){
    case FrameScheme::SYSTEM:
        color_scheme = Qt::ColorScheme::Unknown;
        break;
    case FrameScheme::LIGHT:
        color_scheme = Qt::ColorScheme::Light;
        break;
    case FrameScheme::DARK:
        color_scheme = Qt::ColorScheme::Dark;
        break;
    }
    QGuiApplication::styleHints()->setColorScheme(color_scheme);
#else
    (void)scheme;
#endif
}


}



QPalette make_palette(const ThemeColors& colors){
    const QColor white(0xff, 0xff, 0xff);
    const QColor black(0x00, 0x00, 0x00);
    const QColor button = colors.dark ? colors.surface_alt : colors.surface;

    QPalette palette;
    palette.setColor(QPalette::Window, colors.screen);
    palette.setColor(QPalette::WindowText, colors.text);
    palette.setColor(QPalette::Base, colors.field);
    palette.setColor(QPalette::AlternateBase, colors.surface_alt);
    palette.setColor(QPalette::Text, colors.text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, colors.text);
    palette.setColor(QPalette::BrightText, colors.dark ? white : black);
    palette.setColor(QPalette::ToolTipBase, colors.surface_alt);
    palette.setColor(QPalette::ToolTipText, colors.text);
    palette.setColor(QPalette::PlaceholderText, colors.muted);
    palette.setColor(QPalette::Highlight, colors.accent);
    palette.setColor(QPalette::HighlightedText, colors.accent_text);
    palette.setColor(QPalette::Link, colors.dark ? QColor(0x6c, 0xb6, 0xff) : QColor(0x0b, 0x5c, 0xc4));
    palette.setColor(QPalette::LinkVisited, colors.dark ? QColor(0xc3, 0x9b, 0xff) : QColor(0x6a, 0x3d, 0xb8));

    //  The shades Fusion draws its edges with.
    palette.setColor(QPalette::Light, mix_colors(button, white, colors.dark ? 0.14 : 0.60));
    palette.setColor(QPalette::Midlight, mix_colors(button, white, colors.dark ? 0.07 : 0.30));
    palette.setColor(QPalette::Mid, colors.line);
    palette.setColor(QPalette::Dark, mix_colors(colors.line, black, colors.dark ? 0.35 : 0.20));
    palette.setColor(QPalette::Shadow, mix_colors(colors.screen, black, colors.dark ? 0.60 : 0.70));

    palette.setColor(QPalette::Disabled, QPalette::WindowText, colors.disabled);
    palette.setColor(QPalette::Disabled, QPalette::Text, colors.disabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, colors.disabled);
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, colors.disabled);
    palette.setColor(QPalette::Disabled, QPalette::Base, colors.screen);
    palette.setColor(QPalette::Disabled, QPalette::Button, colors.screen);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, colors.line);
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, colors.muted);
    return palette;
}

QString make_style_sheet(const ThemeColors& colors){
    return
        fill_style_sheet(":/pocketpilot/widgets.qss", colors) + "\n" +
        fill_style_sheet(":/pocketpilot/shell.qss", colors);
}



const ThemeColors& theme_colors(){
    return theme_state().colors;
}

void apply_theme(UiThemeMode mode, Starter starter){
    QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (app == nullptr){
        return;
    }
    ThemeState& state = theme_state();
    if (!state.captured){
        state.captured = true;
        state.system_style = app->style()->objectName();
        state.system_palette = app->palette();
    }

    const bool pocket = mode == UiThemeMode::POCKET_NIGHT || mode == UiThemeMode::POCKET_DAY;

    QString sheet;
    if (pocket){
        state.colors = make_theme_colors(mode == UiThemeMode::POCKET_NIGHT, starter);
        state.changed = true;
        set_frame_scheme(state.colors.dark ? FrameScheme::DARK : FrameScheme::LIGHT);
        if (app->style()->objectName().compare("fusion", Qt::CaseInsensitive) != 0){
            QStyle* style = QStyleFactory::create("Fusion");
            if (style != nullptr){
                app->setStyle(style);
            }
        }
        app->setPalette(make_palette(state.colors));
        sheet = make_style_sheet(state.colors);
        const QColor& surface = state.colors.screen;
        CURRENT_THEME_SURFACE =
            ((uint32_t)surface.red() << 16) | ((uint32_t)surface.green() << 8) | (uint32_t)surface.blue();
    }else{
        if (state.changed){
            //  Back to what the system gave the application.
            state.changed = false;
            set_frame_scheme(FrameScheme::SYSTEM);
            if (app->style()->objectName().compare(state.system_style, Qt::CaseInsensitive) != 0){
                QStyle* style = QStyleFactory::create(state.system_style);
                if (style != nullptr){
                    app->setStyle(style);
                }
            }
            app->setPalette(state.system_palette);
        }
        const bool qdarkstyle = mode == UiThemeMode::DARK_MODE;
        if (qdarkstyle){
            sheet = read_resource(":qdarkstyle/dark/darkstyle.qss");
        }
        state.colors = make_classic_colors(qdarkstyle, app->palette());
        //  After the classic sheet: its rules for all widgets give way to
        //  the ones for the program's own widgets.
        sheet += "\n" + fill_style_sheet(":/pocketpilot/shell.qss", state.colors);
    }

    //  Before the sheet: applying it draws the widgets, and what they draw
    //  themselves asks for the theme.
    CURRENT_THEME = mode;
    app->setStyleSheet(scale_dpi_stylesheet(sheet));
}



QFont display_font(int pixel_size){
    ThemeState& state = theme_state();
    if (!state.font_loaded){
        state.font_loaded = true;
        const int id = QFontDatabase::addApplicationFont(":/pocketpilot/Jersey20-Regular.ttf");
        if (id >= 0){
            const QStringList families = QFontDatabase::applicationFontFamilies(id);
            if (!families.isEmpty()){
                state.font_family = families.first();
            }
        }
    }
    QFont font = QApplication::font();
    if (state.font_family.isEmpty()){
        font.setBold(true);
    }else{
        font.setFamily(state.font_family);
        font.setBold(false);
        font.setItalic(false);
    }
    font.setPixelSize(pixel_size);
    return font;
}



}
}

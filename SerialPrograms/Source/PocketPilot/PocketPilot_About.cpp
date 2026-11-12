/*  PocketPilot About
 *
 *  Part of PocketPilot, which is based on Pokemon Automation
 *  (https://github.com/PokemonAutomation/).
 *
 */

#include "Common/Compiler.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include "PocketPilot_Logo.h"
#include "PocketPilot_Theme.h"
#include "PocketPilot_About.h"

namespace PokemonAutomation{
namespace PocketPilot{

namespace{


QString link(const QString& url, const QString& text, const QColor& color){
    return "<a href=\"" + url + "\" style=\"color: " + color.name() + "\">" + text + "</a>";
}

QLabel* paragraph(QWidget& parent, const QString& html){
    QLabel* label = new QLabel(html, &parent);
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setOpenExternalLinks(true);
    return label;
}


}



std::unique_ptr<QDialog> make_about(QWidget* parent, const AboutInfo& info){
    const ThemeColors& theme = theme_colors();
    //  Links in a color that reads on the window: the accent at night, a
    //  plain blue otherwise.
    const QColor link_color = theme.dark ? theme.accent : QColor(0x0b, 0x5c, 0xc4);

    std::unique_ptr<QDialog> owner = std::make_unique<QDialog>(parent);
    QDialog& dialog = *owner;
    dialog.setWindowTitle("About " + info.name);
    dialog.setMinimumWidth(560);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(26, 22, 26, 18);
    layout->setSpacing(12);

    //  The mark, the name and the build.
    QHBoxLayout* head = new QHBoxLayout();
    head->setSpacing(16);
    layout->addLayout(head);
    QLabel* mark = new QLabel(&dialog);
    const int MARK = 64;
    QPixmap pixmap = app_icon_pixmap((int)(MARK * dialog.devicePixelRatioF() + 0.5));
    pixmap.setDevicePixelRatio(dialog.devicePixelRatioF());
    mark->setPixmap(pixmap);
    mark->setFixedSize(MARK, MARK);
    head->addWidget(mark, 0, Qt::AlignTop);
    QVBoxLayout* names = new QVBoxLayout();
    names->setSpacing(0);
    head->addLayout(names, 1);
    QLabel* name = new QLabel(info.name, &dialog);
    name->setObjectName("ppAboutTitle");
    name->setFont(display_font(42));
    names->addWidget(name);
    QLabel* build = new QLabel(
        "Build " + info.build + ", on " + info.upstream_name + " " + info.upstream_version,
        &dialog
    );
    build->setObjectName("ppMuted");
    build->setTextInteractionFlags(Qt::TextSelectableByMouse);
    names->addWidget(build);
    names->addStretch(1);

    layout->addWidget(paragraph(dialog,
        info.name + " plays games on a Nintendo Switch by itself. It watches the game through a "
        "capture card and presses the buttons through a controller board."
    ));

    layout->addWidget(paragraph(dialog,
        "<b>Where it comes from.</b> " + info.name + " is a personal fork of " +
        link(info.upstream_url, info.upstream_name, link_color) +
        " (Copyright 2021 Alexander J. Yee and its contributors), used under the MIT license. "
        "The programs for the games and the documentation they link to are theirs. This fork is "
        "not theirs: please do not ask them for help with it, but use " +
        link(info.project_url, "this fork's page", link_color) + "."
    ));

    layout->addWidget(paragraph(dialog,
        "<b>Not an official product.</b> " + info.name + " is not made, endorsed or supported by "
        "Nintendo, Game Freak, Creatures or The Pokémon Company. The names of the games "
        "belong to their owners and are used here only to say which game a program is for."
    ));

    layout->addWidget(paragraph(dialog,
        "<b>Licenses.</b> The source is under the MIT license; the builds, with the libraries "
        "they bring along, are for non-commercial use. " + info.name + " uses Qt " +
        QString::fromLatin1(qVersion()) + " under the LGPL version 3 (unmodified, as separate "
        "libraries) and the typeface Jersey 20 by Sarah Cadigan-Fried under the SIL Open Font "
        "License 1.1. The licenses are in the folder \"Licenses\" next to the program."
    ));

    QLabel* details = new QLabel(
        "Compiled for " + info.architecture + " with " + info.compiler + ".",
        &dialog
    );
    details->setObjectName("ppMuted");
    details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(details);
    layout->addStretch(1);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    return owner;
}

void show_about(QWidget* parent, const AboutInfo& info){
    std::unique_ptr<QDialog> dialog = make_about(parent, info);
    dialog->exec();
}



}
}

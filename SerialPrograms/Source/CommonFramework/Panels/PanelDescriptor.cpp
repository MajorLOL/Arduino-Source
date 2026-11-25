/*  Panel Descriptor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PanelDescriptor.h"
#include "PanelSession.h"

namespace PokemonAutomation{



std::string panel_standing_tag(PanelQualityStanding standing, std::string* meaning){
    std::string tag;
    std::string tag_meaning;
    switch (standing){
    case PanelQualityStanding::OFFICIAL:
        break;
    case PanelQualityStanding::OFFICIAL_BETA:
        tag = "beta";
        tag_meaning = "Beta: it may not be fully stable.";
        break;
    case PanelQualityStanding::OFFICIAL_BELOW_STANDARDS:
        tag = "rough";
        tag_meaning = "Rough: it works, but may be difficult to use.";
        break;
    case PanelQualityStanding::THIRD_PARTY:
        tag = "3rd party";
        tag_meaning = "3rd party: made by others and not fully vetted.";
        break;
    case PanelQualityStanding::THIRD_PARTY_UNVETTED:
        tag = "unvetted";
        tag_meaning = "Unvetted: made by others and not checked.";
        break;
    case PanelQualityStanding::UNMAINTAINED:
        tag = "unmaintained";
        tag_meaning = "Unmaintained: nobody keeps it working anymore.";
        break;
    case PanelQualityStanding::DEPRECATED:
        tag = "deprecated";
        tag_meaning = "Deprecated: it will likely be removed.";
        break;
    case PanelQualityStanding::DEVELOPER:
        tag = "dev";
        tag_meaning = "Dev: a tool for developers.";
        break;
    case PanelQualityStanding::EXPERIMENTAL:
        tag = "experimental";
        tag_meaning = "Experimental: new, and not tested on a console yet. Expect mistakes.";
        break;
    }
    if (meaning != nullptr){
        *meaning = std::move(tag_meaning);
    }
    return tag;
}

Color PanelDescriptor::pick_color(PanelQualityStanding standing){
    switch (standing){
    case PanelQualityStanding::OFFICIAL:
        return COLOR_BLUE;
    case PanelQualityStanding::OFFICIAL_BETA:
        return COLOR_DARKGREEN;
    case PanelQualityStanding::OFFICIAL_BELOW_STANDARDS:
        return COLOR_MAGENTA;
    case PanelQualityStanding::THIRD_PARTY:
        return COLOR_PURPLE;
    case PanelQualityStanding::THIRD_PARTY_UNVETTED:
        return COLOR_ORANGE;
    case PanelQualityStanding::UNMAINTAINED:
    case PanelQualityStanding::DEPRECATED:
        return COLOR_RED;
    case PanelQualityStanding::DEVELOPER:
        return COLOR_DARKCYAN;
    case PanelQualityStanding::EXPERIMENTAL:
        return COLOR_ORANGE;
    }
    return Color();
}

PanelDescriptor::PanelDescriptor(
    std::string identifier,
    std::string category, std::string display_name,
    std::string doc_link,
    std::string description,
    PanelQualityStanding standing,
    bool restore_defaults_button
)
    : m_identifier(std::move(identifier))
    , m_category(std::move(category))
    , m_display_name(std::move(display_name))
    , m_doc_link(std::move(doc_link))
    , m_description(std::move(description))
    , m_standing(standing)
    , m_color(pick_color(standing))
    , m_restore_defaults_button(restore_defaults_button)
{}
std::unique_ptr<PanelSession> PanelDescriptor::make_panel() const{
    return std::unique_ptr<PanelSession>(new PanelSession(*this));
}



}

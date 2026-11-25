/*  Panel List
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Logging/Logger.h"
#include "PanelList.h"

//#include <iostream>
//using std::cout;
//using std::endl;

namespace PokemonAutomation{



PanelListDescriptor::PanelListDescriptor(
    std::string name,
    const std::string& icon_path,
    bool enabled
)
    : m_name(std::move(name))
    , m_enabled(enabled)
{
    if (!icon_path.empty()){
        try{
            m_icon = ImageRGB32(icon_path);
        }catch (FileException& e){
            e.log(global_logger_tagged());
        }
    }
}

std::vector<PanelListDescriptor::ProgramSummary> PanelListDescriptor::program_summaries() const{
    std::vector<ProgramSummary> ret;
    std::string section;
    for (const PanelEntry& entry : make_panels()){
        if (entry.descriptor == nullptr){
            section = entry.display_name;
            continue;
        }
        ProgramSummary summary{section, entry.display_name, entry.descriptor->description(), std::string(), std::string()};
        summary.tag = panel_standing_tag(entry.descriptor->standing(), &summary.tag_meaning);
        ret.emplace_back(std::move(summary));
    }
    return ret;
}

PanelListDescriptor::ProgramPanel PanelListDescriptor::make_program_panel(const std::string& name) const{
    ProgramPanel ret;
    for (PanelEntry& entry : make_panels()){
        if (entry.descriptor == nullptr || entry.display_name != name){
            continue;
        }
        ret.descriptor = std::shared_ptr<const PanelDescriptor>(std::move(entry.descriptor));
        ret.session = ret.descriptor->make_panel();
        break;
    }
    return ret;
}




}



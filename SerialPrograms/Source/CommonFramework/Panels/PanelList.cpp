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
        ret.emplace_back(ProgramSummary{section, entry.display_name, entry.descriptor->description()});
    }
    return ret;
}




}


